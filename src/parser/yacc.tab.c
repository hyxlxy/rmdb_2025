/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "yacc.y"

#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);

void yyerror(YYLTYPE *locp, const char* s) {
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;

#line 86 "yacc.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "yacc.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_SHOW = 3,                       /* SHOW  */
  YYSYMBOL_TABLES = 4,                     /* TABLES  */
  YYSYMBOL_CREATE = 5,                     /* CREATE  */
  YYSYMBOL_TABLE = 6,                      /* TABLE  */
  YYSYMBOL_DROP = 7,                       /* DROP  */
  YYSYMBOL_DESC = 8,                       /* DESC  */
  YYSYMBOL_INSERT = 9,                     /* INSERT  */
  YYSYMBOL_INTO = 10,                      /* INTO  */
  YYSYMBOL_VALUES = 11,                    /* VALUES  */
  YYSYMBOL_DELETE = 12,                    /* DELETE  */
  YYSYMBOL_FROM = 13,                      /* FROM  */
  YYSYMBOL_ASC = 14,                       /* ASC  */
  YYSYMBOL_ORDER = 15,                     /* ORDER  */
  YYSYMBOL_BY = 16,                        /* BY  */
  YYSYMBOL_SUM = 17,                       /* SUM  */
  YYSYMBOL_HAVING = 18,                    /* HAVING  */
  YYSYMBOL_ON = 19,                        /* ON  */
  YYSYMBOL_COUNT = 20,                     /* COUNT  */
  YYSYMBOL_MIN = 21,                       /* MIN  */
  YYSYMBOL_MAX = 22,                       /* MAX  */
  YYSYMBOL_AVG = 23,                       /* AVG  */
  YYSYMBOL_LOAD = 24,                      /* LOAD  */
  YYSYMBOL_OUTPUT_FILE = 25,               /* OUTPUT_FILE  */
  YYSYMBOL_OFF = 26,                       /* OFF  */
  YYSYMBOL_EXPLAIN = 27,                   /* EXPLAIN  */
  YYSYMBOL_WHERE = 28,                     /* WHERE  */
  YYSYMBOL_UPDATE = 29,                    /* UPDATE  */
  YYSYMBOL_SET = 30,                       /* SET  */
  YYSYMBOL_SELECT = 31,                    /* SELECT  */
  YYSYMBOL_INT = 32,                       /* INT  */
  YYSYMBOL_CHAR = 33,                      /* CHAR  */
  YYSYMBOL_FLOAT = 34,                     /* FLOAT  */
  YYSYMBOL_INDEX = 35,                     /* INDEX  */
  YYSYMBOL_AND = 36,                       /* AND  */
  YYSYMBOL_JOIN = 37,                      /* JOIN  */
  YYSYMBOL_EXIT = 38,                      /* EXIT  */
  YYSYMBOL_HELP = 39,                      /* HELP  */
  YYSYMBOL_TXN_BEGIN = 40,                 /* TXN_BEGIN  */
  YYSYMBOL_TXN_COMMIT = 41,                /* TXN_COMMIT  */
  YYSYMBOL_TXN_ABORT = 42,                 /* TXN_ABORT  */
  YYSYMBOL_TXN_ROLLBACK = 43,              /* TXN_ROLLBACK  */
  YYSYMBOL_ORDER_BY = 44,                  /* ORDER_BY  */
  YYSYMBOL_ENABLE_NESTLOOP = 45,           /* ENABLE_NESTLOOP  */
  YYSYMBOL_ENABLE_SORTMERGE = 46,          /* ENABLE_SORTMERGE  */
  YYSYMBOL_SEMI = 47,                      /* SEMI  */
  YYSYMBOL_GROUP = 48,                     /* GROUP  */
  YYSYMBOL_AS = 49,                        /* AS  */
  YYSYMBOL_LIMIT = 50,                     /* LIMIT  */
  YYSYMBOL_STATIC_CHECKPOINT = 51,         /* STATIC_CHECKPOINT  */
  YYSYMBOL_LEQ = 52,                       /* LEQ  */
  YYSYMBOL_NEQ = 53,                       /* NEQ  */
  YYSYMBOL_GEQ = 54,                       /* GEQ  */
  YYSYMBOL_T_EOF = 55,                     /* T_EOF  */
  YYSYMBOL_56_ = 56,                       /* '='  */
  YYSYMBOL_57_ = 57,                       /* '<'  */
  YYSYMBOL_58_ = 58,                       /* '>'  */
  YYSYMBOL_59_ = 59,                       /* '+'  */
  YYSYMBOL_60_ = 60,                       /* '-'  */
  YYSYMBOL_61_ = 61,                       /* '*'  */
  YYSYMBOL_62_ = 62,                       /* '/'  */
  YYSYMBOL_63_ = 63,                       /* '('  */
  YYSYMBOL_64_ = 64,                       /* ')'  */
  YYSYMBOL_IDENTIFIER = 65,                /* IDENTIFIER  */
  YYSYMBOL_VALUE_STRING = 66,              /* VALUE_STRING  */
  YYSYMBOL_VALUE_INT = 67,                 /* VALUE_INT  */
  YYSYMBOL_VALUE_FLOAT = 68,               /* VALUE_FLOAT  */
  YYSYMBOL_VALUE_BOOL = 69,                /* VALUE_BOOL  */
  YYSYMBOL_FILE_NAME = 70,                 /* FILE_NAME  */
  YYSYMBOL_71_ = 71,                       /* ';'  */
  YYSYMBOL_72_ = 72,                       /* ','  */
  YYSYMBOL_73_ = 73,                       /* '.'  */
  YYSYMBOL_YYACCEPT = 74,                  /* $accept  */
  YYSYMBOL_start = 75,                     /* start  */
  YYSYMBOL_stmt = 76,                      /* stmt  */
  YYSYMBOL_txnStmt = 77,                   /* txnStmt  */
  YYSYMBOL_dbStmt = 78,                    /* dbStmt  */
  YYSYMBOL_setStmt = 79,                   /* setStmt  */
  YYSYMBOL_ddl = 80,                       /* ddl  */
  YYSYMBOL_dml = 81,                       /* dml  */
  YYSYMBOL_groupby_clause = 82,            /* groupby_clause  */
  YYSYMBOL_having_clause = 83,             /* having_clause  */
  YYSYMBOL_logical_expr = 84,              /* logical_expr  */
  YYSYMBOL_compare_expr = 85,              /* compare_expr  */
  YYSYMBOL_opt_order_clause = 86,          /* opt_order_clause  */
  YYSYMBOL_order_clause_list = 87,         /* order_clause_list  */
  YYSYMBOL_order_clause = 88,              /* order_clause  */
  YYSYMBOL_fieldList = 89,                 /* fieldList  */
  YYSYMBOL_colNameList = 90,               /* colNameList  */
  YYSYMBOL_field = 91,                     /* field  */
  YYSYMBOL_type = 92,                      /* type  */
  YYSYMBOL_valueList = 93,                 /* valueList  */
  YYSYMBOL_value = 94,                     /* value  */
  YYSYMBOL_condition = 95,                 /* condition  */
  YYSYMBOL_optWhereClause = 96,            /* optWhereClause  */
  YYSYMBOL_whereClause = 97,               /* whereClause  */
  YYSYMBOL_col = 98,                       /* col  */
  YYSYMBOL_colList = 99,                   /* colList  */
  YYSYMBOL_colItem = 100,                  /* colItem  */
  YYSYMBOL_agg_expr = 101,                 /* agg_expr  */
  YYSYMBOL_op = 102,                       /* op  */
  YYSYMBOL_expr = 103,                     /* expr  */
  YYSYMBOL_setClauses = 104,               /* setClauses  */
  YYSYMBOL_setClause = 105,                /* setClause  */
  YYSYMBOL_selector = 106,                 /* selector  */
  YYSYMBOL_tableList = 107,                /* tableList  */
  YYSYMBOL_opt_asc_desc = 108,             /* opt_asc_desc  */
  YYSYMBOL_opt_limit_clause = 109,         /* opt_limit_clause  */
  YYSYMBOL_set_knob_type = 110,            /* set_knob_type  */
  YYSYMBOL_file_path = 111,                /* file_path  */
  YYSYMBOL_firsts = 112,                   /* firsts  */
  YYSYMBOL_first = 113,                    /* first  */
  YYSYMBOL_last = 114,                     /* last  */
  YYSYMBOL_tbName = 115,                   /* tbName  */
  YYSYMBOL_file_name = 116,                /* file_name  */
  YYSYMBOL_colName = 117                   /* colName  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  62
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   236

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  74
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  44
/* YYNRULES -- Number of rules.  */
#define YYNRULES  118
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  229

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   316


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      63,    64,    61,    59,    72,    60,    73,    62,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    71,
      57,    56,    58,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    65,    66,    67,    68,    69,    70
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    71,    71,    76,    81,    86,    91,    96,   104,   105,
     106,   107,   108,   113,   117,   121,   125,   132,   136,   143,
     149,   153,   157,   161,   165,   169,   176,   180,   184,   188,
     192,   199,   200,   204,   205,   209,   213,   220,   225,   230,
     235,   244,   248,   252,   256,   263,   270,   274,   281,   285,
     292,   299,   303,   307,   314,   318,   325,   329,   333,   337,
     344,   351,   352,   359,   363,   370,   374,   381,   385,   393,
     397,   401,   405,   413,   417,   421,   425,   429,   433,   440,
     444,   448,   452,   456,   460,   467,   471,   475,   479,   483,
     487,   491,   498,   502,   509,   516,   520,   524,   528,   532,
     536,   540,   547,   548,   549,   553,   557,   561,   562,   566,
     570,   577,   581,   588,   592,   599,   605,   606,   607
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "SHOW", "TABLES",
  "CREATE", "TABLE", "DROP", "DESC", "INSERT", "INTO", "VALUES", "DELETE",
  "FROM", "ASC", "ORDER", "BY", "SUM", "HAVING", "ON", "COUNT", "MIN",
  "MAX", "AVG", "LOAD", "OUTPUT_FILE", "OFF", "EXPLAIN", "WHERE", "UPDATE",
  "SET", "SELECT", "INT", "CHAR", "FLOAT", "INDEX", "AND", "JOIN", "EXIT",
  "HELP", "TXN_BEGIN", "TXN_COMMIT", "TXN_ABORT", "TXN_ROLLBACK",
  "ORDER_BY", "ENABLE_NESTLOOP", "ENABLE_SORTMERGE", "SEMI", "GROUP", "AS",
  "LIMIT", "STATIC_CHECKPOINT", "LEQ", "NEQ", "GEQ", "T_EOF", "'='", "'<'",
  "'>'", "'+'", "'-'", "'*'", "'/'", "'('", "')'", "IDENTIFIER",
  "VALUE_STRING", "VALUE_INT", "VALUE_FLOAT", "VALUE_BOOL", "FILE_NAME",
  "';'", "','", "'.'", "$accept", "start", "stmt", "txnStmt", "dbStmt",
  "setStmt", "ddl", "dml", "groupby_clause", "having_clause",
  "logical_expr", "compare_expr", "opt_order_clause", "order_clause_list",
  "order_clause", "fieldList", "colNameList", "field", "type", "valueList",
  "value", "condition", "optWhereClause", "whereClause", "col", "colList",
  "colItem", "agg_expr", "op", "expr", "setClauses", "setClause",
  "selector", "tableList", "opt_asc_desc", "opt_limit_clause",
  "set_knob_type", "file_path", "firsts", "first", "last", "tbName",
  "file_name", "colName", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-151)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-117)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      89,    12,     1,     6,   -34,    35,    47,   -35,   -34,    79,
      85,  -151,  -151,  -151,  -151,  -151,  -151,  -151,    69,    14,
    -151,  -151,  -151,  -151,  -151,  -151,    78,   -34,   -34,  -151,
     -34,   -34,  -151,  -151,   -34,   -34,  -151,  -151,     9,    90,
       3,  -151,    50,    92,    -4,  -151,  -151,    77,    72,    76,
      86,    91,    95,  -151,    67,   114,   106,  -151,   132,   135,
     109,  -151,  -151,  -151,   -34,   122,   123,  -151,   124,   153,
     160,   128,   -34,  -151,  -151,   -49,  -151,   126,  -151,  -151,
     125,   127,   -15,   127,   127,   127,   130,    94,   131,   -34,
     126,  -151,   126,   126,   126,   134,   127,  -151,  -151,  -151,
     133,  -151,    -5,  -151,   137,  -151,   136,   142,   143,   144,
     145,   146,  -151,  -151,  -151,    11,  -151,  -151,    16,  -151,
     139,    31,  -151,    62,   100,  -151,   165,    99,  -151,   126,
    -151,   -12,  -151,  -151,  -151,  -151,  -151,  -151,   -34,   174,
     -34,   164,  -151,   126,  -151,   150,  -151,  -151,  -151,   126,
    -151,  -151,  -151,  -151,  -151,    73,  -151,   127,  -151,  -151,
    -151,  -151,  -151,  -151,   -12,  -151,   -12,  -151,  -151,  -151,
     115,   196,   -34,  -151,   200,   199,  -151,   151,  -151,  -151,
     100,  -151,   115,   101,   -12,   -12,   -12,   127,   201,    94,
      94,   204,   157,  -151,  -151,   161,   161,  -151,   165,   127,
     106,   187,  -151,    99,    99,   208,   175,  -151,   165,    94,
      -3,    -3,   127,   159,  -151,  -151,  -151,  -151,  -151,  -151,
     155,  -151,    70,  -151,   127,  -151,  -151,  -151,  -151
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     4,     3,    13,    14,    15,    16,     7,     0,     0,
      11,     8,    12,     9,    10,    17,     0,     0,     0,    25,
       0,     0,   116,    22,     0,     0,   117,   109,     0,     0,
       0,   111,     0,     0,     0,   107,   108,     0,     0,     0,
       0,     0,     0,    95,   118,    69,    96,    67,    70,     0,
       0,    66,     1,     2,     0,     0,     0,    21,     0,     0,
      61,     0,     0,   112,   110,     0,   114,     0,     6,     5,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    18,     0,     0,     0,     0,     0,    27,   113,    30,
       0,   118,    61,    92,     0,    19,     0,     0,     0,     0,
       0,     0,    71,    68,    72,    61,    97,    65,     0,    46,
       0,     0,    48,     0,     0,    63,    62,     0,   115,     0,
      28,     0,    75,    73,    74,    76,    77,    78,     0,     0,
       0,    31,    20,     0,    51,     0,    53,    50,    23,     0,
      24,    58,    56,    57,    59,     0,    54,     0,    83,    82,
      84,    79,    80,    81,     0,    93,     0,    85,    86,    87,
      94,    99,     0,   101,     0,    33,    47,     0,    49,    26,
       0,    64,    60,     0,     0,     0,     0,     0,     0,     0,
       0,    42,     0,    55,    91,    88,    89,    90,    98,     0,
      32,    34,    35,     0,     0,     0,   106,    52,   100,     0,
       0,     0,     0,     0,    29,    36,    39,    40,    37,    38,
      41,    43,   104,   105,     0,   103,   102,    45,    44
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -151,  -151,  -151,  -151,  -151,  -151,  -151,  -151,  -151,  -151,
      19,  -151,  -151,  -151,     5,  -151,   138,    87,  -151,  -151,
    -121,    74,   -81,  -150,   -10,    44,   147,    -6,   -94,   -43,
    -151,   107,  -151,  -151,  -151,  -151,  -151,  -151,  -151,   195,
    -151,    -2,   -39,   -50
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    18,    19,    20,    21,    22,    23,    24,   175,   191,
     201,   202,   206,   220,   221,   118,   121,   119,   147,   155,
     167,   125,    97,   126,   168,    56,    57,   169,   164,   170,
     102,   103,    59,   115,   227,   214,    47,    39,    40,    41,
      74,    60,    42,    61
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      55,    75,    33,   156,    58,    48,    43,    27,    49,    50,
      51,    52,    30,    76,    48,    78,    25,    49,    50,    51,
      52,   130,    79,    96,   100,    65,    66,   104,    67,    68,
      36,    32,    69,    70,   141,    37,    28,   198,    38,    96,
     117,    31,   120,   122,   122,    34,   107,    26,   138,   208,
      54,   166,    29,    54,   151,   152,   153,   154,   139,   193,
      35,   128,    91,   151,   152,   153,   154,   129,    36,    62,
      99,   106,   108,   109,   110,   111,    38,    55,   225,   104,
     142,    58,    71,   140,   226,    63,   127,   116,   143,   216,
     218,    64,     1,   120,     2,   148,     3,     4,     5,   178,
      72,     6,    48,   149,    44,    49,    50,    51,    52,   210,
     211,    48,    76,     7,    49,    50,    51,    52,     8,     9,
      10,   182,    77,   183,    45,    46,   150,    11,    12,    13,
      14,    15,    16,    80,   149,    81,   171,   179,   173,    82,
    -116,   195,   196,   197,    17,   180,    53,   127,    89,    83,
      54,   158,   159,   160,    84,   161,   162,   163,    85,    54,
     184,   185,   186,    86,    95,   194,   151,   152,   153,   154,
     188,   144,   145,   146,   184,   185,   186,   127,    87,    55,
     203,    88,    90,    58,   204,    92,    93,    94,    96,   127,
      98,   101,    54,   131,   105,   112,   114,   124,    36,   203,
     132,   157,   222,   204,   217,   219,   133,   134,   135,   136,
     137,   172,   174,   177,   222,   187,   189,   190,   192,   205,
     199,   207,   186,   209,   212,   213,   223,   224,   215,   228,
     176,   181,   123,   200,   113,    73,   165
};

