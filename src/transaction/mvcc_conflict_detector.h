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

#include "mvcc_defs.h"
#include "transaction.h"
#include "transaction_manager.h"

/**
 * @brief 写入冲突类型
 */
enum class ConflictType
{
    NO_CONFLICT,          // 无冲突
    UNCOMMITTED_CONFLICT, // 与未提交事务冲突
    TIMESTAMP_CONFLICT    // 时间戳冲突
};

/**
 * @brief 冲突检测结果
 */
struct ConflictResult
{
    ConflictType type;
    txn_id_t conflicting_txn_id;
    std::string description;

    ConflictResult(ConflictType t = ConflictType::NO_CONFLICT,
                   txn_id_t txn_id = INVALID_TXN_ID,
                   const std::string &desc = "")
        : type(t), conflicting_txn_id(txn_id), description(desc) {}

    bool has_conflict() const { return type != ConflictType::NO_CONFLICT; }
};

/**
 * @brief MVCC写入冲突检测器
 * 实现题目要求的两种写入冲突检测：
 * 1. 事务A尝试更新一条元组时，发现该元组的最新时间戳属于另一个未提交的事务B
 * 2. 事务A尝试更新一条元组时，发现该元组的最新时间戳属于另一个已提交的事务B，且该事务B的提交时间戳大于事务A的读时间戳
 */
class MVCCConflictDetector
{
private:
    TransactionManager *txn_manager_;

public:
    explicit MVCCConflictDetector(TransactionManager *txn_manager)
        : txn_manager_(txn_manager) {}

    /**
     * @brief 检测更新操作的写入冲突
     * @param version 要更新的版本
     * @param txn 当前事务
     * @return 冲突检测结果
     */
    ConflictResult detect_update_conflict(const TupleVersion *version, Transaction *txn)
    {
        if (version == nullptr || txn == nullptr)
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Invalid parameters");
        }

