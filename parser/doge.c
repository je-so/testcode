/* Simulate hard coded Parser comparable to 
 * https://github.com/orangeduck/mpc/blob/master/examples/doge.c
 * 
 * Compile with
 * > gcc -odoge -std=gnu99 doge.c
 *
 * run with 
 * > doge filename
 *
 * where filename contains a lot of "so c so c"
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
   char * addr;
   size_t len;
   size_t line;
} buffer_t;

static inline int nextchar(buffer_t * buffer)
{
   while (buffer->len) {
      char c = buffer->addr[0];
      ++ buffer->addr;
      -- buffer->len;
      if (c == '\n') {
         ++ buffer->line;
         continue;
      }
      if (c != ' ' && c != '\t') return c;
   }

   return 0;
}

int match(unsigned len, char * str, buffer_t* buffer)
{
   for (unsigned i = 0; i < len; ++i) {
      char c = nextchar(buffer);
      if (c != str[i]) {
         if (buffer->len == 0) {
            printf("unexpected end of input;");
         }
         goto ONERR;
      } 
   }

   return 0;
ONERR:
   printf("expected: '%.*s'\n", len, str);
   return EINVAL;
}

int parse_adjective(buffer_t* buffer)
{
   char c;

   c = nextchar(buffer);

   switch (c) {
   case 'w': return match(2, "ow", buffer);
   case 'm': return match(3, "any", buffer);
   case 's': return match(1, "o", buffer);
   }

   printf("Unexpected input (expected wow, many, so)\n");
   return EINVAL;
}

int parse_noun(buffer_t* buffer)
{
   char c;

   c = nextchar(buffer);

   switch (c) {
   case 'c': return 0;
   case 'l': return match(7, "anguage", buffer);
   case 'b': return match(3, "ook", buffer);
   }

   printf("Unexpected input (expected c, language, book)\n");
   return EINVAL;
}

int parse_phrase(buffer_t* buffer)
{
   int err;
   err = parse_adjective(buffer);
   if (err) goto ONERR;
   err = parse_noun(buffer);
   if (err) goto ONERR;
 
   return 0;
ONERR:
   return err;
}

int parse(size_t len, char addr[len])
{
   int err;
   buffer_t buffer = { addr, len, 0 };

   while (nextchar(&buffer)) {
      -- buffer.addr;
      ++ buffer.len;
      err = parse_phrase(&buffer);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

int main(int argc, const char* argv[])
{
   if (argc != 2) {
      printf("Usage: %s <filename>\n", argv[0]);
      return EINVAL;
   }

   FILE* f = fopen(argv[1], "r");
   char * buffer = malloc(1000000);
   size_t len = fread(buffer, 1, 1000000, f);
   if (! feof(f)) {
      printf("read error\n");
   }

   parse(len, buffer);

   return 0;
}
