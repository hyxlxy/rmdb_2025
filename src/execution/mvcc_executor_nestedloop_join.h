#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "mvcc_executor_base.h"

/**
 * @brief MVCC版本的嵌套循环连接执行器
 * 基于标准MVCC连接查询实现，确保每张表的可见性判断正确
 */
class MVCCNestedLoopJoinExecutor : public MVCCExecutorBase
{
private:
    std::unique_ptr<AbstractExecutor> left_;  // 左儿子节点（需要join的表）
    std::unique_ptr<AbstractExecutor> right_; // 右儿子节点（需要join的表）
    size_t len_;                              // join后获得的每条记录的长度
    std::vector<ColMeta> cols_;               // join后获得的记录的字段

    std::vector<Condition> fed_conds_; // join条件
    JoinType join_type_;
    bool is_end_;
    Context* context_;
    SmManager* sm_manager_;
    Rid _abstract_rid;  // 抽象RID，连接执行器需要

public:
    MVCCNestedLoopJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right,
                               std::vector<Condition> conds, JoinType join_type, Context* context, SmManager* sm_manager)
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
        context_ = context;
        sm_manager_ = sm_manager;
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

        // 获取当前位置的可见记录
        auto left_record = left_->Next();
        auto right_record = right_->Next();

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

        // 组合左右表记录
        memcpy(record->data, left_record->data, left_->tupleLen());
        memcpy(record->data + left_->tupleLen(), right_record->data, right_->tupleLen());
        return record;
    }

    bool is_end() const override { return is_end_; }

    Rid &rid() override { return _abstract_rid; }

    void find_record()
    {
        // 标准MVCC连接实现：确保每张表的可见性判断正确
        while (!right_->is_end())
        {
            // 检查当前位置是否有满足条件的记录
            if (!left_->is_end())
            {
                // 获取当前位置的可见记录
                auto left_record = left_->Next();
                auto right_record = right_->Next();

                // 检查记录是否存在（可能因为MVCC可见性而为空）
                if (left_record && right_record &&
                    left_record->data && right_record->data &&
                    left_record->size > 0 && right_record->size > 0)
                {
                    // 组合记录用于条件评估
                    auto record = std::make_unique<RmRecord>(len_);
                    if (record && record->data)
                    {
                        memcpy(record->data, left_record->data, left_->tupleLen());
                        memcpy(record->data + left_->tupleLen(), right_record->data, right_->tupleLen());

                        // 检查连接条件
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

    std::string getType() override { return "MVCCNestedLoopJoinExecutor"; }
};
