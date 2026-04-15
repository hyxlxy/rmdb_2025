/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "transaction_manager.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"
#include "mvcc_manager.h"
// #include "mvcc_manager_fixed.h" // 暂无该文件，先移除引用避免编译失败
#include "snapshot_manager.h"
#include "common/config.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

/**
 * @description: TransactionManager构造函数
 */
TransactionManager::TransactionManager(LockManager *lock_manager, SmManager *sm_manager,
                                       ConcurrencyMode concurrency_mode)
{
    sm_manager_ = sm_manager;
    lock_manager_ = lock_manager;
    concurrency_mode_ = concurrency_mode;
    mvcc_manager_ = nullptr; // 将在需要时初始化
    // mvcc_manager_fixed_ = nullptr; // 无修复版MVCC管理器

    // 只在MVCC启用时初始化快照管理器
    if (ENABLE_MVCC)
    {
        snapshot_manager_ = new SnapshotManager();
    }
    else
    {
        snapshot_manager_ = nullptr;
    }
}

/**
 * @description: TransactionManager析构函数
 */
TransactionManager::~TransactionManager()
{
    if (snapshot_manager_ != nullptr)
    {
        delete snapshot_manager_;
    }
}

/**
 * @description: 设置MVCC管理器
 * @param mvcc_manager MVCC管理器指针
 */
void TransactionManager::set_mvcc_manager(MVCCManager *mvcc_manager)
{
    mvcc_manager_ = mvcc_manager;
    // 设置MVCC管理器的存储管理器引用
    if (mvcc_manager_ != nullptr && sm_manager_ != nullptr)
    {
        mvcc_manager_->set_storage_manager(sm_manager_);
    }
    // 设置MVCC管理器的事务管理器引用
    if (mvcc_manager_ != nullptr)
    {
        mvcc_manager_->set_transaction_manager(this);
    }
}

// 暂无修复版MVCC管理器，保留空实现以兼容旧调用点
void TransactionManager::set_mvcc_manager_fixed(void * /*mvcc_manager_fixed*/)
{
    // no-op
}

/**
 * @description: 事务的开始方法
 * @return {Transaction*} 开始事务的指针
 * @param {Transaction*} txn 事务指针，空指针代表需要创建新事务，否则开始已有事务
 * @param {LogManager*} log_manager 日志管理器指针
 */
Transaction *TransactionManager::begin(Transaction *txn, LogManager *log_manager)
{
    // Todo:
    // 1. 判断传入事务参数是否为空指针
    // 2. 如果为空指针，创建新事务
    // 3. 把开始事务加入到全局事务表中
    // 4. 返回当前事务指针
    std::unique_lock lock(latch_); // 加锁(会自动解锁)
    auto txn_ = txn;
    if (txn == nullptr)
    {
        // 创建新事务
        txn_ = new Transaction(next_txn_id_++);
        txn_->set_state(TransactionState::GROWING);

        // **快照隔离：只在MVCC启用时执行**
        if (ENABLE_MVCC)
        {
            timestamp_t read_ts = next_timestamp_.fetch_add(1);
            if (read_ts < 1)
                read_ts = 1;

            txn_->set_read_ts(read_ts);
            txn_->set_start_ts(read_ts);

            // **关键：注册事务到快照管理器并创建快照**
            if (snapshot_manager_ != nullptr)
            {
                snapshot_manager_->register_transaction(txn_->get_transaction_id());
                auto snapshot = snapshot_manager_->create_snapshot(read_ts);
                txn_->set_snapshot(snapshot.release()); // 转移所有权给Transaction
            }
        }
        else
        {
            // 即使不启用MVCC，也要设置基本的时间戳
            timestamp_t read_ts = next_timestamp_.fetch_add(1);
            if (read_ts < 1)
                read_ts = 1;
            txn_->set_read_ts(read_ts);
            txn_->set_start_ts(read_ts);
        }

        // 初始化/调整 TPCC 运行时一致性参数（按需修改或外部接入）
        // 默认严格一致：容忍度=0；如需压测观察可放宽或开启日志
        // TPCCRuntimeConfig::d_next_oid_tolerance.store(0);
        // TPCCRuntimeConfig::enable_consistency_logging.store(false);

        // 将新事务加入全局事务表
        txn_map.insert_or_assign(txn_->get_transaction_id(), txn_);

        // // 事务开始时，将所有表的count缓存设为无效:这里不管
        // if (sm_manager_ != nullptr) {
        //     try {
        //         for (auto& [table_name, fh] : sm_manager_->fhs_) {
        //             if (fh != nullptr) {
        //                 RmFileHdr file_hdr = fh->get_file_hdr();
        //                 if (file_hdr.count_cache_valid) {  // 只有当前为true时才需要修改
        //                     file_hdr.count_cache_valid = false;
        //                     fh->update_file_hdr(file_hdr);
        //                 }
        //             }
        //         }
        //     } catch (const std::exception& e) {
        //         // 继续执行，不让缓存更新失败阻止事务创建
        //     }
        // }
    }

    // **关键修复：简化日志操作，避免阻塞**
    try
    {
        if (log_manager != nullptr)
        {
            BeginLogRecord *begin_log = new BeginLogRecord(txn_->get_transaction_id());
            log_manager->add_log_to_buffer(begin_log);
        }
    }
    catch (const std::exception &e)
    {
        // 继续执行，不让日志错误阻止事务创建
    }
    return txn_;
}

