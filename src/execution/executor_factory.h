
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

#include "common/config.h"
#include "executor_abstract.h"

// 普通执行器
#include "executor_seq_scan.h"
#include "executor_insert.h"
#include "executor_update.h"
#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"

// MVCC执行器
#include "mvcc_executor_seq_scan.h"
#include "mvcc_executor_insert.h"
#include "mvcc_executor_update.h"
#include "mvcc_executor_delete.h"

/**
 * @brief 执行器工厂类
 * 根据MVCC配置选择使用普通执行器还是MVCC执行器
 */
class ExecutorFactory {
public:
    /**
     * @brief 创建顺序扫描执行器
     */
    static std::unique_ptr<AbstractExecutor> create_seq_scan_executor(
        SmManager* sm_manager,
        const std::string& tab_name,
        std::vector<Condition> conds,
        Context* context) {

        if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
            // 使用MVCC版本
            return std::make_unique<MVCCSeqScanExecutor>(sm_manager, tab_name, std::move(conds), context);
        } else {
            // 使用普通版本
            return std::make_unique<SeqScanExecutor>(sm_manager, tab_name, std::move(conds), context);
        }
    }

    /**
     * @brief 创建插入执行器
     */
    static std::unique_ptr<AbstractExecutor> create_insert_executor(
        SmManager* sm_manager,
        const std::string& tab_name,
        std::vector<Value> values,
        Context* context) {

        if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
            // 使用MVCC版本
            return std::make_unique<MVCCInsertExecutor>(sm_manager, tab_name, std::move(values), context);
        } else {
            // 使用普通版本
            return std::make_unique<InsertExecutor>(sm_manager, tab_name, std::move(values), context);
        }
    }

    /**
     * @brief 创建更新执行器
     */
    static std::unique_ptr<AbstractExecutor> create_update_executor(
        SmManager* sm_manager,
        const std::string& tab_name,
        std::vector<SetClause> set_clauses,
        std::vector<Condition> conds,
        std::vector<Rid> rids,
        Context* context) {

        if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
            // 使用MVCC版本
            return std::make_unique<MVCCUpdateExecutor>(sm_manager, tab_name, std::move(set_clauses), std::move(conds), std::move(rids), context);
        } else {
            // 使用普通版本
            return std::make_unique<UpdateExecutor>(sm_manager, tab_name, std::move(set_clauses), std::move(conds), std::move(rids), context);
        }
    }

    /**
     * @brief 创建删除执行器
     */
    static std::unique_ptr<AbstractExecutor> create_delete_executor(
        SmManager* sm_manager,
        const std::string& tab_name,
        std::vector<Condition> conds,
        std::vector<Rid> rids,
        Context* context) {

        if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
            // 使用MVCC版本
            return std::make_unique<MVCCDeleteExecutor>(sm_manager, tab_name, std::move(conds), std::move(rids), context);
        } else {
            // 使用普通版本
            return std::make_unique<DeleteExecutor>(sm_manager, tab_name, std::move(conds), std::move(rids), context);
        }
    }

    /**
     * @brief 创建索引扫描执行器（暂时只有普通版本）
     */
    static std::unique_ptr<AbstractExecutor> create_index_scan_executor(
        SmManager* sm_manager,
        const std::string& tab_name,
        std::vector<Condition> conds,
        std::vector<std::string> index_col_names,
        Context* context) {

        // 索引扫描暂时只使用普通版本
        return std::make_unique<IndexScanExecutor>(sm_manager, tab_name, std::move(conds), std::move(index_col_names), context);
    }

    /**
     * @brief 创建嵌套循环连接执行器（暂时只有普通版本）
     */
    static std::unique_ptr<AbstractExecutor> create_nestedloop_join_executor(
        std::unique_ptr<AbstractExecutor> left,
        std::unique_ptr<AbstractExecutor> right,
        std::vector<Condition> conds) {

        // 连接操作暂时只使用普通版本
        return std::make_unique<NestedLoopJoinExecutor>(std::move(left), std::move(right), std::move(conds));
    }

    /**
     * @brief 创建投影执行器（暂时只有普通版本）
     */
    static std::unique_ptr<AbstractExecutor> create_projection_executor(
        std::unique_ptr<AbstractExecutor> prev,
        std::vector<TabCol> sel_cols) {

        // 投影操作暂时只使用普通版本
        return std::make_unique<ProjectionExecutor>(std::move(prev), std::move(sel_cols));
    }

    /**
     * @brief 检查是否应该使用MVCC执行器
     */
    static bool should_use_mvcc(Context* context) {
        return ENABLE_MVCC && context != nullptr && context->txn_ != nullptr;
    }
};
