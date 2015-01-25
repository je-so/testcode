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
    * beim Schreiben von [bufaddr..buffaddr+bufsize-1] in den I/O Kanal transferriert. */
   uint8_t* bufaddr;
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
    * Der Zustand der Operation.
    * TODO: Beschreibung + Werte <iostate_e> einführen. */
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
   ioseq_t* owner_next; // managed by calling thread which has its own list of <ioseq_t>
   // TODO: describe fields
   ioseq_t* iolist_next; // managed by <iolist_t>
   uint8_t  state;
   uint16_t nrio;
   ioop_t   ioop[/*nrio*/];
};

// group: lifetime

/* define: ioseq_FREE
 * Statischer Initialisierer. */
#define ioseq_FREE \
         { 0, 0, 0, { ioop_FREE } }



/* struct: iolist_t
 * Eine Liste von auszuführenden I/O Operationen.
 * TODO: Definiere ioop_t
 * TODO: Definiere ioseq_t == ioop_t[1..n] + next pointer ...
 *
 * TODO: Zustände:
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
   // TODO: lock
   uint8_t  lock;
   // TODO: size
   size_t   size;
   /* variable: last
    * Single linked Liste von <ioseq_t>. Verlinkt wird über <ioseq_t.iolist_next>.
    * last->next zeigt auf first. last wird nur von den schreibenden Threads
    * verwendet, so daß auf einen Lock verzichtet werden kann. last wird nur dann vom lesenden
    * <iothread_t> auf 0 gesetzt, dann nämlich, wenn <first> == 0. */
   ioseq_t* last;
};

// group: lifetime

/* define: iolist_FREE
 * Static initializer. */
#define iolist_FREE \
         { 0, 0, 0 }

/* function: init_iolist
 * TODO: Describe Initializes object. */
void init_iolist(/*out*/iolist_t* iolist);

/* function: free_iolist
 * TODO: Describe Frees all associated resources.
 * Diese Funktion darf nur dann aufgerufen werden, wenn garantiert ist, daß kein
 * anderer Thread mehr Zugriff auf iolist hat. */
int free_iolist(iolist_t* iolist);

// group: query

// group: update

/* function: insertlast_iolist
 * TODO:
 * Solange ios noch nicht bearbeitet wurde, darf das Objekt nicht gelöscht werden.
 * Nachdem alle Einträge bearbeitet wurden (<ioseq_t.state>), wird der Besitz implizit
 * wieder an den Aufrufer transferiert, wobei der Eintrag mittels <ioseq_t.owner_next>
 * immer in der Eigentumsliste des Owners verbleibt, auch während der Bearbeitung
 * durch iolist.
 *
 * Der Parameter iothr dient dazu, <iothread_t.resume_iothread> aufzufrufen, falls
 * die Liste vor dem Einfügen leer war. */
void insertlast_iolist(iolist_t* iolist, /*own*/ioseq_t* ios, struct iothread_t* iothr);

/* function: removefirst_iolist
 * Entfernt das erste Elemente aus der Liste und gibt es in ios zurück.
 * Ist die Liste leer, wird der Fehler ENODATA zurückgegeben.
 * Nachdem der Aufrufer (<iothread_t>) das Element bearbeitet hat,
 * geht es automatisch an den Eigentümer zurück.
 * Das Feld <ioseq_t.state> dokumentiert, wann *ios komplett bearbeitet ist.
 *
 * Returncode:
 * 0 - *ios zeigt auf ehemals erstes Element.
 * ENODATA - Die Liste war leer. */
int tryremovefirst_iolist(iolist_t* iolist, /*out*/ioseq_t** ios);


// section: inline implementation

// group: ioop_t

/* define: isvalid_ioop
 * Implements <ioop_t.isvalid_ioop>. */
static inline int isvalid_ioop(const ioop_t* ioop)
{
         return ioop->offset >= 0 && ioop->bufaddr != 0 && ioop->bufsize != 0;
}

// group: iolist_t

/* define: init_iolist
 * Implementiert <iolist_t.init_iolist>. */
#define init_iolist(iolist) \
         ((void)(*(iolist) = (iolist_t) iolist_FREE))


#endif
