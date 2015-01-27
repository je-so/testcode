/* title: IOList

   _SHARED_

   Manage list or sequence of I/O operations.
   I/O operations are performed by special I/O threads (<iothread_t>).

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2015 Jörg Seebohn

   file: C-kern/api/io/subsys/iolist.h
    Header file <IOList>.

   file: C-kern/io/subsys/iolist.c
    Implementation file <IOList impl>.
*/
#ifndef CKERN_IO_SUBSYS_IOLIST_HEADER
#define CKERN_IO_SUBSYS_IOLIST_HEADER

// forward
struct iothread_t;

/* typedef: struct iolist_t
 * Export <iolist_t> into global namespace. */
typedef struct iolist_t iolist_t;

/* typedef: struct ioop_t
 * Export <ioop_t> into global namespace. */
typedef struct ioop_t ioop_t;

/* typedef: struct ioseq_t
 * Export <ioseq_t> into global namespace. */
typedef struct ioseq_t ioseq_t;

/* enums: ioop_e
 * Bezeichnet die auszuführende Operation. Siehe <ioop_t.op>.
 *
 * ioop_NOOP  - Keine Operation, ignoriere Eintrag.
 * ioop_READ  - Initiiere lesende Operation.
 * ioop_WRITE - Initiiere schreibende Operation.
 */
typedef enum ioop_e {
   ioop_NOOP,
   ioop_READ,
   ioop_WRITE,
} ioop_e;

/* enums: iostate_e
 * Bezeichnet den Zustand eines <ioop_t> und einer <ioseq_t>.
 *
 * iostate_VALID - Gültiger Eintrag, der auf Bearbeitung wartet.
 * iostate_EXEC_BIT  - Wird gerade von <iothread_t> bearbeitet.
 * iostate_READY_BIT - Bearbeitung ist abgeschlossen. Falls <iostate_ERROR_BIT> nicht gesetzt, ist kein Fehler aufgetreten.
 * iostate_ERROR_BIT - Zeigt im Zusammenhang mit <iostate_READY_BIT> an, daß bei der Bearbeitung ein Fehler auftrat.
 *                     Das Bit ist nur dann gesetzt, wenn <iostate_READY_BIT> auch gesetzt ist.
 * iostate_OK        - Beinhaltet alle Bitwerte, die eine ohne Fehler abgeschlossene Operation anzeigen.
 * iostate_ERROR     - Beinhaltet alle Bitwerte, die eine fehlerhaft abgeschlossene Operation anzeigen.
 * iostate_CANCEL    - Beinhaltet alle Bitwerte, die eine stornierte Operation anzeigen.
 *                     Die Operation kann nur storniert werden (siehe <iolist_t.cancelall_iolist>), wenn sie noch
 *                     nicht aus der <iolist_t> entfernt wurde, also noch nicht durch einen <iothread_t> bearbeitet wurde.
 *                     Die Werte <ioop_t.state> und <ioseq_t.state> werden auf <iostate_CANCEL>
 *                     und zusätzlich der Wert <ioop_t.err> auf ECANCELED gesetzt.
 */
typedef enum iostate_e {
   iostate_VALID     = 0,
   iostate_EXEC_BIT  = 1,
   iostate_READY_BIT = 2,
   iostate_ERROR_BIT = 4,
   iostate_CANCEL_BIT = 8,
   iostate_OK      = iostate_READY_BIT,
   iostate_ERROR   = iostate_READY_BIT|iostate_ERROR_BIT,
   iostate_CANCEL  = iostate_READY_BIT|iostate_ERROR_BIT|iostate_CANCEL_BIT,
} iostate_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_subsys_iolist
 * Test <iolist_t> functionality. */
int unittest_io_subsys_iolist(void);
#endif


/* struct: ioop_t
 * Beschreibt den Zustand einer I/O Operation.
 *
 * _SHARED_(process, 1R, 1W):
 * Siehe <iolist_t>.
 *
 * */
