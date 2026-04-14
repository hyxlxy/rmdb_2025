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
%type <sv_node> stmt dbStmt ddl dml txnStmt setStmt
%type <sv_field> field
%type <sv_fields> fieldList
%type <sv_type_len> type
%type <sv_comp_op> op
%type <sv_expr> expr agg_expr colItem having_clause compare_expr logical_expr
%type <sv_val> value
%type <sv_vals> valueList
%type <sv_str> tbName colName file_path firsts last first
%type <sv_str> file_name
%type <sv_strs> colNameList
%type <sv_node> tableList
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
        parse_tree = $1;
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

    ;

txnStmt:
        TXN_BEGIN
    {
        $$ = std::make_shared<TxnBegin>();
    }
    |   TXN_COMMIT
    {
        $$ = std::make_shared<TxnCommit>();
    }
    |   TXN_ABORT
    {
        $$ = std::make_shared<TxnAbort>();
    }
    | TXN_ROLLBACK
    {
        $$ = std::make_shared<TxnRollback>();
    }
    ;

dbStmt:
        SHOW TABLES
    {
        $$ = std::make_shared<ShowTables>();
    }
    |   SHOW INDEX FROM tbName
    {
        $$ = std::make_shared<ShowIndex>($4);
    }
    ;

setStmt:
        SET set_knob_type '=' VALUE_BOOL
    {
        $$ = std::make_shared<SetStmt>($2, $4);
    }
    ;
ddl:
        CREATE TABLE tbName '(' fieldList ')'
    {
        $$ = std::make_shared<CreateTable>($3, $5);
    }
    |   DROP TABLE tbName
    {
        $$ = std::make_shared<DropTable>($3);
    }
    |   DESC tbName
    {
        $$ = std::make_shared<DescTable>($2);
    }
    |   CREATE INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<CreateIndex>($3, $5);
    }
    |   DROP INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<DropIndex>($3, $5);
    }
    |   CREATE STATIC_CHECKPOINT
    {
        $$ = std::make_shared<CreateStaticCheckpoint>();
    }
    ;

dml:
        INSERT INTO tbName VALUES '(' valueList ')'
    {
        $$ = std::make_shared<InsertStmt>($3, $6);
    }
    |   DELETE FROM tbName optWhereClause
    {
        $$ = std::make_shared<DeleteStmt>($3, $4);
    }
    |   UPDATE tbName SET setClauses optWhereClause
    {
        $$ = std::make_shared<UpdateStmt>($2, $4, $5);
    }
    | SELECT selector FROM tableList optWhereClause groupby_clause having_clause opt_order_clause opt_limit_clause
    {
        $$ = std::make_shared<SelectStmt>($2, $4, $5, $8, $6, $7, $9);
    }
    | LOAD file_path INTO tbName
    {
        $$ = std::make_shared<LoadStmt>($2, $4);
    }
    ;

groupby_clause
    : /* 空 */ { $$ = {}; }
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
        $$ = std::static_pointer_cast<Expr>(std::make_shared<LogicalExpr>($1, LogicalExpr::AND, $3));
    }
    ;

compare_expr:
        agg_expr op value
    {
        // 聚合函数与常量比较，如 COUNT(*) > 1
        $$ = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>($1, $2, std::static_pointer_cast<Expr>($3)));
    }
    |   agg_expr op agg_expr
    {
        // 聚合函数之间比较，如 MIN(score) > MAX(other)
        $$ = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>($1, $2, $3));
    }
    |   col op value
    {
        // 列与常量比较
        $$ = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>(std::static_pointer_cast<Expr>($1), $2, std::static_pointer_cast<Expr>($3)));
    }
    |   col op agg_expr
    {
        // 列与聚合函数比较
        $$ = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>(std::static_pointer_cast<Expr>($1), $2, $3));
    }
    ;


opt_order_clause:
    ORDER BY order_clause_list
    {
        $$ = $3;
    }
    | /* 空 */ { $$ = {}; }
    ;

order_clause_list:
      order_clause
    {
        $$ = std::vector<std::shared_ptr<OrderBy>>{$1};
    }
    | order_clause_list ',' order_clause
    {
        $$.push_back($3);
    }
    ;

order_clause:
      col opt_asc_desc
    {
        $$ = std::make_shared<OrderBy>($1, $2);
    }
    ;

fieldList:
        field
    {
        $$ = std::vector<std::shared_ptr<Field>>{$1};
    }
    |   fieldList ',' field
    {
        $$.push_back($3);
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
        $$ = std::make_shared<ColDef>($1, $2);
    }
    ;

type:
        INT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, $3);
    }
    |   FLOAT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
    ;

valueList:
        value
    {
        $$ = std::vector<std::shared_ptr<Value>>{$1};
    }
    |   valueList ',' value
    {
        $$.push_back($3);
    }
    ;

