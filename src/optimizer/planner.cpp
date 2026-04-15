/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "planner.h"
#include "logical_optimizer.h"
#include "tpcc_optimizer.h"
#include "../common/tpcc_config.h"

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "execution/executor_delete.h"
#include "execution/executor_index_scan.h"
#include "execution/executor_insert.h"
#include "execution/executor_nestedloop_join.h"
#include "execution/executor_projection.h"
#include "execution/executor_seq_scan.h"
#include "execution/executor_update.h"
#include "index/ix.h"
#include "record_printer.h"

namespace {

std::string make_col_key(const TabCol &col)
{
    return col.tab_name + "." + col.col_name;
}

bool same_column(const TabCol &lhs, const TabCol &rhs)
{
    return lhs.tab_name == rhs.tab_name && lhs.col_name == rhs.col_name;
}

bool same_value(const Value &lhs, const Value &rhs)
{
    if (lhs.type != rhs.type)
    {
        return false;
    }
    switch (lhs.type)
    {
    case TYPE_INT:
        return lhs.int_val == rhs.int_val;
    case TYPE_FLOAT:
        return lhs.float_val == rhs.float_val;
    case TYPE_STRING:
        return lhs.str_val == rhs.str_val;
    default:
        return false;
    }
}

bool same_condition(const Condition &lhs, const Condition &rhs)
{
    if (!same_column(lhs.lhs_col, rhs.lhs_col) || lhs.op != rhs.op || lhs.is_rhs_val != rhs.is_rhs_val)
    {
        return false;
    }
    if (lhs.is_rhs_val)
    {
        return same_value(lhs.rhs_val, rhs.rhs_val);
    }
    return same_column(lhs.rhs_col, rhs.rhs_col);
}

void propagate_equality_constants_in_place(std::shared_ptr<Query> query)
{
    if (!query)
    {
        return;
    }

    std::unordered_map<std::string, TabCol> key_to_col;
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    std::unordered_map<std::string, std::vector<Value>> constants_by_col;

    for (const auto &condition : query->conds)
    {
        if (condition.op != OP_EQ)
        {
            continue;
        }

        const std::string lhs_key = make_col_key(condition.lhs_col);
        key_to_col[lhs_key] = condition.lhs_col;

        if (condition.is_rhs_val)
        {
            constants_by_col[lhs_key].push_back(condition.rhs_val);
            continue;
        }

        const std::string rhs_key = make_col_key(condition.rhs_col);
        key_to_col[rhs_key] = condition.rhs_col;
        adjacency[lhs_key].push_back(rhs_key);
        adjacency[rhs_key].push_back(lhs_key);
    }

    std::unordered_set<std::string> visited;
    std::vector<Condition> propagated;

    for (const auto &[start_key, start_col] : key_to_col)
    {
        if (visited.count(start_key) > 0)
        {
            continue;
        }

        std::queue<std::string> pending;
        std::vector<std::string> component;
        std::vector<Value> component_constants;
        pending.push(start_key);
        visited.insert(start_key);

        while (!pending.empty())
        {
            std::string current = pending.front();
            pending.pop();
            component.push_back(current);

            auto const_it = constants_by_col.find(current);
            if (const_it != constants_by_col.end())
            {
                for (const auto &value : const_it->second)
                {
                    bool exists = std::any_of(component_constants.begin(), component_constants.end(),
                                              [&](const Value &existing) { return same_value(existing, value); });
                    if (!exists)
                    {
                        component_constants.push_back(value);
                    }
                }
            }

            auto adj_it = adjacency.find(current);
            if (adj_it == adjacency.end())
            {
                continue;
            }

            for (const auto &next : adj_it->second)
            {
                if (visited.insert(next).second)
                {
                    pending.push(next);
                }
            }
        }

        if (component_constants.empty())
        {
            continue;
        }

        for (const auto &col_key : component)
        {
            const TabCol &target_col = key_to_col.at(col_key);
            for (const auto &value : component_constants)
            {
                Condition propagated_cond;
                propagated_cond.lhs_col = target_col;
                propagated_cond.op = OP_EQ;
                propagated_cond.is_rhs_val = true;
                propagated_cond.rhs_val = value;

                bool already_exists = std::any_of(query->conds.begin(), query->conds.end(),
                                                  [&](const Condition &existing) {
                                                      return same_condition(existing, propagated_cond);
                                                  }) ||
                                      std::any_of(propagated.begin(), propagated.end(),
                                                  [&](const Condition &existing) {
                                                      return same_condition(existing, propagated_cond);
                                                  });
                if (!already_exists)
                {
                    propagated.push_back(propagated_cond);
                }
            }
        }
    }

    query->conds.insert(query->conds.end(), propagated.begin(), propagated.end());
}

} // namespace

