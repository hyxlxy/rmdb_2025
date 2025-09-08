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
#include <unordered_map>

#include "parser/parser.h"
#include "system/sm.h"
#include "common/common.h"

class Query // 查询
{
public:
    std::shared_ptr<ast::TreeNode> parse;
    // TODO jointree
    std::shared_ptr<ast::TreeNode> jointree;
    // where条件
    std::vector<Condition> conds;
    // JOIN条件（从WHERE条件中分离出来的多表条件）
    std::vector<Condition> join_conds;
    // 过滤条件（单表条件）
    std::vector<Condition> filter_conds;
    // 投影列
    std::vector<TabCol> cols;
    bool is_semi_join_;
    // 表名
    std::vector<std::string> tables;
    // update 的set 值
    std::vector<SetClause> set_clauses;
    // insert 的values值
    std::vector<Value> values;

    // 文件名字
    std::string file_name;
    // 优化相关的扩展字段
    // 每个表的选择条件（用于谓词下推）
    std::unordered_map<std::string, std::vector<Condition>> table_conds;
    // 每个表需要的列（用于投影下推）
    std::unordered_map<std::string, std::vector<TabCol>> table_cols;
    // 是否为SELECT *查询
    bool is_select_all = false;
    // 是否设置了输出文件
    bool set_output_file = true;
    bool is_load = false;

    // 保存所有select表达式（包括聚合、别名等）
    std::vector<std::shared_ptr<ast::Expr>> select_exprs;
    // 保存所有分组表达式
    std::vector<std::shared_ptr<ast::Expr>> group_by;
    // 保存having子句
    std::shared_ptr<ast::Expr> having;
    // 保存orderby子句
    std::vector<std::shared_ptr<ast::OrderBy>> order;
    // 保存limit子句
    int limit;

    Query() {}
};

class Analyze
{
private:
    SmManager *sm_manager_;

public:
    Analyze(SmManager *sm_manager) : sm_manager_(sm_manager) {}
    ~Analyze() {}

    std::shared_ptr<Query> do_analyze(std::shared_ptr<ast::TreeNode> root);

private:
    TabCol check_column(const std::vector<ColMeta> &all_cols, TabCol target);
    void get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols);
    void get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>> &sv_conds, std::vector<Condition> &conds);
    void check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds);
    Value convert_sv_value(const std::shared_ptr<ast::Value> &sv_val);
    CompOp convert_sv_comp_op(ast::SvCompOp op);
    bool can_cast_type(ColType from, ColType to);
    void cast_value(Value &val, ColType to);
    void check_agg_expr(const std::shared_ptr<ast::Expr> &expr, const std::vector<ColMeta> &all_cols);
    void check_group_by_semantics(const std::vector<std::shared_ptr<ast::Expr>> &select_exprs,
                                  const std::vector<std::shared_ptr<ast::Expr>> &group_by_exprs,
                                  const std::vector<ColMeta> &all_cols);
    void check_order_by_semantics(const std::vector<std::shared_ptr<ast::OrderBy>> &order_by_exprs,
                                  const std::vector<std::shared_ptr<ast::Expr>> &group_by_exprs,
                                  const std::vector<std::shared_ptr<ast::Expr>> &select_exprs,
                                  const std::vector<ColMeta> &all_cols);
};
