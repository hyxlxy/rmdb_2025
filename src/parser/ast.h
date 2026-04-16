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

#include <vector>
#include <string>
#include <memory>

enum JoinType
{
    INNER_JOIN,
    LEFT_JOIN,
    RIGHT_JOIN,
    FULL_JOIN,
    SEMI_JOIN
};
namespace ast
{

    enum SvType
    {
        SV_TYPE_INT,
        SV_TYPE_FLOAT,
        SV_TYPE_STRING,
        SV_TYPE_BOOL
    };

    enum SvCompOp
    {
        SV_OP_EQ,
        SV_OP_NE,
        SV_OP_LT,
        SV_OP_GT,
        SV_OP_LE,
        SV_OP_GE
    };

    enum AggType
    {
        AGG_COUNT,
        AGG_SUM,
        AGG_MIN,
        AGG_MAX,
        AGG_AVG
    };

    enum class ArithOp
    {
        ADD, // +
        SUB, // -
        MUL, // *
        DIV  // /
    };

    enum OrderByDir
    {
        OrderBy_DEFAULT,
        OrderBy_ASC,
        OrderBy_DESC
    };

    enum SetKnobType
    {
        EnableNestLoop,
        EnableSortMerge,
        OutputFile
    };

    // Base class for tree nodes
    struct TreeNode
    {
        virtual ~TreeNode() = default; // enable polymorphism
    };

    struct Help : public TreeNode
    {
    };

    struct ShowTables : public TreeNode
    {
    };

    struct ShowIndex : public TreeNode
    {
        std::string tab_name;

        ShowIndex(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
    };

    struct TxnBegin : public TreeNode
    {
    };

    struct TxnCommit : public TreeNode
    {
    };

    struct TxnAbort : public TreeNode
    {
    };

    struct TxnRollback : public TreeNode
    {
    };

    struct TypeLen : public TreeNode
    {
        SvType type;
        int len;

        TypeLen(SvType type_, int len_) : type(type_), len(len_) {}
    };

    struct Field : public TreeNode
    {
    };

    struct ColDef : public Field
    {
        std::string col_name;
        std::shared_ptr<TypeLen> type_len;

        ColDef(std::string col_name_, std::shared_ptr<TypeLen> type_len_) : col_name(std::move(col_name_)), type_len(std::move(type_len_)) {}
    };

    struct CreateTable : public TreeNode
    {
        std::string tab_name;
        std::vector<std::shared_ptr<Field>> fields;

        CreateTable(std::string tab_name_, std::vector<std::shared_ptr<Field>> fields_) : tab_name(std::move(tab_name_)), fields(std::move(fields_)) {}
    };

    struct DropTable : public TreeNode
    {
        std::string tab_name;

        DropTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
    };

    struct DescTable : public TreeNode
    {
        std::string tab_name;

        DescTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
    };

    struct CreateIndex : public TreeNode
    {
        std::string tab_name;
        std::vector<std::string> col_names;

        CreateIndex(std::string tab_name_, std::vector<std::string> col_names_) : tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
    };

    struct DropIndex : public TreeNode
    {
        std::string tab_name;
        std::vector<std::string> col_names;

        DropIndex(std::string tab_name_, std::vector<std::string> col_names_) : tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
    };

    struct CreateStaticCheckpoint : public TreeNode
    {
    };

    struct ExplainStmt : public TreeNode
    {
        std::shared_ptr<TreeNode> stmt;

        explicit ExplainStmt(std::shared_ptr<TreeNode> stmt_) : stmt(std::move(stmt_)) {}
    };

    struct Expr : public TreeNode
    {
    };

    struct Value : public Expr
    {
    };

    struct IntLit : public Value
    {
        int val;

        IntLit(int val_) : val(val_) {}
    };

    struct FloatLit : public Value
    {
        float val;

        FloatLit(float val_) : val(val_) {}
    };

    struct StringLit : public Value
    {
        std::string val;

        StringLit(std::string val_) : val(std::move(val_)) {}
    };

    struct BoolLit : public Value
    {
        bool val;

        BoolLit(bool val_) : val(val_) {}
    };

    struct Col : public Expr
    {
        std::string tab_name;
        std::string col_name;

