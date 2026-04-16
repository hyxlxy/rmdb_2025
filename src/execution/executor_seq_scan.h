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

#include <unordered_set>
#include "execution_defs.h"
#include "execution_manager.h"
#include "mvcc_executor_base.h"
#include "index/ix.h"
#include "system/sm.h"

class SeqScanExecutor : public MVCCExecutorBase {
   private:
    std::string tab_name_;              // 表的名称
    std::vector<Condition> conds_;      // scan的条件
    RmFileHandle *fh_;                  // 表的数据文件句柄
    std::vector<ColMeta> cols_;         // 原始表的全部列（用于条件求值）
    size_t len_;                        // 原始记录长度
    std::vector<Condition> fed_conds_;

    // 投影下推：输出列元数据、偏移、长度、缓冲区
    std::vector<ColMeta> proj_cols_;    // 投影后的列（含调整后的 offset）
    std::vector<int>     proj_offsets_; // 原始记录中各投影列的 offset
    std::vector<int>     proj_lens_;    // 各投影列的长度
    size_t               proj_len_ = 0; // 投影后记录总长度
    char                *proj_buf_ = nullptr; // 预分配投影缓冲区

    Rid rid_;
    std::unique_ptr<RecScan> scan_;     // table_iterator

    SmManager *sm_manager_;

   public:
    SeqScanExecutor(SmManager *sm_manager, std::string tab_name,
                    std::vector<Condition> conds, Context *context,
                    std::vector<TabCol> projection_col = {}) {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        context_ = context;
        fed_conds_ = conds_;

        // 构建投影元数据（去重保序）
        if (!projection_col.empty()) {
            std::unordered_set<std::string> seen;
            size_t off = 0;
            for (auto &tc : projection_col) {
                if (seen.count(tc.col_name)) continue;
                seen.insert(tc.col_name);
                auto it = std::find_if(cols_.begin(), cols_.end(),
                    [&](const ColMeta &c){ return c.name == tc.col_name; });
                if (it == cols_.end()) continue;
                proj_offsets_.push_back(it->offset);
                proj_lens_.push_back(it->len);
                proj_len_ += it->len;
                ColMeta cm = *it;
                cm.offset = off;
                off += cm.len;
                proj_cols_.push_back(cm);
            }
            if (proj_len_ > 0) proj_buf_ = new char[proj_len_];
        }
    }

    ~SeqScanExecutor() override { delete[] proj_buf_; }

    void beginTuple() override
    {
        scan_ = std::make_unique<RmScan>(fh_);
        // 移动到第一个满足条件的记录（无条件时直接返回当前记录，避免不必要的get_record）
        while (!scan_->is_end())
        {
            rid_ = scan_->rid();
            if (context_ != nullptr)
            {
                context_->current_table_name_ = tab_name_;
            }
            if (fed_conds_.empty()) {
                return;
            }
            auto rec = get_record_mvcc(fh_, rid_, context_, sm_manager_);
            if (!rec)
            {
                scan_->next();
                continue;
            }
            if (eval_conds(cols_, fed_conds_, rec.get()))
            {
                return;
            }
            scan_->next();
        }
    }

    void nextTuple() override
    {
        if (scan_ == nullptr)
        {
            throw InternalError("Scan not initialized at " + getType());
        }
        if (!scan_->is_end())
        {
            scan_->next();
        }
        // 移动到下一个满足条件的记录（无条件时直接返回当前记录，避免不必要的get_record）
        while (!scan_->is_end())
        {
            rid_ = scan_->rid();
            if (context_ != nullptr)
            {
                context_->current_table_name_ = tab_name_;
            }
            if (fed_conds_.empty()) {
                return;
            }
            auto rec = get_record_mvcc(fh_, rid_, context_, sm_manager_);
            if (!rec)
            {
                scan_->next();
                continue;
            }
            if (eval_conds(cols_, fed_conds_, rec.get()))
            {
                return;
            }
            scan_->next();
        }
    }

    bool is_end() const override { return scan_ == nullptr || scan_->is_end(); }

    std::unique_ptr<RmRecord> Next()
    {
        if (scan_ == nullptr)
        {
            beginTuple();
        }
        if (is_end())
        {
            return nullptr;
        }
        if (context_ != nullptr)
        {
            context_->current_table_name_ = tab_name_;
        }
        auto record = get_record_mvcc(fh_, rid_, context_, sm_manager_);
        if (!record) return nullptr;

        // 投影下推：只返回需要的列
        if (proj_buf_ != nullptr) {
            for (size_t i = 0; i < proj_offsets_.size(); ++i) {
                memcpy(proj_buf_ + proj_cols_[i].offset,
                       record->data + proj_offsets_[i],
                       proj_lens_[i]);
            }
            auto rec = std::make_unique<RmRecord>();
            rec->data       = proj_buf_;
            rec->size       = static_cast<int>(proj_len_);
            rec->allocated_ = false;
            return rec;
        }
        return record;
    }

    size_t tupleLen() const override { return proj_buf_ ? proj_len_ : len_; }

    const std::vector<ColMeta> &cols() const override {
        return proj_buf_ ? proj_cols_ : cols_;
    }

    Rid &rid() override { return rid_; }

    std::string getType() override { return "SeqScanExecutor"; }

    public:
    // 检查是否没有过滤条件
    bool has_no_conditions() const {
        return fed_conds_.empty();
    }
    
    // 获取表的文件句柄（用于访问缓存的记录数）
    RmFileHandle* get_file_handle() const {
        return fh_;
    }
};
