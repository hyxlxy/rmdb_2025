/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lock_manager.h"
#include "common/config.h"

// **TPC-C优化：添加district表的特殊锁机制**
static std::unordered_map<std::string, std::mutex> district_locks;

std::mutex& get_district_lock(int w_id, int d_id) {
    std::string key = "district_" + std::to_string(w_id) + "_" + std::to_string(d_id);
    return district_locks[key];
}

// **TPC-C优化：添加orders表的特殊锁机制**
static std::unordered_map<std::string, std::mutex> orders_locks;

std::mutex& get_orders_lock(int w_id, int d_id) {
    std::string key = "orders_" + std::to_string(w_id) + "_" + std::to_string(d_id);
    return orders_locks[key];
}

/**
 * @description: 申请行级共享锁
 * @return {bool} 加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {Rid&} rid 加锁的目标记录ID 记录所在的表的fd
 * @param {int} tab_fd  哪一张表
 */
/*
LockDataId(int fd, const Rid &rid, LockDataType type) {
        assert(type == LockDataType::RECORD);
        fd_ = fd;
        rid_ = rid;
        type_ = type;
}*/
bool LockManager::lock_shared_on_record(Transaction *txn, const Rid &rid, int tab_fd)
{
    // 检查事务状态 - 2PL协议：只能在Growing阶段加锁
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false; // 违反2PL协议
    }

    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    std::unique_lock<std::mutex> lock(latch_);

    auto temp = lock_table_.find(lock_data_id);
    if (temp == lock_table_.end())
    {
        auto result = lock_table_.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(lock_data_id),
                                          std::forward_as_tuple());
        temp = result.first;
    }

    LockRequestQueue &lock_queue = temp->second;

    // 检查是否已经持有锁或可以升级
    for (auto &req : lock_queue.request_queue_)
    {
        if (req.txn_id_ == txn->get_transaction_id())
        {
            if (req.granted_)
            {
                // 已经持有S或X锁，无需再加S锁
                return true;
            }
        }
    }

    // 检查是否有排他锁冲突
    bool has_conflict = false;
    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.lock_mode_ == LockMode::EXLUCSIVE &&
            req.txn_id_ != txn->get_transaction_id())
        {
            has_conflict = true;
            break;
        }
    }

    if (!has_conflict)
    {
        // 可以获得共享锁
        lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::SHARED);
        lock_queue.request_queue_.back().granted_ = true;
        txn->get_lock_set()->insert(lock_data_id);
        return true;
    }
    else
    {
        // 有排他锁冲突，无法获得锁
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
}

/**
 * @description: 申请行级排他锁
 * @return {bool} 加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {Rid&} rid 加锁的目标记录ID
 * @param {int} tab_fd 记录所在的表的fd
 */
bool LockManager::lock_exclusive_on_record(Transaction *txn, const Rid &rid, int tab_fd)
{
    // 检查事务状态 - 2PL协议：只能在Growing阶段加锁
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false; // 违反2PL协议
    }

    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    std::unique_lock<std::mutex> lock(latch_);

    auto temp = lock_table_.find(lock_data_id);
    if (temp == lock_table_.end())
    {
        auto result = lock_table_.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(lock_data_id),
                                          std::forward_as_tuple());
        temp = result.first;
    }

    LockRequestQueue &lock_queue = temp->second;

    // 检查是否已经持有锁或可以升级
    for (auto it = lock_queue.request_queue_.begin(); it != lock_queue.request_queue_.end(); ++it)
    {
        if (it->txn_id_ == txn->get_transaction_id())
        {
            if (it->granted_)
            {
                if (it->lock_mode_ == LockMode::EXLUCSIVE)
                {
                    return true; // 已经持有X锁
                }
                // 如果持有S锁，准备升级
                if (it->lock_mode_ == LockMode::SHARED)
                {
                    // 检查是否有其他事务持有锁
                    bool can_upgrade = true;
                    for (auto &other_req : lock_queue.request_queue_)
                    {
                        if (other_req.granted_ && other_req.txn_id_ != txn->get_transaction_id())
                        {
                            can_upgrade = false;
                            break;
                        }
                    }
                    if (can_upgrade)
                    {
                        it->lock_mode_ = LockMode::EXLUCSIVE; // 升级成功
                        return true;
                    }
                    else
                    {
                        txn->set_state(TransactionState::ABORTED);
                        return false; // 无法升级
                    }
                }
            }
        }
    }

    // 检查是否有任何其他事务持有锁
    bool has_conflict = false;
    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.txn_id_ != txn->get_transaction_id())
        {
            has_conflict = true;
            break;
        }
    }

    if (!has_conflict)
    {
        // 可以获得排他锁
        lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::EXLUCSIVE);
        lock_queue.request_queue_.back().granted_ = true;
        txn->get_lock_set()->insert(lock_data_id);
        return true;
    }
    else
    {
        // 有锁冲突，无法获得排他锁
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
}