static const yytype_uint8 yycheck[] =
{
      10,    40,     4,   124,    10,    17,     8,     6,    20,    21,
      22,    23,     6,    62,    17,    19,     4,    20,    21,    22,
      23,   102,    26,    28,    73,    27,    28,    77,    30,    31,
      65,    65,    34,    35,   115,    70,    35,   187,    73,    28,
      90,    35,    92,    93,    94,    10,    61,    35,    37,   199,
      65,    63,    51,    65,    66,    67,    68,    69,    47,   180,
      13,   100,    64,    66,    67,    68,    69,    72,    65,     0,
      72,    81,    82,    83,    84,    85,    73,    87,     8,   129,
      64,    87,    73,    72,    14,    71,    96,    89,    72,   210,
     211,    13,     3,   143,     5,    64,     7,     8,     9,   149,
      10,    12,    17,    72,    25,    20,    21,    22,    23,   203,
     204,    17,    62,    24,    20,    21,    22,    23,    29,    30,
      31,   164,    30,   166,    45,    46,    64,    38,    39,    40,
      41,    42,    43,    56,    72,    63,   138,    64,   140,    63,
      73,   184,   185,   186,    55,    72,    61,   157,    13,    63,
      65,    52,    53,    54,    63,    56,    57,    58,    63,    65,
      59,    60,    61,    49,    11,    64,    66,    67,    68,    69,
     172,    32,    33,    34,    59,    60,    61,   187,    72,   189,
     190,    49,    73,   189,   190,    63,    63,    63,    28,   199,
      62,    65,    65,    56,    69,    65,    65,    63,    65,   209,
      64,    36,   212,   209,   210,   211,    64,    64,    64,    64,
      64,    37,    48,    63,   224,    19,    16,    18,    67,    15,
      19,    64,    61,    36,    16,    50,    67,    72,   209,   224,
     143,   157,    94,   189,    87,    40,   129
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     5,     7,     8,     9,    12,    24,    29,    30,
      31,    38,    39,    40,    41,    42,    43,    55,    75,    76,
      77,    78,    79,    80,    81,     4,    35,     6,    35,    51,
       6,    35,    65,   115,    10,    13,    65,    70,    73,   111,
     112,   113,   116,   115,    25,    45,    46,   110,    17,    20,
      21,    22,    23,    61,    65,    98,    99,   100,   101,   106,
     115,   117,     0,    71,    13,   115,   115,   115,   115,   115,
     115,    73,    10,   113,   114,   116,    62,    30,    19,    26,
      56,    63,    63,    63,    63,    63,    49,    72,    49,    13,
      73,   115,    63,    63,    63,    11,    28,    96,    62,   115,
      73,    65,   104,   105,   117,    69,    98,    61,    98,    98,
      98,    98,    65,   100,    65,   107,   115,   117,    89,    91,
     117,    90,   117,    90,    63,    95,    97,    98,   116,    72,
      96,    56,    64,    64,    64,    64,    64,    64,    37,    47,
      72,    96,    64,    72,    32,    33,    34,    92,    64,    72,
      64,    66,    67,    68,    69,    93,    94,    36,    52,    53,
      54,    56,    57,    58,   102,   105,    63,    94,    98,   101,
     103,   115,    37,   115,    48,    82,    91,    63,   117,    64,
      72,    95,   103,   103,    59,    60,    61,    19,   115,    16,
      18,    83,    67,    94,    64,   103,   103,   103,    97,    19,
      99,    84,    85,    98,   101,    15,    86,    64,    97,    36,
     102,   102,    16,    50,   109,    84,    94,   101,    94,   101,
      87,    88,    98,    67,    72,     8,    14,   108,    88
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    74,    75,    75,    75,    75,    75,    75,    76,    76,
      76,    76,    76,    77,    77,    77,    77,    78,    78,    79,
      80,    80,    80,    80,    80,    80,    81,    81,    81,    81,
      81,    82,    82,    83,    83,    84,    84,    85,    85,    85,
      85,    86,    86,    87,    87,    88,    89,    89,    90,    90,
      91,    92,    92,    92,    93,    93,    94,    94,    94,    94,
      95,    96,    96,    97,    97,    98,    98,    99,    99,   100,
     100,   100,   100,   101,   101,   101,   101,   101,   101,   102,
     102,   102,   102,   102,   102,   103,   103,   103,   103,   103,
     103,   103,   104,   104,   105,   106,   106,   107,   107,   107,
     107,   107,   108,   108,   108,   109,   109,   110,   110,   111,
     111,   112,   112,   113,   113,   114,   115,   116,   117
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     3,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     4,     4,
       6,     3,     2,     6,     6,     2,     7,     4,     5,     9,
       4,     0,     3,     0,     2,     1,     3,     3,     3,     3,
       3,     3,     0,     1,     3,     2,     1,     3,     1,     3,
       2,     1,     4,     1,     1,     3,     1,     1,     1,     1,
       3,     0,     2,     1,     3,     3,     1,     1,     3,     1,
       1,     3,     3,     4,     4,     4,     4,     4,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     3,
       3,     3,     1,     3,     3,     1,     1,     1,     5,     3,
       6,     3,     1,     1,     0,     2,     0,     1,     1,     1,
       2,     1,     2,     3,     2,     3,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]));
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* start: stmt ';'  */
#line 72 "yacc.y"
    {
        parse_tree = (yyvsp[-1].sv_node);
        YYACCEPT;
    }
