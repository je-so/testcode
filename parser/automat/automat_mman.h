/* title: Ad-hoc-Memory-Manager

   TODO: describe module interface

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/automat_mman.h
    Header file <Ad-hoc-Memory-Manager>.

   file: C-kern/proglang/automat_mman.c
    Implementation file <Ad-hoc-Memory-Manager impl>.
*/
#ifndef CKERN_PROGLANG_AUTOMAT_MMAN_HEADER
#define CKERN_PROGLANG_AUTOMAT_MMAN_HEADER

// === exported types
struct automat_mman_t;
struct automat_mman_state_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_automat_mman
 * Test <automat_mman_t> functionality. */
int unittest_proglang_automat_mman(void);
#endif

/* struct: automat_mman_state_t
 * Remembered allocation state of automat_mman_t.
 * Can be used to restore a previous state.
 * This allows to free multiple allocations with one call. */
typedef struct automat_mman_state_t {
   void    *last_page;
   size_t   freesize;
   size_t   allocated;
   size_t   wasted;
} automat_mman_state_t;


/* struct: automat_mman_t
 * Private type which manages a memory heap used for a single object of type <automat_t>. */
typedef struct automat_mman_t automat_mman_t;

// group: lifetime

/* function: new_automatmman
 * Allokiert eine neue Speicherseite und belegt deren erste sizeof(automat_mman_t) Bytes
 * für ein neues und mit der neuen Speicherseite initialisierte Memory-Manager-Objekt.
 * Das neu erzeugte Objekt wird in mman zurückgegeben. */
int new_automatmman(/*out*/struct automat_mman_t **mman);

/* function: delete_automatmman
 * Gibt alle belegten (Speicher-)Ressourcen frei. */
int delete_automatmman(struct automat_mman_t **mman);

// group: query

size_t SIZEALLOCATED_PAGECACHE(void);

/* function: refcount_automatmman
 * Gibt Anzahl der Objekte an, die mman nutzen. */
size_t refcount_automatmman(const struct automat_mman_t *mman);

/* function: sizeallocated_automatmman
 * Gibt insgesamt allokierten Speicher in Bytes zurück. */
size_t sizeallocated_automatmman(const struct automat_mman_t *mman);

/* function: wasted_automatmman
 * Gibt allokierten aber nicht mehr genutzten Speicher in Bytes zurück. */
size_t wasted_automatmman(const struct automat_mman_t *mman);

// group: update

/* function: reset_automatmman
 * Gibt alle Allokation frei.
 *
 * Attention:
 * Call it only if refcount_automatmman(mman) == 0 or if you know that you do not access
 * the allocated memory any more. */
void reset_automatmman(automat_mman_t *mman);

/* function: incruse_automatmman
 * Erhöht die Anzahl derer, die mman nutzen. */
void incruse_automatmman(struct automat_mman_t *mman);

/* function: decruse_automatmman
 * Verringert die Anzahl derer, die mman nutzen. Darf nur nach einem vorherigen
 * Aufruf von <incruse_automatmman> aufgerufen werden. */
size_t decruse_automatmman(struct automat_mman_t *mman);

/* function: incrwasted_automatmman
 * Parameter wasted gibt die Anzahl allokierten aber nicht mehr genutzten Speichers in Bytes an.
 * Die Gesamtanzahl wird um wasted erhöht. */
void incrwasted_automatmman(struct automat_mman_t *mman, size_t wasted);

// group: memory-allocation

/* function: malloc_automatmman
 * Allokiert mem_size bytes Speicher und gibt Startadresse in mem_addr zurück.
 * Der allokierte Speicherbereich erstreckt sich über [mem_addr .. mem_addr+mem_size-1]. */
int malloc_automatmman(struct automat_mman_t *mman, size_t mem_size, /*out*/void ** mem_addr);

/* function: mfreelast_automatmman
 * Gibt Speicher ab zuletzt allokiertem Speicher wieder frei.
 * Diese Funktion darf nur einmal nach einem Aufruf von <malloc_automatmman>
 * aufgerufen werden. Weitere Aufrufe sind nicht erlaubt. Diese Funktion dient dazu,
 * einen Speicherbereich wieder freizugeben, der nur temporär angelegt wurde.
 *
 * Precondition:
 * - malloc_automatmman(mman, mem_size2, &mem_addr2) called before this function
 * - mem_addr is in range [mem_addr2..mem_addr2+mem_size2]
 * */
int mfreelast_automatmman(struct automat_mman_t *mman, void *mem_addr);

/* function: storestate_automatmman
 * Speichert aktuellen Zustand von mman.
 * Damit kann er mit Aufruf von <restore_automatmman> wiederhergestellt werden. */
void storestate_automatmman(struct automat_mman_t *mman, /*out*/automat_mman_state_t *state);

/* function: restore_automatmman
 * Stellt gespeicherten Zustand von mman wieder her.
 * Mit diesem Aufruf können mehrere Allokation auf einmal wieder rückgängig gemacht werden.
 *
 * Call Sequence:
 * > storestate_automatmman -> state0
 * > call malloc many times
 * > storestate_automatmman -> state1
 * > call malloc many times
 * > storestate_automatmman -> state2
 * > call malloc many times
 * > restore_automatmman(state2) -> mman reset to state2
 * > ...
 * > restore_automatmman(state1) -> mman reset to state1
 * > ...
 * > restore_automatmman(state0) -> mman reset to state0
 *
 * Unchecked Precondition:
 * - state was initialized by a previous call to storestate_automatmman
 * - no previous call to restore_automatmman was made with state2 older than state */
void restore_automatmman(struct automat_mman_t *mman, const automat_mman_state_t *state);


// section: inline implementation


#endif
