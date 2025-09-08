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

#include "plan.h"
#include "analyze/analyze.h"
#include "system/sm.h"
#include <vector>
#include <memory>
#include <unordered_set>

/**
 * @brief 逻辑优化器
 * 
 * 实现查询优化的三种规则：
 * 1. 选择运算下推（谓词下推）
 * 2. 投影运算下推
 * 3. 连接顺序优化
 */
class LogicalOptimizer {
private:
    SmManager* sm_manager_;

public:
    LogicalOptimizer(SmManager* sm_manager) : sm_manager_(sm_manager) {}

    /**
     * @brief 执行逻辑优化
     * @param query 原始查询
     * @param context 查询上下文
     * @return 优化后的查询
     */
    std::shared_ptr<Query> optimize(std::shared_ptr<Query> query, Context* context);

    /**
     * @brief 选择运算下推优化
     * @param query 查询对象
     * @return 优化后的查询
     */
    std::shared_ptr<Query> predicate_pushdown(std::shared_ptr<Query> query);

    /**
     * @brief 投影运算下推优化
     * @param query 查询对象
     * @return 优化后的查询
     */
    std::shared_ptr<Query> projection_pushdown(std::shared_ptr<Query> query);

    /**
     * @brief 连接顺序优化
     * @param query 查询对象
     * @return 优化后的查询
     */
    std::shared_ptr<Query> join_order_optimization(std::shared_ptr<Query> query);

private:
    /**
     * @brief 分析条件的适用表
     * @param condition 条件
     * @return 条件涉及的表名集合
     */
    std::unordered_set<std::string> analyze_condition_tables(const Condition& condition);

    /**
     * @brief 检查条件是否可以下推到指定表
     * @param condition 条件
     * @param table_name 表名
     * @return 是否可以下推
     */
    bool can_pushdown_to_table(const Condition& condition, const std::string& table_name);

    /**
     * @brief 检查条件是否为连接条件
     * @param condition 条件
     * @return 是否为连接条件
     */
    bool is_join_condition(const Condition& condition);

    /**
     * @brief 分离选择条件和连接条件
     * @param conditions 所有条件
     * @param selection_conditions 输出：选择条件
     * @param join_conditions 输出：连接条件
     */
    void separate_conditions(const std::vector<Condition>& conditions,
                           std::vector<Condition>& selection_conditions,
                           std::vector<Condition>& join_conditions);

    /**
     * @brief 按表分组选择条件
     * @param conditions 选择条件
     * @return 表名到条件列表的映射
     */
    std::unordered_map<std::string, std::vector<Condition>> group_conditions_by_table(
        const std::vector<Condition>& conditions);

    /**
     * @brief 计算查询需要的列
     * @param query 查询对象
     * @return 表名到列名集合的映射
     */
    std::unordered_map<std::string, std::unordered_set<std::string>> calculate_required_columns(
        std::shared_ptr<Query> query);

    /**
     * @brief 检查列是否在连接条件中使用
     * @param table_name 表名
     * @param column_name 列名
     * @param join_conditions 连接条件
     * @return 是否在连接条件中使用
     */
    bool is_column_used_in_joins(const std::string& table_name, 
                                const std::string& column_name,
                                const std::vector<Condition>& join_conditions);

    /**
     * @brief 检查列是否在选择条件中使用
     * @param table_name 表名
     * @param column_name 列名
     * @param selection_conditions 选择条件
     * @return 是否在选择条件中使用
     */
    bool is_column_used_in_selections(const std::string& table_name,
                                     const std::string& column_name,
                                     const std::vector<Condition>& selection_conditions);

    /**
     * @brief 将条件中使用的列添加到必需列集合中
     * @param conditions 条件列表
     * @param required_columns 必需列集合（输出参数）
     */
    void add_condition_columns_to_required(
        const std::vector<Condition>& conditions,
        std::unordered_map<std::string, std::unordered_set<std::string>>& required_columns);

    /**
     * @brief 检查是否为SELECT *
     * @param query 查询对象
     * @return 是否为SELECT *
     */
    bool check_if_select_all(std::shared_ptr<Query> query);

    /**
     * @brief 为每个表创建投影下推信息
     * @param query 查询对象
     * @param required_columns 每个表需要的列
     */
    void create_table_projections(
        std::shared_ptr<Query> query,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& required_columns);

    /**
     * @brief 简化的连接顺序优化
     * @param tables 表名列表
     * @param join_conditions 连接条件
     * @return 优化后的表顺序
     */
    std::vector<std::string> optimize_join_order_simple(
        const std::vector<std::string>& tables,
        const std::vector<Condition>& join_conditions);

    /**
     * @brief 检查表是否可以与已选择的表连接
     * @param table 待检查的表
     * @param selected_tables 已选择的表
     * @param join_conditions 连接条件
     * @return 是否可以连接
     */
    bool can_join_with_selected(const std::string& table,
                               const std::vector<std::string>& selected_tables,
                               const std::vector<Condition>& join_conditions);

