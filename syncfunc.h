/* title: SyncFunc

   Definiert einen Ausführungskontext (execution context)
   für Funktionen der Signatur <syncfunc_f>.

   Im einfachsten Fall ist <syncfunc_t> ein einziger
   Funktionszeiger <syncfunc_f>.

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

   file: C-kern/api/task/syncfunc.h
    Header file <SyncFunc>.

   file: C-kern/task/syncfunc.c
    Implementation file <SyncFunc impl>.
*/
#ifndef CKERN_TASK_SYNCFUNC_HEADER
#define CKERN_TASK_SYNCFUNC_HEADER

#include "C-kern/api/task/synclink.h"

// forward
struct syncrunner_t;
struct syncwait_condition_t;

/* typedef: struct syncfunc_t
 * Export <syncfunc_t> into global namespace. */
typedef struct syncfunc_t syncfunc_t;

/* typedef: struct syncfunc_param_t
 * Export <syncfunc_param_t> into global namespace. */
typedef struct syncfunc_param_t syncfunc_param_t;

/* typedef: syncfunc_f
 * Definiert Signatur einer wiederholt auszuführenden Funktion.
 * Diese Funktionen werden *nicht* präemptiv unterbrochen und basieren
 * auf Kooperation mit anderen Funktionen.
 *
 * Parameter:
 * sfparam  - Ein- Ausgabe Parameter.
 *            Einige Eingabe (in) Felder sind nur bei einem bestimmten Wert in sfcmd gültig.
 *            Siehe <syncfunc_param_t> für eine ausführliche Beschreibung.
 * sfcmd    - Ein das auszuführenede Kommando beschreibende Wert aus <syncfunc_cmd_e>.
 *
 * Return:
 * Der Rückgabewert ist das vom Aufrufer auszuführende Kommando – auch aus <syncfunc_cmd_e>. */
typedef int (* syncfunc_f) (syncfunc_param_t * sfparam, uint32_t sfcmd);

/* enums: syncfunc_cmd_e
 * Kommando Parameter. Wird verwendet für sfcmd Parameter in <syncfunc_f>
 * und als »return« Wert von <syncfunc_f>.
 *
 * syncfunc_cmd_RUN         - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONRUN:« gekennzeichneten Stelle.
 *                            *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <syncfunc_cmd_RUN>
 *                            zu starten – <syncfunc_param_t.contlabel> ist ungültig.
 * syncfunc_cmd_CONTINUE    - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            setzt die Ausführung an der Stelle, die in <syncfunc_param_t.contlabel> gespeichert wurde.
 *                            *Aus*: Als Rückgabewert verlangt es, die nächste Ausführung mit dem Kommando <syncfunc_cmd_CONTINUE>
 *                            zu starten, genau an der Stelle, die vor dem »return« in <syncfunc_param_t.contlabel> abgelegt wurde.
 * syncfunc_cmd_EXIT        - *Ein*: Das Kommando wird von <start_syncfunc> verarbeitet und
 *                            startet die Ausführung an einer zumeist mit »ONEXIT:« gekennzeichneten Stelle.
 *                            Die Umgebung hat zu wenig Ressourcen, um die Funktion weiter auszuführen. Deshalb sollte sie
 *                            alle belegten und durch <syncfunc_param_t.state> referenzierten Ressourcen freigeben.
 *                            Danach mit dem Rückgabewert <syncfunc_cmd_EXIT> zurückkehren – jeder andere »return« Wert wird aber
 *                            auch als <syncfunc_cmd_EXIT> interpretiert.
 *                            *Aus*: Als Rückgabewert verlangt es, die Funktion nicht mehr aufzurufen, da die Berechnung beendet wurde.
 *                            Der als Erfolgsmeldung interpretierte Rückgabewert muss vorher in <syncfunc_param_t.retcode> abgelegt werden.
 *                            Die Funktion <syncfunc_t.exit_syncfunc> übernimmt diese Aufgabe. Wert 0 wird als Erfolg und ein Wert > 0
 *                            als Fehlercode interpretiert.
 * syncfunc_cmd_WAIT       -  *Ein*: Wird für die Kommandoeingabe nicht verwendet.
 *                            *Aus*: Als Rückgabewert bedeutet es, daß vor der nächsten Ausführung die durch die Variable
 *                            <syncfunc_param_t.contlabel> referenzierte Bedingung erfüllt sein muss. Die Ausführung pausiert
 *                            solange und wird mit der Erfüllung der Bedingung durch das Kommando <syncfunc_cmd_CONTINUE>
 *                            wieder aufgenommen. Also genau an der Stelle wieder gestartet, die vor dem »return«
 *                            in <syncfunc_param_t.contlabel> abgelegt wurde. Falls die Warteoperation mangels Ressourcen
 *                            nicht durchgeführt werden kann, wird in <syncfunc_param_t.waiterr> ein Fehlercode abgelegt,
 *                            der nur bei der nächsten Ausführung mit Kommando <syncfunc_cmd_CONTINUE> gültig ist.
 *                            Der Wert 0 zeigt eine gültige Warteoperation an. Der spezielle Wert 0 in <syncfunc_param_t.condition>
 *                            bedeutet, daß auf das Ende der zuletzt erzeugten <syncfunc_t> gewartet werdfen soll – siehe <syncrunner_t>.
 *                            Die Funktionen <wait_syncfunc> und <waitexit_syncfunc> implementieren dieses Protokoll.
 * */
