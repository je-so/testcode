/*
 * Build Prototype of Context Oriented Parser.
 *
 * The context is considered a structured (or object-) data model.
 *
 * The text (linear representation of an idea) is read from left to right
 * and transformed into a structured representation (usage of C structs).
 *
 * The structured data represents the already read text and is considered
 * the context. Newly read text adds new data to the data model or transforms it.
 *
 * This simple prototype parses simple expressions with operator precedence.
 *
 * The following table lists the supported C operators.
 * The first row has the highest precedence (rank 1)
 * and the last row the lowest (rank 15).
 *
 * ---------------------------------------------------------------
 * Supported C Operators              | Associativity | Precedence
 * ---------------------------------------------------------------
 * () [] -> .                         | left to right |  1
 * ! ~ + -                            | right to left |  2
 * * / %                              | left to right |  3
 * + -                                | left to right |  4
 * << >>                              | left to right |  5
 * < <= > >=                          | left to right |  6
 * == !=                              | left to right |  7
 * &                                  | left to right |  8
 * ^                                  | left to right |  9
 * |                                  | left to right | 10
 * &&                                 | left to right | 11
 * ||                                 | left to right | 12
 * ?:                                 | right to left | 13   // is supported!!
 * = += -= *= /= %= <<= >>= &= ^= |=  | right to left | 14
 * ,                                  | left to right | 15
 *
 * Every simple expression starts with
 * 1. integer value  ['0'-'9']+
 * 2. subexpression  '(' expr ')'
 * 3. prefix operator ['+' '-' '!' '++' '--'] expr
 *
 * The parsed expression should is stored in a structured form
 * which uses the follwing AST types:
 *
 * struct expr_t {
 *    unsigned char type;
 * };
 * struct expr_integer_t {
 *    unsigned char type;
 *    int val;
 * };
 * struct expr_1ary_t {
 *    unsigned char type;
 *    expr_t * arg1;
 * };
 * struct expr_2ary_t {
 *    unsigned char type;
 *    unsigned char assign_type; // used for +=, -=, ...
 *    expr_t * arg1;
 *    expr_t * arg2;
 * };
 * struct expr_3ary_t {
 *    unsigned char type;
 *    struct expr_3ary_t * prev_expectmore; // used to build list of 3ary ops
 *                                          // whose arg3 is not yet matched
 *    expr_t * arg1;
 *    expr_t * arg2;
 *    expr_t * arg3;
 * };
 *
 * Already parsed input is stored as an expr_t.
 *
 * The parsing algorithm is simple and does not use recursion.
 *
 * All state is stored in the context (tree of expr_t).
 * Expression are on different levels to support operator precedence.
 * For every precedence rank there is a different level.
 * This simulates the stack of a LL(1) recursive descent parser used
 * to implement correct parsing of operator precedence.
 *
 * TODO: Describe AST structure and parse algorithm used !!
 *
 * Compile with:
 * > gcc -Wall -Wextra -Wconversion -std=gnu99 -ocop context_oriented_parser.c
 *
 * Run with:
 * > ./cop <filename>
 *
 * If file with name <filename> contains the expression
 *
 *   - 123[20] += 10 ? 20 ? 30 : 40 : 30
 *
 * the generated output should be
 *
 * {{- {123[20]}} += {10 ? {20 ? 30 : 40} : 30}}
 *
 * Where { } is used to show the associativity of the operators.
 *
 * */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================
 *  Memory Management
 * =================== */

typedef struct memblock memblock_t;
typedef struct mman mman_t;

#define memblock_SIZE        ((size_t)65536 - 8u/*malloc admin size?*/)
#define memblock_DATASIZE    (memblock_SIZE - offsetof(memblock_t, data))
#define memblock_ALIGN(size) (((size)+(offsetof(memblock_t, data)-1)) & ~(size_t)(offsetof(memblock_t, data)-1))

struct memblock {
   memblock_t* next;
   long        data[1];
};

int new_memblock(/*out*/memblock_t** block)
{
   *block = malloc(memblock_SIZE);
   if (!*block) {
      return ENOMEM;
   }
   return 0;
}

void free_memblock(memblock_t* block)
{
   free(block);
}

struct mman {
   size_t free_bytes;
   memblock_t * last;
};

void init_mman(mman_t* mm)
{
   mm->free_bytes = 0;
   mm->last = 0;
}

void free_mman(mman_t* mm)
{
   if (mm->last) {
      memblock_t* block = mm->last;
      do {
         memblock_t* freeblock = block;
         block = block->next;
         free_memblock(freeblock);
      } while (block != mm->last);
   }

   memset(mm, 0, sizeof(*mm));
}

