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
 * The first row has the highest precedence (rank 0)
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
 * ?:                                 | right to left | 13
 * = += -= *= /= %= <<= >>= &= ^= |=  | right to left | 14
 * ,                                  | left to right | 15
 *
 * Let us begin with a simpe value and any possible prefix operators.
 *
 * Every simple expression starts therefore with
 * 1. integer value  [0-9]+
 * 2. subexpression  '(' value ')'
 * 3. computed value  '+' value, '-' value, '!' value, '++' value, ...
 *
 * The parsed value expression should be stored in a structured form
 * which supports the above 3 value types.
 *
 * struct value_constant_t {
 *    int value;
 * };
 *
 * struct value_prefixop_t {
 *    char prefix_op[2];
 *    value_t* arg1;
 * };
 *
 * struct value_t {
 *    const char type;
 *    union {
 *       value_prefixop_t preop;
 *       value_constant_t val;
 *    };
 * };
 *
 *
 * TODO: Describe context (structs used to represent already read text)
 *
 * TODO: implement parse_expression
 *
 * Compile with:
 * > ggcc -Wall -Wextra -Wconversion -std=gnu99 -ocop context_oriented_parser.c
 *
 * Run with:
 * > ./cop <filename>
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
/* align to 8 bytes */
#define memblock_ALIGN(size) (((size)+7u) & ~(size_t)7)

