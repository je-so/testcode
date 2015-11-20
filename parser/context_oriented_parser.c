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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================
 *  Memory Management
 * =================== */

typedef struct memblock memblock_t;
typedef struct mman mman_t;

#define memblock_ALIGN(size) (((size) + 7u) & ~(size_t)7)

#define memblock_DATASIZE memblock_ALIGN(65536 - sizeof(memblock_t*) - 32/*used by malloc?*/)

struct memblock {
   memblock_t* next;
   char        data[memblock_DATASIZE];
};

int new_memblock(/*out*/memblock_t** block)
{
   *block = malloc(sizeof(memblock_t));
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
         free_memblock(block);
      } while (block != mm->last);
   }

   memset(mm, 0, sizeof(*mm));
}

void* alloc_mman(mman_t* mm, size_t size)
{
   int err;
   size_t off;

   // align to 8 btes
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
   mm->free_bytes += aligned_size;

   return mm->last->data + off;
}

/* ==========
 *  buffer_t
 * ========== */

typedef struct buffer buffer_t;

struct buffer {
   char * addr;
   size_t len;
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
   buffer->line = 0;
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
   while (buffer->len) {
      char c = buffer->addr[0];
      ++ buffer->addr;
      -- buffer->len;
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
      -- buffer->addr;
      ++ buffer->len;
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
};

int init_parser(/*out*/parser_t* parser, const char* filename)
{
   int err;

   err = read_buffer(&parser->buffer, filename);
   if (err) return err;

   init_mman(&parser->mm);

   return 0;
}

void free_parser(parser_t* parser)
{
   free_buffer(&parser->buffer);
   free_mman(&parser->mm);
}

/* =======
 *  Value
 * ======= */

typedef struct value_constant value_constant_t;
typedef struct value_prefixop value_prefixop_t;
typedef struct value value_t;

struct value_constant {
    int value;
};

struct value_prefixop {
   char prefix_op[2];
   value_t* argument;
};

struct value {
   const char type;
   union {
      value_prefixop_t preop;
      value_constant_t val;
   };
};

static /*err*/int parse_value(parser_t * parser, /*out*/value_t** value)
{
   (void) parser;
   (void) value;
   return ENOSYS;
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

   return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