        // 如果是当前事务创建的版本，没有冲突
        if (version->txn_id == txn->get_transaction_id())
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Self-created version");
        }

        // 冲突情况1: 版本属于另一个未提交的事务
        if (version->create_ts == INVALID_TIMESTAMP)
        {
            Transaction *creator_txn = txn_manager_->get_transaction(version->txn_id);
            if (creator_txn != nullptr && creator_txn->is_active())
            {
                return ConflictResult(
                    ConflictType::UNCOMMITTED_CONFLICT,
                    version->txn_id,
                    "Conflict with uncommitted transaction " + std::to_string(version->txn_id));
            }
            // 如果事务不存在或不活跃，可能已经被回滚，不算冲突
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Creator transaction not active");
        }

        // 检查是否为回滚版本
        if (version->create_ts == INT32_MAX)
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Rolled back version");
        }

        // 冲突情况2: 版本在当前事务读取时间戳之后提交
        if (version->create_ts != INVALID_TIMESTAMP && version->create_ts > txn->get_read_ts())
        {
            return ConflictResult(
                ConflictType::TIMESTAMP_CONFLICT,
                version->txn_id,
                "Conflict with transaction " + std::to_string(version->txn_id) +
                    " committed at " + std::to_string(version->create_ts) +
                    " after read timestamp " + std::to_string(txn->get_read_ts()));
        }

        return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "No conflict detected");
    }

    /**
     * @brief 检测删除操作的写入冲突
     * @param version 要删除的版本
     * @param txn 当前事务
     * @return 冲突检测结果
     */
    ConflictResult detect_delete_conflict(const TupleVersion *version, Transaction *txn)
    {
        // **关键修复：删除操作需要更严格的冲突检测**
        if (version == nullptr || txn == nullptr)
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Invalid parameters");
        }

        // 如果是当前事务创建的版本，检查是否已被删除
        if (version->txn_id == txn->get_transaction_id())
        {
            if (version->is_deleted)
            {
                // **特殊情况：尝试删除已被当前事务删除的记录，这可能是读写冲突场景**
                return ConflictResult(
                    ConflictType::UNCOMMITTED_CONFLICT,
                    version->txn_id,
                    "Attempting to delete already deleted record in same transaction");
            }
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Self-created version");
        }

        // 对于其他情况，使用标准的更新冲突检测
        return detect_update_conflict(version, txn);
    }

    /**
     * @brief 检测版本链中的写入冲突
     * @param chain 版本链
     * @param txn 当前事务
     * @return 冲突检测结果
     */
    ConflictResult detect_chain_conflict(VersionChain *chain, Transaction *txn)
    {
        if (chain == nullptr || txn == nullptr)
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Invalid parameters");
        }

        TupleVersion *head = chain->get_head();
        if (head == nullptr)
        {
            return ConflictResult(ConflictType::NO_CONFLICT, INVALID_TXN_ID, "Empty version chain");
        }

        return detect_update_conflict(head, txn);
    }

    /**
     * @brief 检查事务是否可以安全地进行写入操作
     * @param version 目标版本
     * @param txn 当前事务
     * @return 是否可以安全写入
     */
    bool can_write_safely(const TupleVersion *version, Transaction *txn)
    {
        ConflictResult result = detect_update_conflict(version, txn);
        return !result.has_conflict();
    }

    /**
     * @brief 获取冲突类型的描述字符串
     * @param type 冲突类型
     * @return 描述字符串
     */
    std::string get_conflict_type_description(ConflictType type)
    {
        switch (type)
        {
        case ConflictType::NO_CONFLICT:
            return "No conflict";
        case ConflictType::UNCOMMITTED_CONFLICT:
            return "Conflict with uncommitted transaction";
        case ConflictType::TIMESTAMP_CONFLICT:
            return "Timestamp conflict with committed transaction";
        default:
            return "Unknown conflict type";
        }
    }

    /**
     * @brief 检测并处理写入冲突
     * @param version 目标版本
     * @param txn 当前事务
     * @param operation_name 操作名称（用于日志）
     * @return 是否可以继续操作
     */
    bool check_and_handle_conflict(const TupleVersion *version, Transaction *txn,
                                   const std::string &operation_name = "write")
    {
        ConflictResult result = detect_update_conflict(version, txn);

        if (result.has_conflict())
        {
            throw TransactionAbortException(txn->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
        }

        return true;
    }

    /**
     * @brief 验证事务的写入操作序列是否满足MVCC要求
     * @param txn 事务
     * @return 验证结果
     */
    bool validate_transaction_writes(Transaction *txn)
    {
        if (txn == nullptr)
            return false;

        // 检查事务的时间戳设置是否正确
        if (txn->get_read_ts() == INVALID_TIMESTAMP)
        {
            return false;
        }

        // 如果事务已提交，检查提交时间戳
        if (txn->is_committed() && txn->get_commit_ts() == INVALID_TIMESTAMP)
        {
            return false;
        }

        // 检查提交时间戳是否大于读取时间戳
        if (txn->is_committed() && txn->get_commit_ts() <= txn->get_read_ts())
        {
            return false;
        }

        return true;
    }

    /**
     * @brief 获取详细的冲突分析报告
     * @param version 版本
     * @param txn 事务
     * @return 分析报告
     */
    std::string get_conflict_analysis(const TupleVersion *version, Transaction *txn)
    {
        if (version == nullptr || txn == nullptr)
        {
            return "Invalid parameters for conflict analysis";
        }

        std::string report = "Conflict Analysis:\n";
        report += "  Current Transaction ID: " + std::to_string(txn->get_transaction_id()) + "\n";
        report += "  Current Transaction Read TS: " + std::to_string(txn->get_read_ts()) + "\n";
        report += "  Version Creator Transaction ID: " + std::to_string(version->txn_id) + "\n";
        report += "  Version Create TS: " + std::to_string(version->create_ts) + "\n";
        report += "  Version Expire TS: " + std::to_string(version->expire_ts) + "\n";
        report += "  Version Deleted: " + std::string(version->is_deleted ? "true" : "false") + "\n";

        ConflictResult result = detect_update_conflict(version, txn);
        report += "  Conflict Result: " + get_conflict_type_description(result.type) + "\n";
        if (result.has_conflict())
        {
            report += "  Conflict Description: " + result.description + "\n";
        }

        return report;
    }
};
