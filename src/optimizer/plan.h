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

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "parser/ast.h"
#include "parser/parser.h"
#include "common/common.h"
#include "system/sm_manager.h"
#include "system/sm_manager.h"

// 前向声明
class SmManager;
struct ColMeta;
struct TabMeta;

typedef enum PlanTag
{
    T_Invalid = 1,
    T_Help,
    T_ShowTable,
    T_ShowIndex,
    T_DescTable,
    T_CreateTable,
    T_DropTable,
    T_CreateIndex,
    T_DropIndex,
    T_CreateStaticCheckpoint,
    T_SetKnob,
    T_Explain,
    T_Insert,
    T_Update,
    T_Delete,
    T_select,
    T_Transaction_begin,
    T_Transaction_commit,
    T_Transaction_abort,
    T_Transaction_rollback,
    T_SeqScan,
    T_IndexScan,
    T_NestLoop,
    T_SortMerge,
    T_Sort,
    T_Projection,
    T_Aggregation,
    T_Group,  // 新增GROUP算子
    T_Load
} PlanTag;

// 查询执行计划
class Plan
{
public:
    PlanTag tag;

    virtual ~Plan() = default;
};

class ScanPlan : public Plan
{
public:
    ScanPlan(PlanTag tag, SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names)
    {
        Plan::tag = tag;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager->db_.get_table(tab_name_);
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        fed_conds_ = conds_;
        index_col_names_ = index_col_names;
    }
    ~ScanPlan() {}
    // 以下变量同ScanExecutor中的变量
    std::string tab_name_;
    std::vector<ColMeta> cols_;
    std::vector<Condition> conds_;
    size_t len_;
    std::vector<Condition> fed_conds_;
    std::vector<std::string> index_col_names_;
};

class GroupPlan : public Plan
{
public:
    std::vector<std::shared_ptr<ast::Expr>> group_by_;
    std::shared_ptr<ast::Expr> having_;

    GroupPlan(PlanTag tag,
              std::shared_ptr<Plan> input,
              const std::vector<std::shared_ptr<ast::Expr>> &group_by,
              const std::shared_ptr<ast::Expr> &having = nullptr)
    {
        Plan::tag = tag;
        group_by_ = group_by;
        having_ = having;
        input_ = std::move(input);
    }
    
    std::shared_ptr<Plan> input_;
};

class AggPlan : public Plan
{
public:
    std::vector<std::shared_ptr<ast::Expr>> select_exprs_;

    AggPlan(PlanTag tag,
            std::shared_ptr<Plan> input,
            const std::vector<std::shared_ptr<ast::Expr>> &select_exprs)
    {
        Plan::tag = tag;
        select_exprs_ = select_exprs;
        input_ = std::move(input);
    }
    std::shared_ptr<Plan> input_;
};

class JoinPlan : public Plan
{
public:
    JoinPlan(PlanTag tag, std::shared_ptr<Plan> left, std::shared_ptr<Plan> right, std::vector<Condition> conds, JoinType type_ = SEMI_JOIN)
    {
        Plan::tag = tag;
        left_ = std::move(left);
        right_ = std::move(right);
        conds_ = std::move(conds);
        type = type_;
    }
    ~JoinPlan() {}
    // 左节点
    std::shared_ptr<Plan> left_;
    // 右节点
    std::shared_ptr<Plan> right_;
    // 连接条件
    std::vector<Condition> conds_;
    // future TODO: 后续可以支持的连接类型
    JoinType type;
};

class ProjectionPlan : public Plan
{
public:
    ProjectionPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::vector<TabCol> sel_cols, bool is_select_all = false)
    {
        Plan::tag = tag;
        subplan_ = std::move(subplan);
        sel_cols_ = std::move(sel_cols);
        is_select_all_ = is_select_all;
    }
    ~ProjectionPlan() {}
    std::shared_ptr<Plan> subplan_;
    std::vector<TabCol> sel_cols_;
    bool is_select_all_; // 标记原始查询是否为SELECT *
};

class SortPlan : public Plan
{
public:
    // 单列排序构造函数（保持向后兼容）
    SortPlan(PlanTag tag, std::shared_ptr<Plan> subplan, TabCol sel_col, bool is_desc)
    {
        Plan::tag = tag;
        subplan_ = std::move(subplan);
        sel_cols_.push_back(sel_col);
        is_desc_.push_back(is_desc);
    }