enum syncfunc_cmd_e {
   syncfunc_cmd_RUN,
   syncfunc_cmd_CONTINUE,
   syncfunc_cmd_EXIT,
   syncfunc_cmd_WAIT
} ;

typedef enum syncfunc_cmd_e syncfunc_cmd_e;

/* enums: syncfunc_opt_e
 * Bitfeld das die vorhandenen Felder in <syncfunc_t> kodiert.
 *
 * syncfunc_opt_NONE      - Weder <syncfunc_t.state> noch <syncfunc_t.contlabel> sind vorhanden.
 * syncfunc_opt_WAITFOR_CALLED    - Das Feld <syncfunc_t.waitfor> ist vorhanden und zeigt auf einen <syncfunc_t.caller>.
 * syncfunc_opt_WAITFOR_CONDITION - Das Feld <syncfunc_t.waitfor> ist vorhanden und zeigt auf einen <syncwait_condition_t.waitfunc>.
 * syncfunc_opt_WAITFOR_MASK - Maskiert die Bits für <syncfunc_opt_WAITFOR_CALLED> und <syncfunc_opt_WAITFOR_CONDITION>.
 *                             Entweder sind alle Bits in <syncfunc_opt_WAITFOR_MASK> 0 oder gleich
 *                             <syncfunc_opt_WAITFOR_CALLED> bzw. gleich <syncfunc_opt_WAITFOR_CONDITION>.
 * syncfunc_opt_WAITLIST  - Das Feld <syncfunc_t.waitlist> ist vorhanden.
 * syncfunc_opt_CALLER    - Das Feld <syncfunc_t.caller> ist vorhanden.
 * syncfunc_opt_STATE     - Das Feld <syncfunc_t.state> ist vorhanden.
 * syncfunc_opt_ALL       - Alle Felder sind vorhanden, wobei <syncfunc_opt_WAITFOR_CONDITION> gültig ist und
 *                          nicht <syncfunc_opt_WAITFOR_CALLER>. */
enum syncfunc_opt_e {
   syncfunc_opt_NONE              = 0,
   syncfunc_opt_WAITFOR_CALLED    = 1,
   syncfunc_opt_WAITFOR_CONDITION = 1+2,
   syncfunc_opt_WAITFOR_MASK      = 3,
   syncfunc_opt_WAITLIST          = 4,
   syncfunc_opt_CALLER            = 8,
   syncfunc_opt_STATE             = 16,
   syncfunc_opt_ALL               = 31
};

typedef enum syncfunc_opt_e syncfunc_opt_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncfunc
 * Teste <syncfunc_t>. */
int unittest_task_syncfunc(void);
#endif


/* struct: syncfunc_param_t
 * Definiert Ein- Ausgabeparameter von <syncfunc_f> Funktionen. */