// 判断是否需要聚合算子
static bool need_agg_plan(const std::shared_ptr<Query> &query)
{
    // 1. group by 非空则需要聚合
    if (!query->group_by.empty())
        return true;

    // 2. select_exprs 中有聚合表达式也需要聚合
    for (const auto &expr : query->select_exprs)
    {
        std::shared_ptr<ast::Expr> e = expr;
        // 处理别名包装
        if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(e))
        {
            e = alias->expr;
        }
        if (std::dynamic_pointer_cast<ast::AggExpr>(e))
        {
            return true;
        }
    }
    return false;
}

// TPCC推荐索引检查函数声明
bool is_tpcc_recommended_index(const std::string& table_name, const std::vector<std::string>& index_columns);

// 优化的索引选择算法：实现最左匹配原则和范围查询支持
bool Planner::get_index_cols(std::string tab_name, std::vector<Condition> curr_conds, std::vector<std::string> &index_col_names)
{
    index_col_names.clear();
    TabMeta &target_table = sm_manager_->db_.get_table(tab_name);

    // **TPCC优化：优先使用TPCC专用优化器**
    #ifdef ENABLE_LEFTMOST_PREFIX_OPTIMIZATION
    std::vector<std::string> tpcc_optimal_index;
    if (TPCCOptimizer::optimize_tpcc_index_selection(tab_name, curr_conds, tpcc_optimal_index)) {
        // 验证推荐的索引是否存在
        for (const auto &index_entry : target_table.indexes) {
            if (static_cast<size_t>(index_entry.second.col_num) == tpcc_optimal_index.size()) {
                bool index_matches = true;
                for (int i = 0; i < index_entry.second.col_num; ++i) {
                    if (index_entry.second.cols[i].name != tpcc_optimal_index[static_cast<size_t>(i)]) {
                        index_matches = false;
                        break;
                    }
                }
                if (index_matches) {
                    index_col_names = tpcc_optimal_index;
                    return true;
                }
            }
        }
    }
    #endif

    // 构建条件映射：使用unordered_map提高查找效率
    std::vector<Condition> applicable_conditions;
    std::unordered_map<std::string, std::vector<Condition *>> column_condition_mapping;

    // 筛选适用于当前表的条件
    for (auto &condition : curr_conds)
    {
        bool is_table_condition = (condition.is_rhs_val && condition.lhs_col.tab_name == tab_name);
        if (is_table_condition)
        {
            applicable_conditions.push_back(condition);
            std::string column_identifier = condition.lhs_col.col_name;
            column_condition_mapping[column_identifier].push_back(&applicable_conditions.back());
        }
    }

    // 如果没有适用的条件，直接返回
    if (applicable_conditions.empty())
        return false;

    // 索引评估和选择
    std::vector<std::string> optimal_index_columns;
    int maximum_match_score = 0;

    // 遍历所有可用索引进行评估
    for (const auto &index_entry : target_table.indexes)
    {
        int current_match_score = 0;
        bool prefix_matching_continues = true;

        // 应用最左匹配原则评估索引
        for (int column_index = 0; column_index < index_entry.second.col_num && prefix_matching_continues; ++column_index)
        {
            const std::string &indexed_column_name = index_entry.second.cols[column_index].name;
            auto condition_iterator = column_condition_mapping.find(indexed_column_name);

            if (condition_iterator != column_condition_mapping.end())
            {
                bool has_equality_condition = false, has_range_condition = false;

                // 分析该列的条件类型
                for (auto *condition_ptr : condition_iterator->second)
                {
                    if (condition_ptr->op == OP_EQ)
                    {
                        has_equality_condition = true;
                    }
                    else if (condition_ptr->op == OP_LT || condition_ptr->op == OP_LE ||
                             condition_ptr->op == OP_GT || condition_ptr->op == OP_GE)
                    {
                        has_range_condition = true;
                    }
                }

                // 根据条件类型更新匹配分数 - 优化权重计算（仅影响计划，不改结果）
                if (has_equality_condition)
                {
                    current_match_score += INDEX_EQ_WEIGHT; // 强化等值权重
                }
                else if (has_range_condition)
                {
                    current_match_score += INDEX_RANGE_WEIGHT; // 范围权重略低
                    prefix_matching_continues = false; // 范围条件终止最左匹配
                }
                else
                {
                    break; // 无有效条件，终止匹配
                }
            }
            else
            {
                break; // 最左前缀中断，停止评估
            }
        }

        // 构建当前索引的完整列名列表
        std::vector<std::string> current_index_column_names;
        for (int col_idx = 0; col_idx < index_entry.second.col_num; ++col_idx)
        {
            current_index_column_names.push_back(index_entry.second.cols[col_idx].name);
        }

        // 更新最优索引选择 - 考虑TPCC特定优化
        bool should_update = false;

        if (current_match_score > maximum_match_score)
        {
            should_update = true;
        }
        else if (current_match_score == maximum_match_score)
        {
            // 分数相同时，优先选择TPCC推荐的索引模式（小幅奖励）
            #ifdef ENABLE_LEFTMOST_PREFIX_OPTIMIZATION
            if (is_tpcc_recommended_index(tab_name, current_index_column_names))
            {
                current_match_score += INDEX_TPCC_BONUS;
                should_update = true;
            }
            #endif
        }

        if (should_update)
        {
            maximum_match_score = current_match_score;
            optimal_index_columns = current_index_column_names;
        }
    }

    // 返回最优索引选择结果
    if (maximum_match_score > 0)
    {
        index_col_names = optimal_index_columns;
        return true;
    }

    return false; // 未找到合适的索引
}

