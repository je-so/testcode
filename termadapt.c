/* title: TerminalAdapter impl

   Implements <TerminalAdapter>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/termadapt.h
    Header file <TerminalAdapter>.

   file: C-kern/io/terminal/termadapt.c
    Implementation file <TerminalAdapter impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/termadapt.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// SWITCH FOR RUNNING a USERTEST
// #define KONFIG_USERTEST_INPUT  // let user press key and tries to determine it
#define KONFIG_USERTEST_EDIT   // let user edit dummy text in memory


// section: termadapt_t

// group: static variables

/* varibale: s_termadapt_builtin
 * Defines two builtin terminal types which are supported right now. */
static termadapt_t s_termadapt_builtin[] = {
   {
      termid_LINUXCONSOLE,
      (const uint8_t*) "linux|linux console",
   },

   {
      termid_XTERM,
      (const uint8_t*) "xterm|xterm-debian|X11 terminal emulator",
   },

};

// group: lifetime

int new_termadapt(/*out*/termadapt_t ** termadapt, const termid_e termid)
{
   int err;

   VALIDATE_INPARAM_TEST(termid < lengthof(s_termadapt_builtin), ONERR, );

   *termadapt = &s_termadapt_builtin[termid];
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int newfromtype_termadapt(/*out*/termadapt_t ** termadapt, const uint8_t * type)
{
   size_t tlen = strlen((const char*)type);
   for (unsigned i = 0; i < lengthof(s_termadapt_builtin); ++i) {
      const uint8_t * typelist = s_termadapt_builtin[i].typelist;
      const uint8_t * pos = (const uint8_t*) strstr((const char*)typelist, (const char*)type);
      if (  pos && (pos == typelist || pos[-1] == '|') /*begin of type is located at start of string or right after '|'*/
            && (pos[tlen] == '|' || pos[tlen] == 0)) { /*end of type is located at end of string or left of '|'*/
         return new_termadapt(termadapt, i);
      }
   }

   return ENOENT;
}

// group: write helper

/* define: COPY_CODE_SEQUENCE
 * TODO: */
#define COPY_CODE_SEQUENCE(CODESEQ) \
         const unsigned size = sizeof(CODESEQ)-1;  \
         CHECK_SIZE(size);                         \
         write_memstream(ctrlcodes, size, CODESEQ);

/* define: WRITEDECIMAL
 * TODO: */
#define WRITEDECIMAL(NR) \
         if (NR > 99) writebyte_memstream(ctrlcodes, (uint8_t) ('0' + NR/100)); \
         if (NR > 9)  writebyte_memstream(ctrlcodes, (uint8_t) ('0' + (NR/10)%10)); \
         writebyte_memstream(ctrlcodes, (uint8_t) ('0' + NR%10))

/* define: CHECK_PARAM
 * TODO: */
#define CHECK_PARAM(COND) \
         if (!(COND)) return EINVAL

/* define: CHECK_PARAM_MAX
 * TODO: */
#define CHECK_PARAM_MAX(PARAM, MAX_VALUE) \
         if ((PARAM) > (MAX_VALUE)) return EINVAL

/* define: CHECK_PARAM_RANGE
 * TODO: */
#define CHECK_PARAM_RANGE(PARAM, MIN_VALUE, MAX_VALUE) \
         if ((PARAM) < (MIN_VALUE) || (PARAM) > (MAX_VALUE)) return EINVAL

/* define: SIZEDECIMAL
 * TODO: */
#define SIZEDECIMAL(PARAM) \
         1u + ((unsigned)((PARAM)>9)) + ((unsigned)((PARAM)>99))

#define CHECK_SIZE(SIZE) \
         if ((SIZE) > size_memstream(ctrlcodes)) return ENOBUFS

// group: write control-codes

int startedit_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   if (termadapt->termid == termid_LINUXCONSOLE) {
      // 1. Save current state (cursor coordinates, attributes, character sets pointed at by G0, G1).
      // 2. Clear screen
      // 3. Normal Cursor Keys \e[?1l
      // 4. Normal Keypad \e>
      // 5. Replace Mode \e[4l
      // 6. Line Wrap off \e[?7l
      COPY_CODE_SEQUENCE("\x1b""7" "\x1b[H\x1b[J" "\x1b[?1l" "\x1b>" "\x1b[4l" "\x1b[?7l")

   } else {    // assume XTERM
      // 1. Save current state and switch alternate screen
      // 2. Normal Cursor Keys \e[?1l
      // 3. Normal Keypad \e>
      // 4. Replace Mode \e[4l
      // 6. Line Wrap off \e[?7l
      COPY_CODE_SEQUENCE("\x1b[?1049h" "\x1b[?1l" "\x1b>" "\x1b[4l" "\x1b[?7l");
   }

   return 0;
}

int endedit_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   if (termadapt->termid == termid_LINUXCONSOLE) {
      // 1. Line Wrap on \e[?7h
      // 2. Clear screen
      // 3. Restore state most recently saved by startedit_termadapt
      COPY_CODE_SEQUENCE("\x1b[?7h" "\x1b[H\x1b[J" "\x1b""8")

   } else {
      // 1. Line Wrap on \e[?7h
      // 2. Restore state most recently saved by startedit_termadapt
      COPY_CODE_SEQUENCE("\x1b[?7h" "\x1b[?1049l");
   }

   return 0;
}

int clearline_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[2K")

   return 0;
}

int clearendofline_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[K")

   return 0;
}


int clearscreen_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[H\x1b[J")

   return 0;
}

int movecursor_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, unsigned cursorx, unsigned cursory)
{
   (void) termadapt;
   CHECK_PARAM_MAX(cursorx, 998);
   CHECK_PARAM_MAX(cursory, 998);

   // adapt parameter (col, row start from 1)
   cursorx++;
   cursory++;

   const unsigned size = 4u + SIZEDECIMAL(cursorx) + SIZEDECIMAL(cursory);
   CHECK_SIZE(size);

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(cursory);
   writebyte_memstream(ctrlcodes, ';');
   WRITEDECIMAL(cursorx);
   writebyte_memstream(ctrlcodes, 'H');

   return 0;
}

