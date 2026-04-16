%{
#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc, void *yyscanner);

void yyerror(YYLTYPE *locp, void *yyscanner, const char* s) {
    (void)yyscanner;
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;

// 将裸指针 vector 转成 shared_ptr vector 并 delete 原始 vector
template<typename T>
static std::vector<std::shared_ptr<T>> adopt(std::vector<T*>* raw) {
    if (!raw) return {};
    std::vector<std::shared_ptr<T>> result;
    result.reserve(raw->size());
    for (T* p : *raw) result.emplace_back(p);
    delete raw;
    return result;
}
%}

// request a pure (reentrant) parser
%define api.pure full
// enable location in error handler
%locations
// enable verbose syntax error message
%define parse.error verbose
%parse-param { void *yyscanner }
%lex-param { void *yyscanner }

// keywords
%token SHOW TABLES CREATE TABLE DROP DESC INSERT INTO VALUES DELETE FROM ASC ORDER BY SUM HAVING ON COUNT MIN MAX AVG LOAD OUTPUT_FILE OFF EXPLAIN
WHERE UPDATE SET SELECT INT CHAR FLOAT INDEX AND JOIN EXIT HELP TXN_BEGIN TXN_COMMIT TXN_ABORT TXN_ROLLBACK ORDER_BY ENABLE_NESTLOOP ENABLE_SORTMERGE SEMI GROUP AS LIMIT STATIC_CHECKPOINT
// non-keywords
%token LEQ NEQ GEQ T_EOF

// 操作符优先级（从低到高）
%left AND
%left '=' '<' '>' LEQ NEQ GEQ
%left '+' '-'
%left '*' '/'
%left '(' ')'
%right UMINUS    /* 给一元减法更高的优先级 */

// type-specific tokens
%token <sv_str> IDENTIFIER VALUE_STRING
%token <sv_int> VALUE_INT
%token <sv_float> VALUE_FLOAT
%token <sv_bool> VALUE_BOOL
%token <sv_str> FILE_NAME

// specify types for non-terminal symbol
%type <sv_node> stmt dbStmt ddl dml txnStmt setStmt explainStmt
%type <sv_field> field
%type <sv_fields> fieldList
%type <sv_type_len> type
%type <sv_comp_op> op
%type <sv_expr> expr agg_expr colItem having_clause compare_expr logical_expr
%type <sv_val> value
%type <sv_vals> valueList
%type <sv_str> tbName colName alias file_path firsts last first
%type <sv_str> file_name
%type <sv_strs> colNameList
%type <sv_node> tableList tableRef
%type <sv_col> col
%type <sv_exprs> colList selector groupby_clause
%type <sv_set_clause> setClause
%type <sv_set_clauses> setClauses
%type <sv_cond> condition
%type <sv_conds> whereClause optWhereClause
%type <sv_orderby>  order_clause
%type <sv_orderbys> opt_order_clause
%type <sv_orderbys> order_clause_list
%type <sv_orderby_dir> opt_asc_desc
%type <sv_setKnobType> set_knob_type
%type <sv_int> opt_limit_clause

%%
start:
        stmt ';'
    {
        parse_tree = std::shared_ptr<ast::TreeNode>($1);
        YYACCEPT;
    }
    |   HELP
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
    |   EXIT
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    |   SET OUTPUT_FILE OFF
    {
        parse_tree = std::make_shared<SetKnobStmt>(ast::SetKnobType::OutputFile, false);
        YYACCEPT;
    }
    |   SET OUTPUT_FILE ON
    {
        parse_tree = std::make_shared<SetKnobStmt>(ast::SetKnobType::OutputFile, true);
        YYACCEPT;
    }
    |   T_EOF
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    ;

stmt:
        dbStmt
    |   ddl
    |   dml
    |   txnStmt
    |   setStmt
    |   explainStmt

    ;

txnStmt:
        TXN_BEGIN
    {
        $$ = new TxnBegin();
    }
    |   TXN_COMMIT
    {
        $$ = new TxnCommit();
    }
    |   TXN_ABORT
    {
        $$ = new TxnAbort();
    }
    | TXN_ROLLBACK
    {
        $$ = new TxnRollback();
    }
    ;

dbStmt:
        SHOW TABLES
    {
        $$ = new ShowTables();
    }
    |   SHOW INDEX FROM tbName
    {
        $$ = new ShowIndex($4);
    }
    ;

setStmt:
        SET set_knob_type '=' VALUE_BOOL
    {
        $$ = new SetStmt($2, $4);
    }
    ;
ddl:
        CREATE TABLE tbName '(' fieldList ')'
    {
        $$ = new CreateTable($3, adopt<Field>($5));
    }
    |   DROP TABLE tbName
    {
        $$ = new DropTable($3);
    }
    |   DESC tbName
    {
        $$ = new DescTable($2);
    }
    |   CREATE INDEX tbName '(' colNameList ')'
    {
        $$ = new CreateIndex($3, $5);
    }
    |   DROP INDEX tbName '(' colNameList ')'
    {
        $$ = new DropIndex($3, $5);
    }
    |   CREATE STATIC_CHECKPOINT
    {
        $$ = new CreateStaticCheckpoint();
    }
    ;

dml:
        INSERT INTO tbName VALUES '(' valueList ')'
    {
        $$ = new InsertStmt($3, adopt<Value>($6));
    }
    |   DELETE FROM tbName optWhereClause
    {
        $$ = new DeleteStmt($3, adopt<BinaryExpr>($4));
    }
    |   UPDATE tbName SET setClauses optWhereClause
    {
        $$ = new UpdateStmt($2, adopt<SetClause>($4), adopt<BinaryExpr>($5));
    }
    | SELECT selector FROM tableList optWhereClause groupby_clause having_clause opt_order_clause opt_limit_clause
    {
        $$ = new SelectStmt(adopt<Expr>($2), std::shared_ptr<TreeNode>($4), adopt<BinaryExpr>($5), adopt<OrderBy>($8), adopt<Expr>($6), std::shared_ptr<Expr>($7), $9);
    }
    | LOAD file_path INTO tbName
    {
        $$ = new LoadStmt($2, $4);
    }
    ;

explainStmt:
        EXPLAIN dml
    {
        $$ = new ExplainStmt(std::shared_ptr<TreeNode>($2));
    }
    ;

groupby_clause
    : /* 空 */ { $$ = nullptr; }
    | GROUP BY colList { $$ = $3; }
    ;

having_clause
    : /* 空 */ { $$ = nullptr; }
    | HAVING logical_expr { $$ = $2; }
    ;

logical_expr:
        compare_expr
    {
        $$ = $1;
    }
    |   logical_expr AND logical_expr
    {
        $$ = new LogicalExpr(std::shared_ptr<Expr>($1), LogicalExpr::AND, std::shared_ptr<Expr>($3));
    }
    ;

compare_expr:
        agg_expr op value
    {
        // 聚合函数与常量比较，如 COUNT(*) > 1
        $$ = new CompareExpr(std::shared_ptr<Expr>($1), $2, std::shared_ptr<Expr>($3));
    }
    |   agg_expr op agg_expr
    {
        // 聚合函数之间比较，如 MIN(score) > MAX(other)
        $$ = new CompareExpr(std::shared_ptr<Expr>($1), $2, std::shared_ptr<Expr>($3));
    }
    |   col op value
    {
        // 列与常量比较
        $$ = new CompareExpr(std::shared_ptr<Expr>(static_cast<Expr*>($1)), $2, std::shared_ptr<Expr>($3));
    }
    |   col op agg_expr
    {
        // 列与聚合函数比较
        $$ = new CompareExpr(std::shared_ptr<Expr>(static_cast<Expr*>($1)), $2, std::shared_ptr<Expr>($3));
    }
    ;


opt_order_clause:
    ORDER BY order_clause_list
    {
        $$ = $3;
    }
    | /* 空 */ { $$ = nullptr; }
    ;

order_clause_list:
      order_clause
    {
        $$ = new std::vector<OrderBy*>{$1};
    }
    | order_clause_list ',' order_clause
    {
        $$->push_back($3);
    }
    ;

order_clause:
      col opt_asc_desc
    {
        $$ = new OrderBy(std::shared_ptr<Col>($1), $2);
    }
    ;

fieldList:
        field
    {
        $$ = new std::vector<Field*>{$1};
    }
    |   fieldList ',' field
    {
        $$->push_back($3);
    }
    ;

colNameList:
        colName
    {
        $$ = std::vector<std::string>{$1};
    }
    | colNameList ',' colName
    {
        $$.push_back($3);
    }
    ;

field:
        colName type
    {
        $$ = new ColDef($1, std::shared_ptr<TypeLen>($2));
    }
    ;

type:
        INT
    {
        $$ = new TypeLen(SV_TYPE_INT, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        $$ = new TypeLen(SV_TYPE_STRING, $3);
    }
    |   FLOAT
    {
        $$ = new TypeLen(SV_TYPE_FLOAT, sizeof(float));
    }
    ;

valueList:
        value
    {
        $$ = new std::vector<Value*>{$1};
    }
    |   valueList ',' value
    {
        $$->push_back($3);
    }
    ;

value:
        VALUE_INT
    {
        $$ = new IntLit($1);
    }
    |   '+' VALUE_INT %prec UMINUS
    {
        $$ = new IntLit($2);
    }
    |   '-' VALUE_INT %prec UMINUS
    {
        $$ = new IntLit(-$2);
    }
    |   VALUE_FLOAT
    {
        $$ = new FloatLit($1);
    }
    |   '+' VALUE_FLOAT %prec UMINUS
    {
        $$ = new FloatLit($2);
    }
    |   '-' VALUE_FLOAT %prec UMINUS
    {
        $$ = new FloatLit(-$2);
    }
    |   VALUE_STRING
    {
        $$ = new StringLit($1);
    }
    |   VALUE_BOOL
    {
        $$ = new BoolLit($1);
    }
    ;

condition:
        col op expr
    {
        $$ = new BinaryExpr(std::shared_ptr<Col>($1), $2, std::shared_ptr<Expr>($3));
    }
    ;

optWhereClause:
        /* epsilon */ { $$ = nullptr; }
    |   WHERE whereClause
    {
        $$ = $2;
    }
    ;

whereClause:
        condition
    {
        $$ = new std::vector<BinaryExpr*>{$1};
    }
    |   whereClause AND condition
    {
        $$->push_back($3);
    }
    ;

col:
        tbName '.' colName
    {
        $$ = new Col($1, $3);
    }
    |   colName
    {
        $$ = new Col("", $1);
    }
    ;

colList:
      colItem
    {
        $$ = new std::vector<Expr*>{$1};
    }
    | colList ',' colItem
    {
        $$->push_back($3);
    }
    ;


colItem:
      col
    {
        $$ = static_cast<Expr*>($1);
    }
    | agg_expr
    {
        $$ = $1;
    }
    | col AS IDENTIFIER
    {
        $$ = new AliasExpr(std::shared_ptr<Expr>(static_cast<Expr*>($1)), $3);
    }
    | agg_expr AS IDENTIFIER
    {
        $$ = new AliasExpr(std::shared_ptr<Expr>($1), $3);
    }
    ;


agg_expr:
      COUNT '(' '*' ')'
    {
        $$ = new AggExpr(AGG_COUNT, nullptr);
    }
    | COUNT '(' col ')'
    {
        $$ = new AggExpr(AGG_COUNT, std::shared_ptr<Expr>(static_cast<Expr*>($3)));
    }
    | SUM '(' col ')'
    {
        $$ = new AggExpr(AGG_SUM, std::shared_ptr<Expr>(static_cast<Expr*>($3)));
    }
    | MIN '(' col ')'
    {
        $$ = new AggExpr(AGG_MIN, std::shared_ptr<Expr>(static_cast<Expr*>($3)));
    }
    | MAX '(' col ')'
    {
        $$ = new AggExpr(AGG_MAX, std::shared_ptr<Expr>(static_cast<Expr*>($3)));
    }
    | AVG '(' col ')'
    {
        $$ = new AggExpr(AGG_AVG, std::shared_ptr<Expr>(static_cast<Expr*>($3)));
    }
    ;

op:
        '='
    {
        $$ = SV_OP_EQ;
    }
    |   '<'
    {
        $$ = SV_OP_LT;
    }
    |   '>'
    {
        $$ = SV_OP_GT;
    }
    |   NEQ
    {
        $$ = SV_OP_NE;
    }
    |   LEQ
    {
        $$ = SV_OP_LE;
    }
    |   GEQ
    {
        $$ = SV_OP_GE;
    }
    ;

expr:
        value
    {
        $$ = static_cast<Expr*>($1);
    }
    |   col
    {
        $$ = static_cast<Expr*>($1);
    }
    |   agg_expr
    {
        $$ = $1;
    }
    |   '+' expr %prec UMINUS
    {
        /* 一元+操作符，值不变 */
        $$ = $2;
    }
    |   '-' expr %prec UMINUS
    {
        /* 一元-操作符，对数值取反 */
        if (auto intLit = dynamic_cast<IntLit*>($2)) {
            intLit->val = -intLit->val;
            $$ = intLit;
        } else if (auto floatLit = dynamic_cast<FloatLit*>($2)) {
            floatLit->val = -floatLit->val;
            $$ = floatLit;
        } else {
            /* 如果不是简单数值，创建一个取反的算术表达式 */
            auto zero = new IntLit(0);
            $$ = new ArithExpr(std::shared_ptr<Expr>(zero), ArithOp::SUB, std::shared_ptr<Expr>($2));
        }
    }
    |   expr '+' expr
    {
        $$ = new ArithExpr(std::shared_ptr<Expr>($1), ArithOp::ADD, std::shared_ptr<Expr>($3));
    }
    |   expr '-' expr
    {
        $$ = new ArithExpr(std::shared_ptr<Expr>($1), ArithOp::SUB, std::shared_ptr<Expr>($3));
    }
    |   expr '*' expr
    {
        $$ = new ArithExpr(std::shared_ptr<Expr>($1), ArithOp::MUL, std::shared_ptr<Expr>($3));
    }
    |   expr '/' expr
    {
        $$ = new ArithExpr(std::shared_ptr<Expr>($1), ArithOp::DIV, std::shared_ptr<Expr>($3));
    }
    |   '(' expr ')'
    {
        $$ = $2;
    }
    ;

setClauses:
        setClause
    {
        $$ = new std::vector<SetClause*>{$1};
    }
    |   setClauses ',' setClause
    {
        $$->push_back($3);
    }
    ;

setClause:
        colName '=' expr
    {
        $$ = new SetClause($1, std::shared_ptr<Expr>($3));
    }
    ;

selector:
        '*'
    {
        $$ = nullptr;
    }
    |   colList
    ;

tableList:
        tableRef
    {
        $$ = $1;
    }
    |   tableList JOIN tableRef ON whereClause
    {
        $$ = new JoinExpr(std::shared_ptr<TreeNode>($1), std::shared_ptr<TreeNode>($3), adopt<BinaryExpr>($5), INNER_JOIN);
    }
    |   tableList JOIN tableRef
    {
        $$ = new JoinExpr(std::shared_ptr<TreeNode>($1), std::shared_ptr<TreeNode>($3), {}, INNER_JOIN);
    }
    |   tableList SEMI JOIN tableRef ON whereClause
    {
        $$ = new JoinExpr(std::shared_ptr<TreeNode>($1), std::shared_ptr<TreeNode>($4), adopt<BinaryExpr>($6), SEMI_JOIN);
    }
    |   tableList ',' tableRef
    {
        $$ = new JoinExpr(std::shared_ptr<TreeNode>($1), std::shared_ptr<TreeNode>($3), {}, INNER_JOIN);
    }
    ;

tableRef:
        tbName
    {
        $$ = new TableRef($1);
    }
    |   tbName AS alias
    {
        $$ = new TableRef($1, $3);
    }
    |   tbName alias
    {
        $$ = new TableRef($1, $2);
    }
    ;

alias:
        IDENTIFIER
    {
        $$ = $1;
    }
    ;

opt_asc_desc:
    ASC          { $$ = OrderBy_ASC;     }
    |  DESC      { $$ = OrderBy_DESC;    }
    |       { $$ = OrderBy_DEFAULT; }
    ;

opt_limit_clause:
    LIMIT VALUE_INT
    {
        $$ = $2;
    }
    | /* 空 */ { $$ = -1; }  // -1 表示没有 LIMIT
    ;

set_knob_type:
    ENABLE_NESTLOOP { $$ = EnableNestLoop; }
    |   ENABLE_SORTMERGE { $$ = EnableSortMerge; }
    ;

file_path:
    FILE_NAME
    {
        $$ = $1;
    }
    |   firsts last
    {
        $$ = $1 + $2;
    }
    ;

firsts:
    first
    {
        $$ = $1;
    }
    |   firsts first
    {
        $$ += $2;
    }
    ;

first:
    '.' '.' '/'
    {
        $$ = "../";
    }
    |   file_name '/'
    {
        $$ = $1 + "/";
    }
    ;

last:
    file_name '.' file_name
    {
        $$ = $1 + "." + $3;
    }
    ;

tbName: IDENTIFIER;
file_name: IDENTIFIER;
colName: IDENTIFIER;
%%
