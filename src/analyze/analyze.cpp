/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "analyze.h"

void extract_table_names(const std::shared_ptr<ast::TreeNode> &node, std::vector<std::string> &tables) // 将表名进行转换
{
    if (auto tref = std::dynamic_pointer_cast<ast::TableRef>(node))
    {
        tables.push_back(tref->table_name);
    }
    else if (auto join = std::dynamic_pointer_cast<ast::JoinExpr>(node)) // 递归
    {
        extract_table_names(join->left, tables);
        extract_table_names(join->right, tables);
    }
}

void extract_table_aliases(const std::shared_ptr<ast::TreeNode> &node,
                           std::unordered_map<std::string, std::string> &alias_to_table,
                           std::unordered_map<std::string, std::string> &table_to_alias)
{
    if (auto tref = std::dynamic_pointer_cast<ast::TableRef>(node))
    {
        if (!tref->alias.empty())
        {
            if (alias_to_table.count(tref->alias) > 0)
            {
                throw InternalError("Duplicate table alias: " + tref->alias);
            }
            alias_to_table[tref->alias] = tref->table_name;
            table_to_alias[tref->table_name] = tref->alias;
        }
    }
    else if (auto join = std::dynamic_pointer_cast<ast::JoinExpr>(node))
    {
        extract_table_aliases(join->left, alias_to_table, table_to_alias);
        extract_table_aliases(join->right, alias_to_table, table_to_alias);
    }
}

std::string resolve_table_alias_name(const std::string &name,
                                     const std::unordered_map<std::string, std::string> &alias_to_table)
{
    if (name.empty())
    {
        return name;
    }
    auto it = alias_to_table.find(name);
    if (it != alias_to_table.end())
    {
        return it->second;
    }
    return name;
}

void resolve_aliases_in_expr(const std::shared_ptr<ast::Expr> &expr,
                             const std::unordered_map<std::string, std::string> &alias_to_table)
{
    if (!expr)
    {
        return;
    }

    if (auto col = std::dynamic_pointer_cast<ast::Col>(expr))
    {
        col->tab_name = resolve_table_alias_name(col->tab_name, alias_to_table);
        return;
    }
    if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
    {
        resolve_aliases_in_expr(alias->expr, alias_to_table);
        return;
    }
    if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
    {
        resolve_aliases_in_expr(agg->arg, alias_to_table);
        return;
    }
    if (auto arith = std::dynamic_pointer_cast<ast::ArithExpr>(expr))
    {
        resolve_aliases_in_expr(arith->lhs, alias_to_table);
        resolve_aliases_in_expr(arith->rhs, alias_to_table);
        return;
    }
    if (auto cmp = std::dynamic_pointer_cast<ast::CompareExpr>(expr))
    {
        resolve_aliases_in_expr(cmp->lhs, alias_to_table);
        resolve_aliases_in_expr(cmp->rhs, alias_to_table);
        return;
    }
    if (auto logical = std::dynamic_pointer_cast<ast::LogicalExpr>(expr))
    {
        resolve_aliases_in_expr(logical->lhs, alias_to_table);
        resolve_aliases_in_expr(logical->rhs, alias_to_table);
        return;
    }
}

void resolve_aliases_in_binary_exprs(const std::vector<std::shared_ptr<ast::BinaryExpr>> &conds,
                                     const std::unordered_map<std::string, std::string> &alias_to_table)
{
    for (const auto &cond : conds)
    {
        if (!cond)
        {
            continue;
        }
        cond->lhs->tab_name = resolve_table_alias_name(cond->lhs->tab_name, alias_to_table);
        resolve_aliases_in_expr(cond->rhs, alias_to_table);
    }
}

void resolve_aliases_in_order_by(const std::vector<std::shared_ptr<ast::OrderBy>> &order_by,
                                 const std::unordered_map<std::string, std::string> &alias_to_table)
{
    for (const auto &order : order_by)
    {
        if (order && order->cols)
        {
            order->cols->tab_name = resolve_table_alias_name(order->cols->tab_name, alias_to_table);
        }
    }
}

