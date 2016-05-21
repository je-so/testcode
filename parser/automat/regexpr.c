/* title: RegularExpression impl

   Implements <RegularExpression>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/regexpr.h
    Header file <RegularExpression>.

   file: C-kern/proglang/regexpr.c
    Implementation file <RegularExpression impl>.
*/

#include "config.h"
#include "regexpr.h"
#include "memstream.h"
#include "utf8.h"
#include "test_errortimer.h"

// === private types
struct buffer_t;

// forward

static int parse_regexpr(struct buffer_t* buffer);

#ifdef KONFIG_UNITTEST
static test_errortimer_t s_regex_errtimer;
#endif


// struct: regexpr_err_t

// group: log

void log_regexprerr(const regexpr_err_t *err, uint8_t channel)
{
   if (err->type <= 1) {
      // TODO: PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_EXPECT_INSTEADOF_ERRLOG, err->expect, err->type ? 0 : err->unexpected);
   } else if (err->type == 2) {
      // TODO: PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_UNEXPECTED_CHAR_ERRLOG, err->unexpected);
   } else if (err->type == 3) {
      // TODO: PRINTTEXT_LOG(,channel, log_flags_NONE, 0, PARSEERROR_ILLEGALCHARACTERENCODING_ERRLOG, err->unexpected);
   }
}

/* struct: buffer_t
 * Siehe auch <memstream_ro_t> und <memstream_t>.
 *
 * Invariant:
 * Anzahl lesbare Bytes == (size_t) (end-next). */
typedef struct buffer_t {
   automat_t         mman;
   memstream_ro_t    input;
   /*== out ==*/
   automat_t         result;
   regexpr_err_t     err;
} buffer_t;

// group: lifetime

int init_buffer(/*out*/buffer_t* buffer, size_t len, const char str[len]) \
{
   int err;

   err = initempty_automat(&buffer->mman, 0);
   if (err) goto ONERR;
   buffer->input = (memstream_ro_t) memstream_INIT((const uint8_t*)str, (const uint8_t*)str + len);

   return 0;
ONERR:
   return err;
}

int free_buffer(buffer_t* buffer)
{
   int err;

   err = free_automat(&buffer->mman);
   if (err) goto ONERR;
   buffer->input = (memstream_ro_t) memstream_FREE;

   return 0;
ONERR:
   return err;
}

// group: parsing

/* function: read_next
 * Liest das nächste Byte, das nicht ein Leerzeichen ist und bewegt den Lesezeiger.
 * Leerzeichen werden überlesen. Falls ein Leerzeichen zurückgegeben
 * wird, wurde das Eingabeende erreicht. */
static inline uint8_t read_next(buffer_t* buffer)
{
   uint8_t next = ' ';
   while (isnext_memstream(&buffer->input)) {
      next = nextbyte_memstream(&buffer->input);
      if (next != ' ') break;
   }
   return next;
}

/* function: skip_next
 * Bewegt den Lesezeiger ein Byte weiter.
 *
 * Unchecked Precondition:
 * - Last call to <peek_next> returned value != ' '. */
static inline void skip_next(buffer_t* buffer)
{
   skip_memstream(&buffer->input, 1);
}

/* function: peek_next
 * Gibt das aktuelle Byte an der Leseposition zurück, ohne den Lesezeiger zu bewegen.
 * Falls jedoch ein Leerzeichen an der aktuellen Position steht, wird der Lesezeiger
 * solange weiterbewegt, bis die Position eines Nichtleerzeichens erreicht wird.
 * Falls ein Leerzeichen zurückgegeben wird, wurde das Eingabeende erreicht. */
static inline uint8_t peek_next(buffer_t* buffer)
{
   uint8_t next = ' ';
   while (isnext_memstream(&buffer->input)) {
      next = *buffer->input.next;
      if (next != ' ') break;
      skip_memstream(&buffer->input, 1);
   }
   return next;
}

static inline int parse_utf8(buffer_t* buffer, uint8_t next, char32_t* chr)
{
   unsigned nrbytes = sizePfirst_utf8(next);
   if (  nrbytes > size_memstream(&buffer->input)+1
         || nrbytes != decodechar_utf8(&next_memstream(&buffer->input)[-1], chr)) {
      buffer->err.type = 3;
      buffer->err.chr  = next;
      buffer->err.pos  = (const char*) buffer->input.next - 1;
      buffer->err.expect = 0;
      nrbytes = nrbytes > size_memstream(&buffer->input)+1 ? size_memstream(&buffer->input)+1 : nrbytes;
      buffer->err.unexpected[0] = (char) next;
      for (size_t i = 1; i < nrbytes; ++i) {
         buffer->err.unexpected[i] = (char) buffer->input.next[i-1];
      }
      buffer->err.unexpected[nrbytes] = 0;
      return EILSEQ;
   }
   skip_memstream(&buffer->input, nrbytes-1u);

   return 0;
}

