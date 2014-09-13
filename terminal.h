/* title: Terminal

   Erlaubt die Konfiguration des Terminalmodus
   und die Abfrage von Informationen wie die Anzahl der
   Zeilen und Zeichen pro Zeile.

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

   file: C-kern/api/io/terminal/terminal.h
    Header file <Terminal>.

   file: C-kern/platform/Linux/io/terminal.c
    Implementation file <Terminal impl>.
*/
#ifndef CKERN_IO_TERMINAL_TERMINAL_HEADER
#define CKERN_IO_TERMINAL_TERMINAL_HEADER

/* typedef: struct terminal_t
 * Export <terminal_t> into global namespace. */
typedef struct terminal_t terminal_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_terminal_terminal
 * Test <terminal_t> functionality. */
int unittest_io_terminal_terminal(void);
#endif



/* struct: terminal_t
 * Dient zur Ermittlung und Konfiguration des »controlling Terminals«.
 * Mit der Funktion <configrawedit_terminal> wird in den Modus zur zeichenweisen
 * Tastaturverarbeitung umgeschalten, mit <configrestore_terminal> wird wieder
 * in den zeilenweisen Eingabemodus (siehe auch <input_terminal>) umgeschalten.
 *
 * Der zeilenweise Eingabemodus erlaubt auch das Senden von Signalen (CTRL-C, CTRL-Z, ...)
 * an den Vordergrundprozess und er unterstützt rudimentäre Editierfunktionen.
 *
 * Mit dem zeichenweisen (raw edit) Eingabemodus werden die Sondertasten zur Signalerzeugung
 * abgeschalten und die Tastendrücke werden vom Terminal direkt an den Prozess weitergereicht.
 * Der Prozess muss dann alle nötigen Editierfunktionen implementieren.
 *
 * Da manche Sondertasten auf der Tastatur, etwa F1 bis F12, als Esacpesequenzen kodiert werden
 * und es sein könnte, daß nicht alle Bytes gelesen wurden, gibt es die Funktion <tryread_terminal>.
 * Diese wartet bis zu 1/10 Sekunde auf noch einzulesendes Bytes, um alle Zeichen einer Escapesequenz
 * zu lesen. Da der Benutzer diese Sequenzen manuell eingeben könnte, ist die Wartezeit auf 1/10
 * Sekunde voreingestellt um eine manuelle Eingabe einer Zeichenfolge von einer zusammengehörenden
 * Escapesequenz zu unterscheiden.
 *
 * Änderung der Fenstergröße eines Terminal:
 * Ein vom Terminal im Vordergrund gestarteter Prozess wird zu der Vordergrundprozessgruppe des
 * Terminals zugeordnet und bekommt Änderung der Fenstergröße vom Terminal in der Folge gemeldet.
 *
 * Ordnet sich der Prozess in eine andere Prozessgruppe um, oder wird die
 * Vordergrundprozessgrupee des Terminal geändert (siehe tcgetpgrp(3) und tcsetpgrp(3)),
 * dann bekommt er keine Änderungen mehr mitgeteilt.
 *
 * Mit <issizechange_terminal> kann eine änderung abgefragt werden.
 *
 * Weitere Terminals:
 * Mittels open können von diesem Prozess aus weitere Terminals geöffnet wurden, diese
 * teilen aber nicht die Änderung der Fenstergröße mit und der Prozess teilt sich die
 * Ein- und Ausgabe möglicherweise mit Prozessen, die einer anderen Session und Prozessgruppe angehören.
 * Dabei würden mögliche Eingaben byteweise zerhackt an verschiedene Prozesse weitergereicht werden,
 * was eine sinnvolle Verarbeitung unmöglich machte.
 *
 * Controlling Terminal:
 * Das Terminal, welches diesen Prozess startet wird auch als »controlling Terminal« vermerkt.
 *
 * Ein Prozess ist in der Regel mit einem »controlling Terminal« verbunden ist.
 * Auf dieses Terminal zeigen die Filedescriptoren <sys_iochannel_STDIN>, <sys_iochannel_STDOUT>
 * und <sys_iochannel_STDERR> direkt nach dem Programmstart.
 * Ein controlling Terminal ist eindeutig einer einzigen Session zugeordnet, der Session Leader
 * (Sitzungsführer), der sich zuerst mit dem Terminal verbunden hat, wird zum »controlling process«
 * des Terminals. Andere Prozesse, die derselben Sitzung angehören, also durch die Shell gestartet
 * wurden, bekommen dasselbe controlling Terminal zugeordnet.
 * Der Session Leader bestimmt, mit welchem controlling Terminal die gesamte Session
 * (alle darin befindlichen Prozesse) verbunden ist.
 *
 * Ein Daemonprozesses hat kein controlling Terminal.
 *
 * Ein Terminal zum »controlling Terminal« zu erheben, ist nur dem Session Leader
 * möglich und auch nur dann, wenn das Terminal nicht bereits einer anderen Session
 * als controlling Terminal zugeordnet ist.
 *
 * Controlling Process:
 * Um der kontrollierende Prozess (der Sitzungführer, der zuerst das Terminal geöffnet hat)
 * zu werden, kann in einer Bash-Shell "> exec <progamname>" ausgeführt werden. Die Shell,
 * also der kontrollierende Prozess des Terminals wird durch <progname> ersetzt. <progname>
 * wird dabei der neue kontrollierende Prozess des Terminals.
 *
 * Siehe auch (Linux):
 * - open(path, O_CLOEXEC|O_NOCTTY, ...)
 * - ioctl(fd, TIOCNOTTY)
 * - ioctl(fd, TIOCSCTTY)
 *
 * */
