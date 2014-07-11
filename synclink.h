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

// group: query

/* function: isvalid_synclink
 * Return (slink->link != 0). */
int isvalid_synclink(const synclink_t * slink);

// group: update

/* function: relink_synclink
 * Verbindet slink->link->link mit slink.
 * Stellt die Verbindung zu dem Nachbar wieder her, nachdem slink im Speicher verschoben wurde.
 *
 * Ungeprüfte Precondition:
 * o isvalid_synclink(slink) */
void relink_synclink(synclink_t * __restrict__ slink);

/* function: unlink_synclink
 * Invalidiert slink->link->link, setzt ihn auf 0.
 * slink selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_synclink>.
 *
 * Unchecked Precondition:
 * o isvalid_synclink(slink) */
void unlink_synclink(synclink_t * __restrict__ slink);


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
 * >           ╭─────────────╮
 * >           ┤ synclinkd_t ├
 * >  0 == next╰─────────────╯prev == 0
 *
 * Unchecked Invariant:
 * o (<prev> == 0 && <next> == 0) || (<prev> != 0 && <next> != 0)
 * */
struct synclinkd_t {
   // group: private fields
   /* variable: prev
    * Zeigt auf vorherigen Link Nachbarn. */
   synclinkd_t * prev;
   /* variable: next
    * Zeigt auf nächsten Link Nachbarn. */
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

/* function: initprev_synclinkd
 * Fügt prev vor Knoten slink ein. Der Link prev wird initialisiert und verbunden mit den Knoten
 * oldprev=slink->prev und slink. Nach Rückkehr zeigt oldprev->next auf prev, prev->prev auf oldprev
 * und prev->next auf slink und slink->prev auf prev. */
void initprev_synclinkd(/*out*/synclinkd_t * prev, synclinkd_t * slink);

/* function: initnext_synclinkd
 * Fügt next nach Knoten slink ein. Der Link next wird initialisiert und verbunden mit den Knoten
 * slink und oldnext=slink->next. Nach Rückkehr zeigt oldnext->prev auf next, next->next auf oldnext
 * und next->prev auf slink und slink->next auf next. */
void initnext_synclinkd(/*out*/synclinkd_t * next, synclinkd_t * slink);

/* function: initself_synclinkd
 * Initialisiert slink, so daß er auf sich selbst verlinkt.
 * Ein, Link der auf sich selbst zeigt, kann nicht im Speicher verschoben werden,
 * da <relink_synclinkd> nicht funktioniert. Erst der erneute Aufruf <initself_synclinkd>
 * nach dem Verschieben im Speicher macht ihn wieder gültig.
 * Nach der Initialisierung können mittels <initprev_synclinkd> bzw. <initnext_synclinkd> neue Knoten
 * verlinkt werden. */
void initself_synclinkd(/*out*/synclinkd_t * slink);

/* function: free_synclinkd
 * Trennt slink aus einer Link-Kette heraus.
 * slink wird auf den Wert <synclinkd_FREE> gesetzt.
 * Falls slink der vorletzte Eintrag in der Kette war (slink->next==slink->prev),
 * werden slink->next->prev und slink->next->next auch auf 0 gestzt. */
void free_synclinkd(synclinkd_t * slink);

// group: query

/* function: isvalid_synclinkd
 * Return (slink->prev != 0). */
int isvalid_synclinkd(const synclinkd_t * slink);

/* function: isself_synclinkd
 * Return (slink->prev == slink). */
int isself_synclinkd(const synclinkd_t * slink);

// group: update

/* function: relink_synclinkd
 * Stellt die Verbindung zu den Nachbarn wieder her, nachdem slink im Speicher verschoben wurde.
 *
 * Unchecked Precondition:
 * o isvalid_synclinkd(slink) */
void relink_synclinkd(synclinkd_t * slink);

/* function: unlink_synclinkd
 * Trennt slink aus einer Link-Kette heraus.
 * slink selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_synclinkd>.
 *
 * Unchecked Precondition:
 * o isvalid_synclinkd(slink) */
void unlink_synclinkd(synclink_t * __restrict__ slink);

/* function: unlinkkeepself_synclinkd
 * Trennt slink aus einer Link-Kette heraus.
 * slink selbst wird nicht verändert.
 * Dies ist eine Optimierung gegenüber <free_synclinkd>.
 * Falls slink der vorletzte Knoten in der Kette ist,
 * dann gilt <isself_synclinkd>(slink->next) nach Return.
 *
 * Unchecked Precondition:
 * o isvalid_synclinkd(slink) */
void unlinkkeepself_synclinkd(synclink_t * __restrict__ slink);

/* function: spliceprev_synclinkd
 * Fügt Liste der Knoten, auf die prev zeigt, vor Knoten slink ein.
 * Nach Return zeigt slink->prev auf alten Wert von prev->prev und prev->prev zeigt
 * alten Wert von slink->prev.
 *
 * Unchecked Precondition:
 * o isvalid_synclinkd(slink)
 * o isvalid_synclinkd(prev)
 * */
void spliceprev_synclinkd(synclinkd_t * prev, synclinkd_t * slink);


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

/* define: isvalid_synclink
 * Implementiert <synclink_t.isvalid_synclink>. */
#define isvalid_synclink(slink) \
         ((slink)->link != 0)

/* define: relink_synclink
 * Implementiert <synclink_t.relink_synclink>. */
#define relink_synclink(slink) \
         do {                             \
            synclink_t * _sl = (slink);   \
            _sl->link->link = _sl;        \
         } while (0)

/* define: unlink_synclink
 * Implementiert <synclink_t.unlink_synclink>. */
#define unlink_synclink(slink) \
         do {                             \
            synclink_t * _sl = (slink);   \
            _sl->link->link = 0;          \
         } while (0)

// group: synclinkd_t

/* define: free_synclinkd
 * Implementiert <synclinkd_t.free_synclinkd>. */
#define free_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl2 = (slink); \
            if (_sl2->prev) {             \
               unlink_synclinkd(slink);   \
            }                             \
            _sl2->next = 0;               \
            _sl2->prev = 0;               \
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

/* define: initnext_synclinkd
 * Implementiert <synclinkd_t.initnext_synclinkd>. */
#define initnext_synclinkd(_next, slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _ne = (_next);  \
            _ne->next = _sl->next;        \
            _ne->next->prev = _ne;        \
            _ne->prev = _sl;              \
            _sl->next = _ne;              \
         } while (0)