/**
 * @description: 事务的提交方法
 * @param {Transaction*} txn 需要提交的事务
 * @param {LogManager*} log_manager 日志管理器指针
 */
void TransactionManager::commit(Transaction *txn, LogManager *log_manager)
{
    auto write_set = txn->get_write_set();

    txn->set_state(TransactionState::SHRINKING);

    if (ENABLE_MVCC)
    {
        timestamp_t commit_ts = get_next_timestamp();
        txn->set_commit_ts(commit_ts);
        try
        {
            commit_mvcc_versions(txn);
        }
        catch (const std::exception &e)
        {
            txn->set_state(TransactionState::ABORTED);
            if (mvcc_manager_ != nullptr)
            {
                rollback_mvcc_versions(txn);
            }
            if (!DISABLE_DEBUG_OUTPUT)
            {
                std::cerr << "[MVCC] MVCC commit failed but server continuing: " << e.what() << std::endl;
            }
            return;
        }
    }

    auto lock_set = txn->get_lock_set();

    // **关键修复：避免在遍历时修改容器，先复制锁集合**
    std::vector<LockDataId> locks_to_release(lock_set->begin(), lock_set->end());
    for (const auto &lock_data_id : locks_to_release)
    {
        lock_manager_->unlock(txn, lock_data_id);
    } // 释放全体锁

    // **修复：延迟清空事务相关资源，确保MVCC操作完全完成**
    // 在MVCC模式下，需要确保所有版本都已正确持久化后再清理
    if (ENABLE_MVCC)
    {
        // 清空MVCC撤销日志
        txn->get_undo_logs().clear();

        // **关键修复：最后清空write_set，确保MVCC持久化完成**
        write_set->clear();
    }
    else
    {
        // 传统模式下可以立即清空
        write_set->clear();
    }

    lock_set->clear();

    // **修复：清空索引相关的页面集合**
    auto index_deleted_pages = txn->get_index_deleted_page_set();
    auto index_latch_pages = txn->get_index_latch_page_set();
    
    // 这些 Page* 可能已经失效，这里只清空事务侧跟踪容器，避免解引用悬空指针
    if (index_deleted_pages) {
        index_deleted_pages->clear();
    }

    if (index_latch_pages) {
        index_latch_pages->clear();
    }

    // 7. 更新事务状态为已提交
    txn->set_state(TransactionState::COMMITTED);

    // **快照隔离：从快照管理器注销事务（只在MVCC启用时）**
    if (ENABLE_MVCC && snapshot_manager_ != nullptr)
    {
        snapshot_manager_->unregister_transaction(txn->get_transaction_id());
    }

    // **关键修复：添加日志操作的异常处理**
    try
    {
        if (log_manager != nullptr)
        {
            log_manager->add_log_to_buffer(new CommitLogRecord(txn->get_transaction_id()));
        }
    }
    catch (const std::exception &e)
    {
        // 继续执行，不让日志错误阻止事务提交
    }
    // 8. 从全局事务表中移除事务
    {
        std::unique_lock<std::mutex> lock(latch_);
        txn_map.erase(txn->get_transaction_id());
    }
}