int cursoroff_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[?25l")

   return 0;
}

int cursoron_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[?12l\x1b[?25h")

   return 0;
}

int bold_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[1m")

   return 0;
}

int fgcolor_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, bool bright, unsigned fgcolor)
{
   (void) termadapt;
   CHECK_PARAM_MAX(fgcolor, termcol_NROFCOLOR-1);
   CHECK_SIZE(5);

   // is bright supported ?
   bright &= (termadapt->termid != termid_LINUXCONSOLE);

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   writebyte_memstream(ctrlcodes, bright ? '9' : '3');
   writebyte_memstream(ctrlcodes, (uint8_t) ('0' + fgcolor));
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int bgcolor_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, bool bright, unsigned bgcolor)
{
   (void) termadapt;
   CHECK_PARAM_MAX(bgcolor, termcol_NROFCOLOR-1);

   // is bright supported ?
   bright &= (termadapt->termid != termid_LINUXCONSOLE);

   CHECK_SIZE(5u + (bright!=0));

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   if (bright) {
      writebyte_memstream(ctrlcodes, '1');
      writebyte_memstream(ctrlcodes, '0');
   } else {
      writebyte_memstream(ctrlcodes, '4');
   }
   writebyte_memstream(ctrlcodes, (uint8_t) ('0' + bgcolor));
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int normtext_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[m")

   return 0;
}

int scrollregion_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, unsigned starty, unsigned endy)
{
   (void) termadapt;
   CHECK_PARAM_MAX(endy, 998);
   CHECK_PARAM(starty <= endy);

   // adapt parameter (rows start from 1)
   starty++;
   endy++;

   CHECK_SIZE(4u + SIZEDECIMAL(starty) + SIZEDECIMAL(endy));

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(starty);
   writebyte_memstream(ctrlcodes, ';');
   WRITEDECIMAL(endy);
   writebyte_memstream(ctrlcodes, 'r');

   return 0;
}

int scrollregionoff_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[r");

   return 0;
}

int scrollup_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\n");

   return 0;
}

int scrolldown_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1bM");

   return 0;
}

int delchar_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes)
{
   (void) termadapt;
   COPY_CODE_SEQUENCE("\x1b[P");

   return 0;
}

int dellines_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, unsigned nroflines)
{
   (void) termadapt;
   CHECK_PARAM_RANGE(nroflines, 1, 999);
   CHECK_SIZE(3u + SIZEDECIMAL(nroflines));

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(nroflines);
   writebyte_memstream(ctrlcodes, 'M');

   return 0;
}

int inslines_termadapt(const termadapt_t * termadapt, /*ret*/memstream_t * ctrlcodes, unsigned nroflines)
{
   (void) termadapt;
   CHECK_PARAM_RANGE(nroflines, 1, 999);
   CHECK_SIZE(3u + SIZEDECIMAL(nroflines));

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(nroflines);
   writebyte_memstream(ctrlcodes, 'L');

   return 0;
}

// group: read keycodes

/* define: QUERYMOD
 * Eine Modifikation beginnt mit "1;", wenn der Tastencode nur
 * 3 Bytes lang ist ("\e[T" bzw "\OT"). Sie beginnt mit ";", wenn
 * der Tastencode länger als 3 Byte ist. Die Modifikation besteht
 * aus den Zahlen 2 bis 16. Die Modifiktaion "1;X" bzw. ";X" wird
 * vor dem letzten Byte des Tastencodes eingefügt.
 *
 * Parameter:
 * CODELEN - Konstante. keycodes->next[CODELEN] zeigt auf X, also
 *           das Byte nach dem ";".
 *
 * Precondition:
 * - keycodes->next[CODELEN] ist gültig, d.h. size > CODELEN
 * - keycodes->next[CODELEN-1] == ';'
 *
 * Zuordnung:
 * 2 - Shift
 * 3 - Alt
 * 4 - Alt + Shift
 * 5 - Control
 * 6 - Control + Shift
 * 7 - Control + Alt
 * 8 - Control + Alt + Shift
 * 9 - Meta
 * 10 - Meta + Shift
 * 11 - Meta + Alt
 * 12 - Meta + Alt + Shift
 * 13 - Meta + Control
 * 14 - Meta + Control + Shift
 * 15 - Meta + Control + Alt
 * 16 - Meta + Control + Alt + Shift */
#define QUERYMOD(CODELEN)                    \
   if (size < CODELEN+2u) return ENODATA;    \
   next = keycodes->next[CODELEN];           \
                                             \
   if (next == '1') {                        \
      next = keycodes->next[CODELEN+1];      \
      if (next < '0' || next > '6') return EILSEQ;    \
      if (size < CODELEN+3u) return ENODATA;          \
      mod  = (termmodkey_e) (next - ('0' - 9));   \
      next = keycodes->next[CODELEN+2u];              \
      codelen = CODELEN+3u;                           \
                                                      \
   } else {                                           \
      if (next < '1' || next > '9') return EILSEQ;    \
      mod  = (termmodkey_e) (next - '1');         \
      next = keycodes->next[CODELEN+1u];              \
      codelen = CODELEN+2u;                           \
   }