/* define: initprev_synclinkd
 * Implementiert <synclinkd_t.initprev_synclinkd>. */
#define initprev_synclinkd(_prev, slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _pr = (_prev);  \
            _pr->prev = _sl->prev;        \
            _pr->prev->next = _pr;        \
            _pr->next = _sl;              \
            _sl->prev = _pr;              \
         } while (0)

/* define: initself_synclinkd
 * Implementiert <synclinkd_t.initself_synclinkd>. */
#define initself_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            _sl->prev = _sl;              \
            _sl->next = _sl;              \
         } while (0)

/* define: isvalid_synclinkd
 * Implementiert <synclinkd_t.isvalid_synclinkd>. */
#define isvalid_synclinkd(slink) \
         ((slink)->prev != 0)

/* define: isself_synclinkd
 * Implementiert <synclinkd_t.isself_synclinkd>. */
#define isself_synclinkd(slink) \
         ( __extension__ ({               \
            synclinkd_t * _sl = (slink);  \
            _sl->prev == _sl;             \
         }))

/* define: relink_synclinkd
 * Implementiert <synclinkd_t.relink_synclinkd>. */
#define relink_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            _sl->prev->next = _sl;        \
            _sl->next->prev = _sl;        \
         } while (0)

/* define: spliceprev_synclinkd
 * Implementiert <synclink_t.spliceprev_synclinkd>. */
#define spliceprev_synclinkd(_prev, slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            synclinkd_t * _pr = (_prev);  \
            synclinkd_t * _ppr;           \
            _ppr = _pr->prev;             \
            _pr->prev = _sl->prev;        \
            _pr->prev->next = _pr;        \
            _ppr->next = _sl;             \
            _sl->prev = _ppr;             \
         } while (0)

/* define: unlink_synclinkd
 * Implementiert <synclink_t.unlink_synclinkd>. */
#define unlink_synclinkd(slink) \
         do {                                \
            synclinkd_t * _sl = (slink);     \
            if (_sl->prev == _sl->next) {    \
               _sl->next->prev = 0;          \
               _sl->next->next = 0;          \
            } else {                         \
               _sl->next->prev = _sl->prev;  \
               _sl->prev->next = _sl->next;  \
            }                                \
         } while (0)

/* define: unlinkkeepself_synclinkd
 * Implementiert <synclink_t.unlinkkeepself_synclinkd>. */
#define unlinkkeepself_synclinkd(slink) \
         do {                             \
            synclinkd_t * _sl = (slink);  \
            _sl->next->prev = _sl->prev;  \
            _sl->prev->next = _sl->next;  \
         } while (0)

#endif
