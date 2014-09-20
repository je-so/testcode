/* title: TerminalAdapter

   Liefert korrekte Control-Codes (ASCII control character sequence)
   für die Ansteuerung eines Terminals abhängig von seinem Typ und
   ordnet gelesene Control-Codes den Spezialtasten wie Cursorsteuerung
   zu.


   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/termadapt.h
    Header file <TerminalAdapter>.

   file: C-kern/io/terminal/termadapt.c
    Implementation file <TerminalAdapter impl>.
*/
#ifndef CKERN_IO_TERMINAL_TERMADAPT_HEADER
#define CKERN_IO_TERMINAL_TERMADAPT_HEADER

// forward
struct memstream_t;
struct memstream_ro_t;

/* typedef: struct termadapt_t
 * Export <termadapt_t> into global namespace. */
typedef struct termadapt_t termadapt_t;

/* typedef: struct termkey_t
 * Export <termkey_t> into global namespace. */
typedef struct termkey_t termkey_t;

/* enums: termcol_e
 * Farbwerte, die als Argumente der Funktionen <termadapt_t.fgcolor_termadapt>
 * und <termadapt_t.bgcolor_termadapt> Verwendung finden.
 *
 * termcol_BLACK      - Farbe Schwarz.
 * termcol_RED        - Farbe Rot.
 * termcol_GREEN      - Farbe Grün.
 * termcol_YELLOW     - Farbe Gelb.
 * termcol_BLUE       - Farbe Blau.
 * termcol_MAGENTA    - Farbe Magenta (blau und rot).
 * termcol_CYAN       - Farbe Türkis (blau und grün).
 * termcol_WHITE      - Farbe Weiß.
 * termcol_NROFCOLOR  - Anzahl Farben. Die Zahlcodes gehen von 0 bis termcol_NROFCOLOR-1.
 * */
typedef enum termcol_e {
   termcol_BLACK,
   termcol_RED,
   termcol_GREEN,
   termcol_YELLOW,
   termcol_BLUE,
   termcol_MAGENTA,
   termcol_CYAN,
   termcol_WHITE,
   termcol_NROFCOLOR
} termcol_e;

/* enums: termkey_e
 * Special Key Number.
 *
 * termkey_F1 - F1 Function Key
 * termkey_F2 - F2 Function Key
 * termkey_F3 - F3 Function Key
 * termkey_F4 - F4 Function Key
 * termkey_F5 - F5 Function Key
 * termkey_F6 - F6 Function Key
 * termkey_F7 - F7 Function Key
 * termkey_F8 - F8 Function Key
 * termkey_F9 - F9 Function Key
 * termkey_F10 - F10 Function Key
 * termkey_F11 - F11 Function Key
 * termkey_F12 - F12 Function Key
 * termkey_BS  - Backspace Key (Rücktaste)
 * termkey_INS - Insert Key
 * termkey_DEL - Delete Key
 * termkey_HOME - Home Key (Pos1 Taste)
 * termkey_END  - End Key
 * termkey_PAGEUP   - Page up Key (Bildauf Taste)
 * termkey_PAGEDOWN - Page down Key (Bildrunter Taste)
 * termkey_UP       - Up Arrow Key
 * termkey_DOWN     - Down Arrow Key
 * termkey_LEFT     - Left Arrow Key
 * termkey_RIGHT    - Right Arrow Key
 * termkey_CENTER,  - Mittlere Keypad Taste. Im Number-Modus ist das die Taste '5'.
 * */
typedef enum termkey_e {
   termkey_UNKNOWN,
   termkey_F1,
   termkey_F2,
   termkey_F3,
   termkey_F4,
   termkey_F5,
   termkey_F6,
   termkey_F7,
   termkey_F8,
   termkey_F9,
   termkey_F10,
   termkey_F11,
   termkey_F12,
   termkey_BS,       // Backspace
   termkey_HOME,     // Pos1 (German)
   termkey_INS,
   termkey_DEL,
   termkey_END,
   termkey_PAGEUP,
   termkey_PAGEDOWN,
   termkey_UP,
   termkey_DOWN,
   termkey_RIGHT,
   termkey_LEFT,
   termkey_CENTER,   // center of keypad ('5')
} termkey_e;

/* enums: termmodkey_e
 * Bits, welche die gedrückten Zusatztasten (modifier keys) kodieren.
 * Generell wird es nur von xterm unterstützt. Die Linuxconsole unterstützt
 * nur die Tasten Shift-F1 bis Shift-F8.
 * termmodkey_NONE  - Keine Zusatztasten gedrückt.
 * termmodkey_SHIFT - Es wurde zusätzlich die Shift-Taste gedrückt.
 * termmodkey_ALT   - Es wurde zusätzlich die Alt-Taste gedrückt.
 * termmodkey_CTRL  - Es wurde zusätzlich die Ctrl-Taste (Strg - Steuerung auf Deutsch) gedrückt.
 * termmodkey_META  - Es wurde zusätzlich die Meta-Taste (sofern vorhanden) gedrückt.
 * termmodkey_MASK  - Alle Bits der einzelnen Werte vereinigt.
 * * */
typedef enum termmodkey_e {
   termmodkey_NONE,
   termmodkey_SHIFT = 1,
   termmodkey_ALT   = 2,
   termmodkey_CTRL  = 4,
   termmodkey_META  = 8,

   termmodkey_MASK  = 15
} termmodkey_e;

/* enums: termid_e
 * Liste der unterstützten Terminaltypen.
 * termid_LINUXCONSOLE - "linux" Terminal.
 * termid_XTERM        - "xterm" Terminal. */
