/* title: TerminalControlDatabase

   Liefert korrekte Control-Codes (ASCII control character sequence)
   für die Ansteuerung eines Terminals abhängig von seinem Typ.

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
#ifndef CKERN_IO_TERMINAL_TERMCDB_HEADER
#define CKERN_IO_TERMINAL_TERMCDB_HEADER

#include "C-kern/api/memory/memstream.h"

/* typedef: struct termcdb_t
 * Export <termcdb_t> into global namespace. */
typedef struct termcdb_t termcdb_t;

/* typedef: struct termcdb_key_t
 * Export <termcdb_key_t> into global namespace. */
typedef struct termcdb_key_t termcdb_key_t;

/* enums: termcdb_col_e
 * Farbwerte, die als Argumente der Funktionen <termcdb_t.setfgcolor_termcdb>
 * und <termcdb_t.setbgcolor_termcdb> Verwendung finden.
 *
 * termcdb_col_BLACK      - Farbe Schwarz.
 * termcdb_col_RED        - Farbe Rot.
 * termcdb_col_GREEN      - Farbe Grün.
 * termcdb_col_YELLOW     - Farbe Gelb.
 * termcdb_col_BLUE       - Farbe Blau.
 * termcdb_col_MAGENTA    - Farbe Magenta (blau und rot).
 * termcdb_col_CYAN       - Farbe Türkis (blau und grün).
 * termcdb_col_WHITE      - Farbe Weiß.
 * termcdb_col_NROFCOLOR  - Anzahl Farben. Die Zahlcodes gehen von 0 bis termcdb_col_NROFCOLOR-1.
 * */
typedef enum termcdb_col_e {
   termcdb_col_BLACK,
   termcdb_col_RED,
   termcdb_col_GREEN,
   termcdb_col_YELLOW,
   termcdb_col_BLUE,
   termcdb_col_MAGENTA,
   termcdb_col_CYAN,
   termcdb_col_WHITE,
   termcdb_col_NROFCOLOR
} termcdb_col_e;

/* enums: termcdb_keymr_e
 * Special Key Number.
 *
 * termcdb_keynr_F1 - F1 Function Key
 * termcdb_keynr_F2 - F2 Function Key
 * termcdb_keynr_F3 - F3 Function Key
 * termcdb_keynr_F4 - F4 Function Key
 * termcdb_keynr_F5 - F5 Function Key
 * termcdb_keynr_F6 - F6 Function Key
 * termcdb_keynr_F7 - F7 Function Key
 * termcdb_keynr_F8 - F8 Function Key
 * termcdb_keynr_F9 - F9 Function Key
 * termcdb_keynr_F10 - F10 Function Key
 * termcdb_keynr_F11 - F11 Function Key
 * termcdb_keynr_F12 - F12 Function Key
 * termcdb_keynr_BS  - Backspace Key (Rücktaste)
 * termcdb_keynr_INS - Insert Key
 * termcdb_keynr_DEL - Delete Key
 * termcdb_keynr_HOME - Home Key (Pos1 Taste)
 * termcdb_keynr_END  - End Key
 * termcdb_keynr_PAGEUP   - Page up Key (Bildauf Taste)
 * termcdb_keynr_PAGEDOWN - Page down Key (Bildrunter Taste)
 * termcdb_keynr_UP       - Up Arrow Key
 * termcdb_keynr_DOWN     - Down Arrow Key
 * termcdb_keynr_LEFT     - Left Arrow Key
 * termcdb_keynr_RIGHT    - Right Arrow Key
 * */
typedef enum termcdb_keynr_e {
   termcdb_keynr_UNKNOWN,
   termcdb_keynr_F1,
   termcdb_keynr_F2,
   termcdb_keynr_F3,
   termcdb_keynr_F4,
   termcdb_keynr_F5,
   termcdb_keynr_F6,
   termcdb_keynr_F7,
   termcdb_keynr_F8,
   termcdb_keynr_F9,
   termcdb_keynr_F10,
   termcdb_keynr_F11,
   termcdb_keynr_F12,
   termcdb_keynr_BS,
   termcdb_keynr_HOME, // POS1
   termcdb_keynr_INS,
   termcdb_keynr_DEL,
   termcdb_keynr_END,
   termcdb_keynr_PAGEUP,
   termcdb_keynr_PAGEDOWN,
   termcdb_keynr_UP,
   termcdb_keynr_DOWN,
   termcdb_keynr_RIGHT,
   termcdb_keynr_LEFT,
   termcdb_keynr_CENTER, // center of keypad (5)

} termcdb_keynr_e;

/* enums: termcdb_keymod_e
 * Modifier Key Bit. */
typedef enum termcdb_keymod__e {
   termcdb_keymod_NONE,
   termcdb_keymod_SHIFT = 2,
   termcdb_keymod_CTRL  = 4,
   termcdb_keymod_ALT   = 8,
   termcdb_keymod_META  = 16
} termcdb_keymod_e;

