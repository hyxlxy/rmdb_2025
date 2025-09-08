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

#include "mvcc_executor_base.h"
#include "execution_defs.h"
#include "execution_manager.h"
#include "index/ix.h"
#include "system/sm.h"

/**
 * @brief MVCC感知的插入执行器
 * 支持多版本并发控制的数据插入
 */
class MVCCInsertExecutor : public MVCCExecutorBase
{
private:
    TabMeta tab_;               // 表的元数据
    std::vector<Value> values_; // 需要插入的数据
    RmFileHandle *fh_;          // 表的数据文件句柄
    std::string tab_name_;      // 表名称
    Rid rid_;                   // 插入的位置
    SmManager *sm_manager_;
    bool inserted_ = false; // 是否已经插入过，防止多次插入

public:
    MVCCInsertExecutor(SmManager *sm_manager, const std::string &tab_name,
                       std::vector<Value> values, Context *context)
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
    }

    std::unique_ptr<RmRecord> Next() override
    {
        // 防止多次插入
        if (inserted_)
            return nullptr;
        inserted_ = true;

        // **完全兼容模式：直接使用普通插入逻辑，确保稳定性**
        return execute_regular_insert();
    }

    /**
     * @brief 回退到普通插入逻辑（当没有事务上下文时）
     */
    std::unique_ptr<RmRecord> execute_regular_insert()
    {

        // 构建记录缓冲区
        RmRecord rec(fh_->get_file_hdr().record_size);
        for (size_t i = 0; i < values_.size(); i++)
        {
            auto &col = tab_.cols[i];
            auto &val = values_[i];

            // 类型检查和自动转换（与普通InsertExecutor保持一致）
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
                // 其他类型不兼容
                else
                {
                    throw IncompatibleTypeError(coltype2str(col.type), coltype2str(val.type));
                }
            }

            val.init_raw(col.len);
            memcpy(rec.data + col.offset, val.raw->data, col.len);
        }

        // 执行唯一性约束验证
        for (const auto &index_descriptor : tab_.indexes)
        {
            const std::string &index_id = index_descriptor.first;
            const IndexMeta &index_spec = index_descriptor.second;

            auto index_accessor = sm_manager_->ihs_.at(index_id).get();

            char *constraint_key = new char[index_spec.col_tot_len];
            int key_offset = 0;
            for (int i = 0; i < index_spec.col_num; ++i)
            {
                memcpy(constraint_key + key_offset, rec.data + index_spec.cols[i].offset, index_spec.cols[i].len);
                key_offset += index_spec.cols[i].len;
            }

            // 检查唯一性
            Rid conflicting_record{};
            bool violates_uniqueness = !index_accessor->is_unique(constraint_key, conflicting_record, nullptr);
            if (violates_uniqueness)
            {
                delete[] constraint_key;
                throw UniqueCheckError();
            }
            delete[] constraint_key;
        }

        // 使用MVCC感知的插入操作
        rid_ = insert_record_mvcc(fh_, rec.data, context_, sm_manager_);

        // 记录WriteRecord到事务的write_set中
        if (context_ && context_->txn_)
        {
            WriteRecord *write_record = new WriteRecord(WType::INSERT_TUPLE, tab_name_, rid_, rec);
            context_->txn_->append_write_record(write_record);
        }

        // 更新索引
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

        return nullptr;
    }

    Rid &rid() override
    {
        return rid_;
    }

    std::string getType() override
    {
        return "MVCCInsertExecutor";
    }
};
