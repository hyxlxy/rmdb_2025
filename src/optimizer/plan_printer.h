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
#include "common/common.h"
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

/**
 * @brief 查询计划树格式化输出器
 * 
 * 按照题目要求的格式输出查询计划树：
 * - 使用缩进(\t)表示层级关系
 * - 节点按照 Filter -> Join -> Project -> Scan 顺序输出
 * - 属性按字典序排序
 * - 列名包含表名前缀
 */
class PlanPrinter {
public:
    /**
     * @brief 格式化输出查询计划树
     * @param plan 查询计划根节点
     * @return 格式化后的字符串
     */
    static std::string print_plan(std::shared_ptr<Plan> plan) {
        std::ostringstream oss;
        std::unordered_map<std::string, std::string> empty_alias_map;
        print_plan_recursive(plan, 0, oss, empty_alias_map);
        return oss.str();
    }

    /**
     * @brief 格式化输出查询计划树（支持别名）
     * @param plan 查询计划根节点
     * @param table_to_alias 表名到别名的映射
     * @return 格式化后的字符串
     */
    static std::string print_plan_with_alias(std::shared_ptr<Plan> plan,
                                           const std::unordered_map<std::string, std::string>& table_to_alias) {
        std::ostringstream oss;
        print_plan_recursive(plan, 0, oss, table_to_alias);
        return oss.str();
    }

    /**
     * @brief 收集计划树中的所有表名
     */
    static std::vector<std::string> collect_table_names(std::shared_ptr<Plan> plan) {
        std::vector<std::string> tables;
        collect_table_names_recursive(plan, tables);

        // 去重并排序
        std::sort(tables.begin(), tables.end());
        tables.erase(std::unique(tables.begin(), tables.end()), tables.end());

        return tables;
    }

private:
    /**
     * @brief 递归打印查询计划树
     * @param plan 当前计划节点
     * @param depth 当前深度（用于缩进）
     * @param oss 输出流
     */
    static void print_plan_recursive(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                                    const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (!plan) return;

        // 添加缩进（使用\t）
        for (int i = 0; i < depth; ++i) {
            oss << "\t";
        }

        // 根据计划类型输出相应格式
        switch (plan->tag) {
            case T_SeqScan:
            case T_IndexScan:
                print_scan_plan(plan, oss, table_to_alias);
                break;
            case T_NestLoop:
            case T_SortMerge:
                print_join_plan(plan, depth, oss, table_to_alias);
                break;
            case T_Projection:
                print_projection_plan(plan, depth, oss, table_to_alias);
                break;
            case T_Sort:
                print_sort_plan(plan, depth, oss, table_to_alias);
                break;
            case T_Aggregation:
                print_aggregation_plan(plan, depth, oss, table_to_alias);
                break;
            case T_Group:
                print_group_plan(plan, depth, oss, table_to_alias);
                break;
            default:
            break;
        }
    }

