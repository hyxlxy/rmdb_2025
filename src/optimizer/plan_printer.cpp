
/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "plan_printer.h"
#include "system/sm.h"

/**
 * @brief 查询计划树排序比较器
 *
 * 按照题目要求的顺序排序：Filter -> Join -> Project -> Scan
 * 同类型节点按照特定规则排序
 */
class PlanNodeComparator {
public:
    static int get_node_priority(std::shared_ptr<Plan> plan) {
        switch (plan->tag) {
            case T_NestLoop:
            case T_SortMerge: return 1;
            case T_Projection: return 2;
            case T_SeqScan:
            case T_IndexScan: return 3;
            case T_Sort: return 4;
            case T_Aggregation: return 5;
            default: return 6;
        }
    }

    static bool compare_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        int priority_a = get_node_priority(a);
        int priority_b = get_node_priority(b);

        if (priority_a != priority_b) {
            return priority_a < priority_b;
        }

        // 同类型节点的排序规则
        if (a->tag == b->tag) {
            switch (a->tag) {
                case T_SeqScan:
                case T_IndexScan:
                    return compare_scan_plans(a, b);
                case T_NestLoop:
                case T_SortMerge:
                    return compare_join_plans(a, b);
                case T_Projection:
                    return compare_projection_plans(a, b);
                case T_Sort:
                    return compare_sort_plans(a, b);
                case T_Aggregation:
                    return compare_agg_plans(a, b);
                default:
                    return false;
            }
        }

        return false;
    }

private:
    static bool compare_scan_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        auto scan_a = std::dynamic_pointer_cast<ScanPlan>(a);
        auto scan_b = std::dynamic_pointer_cast<ScanPlan>(b);
        if (scan_a && scan_b) {
            return scan_a->tab_name_ < scan_b->tab_name_;
        }
        return false;
    }

    static bool compare_join_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        auto join_a = std::dynamic_pointer_cast<JoinPlan>(a);
        auto join_b = std::dynamic_pointer_cast<JoinPlan>(b);
        if (join_a && join_b) {
            // 比较表名列表
            auto tables_a = PlanPrinter::collect_table_names(a);
            auto tables_b = PlanPrinter::collect_table_names(b);
            return tables_a < tables_b;
        }
        return false;
    }

    static bool compare_projection_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        auto proj_a = std::dynamic_pointer_cast<ProjectionPlan>(a);
        auto proj_b = std::dynamic_pointer_cast<ProjectionPlan>(b);
        if (proj_a && proj_b) {
            // 比较选择的列数量
            return proj_a->sel_cols_.size() < proj_b->sel_cols_.size();
        }
        return false;
    }

    static bool compare_sort_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        auto sort_a = std::dynamic_pointer_cast<SortPlan>(a);
        auto sort_b = std::dynamic_pointer_cast<SortPlan>(b);
        if (sort_a && sort_b) {
            // 比较排序列数量
            return sort_a->sel_cols_.size() < sort_b->sel_cols_.size();
        }
        return false;
    }

    static bool compare_agg_plans(std::shared_ptr<Plan> a, std::shared_ptr<Plan> b) {
        auto agg_a = std::dynamic_pointer_cast<AggPlan>(a);
        auto agg_b = std::dynamic_pointer_cast<AggPlan>(b);
        if (agg_a && agg_b) {
            // 比较聚合表达式数量
            return agg_a->select_exprs_.size() < agg_b->select_exprs_.size();
        }
        return false;
    }
};

/**
 * @brief 扩展的计划打印器实现
 */
namespace PlanPrinterImpl {