    // 多列排序构造函数
    SortPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::vector<TabCol> sel_cols, std::vector<ast::OrderByDir> order_dirs, int limit = -1)
    {
        Plan::tag = tag;
        subplan_ = std::move(subplan);
        sel_cols_ = std::move(sel_cols);
        limit_ = limit;
        // 转换OrderByDir到bool
        for (const auto &dir : order_dirs)
        {
            is_desc_.push_back(dir == ast::OrderBy_DESC);
        }
    }

    ~SortPlan() {}
    std::shared_ptr<Plan> subplan_;
    std::vector<TabCol> sel_cols_; // 支持多列排序
    std::vector<bool> is_desc_;    // 每列的排序方向
    int limit_;                    // LIMIT 子句，-1 表示没有限制

    // 为了向后兼容，保留单列访问接口
    TabCol sel_col() const { return sel_cols_.empty() ? TabCol{} : sel_cols_[0]; }
    bool is_desc() const { return is_desc_.empty() ? false : is_desc_[0]; }
};

// dml语句，包括insert; delete; update; select语句
class DMLPlan : public Plan
{
public:
    DMLPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::string tab_name,
            std::vector<Value> values, std::vector<Condition> conds,
            std::vector<SetClause> set_clauses)
    {
        Plan::tag = tag;
        subplan_ = std::move(subplan);
        tab_name_ = std::move(tab_name);
        values_ = std::move(values);
        conds_ = std::move(conds);
        set_clauses_ = std::move(set_clauses);
    }
    ~DMLPlan() {}
    std::shared_ptr<Plan> subplan_;
    std::string tab_name_;
    std::vector<Value> values_;
    std::vector<Condition> conds_;
    std::vector<SetClause> set_clauses_;
};

// ddl语句, 包括create/drop table; create/drop index;
class DDLPlan : public Plan
{
public:
    DDLPlan(PlanTag tag, std::string tab_name, std::vector<std::string> col_names, std::vector<ColDef> cols)
    {
        Plan::tag = tag;
        tab_name_ = std::move(tab_name);
        cols_ = std::move(cols);
        tab_col_names_ = std::move(col_names);
    }
    ~DDLPlan() {}
    std::string tab_name_;
    std::vector<std::string> tab_col_names_;
    std::vector<ColDef> cols_;
};

// help; show tables; desc tables; begin; abort; commit; rollback语句对应的plan
class OtherPlan : public Plan
{
public:
    OtherPlan(PlanTag tag, std::string tab_name)
    {
        Plan::tag = tag;
        tab_name_ = std::move(tab_name);
    }
    ~OtherPlan() {}
    std::string tab_name_;
};

// Set Knob Plan
class SetKnobPlan : public Plan
{
public:
    SetKnobPlan(ast::SetKnobType knob_type, bool bool_value)
    {
        Plan::tag = T_SetKnob;
        set_knob_type_ = knob_type;
        bool_value_ = bool_value;
    }

    ast::SetKnobType set_knob_type_;
    bool bool_value_;
};

class ExplainPlan : public Plan
{
public:
    explicit ExplainPlan(std::shared_ptr<Plan> inner_plan)
    {
        Plan::tag = T_Explain;
        inner_plan_ = std::move(inner_plan);
    }

    std::shared_ptr<Plan> inner_plan_;
};

class plannerInfo
{
public:
    std::shared_ptr<ast::SelectStmt> parse;
    std::vector<Condition> where_conds;
    std::vector<TabCol> sel_cols;
    std::shared_ptr<Plan> plan;
    std::vector<std::shared_ptr<Plan>> table_scan_executors;
    std::vector<SetClause> set_clauses;
    plannerInfo(std::shared_ptr<ast::SelectStmt> parse_) : parse(std::move(parse_)) {}
};
class LoadPlan : public Plan
{
public:
    std::string file_name;
    std::string table_name;

    LoadPlan(const std::string &file, const std::string &table)
    {
        Plan::tag = T_Load;
        file_name = file;
        table_name = table;
    }
    ~LoadPlan() override = default;
};