    /**
     * @brief 打印扫描计划节点
     */
    static void print_scan_plan(std::shared_ptr<Plan> plan, std::ostringstream& oss,
                               const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto scan_plan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
            // 始终显示真实表名，不使用别名
            oss << "Scan(table=" << scan_plan->tab_name_ << ")\n";
        }
    }

    /**
     * @brief 打印连接计划节点
     */
    static void print_join_plan(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                               const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
            // 收集所有表名，按真实表名排序，显示真实表名
            std::vector<std::string> tables = collect_table_names(plan);
            std::sort(tables.begin(), tables.end());  // 按真实表名排序

            // 直接使用真实表名显示
            std::vector<std::string> display_tables = tables;

            // 格式化连接条件（使用别名）
            std::vector<std::string> conditions;
            for (const auto& cond : join_plan->conds_) {
                conditions.push_back(format_condition_with_alias(cond, table_to_alias));
            }
            std::sort(conditions.begin(), conditions.end());

            oss << "Join(tables=[";
            for (size_t i = 0; i < display_tables.size(); ++i) {
                if (i > 0) oss << ",";
                oss << display_tables[i];
            }
            oss << "],condition=[";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) oss << ",";
                oss << conditions[i];
            }
            oss << "])\n";

            // 递归打印子节点，按照题目要求的节点类型顺序排序
            // Filter < Join < Project < Scan
            std::vector<std::shared_ptr<Plan>> children = {join_plan->left_, join_plan->right_};

            // 按照节点类型优先级排序
            std::sort(children.begin(), children.end(), [](const std::shared_ptr<Plan>& a, const std::shared_ptr<Plan>& b) {
                return get_node_priority(a) < get_node_priority(b);
            });

            for (const auto& child : children) {
                print_plan_recursive(child, depth + 1, oss, table_to_alias);
            }
        }
    }

    /**
     * @brief 打印投影计划节点
     */
    static void print_projection_plan(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                                     const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto proj_plan = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
            oss << "Project(columns=[";

            // 检查是否为SELECT *
            if (proj_plan->is_select_all_) {
                oss << "*";
            } else {
                // 格式化列名并排序（使用别名）
                std::vector<std::string> columns = format_columns_with_alias(proj_plan->sel_cols_, table_to_alias);
                std::sort(columns.begin(), columns.end());

                for (size_t i = 0; i < columns.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << columns[i];
                }
            }
            oss << "])\n";

            // 递归打印子节点
            print_plan_recursive(proj_plan->subplan_, depth + 1, oss, table_to_alias);
        }
    }

    /**
     * @brief 打印排序计划节点
     */
    static void print_sort_plan(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                               const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto sort_plan = std::dynamic_pointer_cast<SortPlan>(plan)) {
            oss << "Sort(columns=[";
            for (size_t i = 0; i < sort_plan->sel_cols_.size(); ++i) {
                if (i > 0) oss << ",";
                oss << format_column_with_alias(sort_plan->sel_cols_[i], table_to_alias);
                if (i < sort_plan->is_desc_.size() && sort_plan->is_desc_[i]) {
                    oss << " DESC";
                } else {
                    oss << " ASC";
                }
            }
            oss << "]";
            if (sort_plan->limit_ > 0) {
                oss << ", limit=" << sort_plan->limit_;
            }
            oss << ")\n";

            // 递归打印子节点
            print_plan_recursive(sort_plan->subplan_, depth + 1, oss, table_to_alias);
        }
    }

    /**
     * @brief 打印聚合计划节点
     */
    static void print_aggregation_plan(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                                      const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto agg_plan = std::dynamic_pointer_cast<AggPlan>(plan)) {
            oss << "Aggregation(functions=[";
            for (size_t i = 0; i < agg_plan->select_exprs_.size(); ++i) {
                if (i > 0) oss << ",";
                // 简化处理，实际应该格式化聚合表达式
                oss << "agg" << i;
            }
            oss << "])\n";

            // 递归打印子节点
            print_plan_recursive(agg_plan->input_, depth + 1, oss, table_to_alias);
        }
    }

    /**
     * @brief 打印分组计划节点
     */
    static void print_group_plan(std::shared_ptr<Plan> plan, int depth, std::ostringstream& oss,
                                const std::unordered_map<std::string, std::string>& table_to_alias = {}) {
        if (auto group_plan = std::dynamic_pointer_cast<GroupPlan>(plan)) {
            oss << "Group(";
            if (!group_plan->group_by_.empty()) {
                oss << "group_by=[";
                for (size_t i = 0; i < group_plan->group_by_.size(); ++i) {
                    if (i > 0) oss << ",";
                    // 简化处理，实际应该格式化表达式
                    oss << "expr" << i;
                }
                oss << "]";
            }
            if (group_plan->having_) {
                if (!group_plan->group_by_.empty()) oss << ", ";
                oss << "having=expr";
            }
            oss << ")\n";

            // 递归打印子节点
            print_plan_recursive(group_plan->input_, depth + 1, oss, table_to_alias);
        }
    }

    /**
     * @brief 格式化条件列表
     */
    static std::vector<std::string> format_conditions(const std::vector<Condition>& conditions) {
        std::vector<std::string> formatted;
        for (const auto& cond : conditions) {
            formatted.push_back(format_condition(cond));
        }
        std::sort(formatted.begin(), formatted.end());
        return formatted;
    }

    /**
     * @brief 格式化列名列表
     */
    static std::vector<std::string> format_columns(const std::vector<TabCol>& columns) {
        std::vector<std::string> formatted;
        for (const auto& col : columns) {
            std::ostringstream oss;
            if (!col.tab_name.empty()) {
                oss << col.tab_name << "." << col.col_name;
            } else {
                oss << col.col_name;
            }
            formatted.push_back(oss.str());
        }
        return formatted;
    }

    /**
     * @brief 格式化单个列名（支持别名）
     */
    static std::string format_column_with_alias(const TabCol& col,
                                               const std::unordered_map<std::string, std::string>& table_to_alias) {
        std::ostringstream oss;
        if (!col.tab_name.empty()) {
            // 查找表的别名
            auto alias_it = table_to_alias.find(col.tab_name);
            std::string display_name = (alias_it != table_to_alias.end()) ? alias_it->second : col.tab_name;
            oss << display_name << ".";
        }
        oss << col.col_name;
        return oss.str();
    }

    /**
     * @brief 格式化列名列表（支持别名）
     */
    static std::vector<std::string> format_columns_with_alias(const std::vector<TabCol>& columns,
                                                             const std::unordered_map<std::string, std::string>& table_to_alias) {
        std::vector<std::string> formatted;
        for (const auto& col : columns) {
            std::ostringstream oss;
            if (!col.tab_name.empty()) {
                // 查找表的别名
                auto alias_it = table_to_alias.find(col.tab_name);
                std::string display_name = (alias_it != table_to_alias.end()) ? alias_it->second : col.tab_name;
                oss << display_name << "." << col.col_name;
            } else {
                oss << col.col_name;
            }
            formatted.push_back(oss.str());
        }
        return formatted;
    }