void* alloc_mman(mman_t* mm, size_t size)
{
   int err;
   size_t off;

   size_t aligned_size = memblock_ALIGN(size);

   if (aligned_size < size || aligned_size > memblock_DATASIZE) {
      return 0;
   }

   if (aligned_size > mm->free_bytes) {
      memblock_t* block;

      err = new_memblock(&block);
      if (err) return 0;

      if (mm->last) {
         block->next = mm->last->next;
         mm->last->next = block;
      } else {
         block->next = block;
      }

      mm->free_bytes = memblock_DATASIZE;
      mm->last = block;
   }

   off = memblock_DATASIZE - mm->free_bytes;
   mm->free_bytes -= aligned_size;

   return mm->last->data + off;
}

/* ==========
 *  buffer_t
 * ========== */

typedef struct buffer buffer_t;

struct buffer {
   char * addr;
   size_t len;
   size_t off;
   size_t line;
   size_t col;
};

static /*err*/int read_buffer(/*out*/buffer_t* buffer, const char * filename)
{
   FILE * file = fopen(filename, "rb");
   long size;

   if (!file) {
      fprintf(stderr, "Can not open file '%s'\n", filename);
      return ENOENT;
   }

   if (0 != fseek(file, 0, SEEK_END) || 0 > (size = ftell(file)) || 0 != fseek(file, 0, SEEK_SET)) {
      fprintf(stderr, "Can not determine length of file '%s'\n", filename);
      fclose(file);
      return ENOENT;
   }

   buffer->addr = malloc((size_t) size);
   if (0 == buffer->addr) {
      fprintf(stderr, "Out of memory\n");
      fclose(file);
      return ENOMEM;
   }
   buffer->len  = (size_t) size;
   buffer->off  = 0;
   buffer->line = 1;
   buffer->col  = 0;

   size = (long) fread(buffer->addr, 1, buffer->len, file);

   fclose(file);

   if (buffer->len != (size_t) size) {
      fprintf(stderr, "Can not read file '%s'\n", filename);
      return EIO;
   }

   return 0;
}

static void free_buffer(buffer_t * buffer)
{
   free(buffer->addr);
   memset(buffer, 0, sizeof(*buffer));
}

static inline /*char*/int nextchar(buffer_t * buffer)
{
   while (buffer->off < buffer->len) {
      char c = buffer->addr[buffer->off];
      ++ buffer->off;
      ++ buffer->col; /* no multibyte support ! */
      if (c == '\n') {
         ++ buffer->line;
         buffer->col = 0;
         continue;
      } else if (c == ' ' || c == '\t') {
         continue;
      }

      return c;
   }

   return 0;
}

static inline /*char*/int peekchar(buffer_t * buffer)
{
   int c = nextchar(buffer);

   if (c) {
      -- buffer->off;
      -- buffer->col;
   }

   return c;
}

/* ===================
 *  struct data model
 * =================== */

#define NROF_PRECEDENCE_LEVEL 16

#define EXPR_COMMON_FIELDS  unsigned char type

typedef struct expr_t expr_t;
typedef struct expr_integer_t expr_integer_t;
typedef struct expr_1ary_t expr_1ary_t;
typedef struct expr_2ary_t expr_2ary_t;
typedef struct expr_3ary_t expr_3ary_t;

typedef enum expr_type {
   EXPR_VOID,
   EXPR_INTEGER,
   EXPR_1ARY_BRACKET,      /* () */
   EXPR_1ARY_MINUS,
   EXPR_1ARY_PLUS,
   EXPR_1ARY_LOGICAL_NOT,  /* ! */
   EXPR_1ARY_BITWISE_NOT,  /* ~ */
   EXPR_1ARY_PREINCR,
   EXPR_1ARY_PREDECR,
   EXPR_1ARY_POSTINCR,
   EXPR_1ARY_POSTDECR,
   EXPR_2ARY_COMMA,        /* , */
   EXPR_2ARY_MINUS,
   EXPR_2ARY_PLUS,
   EXPR_2ARY_MULT,
   EXPR_2ARY_DIV,
   EXPR_2ARY_LOGICAL_AND,  /* && */
   EXPR_2ARY_LOGICAL_OR,   /* || */
   EXPR_2ARY_BITWISE_AND,  /* & */
   EXPR_2ARY_BITWISE_OR,   /* | */
   EXPR_2ARY_BITWISE_XOR,  /* ^ */
   EXPR_2ARY_ASSIGN,       /* = */
   EXPR_2ARY_ARRAYINDEX,   /* [] */
   EXPR_TERNARY,           /* ?: */
} expr_type_e;

