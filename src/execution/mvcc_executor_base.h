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

#include "executor_abstract.h"
#include "../record/mvcc_record_manager.h"
#include "../transaction/transaction_manager.h"
// 前向声明
class MVCCManager;

/**
 * @brief MVCC执行器基类
 * 为所有执行器提供MVCC支持的基础功能
 */
class MVCCExecutorBase : public AbstractExecutor
{
protected:
    static MVCCRecordManager *mvcc_record_manager_;
    static TransactionManager *transaction_manager_;

public:
    /**
     * @brief 初始化MVCC执行器的全局组件
     * @param mvcc_manager MVCC记录管理器
     * @param txn_manager 事务管理器
     */
    static void initialize_mvcc_components(MVCCRecordManager *mvcc_manager,
                                           TransactionManager *txn_manager)
    {
        mvcc_record_manager_ = mvcc_manager;
        transaction_manager_ = txn_manager;
    }

    /**
     * @brief 获取MVCC记录管理器
     */
    static MVCCRecordManager *get_mvcc_record_manager()
    {
        return mvcc_record_manager_;
    }

protected:
    /**
     * @brief MVCC感知的记录读取
     * @param fh 文件句柄
     * @param rid 记录ID
     * @param context 上下文
     * @param sm_manager 存储管理器
     * @return 可见的记录
     */
    std::unique_ptr<RmRecord> get_record_mvcc(RmFileHandle *fh, const Rid &rid, Context *context, SmManager *sm_manager)
    {
        if (!context || !context->txn_)
        {
            return fh->get_record(rid, context);
        }

        auto mvcc_manager = get_mvcc_manager_from_context(context);
        if (!mvcc_manager)
        {
            return fh->get_record(rid, context);
        }

        // 如果当前事务没有版本，使用标准的可见性判断
        TupleVersion *visible_version = mvcc_manager->read_version(rid, context->txn_, context->current_table_name_);
        if (!visible_version)
        {
            return nullptr;
        }

        if (visible_version->is_deleted)
        {
            return nullptr;
        }

        auto record = std::make_unique<RmRecord>(visible_version->size);
        memcpy(record->data, visible_version->data, visible_version->size);
        return record;
    }

    /**
     * @brief 查找当前事务删除的记录槽位，用于重用
     * @param mvcc_manager MVCC管理器
     * @param txn 当前事务
     * @param table_name 表名
     * @return 可重用的RID，如果没有则返回{-1, -1}
     */
    Rid find_reusable_deleted_slot(MVCCManager *mvcc_manager, Transaction *txn, const std::string &table_name)
    {
        if (!mvcc_manager || !txn)
        {
            return Rid{-1, -1};
        }

        // 遍历所有版本链，查找当前事务删除的记录
        auto &version_chains = mvcc_manager->get_all_version_chains();
        for (auto &[key, chain] : version_chains)
        {
            if (chain == nullptr)
                continue;

            // 检查key是否属于当前表
            if (table_name.empty() || key.find(table_name + "_") == 0)
            {
                TupleVersion *head = chain->get_head();
                if (head != nullptr &&
                    head->txn_id == txn->get_transaction_id() &&
                    head->create_ts == INVALID_TIMESTAMP &&
                    head->is_deleted)
                {
                    // 找到当前事务删除的记录，可以重用其槽位
                    return key_to_rid(key, table_name);
                }
            }
        }

        return Rid{-1, -1}; // 没有找到可重用的槽位
    }

