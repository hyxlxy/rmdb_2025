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
#include "executor_seq_scan.h"
#include "../record/rm_scan.h"
#include <set>
#include <string>

/**
 * @brief MVCC感知的顺序扫描执行器
 * 支持快照隔离级别的数据读取
 */
class MVCCSeqScanExecutor : public MVCCExecutorBase
{
private:
    SmManager *sm_manager_;
    std::string tab_name_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;
    std::unique_ptr<RmScan> scan_;
    Rid rid_;

public:
    MVCCSeqScanExecutor(SmManager *sm_manager, std::string tab_name,
                        std::vector<Condition> conds, Context *context)
    {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        context_ = context;
        fed_conds_ = conds_;

        if (context_)
        {
            context_->current_table_name_ = tab_name_;
        }
    }

    void beginTuple() override
    {
        scan_ = std::make_unique<RmScan>(fh_);
        int visible_count = 0;
        int total_count = 0;

        while (!scan_->is_end())
        {
            rid_ = scan_->rid();
            total_count++;

            try
            {
                if (fh_->is_record(rid_))
                {
                    // Physical record exists
                }
                else
                {
                    scan_->next();
                    continue;
                }
            }
            catch (...)
            {
                scan_->next();
                continue;
            }

            // **修复模式：临时设置表名，操作完成后恢复**
            std::string original_table_name;
            bool need_restore = false;

            if (context_ && context_->current_table_name_ != tab_name_)
            {
                original_table_name = context_->current_table_name_;
                context_->current_table_name_ = tab_name_;
                need_restore = true;
            }

            auto rec = get_record_mvcc(fh_, rid_, context_, sm_manager_);

            // 恢复原始表名
            if (need_restore)
            {
                context_->current_table_name_ = original_table_name;
            }

            if (rec)
            {
                visible_count++;

                if (eval_conds(cols_, fed_conds_, rec.get()))
                {
                    return;
                }
            }
            scan_->next();
        }

        if (visible_count == 0)
        {
            scan_.reset();
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

        while (!scan_->is_end())
        {
            rid_ = scan_->rid();

            // **修复模式：临时设置表名，操作完成后恢复**
            std::string original_table_name;
            bool need_restore = false;

            if (context_ && context_->current_table_name_ != tab_name_)
            {
                original_table_name = context_->current_table_name_;
                context_->current_table_name_ = tab_name_;
                need_restore = true;
            }

            auto rec = get_record_mvcc(fh_, rid_, context_, sm_manager_);

            // 恢复原始表名
            if (need_restore)
            {
                context_->current_table_name_ = original_table_name;
            }

            if (rec && eval_conds(cols_, fed_conds_, rec.get()))
            {
                return;
            }
            scan_->next();
        }
    }

    std::unique_ptr<RmRecord> Next() override
    {
        if (scan_ == nullptr || scan_->is_end())
        {
            return nullptr;
        }

        // **修复模式：临时设置表名，操作完成后恢复**
        std::string original_table_name;
        bool need_restore = false;

        if (context_ && context_->current_table_name_ != tab_name_)
        {
            original_table_name = context_->current_table_name_;
            context_->current_table_name_ = tab_name_;
            need_restore = true;
        }

        auto result = get_record_mvcc(fh_, rid_, context_, sm_manager_);

        // 恢复原始表名
        if (need_restore)
        {
            context_->current_table_name_ = original_table_name;
        }

        return result;
    }

    bool is_end() const override
    {
        return scan_ == nullptr || scan_->is_end();
    }

    size_t tupleLen() const override
    {
        return len_;
    }

    const std::vector<ColMeta> &cols() const override
    {
        return cols_;
    }

    std::string getType() override
    {
        return "MVCCSeqScanExecutor";
    }

    ColMeta get_col_offset(const TabCol &target) override
    {
        auto pos = std::find_if(cols_.begin(), cols_.end(), [&](const ColMeta &col)
                                { return col.tab_name == target.tab_name && col.name == target.col_name; });
        if (pos == cols_.end())
        {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
        return *pos;
    }

    Rid &rid() override
    {
        return rid_;
    }

    /**
     * @brief 获取扫描统计信息（用于调试）
     */
    struct ScanStats
    {
        int total_records_scanned = 0;
        int visible_records = 0;
        int filtered_records = 0;
    };

    ScanStats get_scan_stats()
    {
        ScanStats stats;

        // **简化统计信息收集，避免复杂的MVCC逻辑**
        auto temp_scan = std::make_unique<RmScan>(fh_);
        while (!temp_scan->is_end())
        {
            stats.total_records_scanned++;
            Rid temp_rid = temp_scan->rid();

            auto rec = fh_->get_record(temp_rid, context_);
            if (rec != nullptr)
            {
                stats.visible_records++;
                if (eval_conds(cols_, fed_conds_, rec.get()))
                {
                    stats.filtered_records++;
                }
            }
            temp_scan->next();
        }

        return stats;
    }

    /**
     * @brief 检查当前事务的快照一致性
     */
    bool validate_snapshot_consistency()
    {
        Transaction *txn = get_current_transaction(context_);
        if (txn == nullptr)
            return true;

        // 检查读取时间戳是否有效
        if (txn->get_read_ts() == INVALID_TIMESTAMP)
        {
            return false;
        }

        // 检查事务状态
        if (!txn->is_active())
        {
            return false;
        }

        return true;
    }

private:
    /**
     * @brief 获取逻辑记录的键（基于记录内容而不是物理位置）
     * @param rid 记录ID
     * @return 逻辑记录键
     */
    std::string get_logical_record_key(const Rid &rid)
    {
        // **完全兼容模式：简化键生成，直接使用RID**
        return tab_name_ + "_" + std::to_string(rid.page_no) + "_" + std::to_string(rid.slot_no);
    }
};