void extract_join_conds(const std::shared_ptr<ast::TreeNode> &node, std::vector<std::shared_ptr<ast::BinaryExpr>> &conds)
{
    if (auto join = std::dynamic_pointer_cast<ast::JoinExpr>(node))
    {
        // 合并当前 JoinExpr 的 conds
        conds.insert(conds.end(), join->conds.begin(), join->conds.end());
        // 递归左右子树
        extract_join_conds(join->left, conds);
        extract_join_conds(join->right, conds);
    }
}

// 辅助函数：检查聚合参数是否合法
void Analyze::check_agg_expr(const std::shared_ptr<ast::Expr> &expr, const std::vector<ColMeta> &all_cols)
{
    using namespace ast;
    if (auto alias = std::dynamic_pointer_cast<AliasExpr>(expr))
    {
        check_agg_expr(alias->expr, all_cols);
    }
    else if (auto agg = std::dynamic_pointer_cast<AggExpr>(expr))
    {
        if (agg->agg_type == AGG_COUNT && !agg->arg)
        {
            // COUNT(*)，合法
            return;
        }
        // 只支持 COUNT/SUM/AVG/MIN/MAX(col)
        auto col = std::dynamic_pointer_cast<Col>(agg->arg);
        if (!col)
        {
            throw InternalError("聚合函数参数必须是列");
        }
        // 查找列元数据

        const ColMeta *meta = nullptr;
        // 简化匹配逻辑：如果AST中表名为空，只匹配列名
        if (col->tab_name.empty())
        {
            // 表名为空，只匹配列名
            for (const auto &c : all_cols)
            {
                if (c.name == col->col_name)
                {
                    meta = &c;
                    break;
                }
            }
        }
        else
        {
            // 表名不为空，精确匹配
            for (const auto &c : all_cols)
            {
                if (c.tab_name == col->tab_name && c.name == col->col_name)
                {
                    meta = &c;
                    break;
                }
            }
        }
        if (!meta)
        {
            throw ColumnNotFoundError(col->tab_name + "." + col->col_name);
        }
        // 类型检查
        switch (agg->agg_type)
        {
        case AGG_COUNT:
            // int/float/char 都支持
            break;
        case AGG_SUM:
        case AGG_AVG:
            // 仅需支持int和float类型
            if (meta->type != TYPE_INT && meta->type != TYPE_FLOAT)
            {
                throw IncompatibleTypeError(coltype2str(meta->type), "int/float");
            }
            break;
        case AGG_MIN:
        case AGG_MAX:
            if (meta->type != TYPE_INT && meta->type != TYPE_FLOAT)
            {
                throw IncompatibleTypeError(coltype2str(meta->type), "int/float");
            }
            break;
        default:
            throw InternalError("未知聚合类型");
        }
    }
    // 普通列不做检查
}

// 检查GROUP BY语义：SELECT列表中的非聚合列必须出现在GROUP BY子句中
void Analyze::check_group_by_semantics(const std::vector<std::shared_ptr<ast::Expr>> &select_exprs,
                                       const std::vector<std::shared_ptr<ast::Expr>> &group_by_exprs,
                                       const std::vector<ColMeta> &all_cols)
{
    using namespace ast;

    for (const auto &select_expr : select_exprs)
    {
        std::shared_ptr<Expr> expr = select_expr;

        // 处理别名表达式
        if (auto alias = std::dynamic_pointer_cast<AliasExpr>(expr))
        {
            expr = alias->expr;
        }

        // 如果是聚合函数，跳过检查
        if (std::dynamic_pointer_cast<AggExpr>(expr))
        {
            continue;
        }

        // 如果是普通列，检查是否在GROUP BY中
        if (auto col = std::dynamic_pointer_cast<Col>(expr))
        {
            bool found_in_group_by = false;

            // 在GROUP BY子句中查找这个列
            for (const auto &group_expr : group_by_exprs)
            {
                if (auto group_col = std::dynamic_pointer_cast<Col>(group_expr))
                {
                    // 比较列名（考虑表名可能为空的情况）
                    bool names_match = false;
                    if (col->tab_name.empty() || group_col->tab_name.empty())
                    {
                        // 如果任一表名为空，只比较列名
                        names_match = (col->col_name == group_col->col_name);
                    }
                    else
                    {
                        // 都有表名，精确匹配
                        names_match = (col->tab_name == group_col->tab_name &&
                                       col->col_name == group_col->col_name);
                    }

                    if (names_match)
                    {
                        found_in_group_by = true;
                        break;
                    }
                }
            }

            if (!found_in_group_by)
            {
                // 构造错误信息
                std::string col_full_name = col->tab_name.empty() ? col->col_name : col->tab_name + "." + col->col_name;
                throw InternalError("Column '" + col_full_name +
                                    "' must appear in the GROUP BY clause or be used in an aggregate function");
            }
        }
        // 其他类型的表达式（如常量）暂时不检查
    }
}