/**
 * @description: 事务的终止（回滚）方法
 * @param {Transaction *} txn 需要回滚的事务
 * @param {LogManager} *log_manager 日志管理器指针
 */
void TransactionManager::abort(Transaction *txn, LogManager *log_manager)
{
    // 移除调试输出以避免高并发场景下的性能问题

    // Todo:
    // 1. 回滚所有写操作
    // 2. 释放所有锁
    // 3. 清空事务相关资源，eg.锁集
    // 4. 把事务日志刷入磁盘中
    // 5. 更新事务状态

    // **关键修复：参考BuzzDB实现，确保正确的事务回滚处理**
    // 1. 首先回滚MVCC版本（清理版本链）- 只在MVCC启用时
    if (ENABLE_MVCC && mvcc_manager_ != nullptr)
    {
        rollback_mvcc_versions(txn);
    }

    // **关键修复：确保abort信息被正确输出**
    // 这里不直接输出，让上层调用者（rmdb.cpp）处理输出

    // 2. 回滚物理存储操作
    auto write_set = txn->get_write_set();

    if (ENABLE_MVCC)
    {
        // MVCC模式：使用新的回滚逻辑
        for (auto it = write_set->rbegin(); it != write_set->rend(); ++it)
        {
            WriteRecord *write_record = *it;
            try
            {
                rollback_single_operation(write_record, txn);
            }
            catch (const std::exception &e)
            {
                // 继续回滚其他操作，不要因为一个失败就停止
            }
        }

        // 清理写集合
        for (auto &write_record : *write_set)
        {
            delete write_record;
        }
        write_set->clear();
    }
    else
    {
        // 传统模式：使用原始回滚逻辑
        rollback_traditional(txn, log_manager);
    }

    // 注释掉传统回滚逻辑，避免与MVCC冲突
    /*
    while (!write_set->empty())
    {
        auto write_record = write_set->back(); // 获取最后一个写记录
        write_set->pop_back();                 // 移除出记录.
        // 传统回滚逻辑已被注释，避免与MVCC冲突
        /*
        switch (write_record->GetWriteType())
        {
        case WType::INSERT_TUPLE:
        {
            auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get(); // 找到在哪里写的插入 并获取到对那张表的操作
            auto log_ = new DeleteLogRecord( // 创建删除日志记录
                txn->get_transaction_id(),
                write_record->GetRid(),
                write_record->GetTableName(),
                write_record->GetRecord());
            log_manager->add_log_to_buffer(log_);
            delete log_;
            fh_->delete_record(write_record->GetRid(), nullptr);
            sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);

            break;
        }
        case WType::UPDATE_TUPLE:
        {
            auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get();   // 找到在哪里写的插入
            auto old_record = fh_->get_record(write_record->GetRid(), nullptr); // 将老记录留下来(此时Rid还没更新)
            auto log_ = new UpdateLogRecord(
                txn->get_transaction_id(),
                write_record->GetRecord(),
                *old_record,
                write_record->GetRid(),
                write_record->GetTableName());
            log_manager->add_log_to_buffer(log_);
            delete log_;
            fh_->update_record(write_record->GetRid(), write_record->GetRecord().data, nullptr); // 将老数据COPY回去
            sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);
            break;
        }
        case WType::DELETE_TUPLE:
        {
            auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get(); // 找到在哪里写的插入 并获取到对那张表的操作
            auto log_ = new InsertLogRecord(
                txn->get_transaction_id(),
                write_record->GetRid(),
                write_record->GetTableName(),
                write_record->GetRecord());
            log_manager->add_log_to_buffer(log_);
            delete log_;
            fh_->insert_record(write_record->GetRid(), write_record->GetRecord().data); // 将数据COPY回去
            sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);
            break;
        }
        case WType::IX_INSERT_TUPLE:
        {
            // 删除索引
            sm_manager_->ihs_[write_record->GetTableName()]->delete_entry(write_record->GetRecord().data, txn);
            break;
        }
        case WType::IX_DELETE_TUPLE:
        {
            sm_manager_->ihs_[write_record->GetTableName()]->insert_entry(write_record->GetRecord().data, write_record->GetRid(), txn);
            break;
        }
        default:
            break;
        }
    }
    */
    // 结束注释块

    // 释放所有锁
    auto lock_set = txn->get_lock_set();
    // **关键修复：避免在遍历时修改容器，先复制锁集合**
    std::vector<LockDataId> locks_to_release(lock_set->begin(), lock_set->end());
    for (const auto &lock_data_id : locks_to_release)
    {
        lock_manager_->unlock(txn, lock_data_id);
    }

    // 清空事务相关资源
    txn->get_write_set()->clear();
    lock_set->clear();

    // 清空MVCC撤销日志（只在MVCC启用时）
    if (ENABLE_MVCC)
    {
        txn->get_undo_logs().clear();
    }

    // **修复：处理索引相关的页面回滚**
    auto index_deleted_pages = txn->get_index_deleted_page_set();
    auto index_latch_pages = txn->get_index_latch_page_set();

    // 这些 Page* 在回滚路径上可能已经悬空，不能再安全访问
    if (index_deleted_pages) {
        index_deleted_pages->clear();
    }

    if (index_latch_pages) {
        index_latch_pages->clear();
    }

    // 更新事务状态
    txn->set_state(TransactionState::ABORTED);

    // **快照隔离：从快照管理器注销事务（只在MVCC启用时）**
    if (ENABLE_MVCC && snapshot_manager_ != nullptr)
    {
        snapshot_manager_->unregister_transaction(txn->get_transaction_id());
    }

    // **关键修复：添加日志操作的异常处理**
    try
    {
        if (log_manager != nullptr)
        {
            log_manager->add_log_to_buffer(new AbortLogRecord(txn->get_transaction_id()));
        }
    }
    catch (const std::exception &e)
    {
        // 继续执行，不让日志错误阻止事务回滚
    }

    // 从全局事务表中移除事务
    {
        std::unique_lock<std::mutex> lock(latch_);
        txn_map.erase(txn->get_transaction_id());
    }

    // 移除调试输出以避免高并发场景下的性能问题
}

