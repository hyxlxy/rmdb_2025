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
#include "execution_manager.h"
#include "mvcc_executor_base.h"
#include "fliter.h"
#include "index/ix.h"
#include "system/sm.h"
#include "parser/ast.h"

class NestedLoopJoinExecutor : public MVCCExecutorBase
{
private:
    std::unique_ptr<AbstractExecutor> left_;  // 左儿子节点（需要join的表）
    std::unique_ptr<AbstractExecutor> right_; // 右儿子节点（需要join的表）
    size_t len_;                              // join后获得的每条记录的长度
    std::vector<ColMeta> cols_;               // join后获得的记录的字段

    std::vector<Condition> fed_conds_; // join条件
    JoinType join_type_;
    bool is_end_;

    // Fliter: 预计算 join 条件列的偏移，避免每次 tuple 对比时查找列元数据
    Fliter *fliter_ = nullptr;
    // 预分配合并记录缓冲区，Next() 复用
    char *join_buf_ = nullptr;

public:
    NestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                           std::vector<Condition> conds, JoinType join_type = SEMI_JOIN)
    {
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();

        // 计算右表列的偏移量（在合并记录中）
        size_t left_tuple_len = left_->tupleLen();
        for (auto &col : right_cols)
        {
            col.offset += left_tuple_len;
        }

        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        is_end_ = false;
        fed_conds_ = std::move(conds);
        join_type_ = join_type;

        // 构建 Fliter：为每个 join 条件预计算左右列的偏移
        // do_predict(left_rec, right_rec) 版本：left 偏移相对左记录，right 偏移相对右记录
        const auto &lcols = left_->cols();
        const auto &rcols = right_->cols(); // 此时 right_cols 已调整过，用原始 right_->cols()
        std::vector<MiniCol> cond_cols;
        for (const auto &cond : fed_conds_) {
            // 左列：在左记录中的偏移
            MiniCol lm;
            for (const auto &c : lcols) {
                if (c.tab_name == cond.lhs_col.tab_name && c.name == cond.lhs_col.col_name) {
                    lm.offset = c.offset;
                    lm.len    = c.len;
                    lm.type   = c.type;
                    break;
                }
            }
            cond_cols.push_back(lm);
            // 右列：在右记录中的偏移（原始，不加 left_tuple_len）
            MiniCol rm;
            if (!cond.is_rhs_val) {
                for (const auto &c : rcols) {
                    if (c.tab_name == cond.rhs_col.tab_name && c.name == cond.rhs_col.col_name) {
                        rm.offset = c.offset;
                        rm.len    = c.len;
                        rm.type   = c.type;
                        break;
                    }
                }
            }
            cond_cols.push_back(rm);
        }
        fliter_ = new Fliter(fed_conds_, cond_cols);

        // 预分配合并缓冲区
        if (len_ > 0) join_buf_ = new char[len_];
    }

    ~NestedLoopJoinExecutor() override {
        delete fliter_;
        delete[] join_buf_;
    }

    const std::vector<ColMeta> &cols() const override
    {
        return cols_;
    }

    void beginTuple() override
    {
        left_->beginTuple();
        right_->beginTuple();
        if (left_->is_end() || right_->is_end())
        {
            is_end_ = true;
            return;
        }
        find_record();
    }

    void nextTuple() override
    {
        if (is_end())
            return;
        left_->nextTuple();
        if (left_->is_end())
        {
            right_->nextTuple();
            left_->beginTuple();
        }
        find_record();
    }

    std::unique_ptr<RmRecord> Next() override
    {
        if (is_end()) return nullptr;

        auto left_record  = left_->Next();
        auto right_record = right_->Next();
        if (!left_record || !right_record) return nullptr;

        // 使用预分配缓冲区，返回非拥有 RmRecord
        memcpy(join_buf_,                    left_record->data,  left_->tupleLen());
        memcpy(join_buf_ + left_->tupleLen(), right_record->data, right_->tupleLen());

        auto rec = std::make_unique<RmRecord>();
        rec->data       = join_buf_;
        rec->size       = static_cast<int>(len_);
        rec->allocated_ = false;
        return rec;
    }

    bool is_end() const override { return is_end_; }

    Rid &rid() override { return _abstract_rid; }

    void find_record()
    {
        while (!right_->is_end())
        {
            if (!left_->is_end())
            {
                auto left_record  = left_->Next();
                auto right_record = right_->Next();

                if (left_record && right_record &&
                    fliter_->do_predict(left_record, right_record))
                {
                    return;
                }
            }

            left_->nextTuple();
            if (left_->is_end())
            {
                right_->nextTuple();
                left_->beginTuple();
            }
        }
        is_end_ = true;
    }

    size_t tupleLen() const override { return len_; }

    std::string getType() override { return "NestedLoopJoinExecutor"; }
};