#line 1750 "yacc.tab.c"
    break;

  case 3: /* start: HELP  */
#line 77 "yacc.y"
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
#line 1759 "yacc.tab.c"
    break;

  case 4: /* start: EXIT  */
#line 82 "yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1768 "yacc.tab.c"
    break;

  case 5: /* start: SET OUTPUT_FILE OFF  */
#line 87 "yacc.y"
    {
        parse_tree = std::make_shared<SetKnobStmt>(ast::SetKnobType::OutputFile, false);
        YYACCEPT;
    }
#line 1777 "yacc.tab.c"
    break;

  case 6: /* start: SET OUTPUT_FILE ON  */
#line 92 "yacc.y"
    {
        parse_tree = std::make_shared<SetKnobStmt>(ast::SetKnobType::OutputFile, true);
        YYACCEPT;
    }
#line 1786 "yacc.tab.c"
    break;

  case 7: /* start: T_EOF  */
#line 97 "yacc.y"
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
#line 1795 "yacc.tab.c"
    break;

  case 13: /* txnStmt: TXN_BEGIN  */
#line 114 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnBegin>();
    }
#line 1803 "yacc.tab.c"
    break;

  case 14: /* txnStmt: TXN_COMMIT  */
#line 118 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnCommit>();
    }
#line 1811 "yacc.tab.c"
    break;

  case 15: /* txnStmt: TXN_ABORT  */