struct syncfunc_param_t {
   /* variable: syncrun
    * Eingabe-Param: Der Verwalter-Kontext von <syncfunc_t>. */
   struct syncrunner_t * syncrun;
   /* variable: contoffset
    * Ein- Ausgabe-Param: Die Stelle, and der mit der Ausführung weitergemacht werden soll.
    * Nur gültig, wenn Parameter sfcmd den Wert <syncfunc_cmd_CONTINUE> besitzt.
    * Der Wert wird nach »return« gespeichert, wenn die Funktion <syncfunc_cmd_CONTINUE>
    * oder <syncfunc_cmd_WAIT> zurückgibt. */
   uint16_t    contoffset;
   /* variable: state
    * Ein- Ausgabe-Param: Der gespeicherte Funktionszustand.
    * Muss von der Funktion verwaltet werden. */
   void *      state;
   /* variable: condition
    * Ausgabe-Param: Referenziert die Bedingung, auf die gewartet werden soll.
    * Der Wert wird nach »return« verwendet, wenn die Funktion den Wert <syncfunc_cmd_WAIT> zurückgibt. */
   struct syncwait_condition_t
             * condition;
   /* variable: waiterr
    * Eingabe-Param: Das Ergebnis der Warteoperation.
    * Eine 0 zeigt Erfolg an, != 0 ist ein Fehlercode – etwa ENOMEM wenn zu wenig Ressourcen
    * frei waren, um fie Funktion in die Warteliste einzutragen.
    * Nur gültig, wenn das Kommando <syncfunc_cmd_CONTINUE> ist und zuletzt die Funktion mit
    * »return <syncfunc_cmd_WAIT>« beendet wurde. */
   int        waiterr;
   /* variable: retcode
    * Ein- Ausgabe-Param: Der Returnwert der Funktion.
    * Beendet sich die Funktion mit »return <syncfunc_cmd_EXIT>«, dann steht in diesem Wert
    * der Erfolgswert drin (0 für erfolgreich, != 0 Fehlercode).
    * Der Eingabewert ist nur gültig, wenn das Kommando <syncfunc_cmd_CONTINUE> ist, zuletzt
    * die Funktion mit »return <syncfunc_cmd_WAIT>« beendet wurde, wobei <condition> 0 war.
    * Der Wert trägt dann den Erfolgswert der Funktion, auf deren Ende gewartet wurde. */
   int         retcode;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0, 0 }


/* struct: syncfunc_t
 * Der Ausführungskontext einer Funktion der Signatur <syncfunc_f>.
 * Dieser wird von einem <syncrunner_t> verwaltet, der alle
 * verwalteten Funktionen nacheinander aufruft. Durch die Ausführung
 * nacheinander und den Verzicht auf präemptive Zeiteinteilung,
 * ist die Ausführung synchron. Dieses kooperative Multitasking zwischen
 * allen Funktionen eines einzigen <syncrunner_t>s erlaubt und gebietet
 * den Verzicht Locks verzichtet werden (siehe <mutex_t>). Warteoperationen
 * müssen mittels einer <syncwait_condition_t> durchgeführt werden.
 *
 * Optionale Datenfelder:
 * Die Variablen <state> und <contlabel> sind optionale Felder.
 *
 * In einfachsten Fall beschreibt <mainfct> alleine eine syncfunc_t.
 *
 * Auf 0 gesetzte Felder sind ungültig. Das Bitfeld <syncfunc_opt_e>
 * wird verwendet, um optionale Felder als vorhanden zu markieren.
 *
 * Die Grundstruktur einer Implementierung:
 * Als ersten Parameter bekommt eine <syncfunc_f> nicht <syncfunc_t>
 * sondern den Parametertyp <syncfunc_param_t>. Das erlaubt ein
 * komplexeres Protokoll mit verschiedenen Ein- und Ausgabeparametern,
 * ohne <syncfunc_t> damit zu belasten.
 *
 * > int test_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
 * > {
 * >    int err = EINTR; // Melde Unterbrochen-Fehler, falls
 * >                     // syncfunc_cmd_EXIT empfangen wird
 * >    start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);
 * >    goto ONERR;
 * > ONRUN:
 * >    void * state = malloc(...);
 * >    setstate_syncfunc(sfparam, state);
 * >    ...
 * >    yield_syncfunc(sfparam);
 * >    ...
 * >    syncwait_condition_t * condition = ...;
 * >    err = wait_syncfunc(sfparam, condition);
 * >    if (err) goto ONERR;
 * >    ...
 * >    exit_syncfunc(sfparam, 0);
 * > ONERR:
 * >    free( getstate_syncfunc(sfparam) );
 * >    exit_syncfunc(sfparam, err);
 * > }
 * */