// TPCC推荐索引检查函数
bool is_tpcc_recommended_index(const std::string& table_name, const std::vector<std::string>& index_columns)
{
    if (table_name == "district" && index_columns.size() >= 2 &&
        index_columns[0] == "d_w_id" && index_columns[1] == "d_id") {
        return true;
    }
    if (table_name == "customer" && index_columns.size() >= 3 &&
        index_columns[0] == "c_w_id" && index_columns[1] == "c_d_id" && index_columns[2] == "c_id") {
        return true;
    }
    if (table_name == "stock" && index_columns.size() >= 2 &&
        index_columns[0] == "s_w_id" && index_columns[1] == "s_i_id") {
        return true;
    }
    if (table_name == "orders" && index_columns.size() >= 3 &&
        index_columns[0] == "o_w_id" && index_columns[1] == "o_d_id" && index_columns[2] == "o_id") {
        return true;
    }
    if (table_name == "new_orders" && index_columns.size() >= 3 &&
        index_columns[0] == "no_w_id" && index_columns[1] == "no_d_id" && index_columns[2] == "no_o_id") {
        return true;
    }
    if (table_name == "order_line" && index_columns.size() >= 4 &&
        index_columns[0] == "ol_w_id" && index_columns[1] == "ol_d_id" &&
        index_columns[2] == "ol_o_id" && index_columns[3] == "ol_number") {
        return true;
    }
    return false;
}

/**
 * @brief 表算子条件谓词生成
 *
 * @param conds 条件
 * @param tab_names 表名
 * @return std::vector<Condition>
 */
