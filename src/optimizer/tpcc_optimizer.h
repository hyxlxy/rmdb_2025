#pragma once

#include "planner.h"
#include "../common/tpcc_config.h"
#include <unordered_map>
#include <vector>
#include <string>

/**
 * @brief TPCC专用查询优化器
 * 针对TPCC工作负载的特点进行专门优化
 */
class TPCCOptimizer {
public:
    /**
     * @brief TPCC表的查询模式分析
     */
    struct TPCCQueryPattern {
        std::string table_name;
        std::vector<std::string> common_where_columns;  // 常用的WHERE条件列
        std::vector<std::string> common_select_columns; // 常用的SELECT列
        bool is_high_frequency;  // 是否是高频访问表
        int estimated_selectivity; // 估计的选择性
    };
    
    /**
     * @brief 获取TPCC表的查询模式
     */
    static std::unordered_map<std::string, TPCCQueryPattern> get_tpcc_patterns() {
        std::unordered_map<std::string, TPCCQueryPattern> patterns;

        patterns["warehouse"] = {"warehouse", {"w_id"}, {"w_id", "w_name", "w_tax", "w_ytd"}, true, 1};
        patterns["district"] = {"district", {"d_w_id", "d_id"}, {"d_w_id", "d_id", "d_tax", "d_next_o_id"}, true, 10};
        patterns["customer"] = {"customer", {"c_w_id", "c_d_id", "c_id"}, {"c_w_id", "c_d_id", "c_id", "c_discount", "c_last", "c_credit"}, true, 3000};
        patterns["item"] = {"item", {"i_id"}, {"i_id", "i_price", "i_name", "i_data"}, true, 100000};
        patterns["stock"] = {"stock", {"s_w_id", "s_i_id"}, {"s_w_id", "s_i_id", "s_quantity", "s_ytd", "s_order_cnt"}, true, 100000};
        patterns["orders"] = {"orders", {"o_w_id", "o_d_id", "o_id"}, {"o_w_id", "o_d_id", "o_id", "o_c_id", "o_entry_d"}, true, 30000};
        patterns["new_orders"] = {"new_orders", {"no_w_id", "no_d_id", "no_o_id"}, {"no_w_id", "no_d_id", "no_o_id"}, true, 9000};
        patterns["order_line"] = {"order_line", {"ol_w_id", "ol_d_id", "ol_o_id"}, {"ol_w_id", "ol_d_id", "ol_o_id", "ol_i_id", "ol_quantity"}, true, 300000};
        patterns["history"] = {"history", {}, {"h_c_id", "h_c_d_id", "h_c_w_id", "h_date", "h_amount"}, false, 30000};

        return patterns;
    }
    
    /**
     * @brief 优化TPCC查询的索引选择
     */
    static bool optimize_tpcc_index_selection(const std::string& table_name, 
                                            const std::vector<Condition>& conditions,
                                            std::vector<std::string>& optimal_index_cols) {
        auto patterns = get_tpcc_patterns();
        auto it = patterns.find(table_name);
        if (it == patterns.end()) {
            return false;
        }
        
        // 分析条件中涉及的列
        std::vector<std::string> condition_columns;
        for (const auto& cond : conditions) {
            if (cond.is_rhs_val && cond.lhs_col.tab_name == table_name) {
                condition_columns.push_back(cond.lhs_col.col_name);
            }
        }
        
        // 根据TPCC模式推荐最优索引
        if (table_name == "district" && 
            contains_columns(condition_columns, {"d_w_id", "d_id"})) {
            optimal_index_cols = {"d_w_id", "d_id"};
            return true;
        }
        
        if (table_name == "customer" && 
            contains_columns(condition_columns, {"c_w_id", "c_d_id"})) {
            optimal_index_cols = {"c_w_id", "c_d_id", "c_id"};
            return true;
        }
        
        if (table_name == "stock" && 
            contains_columns(condition_columns, {"s_w_id", "s_i_id"})) {
            optimal_index_cols = {"s_w_id", "s_i_id"};
            return true;
        }
        
        if (table_name == "orders" && 
            contains_columns(condition_columns, {"o_w_id", "o_d_id"})) {
            optimal_index_cols = {"o_w_id", "o_d_id", "o_id"};
            return true;
        }
        
        if (table_name == "new_orders" && 
            contains_columns(condition_columns, {"no_w_id", "no_d_id"})) {
            optimal_index_cols = {"no_w_id", "no_d_id", "no_o_id"};
            return true;
        }
        
        if (table_name == "order_line" && 
            contains_columns(condition_columns, {"ol_w_id", "ol_d_id", "ol_o_id"})) {
            optimal_index_cols = {"ol_w_id", "ol_d_id", "ol_o_id", "ol_number"};
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief 检查条件列是否包含指定的列
     */
    static bool contains_columns(const std::vector<std::string>& condition_columns,
                               const std::vector<std::string>& required_columns) {
        for (const auto& required : required_columns) {
            bool found = false;
            for (const auto& condition : condition_columns) {
                if (condition == required) {
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
     * @brief 估算查询成本
     */
    static double estimate_query_cost(const std::string& table_name,
                                    const std::vector<std::string>& index_columns,
                                    const std::vector<Condition>& conditions) {
        auto patterns = get_tpcc_patterns();
        auto it = patterns.find(table_name);
        if (it == patterns.end()) {
            return 1000.0; // 默认高成本
        }
        
        const auto& pattern = it->second;
        double base_cost = pattern.estimated_selectivity;
        
        // 如果使用了推荐的索引，大幅降低成本
        if (is_optimal_index_for_table(table_name, index_columns)) {
            base_cost *= 0.1; // 降低90%的成本
        }
        
        // 根据条件的选择性调整成本
        for (const auto& cond : conditions) {
            if (cond.op == OP_EQ) {
                base_cost *= 0.1; // 等值条件选择性很高
            } else if (cond.op == OP_LT || cond.op == OP_LE || 
                      cond.op == OP_GT || cond.op == OP_GE) {
                base_cost *= 0.3; // 范围条件选择性中等
            }
        }
        
        return base_cost;
    }
    
private:
    /**
     * @brief 检查是否是表的最优索引
     */
    static bool is_optimal_index_for_table(const std::string& table_name,
                                          const std::vector<std::string>& index_columns) {
        if (table_name == "district") {
            return index_columns.size() >= 2 && 
                   index_columns[0] == "d_w_id" && index_columns[1] == "d_id";
        }
        if (table_name == "customer") {
            return index_columns.size() >= 3 && 
                   index_columns[0] == "c_w_id" && index_columns[1] == "c_d_id" && 
                   index_columns[2] == "c_id";
        }
        if (table_name == "stock") {
            return index_columns.size() >= 2 && 
                   index_columns[0] == "s_w_id" && index_columns[1] == "s_i_id";
        }
        if (table_name == "orders") {
            return index_columns.size() >= 3 && 
                   index_columns[0] == "o_w_id" && index_columns[1] == "o_d_id" && 
                   index_columns[2] == "o_id";
        }
        if (table_name == "new_orders") {
            return index_columns.size() >= 3 && 
                   index_columns[0] == "no_w_id" && index_columns[1] == "no_d_id" && 
                   index_columns[2] == "no_o_id";
        }
        if (table_name == "order_line") {
            return index_columns.size() >= 4 && 
                   index_columns[0] == "ol_w_id" && index_columns[1] == "ol_d_id" && 
                   index_columns[2] == "ol_o_id" && index_columns[3] == "ol_number";
        }
        return false;
    }
};
