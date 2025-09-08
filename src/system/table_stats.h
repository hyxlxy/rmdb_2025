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

#include <string>
#include <unordered_map>
#include <memory>
#include "record/rm.h"
#include "system/sm_meta.h"

// 前向声明避免循环依赖
struct Condition;

/**
 * @brief 表统计信息结构
 */
struct TableStats {
    std::string table_name;     // 表名
    size_t row_count;          // 行数
    size_t page_count;         // 页数
    double avg_row_size;       // 平均行大小
    size_t last_update_time;   // 最后更新时间戳

    TableStats() : row_count(0), page_count(0), avg_row_size(0.0), last_update_time(0) {}
    
    TableStats(const std::string& name, size_t rows, size_t pages, double avg_size) 
        : table_name(name), row_count(rows), page_count(pages), avg_row_size(avg_size), last_update_time(0) {}
};

/**
 * @brief 表统计信息管理器
 * 
 * 负责维护和更新表的统计信息，为查询优化器提供基础数据
 */
class TableStatsManager {
private:
    std::unordered_map<std::string, TableStats> stats_cache_;  // 统计信息缓存
    bool cache_dirty_;  // 缓存是否需要更新

public:
    TableStatsManager() : cache_dirty_(false) {}

    /**
     * @brief 获取表的行数
     * @param table_name 表名
     * @param fh 表文件句柄
     * @return 表的行数
     */
    size_t get_table_row_count(const std::string& table_name, RmFileHandle* fh);

    /**
     * @brief 获取表的统计信息
     * @param table_name 表名
     * @param fh 表文件句柄
     * @return 表的统计信息
     */
    TableStats get_table_stats(const std::string& table_name, RmFileHandle* fh);

    /**
     * @brief 更新表的统计信息
     * @param table_name 表名
     * @param fh 表文件句柄
     */
    void update_table_stats(const std::string& table_name, RmFileHandle* fh);

    /**
     * @brief 批量更新所有表的统计信息
     * @param fhs 所有表的文件句柄映射
     */
    void update_all_stats(const std::unordered_map<std::string, std::unique_ptr<RmFileHandle>>& fhs);

    /**
     * @brief 获取缓存的统计信息（如果存在）
     * @param table_name 表名
     * @return 统计信息指针，如果不存在返回nullptr
     */
    const TableStats* get_cached_stats(const std::string& table_name) const;

    /**
     * @brief 清空统计信息缓存
     */
    void clear_cache();

    /**
     * @brief 检查缓存是否需要更新
     */
    bool is_cache_dirty() const { return cache_dirty_; }

    /**
     * @brief 标记缓存为脏
     */
    void mark_cache_dirty() { cache_dirty_ = true; }

private:
    /**
     * @brief 计算表的实际行数（通过扫描）
     * @param fh 表文件句柄
     * @return 实际行数
     */
    size_t count_table_rows(RmFileHandle* fh);

    /**
     * @brief 计算表的页数
     * @param fh 表文件句柄
     * @return 页数
     */
    size_t count_table_pages(RmFileHandle* fh);

    /**
     * @brief 计算平均行大小
     * @param fh 表文件句柄
     * @param row_count 行数
     * @return 平均行大小
     */
    double calculate_avg_row_size(RmFileHandle* fh, size_t row_count);
};

/**
 * @brief 表基数估计器
 * 
 * 为连接顺序优化提供表基数估计功能
 */
class CardinalityEstimator {
private:
    TableStatsManager* stats_manager_;

public:
    CardinalityEstimator(TableStatsManager* stats_manager) : stats_manager_(stats_manager) {}

    /**
     * @brief 估计表的基数（行数）
     * @param table_name 表名
     * @param fh 表文件句柄
     * @return 估计的基数
     */
    size_t estimate_table_cardinality(const std::string& table_name, RmFileHandle* fh);

    /**
     * @brief 估计连接结果的基数
     * @param left_table 左表名
     * @param right_table 右表名
     * @param left_fh 左表文件句柄
     * @param right_fh 右表文件句柄
     * @param join_conditions 连接条件
     * @return 估计的连接结果基数
     */
    size_t estimate_join_cardinality(const std::string& left_table, 
                                   const std::string& right_table,
                                   RmFileHandle* left_fh, 
                                   RmFileHandle* right_fh,
                                   const std::vector<Condition>& join_conditions);

    /**
     * @brief 估计选择操作后的基数
     * @param table_name 表名
     * @param fh 表文件句柄
     * @param conditions 选择条件
     * @return 估计的选择结果基数
     */
    size_t estimate_selection_cardinality(const std::string& table_name,
                                        RmFileHandle* fh,
                                        const std::vector<Condition>& conditions);

private:
    /**
     * @brief 估计选择率
     * @param condition 选择条件
     * @return 选择率（0.0-1.0）
     */
    double estimate_selectivity(const Condition& condition);

    /**
     * @brief 估计连接选择率
     * @param condition 连接条件
     * @param left_cardinality 左表基数
     * @param right_cardinality 右表基数
     * @return 连接选择率
     */
    double estimate_join_selectivity(const Condition& condition, 
                                   size_t left_cardinality, 
                                   size_t right_cardinality);
};

/**
 * @brief 连接顺序优化器
 * 
 * 基于表基数实现贪心算法的连接顺序优化
 */
class JoinOrderOptimizer {
private:
    CardinalityEstimator* cardinality_estimator_;

public:
    JoinOrderOptimizer(CardinalityEstimator* estimator) : cardinality_estimator_(estimator) {}

    /**
     * @brief 优化连接顺序
     * @param tables 参与连接的表名列表
     * @param fhs 表文件句柄映射
     * @param join_conditions 连接条件
     * @return 优化后的连接顺序
     */
    std::vector<std::string> optimize_join_order(
        const std::vector<std::string>& tables,
        const std::unordered_map<std::string, std::unique_ptr<RmFileHandle>>& fhs,
        const std::vector<Condition>& join_conditions);

    /**
     * @brief 使用贪心算法选择下一个最优表
     * @param remaining_tables 剩余的表
     * @param joined_tables 已连接的表
     * @param fhs 表文件句柄映射
     * @param join_conditions 连接条件
     * @return 下一个最优表的名称
     */
    std::string select_next_table(
        const std::vector<std::string>& remaining_tables,
        const std::vector<std::string>& joined_tables,
        const std::unordered_map<std::string, std::unique_ptr<RmFileHandle>>& fhs,
        const std::vector<Condition>& join_conditions);

private:
    /**
     * @brief 计算连接代价
     * @param left_tables 左侧表集合
     * @param right_table 右侧表
     * @param fhs 表文件句柄映射
     * @param join_conditions 连接条件
     * @return 连接代价
     */
    double calculate_join_cost(
        const std::vector<std::string>& left_tables,
        const std::string& right_table,
        const std::unordered_map<std::string, std::unique_ptr<RmFileHandle>>& fhs,
        const std::vector<Condition>& join_conditions);

    /**
     * @brief 检查表是否可以连接
     * @param table1 表1
     * @param table2 表2
     * @param join_conditions 连接条件
     * @return 是否可以连接
     */
    bool can_join(const std::string& table1, 
                  const std::string& table2, 
                  const std::vector<Condition>& join_conditions);
};
