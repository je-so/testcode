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
 * The first row has the highest precedence (15)
 * and the last row the lowest (1).
 *
 * ---------------------------------------------------------------
 * Supported C Operators              | Associativity | Precedence
 * ---------------------------------------------------------------
 * () [] -> .                         | left to right | 15
 * ! ~ ++ -- + - * & sizeof           | right to left | 14
 * * / %                              | left to right | 13
 * + -                                | left to right | 12
 * << >>                              | left to right | 11
 * < <= > >=                          | left to right | 10
 * == !=                              | left to right |  9
 * &                                  | left to right |  8
 * ^                                  | left to right |  7
 * |                                  | left to right |  6
 * &&                                 | left to right |  5
 * ||                                 | left to right |  4
 * ?:                                 | right to left |  3
 * = += -= *= /= %= <<= >>= &= ^= |=  | right to left |  2
 * ,                                  | left to right |  1
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
 *    value_t* argument;
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
 * > gcc -ocop context_oriented_parser.c
 *
 * Run with:
 * > ./cop <filename>
 *
 * */

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

/* ==========
 *  parser_t
 * ========== */

typedef struct parser parser_t;

struct parser {
   buffer_t buffer;
   mman_t   mm;
   const char * filename;
};

int init_parser(/*out*/parser_t* parser, const char* filename)
{
   int err;

   err = read_buffer(&parser->buffer, filename);
   if (err) return err;

   init_mman(&parser->mm);

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

/* =======
 *  Value
 * ======= */

enum node_type {
   TYPE_VALUE_CONSTANT,
   TYPE_VALUE_PREFIXOP
};

typedef struct value_constant value_constant_t;
typedef struct value_prefixop value_prefixop_t;
typedef struct value value_t;

#define value_COMMON_FIELDS char type

struct value {
   value_COMMON_FIELDS;
};

struct value_constant {
   value_COMMON_FIELDS;
   int value;
};

int new_valueconstant(parser_t* parser, /*out*/value_constant_t** cval, int value)
{
   *cval = (value_constant_t*) alloc_mman(&parser->mm, sizeof(value_constant_t));

   if (!*cval) {
      return ENOMEM;
   }

   (*cval)->type = TYPE_VALUE_CONSTANT;
   (*cval)->value = value;

   print_debug(parser, "Matched integer %d\n", value);

   return 0;
}

struct value_prefixop {
   value_COMMON_FIELDS;
   char prefix_op[2];
   value_t* argument;
};


static /*err*/int parse_integer(parser_t * parser, /*out*/value_constant_t** cval, int c)
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

   err = new_valueconstant(parser, cval, value);

   return err;
}

static /*err*/int parse_value(parser_t * parser, /*out*/value_t** value)
{
   int err;
   int c = nextchar(&parser->buffer);
   value_constant_t * cval = 0;

   switch (c) {
   case 0:
      /* reached end of input */
      *value = 0;
      return 0;
   case '0': case '1': case '2': case '3': case '4':
   case '5': case '6': case '7': case '8': case '9':
      err = parse_integer(parser, &cval, c);
      if (err) goto ONERR;
      break;
   case '+':
      break;
   case '-':
      break;
   default:
      print_error(parser, "unexpected input '%c'\n", c);
      break;
   }

   return ENOSYS;
ONERR:
   return err;
}



static /*err*/int parse_expression(parser_t * parser)
{
   int err;
   value_t* value;

   err = parse_value(parser, &value);
   if (err) return err;

   return 0;
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

   free_parser(&parser);

   malloc_stats(); /* TODO: remove ! */

   return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
