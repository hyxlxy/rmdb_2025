// NOTE: Unused marker (部分类型结构可能作为头文件被间接包含使用)
// 初步扫描显示未直接引用；保留不移除构建，避免破坏头文件依赖。

/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include "common/config.h"
#include "defs.h"
#include "record/rm_defs.h"

/**
 * @brief 元组版本结构，用于MVCC版本链
 */
struct TupleVersion
{
    char *data;            // 元组数据
    int size;              // 数据大小
    txn_id_t txn_id;       // 创建该版本的事务ID
    timestamp_t create_ts; // 版本创建时间戳
    timestamp_t expire_ts; // 版本失效时间戳，UINT64_MAX表示未失效
    bool is_deleted;       // 是否为删除标记
    TupleVersion *prev;    // 指向前一个版本（版本链）

    TupleVersion() : data(nullptr), size(0), txn_id(INVALID_TXN_ID),
                     create_ts(INVALID_TIMESTAMP), expire_ts(INT32_MAX),
                     is_deleted(false), prev(nullptr) {}

    TupleVersion(char *data_, int size_, txn_id_t txn_id_)
        : size(size_), txn_id(txn_id_), create_ts(INVALID_TIMESTAMP),
          expire_ts(INT32_MAX), is_deleted(false), prev(nullptr)
    {
        data = new char[size];
        memcpy(data, data_, size);
    }

    ~TupleVersion()
    {
        if (data != nullptr)
        {
            delete[] data;
            data = nullptr;
        }
    }

    // 禁用拷贝构造和赋值
    TupleVersion(const TupleVersion &) = delete;
    TupleVersion &operator=(const TupleVersion &) = delete;
};

/**
 * @brief 撤销日志记录类型
 */
enum class UndoType
{
    INSERT, // 插入操作的撤销
    UPDATE, // 更新操作的撤销
    DELETE  // 删除操作的撤销
};

/**
 * @brief 撤销日志记录结构
 */
struct UndoRecord
{
    UndoType type;             // 撤销操作类型
    Rid rid;                   // 记录的RID
    TupleVersion *old_version; // 指向被覆盖的旧版本
    std::string table_name;    // 表名

    UndoRecord(UndoType type_, const Rid &rid_, TupleVersion *old_version_,
               const std::string &table_name_)
        : type(type_), rid(rid_), old_version(old_version_), table_name(table_name_) {}

    bool operator==(const UndoRecord &other) const
    {
        return rid == other.rid && old_version == other.old_version &&
               table_name == other.table_name;
    }
};

/**
 * @brief 版本链管理器
 */
class VersionChain
{
private:
    TupleVersion *head_; // 版本链头部（最新版本）

public:
    VersionChain() : head_(nullptr) {}

    ~VersionChain()
    {
        // 清理整个版本链
        TupleVersion *current = head_;
        while (current != nullptr)
        {
            TupleVersion *next = current->prev;
            delete current;
            current = next;
        }
    }

    /**
     * @brief 获取版本链头部（最新版本）
     */
    TupleVersion *get_head() const { return head_; }

    /**
     * @brief 设置版本链头部（用于回滚时更新头指针）
     */
    void set_head(TupleVersion *new_head) { head_ = new_head; }

    /**
     * @brief 添加新版本到版本链头部
     */
    void add_version(TupleVersion *new_version)
    {
        new_version->prev = head_;
        head_ = new_version;
    }

    /**
     * @brief 提交版本（设置创建时间戳并处理版本失效）
     */
    void commit_version(TupleVersion *version, timestamp_t commit_ts)
    {
        if (version == nullptr)
            return;

        version->create_ts = commit_ts;

        // 如果这个版本有前一个版本，设置前一个版本的失效时间戳
        if (version->prev != nullptr && version->prev->expire_ts == INT32_MAX)
        {
            version->prev->expire_ts = commit_ts;
        }
    }

    /**
     * @brief 快照隔离可见性判断 - 简化且正确的实现
     */
    bool is_version_visible(const TupleVersion *version, txn_id_t current_txn_id, timestamp_t read_ts) const
    {
        if (version == nullptr)
        {
            return false;
        }

        // **规则1: 当前事务自己创建的版本**
        if (version->txn_id == current_txn_id)
        {
            // 删除版本对任何事务都不可见（包括创建它的事务）
            if (version->is_deleted)
            {
                return false;
            }
            // 当前事务的非删除版本总是可见
            return true;
        }

        // **规则2: 其他事务的版本 - 快照隔离核心逻辑**

        // 删除版本永远不可见（删除版本只是标记，不返回数据）
        if (version->is_deleted)
        {
            return false;
        }

        // 未提交的版本不可见（防止脏读）
        if (version->create_ts == INVALID_TIMESTAMP)
        {
            return false;
        }

        // 已回滚的版本不可见
        if (version->create_ts == INT32_MAX)
        {
            return false;
        }

        // 快照隔离：只有在快照时间戳前提交的版本才可见
        if (version->create_ts > read_ts)
        {
            return false;
        }

        // 检查版本是否在快照时间戳前被删除
        if (version->expire_ts != INT32_MAX &&
            version->expire_ts != INVALID_TIMESTAMP &&
            version->expire_ts <= read_ts)
        {
            return false;
        }

        return true;
    }