struct syncfunc_t {
   // group: private fields

   /* variable: mainfct
    * Zeiger auf immer wieder auszuführende Funktion. */
   syncfunc_f  mainfct;
   /* variable: contoffset
    * Speichert Offset von syncfunc_START (siehe <start_syncfunc>)
    * zu einer Labeladresse und erlaubt es so, die Ausführung an dieser Stelle fortzusetzen.
    * Fungiert als Register für »Instruction Pointer« bzw. »Programm Zähler«.
    * Diese GCC Erweiterung – Erläuterung auf https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html –
    * wird folgendermaßen genutzt;
    *
    * > contlabel = && LABEL;
    * > goto * contlabel;
    *
    * Mit einer weiteren GCC Erweiterung "Locally Declared Labels"
    * wird das Schreiben eines Makros zur Abgabe des Prozessors an andere Funktionen
    * zum Kinderspiel.
    *
    * > int test_syncfunc(syncfunc_param_t * sfparam, uint32_t sfcmd)
    * > {
    * >    ...
    * >    {  // yield MACRO
    * >       __label__ continue_after_wait;
    * >       sfparam->contoffset = &&continue_after_wait - &&syncfunc_START;
    * >       // yield processor to other functions
    * >       return syncfunc_cmd_CONTINUE;
    * >       // execution continues here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   uint16_t    contoffset;
   /* variable: optfields
    * TODO: */
   uint8_t     optfields;

   // == optional fields for use in wait operations ==

   /* variable: waitfor
    * *Optional*: TODO: */
   synclink_t  waitfor;
   /* variable: waitlist
    * *Optional*: TODO: */
   synclinkd_t waitlist;

   // == optional fields describing run state ==

   /* variable: caller
    * *Optional*: Zeigt auf aufrufenden syncthread_t und ist verbunden mit <syncthread_t.waitfor>. */
   synclink_t  caller;
   /* variable: state
    * *Optional*: Zeigt auf gesicherten Zustand der Funktion.
    * Der Wert ist anfangs 0 (ungültig). Der Speicher wird von <mainfct> verwaltet. */
   void *      state;
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, synclink_FREE, synclinkd_FREE, synclink_FREE, 0 }

// group: query

/* function: getsize_syncfunc
 * Returns the size in bytes of a <syncfunc_t> with optional fields encoded in optfields.
 * See also <syncfunc_opt_e>. */
size_t getsize_syncfunc(const syncfunc_opt_e optfields);

/* function: offwaitfor_syncfunc
 * Liefere den Byteoffset zum Feld <waitfor>. */
size_t offwaitfor_syncfunc(void);

/* function: offwaitlist_syncfunc
 * Liefere den Byteoffset zum Feld <waitlist>.
 * Parameter iswaitfor gibt an, ob Feld <waitfor> vorhanden ist. */
size_t offwaitlist_syncfunc(const bool iswaitfor);

/* function: offcaller_syncfunc
 * Liefere den BYteoffset zum Feld <caller>.
 * Die Parameter isstate und iscaller zeigen an, ob Felder <state> und/oder Feld <caller> vorhanden sind. */
size_t offcaller_syncfunc(size_t structsize, const bool isstate, const bool iscaller);

/* function: offstate_syncfunc
 * Liefere den Byteoffset zum Feld <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an
 * und isstate, ob das Feld state vorhanden ist. */
size_t offstate_syncfunc(size_t structsize, const bool isstate);

/* function: addrwaitfor_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <waitfor>.
 *
 * Precondition:
 * Feld <waitfor> ist vorhanden. */
