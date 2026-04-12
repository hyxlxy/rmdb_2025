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

#include <atomic>
#include <unordered_map>

#include "transaction.h"
#include "recovery/log_manager.h"
#include "concurrency/lock_manager.h"
#include "system/sm_manager.h"

// 前向声明
class MVCCManager;
// class MVCCManagerFixed; // 暂无该类型，先移除避免编译失败
class SnapshotManager;

/* 系统采用的并发控制算法，当前题目中要求两阶段封锁并发控制算法 */
enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0, BASIC_TO };

class TransactionManager{
public:
    explicit TransactionManager(LockManager *lock_manager, SmManager *sm_manager,
                             ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING);  // 移到实现文件

    ~TransactionManager();  // 析构函数

    /**
     * @description: 设置MVCC管理器
     * @param mvcc_manager MVCC管理器指针
     */
    void set_mvcc_manager(MVCCManager* mvcc_manager);  // 移到实现文件中

    /**
     * @description: 设置修复版MVCC管理器
     * @param mvcc_manager_fixed 修复版MVCC管理器指针
     */
    void set_mvcc_manager_fixed(void* mvcc_manager_fixed);

    Transaction* begin(Transaction* txn, LogManager* log_manager);

    void commit(Transaction* txn, LogManager* log_manager);

    void abort(Transaction* txn, LogManager* log_manager);

    ConcurrencyMode get_concurrency_mode() { return concurrency_mode_; }

    void set_concurrency_mode(ConcurrencyMode concurrency_mode) { concurrency_mode_ = concurrency_mode; }

    LockManager* get_lock_manager() { return lock_manager_; }

    /**
     * @description: 获取事务ID为txn_id的事务对象
     * @return {Transaction*} 事务对象的指针
     * @param {txn_id_t} txn_id 事务ID
     */
    Transaction* get_transaction(txn_id_t txn_id) {
        if(txn_id == INVALID_TXN_ID) return nullptr;

        std::unique_lock<std::mutex> lock(latch_);
        auto it = TransactionManager::txn_map.find(txn_id);
        if (it == TransactionManager::txn_map.end()) {
            return nullptr; // 事务不存在（可能已经提交或中止）
        }
        auto *res = it->second;
        lock.unlock();
        if (res == nullptr) return nullptr;

        // 注释掉线程ID检查，因为在MVCC中可能需要跨线程访问事务信息
        // assert(res->get_thread_id() == std::this_thread::get_id());

        return res;
    }

    // MVCC相关方法
    /**
     * @description: 获取下一个时间戳
     * @return {timestamp_t} 新的时间戳
     */
    timestamp_t get_next_timestamp() {
        return next_timestamp_++;
    }

    /**
     * @description: 获取当前时间戳
     * @return {timestamp_t} 当前时间戳
     */
    timestamp_t get_current_timestamp() {
        return next_timestamp_.load();
    }

    /**
     * @description: 判断事务是否已提交
     * @param txn_id 事务ID
     * @return 是否已提交
     */
    bool is_committed(txn_id_t txn_id) {
        if (txn_id == INVALID_TXN_ID) return false;

        std::unique_lock<std::mutex> lock(latch_);
        auto it = txn_map.find(txn_id);
        if (it == txn_map.end()) {
            // 事务不在表中，可能已经提交并清理
            return true;
        }

        Transaction* txn = it->second;
        return txn != nullptr && txn->get_state() == TransactionState::COMMITTED;
    }

    /**
     * @description: 判断事务是否已中止
     * @param txn_id 事务ID
     * @return 是否已中止
     */
    bool is_aborted(txn_id_t txn_id) {
        if (txn_id == INVALID_TXN_ID) return false;

        std::unique_lock<std::mutex> lock(latch_);
        auto it = txn_map.find(txn_id);
        if (it == txn_map.end()) {
            // 事务不在表中，不是中止状态
            return false;
        }

        Transaction* txn = it->second;
        return txn != nullptr && txn->get_state() == TransactionState::ABORTED;
    }

    /**
     * @description: 获取当前最大已提交时间戳
     * @return {timestamp_t} 最大已提交时间戳
     */
    timestamp_t get_max_committed_timestamp() {
        std::unique_lock<std::mutex> lock(latch_);
        timestamp_t max_committed = 0;
        for (const auto& [txn_id, txn] : txn_map) {
            if (txn->is_committed() && txn->get_commit_ts() > max_committed) {
                max_committed = txn->get_commit_ts();
            }
        }
        return max_committed;
    }

    /**
     * @description: 提交事务的MVCC版本
     * @param txn 事务
     */
    void commit_mvcc_versions(Transaction* txn);

    /**
     * @description: 回滚事务的MVCC版本
     * @param txn 事务
     */
    void rollback_mvcc_versions(Transaction* txn);

    /**
     * @description: 回滚单个操作（处理物理存储）
     * @param write_record 写记录
     * @param txn 事务
     */
    void rollback_single_operation(WriteRecord* write_record, Transaction* txn);

    /**
     * @description: 传统模式的事务回滚
     * @param txn 事务
     * @param log_manager 日志管理器
     */
    void rollback_traditional(Transaction* txn, LogManager* log_manager);

    static std::unordered_map<txn_id_t, Transaction *> txn_map;     // 全局事务表，存放事务ID与事务对象的映射关系

    ConcurrencyMode concurrency_mode_;      // 事务使用的并发控制算法，目前只需要考虑2PL
    std::atomic<txn_id_t> next_txn_id_{0};  // 用于分发事务ID
    std::atomic<timestamp_t> next_timestamp_{1};    // 用于分发事务时间戳
    std::mutex latch_;  // 用于txn_map的并发
    SmManager *sm_manager_;
    LockManager *lock_manager_;
    MVCCManager *mvcc_manager_;  // MVCC管理器
    // MVCCManagerFixed *mvcc_manager_fixed_ = nullptr; // 暂无修复版管理器
    SnapshotManager* snapshot_manager_;  // 快照管理器（使用原始指针）
};