struct memblock {
   memblock_t* next;
   char        data[1];
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

/* ================
 *  parser_state_t
 * ================ */

typedef struct expr_t expr_t;

typedef struct parser_state_t parser_state_t;
typedef struct precedence_t precedence_t;

#define NROF_PRECEDENCE_LEVEL 16

struct precedence_t {
   expr_t ** expect; /* expect another expression as argument */
   expr_t  * root;   /* root of expression of this precedence level */
   expr_t  * last;   /* last parsed expression of this precedence level */
};

struct parser_state_t {
   parser_state_t * prev;
   expr_t  * root;
   expr_t ** expect;
   precedence_t precedence[NROF_PRECEDENCE_LEVEL];
};

void init_parserstate(/*out*/parser_state_t * state, parser_state_t * prev)
{
   state->prev = prev;
   state->root = 0;
   state->expect = &state->root;
   memset(&state->precedence, 0, sizeof(state->precedence));
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

void print_debug(parser_t* parser, const char * format, ...)
{
   va_list args;
   va_start(args, format);
   fprintf(stdout, "%s:%zd,%zd: ", parser->filename, parser->buffer.line, parser->buffer.col);
   vfprintf(stdout, format, args);
   va_end(args);
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

void prevstate_parserstate(parser_t * parser)
{
   parser_state_t * prev = parser->state->prev;

   // check that state is not root state
   assert(prev);

   if (!parser->state->root) {
      print_error(parser, "expected non empty sub expression\n");
      assert(parser->state->root);
   }

   if (!prev->expect) {
      print_error(parser, "unused sub expression\n");
      assert(prev->expect);
   }

   // register sub expression with surrounding expression
   *prev->expect = parser->state->root;
   prev->expect = 0;

   // add parser->state to list of free states
   parser->state->prev = parser->freestate;
   parser->freestate = parser->state;

   // switch to prev state
   parser->state = prev;

}

/* ===================
 *  struct data model
 * =================== */

#define EXPR_COMMON_FIELDS  unsigned char type

typedef struct expr_t expr_t;
typedef struct expr_integer_t expr_integer_t;
typedef struct expr_1ary_t expr_1ary_t;
typedef struct expr_2ary_t expr_2ary_t;

typedef enum expr_type {
   EXPR_VOID,
   EXPR_INTEGER,
   EXPR_1ARY_BRACKET,      /* () */
   EXPR_1ARY_MINUS,
   EXPR_1ARY_PLUS,
   EXPR_1ARY_LOGICAL_NOT,  /* ! */
   EXPR_1ARY_BITWISE_NOT,  /* ~ */
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
} expr_type_e;

typedef enum expr_precedence {
   EXPR_PREC_VOID    = 0,
   EXPR_PREC_INTEGER = 0,
   EXPR_PREC_1ARY_BRACKET = 1,
   EXPR_PREC_1ARY_MINUS = 2,
   EXPR_PREC_1ARY_PLUS  = 2,
   EXPR_PREC_1ARY_LOGICAL_NOT = 2,
   EXPR_PREC_1ARY_BITWISE_NOT = 2,
   EXPR_PREC_2ARY_MINUS = 4,
   EXPR_PREC_2ARY_PLUS  = 4,
   EXPR_PREC_2ARY_MULT  = 3,
   EXPR_PREC_2ARY_DIV   = 3,
   EXPR_PREC_2ARY_LOGICAL_AND = 11,
   EXPR_PREC_2ARY_LOGICAL_OR  = 12,
   EXPR_PREC_2ARY_BITWISE_AND = 8,
   EXPR_PREC_2ARY_BITWISE_OR  = 10,
   EXPR_PREC_2ARY_BITWISE_XOR = 9,
   EXPR_PREC_2ARY_ASSIGN     = 14,
   EXPR_PREC_2ARY_ARRAYINDEX = 1,
} expr_precedence;

static char s_expr_type_names[][4] = {
   [EXPR_VOID]    = "",
   [EXPR_INTEGER] = "",
   [EXPR_1ARY_BRACKET] = "(",
   [EXPR_1ARY_MINUS] = "-",
   [EXPR_1ARY_PLUS]  = "+",
   [EXPR_1ARY_LOGICAL_NOT] = "!",
   [EXPR_1ARY_BITWISE_NOT] = "~",
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
   [EXPR_2ARY_ARRAYINDEX]  = "["
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
      case EXPR_1ARY_BITWISE_NOT: {
         expr_1ary_t * e = (expr_1ary_t*) expr;
         printf("{");
         printf("%s ", s_expr_type_names[e->type]);
         print_expr2(e->arg1);
         printf("}");
         break;
      }
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
   }
}

static inline void print_expr(expr_t * expr)
{
   if (expr) {
      print_expr2(expr);
      printf("\n");
   }
}

/* =================
 *  Parse Algorithm
 * ================= */

static /*err*/int parse_integer(parser_t * parser, int c)
{
   int err;
   int value = c - '0';
   expr_integer_t * expr;

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

   err = new_exprinteger(parser, &expr, value);

   *parser->state->expect = (expr_t*) expr;
   parser->state->expect = 0;

   return err;
}

static /*err*/int parse_1ary(parser_t * parser, unsigned char type)
{
   int err;
   expr_1ary_t * expr;

   err = new_expr1ary(parser, &expr, type);
   if (err) return err;

   *parser->state->expect = (expr_t*) expr;
   parser->state->expect = &expr->arg1;

   return 0;
}

static /*err*/int parse_prefix_or_value(parser_t * parser)
{
   int err;
   int c = nextchar(&parser->buffer);

   // TODO: dokument why ?
   assert(parser->state->expect);

   /* support prefix operators */
   /* ! ~ + - (associativity from right to left) */

   switch (c) {
   case 0:
      /* reached end of input */
      print_error(parser, "Unexpected end of input; expected number\n");
      return ENODATA;
   case '(':
      err = newstate_parser(parser);
      if (err) return err;
      err = parse_1ary(parser, EXPR_1ARY_BRACKET);
      if (err) goto ONERR;
      break;
   case '0': case '1': case '2': case '3': case '4':
   case '5': case '6': case '7': case '8': case '9':
      err = parse_integer(parser, c);
      if (err) goto ONERR;
      break;
   case '~':
      err = parse_1ary(parser, EXPR_1ARY_BITWISE_NOT);
      if (err) goto ONERR;
      break;
   case '!':
      err = parse_1ary(parser, EXPR_1ARY_LOGICAL_NOT);
      if (err) goto ONERR;
      break;
   case '+':
      err = parse_1ary(parser, EXPR_1ARY_PLUS);
      if (err) goto ONERR;
      break;
   case '-':
      err = parse_1ary(parser, EXPR_1ARY_MINUS);
      if (err) goto ONERR;
      break;
   default:
      print_error(parser, "Unexpected input '%c'; expected number\n", c);
      break;
   }

   return 0;
ONERR:
   return err;
}

static /*err*/int parse_expression(parser_t * parser)
{
   int err;
   int c;

   // start of expression: a value or unary (prefix) operator is expected
   // parser->state->expect != 0

   for (;;) {

      while (parser->state->expect) {
         err = parse_prefix_or_value(parser);
         if (err) goto ONERR;
      }

      c = nextchar(&parser->buffer);
      if (')' == c) {
         if (! parser->state->prev) {
            print_error(parser, "Unmatched ')'\n");
            err = EINVAL;
            goto ONERR;
         }
         prevstate_parserstate(parser);

      } else if (0 == c) {
         /* end of input */
         if (parser->state->prev) {
            print_error(parser, "Unexpected end of input; unmatched '('\n");
            err = EINVAL;
            goto ONERR;
         }
         break;

      } else {
         print_error(parser, "Unexpected input '%c'\n", c);
         err = EINVAL;
         goto ONERR;
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

   print_expr(parser.startstate.root);

   free_parser(&parser);

   malloc_stats(); /* TODO: remove ! */

   return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