typedef enum precedence {
   PREC_INTEGER = 0,
   PREC_1ARY_BRACKET = 1,
   PREC_1ARY_MINUS = 2,
   PREC_1ARY_PLUS  = 2,
   PREC_1ARY_LOGICAL_NOT = 2,
   PREC_1ARY_BITWISE_NOT = 2,
   PREC_1ARY_PREINCR  = 2,
   PREC_1ARY_PREDECR  = 2,
   PREC_1ARY_POSTINCR = 2,
   PREC_1ARY_POSTDECR = 2,
   PREC_2ARY_COMMA    = 15,
   PREC_2ARY_MINUS = 4,
   PREC_2ARY_PLUS  = 4,
   PREC_2ARY_MULT  = 3,
   PREC_2ARY_DIV   = 3,
   PREC_2ARY_LOGICAL_AND = 11,
   PREC_2ARY_LOGICAL_OR  = 12,
   PREC_2ARY_BITWISE_AND = 8,
   PREC_2ARY_BITWISE_OR  = 10,
   PREC_2ARY_BITWISE_XOR = 9,
   PREC_2ARY_ASSIGN     = 14,
   PREC_2ARY_ARRAYINDEX = 1,
   PREC_TERNARY         = 13,
} precedence_e;

typedef enum associativity {
   ASSOC_LEFT,
   ASSOC_RIGHT
} associativity_e;

static char s_associativity_preclevel[NROF_PRECEDENCE_LEVEL] = {
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_RIGHT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_LEFT,
   ASSOC_RIGHT,
   ASSOC_RIGHT,
   ASSOC_LEFT
};

static char s_expr_type_names[][4] = {
   [EXPR_VOID]    = "",
   [EXPR_INTEGER] = "",
   [EXPR_1ARY_BRACKET] = "(",
   [EXPR_1ARY_MINUS] = "-",
   [EXPR_1ARY_PLUS]  = "+",
   [EXPR_1ARY_LOGICAL_NOT] = "!",
   [EXPR_1ARY_BITWISE_NOT] = "~",
   [EXPR_1ARY_PREINCR]  = "++",
   [EXPR_1ARY_PREDECR]  = "--",
   [EXPR_1ARY_POSTINCR] = "++",
   [EXPR_1ARY_POSTDECR] = "--",
   [EXPR_2ARY_COMMA]    = ",",
   [EXPR_2ARY_MINUS] = "-",
   [EXPR_2ARY_PLUS] = "+",
   [EXPR_2ARY_MULT] = "*",
   [EXPR_2ARY_DIV]  = "/",
   [EXPR_2ARY_LOGICAL_AND] = "&&",
   [EXPR_2ARY_LOGICAL_OR]  = "||",
   [EXPR_2ARY_BITWISE_AND] = "&",
   [EXPR_2ARY_BITWISE_OR]  = "|",
   [EXPR_2ARY_BITWISE_XOR] = "^",
   [EXPR_2ARY_ASSIGN]      = "=",
   [EXPR_2ARY_ARRAYINDEX]  = "[",
   [EXPR_TERNARY]          = "?",
};

static unsigned char s_expr_type_nrargs[] = {
   [EXPR_VOID]    = 0,
   [EXPR_INTEGER] = 0,
   [EXPR_1ARY_BRACKET] = 1,
   [EXPR_1ARY_MINUS] = 1,
   [EXPR_1ARY_PLUS]  = 1,
   [EXPR_1ARY_PREINCR] = 1,
   [EXPR_1ARY_PREDECR] = 1,
   [EXPR_1ARY_POSTINCR] = 1,
   [EXPR_1ARY_POSTDECR] = 1,
   [EXPR_1ARY_LOGICAL_NOT] = 1,
   [EXPR_1ARY_BITWISE_NOT] = 1,
   [EXPR_2ARY_MINUS] = 2,
   [EXPR_2ARY_PLUS]  = 2,
   [EXPR_2ARY_MULT]  = 2,
   [EXPR_2ARY_DIV]   = 2,
   [EXPR_2ARY_LOGICAL_AND] = 2,
   [EXPR_2ARY_LOGICAL_OR]  = 2,
   [EXPR_2ARY_BITWISE_AND] = 2,
   [EXPR_2ARY_BITWISE_OR]  = 2,
   [EXPR_2ARY_BITWISE_XOR] = 2,
   [EXPR_2ARY_ASSIGN]      = 2,
   [EXPR_2ARY_ARRAYINDEX]  = 2
};

struct expr_t {
   EXPR_COMMON_FIELDS;
};

struct expr_integer_t {
   EXPR_COMMON_FIELDS;
   int val;
};

struct expr_1ary_t {
   EXPR_COMMON_FIELDS;
   expr_t * arg1;
};

struct expr_2ary_t {
   EXPR_COMMON_FIELDS;
   unsigned char assign_type;
   expr_t * arg1;
   expr_t * arg2;
};

struct expr_3ary_t {
   EXPR_COMMON_FIELDS;
   expr_3ary_t * prev_expectmore; // used to build list of 3ary ops
                                  // whose arg3 is not yet matched
   expr_t * arg1;
   expr_t * arg2;
   expr_t * arg3;
};