        Col(std::string tab_name_, std::string col_name_) : tab_name(std::move(tab_name_)), col_name(std::move(col_name_)) {}
    };

    // 聚合表达式节点
    struct AggExpr : public Expr
    {
        AggType agg_type;
        std::shared_ptr<Expr> arg; // COUNT(*) 时 arg 为 nullptr

        AggExpr(AggType agg_type_, std::shared_ptr<Expr> arg_)
            : agg_type(agg_type_), arg(std::move(arg_)) {}
    };

    // 表达式包装节点以保存别名
    struct AliasExpr : public Expr
    {
        std::shared_ptr<Expr> expr;
        std::string alias;

        AliasExpr(std::shared_ptr<Expr> expr_, std::string alias_)
            : expr(std::move(expr_)), alias(std::move(alias_)) {}
    };

    // 算术表达式节点
    struct ArithExpr : public Expr
    {
        std::shared_ptr<Expr> lhs; // 左操作数
        ArithOp op;                // 运算符
        std::shared_ptr<Expr> rhs; // 右操作数

        ArithExpr(std::shared_ptr<Expr> lhs_, ArithOp op_, std::shared_ptr<Expr> rhs_)
            : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
    };

    struct SetClause : public TreeNode
    {
        std::string col_name;
        std::shared_ptr<Expr> expr;  // 统一使用表达式

        SetClause(std::string col_name_, std::shared_ptr<Expr> expr_)
            : col_name(std::move(col_name_)), expr(std::move(expr_)) {}
    };

    struct BinaryExpr : public TreeNode
    {
        std::shared_ptr<Col> lhs;
        SvCompOp op;
        std::shared_ptr<Expr> rhs;

        BinaryExpr(std::shared_ptr<Col> lhs_, SvCompOp op_, std::shared_ptr<Expr> rhs_) : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
    };

    // 通用比较表达式，支持任意表达式作为操作数
    struct CompareExpr : public Expr
    {
        std::shared_ptr<Expr> lhs;
        SvCompOp op;
        std::shared_ptr<Expr> rhs;

        CompareExpr(std::shared_ptr<Expr> lhs_, SvCompOp op_, std::shared_ptr<Expr> rhs_)
            : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
    };

    // 逻辑表达式，支持 AND/OR
    struct LogicalExpr : public Expr
    {
        enum LogicalOp
        {
            AND,
            OR
        };

        std::shared_ptr<Expr> lhs;
        LogicalOp op;
        std::shared_ptr<Expr> rhs;

        LogicalExpr(std::shared_ptr<Expr> lhs_, LogicalOp op_, std::shared_ptr<Expr> rhs_)
            : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
    };

    struct OrderBy : public TreeNode
    {
        std::shared_ptr<Col> cols;
        OrderByDir orderby_dir;
        OrderBy(std::shared_ptr<Col> cols_, OrderByDir orderby_dir_) : cols(std::move(cols_)), orderby_dir(std::move(orderby_dir_)) {}
    };

    class SetKnobStmt : public TreeNode
    {
    public:
        SetKnobType set_knob_type;
        bool bool_value; // 例如 output_file on/off

        SetKnobStmt(SetKnobType type, bool value)
            : set_knob_type(type), bool_value(value) {}
    };

    struct InsertStmt : public TreeNode
    {
        std::string tab_name;
        std::vector<std::shared_ptr<Value>> vals;

        InsertStmt(std::string tab_name_, std::vector<std::shared_ptr<Value>> vals_) : tab_name(std::move(tab_name_)), vals(std::move(vals_)) {}
    };

    struct DeleteStmt : public TreeNode
    {
        std::string tab_name;
        std::vector<std::shared_ptr<BinaryExpr>> conds;

        DeleteStmt(std::string tab_name_, std::vector<std::shared_ptr<BinaryExpr>> conds_) : tab_name(std::move(tab_name_)), conds(std::move(conds_)) {}
    };

    struct UpdateStmt : public TreeNode
    {
        std::string tab_name;
        std::vector<std::shared_ptr<SetClause>> set_clauses;
        std::vector<std::shared_ptr<BinaryExpr>> conds;

        UpdateStmt(std::string tab_name_,
                   std::vector<std::shared_ptr<SetClause>> set_clauses_,
                   std::vector<std::shared_ptr<BinaryExpr>> conds_) : tab_name(std::move(tab_name_)), set_clauses(std::move(set_clauses_)), conds(std::move(conds_)) {}
    };