/* function: parse_char
 * Falls next der Anfang eines utf-8 kodierten Zeichens ist, wird dieses dekodiert in chr
 * zurückgegeben. Dabei soviele Bytes aus buffer konsumiert wie nötig.
 * Besitzt next keine weiteren Bytes, wird dessen Wert direkt in chr übergeben,
 * ohne weitere Bytes zu konsumieren. */
static inline int parse_char(buffer_t* buffer, uint8_t next, /*out*/char32_t* chr)
{
   if (issinglebyte_utf8(next)) {
      if (next == '\\' && isnext_memstream(&buffer->input)) {
         next = nextbyte_memstream(&buffer->input);
         if (issinglebyte_utf8(next)) {
            if (next == 'n') next = '\n';
            else if (next == 'r') next = '\r';
            else if (next == 't') next = '\t';
            *chr = next;
            return 0;
         }
      } else {
         *chr = next;
         return 0;
      }
   }

   return parse_utf8(buffer, next, chr);
}

static inline int ERR_EXPECT_OR_UNMATCHED(buffer_t* buffer, const char* expect, const uint8_t next, bool isEndOfFile)
{
   buffer->err.type = expect ? isEndOfFile ? 1 : 0 : 2;
   buffer->err.chr  = next;
   buffer->err.pos  = (const char*) buffer->input.next - !isEndOfFile;
   buffer->err.expect = expect;

   if (! isEndOfFile && ! issinglebyte_utf8(next)) {
      int err = parse_utf8(buffer, next, &buffer->err.chr);
      if (err) return err;
   }

   static_assert( maxsize_utf8() < sizeof(buffer->err.unexpected), "check array overflow");
   if (isEndOfFile) {
      buffer->err.unexpected[0] = (char) next;
      buffer->err.unexpected[1] = 0;
   } else {
      uint8_t len = encodechar_utf8(buffer->err.chr, sizeof(buffer->err.unexpected), (uint8_t*)buffer->err.unexpected);
      buffer->err.unexpected[len] = 0;
   }

   return ESYNTAX;
}

static int operator_not(buffer_t* buffer)
{
   int err;
   automat_t notchar;

   err = initmatch_automat(&notchar, &buffer->mman, 1, (char32_t[]){ 0 }, (char32_t[]){ 0x7fffffff });
   if (err) return err;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = opandnot_automat(&notchar, &buffer->result);
   }
   if (err) {
      free_automat(&notchar);
   } else {
      initmove_automat(&buffer->result, &notchar);
   }

   return err;
}

static int operator_optional(buffer_t* buffer)
{
   int err;
   automat_t empty;

   err = initempty_automat(&empty, &buffer->mman);
   if (err) return err;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = opor_automat(&buffer->result, &empty);
   }
   if (err) free_automat(&empty);

   return err;
}


// section: regexpr_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_regex_errtimer
 * Simuliert Fehler in Funktionen für <buffer_t> und <regexpr_t>. */
static test_errortimer_t   s_regex_errtimer = test_errortimer_FREE;
#endif

// group: parsing

/* function: parse_atom
 * Erwartet wird mindestens ein Zeichen.
 * '.', '[' und '(' und '\\' werden gesondert behandelt. */