static void print_expr2(expr_t * expr)
{
   switch ((expr_type_e) expr->type) {
      case EXPR_VOID:
         break;
      case EXPR_INTEGER: {
         expr_integer_t * e = (expr_integer_t*) expr;
         printf("%d", e->val);
         break;
      }
      case EXPR_1ARY_BRACKET: {
         expr_1ary_t * e = (expr_1ary_t*) expr;
         printf("(");
         print_expr2(e->arg1);
         printf(")");
         break;
      }
      case EXPR_1ARY_MINUS:
      case EXPR_1ARY_PLUS:
      case EXPR_1ARY_LOGICAL_NOT:
      case EXPR_1ARY_BITWISE_NOT:
      case EXPR_1ARY_PREINCR:
      case EXPR_1ARY_PREDECR: {
         expr_1ary_t * e = (expr_1ary_t*) expr;
         printf("{");
         printf("%s ", s_expr_type_names[e->type]);
         print_expr2(e->arg1);
         printf("}");
         break;
      }
      case EXPR_1ARY_POSTINCR:
      case EXPR_1ARY_POSTDECR: {
         expr_1ary_t * e = (expr_1ary_t*) expr;
         printf("{");
         print_expr2(e->arg1);
         printf(" %s", s_expr_type_names[e->type]);
         printf("}");
         break;
      }
      case EXPR_2ARY_COMMA:
      case EXPR_2ARY_MINUS:
      case EXPR_2ARY_PLUS:
      case EXPR_2ARY_MULT:
      case EXPR_2ARY_DIV:
      case EXPR_2ARY_LOGICAL_AND:
      case EXPR_2ARY_LOGICAL_OR:
      case EXPR_2ARY_BITWISE_AND:
      case EXPR_2ARY_BITWISE_OR:
      case EXPR_2ARY_BITWISE_XOR: {
         expr_2ary_t * e = (expr_2ary_t*) expr;
         printf("{");
         print_expr2(e->arg1);
         printf(" %s ", s_expr_type_names[e->type]);
         print_expr2(e->arg2);
         printf("}");
         break;
      }
      case EXPR_2ARY_ASSIGN: {
         expr_2ary_t * e = (expr_2ary_t*) expr;
         printf("{");
         print_expr2(e->arg1);
         printf(" %s%s ", s_expr_type_names[e->assign_type], s_expr_type_names[e->type]);
         print_expr2(e->arg2);
         printf("}");
         break;
      }
      case EXPR_2ARY_ARRAYINDEX: {
         expr_2ary_t * e = (expr_2ary_t*) expr;
         printf("{");
         print_expr2(e->arg1);
         printf("[");
         print_expr2(e->arg2);
         printf("]");
         printf("}");
         break;
      }
      case EXPR_TERNARY: {
         expr_3ary_t * e = (expr_3ary_t*) expr;
         printf("{");
         print_expr2(e->arg1);
         printf(" ? ");
         print_expr2(e->arg2);
         printf(" : ");
         print_expr2(e->arg3);
         printf("}");
         break;
      }
   }
}

static inline void print_expr(expr_t * expr)
{
   if (expr) {
      print_expr2(expr);
      printf("\n");
   }
}

/* ================
 *  parser_state_t
 * ================ */

typedef struct parser_state_t parser_state_t;
typedef struct precedence_level_t precedence_level_t;

struct precedence_level_t {
   expr_t  * root;   /* root of expression of this precedence level */
   expr_t ** last;   /* last assigned argument */
   expr_t ** expect; /* expect another expression as argument */
   /* root == 0 ==> last == 0
    * expect == 0 ==> no argument is expected, else expect points to arg1 or arg2 */
};

static inline void init_precedencelevel(precedence_level_t * level)
{
   level->root = 0;
   level->last = 0;
   level->expect = &level->root;
}

struct parser_state_t {
   parser_state_t * prev;
   precedence_level_t * current;
   expr_3ary_t * list_expectmore;
   precedence_level_t preclevel[NROF_PRECEDENCE_LEVEL];
};

void init_parserstate(/*out*/parser_state_t * state, parser_state_t * prev)
{
   state->prev = prev;
   state->current = &state->preclevel[0];
   state->list_expectmore = 0;
   for (unsigned i = 0; i < NROF_PRECEDENCE_LEVEL; ++i) {
      init_precedencelevel(&state->preclevel[i]);
   }
}

/* Every expression from precedence level i < prec is linked to higher level.
 *
 * Precondition:
 * Precedence level prec must expect an argument.
 * A precedence level 0 <= h < prec exists with root != 0 && expect == 0.
 * For all levels 0 <= x < h: root == 0 && expect == 0
 * For all levels h < x < prec: root == 0 && expect == 0 || root != 0 && expect != 0
 * Find first precedence level H Precedence levels 0..prec-1 must expect an argument.
 * */
