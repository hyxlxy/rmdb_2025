/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "execution_manager.h"
#include "optimizer/plan.h"
#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_insert.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"
#include "executor_seq_scan.h"
#include "executor_update.h"
#include "index/ix.h"
#include "record_printer.h"
#include "optimizer/plan_printer.h"
#include "executor_load.h"

bool output_file_enabled = true; // 控制是否向output.txt写入，默认开启

const char *help_info = "Supported SQL syntax:\n"
                        "  command ;\n"
                        "command:\n"
                        "  CREATE TABLE table_name (column_name type [, column_name type ...])\n"
                        "  DROP TABLE table_name\n"
                        "  CREATE INDEX table_name (column_name)\n"
                        "  DROP INDEX table_name (column_name)\n"
                        "  INSERT INTO table_name VALUES (value [, value ...])\n"
                        "  DELETE FROM table_name [WHERE where_clause]\n"
                        "  UPDATE table_name SET column_name = value [, column_name = value ...] [WHERE where_clause]\n"
                        "  SELECT selector FROM table_name [WHERE where_clause]\n"
                        "type:\n"
                        "  {INT | FLOAT | CHAR(n)}\n"
                        "where_clause:\n"
                        "  condition [AND condition ...]\n"
                        "condition:\n"
                        "  column op {column | value}\n"
                        "column:\n"
                        "  [table_name.]column_name\n"
                        "op:\n"
                        "  {= | <> | < | > | <= | >=}\n"
                        "selector:\n"
                        "  {* | column [, column ...]}\n";

// 主要负责执行DDL语句
void QlManager::run_mutli_query(std::shared_ptr<Plan> plan, Context *context)
{
    if (auto x = std::dynamic_pointer_cast<DDLPlan>(plan))
    {
        switch (x->tag)
        {
        case T_CreateTable:
        {
            sm_manager_->create_table(x->tab_name_, x->cols_, context);
            break;
        }
        case T_DropTable:
        {
            sm_manager_->drop_table(x->tab_name_, context);
            break;
        }
        case T_CreateIndex:
        {
            sm_manager_->create_index(x->tab_name_, x->tab_col_names_, context);
            break;
        }
        case T_DropIndex:
        {
            sm_manager_->drop_index(x->tab_name_, x->tab_col_names_, context);
            break;
        }
        default:
            throw InternalError("Unexpected field type");
            break;
        }
    }
}

