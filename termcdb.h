/* title: TerminalCapabilityDatabase

   Liefert korrekte Control-Codes (ASCII control character sequence)
   für die Ansteuerung eines Terminals abhängig von seinem Typ.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/termcdb.h
    Header file <TerminalCapabilityDatabase>.

   file: C-kern/io/terminal/termcdb.c
    Implementation file <TerminalCapabilityDatabase impl>.
*/
#ifndef CKERN_IO_TERMINAL_TERMCDB_HEADER
#define CKERN_IO_TERMINAL_TERMCDB_HEADER

// forward
struct memstream_t;
struct memstream_ro_t;

/* typedef: struct termcdb_t
 * Export <termcdb_t> into global namespace. */
typedef struct termcdb_t termcdb_t;

/* typedef: struct termcdb_key_t
 * Export <termcdb_key_t> into global namespace. */
typedef struct termcdb_key_t termcdb_key_t;

/* enums: termcdb_col_e
 * Farbwerte, die als Argumente der Funktionen <termcdb_t.fgcolor_termcdb>
 * und <termcdb_t.bgcolor_termcdb> Verwendung finden.
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

/* enums: termcdb_keynr_e
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
 * termcdb_keynr_CENTER,  - Mittlere Keypad Taste. Im Number-Modus ist das die Taste '5'.
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
   termcdb_keynr_BS,       // Backspace
   termcdb_keynr_HOME,     // Pos1 (German)
   termcdb_keynr_INS,
   termcdb_keynr_DEL,
   termcdb_keynr_END,
   termcdb_keynr_PAGEUP,
   termcdb_keynr_PAGEDOWN,
   termcdb_keynr_UP,
   termcdb_keynr_DOWN,
   termcdb_keynr_RIGHT,
   termcdb_keynr_LEFT,
   termcdb_keynr_CENTER,   // center of keypad ('5')
} termcdb_keynr_e;

/* enums: termcdb_keymod_e
 * Bits, welche die gedrückten Zusatztasten (modifier keys) kodieren.
 * Generell wird es nur von xterm unterstützt. Die Linuxconsole unterstützt
 * nur die Tasten Shift-F1 bis Shift-F8.
 * termcdb_keymod_NONE  - Keine Zusatztasten gedrückt.
 * termcdb_keymod_SHIFT - Es wurde zusätzlich die Shift-Taste gedrückt.
 * termcdb_keymod_ALT   - Es wurde zusätzlich die Alt-Taste gedrückt.
 * termcdb_keymod_CTRL  - Es wurde zusätzlich die Ctrl-Taste (Strg - Steuerung auf Deutsch) gedrückt.
 * termcdb_keymod_META  - Es wurde zusätzlich die Meta-Taste (sofern vorhanden) gedrückt.
 * * */
typedef enum termcdb_keymod_e {
   termcdb_keymod_NONE,
   termcdb_keymod_SHIFT = 1,
   termcdb_keymod_ALT   = 2,
   termcdb_keymod_CTRL  = 4,
   termcdb_keymod_META  = 8
} termcdb_keymod_e;

/* enums: termcdb_id_e
 * Liste der unterstützten Terminaltypen.
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
 * Definiert eine von <key_termcdb> erkannte Spezialtaste. */
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
 * Beschreibt Terminaltyp und implementiert typabhängige Ansteuerung der Textausgabe und Abfrage der Tastatur.
 *
 * Einschränkung:
 * Es werden nur Terminals aus der Liste <termcdb_id_e> unterstützt.
 *
 * Spalten(x) und Zeilen(y):
 * Die X Werte bezeichnen die Spalten,
 * die Y Werte die Zeilen, beide beginnend mit 0.
 * Die linke obere Ecke des Terminals wird mit den Werten (0,0)
 * adressiert.
 * */
struct termcdb_t {
   /* variable: termid
    * Interne Nummer des Terminals beginnend von 0. */
   const uint16_t  termid;
   /* variable: typelist
    * Liste von Typnamen, unter denen das Terminal bekannt ist, jeweils getrennt mit '|'.
    * Die Liste ist mit einem \0 Byte abgeschlossen. */
   const uint8_t * typelist; // "name1|name2|...|nameN\0"
};

// group: lifetime

/* function: new_termcdb
 * Allokiert statisch ein <termcdb_t> und gibt in termcdb einen Zeiger darauf zurück.
 * Es werden nur Terminals aus <termcdb_id_e> unterstützt. */
int new_termcdb(/*out*/termcdb_t ** termcdb, const termcdb_id_e termid);

/* function: newfromtype_termcdb
 * Wie <new_termcdb>, erwartet in type aber Rückgabewert von <type_terminal>.
 *
 * Einschränkung:
 * Es werden nur die Typnamen "xterm" und "linux" unterstützt bzw.
 * die Alternativen: "xterm-debian", "X11 terminal emulator" und "linux console". */
int newfromtype_termcdb(/*out*/termcdb_t ** termcdb, const uint8_t * type);