    /**
     * @brief 估计连接结果的基数
     * @param selected_tables 已选择的表
     * @param new_table 新加入的表
     * @param join_conditions 连接条件
     * @return 估计的结果基数
     */
    size_t estimate_join_result_cardinality(const std::vector<std::string>& selected_tables,
                                           const std::string& new_table,
                                           const std::vector<Condition>& join_conditions);
};

/**
 * @brief 查询计划构建器
 * 
 * 根据优化后的查询构建查询计划树
 */
class PlanBuilder {
private:
    SmManager* sm_manager_;
    LogicalOptimizer* optimizer_;

public:
    PlanBuilder(SmManager* sm_manager, LogicalOptimizer* optimizer) 
        : sm_manager_(sm_manager), optimizer_(optimizer) {}

    /**
     * @brief 构建优化后的查询计划
     * @param query 优化后的查询
     * @param context 查询上下文
     * @return 查询计划树
     */
    std::shared_ptr<Plan> build_optimized_plan(std::shared_ptr<Query> query, Context* context);

    /**
     * @brief 构建单表扫描计划
     * @param table_name 表名
     * @param conditions 该表的选择条件
     * @param required_columns 需要的列
     * @return 扫描计划（可能包含Filter和Project节点）
     */
    std::shared_ptr<Plan> build_table_scan_plan(const std::string& table_name,
                                               const std::vector<Condition>& conditions,
                                               const std::unordered_set<std::string>& required_columns);

    /**
     * @brief 构建连接计划
     * @param tables 参与连接的表（按优化后的顺序）
     * @param table_plans 各表的扫描计划
     * @param join_conditions 连接条件
     * @return 连接计划树
     */
    std::shared_ptr<Plan> build_join_plan(const std::vector<std::string>& tables,
                                         const std::unordered_map<std::string, std::shared_ptr<Plan>>& table_plans,
                                         const std::vector<Condition>& join_conditions);

    /**
     * @brief 构建最终的投影计划
     * @param subplan 子计划
     * @param select_columns 选择的列
     * @return 投影计划
     */
    std::shared_ptr<Plan> build_projection_plan(std::shared_ptr<Plan> subplan,
                                               const std::vector<TabCol>& select_columns);

private:
    /**
     * @brief 创建扫描计划节点
     * @param table_name 表名
     * @param conditions 条件
     * @return 扫描计划
     */
    std::shared_ptr<Plan> create_scan_plan(const std::string& table_name,
                                          const std::vector<Condition>& conditions);



    /**
     * @brief 创建投影计划节点
     * @param subplan 子计划
     * @param columns 投影列
     * @return 投影计划
     */
    std::shared_ptr<Plan> create_projection_plan(std::shared_ptr<Plan> subplan,
                                                const std::vector<TabCol>& columns);

    /**
     * @brief 创建连接计划节点
     * @param left_plan 左子计划
     * @param right_plan 右子计划
     * @param join_conditions 连接条件
     * @return 连接计划
     */
    std::shared_ptr<Plan> create_join_plan(std::shared_ptr<Plan> left_plan,
                                          std::shared_ptr<Plan> right_plan,
                                          const std::vector<Condition>& join_conditions);

    /**
     * @brief 获取两个表之间的连接条件
     * @param table1 表1
     * @param table2 表2
     * @param all_join_conditions 所有连接条件
     * @return 两表之间的连接条件
     */
    std::vector<Condition> get_join_conditions_between_tables(const std::string& table1,
                                                             const std::string& table2,
                                                             const std::vector<Condition>& all_join_conditions);

    /**
     * @brief 检查是否需要索引扫描
     * @param table_name 表名
     * @param conditions 条件
     * @return 是否使用索引扫描
     */
    bool should_use_index_scan(const std::string& table_name, const std::vector<Condition>& conditions);

    /**
     * @brief 使用现有逻辑构建计划
     * @param query 查询对象
     * @param context 查询上下文
     * @return 查询计划
     */
    std::shared_ptr<Plan> build_plan_with_existing_logic(std::shared_ptr<Query> query, Context* context);

    /**
     * @brief 创建简单的连接计划
     * @param query 查询对象
     * @return 连接计划
     */
    std::shared_ptr<Plan> create_simple_join_plan(std::shared_ptr<Query> query);
};

/**
 * @brief 优化规则应用器
 *
 * 提供各种优化规则的具体实现
 */
class OptimizationRules {
public:
    /**
     * @brief 应用选择下推规则
     * @param query 查询对象
     * @param sm_manager 系统管理器
     * @return 是否应用了优化
     */
    static bool apply_predicate_pushdown(std::shared_ptr<Query> query, SmManager* sm_manager);

    /**
     * @brief 应用投影下推规则
     * @param query 查询对象
     * @param sm_manager 系统管理器
     * @return 是否应用了优化
     */
    static bool apply_projection_pushdown(std::shared_ptr<Query> query, SmManager* sm_manager);

    /**
     * @brief 应用连接顺序优化规则
     * @param query 查询对象
     * @param sm_manager 系统管理器
     * @return 是否应用了优化
     */
    static bool apply_join_reordering(std::shared_ptr<Query> query, SmManager* sm_manager);
};