std::vector<Condition> pop_conds(std::vector<Condition> &conds, std::string tab_names)
{
    // auto has_tab = [&](const std::string &tab_name) {
    //     return std::find(tab_names.begin(), tab_names.end(), tab_name) != tab_names.end();
    // };
    std::vector<Condition> solved_conds;
    auto it = conds.begin();
    while (it != conds.end())
    {
        // 判断
        if ((tab_names.compare(it->lhs_col.tab_name) == 0 && it->is_rhs_val) || (it->lhs_col.tab_name.compare(it->rhs_col.tab_name) == 0))
        {
            solved_conds.emplace_back(std::move(*it));
            it = conds.erase(it);
        }
        else
        {
            it++;
        }
    }
    return solved_conds;
}

int push_conds(Condition *cond, std::shared_ptr<Plan> plan)
{
    if (auto x = std::dynamic_pointer_cast<ScanPlan>(plan)) // 转换指针
    {
        if (x->tab_name_.compare(cond->lhs_col.tab_name) == 0) // 条件左边为表
        {
            return 1;
        }
        else if (x->tab_name_.compare(cond->rhs_col.tab_name) == 0)
        {
            return 2;
        }
        else
        {
            return 0;
        }
    }
    else if (auto x = std::dynamic_pointer_cast<JoinPlan>(plan))
    {
        int left_res = push_conds(cond, x->left_);
        // 条件已经下推到左子节点
        if (left_res == 3)
        {
            return 3;
        }
        int right_res = push_conds(cond, x->right_);
        // 条件已经下推到右子节点
        if (right_res == 3)
        {
            return 3;
        }
        // 左子节点或右子节点有一个没有匹配到条件的列
        if (left_res == 0 || right_res == 0)
        {
            return left_res + right_res;
        }
        // 左子节点匹配到条件的右边
        if (left_res == 2)
        {
            // 需要将左右两边的条件变换位置
            std::map<CompOp, CompOp> swap_op = {
                {OP_EQ, OP_EQ},
                {OP_NE, OP_NE},
                {OP_LT, OP_GT},
                {OP_GT, OP_LT},
                {OP_LE, OP_GE},
                {OP_GE, OP_LE},
            };
            std::swap(cond->lhs_col, cond->rhs_col);
            cond->op = swap_op.at(cond->op);
        }
        x->conds_.emplace_back(std::move(*cond));
        return 3;
    }
    return false;
} // 对条件进行处理.

std::shared_ptr<Plan> pop_scan(int *scantbl, std::string table, std::vector<std::string> &joined_tables,
                               std::vector<std::shared_ptr<Plan>> plans)
{
    for (size_t i = 0; i < plans.size(); i++)
    {
        auto x = std::dynamic_pointer_cast<ScanPlan>(plans[i]);
        if (x->tab_name_.compare(table) == 0)
        {
            scantbl[i] = 1;
            joined_tables.emplace_back(x->tab_name_);
            return plans[i];
        }
    }
    return nullptr;
}

std::shared_ptr<Query> Planner::logical_optimization(std::shared_ptr<Query> query, Context *context)
{
    if (!query)
    {
        return query;
    }

    // 在当前主 planner 路径上做简单、通用的等值常量传播：
    // A = B, B = const => A = const
    // 仅对 OP_EQ 生效，避免范围/不等式传播带来的语义风险。
    propagate_equality_constants_in_place(query);

    return query;
}

std::shared_ptr<Plan> Planner::physical_optimization(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plan = make_one_rel(query);
    
    // 移除聚合处理，这些在 generate_select_plan 中处理
    // 只保留排序处理
    plan = generate_sort_plan(query, std::move(plan));

    return plan;
}