/* enums: termcdb_id_e
 * TODO:
 * termcdb_id_LINUXCONSOLE - "linux" Terminal.
 * termcdb_id_XTERM        - "xterm" Terminal. */
typedef enum termcdb_id_e {
   termcdb_id_LINUXCONSOLE,
   termcdb_id_XTERM
} termcdb_id_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_termcdb
 * Test <termcdb_t> functionality. */
int unittest_io_terminal_termcdb(void);
#endif


/* struct: termcdb_key_t
 * Definiert eine von <querykey_termcdb> erkannte Spezialtaste. */
struct termcdb_key_t {
   // group: public fields
   /* variable: nr
    * Nummer der Spezialtaste - siehe <termcdb_keynr_e>. */
   termcdb_keynr_e   nr;
   /* variable: mod
    * Bitkombination der Zusatztasten (modifier) - siehe <termcdb_keymod_e>. */
   termcdb_keymod_e  mod;
};

// group: lifetime

/* define: termcdb_key_INIT
 * Statischer Initialisierer. */
#define termcdb_key_INIT(nr, mod) \
         { nr, mod }


/* struct: termcdb_t
 * TODO: describe type
 *
 * Einschränkung:
 * Es werden nur Terminals des Typs "xterm" und "linux" unterstützt.
 *
 * Spalten(x) und Zeilen(y):
 * Die X Werte bezeichnen die Spalten,
 * die Y Werte die Zeilen, beide beginnend mit 1.
 * Die linke obere Ecke des Terminals wird mit den Werten (1,1)
 * adressiert. Der Parameter screensizey gibt die Anzahl Zeilen (nr of rows)
 * des Terminals an.
 * */
struct termcdb_t {
   /* variable: termid
    * Interne nummer des Terminals beginnend von 0. */
   const uint16_t  termid;
   /* variable: typelist
    * Liste von Typnamen, unter denen das Terminal bekannt ist, jeweils getrennt mit '|'.
    * Die Liste ist mit einem \0 Byte abgeschlossen. */
   const uint8_t * typelist; // "name1|name2|...|nameN\0"
};

// group: lifetime

/* function: new_termcdb
 * TODO: Describe Initializes object.
 *
 * Einschränkung:
 * Es werden nur Terminals des Typs "xterm" und "linux" unterstützt
 * Alternative Namen sind: "xterm-debian", "X11 terminal emulator", "linux console". */
int new_termcdb(/*out*/termcdb_t ** termcdb, const termcdb_id_e termid);

/* function: newfromtype_termcdb
 * TODO: Describe Initializes object.
 *
 * Einschränkung:
 * Es werden nur Terminals des Typs "xterm" und "linux" unterstützt
 * Alternative Namen sind: "xterm-debian", "X11 terminal emulator", "linux console". */
int newfromtype_termcdb(/*out*/termcdb_t ** termcdb, const uint8_t * type);

/* function: delete_termcdb
 * TODO: Describe Frees all associated resources. */
int delete_termcdb(termcdb_t ** termcdb);

// group: control-codes

// TODO: add set replace mode
// TODO: set keyboard application mode

int startedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int endedit_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

/* function: setkeypadnormal_termcdb
 * TODO: German
 * cursor and keypad keys could be configured for application and normal mode
 * in application mode the keypad keys '/','*','-','+','<cr>' generate also special kodes
 * which are not backed up with constants from termcdb_keynr_e.
 * Therefore during init call setkeypadnormal_termcdb to switch to normal mode. */
int setkeypadnormal_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int movecursor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t cursorx, uint16_t cursory);

int clearline_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

// cursor at 1, 1
int clearscreen_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int cursoroff_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int cursoron_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int setbold_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

int setfgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, uint8_t fgcolor);

int setbgcolor_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, bool bright, uint8_t bgcolor);

int resetstyle_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

// cursor undefined
int setscrollregion_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t starty, uint16_t endy);

// cursor undefined
int resetscrollregion_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes);

// cursor undefined
int deletelines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t nroflines);

// cursor undefined
int insertlines_termcdb(const termcdb_t * termcdb, /*ret*/memstream_t * ctrlcodes, uint16_t nroflines);

// group: keycodes

/* function: querykey_termcdb
 * Returns:
 * ENODATA - Zu wenig Bytes in keycodes. Weder keycodes noch key wurde verändert.
 * EILSEQ  - Der Tastenkode beginnend von keycodes->next ist nicht bekannt.
 * 0       - Die Spezialtaste wurde erkannt. keycodes->next wurde iunkrementiert und key enthält die
 *           Nummer der erkannten Taste.
 *
 * Es wird *kein* Logeintrag im Falle eines Fehlers geschrieben. */
int querykey_termcdb(const termcdb_t * termcdb, memstream_t * keycodes, /*out*/termcdb_key_t * key);

// group: update
// TODO: remove group



// section: inline implementation

/* define: delete_termcdb
 * Implements <termcdb_t.delete_termcdb>. */
#define delete_termcdb(termcdb) \
         (*(termcdb) = 0)


#endif
