/* title: TerminalCapabilityDatabase impl

   Implements <TerminalCapabilityDatabase>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/termcdb.h
    Header file <TerminalCapabilityDatabase>.

   file: C-kern/io/terminal/termcdb.c
    Implementation file <TerminalCapabilityDatabase impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/termcdb.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memstream.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// SWITCH FOR RUNNING a USERTEST
// #define KONFIG_USERTEST_INPUT  // let user press key and tries to determine it
#define KONFIG_USERTEST_EDIT   // let user edit dummy text in memory


// section: termcdb_t

// group: static variables

/* varibale: s_termcdb_builtin
 * Defines two builtin terminal types which are supported right now. */
static termcdb_t s_termcdb_builtin[] = {
   {
      termcdb_id_LINUXCONSOLE,
      (const uint8_t*) "linux|linux console",
   },

   {
      termcdb_id_XTERM,
      (const uint8_t*) "xterm|xterm-debian|X11 terminal emulator",
   },

};

// group: lifetime

int new_termcdb(/*out*/termcdb_t ** termcdb, const termcdb_id_e termid)
{
   int err;

   VALIDATE_INPARAM_TEST(termid < lengthof(s_termcdb_builtin), ONERR, );

   *termcdb = &s_termcdb_builtin[termid];
   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int newfromtype_termcdb(/*out*/termcdb_t ** termcdb, const uint8_t * type)
{
   size_t tlen = strlen((const char*)type);
   for (unsigned i = 0; i < lengthof(s_termcdb_builtin); ++i) {
      const uint8_t * typelist = s_termcdb_builtin[i].typelist;
      const uint8_t * pos = (const uint8_t*) strstr((const char*)typelist, (const char*)type);
      if (  pos && (pos == typelist || pos[-1] == '|') /*begin of type is located at start of string or right after '|'*/
            && (pos[tlen] == '|' || pos[tlen] == 0)) { /*end of type is located at end of string or left of '|'*/
         return new_termcdb(termcdb, i);
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

int startedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   if (termcdb->termid == termcdb_id_LINUXCONSOLE) {
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

int endedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   if (termcdb->termid == termcdb_id_LINUXCONSOLE) {
      // 1. Line Wrap on \e[?7h
      // 2. Clear screen
      // 3. Restore state most recently saved by startedit_termcdb
      COPY_CODE_SEQUENCE("\x1b[?7h" "\x1b[H\x1b[J" "\x1b""8")

   } else {
      // 1. Line Wrap on \e[?7h
      // 2. Restore state most recently saved by startedit_termcdb
      COPY_CODE_SEQUENCE("\x1b[?7h" "\x1b[?1049l");
   }

   return 0;
}

int clearline_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[2K")

   return 0;
}

int clearendofline_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[K")

   return 0;
}


int clearscreen_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[H\x1b[J")

   return 0;
}

int movecursor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, unsigned cursorx, unsigned cursory)
{
   (void) termcdb;
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

int cursoroff_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[?25l")

   return 0;
}

int cursoron_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[?12l\x1b[?25h")

   return 0;
}

int bold_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[1m")

   return 0;
}

int fgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, unsigned fgcolor)
{
   (void) termcdb;
   CHECK_PARAM_MAX(fgcolor, termcdb_col_NROFCOLOR-1);
   CHECK_SIZE(5);

   // is bright supported ?
   bright &= (termcdb->termid != termcdb_id_LINUXCONSOLE);

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   writebyte_memstream(ctrlcodes, bright ? '9' : '3');
   writebyte_memstream(ctrlcodes, (uint8_t) ('0' + fgcolor));
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int bgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, unsigned bgcolor)
{
   (void) termcdb;
   CHECK_PARAM_MAX(bgcolor, termcdb_col_NROFCOLOR-1);

   // is bright supported ?
   bright &= (termcdb->termid != termcdb_id_LINUXCONSOLE);

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

int normtext_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[m")

   return 0;
}

int scrollregion_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, unsigned starty, unsigned endy)
{
   (void) termcdb;
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

int scrollregionoff_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[r");

   return 0;
}

int scrollup_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\n");

   return 0;
}

int scrolldown_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1bM");

   return 0;
}

int delchar_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[P");

   return 0;
}

int dellines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, unsigned nroflines)
{
   (void) termcdb;
   CHECK_PARAM_RANGE(nroflines, 1, 999);
   CHECK_SIZE(3u + SIZEDECIMAL(nroflines));

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(nroflines);
   writebyte_memstream(ctrlcodes, 'M');

   return 0;
}

int inslines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, unsigned nroflines)
{
   (void) termcdb;
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
      mod  = (termcdb_keymod_e) (next - ('0' - 9));   \
      next = keycodes->next[CODELEN+2u];              \
      codelen = CODELEN+3u;                           \
                                                      \
   } else {                                           \
      if (next < '1' || next > '9') return EILSEQ;    \
      mod  = (termcdb_keymod_e) (next - '1');         \
      next = keycodes->next[CODELEN+1u];              \
      codelen = CODELEN+2u;                           \
   }

int key_termcdb(const termcdb_t * termcdb, memstream_ro_t * keycodes, /*out*/termcdb_key_t * key)
{
   (void) termcdb; // handle linux / xterm at the same (most codes are equal)

   size_t size = size_memstream(keycodes);

   if (size == 0) return ENODATA;

   uint8_t next = keycodes->next[0];

   if (next == 0x7f) {
      *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_BS, termcdb_keymod_NONE);
      skip_memstream(keycodes, 1);
      return 0;

   } else if (next == 0x1b) {
      termcdb_keymod_e mod = termcdb_keymod_NONE;
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
            *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_UP + next - 'A'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('H' == next) {
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_HOME, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('F' == next) {
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_END, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('P' <= next && next <= 'S') {
            *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F1 + next - 'P'), mod);
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
         *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F1 + next - 'A'), termcdb_keymod_NONE);
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
            *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_UP + next - 'A'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('G' == next) {
            // linux
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_CENTER, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('H' == next) {
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_HOME, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('F' == next) {
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_END, mod);
            skip_memstream(keycodes, codelen);
            return 0;
         } else if ('~' == next /*mod is set*/) {
            *key = (termcdb_key_t) termcdb_key_INIT(termcdb_keynr_HOME, mod);
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
         *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) ((termcdb_keynr_HOME-1) + nr), mod);
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
         *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F5 + nr - 15 - (nr > 16) - (nr > 22)), mod);
         skip_memstream(keycodes, codelen);
         return 0;
      }

      // linux shift F1-F8 (F13-F20)
      *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F1 + nr - 25 - (nr > 27) - (nr > 30)), termcdb_keymod_SHIFT);
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
   termcdb_t * termcdb = 0;
   struct {
      termcdb_id_e     id;
      const uint8_t ** types;
   } terminaltypes[] = {
      {
         termcdb_id_LINUXCONSOLE, (const uint8_t*[]) {
            (const uint8_t*) "linux", (const uint8_t*) "linux console", 0
         }
      },
      {
         termcdb_id_XTERM, (const uint8_t*[]) {
            (const uint8_t*) "xterm", (const uint8_t*) "xterm-debian", (const uint8_t*) "X11 terminal emulator", 0
         }
      }
   };

   for (unsigned i = 0; i < lengthof(terminaltypes); ++i) {

      // TEST new_termcdb
      TEST(0 == new_termcdb(&termcdb, terminaltypes[i].id));
      TEST(0 != termcdb);
      TEST(termcdb == &s_termcdb_builtin[terminaltypes[i].id]);
      TEST(termcdb->termid == terminaltypes[i].id);

      // TEST delete_termcdb
      TEST(0 == delete_termcdb(&termcdb));
      TEST(0 == termcdb);

      for (unsigned ti = 0; terminaltypes[i].types[ti]; ++ti) {

         // TEST newfromtype_termcdb
         TEST(0 == newfromtype_termcdb(&termcdb, terminaltypes[i].types[ti]));
         TEST(0 != termcdb);
         TEST(termcdb == &s_termcdb_builtin[terminaltypes[i].id]);
         TEST(termcdb->termid == terminaltypes[i].id);

         // TEST delete_termcdb
         TEST(0 == delete_termcdb(&termcdb));
         TEST(0 == termcdb);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

#define INIT_TYPES \
   const termcdb_id_e types[] = {   \
      termcdb_id_LINUXCONSOLE,      \
      termcdb_id_XTERM              \
   };

static int test_query(void)
{
   INIT_TYPES // const uint8_t * types[] = { ... }
   termcdb_t * termcdb = 0;

   // TEST id_termcdb: returns any value
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      termcdb_t term = { .termid = i };
      TEST(i == id_termcdb(&term));
   }

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termcdb(&termcdb, types[i]));

      // TEST id_termcdb: test value after new_termcdb
      TEST(i == id_termcdb(termcdb));

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef int (* no_param_f) (const termcdb_t * termcdb, memstream_t * ctrlcodes);

static int testhelper_CODES0(termcdb_t * termcdb, const char * code, no_param_f no_param)
{
   size_t      codelen;
   uint8_t     buffer[100];
   uint8_t     zerobuf[100] = { 0 };
   memstream_t strbuf;

   // test OK
   codelen = strlen(code);
   TEST(codelen <= sizeof(buffer));
   init_memstream(&strbuf, buffer, buffer+codelen);
   TEST(0 == no_param(termcdb, &strbuf));
   TEST(buffer+codelen == strbuf.next);
   TEST(buffer+codelen == strbuf.end);
   TEST(0 == memcmp(buffer, code, codelen));

   // test ENOBUFS
   init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
   TEST(ENOBUFS == no_param(termcdb, &strbuf));
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
   termcdb_t * termcdb = 0;
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
      TEST(0 == new_termcdb(&termcdb, types[i]));

      // TEST startedit_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_startedit[i], &startedit_termcdb));

      // TEST endedit_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_endedit[i], &endedit_termcdb));

      // TEST clearline_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_clearline, &clearline_termcdb));

      // TEST clearendofline_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_clearendofline, &clearendofline_termcdb));

      // TEST clearscreen_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_clearscreen, &clearscreen_termcdb));

      // TEST cursoroff_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_cursoroff, &cursoroff_termcdb));

      // TEST cursoron_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_cursoron, &cursoron_termcdb));

      // TEST bold_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_setbold, &bold_termcdb));

      // TEST normtext_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_normtext, &normtext_termcdb));

      // TEST scrollregionoff_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_scrolloff, &scrollregionoff_termcdb));

      // TEST scrollup_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_scrollup, &scrollup_termcdb));

      // TEST scrolldown_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_scrolldown, &scrolldown_termcdb));

      // TEST delchar_termcdb: OK, ENOBUFS
      TEST(0 == testhelper_CODES0(termcdb, codes_delchar, &delchar_termcdb));

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

typedef int (* param_1_f) (const termcdb_t * termcdb, memstream_t * ctrlcodes, unsigned p1);

static int testhelper_CODES1(termcdb_t * termcdb, const char * code, param_1_f param1, unsigned p1, unsigned err1p1, unsigned err2p1)
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
   TEST(0 == param1(termcdb, &strbuf, p1));
   TEST(buffer+codelen == strbuf.next);
   TEST(buffer+codelen == strbuf.end);
   TEST(0 == memcmp(buffer, code, codelen));

   // test ENOBUFS
   init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
   TEST(ENOBUFS == param1(termcdb, &strbuf, p1));
   TEST(strbuf.next == zerobuf);
   TEST(strbuf.end  == zerobuf+codelen-1);
   TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

   // test EINVAL
   for (int i = 0; i <= 1; ++i) {
      init_memstream(&strbuf, zerobuf, zerobuf+codelen);
      TEST(EINVAL == param1(termcdb, &strbuf, i ? err2p1 : err1p1));
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
   termcdb_t * termcdb = 0;
   INIT_TYPES // const uint8_t * types[] = { ... }
   int         codelen;
   char        code[100];

   /////////////////////////////////
   // 1 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termcdb(&termcdb, types[i]));

      for (unsigned p1 = 1; p1 <= 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 999) p1 = 999;

         // TEST dellines_termcdb: OK, ENOBUFS, EINVAL
         codelen = snprintf(code, sizeof(code), "\x1b[%dM", p1);
         TEST(0 < codelen && codelen < (int)sizeof(code));
         TEST(0 == testhelper_CODES1(termcdb, code, &dellines_termcdb, p1, 0, 1000));

         // TEST inslines_termcdb
         codelen = snprintf(code, sizeof(code), "\x1b[%dL", p1);
         TEST(0 < codelen && codelen < (int)sizeof(code));
         TEST(0 == testhelper_CODES1(termcdb, code, &inslines_termcdb, p1, 0, 1000));
      }

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlcodes2(void)
{
   termcdb_t * termcdb = 0;
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
      TEST(0 == new_termcdb(&termcdb, types[i]));

      for (unsigned p1 = 0; p1 < 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 998) p1 = 998;
      for (unsigned p2 = 0; p2 < 999; ++p2) {
         if (p2 > 30) p2 += 100;
         if (p2 > 998) p2 = 998;

         // TEST movecursor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dH", p2+1, p1+1);
         TEST(6 <= codelen && codelen < sizeof(buffer2));
         init_memstream(&strbuf, buffer, buffer+codelen);
         TESTP(0 == movecursor_termcdb(termcdb, &strbuf, p1, p2), "p1=%d p2=%d", p1, p2);
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // TEST movecursor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TESTP(ENOBUFS == movecursor_termcdb(termcdb, &strbuf, p1, p2), "p1=%d p2=%d", p1, p2);
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // TEST movecursor_termcdb: EINVAL
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 999, 0));
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 0, 999));

         if (p1 <= p2) {
            // TEST scrollregion_termcdb
            codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dr", p1+1, p2+1);
            TEST(6 <= codelen && codelen < sizeof(buffer2));
            init_memstream(&strbuf, buffer, buffer+codelen);
            TEST(0 == scrollregion_termcdb(termcdb, &strbuf, p1, p2));
            TEST(buffer+codelen == strbuf.next);
            TEST(buffer+codelen == strbuf.end);
            TEST(0 == memcmp(buffer, buffer2, codelen));

            // TEST scrollregion_termcdb: ENOBUFS
            init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
            TEST(ENOBUFS == scrollregion_termcdb(termcdb, &strbuf, p1, p2));
            TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         } else {
            // TEST scrollregion_termcdb: EINVAL
            TEST(EINVAL == scrollregion_termcdb(termcdb, &strbuf, 0, 999));
            TEST(EINVAL == scrollregion_termcdb(termcdb, &strbuf, p1, p2));
         }

      }}

      for (unsigned p1 = 0; p1 <= 1; ++p1) {
      for (unsigned p2 = 0; p2 < termcdb_col_NROFCOLOR; ++p2) {

         unsigned p1_expect = (termcdb->termid == termcdb_id_LINUXCONSOLE) ? 0 : p1;

         // fgcolor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 90 : 30));
         TEST(5 == codelen);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == fgcolor_termcdb(termcdb, &strbuf, p1, p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // fgcolor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == fgcolor_termcdb(termcdb, &strbuf, p1, p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // fgcolor_termcdb: EINVAL
         TEST(EINVAL == fgcolor_termcdb(termcdb, &strbuf, p1, termcdb_col_NROFCOLOR));

         // bgcolor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 100 : 40));
         TEST(5 <= codelen && codelen <= 6);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == bgcolor_termcdb(termcdb, &strbuf, p1, p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // bgcolor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == bgcolor_termcdb(termcdb, &strbuf, p1, p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // bgcolor_termcdb: EINVAL
         TEST(EINVAL == bgcolor_termcdb(termcdb, &strbuf, p1, termcdb_col_NROFCOLOR));

      }}

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int testhelper_EILSEQ(termcdb_t * termcdb, const uint8_t * str, const uint8_t * end)
{
   termcdb_key_t  key;
   memstream_ro_t keycodes = memstream_INIT(str, end);
   TEST(EILSEQ == key_termcdb(termcdb, &keycodes, &key));
   TEST(str == keycodes.next);
   TEST(end == keycodes.end);

   return 0;
ONERR:
   return EINVAL;
}

static int test_keycodes(void)
{
   INIT_TYPES // const uint8_t * types[] = { ... }
   termcdb_t *     termcdb = 0;
   termcdb_key_t   key;
   struct {
      termcdb_keynr_e  key;
      const char **    codes;
   } testkeycodes[] = {
      // assumes cursor and keypad keys are configured operating in normal mode
      // in application mode the keypad keys '/','*','-','+','<cr>' generate also special kodes
      // which are not backed up with constants from termcdb_keynr_e.
      { termcdb_keynr_F1, (const char*[]) { "\x1b[[A"/*linux*/, "\x1bOP", 0 } },
      { termcdb_keynr_F2, (const char*[]) { "\x1b[[B"/*linux*/, "\x1bOQ", 0 } },
      { termcdb_keynr_F3, (const char*[]) { "\x1b[[C"/*linux*/, "\x1bOR", 0 } },
      { termcdb_keynr_F4, (const char*[]) { "\x1b[[D"/*linux*/, "\x1bOS", 0 } },
      { termcdb_keynr_F5, (const char*[]) { "\x1b[[E"/*linux*/, "\x1b[15~", 0 } },
      { termcdb_keynr_F6, (const char*[]) { "\x1b[17~", 0 } },
      { termcdb_keynr_F7, (const char*[]) { "\x1b[18~", 0 } },
      { termcdb_keynr_F8, (const char*[]) { "\x1b[19~", 0 } },
      { termcdb_keynr_F9, (const char*[]) { "\x1b[20~", 0 } },
      { termcdb_keynr_F10, (const char*[]) { "\x1b[21~", 0 } },
      { termcdb_keynr_F11, (const char*[]) { "\x1b[23~", 0 } },
      { termcdb_keynr_F12, (const char*[]) { "\x1b[24~", 0 } },
      { termcdb_keynr_BS,  (const char*[]) { "\x7f", 0 } },
      { termcdb_keynr_INS, (const char*[]) { "\x1b[2~", 0 } },
      { termcdb_keynr_DEL, (const char*[]) { "\x1b[3~", 0 } },
      { termcdb_keynr_HOME, (const char*[]) { "\x1b[1~"/*VT220*/, "\x1bOH"/*app*/, "\x1b[H"/*normal*/, 0 } },
      { termcdb_keynr_END,  (const char*[]) { "\x1b[4~"/*VT220*/, "\x1bOF"/*app*/, "\x1b[F"/*normal*/, 0 } },
      { termcdb_keynr_PAGEUP,   (const char*[]) { "\x1b[5~", 0 } },
      { termcdb_keynr_PAGEDOWN, (const char*[]) { "\x1b[6~", 0 } },
      { termcdb_keynr_UP,     (const char*[]) { "\x1bOA"/*app*/, "\x1b[A"/*normal*/, 0 } },
      { termcdb_keynr_DOWN,   (const char*[]) { "\x1bOB"/*app*/, "\x1b[B"/*normal*/, 0 } },
      { termcdb_keynr_RIGHT,  (const char*[]) { "\x1bOC"/*app*/, "\x1b[C"/*normal*/, 0 } },
      { termcdb_keynr_LEFT,   (const char*[]) { "\x1bOD"/*app*/, "\x1b[D"/*normal*/, 0 } },
      { termcdb_keynr_CENTER, (const char*[]) { "\x1bOE"/*app*/, "\x1b[E"/*normal*/, "\x1b[G"/*linux*/, 0 } }
   };

   struct {
      termcdb_keynr_e  key;
      const char **    codes;
   } testshiftkeycodes[] = {
      /*all linux specific F13-F20 mapped to shift F1-F8 (which is what you have to press on the keyboard)*/
      { termcdb_keynr_F1, (const char*[]) { "\x1b[25~", 0 } },
      { termcdb_keynr_F2, (const char*[]) { "\x1b[26~", 0 } },
      { termcdb_keynr_F3, (const char*[]) { "\x1b[28~", 0 } },
      { termcdb_keynr_F4, (const char*[]) { "\x1b[29~", 0 } },
      { termcdb_keynr_F5, (const char*[]) { "\x1b[31~", 0 } },
      { termcdb_keynr_F6, (const char*[]) { "\x1b[32~", 0 } },
      { termcdb_keynr_F7, (const char*[]) { "\x1b[33~", 0 } },
      { termcdb_keynr_F8, (const char*[]) { "\x1b[34~", 0 } },
   };

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termcdb(&termcdb, types[i]));

      // TEST key_termcdb: unshifted keycodes (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testkeycodes); ++tk) {
         termcdb_keynr_e K = testkeycodes[tk].key;
         for (unsigned ci = 0; testkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == key_termcdb(termcdb, &keycodes, &key));
            TEST(K == key.nr);
            TEST(0 == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == key_termcdb(termcdb, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }

         }
      }

      // TEST key_termcdb: linux F13 - F20 (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testshiftkeycodes); ++tk) {
         termcdb_keynr_e K = testshiftkeycodes[tk].key;
         for (unsigned ci = 0; testshiftkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testshiftkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testshiftkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == key_termcdb(termcdb, &keycodes, &key));
            TEST(K == key.nr);
            TEST(termcdb_keymod_SHIFT == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == key_termcdb(termcdb, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }
         }
      }

      // TEST key_termcdb: shifted keycodes (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testkeycodes); ++tk) {
         termcdb_keynr_e K = testkeycodes[tk].key;
         for (unsigned ci = 0; testkeycodes[tk].codes[ci]; ++ci) {
            size_t len = strlen(testkeycodes[tk].codes[ci]);

            if (  (len < 3) // BACKSPACE
                  || (len == 4 && 0 == memcmp(testkeycodes[tk].codes[ci], "\x1b[[", 3))/*skip linux F1-F5*/) {
               continue;
            }

            const termcdb_keymod_e miMAX = (termcdb_keymod_SHIFT|termcdb_keymod_ALT|termcdb_keymod_CTRL|termcdb_keymod_META);
            for (termcdb_keymod_e mi = termcdb_keymod_SHIFT; mi <= miMAX; ++mi) {
               uint8_t        buffer[100];
               size_t         len2     = len + 2 + (mi > 8) + (len == 3);
               const uint8_t* end      = buffer + len2;
               memstream_ro_t keycodes = memstream_INIT(buffer, end);

               // test OK
               memcpy(buffer, testkeycodes[tk].codes[ci], len-1);
               sprintf((char*)buffer+len-1, "%s;%d%c", (len == 3) ? "1" : "", mi+1, testkeycodes[tk].codes[ci][len-1]);
               memset(&key, 255, sizeof(key));
               TEST(0 == key_termcdb(termcdb, &keycodes, &key));
               TEST(K == key.nr);
               TEST(mi == key.mod);
               TEST(end == keycodes.next);
               TEST(end == keycodes.end);

               // test ENODATA
               for (uint8_t * end2 = buffer+1; end2 != end; ++end2) {
                  keycodes = (memstream_ro_t) memstream_INIT(buffer, end2);
                  TEST(ENODATA == key_termcdb(termcdb, &keycodes, &key));
                  TEST(buffer == keycodes.next);
                  TEST(end2   == keycodes.end);
               }

            }
         }
      }

      // TEST key_termcdb: EILSEQ
      for (uint16_t ch = 1; ch <= 255; ++ch) {

         if (ch != 0x7f && ch != 0x1b) {
            uint8_t str[1] = { (uint8_t)ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+1));
         }

         if (ch != '[' && ch != 'O') {
            uint8_t str[2] = { '\x1b', (uint8_t)ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+2));
         }

         if (! ('A' <= ch && ch <= 'E')) {
            uint8_t str[4] = { '\x1b', '[', '[', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (  ! ('P' <= ch && ch <= 'S') && ! ('A' <= ch && ch <= 'F')
               && 'H' != ch && '1' != ch){
            uint8_t str[3] = { '\x1b', 'O', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+3));
         }

         if (  ! ('A' <= ch && ch <= 'H')
               && ch != '[' && ! ('1' <= ch && ch <= '6')){
            uint8_t str[3] = { '\x1b', '[', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+3));
         }

         if (  ! ('7' <= ch && ch <= '9')
               && ch != '5' && ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '1', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (  ! ('0' <= ch && ch <= '6')
               && ch != '8' && ch != '9' && ch != '~' && ch != ';') {
            uint8_t str[4] = { '\x1b', '[', '2', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (  ! ('1' <= ch && ch <= '4')
               && ch != '~' && ch != ';') {
            uint8_t str[4] = { '\x1b', '[', '3', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '4', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '5', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

         if (ch != ';' && ch != '~') {
            uint8_t str[4] = { '\x1b', '[', '6', (uint8_t) ch };
            TEST(0 == testhelper_EILSEQ(termcdb, str, str+4));
         }

      }

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
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
   termcdb_t * tcdb;
   uint8_t     buffer[100];
   memstream_t codes = memstream_INIT(buffer, buffer+sizeof(buffer));
   size_t      len;

   // prepare
   TEST(0 == init_terminal(&term));
   TEST(0 == type_terminal(sizeof(buffer), buffer));
   TEST(0 == newfromtype_termcdb(&tcdb, buffer));

   // start edit mode
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == startedit_termcdb(tcdb, &codes));
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
      termcdb_key_t key;
      if (key_termcdb(tcdb, &keycodes, &key)) {
         printf("UNKNOWN\r\n");
      } else {
         printf("KEY %d\r\n", key.nr);
      }

      if (keys[0] == 'q') break;
   }

   codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   TEST(0 == endedit_termcdb(tcdb, &codes));
   len = offset_memstream(&codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == free_terminal(&term));

   return 0;
ONERR:
   if (tcdb) {
      codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      endedit_termcdb(tcdb, &codes);
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
   termcdb_t * termcdb;
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
      movecursor_termcdb(state->termcdb, &codes, 0, y);
      size_t len = offset_memstream(&codes, buffer);
      if (len != (size_t)write(1, buffer, len)) return EINVAL;
      if (x != (size_t)write(1, &state->lines.addr[y*state->width], x)) return EINVAL;
   }

   memstream_t codes = memstream_INIT(buffer, buffer+sizeof(buffer));
   movecursor_termcdb(state->termcdb, &codes, 0, 0);
   size_t len = offset_memstream(&codes, buffer);
   if (len != (size_t)write(1, buffer, len)) return EINVAL;

   return 0;
}

static void updatestatusline(edit_state_t * state, bool isClearLine)
{
   cursoroff_termcdb(state->termcdb, &state->codes);
   movecursor_termcdb(state->termcdb, &state->codes, 3, state->width-1);
   if (isClearLine) {
      clearline_termcdb(state->termcdb,  &state->codes);
   }
   write_memstream(&state->codes, 10, "Position: ");
   uint8_t * buffer = state->codes.next;
   printf_memstream(&state->codes, "(%d, %d)", state->cy+1, state->cx+1);
   while (offset_memstream(&state->codes, buffer) < 10) {
      writebyte_memstream(&state->codes, ' ');
   }
   movecursor_termcdb(state->termcdb, &state->codes, state->cx, state->cy);
   cursoron_termcdb(state->termcdb, &state->codes);
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
   TEST(0 == newfromtype_termcdb(&state.termcdb, buffer));
   TEST(0 == size_terminal(&term, &state.width, &state.height));
   screenbytes = state.height * state.width;
   TEST(0 == ALLOC_MM(state.height*state.width, &state.lines));

   // start edit mode
   TEST(0 == configrawedit_terminal(&term));
   TEST(0 == startedit_termcdb(state.termcdb, &state.codes));
   len = offset_memstream(&state.codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));

   fillscreen(&state);
   state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
   updatestatusline(&state, false);
   scrollregion_termcdb(state.termcdb, &state.codes, 0, state.height-2);
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
         termcdb_key_t key;
         unsigned oldx = state.cx;
         unsigned oldy = state.cy;
         bool isStatusChange = false;
         state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
         err = key_termcdb(state.termcdb, &keycodes, &key);
         if (0 == err) {
            sizeread -= offset_memstream(&keycodes, keys);
            memmove(keys, keycodes.next, sizeread);

            if (key.nr == termcdb_keynr_DOWN) {
               if (state.cy + 2 < state.height) {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx, ++ state.cy);
               } else {
                  // scroll up
                  memmove(state.lines.addr, state.lines.addr+state.width, screenbytes - state.width);
                  memset(state.lines.addr+screenbytes-state.width, 0, state.width);
                  scrollup_termcdb(state.termcdb, &state.codes);
                  isStatusChange = true;
               }

            } else if (key.nr == termcdb_keynr_UP) {
               if (state.cy) {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx, -- state.cy);
               } else {
                  // scroll down
                  memmove(state.lines.addr+state.width, state.lines.addr, screenbytes - state.width);
                  memset(state.lines.addr, 0, state.width);
                  scrolldown_termcdb(state.termcdb, &state.codes);
                  isStatusChange = true;
               }

            } else if (key.nr == termcdb_keynr_LEFT) {
               if (state.cx) {
                  movecursor_termcdb(state.termcdb, &state.codes, -- state.cx, state.cy);
               }

            } else if (key.nr == termcdb_keynr_RIGHT) {
               if (state.cx + 1 < state.width) {
                  movecursor_termcdb(state.termcdb, &state.codes, ++ state.cx, state.cy);
               }

            } else if (key.nr == termcdb_keynr_HOME) {
               if (state.cx) {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx  = 0, state.cy);
               } else {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx  = 0, state.cy = 0);
               }

            } else if (key.nr == termcdb_keynr_END) {
               if (state.cx + 1 < state.width) {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx = (state.width-1u), state.cy);
               } else {
                  movecursor_termcdb(state.termcdb, &state.codes, state.cx = (state.width-1u), state.cy = (state.height-2u));
               }

            } else if (key.nr == termcdb_keynr_DEL) {
               if (nroflines) {
                  // delete multiple lines
                  size_t lineoff = state.cy * state.width;
                  size_t lineend = lineoff + nroflines * state.width;
                  if (lineend < screenbytes) {
                     memmove(state.lines.addr+lineoff, state.lines.addr+lineend, screenbytes-lineend);
                  }
                  dellines_termcdb(state.termcdb, &state.codes, nroflines);
               } else {
                  // delete single character
                  size_t cursoff = state.cy * state.width + state.cx;
                  memmove(state.lines.addr+cursoff, state.lines.addr+cursoff+1, state.width - state.cx -1);
                  delchar_termcdb(state.termcdb, &state.codes);
               }

            } else if (key.nr == termcdb_keynr_INS) {
               if (nroflines) {
                  // insert multiple lines
                  size_t lineoff = state.cy * state.width;
                  size_t lineend = lineoff + nroflines * state.width;
                  if (lineend < screenbytes) {
                     memmove(state.lines.addr+lineend, state.lines.addr+lineoff, screenbytes-lineend);
                  }
                  inslines_termcdb(state.termcdb, &state.codes, nroflines);

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
               bold_termcdb(state.termcdb, &state.codes);
               writebyte_memstream(&state.codes, keys[0]);
               normtext_termcdb(state.termcdb, &state.codes);
               write_memstream(&state.codes, 4, "\x1b[4l"); // exit_insert_mode
               ++ state.cx;
               if (state.cx == state.width) {
                  movecursor_termcdb(state.termcdb, &state.codes, -- state.cx, state.cy);
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
   TEST(0 == endedit_termcdb(state.termcdb, &state.codes));
   len = offset_memstream(&state.codes, buffer);
   TEST(len == (size_t)write(1, buffer, len));
   TEST(0 == configrestore_terminal(&term));
   TEST(0 == free_terminal(&term));
   TEST(0 == FREE_MM(&state.lines));

   return 0;
ONERR:
   if (state.termcdb) {
      state.codes = (memstream_t) memstream_INIT(buffer, buffer+sizeof(buffer));
      endedit_termcdb(state.termcdb, &state.codes);
      len = offset_memstream(&state.codes, buffer);
      len = (size_t) write(1, buffer, len);
   }
   configrestore_terminal(&term);
   free_terminal(&term);
   FREE_MM(&state.lines);
   return EINVAL;
}

#endif // KONFIG_USERTEST_EDIT

int unittest_io_terminal_termcdb()
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