/**
 * @description: 提交事务的MVCC版本
 * @param txn 事务
 */
void TransactionManager::commit_mvcc_versions(Transaction *txn)
{
    if (mvcc_manager_ != nullptr && txn != nullptr)
    {
        mvcc_manager_->commit_transaction(txn);
    }
}

/**
 * @description: 回滚事务的MVCC版本
 * @param txn 事务
 */
void TransactionManager::rollback_mvcc_versions(Transaction *txn)
{
    if (mvcc_manager_ != nullptr && txn != nullptr)
    {
        mvcc_manager_->rollback_transaction(txn);
    }
}

/**
 * @description: 回滚单个操作（处理物理存储）
 * @param write_record 写记录
 * @param txn 事务
 */
void TransactionManager::rollback_single_operation(WriteRecord *write_record, Transaction *txn)
{
    if (write_record == nullptr || sm_manager_ == nullptr)
        return;

    try
    {
        switch (write_record->GetWriteType())
        {
        case WType::INSERT_TUPLE:
        {
            // **修复：INSERT回滚时直接删除物理记录，不检查is_record**
            // 在MVCC中，MVCC版本已经被回滚，现在需要清理物理存储
            auto fh = sm_manager_->fhs_[write_record->GetTableName()].get();
            try
            {
                fh->delete_record(write_record->GetRid(), nullptr);
            }
            catch (const std::exception &e)
            {
                // 如果删除失败（比如记录已经不存在），继续执行
            }
            break;
        }
        case WType::UPDATE_TUPLE:
        {
            // **修复：UPDATE回滚时恢复原始数据**
            // write_record->GetRecord().data 包含的是原始数据（旧值）
            auto fh = sm_manager_->fhs_[write_record->GetTableName()].get();
            try
            {
                fh->update_record(write_record->GetRid(), write_record->GetRecord().data, nullptr);
            }
            catch (const std::exception &e)
            {
                // 如果更新失败，继续执行
            }
            break;
        }
        case WType::DELETE_TUPLE:
        {
            // **关键修复：删除操作的回滚需要恢复记录**
            // 注意：在MVCC模式下，删除操作通常不直接删除底层存储
            // 所以这里可能不需要特殊处理，版本链回滚已经处理了
            break;
        }
        case WType::IX_INSERT_TUPLE:
        {
            auto ih = sm_manager_->ihs_[write_record->GetTableName()].get();
            try
            {
                ih->delete_entry(write_record->GetRecord().data, txn);
            }
            catch (const std::exception &e)
            {
            }
            break;
        }
        case WType::IX_DELETE_TUPLE:
        {
            auto ih = sm_manager_->ihs_[write_record->GetTableName()].get();
            try
            {
                ih->insert_entry(write_record->GetRecord().data, write_record->GetRid(), txn);
            }
            catch (const std::exception &e)
            {
            }
            break;
        }
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        throw;
    }
}

/**
 * @description: 传统模式的事务回滚
 * @param txn 事务
 * @param log_manager 日志管理器
 */
void TransactionManager::rollback_traditional(Transaction *txn, LogManager *log_manager)
{
    auto write_set = txn->get_write_set();

    while (!write_set->empty())
    {
        auto write_record = write_set->back();
        write_set->pop_back();

        try
        {
            switch (write_record->GetWriteType())
            {
            case WType::INSERT_TUPLE:
            {
                auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get();
                if (log_manager != nullptr)
                {
                    auto log_ = new DeleteLogRecord(
                        txn->get_transaction_id(),
                        write_record->GetRid(),
                        write_record->GetTableName(),
                        write_record->GetRecord());
                    log_manager->add_log_to_buffer(log_);
                    delete log_;
                }
                fh_->delete_record(write_record->GetRid(), nullptr);
                sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);
                break;
            }
            case WType::UPDATE_TUPLE:
            {
                auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get();
                if (log_manager != nullptr)
                {
                    auto old_record = fh_->get_record(write_record->GetRid(), nullptr);
                    auto log_ = new UpdateLogRecord(
                        txn->get_transaction_id(),
                        write_record->GetRecord(),
                        *old_record,
                        write_record->GetRid(),
                        write_record->GetTableName());
                    log_manager->add_log_to_buffer(log_);
                    delete log_;
                }
                fh_->update_record(write_record->GetRid(), write_record->GetRecord().data, nullptr);
                sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);
                break;
            }
            case WType::DELETE_TUPLE:
            {
                auto fh_ = sm_manager_->fhs_[write_record->GetTableName()].get();
                if (log_manager != nullptr)
                {
                    auto log_ = new InsertLogRecord(
                        txn->get_transaction_id(),
                        write_record->GetRid(),
                        write_record->GetTableName(),
                        write_record->GetRecord());
                    log_manager->add_log_to_buffer(log_);
                    delete log_;
                }
                fh_->insert_record(write_record->GetRid(), write_record->GetRecord().data);
                sm_manager_->get_bpm()->unpin_page({fh_->GetFd(), write_record->GetRid().page_no}, true);
                break;
            }
            case WType::IX_INSERT_TUPLE:
            {
                sm_manager_->ihs_[write_record->GetTableName()]->delete_entry(write_record->GetRecord().data, txn);
                break;
            }
            case WType::IX_DELETE_TUPLE:
            {
                sm_manager_->ihs_[write_record->GetTableName()]->insert_entry(write_record->GetRecord().data, write_record->GetRid(), txn);
                break;
            }
            default:
                break;
            }
        }
        catch (const std::exception &e)
        {
            // 继续回滚其他操作，不要因为一个失败就停止
        }

        delete write_record;
    }

  

}