// 检查ORDER BY语义
void Analyze::check_order_by_semantics(const std::vector<std::shared_ptr<ast::OrderBy>> &order_by_exprs,
                                       const std::vector<std::shared_ptr<ast::Expr>> &group_by_exprs,
                                       const std::vector<std::shared_ptr<ast::Expr>> &select_exprs,
                                       const std::vector<ColMeta> &all_cols)
{
    using namespace ast;

    for (const auto &order_by : order_by_exprs)
    {
        if (!order_by->cols)
        {
            throw InternalError("ORDER BY column is null");
        }

        auto col = order_by->cols;

        // 1. 检查列是否存在
        bool col_exists = false;
        std::string resolved_tab_name;

        for (const auto &col_meta : all_cols)
        {
            bool names_match = false;
            if (col->tab_name.empty())
            {
                // 只比较列名
                names_match = (col_meta.name == col->col_name);
            }
            else
            {
                // 精确匹配表名和列名
                names_match = (col_meta.tab_name == col->tab_name && col_meta.name == col->col_name);
            }

            if (names_match)
            {
                if (col_exists && col->tab_name.empty())
                {
                    // 列名有歧义
                    throw AmbiguousColumnError(col->col_name);
                }
                col_exists = true;
                resolved_tab_name = col_meta.tab_name;
            }
        }

        if (!col_exists)
        {
            std::string col_full_name = col->tab_name.empty() ? col->col_name : col->tab_name + "." + col->col_name;
            throw ColumnNotFoundError(col_full_name);
        }

        // 2. 如果有GROUP BY，检查ORDER BY列的合法性
        if (!group_by_exprs.empty())
        {
            bool valid_in_group_by = false;

            // 检查是否在GROUP BY列表中
            for (const auto &group_expr : group_by_exprs)
            {
                if (auto group_col = std::dynamic_pointer_cast<Col>(group_expr))
                {
                    bool names_match = false;
                    if (col->tab_name.empty() || group_col->tab_name.empty())
                    {
                        names_match = (col->col_name == group_col->col_name);
                    }
                    else
                    {
                        names_match = (col->tab_name == group_col->tab_name &&
                                       col->col_name == group_col->col_name);
                    }

                    if (names_match)
                    {
                        valid_in_group_by = true;
                        break;
                    }
                }
            }

            // 如果不在GROUP BY中，检查是否在SELECT中作为聚合函数的一部分
            if (!valid_in_group_by)
            {
                bool valid_in_select = false;

                for (const auto &select_expr : select_exprs)
                {
                    std::shared_ptr<Expr> expr = select_expr;

                    // 处理别名表达式
                    if (auto alias = std::dynamic_pointer_cast<AliasExpr>(expr))
                    {
                        expr = alias->expr;
                    }

                    // 如果是聚合函数，检查其参数
                    if (auto agg = std::dynamic_pointer_cast<AggExpr>(expr))
                    {
                        if (agg->arg)
                        {
                            if (auto agg_col = std::dynamic_pointer_cast<Col>(agg->arg))
                            {
                                bool names_match = false;
                                if (col->tab_name.empty() || agg_col->tab_name.empty())
                                {
                                    names_match = (col->col_name == agg_col->col_name);
                                }
                                else
                                {
                                    names_match = (col->tab_name == agg_col->tab_name &&
                                                   col->col_name == agg_col->col_name);
                                }

                                if (names_match)
                                {
                                    valid_in_select = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (!valid_in_select)
                {
                    std::string col_full_name = col->tab_name.empty() ? col->col_name : col->tab_name + "." + col->col_name;
                    throw InternalError("Column '" + col_full_name +
                                        "' in ORDER BY clause must appear in the GROUP BY clause or be used in an aggregate function");
                }
            }
        }
    }
}
/**
 * @description: 分析器，进行语义分析和查询重写，需要检查不符合语义规定的部分
 * @param {shared_ptr<ast::TreeNode>} parse parser生成的结
 * @return {shared_ptr<Query>} Query
 */
std::shared_ptr<Query> Analyze::do_analyze(std::shared_ptr<ast::TreeNode> parse) // 处理查询
{
    std::shared_ptr<Query> query = std::make_shared<Query>();
    if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(parse)) // 将treeNode转化为指向SelectStmt的指针
    {
        // 处理表名
        extract_table_names(x->tabs, query->tables); // x指向ast::SelectStmt的指针，tabs是一个表名列表
        std::unordered_map<std::string, std::string> alias_to_table;
        extract_table_aliases(x->tabs, alias_to_table, query->tb2alias);
        /** TODO: 检查表是否存在 */
        for (const auto &tab_name : query->tables) // 在query->tables中遍历表名
        {
            if (!sm_manager_->db_.is_table(tab_name))
            {
                throw TableNotFoundError(tab_name);
            }
        }
        // 传递having表达式
        query->having = x->having;
        // 处理group by子句
        query->group_by = x->group_by;
        // 传递order by子句信息
        query->order = x->order;
        // 传递limit子句信息
        query->limit = x->limit;

        // 合并所有JoinExpr的conds到x->conds
        std::vector<std::shared_ptr<ast::BinaryExpr>> join_conds;
        extract_join_conds(x->tabs, join_conds);
        if (!join_conds.empty())
        {
            if (x->conds.empty())
            {
                x->conds = join_conds;
            }
            else
            {
                x->conds.insert(x->conds.end(), join_conds.begin(), join_conds.end());
            }
        }

        for (auto &expr : x->cols)
        {
            resolve_aliases_in_expr(expr, alias_to_table);
        }
        for (auto &expr : query->group_by)
        {
            resolve_aliases_in_expr(expr, alias_to_table);
        }
        resolve_aliases_in_expr(query->having, alias_to_table);
        resolve_aliases_in_order_by(query->order, alias_to_table);
        resolve_aliases_in_binary_exprs(x->conds, alias_to_table);

        // 将聚合函数中select信息传递到query->select-exprs中，并且保留非聚合函数的col传递
        for (auto &sv_sel_col : x->cols)
        {
            query->select_exprs.push_back(sv_sel_col);

            // 处理普通列
            if (auto col_expr = std::dynamic_pointer_cast<ast::Col>(sv_sel_col))
            {
                // 补充表名
                TabCol sel_col = {.tab_name = col_expr->tab_name, .col_name = col_expr->col_name};
                query->cols.push_back(sel_col);
            }
            // 处理别名表达式（包括聚合函数别名）
            else if (auto alias_expr = std::dynamic_pointer_cast<ast::AliasExpr>(sv_sel_col))
            {
                // 对于别名表达式，使用别名作为列名
                TabCol sel_col = {.tab_name = "", .col_name = alias_expr->alias};
                query->cols.push_back(sel_col);
            }
            // 处理聚合函数（没有别名的情况）
            else if (auto agg_expr = std::dynamic_pointer_cast<ast::AggExpr>(sv_sel_col))
            {
                // 为聚合函数生成默认名称
                std::string agg_name;
                switch (agg_expr->agg_type)
                {
                case ast::AGG_COUNT:
                    if (!agg_expr->arg)
                    {
                        agg_name = "COUNT(*)";
                    }
                    else if (auto col = std::dynamic_pointer_cast<ast::Col>(agg_expr->arg))
                    {
                        agg_name = "COUNT(" + col->col_name + ")";
                    }
                    else
                    {
                        agg_name = "COUNT";
                    }
                    break;
                case ast::AGG_SUM:
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg_expr->arg))
                    {
                        agg_name = "SUM(" + col->col_name + ")";
                    }
                    else
                    {
                        agg_name = "SUM";
                    }
                    break;
                case ast::AGG_AVG:
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg_expr->arg))
                    {
                        agg_name = "AVG(" + col->col_name + ")";
                    }
                    else
                    {
                        agg_name = "AVG";
                    }
                    break;
                case ast::AGG_MIN:
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg_expr->arg))
                    {
                        agg_name = "MIN(" + col->col_name + ")";
                    }
                    else
                    {
                        agg_name = "MIN";
                    }
                    break;
                case ast::AGG_MAX:
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg_expr->arg))
                    {
                        agg_name = "MAX(" + col->col_name + ")";
                    }
                    else
                    {
                        agg_name = "MAX";
                    }
                    break;
                default:
                    agg_name = "AGG";
                    break;
                }
                TabCol sel_col = {.tab_name = "", .col_name = agg_name};
                query->cols.push_back(sel_col);
            }
        }
        std::vector<ColMeta> all_cols;
        if (auto x1 = std::dynamic_pointer_cast<ast::JoinExpr>(x->tabs)) // 如果有连接条件 x->tabs为根节点
        {
            if (x1->type == JoinType::SEMI_JOIN) // tabs是节点类型，如果x1为半连接
            {
                auto x2 = std::dynamic_pointer_cast<ast::TableRef>(x1->left);
                all_cols = sm_manager_->db_.get_table(x2->table_name).cols; // 获取左表的元数据
                query->is_semi_join_ = true;
            }
            else
                get_all_cols(query->tables, all_cols); // 获取元数据 将汇总后的内容放入all_cols
        }
        else
            get_all_cols(query->tables, all_cols); // 获取元数据 将汇总后的内容放入all_cols

        // 处理select *的情况
        if (query->cols.empty())
        {

            // 检查是否是聚合查询
            bool has_aggregation = false;
            for (const auto &expr : query->select_exprs)
            {
                std::shared_ptr<ast::Expr> e = expr;
                if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(e))
                {
                    e = alias->expr;
                }
                if (std::dynamic_pointer_cast<ast::AggExpr>(e))
                {
                    has_aggregation = true;
                    break;
                }
            }

            if (!has_aggregation)
            {
                // select all columns
                for (auto &col : all_cols)
                {
                    TabCol sel_col = {.tab_name = col.tab_name, .col_name = col.name};
                    query->cols.push_back(sel_col); // 将表的所有字段添加到query->cols中
                }
            }
            else
            {
            }
        }
        else
        {
            // infer table name from column name

            // 检查是否是聚合查询（有GROUP BY或有聚合函数）
            bool has_group_by = !query->group_by.empty();
            bool has_aggregation = false;

            // 检查是否有聚合函数
            for (const auto &expr : query->select_exprs)
            {
                std::shared_ptr<ast::Expr> e = expr;
                if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(e))
                {
                    e = alias->expr;
                }
                if (std::dynamic_pointer_cast<ast::AggExpr>(e))
                {
                    has_aggregation = true;
                    break;
                }
            }

            for (auto &sel_col : query->cols)
            {

                if (has_aggregation)
                {
                    // 对于聚合查询，需要区分普通列和聚合结果列
                    bool is_table_col = false;

                    if (has_group_by)
                    {
                        // 有GROUP BY时，检查是否是分组列
                        for (const auto &group_expr : query->group_by)
                        {
                            if (auto col_expr = std::dynamic_pointer_cast<ast::Col>(group_expr))
                            {
                                if (col_expr->col_name == sel_col.col_name)
                                {
                                    is_table_col = true;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        // 没有GROUP BY时，检查是否是表中的普通列（通过检查是否有表名前缀或在表中存在）
                        for (const auto &col : all_cols)
                        {
                            if (col.name == sel_col.col_name)
                            {
                                is_table_col = true;
                                break;
                            }
                        }
                    }

                    // 只对表列进行验证，聚合结果列跳过验证
                    if (is_table_col)
                    {
                        sel_col = check_column(all_cols, sel_col); // 列元数据校验
                    }
                    else
                    {
                        // 聚合结果列不需要在原始表中验证，保持原样
                    }
                }
                else
                {
                    // 非聚合查询，正常验证所有列
                    sel_col = check_column(all_cols, sel_col); // 列元数据校验
                }
            }
        }

        // 检查聚合函数参数是否合法
        for (auto &expr : x->cols)
        {
            check_agg_expr(expr, all_cols);
        }

        // 检查GROUP BY语义
        if (!query->group_by.empty())
        {
            check_group_by_semantics(x->cols, query->group_by, all_cols);
        }

        // 检查ORDER BY语义
        if (!query->order.empty())
        {
            check_order_by_semantics(query->order, query->group_by, x->cols, all_cols);
        }

        // 处理where条件
        get_clause(x->conds, query->conds);
        check_clause(query->tables, query->conds);
    }
    else if (auto x = std::dynamic_pointer_cast<ast::LoadStmt>(parse)) // 处理Load
    {
        query->file_name = x->file_name;
        query->tables.push_back(x->table_name);
        query->is_load = true; // 标记为Load查询
        query->parse = std::move(parse);
        return query;
    }
    else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(parse)) // 处理更新
    {
        /** TODO: */
        // 处理表名
        query->tables.push_back(x->tab_name);
        if (!sm_manager_->db_.is_table(x->tab_name))
        {
            throw TableNotFoundError(x->tab_name);
        }
        // 处理set子句
        for (const auto &sv_set_clause : x->set_clauses) // 子查询变量sv_set_clauses
        {
            SetClause set_clause;
            set_clause.lhs = {.tab_name = x->tab_name, .col_name = sv_set_clause->col_name};
            set_clause.expr = sv_set_clause->expr; // 保存原始表达式

            // 如果是简单值，直接转换；如果是表达式，运行时计算
            if (auto value_expr = std::dynamic_pointer_cast<ast::Value>(sv_set_clause->expr))
            {
                set_clause.rhs = convert_sv_value(value_expr);
            }
            else
            {
                set_clause.rhs.set_int(0);
            }

            query->set_clauses.push_back(set_clause);
        }
        std::vector<ColMeta> all_cols;
        get_all_cols(query->tables, all_cols); // 将所有插入到query的表中
        for (auto &set_clause : query->set_clauses)
        {
            set_clause.lhs = check_column(all_cols, set_clause.lhs); // 列元数据校验

            // **关键修复：只有当表达式为空且rhs类型与列类型匹配时才初始化raw字段**
            if (set_clause.expr == nullptr)
            {
                // 对于简单值，初始化raw字段
                TabMeta &tab = sm_manager_->db_.get_table(set_clause.lhs.tab_name);
                auto col = tab.get_col(set_clause.lhs.col_name);

                // 确保类型匹配再初始化
                if ((col->type == TYPE_STRING && set_clause.rhs.type == TYPE_STRING) ||
                    (col->type == TYPE_INT && set_clause.rhs.type == TYPE_INT) ||
                    (col->type == TYPE_FLOAT && set_clause.rhs.type == TYPE_FLOAT))
                {
                    set_clause.rhs.init_raw(col->len);
                }
                else
                {
                }
            }
            // 对于表达式，不需要在这里初始化raw字段，会在执行时计算
        }
        // 处理where条件
        get_clause(x->conds, query->conds);
        check_clause(query->tables, query->conds);
    }
    else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(parse)) // 删除
    {
        // 处理表名
        query->tables.push_back(x->tab_name);
        if (!sm_manager_->db_.is_table(x->tab_name))
        {
            throw TableNotFoundError(x->tab_name);
        }

        // 处理where条件
        get_clause(x->conds, query->conds); // 将x的条件转换为查询中的条件 方便后续执行.
        check_clause({x->tab_name}, query->conds);
    }
    else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(parse))
    {
        // 处理表名
        query->tables.push_back(x->tab_name);
        if (!sm_manager_->db_.is_table(x->tab_name))
        {
            throw TableNotFoundError(x->tab_name);
        }

        // 处理insert 的values值
        for (auto &sv_val : x->vals)
        {
            query->values.push_back(convert_sv_value(sv_val));
        }
    }
    else if (auto x = std::dynamic_pointer_cast<ast::ExplainStmt>(parse))
    {
        query->inner_query = do_analyze(x->stmt);
    }
    else
    {
        // do nothing
    }
    query->parse = std::move(parse);
    return query;
}