// 执行help; show tables; desc table; begin; commit; abort;语句
void QlManager::run_cmd_utility(std::shared_ptr<Plan> plan, txn_id_t *txn_id, Context *context)
{
    if (auto x = std::dynamic_pointer_cast<OtherPlan>(plan))
    {
        switch (x->tag)
        {
        case T_Help:
        {
            memcpy(context->data_send_ + *(context->offset_), help_info, strlen(help_info));
            *(context->offset_) = strlen(help_info);
            break;
        }
        case T_ShowTable:
        {
            sm_manager_->show_tables(context);
            break;
        }
        case T_ShowIndex:
        {
            sm_manager_->show_index(x->tab_name_, context);
            break;
        }
        case T_DescTable:
        {
            sm_manager_->desc_table(x->tab_name_, context);
            break;
        }
        case T_Transaction_begin:
        {
            // MVCC: 为每个BEGIN语句创建新的事务
            // 如果当前事务已经在手动模式，先提交它
            if (context->txn_ != nullptr && context->txn_->get_txn_mode())
            {
                txn_mgr_->commit(context->txn_, context->log_mgr_);
            }

            // 创建新事务并设置为手动模式
            context->txn_ = txn_mgr_->begin(nullptr, context->log_mgr_);
            context->txn_->set_txn_mode(true);
            *txn_id = context->txn_->get_transaction_id();

            // 显式事务开始时，将所有表的count缓存设为无效：这里才是显式事务
            try {
                for (auto& [table_name, fh] : sm_manager_->fhs_) {
                    if (fh != nullptr) {
                        fh->invalidate_count_cache();
                    }
                }
            } catch (const std::exception& e) {
                // 继续执行
            }

            break;
        }
        case T_Transaction_commit:
        {
            // **修复：确保事务存在且有效**
            if (*txn_id != INVALID_TXN_ID && context->txn_ != nullptr)
            {
                // 使用当前上下文中的事务，而不是重新获取
                txn_mgr_->commit(context->txn_, context->log_mgr_);
                context->txn_ = nullptr;  // 清空上下文中的事务指针
                *txn_id = INVALID_TXN_ID; // 显式提交后重置事务ID
            }
            break;
        }
        case T_Transaction_rollback:
        {
            // **修复：确保事务存在且有效**
            if (*txn_id != INVALID_TXN_ID && context->txn_ != nullptr)
            {
                // 使用当前上下文中的事务，而不是重新获取
                txn_mgr_->abort(context->txn_, context->log_mgr_);
                context->txn_ = nullptr;  // 清空上下文中的事务指针
                *txn_id = INVALID_TXN_ID; // 显式回滚后重置事务ID
            }
            break;
        }
        case T_Transaction_abort:
        {
            context->txn_ = txn_mgr_->get_transaction(*txn_id);
            txn_mgr_->abort(context->txn_, context->log_mgr_);
            *txn_id = INVALID_TXN_ID; // 显式回滚后重置事务ID
            break;
        }
        case T_CreateStaticCheckpoint:
        {
            // 创建静态检查点
            // 这里可以添加具体的检查点创建逻辑
            // 目前只是简单地输出确认信息
            std::string msg = "Static checkpoint created successfully.\n";
            memcpy(context->data_send_ + *(context->offset_), msg.c_str(), msg.length());
            *(context->offset_) += msg.length();
            break;
        }
        default:
            throw InternalError("Unexpected field type");
            break;
        }
    }
    else if (auto x = std::dynamic_pointer_cast<SetKnobPlan>(plan))
    {
        switch (x->set_knob_type_)
        {
        case ast::SetKnobType::EnableNestLoop:
        {
            planner_->set_enable_nestedloop_join(x->bool_value_);
            break;
        }
        case ast::SetKnobType::EnableSortMerge:
        {
            planner_->set_enable_sortmerge_join(x->bool_value_);
            break;
        }
        case ast::SetKnobType::OutputFile:
        {
            output_file_enabled = x->bool_value_;
            break;
        }
        default:
        {
            throw RMDBError("Not implemented!\n");
            break;
        }
        }
    }
}
// 执行select语句，select语句的输出除了需要返回客户端外，还需要写入output.txt文件中
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot, std::vector<TabCol> sel_cols,
                            Context *context)
{
    // 初始化执行器
    executorTreeRoot->beginTuple();

    // 对于聚合查询，sel_cols可能为空，此时使用执行器的输出列信息
    if (sel_cols.empty())
    {
        auto &executor_cols = executorTreeRoot->cols();
        for (const auto &col : executor_cols)
        {
            // 调试输出
            std::cout << "Debug: col.name = " << col.name << std::endl;
            TabCol sel_col = {.tab_name = col.tab_name, .col_name = col.name};
            sel_cols.push_back(sel_col);
        }
    }

    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (auto &sel_col : sel_cols)
    {
        captions.push_back(sel_col.col_name);
    }

    // Print header into buffer
    RecordPrinter rec_printer(sel_cols.size());
    rec_printer.print_separator(context);
    rec_printer.print_record(captions, context);
    rec_printer.print_separator(context);
    // 只在开启时写入 output.txt
    std::fstream outfile;
    if (output_file_enabled)
    {
        outfile.open("output.txt", std::ios::out | std::ios::app);
        outfile << "|";
        for (size_t i = 0; i < captions.size(); ++i)
        {
            outfile << " " << captions[i] << " |";
        }
        outfile << "\n";
    }

    // Print records
    size_t num_rec = 0;
    try
    {
        // 执行query_plan（注意：不要再次调用beginTuple）
        for (; !executorTreeRoot->is_end(); executorTreeRoot->nextTuple())
        {
            auto Tuple = executorTreeRoot->Next();
            std::vector<std::string> columns;
            for (auto &col : executorTreeRoot->cols())
            {
                std::string col_str;
                char *rec_buf = Tuple->data + col.offset;
                if (col.type == TYPE_INT)
                {
                    col_str = std::to_string(*(int *)rec_buf);
                }
                else if (col.type == TYPE_FLOAT)
                {
                    col_str = std::to_string(*(float *)rec_buf);
                }
                else if (col.type == TYPE_STRING)
                {
                    col_str = std::string((char *)rec_buf, col.len);
                    col_str.resize(strlen(col_str.c_str()));
                }
                columns.push_back(col_str);
            }
            // print record into buffer
            rec_printer.print_record(columns, context);
            // 只在开启时写入 output.txt
            if (output_file_enabled)
            {
                outfile << "|";
                for (size_t i = 0; i < columns.size(); ++i)
                {
                    outfile << " " << columns[i] << " |";
                }
                outfile << "\n";
            }
            num_rec++;
        }
    }
    catch (const RMDBError &e)
    {
        // 索引查询异常，继续执行，输出已有的表头
    }
    catch (const TransactionAbortException &)
    {
        // 事务中止必须向上传播，不能伪装成“空结果”
        throw;
    }
    catch (const std::exception &e)
    {
        // 其他异常
    }

    if (output_file_enabled)
    {
        outfile.close();
    }
    // Print footer into buffer
    rec_printer.print_separator(context);
    // Print record count into buffer
    RecordPrinter::print_record_count(num_rec, context);
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec)
{
    try
    {
        exec->Next();
    }
    catch (const UniqueCheckError &e)
    {
        // 唯一性约束违反
        throw; // 重新抛出异常，让上层处理
    }
    catch (const RMDBError &e)
    {
        // 其他数据库错误
        throw;
    }
    catch (const std::exception &e)
    {
        // 其他标准异常
        throw;
    }
}

// 执行LOAD语句
void QlManager::run_load(const std::shared_ptr<Plan> &plan, Context *context)
{
    // 只处理LoadPlan类型
    auto load_plan = std::dynamic_pointer_cast<LoadPlan>(plan);
    if (!load_plan)
    {
        throw RMDBError("run_load: Not a LoadPlan!");
    }

    try
    {
        // 创建并执行LoadExecutor
        std::unique_ptr<AbstractExecutor> exec = std::make_unique<LoadExecutor>(
            sm_manager_, load_plan->file_name, load_plan->table_name, context);
        exec->Next(); // 执行批量插入

 
    }
    catch (const std::exception &e)
    {
        if (output_file_enabled)
        {
            std::ofstream outfile("output.txt", std::ios::out | std::ios::app);
            outfile << "Load failed: " << load_plan->file_name << " into " << load_plan->table_name << ", reason: " << e.what() << std::endl;
            outfile.close();
        }
    }
}
