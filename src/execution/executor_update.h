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

#include <utility>
#include <vector>

#include "execution_defs.h"
#include "execution_manager.h"
#include "mvcc_executor_base.h"
#include "index/ix.h"
#include "system/sm.h"
#include "expression_evaluator.h"

class UpdateExecutor : public MVCCExecutorBase
{
private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager *sm_manager_;
    bool need_update_index_; // 预计算：set子句中是否有列涉及索引

    // 检查 set_clauses 中是否有列属于某个索引
    bool compute_need_update_index() const
    {
        for (auto &[index_name, index] : tab_.indexes)
        {
            for (auto &sc : set_clauses_)
            {
                for (int i = 0; i < index.col_num; ++i)
                {
                    if (index.cols[i].name == sc.lhs.col_name)
                        return true;
                }
            }
        }
        return false;
    }

private:
    Value evaluate_set_clause(SetClause &set_clause, RmRecord *record, const ColMeta &col_meta)
    {
        if (set_clause.expr != nullptr)
        {
            ExpressionEvaluator evaluator(tab_.cols, record);
            Value new_value = evaluator.evaluate(set_clause.expr);

            if (col_meta.type == TYPE_STRING && new_value.type != TYPE_STRING)
            {
                new_value.set_str(new_value.to_string());
            }
            else if (col_meta.type == TYPE_INT && new_value.type != TYPE_INT)
            {
                if (new_value.type == TYPE_FLOAT)
                {
                    new_value.set_int(static_cast<int>(new_value.float_val));
                }
                else if (new_value.type == TYPE_STRING)
                {
                    new_value.set_int(std::stoi(new_value.str_val));
                }
            }
            else if (col_meta.type == TYPE_FLOAT && new_value.type != TYPE_FLOAT)
            {
                if (new_value.type == TYPE_INT)
                {
                    new_value.set_float(static_cast<float>(new_value.int_val));
                }
                else if (new_value.type == TYPE_STRING)
                {
                    new_value.set_float(std::stof(new_value.str_val));
                }
            }

            if (new_value.raw == nullptr)
            {
                new_value.init_raw(col_meta.len);
            }
            return new_value;
        }

        if (set_clause.rhs.raw == nullptr)
        {
            set_clause.rhs.init_raw(col_meta.len);
        }
        return set_clause.rhs;
    }

public:
    UpdateExecutor(SmManager *sm_manager, std::string tab_name, std::vector<SetClause> set_clauses,
                   std::vector<Condition> conds, std::vector<Rid> rids, Context *context)
    {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        set_clauses_ = std::move(set_clauses);
        tab_ = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        conds_ = std::move(conds);
        rids_ = std::move(rids);
        context_ = context;
        if (context_)
        {
            context_->current_table_name_ = tab_name_;
        }
        need_update_index_ = compute_need_update_index();
    }

    std::unique_ptr<RmRecord> Next() override
    {
        for (auto &rid : rids_)
        {
            if (context_ && context_->txn_ && context_->lock_mgr_)
            {
                if (!context_->lock_mgr_->lock_exclusive_on_record(context_->txn_, rid, fh_->GetFd()))
                {
                    throw TransactionAbortException(context_->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
                }
            }

            auto updated_record = get_record_mvcc(fh_, rid, context_, sm_manager_);
            if (!updated_record)
            {
                continue;
            }

            auto old_record = std::make_unique<RmRecord>(*updated_record);

            for (auto &set_clause : set_clauses_)
            {
                const ColMeta &col_meta = *tab_.get_col(set_clause.lhs.col_name);
                Value new_value = evaluate_set_clause(set_clause, updated_record.get(), col_meta);

                if (new_value.raw == nullptr || new_value.raw->data == nullptr)
                {
                    throw std::runtime_error("Failed to materialize value for update");
                }

                memcpy(updated_record->data + col_meta.offset, new_value.raw->data, col_meta.len);
            }

            std::vector<std::pair<std::string, std::unique_ptr<char[]>>> new_index_keys;

            if (need_update_index_)
            {
            for (auto &[index_name, index] : tab_.indexes)
            {
                auto ih = sm_manager_->ihs_.at(index_name).get();
                std::unique_ptr<char[]> old_key(new char[index.col_tot_len]);
                std::unique_ptr<char[]> new_key(new char[index.col_tot_len]);
                int offset = 0;

                for (int i = 0; i < index.col_num; ++i)
                {
                    memcpy(old_key.get() + offset, old_record->data + index.cols[i].offset, index.cols[i].len);
                    memcpy(new_key.get() + offset, updated_record->data + index.cols[i].offset, index.cols[i].len);
                    offset += index.cols[i].len;
                }

                if (memcmp(old_key.get(), new_key.get(), index.col_tot_len) == 0)
                {
                    continue;
                }

                Rid unique_rid{};
                if (!ih->is_unique(new_key.get(), unique_rid, nullptr) && rid != unique_rid)
                {
                    throw UniqueCheckError();
                }

                ih->delete_entry(old_key.get(), context_->txn_);
                if (context_ && context_->txn_)
                {
                    RmRecord deleted_index_record(index.col_tot_len);
                    memcpy(deleted_index_record.data, old_key.get(), index.col_tot_len);
                    auto *delete_index_write = new WriteRecord(WType::IX_DELETE_TUPLE, index_name, rid, deleted_index_record);
                    context_->txn_->append_write_record(delete_index_write);
                }

                auto key_copy = std::make_unique<char[]>(index.col_tot_len);
                memcpy(key_copy.get(), new_key.get(), index.col_tot_len);
                new_index_keys.emplace_back(index_name, std::move(key_copy));
            }
            } // end if (need_update_index_)

            if (!update_record_mvcc(fh_, rid, updated_record->data, context_, sm_manager_))
            {
                throw TransactionAbortException(context_->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
            }

            for (auto &[index_name, new_key] : new_index_keys)
            {
                auto ih = sm_manager_->ihs_.at(index_name).get();
                ih->insert_entry(new_key.get(), rid, context_->txn_);
                if (context_ && context_->txn_)
                {
                    const auto &index = tab_.indexes.at(index_name);
                    RmRecord inserted_index_record(index.col_tot_len);
                    memcpy(inserted_index_record.data, new_key.get(), index.col_tot_len);
                    auto *insert_index_write = new WriteRecord(WType::IX_INSERT_TUPLE, index_name, rid, inserted_index_record);
                    context_->txn_->append_write_record(insert_index_write);
                }
            }

            if (context_ && context_->txn_)
            {
                auto *write_record = new WriteRecord(WType::UPDATE_TUPLE, tab_name_, rid, *old_record);
                context_->txn_->append_write_record(write_record);
            }
        }

        return nullptr;
    }

    Rid &rid() override { return _abstract_rid; }
};
