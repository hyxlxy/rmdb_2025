/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "tpcc_runtime_coordinator.h"
#include <iostream>
#include <sstream>
#include "system/sm_manager.h"
#include "record/rm.h"
#include "index/ix.h"

// 静态成员变量定义
std::unordered_map<std::string, std::atomic<int>> TPCCRuntimeCoordinator::district_order_counters_;
std::unordered_map<std::string, std::unique_ptr<std::mutex>> TPCCRuntimeCoordinator::district_mutexes_;
std::mutex TPCCRuntimeCoordinator::mutex_map_lock_;
std::atomic<int> TPCCRuntimeCoordinator::new_order_count_{0};
std::atomic<int> TPCCRuntimeCoordinator::delivery_count_{0};
std::atomic<int> TPCCRuntimeCoordinator::consistency_violations_{0};
SmManager* TPCCRuntimeCoordinator::sm_ = nullptr;

int TPCCRuntimeCoordinator::allocate_next_order_id(int w_id, int d_id) {
    std::string district_key = get_district_key(w_id, d_id);

    // 确保district计数器已初始化
    {
        std::lock_guard<std::mutex> lock(mutex_map_lock_);
        if (district_order_counters_.find(district_key) == district_order_counters_.end()) {
            initialize_district_counter(w_id, d_id);
        }
    }

    // 原子地分配下一个订单ID
    int next_id = district_order_counters_[district_key].fetch_add(1);

    if (!DISABLE_DEBUG_OUTPUT) {
        std::cout << "[TPCC] Allocated order ID " << next_id
                  << " for district (w_id=" << w_id << ", d_id=" << d_id << ")" << std::endl;
    }
    return next_id;
}


int TPCCRuntimeCoordinator::peek_next_order_id(int w_id, int d_id) {
    std::string district_key = get_district_key(w_id, d_id);
    // 确保初始化
    {
        std::lock_guard<std::mutex> lock(mutex_map_lock_);
        if (district_order_counters_.find(district_key) == district_order_counters_.end()) {
            initialize_district_counter(w_id, d_id);
        }
    }
    // 只读返回当前值
    return district_order_counters_[district_key].load();
}

std::mutex& TPCCRuntimeCoordinator::get_district_mutex(int w_id, int d_id) {
    std::string district_key = get_district_key(w_id, d_id);

    std::lock_guard<std::mutex> lock(mutex_map_lock_);
    if (district_mutexes_.find(district_key) == district_mutexes_.end()) {
        district_mutexes_[district_key] = std::make_unique<std::mutex>();
    }

    return *district_mutexes_[district_key];
}

int TPCCRuntimeCoordinator::execute_atomic_new_order(int w_id, int d_id, int c_id, int ol_cnt) {
    // 获取district专用锁，确保整个new_order事务的原子性
    std::mutex& district_mutex = get_district_mutex(w_id, d_id);
    std::lock_guard<std::mutex> lock(district_mutex);

    try {
        // 1. 分配订单ID
        int o_id = allocate_next_order_id(w_id, d_id);
        if (o_id <= 0) {
            consistency_violations_.fetch_add(1);
            std::cerr << "[TPCC ERROR] Failed to allocate order ID for district (w_id="
                     << w_id << ", d_id=" << d_id << ")" << std::endl;
            return -1;
        }

        // 2. 这里应该依次执行：
        //    - UPDATE district SET d_next_o_id = o_id + 1
        //    - INSERT INTO orders
        //    - INSERT INTO new_orders
        //    - INSERT INTO order_line (多次)
        //
        // 由于我们在执行器层面已经添加了验证，这里主要负责协调

        new_order_count_.fetch_add(1);

        if (!DISABLE_DEBUG_OUTPUT) {
            std::cout << "[TPCC] Successfully coordinated new_order transaction: o_id=" << o_id
                     << " for district (w_id=" << w_id << ", d_id=" << d_id << ")" << std::endl;
        }

        return o_id;

    } catch (const std::exception& e) {
        consistency_violations_.fetch_add(1);
        std::cerr << "[TPCC ERROR] Exception in new_order transaction: " << e.what() << std::endl;
        return -1;
    }
}

int TPCCRuntimeCoordinator::execute_atomic_delivery(int w_id, int d_id, int carrier_id) {
    // 获取district专用锁，确保delivery事务的原子性
    std::mutex& district_mutex = get_district_mutex(w_id, d_id);
    std::lock_guard<std::mutex> lock(district_mutex);

    try {
        // 1. 查找new_orders表中最小的订单ID
        // 2. 删除该new_orders记录
        // 3. 更新对应的orders记录
        // 4. 更新order_line记录
        // 5. 更新customer记录
        //
        // 这个实现需要与具体的执行器集成

        delivery_count_.fetch_add(1);

        if (!DISABLE_DEBUG_OUTPUT) {
            std::cout << "[TPCC] Successfully coordinated delivery transaction for district (w_id="
                     << w_id << ", d_id=" << d_id << ")" << std::endl;
        }

        return 1; // 简化实现，返回成功标志

    } catch (const std::exception& e) {
        consistency_violations_.fetch_add(1);
        std::cerr << "[TPCC ERROR] Exception in delivery transaction: " << e.what() << std::endl;
        return -1;
    }
}

