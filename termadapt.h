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

/* enums: term_modkey_e
 * Bits, welche die gedrückten Zusatztasten (modifier keys) kodieren.
 * Generell wird es nur von xterm unterstützt. Die Linuxconsole unterstützt
 * nur die Tasten Shift-F1 bis Shift-F8.
 * term_modkey_NONE  - Keine Zusatztasten gedrückt.
 * term_modkey_SHIFT - Es wurde zusätzlich die Shift-Taste gedrückt.
 * term_modkey_ALT   - Es wurde zusätzlich die Alt-Taste gedrückt.
 * term_modkey_CTRL  - Es wurde zusätzlich die Ctrl-Taste (Strg - Steuerung auf Deutsch) gedrückt.
 * term_modkey_META  - Es wurde zusätzlich die Meta-Taste (sofern vorhanden) gedrückt.
 * term_modkey_MASK  - Alle Bits der einzelnen Werte vereinigt.
 * * */
typedef enum term_modkey_e {
   term_modkey_NONE,
   term_modkey_SHIFT = 1,
   term_modkey_ALT   = 2,
   term_modkey_CTRL  = 4,
   term_modkey_META  = 8,

   term_modkey_MASK  = 15
} term_modkey_e;

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
    * Bitkombination der Zusatztasten (modifier) - siehe <term_modkey_e>. */
   term_modkey_e  mod;
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
 *
 * Steueranweisungen (»write control-codes«):
 * Alle Funktionen wie <movecursor_termadapt> erzeugen vom Terminaltyp abhängige Steueranweisungen
 * (Escapesequenzen) und schreiben diese in den durch ctrlcodes referenzierten Speicher.
 * ctrlcodes ist vom Type <memstream_t> und <memstream_t.next> wird um die Länge der Steueranweisung erhöht.
 *
 * Sollte in ctrlcodes nicht genügend Platz vorhanden sein, liefern alle Funktionen
 * die Fehlermeldung ENOBUFS zurück, ansonsten 0 for OK.
 *
 * Zur Ausführung der Steueranweisungen sind diese in den Terminal Eingabekanal zu schreiben.
 * Es ist aber sinnvoll, mehrere Anweisungen im Speicher zusammenzufügen und diese auf einmal
 * an das Terminal zu übertragen.
 *
 * Maskieren von Controlcodes:
 * Steueranweisungen beginnen in der Regel mit dem Escape-Controlzeichen (Zeichencode 27 bzw. "\x1b").
 * Bei der Ausgabe von UTF-8 Zeichen ist darauf zu achten, daß alle Steuerzeichen maskiert werden
 * in den Zeichencodebereichen:
 * 0..31    - C0 Steuerzeichen (Siehe auch http://www.unicode.org/charts/PDF/U0000.pdf)
 * 127      - Delete (DEL) Zeichen (Siehe auch http://www.unicode.org/charts/PDF/U0000.pdf)
 * 128..159 - C1 Steuerzeichen (Siehe auch http://www.unicode.org/charts/PDF/U0080.pdf).
 *            Ab 160 gehen wieder die UTF-8 Nichsteuerzeichen los mit NBSP (No-Break Space), auch als
 *            nicht umgebrochenes Leerzeichen bekannt.
 *
 * Darstellung von Controlcodes:
 * Im Terminal werden alle Steuerzeichen von 0 bis 31 als ^A bis ^_ Dargestellt.
 * ^ ist das Zeichen für die Control bzw. Strg (Steuerungs-) Taste.
 * Siehe auch http://en.wikipedia.org/wiki/C0_and_C1_control_codes für eine Übersicht.
 *
 * Start und Ende:
 * Bevor irgendwelche Steuerzeichen gesendet werden, sollten die von <startedit_termadapt>
 * zurückgegebenen Steuerzeichen gesendet werden. Am Ende der Terminalansteuerung sollten
 * die von <endedit_termadapt> zurückgegebenen Steuerzeichen gesendet werden, um das Terminal
 * wieder zurückzustellen.
 *
 * Abfrage deer Tastatur:
 * Vom Terminal gelesene Bytes beschreiben entweder UTF-8 kodierte (sofern als UTF-8 konfiguriert)
 * Zeichen oder zumeist beginnend mit Escape eine Sondertaste wie Pfeiltasten für Cursorsteuerung,
 * Bild-auf und -ab Tasten usw. Alle unterstützten Sondertasten sind in <termkey_e> gelistet.
 * Um zwischen normalen Zeichenfolgen und eine Sondertaste beschreibene Zeichenfolge unterscheiden
 * zu können, wird fie Funktion <key_termadapt> aufgerufen.
 *
 * Über den Rückgabewert EILSEQ weiss der Aufrufer, daß das nächste Byte ein normales ASCII-Zeichen
 * oder das Startbyte eines UTF-8 kodierten Zeichens ist.
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
 * Es werden nur Terminals aus <termid_e> unterstützt.
 *
 * Return:
 * 0      - In termadapt wurde erfolgreich ein Zeicher auf ein initialisiertes <termadapt_t> Objekt abgelegt.
 * EINVAL - Die Terminal ID in termid ist ein Wert ausserhalb der Liste <termid_e>. */
int new_termadapt(/*out*/termadapt_t ** termadapt, const termid_e termid);

/* function: newPtype_termadapt
 * Wie <new_termadapt>, erwartet in type aber Rückgabewert von <type_terminal>.
 *
 * Einschränkung:
 * Es werden nur die Typnamen "xterm" und "linux" unterstützt bzw.
 * die Alternativen: "xterm-debian", "X11 terminal emulator" und "linux console".
 *
 * Return:
 * 0      - In termadapt wurde erfolgreich ein Zeicher auf ein initialisiertes <termadapt_t> Objekt abgelegt.
 * ENOENT - Der durch den String type beschriebene Terminaltyp wird nicht unterstützt.
 *          Dieser Fehler wird nicht geloggt.
 * */
int newPtype_termadapt(/*out*/termadapt_t ** termadapt, const uint8_t * type);

/* function: delete_termadapt
 * Setzt termadapt auf 0. */
int delete_termadapt(termadapt_t ** termadapt);

// group: query

/* function: id_termadapt
 * Gibt die interne ID des Terminals zurück - siehe <termid_e>. */
termid_e id_termadapt(const termadapt_t * termadapt);

// group: write control-codes

// ======= START + END =======

/* function: startedit_termadapt
 * Startet den Editiermodus - schaltet auf Alternativschirm, wenn untertützt.
 *
 * Initialisiert wird Folgendes:
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
 * Beendet Editiermodus und stellt ovrherigen Zustand – soweit möglich – wieder her.
 * Diese Steuerbefehle sollten immer vor Ende des Prozesses, sofern zuvor <startedit_termadapt>
 * aufgerufen wurde, an das Terminal gesendet werden, ansonsten könnte die Shelleingabe nicht mehr
 * richtig funktionieren. */
int endedit_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

// ======= Cursor =======

/* function: cursoroff_termadapt
 * Schaltet den Cursor unsichtbar. */
int cursoroff_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: cursoron_termadapt
 * Schaltet den Cursor wieder auf sichtbar. */
int cursoron_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: movecursor_termadapt
 * Bewegt den Cursor in die Spalte cursorx und Zeile cursory.
 * Die Werte (0,0) bezeichnen die linke obere Ecke. */
int movecursor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned cursorx, unsigned cursory);

// ======= Clear =======

/* function: clearline_termadapt
 * Löscht die Zeile, auf der sich der Cursor befindet. */
int clearline_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearendofline_termadapt
 * Löscht den Inhalt von der aktuellen Cursorposition bis zum Zeilenende. */
int clearendofline_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: clearscreen_termadapt
 * Löscht den gesamten Terminalschirm und setzt den Cursor auf Position (0,0) | die linke obere Ecke. */
int clearscreen_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

// ======= Text Style =======

/* function: bold_termadapt
 * Schaltet den Text auf fett gedruckte Darstellung. */
int bold_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: fgcolor_termadapt
 * Setzt die Vordergrundfarbe des Textes auf fgcolor.
 * Die Farbe kann aus der Palette <termcol_e> gewählt werden.
 * Ist bright auf true gesetzt, werden hellere Farben verwendet (die zweite Palette),
 * sofern das Terminal es unterstützt. */
int fgcolor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned fgcolor);

/* function: bgcolor_termadapt
 * Setzt die Hintergrundfarbe des Textes auf bgcolor.
 * Die Farbe kann aus der Palette <termcol_e> gewählt werden.
 * Ist bright auf true gesetzt, werden hellere Farben verwendet (die zweite Palette),
 * sofern das Terminal es unterstützt. */
int bgcolor_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, bool bright, unsigned bgcolor);