    // 表信息结构，支持表别名
    struct TableRef : public TreeNode
    {
        std::string table_name;
        std::string alias;

        TableRef(std::string table_name_, std::string alias_ = "") : table_name(std::move(table_name_)), alias(std::move(alias_)) {}
    };

    struct JoinExpr : public TreeNode
    {
        std::shared_ptr<TreeNode> left;
        std::shared_ptr<TreeNode> right;
        std::vector<std::shared_ptr<BinaryExpr>> conds;
        JoinType type;

        JoinExpr(std::shared_ptr<TreeNode> left_, std::shared_ptr<TreeNode> right_,
                 std::vector<std::shared_ptr<BinaryExpr>> conds_, JoinType type_ = SEMI_JOIN) : left(std::move(left_)), right(std::move(right_)), conds(std::move(conds_)), type(type_) {}
    };

    struct SelectStmt : public TreeNode
    {
        std::vector<std::shared_ptr<Expr>> cols;
        std::shared_ptr<TreeNode> tabs;
        std::vector<std::shared_ptr<BinaryExpr>> conds;
        std::vector<std::shared_ptr<JoinExpr>> jointree;

        bool has_sort;
        std::vector<std::shared_ptr<OrderBy>> order;

        std::vector<std::shared_ptr<Expr>> group_by; // 支持多列分组
        std::shared_ptr<Expr> having;                // HAVING 条件表达式
        int limit;                                   // LIMIT 子句，-1 表示没有限制

        SelectStmt(std::vector<std::shared_ptr<Expr>> cols_,
                   std::shared_ptr<TreeNode> tabs_,
                   std::vector<std::shared_ptr<BinaryExpr>> conds_,
                   std::vector<std::shared_ptr<OrderBy>> order_,
                   std::vector<std::shared_ptr<Expr>> group_by_ = {},
                   std::shared_ptr<Expr> having_ = nullptr,
                   int limit_ = -1)
            : cols(std::move(cols_)), tabs(std::move(tabs_)), conds(std::move(conds_)),
              order(std::move(order_)), group_by(std::move(group_by_)), having(std::move(having_)), limit(limit_)
        {
            has_sort = !order.empty() || limit > 0;
        }
    };

    // set enable_nestloop
    struct SetStmt : public TreeNode
    {
        SetKnobType set_knob_type_;
        bool bool_val_;

        SetStmt(SetKnobType &type, bool bool_value) : set_knob_type_(type), bool_val_(bool_value) {}
    };

    class LoadStmt : public TreeNode
    {
    public:
        std::string file_name;
        std::string table_name;
        LoadStmt(const std::string &file, const std::string &table)
            : file_name(file), table_name(table) {}
    };

    // Semantic value
    // 将 shared_ptr 全部改为裸指针，消除 Bison 每次 shift/reduce 的原子 refcount 开销
    struct SemValue
    {
        int sv_int = 0;
        float sv_float = 0.0f;
        std::string sv_str;
        bool sv_bool = false;
        OrderByDir sv_orderby_dir;
        std::vector<std::string> sv_strs;

        TreeNode *sv_node = nullptr;

        SvCompOp sv_comp_op;

        TypeLen *sv_type_len = nullptr;

        Field *sv_field = nullptr;
        std::vector<Field*>* sv_fields = nullptr;

        Expr *sv_expr = nullptr;
        ArithOp sv_arith_op;

        Value *sv_val = nullptr;
        std::vector<Value*>* sv_vals = nullptr;

        Col *sv_col = nullptr;

        SetClause *sv_set_clause = nullptr;
        std::vector<SetClause*>* sv_set_clauses = nullptr;

        BinaryExpr *sv_cond = nullptr;
        std::vector<BinaryExpr*>* sv_conds = nullptr;

        OrderBy *sv_orderby = nullptr;
        std::vector<OrderBy*>* sv_orderbys = nullptr;

        SetKnobType sv_setKnobType;

        std::vector<Expr*>* sv_exprs = nullptr;
    };

    extern thread_local std::shared_ptr<ast::TreeNode> parse_tree;

}

#define YYSTYPE ast::SemValue