struct terminal_t {
   /* variable: input
    * Eingabe IOchannel zum Lesen der Tastatureingaben des Terminals. */
   sys_iochannel_t input;
   /* variable: output
    * Ausgabe IOchannel zum Schreiben von Text auf das Terminal. */
   sys_iochannel_t output;
   /* variable: ctrl_lnext
    * Taste, um die Nachfolgende als Wert und nicht als Kommando zu verstehen. */
   uint8_t         ctrl_lnext;
   /* variable: ctrl_susp
    * Taste, um das Vordergrundprogrammzu stoppen und auf die Shell zurückzuschalten. */
   uint8_t         ctrl_susp;
   /* variable: oldconf_vmin
    * Speichert den alten VMIN Wert des Terminals.
    * Legt die Anzahl Bytes fest, die mindestens pro Lesevorgang vom Eingabestrom
    * gelesen werden sollen. */
   uint8_t         oldconf_vmin;
   /* variable: oldconf_vtime
    * Speichert den alten VTIME Wert des Terminals.
    * Legt die Zeit in 10tel Sekunden fest, die maximal zwischen dem Empfang von zwei Bytes
    * im Eingabestrom liegen darf. */
   uint8_t         oldconf_vtime;
   /* variable: oldconf_echo
    * Speichert die alte Belegung des echo Flags in <init_terminal>.
    * Bestimmt, ob eingegebene Zeichen sofort in der Ausgabe sichtbar gemacht werden (an),
    * auch wenn sie vom Applikationprogramm noch nicht aus dem Eingabepuffer gelesen wurden. */
   unsigned        oldconf_echo:1;
   /* variable: oldconf_icanon
    * Speichert die alte Belegung des icanon Flags in <init_terminal>.
    * Gibt an, ob bei Eingabe Zeile für Zeile (an) oder Zeichen für Zeichen (aus) von der Tastatur gelesen werden soll.
    * Ist es an, sind auch alle Editierfunktionen des Terminals aktiv. */
   unsigned        oldconf_icanon:1;
   /* variable: oldconf_icrnl
    * Speichert die alte Belegung des icrnl Flags in <init_terminal>.
    * Gibt an, ob bei Eingabe von Carriage Return ein Newline zurückgegeben werden soll (an). */
   unsigned        oldconf_icrnl:1;
   /* variable: oldconf_isig
    * Speichert die alte Belegung des isig Flags in <init_terminal>.
    * Gibt an, ob bei Eingabe spezieller Control-Tasten (CTRL-C, CTRL-Z) Signale wie SIGINT bzw. SIGTSTP
    * erzeugt und dem Prozess gesendet werden sollen (an). */
   unsigned        oldconf_isig:1;
   /* variable: oldconf_ixon
    * Speichert die alte Belegung des ixon Flags in <init_terminal>.
    * Gibt an, ob bei Eingabe der STOP Taste (default CTRL-S) der Ausgabeprozess gestoppt wird und bei
    * Eingabe der START Taste (default CTRL-Q) fortgeführt. */
   unsigned        oldconf_ixon:1;
   /* variable: oldconf_onlcr
    * Speichert die alte Belegung des onlcr Flags in <init_terminal>.
    * Gibt an, ob bei Ausgabe anstelle von '\n' (Newline) die Zeichen '\r' und '\n' (Carriage Return, Newline)
    * ausgegeben werden sollen (an). */
   unsigned        oldconf_onlcr:1;
   /* variable: doclose
    * Bestimmt, ob in <free_terminal> die Kanäle <input> und <output> geschlossen werden sollen (an). */
   unsigned        doclose:1;
};

