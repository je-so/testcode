/* title: SyncLink

   Ein Link besteht aus zwei verbunden Zeigern – ein Zeiger in einer Struktur
   zeigt auf den Zeiger des zugehörigen Links in der anderen Struktur
   und umgekehrt.

   A link is formed from two connected pointers – one pointer in one structure
   points to the pointer in other structure corresponding to the same link and
   vice versa.

   >  ╭────────────╮      ╭────────────╮
   >  | synclink_t ├──────┤ synclink_t |
   >  ╰────────────╯1    1╰────────────╯

   Ein Dual-Link (doppelter/double Link):

   >                    ╭─────────────╮
   >    ╭───────────────┤ synclinkd_t ├─────────────╮
   >    |           next╰─────────────╯prev         |
   >    |╭─────────────╮            ╭─────────────╮ |
   >    ╰┤ synclinkd_t ├────────────┤ synclinkd_t ├─╯
   > prev╰─────────────╯next    prev╰─────────────╯next

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

   file: C-kern/api/task/synclink.h
    Header file <SyncLink>.

   file: C-kern/task/synclink.c
    Implementation file <SyncLink impl>.
*/
#ifndef CKERN_TASK_SYNCLINK_HEADER
#define CKERN_TASK_SYNCLINK_HEADER

/* typedef: struct synclink_t
 * Export <synclink_t> into global namespace. */
typedef struct synclink_t synclink_t;

/* typedef: struct synclinkd_t
 * Export <synclinkd_t> into global namespace. */
typedef struct synclinkd_t synclinkd_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_synclink
 * Teste <synclink_t>. */
int unittest_task_synclink(void);
#endif


/* struct: synclink_t
 * Verbindet zwei Strukturen miteinander.
 * Enthält die eine Seite des Links.
 * Eine Änderung des einen Link hat die Änderung
 * des gegenüberliegenden zur Folge.
 *
 * Invariant(synclink_t * sl):
 * - (sl != sl->link)
 * - (sl->link == 0 || sl->link->link == sl) */
struct synclink_t {
   synclink_t * link;
};

// group: lifetime

/* define: synclink_FREE
 * Static initializer. */
#define synclink_FREE \
         { 0 }

/* function: init_synclink
 * Initialisiert zwei Link-Seiten zu einem verbundenen Link. */
void init_synclink(/*out*/synclink_t * __restrict__ slink, /*out*/synclink_t * __restrict__ other);

/* function: free_synclink
 * Trennt einen link. Der Pointer dieses und der gegenüberliegenden Seite
 * werden auf 0 gesetzt. */
void free_synclink(synclink_t * slink);

// group: update

/* function: relink_synclink
 * Verbindet slink->link mit slink.
 *
 * Ungeprüfte Precondition:
 * - slink->link != 0 */
void relink_synclink(synclink_t * __restrict__ slink);


/* struct: synclinkd_t
 * Doppelter Link erlaubt das Verlinken in einer Kette.
 * Mittels <prev> zeigt er auf den vorherigen Knoten,
 * der mittels <next> auf diesen zurückzeigt.
 * Mittels <next> zeigt er auf den nächsten Knoten,
 * der mittels <prev> auf diesen zurückzeigt.
 *
 * Dieser duale Link ist verschränkt, next ist mit prev und prev
 * mit next verbunden:
 *  >     ╭─────╮          ╭─────╮          ╭─────╮
 *  >     |  x  ├──────────┤  y  ├──────────┤  z  |
 *  > prev╰─────╯next  prev╰─────╯next  prev╰─────╯next
 *
 * Verschieben im Speicher:
 *
 * Wird ein Link im Speicher verschoben, dann muss danach <relink_synclinkd>
 * auf diesem Link aufgerufen werden, damit die Nachbarn wieder korrekt
 * auf diesen Link zeigen.
 *
 * Nach dem Entfernen des letzten d-Link Nachbarn:
 *
 * >        ╭─────────────╮
 * >  ╭─────┤ synclinkd_t ├─────╮
 * >  | next╰─────────────╯prev |
 * >  ╰─────────────────────────╯
 *
 * Diser Zustand kann mit <issingle_synclinkd> überprüft werden.
 *
 * Würde dieser Link im Speicher verschoben, dann müsste danach
 * <relinksingle_synclinkd> anstatt <relink_synclinkd> aufgerufen werden.
 *
 * */
struct synclinkd_t {
   synclinkd_t * prev;
   synclinkd_t * next;
};

// group: lifetime

/* define: synclinkd_FREE
 * Static initializer. */
#define synclinkd_FREE \
         { 0, 0 }

/* function: init_synclinkd
 * Initialisiert zwei Link-Seiten zu einem verbundenen Link. */
void init_synclinkd(/*out*/synclinkd_t * slink, /*out*/synclinkd_t * other);

/* function: free_synclinkd
 * Trennt slink aus einer Link-Kette heraus. */
void free_synclinkd(synclinkd_t * slink);

// group: query

/* function: issingle_synclinkd
 * Gibt true zurück, wenn slink auf sich selbst zeigt.
 * return (slink->prev == slink); */
void issingle_synclinkd(const synclinkd_t * slink);

// group: update

/* function: insertprev_synclinkd
 * Verbindet slink->prev mit prev->next und prev->prev mit next des Knotens, auf den slink->prev vorher zeigte.
 * Der Link prev wird initialisiert und verbunden mit den Knoten slink und slink->prev und den Links slink->prev
 * und slink->prev->next. */
void insertprev_synclinkd(synclinkd_t * slink, /*out*/synclinkd_t * prev);

