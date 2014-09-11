/* title: TerminalControlDatabase impl

   Implements <TerminalControlDatabase>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/termcdb.h
    Header file <TerminalControlDatabase>.

   file: C-kern/io/terminal/termcdb.c
    Implementation file <TerminalControlDatabase impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/termcdb.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif



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

// group: control-codes

#define COPY_CODE_SEQUENCE(CODESEQ) \
   const unsigned len = sizeof(CODESEQ)-1; \
   \
   if (len > size_memstream(ctrlcodes)) return ENOBUFS; \
   \
   write_memstream(ctrlcodes, len, CODESEQ);

#define WRITEDECIMAL(NR) \
   if (NR > 99) writebyte_memstream(ctrlcodes, (uint8_t) ('0' + NR/100)); \
   if (NR > 9)  writebyte_memstream(ctrlcodes, (uint8_t) ('0' + (NR/10)%10)); \
   writebyte_memstream(ctrlcodes, (uint8_t) ('0' + NR%10))

int startedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: smcup

   if (termcdb->termid == termcdb_id_LINUXCONSOLE) {
      // save current state (cursor coordinates, attributes, character sets pointed at by G0, G1).
      COPY_CODE_SEQUENCE("\x1b""7")
   } else {    // assume XTERM
      // save current state and switch alternate screen
      COPY_CODE_SEQUENCE("\x1b[?1049h");
   }

   return 0;
}

int endedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: rmcup

   if (termcdb->termid == termcdb_id_LINUXCONSOLE) {
      // restore state most recently saved by startedit_termcdb
      COPY_CODE_SEQUENCE("\x1b""8")
   } else {
      // restore state most recently saved by startedit_termcdb
      COPY_CODE_SEQUENCE("\x1b[?1049l");
   }

   return 0;
}

int setnormkeys_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   if (termcdb->termid == termcdb_id_XTERM) {
      // Normal Cursor Keys \e[?1l
      // Normal Keypad \e>
      COPY_CODE_SEQUENCE("\x1b[?1l\x1b>");
   }

   return 0;
}

int clearline_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: el1 el

   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[1K\x1b[K")

   return 0;
}

int clearscreen_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: clear

   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[H\x1b[J")

   return 0;
}

int movecursor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t cursorx, uint16_t cursory)
{
   // terminfo: cup "\x1b[9;9H"

   (void) termcdb;
   if (cursorx < 1 || cursorx > 999 || cursory < 1 || cursory > 999) return EINVAL;

   const unsigned len = 6;
   const unsigned p1len = (unsigned) ((cursorx > 9) + (cursorx > 99));
   const unsigned p2len = (unsigned) ((cursory > 9) + (cursory > 99));

   if (len + p1len + p2len > size_memstream(ctrlcodes)) return ENOBUFS;

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
   // terminfo: civis "\x1b[?25l"
   #define CODESEQ "\x1b[?25l"

   (void) termcdb;
   if (sizeof(CODESEQ)-1 > size_memstream(ctrlcodes)) return ENOBUFS;

   write_memstream(ctrlcodes, sizeof(CODESEQ)-1, CODESEQ);

   #undef CODESEQ
   return 0;
}

int cursoron_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: cnorm "\x1b[?12l\x1b[?25h"
   #define CODESEQ "\x1b[?12l\x1b[?25h"

   (void) termcdb;
   if (sizeof(CODESEQ)-1 > size_memstream(ctrlcodes)) return ENOBUFS;

   write_memstream(ctrlcodes, sizeof(CODESEQ)-1, CODESEQ);

   #undef CODESEQ
   return 0;
}

int setbold_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: bold

   (void) termcdb;
   if (4 > size_memstream(ctrlcodes)) return ENOBUFS;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   writebyte_memstream(ctrlcodes, '1');
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int setfgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, uint8_t fgcolor)
{
   // terminfo: setf

   (void) termcdb;
   if (fgcolor >= termcdb_col_NROFCOLOR) return EINVAL;
   if (5 > size_memstream(ctrlcodes)) return ENOBUFS;

   // is bright supported
   bright = bright ? (termcdb->termid != termcdb_id_LINUXCONSOLE) : bright;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   writebyte_memstream(ctrlcodes, bright ? '9' : '3');
   writebyte_memstream(ctrlcodes, (uint8_t) ('0' + fgcolor));
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int setbgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, uint8_t bgcolor)
{
   // terminfo: setb

   (void) termcdb;
   if (bgcolor >= termcdb_col_NROFCOLOR) return EINVAL;

   // is bright supported
   bright = bright & (termcdb->termid != termcdb_id_LINUXCONSOLE);

   if ((unsigned)(5 + (bright != 0)) > size_memstream(ctrlcodes)) return ENOBUFS;

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

int resetstyle_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: rmul rmso

   (void) termcdb;
   if (3 > size_memstream(ctrlcodes)) return ENOBUFS;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   writebyte_memstream(ctrlcodes, 'm');

   return 0;
}

int setscrollregion_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t starty, uint16_t endy)
{
   // terminfo: csr "\x1b[9;9r"

   (void) termcdb;
   if (starty < 1 || starty > endy || endy > 999) return EINVAL;

   const unsigned len = 6;
   const unsigned p1len = (unsigned) ((starty > 9) + (starty > 99));
   const unsigned p2len = (unsigned) ((endy   > 9) + (endy   > 99));

   if (len + p1len + p2len > size_memstream(ctrlcodes)) return ENOBUFS;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(starty);
   writebyte_memstream(ctrlcodes, ';');
   WRITEDECIMAL(endy);
   writebyte_memstream(ctrlcodes, 'r');

   return 0;
}

int resetscrollregion_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes)
{
   // terminfo: csr "\x1b[r"

   (void) termcdb;
   COPY_CODE_SEQUENCE("\x1b[r");

   return 0;
}

int deletelines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t nroflines)
{
   // terminfo: dl "\x1b[9M"

   (void) termcdb;
   if (nroflines < 1 || nroflines > 999) return EINVAL;

   const unsigned len = 4;
   const unsigned p1len = (unsigned) ((nroflines > 9) + (nroflines > 99));

   if (len + p1len > size_memstream(ctrlcodes)) return ENOBUFS;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(nroflines);
   writebyte_memstream(ctrlcodes, 'M');

   return 0;
}

int insertlines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t nroflines)
{
   // terminfo: il "\x1b[9L"

   (void) termcdb;
   if (nroflines < 1 || nroflines > 999) return EINVAL;

   const unsigned len = 4;
   const unsigned p1len = (unsigned) ((nroflines > 9) + (nroflines > 99));

   if (len + p1len > size_memstream(ctrlcodes)) return ENOBUFS;

   writebyte_memstream(ctrlcodes, '\x1b');
   writebyte_memstream(ctrlcodes, '[');
   WRITEDECIMAL(nroflines);
   writebyte_memstream(ctrlcodes, 'L');

   return 0;
}

// group: keycodes

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

int querykey_termcdb(const termcdb_t * termcdb, memstream_ro_t * keycodes, /*out*/termcdb_key_t * key)
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
      if (size < 3) return ENODATA;
      next = keycodes->next[1];

      if (next == 'O') {
         // matched SS3: '\eO'
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
         } else if (next >= 'P' || next <= 'S') {
            *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F1 + next - 'P'), mod);
            skip_memstream(keycodes, codelen);
            return 0;
         }

         return EILSEQ;
      }

      if (next != '[') return EILSEQ;
      // matched CSI: '\e['
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
      if (size < 5) return ENODATA;
      next = keycodes->next[4];
      codelen = 5;
      if (';' == next) {
         QUERYMOD(5)
      }
      if ('~' != next) return EILSEQ;
      // matched \e[10~ ... \e[39~ || \e[10;X~ ... \e[39;X~
      if (nr <= 24) {
         if (nr < 15 || nr == 16 || nr == 22) return EILSEQ;
         *key = (termcdb_key_t) termcdb_key_INIT((termcdb_keynr_e) (termcdb_keynr_F5 + nr - 15 - (nr > 16) - (nr > 22)), mod);
         skip_memstream(keycodes, codelen);
         return 0;
      }
      if (mod/*linux supports no mod*/ || nr > 34 || nr == 27 || nr == 30) return EILSEQ;

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