// group: lifetime

/* define: terminal_FREE
 * Static initializer. */
#define terminal_FREE \
         { sys_iochannel_FREE, sys_iochannel_FREE, 0,0,0,0,0,0,0,0,0,0,0 }

/* function: init_terminal
 * Initialisiert terml mit dem »controlling Terminal«.
 * Falls <sys_iochannel_STDIN>, <sys_iochannel_STDOUT> mit dem controlling Terminal
 * verbunden sind, werden diese Filedescriptoren verwendet.
 * Ansonsten wird versucht, eine neue Verbindung zum controlling Terminal aufzubauen. */
int init_terminal(/*out*/terminal_t * terml);

/* function: free_terminal
 * Schliesst geöffneten IOchannel, falls nicht <sys_iochannel_STDIN> und <sys_iochannel_STDOUT> verwendet wurden.
 * Falls <configrawedit_terminal> aufgerufen wurde, muss vor <free_terminal> explizit <configrestore_terminal>
 * aufgerufen werden, um das Terminal wieder in den regulären Modus zurückzuschalten. */
int free_terminal(terminal_t * terml);

// group: query

/* function: input_terminal
 * Gibt den Eingabekanal vom Terminal terml zurück.
 * Eingelesene Bytes sind die UTF8-Codes der Zeichen plus als Esacpesequenzen kodierte Sondertasten.
 * Mit der Funktion <querykey_termcdb> können diese Sondertasten dekodiert werden. */
sys_iochannel_t input_terminal(terminal_t * terml);

/* function: output_terminal
 * Gibt den Ausgabekanal vom Terminal terml zurück.
 * Geschriebene Bytes sollten in UTF8 kodierte Zeichen sein plus als Esacpesequenzen kodierte Sonderfunktionen.
 * Die Typ <termcdb_t> bietet Funktionen wie <movecursor_termcdb> und weitere, welche diese Escapesequenzen
 * dem Typ des Terminals entsprechend erzeugen. */
sys_iochannel_t output_terminal(terminal_t * terml);

/* function: hascontrolling_terminal
 * Gibt true zurück, falls der Prozess mit einem »controlling Terminal« verbunden ist.
 * Das controlling Terminal ist eindeutig einer einzigen Session zugeordnet.
 * Im Falle eines Daemonprozesses, der auch der Session Leader ist, hat eine Session
 * kein controlling Terminal. */
bool hascontrolling_terminal(void);

/* function: is_terminal
 * Gibt true zurück, falls fd auf ein Terminal zeigt. */
bool is_terminal(sys_iochannel_t fd);

