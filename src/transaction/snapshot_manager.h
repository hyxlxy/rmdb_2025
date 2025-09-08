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

#include <unordered_set>
#include <mutex>
#include "common/config.h"
#include "mvcc_defs.h"

/**
 * @brief 快照隔离的快照数据结构
 */
class Snapshot {
public:
    timestamp_t snapshot_time;                    // 快照时间戳
    std::unordered_set<txn_id_t> active_tx_ids;  // 快照时的活跃事务ID集合
    
    Snapshot(timestamp_t time, const std::unordered_set<txn_id_t>& active_txs)
        : snapshot_time(time), active_tx_ids(active_txs) {}
    
    /**
     * @brief 判断版本对此快照是否可见（简化版本）
     * @param version 要检查的版本
     * @return 是否可见
     */
    bool is_visible(const TupleVersion* version, txn_id_t current_txn_id) const {
        if (version == nullptr) {
            return false;
        }

        // 规则1: 当前事务自身的写入始终可见（除了删除版本）
        if (version->txn_id == current_txn_id) {
            return !version->is_deleted;
        }

        // 规则2: 删除版本不可见
        if (version->is_deleted) {
            return false;
        }

        // 规则3: 未提交的版本不可见
        if (version->create_ts == INVALID_TIMESTAMP) {
            return false;
        }

        // 规则4: 简化的时间戳检查
        if (version->create_ts <= snapshot_time) {
            return true;
        }

        return false;
    }
};

/**
 * @brief 快照管理器
 */
class SnapshotManager {
private:
    std::mutex mutex_;
    std::unordered_set<txn_id_t> active_transactions_;
    
public:
    /**
     * @brief 为事务创建快照
     * @param snapshot_time 快照时间戳
     * @return 快照对象
     */
    std::unique_ptr<Snapshot> create_snapshot(timestamp_t snapshot_time) {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::make_unique<Snapshot>(snapshot_time, active_transactions_);
    }
    
    /**
     * @brief 注册活跃事务
     * @param txn_id 事务ID
     */
    void register_transaction(txn_id_t txn_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.insert(txn_id);
    }
    
    /**
     * @brief 注销事务（提交或回滚时）
     * @param txn_id 事务ID
     */
    void unregister_transaction(txn_id_t txn_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_transactions_.erase(txn_id);
    }
    
    /**
     * @brief 获取当前活跃事务数量
     */
    size_t get_active_count() {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_transactions_.size();
    }
};