#line 122 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnAbort>();
    }
#line 1819 "yacc.tab.c"
    break;

  case 16: /* txnStmt: TXN_ROLLBACK  */
#line 126 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TxnRollback>();
    }
#line 1827 "yacc.tab.c"
    break;

  case 17: /* dbStmt: SHOW TABLES  */
#line 133 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<ShowTables>();
    }
#line 1835 "yacc.tab.c"
    break;

  case 18: /* dbStmt: SHOW INDEX FROM tbName  */
#line 137 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<ShowIndex>((yyvsp[0].sv_str));
    }
#line 1843 "yacc.tab.c"
    break;

  case 19: /* setStmt: SET set_knob_type '=' VALUE_BOOL  */
#line 144 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SetStmt>((yyvsp[-2].sv_setKnobType), (yyvsp[0].sv_bool));
    }
#line 1851 "yacc.tab.c"
    break;

  case 20: /* ddl: CREATE TABLE tbName '(' fieldList ')'  */
#line 150 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateTable>((yyvsp[-3].sv_str), (yyvsp[-1].sv_fields));
    }
#line 1859 "yacc.tab.c"
    break;

  case 21: /* ddl: DROP TABLE tbName  */
#line 154 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropTable>((yyvsp[0].sv_str));
    }
#line 1867 "yacc.tab.c"
    break;

  case 22: /* ddl: DESC tbName  */
