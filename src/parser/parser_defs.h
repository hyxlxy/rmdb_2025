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

#include "defs.h"

typedef struct yy_buffer_state *YY_BUFFER_STATE;
typedef void *yyscan_t;

int yyparse(yyscan_t scanner);
int yyparse();

int yylex_init(yyscan_t *scanner);
int yylex_destroy(yyscan_t scanner);

YY_BUFFER_STATE yy_scan_string(const char *str, yyscan_t scanner);
YY_BUFFER_STATE yy_scan_string(const char *str);

void yy_delete_buffer(YY_BUFFER_STATE buffer, yyscan_t scanner);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
