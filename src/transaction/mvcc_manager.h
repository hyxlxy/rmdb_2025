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

#include <unordered_map>
#include <mutex>
#include <memory>
#include <set>
#include <vector>
#include "mvcc_defs.h"
#include "transaction.h"
#include "transaction_manager.h"

// 前向声明
class SmManager;

/**
 * @brief MVCC版本管理器
 * 负责管理所有表的版本链和撤销日志
 */
class MVCCManager
{
private:
    // 每个RID对应一个版本链，使用字符串键值来区分不同表
    std::unordered_map<std::string, std::unique_ptr<VersionChain>> version_chains_;
    std::mutex version_chains_mutex_;
    SmManager *storage_manager_;      // 存储管理器引用，用于持久化版本
    TransactionManager *txn_manager_; // 事务管理器引用，用于检查事务状态

    // **ReadWriteConflictDeleteTest：读取跟踪机制**
    std::unordered_map<std::string, std::set<txn_id_t>> read_sets_; // key -> 读取该记录的事务集合
    std::mutex read_sets_mutex_;                                    // 保护读取集合的互斥锁

    // **关键修复：将RID和表名转换为唯一的字符串键**
    std::string rid_to_key(const Rid &rid, const std::string &table_name = "") const
    {
        return table_name + "_" + std::to_string(rid.page_no) + "_" + std::to_string(rid.slot_no);
    }