int propagate_parserstate(parser_state_t * state, unsigned prec)
{
   unsigned h;

   assert(prec < NROF_PRECEDENCE_LEVEL);
   assert(state->preclevel[prec].expect);

   if (prec > PREC_TERNARY && state->list_expectmore) {
      return ENOENT;
   }

   for (h = 0; h < prec; ++h) {
      if (state->preclevel[h].root) {
         if (state->preclevel[h].expect) {
            return ENODATA;
         }
         break;
      }
   }

   if (h < prec) {
      for (unsigned i = h+1; i < prec; ++i) {
         if (state->preclevel[i].root) {
            assert(state->preclevel[i].expect);
            *state->preclevel[i].expect = state->preclevel[h].root;
            init_precedencelevel(&state->preclevel[h]);
            h = i;
         }
      }

      *state->preclevel[prec].expect = state->preclevel[h].root;
      state->preclevel[prec].last   = state->preclevel[prec].expect;
      state->preclevel[prec].expect = 0;
      init_precedencelevel(&state->preclevel[h]);
   }

   state->current = &state->preclevel[prec];

   return 0;
}

int propagatemax_parserstate(parser_state_t * state, /*out*/unsigned * prec)
{
   unsigned h;

   if (state->list_expectmore) {
      return ENOENT;
   }

   for (h = 0; h < NROF_PRECEDENCE_LEVEL; ++h) {
      if (state->preclevel[h].root) {
         if (state->preclevel[h].expect) {
            return ENODATA;
         }
         break;
      }
   }

   for (unsigned i = h+1; i < NROF_PRECEDENCE_LEVEL; ++i) {
      if (state->preclevel[i].root) {
         assert(state->preclevel[i].expect);
         *state->preclevel[i].expect = state->preclevel[h].root;
         state->preclevel[i].last    = state->preclevel[i].expect;
         state->preclevel[i].expect  = 0;
         init_precedencelevel(&state->preclevel[h]);
         h = i;
      }
   }

   state->current = &state->preclevel[h];

   *prec = h;
   return 0;
}

/* ==========
 *  parser_t
 * ========== */

typedef struct parser_t parser_t;

struct parser_t {
   buffer_t buffer;
   mman_t   mm;
   parser_state_t * freestate;
   parser_state_t * state;
   parser_state_t startstate;
   const char * filename;
};

int init_parser(/*out*/parser_t* parser, const char* filename)
{
   int err;

   err = read_buffer(&parser->buffer, filename);
   if (err) return err;

   init_mman(&parser->mm);

   parser->freestate = 0;
   parser->state = &parser->startstate;
   init_parserstate(parser->state, 0/*root start state*/);

   parser->filename = filename;

   return 0;
}

void free_parser(parser_t* parser)
{
   free_buffer(&parser->buffer);
   free_mman(&parser->mm);
}

void print_error(parser_t* parser, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   fprintf(stderr, "%s:%zd,%zd: ", parser->filename, parser->buffer.line, parser->buffer.col);
   vfprintf(stderr, format, args);
   va_end(args);
}

#define print_debug(...) // TODO: remove + rename next line

void _print_debug(parser_t* parser, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   fprintf(stdout, "%s:%zd,%zd: ", parser->filename, parser->buffer.line, parser->buffer.col);
   vfprintf(stdout, format, args);
   va_end(args);
}

static int new_exprinteger(parser_t* parser, /*out*/expr_integer_t** node, int val)
{
   *node = (expr_integer_t*) alloc_mman(&parser->mm, sizeof(expr_integer_t));
   if (!*node) return ENOMEM;
   (*node)->type = EXPR_INTEGER;
   (*node)->val = val;
   print_debug(parser, "Matched expr_integer_t %d\n", val);
   return 0;
}

static int new_expr1ary(parser_t* parser, /*out*/expr_1ary_t** node, unsigned char type)
{
   *node = (expr_1ary_t*) alloc_mman(&parser->mm, sizeof(expr_1ary_t));
   if (!*node) return ENOMEM;
   (*node)->type = type;
   print_debug(parser, "Matched expr_1ary_t %s\n", s_expr_type_names[type]);
   return 0;
}

static int new_expr2ary(parser_t* parser, /*out*/expr_2ary_t** node, unsigned char type, unsigned char assign_type)
{
   *node = (expr_2ary_t*) alloc_mman(&parser->mm, sizeof(expr_2ary_t));
   if (!*node) return ENOMEM;
   (*node)->type = type;
   (*node)->assign_type = assign_type;
   print_debug(parser, "Matched expr_2ary_t %s%s\n", type == EXPR_2ARY_ASSIGN ? s_expr_type_names[assign_type] : "", s_expr_type_names[type]);
   return 0;
}

static int new_expr3ary(parser_t* parser, /*out*/expr_3ary_t** node, unsigned char type)
{
   *node = (expr_3ary_t*) alloc_mman(&parser->mm, sizeof(expr_3ary_t));
   if (!*node) return ENOMEM;
   (*node)->type = type;
   (*node)->prev_expectmore = 0;
   print_debug(parser, "Matched expr_3ary_t %s\n", s_expr_type_names[type]);
   return 0;
}