#line 158 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DescTable>((yyvsp[0].sv_str));
    }
#line 1875 "yacc.tab.c"
    break;

  case 23: /* ddl: CREATE INDEX tbName '(' colNameList ')'  */
#line 162 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1883 "yacc.tab.c"
    break;

  case 24: /* ddl: DROP INDEX tbName '(' colNameList ')'  */
#line 166 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DropIndex>((yyvsp[-3].sv_str), (yyvsp[-1].sv_strs));
    }
#line 1891 "yacc.tab.c"
    break;

  case 25: /* ddl: CREATE STATIC_CHECKPOINT  */
#line 170 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<CreateStaticCheckpoint>();
    }
#line 1899 "yacc.tab.c"
    break;

  case 26: /* dml: INSERT INTO tbName VALUES '(' valueList ')'  */
#line 177 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<InsertStmt>((yyvsp[-4].sv_str), (yyvsp[-1].sv_vals));
    }
#line 1907 "yacc.tab.c"
    break;

  case 27: /* dml: DELETE FROM tbName optWhereClause  */
#line 181 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<DeleteStmt>((yyvsp[-1].sv_str), (yyvsp[0].sv_conds));
    }
#line 1915 "yacc.tab.c"
    break;

  case 28: /* dml: UPDATE tbName SET setClauses optWhereClause  */
#line 185 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<UpdateStmt>((yyvsp[-3].sv_str), (yyvsp[-1].sv_set_clauses), (yyvsp[0].sv_conds));
    }
#line 1923 "yacc.tab.c"
    break;

  case 29: /* dml: SELECT selector FROM tableList optWhereClause groupby_clause having_clause opt_order_clause opt_limit_clause  */
#line 189 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<SelectStmt>((yyvsp[-7].sv_exprs), (yyvsp[-5].sv_node), (yyvsp[-4].sv_conds), (yyvsp[-1].sv_orderbys), (yyvsp[-3].sv_exprs), (yyvsp[-2].sv_expr), (yyvsp[0].sv_int));
    }
#line 1931 "yacc.tab.c"
    break;

  case 30: /* dml: LOAD file_path INTO tbName  */
#line 193 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<LoadStmt>((yyvsp[-2].sv_str), (yyvsp[0].sv_str));
    }
#line 1939 "yacc.tab.c"
    break;

  case 31: /* groupby_clause: %empty  */
#line 199 "yacc.y"
                { (yyval.sv_exprs) = {}; }
#line 1945 "yacc.tab.c"
    break;

  case 32: /* groupby_clause: GROUP BY colList  */
#line 200 "yacc.y"
                       { (yyval.sv_exprs) = (yyvsp[0].sv_exprs); }
#line 1951 "yacc.tab.c"
    break;

  case 33: /* having_clause: %empty  */
#line 204 "yacc.y"
                { (yyval.sv_expr) = nullptr; }
#line 1957 "yacc.tab.c"
    break;

  case 34: /* having_clause: HAVING logical_expr  */
#line 205 "yacc.y"
                          { (yyval.sv_expr) = (yyvsp[0].sv_expr); }
#line 1963 "yacc.tab.c"
    break;

  case 35: /* logical_expr: compare_expr  */
#line 210 "yacc.y"
    {
        (yyval.sv_expr) = (yyvsp[0].sv_expr);
    }
#line 1971 "yacc.tab.c"
    break;

  case 36: /* logical_expr: logical_expr AND logical_expr  */
#line 214 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>(std::make_shared<LogicalExpr>((yyvsp[-2].sv_expr), LogicalExpr::AND, (yyvsp[0].sv_expr)));
    }
#line 1979 "yacc.tab.c"
    break;

  case 37: /* compare_expr: agg_expr op value  */