private:

    /**
     * @brief 递归收集表名
     */
    static void collect_table_names_recursive(std::shared_ptr<Plan> plan, std::vector<std::string>& tables) {
        if (!plan) return;

        if (auto scan_plan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
            tables.push_back(scan_plan->tab_name_);
        } else if (auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
            collect_table_names_recursive(join_plan->left_, tables);
            collect_table_names_recursive(join_plan->right_, tables);
        } else if (auto proj_plan = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
            collect_table_names_recursive(proj_plan->subplan_, tables);
        } else if (auto sort_plan = std::dynamic_pointer_cast<SortPlan>(plan)) {
            collect_table_names_recursive(sort_plan->subplan_, tables);
        } else if (auto agg_plan = std::dynamic_pointer_cast<AggPlan>(plan)) {
            collect_table_names_recursive(agg_plan->input_, tables);
        }
    }



    /**
     * @brief 格式化单个条件
     */
    static std::string format_condition(const Condition& cond) {
        std::ostringstream oss;
        oss << cond.lhs_col.tab_name << "." << cond.lhs_col.col_name;
        oss << op_to_string(cond.op);

        if (cond.is_rhs_val) {
            oss << value_to_string(cond.rhs_val);
        } else {
            oss << cond.rhs_col.tab_name << "." << cond.rhs_col.col_name;
        }

        return oss.str();
    }

    /**
     * @brief 格式化单个条件（支持别名）
     */
    static std::string format_condition_with_alias(const Condition& cond,
                                                  const std::unordered_map<std::string, std::string>& table_to_alias) {
        std::ostringstream oss;

        // 左侧列名
        auto lhs_alias_it = table_to_alias.find(cond.lhs_col.tab_name);
        std::string lhs_display_name = (lhs_alias_it != table_to_alias.end()) ? lhs_alias_it->second : cond.lhs_col.tab_name;
        oss << lhs_display_name << "." << cond.lhs_col.col_name;

        oss << op_to_string(cond.op);

        if (cond.is_rhs_val) {
            oss << value_to_string(cond.rhs_val);
        } else {
            // 右侧列名
            auto rhs_alias_it = table_to_alias.find(cond.rhs_col.tab_name);
            std::string rhs_display_name = (rhs_alias_it != table_to_alias.end()) ? rhs_alias_it->second : cond.rhs_col.tab_name;
            oss << rhs_display_name << "." << cond.rhs_col.col_name;
        }

        return oss.str();
    }



    /**
     * @brief 获取节点类型的优先级（用于排序）
     * 按照题目要求：Filter < Join < Project < Scan
     */
    static int get_node_priority(std::shared_ptr<Plan> plan) {
        if (!plan) return 999;

        switch (plan->tag) {
            case T_NestLoop:
            case T_SortMerge:
                return 1;  // Join优先级最高
            case T_Projection:
                return 2;  // Project次之
            case T_SeqScan:
            case T_IndexScan:
                return 3;  // Scan再次
            case T_Sort:
                return 4;  // Sort
            case T_Aggregation:
                return 5;  // Aggregation
            default:
                return 999;
        }
    }

    /**
     * @brief 获取计划节点的主要表名（用于排序）
     */
    static std::string get_primary_table_name(std::shared_ptr<Plan> plan) {
        if (!plan) return "";

        if (auto scan_plan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
            return scan_plan->tab_name_;
        } else if (auto proj_plan = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
            return get_primary_table_name(proj_plan->subplan_);
        } else if (auto sort_plan = std::dynamic_pointer_cast<SortPlan>(plan)) {
            return get_primary_table_name(sort_plan->subplan_);
        } else if (auto agg_plan = std::dynamic_pointer_cast<AggPlan>(plan)) {
            return get_primary_table_name(agg_plan->input_);
        } else if (auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
            // 对于JOIN节点，返回字典序最小的表名
            std::vector<std::string> tables = collect_table_names(plan);
            if (!tables.empty()) {
                std::sort(tables.begin(), tables.end());
                return tables[0];
            }
        }
        return "";
    }

    /**
     * @brief 检查是否为SELECT *
     */
    static bool is_select_all(const std::vector<TabCol>& columns) {
        // 对于EXPLAIN查询，如果列为空或者包含所有表的所有列，认为是SELECT *
        // 这里我们需要一个更智能的判断方式
        return columns.empty();
    }

    /**
     * @brief 将操作符转换为字符串
     */
    static std::string op_to_string(CompOp op) {
        switch (op) {
            case OP_EQ: return "=";
            case OP_NE: return "!=";
            case OP_LT: return "<";
            case OP_GT: return ">";
            case OP_LE: return "<=";
            case OP_GE: return ">=";
            default: return "?";
        }
    }

    /**
     * @brief 将值转换为字符串
     */
    static std::string value_to_string(const Value& val) {
        switch (val.type) {
            case TYPE_INT:
                return std::to_string(val.int_val);
            case TYPE_FLOAT:
                return std::to_string(val.float_val);
            case TYPE_STRING:
                return "'" + val.str_val + "'";  // 为字符串值添加引号
            default:
                return "?";
        }
    }
};