std::shared_ptr<Plan> Planner::make_one_rel(std::shared_ptr<Query> query)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
    std::vector<std::string> tables = query->tables;
    // // Scan table , 生成表算子列表tab_nodes
    std::vector<std::shared_ptr<Plan>> table_scan_executors(tables.size());
    for (size_t i = 0; i < tables.size(); i++)
    {
        auto curr_conds = pop_conds(query->conds, tables[i]);
        // int index_no = get_indexNo(tables[i], curr_conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(tables[i], curr_conds, index_col_names);
        if (index_exist == false)
        { // 该表没有索引
            index_col_names.clear();
            // std::cout << "DEBUG: Using SeqScan for table " << tables[i] << std::endl;
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, tables[i], curr_conds, index_col_names);
        }
        else
        { // 存在索引
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, tables[i], curr_conds, index_col_names);
        }
    }
    // 只有一个表，不需要join。
    // if(tables.size() == 1)
    // {
    //     return table_scan_executors[0];
    // }
    // 获取where条件
    auto conds = std::move(query->conds);
    JoinType join_type = query->is_semi_join_ ? SEMI_JOIN : INNER_JOIN;
    std::shared_ptr<Plan> table_join_executors;
    table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, nullptr, nullptr, std::vector<Condition>(), join_type);

    int scantbl[tables.size()];
    for (size_t i = 0; i < tables.size(); i++)
    {
        scantbl[i] = -1;
    }

    // 假设在ast中已经添加了jointree，这里需要修改的逻辑是，先处理jointree，然后再考虑剩下的部分
    if (conds.size() >= 1)
    {
        // 有连接条件

        // 根据连接条件，生成第一层join
        std::vector<std::string> joined_tables(tables.size());
        auto it = conds.begin();
        while (it != conds.end())
        {
            std::shared_ptr<Plan> left, right;
            left = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
            right = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
            std::vector<Condition> join_conds{*it};
            // 建立join
            //  判断使用哪种join方式
            if (enable_nestedloop_join && enable_sortmerge_join)
            {
                // 默认nested loop join
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds, join_type);
            }
            else if (enable_nestedloop_join)
            {
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds, join_type);
            }
            else if (enable_sortmerge_join)
            {
                table_join_executors = std::make_shared<JoinPlan>(T_SortMerge, std::move(left), std::move(right), join_conds, join_type);
            }
            else
            {
                // error
                throw RMDBError("No join executor selected!");
            }

            // table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds);
            it = conds.erase(it);
            break;
        }
        // 根据连接条件，生成第2-n层join
        it = conds.begin();
        while (it != conds.end())
        {
            std::shared_ptr<Plan> left_need_to_join_executors = nullptr;
            std::shared_ptr<Plan> right_need_to_join_executors = nullptr;
            bool isneedreverse = false;
            if (std::find(joined_tables.begin(), joined_tables.end(), it->lhs_col.tab_name) == joined_tables.end())
            {
                left_need_to_join_executors = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
            }
            if (std::find(joined_tables.begin(), joined_tables.end(), it->rhs_col.tab_name) == joined_tables.end())
            {
                right_need_to_join_executors = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
                isneedreverse = true;
            }

            if (left_need_to_join_executors != nullptr && right_need_to_join_executors != nullptr)
            {
                std::vector<Condition> join_conds{*it};
                std::shared_ptr<Plan> temp_join_executors = std::make_shared<JoinPlan>(T_NestLoop,
                                                                                       std::move(left_need_to_join_executors),
                                                                                       std::move(right_need_to_join_executors),
                                                                                       join_conds, join_type);
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(temp_join_executors),
                                                                  std::move(table_join_executors),
                                                                  std::vector<Condition>(), join_type);
            }
            else if (left_need_to_join_executors != nullptr || right_need_to_join_executors != nullptr)
            {
                if (isneedreverse)
                {
                    std::map<CompOp, CompOp> swap_op = {
                        {OP_EQ, OP_EQ},
                        {OP_NE, OP_NE},
                        {OP_LT, OP_GT},
                        {OP_GT, OP_LT},
                        {OP_LE, OP_GE},
                        {OP_GE, OP_LE},
                    };
                    std::swap(it->lhs_col, it->rhs_col);
                    it->op = swap_op.at(it->op);
                    left_need_to_join_executors = std::move(right_need_to_join_executors);
                }
                std::vector<Condition> join_conds{*it};
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left_need_to_join_executors),
                                                                  std::move(table_join_executors), join_conds, join_type);
            }
            else
            {
                push_conds(std::move(&(*it)), table_join_executors);
            }
            it = conds.erase(it);
        }
    }
    else
    {
        table_join_executors = table_scan_executors[0];
        scantbl[0] = 1;
    }

    // 连接剩余表
    for (size_t i = 0; i < tables.size(); i++)
    {
        if (scantbl[i] == -1)
        {
            table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(table_scan_executors[i]),
                                                              std::move(table_join_executors), std::vector<Condition>(), join_type);
        }
    }

    return table_join_executors;
}