#line 221 "yacc.y"
    {
        // 聚合函数与常量比较，如 COUNT(*) > 1
        (yyval.sv_expr) = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>((yyvsp[-2].sv_expr), (yyvsp[-1].sv_comp_op), std::static_pointer_cast<Expr>((yyvsp[0].sv_val))));
    }
#line 1988 "yacc.tab.c"
    break;

  case 38: /* compare_expr: agg_expr op agg_expr  */
#line 226 "yacc.y"
    {
        // 聚合函数之间比较，如 MIN(score) > MAX(other)
        (yyval.sv_expr) = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>((yyvsp[-2].sv_expr), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_expr)));
    }
#line 1997 "yacc.tab.c"
    break;

  case 39: /* compare_expr: col op value  */
#line 231 "yacc.y"
    {
        // 列与常量比较
        (yyval.sv_expr) = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>(std::static_pointer_cast<Expr>((yyvsp[-2].sv_col)), (yyvsp[-1].sv_comp_op), std::static_pointer_cast<Expr>((yyvsp[0].sv_val))));
    }
#line 2006 "yacc.tab.c"
    break;

  case 40: /* compare_expr: col op agg_expr  */
#line 236 "yacc.y"
    {
        // 列与聚合函数比较
        (yyval.sv_expr) = std::static_pointer_cast<Expr>(std::make_shared<CompareExpr>(std::static_pointer_cast<Expr>((yyvsp[-2].sv_col)), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_expr)));
    }
#line 2015 "yacc.tab.c"
    break;

  case 41: /* opt_order_clause: ORDER BY order_clause_list  */
#line 245 "yacc.y"
    {
        (yyval.sv_orderbys) = (yyvsp[0].sv_orderbys);
    }
#line 2023 "yacc.tab.c"
    break;

  case 42: /* opt_order_clause: %empty  */
#line 248 "yacc.y"
                { (yyval.sv_orderbys) = {}; }
#line 2029 "yacc.tab.c"
    break;

  case 43: /* order_clause_list: order_clause  */
#line 253 "yacc.y"
    {
        (yyval.sv_orderbys) = std::vector<std::shared_ptr<OrderBy>>{(yyvsp[0].sv_orderby)};
    }
#line 2037 "yacc.tab.c"
    break;

  case 44: /* order_clause_list: order_clause_list ',' order_clause  */
#line 257 "yacc.y"
    {
        (yyval.sv_orderbys).push_back((yyvsp[0].sv_orderby));
    }
#line 2045 "yacc.tab.c"
    break;

  case 45: /* order_clause: col opt_asc_desc  */
#line 264 "yacc.y"
    {
        (yyval.sv_orderby) = std::make_shared<OrderBy>((yyvsp[-1].sv_col), (yyvsp[0].sv_orderby_dir));
    }
#line 2053 "yacc.tab.c"
    break;

  case 46: /* fieldList: field  */
#line 271 "yacc.y"
    {
        (yyval.sv_fields) = std::vector<std::shared_ptr<Field>>{(yyvsp[0].sv_field)};
    }
#line 2061 "yacc.tab.c"
    break;

  case 47: /* fieldList: fieldList ',' field  */
#line 275 "yacc.y"
    {
        (yyval.sv_fields).push_back((yyvsp[0].sv_field));
    }
#line 2069 "yacc.tab.c"
    break;

  case 48: /* colNameList: colName  */
#line 282 "yacc.y"
    {
        (yyval.sv_strs) = std::vector<std::string>{(yyvsp[0].sv_str)};
    }
#line 2077 "yacc.tab.c"
    break;

  case 49: /* colNameList: colNameList ',' colName  */
#line 286 "yacc.y"
    {
        (yyval.sv_strs).push_back((yyvsp[0].sv_str));
    }
#line 2085 "yacc.tab.c"
    break;

  case 50: /* field: colName type  */
#line 293 "yacc.y"
    {
        (yyval.sv_field) = std::make_shared<ColDef>((yyvsp[-1].sv_str), (yyvsp[0].sv_type_len));
    }
#line 2093 "yacc.tab.c"
    break;

  case 51: /* type: INT  */
#line 300 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
#line 2101 "yacc.tab.c"
    break;

  case 52: /* type: CHAR '(' VALUE_INT ')'  */
#line 304 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_STRING, (yyvsp[-1].sv_int));
    }
#line 2109 "yacc.tab.c"
    break;

  case 53: /* type: FLOAT  */
#line 308 "yacc.y"
    {
        (yyval.sv_type_len) = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
#line 2117 "yacc.tab.c"
    break;

  case 54: /* valueList: value  */
#line 315 "yacc.y"
    {
        (yyval.sv_vals) = std::vector<std::shared_ptr<Value>>{(yyvsp[0].sv_val)};
    }
#line 2125 "yacc.tab.c"
    break;

  case 55: /* valueList: valueList ',' value  */
#line 319 "yacc.y"
    {
        (yyval.sv_vals).push_back((yyvsp[0].sv_val));
    }
#line 2133 "yacc.tab.c"
    break;

  case 56: /* value: VALUE_INT  */
#line 326 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<IntLit>((yyvsp[0].sv_int));
    }
#line 2141 "yacc.tab.c"
    break;

  case 57: /* value: VALUE_FLOAT  */
#line 330 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<FloatLit>((yyvsp[0].sv_float));
    }
#line 2149 "yacc.tab.c"
    break;

  case 58: /* value: VALUE_STRING  */
#line 334 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<StringLit>((yyvsp[0].sv_str));
    }
#line 2157 "yacc.tab.c"
    break;

  case 59: /* value: VALUE_BOOL  */
#line 338 "yacc.y"
    {
        (yyval.sv_val) = std::make_shared<BoolLit>((yyvsp[0].sv_bool));
    }
#line 2165 "yacc.tab.c"
    break;

  case 60: /* condition: col op expr  */
#line 345 "yacc.y"
    {
        (yyval.sv_cond) = std::make_shared<BinaryExpr>((yyvsp[-2].sv_col), (yyvsp[-1].sv_comp_op), (yyvsp[0].sv_expr));
    }