    /**
     * @brief 检查是否为SELECT *的改进实现
     */
    bool is_select_all_improved(const std::vector<TabCol>& sel_cols,
                               const std::vector<std::string>& all_tables,
                               SmManager* sm_manager) {
        if (!sm_manager) return false;

        // 收集所有表的所有列
        std::vector<TabCol> all_cols;
        for (const auto& table_name : all_tables) {
            try {
                TabMeta& tab = sm_manager->db_.get_table(table_name);
                for (const auto& col : tab.cols) {
                    TabCol tab_col;
                    tab_col.tab_name = table_name;
                    tab_col.col_name = col.name;
                    all_cols.push_back(tab_col);
                }
            } catch (...) {
                // 表不存在，跳过
                continue;
            }
        }

        // 检查选择的列是否包含所有列
        if (sel_cols.size() != all_cols.size()) {
            return false;
        }

        // 简单的包含检查（可以进一步优化）
        for (const auto& sel_col : sel_cols) {
            bool found = false;
            for (const auto& all_col : all_cols) {
                if (sel_col.tab_name == all_col.tab_name &&
                    sel_col.col_name == all_col.col_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 格式化表名，处理别名
     */
    std::string format_table_name(const std::string& table_name, const std::string& alias) {
        if (!alias.empty() && alias != table_name) {
            return alias;
        }
        return table_name;
    }

    /**
     * @brief 格式化列名，处理表名前缀和别名
     */
    std::string format_column_name(const TabCol& col, const std::map<std::string, std::string>& table_aliases) {
        std::string table_name = col.tab_name;

        // 检查是否有别名
        auto alias_it = table_aliases.find(col.tab_name);
        if (alias_it != table_aliases.end()) {
            table_name = alias_it->second;
        }

        if (!table_name.empty()) {
            return table_name + "." + col.col_name;
        } else {
            return col.col_name;
        }
    }

    /**
     * @brief 验证查询计划树的正确性
     */
    bool validate_plan_tree(std::shared_ptr<Plan> plan) {
        if (!plan) return false;

        switch (plan->tag) {
            case T_SeqScan:
            case T_IndexScan:
                // 扫描节点应该是叶节点
                return true;

            case T_Projection:
                if (auto proj_plan = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
                    return proj_plan->subplan_ && validate_plan_tree(proj_plan->subplan_);
                }
                return false;

            case T_Sort:
                if (auto sort_plan = std::dynamic_pointer_cast<SortPlan>(plan)) {
                    return sort_plan->subplan_ && validate_plan_tree(sort_plan->subplan_);
                }
                return false;

            case T_Aggregation:
                if (auto agg_plan = std::dynamic_pointer_cast<AggPlan>(plan)) {
                    return agg_plan->input_ && validate_plan_tree(agg_plan->input_);
                }
                return false;

            case T_NestLoop:
            case T_SortMerge:
                if (auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
                    return join_plan->left_ && join_plan->right_ &&
                           validate_plan_tree(join_plan->left_) &&
                           validate_plan_tree(join_plan->right_);
                }
                return false;

            default:
                return false;
        }
    }

    /**
     * @brief 计算查询计划树的深度
     */
    int calculate_plan_depth(std::shared_ptr<Plan> plan) {
        if (!plan) return 0;

        switch (plan->tag) {
            case T_SeqScan:
            case T_IndexScan:
                return 1;

            case T_Projection:
                if (auto proj_plan = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
                    return 1 + calculate_plan_depth(proj_plan->subplan_);
                }
                return 1;

            case T_Sort:
                if (auto sort_plan = std::dynamic_pointer_cast<SortPlan>(plan)) {
                    return 1 + calculate_plan_depth(sort_plan->subplan_);
                }
                return 1;

            case T_Aggregation:
                if (auto agg_plan = std::dynamic_pointer_cast<AggPlan>(plan)) {
                    return 1 + calculate_plan_depth(agg_plan->input_);
                }
                return 1;

            case T_NestLoop:
            case T_SortMerge:
                if (auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
                    return 1 + std::max(calculate_plan_depth(join_plan->left_),
                                       calculate_plan_depth(join_plan->right_));
                }
                return 1;

            default:
                return 1;
        }
    }
}

/**
 * @brief 公共接口函数
 */
std::string format_plan_with_validation(std::shared_ptr<Plan> plan) {
    if (!PlanPrinterImpl::validate_plan_tree(plan)) {
        return "Error: Invalid plan tree structure\n";
    }

    return PlanPrinter::print_plan(plan);
}

int get_plan_complexity(std::shared_ptr<Plan> plan) {
    return PlanPrinterImpl::calculate_plan_depth(plan);
}