std::shared_ptr<Plan> Planner::generate_sort_plan(std::shared_ptr<Query> query, std::shared_ptr<Plan> plan)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);

    // 检查是否需要排序（ORDER BY 或 LIMIT）
    if (!x->has_sort)
    {
        return plan;
    }

    bool has_order_by = !x->order.empty();

    std::vector<TabCol> sort_cols;
    std::vector<ast::OrderByDir> sort_dirs;

    if (has_order_by)
    {
        std::vector<std::string> tables = query->tables;
        std::vector<ColMeta> all_cols;
        for (auto &sel_tab_name : tables)
        {
            const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
            all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
        }

        // 支持多列排序
        for (auto &orderby : x->order)
        {
            TabCol sel_col;
            for (auto &col : all_cols)
            {
                if (col.name == orderby->cols->col_name &&
                    (orderby->cols->tab_name.empty() || col.tab_name == orderby->cols->tab_name))
                {
                    sel_col = {.tab_name = col.tab_name, .col_name = col.name};
                    break;
                }
            }
            sort_cols.push_back(sel_col);
            sort_dirs.push_back(orderby->orderby_dir);
        }
    }

    // 传递 LIMIT 参数
    return std::make_shared<SortPlan>(T_Sort, std::move(plan), sort_cols, sort_dirs, x->limit);
}

/**
 * @brief select plan 生成
 *
 * @param sel_cols select plan 选取的列
 * @param tab_names select plan 目标的表
 * @param conds select plan 选取条件
 */
std::shared_ptr<Plan> Planner::generate_select_plan(std::shared_ptr<Query> query, Context *context)
{
    // 逻辑优化
    query = logical_optimization(std::move(query), context);

    // 物理优化 - 获取基础计划（扫描+连接）
    std::shared_ptr<Plan> base_plan = physical_optimization(query, context);
    
    auto sel_cols = query->cols;
    bool is_select_all = query->is_select_all;
    
    // 检查是否需要分组和聚合
    bool has_group_by = !query->group_by.empty();
    bool has_aggregation = need_agg_plan(query);
    
    if (has_group_by || has_aggregation)
    {
        std::shared_ptr<Plan> current_plan = base_plan;
        
        // 如果有GROUP BY或HAVING，先创建GroupPlan
        if (has_group_by || query->having)
        {
            current_plan = std::make_shared<GroupPlan>(T_Group, std::move(current_plan),
                                                      query->group_by, query->having);
        }
        
        // 如果有聚合函数，创建AggPlan作为GroupPlan的父节点
        if (has_aggregation)
        {
            current_plan = std::make_shared<AggPlan>(T_Aggregation, std::move(current_plan),
                                                    query->select_exprs);
        }
        
        // 投影算子包装聚合/分组算子
        return std::make_shared<ProjectionPlan>(T_Projection, std::move(current_plan),
                                               std::move(sel_cols), is_select_all);
    }
    else
    {
        // 普通查询，投影算子直接包装基础计划
        return std::make_shared<ProjectionPlan>(T_Projection, std::move(base_plan),
                                               std::move(sel_cols), is_select_all);
    }
}