#line 2173 "yacc.tab.c"
    break;

  case 61: /* optWhereClause: %empty  */
#line 351 "yacc.y"
                      { /* ignore*/ }
#line 2179 "yacc.tab.c"
    break;

  case 62: /* optWhereClause: WHERE whereClause  */
#line 353 "yacc.y"
    {
        (yyval.sv_conds) = (yyvsp[0].sv_conds);
    }
#line 2187 "yacc.tab.c"
    break;

  case 63: /* whereClause: condition  */
#line 360 "yacc.y"
    {
        (yyval.sv_conds) = std::vector<std::shared_ptr<BinaryExpr>>{(yyvsp[0].sv_cond)};
    }
#line 2195 "yacc.tab.c"
    break;

  case 64: /* whereClause: whereClause AND condition  */
#line 364 "yacc.y"
    {
        (yyval.sv_conds).push_back((yyvsp[0].sv_cond));
    }
#line 2203 "yacc.tab.c"
    break;

  case 65: /* col: tbName '.' colName  */
#line 371 "yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>((yyvsp[-2].sv_str), (yyvsp[0].sv_str));
    }
#line 2211 "yacc.tab.c"
    break;

  case 66: /* col: colName  */
#line 375 "yacc.y"
    {
        (yyval.sv_col) = std::make_shared<Col>("", (yyvsp[0].sv_str));
    }
#line 2219 "yacc.tab.c"
    break;

  case 67: /* colList: colItem  */
#line 382 "yacc.y"
    {
        (yyval.sv_exprs).push_back((yyvsp[0].sv_expr));
    }
#line 2227 "yacc.tab.c"
    break;

  case 68: /* colList: colList ',' colItem  */
#line 386 "yacc.y"
    {
        (yyval.sv_exprs).push_back((yyvsp[0].sv_expr));
    }
#line 2235 "yacc.tab.c"
    break;

  case 69: /* colItem: col  */
#line 394 "yacc.y"
    {
        (yyval.sv_expr) = (yyvsp[0].sv_col);
    }
#line 2243 "yacc.tab.c"
    break;

  case 70: /* colItem: agg_expr  */
#line 398 "yacc.y"
    {
        (yyval.sv_expr) = (yyvsp[0].sv_expr);
    }
#line 2251 "yacc.tab.c"
    break;

  case 71: /* colItem: col AS IDENTIFIER  */
#line 402 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AliasExpr>((yyvsp[-2].sv_col), (yyvsp[0].sv_str));
    }
#line 2259 "yacc.tab.c"
    break;

  case 72: /* colItem: agg_expr AS IDENTIFIER  */
#line 406 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AliasExpr>((yyvsp[-2].sv_expr), (yyvsp[0].sv_str));
    }
#line 2267 "yacc.tab.c"
    break;

  case 73: /* agg_expr: COUNT '(' '*' ')'  */
#line 414 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_COUNT, nullptr);
    }
#line 2275 "yacc.tab.c"
    break;

  case 74: /* agg_expr: COUNT '(' col ')'  */
#line 418 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_COUNT, (yyvsp[-1].sv_col));
    }
#line 2283 "yacc.tab.c"
    break;

  case 75: /* agg_expr: SUM '(' col ')'  */
#line 422 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_SUM, (yyvsp[-1].sv_col));
    }
#line 2291 "yacc.tab.c"
    break;

  case 76: /* agg_expr: MIN '(' col ')'  */
#line 426 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_MIN, (yyvsp[-1].sv_col));
    }
#line 2299 "yacc.tab.c"
    break;

  case 77: /* agg_expr: MAX '(' col ')'  */
#line 430 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_MAX, (yyvsp[-1].sv_col));
    }
#line 2307 "yacc.tab.c"
    break;

  case 78: /* agg_expr: AVG '(' col ')'  */
#line 434 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<AggExpr>(AGG_AVG, (yyvsp[-1].sv_col));
    }
#line 2315 "yacc.tab.c"
    break;

  case 79: /* op: '='  */
#line 441 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_EQ;
    }
#line 2323 "yacc.tab.c"
    break;

  case 80: /* op: '<'  */
#line 445 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LT;
    }
#line 2331 "yacc.tab.c"
    break;

  case 81: /* op: '>'  */
#line 449 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GT;
    }
#line 2339 "yacc.tab.c"
    break;

  case 82: /* op: NEQ  */
#line 453 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_NE;
    }
#line 2347 "yacc.tab.c"
    break;

  case 83: /* op: LEQ  */
#line 457 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_LE;
    }
#line 2355 "yacc.tab.c"
    break;

  case 84: /* op: GEQ  */
#line 461 "yacc.y"
    {
        (yyval.sv_comp_op) = SV_OP_GE;
    }
#line 2363 "yacc.tab.c"
    break;

  case 85: /* expr: value  */
#line 468 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_val));
    }
#line 2371 "yacc.tab.c"
    break;

  case 86: /* expr: col  */
#line 472 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_col));
    }
#line 2379 "yacc.tab.c"
    break;

  case 87: /* expr: agg_expr  */
#line 476 "yacc.y"
    {
        (yyval.sv_expr) = std::static_pointer_cast<Expr>((yyvsp[0].sv_expr));
    }
#line 2387 "yacc.tab.c"
    break;

  case 88: /* expr: expr '+' expr  */
#line 480 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ArithOp::ADD, (yyvsp[0].sv_expr));
    }
#line 2395 "yacc.tab.c"
    break;

  case 89: /* expr: expr '-' expr  */
#line 484 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ArithOp::SUB, (yyvsp[0].sv_expr));
    }
#line 2403 "yacc.tab.c"
    break;

  case 90: /* expr: expr '*' expr  */
#line 488 "yacc.y"
    {
        (yyval.sv_expr) = std::make_shared<ArithExpr>((yyvsp[-2].sv_expr), ArithOp::MUL, (yyvsp[0].sv_expr));
    }
#line 2411 "yacc.tab.c"
    break;

  case 91: /* expr: '(' expr ')'  */