value:
        VALUE_INT
    {
        $$ = std::make_shared<IntLit>($1);
    }
    |   '+' VALUE_INT %prec UMINUS
    {
        $$ = std::make_shared<IntLit>($2);
    }
    |   '-' VALUE_INT %prec UMINUS
    {
        $$ = std::make_shared<IntLit>(-$2);
    }
    |   VALUE_FLOAT
    {
        $$ = std::make_shared<FloatLit>($1);
    }
    |   '+' VALUE_FLOAT %prec UMINUS
    {
        $$ = std::make_shared<FloatLit>($2);
    }
    |   '-' VALUE_FLOAT %prec UMINUS
    {
        $$ = std::make_shared<FloatLit>(-$2);
    }
    |   VALUE_STRING
    {
        $$ = std::make_shared<StringLit>($1);
    }
    |   VALUE_BOOL
    {
        $$ = std::make_shared<BoolLit>($1);
    }
    ;

condition:
        col op expr
    {
        $$ = std::make_shared<BinaryExpr>($1, $2, $3);
    }
    ;

optWhereClause:
        /* epsilon */ { /* ignore*/ }
    |   WHERE whereClause
    {
        $$ = $2;
    }
    ;

whereClause:
        condition
    {
        $$ = std::vector<std::shared_ptr<BinaryExpr>>{$1};
    }
    |   whereClause AND condition
    {
        $$.push_back($3);
    }
    ;

col:
        tbName '.' colName
    {
        $$ = std::make_shared<Col>($1, $3);
    }
    |   colName
    {
        $$ = std::make_shared<Col>("", $1);
    }
    ;

colList:
      colItem
    {
        $$.push_back($1);
    }
    | colList ',' colItem
    {
        $$.push_back($3);
    }
    ;


colItem:
      col
    {
        $$ = $1;
    }
    | agg_expr
    {
        $$ = $1;
    }
    | col AS IDENTIFIER
    {
        $$ = std::make_shared<AliasExpr>($1, $3);
    }
    | agg_expr AS IDENTIFIER
    {
        $$ = std::make_shared<AliasExpr>($1, $3);
    }
    ;


agg_expr:
      COUNT '(' '*' ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_COUNT, nullptr);
    }
    | COUNT '(' col ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_COUNT, $3);
    }
    | SUM '(' col ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_SUM, $3);
    }
    | MIN '(' col ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_MIN, $3);
    }
    | MAX '(' col ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_MAX, $3);
    }
    | AVG '(' col ')'
    {
        $$ = std::make_shared<AggExpr>(AGG_AVG, $3);
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
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   col
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   agg_expr
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   '+' expr %prec UMINUS
    {
        /* 一元+操作符，值不变 */
        $$ = $2;
    }
    |   '-' expr %prec UMINUS
    {
        /* 一元-操作符，对数值取反 */
        if (auto intLit = std::dynamic_pointer_cast<IntLit>($2)) {
            $$ = std::make_shared<IntLit>(-intLit->val);
        } else if (auto floatLit = std::dynamic_pointer_cast<FloatLit>($2)) {
            $$ = std::make_shared<FloatLit>(-floatLit->val);
        } else {
            /* 如果不是简单数值，创建一个取反的算术表达式 */
            auto zero = std::make_shared<IntLit>(0);
            $$ = std::make_shared<ArithExpr>(std::static_pointer_cast<Expr>(zero), ArithOp::SUB, $2);
        }
    }
    |   expr '+' expr
    {
        $$ = std::make_shared<ArithExpr>($1, ArithOp::ADD, $3);
    }
    |   expr '-' expr
    {
        $$ = std::make_shared<ArithExpr>($1, ArithOp::SUB, $3);
    }
    |   expr '*' expr
    {
        $$ = std::make_shared<ArithExpr>($1, ArithOp::MUL, $3);
    }
    |   expr '/' expr
    {
        $$ = std::make_shared<ArithExpr>($1, ArithOp::DIV, $3);
    }
    |   '(' expr ')'
    {
        $$ = $2;
    }
    ;

setClauses:
        setClause
    {
        $$ = std::vector<std::shared_ptr<SetClause>>{$1};
    }
    |   setClauses ',' setClause
    {
        $$.push_back($3);
    }
    ;

setClause:
        colName '=' expr
    {
        $$ = std::make_shared<SetClause>($1, $3);
    }
    ;

selector:
        '*'
    {
        $$ = {};
    }
    |   colList
    ;

tableList:
        tbName
    {
        $$ = std::make_shared<TableRef>($1);
    }
    |   tableList JOIN tbName ON whereClause
    {
        $$ = std::make_shared<JoinExpr>($1,std::make_shared<TableRef>($3),$5,INNER_JOIN);
    }
    |   tableList JOIN tbName
    {
        $$ = std::make_shared<JoinExpr>($1, std::make_shared<TableRef>($3), std::vector<std::shared_ptr<BinaryExpr>>{}, INNER_JOIN);
    }
    |   tableList SEMI JOIN tbName ON whereClause
    {
        $$ = std::make_shared<JoinExpr>($1,std::make_shared<TableRef>($4),$6,SEMI_JOIN);
    }
    |   tableList ',' tbName
    {
        $$ = std::make_shared<JoinExpr>($1, std::make_shared<TableRef>($3), std::vector<std::shared_ptr<BinaryExpr>>{}, INNER_JOIN);
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