int match1ary_parser(parser_t * parser, unsigned prec, expr_1ary_t * expr, unsigned char postfix_type)
{
   parser_state_t * state = parser->state;

   /* assume right associativity for all 1ary operators */

   precedence_level_t * pl = &state->preclevel[prec];

   if (! state->current->expect) {
      if (postfix_type == EXPR_VOID) {
         print_error(parser, "Operator '%s' not allowed as postifx operator\n", s_expr_type_names[expr->type]);
         return EINVAL;
      } else {
         expr->type = postfix_type;
      }
   }

   if (pl == state->current) {
      if (pl->expect) {
         *pl->expect = (expr_t*) expr;
         pl->last    = pl->expect;
         pl->expect  = &expr->arg1;
      } else {
         if (!pl->last || (*pl->last)->type != EXPR_INTEGER) goto ONERR_EXPECT;
         expr->arg1 = *pl->last;
         *pl->last  = (expr_t*) expr;
      }
   } else if (pl < state->current) {
      /* pl higher precedence */
      if (state->current->expect) {
         pl->root   = (expr_t*) expr;
         pl->last   = &pl->root;
         pl->expect = &expr->arg1;
      } else {
         if (!state->current->last || (*state->current->last)->type != EXPR_INTEGER) goto ONERR_EXPECT;
         pl->root   = (expr_t*) expr;
         pl->last   = &pl->root;
         pl->expect = 0;
         expr->arg1 = *state->current->last;
         *state->current->last  = 0;
         state->current->expect = state->current->last;
         state->current->last   = 0;
      }
      state->current = pl;
   } else {
      /* pl has lower precedence */
      if (state->current->root) {
         if (propagate_parserstate(state, prec)) goto ONERR_EXPECT;
         *pl->last   = (expr_t*) expr;
         expr->arg1  = *pl->last;
      } else {
         pl->root = (expr_t*) expr;
         pl->last = &pl->root;
         pl->expect = &expr->arg1;
         state->current = pl;
      }
   }

   return 0;
ONERR_EXPECT:
   print_error(parser, "Expected integer instead of operator\n");
   return EINVAL;
}

int match2ary_parser(parser_t * parser, unsigned prec, expr_2ary_t * expr)
{
   int err;
   parser_state_t * state = parser->state;

   precedence_level_t * pl = &state->preclevel[prec];

   if (state->current->expect) {
      print_error(parser, "Integer expected instead of operator\n");
      return EINVAL;
   }

   if (pl == state->current) {
      if (ASSOC_RIGHT == s_associativity_preclevel[prec]) {
         expr->arg1 = *pl->last;
         *pl->last  = (expr_t*) expr;
      } else {
         /*left associative*/
         expr->arg1 = pl->root;
         pl->root   = (expr_t*) expr;
         pl->last   = &pl->root;
      }
      pl->expect = &expr->arg2;

   } else if (pl < state->current) {
      /* higher precedence */
      pl->root   = (expr_t*) expr;
      pl->last   = &pl->root;
      pl->expect = &expr->arg2;
      expr->arg1 = *state->current->last;
      *state->current->last  = 0;
      state->current->expect = state->current->last;
      state->current->last   = 0;
      state->current = pl;

   } else {
      /* lower precedence */
      err = propagate_parserstate(state, prec);
      if (err) {
         print_error(parser, "Expected ':' to instead of '%s'\n", s_expr_type_names[expr->type]);
         return err;
      }
      if (ASSOC_RIGHT == s_associativity_preclevel[prec]) {
         expr->arg1 = *pl->last;
         *pl->last  = (expr_t*) expr;
      } else {
         /*left associative*/
         expr->arg1 = pl->root;
         pl->root   = (expr_t*) expr;
         pl->last   = &pl->root;
      }
      pl->expect = &expr->arg2;
   }

   return 0;
}

int matchstart3ary_parser(parser_t * parser, expr_3ary_t * expr)
{
   int err;
   parser_state_t * state = parser->state;

   precedence_level_t * pl = &state->preclevel[PREC_TERNARY];

   /* assume right associativity for single 3ary operator */

   if (state->current->expect) {
      print_error(parser, "Integer expected instead of operator\n");
      return EINVAL;
   }

   if (pl == state->current) {
      expr->arg1 = *pl->last;
      *pl->last  = (expr_t*) expr;
      pl->expect = &expr->arg2;

   } else if (pl < state->current) {
      /* higher precedence */
      pl->root   = (expr_t*) expr;
      pl->last   = &pl->root;
      pl->expect = &expr->arg2;
      expr->arg1 = *state->current->last;
      *state->current->last  = 0;
      state->current->expect = state->current->last;
      state->current->last   = 0;
      state->current = pl;

   } else {
      /* lower precedence */
      err = propagate_parserstate(state, PREC_TERNARY);
      assert(!err);
      expr->arg1 = *pl->last;
      *pl->last  = (expr_t*) expr;
      pl->expect = &expr->arg2;
   }

   expr->prev_expectmore = state->list_expectmore;
   state->list_expectmore = expr;

   return 0;
}