/* function: delete_termcdb
 * Setzt termcdb auf 0. */
int delete_termcdb(termcdb_t ** termcdb);

// group: query

/* function: id_termcdb
 * Gibt die interne ID des Terminals zurück - siehe <termcdb_id_e>. */
termcdb_id_e id_termcdb(const termcdb_t * termcdb);

// group: write control-codes

// TODO: add replace/insert mode

/* function: startedit_termcdb
 *
 * Initialisiert 2 Dinge:
 * - Speichert Zustand und schaltet - wenn möglich - auf einen Alternativschirm.
 * - Schaltet Replace-Modus an.
 * - Schaltet Linewrap aus (am Ende der Zeile werden die Zeichen abgeschnitten anstatt in die nächste Zeile zu gehen).
 * - Cursor- and Keypadtasten werden in den Modus »normal« geschalten.
 *   Der Modus »application« ist die Alternative, bei dem die Keypadtasten
 *   sich durch alterniv gesendete Codes unterscheiden. Im Normalmodus
 *   senden die Keypadtasten '/','*','-','+','<cr>' die aufgedruckten Symbole,
 *   im Appmode Escapesequenzen zur Unterscheidung. */
int startedit_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: endedit_termcdb
 * TODO: */
int endedit_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: movecursor_termcdb
 * TODO: */
int movecursor_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, unsigned cursorx, unsigned cursory);

/* function: clearline_termcdb
 * TODO: */
int clearline_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearendofline_termcdb
 * Löscht von der aktuellen Cursorposition bis zum Zeilenende den Inhalt. */
int clearendofline_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearscreen_termcdb
 * cursor at 0, 0
 * TODO: */
int clearscreen_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: cursoroff_termcdb
 * TODO: */
int cursoroff_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: cursoron_termcdb
 * TODO: */
int cursoron_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: bold_termcdb
 * TODO: */
int bold_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: fgcolor_termcdb
 * TODO: */
int fgcolor_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned fgcolor);

/* function: bgcolor_termcdb
 * TODO: */
int bgcolor_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned bgcolor);

/* function: normtext_termcdb
 * TODO: */
int normtext_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrollregion_termcdb
 * cursor undefined
 * TODO: */
int scrollregion_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, unsigned starty, unsigned endy);

/* function: scrollregionoff_termcdb
 * cursor undefined
 * TODO: */
int scrollregionoff_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrollup_termcdb
 * Scrollt eine Zeile nach oben, sofern der Cursor in der letzten Zeile steht.
 * Mit der letzen Zeile ist die Höhe-1 des Terminals in Zeilen gemeint oder endy,
 * falls <scrollregion_termcdb> angeschalten ist. */
int scrollup_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrolldown_termcdb
 * Scrollt eine Zeile nach unten, sofern der Cursor in der ersten Zeile steht.
 * Mit der ersten Zeile ist 0 gemeint oder starty, falls <scrollregion_termcdb> angeschalten ist. */
int scrolldown_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: delchar_termcdb
 * Löscht das Zeichen, auf dem der Cursor steht. Zeichen rechts davon werden nach links geschoben.
 * Am rechten Bildschirmrand erscheint eine Leerstelle. */
int delchar_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes);

/* function: dellines_termcdb
 * cursor undefined
 * TODO: */
int dellines_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, unsigned  nroflines);

/* function: inslines_termcdb
 * cursor undefined
 * TODO: */
int inslines_termcdb(const termcdb_t * termcdb, /*ret*/struct memstream_t * ctrlcodes, unsigned  nroflines);

// group: read keycodes

/* function: key_termcdb
 * Gibt in key die Beschreibung der gedrückten Spezialtaste zurück.
 * Parameter keycodes referenziert den von der Tastatur gelesenen Bytestrom.
 *
 * Returns:
 * ENODATA - Zu wenig Bytes in keycodes. Weder keycodes noch key wurde verändert.
 * EILSEQ  - Der Tastenkode beginnend von keycodes->next ist nicht bekannt.
 *           Die Funktion muss mit (keycodes->next+1) wieder aufgerufen werden.
 * 0       - Die Spezialtaste wurde erkannt. keycodes->next wurde um die Anzahl an Bytes inkrementiert,
 *           welche die Spezialtaste beschreibenm, und key enthält deren Beschreibung.
 *
 * Es wird *kein* Logeintrag im Falle eines Fehlers geschrieben. */
int key_termcdb(const termcdb_t * termcdb, struct memstream_ro_t * keycodes, /*out*/termcdb_key_t * key);



// section: inline implementation

/* define: delete_termcdb
 * Implements <termcdb_t.delete_termcdb>. */
#define delete_termcdb(termcdb) \
         (*(termcdb) = 0)

/* define: id_termcdb
 * Implements <termcdb_t.id_termcdb>. */
#define id_termcdb(termcdb) \
         ((termcdb)->termid)


#endif
