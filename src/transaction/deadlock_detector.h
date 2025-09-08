// NOTE: Unused component marker
// 当前扫描未发现外部引用，标注为未使用，保留以备后续使用/参考；不移除构建，不影响行为。

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
#include <unordered_set>
#include <vector>
#include <mutex>
#include "common/config.h"
#include "txn_defs.h"

/**
 * @brief 简单的死锁检测器
 * 使用等待图(wait-for graph)检测死锁
 */
class DeadlockDetector {
private:
    std::mutex mutex_;
    // 等待图：txn_id -> 等待的事务ID集合
    std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> wait_for_graph_;

public:
    /**
     * @brief 添加等待关系
     * @param waiter 等待的事务ID
     * @param holder 持有锁的事务ID
     */
    void add_wait_edge(txn_id_t waiter, txn_id_t holder) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (waiter != holder) {
            wait_for_graph_[waiter].insert(holder);
        }
    }

    /**
     * @brief 移除等待关系
     * @param waiter 等待的事务ID
     * @param holder 持有锁的事务ID
     */
    void remove_wait_edge(txn_id_t waiter, txn_id_t holder) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = wait_for_graph_.find(waiter);
        if (it != wait_for_graph_.end()) {
            it->second.erase(holder);
            if (it->second.empty()) {
                wait_for_graph_.erase(it);
            }
        }
    }

    /**
     * @brief 移除事务的所有等待关系
     * @param txn_id 事务ID
     */
    void remove_transaction(txn_id_t txn_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 移除该事务作为等待者的关系
        wait_for_graph_.erase(txn_id);

        // 移除该事务作为被等待者的关系
        for (auto& [waiter, holders] : wait_for_graph_) {
            holders.erase(txn_id);
        }

        // 清理空的等待集合
        auto it = wait_for_graph_.begin();
        while (it != wait_for_graph_.end()) {
            if (it->second.empty()) {
                it = wait_for_graph_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief 检测死锁
     * @return 如果检测到死锁，返回参与死锁的事务ID；否则返回INVALID_TXN_ID
     */
    txn_id_t detect_deadlock() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::unordered_set<txn_id_t> visited;
        std::unordered_set<txn_id_t> rec_stack;

        for (const auto& [txn_id, _] : wait_for_graph_) {
            if (visited.find(txn_id) == visited.end()) {
                txn_id_t victim = dfs_detect_cycle(txn_id, visited, rec_stack);
                if (victim != INVALID_TXN_ID) {
                    return victim;
                }
            }
        }

        return INVALID_TXN_ID;
    }

    /**
     * @brief 检查特定事务是否会导致死锁
     * @param txn_id 要检查的事务ID
     * @return 是否会导致死锁
     */
    bool would_cause_deadlock(txn_id_t waiter, txn_id_t holder) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 临时添加边
        wait_for_graph_[waiter].insert(holder);

        // 检测是否有环
        std::unordered_set<txn_id_t> visited;
        std::unordered_set<txn_id_t> rec_stack;
        bool has_cycle = false;

        for (const auto& [txn_id, _] : wait_for_graph_) {
            if (visited.find(txn_id) == visited.end()) {
                if (dfs_detect_cycle(txn_id, visited, rec_stack) != INVALID_TXN_ID) {
                    has_cycle = true;
                    break;
                }
            }
        }

        // 移除临时添加的边
        wait_for_graph_[waiter].erase(holder);
        if (wait_for_graph_[waiter].empty()) {
            wait_for_graph_.erase(waiter);
        }

        return has_cycle;
    }

private:
    /**
     * @brief 使用DFS检测环
     * @param txn_id 当前访问的事务ID
     * @param visited 已访问的事务集合
     * @param rec_stack 递归栈
     * @return 如果检测到环，返回环中的一个事务ID；否则返回INVALID_TXN_ID
     */
    txn_id_t dfs_detect_cycle(txn_id_t txn_id,
                              std::unordered_set<txn_id_t>& visited,
                              std::unordered_set<txn_id_t>& rec_stack) {
        visited.insert(txn_id);
        rec_stack.insert(txn_id);

        auto it = wait_for_graph_.find(txn_id);
        if (it != wait_for_graph_.end()) {
            for (txn_id_t neighbor : it->second) {
                if (visited.find(neighbor) == visited.end()) {
                    txn_id_t result = dfs_detect_cycle(neighbor, visited, rec_stack);
                    if (result != INVALID_TXN_ID) {
                        return result;
                    }
                } else if (rec_stack.find(neighbor) != rec_stack.end()) {
                    // 找到环，返回环中的事务ID（选择ID较小的作为牺牲者）
                    return std::min(txn_id, neighbor);
                }
            }
        }

        rec_stack.erase(txn_id);
        return INVALID_TXN_ID;
    }
};