int matchnext3ary_parser(parser_t * parser)
{
   int err;
   parser_state_t * state = parser->state;

   precedence_level_t * pl = &state->preclevel[PREC_TERNARY];

   /* assume right associativity for single 3ary operator */

   if (! state->list_expectmore) {
      print_error(parser, "Unmatched ':'\n");
      return EINVAL;
   }

   if (state->current->expect) {
      print_error(parser, "Integer expected instead of ':'\n");
      return EINVAL;
   }

   if (pl == state->current) {

   } else if (pl < state->current) {
      /* higher precedence */
      assert(0 && "internal parser error");

   } else {
      /* lower precedence */
      err = propagate_parserstate(state, PREC_TERNARY);
      assert(!err);
   }

   state->current->expect = &state->list_expectmore->arg3;
   state->list_expectmore = state->list_expectmore->prev_expectmore;

   return 0;
}

int matchinteger_parser(parser_t * parser, int value)
{
   int err;
   expr_integer_t * expr;

   if (! parser->state->current->expect) {
      print_error(parser, "Operator expected instead of integer\n");
      return EINVAL;
   }

   err = new_exprinteger(parser, &expr, value);
   if (err) return err;

   parser->state->current->last = parser->state->current->expect;
   *parser->state->current->expect = (expr_t*) expr;
   parser->state->current->expect = 0;

   return 0;
}

int newstate_parser(/*out*/parser_t * parser)
{
   parser_state_t * startstate;

   if (parser->freestate) {
      startstate = parser->freestate;
      parser->freestate = parser->freestate->prev;

   } else {
      startstate = (parser_state_t*) alloc_mman(&parser->mm, sizeof(parser_state_t));
      if (!startstate) {
         return ENOMEM;
      }
   }

   init_parserstate(startstate, /*prev*/parser->state);
   parser->state = startstate;

   return 0;
}

int prevstate_parser(parser_t * parser, unsigned char expect_type, int c)
{
   int err;
   parser_state_t * prev = parser->state->prev;
   unsigned prec;

   if (! prev || !prev->current->expect || (*prev->current->last)->type != expect_type) {
      print_error(parser, "Unmatched '%c'\n", c);
      return EINVAL;
   }

   if ((*prev->current->last)->type != expect_type) {
      print_error(parser, "Character '%s' does not match '%c'\n", s_expr_type_names[(*prev->current->last)->type], c);
      return EINVAL;
   }

   if (parser->state->current->expect) {
      print_error(parser, "Expected integer instead of '%c'\n", c);
      return EINVAL;
   }

   err = propagatemax_parserstate(parser->state, &prec);
   if (err) {
      print_error(parser, "Expected integer instead of '%c'\n", c);
      return err;
   }

   // register sub expression with surrounding expression
   *prev->current->expect = parser->state->preclevel[prec].root;
   prev->current->last    = prev->current->expect;
   prev->current->expect  = 0;

   // add parser->state to list of free states
   parser->state->prev = parser->freestate;
   parser->freestate = parser->state;

   // switch to prev state
   parser->state = prev;

   return 0;
}

/* =================
 *  Parse Algorithm
 * ================= */

static /*err*/int parse_integer(parser_t * parser, int c)
{
   int err;
   int value = c - '0';

   for (;;) {
      c = peekchar(&parser->buffer);
      if ('0' <= c && c <= '9') {
         c = nextchar(&parser->buffer);
         if (value >= INT_MAX/10) {
            print_error(parser, "integer value too large\n");
            return EOVERFLOW;
         }
         value *= 10;
         if (value > INT_MAX - (c - '0')) {
            print_error(parser, "integer value too large\n");
            return EOVERFLOW;
         }
         value += c - '0';
      } else {
         break;
      }
   }

   err = matchinteger_parser(parser, value);

   return err;
}

static /*err*/int parse_1ary(parser_t * parser, unsigned char precedence, unsigned char type, unsigned char postfix_type)
{
   int err;
   expr_1ary_t * expr;

   err = new_expr1ary(parser, &expr, type);
   if (err) return err;

   err = match1ary_parser(parser, precedence, expr, postfix_type);

   return err;
}

static /*err*/int parse_2ary(parser_t * parser, unsigned char precedence, unsigned char type, unsigned char assign_type)
{
   int err;
   expr_2ary_t * expr;

   err = new_expr2ary(parser, &expr, type, assign_type);
   if (err) return err;

   err = match2ary_parser(parser, precedence, expr);

   return err;
}

static /*err*/int parse_3ary(parser_t * parser)
{
   int err;
   expr_3ary_t * expr;

   err = new_expr3ary(parser, &expr, EXPR_TERNARY);
   if (err) return err;

   err = matchstart3ary_parser(parser, expr);

   return err;
}