/* function: iscontrolling_terminal
 * Gibt true zurück, falls fd auf ein »controlling Terminal« zeigt.
 * Das controlling Terminal ist eindeutig einer einzigen Session zugeordnet.
 * Im Falle eines Daemonprozesses, der auch der Session Leader ist, hat eine Session
 * kein controlling Terminal. */
bool iscontrolling_terminal(sys_iochannel_t fd);

/* function: issizechange_terminal
 * Gibt true zurück, falls sich die Fenstergröße des Terminals, von dem aus der Prozes gestartet wurde,  geändert hat.
 * Nur Prozesse, welche der Vordergrundprozessgruppe des Terminals angehören, empfangen dieses Signal. */
bool issizechange_terminal(void);

/* function: waitsizechange_terminal
 * Gibt 0 zurück, falls sich die Fenstergröße des Terminal, von dem aus der Prozes gestartet wurde, geändert hat.
 * Das Warten kann durch das Signal SIGINT unterbrochen werden.  Siehe auch <issizechange_terminal>.
 *
 * Return:
 * 0     - Windowgröße des »controlling Terminal« hat sich geändert.
 * EINTR - Falls das Warten durch einen Interrupt unterbrochen wurde. */
int waitsizechange_terminal(void);

/* function: isutf8_terminal
 * Gibt true zurück, wenn das Terminal auf Verarbeitung von UTF8 konfiguriert ist. */
bool isutf8_terminal(terminal_t * terml);

/* function: type_terminal
 * Liefert den Typ controlling Terminals als String zurück.
 *
 * Return:
 * 0       - type enthält \0 terminierten Typ des controlling Terminals.
 * ENODATA - Der Typ konnte nicht ermittelt werden (Fehler wird nicht geloggt).
 * ENOBUFS - Der Buffer type ist zu klein, um alle Zeichen und das abschließende \0 Zeichen aufzunehmen
 *           (Fehler wird nicht geloggt).
 *
 * Hintergrund:
 * Der Typ des Terminal wird aus der Environment Variablen TERM ausgelesen und nach type kopiert. */
int type_terminal(uint16_t len, /*out*/uint8_t type[len]);

/* function: pathname_terminal
 * Gibt den Pfad auf das von fd referenzierte Terminal zurück.
 *
 * Return:
 * 0      - name enthält \0 terminierten Dateipfad des von Dateideskriptor fd referenzierten Terminals.
 * EBADF  - terml ist uninitialisiert und besitzt keinen gültigen Dateideskriptor.
 * ENOBUFS - Der Buffer name ist zu klein, um alle Zeichen und das abschließende \0 Zeichen aufzunehmen.
 *          Der Inhalt von name wurde möglicherweise aber trotzdem teilweise geändert.
 *
 * Alle Fehler außer ENOBUFS werden geloggt! */
int pathname_terminal(const terminal_t * terml, uint16_t len, /*err;out*/uint8_t name[len]);

/* function: ctrlsusp_terminal
 * Gibt CTRL-? Keycode zurück, der den aktuellen Vordergrundprozess schlafen legt (Signal SIGTSTP).
 * Defaultwert ist CTRL-Z. */
uint8_t ctrlsusp_terminal(const terminal_t * terml);

/* function: ctrllnext_terminal
 * Gibt CTRL-? Keycode zurück, der die nächste Kommandotaste als Wert markiert und sie ihrer Kommandofunktion enthebt.
 * Defaultwert ist CTRL-V. */
uint8_t ctrllnext_terminal(const terminal_t * terml);

// group: read