TabCol Analyze::check_column(const std::vector<ColMeta> &all_cols, TabCol target) // 确认列没问题
{
    if (target.tab_name.empty())
    {
        // Table name not specified, infer table name from column name
        std::string tab_name;
        for (auto &col : all_cols)
        {
            if (col.name == target.col_name)
            {
                if (!tab_name.empty())
                {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty())
        {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = tab_name;
    } // 确认每一列是独一无二的
    else
    {
        /** 修复JOIN查询中的列名解析问题 */
        bool found = false;

        // 尝试精确匹配表名和列名
        for (auto &col : all_cols)
        {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            std::string col_full_name = target.tab_name + "." + target.col_name;
            throw ColumnNotFoundError(col_full_name);
        }
    }
    return target;
}

void Analyze::get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols)
{
    for (auto &sel_tab_name : tab_names)
    {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end()); // 将表的所有字段元数据添加到all_cols中
    }
}

void Analyze::get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>> &sv_conds, std::vector<Condition> &conds)
{
    conds.clear();
    for (auto &expr : sv_conds)
    {
        Condition cond;
        cond.lhs_col = {.tab_name = expr->lhs->tab_name, .col_name = expr->lhs->col_name};
        cond.op = convert_sv_comp_op(expr->op);
        if (auto rhs_val = std::dynamic_pointer_cast<ast::Value>(expr->rhs))
        {
            cond.is_rhs_val = true;
            cond.rhs_val = convert_sv_value(rhs_val);
        }
        else if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(expr->rhs))
        {
            cond.is_rhs_val = false;
            cond.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
        }
        conds.push_back(cond);
    }
}

