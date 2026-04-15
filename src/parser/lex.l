/* keywords are case insensitive */
%option caseless
    /* we don't need yywrap() function */
%option noyywrap
    /* we don't need yyunput() function */
%option nounput
    /* we don't need input() function */
%option noinput
    /* enable location */
%option bison-bridge
%option bison-locations
%option reentrant

%{
#include "ast.h"
#include "yacc.tab.h"
#include <iostream>

// automatically update location
#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for (int i = 0; yytext[i] != '\0'; i++) { \
        if(yytext[i] == '\n') { \
            yylloc->last_line++; \
            yylloc->last_column = 1; \
        } else { \
            yylloc->last_column++; \
        } \
    }

%}

alpha [a-zA-Z]
digit [0-9]
white_space [ \t]+
new_line "\r"|"\n"|"\r\n"
sign "+"|"-"
identifier {alpha}(_|{alpha}|{digit})*
value_int {digit}+
value_float {digit}+\.({digit}+)?
value_string '[^']*'
single_op ";"|"("|")"|","|"*"|"="|">"|"<"|"."|"+"|"-"|"/"
/* 支持相对路径和绝对路径的文件名规则，但不匹配简单的表名.列名格式 */
file_name ((\/)|((\.\.?\/)+))[a-zA-Z0-9_]+(\/[a-zA-Z0-9_]+)*\.[a-zA-Z0-9_]+|[a-zA-Z0-9_]+(\/[a-zA-Z0-9_]+)+\.[a-zA-Z0-9_]{3,}

%x STATE_COMMENT

%%
    /* block comment */
"/*" { BEGIN(STATE_COMMENT); }
<STATE_COMMENT>"*/" { BEGIN(INITIAL); }
<STATE_COMMENT>[^*] { /* ignore the text of the comment */ }
<STATE_COMMENT>\* { /* ignore *'s that aren't part of */ }
    /* single line comment */
"--".* { /* ignore single line comment */ }
    /* white space and new line */
{white_space} { /* ignore white space */ }
{new_line} { /* ignore new line */ }
    /* keywords */
"SHOW" { return SHOW; }

"BEGIN" { return TXN_BEGIN; }
"COMMIT" { return TXN_COMMIT; }
"ABORT" { return TXN_ABORT; }
"ROLLBACK" { return TXN_ROLLBACK; }
"TABLES" { return TABLES; }
"CREATE" { return CREATE; }
"STATIC_CHECKPOINT" { return STATIC_CHECKPOINT; }
"TABLE" { return TABLE; }
"DROP" { return DROP; }
"DESC" { return DESC; }
"INSERT" { return INSERT; }
"INTO" { return INTO; }
"VALUES" { return VALUES; }
"DELETE" { return DELETE; }
"FROM" { return FROM; }
"WHERE" { return WHERE; }
"UPDATE" { return UPDATE; }
"SET" { return SET; }
"SELECT" { return SELECT; }
"INT" { return INT; }
"CHAR" { return CHAR; }
"FLOAT" { return FLOAT; }
"INDEX" { return INDEX; }
"AND" { return AND; }
"JOIN" {return JOIN;}
"ON" {return ON;}
"EXIT" { return EXIT; }
"HELP" { return HELP; }
"ORDER" { return ORDER; }
"BY" {  return BY; }
"ASC" { return ASC; }
"GROUP" { return GROUP; }
"AS" { return AS; }
"HAVING" { return HAVING; }
"SEMI" {return SEMI; }
"ON" {return ON; }
"COUNT" { return COUNT; }
"SUM"   { return SUM; }
"MIN"   { return MIN; }
"MAX"   { return MAX; }
"AVG"   { return AVG; }
"EXPLAIN" { return EXPLAIN; }
"ENABLE_NESTLOOP" { return ENABLE_NESTLOOP; }
"ENABLE_SORTMERGE" { return ENABLE_SORTMERGE; }
"OUTPUT_FILE" {return OUTPUT_FILE; }
"OFF" {return OFF; }
"LOAD" {return LOAD; }
"LIMIT" { return LIMIT; }
"TRUE" { 
    yylval->sv_bool = true;
    return VALUE_BOOL; 
}
"FALSE" {
    yylval->sv_bool = false;
    return VALUE_BOOL;
}
    /* operators */
">=" { return GEQ; }
"<=" { return LEQ; }
"<>" { return NEQ; }
{single_op} { return yytext[0]; }
    /* id */
{identifier} {
    yylval->sv_str = yytext;
    return IDENTIFIER;
}
    /* literals */
{value_int} {
    yylval->sv_int = atoi(yytext);
    return VALUE_INT;
}
{value_float} {
    yylval->sv_float = atof(yytext);
    return VALUE_FLOAT;
}
{value_string} {
    yylval->sv_str = std::string(yytext + 1, strlen(yytext) - 2);
    return VALUE_STRING;
}
{file_name} {
    yylval->sv_str = yytext;
    return FILE_NAME;
}
    /* EOF */
<<EOF>> { return T_EOF; }
    /* unexpected char */
. { std::cerr << "Lexer Error: unexpected character " << yytext[0] << std::endl; }
%%
