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

#include <cerrno>
#include <cstring>
#include <string>
#include "optimizer/plan.h"
#include "execution/executor_abstract.h"
#include "execution/executor_nestedloop_join.h"
#include "execution/mvcc_executor_nestedloop_join.h"
#include "execution/executor_projection.h"
#include "execution/executor_seq_scan.h"
#include "execution/executor_index_scan.h"
#include "execution/executor_update.h"
#include "execution/executor_insert.h"
#include "execution/executor_delete.h"
#include "execution/mvcc_executor_update.h"
#include "execution/mvcc_executor_seq_scan.h"
#include "execution/mvcc_executor_insert.h"
#include "execution/mvcc_executor_delete.h"
#include "execution/execution_sort.h"
#include "execution/executor_aggregation_optimized.h"
#include "execution/executor_group.h"
#include "common/common.h"
#include "system/sm_manager.h"
#include "execution/executor_load.h"
#include "common/config.h"

typedef enum portalTag
{
    PORTAL_Invalid_Query = 0,
    PORTAL_ONE_SELECT,
    PORTAL_DML_WITHOUT_SELECT,
    PORTAL_MULTI_QUERY,
    PORTAL_CMD_UTILITY,
    PORTAL_LOAD
} portalTag;

struct PortalStmt
{
    portalTag tag;
    std::vector<TabCol> sel_cols;
    std::unique_ptr<AbstractExecutor> root;
    std::shared_ptr<Plan> plan;

    PortalStmt(portalTag tag_, std::vector<TabCol> sel_cols_, std::unique_ptr<AbstractExecutor> root_, std::shared_ptr<Plan> plan_)
        : tag(tag_), sel_cols(std::move(sel_cols_)), root(std::move(root_)), plan(std::move(plan_)) {}
};

class Portal
{
private:
    SmManager *sm_manager_;

public:
    Portal(SmManager *sm_manager) : sm_manager_(sm_manager) {}
    ~Portal() {}

    // 将查询执行计划转换成对应的算子树
    std::shared_ptr<PortalStmt> start(std::shared_ptr<Plan> plan, Context *context)
    {
        // 这里可以将select进行拆分，例如：一个select，带有return的select等
        if (auto x = std::dynamic_pointer_cast<OtherPlan>(plan))
        {
            return std::make_shared<PortalStmt>(PORTAL_CMD_UTILITY, std::vector<TabCol>(), std::unique_ptr<AbstractExecutor>(), plan);
        }
        else if (auto x = std::dynamic_pointer_cast<SetKnobPlan>(plan))
        {
            return std::make_shared<PortalStmt>(PORTAL_CMD_UTILITY, std::vector<TabCol>(), std::unique_ptr<AbstractExecutor>(), plan);
        }
        else if (auto x = std::dynamic_pointer_cast<DDLPlan>(plan))
        {
            return std::make_shared<PortalStmt>(PORTAL_MULTI_QUERY, std::vector<TabCol>(), std::unique_ptr<AbstractExecutor>(), plan);
        }
        else if (auto x = std::dynamic_pointer_cast<LoadPlan>(plan))
        {
            return std::make_shared<PortalStmt>(PORTAL_LOAD, std::vector<TabCol>(), std::unique_ptr<AbstractExecutor>(), plan);
        }
        else if (auto x = std::dynamic_pointer_cast<DMLPlan>(plan))
        {
            switch (x->tag)
            {
            case T_select:
            {
                std::shared_ptr<ProjectionPlan> p = std::dynamic_pointer_cast<ProjectionPlan>(x->subplan_);
                if (p)
                {
                    // 有ProjectionPlan的情况
                    std::unique_ptr<AbstractExecutor> root = convert_plan_executor(p, context);
                    return std::make_shared<PortalStmt>(PORTAL_ONE_SELECT, std::move(p->sel_cols_), std::move(root), plan);
                }
                else
                {
                    // 没有ProjectionPlan的情况（如聚合查询）
                    std::unique_ptr<AbstractExecutor> root = convert_plan_executor(x->subplan_, context);
                    // 对于聚合查询，使用空的sel_cols，因为聚合执行器已经处理了列选择
                    return std::make_shared<PortalStmt>(PORTAL_ONE_SELECT, std::vector<TabCol>(), std::move(root), plan);
                }
            }

            case T_Update:
            {
                std::unique_ptr<AbstractExecutor> scan = convert_plan_executor(x->subplan_, context);
                std::vector<Rid> rids;
                for (scan->beginTuple(); !scan->is_end(); scan->nextTuple())
                {
                    rids.push_back(scan->rid());
                }

                // **简化选择逻辑：只在MVCC启用时使用MVCC执行器**
                std::unique_ptr<AbstractExecutor> root;
                if (ENABLE_MVCC) {
                    root = std::make_unique<MVCCUpdateExecutor>(sm_manager_, x->tab_name_, x->set_clauses_, x->conds_, rids, context);
                } else {
                    root = std::make_unique<UpdateExecutor>(sm_manager_, x->tab_name_, x->set_clauses_, x->conds_, rids, context);
                }
                return std::make_shared<PortalStmt>(PORTAL_DML_WITHOUT_SELECT, std::vector<TabCol>(), std::move(root), plan);
            }
            case T_Delete:
            {
                std::unique_ptr<AbstractExecutor> scan = convert_plan_executor(x->subplan_, context);
                std::vector<Rid> rids;
                for (scan->beginTuple(); !scan->is_end(); scan->nextTuple())
                {
                    rids.push_back(scan->rid());
                }

                // **简化选择逻辑：只在MVCC启用时使用MVCC执行器**
                std::unique_ptr<AbstractExecutor> root;
                if (ENABLE_MVCC) {
                    root = std::make_unique<MVCCDeleteExecutor>(sm_manager_, x->tab_name_, x->conds_, rids, context);
                } else {
                    root = std::make_unique<DeleteExecutor>(sm_manager_, x->tab_name_, x->conds_, rids, context);
                }

                return std::make_shared<PortalStmt>(PORTAL_DML_WITHOUT_SELECT, std::vector<TabCol>(), std::move(root), plan);
            }

            case T_Insert:
            {
                // **简化选择逻辑：只在MVCC启用时使用MVCC执行器**
                std::unique_ptr<AbstractExecutor> root;
                if (ENABLE_MVCC) {
                    root = std::make_unique<MVCCInsertExecutor>(sm_manager_, x->tab_name_, x->values_, context);
                } else {
                    root = std::make_unique<InsertExecutor>(sm_manager_, x->tab_name_, x->values_, context);
                }

                return std::make_shared<PortalStmt>(PORTAL_DML_WITHOUT_SELECT, std::vector<TabCol>(), std::move(root), plan);
            }
            default:
                throw InternalError("Unexpected field type");
                break;
            }
        }
        else
        {
            throw InternalError("Unexpected field type");
        }
        return nullptr;
    }