static /*err*/int parse_expression(parser_t * parser)
{
   int err;

   // start of expression: a value or unary (prefix) operator is expected
   // parser->state->expect != 0

   for (;;) {

      int c = nextchar(&parser->buffer);

      switch (c) {
      case 0:
         /* reached end of input */
         if (parser->state->prev) {
            err = ENODATA;
         } else {
            unsigned prec;
            err = propagatemax_parserstate(parser->state, &prec);
         }
         if (err) {
            print_error(parser, "Unexpected end of input\n");
         }
         goto ONERR;
      case '?':
         err = parse_3ary(parser);
         if (err) goto ONERR;
         break;
      case ':':
         err = matchnext3ary_parser(parser);
         if (err) goto ONERR;
         break;
      case '(':
         err = parse_1ary(parser, PREC_1ARY_BRACKET, EXPR_1ARY_BRACKET, EXPR_VOID);
         if (err) goto ONERR;
         err = newstate_parser(parser);
         if (err) goto ONERR;
         break;
      case ')':
         err = prevstate_parser(parser, EXPR_1ARY_BRACKET, ')');
         if (err) goto ONERR;
         break;
      case '[':
         err = parse_2ary(parser, PREC_2ARY_ARRAYINDEX, EXPR_2ARY_ARRAYINDEX, EXPR_VOID);
         if (err) goto ONERR;
         err = newstate_parser(parser);
         if (err) goto ONERR;
         break;
      case ']':
         err = prevstate_parser(parser, EXPR_2ARY_ARRAYINDEX, ']');
         if (err) goto ONERR;
         break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
         err = parse_integer(parser, c);
         if (err) goto ONERR;
         break;
      case '~':
         err = parse_1ary(parser, PREC_1ARY_BITWISE_NOT, EXPR_1ARY_BITWISE_NOT, EXPR_VOID);
         if (err) goto ONERR;
         break;
      case '!':
         err = parse_1ary(parser, PREC_1ARY_LOGICAL_NOT, EXPR_1ARY_LOGICAL_NOT, EXPR_VOID);
         if (err) goto ONERR;
         break;
      case '+':
         c = peekchar(&parser->buffer);
         if ('+' == c) {
            nextchar(&parser->buffer);
            err = parse_1ary(parser, PREC_1ARY_PREINCR, EXPR_1ARY_PREINCR, EXPR_1ARY_POSTINCR);
         } else if ('=' == c) {
            nextchar(&parser->buffer);
            err = parse_2ary(parser, PREC_2ARY_ASSIGN, EXPR_2ARY_ASSIGN, EXPR_2ARY_PLUS);
         } else if (parser->state->current->expect) {
            err = parse_1ary(parser, PREC_1ARY_PLUS, EXPR_1ARY_PLUS, EXPR_VOID);
         } else {
            err = parse_2ary(parser, PREC_2ARY_PLUS, EXPR_2ARY_PLUS, EXPR_VOID);
         }
         if (err) goto ONERR;
         break;
      case '=':
         c = peekchar(&parser->buffer);
         // TODO: ...
         err = parse_2ary(parser, PREC_2ARY_ASSIGN, EXPR_2ARY_ASSIGN, EXPR_VOID);
         if (err) goto ONERR;
         break;
      case '-':
         if ('-' == peekchar(&parser->buffer)) {
            nextchar(&parser->buffer);
            err = parse_1ary(parser, PREC_1ARY_PREINCR, EXPR_1ARY_PREDECR, EXPR_1ARY_POSTDECR);
         } else if ('=' == c) {
            nextchar(&parser->buffer);
            err = parse_2ary(parser, PREC_2ARY_ASSIGN, EXPR_2ARY_ASSIGN, EXPR_2ARY_MINUS);
         } else if (parser->state->current->expect) {
            err = parse_1ary(parser, PREC_1ARY_PLUS, EXPR_1ARY_MINUS, EXPR_VOID);
         } else {
            err = parse_2ary(parser, PREC_2ARY_PLUS, EXPR_2ARY_MINUS, EXPR_VOID);
         }
         if (err) goto ONERR;
         break;
      case ',':
         err = parse_2ary(parser, PREC_2ARY_COMMA, EXPR_2ARY_COMMA, EXPR_VOID);
         if (err) goto ONERR;
         break;
      default:
         print_error(parser, "Unexpected input '%c'; expected number\n", c);
         break;
      }

   }

ONERR:
   print_debug(parser, "parse_expression error %d\n", err);
   return err;
}


int main(int argc, const char * argv[])
{
   int err;
   parser_t parser;

   if (argc != 2) {
      fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
      return EXIT_FAILURE;
   }

   if (init_parser(&parser, argv[1])) {
      return EXIT_FAILURE;
   }

   err = parse_expression(&parser);

   // TODO:
   if (!err) print_expr(parser.state->current->root);

   free_parser(&parser);

   // TODO: malloc_stats(); /* TODO: remove ! */

   return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