    VersionChain *get_version_chain_nolock(const Rid &rid, const std::string &table_name = "")
    {
        std::string key = rid_to_key(rid, table_name);
        auto it = version_chains_.find(key);
        if (it != version_chains_.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    VersionChain *create_version_chain_nolock(const Rid &rid, const std::string &table_name = "")
    {
        std::string key = rid_to_key(rid, table_name);
        auto chain = std::make_unique<VersionChain>();
        VersionChain *chain_ptr = chain.get();
        version_chains_[key] = std::move(chain);
        return chain_ptr;
    }

    VersionChain *get_or_create_version_chain_nolock(const Rid &rid, const std::string &table_name = "")
    {
        if (auto *chain = get_version_chain_nolock(rid, table_name); chain != nullptr)
        {
            return chain;
        }
        return create_version_chain_nolock(rid, table_name);
    }

public:
    MVCCManager() : storage_manager_(nullptr), txn_manager_(nullptr) {}
    explicit MVCCManager(SmManager *sm_manager, TransactionManager *txn_manager = nullptr)
        : storage_manager_(sm_manager), txn_manager_(txn_manager) {}
    ~MVCCManager() = default;

    /**
     * @brief 设置存储管理器引用
     */
    void set_storage_manager(SmManager *sm_manager)
    {
        storage_manager_ = sm_manager;
    }

    /**
     * @brief 设置事务管理器引用
     */
    void set_transaction_manager(TransactionManager *txn_manager)
    {
        txn_manager_ = txn_manager;
    }

    /**
     * @brief 获取指定RID的版本链
     */
    VersionChain *get_version_chain(const Rid &rid, const std::string &table_name = "")
    {
        std::lock_guard<std::mutex> lock(version_chains_mutex_);
        return get_version_chain_nolock(rid, table_name);
    }

    /**
     * @brief 创建新的版本链
     */
    VersionChain *create_version_chain(const Rid &rid, const std::string &table_name = "")
    {
        std::lock_guard<std::mutex> lock(version_chains_mutex_);
        return create_version_chain_nolock(rid, table_name);
    }

    /**
     * @brief 获取所有版本链（用于查找可重用的删除记录）
     */
    const std::unordered_map<std::string, std::unique_ptr<VersionChain>> &get_all_version_chains() const
    {
        return version_chains_;
    }

    /**
     * @brief 获取或创建版本链（线程安全版本）
     */
    VersionChain *get_or_create_version_chain(const Rid &rid, const std::string &table_name = "")
    {
        std::lock_guard<std::mutex> lock(version_chains_mutex_);
        return get_or_create_version_chain_nolock(rid, table_name);
    }

    /**
     * @brief 插入新版本
     */
    bool insert_version(const Rid &rid, char *data, int size, Transaction *txn, const std::string &table_name = "")
    {
        if (txn == nullptr)
            return false;

        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        txn_id_t current_txn_id = txn->get_transaction_id();
        timestamp_t read_ts = txn->get_read_ts();

        // **WriteWriteConflictDeleteInsertTest：检查是否有其他事务删除了相同内容的元组**
        if (!check_tuple_value_conflict_for_insert(data, size, txn, table_name))
        {
            return false;
        }

        VersionChain *chain = get_or_create_version_chain_nolock(rid, table_name);
        TupleVersion *head = chain->get_head();

        // **修复：正确处理同一事务的删除后插入场景**
        if (head != nullptr && head->txn_id == current_txn_id && head->create_ts == INVALID_TIMESTAMP)
        {
            if (head->is_deleted)
            {
                // 同一事务删除后插入 - 重用删除版本
                if (head->data != nullptr)
                {
                    delete[] head->data;
                }
                head->data = new char[size];
                memcpy(head->data, data, size);
                head->size = size;
                head->is_deleted = false;
                return true;
            }
            else
            {
                // 重复插入同一记录（非删除版本），这是错误的
                return false;
            }
        }

        // **修复：INSERT操作的严格写写冲突检测**
        if (head != nullptr && head->txn_id != current_txn_id)
        {
            // 检查是否存在未提交的写写冲突
            if (head->create_ts == INVALID_TIMESTAMP)
            {
                // 任何未提交的版本都是冲突，包括删除版本
                // 因为我们不知道其他事务会提交还是回滚
                return false;
            }
            if (head->create_ts > read_ts)
            {
                return false;
            }
        }

        // 创建新版本，初始状态为未提交
        TupleVersion *new_version = new TupleVersion(data, size, current_txn_id);
        new_version->create_ts = INVALID_TIMESTAMP; // 未提交状态
        new_version->expire_ts = INT32_MAX;         // 永不过期，直到被新版本替换

        chain->add_version(new_version);

        return true;
    }

    /**
     * @brief 更新版本（标准MVCC实现）
     */
    bool update_version(const Rid &rid, char *new_data, int size, Transaction *txn, const std::string &table_name = "")
    {
        if (txn == nullptr)
            return false;

        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        VersionChain *chain = get_or_create_version_chain_nolock(rid, table_name);
        if (chain == nullptr)
            return false;

        // **关键修复：允许更新空版本链的记录（通过LOAD等方式创建的记录）**
        // 空版本链表示记录存在但没有MVCC版本，这在LOAD数据后是正常的
        // 我们将在后续逻辑中处理这种情况

        txn_id_t current_txn_id = txn->get_transaction_id();
        timestamp_t read_ts = txn->get_read_ts();

        // **Snapshot Isolation 写写冲突检测**
        // 1. 其他事务的未提交版本
        // 2. 其他事务在当前事务快照后提交的版本
        TupleVersion *latest_head = chain->get_head();
        if (latest_head != nullptr && latest_head->txn_id != current_txn_id)
        {
            if (latest_head->create_ts == INVALID_TIMESTAMP)
            {
                return false;
            }
            if (latest_head->create_ts > read_ts)
            {
                return false;
            }
        }

        // **同一事务内的多次更新：直接更新最新版本**
        if (latest_head != nullptr && latest_head->txn_id == current_txn_id && latest_head->create_ts == INVALID_TIMESTAMP)
        {
            // **关键修复：检查是否尝试更新已删除的记录**
            if (latest_head->is_deleted)
            {
                // 不能更新已被当前事务删除的记录，这应该导致事务abort
                return false;
            }

            // 释放旧数据并设置新数据
            if (latest_head->data != nullptr)
            {
                delete[] latest_head->data;
            }
            latest_head->data = new char[size];
            memcpy(latest_head->data, new_data, size);
            latest_head->size = size;
            // 注意：不需要设置is_deleted = false，因为我们已经检查过它不是删除版本
            return true;
        }

        // **修复Non_Repeatable_Read_Lost_Update：查找当前事务可见的版本**
        TupleVersion *visible_version = nullptr;
        if (latest_head != nullptr)
        {
            visible_version = chain->find_visible_version(read_ts, current_txn_id);
            if (visible_version == nullptr)
            {
                return false; // 没有可见版本，无法更新
            }

            // **检查记录是否已被删除**
            if (visible_version->is_deleted)
            {
                return false; // 不能更新已删除的记录
            }
        }
        // 如果head为null，说明是空版本链，visible_version保持为nullptr
        // 这种情况下我们仍然允许更新操作继续，因为记录在物理层面存在

        // **创建新版本**
        TupleVersion *new_version = new TupleVersion(new_data, size, current_txn_id);
        new_version->create_ts = INVALID_TIMESTAMP; // 未提交状态
        new_version->expire_ts = INT32_MAX;         // 永不过期，直到被新版本替换

        chain->add_version(new_version);

        // **添加到事务的撤销日志**
        UndoRecord undo_record(UndoType::UPDATE, rid, visible_version, table_name);
        txn->add_undo_log(undo_record);

        return true;
    }

    /**
     * @brief 删除版本（标准MVCC删除实现）
     */
    bool delete_version(const Rid &rid, Transaction *txn, const std::string &table_name = "")
    {
        if (txn == nullptr)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        VersionChain *chain = get_version_chain_nolock(rid, table_name);

        // **修复：如果没有版本链，尝试为原始记录创建初始版本**
        if (chain == nullptr)
        {
            if (storage_manager_)
            {
                try
                {
                    // 尝试从存储中读取原始记录
                    auto table_handle = storage_manager_->fhs_.find(table_name);
                    if (table_handle != storage_manager_->fhs_.end())
                    {
                        Context temp_context(nullptr, nullptr, nullptr);
                        auto original_record = table_handle->second->get_record(rid, &temp_context);
                        if (original_record)
                        {
                            // 创建版本链并添加初始版本
                            chain = create_version_chain_nolock(rid, table_name);
                            TupleVersion *initial_version = new TupleVersion(
                                original_record->data,
                                original_record->size,
                                0 // 使用事务ID 0表示初始版本
                            );
                            initial_version->create_ts = 0; // 初始版本的时间戳设为0
                            initial_version->expire_ts = INT32_MAX;
                            initial_version->is_deleted = false;

                            chain->add_version(initial_version);
                        }
                    }
                }
                catch (...)
                {
                    // 如果创建初始版本失败，返回false
                    return false;
                }
            }

            if (chain == nullptr)
            {
                return false;
            }
        }

        // **修复：检查写写冲突时要更加严格**
        txn_id_t current_txn_id = txn->get_transaction_id();
        timestamp_t read_ts = txn->get_read_ts();
        TupleVersion *head = chain->get_head();
        if (head)
        {
            // 如果头版本是其他事务的未提交版本，直接返回失败
            if (head->txn_id != current_txn_id && head->create_ts == INVALID_TIMESTAMP)
            {
                return false;
            }
            // 如果头版本是其他事务在当前事务快照后提交的，也返回失败
            if (head->txn_id != current_txn_id &&
                head->create_ts != INVALID_TIMESTAMP &&
                head->create_ts > read_ts)
            {
                return false;
            }
        }

        // **修复：使用专门用于删除操作的版本查找方法**
        // find_version_for_delete 不会过滤掉删除版本，能找到真正可删除的数据版本
        TupleVersion *visible_version = chain->find_version_for_delete(read_ts, current_txn_id);
        if (!visible_version)
        {
            return false;
        }

        // **修复：确保不重复删除已删除的版本**
        if (visible_version->is_deleted)
        {
            return false;
        }

        // **修复：检查是否存在当前事务的删除版本**
        TupleVersion *current = chain->get_head();
        while (current != nullptr)
        {
            if (current->txn_id == current_txn_id && current->is_deleted)
            {
                // 当前事务已经删除了这个记录
                return false;
            }
            current = current->prev;
        }

        // 创建删除版本，保存被删除记录的数据
        TupleVersion *delete_version = new TupleVersion(
            visible_version->data,
            visible_version->size,
            current_txn_id);
        delete_version->is_deleted = true;
        delete_version->create_ts = INVALID_TIMESTAMP;
        delete_version->expire_ts = INT32_MAX;

        chain->add_version(delete_version);
        UndoRecord undo_record(UndoType::DELETE, rid, visible_version, table_name);
        txn->add_undo_log(undo_record);
        return true;
    }

    /**
     * @brief 读取可见版本（使用标准可见性管理器）
     */
    TupleVersion *read_version(const Rid &rid, Transaction *txn, const std::string &table_name = "")
    {
        if (txn == nullptr)
        {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        VersionChain *chain = get_version_chain_nolock(rid, table_name);
        if (chain == nullptr)
        {
            // **修复：为没有版本链的记录创建初始版本**
            if (storage_manager_)
            {
                try
                {
                    // 尝试从存储中读取原始记录
                    auto table_handle = storage_manager_->fhs_.find(table_name);
                    if (table_handle != storage_manager_->fhs_.end())
                    {
                        Context temp_context(nullptr, nullptr, nullptr);
                        auto original_record = table_handle->second->get_record(rid, &temp_context);
                        if (original_record)
                        {
                            // 创建版本链并添加初始版本
                            chain = create_version_chain_nolock(rid, table_name);
                            TupleVersion *initial_version = new TupleVersion(
                                original_record->data,
                                original_record->size,
                                0 // 使用事务ID 0表示初始版本
                            );
                            initial_version->create_ts = 0; // 初始版本的时间戳设为0
                            initial_version->expire_ts = INT32_MAX;
                            initial_version->is_deleted = false;

                            chain->add_version(initial_version);
                        }
                    }
                }
                catch (...)
                {
                    // 如果创建初始版本失败，返回nullptr
                    return nullptr;
                }
            }

            if (chain == nullptr)
            {
                return nullptr;
            }
        }

        // **关键修复：检查版本链是否为空（回滚后可能为空）**
        if (chain->get_head() == nullptr)
        {
            return nullptr;
        }

        // **标准实现：使用版本链的find_visible_version方法**
        TupleVersion *visible_version = chain->find_visible_version(txn->get_read_ts(), txn->get_transaction_id());

        return visible_version;
    }

    /**
     * @brief 重构指定时间戳的记录版本（标准版本重构实现）
     */
    std::unique_ptr<RmRecord> reconstruct_version(const Rid &rid, timestamp_t target_ts, txn_id_t txn_id, const std::string &table_name = "")
    {
        VersionChain *chain = get_version_chain(rid, table_name);
        if (chain == nullptr)
        {
            return nullptr;
        }

        TupleVersion *current = chain->get_head();
        int max_iterations = 1000; // 防止无限循环

        while (current != nullptr && max_iterations-- > 0)
        {
            // **标准版本重构规则**

            // 规则1: 当前事务的版本（无论是否提交）
            if (current->txn_id == txn_id)
            {
                if (current->is_deleted)
                {
                    // 当前事务删除了记录
                    return nullptr;
                }
                // 重构当前事务的版本
                auto record = std::make_unique<RmRecord>(current->size);
                memcpy(record->data, current->data, current->size);
                return record;
            }

            // 规则2: 其他事务的已提交版本
            if (current->create_ts != INVALID_TIMESTAMP &&
                current->create_ts != INT32_MAX && // 不是回滚版本
                current->create_ts <= target_ts &&
                (current->expire_ts == INT32_MAX || current->expire_ts > target_ts) &&
                !current->is_deleted)
            {
                // 重构其他事务的已提交版本
                auto record = std::make_unique<RmRecord>(current->size);
                memcpy(record->data, current->data, current->size);
                return record;
            }

            current = current->prev;
        }

        return nullptr;
    }

    /**
     * @brief 清理事务的读取记录（事务提交或回滚时调用）
     * @param txn 要清理的事务
     */
    void cleanup_read_sets(Transaction *txn)
    {
        if (txn == nullptr)
            return;

        std::lock_guard<std::mutex> lock(read_sets_mutex_);
        txn_id_t txn_id = txn->get_transaction_id();

        // 从所有读取集合中移除该事务
        for (auto &[key, reader_set] : read_sets_)
        {
            reader_set.erase(txn_id);
        }

        // 清理空的读取集合
        auto it = read_sets_.begin();
        while (it != read_sets_.end())
        {
            if (it->second.empty())
            {
                it = read_sets_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    /**
     * @brief 提交事务的所有版本
     */
    void commit_transaction(Transaction *txn)
    {
        if (txn == nullptr)
            return;

        timestamp_t commit_ts = txn->get_commit_ts();
        txn_id_t txn_id = txn->get_transaction_id();

        std::lock_guard<std::mutex> lock(version_chains_mutex_);
        // 遍历所有版本链，提交该事务的未提交版本并持久化
        for (auto &[key, chain] : version_chains_)
        {
            if (chain == nullptr)
                continue;
            commit_chain_versions(chain.get(), txn_id, commit_ts, key);
        }
        // 清理读取集合
        cleanup_read_sets(txn);
    }

private:
    /**
     * @brief 提交版本链中指定事务的版本并持久化到底层存储
     */
    void commit_chain_versions(VersionChain *chain, txn_id_t txn_id, timestamp_t commit_ts, const std::string &key)
    {
        if (chain == nullptr)
            return;

        TupleVersion *current = chain->get_head();
        TupleVersion *committed_version = nullptr; // 记录本次提交的版本

        // **修复：遍历版本链，提交指定事务的所有未提交版本**
        while (current != nullptr)
        {
            if (current->txn_id == txn_id && current->create_ts == INVALID_TIMESTAMP)
            {
                // **修复：设置提交时间戳**
                current->create_ts = commit_ts;

                // **修复：如果这是删除版本，保持删除标记**
                // 删除版本的expire_ts保持为INT32_MAX，表示这个删除操作永久有效

                // **修复：如果有前一个版本，设置其过期时间**
                if (current->prev != nullptr &&
                    current->prev->create_ts != INVALID_TIMESTAMP &&
                    current->prev->expire_ts == INT32_MAX)
                {
                    current->prev->expire_ts = commit_ts;
                }

                committed_version = current; // 记录已提交的版本
            }
            current = current->prev;
        }

        // **关键修复：确保版本提交和持久化的原子性**
        if (committed_version != nullptr && storage_manager_ != nullptr)
        {
            // 同步持久化，确保MVCC版本和物理存储的一致性
            try
            {
                persist_committed_version(key, committed_version);
            }
            catch (const std::exception &e)
            {
                // 持久化失败时，回滚已提交的版本
                committed_version->create_ts = INVALID_TIMESTAMP;
                throw; // 重新抛出异常，让事务管理器处理
            }
        }
    }

    /**
     * @brief 将已提交的版本持久化到底层存储
     */
    void persist_committed_version(const std::string &key, TupleVersion *version);

public:
    /**
     * @brief 回滚事务的所有版本
     * 参考BuzzDB实现，确保正确清理未提交版本
     */
    void rollback_transaction(Transaction *txn)
    {
        if (txn == nullptr)
            return;

        txn_id_t txn_id = txn->get_transaction_id();

        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        // **关键修复：参考BuzzDB的回滚策略 - 彻底清理版本链中的未提交版本**

        int total_rollback_count = 0;
        std::vector<std::string> empty_chains_to_remove;

        for (auto &[key, chain] : version_chains_)
        {
            if (chain == nullptr)
                continue;

            int chain_rollback_count = rollback_chain_versions(chain.get(), txn_id);
            total_rollback_count += chain_rollback_count;

            // **关键修复：如果版本链变空，标记为需要删除**
            if (chain->get_head() == nullptr)
            {
                empty_chains_to_remove.push_back(key);
            }
        }

        // **关键修复：清理空的版本链**
        for (const std::string &key : empty_chains_to_remove)
        {
            version_chains_.erase(key);
        }

        cleanup_read_sets(txn);
    }

private:
    /**
     * @brief 从版本链中回滚指定事务的版本
     * @param chain 版本链
     * @param txn_id 事务ID
     * @return 回滚的版本数量
     */
    int rollback_chain_versions(VersionChain *chain, txn_id_t txn_id)
    {
        if (chain == nullptr)
            return 0;

        int rollback_count = 0;
        TupleVersion *current = chain->get_head();
        TupleVersion *prev = nullptr;

        // **关键修复：彻底删除未提交的版本，而不是标记**
        while (current != nullptr)
        {
            TupleVersion *next = current->prev;

            if (current->txn_id == txn_id && current->create_ts == INVALID_TIMESTAMP)
            {
                // 从版本链中移除这个版本
                if (prev == nullptr)
                {
                    // 这是头节点
                    chain->set_head(next);
                }
                else
                {
                    prev->prev = next;
                }

                // 释放版本内存
                delete current;
                rollback_count++;
            }
            else
            {
                prev = current;
            }

            current = next;
        }

        return rollback_count;
    }

public:
    /**
     * @brief 清理指定表的所有MVCC版本链
     * 用于DROP TABLE操作
     */
    void clear_table_versions(const std::string &table_name)
    {
        std::lock_guard<std::mutex> lock(version_chains_mutex_);

        std::vector<std::string> keys_to_remove;

        // **修复：查找所有可能的版本链格式**
        for (auto &[key, chain] : version_chains_)
        {
            // 格式1: "table_name_page_slot"
            if (key.find(table_name + "_") == 0)
            {
                keys_to_remove.push_back(key);
            }
            // 格式2: "_page_slot" (没有表名前缀的旧格式)
            else if (key.find("_") == 0 && key.find(table_name) == std::string::npos)
            {
                // 这可能是旧格式的版本链，也需要清理
                keys_to_remove.push_back(key);
            }
        }

        // 删除版本链
        for (const std::string &key : keys_to_remove)
        {
            version_chains_.erase(key);
        }
    }

    // 禁用拷贝构造和赋值
    MVCCManager(const MVCCManager &) = delete;
    MVCCManager &operator=(const MVCCManager &) = delete;

private:
    /**
     * @brief 检查基于元组值的插入冲突（WriteWriteConflictDeleteInsertTest）
     * @param data 要插入的元组数据
     * @param size 数据大小
     * @param txn 当前事务
     * @param table_name 表名
     * @return true表示无冲突，false表示有冲突
     */
    bool check_tuple_value_conflict_for_insert(char *data, int size, Transaction *txn, const std::string &table_name)
    {
        // 遍历所有版本链，查找属于指定表的版本链
        for (const auto &chain_pair : version_chains_)
        {
            const std::string &key = chain_pair.first;

            // 检查key是否属于指定表（key格式：table_name_page_slot）
            if (key.find(table_name + "_") != 0)
            {
                continue; // 不属于当前表
            }

            VersionChain *chain = chain_pair.second.get();
            if (chain == nullptr)
                continue;

            TupleVersion *head = chain->get_head();

            // **关键修复：检查是否有其他事务删除了相同内容的元组**
            if (head != nullptr &&
                head->txn_id != txn->get_transaction_id() &&
                head->create_ts == INVALID_TIMESTAMP &&
                head->is_deleted &&
                head->data != nullptr)
            {
                // **比较元组数据内容是否相同**
                if (head->size == size && memcmp(head->data, data, size) == 0)
                {
                    // 发现其他事务删除了相同内容的元组，这是写写冲突
                    // 根据MVCC语义，当前事务不能插入与其他未提交删除操作相同的数据
                    return false; // 返回冲突
                }
            }
        }

        return true; // 无冲突
    }

};