    // 遍历算子树并执行算子生成执行结果
    void run(std::shared_ptr<PortalStmt> portal, QlManager *ql, txn_id_t *txn_id, Context *context)
    {
        switch (portal->tag)
        {
        case PORTAL_ONE_SELECT:
        {
            ql->select_from(std::move(portal->root), std::move(portal->sel_cols), context);
            break;
        }

        case PORTAL_DML_WITHOUT_SELECT:
        {
            ql->run_dml(std::move(portal->root));
            break;
        }
        case PORTAL_MULTI_QUERY:
        {
            ql->run_mutli_query(portal->plan, context);
            break;
        }
        case PORTAL_CMD_UTILITY:
        {
            ql->run_cmd_utility(portal->plan, txn_id, context);
            break;
        }
        case PORTAL_LOAD:
        {
            ql->run_load(portal->plan, context);
            break;
        }
        default:
        {
            throw InternalError("Unexpected field type");
        }
        }
    }

    // 清空资源
    void drop() {}

    std::unique_ptr<AbstractExecutor>
    convert_plan_executor(std::shared_ptr<Plan> plan, Context *context)
    {
        if (auto x = std::dynamic_pointer_cast<ProjectionPlan>(plan))
        {
            return std::make_unique<ProjectionExecutor>(convert_plan_executor(x->subplan_, context),
                                                        x->sel_cols_);
        }
        else if (auto x = std::dynamic_pointer_cast<ScanPlan>(plan))
        {
            if (x->tag == T_SeqScan)
            {
                // **修复：只在有事务上下文时才使用MVCC执行器**
                if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
                    return std::make_unique<MVCCSeqScanExecutor>(sm_manager_, x->tab_name_, x->conds_, context);
                } else {
                    return std::make_unique<SeqScanExecutor>(sm_manager_, x->tab_name_, x->conds_, context);
                }
            }
            else
            {
                return std::make_unique<IndexScanExecutor>(sm_manager_, x->tab_name_, x->conds_, x->index_col_names_, context);
            }
        }
        else if (auto x = std::dynamic_pointer_cast<JoinPlan>(plan))
        {
            std::unique_ptr<AbstractExecutor> left = convert_plan_executor(x->left_, context);
            std::unique_ptr<AbstractExecutor> right = convert_plan_executor(x->right_, context);

            // **修复：在MVCC模式下使用MVCC版本的连接执行器**
            if (ENABLE_MVCC && context != nullptr && context->txn_ != nullptr) {
                std::unique_ptr<AbstractExecutor> join = std::make_unique<MVCCNestedLoopJoinExecutor>(
                    std::move(left), std::move(right), std::move(x->conds_), x->type, context, sm_manager_);
                return join;
            } else {
                std::unique_ptr<AbstractExecutor> join = std::make_unique<NestedLoopJoinExecutor>(
                    std::move(left), std::move(right), std::move(x->conds_), x->type);
                return join;
            }
        }
        else if (auto x = std::dynamic_pointer_cast<SortPlan>(plan))
        {
            return std::make_unique<SortExecutor>(convert_plan_executor(x->subplan_, context),
                                                  x->sel_cols_, x->is_desc_, x->limit_);
        }
        else if (auto x = std::dynamic_pointer_cast<AggPlan>(plan))
        {
            std::unique_ptr<AbstractExecutor> child = convert_plan_executor(x->input_, context);
            return std::make_unique<OptimizedAggregationExecutor>(std::move(child), x->select_exprs_);
        }
        else if (auto x = std::dynamic_pointer_cast<GroupPlan>(plan))
        {
            std::unique_ptr<AbstractExecutor> child = convert_plan_executor(x->input_, context);
            return std::make_unique<GroupExecutor>(std::move(child), x->group_by_, x->having_);
        }
        return nullptr;
    }
};