static int test_controlcodes0(void)
{
   termcdb_t * termcdb = 0;
   INIT_TYPES // const uint8_t * types[] = { ... }
   const char * codes_startedit[lengthof(types)] = {
      "\x1b""7", "\x1b[?1049h"
   };
   const char * codes_endedit[lengthof(types)] = {
      "\x1b""8", "\x1b[?1049l"
   };
   const char * codes_setnormkeys[lengthof(types)] = {
      "", "\x1b[?1l\x1b>"
   };
   const char * codes_clearline = "\x1b[1K\x1b[K";
   const char * codes_clearscreen = "\x1b[H\x1b[J";
   const char * codes_cursoroff = "\x1b[?25l";
   const char * codes_cursoron  = "\x1b[?12l\x1b[?25h";
   const char * codes_setbold    = "\x1b[1m";
   const char * codes_resetstyle = "\x1b[m";
   const char * codes_resetscroll = "\x1b[r";
   size_t      codelen;
   uint8_t     buffer[100];
   uint8_t     zerobuf[100] = { 0 };
   uint8_t     zerobuf2[100] = { 0 };
   memstream_t strbuf;

   /////////////////////////////////
   // 0 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termcdb(&termcdb, types[i]));

      // TEST startedit_termcdb
      codelen = strlen(codes_startedit[i]);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == startedit_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_startedit[i], codelen));

      // TEST startedit_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == startedit_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST endedit_termcdb
      codelen = strlen(codes_endedit[i]);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == endedit_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_endedit[i], codelen));

      // TEST endedit_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == endedit_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST setnormkeys_termcdb
      codelen = strlen(codes_setnormkeys[i]);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == setnormkeys_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_setnormkeys[i], codelen));

      // TEST setnormkeys_termcdb: ENOBUFS
      if (codelen > 0) {
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == setnormkeys_termcdb(termcdb, &strbuf));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed
      }

      // TEST clearline_termcdb
      codelen = strlen(codes_clearline);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == clearline_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_clearline, codelen));

      // TEST clearline_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == clearline_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST clearscreen_termcdb
      codelen = strlen(codes_clearscreen);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == clearscreen_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_clearscreen, codelen));

      // TEST clearscreen_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == clearscreen_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST cursoroff_termcdb
      codelen = strlen(codes_cursoroff);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == cursoroff_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_cursoroff, codelen));

      // TEST cursoroff_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == cursoroff_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST cursoron_termcdb
      codelen = strlen(codes_cursoron);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == cursoron_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_cursoron, codelen));

      // TEST cursoron_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == cursoron_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST setbold_termcdb
      codelen = strlen(codes_setbold);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == setbold_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_setbold, codelen));

      // TEST setbold_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == setbold_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST resetstyle_termcdb
      codelen = strlen(codes_resetstyle);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == resetstyle_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_resetstyle, codelen));

      // TEST resetstyle_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == resetstyle_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // TEST resetscrollregion_termcdb
      codelen = strlen(codes_resetscroll);
      init_memstream(&strbuf, buffer, buffer+codelen);
      TEST(0 == resetscrollregion_termcdb(termcdb, &strbuf));
      TEST(buffer+codelen == strbuf.next);
      TEST(buffer+codelen == strbuf.end);
      TEST(0 == memcmp(buffer, codes_resetscroll, codelen));

      // TEST resetscrollregion_termcdb: ENOBUFS
      init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
      TEST(ENOBUFS == resetscrollregion_termcdb(termcdb, &strbuf));
      TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlcodes1(void)
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
   // 1 Parameter Code Sequences

   for (unsigned i = 0; i < lengthof(types); ++i) {
      // prepare
      TEST(0 == new_termcdb(&termcdb, types[i]));

      for (unsigned p1 = 1; p1 <= 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 999) p1 = 999;

         // TEST deletelines_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dM", p1);
         TEST(4 <= codelen && codelen < sizeof(buffer2));
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == deletelines_termcdb(termcdb, &strbuf, (uint16_t)p1));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // TEST deletelines_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == deletelines_termcdb(termcdb, &strbuf, (uint16_t)p1));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // TEST deletelines_termcdb: EINVAL
         TEST(EINVAL == deletelines_termcdb(termcdb, &strbuf, 0));
         TEST(EINVAL == deletelines_termcdb(termcdb, &strbuf, 1000));

         // TEST insertlines_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dL", p1);
         TEST(4 <= codelen && codelen < sizeof(buffer2));
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == insertlines_termcdb(termcdb, &strbuf, (uint16_t)p1));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // TEST insertlines_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == insertlines_termcdb(termcdb, &strbuf, (uint16_t)p1));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // TEST insertlines_termcdb: EINVAL
         TEST(EINVAL == insertlines_termcdb(termcdb, &strbuf, 0));
         TEST(EINVAL == insertlines_termcdb(termcdb, &strbuf, 1000));
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

      for (unsigned p1 = 1; p1 <= 999; ++p1) {
         if (p1 > 30) p1 += 100;
         if (p1 > 999) p1 = 999;
      for (unsigned p2 = 1; p2 <= 999; ++p2) {
         if (p2 > 30) p2 += 100;
         if (p2 > 999) p2 = 999;

         // TEST movecursor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dH", p2, p1);
         TEST(6 <= codelen && codelen < sizeof(buffer2));
         init_memstream(&strbuf, buffer, buffer+codelen);
         TESTP(0 == movecursor_termcdb(termcdb, &strbuf, (uint16_t)p1, (uint16_t)p2), "p1=%d p2=%d", p1, p2);
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // TEST movecursor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TESTP(ENOBUFS == movecursor_termcdb(termcdb, &strbuf, (uint16_t)p1, (uint16_t)p2), "p1=%d p2=%d", p1, p2);
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // TEST movecursor_termcdb: EINVAL
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 0, 1));
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 1000, 1));
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 1, 0));
         TEST(EINVAL == movecursor_termcdb(termcdb, &strbuf, 1, 1000));

         if (p1 <= p2) {
            // TEST setscrollregion_termcdb
            codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%d;%dr", p1, p2);
            TEST(6 <= codelen && codelen < sizeof(buffer2));
            init_memstream(&strbuf, buffer, buffer+codelen);
            TEST(0 == setscrollregion_termcdb(termcdb, &strbuf, (uint16_t)p1, (uint16_t)p2));
            TEST(buffer+codelen == strbuf.next);
            TEST(buffer+codelen == strbuf.end);
            TEST(0 == memcmp(buffer, buffer2, codelen));

            // TEST setscrollregion_termcdb: ENOBUFS
            init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
            TEST(ENOBUFS == setscrollregion_termcdb(termcdb, &strbuf, (uint16_t)p1, (uint16_t)p2));
            TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         } else {
            // TEST setscrollregion_termcdb: EINVAL
            TEST(EINVAL == setscrollregion_termcdb(termcdb, &strbuf, 0, 10));
            TEST(EINVAL == setscrollregion_termcdb(termcdb, &strbuf, 1, 1000));
            TEST(EINVAL == setscrollregion_termcdb(termcdb, &strbuf, (uint16_t)p1, (uint16_t)p2));
         }

      }}

      for (unsigned p1 = 0; p1 <= 1; ++p1) {
      for (unsigned p2 = 0; p2 < termcdb_col_NROFCOLOR; ++p2) {

         unsigned p1_expect = (termcdb->termid == termcdb_id_LINUXCONSOLE) ? 0 : p1;

         // setfgcolor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 90 : 30));
         TEST(5 == codelen);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == setfgcolor_termcdb(termcdb, &strbuf, p1, (uint8_t)p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // setfgcolor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == setfgcolor_termcdb(termcdb, &strbuf, p1, (uint8_t)p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // setfgcolor_termcdb: EINVAL
         TEST(EINVAL == setfgcolor_termcdb(termcdb, &strbuf, p1, termcdb_col_NROFCOLOR));

         // setbgcolor_termcdb
         codelen = (size_t) snprintf(buffer2, sizeof(buffer2), "\x1b[%dm", p2 + (p1_expect ? 100 : 40));
         TEST(5 <= codelen && codelen <= 6);
         init_memstream(&strbuf, buffer, buffer+codelen);
         TEST(0 == setbgcolor_termcdb(termcdb, &strbuf, p1, (uint8_t)p2));
         TEST(buffer+codelen == strbuf.next);
         TEST(buffer+codelen == strbuf.end);
         TEST(0 == memcmp(buffer, buffer2, codelen));

         // setbgcolor_termcdb: ENOBUFS
         init_memstream(&strbuf, zerobuf, zerobuf+codelen-1);
         TEST(ENOBUFS == setbgcolor_termcdb(termcdb, &strbuf, p1, (uint8_t)p2));
         TEST(0 == memcmp(zerobuf, zerobuf2, sizeof(zerobuf))); // not changed

         // setbgcolor_termcdb: EINVAL
         TEST(EINVAL == setbgcolor_termcdb(termcdb, &strbuf, p1, termcdb_col_NROFCOLOR));

      }}

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

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
      // cursor and keypad keys could be configured for application and normal mode
      // in application mode the keypad keys '/','*','-','+','<cr>' generate also special kodes
      // which are not backed up with constants from termcdb_keynr_e.
      // Therefore during init call setnormkeys_termcdb to switch to normal mode
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

      // TEST querykey_termcdb: unshifted keycodes (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testkeycodes); ++tk) {
         termcdb_keynr_e K = testkeycodes[tk].key;
         for (unsigned ci = 0; testkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == querykey_termcdb(termcdb, &keycodes, &key));
            TEST(K == key.nr);
            TEST(0 == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == querykey_termcdb(termcdb, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }

         }
      }

      // TEST querykey_termcdb: linux F13 - F20 (+ ENODATA)
      for (unsigned tk = 0; tk < lengthof(testshiftkeycodes); ++tk) {
         termcdb_keynr_e K = testshiftkeycodes[tk].key;
         for (unsigned ci = 0; testshiftkeycodes[tk].codes[ci]; ++ci) {
            const uint8_t* start    = (const uint8_t*) testshiftkeycodes[tk].codes[ci];
            const uint8_t* end      = start + strlen(testshiftkeycodes[tk].codes[ci]);
            memstream_ro_t keycodes = memstream_INIT(start, end);

            // test OK
            memset(&key, 255, sizeof(key));
            TEST(0 == querykey_termcdb(termcdb, &keycodes, &key));
            TEST(K == key.nr);
            TEST(termcdb_keymod_SHIFT == key.mod);
            TEST(end == keycodes.next);
            TEST(end == keycodes.end);

            // test ENODATA
            for (const uint8_t * end2 = start+1; end2 != end; ++end2) {
               keycodes = (memstream_ro_t) memstream_INIT(start, end2);
               TEST(ENODATA == querykey_termcdb(termcdb, &keycodes, &key));
               TEST(start == keycodes.next);
               TEST(end2  == keycodes.end);
            }
         }
      }

      // TEST querykey_termcdb: shifted keycodes (+ ENODATA)
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
               TEST(0 == querykey_termcdb(termcdb, &keycodes, &key));
               TEST(K == key.nr);
               TEST(mi == key.mod);
               TEST(end == keycodes.next);
               TEST(end == keycodes.end);

               // test ENODATA
               for (uint8_t * end2 = buffer+1; end2 != end; ++end2) {
                  keycodes = (memstream_ro_t) memstream_INIT(buffer, end2);
                  TEST(ENODATA == querykey_termcdb(termcdb, &keycodes, &key));
                  TEST(buffer == keycodes.next);
                  TEST(end2   == keycodes.end);
               }

            }
         }
      }

      // TEST querykey_termcdb: EILSEQ
      // TODO:

      // unprepare
      TEST(0 == delete_termcdb(&termcdb));
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_io_terminal_termcdb()
{
   if (test_initfree())       goto ONERR;
   if (test_controlcodes0())  goto ONERR;
   if (test_controlcodes1())  goto ONERR;
   if (test_controlcodes2())  goto ONERR;
   if (test_keycodes())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