#line 492 "yacc.y"
    {
        (yyval.sv_expr) = (yyvsp[-1].sv_expr);
    }
#line 2419 "yacc.tab.c"
    break;

  case 92: /* setClauses: setClause  */
#line 499 "yacc.y"
    {
        (yyval.sv_set_clauses) = std::vector<std::shared_ptr<SetClause>>{(yyvsp[0].sv_set_clause)};
    }
#line 2427 "yacc.tab.c"
    break;

  case 93: /* setClauses: setClauses ',' setClause  */
#line 503 "yacc.y"
    {
        (yyval.sv_set_clauses).push_back((yyvsp[0].sv_set_clause));
    }
#line 2435 "yacc.tab.c"
    break;

  case 94: /* setClause: colName '=' expr  */
#line 510 "yacc.y"
    {
        (yyval.sv_set_clause) = std::make_shared<SetClause>((yyvsp[-2].sv_str), (yyvsp[0].sv_expr));
    }
#line 2443 "yacc.tab.c"
    break;

  case 95: /* selector: '*'  */
#line 517 "yacc.y"
    {
        (yyval.sv_exprs) = {};
    }
#line 2451 "yacc.tab.c"
    break;

  case 97: /* tableList: tbName  */
#line 525 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<TableRef>((yyvsp[0].sv_str));
    }
#line 2459 "yacc.tab.c"
    break;

  case 98: /* tableList: tableList JOIN tbName ON whereClause  */
#line 529 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<JoinExpr>((yyvsp[-4].sv_node),std::make_shared<TableRef>((yyvsp[-2].sv_str)),(yyvsp[0].sv_conds),INNER_JOIN);
    }
#line 2467 "yacc.tab.c"
    break;

  case 99: /* tableList: tableList JOIN tbName  */
#line 533 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<JoinExpr>((yyvsp[-2].sv_node), std::make_shared<TableRef>((yyvsp[0].sv_str)), std::vector<std::shared_ptr<BinaryExpr>>{}, INNER_JOIN);
    }
#line 2475 "yacc.tab.c"
    break;

  case 100: /* tableList: tableList SEMI JOIN tbName ON whereClause  */
#line 537 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<JoinExpr>((yyvsp[-5].sv_node),std::make_shared<TableRef>((yyvsp[-2].sv_str)),(yyvsp[0].sv_conds),SEMI_JOIN);
    }
#line 2483 "yacc.tab.c"
    break;

  case 101: /* tableList: tableList ',' tbName  */
#line 541 "yacc.y"
    {
        (yyval.sv_node) = std::make_shared<JoinExpr>((yyvsp[-2].sv_node), std::make_shared<TableRef>((yyvsp[0].sv_str)), std::vector<std::shared_ptr<BinaryExpr>>{}, INNER_JOIN);
    }
#line 2491 "yacc.tab.c"
    break;

  case 102: /* opt_asc_desc: ASC  */
#line 547 "yacc.y"
                 { (yyval.sv_orderby_dir) = OrderBy_ASC;     }
#line 2497 "yacc.tab.c"
    break;

  case 103: /* opt_asc_desc: DESC  */
#line 548 "yacc.y"
                 { (yyval.sv_orderby_dir) = OrderBy_DESC;    }
#line 2503 "yacc.tab.c"
    break;

  case 104: /* opt_asc_desc: %empty  */
#line 549 "yacc.y"
            { (yyval.sv_orderby_dir) = OrderBy_DEFAULT; }
#line 2509 "yacc.tab.c"
    break;

  case 105: /* opt_limit_clause: LIMIT VALUE_INT  */
#line 554 "yacc.y"
    {
        (yyval.sv_int) = (yyvsp[0].sv_int);
    }
#line 2517 "yacc.tab.c"
    break;

  case 106: /* opt_limit_clause: %empty  */
#line 557 "yacc.y"
                { (yyval.sv_int) = -1; }
#line 2523 "yacc.tab.c"
    break;

  case 107: /* set_knob_type: ENABLE_NESTLOOP  */
#line 561 "yacc.y"
                    { (yyval.sv_setKnobType) = EnableNestLoop; }
#line 2529 "yacc.tab.c"
    break;

  case 108: /* set_knob_type: ENABLE_SORTMERGE  */
#line 562 "yacc.y"
                         { (yyval.sv_setKnobType) = EnableSortMerge; }
#line 2535 "yacc.tab.c"
    break;

  case 109: /* file_path: FILE_NAME  */
#line 567 "yacc.y"
    {
        (yyval.sv_str) = (yyvsp[0].sv_str);
    }
#line 2543 "yacc.tab.c"
    break;

  case 110: /* file_path: firsts last  */
#line 571 "yacc.y"
    {
        (yyval.sv_str) = (yyvsp[-1].sv_str) + (yyvsp[0].sv_str);
    }
#line 2551 "yacc.tab.c"
    break;

  case 111: /* firsts: first  */
#line 578 "yacc.y"
    {
        (yyval.sv_str) = (yyvsp[0].sv_str);
    }
#line 2559 "yacc.tab.c"
    break;

  case 112: /* firsts: firsts first  */
#line 582 "yacc.y"
    {
        (yyval.sv_str) += (yyvsp[0].sv_str);
    }
#line 2567 "yacc.tab.c"
    break;

  case 113: /* first: '.' '.' '/'  */
#line 589 "yacc.y"
    {
        (yyval.sv_str) = "../";
    }
#line 2575 "yacc.tab.c"
    break;

  case 114: /* first: file_name '/'  */
#line 593 "yacc.y"
    {
        (yyval.sv_str) = (yyvsp[-1].sv_str) + "/";
    }
#line 2583 "yacc.tab.c"
    break;

  case 115: /* last: file_name '.' file_name  */
#line 600 "yacc.y"
    {
        (yyval.sv_str) = (yyvsp[-2].sv_str) + "." + (yyvsp[0].sv_str);
    }
#line 2591 "yacc.tab.c"
    break;


#line 2595 "yacc.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 608 "yacc.y"