#include "common/tpcc_runtime_config.h"

bool TPCCRuntimeCoordinator::validate_runtime_consistency(int w_id, int d_id) {
    std::string district_key = get_district_key(w_id, d_id);

    try {
        // 获取当前分配的订单计数器值
        int current_counter = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_map_lock_);
            if (district_order_counters_.find(district_key) != district_order_counters_.end()) {
                current_counter = district_order_counters_[district_key].load();
            }
        }

        // 从数据库获取实际的d_next_o_id
        int db_d_next_o_id = get_current_d_next_o_id_from_db(w_id, d_id);

        // 验证一致性，允许一定容忍度
        int diff = std::abs(current_counter - db_d_next_o_id);
        bool is_consistent = (diff <= TPCCRuntimeConfig::d_next_oid_tolerance.load());

        if (!is_consistent && TPCCRuntimeConfig::enable_consistency_logging.load()) {
            consistency_violations_.fetch_add(1);
            std::cerr << "[TPCC CONSISTENCY ERROR] District (w_id=" << w_id << ", d_id=" << d_id
                     << "): counter=" << current_counter << ", db_d_next_o_id=" << db_d_next_o_id <<
                     ", diff=" << diff << ", tol=" << TPCCRuntimeConfig::d_next_oid_tolerance.load() << std::endl;
        }

        return is_consistent;

    } catch (const std::exception& e) {
        consistency_violations_.fetch_add(1);
        if (TPCCRuntimeConfig::enable_consistency_logging.load()) {
            std::cerr << "[TPCC ERROR] Exception validating consistency: " << e.what() << std::endl;
        }
        return false;
    }
}

void TPCCRuntimeCoordinator::reset() {
    std::lock_guard<std::mutex> lock(mutex_map_lock_);

    district_order_counters_.clear();
    district_mutexes_.clear();
    new_order_count_.store(0);
    delivery_count_.store(0);
    consistency_violations_.store(0);

    std::cout << "[TPCC] Runtime coordinator reset" << std::endl;
}

void TPCCRuntimeCoordinator::initialize_district_counter(int w_id, int d_id) {
    std::string district_key = get_district_key(w_id, d_id);

    // 从数据库获取当前的d_next_o_id作为初始值（索引优先）
    int current_d_next_o_id = get_current_d_next_o_id_from_db(w_id, d_id);

    if (current_d_next_o_id <= 0) {
        // 如果数据库中没有记录或值无效，使用保守默认值 1
        current_d_next_o_id = 1;
        if (!DISABLE_DEBUG_OUTPUT) {
            std::cerr << "[TPCC WARNING] Using fallback d_next_o_id=" << current_d_next_o_id
                      << " for district (w_id=" << w_id << ", d_id=" << d_id << ")" << std::endl;
        }
    }

    district_order_counters_[district_key].store(current_d_next_o_id);

    if (!DISABLE_DEBUG_OUTPUT) {
        std::cout << "[TPCC] Initialized district counter (w_id=" << w_id << ", d_id=" << d_id
                 << ") with d_next_o_id=" << current_d_next_o_id << std::endl;
    }
}

int TPCCRuntimeCoordinator::get_current_d_next_o_id_from_db(int w_id, int d_id) {
    if (sm_ == nullptr) return -1;
    try {
        TabMeta &dtab = sm_->db_.get_table("district");
        // 优先用 (d_w_id,d_id) 索引点查
        std::string idx_name;
        for (auto &[iname, meta] : dtab.indexes) {
            if (meta.col_num == 2 && meta.cols[0].name == "d_w_id" && meta.cols[1].name == "d_id") { idx_name = iname; break; }
        }
        if (!idx_name.empty()) {
            auto ih = sm_->ihs_.at(idx_name).get();
            const auto &meta = dtab.indexes[idx_name];
            std::unique_ptr<char[]> key(new char[meta.col_tot_len]);
            int off = 0;
            memcpy(key.get()+off, &w_id, meta.cols[0].len); off += meta.cols[0].len;
            memcpy(key.get()+off, &d_id, meta.cols[1].len);
            std::vector<Rid> results;
            if (ih->get_value(key.get(), &results, nullptr) && !results.empty()) {
                auto dfh = sm_->fhs_.at("district").get();
                auto rec = dfh->get_record(results[0], nullptr);
                int d_next = *(int*)(rec->data + dtab.get_col("d_next_o_id")->offset);
                return d_next;
            }
        }
        // 回退：顺序扫描
        auto dfh = sm_->fhs_.at("district").get();
        RmScan scan(dfh);
        while (!scan.is_end()) {
            auto rec = dfh->get_record(scan.rid(), nullptr);
            int dw = *(int*)(rec->data + dtab.get_col("d_w_id")->offset);
            int dd = *(int*)(rec->data + dtab.get_col("d_id")->offset);
            if (dw == w_id && dd == d_id) {
                int d_next = *(int*)(rec->data + dtab.get_col("d_next_o_id")->offset);
                return d_next;
            }
            scan.next();
        }
    } catch (...) {
        // ignore
    }
    return -1;
}