void Analyze::check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds)
{
    // auto all_cols = get_all_cols(tab_names);
    std::vector<ColMeta> all_cols;
    get_all_cols(tab_names, all_cols);
    // Get raw values in where clause
    for (auto &cond : conds)
    {
        // Infer table name from column name
        cond.lhs_col = check_column(all_cols, cond.lhs_col);
        if (!cond.is_rhs_val)
        {
            cond.rhs_col = check_column(all_cols, cond.rhs_col);
        }
        TabMeta &lhs_tab = sm_manager_->db_.get_table(cond.lhs_col.tab_name);
        auto lhs_col = lhs_tab.get_col(cond.lhs_col.col_name);
        ColType lhs_type = lhs_col->type;
        ColType rhs_type;
        if (cond.is_rhs_val)
        {
            cond.rhs_val.init_raw(lhs_col->len);
            rhs_type = cond.rhs_val.type;
        }
        else
        {
            TabMeta &rhs_tab = sm_manager_->db_.get_table(cond.rhs_col.tab_name);
            auto rhs_col = rhs_tab.get_col(cond.rhs_col.col_name);
            rhs_type = rhs_col->type;
        }
        if (lhs_type != rhs_type)
        {
            // 检查是否都是数值类型（INT或FLOAT）
            if ((lhs_type == TYPE_INT || lhs_type == TYPE_FLOAT) &&
                (rhs_type == TYPE_INT || rhs_type == TYPE_FLOAT))
            {
                // 数值类型之间可以自动转换，不抛出错误
                if (cond.is_rhs_val)
                {
                    if (lhs_type == TYPE_FLOAT && rhs_type == TYPE_INT)
                    {
                        // 将右侧INT转换为FLOAT
                        float float_val = static_cast<float>(cond.rhs_val.int_val);
                        cond.rhs_val.set_float(float_val);
                        // 更新右侧类型
                        rhs_type = TYPE_FLOAT;
                    }
                    else if (lhs_type == TYPE_INT && rhs_type == TYPE_FLOAT)
                    {
                        // 将右侧FLOAT转换为INT
                        int int_val = static_cast<int>(cond.rhs_val.float_val);
                        cond.rhs_val.set_int(int_val);
                        // 更新右侧类型
                        rhs_type = TYPE_INT;
                    }
                }
                // 如果不是右侧值，则在执行阶段处理类型转换
            }
            else
            {
                throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(rhs_type));
            }
        }
    }
}

Value Analyze::convert_sv_value(const std::shared_ptr<ast::Value> &sv_val)
{
    Value val;
    if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val))
    {
        val.set_int(int_lit->val);
    }
    else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val))
    {
        val.set_float(float_lit->val);
    }
    else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val))
    {
        val.set_str(str_lit->val);
    }
    else
    {
        throw InternalError("Unexpected sv value type");
    }
    return val;
}

CompOp Analyze::convert_sv_comp_op(ast::SvCompOp op)
{
    std::map<ast::SvCompOp, CompOp> m = {
        {ast::SV_OP_EQ, OP_EQ},
        {ast::SV_OP_NE, OP_NE},
        {ast::SV_OP_LT, OP_LT},
        {ast::SV_OP_GT, OP_GT},
        {ast::SV_OP_LE, OP_LE},
        {ast::SV_OP_GE, OP_GE},
    };
    return m.at(op);
}
