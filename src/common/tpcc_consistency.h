#pragma once
#include <unordered_map>
#include <mutex>
#include <string>

// 轻量级TPCC一致性辅助（仅用于运行期统计与可选校验日志）
struct TPCCConsistencyManager {
    static std::mutex consistency_mutex_;
    static std::unordered_map<std::string, int> district_next_o_id_cache_;
    static std::unordered_map<std::string, int> orders_count_cache_;
    static std::unordered_map<std::string, int> order_line_count_cache_;

    static bool validate_district_orders_consistency(int w_id, int d_id, int d_next_o_id, int max_o_id, int max_no_o_id);
    static bool validate_orders_orderline_consistency(int w_id, int d_id, int sum_o_ol_cnt, int count_ol_o_id);
    static bool validate_new_orders_consistency(int w_id, int d_id, int count_no_o_id, int max_no_o_id, int min_no_o_id);

    static int allocate_next_order_id(int w_id, int d_id);
    static void record_order_insert(int w_id, int d_id, int o_id, int o_ol_cnt);
    static void record_order_line_insert(int w_id, int d_id, int o_id);
    static void clear_cache();
};

// 若需容忍差异，可在其他配置处提供宏或常量；此处仅声明默认值
#ifndef TPCC_CONSISTENCY_TOLERANCE
#define TPCC_CONSISTENCY_TOLERANCE 0
#endif