/* function: tryread_terminal
 * Liest maximal len Datenbytes von der Terminaleingabe.
 * Falls <configrawedit_terminal> aufgerufen wurde, dann
 * wartet diese Funktion maximal 1/10 Sekunde zwischen den einzelnen Datenbytes.
 * Diese Funktion kann zum Lesen für Spezialtasten, die 2 und mehr Bytes pro Taste (etwa "\033""OC") übertragen,
 * aufgerufen werden, wenn am Ende des gelesenen Buffer das Prefix einer unvollständigen Spezialtaste vorliegt.
 * Falls <configrawedit_terminal> nicht oder danach <configrestore_terminal> aufgerufen wurde,
 * wartet diese Funktion solange, bis der Benutzer eine Eingabezeile eingegben hat.
 *
 * Return:
 * 0      - Keine Daten innerhalb von 1/10 Sekunde gelesen (<configrawedit_terminal> wurde aufgerufen).
 * != 0   - Anzahl an Bytes, die nach keys gelesen wurden. Der Wert ist immer <= len. */
size_t tryread_terminal(terminal_t * terml, size_t len, /*out*/uint8_t keys[len]);

/* function: readsize_terminal
 * Liest die aktuelle Höhe und Breite des Terminals in Zeichen.
 * Ändert sich die Terminalgröße, kann mittels <waitsizechange_terminal>
 * bzw. <issizechange_terminal> die Änderung ermittelt werden.
 * Meldet eine der beiden Funktionen eine Änderung, muss die Funktion <readsize_terminal>
 * aufgerufen werden, um die Größe auf den neuesten Stand zu bringen. */
int readsize_terminal(terminal_t * terml, uint16_t * rowsize, uint16_t * colsize);

// group: update

/* function: removecontrolling_terminal
 * Entfernt die Verbindung des controlling Terminal mit diesem Prozess.
 * Das Terminal kann weiterhin angesprochen werden, es ist nur nicht mehr
 * als das kontrollierende Terminal mit diesem Prozess registriert.
 *
 * Return:
 * 0     - Erfolg.
 * ENXIO - Kein kontrollierendes Terminal registriert.
 * Andere Fehler wie ENOMEM, ENFILE auch möglich.
 *
 * Alternativen:
 * Wird die Session ID (POSIX: setsid) des Prozesses geändert, löst sich der Prozess
 * auch von seinem controlling Terminal – siehe dazu <process_t.daemonize_process>.
 * */
int removecontrolling_terminal(void);

// group: config line discipline

/* function: configstore_terminal
 * Merkt sich die aktuellen Terminaleinstellungen.
 * Wird von <init_terminal> automatisch aufgerufen.
 * Die gespeicherten Einstellungen können mit <configrestore_terminal> wiederhergestellt werden.
 * Dies Funktion sollte immer dann aufgerufen werden, wenn der Prozess das Signal SIGCONT empfängt. */
int configstore_terminal(terminal_t * terml);

/* function: configrestore_terminal
 * Macht Änderungen von <configrawedit_terminal> rückgängig.
 * Dies Funktion sollte immer dann aufgerufen werden, wenn der Prozess das Signal SIGTSTP empfängt. */
int configrestore_terminal(terminal_t * terml);

/* function: configrawedit_terminal
 * Schaltet um von zeilenweise Eingabe auf zeichenweise Eingabe und Funktion der Control-Tasten aus.
 * Die Änderung der Konfiguration muss vor Schließen von terml mit <configrestore_terminal>
 * wieder rückgängig gemacht werden, ansonsten muß der Benutzer manuell "stty sane" blind auf
 * der Kommandozeile eingeben, da die Echofunktion auch ausgeschalten ist. */
int configrawedit_terminal(terminal_t * terml);



// section: inline implementation

/* define: ctrllnext_terminal
 * Implements <terminal_t.ctrllnext_terminal>. */
#define ctrllnext_terminal(terml) \
         ((terml)->ctrl_lnext)

/* define: ctrlsusp_terminal
 * Implements <terminal_t.ctrlsusp_terminal>. */
#define ctrlsusp_terminal(terml) \
         ((terml)->ctrl_susp)

/* define: input_terminal
 * Implements <terminal_t.input_terminal>. */
#define input_terminal(terml) \
         ((sys_iochannel_t) (terml)->input)

/* define: output_terminal
 * Implements <terminal_t.output_terminal>. */
#define output_terminal(terml) \
         ((sys_iochannel_t) (terml)->output)


#endif
