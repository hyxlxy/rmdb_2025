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
            if (context_ && context_->txn_)
            {
                RmRecord index_record(index.col_tot_len);
                memcpy(index_record.data, key, index.col_tot_len);
                auto *index_write = new WriteRecord(WType::IX_INSERT_TUPLE, index_name, rid_, index_record);
                context_->txn_->append_write_record(index_write);
            }
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