/* function: insertnext_synclinkd
 * Verbindet slink->next mit next->prev und next->next mit prev des Knotens, auf den slink->next vorher zeigte.
 * Der Link next wird initialisiert und verbunden mit den Knoten slink und slink->next und den Links slink->next
 * und slink->next->prev. */
void insertnext_synclinkd(synclinkd_t * slink, /*out*/synclinkd_t * next);

/* function: removeprev_synclinkd
 * Entfernt Link slink->prev aus Kette. Dasselbe kann mit <free_synclinkd>(slink->prev) erreicht werden.
 * Der Ausgabeparameter prev wird auf den alten Wert von slink->prev gesetzt. */
void removeprev_synclinkd(synclinkd_t * slink, /*out*/synclinkd_t ** prev);

/* function: removenext_synclinkd
 * Entfernt Link slink->next aus Kette. Dasselbe kann mit <free_synclinkd>(slink->next) erreicht werden.
 * Der Ausgabeparameter next wird auf den alten Wert von slink->next gesetzt. */
void removenext_synclinkd(synclinkd_t * slink, /*out*/synclinkd_t ** next);

/* function: relink_synclinkd
 * Stellt die Verbindung zu den Nachbarn wieder her, nachdem slink im Speicher verschoben wurde.
 *
 * Precondition:
 * - Vor dem Verschieben im Speicher: issingle_synclinkd(slink)==false */
void relink_synclinkd(synclinkd_t * slink);

/* function: relinksingle_synclinkd
 * Stellt die Verbindung zu sich selbst wieder her, nachdem slink im Speicher verschoben wurde.
 *
 * Precondition:
 * - Vor dem Verschieben im Speicher: issingle_synclinkd(slink)==true */
void relinksingle_synclinkd(synclinkd_t * slink);


// section: inline implementation

// group: synclink_t

/* define: init_synclink
 * Implementiert <synclink_t.init_synclink>. */
#define init_synclink(slink, other) \
         do {                             \
            synclink_t * _sl1 = (slink);  \
            synclink_t * _sl2 = (other);  \
            _sl1->link = _sl2;            \
            _sl2->link = _sl1;            \
         } while (0)

/* define: free_synclink
 * Implementiert <synclink_t.free_synclink>. */
#define free_synclink(slink) \
         do {                             \
            synclink_t * _sl = (slink);   \
            if (_sl->link) {              \
               _sl->link->link = 0;       \
            }                             \
            _sl->link = 0;                \
         } while (0)

/* define: relink_synclink
 * Implementiert <synclink_t.relink_synclink>. */
#define relink_synclink(slink) \
         do {                             \
            synclink_t * _sl = (slink);   \
            _sl->link->link = _sl;        \
         } while (0)

// group: synclinkd_t

/* define: free_synclinkd
 * Implementiert <synclinkd_t.free_synclinkd>. */
#define free_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            if (_sl->prev) {              \
            _sl->next->prev = _sl->prev;  \
            _sl->prev->next = _sl->next;  \
            }                             \
            _sl->next = 0;                \
            _sl->prev = 0;                \
         } while (0)

/* define: init_synclinkd
 * Implementiert <synclinkd_t.init_synclinkd>. */
#define init_synclinkd(slink, other) \
         do {                             \
            synclinkd_t * _sl1 = (slink); \
            synclinkd_t * _sl2 = (other); \
            _sl1->next = _sl2;            \
            _sl1->prev = _sl2;            \
            _sl2->next = _sl1;            \
            _sl2->prev = _sl1;            \
         } while (0)

/* define: insertnext_synclinkd
 * Implementiert <synclinkd_t.insertnext_synclinkd>. */
#define insertnext_synclinkd(slink, _next) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _ne = (_next);  \
            _ne->next = _sl->next;        \
            _ne->next->prev = _ne;        \
            _ne->prev = _sl;              \
            _sl->next = _ne;              \
         } while (0)

/* define: insertprev_synclinkd
 * Implementiert <synclinkd_t.insertprev_synclinkd>. */
#define insertprev_synclinkd(slink, _prev) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _pr = (_prev);  \
            _pr->prev = _sl->prev;        \
            _pr->prev->next = _pr;        \
            _pr->next = _sl;              \
            _sl->prev = _pr;              \
         } while (0)

/* define: issingle_synclinkd
 * Implementiert <synclinkd_t.issingle_synclinkd>. */
#define issingle_synclinkd(slink) \
         ( __extension__ ({      \
            const synclinkd_t *  \
                  _sl = (slink); \
            (_sl->prev == _sl);  \
         }))

/* define: relink_synclinkd
 * Implementiert <synclinkd_t.relink_synclinkd>. */
#define relink_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            _sl->prev->next = _sl;        \
            _sl->next->prev = _sl;        \
         } while (0)

/* define: relinksingle_synclinkd
 * Implementiert <synclinkd_t.relinksingle_synclinkd>. */
#define relinksingle_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            _sl->prev = _sl;              \
            _sl->next = _sl;              \
         } while (0)

/* define: removeprev_synclinkd
 * Implementiert <synclinkd_t.removeprev_synclinkd>. */
#define removeprev_synclinkd(slink, _prev) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _pr;            \
            _pr = _sl->prev;              \
            _sl->prev = _pr->prev;        \
            _sl->prev->next = _sl;        \
            _pr->prev = 0;                \
            _pr->next = 0;                \
            *(_prev) = _pr;               \
         } while (0)

/* define: removenext_synclinkd
 * Implementiert <synclinkd_t.removenext_synclinkd>. */
#define removenext_synclinkd(slink, _next) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _ne;            \
            _ne = _sl->next;              \
            _sl->next = _ne->next;        \
            _sl->next->prev = _sl;        \
            _ne->prev = 0;                \
            _ne->next = 0;                \
            *(_next) = _ne;               \
         } while (0)

#endif