int key_termadapt(const termadapt_t * termadapt, memstream_ro_t * keycodes, /*out*/termkey_t * key)
{
   (void) termadapt; // handle linux / xterm at the same (most codes are equal)

   size_t size = size_memstream(keycodes);

   if (size == 0) return ENODATA;

   uint8_t next = keycodes->next[0];

   if (next == 0x7f) {
      *key = (termkey_t) termkey_INIT(termkey_BS, termmodkey_NONE);
      skip_memstream(keycodes, 1);
      return 0;

   } else if (next == 0x1b) {
      termmodkey_e mod = termmodkey_NONE;
      unsigned codelen;
      if (size < 2) return ENODATA;
      next = keycodes->next[1];

      if (next == 'O') {
         // matched SS3: '\eO'
         if (size < 3) return ENODATA;
         next = keycodes->next[2];
         codelen = 3;
         if (next == '1' /*mod key pressed*/) {
            if (size >= 4 && ';' != keycodes->next[3]) return EILSEQ;
            QUERYMOD(4)
         }
         if ('A' <= next && next <= 'E') {
            *key = (termkey_t) termkey_INIT((termkey_e) (termkey_UP + next - 'A'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('H' == next) {
            *key = (termkey_t) termkey_INIT(termkey_HOME, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('F' == next) {
            *key = (termkey_t) termkey_INIT(termkey_END, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('P' <= next && next <= 'S') {
            *key = (termkey_t) termkey_INIT((termkey_e) (termkey_F1 + next - 'P'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         }

         return EILSEQ;
      }

      if (next != '[') return EILSEQ;
      // matched CSI: '\e['
      if (size < 3) return ENODATA;
      next = keycodes->next[2];

      if (next == '[') {
         // linux F1 - F5 keys
         if (size < 4) return ENODATA;
         next = keycodes->next[3];
         if (next < 'A' || next > 'E') return EILSEQ;
         *key = (termkey_t) termkey_INIT((termkey_e) (termkey_F1 + next - 'A'), termmodkey_NONE);
         skip_memstream(keycodes, 4);
         return 0;
      }

      if (  ('A' <= next && next <= 'H')
            || (next == '1' && size >= 4 && ';' == keycodes->next[3]/*mod key pressed*/)) {

         codelen = 3;
         if (next == '1') {
            QUERYMOD(4)
         }

         if ('A' <= next && next <= 'E') {
            *key = (termkey_t) termkey_INIT((termkey_e) (termkey_UP + next - 'A'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('G' == next) {
            // linux
            *key = (termkey_t) termkey_INIT(termkey_CENTER, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('H' == next) {
            *key = (termkey_t) termkey_INIT(termkey_HOME, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('F' == next) {
            *key = (termkey_t) termkey_INIT(termkey_END, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('~' == next /*mod is set*/) {
            *key = (termkey_t) termkey_INIT(termkey_HOME, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         }

         return EILSEQ;
      }

      if (next < '1' || next > '6') return EILSEQ;
      if (size < 4) return ENODATA;
      int nr = next - '0';

      next = keycodes->next[3];
      if (  (next == '~')
            || (next == ';' /*mod key pressed*/)) {

         codelen = 4;
         if (next == ';') {
            QUERYMOD(4)
         }

         // matched \e[1~ ... \e[6~
         *key = (termkey_t) termkey_INIT((termkey_e) ((termkey_HOME-1) + nr), mod);
         skip_memstream(keycodes, codelen);
         return 0;
      }

      nr *= 10;
      if (next < '0' || next > '9') return EILSEQ;
      nr += (next - '0');
      if (nr < 15 || nr > 34) return EILSEQ;
      if (nr == 16 || nr == 22 || nr == 27 || nr == 30) return EILSEQ;
      if (size < 5) return ENODATA;
      next = keycodes->next[4];
      codelen = 5;
      if (';' == next && nr <= 24/*linux supports no mod*/) {
         QUERYMOD(5)
      }
      if ('~' != next) return EILSEQ;
      // matched \e[10~ ... \e[39~ || \e[10;X~ ... \e[39;X~
      if (nr <= 24) {
         *key = (termkey_t) termkey_INIT((termkey_e) (termkey_F5 + nr - 15 - (nr > 16) - (nr > 22)), mod);
         skip_memstream(keycodes, codelen);
         return 0;
      }

      // linux shift F1-F8 (F13-F20)
      *key = (termkey_t) termkey_INIT((termkey_e) (termkey_F1 + nr - 25 - (nr > 27) - (nr > 30)), termmodkey_SHIFT);
      skip_memstream(keycodes, 5);
      return 0;
   }

   return EILSEQ;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   termadapt_t * termadapt = 0;
   struct {
      termid_e     id;
      const uint8_t ** types;
   } terminaltypes[] = {
      {
         termid_LINUXCONSOLE, (const uint8_t*[]) {
            (const uint8_t*) "linux", (const uint8_t*) "linux console", 0
         }
      },
      {
         termid_XTERM, (const uint8_t*[]) {
            (const uint8_t*) "xterm", (const uint8_t*) "xterm-debian", (const uint8_t*) "X11 terminal emulator", 0
         }
      }
   };

   for (unsigned i = 0; i < lengthof(terminaltypes); ++i) {

      // TEST new_termadapt
      TEST(0 == new_termadapt(&termadapt, terminaltypes[i].id));
      TEST(0 != termadapt);
      TEST(termadapt == &s_termadapt_builtin[terminaltypes[i].id]);
      TEST(termadapt->termid == terminaltypes[i].id);

      // TEST delete_termadapt
      TEST(0 == delete_termadapt(&termadapt));
      TEST(0 == termadapt);

      for (unsigned ti = 0; terminaltypes[i].types[ti]; ++ti) {

         // TEST newfromtype_termadapt
         TEST(0 == newfromtype_termadapt(&termadapt, terminaltypes[i].types[ti]));
         TEST(0 != termadapt);
         TEST(termadapt == &s_termadapt_builtin[terminaltypes[i].id]);
         TEST(termadapt->termid == terminaltypes[i].id);

         // TEST delete_termadapt
         TEST(0 == delete_termadapt(&termadapt));
         TEST(0 == termadapt);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

#define INIT_TYPES \
   const termid_e types[] = {   \
      termid_LINUXCONSOLE,      \
      termid_XTERM              \
   };

static int test_query(void)
{
   INIT_TYPES // const uint8_t * types[] = { ... }
   termadapt_t * termadapt = 0;

   // TEST id_termadapt: returns any value
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      termadapt_t term = { .termid = i };
      TEST(i == id_termadapt(&term));
   }

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termadapt(&termadapt, types[i]));

      // TEST id_termadapt: test value after new_termadapt
      TEST(i == id_termadapt(termadapt));

      // unprepare
      TEST(0 == delete_termadapt(&termadapt));
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef int (* no_param_f) (const termadapt_t * termadapt, memstream_t * ctrlcodes);

static int testhelper_CODES0(termadapt_t * termadapt, const char * code, no_param_f no_param)
{
   size_t      codelen;
   uint8_t     buffer[100];
   uint8_t     zerobuf[100] = { 0 };
   memstream_t strbuf;

   // test OK
   codelen = strlen(code);
   TEST(codelen <= sizeof(buffer));
   init_memstream(&strbuf, buffer, buffer+codelen);
   TEST(0 == no_param(termadapt, &strbuf));
   TEST(buffer+codelen == strbuf.next);
   TEST(buffer+codelen == strbuf.end);
   TEST(0 == memcmp(buffer, code, codelen));

   // test ENOBUFS
   init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
   TEST(ENOBUFS == no_param(termadapt, &strbuf));
   TEST(strbuf.next == zerobuf);
   TEST(strbuf.end  == zerobuf+codelen-1);
   for (unsigned i = 0; i < sizeof(zerobuf); ++i) {
      TEST(0 == zerobuf[i]);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlcodes0(void)
{
   termadapt_t * termadapt = 0;
   INIT_TYPES // const uint8_t * types[] = { ... }
   const char * codes_startedit[lengthof(types)] = {
      "\x1b""7" "\x1b[H\x1b[J" "\x1b[?1l\x1b>" "\x1b[4l" "\x1b[?7l",
      "\x1b[?1049h" "\x1b[?1l\x1b>" "\x1b[4l" "\x1b[?7l"
   };
   const char * codes_endedit[lengthof(types)] = {
      "\x1b[?7h" "\x1b[H\x1b[J" "\x1b""8" , "\x1b[?7h" "\x1b[?1049l"
   };
   const char * codes_clearline = "\x1b[2K";
   const char * codes_clearendofline = "\x1b[K";
   const char * codes_clearscreen = "\x1b[H\x1b[J";
   const char * codes_cursoroff = "\x1b[?25l";
   const char * codes_cursoron  = "\x1b[?12l\x1b[?25h";
   const char * codes_setbold    = "\x1b[1m";
   const char * codes_normtext = "\x1b[m";
   const char * codes_scrolloff  = "\x1b[r";
   const char * codes_scrollup   = "\n";
   const char * codes_scrolldown = "\x1bM";
   const char * codes_delchar    = "\x1b[P";

   /////////////////////////////////
   // 0 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termadapt(&termadapt, types[i]));

      // TEST startedit_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_startedit[i], &startedit_termadapt));

      // TEST endedit_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_endedit[i], &endedit_termadapt));

      // TEST clearline_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_clearline, &clearline_termadapt));

      // TEST clearendofline_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_clearendofline, &clearendofline_termadapt));

      // TEST clearscreen_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_clearscreen, &clearscreen_termadapt));

      // TEST cursoroff_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_cursoroff, &cursoroff_termadapt));

      // TEST cursoron_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_cursoron, &cursoron_termadapt));

      // TEST bold_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_setbold, &bold_termadapt));

      // TEST normtext_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_normtext, &normtext_termadapt));

      // TEST scrollregionoff_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_scrolloff, &scrollregionoff_termadapt));

      // TEST scrollup_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_scrollup, &scrollup_termadapt));

      // TEST scrolldown_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_scrolldown, &scrolldown_termadapt));

      // TEST delchar_termadapt: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termadapt, codes_delchar, &delchar_termadapt));

      // unprepare
      TEST(0 == delete_termadapt(&termadapt));
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef int (* param_1_f) (const termadapt_t * termadapt, memstream_t * ctrlcodes, unsigned p1);

static int testhelper_CODES1(termadapt_t * termadapt, const char * code, param_1_f param1, unsigned p1, unsigned err1p1, unsigned err2p1)
{
   size_t      codelen;
   uint8_t     buffer[100];
   uint8_t     zerobuf[100]  = { 0 };
   uint8_t     zerobuf2[100] = { 0 };
   memstream_t strbuf;

   // test OK
   codelen = strlen(code);
   TEST(codelen <= sizeof(buffer));
   init_memstream(&strbuf, buffer, buffer+codelen);
   TEST(0 == param1(termadapt, &strbuf, p1));
   TEST(buffer+codelen == strbuf.next);
   TEST(buffer+codelen == strbuf.end);
   TEST(0 == memcmp(buffer, code, codelen));

   // test ENOBUFS
   init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
   TEST(ENOBUFS == param1(termadapt, &strbuf, p1));
   TEST(strbuf.next == zerobuf);
   TEST(strbuf.end  == zerobuf+codelen-1);
   TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

   // test EINVAL
   for (int i = 0; i <= 1; ++i) {
      init_memstream(&strbuf, zerobuf, zerobuf+codelen);
      TEST(EINVAL == param1(termadapt, &strbuf, i ? err2p1 : err1p1));
      TEST(strbuf.next == zerobuf);
      TEST(strbuf.end  == zerobuf+codelen);
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlcodes1(void)
{
   termadapt_t * termadapt = 0;
   INIT_TYPES // const uint8_t * types[] = { ... }
   int         codelen;
   char        code[100];

   /////////////////////////////////
   // 1 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termadapt(&termadapt, types[i]));

      for (unsigned p1 = 1; p1 <= 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 999) p1 = 999;

         // TEST dellines_termadapt: OK, ENOBUFS, EINVAL
         codelen = snprintf(code, sizeof(code), "\x1b[%dM", p1);
         TEST(0 < codelen && codelen < (int)sizeof(code));
         TEST(0 == testhelper_CODES1(termadapt, code, &dellines_termadapt, p1, 0, 1000));

         // TEST inslines_termadapt
         codelen = snprintf(code, sizeof(code), "\x1b[%dL", p1);
         TEST(0 < codelen && codelen < (int)sizeof(code));
         TEST(0 == testhelper_CODES1(termadapt, code, &inslines_termadapt, p1, 0, 1000));
      }

      // unprepare
      TEST(0 == delete_termadapt(&termadapt));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlcodes2(void)
{
   termadapt_t * termadapt = 0;
   INIT_TYPES // const uint8_t * types[] = { ... }
   size_t      codelen;
   uint8_t     buffer[100];
   char        buffer2[100];
   uint8_t     zerobuf[100] = { 0 };
   uint8_t     zerobuf2[100] = { 0 };
   memstream_t strbuf;

   /////////////////////////////////
   // 2 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termadapt(&termadapt, types[i]));

      for (unsigned p1 = 0; p1 < 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 998) p1 = 998;
      for (unsigned p2 = 0; p2 < 999; ++p2) {
         if (p2 > 30) p2 += 100;
         if (p2 > 998) p2 = 998;

         // TEST movecursor_termadapt
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dH", p2+1, p1+1);
         TEST(6 <= codelen && codelen < sizeof(buffer2));
         init_memstream(&strbuf, buffer, buffer+codelen);
         TESTP(0 == movecursor_termadapt(termadapt, &strbuf, p1, p2), "p1=%d p2=%d", p1, p2);
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // TEST movecursor_termadapt: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TESTP(ENOBUFS == movecursor_termadapt(termadapt, &strbuf, p1, p2), "p1=%d p2=%d", p1, p2);
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // TEST movecursor_termadapt: EINVAL
         TEST(EINVAL == movecursor_termadapt(termadapt, &strbuf, 999, 0));
         TEST(EINVAL == movecursor_termadapt(termadapt, &strbuf, 0, 999));

         if (p1 <= p2) {
            // TEST scrollregion_termadapt
            codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dr", p1+1, p2+1);
            TEST(6 <= codelen && codelen < sizeof(buffer2));
            init_memstream(&strbuf, buffer, buffer+codelen);
            TEST(0 == scrollregion_termadapt(termadapt, &strbuf, p1, p2));
            TEST(buffer+codelen == strbuf.next);
            TEST(buffer+codelen == strbuf.end);
            TEST(0 == memcmp(buffer, buffer2, codelen));

            // TEST scrollregion_termadapt: ENOBUFS
            init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
            TEST(ENOBUFS == scrollregion_termadapt(termadapt, &strbuf, p1, p2));
            TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         } else {
            // TEST scrollregion_termadapt: EINVAL
            TEST(EINVAL == scrollregion_termadapt(termadapt, &strbuf, 0, 999));
            TEST(EINVAL == scrollregion_termadapt(termadapt, &strbuf, p1, p2));
         }

      }}

      for (unsigned p1 = 0; p1 <= 1; ++p1) {
      for (unsigned p2 = 0; p2 < termcol_NROFCOLOR; ++p2) {

         unsigned p1_expect = (termadapt->termid == termid_LINUXCONSOLE) ? 0 : p1;

         // fgcolor_termadapt
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 90 : 30));
         TEST(5 == codelen);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == fgcolor_termadapt(termadapt, &strbuf, p1, p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // fgcolor_termadapt: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == fgcolor_termadapt(termadapt, &strbuf, p1, p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // fgcolor_termadapt: EINVAL
         TEST(EINVAL == fgcolor_termadapt(termadapt, &strbuf, p1, termcol_NROFCOLOR));

         // bgcolor_termadapt
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 100 : 40));
         TEST(5 <= codelen && codelen <= 6);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == bgcolor_termadapt(termadapt, &strbuf, p1, p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // bgcolor_termadapt: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == bgcolor_termadapt(termadapt, &strbuf, p1, p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // bgcolor_termadapt: EINVAL
         TEST(EINVAL == bgcolor_termadapt(termadapt, &strbuf, p1, termcol_NROFCOLOR));

      }}

      // unprepare
      TEST(0 == delete_termadapt(&termadapt));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int testhelper_EILSEQ(termadapt_t * termadapt, const uint8_t * str, const uint8_t * end)
{
   termkey_t  key;
   memstream_ro_t keycodes = memstream_INIT(str, end);
   TEST(EILSEQ == key_termadapt(termadapt, &keycodes, &key));
   TEST(str == keycodes.next);
   TEST(end == keycodes.end);

   return 0;
ONERR:
   return EINVAL;
}

static int test_keycodes(void)
{
   INIT_TYPES // const uint8_t * types[] = { ... }
   termadapt_t *     termadapt = 0;
   termkey_t   key;
   struct {
      termkey_e  key;
      const char **    codes;
   } testkeycodes[] = {
      // assumes cursor and keypad keys are configured operating in normal mode
      // in application mode the keypad keys '/','*','-','+','<cr>' generate also special kodes
      // which are not backed up with constants from termkey_e.
      { termkey_F1, (const char*[]) { "\x1b[[A"/*linux*/, "\x1bOP", 0 } },
      { termkey_F2, (const char*[]) { "\x1b[[B"/*linux*/, "\x1bOQ", 0 } },
      { termkey_F3, (const char*[]) { "\x1b[[C"/*linux*/, "\x1bOR", 0 } },
      { termkey_F4, (const char*[]) { "\x1b[[D"/*linux*/, "\x1bOS", 0 } },
      { termkey_F5, (const char*[]) { "\x1b[[E"/*linux*/, "\x1b[15~", 0 } },
      { termkey_F6, (const char*[]) { "\x1b[17~", 0 } },
      { termkey_F7, (const char*[]) { "\x1b[18~", 0 } },
      { termkey_F8, (const char*[]) { "\x1b[19~", 0 } },
      { termkey_F9, (const char*[]) { "\x1b[20~", 0 } },
      { termkey_F10, (const char*[]) { "\x1b[21~", 0 } },
      { termkey_F11, (const char*[]) { "\x1b[23~", 0 } },
      { termkey_F12, (const char*[]) { "\x1b[24~", 0 } },
      { termkey_BS,  (const char*[]) { "\x7f", 0 } },
      { termkey_INS, (const char*[]) { "\x1b[2~", 0 } },
      { termkey_DEL, (const char*[]) { "\x1b[3~", 0 } },
      { termkey_HOME, (const char*[]) { "\x1b[1~"/*VT220*/, "\x1bOH"/*app*/, "\x1b[H"/*normal*/, 0 } },
      { termkey_END,  (const char*[]) { "\x1b[4~"/*VT220*/, "\x1bOF"/*app*/, "\x1b[F"/*normal*/, 0 } },
      { termkey_PAGEUP,   (const char*[]) { "\x1b[5~", 0 } },
      { termkey_PAGEDOWN, (const char*[]) { "\x1b[6~", 0 } },
      { termkey_UP,     (const char*[]) { "\x1bOA"/*app*/, "\x1b[A"/*normal*/, 0 } },
      { termkey_DOWN,   (const char*[]) { "\x1bOB"/*app*/, "\x1b[B"/*normal*/, 0 } },
      { termkey_RIGHT,  (const char*[]) { "\x1bOC"/*app*/, "\x1b[C"/*normal*/, 0 } },
      { termkey_LEFT,   (const char*[]) { "\x1bOD"/*app*/, "\x1b[D"/*normal*/, 0 } },
      { termkey_CENTER, (const char*[]) { "\x1bOE"/*app*/, "\x1b[E"/*normal*/, "\x1b[G"/*linux*/, 0 } }
   };

   struct {
      termkey_e  key;
      const char **    codes;
   } testshiftkeycodes[] = {
      /*all linux specific F13-F20 mapped to shift F1-F8 (which is what you have to press on the keyboard)*/
      { termkey_F1, (const char*[]) { "\x1b[25~", 0 } },
      { termkey_F2, (const char*[]) { "\x1b[26~", 0 } },
      { termkey_F3, (const char*[]) { "\x1b[28~", 0 } },
      { termkey_F4, (const char*[]) { "\x1b[29~", 0 } },
      { termkey_F5, (const char*[]) { "\x1b[31~", 0 } },
      { termkey_F6, (const char*[]) { "\x1b[32~", 0 } },
      { termkey_F7, (const char*[]) { "\x1b[33~", 0 } },
      { termkey_F8, (const char*[]) { "\x1b[34~", 0 } },
   };

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termadapt(&termadapt, types[i]));

      // TEST key_termadapt: unshifted keycodes (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testkeycodes); ++tk) {
         termkey_e K = testkeycodes[tk].key;
         for (unsigned ci = 0; testkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == key_termadapt(termadapt, &keycodes, &key));
            TEST(K == key.nr);
            TEST(0 == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == key_termadapt(termadapt, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }

         }
      }

      // TEST key_termadapt: linux F13 - F20 (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testshiftkeycodes); ++tk) {
         termkey_e K = testshiftkeycodes[tk].key;
         for (unsigned ci = 0; testshiftkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testshiftkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testshiftkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == key_termadapt(termadapt, &keycodes, &key));
            TEST(K == key.nr);
            TEST(termmodkey_SHIFT == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == key_termadapt(termadapt, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }
         }
      }

      // TEST key_termadapt: shifted keycodes (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testkeycodes); ++tk) {
         termkey_e K = testkeycodes[tk].key;
         for (unsigned ci = 0; testkeycodes[tk].codes[ci]; ++ci) {
            size_t len = strlen(testkeycodes[tk].codes[ci]);

            if (  (len < 3) // BACKSPACE
                  || (len == 4 && 0 == memcmp(testkeycodes[tk].codes[ci], "\x1b[[", 3))/*skip linux F1-F5*/) {
               continue;
            }

            for (termmodkey_e mi = termmodkey_NONE+1; mi <= termmodkey_MASK; ++mi) {
               uint8_t        buffer[100];
               size_t         len2     = len + 2 + (mi > 8) + (len == 3);
               const uint8_t* end      = buffer + len2;
               memstream_ro_t keycodes = memstream_INIT(buffer, end);

               // test OK
               memcpy(buffer, testkeycodes[tk].codes[ci], len-1);
               sprintf((char*)buffer+len-1, "%s;%d%c", (len == 3) ? "1" : "", mi+1, testkeycodes[tk].codes[ci][len-1]);
               memset(&key, 255, sizeof(key));
               TEST(0 == key_termadapt(termadapt, &keycodes, &key));
               TEST(K == key.nr);
               TEST(mi == key.mod);
               TEST(end == keycodes.next);
               TEST(end == keycodes.end);

               // test ENODATA
               for (uint8_t * end2 = buffer+1; end2 != end; ++end2) {
                  keycodes = (memstream_ro_t) memstream_INIT(buffer, end2);
                  TEST(ENODATA == key_termadapt(termadapt, &keycodes, &key));
                  TEST(buffer == keycodes.next);
                  TEST(end2   == keycodes.end);
               }

            }
         }
      }

      // TEST key_termadapt: EILSEQ
      for (uint16_t ch = 1; ch <= 255; ++ch) {

         if (ch != 0x7f && ch != 0x1b) {
            uint8_t str[1] = { (uint8_t)ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+1));
         }

         if (ch != '[' && ch != 'O') {
            uint8_t str[2] = { '\x1b', (uint8_t)ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+2));
         }

         if (! ('A' <= ch && ch <= 'E')) {
            uint8_t str[4] = { '\x1b', '[', '[', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (  ! ('P' <= ch && ch <= 'S') && ! ('A' <= ch && ch <= 'F')
               && 'H' != ch && '1' != ch){
            uint8_t str[3] = { '\x1b', 'O', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+3));
         }

         if (  ! ('A' <= ch && ch <= 'H')
               && ch != '[' && ! ('1' <= ch && ch <= '6')){
            uint8_t str[3] = { '\x1b', '[', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+3));
         }

         if (  ! ('7' <= ch && ch <= '9')
               && ch != '5' && ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '1', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (  ! ('0' <= ch && ch <= '6')
               && ch != '8' && ch != '9' && ch != '~' && ch != ';') {
            uint8_t str[4] = { '\x1b', '[', '2', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (  ! ('1' <= ch && ch <= '4')
               && ch != '~' && ch != ';') {
            uint8_t str[4] = { '\x1b', '[', '3', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '4', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '5', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '6', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termadapt, str, str+4));
         }

      }

      // unprepare
      TEST(0 == delete_termadapt(&termadapt));
   }

   return 0;
ONERR:
   return EINVAL;
}

#ifdef KONFIG_USERTEST_INPUT

#include "C-kern/api/io/terminal/terminal.h"

static int usertest_input(void)
{
   terminal_t  term = terminal_FREE;
   struct
   termadapt_t * tcdb;
   uint8_t     buffer[100];
   memstream_t codes = memstream_INIT(buffer, buffer+sizeof(buffer));
   size_t      len;

   // prepare
   TEST(0 == init_terminal(&term));
   TEST(0 == type_terminal(sizeof(buffer), buffer));
   TEST(0 == newfromtype_termadapt(&tcdb, buffer));

   // start edit mode
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == startedit_termadapt(tcdb, &codes));
   write_memstream(&codes, 23, "PRESS KEY [q to exit]\r\n");
   len = offset_memstream(&codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));

   for (int r = 0; r < 50; ++r) {
      size_t  sizeread;
      uint8_t keys[10];
      struct pollfd pfd;
      pfd.events = POLLIN;
      pfd.fd  = term.input;
      poll(&pfd, 1, -1);
      sizeread = tryread_terminal(&term, sizeof(keys), keys);
      printf("[size: %d]:", sizeread);
      for (unsigned i = 0; i < sizeread; ++i) {
         printf("%x", keys[i]);
      }
      printf(";;");
      for (unsigned i = 0; i < sizeread; ++i) {
         if (keys[i] < 32 || keys[i] == 127)
            printf("^%c", 64^keys[i]);
         else
            printf("%c", keys[i]);
      }
      printf("\r\n");
      memstream_ro_t keycodes = memstream_INIT(keys, keys+sizeread);
      termkey_t key;
      if (key_termadapt(tcdb, &keycodes, &key)) {
         printf("UNKNOWN\r\n");
      } else {
         printf("KEY %d\r\n", key.nr);
      }

      if (keys[0] == 'q') break;
   }

   codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   TEST(0 == endedit_termadapt(tcdb, &codes));
   len = offset_memstream(&codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == free_terminal(&term));

   return 0;
ONERR:
   if (tcdb) {
      codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      endedit_termadapt(tcdb, &codes);
      len = offset_memstream(&codes, buffer);
      len = (size_t) write(1, buffer, len);
   }
   configrestore_terminal(&term);
   free_terminal(&term);
   return EINVAL;
}

#endif // KONFIG_USERTEST_INPUT


#ifdef KONFIG_USERTEST_EDIT

#include "C-kern/api/io/terminal/terminal.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"

typedef struct edit_state_t edit_state_t;

struct edit_state_t {
   termadapt_t * termadapt;
   memblock_t lines;
   memstream_t codes;
   unsigned width;
   unsigned height;
   unsigned cx;
   unsigned cy;
};

static int fillscreen(edit_state_t * state)
{
   uint8_t buffer[100];
   memset(state->lines.addr, 0, state->lines.size);

   for (unsigned y = 0; y < state->height-1; ++y) {
      for (unsigned x = 0; x < state->width/2; ++x) {
         state->lines.addr[y*state->width+x] = (uint8_t) ('A' + (x % 32));
      }
   }

   for (unsigned y = 0; y < state->height-1; ++y) {
      unsigned x;
      for (x = 0; x < state->width; ++x) {
         if (! state->lines.addr[y*state->width+x]) break;
      }
      memstream_t codes = memstream_INIT(buffer, buffer+sizeof(buffer));
      movecursor_termadapt(state->termadapt, &codes, 0, y);
      size_t len = offset_memstream(&codes, buffer);
      if (len != (size_t)write(1, buffer, len)) return EINVAL;
      if (x != (size_t)write(1, &state->lines.addr[y*state->width], x)) return EINVAL;
   }

   memstream_t codes = memstream_INIT(buffer, buffer+sizeof(buffer));
   movecursor_termadapt(state->termadapt, &codes, 0, 0);
   size_t len = offset_memstream(&codes, buffer);
   if (len != (size_t)write(1, buffer, len)) return EINVAL;

   return 0;
}

static void updatestatusline(edit_state_t * state, bool isClearLine)
{
   cursoroff_termadapt(state->termadapt, &state->codes);
   movecursor_termadapt(state->termadapt, &state->codes, 3, state->width-1);
   if (isClearLine) {
      clearline_termadapt(state->termadapt,  &state->codes);
   }
   write_memstream(&state->codes, 10, "Position: ");
   uint8_t * buffer = state->codes.next;
   printf_memstream(&state->codes, "(%d, %d)", state->cy+1, state->cx+1);
   while (offset_memstream(&state->codes, buffer) < 10) {
      writebyte_memstream(&state->codes, ' ');
   }
   movecursor_termadapt(state->termadapt, &state->codes, state->cx, state->cy);
   cursoron_termadapt(state->termadapt, &state->codes);
}


static int usertest_edit(void)
{
   int err;
   terminal_t  term = terminal_FREE;
   uint8_t     buffer[200];
   size_t      len;
   size_t      screenbytes;
   edit_state_t state = { .lines = memblock_FREE, .cx = 0, .cy = 0, .codes = memstream_INIT(buffer, buffer+sizeof(buffer)) };

   // prepare
   TEST(0 == init_terminal(&term));
   TEST(0 == type_terminal(sizeof(buffer), buffer));
   TEST(0 == newfromtype_termadapt(&state.termadapt, buffer));
   TEST(0 == size_terminal(&term, &state.width, &state.height));
   screenbytes = state.height * state.width;
   TEST(0 == ALLOC_MM(state.height*state.width, &state.lines));

   // start edit mode
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == startedit_termadapt(state.termadapt, &state.codes));
   len = offset_memstream(&state.codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));

   fillscreen(&state);
   state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   updatestatusline(&state, false);
   scrollregion_termadapt(state.termadapt, &state.codes, 0, state.height-2);
   len = offset_memstream(&state.codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));

   size_t sizeread = 0;
   unsigned nroflines = 0;
   for (int r = 0; r < 150; ++r) {
      uint8_t keys[20];
      struct pollfd pfd;
      pfd.events = POLLIN;
      pfd.fd  = term.input;
      poll(&pfd, 1, -1);
      sizeread += tryread_terminal(&term, sizeof(keys)-sizeread, keys+sizeread);
      while (sizeread) {
         memstream_ro_t keycodes = memstream_INIT(keys, keys+sizeread);
         termkey_t key;
         unsigned oldx = state.cx;
         unsigned oldy = state.cy;
         bool isStatusChange = false;
         state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
         err = key_termadapt(state.termadapt, &keycodes, &key);
         if (0 == err) {
            sizeread -= offset_memstream(&keycodes, keys);
            memmove(keys, keycodes.next, sizeread);

            if (key.nr == termkey_DOWN) {
               if (state.cy + 2 < state.height) {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx, ++ state.cy);
               } else {
                  // scroll up
                  memmove(state.lines.addr, state.lines.addr+state.width, screenbytes - state.width);
                  memset(state.lines.addr+screenbytes-state.width, 0, state.width);
                  scrollup_termadapt(state.termadapt, &state.codes);
                  isStatusChange = true;
               }

            } else if (key.nr == termkey_UP) {
               if (state.cy) {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx, -- state.cy);
               } else {
                  // scroll down
                  memmove(state.lines.addr+state.width, state.lines.addr, screenbytes - state.width);
                  memset(state.lines.addr, 0, state.width);
                  scrolldown_termadapt(state.termadapt, &state.codes);
                  isStatusChange = true;
               }

            } else if (key.nr == termkey_LEFT) {
               if (state.cx) {
                  movecursor_termadapt(state.termadapt, &state.codes, -- state.cx, state.cy);
               }

            } else if (key.nr == termkey_RIGHT) {
               if (state.cx + 1 < state.width) {
                  movecursor_termadapt(state.termadapt, &state.codes, ++ state.cx, state.cy);
               }

            } else if (key.nr == termkey_HOME) {
               if (state.cx) {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx  = 0, state.cy);
               } else {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx  = 0, state.cy = 0);
               }

            } else if (key.nr == termkey_END) {
               if (state.cx + 1 < state.width) {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx = (state.width-1u), state.cy);
               } else {
                  movecursor_termadapt(state.termadapt, &state.codes, state.cx = (state.width-1u), state.cy = (state.height-2u));
               }

            } else if (key.nr == termkey_DEL) {
               if (nroflines) {
                  // delete multiple lines
                  size_t lineoff = state.cy * state.width;
                  size_t lineend = lineoff + nroflines * state.width;
                  if (lineend < screenbytes) {
                     memmove(state.lines.addr+lineoff, state.lines.addr+lineend, screenbytes-lineend);
                  }
                  dellines_termadapt(state.termadapt, &state.codes, nroflines);
               } else {
                  // delete single character
                  size_t cursoff = state.cy * state.width + state.cx;
                  memmove(state.lines.addr+cursoff, state.lines.addr+cursoff+1, state.width - state.cx -1);
                  delchar_termadapt(state.termadapt, &state.codes);
               }

            } else if (key.nr == termkey_INS) {
               if (nroflines) {
                  // insert multiple lines
                  size_t lineoff = state.cy * state.width;
                  size_t lineend = lineoff + nroflines * state.width;
                  if (lineend < screenbytes) {
                     memmove(state.lines.addr+lineend, state.lines.addr+lineoff, screenbytes-lineend);
                  }
                  inslines_termadapt(state.termadapt, &state.codes, nroflines);

               }
            }
            nroflines = 0;

         } else if (err == EILSEQ) {
            if (keys[0] == 'q') goto END_EDIT;
            if ('1' <= keys[0] && keys[0] <= '9') {
               nroflines = (unsigned) (keys[0] - '0');

            } else {
               nroflines = 0;
               write_memstream(&state.codes, 4, "\x1b[4h"); // enter_insert_mode
               bold_termadapt(state.termadapt, &state.codes);
               writebyte_memstream(&state.codes, keys[0]);
               normtext_termadapt(state.termadapt, &state.codes);
               write_memstream(&state.codes, 4, "\x1b[4l"); // exit_insert_mode
               ++ state.cx;
               if (state.cx == state.width) {
                  movecursor_termadapt(state.termadapt, &state.codes, -- state.cx, state.cy);
               }
            }
            -- sizeread;
            memmove(keys, keys+1, sizeread);

         } else {
            break; // ENODATA (need more data)
         }

         if (isStatusChange || oldx != state.cx || oldy != state.cy) {
            updatestatusline(&state, false);
         }
         len = offset_memstream(&state.codes, buffer);
         TEST(len == (size_t)write(1, buffer, len));
      }
   }

END_EDIT:
   state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   TEST(0 == endedit_termadapt(state.termadapt, &state.codes));
   len = offset_memstream(&state.codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == free_terminal(&term));
   TEST(0 == FREE_MM(&state.lines));

   return 0;
ONERR:
   if (state.termadapt) {
      state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      endedit_termadapt(state.termadapt, &state.codes);
      len = offset_memstream(&state.codes, buffer);
      len = (size_t) write(1, buffer, len);
   }
   configrestore_terminal(&term);
   free_terminal(&term);
   FREE_MM(&state.lines);
   return EINVAL;
}

#endif // KONFIG_USERTEST_EDIT

int unittest_io_terminal_termadapt()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_controlcodes0())  goto ONERR;
   if (test_controlcodes1())  goto ONERR;
   if (test_controlcodes2())  goto ONERR;
   if (test_keycodes())       goto ONERR;

   #ifdef KONFIG_USERTEST_INPUT
   execasprocess_unittest(&usertest_input, 0);
   #endif

   #ifdef KONFIG_USERTEST_EDIT
   execasprocess_unittest(&usertest_edit, 0);
   #endif

   return 0;
ONERR:
   return EINVAL;
}

#endif
