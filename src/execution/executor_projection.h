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
#include "index/ix.h"
#include "system/sm.h"

// 连续块描述：将相邻列合并为一次 memcpy
struct ProjectionRun {
    size_t src_offset; // 原始记录中的起始偏移
    size_t dst_offset; // 投影记录中的起始偏移
    size_t len;        // 连续块总长度
};

class ProjectionExecutor : public MVCCExecutorBase
{
private:
    std::unique_ptr<AbstractExecutor> prev_; // 投影节点的儿子节点
    std::vector<ColMeta> cols_;              // 需要投影的字段
    size_t len_;                             // 字段总长度
    std::vector<size_t> sel_idxs_;           // 选中列在原始记录中的索引位置
    std::vector<ProjectionRun> runs_;        // run-length encoding：连续块列表
    char *proj_buf_ = nullptr;              // 预分配投影缓冲区，复用避免每次 malloc

public:
    /**
     * @brief ProjectionExecutor构造函数
     *
     * @param prev 子执行器，提供原始数据源
     * @param sel_cols 需要投影的列信息
     *
     * 构造函数执行以下工作：
     * 1. 接收子执行器和选择的列信息
     * 2. 建立列索引映射关系
     * 3. 重新计算投影后的列偏移量和总长度
     */
    ProjectionExecutor(std::unique_ptr<AbstractExecutor> prev, const std::vector<TabCol> &sel_cols)
    {

        prev_ = std::move(prev);

        size_t curr_offset = 0;
        auto &prev_cols = prev_->cols();

        // 检查是否是聚合执行器的输出（列没有表名）
        bool is_agg_output = !prev_cols.empty() && prev_cols[0].tab_name.empty();

        // 如果sel_cols为空且是聚合输出，使用所有子执行器的输出列
        if (sel_cols.empty() && is_agg_output)
        {
            for (size_t i = 0; i < prev_cols.size(); i++)
            {
                sel_idxs_.push_back(i);
                auto col = prev_cols[i];
                col.offset = curr_offset;
                curr_offset += col.len;
                cols_.push_back(col);
            }
        }
        else
        {
            // 正常的列投影逻辑
            for (size_t i = 0; i < sel_cols.size(); i++)
            {
                auto &sel_col = sel_cols[i];

                if (is_agg_output)
                {
                    // 对于聚合执行器的输出，直接按索引匹配
                    if (i < prev_cols.size())
                    {
                        sel_idxs_.push_back(i);
                        auto col = prev_cols[i];
                        col.offset = curr_offset;
                        curr_offset += col.len;
                        cols_.push_back(col);
                    }
                    else
                    {
                        // 如果索引超出范围，跳过这个列
                        std::cerr << "WARNING: ProjectionExecutor - sel_col index " << i << " out of range for aggregation output" << std::endl;
                    }
                }
                else
                {
                    // 对于普通执行器的输出，按列名匹配
                    try
                    {
                        auto pos = get_col(prev_cols, sel_col);
                        sel_idxs_.push_back(pos - prev_cols.begin());
                        auto col = *pos;
                        col.offset = curr_offset;
                        curr_offset += col.len;
                        cols_.push_back(col);
                    }
                    catch (const ColumnNotFoundError &e)
                    {
                        std::cerr << "ERROR: ProjectionExecutor - Column not found: " << sel_col.tab_name << "." << sel_col.col_name << std::endl;
                        throw;
                    }
                }
            }
        }
        len_ = curr_offset;

        // **构建 run-length encoding：将物理地址连续的列合并为单次 memcpy**
        if (!sel_idxs_.empty()) {
            const auto &prev_cols_ref = prev_->cols();
            ProjectionRun cur;
            cur.src_offset = prev_cols_ref[sel_idxs_[0]].offset;
            cur.dst_offset = cols_[0].offset;
            cur.len        = prev_cols_ref[sel_idxs_[0]].len;

            for (size_t i = 1; i < sel_idxs_.size(); ++i) {
                size_t pi = sel_idxs_[i - 1];
                size_t ci = sel_idxs_[i];
                size_t prev_end   = prev_cols_ref[pi].offset + prev_cols_ref[pi].len;
                size_t curr_start = prev_cols_ref[ci].offset;
                if (prev_end == curr_start) {
                    cur.len += prev_cols_ref[ci].len; // 扩展当前 run
                } else {
                    runs_.push_back(cur);
                    cur.src_offset = curr_start;
                    cur.dst_offset = cols_[i].offset;
                    cur.len        = prev_cols_ref[ci].len;
                }
            }
            runs_.push_back(cur);
        }

        // 预分配投影缓冲区，整个生命周期复用
        if (len_ > 0) proj_buf_ = new char[len_];
    }

    ~ProjectionExecutor() override {
        delete[] proj_buf_;
    }

    /**
     * @brief 开始元组遍历
     * 委托给子执行器处理
     */
    void beginTuple() override
    {
        prev_->beginTuple();
    }

    /**
     * @brief 移动到下一个元组
     * 委托给子执行器处理
     */
    void nextTuple() override
    {
        prev_->nextTuple();
    }

    /**
     * @brief 检查是否到达结束位置
     * @return bool 如果子执行器结束则返回true
     */
    bool is_end() const override
    {
        return prev_->is_end();
    }

    /**
     * @brief 获取投影后的元组长度
     * @return size_t 投影后记录的总长度
     */
    size_t tupleLen() const override
    {
        return len_;
    }

    /**
     * @brief 获取投影后的列元数据
     * @return const std::vector<ColMeta>& 投影后的列信息
     */
    const std::vector<ColMeta> &cols() const override
    {
        return cols_;
    }

    /**
     * @brief 获取下一条投影后的记录
     *
     * @return std::unique_ptr<RmRecord> 投影后的记录，如果没有更多记录则返回nullptr
     *
     * 执行流程：
     * 1. 从子执行器获取下一条原始记录
     * 2. 根据选择的列索引提取对应数据
     * 3. 重新组织数据生成投影后的记录
     */
    std::unique_ptr<RmRecord> Next() override
    {
        // 从子执行器获取下一条记录
        auto prev_record = prev_->Next();
        if (prev_record == nullptr)
            return nullptr;

        // **使用 run-length encoding 减少 memcpy 次数**
        for (const auto &run : runs_) {
            memcpy(proj_buf_ + run.dst_offset,
                   prev_record->data + run.src_offset,
                   run.len);
        }

        // 返回指向预分配缓冲区的非拥有 RmRecord（allocated_=false，不会 double-free）
        auto rec = std::make_unique<RmRecord>();
        rec->data       = proj_buf_;
        rec->size       = static_cast<int>(len_);
        rec->allocated_ = false; // 缓冲区由 ProjectionExecutor 自己管理
        return rec;
    }

    /**
     * @brief 获取当前记录的位置标识符
     * @return Rid& 委托给子执行器的rid
     */
    Rid &rid() override
    {
        return prev_->rid();
    }

    /**
     * @brief 获取执行器类型名称
     * @return std::string 返回执行器类型标识
     */
    std::string getType() override
    {
        return "ProjectionExecutor";
    }
};