synclink_t * addrwaitfor_syncfunc(syncfunc_t * sfunc);

/* function: addrwaitlist_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <waitlist>.
 * Parameter iswaitfor gibt an, ob Feld <waitfor> vorhanden ist.
 *
 * Precondition:
 * Feld <waitlist> ist vorhanden. */
synclinkd_t * addrwaitlist_syncfunc(syncfunc_t * sfunc, const bool iswaitfor);

/* function: addrcaller_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an
 * und isstate, ob Feld <state> vorhanden ist.
 *
 * Precondition:
 * Feld <caller> ist vorhanden. */
synclink_t * addrcaller_syncfunc(syncfunc_t * sfunc, const size_t structsize, const bool isstate);

/* function: addrstate_syncfunc
 * Liefere die Adresse Wert des optionalen Feldes <state>.
 * Parameter structsize gibt die mit <getsize_syncfunc> ermittelte Größe der Struktur an.
 *
 * Precondition:
 * Feld <state> ist vorhanden. */
void ** addrstate_syncfunc(syncfunc_t * sfunc, const size_t structsize);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: getstate_syncfunc
 * Liest Zustand der aktuell ausgeführten Funktion.
 * Der Wert 0 ist ein ungültiger Wert und zeigt an, daß
 * der Zustand noch nicht gesetzt wurde.
 * Der Wert wird aber nicht aus <syncfunc_t> sondern
 * aus dem Parameter <syncfunc_param_t> herausgelesen.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void * getstate_syncfunc(const syncfunc_param_t * sfparam);

/* function: setstate_syncfunc
 * Setzt den Zustand der gerade ausgeführten Funktion.
 * Wurde der Zustand gelöscht, ist der Wert 0 zu setzen.
 * Der Wert wird aber nicht nach <syncfunc_t> sondern
 * in den Parameter <syncfunc_param_t> geschrieben.
 * Er trägt eine Kopie des Zustandes und
 * andere nützliche, über <syncfunc_t> hinausgehende Informationen. */
void setstate_syncfunc(syncfunc_param_t * sfparam, void * new_state);

/* function: start_syncfunc
 * Springt zu onrun, onexit oder zu einer vorher gespeicherten Adresse.
 * Die ersten beiden Parameter müssen die Namen der Parameter von der aktuell
 * ausgeführten <syncfunc_f> sein. Das Verhalten des Makros hängt vom zweiten
 * Parameter sfcmd ab. Der erste ist nur von Interesse, falls sfcmd == <syncfunc_cmd_CONTINUE>.
 *
 * Implementiert als Makro definiert start_syncfunc zusätzlich das Label syncfunc_START.
 * Es findet Verwendung bei der Berechnung von Sprungzielen.
 *
 * Verarbeitete sfcmd Werte:
 * syncfunc_cmd_RUN         - Springe zu Label onrun. Dies ist der normale Ausführungszweig.
 * syncfunc_cmd_CONTINUE    - Springt zu der Adresse, die bei der vorherigen Ausführung in
 *                            <syncfunc_param_t.contlabel> gespeichert wurde.
 * syncfunc_cmd_EXIT        - Springe zu Label onexit. Dieser Zweig sollte alle Ressourcen freigeben
 *                            (free(<getstate_syncfunc>(sfparam)) und <exit_syncfunc>(sfparam,EINTR) aufrufen.
 * Alle anderen Werte       - Das Makro tut nichts und kehrt zum Aufrufer zurück.
 * */
void start_syncfunc(const syncfunc_param_t * sfparam, uint32_t sfcmd, LABEL onrun, IDNAME onexit);

/* function: yield_syncfunc
 * Tritt Prozessor an andere <syncfunc_t> ab.
 * Die nächste Ausführung beginnt nach yield_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
void yield_syncfunc(const syncfunc_param_t * sfparam);

/* function: exit_syncfunc
 * Beendet diese Funktion und setzt retcode als Ergebnis.
 * Konvention: Ein retcode von 0 meint OK, Werte > 0 sind System & App. Fehlercodes. */
void exit_syncfunc(const syncfunc_param_t * sfparam, int retcode);

