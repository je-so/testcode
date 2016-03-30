/* title: Ad-hoc-Memory-Manager impl

   Implements <Ad-hoc-Memory-Manager>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/automat_mman.h
    Header file <Ad-hoc-Memory-Manager>.

   file: C-kern/proglang/automat_mman.c
    Implementation file <Ad-hoc-Memory-Manager impl>.
*/

#include "config.h"
#include "automat_mman.h"
#include "foreach.h"
#include "test_errortimer.h"

// === private types
struct memory_page_t;

// forward
#ifdef KONFIG_UNITTEST
static test_errortimer_t s_automat_mman_errtimer;
#endif


// struct: memory_page_t

typedef struct memory_page_t {
   /* variable: next
    * Verlinkt die Seiten zu einer Liste. */
   slist_node_t next;
   /* variable: data
    * Beginn der gespeicherten Daten. Diese sind auf den long Datentyp ausgerichtet. */
   long         data[1];
} memory_page_t;

// group: static variables

static size_t s_memory_page_sizeallocated = 0;

// group: constants

/* define: memory_page_SIZE
 * Die Größe in Bytes einer <memory_page_t>. */
#define memory_page_SIZE (256*1024)

// group: helper-types

slist_IMPLEMENT(_pagelist, memory_page_t, next.next)

// group: query

static inline size_t SIZEALLOCATED_PAGECACHE(void)
{
   return s_memory_page_sizeallocated;
}

// group: lifetime

/* function: new_memorypage
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von (*page)->data und (*page)->next sind undefiniert. */
static int new_memorypage(/*out*/memory_page_t ** page)
{
   int err;
   void * addr;

   if (! PROCESS_testerrortimer(&s_automat_mman_errtimer, &err)) {
      addr = malloc(memory_page_SIZE);
      err = !addr ? ENOMEM : 0;
   }

   if (err) goto ONERR;

   s_memory_page_sizeallocated += memory_page_SIZE; 

   // set out
   *page = (memory_page_t*) addr;

   return 0;
ONERR:
   return err;
}

/* function: free_automatmman
 * Gibt eine Speicherseite frei. */
static int delete_memorypage(memory_page_t * page)
{
   int err = 0;
   free(page);
   s_memory_page_sizeallocated -= memory_page_SIZE; 
   (void) PROCESS_testerrortimer(&s_automat_mman_errtimer, &err);
   return err;
}


// struct: automat_mman_t
struct automat_mman_t;
#if 0
// TODO: replace header with this private version
typedef struct automat_mman_t {
   slist_t  pagelist;   // list of allocated memory pages
   slist_t  pagecache;  // list of free pages not freed, waiting to be reused
   size_t   refcount;   // number of <automat_t> which use resources managed by this object
   uint8_t* freemem;    // freemem points to end of memory_page_t
                        // addr_next_free_memblock == freemem - freesize
   size_t   freesize;   // 0 <= freesize <= memory_page_SIZE - offsetof(memory_page_t, data)
   size_t   allocated;  // total amount of allocated memory
} automat_mman_t;
#endif

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_automat_mman_errtimer
 * Simuliert Fehler in Funktionen für <automat_t> und <automat_mman_t>. */
static test_errortimer_t   s_automat_mman_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

