/* Simulate interpreted simple LL(1) Parser comparable to 
 * https://github.com/orangeduck/mpc/blob/master/examples/doge.c
 * 
 * Compile with
 * > gcc -odoge2 -std=gnu99 doge2.c
 *
 * run with 
 * > doge2 filename
 *
 * where filename contains a lot of "so c so c"
 *
 * Result:
 * This parser runs +/- 65 times faster than interpreted parser 
 * (mpc is configured to use LL(1) parsing without backtracking)
 *
 * mpc builds AST but this small parser does not !! 
 * so 20 times faster should be more realisitic.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
   char * addr;
   size_t len;
   size_t line;
} buffer_t;

enum parser_state {
  PS_SEQUENCE,
  PS_REPEAT,
  PS_OR,
  PS_MATCH
};

typedef struct {
   size_t count;
   size_t * si;
} ps_sequence_t;

typedef struct {
   size_t count;
   char * first;
   size_t * si;
} ps_or_t;

typedef struct {
   size_t si;
} ps_repeat_t;

typedef struct {
   size_t len;
   char * str;
} ps_match_t;

typedef struct {
   enum parser_state type;
   union {
      ps_sequence_t seq;
      ps_repeat_t rep;
      ps_or_t or;
      ps_match_t match;
   };
} parser_state_t;


typedef struct {
   size_t nrstate;
   parser_state_t * state;
} parser_t;

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

int parse_state(parser_t* parser, buffer_t* buffer, size_t si);

int parse_sequence(parser_t* parser, buffer_t* buffer, size_t si)
{
   int err;

   for (size_t i = 0; i < parser->state[si].seq.count; ++i) {
      err = parse_state(parser, buffer, parser->state[si].seq.si[i]);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   printf("error in parse_sequence\n");
   return err;
}

int parse_match(parser_t* parser, buffer_t* buffer, size_t si)
{
   for (unsigned i = 0; i < parser->state[si].match.len; ++i) {
      char c = nextchar(buffer);
      if (c != parser->state[si].match.str[i]) {
         if (buffer->len == 0) {
            printf("unexpected end of input;");
         }
         goto ONERR;
      } 
   }

   return 0;
ONERR:
   printf("expected: '%.*s'\n", parser->state[si].match.len, parser->state[si].match.str);
   return EINVAL;
}

int parse_or(parser_t* parser, buffer_t* buffer, size_t si)
{
   char c;

   c = nextchar(buffer);
   if (c) {
      -- buffer->addr;
      ++ buffer->len;
   }
  
   for (size_t i = 0; i < parser->state[si].or.count; ++i) {
      if (c == parser->state[si].or.first[i]) {
         return parse_state(parser, buffer, parser->state[si].or.si[i]);
      }
   } 

   printf("error in parse_or\n");
   return EINVAL;
}

int parse_repeat(parser_t* parser, buffer_t* buffer, size_t si)
{
   int err;

   while (nextchar(buffer)) {
      -- buffer->addr;
      ++ buffer->len;
      err = parse_state(parser, buffer, parser->state[si].rep.si);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   printf("error in parse_repeat\n");
   return err;
}

int parse_state(parser_t* parser, buffer_t* buffer, size_t si)
{
   switch (parser->state[si].type) {
   case PS_MATCH: return parse_match(parser, buffer, si);
   case PS_OR: return parse_or(parser, buffer, si);
   case PS_SEQUENCE: return parse_sequence(parser, buffer, si);
   case PS_REPEAT: return parse_repeat(parser, buffer, si);
   }

   printf("error in parse_state\n");
   return EINVAL;
}

int parse(parser_t* parser, size_t len, char addr[len])
{
   int err;
   buffer_t buffer = { addr, len, 0 };
 
   err = parse_state(parser, &buffer, 0);

   if (buffer.len == 0) {
      printf("OK\n");
   }

   return err;
}

int main(int argc, const char* argv[])
{
   if (argc != 2) {
      printf("Usage: %s <filename>\n", argv[0]);
      return EINVAL;
   }

   FILE* f = fopen(argv[1], "r");
   char * buffer = malloc(10000000);
   size_t len = fread(buffer, 1, 10000000, f);
   if (! feof(f)) {
      printf("read error\n");
   }
   fclose(f);
   f = 0;
 
   parser_state_t states[100];

   parser_t parser = { 100, states };

   states[0].type = PS_REPEAT;
   states[0].rep.si = 1;
   states[1].type = PS_SEQUENCE;
   states[1].seq.count = 2;
   states[1].seq.si = (size_t[2]) { 2, 3 };
   states[2].type = PS_OR;
   states[2].or.count = 3;
   states[2].or.first = (char[3]) { 'w', 'm', 's' };
   states[2].or.si = (size_t[3]) { 4, 5, 6 };
   states[3].type = PS_OR;
   states[3].or.count = 3;
   states[3].or.first = (char[3]) { 'c', 'l', 'b' };
   states[3].or.si = (size_t[3]) { 7, 8, 9 };
   states[4].type = PS_MATCH;
   states[4].match.len = 3;
   states[4].match.str = "wow";
   states[5].type = PS_MATCH;
   states[5].match.len = 4;
   states[5].match.str = "many";
   states[6].type = PS_MATCH;
   states[6].match.len = 2;
   states[6].match.str = "so";
   states[7].type = PS_MATCH;
   states[7].match.len = 1;
   states[7].match.str = "c";
   states[8].type = PS_MATCH;
   states[8].match.len = 8;
   states[8].match.str = "language";
   states[9].type = PS_MATCH;
   states[9].match.len = 4;
   states[9].match.str = "book";

   parse(&parser, len, buffer);

   free(buffer);
   return 0;
}