/* function: wait_syncfunc
 * Wartet bis condition wahr ist.
 * Gibt 0 zurück, wenn das Warten erfolgreich war.
 * Der Wert ENOMEM zeigt an, daß nicht genügend Ressourcen bereitstanden
 * und die Warteoperation abgebrochen oder gar nicht erst gestartet wurde.
 * Andere Fehlercodes sind auch möglich.
 * Die nächste Ausführung beginnt nach wait_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int wait_syncfunc(const syncfunc_param_t * sfparam, struct syncwait_condition_t * condition);

/* function: waitexit_syncfunc
 * Wartet auf Exit der zuletzt gestarteten Funktionen.
 * In retcode wird der Rückgabewert der beendeten Funktion abgelegt (siehe <exit_syncfunc>),
 * auch im Fehlerfall, dann ist der Wert allerdings ungültig.
 *
 * Gibt 0 im Erfolgsfall zurück. Der Fehler EINVAL zeigt an,
 * daß keine Funktion innerhalb der aktuellen Ausführung dieser Funktion gestartet wurde
 * und somit kein Warten mölich ist. Im Falle zu weniger Ressourcen wird ENOMEM zurückgegeben.
 *
 * Die nächste Ausführung beginnt nach waitexit_syncfunc, sofern
 * <start_syncfunc> am Anfang dieser Funktion aufgerufen wird. */
int waitexit_syncfunc(const syncfunc_param_t * sfparam, /*out;err*/int * retcode);



// section: inline implementation

// group: syncfunc_t

/* define: start_syncfunc
 * Implementiert <syncfunc_t.start_syncfunc>. */
#define start_syncfunc(sfparam, sfcmd, onrun, onexit) \
         syncfunc_START:                     \
         ( __extension__ ({                  \
            const syncfunc_param_t * _sf;    \
            _sf = (sfparam);                 \
            switch ( (syncfunc_cmd_e)        \
                     (sfcmd)) {              \
            case syncfunc_cmd_RUN:           \
               goto onrun;                   \
            case syncfunc_cmd_CONTINUE:      \
               goto * (void*) ( (uintptr_t)  \
                  &&syncfunc_START           \
                  + _sf->contoffset);        \
            case syncfunc_cmd_EXIT:          \
               goto onexit;                  \
            default: /*ignoring all other*/  \
               break;                        \
         }}))

/* define: exit_syncfunc
 * Implementiert <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                                \
            (sfparam)->retcode = (_rc);   \
            return syncfunc_cmd_EXIT;     \
         }

/* define: getsize_syncfunc
 * Implementiert <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optfields) \
         ( __extension__ ({                        \
            syncfunc_opt_e _opt = (optfields);     \
            offwaitfor_syncfunc()                  \
            + ((_opt & syncfunc_opt_WAITFOR_MASK)  \
              ? sizeof(synclink_t) : 0)            \
            + ((_opt & syncfunc_opt_WAITLIST)      \
              ? sizeof(synclinkd_t) : 0)           \
            + ((_opt & syncfunc_opt_CALLER)        \
              ? sizeof(synclink_t) : 0)            \
            + ((_opt & syncfunc_opt_STATE)         \
              ? sizeof(void*) : 0);                \
         }))

/* define: getstate_syncfunc
 * Implementiert <syncfunc_t.getstate_syncfunc>. */
#define getstate_syncfunc(sfparam) \
         ((sfparam)->state)

/* define: offwaitfor_syncfunc
 * Implementiert <syncfunc_t.offwaitfor_syncfunc>. */
#define offwaitfor_syncfunc() \
         (offsetof(syncfunc_t, waitfor))

/* define: offwaitlist_syncfunc
 * Implementiert <syncfunc_t.offwaitlist_syncfunc>. */
#define offwaitlist_syncfunc(iswaitfor) \
         (offwaitfor_syncfunc() + ((iswaitfor) ? sizeof(synclink_t) : 0))

/* define: offcaller_syncfunc
 * Implementiert <syncfunc_t.offcaller_syncfunc>. */