typedef enum termid_e {
   termid_LINUXCONSOLE,
   termid_XTERM
} termid_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_termadapt
 * Test <termadapt_t> functionality. */
int unittest_io_terminal_termadapt(void);
#endif


/* struct: termkey_t
 * Definiert eine von <key_termadapt> erkannte Spezialtaste. */
struct termkey_t {
   // group: public fields
   /* variable: nr
    * Nummer der Spezialtaste - siehe <termkey_e>. */
   termkey_e     nr;
   /* variable: mod
    * Bitkombination der Zusatztasten (modifier) - siehe <termmodkey_e>. */
   termmodkey_e  mod;
};

// group: lifetime

/* define: termkey_INIT
 * Statischer Initialisierer. */
#define termkey_INIT(nr, mod) \
         { nr, mod }


/* struct: termadapt_t
 * Beschreibt Terminaltyp und implementiert typabhängige Ansteuerung der Textausgabe und Abfrage der Tastatur.
 *
 * Einschränkung:
 * Es werden nur Terminals aus der Liste <termid_e> unterstützt.
 *
 * Spalten(x) und Zeilen(y):
 * Die X Werte bezeichnen die Spalten,
 * die Y Werte die Zeilen, beide beginnend mit 0.
 * Die linke obere Ecke des Terminals wird folglich
 * mit den Werten (0,0) adressiert.
 * */
struct termadapt_t {
   /* variable: termid
    * Interne Nummer des Terminals beginnend von 0. */
   const uint16_t  termid;
   /* variable: typelist
    * Liste von Typnamen, unter denen das Terminal bekannt ist, jeweils getrennt mit '|'.
    * Die Liste ist mit einem \0 Byte abgeschlossen. */
   const uint8_t * typelist; // "name1|name2|...|nameN\0"
};

// group: lifetime

/* function: new_termadapt
 * Allokiert statisch ein <termadapt_t> und gibt in termadapt einen Zeiger darauf zurück.
 * Es werden nur Terminals aus <termid_e> unterstützt. */
int new_termadapt(/*out*/termadapt_t ** termadapt, const termid_e termid);

/* function: newfromtype_termadapt
 * Wie <new_termadapt>, erwartet in type aber Rückgabewert von <type_terminal>.
 *
 * Einschränkung:
 * Es werden nur die Typnamen "xterm" und "linux" unterstützt bzw.
 * die Alternativen: "xterm-debian", "X11 terminal emulator" und "linux console". */
int newfromtype_termadapt(/*out*/termadapt_t ** termadapt, const uint8_t * type);

/* function: delete_termadapt
 * Setzt termadapt auf 0. */
int delete_termadapt(termadapt_t ** termadapt);

// group: query

/* function: id_termadapt
 * Gibt die interne ID des Terminals zurück - siehe <termid_e>. */
termid_e id_termadapt(const termadapt_t * termadapt);

// group: write control-codes

// TODO: add replace/insert mode

/* function: startedit_termadapt
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
int startedit_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: endedit_termadapt
 * TODO: */
int endedit_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: movecursor_termadapt
 * TODO: */
int movecursor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned cursorx, unsigned cursory);

/* function: clearline_termadapt
 * TODO: */
int clearline_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearendofline_termadapt
 * Löscht von der aktuellen Cursorposition bis zum Zeilenende den Inhalt. */
int clearendofline_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearscreen_termadapt
 * cursor at 0, 0
 * TODO: */
int clearscreen_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: cursoroff_termadapt
 * TODO: */
int cursoroff_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: cursoron_termadapt
 * TODO: */
int cursoron_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: bold_termadapt
 * TODO: */
int bold_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: fgcolor_termadapt
 * TODO: */
int fgcolor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned fgcolor);

/* function: bgcolor_termadapt
 * TODO: */
int bgcolor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned bgcolor);

/* function: normtext_termadapt
 * TODO: */
int normtext_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrollregion_termadapt
 * cursor undefined
 * TODO: */
int scrollregion_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned starty, unsigned endy);

/* function: scrollregionoff_termadapt
 * cursor undefined
 * TODO: */
int scrollregionoff_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrollup_termadapt
 * Scrollt eine Zeile nach oben, sofern der Cursor in der letzten Zeile steht.
 * Mit der letzen Zeile ist die Höhe-1 des Terminals in Zeilen gemeint oder endy,
 * falls <scrollregion_termadapt> angeschalten ist. */
int scrollup_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: scrolldown_termadapt
 * Scrollt eine Zeile nach unten, sofern der Cursor in der ersten Zeile steht.
 * Mit der ersten Zeile ist 0 gemeint oder starty, falls <scrollregion_termadapt> angeschalten ist. */
int scrolldown_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: delchar_termadapt
 * Löscht das Zeichen, auf dem der Cursor steht. Zeichen rechts davon werden nach links geschoben.
 * Am rechten Bildschirmrand erscheint eine Leerstelle. */
int delchar_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: dellines_termadapt
 * cursor undefined
 * TODO: */
int dellines_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned  nroflines);

/* function: inslines_termadapt
 * cursor undefined
 * TODO: */
int inslines_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned  nroflines);

// group: read keycodes

/* function: key_termadapt
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
int key_termadapt(const termadapt_t * termadapt, struct memstream_ro_t * keycodes, /*out*/termkey_t * key);



// section: inline implementation

/* define: delete_termadapt
 * Implements <termadapt_t.delete_termadapt>. */
#define delete_termadapt(termadapt) \
         (*(termadapt) = 0)

/* define: id_termadapt
 * Implements <termadapt_t.id_termadapt>. */
#define id_termadapt(termadapt) \
         ((termadapt)->termid)


#endif
