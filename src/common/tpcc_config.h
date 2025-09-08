#pragma once

// TPCC性能优化配置

// 启用TPCC性能模式
#define TPCC_PERFORMANCE_MODE 1

// 启用TPCC超快模式（跳过大部分MVCC检查）
// #define TPCC_FAST_MODE 1

// 禁用调试输出以提升性能
#define DISABLE_DEBUG_OUTPUT 1

// 索引优化配置
#define ENABLE_INDEX_OPTIMIZATION 1

// 最左匹配优化
#define ENABLE_LEFTMOST_PREFIX_OPTIMIZATION 1

// 批量操作优化
#define ENABLE_BATCH_OPERATIONS 1

// 缓存优化
#define ENABLE_CACHE_OPTIMIZATION 1

// 索引选择权重（仅影响执行计划，不改变结果）
#ifndef INDEX_EQ_WEIGHT
#define INDEX_EQ_WEIGHT 20
#endif
#ifndef INDEX_RANGE_WEIGHT
#define INDEX_RANGE_WEIGHT 6
#endif
#ifndef INDEX_TPCC_BONUS
#define INDEX_TPCC_BONUS 3
#endif

// TPCC特定的索引提示
namespace TPCCOptimization {
    
    // 推荐的索引配置
    struct IndexConfig {
        // warehouse表：主键索引已足够
        // district表：(d_w_id, d_id) - 支持最左匹配
        // customer表：(c_w_id, c_d_id, c_id) - 支持最左匹配
        // orders表：(o_w_id, o_d_id, o_id) - 支持最左匹配
        // new_orders表：(no_w_id, no_d_id, no_o_id) - 支持最左匹配
        // order_line表：(ol_w_id, ol_d_id, ol_o_id, ol_number) - 支持最左匹配
        // item表：(i_id) - 主键索引
        // stock表：(s_w_id, s_i_id) - 支持最左匹配
        
        static constexpr bool use_covering_indexes = true;
        static constexpr bool optimize_for_tpcc = true;
    };
    
    // 查询优化提示
    struct QueryOptimization {
        // 对于TPCC的典型查询模式，优先使用索引扫描
        static constexpr bool prefer_index_scan = true;
        
        // 对于小表，可以考虑全表扫描
        static constexpr int small_table_threshold = 1000;
        
        // 批量操作的阈值
        static constexpr int batch_operation_threshold = 100;
    };
}