static int parse_atom(buffer_t* buffer)
{
   int err;
   uint8_t next = read_next(buffer);

   // recognize '.' | '[' | ']' | '(' | ')'

   if (next == ' ') {
      err = initempty_automat(&buffer->result, &buffer->mman);
      if (err) goto ONERR;
   } else if (next == '(') {
      err = parse_regexpr(buffer);
      if (err) goto ONERR;
      next = read_next(buffer);
      if (next != ')') {
         err = ERR_EXPECT_OR_UNMATCHED(buffer, ")", next, next == ' ');
         goto ONERR;
      }
   } else if (next == '[') {
      bool isFirst = true;
      bool isNot = (peek_next(buffer) == '^');
      if (isNot) skip_next(buffer);
      for (;;) {
         char32_t from, to;
         next = read_next(buffer);
         if (next == ' ') {
            err = ERR_EXPECT_OR_UNMATCHED(buffer, "]", next, true/*end of file*/);
            goto ONERR;
         }
         if (next == ']') break;
         err = parse_char(buffer, next, &from);
         if (err) goto ONERR;
         if (peek_next(buffer) == '-') {
            /* range */
            skip_next(buffer);
            err = parse_char(buffer, read_next(buffer), &to);
            if (err) goto ONERR;
            if (to == ']') {
               err = ERR_EXPECT_OR_UNMATCHED(buffer, "<char>", ']', false/*end of file*/);
               goto ONERR;
            }
         } else {
            /* single char */
            to = from;
         }
         if (isFirst) {
            isFirst = false;
            err = initmatch_automat(&buffer->result, &buffer->mman, 1, (char32_t[]){ from }, (char32_t[]){ to });
         } else {
            err = extendmatch_automat(&buffer->result, 1, (char32_t[]){ from }, (char32_t[]){ to });
         }
         if (err) goto ONERR;
      }
      if (isFirst) {
         err = initempty_automat(&buffer->result, &buffer->mman);
         if (err) goto ONERR;
      }
      if (isNot) {
         err = operator_not(buffer);
         if (err) goto ONERR;
      }
   } else {
      char32_t from;
      char32_t to;
      if (next == '.') {
         from = 0;
         to   = 0x7fffffff;
      } else {
         err = parse_char(buffer, next, &from);
         if (err) goto ONERR;
         to = from;
      }
      err = initmatch_automat(&buffer->result, &buffer->mman, 1, (char32_t[]){ from }, (char32_t[]){ to });
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

/* function: parse_sequence
 * Analysiert Syntax einer regulären Sequenz und baut einen enstprechenden Automaten vom Typ <automat_t>. */
static int parse_sequence(buffer_t* buffer)
{
   int err;
   int isResult = 0;
   uint8_t next = peek_next(buffer);
   automat_t seqresult;

   do {
      int isNot = 0;
      while (next == '!') {
         skip_next(buffer);
         isNot = !isNot;
         next = peek_next(buffer);
      }

      if (next == '*' || next == '+' || next == '|' || next == '&' || next == ')' || next == ']' || next == '?') {
         skip_next(buffer);
         err = ERR_EXPECT_OR_UNMATCHED(buffer, "<char>", next, false);
         goto ONERR;
      }

      err = parse_atom(buffer);
      if (err) goto ONERR;

      next = peek_next(buffer);
      if (next == '*' || next == '+') {
         skip_next(buffer);
         err = oprepeat_automat(&buffer->result, next == '+');
         if (err) goto ONERR;
         next = peek_next(buffer);
      } else if (next == '?') {
         skip_next(buffer);
         err = operator_optional(buffer);
         if (err) goto ONERR;
         next = peek_next(buffer);
      }

      if (isNot) {
         err = opnot_automat(&buffer->result);
         if (err) goto ONERR;
      }

      if (isResult) {
         err = opsequence_automat(&seqresult, &buffer->result);
         if (err) goto ONERR;
      } else {
         isResult = 1;
         initmove_automat(&seqresult, &buffer->result);
      }

   } while (next != ' ' && next != '|' && next != '&' && next != ')');

   initmove_automat(&buffer->result, &seqresult);

   return 0;
ONERR:
   if (isResult) {
      free_automat(&seqresult);
   }
   return err;
}

/* function: parse_regexpr
 * Analysiert Syntax und baut einen enstprechenden Automaten vom Typ <automat_t>. */
static int parse_regexpr(buffer_t* buffer)
{
   int err;
   uint8_t op = 0;
   uint8_t next = peek_next(buffer);
   automat_t regexresult;

   for (;;) {

      if (next == '|' || next == '&' || next == ')') {
         err = initempty_automat(&buffer->result, &buffer->mman);
         if (err) goto ONERR;
      } else {
         err = parse_sequence(buffer);
         if (err) goto ONERR;
         next = peek_next(buffer);
      }

      if (op) {
         if (op == '!') {
            err = opandnot_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         } else if (op == '&') {
            err = opand_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         } else {
            err = opor_automat(&regexresult, &buffer->result);
            if (err) goto ONERR;
         }
      } else {
         op = '|'; // mark regexresult as valid (free result in error case)
         initmove_automat(&regexresult, &buffer->result);
      }

      if (next == '|') {
         op = next;
         skip_next(buffer);
         next = peek_next(buffer);
      } else if (next == '&') {
         op = next;
         skip_next(buffer);
         if (isnext_memstream(&buffer->input) && *next_memstream(&buffer->input) == '!') {
            op = '!';
            skip_next(buffer);
         }
         next = peek_next(buffer);
      } else {
         break;
      }
   }

   initmove_automat(&buffer->result, &regexresult);

   return 0;
ONERR:
   if (op) {
      free_automat(&regexresult);
   }
   return err;
}

// group: lifetime

int free_regexpr(regexpr_t* regex)
{
   int err;

   err = free_automat(&regex->matcher);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_regexpr(/*out*/regexpr_t* regex, size_t len, const char definition[len], /*err*/regexpr_err_t *errdescr)
{
   int err;
   int isBuffer = 0;
   buffer_t buffer;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = init_buffer(&buffer, len, definition);
   }
   if (err) goto ONERR;

   buffer.result = (automat_t) automat_FREE;
   isBuffer = 1;

   if (!PROCESS_testerrortimer(&s_regex_errtimer, &err)) {
      err = parse_regexpr(&buffer);
   }
   if (err) goto ONERR;

   uint8_t next = read_next(&buffer);
   if (next != ' ') {
      err = ERR_EXPECT_OR_UNMATCHED(&buffer, 0, next, false);
      goto ONERR;
   }

   err = free_buffer(&buffer);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   err = minimize_automat(&buffer.result);
   PROCESS_testerrortimer(&s_regex_errtimer, &err);
   if (err) goto ONERR;

   // set out
   regex->matcher = buffer.result;

   return 0;
ONERR:
   if (errdescr && (err == ESYNTAX || err == EILSEQ)) {
      *errdescr = buffer.err;
   }

   if (isBuffer) {
      (void) free_automat(&buffer.result);
      (void) free_buffer(&buffer);
   }
   if (err != ESYNTAX && err != EILSEQ) {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}
