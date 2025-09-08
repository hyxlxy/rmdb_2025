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
#include "../transaction/txn_defs.h"

/**
 * @brief MVCC感知的删除执行器
 * 支持多版本并发控制的数据删除
 */
class MVCCDeleteExecutor : public MVCCExecutorBase
{
private:
    TabMeta tab_;      // 表的元数据
    RmFileHandle *fh_; // 表的数据文件句柄
    SmManager *sm_manager_;
    std::string tab_name_;         // 表名称
    std::vector<Rid> rids_;        // 需要删除的记录的位置
    std::vector<Condition> conds_; // delete的条件
    int deleted_count_ = 0;        // 删除计数

public:
    MVCCDeleteExecutor(SmManager *sm_manager, std::string tab_name,
                       std::vector<Condition> conds, std::vector<Rid> rids, Context *context)
        : sm_manager_(sm_manager), tab_name_(std::move(tab_name)),
          rids_(std::move(rids)), conds_(std::move(conds))
    {
        tab_ = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        context_ = context;
        if (context_)
        {
            context_->current_table_name_ = tab_name_;
        }
    }

    // 只执行一次
    std::unique_ptr<RmRecord> Next() override
    {
        // **完全兼容模式：直接使用普通删除逻辑，确保稳定性**
        return execute_regular_delete();
    }

    Rid &rid() override
    {
        return _abstract_rid;
    }

    std::string getType() override
    {
        return "MVCCDeleteExecutor";
    }

    /**
     * @brief 回退到普通删除逻辑（当没有事务上下文时）
     */
    std::unique_ptr<RmRecord> execute_regular_delete()
    {
        for (auto &rid : rids_)
        {
            try
            {
                // 获取记录并检查可见性
                auto rec = get_record_mvcc(fh_, rid, context_, sm_manager_);
                if (!rec)
                    continue; // 记录不可见，跳过

                // 使用MVCC感知的删除操作
                delete_record_mvcc(fh_, rid, context_, sm_manager_);

                // 记录WriteRecord到事务的write_set中，用于回滚
                if (context_ && context_->txn_)
                {
                    auto *wr = new WriteRecord(WType::DELETE_TUPLE, tab_name_, rid, *rec);
                    context_->txn_->append_write_record(wr);
                }
            }
            catch (const TransactionAbortException &e)
            {
                // 重新抛出TransactionAbortException，让上层处理
                throw;
            }
            catch (const std::exception &e)
            {
                // 其他异常转换为TransactionAbortException
                if (context_ && context_->txn_)
                {
                    throw TransactionAbortException(context_->txn_->get_transaction_id(), AbortReason::WRITE_WRITE_CONFLICT);
                }
                else
                {
                    throw;
                }
            }
        }

        return nullptr;
    }

    /**
     * @brief 获取删除的记录数量
     */
    int get_deleted_count() const
    {
        return deleted_count_;
    }
};
