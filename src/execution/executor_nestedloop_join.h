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
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "parser/ast.h"

class NestedLoopJoinExecutor : public AbstractExecutor
{
private:
    std::unique_ptr<AbstractExecutor> left_;  // 左儿子节点（需要join的表）
    std::unique_ptr<AbstractExecutor> right_; // 右儿子节点（需要join的表）
    size_t len_;                              // join后获得的每条记录的长度
    std::vector<ColMeta> cols_;               // join后获得的记录的字段

    std::vector<Condition> fed_conds_; // join条件
    JoinType join_type_;
    bool is_end_;

public:
    NestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                           std::vector<Condition> conds, JoinType join_type = SEMI_JOIN)
    {
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();

        // 计算右表列的偏移量
        size_t left_tuple_len = left_->tupleLen();
        for (auto &col : right_cols)
        {
            col.offset += left_tuple_len;
        }

        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        is_end_ = false;
        fed_conds_ = std::move(conds);
        join_type_ = join_type;
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
        if (is_end()) {
            return nullptr;
        }

        // 获取当前位置的记录
        auto left_record = left_->Next();
        auto right_record = right_->Next();

        // 安全检查：确保记录存在
        if (!left_record || !right_record) {
            return nullptr;
        }

        // 安全检查：确保记录数据存在
        if (!left_record->data || !right_record->data) {
            return nullptr;
        }

        // 安全检查：确保记录大小有效
        if (left_record->size <= 0 || right_record->size <= 0) {
            return nullptr;
        }

        // 创建组合记录
        auto record = std::make_unique<RmRecord>(len_);
        if (!record || !record->data) {
            return nullptr;
        }

        memcpy(record->data, left_record->data, left_->tupleLen());
        memcpy(record->data + left_->tupleLen(), right_record->data, right_->tupleLen());
        return record;
    }

    bool is_end() const override { return is_end_; }

    Rid &rid() override { return _abstract_rid; }

    void find_record()
    {
        while (!right_->is_end())
        {
            // 检查当前位置是否有满足条件的记录
            if (!left_->is_end())
            {
                // 获取当前位置的记录进行条件检查
                auto left_record = left_->Next();
                auto right_record = right_->Next();

                // 安全检查：确保记录存在
                if (left_record && right_record &&
                    left_record->data && right_record->data &&
                    left_record->size > 0 && right_record->size > 0)
                {
                    // 创建组合记录进行条件检查
                    auto record = std::make_unique<RmRecord>(len_);
                    if (record && record->data)
                    {
                        memcpy(record->data, left_record->data, left_->tupleLen());
                        memcpy(record->data + left_->tupleLen(), right_record->data, right_->tupleLen());

                        if (fed_conds_.empty() || eval_conds(cols_, fed_conds_, record.get()))
                        {
                            return; // 找到满足条件的记录
                        }
                    }
                }
            }

            // 移动到下一个位置
            left_->nextTuple();
            if (left_->is_end())
            {
                right_->nextTuple();
                left_->beginTuple();
            }
        }
        is_end_ = true;
    }

    std::string getType() override { return "NestedLoopJoinExecutor"; }
};