    /**
     * @brief 将字符串键转换为RID
     * @param key 字符串键（格式：table_name_page_no_slot_no）
     * @param table_name 表名
     * @return RID
     */
    Rid key_to_rid(const std::string &key, const std::string &table_name)
    {
        // 解析格式：table_name_page_no_slot_no
        size_t last_underscore = key.find_last_of('_');
        if (last_underscore == std::string::npos)
            return Rid{-1, -1};

        size_t second_last_underscore = key.find_last_of('_', last_underscore - 1);
        if (second_last_underscore == std::string::npos)
            return Rid{-1, -1};

        try
        {
            int page_no = std::stoi(key.substr(second_last_underscore + 1, last_underscore - second_last_underscore - 1));
            int slot_no = std::stoi(key.substr(last_underscore + 1));
            return Rid{page_no, slot_no};
        }
        catch (const std::exception &e)
        {
            return Rid{-1, -1};
        }
    }

public:
    /**
     * @brief MVCC感知的记录插入
     * @param fh 文件句柄
     * @param buf 数据缓冲区
     * @param context 上下文
     * @param sm_manager 存储管理器
     * @return 插入的记录ID
     */
    Rid insert_record_mvcc(RmFileHandle *fh, char *buf, Context *context, SmManager *sm_manager)
    {
        if (!context || !context->txn_)
        {
            return fh->insert_record(buf, context);
        }

        auto mvcc_manager = get_mvcc_manager_from_context(context);
        if (!mvcc_manager)
        {
            return fh->insert_record(buf, context);
        }

        Rid rid;
        int record_size = fh->get_file_hdr().record_size;

        try
        {
            // 先插入物理记录，再创建 MVCC 版本。
            // 暂时禁用“重用删除槽位”优化，避免高并发下无锁遍历版本链导致崩溃。
            rid = fh->insert_record(buf, context);

            if (!mvcc_manager->insert_version(rid, buf, record_size, context->txn_, context->current_table_name_))
            {
                fh->delete_record(rid, context);
                throw TransactionAbortException(context->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
            }
        }
        catch (const TransactionAbortException &)
        {
            throw;
        }

        return rid;
    }

    /**
     * @brief MVCC感知的记录更新
     * @param fh 文件句柄
     * @param rid 记录ID
     * @param buf 新数据
     * @param context 上下文
     * @return 是否更新成功
     */
    bool update_record_mvcc(RmFileHandle *fh, const Rid &rid, char *buf, Context *context, SmManager *sm_manager)
    {
        if (!context || !context->txn_)
        {
            fh->update_record(rid, buf, context);
            return true;
        }

        auto mvcc_manager = get_mvcc_manager_from_context(context);
        if (!mvcc_manager)
        {
            fh->update_record(rid, buf, context);
            return true;
        }

        // **关键修复：在显式事务中进行更严格的冲突检测**
        bool in_explicit_transaction = context->txn_->get_txn_mode();

        // **关键修复：检查记录是否存在且可见**
        auto version_chain_full = mvcc_manager->get_or_create_version_chain(rid, context->current_table_name_);
        if (version_chain_full)
        {
            auto head = version_chain_full->get_head();
            txn_id_t current_txn_id = context->txn_->get_transaction_id();
            timestamp_t read_ts = context->txn_->get_read_ts();
            if (!head)
            {
                // 版本链为空，说明这是一个通过LOAD或其他非MVCC方式创建的记录
                // 我们需要为它创建一个初始版本，表示记录在事务开始前就存在

                // 读取当前的物理记录作为初始版本
                try
                {
                    auto current_record = fh->get_record(rid, context);
                    if (current_record)
                    {
                        // 创建一个已提交的初始版本，时间戳设为0（表示很早就存在）
                        TupleVersion *initial_version = new TupleVersion(
                            current_record->data,
                            current_record->size,
                            INVALID_TXN_ID // 使用无效事务ID表示系统记录
                        );
                        initial_version->create_ts = 0; // 设置为最早时间戳
                        initial_version->expire_ts = INT32_MAX;
                        initial_version->is_deleted = false;

                        version_chain_full->add_version(initial_version);
                        head = initial_version;
                    }
                }
                catch (const std::exception &e)
                {
                    throw TransactionAbortException(current_txn_id, AbortReason::WRITE_WRITE_CONFLICT);
                }
            }

            TupleVersion *visible_version = version_chain_full->find_visible_version(read_ts, current_txn_id);
            if (!visible_version)
            {
                // 没有可见版本，说明记录已被删除或不存在
                throw TransactionAbortException(current_txn_id, AbortReason::RECORD_NOT_FOUND);
            }
            // 只有未提交的删除版本才是真正的写写冲突，已提交的删除应该是正常的abort
            if (head && head->is_deleted && head->txn_id != current_txn_id)
            {
                if (head->create_ts == INVALID_TIMESTAMP)
                {
                    // 未提交的删除版本，这是真正的写写冲突
                    throw TransactionAbortException(current_txn_id, AbortReason::WRITE_WRITE_CONFLICT);
                }
                else
                {
                    // 已提交的删除版本，记录不存在，这是正常的abort而不是写写冲突
                    throw TransactionAbortException(current_txn_id, AbortReason::RECORD_NOT_FOUND);
                }
            }

            // 基本的未提交版本冲突检测
            if (head && head->txn_id != current_txn_id && head->create_ts == INVALID_TIMESTAMP && !head->is_deleted)
            {
                throw TransactionAbortException(current_txn_id, AbortReason::WRITE_WRITE_CONFLICT);
            }

            // **修复：更合理的时间戳冲突检测**
            // 只在确实存在并发修改冲突时才抛出写写冲突
            if (in_explicit_transaction && head && head->txn_id != current_txn_id && !head->is_deleted)
            {
                // 检查是否有其他事务在当前事务开始后提交的版本
                // 但要确保这确实是一个需要abort的情况，而不是可以正常处理的情况
                if (head->create_ts != INVALID_TIMESTAMP && head->create_ts > read_ts)
                {
                    // 这种情况下，记录在当前事务快照后被修改，应该是读写冲突而不是写写冲突
                    throw TransactionAbortException(current_txn_id, AbortReason::READ_WRITE_CONFLICT);
                }
            }
        }

        // 创建新版本
        int record_size_final = fh->get_file_hdr().record_size;
        if (mvcc_manager->update_version(rid, buf, record_size_final, context->txn_, context->current_table_name_))
        {
            fh->update_record(rid, buf, context);
            return true;
        }
        else
        {
            throw TransactionAbortException(context->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
        }
    }

    /**
     * @brief MVCC感知的记录删除
     * @param fh 文件句柄
     * @param rid 记录ID
     * @param context 上下文
     * @return 是否删除成功
     */
    bool delete_record_mvcc(RmFileHandle *fh, const Rid &rid, Context *context, SmManager *sm_manager)
    {
        if (!context || !context->txn_)
        {
            fh->delete_record(rid, context);
            return true;
        }

        auto mvcc_manager = get_mvcc_manager_from_context(context);
        if (!mvcc_manager)
        {
            fh->delete_record(rid, context);
            return true;
        }

        txn_id_t current_txn_id = context->txn_->get_transaction_id();
        timestamp_t read_ts = context->txn_->get_read_ts();

        // 检查版本链中的冲突
        auto version_chain = mvcc_manager->get_version_chain(rid, context->current_table_name_);
        if (version_chain)
        {
            auto head = version_chain->get_head();
            if (head)
            {
                // 检查冲突：其他未提交事务的版本
                if (head->txn_id != current_txn_id && head->create_ts == INVALID_TIMESTAMP)
                {
                    throw TransactionAbortException(current_txn_id, AbortReason::WRITE_WRITE_CONFLICT);
                }

                // 检查冲突：在当前事务快照后提交的版本
                if (head->txn_id != current_txn_id &&
                    head->create_ts != INVALID_TIMESTAMP &&
                    head->create_ts > read_ts)
                {
                    throw TransactionAbortException(current_txn_id, AbortReason::WRITE_WRITE_CONFLICT);
                }
            }
        }

        // 执行删除操作
        bool result = mvcc_manager->delete_version(rid, context->txn_, context->current_table_name_);

        if (!result)
        {
            throw TransactionAbortException(context->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
        }

        return result;
    }

    /**
     * @brief 检查记录是否对当前事务可见
     * @param rid 记录ID
     * @param context 上下文
     * @return 是否可见
     */
    bool is_record_visible(const Rid &rid, Context *context, SmManager *sm_manager)
    {
        if (!context || !context->txn_)
        {
            return true;
        }

        auto mvcc_manager = get_mvcc_manager_from_context(context);
        if (!mvcc_manager)
        {
            return true;
        }

        TupleVersion *visible_version = mvcc_manager->read_version(rid, context->txn_, context->current_table_name_);
        return visible_version != nullptr && !visible_version->is_deleted;
    }

    /**
     * @brief 获取当前事务
     * @param context 上下文
     * @return 事务指针
     */
    Transaction *get_current_transaction(Context *context)
    {
        if (context != nullptr)
        {
            return context->txn_;
        }
        return nullptr;
    }

    /**
     * @brief 从Context获取MVCC管理器
     * @param context 上下文
     * @return MVCC管理器指针
     */
    MVCCManager *get_mvcc_manager_from_context(Context *context)
    {
        // **关键修复：即使没有事务上下文，也要尝试获取MVCC管理器**
        // 这确保了隐式事务的INSERT操作也能创建MVCC版本，支持后续的快照隔离

        // 通过MVCC执行器基类的静态方法获取MVCC记录管理器
        auto mvcc_record_manager = MVCCExecutorBase::get_mvcc_record_manager();
        if (mvcc_record_manager)
        {
            return mvcc_record_manager->get_mvcc_manager();
        }

        return nullptr;
    }

    /**
     * @brief 检查是否启用了MVCC
     * @return 是否启用MVCC
     */
    bool is_mvcc_enabled()
    {
        return MVCCExecutorBase::mvcc_record_manager_ != nullptr && MVCCExecutorBase::transaction_manager_ != nullptr;
    }

    /**
     * @brief 处理MVCC写入冲突
     * @param operation_name 操作名称
     * @param rid 记录ID
     * @param context 上下文
     * @return 是否可以继续操作
     */
    bool handle_write_conflict(const std::string &operation_name, const Rid &rid, Context *context)
    {
        if (!is_mvcc_enabled() || context == nullptr || context->txn_ == nullptr)
        {
            return true; // 没有MVCC时不检查冲突
        }

        // 这里可以添加具体的冲突处理逻辑
        // 目前简单返回true，具体实现在各个执行器中
        return true;
    }
};