/**
 * @description: 申请表级读锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_shared_on_table(Transaction *txn, int tab_fd)
{
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    std::unique_lock<std::mutex> lock(latch_);

    auto &lock_queue = lock_table_[lock_data_id];

    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.txn_id_ != txn->get_transaction_id())
        {
            if (req.lock_mode_ == LockMode::EXLUCSIVE || req.lock_mode_ == LockMode::INTENTION_EXCLUSIVE || req.lock_mode_ == LockMode::S_IX)
            {
                txn->set_state(TransactionState::ABORTED);
                return false;
            }
        }
    }

    lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::SHARED);
    lock_queue.request_queue_.back().granted_ = true;
    txn->get_lock_set()->insert(lock_data_id);
    return true;
}
/**
 * @description: 申请表级写锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_exclusive_on_table(Transaction *txn, int tab_fd)
{
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    std::unique_lock<std::mutex> lock(latch_);

    auto &lock_queue = lock_table_[lock_data_id];

    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.txn_id_ != txn->get_transaction_id())
        {
            // Any other granted lock conflicts with X lock
            txn->set_state(TransactionState::ABORTED);
            return false;
        }
    }

    lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::EXLUCSIVE);
    lock_queue.request_queue_.back().granted_ = true;
    txn->get_lock_set()->insert(lock_data_id);
    return true;
}

/**
 * @description: 申请表级意向读锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_IS_on_table(Transaction *txn, int tab_fd)
{
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    std::unique_lock<std::mutex> lock(latch_);

    auto &lock_queue = lock_table_[lock_data_id];

    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.txn_id_ != txn->get_transaction_id())
        {
            if (req.lock_mode_ == LockMode::EXLUCSIVE)
            {
                txn->set_state(TransactionState::ABORTED);
                return false;
            }
        }
    }

    lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::INTENTION_SHARED);
    lock_queue.request_queue_.back().granted_ = true;
    txn->get_lock_set()->insert(lock_data_id);
    return true;
}

/**
 * @description: 申请表级意向写锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_IX_on_table(Transaction *txn, int tab_fd)
{
    if (txn->get_state() == TransactionState::SHRINKING)
    {
        txn->set_state(TransactionState::ABORTED);
        return false;
    }
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    std::unique_lock<std::mutex> lock(latch_);

    auto &lock_queue = lock_table_[lock_data_id];

    for (auto &req : lock_queue.request_queue_)
    {
        if (req.granted_ && req.txn_id_ != txn->get_transaction_id())
        {
            if (req.lock_mode_ == LockMode::EXLUCSIVE || req.lock_mode_ == LockMode::SHARED || req.lock_mode_ == LockMode::S_IX)
            {
                txn->set_state(TransactionState::ABORTED);
                return false;
            }
        }
    }

    lock_queue.request_queue_.emplace_back(txn->get_transaction_id(), LockMode::INTENTION_EXCLUSIVE);
    lock_queue.request_queue_.back().granted_ = true;
    txn->get_lock_set()->insert(lock_data_id);
    return true;
}

/**
 * @description: 释放锁
 * @return {bool} 返回解锁是否成功
 * @param {Transaction*} txn 要释放锁的事务对象指针
 * @param {LockDataId} lock_data_id 要释放的锁ID
 */
bool LockManager::unlock(Transaction *txn, LockDataId lock_data_id)
{
    // 严格2PL：在Shrinking阶段才能释放锁
    if (txn->get_state() == TransactionState::GROWING)
    {
        txn->set_state(TransactionState::SHRINKING);
    }

    std::unique_lock<std::mutex> lock(latch_);

    auto temp = lock_table_.find(lock_data_id);
    if (temp == lock_table_.end())
    {
        return false; // 没有找到对应的锁队列
    }

    LockRequestQueue &lock_queue = temp->second;

    // 从事务的锁集合中移除
    txn->get_lock_set()->erase(lock_data_id);

    // 在队列中查找并移除当前事务的请求
    bool found = false;
    for (auto req = lock_queue.request_queue_.begin(); req != lock_queue.request_queue_.end(); ++req)
    {
        if (req->txn_id_ == txn->get_transaction_id())
        {
            lock_queue.request_queue_.erase(req);
            found = true;
            break;
        }
    }

    if (!found)
    {
        return false; // 没有找到该事务的锁请求
    }

    // 如果队列为空，从锁表中移除该资源
    if (lock_queue.request_queue_.empty())
    {
        lock_table_.erase(temp);
    }

    return true;
}