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

#include "slist.h"

// === exported types
struct automat_mman_t; // opaque


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_automat_mman
 * Test <automat_mman_t> functionality. */
int unittest_proglang_automat_mman(void);
#endif


/* struct: automat_mman_t
 *
 * TODO: Replace automat_mman_t with general implementation stream_memory_manager_t
 *
 * TODO: make type private !!!
 * */
typedef struct automat_mman_t {
   slist_t  pagelist;   // list of allocated memory pages
   slist_t  pagecache;  // list of free pages not freed, waiting to be reused
   size_t   refcount;   // number of <automat_t> which use resources managed by this object
   uint8_t* freemem;    // freemem points to end of memory_page_t
                        // addr_of_next_free_memblock == freemem - freesize
   size_t   freesize;   // 0 <= freesize <= memory_page_SIZE - offsetof(memory_page_t, data)
   size_t   allocated;  // total amount of allocated memory
} automat_mman_t;

// group: lifetime

/* define: automat_mman_INIT
 * Statischer Initialisierer. */
#define automat_mman_INIT \
         { slist_INIT, slist_INIT, 0, 0, 0, 0 }

/* function: new_automatmman
 * TODO: implement => remove automat_mman_INIT
 * */
int new_automatmman(/*out*/struct automat_mman_t ** mman);

/* function: free_automatmman
 * Gibt alle belegten (Speicher-)Ressourcen frei.
 * TODO: Rename into delete_automatmman*/
int free_automatmman(struct automat_mman_t * mman);

// group: query

/* function: sizeallocated_automatmman
 * Gibt insgesamt allokierten Speicher in Bytes zurück. */
size_t sizeallocated_automatmman(const automat_mman_t * mman);

/* function: refcount_automatmman
 * Gibt Anzahl der Objekte an, die mman nutzen. */
size_t refcount_automatmman(const automat_mman_t * mman);

// group: update

/* function: incruse_automatmman
 * Erhöht die Anzahl derer, die mman nutzen. */
void incruse_automatmman(struct automat_mman_t * mman);

/* function: decruse_automatmman
 * Verringert die Anzahl derer, die mman nutzen. Darf nur nach einem vorherigen
 * Aufruf von <incruse_automatmman> aufgerufen werden. */
void decruse_automatmman(struct automat_mman_t * mman);

// group: memory-allocation

/* function: allocmem_automatmman
 * Allokiert mem_size bytes Speicher und gibt Startadresse in mem_addr zurück.
 * Der allokierte Speicherbereich erstreckt sich über [mem_addr .. mem_addr+mem_size-1]. */
int allocmem_automatmman(struct automat_mman_t * mman, uint16_t mem_size, /*out*/void ** mem_addr);


// section: inline implementation


#endif
