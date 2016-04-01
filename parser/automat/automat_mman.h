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


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_automat_mman
 * Test <automat_mman_t> functionality. */
int unittest_proglang_automat_mman(void);
#endif


/* struct: automat_mman_t
 * Private type which manages a memory heap used for a single object of type <automat_t>. */
typedef struct automat_mman_t automat_mman_t;

// group: lifetime

/* function: new_automatmman
 * Allokiert eine neue Speicherseite und belegt deren erste sizeof(automat_mman_t) Bytes
 * für ein neues und mit der neuen Speicherseite initialisierte Memory-Manager-Objekt.
 * Das neu erzeugte Objekt wird in mman zurückgegeben. */
int new_automatmman(/*out*/struct automat_mman_t ** mman);

/* function: delete_automatmman
 * Gibt alle belegten (Speicher-)Ressourcen frei. */
int delete_automatmman(struct automat_mman_t ** mman);

// group: query

size_t SIZEALLOCATED_PAGECACHE(void);

/* function: refcount_automatmman
 * Gibt Anzahl der Objekte an, die mman nutzen. */
size_t refcount_automatmman(const struct automat_mman_t * mman);

/* function: sizeallocated_automatmman
 * Gibt insgesamt allokierten Speicher in Bytes zurück. */
size_t sizeallocated_automatmman(const struct automat_mman_t * mman);

/* function: wasted_automatmman
 * Gibt allokierten aber nicht mehr genutzten Speicher in Bytes zurück. */
size_t wasted_automatmman(const struct automat_mman_t * mman);


// group: update

/* function: incruse_automatmman
 * Erhöht die Anzahl derer, die mman nutzen. */
void incruse_automatmman(struct automat_mman_t * mman);

/* function: decruse_automatmman
 * Verringert die Anzahl derer, die mman nutzen. Darf nur nach einem vorherigen
 * Aufruf von <incruse_automatmman> aufgerufen werden. */
size_t decruse_automatmman(struct automat_mman_t * mman);

/* function: incrwasted_automatmman
 * Parameter wasted gibt die Anzahl allokierten aber nicht mehr genutzten Speichers in Bytes an.
 * Die Gesamtanzahl wird um wasted erhöht. */
void incrwasted_automatmman(struct automat_mman_t * mman, size_t wasted);

// group: memory-allocation

/* function: allocmem_automatmman
 * Allokiert mem_size bytes Speicher und gibt Startadresse in mem_addr zurück.
 * Der allokierte Speicherbereich erstreckt sich über [mem_addr .. mem_addr+mem_size-1]. */
int allocmem_automatmman(struct automat_mman_t * mman, uint16_t mem_size, /*out*/void ** mem_addr);


// section: inline implementation


#endif