/* function: normtext_termadapt
 * Setzt den Text auf normale Breite und Standard Vordergrund und Hintergrundfarbe. */
int normtext_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

// ======= Scroll =======

/* function: scrollregion_termadapt
 * Setzt die Scrollregion von Zeile starty bis einschließlich Zeile endy.
 * Die Zeilennummern werden ab 0 gezählt. Damit kann eine Statuszeile am oberen
 * oder unteren Ende des Terminalschirms realisiert werden. */
int scrollregion_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned starty, unsigned endy);

/* function: scrollregionoff_termadapt
 * Setzt den gesamten Schirm als Scrollregion. */
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

// ======= Insert / Delete =======

/* function: replacemode_termadapt
 * Schaltet Replacemodus ein (insert mode: OFF).
 * Ein ausgegebenes Zeichen überschreibt das unter dem Cursor liegende und setzt den Cursor eine
 * Spalte weiter nach rechts. */
int replacemode_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: insertmode_termadapt
 * Schaltet Insertmodus ein (insert mode: ON).
 * Ein ausgegebenes Zeichen verschiebt alle Zeichen beginnend von dem unter dem Cursor stehenden
 * nach rechts, fügt an der aktuellen Cursorposition ein neues ein und setzt den Cursor eine
 * Spalte weiter nach rechts. Ein am Ende der Zeile nach rechts geschobenes Zeichen wird abgeschnitten.
 * Das Abschneiden wird in <startedit_termadapt> gesetzt. */