int free_automatmman(automat_mman_t * mman)
{
   int err = 0;
   int err2;

   // append mman->pagecache to mman->pagelist
   // and clear mman->pagecache
   insertlastPlist_pagelist(&mman->pagelist, &mman->pagecache);

   // free allocated pages
   slist_t pagelist = mman->pagelist;
   foreach (_pagelist, page, &pagelist) {
      err2 = delete_memorypage(page);
      if (err2) err = err2;
   }
   mman->pagelist = (slist_t) slist_INIT;
   mman->refcount = 0;
   mman->freemem  = 0;
   mman->freesize = 0;
   mman->allocated = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: resource helper

/* function: allocpage_automatmman
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von **page ist undefiniert. */
static inline int getfreepage_automatmman(automat_mman_t * mman, /*out*/memory_page_t ** free_page)
{
   int err;

   if (! isempty_slist(&mman->pagecache)) {
      err = removefirst_pagelist(&mman->pagecache, free_page);
   } else {
      err = new_memorypage(free_page);
   }
   if (err) goto ONERR;

   insertlast_pagelist(&mman->pagelist, *free_page);

   return 0;
ONERR:
   return err;
}

// group: query

size_t sizeallocated_automatmman(const automat_mman_t * mman)
{
   return mman->allocated;
}

size_t refcount_automatmman(const automat_mman_t * mman)
{
   return mman->refcount;
}


// group: update

/* function: incruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom aufrufenden <automat_t> benutzt. */
void incruse_automatmman(automat_mman_t * mman)
{
   ++ mman->refcount;
}

/* function: decruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom aufrufenden <automat_t> unbenutzt. */
void decruse_automatmman(automat_mman_t * mman)
{
   assert(mman->refcount > 0);
   -- mman->refcount;

   if (0 == mman->refcount) {
      insertlastPlist_pagelist(&mman->pagecache, &mman->pagelist);
      mman->freemem  = 0;
      mman->freesize = 0;
      mman->allocated = 0;
   }
}

// group: memory-allocation

int allocmem_automatmman(automat_mman_t * mman, uint16_t mem_size, /*out*/void ** mem_addr)
{
   int err;

   if (mman->freesize < mem_size) {
      memory_page_t * free_page;
      #define FREE_PAGE_SIZE (memory_page_SIZE - offsetof(memory_page_t, data))
      if (  FREE_PAGE_SIZE < UINT16_MAX && mem_size > FREE_PAGE_SIZE) {
         // free page does not meet demand of mem_size bytes which could as big as UINT16_MAX
         err = ENOMEM;
         goto ONERR;
      }
      err = getfreepage_automatmman(mman, &free_page);
      if (err) goto ONERR;
      mman->freemem  = memory_page_SIZE + (uint8_t*) free_page;
      mman->freesize = FREE_PAGE_SIZE;
      #undef FREE_PAGE_SIZE
   }

   size_t freesize = mman->freesize;
   mman->freesize  = freesize - mem_size;
   mman->allocated += mem_size;

   // set out
   *mem_addr = mman->freemem - freesize;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_memorypage(void)
{
   memory_page_t * page[10] = { 0 };
   const size_t    oldsize = SIZEALLOCATED_PAGECACHE();

   // === group constants

   // TEST memory_page_SIZE
   TEST(0 < memory_page_SIZE);
   TEST(ispowerof2_int(memory_page_SIZE));

   // === group helper types

   // TEST pagelist_t
   memory_page_t pageobj[10];
   slist_t       pagelist = slist_INIT;
   for (unsigned i = 0; i < lengthof(pageobj); ++i) {
      insertlast_pagelist(&pagelist, &pageobj[i]);
      TEST(&pageobj[i].next == pagelist.last);
      TEST(pageobj[i].next.next == &pageobj[0].next);
      TEST(pageobj[i ? i-1 : 0].next.next == &pageobj[i].next);
   }

   // === group lifetime

   // TEST new_memorypage
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == new_memorypage(&page[i]));
      // check env
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1) * memory_page_SIZE);
      // check page
      TEST(0 != page[i]);
   }

   // TEST delete_memorypage
   for (unsigned i = lengthof(page)-1; i < lengthof(page); --i) {
      TEST(0 == delete_memorypage(page[i]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i * memory_page_SIZE);
   }

   // TEST new_memorypage: simulated ERROR
   for (unsigned i = 10; i < 13; ++i) {
      memory_page_t * errpage = 0;
      init_testerrortimer(&s_automat_mman_errtimer, 1, (int)i);
      TEST((int)i == new_memorypage(&errpage));
      // check
      TEST(0 == errpage);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
   }

   // TEST delete_memorypage: simulated ERROR
   for (unsigned i = 10; i < 13; ++i) {
      TEST(0 == new_memorypage(&page[0]));
      init_testerrortimer(&s_automat_mman_errtimer, 1, (int)i);
      TEST((int)i == delete_memorypage(page[0]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   automat_mman_t mman = automat_mman_INIT;
   const size_t   oldsize = SIZEALLOCATED_PAGECACHE();
   void *         addr;

   // === group lifetime

   // TEST automat_mman_INIT
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);

   // TEST free_automatmman
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagelist, page);
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagecache, page);
   }
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + 6 * memory_page_SIZE);
   mman.freemem  = (void*) 1;
   mman.freesize = 10;
   mman.refcount = 1;
   mman.allocated = 1;
   TEST( 0 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);

   // TEST free_automatmman: double free
   TEST( 0 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);

   // TEST free_automatmman: simulated error
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST( 0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagelist, page);
      TEST( 0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagecache, page);
   }
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 6 * memory_page_SIZE);
   init_testerrortimer(&s_automat_mman_errtimer, 2, 7);
   mman.freemem  = (void*) 2;
   mman.freesize = 2;
   mman.refcount = 2;
   mman.allocated = 2;
   TEST( 7 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);
   // reset
   mman.refcount = 0;

   // === group query

   // TEST sizeallocated_automatmman
   TEST(0 == sizeallocated_automatmman(&mman));
   for (size_t i = 1; i; i <<= 1) {
      mman.allocated = i;
      TEST(i == sizeallocated_automatmman(&mman));
   }
   // reset
   mman.allocated = 0;

   // TEST refcount_automatmman
   TEST(0 == refcount_automatmman(&mman));
   for (size_t i = 1; i; i <<= 1) {
      mman.refcount = i;
      TEST(i == refcount_automatmman(&mman));
   }
   // reset
   mman.refcount = 0;

   // === group usage

   // TEST incruse_automatmman
   for (unsigned i = 1; i < 100; ++i) {
      automat_mman_t old;
      memcpy(&old, &mman, sizeof(mman));
      incruse_automatmman(&mman);
      // check mman refcount
      TEST( i == mman.refcount);
      // check mman nothing changed except refcount
      ++ old.refcount;
      TEST( 0 == memcmp(&old, &mman, sizeof(mman)));
   }

   // TEST decruse_automatmman
   for (size_t pl_size = 0; pl_size <= 3; pl_size += 3) {
      for (size_t pc_size = 0; pc_size <= 2; pc_size += 2) {
         memory_page_t * page[5] = { 0 };
         size_t          p_size  = 0;
         // prepare
         for (unsigned i = 0; i < pc_size; ++i, ++p_size) {
            TEST( 0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman.pagecache, page[p_size]);
         }
         for (unsigned i = 0; i < pl_size; ++i, ++p_size) {
            TEST(0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman.pagelist, page[p_size]);
         }
         mman.refcount = 10;
         mman.freemem  = (void*)1;
         mman.freesize = 9;
         mman.allocated = 8;
         automat_mman_t old;
         memcpy(&old, &mman, sizeof(mman));
         for (unsigned i = 9; i; --i) {
            // test refcount > 1
            decruse_automatmman(&mman);
            // check mman refcount
            TEST( i == mman.refcount);
            TEST( 1 == (uintptr_t)mman.freemem);
            TEST( 9 == mman.freesize);
            TEST( 8 == mman.allocated);
            // check mman other field not changed
            -- old.refcount;
            TEST(0 == memcmp(&old, &mman, sizeof(mman)));
         }
         // test refcount == 1
         decruse_automatmman(&mman);
         // check mman
         TEST( isempty_slist(&mman.pagelist));
         TEST( (p_size == 0) == isempty_slist(&mman.pagecache));
         TEST( 0 == mman.refcount);
         TEST( 0 == mman.freemem);
         TEST( 0 == mman.freesize);
         TEST( 0 == mman.allocated);
         slist_node_t * it = last_slist(&mman.pagecache);
         for (unsigned i = 0; i < p_size; ++i) {
            it = next_slist(it);
            TEST(it == (void*) page[i]);
         }
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   // === group resource helper

   // prepare
   mman = (automat_mman_t) automat_mman_INIT;
   memory_page_t * page[10];

   // TEST getfreepage_automatmman: pagecache is empty
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == getfreepage_automatmman(&mman, &page[i]));
      // check page
      TEST( page[i] == (void*) next_slist(&page[i?i-1:0]->next));
      TEST( page[0] == (void*) next_slist(&page[i]->next));
      // check mman
      TEST( page[i] == (void*) last_slist(&mman.pagelist));
      TEST( isempty_slist(&mman.pagecache));
      TEST( 0 == mman.refcount);
      TEST( 0 == mman.freemem);
      TEST( 0 == mman.freesize);
      TEST( 0 == mman.allocated);
   }

   // TEST getfreepage_automatmman: pagecache is not empty
   // prepare
   insertlastPlist_slist(&mman.pagecache, &mman.pagelist);
   for (unsigned i = 0; i < lengthof(page); ++i) {
      memory_page_t * cached_page;
      TEST(0 == getfreepage_automatmman(&mman, &cached_page));
      // check cached_page
      TEST(page[i] == cached_page);
      TEST(page[0] == (void*) next_slist(&cached_page->next));
      // check mman
      TEST(page[i] == (void*) last_slist(&mman.pagelist));
      if (i == lengthof(page)-1) {
         TEST( isempty_slist(&mman.pagecache));
      } else {
         TEST( page[i+1] == (void*) first_slist(&mman.pagecache));
      }
      TEST( 0 == mman.refcount);
      TEST( 0 == mman.freemem);
      TEST( 0 == mman.freesize);
      TEST( 0 == mman.allocated);
   }

   // TEST getfreepage_automatmman: simulated ERROR
   {
      memory_page_t * dummy = 0;
      automat_mman_t  old;
      memcpy(&old, &mman, sizeof(mman));
      TEST( isempty_slist(&mman.pagecache));
      init_testerrortimer(&s_automat_mman_errtimer, 1, 8);
      // test
      TEST( 8 == getfreepage_automatmman(&mman, &dummy));
      // check mman not changed
      TEST( 0 == memcmp(&old, &mman, sizeof(mman)));
      TEST( 0 == dummy);
   }

   // reset
   TEST( 0 == free_automatmman(&mman));
   mman = (automat_mman_t) automat_mman_INIT;

   // === group allocate resources

   // TEST allocmem_automatmman: size == 0 (no allocation)
   TEST( 0 == allocmem_automatmman(&mman, 0, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);
   // check addr
   TEST( 0 == addr);
   // check mman
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);

   // TEST allocmem_automatmman: size == 1 (allocate new page)
   TEST( 0 == allocmem_automatmman(&mman, 1, &addr));
   // check env
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + memory_page_SIZE);
   // check addr
   TEST(addr == (void*) last_pagelist(&mman.pagelist)->data);
   // check res
   TEST(!isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( mman.refcount == 0);
   TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
   TEST( mman.freesize == memory_page_SIZE - 1 - offsetof(memory_page_t, data));
   TEST( mman.allocated == 1);

   // TEST allocmem_automatmman: allocate cached page
   // prepare
   mman.freesize = 2;
   TEST(0 == new_memorypage(&page[0]));
   insertlast_pagelist(&mman.pagecache, page[0]);
   // test
   TEST( 0 == allocmem_automatmman(&mman, 3, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
   // check addr
   TEST( addr == (void*) page[0]->data);
   // check mman
   TEST( last_pagelist(&mman.pagelist) == page[0]);
   TEST( isempty_slist(&mman.pagecache));
   TEST( mman.refcount == 0);
   TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
   TEST( mman.allocated == 4);
   TEST( mman.freesize == memory_page_SIZE - 3 - offsetof(memory_page_t, data));

   // TEST allocmem_automatmman: allocate from current page
   for (unsigned i = 0, off = 3, S = 4; i <= UINT16_MAX; S += i, off += i, ++i) {
      if (256 == i) i = UINT16_MAX-2;
      TESTP( 0 == allocmem_automatmman(&mman, (uint16_t) i, &addr), "i:%d", i);
      // check env
      TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
      // check addr
      TEST( addr == off + (uint8_t*) page[0]->data);
      // check res
      TEST( last_pagelist(&mman.pagelist) == page[0]);
      TEST( isempty_slist(&mman.pagecache));
      TEST( mman.refcount == 0);
      TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
      TEST( mman.freesize == memory_page_SIZE - off - i - offsetof(memory_page_t, data));
      TEST( mman.allocated == S + i);
   }

   // reset
   TEST(0 == free_automatmman(&mman));

   return 0;
ONERR:
   free_automatmman(&mman);
   return EINVAL;
}

int unittest_proglang_automat_mman()
{
   if (test_memorypage())  goto ONERR;
   if (test_initfree())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