struct ioop_t {
   // public fields
   /* variable: offset
    * Byteoffset, ab dem gelesen oder geschrieben werden soll.
    * Sollte im Falle von O_DIRECT ein vielfaches der Systempagesize sein. */
   off_t    offset;
   /* variable: bufaddr
    * Die Startadresse des zu transferierenden Speichers.
    * Beim Lesen werden die Daten nach [bufaddr..buffaddr+bufsize-1] geschrieben,
    * beim Schreiben von [bufaddr..buffaddr+bufsize-1] in den I/O Kanal transferiert. */
   void*    bufaddr;
   /* variable: bufsize
    * Die Anzahl zu transferierender Datenbytes.
    * Sollte im Falle von O_DIRECT ein vielfaches
    * beim Schreiben von [bufaddr..buffaddr+bufsize-1] in den I/O Kanal transferriert.
    * Sollte im Falle von O_DIRECT ein vielfaches der Systempagesize sein. */
   size_t   bufsize;
   /* variable: ioc
    * I/O Kanal von dem gelesen oder auf den geschrieben werden soll. */
   sys_iochannel_t ioc;
   /* variable: op
    * Gibt die auszuführende Operation an: Lesen oder Schreiben. Wird mit Wert aus <ioop_e> belegt. */
   uint8_t  op;
   /* variable: state
    * Der Zustand der Operation. Die möglichen Wert werden durch <iostate_e> beschrieben.
    * Bevor <iolist_t.insertlast_iolist> aufgerufen wird, muss dieser Wert mit <iostate_VALID> initialisiert worden sein. */
   uint8_t  state;
   union {
      /* variable: err
       * Der Fehlercode einer fehlerhaft ausgeführten Operation. Nur gültig, falls <state> einen Fehler anzeigt. */
      int      err;
      /* variable: bytesrw
       * Die Anzahl fehlerfrei übertragener Daten. Nur gültig, falls <state> keinen Fehler anzeigt. */
      size_t   bytesrw;
   };
};

// group: lifetime

/* define: ioop_FREE
 * Statischer Initialisierer. */
#define ioop_FREE \
         { 0, 0, 0, sys_iochannel_FREE, ioop_NOOP, 0, { 0 } }

// group: query

/* function: isvalid_ioop
 * Gibt true zurück, wenn ioop gültige Werte (außer ioc) enthält. */
static inline int isvalid_ioop(const ioop_t* ioop);


/* struct: ioseq_t
 * Beschreibt den Zustand einer Reihe von I/O Operation eines Threads.
 *
 * _SHARED_(process, 1R, 1W):
 * Siehe <iolist_t>.
 *
 * */
struct ioseq_t {
   // public fields
   /* variable: owner_next
    * Erlaubt die Verlinkung aller vom Besitzerthread erzeugten <ioseq_t>. */
   ioseq_t* owner_next; // managed by calling thread which has its own list of <ioseq_t>
   /* variable: iolist_next
    * Erlaubt die Verlinkung aller in die <iolist_t> eingefügten <ioseq_t>. */
   ioseq_t* iolist_next; // managed by <iolist_t>
   /* variable: state
    * Zeigt den aktuellen Bearbeitungszustand dieses <ioseq_t>. Die möglichen Wert werden durch <iostate_e> beschrieben.
    * Bevor <iolist_t.insertlast_iolist> aufgerufen wird, muss dieser Wert mit <iostate_VALID> initialisiert worden sein. */
   uint8_t  state; // set to <iostate_VALID> by calling thread before insert; changed by <iothread_t>
   /* variable: nrio
    * Anzahl in <ioop> gespeicherter <ioop_t>. */
   uint16_t nrio; // set by calling thread before insert; never changed
   /* variable: ioop
    * Array von I/O Aufträgen, deren Anzahl durch nrio gegeben ist. */
   ioop_t   ioop[1/*nrio*/]; // set by calling thread before insert; ioop_t.state/err/bytesrw changed by <iothread_t>
};

// group: lifetime

/* define: ioseq_FREE
 * Statischer Initialisierer. */
#define ioseq_FREE \
         { 0, 0, 0, 0, { ioop_FREE } }

/* function: init_ioseq
 * Initialisiert ioseq für nrio <ioop_t>.
 * Nur die erste <ioop_t> wird auf <ioop_t.ioop_FREE> initialisiert.
 *
 * Unchecked Precondition:
 * - nrio >= 1
 * - memory_size(ioseq) >= sizeof(ioseq_t) + (nrio-1)*sizeof(ioop_t) */
void init_ioseq(ioseq_t* ioseq, uint16_t nrio);



/* struct: iolist_t
 * Eine Liste von auszuführenden I/O Operationen.
 * Der Zugriff ist durch ein Spinlock geschützt.
 *
 * _SHARED_(process, 1R, nW):
 * Der <iothread_t> greift lesend darauf zu - er verändert jedoch dabei
 * den das Lesen beschreibende Zustand.
 * Threads, die I/O Operationen ausführen wollen, verwenden das I/O
 * Subsystem und erzeugen eine oder mehrere Einträge in einer <iolist_t>.
 * Für jedes I/O-Device gibt es in der Regel einen zuständigen <iothread_t>, der
 * diese Liste abarbeitet.
 *
 * Writer:
 * Darf neue Elemente vom Typ <ioseq_t> einfügen.
 *
 * Reader:
 * Entfernt Elemente aus der Liste und bearbeitet diese dann.
 *
 * */
