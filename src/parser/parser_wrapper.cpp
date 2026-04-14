#include "parser_defs.h"

static thread_local yyscan_t tls_scanner = nullptr;

int yyparse()
{
    if (tls_scanner == nullptr)
    {
        yylex_init(&tls_scanner);
    }
    return yyparse(tls_scanner);
}

YY_BUFFER_STATE yy_scan_string(const char *str)
{
    if (tls_scanner == nullptr)
    {
        yylex_init(&tls_scanner);
    }
    return yy_scan_string(str, tls_scanner);
}

void yy_delete_buffer(YY_BUFFER_STATE buffer)
{
    if (buffer != nullptr && tls_scanner != nullptr)
    {
        yy_delete_buffer(buffer, tls_scanner);
    }
}