// 生成DDL语句和DML语句的查询执行计划
std::shared_ptr<Plan> Planner::do_planner(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plannerRoot;
    if (auto x = std::dynamic_pointer_cast<ast::CreateTable>(query->parse))
    {
        // create table;
        std::vector<ColDef> col_defs;
        for (auto &field : x->fields)
        {
            if (auto sv_col_def = std::dynamic_pointer_cast<ast::ColDef>(field))
            {
                ColDef col_def = {.name = sv_col_def->col_name,
                                  .type = interp_sv_type(sv_col_def->type_len->type),
                                  .len = sv_col_def->type_len->len};
                col_defs.push_back(col_def);
            }
            else
            {
                throw InternalError("Unexpected field type");
            }
        }
        plannerRoot = std::make_shared<DDLPlan>(T_CreateTable, x->tab_name, std::vector<std::string>(), col_defs);
    }
    else if (auto x = std::dynamic_pointer_cast<ast::DropTable>(query->parse))
    {
        // drop table;
        plannerRoot = std::make_shared<DDLPlan>(T_DropTable, x->tab_name, std::vector<std::string>(), std::vector<ColDef>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::CreateIndex>(query->parse))
    {
        // create index;
        plannerRoot = std::make_shared<DDLPlan>(T_CreateIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::DropIndex>(query->parse))
    {
        // drop index
        plannerRoot = std::make_shared<DDLPlan>(T_DropIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(query->parse))
    {
        // insert;
        plannerRoot = std::make_shared<DMLPlan>(T_Insert, std::shared_ptr<Plan>(), x->tab_name,
                                                query->values, std::vector<Condition>(), std::vector<SetClause>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(query->parse))
    {
        // delete;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(x->tab_name, query->conds, index_col_names);

        if (index_exist == false)
        { // 该表没有索引
            index_col_names.clear();
            table_scan_executors =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }
        else
        { // 存在索引
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }

        plannerRoot = std::make_shared<DMLPlan>(T_Delete, table_scan_executors, x->tab_name,
                                                std::vector<Value>(), query->conds, std::vector<SetClause>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(query->parse))
    {
        // update;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->conds);
        std::vector<std::string> index_col_names;
        bool index_exist = get_index_cols(x->tab_name, query->conds, index_col_names);

        // 安全规则：若UPDATE修改的列与所选索引列有交集，则改用顺序扫描，避免一边扫描一边修改同一索引导致卡死
        if (index_exist) {
            for (const auto &sc : query->set_clauses) {
                for (const auto &idx_col : index_col_names) {
                    if (sc.lhs.col_name == idx_col) {
                        index_exist = false; // 改用顺序扫描
                        index_col_names.clear();
                        break;
                    }
                }
                if (!index_exist) break;
            }
        }

        if (!index_exist)
        { // 该表没有可用索引或为避免修改同一索引，采用顺序扫描
            index_col_names.clear();
            table_scan_executors =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }
        else
        { // 使用索引扫描
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names);
        }
        plannerRoot = std::make_shared<DMLPlan>(T_Update, table_scan_executors, x->tab_name,
                                                std::vector<Value>(), query->conds,
                                                query->set_clauses);
    }
    else if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse))
    {

        std::shared_ptr<plannerInfo> root = std::make_shared<plannerInfo>(x);
        // 生成select语句的查询执行计划
        std::shared_ptr<Plan> projection = generate_select_plan(std::move(query), context);
        plannerRoot = std::make_shared<DMLPlan>(T_select, projection, std::string(), std::vector<Value>(),
                                                std::vector<Condition>(), std::vector<SetClause>());
    }
    else if (auto x = std::dynamic_pointer_cast<ast::CreateStaticCheckpoint>(query->parse))
    {
        // create static checkpoint;
        plannerRoot = std::make_shared<OtherPlan>(T_CreateStaticCheckpoint, "");
    }
    else
    {
        throw InternalError("Unexpected AST root");
    }
    return plannerRoot;
}