    /**
     * @brief 根据快照隔离规则查找可见版本 - 统一的冲突检测策略
     */
    TupleVersion *find_visible_version(timestamp_t read_ts, txn_id_t txn_id) const
    {
        TupleVersion *current = head_;
        int max_iterations = 1000; // 防止无限循环

        while (current != nullptr && max_iterations-- > 0)
        {
            // 1. 当前事务的版本处理
            if (current->txn_id == txn_id)
            {
                if (current->is_deleted)
                {
                    return nullptr;
                }
                return current;
            }

            // 2. 其他事务的版本处理
            if (current->is_deleted)
            {
                // **关键策略：删除版本的快照隔离处理**
                if (current->create_ts == INVALID_TIMESTAMP)
                {
                    current = current->prev;
                    continue;
                }
                else if (current->create_ts > read_ts)
                {
                    current = current->prev;
                    continue;
                }
                else
                {
                    return nullptr;
                }
            }

            // 3. 非删除版本的标准快照隔离判断
            if (current->create_ts == INVALID_TIMESTAMP)
            {
                // Other transaction's uncommitted version - not visible
            }
            else if (current->create_ts == INT32_MAX)
            {
                // Rolled back version - not visible
            }
            else if (current->create_ts <= read_ts &&
                     (current->expire_ts == INT32_MAX || current->expire_ts > read_ts))
            {
                return current;
            }

            current = current->prev;
        }

        return nullptr;
    }

    /**
     * @brief 专门为删除操作查找可见版本（不过滤删除版本）
     */
    TupleVersion *find_version_for_delete(timestamp_t read_ts, txn_id_t txn_id) const
    {
        TupleVersion *current = head_;
        int max_iterations = 1000; // 防止无限循环

        while (current != nullptr && max_iterations-- > 0)
        {
            // **关键：删除操作需要能找到非删除版本进行删除**
            // 不过滤删除版本，但只返回非删除的可见版本
            if (!current->is_deleted && is_version_visible(current, txn_id, read_ts))
            {
                return current;
            }
            current = current->prev;
        }

        return nullptr;
    }

    /**
     * @brief 检查是否存在写写冲突
     */
    bool has_write_conflict(txn_id_t txn_id) const
    {
        TupleVersion *current = head_;

        // 检查是否有其他事务的未提交版本
        if (current != nullptr &&
            current->txn_id != txn_id &&
            current->create_ts == INVALID_TIMESTAMP)
        {
            return true;
        }

        return false;
    }

    /**
     * @brief 获取版本链长度（用于调试）
     */
    int get_version_count() const
    {
        int count = 0;
        TupleVersion *current = head_;
        while (current != nullptr && count < 1000)
        {
            count++;
            current = current->prev;
        }
        return count;
    }

    /**
     * @brief 设置版本的失效时间戳
     */
    void expire_version(TupleVersion *version, timestamp_t expire_ts)
    {
        if (version != nullptr)
        {
            version->expire_ts = expire_ts;
        }
    }

    // 禁用拷贝构造和赋值
    VersionChain(const VersionChain &) = delete;
    VersionChain &operator=(const VersionChain &) = delete;
};

/**
 * @brief MVCC可见性判断函数
 */
class MVCCVisibility
{
public:
    /**
     * @brief 判断版本对事务是否可见
     * @param version 要检查的版本
     * @param txn_id 事务ID
     * @param read_ts 读取时间戳
     * @return 是否可见
     */
    static bool is_visible(const TupleVersion *version, txn_id_t txn_id, timestamp_t read_ts)
    {
        if (version == nullptr)
        {
            return false;
        }

        // 当前事务自身的写入始终可见
        if (version->txn_id == txn_id)
        {
            return !version->is_deleted;
        }

        // 版本创建后提交且在事务开始前提交，且未被删除
        return (version->create_ts != INVALID_TIMESTAMP) &&
               (version->create_ts <= read_ts) &&
               (version->expire_ts > read_ts) &&
               !version->is_deleted;
    }
};