#define offcaller_syncfunc(structsize, isstate, iscaller) \
         (offstate_syncfunc(structsize, isstate) - ((iscaller) ? sizeof(synclink_t) : 0))

/* define: offstate_syncfunc
 * Implementiert <syncfunc_t.offstate_syncfunc>. */
#define offstate_syncfunc(structsize, isstate) \
         ((structsize) - ((isstate) ? sizeof(void*) : 0))

/* define: addrwaitfor_syncfunc
 * Implementiert <syncfunc_t.addrwaitfor_syncfunc>. */
#define addrwaitfor_syncfunc(sfunc) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            synclink_t * _ptr;            \
            _sf = (sfunc);                \
            _ptr = (synclink_t*) (        \
                   (uint8_t*) _sf         \
                  + offwaitfor_syncfunc() \
                   );                     \
            _ptr;                         \
         }))

/* define: addrwaitlist_syncfunc
 * Implementiert <syncfunc_t.addrwaitlist_syncfunc>. */
#define addrwaitlist_syncfunc(sfunc, iswaitfor) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            synclinkd_t * _ptr;           \
            _sf = (sfunc);                \
            _ptr = (synclinkd_t*) (       \
                   (uint8_t*) _sf         \
                  + offwaitlist_syncfunc( \
                     (iswaitfor)));       \
            _ptr;                         \
         }))

/* define: addrcaller_syncfunc
 * Implementiert <syncfunc_t.addrcaller_syncfunc>. */
#define addrcaller_syncfunc(sfunc, structsize, isstate) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            synclink_t * _ptr;            \
            _sf = (sfunc);                \
            _ptr = (synclink_t*) (        \
                   (uint8_t*) _sf         \
                  + offcaller_syncfunc(   \
                    (structsize),         \
                    (isstate), true));    \
            _ptr;                         \
         }))

/* define: addrstate_syncfunc
 * Implementiert <syncfunc_t.addrstate_syncfunc>. */
#define addrstate_syncfunc(sfunc, structsize) \
         ( __extension__ ({               \
            syncfunc_t * _sf;             \
            void ** _ptr;                 \
            _sf = (sfunc);                \
            _ptr = (void**) (             \
                   (uint8_t*) _sf         \
                  + offstate_syncfunc(    \
                    (structsize), true)); \
            _ptr;                         \
         }))

/* define: setstate_syncfunc
 * Implementiert <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->state = (new_state) ; } while(0)

/* define: wait_syncfunc
 * Implementiert <syncfunc_t.wait_syncfunc>. */
#define wait_syncfunc(sfparam, _condition) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = _condition;              \
            static_assert(                                  \
               (uintptr_t)&&continue_after_wait >           \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_wait -        \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                           (uintptr_t)&&continue_after_wait \
                           - (uintptr_t)&&syncfunc_START);  \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            (sfparam)->waiterr;                             \
         }))

/* define: waitexit_syncfunc
 * Implementiert <syncfunc_t.waitexit_syncfunc>. */
#define waitexit_syncfunc(sfparam, _retcode) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = 0;                       \
            static_assert(                                  \
               (uintptr_t)&&continue_after_wait >           \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_wait -        \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                           (uintptr_t)&&continue_after_wait \
                           - (uintptr_t)&&syncfunc_START);  \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            *(_retcode) = (sfparam)->retcode;               \
            (sfparam)->waiterr;                             \
         }))

/* define: yield_syncfunc
 * Implementiert <syncfunc_t.yield_syncfunc>. */
#define yield_syncfunc(sfparam) \
         ( __extension__ ({                                 \
            __label__ continue_after_yield;                 \
            static_assert(                                  \
               (uintptr_t)&&continue_after_yield >          \
               (uintptr_t)&&syncfunc_START                  \
               && (uintptr_t)&&continue_after_yield -       \
                  (uintptr_t)&&syncfunc_START < 65536,      \
                  "conversion to uint16_t works");          \
            (sfparam)->contoffset = (uint16_t) (            \
                        (uintptr_t)&&continue_after_yield   \
                        - (uintptr_t)&&syncfunc_START);     \
            return syncfunc_cmd_CONTINUE;                   \
            continue_after_yield: ;                         \
         }))

#endif