struct iolist_t {
   // group: private fields
   /* variable: lock
    * Schützt Zugriff auf <size> und <last>. */
   uint8_t  lock;
   /* variable: lock
    * Speichert die Anzahl der über <last> verlinkten <ioseq_t>. */
   size_t   size;
   /* variable: last
    * Single linked Liste von <ioseq_t>. Verlinkt wird über <ioseq_t.iolist_next>.
    * last->next zeigt auf first. last wird nur von den schreibenden Threads
    * verwendet, so daß auf einen Lock verzichtet werden kann. last wird nur dann vom lesenden
    * <iothread_t> auf 0 gesetzt, dann nämlich, wenn <first> == 0. */
   ioseq_t* last;
};

// group: lifetime

/* define: iolist_INIT
 * Static initializer. */
#define iolist_INIT \
         { 0, 0, 0 }

/* function: init_iolist
 * Initialisiert iolist als leere Liste. */
void init_iolist(/*out*/iolist_t* iolist);

/* function: free_iolist
 * Setzt alle Felder von iolist auf 0.
 * Ressourcen werden keine freigegeben.
 * Noch verlinkte <ioseq_t> werden entfernt und deren state auf <iostate_CANCEL> gesetzt.
 * Diese Funktion darf nur dann aufgerufen werden, wenn garantiert ist, daß kein
 * anderer Thread mehr Zugriff auf iolist hat. */
void free_iolist(iolist_t* iolist);

// group: query

/* function: size_iolist
 * Gibt Anzahl in iolist verlinkter <ioseq_t> zurück. */
size_t size_iolist(const iolist_t* iolist);

// group: update

/* function: insertlast_iolist
 * Fügt ios in iolist am Ende ein.
 * Der Miteigentumsrecht an ios wechselt temporär zu iolist, nur solange,
 * bis ios bearbeitet wurde. Dann liegt es automatisch wieder beim Aufrufer.
 *
 * Solange ios noch nicht bearbeitet wurde, darf das Objekt nicht gelöscht werden.
 * Nachdem alle Einträge bearbeitet wurden (<ioseq_t.state>), wird der Besitz implizit
 * wieder an den Aufrufer transferiert, wobei der Eintrag mittels <ioseq_t.owner_next>
 * immer in der Eigentumsliste des Owners verbleibt, auch während der Bearbeitung
 * durch iolist.
 *
 * Der Parameter iothr dient dazu, <iothread_t.resume_iothread> aufzufrufen, falls
 * die Liste vor dem Einfügen leer war.
 *
 * Unchecked Precondition:
 * - ios->state == iostate_VALID
 * - for (0 <= i < ios->nrio) : ios->ioop[i].state == iostate_VALID */
void insertlast_iolist(iolist_t* iolist, /*own*/ioseq_t* ios, struct iothread_t* iothr);

/* function: removefirst_iolist
 * Entfernt das erste Elemente aus der Liste und gibt es in ios zurück.
 * ios->iolist_next wird auf 0 gesetzt, alle anderen Felder bleiben unverändert
 * (ios->state == iostate_VALID).
 * Ist die Liste leer, wird der Fehler ENODATA zurückgegeben.
 * Nachdem der Aufrufer (<iothread_t>) das Element bearbeitet hat,
 * geht es automatisch an den Eigentümer zurück.
 * Das Feld <ioseq_t.state> dokumentiert, wann *ios komplett bearbeitet ist.
 *
 * Returncode:
 * 0 - *ios zeigt auf ehemals erstes Element.
 * ENODATA - Die Liste war leer. */
int tryremovefirst_iolist(iolist_t* iolist, /*out*/ioseq_t** ios);

/* function: cancelall_iolist
 * Entferne alle noch nicht bearbeiteten <ioseq_t> und setze deren state auf <iostate_CANCEL>.
 * Für jeden <ioop_t> in <ioseq_t> wird dessen state und err auf <iostate_CANCEL>
 * bzw. ECANCELED gesetzt. */
void cancelall_iolist(iolist_t* iolist);



// section: inline implementation

// group: ioop_t

/* define: isvalid_ioop
 * Implements <ioop_t.isvalid_ioop>. */
static inline int isvalid_ioop(const ioop_t* ioop)
{
         return ioop->offset >= 0 && ioop->bufaddr != 0 && ioop->bufsize != 0;
}

// group: ioseq_t

/* define: init_ioseq
 * Implementiert <ioseq_t.init_ioseq>. */
#define init_ioseq(ioseq, nrio) \
         ((void)((*ioseq) = (ioseq_t) { 0, 0, iostate_VALID, nrio, { ioop_FREE } }))

// group: iolist_t

/* define: free_iolist
 * Implementiert <iolist_t.free_iolist>. */
#define free_iolist(iolist) \
         (cancelall_iolist(iolist))

/* define: init_iolist
 * Implementiert <iolist_t.init_iolist>. */
#define init_iolist(iolist) \
         ((void)(*(iolist) = (iolist_t) iolist_INIT))

/* define: size_iolist
 * Implementiert <iolist_t.size_iolist>. */
#define size_iolist(iolist) \
         ((iolist)->size)

#endif