int insertmode_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: delchar_termadapt
 * Löscht das Zeichen, auf dem der Cursor steht. Zeichen rechts davon werden nach links geschoben.
 * Am rechten Bildschirmrand erscheint eine Leerstelle. */
int delchar_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes);

/* function: dellines_termadapt
 * Löscht nroflines Zeilen. Der Cursor bleibt auf derselben Stelle stehen.
 * Die Zeile, auf der der Cursor steht und nroflines-1 darunterliegende werden gelöscht und
 * alle darunterliegenden Zeilen werden nach oben gescrollt. Nachrückende Zeilen sind leer
 * (oder falls weitere Terminals unterstützt werden könnten auch alte, herausgescrollte
 * Inhalte beinhalten). <scrollregion_termadapt> bestimmt dabei den Scrollbereich. */
int dellines_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned nroflines);

/* function: inslines_termadapt
 * Fügt nroflines neue Leerzeilen ein. Der Cursor bleibt auf derselben Stelle stehen.
 * Die Zeile, auf der der Cursor steht und alle darunterliegenden werden nroflines Zeilen nach untern gescrollt.
 * <scrollregion_termadapt> bestimmt dabei den Scrollbereich. */
int inslines_termadapt(const termadapt_t * termadapt, /*ret*/struct memstream_t * ctrlcodes, unsigned nroflines);

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
