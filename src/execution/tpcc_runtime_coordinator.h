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

#include <mutex>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <string>
#include "common/config.h"
class SmManager;

/**
 * @brief TPC-C运行时协调器
 * 解决运行时并发执行导致的一致性问题
 * 这是解决您遇到的d_next_o_id不一致问题的核心组件
 */
class TPCCRuntimeCoordinator {
private:
    // 每个district的订单ID分配器，确保严格递增
    static std::unordered_map<std::string, std::atomic<int>> district_order_counters_;

    // 每个district的互斥锁，确保new_order事务的原子性
    static std::unordered_map<std::string, std::unique_ptr<std::mutex>> district_mutexes_;
    static std::mutex mutex_map_lock_;

    // 系统管理器（用于访问表/索引）；由执行层初始化
    static SmManager* sm_;

    // 事务执行计数器，用于监控
    static std::atomic<int> new_order_count_;
    static std::atomic<int> delivery_count_;
    static std::atomic<int> consistency_violations_;

public:
    // 初始化：注入 SmManager 指针（幂等）
    static void init(SmManager* sm) { sm_ = sm; }
    /**
     * @brief 为指定district分配下一个订单ID（线程安全，严格递增）
     * 这是解决d_next_o_id不一致问题的关键函数
     *
     * @param w_id 仓库ID
     * @param d_id 地区ID
     * @return 分配的订单ID，如果失败返回-1
     */
    static int allocate_next_order_id(int w_id, int d_id);

    /**
     * @brief 获取district的互斥锁，确保new_order事务的原子性
     *
     * @param w_id 仓库ID
     * @param d_id 地区ID
     * @return district专用的互斥锁
     */
    static std::mutex& get_district_mutex(int w_id, int d_id);

    /**
     * @brief 读取当前待分配的订单ID（不推进计数器）
     */
    static int peek_next_order_id(int w_id, int d_id);

    /**
     * @brief 执行原子的new_order事务
     * 确保district更新、orders插入、new_orders插入、order_line插入的原子性
     *
     * @param w_id 仓库ID
     * @param d_id 地区ID
     * @param c_id 客户ID
     * @param ol_cnt 订单行数量
     * @return 分配的订单ID，失败返回-1
     */
    static int execute_atomic_new_order(int w_id, int d_id, int c_id, int ol_cnt);

    /**
     * @brief 执行原子的delivery事务
     * 确保从new_orders删除记录与更新orders的原子性
     *
     * @param w_id 仓库ID
     * @param d_id 地区ID
     * @param carrier_id 承运商ID
     * @return 处理的订单ID，失败返回-1
     */
    static int execute_atomic_delivery(int w_id, int d_id, int carrier_id);

    /**
     * @brief 验证运行时一致性
     * 检查当前状态是否满足TPC-C一致性要求
     *
     * @param w_id 仓库ID
     * @param d_id 地区ID
     * @return 是否一致
     */
    static bool validate_runtime_consistency(int w_id, int d_id);

    /**
     * @brief 获取统计信息
     */
    static void get_statistics(int& new_orders, int& deliveries, int& violations) {
        new_orders = new_order_count_.load();
        deliveries = delivery_count_.load();
        violations = consistency_violations_.load();
    }

    /**
     * @brief 重置协调器状态（测试用）
     */
    static void reset();

private:
    /**
     * @brief 生成district的唯一键
     */
    static std::string get_district_key(int w_id, int d_id) {
        return std::to_string(w_id) + "_" + std::to_string(d_id);
    }

    /**
     * @brief 初始化district的订单计数器
     */
    static void initialize_district_counter(int w_id, int d_id);

    /**
     * @brief 从数据库获取district的当前d_next_o_id
     */
    static int get_current_d_next_o_id_from_db(int w_id, int d_id);
};
