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
#include "execution_defs.h"
#include <limits>
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "mvcc_executor_base.h"
#include "common/config.h"
#include "common/tpcc_consistency.h"
#include "common/tpcc_runtime_config.h"
#include "executor_seq_scan.h"
#include "tpcc_runtime_coordinator.h"
#include "tpcc_index_utils.h"

class InsertExecutor : public MVCCExecutorBase
{
private:
    TabMeta tab_;               // 表的元数据
    std::vector<Value> values_; // 需要插入的数据
    RmFileHandle *fh_;          // 表的数据文件句柄
    std::string tab_name_;      // 表名称
    Rid rid_;                   // 插入的位置，由于系统默认插入时不指定位置，因此当前rid_在插入后才赋值
    SmManager *sm_manager_;
    bool inserted_ = false; // [唯一性修复标记] 是否已经插入过，防止多次插入

    /**
     * @brief 验证订单ID的一致性（运行时检查）
     */
    bool validate_order_id_consistency(int w_id, int d_id, int o_id) {
        try {
            // 优先使用 district(d_w_id,d_id) 复合索引点查 d_next_o_id
            TabMeta &dtab = sm_manager_->db_.get_table("district");
            std::string idx_name;
            for (auto &[iname, meta] : dtab.indexes) {
                if (meta.col_num == 2 && meta.cols[0].name == "d_w_id" && meta.cols[1].name == "d_id") { idx_name = iname; break; }
            }
            if (!idx_name.empty()) {
                auto ih = sm_manager_->ihs_.at(idx_name).get();
                int key_len = dtab.indexes[idx_name].col_tot_len;
                std::unique_ptr<char[]> key(new char[key_len]);
                int off = 0;
                memcpy(key.get()+off, &w_id, sizeof(int)); off += sizeof(int);
                memcpy(key.get()+off, &d_id, sizeof(int));
                std::vector<Rid> results;
                if (ih->get_value(key.get(), &results, context_?context_->txn_:nullptr) && !results.empty()) {
                    auto dfh = sm_manager_->fhs_.at("district").get();
                    auto rec = dfh->get_record(results[0], context_);
                    auto d_next_col = dtab.get_col("d_next_o_id");
                    int expected_o_id = *(int*)(rec->data + d_next_col->offset);
                    return (o_id == expected_o_id);
                }
                return false;
            }
            // 回退：顺序扫描（极端情况下）
            Context temp_context(nullptr, nullptr, nullptr);
            SeqScanExecutor scanner(sm_manager_, "district", {}, &temp_context);
            auto record = scanner.Next();
            if (record) {
                auto d_next_col = dtab.get_col("d_next_o_id");
                int expected_o_id = *(int*)(record->data + d_next_col->offset);
                return (o_id == expected_o_id);
            }
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Error validating order ID consistency: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief 使用运行时协调器验证订单ID一致性
     * 这是解决您的d_next_o_id不一致问题的关键函数
     */
    // 废弃：不再用“只校验”推进分配器；改为真正分配并回写
    bool validate_order_id_with_coordinator(int, int, int) { return true; }

    /**
     * @brief 验证orders记录是否存在（运行时检查）——优先使用复合索引 (o_w_id,o_d_id,o_id)
     */
    bool validate_order_exists(int w_id, int d_id, int o_id) {
        try {
            TabMeta &otab = sm_manager_->db_.get_table("orders");
            // 查找匹配的复合索引名
            std::string orders_index_name;
            for (auto &[iname, meta] : otab.indexes) {
                if (meta.col_num == 3 && meta.cols[0].name == "o_w_id" && meta.cols[1].name == "o_d_id" && meta.cols[2].name == "o_id") {
                    orders_index_name = iname; break;
                }
            }
            if (!orders_index_name.empty()) {
                auto ih = sm_manager_->ihs_.at(orders_index_name).get();
                const auto &meta = otab.indexes[orders_index_name];
                std::unique_ptr<char[]> key(new char[meta.col_tot_len]);
                int off = 0;
                // 构造复合键（按索引列顺序）
                memcpy(key.get()+off, &w_id, meta.cols[0].len); off += meta.cols[0].len;
                memcpy(key.get()+off, &d_id, meta.cols[1].len); off += meta.cols[1].len;
                memcpy(key.get()+off, &o_id, meta.cols[2].len);
                std::vector<Rid> results;
                if (ih->get_value(key.get(), &results, context_?context_->txn_:nullptr) && !results.empty()) {
                    return true;
                }
                return false;
            }
            // 回退：使用条件扫描
            std::vector<Condition> conditions;
            Condition w_cond; w_cond.lhs_col = {"orders", "o_w_id"}; w_cond.op = OP_EQ; w_cond.is_rhs_val = true; w_cond.rhs_val.set_int(w_id); w_cond.rhs_val.init_raw(sizeof(int)); conditions.push_back(w_cond);
            Condition d_cond; d_cond.lhs_col = {"orders", "o_d_id"}; d_cond.op = OP_EQ; d_cond.is_rhs_val = true; d_cond.rhs_val.set_int(d_id); d_cond.rhs_val.init_raw(sizeof(int)); conditions.push_back(d_cond);
            Condition o_cond; o_cond.lhs_col = {"orders", "o_id"};  o_cond.op = OP_EQ;  o_cond.is_rhs_val = true; o_cond.rhs_val.set_int(o_id); o_cond.rhs_val.init_raw(sizeof(int));  conditions.push_back(o_cond);
            Context temp_context(nullptr, nullptr, nullptr);
            SeqScanExecutor scanner(sm_manager_, "orders", conditions, &temp_context);
            auto record = scanner.Next();
            return (record != nullptr);
        } catch (const std::exception& e) {
            std::cerr << "Error validating order existence: " << e.what() << std::endl;
            return false;
        }
    }

public:
    InsertExecutor(SmManager *sm_manager, const std::string &tab_name, std::vector<Value> values, Context *context)
    {
        sm_manager_ = sm_manager;
        tab_ = sm_manager_->db_.get_table(tab_name);
        values_ = values;
        tab_name_ = tab_name;
        if (values.size() != tab_.cols.size())
        {
            throw InvalidValueCountError();
        }
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        context_ = context;
        if (context_)
        {
            context_->current_table_name_ = tab_name;
        }
        // 初始化协调器（幂等）
        TPCCRuntimeCoordinator::init(sm_manager_);
    };

    std::unique_ptr<RmRecord> Next() override
    {
        // [唯一性修复标记] 防止多次插入
        if (inserted_)
            return nullptr;
        inserted_ = true;
        // Make record buffer
        RmRecord rec(fh_->get_file_hdr().record_size);
        for (size_t i = 0; i < values_.size(); i++)
        {
            auto &col = tab_.cols[i];
            auto &val = values_[i];

            // 类型检查和自动转换
            if (col.type != val.type)
            {
                // 允许INT到FLOAT的自动转换
                if (col.type == TYPE_FLOAT && val.type == TYPE_INT)
                {
                    val.set_float(static_cast<float>(val.int_val));
                }
                // 允许FLOAT到INT的转换（可能丢失精度）
                else if (col.type == TYPE_INT && val.type == TYPE_FLOAT)
                {
                    val.set_int(static_cast<int>(val.float_val));
                }
            }

            val.init_raw(col.len);
            memcpy(rec.data + col.offset, val.raw->data, col.len);
        }
        // 注意：TPCC 的 o_id 由上层 SQL 传入（通常来自 district.d_next_o_id 的读取）
        // 这里不再覆盖 o_id，保持与 new_orders/order_line 引用一致，避免跨语句不一致
        // 先检查 key 是否是 unique
        for (auto &[index_name, index] : tab_.indexes)
        {
            auto ih = sm_manager_->ihs_.at(index_name).get();
            int offset = 0;
            char *key = new char[index.col_tot_len];
            for (int i = 0; i < index.col_num; ++i)
            {
                memcpy(key + offset, rec.data + index.cols[i].offset, index.cols[i].len);
                offset += index.cols[i].len;
            }
            Rid unique_rid{};
            if (!ih->is_unique(key, unique_rid, context_->txn_))
            {
                delete[] key;
                inserted_ = true;         // [唯一性修复标记] 唯一性冲突时也标记已插入，防止异常后再次插入
                throw UniqueCheckError(); // 直接抛出异常，阻止后续插入
            }
            delete[] key;
        }
        // 使用普通插入操作
        rid_ = fh_->insert_record(rec.data, context_);

        // **关键修复：TPC-C运行时一致性保证**
        if (TPCC_CONSISTENCY_MODE) {
            int w_id = 0, d_id = 0, o_id = 0, o_ol_cnt = 0;

            if (tab_name_ == "orders" && values_.size() >= 7) {
                w_id = values_[2].int_val;  // o_w_id
                d_id = values_[1].int_val;  // o_d_id
                o_id = values_[0].int_val;  // o_id
                o_ol_cnt = values_[6].int_val;  // o_ol_cnt

                TPCCConsistencyManager::record_order_insert(w_id, d_id, o_id, o_ol_cnt);
            } else if (tab_name_ == "order_line" && values_.size() >= 3) {
                w_id = values_[2].int_val;  // ol_w_id
                d_id = values_[1].int_val;  // ol_d_id
                o_id = values_[0].int_val;  // ol_o_id

                // 软校验：仅在启用强制校验时才阻塞
                if (TPCCRuntimeConfig::enforce_runtime_validation.load()) {
                    if (!orders_exists_via_index(sm_manager_, w_id, d_id, o_id, context_?context_->txn_:nullptr)) {
                        inserted_ = false; // 重置插入标记，允许重试
                        throw std::runtime_error(std::string("Order line consistency violation: no corresponding orders record found for ") +
                                               "order (w_id=" + std::to_string(w_id) +
                                               ", d_id=" + std::to_string(d_id) +
                                               ", o_id=" + std::to_string(o_id) + ")");
                    }
                }

                TPCCConsistencyManager::record_order_line_insert(w_id, d_id, o_id);
            } else if (tab_name_ == "new_orders" && values_.size() >= 3) {
                w_id = values_[2].int_val;  // no_w_id
                d_id = values_[1].int_val;  // no_d_id
                o_id = values_[0].int_val;  // no_o_id

                // 软校验：仅在启用强制校验时才阻塞
                if (TPCCRuntimeConfig::enforce_runtime_validation.load()) {
                    if (!orders_exists_via_index(sm_manager_, w_id, d_id, o_id, context_?context_->txn_:nullptr)) {
                        inserted_ = false; // 重置插入标记，允许重试
                        throw std::runtime_error(std::string("New order consistency violation: no corresponding orders record found for ") +
                                               "order (w_id=" + std::to_string(w_id) +
                                               ", d_id=" + std::to_string(d_id) +
                                               ", o_id=" + std::to_string(o_id) + ")");
                    }
                }
            }
        }

        //  写入write_set，便于事务回滚
        if (context_ && context_->txn_)
        {
            WriteRecord *write_record = new WriteRecord(WType::INSERT_TUPLE, tab_name_, rid_, rec);
            context_->txn_->append_write_record(write_record);
        }
        for (auto &[index_name, index] : tab_.indexes)
        {
            auto ih = sm_manager_->ihs_.at(index_name).get();
            char *key = new char[index.col_tot_len];
            int offset = 0;
            for (int i = 0; i < index.col_num; ++i)
            {
                memcpy(key + offset, rec.data + index.cols[i].offset, index.cols[i].len);
                offset += index.cols[i].len;
            }
            ih->insert_entry(key, rid_, context_->txn_);
            delete[] key;
        }
        // 在插入操作完成后
        RmFileHdr file_hdr = fh_->get_file_hdr();
        file_hdr.count_cache_valid = false;  // 失效缓存
        fh_->update_file_hdr(file_hdr);
        return nullptr;
    }

    Rid &rid() override { return rid_; }
};
