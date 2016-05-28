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
#include "slist.h"
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
    * Verlinkt die Seiten zu einer einfach verketteten Liste. */
   slist_node_t *next; // links this page to next page (single linked list)
   /* variable: data
    * Beginn der gespeicherten Daten. Diese sind auf den long Datentyp ausgerichtet. */
   long         data[1]; // start of allocated dat aon this page
} memory_page_t;

// group: static variables

static size_t s_memory_page_sizeallocated = 0;

// group: constants

/* define: memory_page_SIZE
 * Die Größe in Bytes einer <memory_page_t>. */
#define memory_page_SIZE (256*1024)

#define memory_page_FREESIZE \
         (memory_page_SIZE - offsetof(memory_page_t, data))

// group: helper-types

slist_IMPLEMENT(_pagelist, memory_page_t, next)

// group: query

size_t SIZEALLOCATED_PAGECACHE(void)
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
typedef struct automat_mman_t {
   slist_t  pagelist;   // list of allocated memory pages
   slist_t  pagecache;  // list of free pages not freed, waiting to be reused
   size_t   refcount;   // number of <automat_t> which use resources managed by this object
   uint8_t* freemem;    // freemem points to end of memory_page_t
                        // addr_next_free_memblock == freemem - freesize
   size_t   freesize;   // 0 <= freesize <= memory_page_SIZE - offsetof(memory_page_t, data)
   size_t   allocated;  // total amount of allocated memory
   size_t   wasted;     // total amount of allocated but unused memory (which has not been freed)
} automat_mman_t;

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_automat_mman_errtimer
 * Simuliert Fehler in Funktionen für <automat_t> und <automat_mman_t>. */
static test_errortimer_t   s_automat_mman_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

/* define: automat_mman_INIT
 * Statischer Initialisierer. */
#define automat_mman_INIT \
         { slist_INIT, slist_INIT, 0, 0, 0, 0, 0}

int new_automatmman(/*out*/struct automat_mman_t **mman)
{
   int err;
   automat_mman_t transient_mman = automat_mman_INIT;
   void * addr_mman;

   err = malloc_automatmman(&transient_mman, sizeof(automat_mman_t), &addr_mman);
   if (err) goto ONERR;

   *(automat_mman_t*)addr_mman = transient_mman;
   ((automat_mman_t*)addr_mman)->allocated = 0;

   // set out
   *mman = addr_mman;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int delete_automatmman(automat_mman_t **mman)
{
   int err;
   int err2;
   automat_mman_t * del_obj = *mman;

   if (del_obj) {
      *mman = 0;
      err = 0;

      // append mman->pagecache to mman->pagelist
      // and clear mman->pagecache
      insertlastPlist_pagelist(&del_obj->pagelist, &del_obj->pagecache);

      // free allocated pages
      slist_t pagelist = del_obj->pagelist;
      // del_obj is located on one of the memory pages
      // ==> do not access del_obj after deleting any memory page
      foreach (_pagelist, page, &pagelist) {
         err2 = delete_memorypage(page);
         if (err2) err = err2;
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: resource helper

/* function: allocpage_automatmman
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von **page ist undefiniert. */
static inline int getfreepage_automatmman(automat_mman_t *mman, /*out*/memory_page_t ** free_page)
{
   int err;

   if (! isempty_slist(&mman->pagecache)) {
      *free_page = removefirst_pagelist(&mman->pagecache);
   } else {
      err = new_memorypage(free_page);
      if (err) goto ONERR;
   }

   insertlast_pagelist(&mman->pagelist, *free_page);

   return 0;
ONERR:
   return err;
}

// group: query

size_t refcount_automatmman(const automat_mman_t *mman)
{
   return mman->refcount;
}

size_t sizeallocated_automatmman(const automat_mman_t *mman)
{
   return mman->allocated;
}

size_t wasted_automatmman(const struct automat_mman_t *mman)
{
   return mman->wasted;
}

// group: update

/* function: reset_automatmman
 * Gibt alle allokierten Ressourcen frei. */
void reset_automatmman(automat_mman_t *mman)
{
   memory_page_t * first_page;   // mman is located on first page
   first_page = removefirst_pagelist(&mman->pagelist);
   insertlastPlist_pagelist(&mman->pagecache, &mman->pagelist);
   initsingle_pagelist(&mman->pagelist, first_page);
   mman->freemem   = memory_page_SIZE + (uint8_t*) first_page;
   mman->freesize  = memory_page_SIZE - offsetof(memory_page_t, data) - sizeof(automat_mman_t);
   mman->allocated = 0;
   mman->wasted = 0;
}

/* function: incruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom aufrufenden <automat_t> benutzt. */
void incruse_automatmman(automat_mman_t *mman)
{
   ++ mman->refcount;
}

/* function: decruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom Aufrufer (<automat_t>) unbenutzt. */
size_t decruse_automatmman(automat_mman_t *mman)
{
   assert(mman->refcount > 0);
   -- mman->refcount;

   if (mman->refcount == 0) {
      reset_automatmman(mman);
   }

   return mman->refcount;
}

void incrwasted_automatmman(struct automat_mman_t *mman, size_t wasted)
{
   // should never overflow cause wasted is the amount of already allocated memory at max
   // ==> mman->wasted <= mman->allocated !
   mman->wasted += wasted;
}

// group: memory-allocation

int malloc_automatmman(automat_mman_t *mman, size_t mem_size, /*out*/void ** mem_addr)
{
   int err;

   if (mman->freesize < mem_size) {
      memory_page_t * free_page;
      if (mem_size > memory_page_FREESIZE) {
         // free page does not meet demand of mem_size bytes
         err = ENOMEM;
         goto ONERR;
      }
      err = getfreepage_automatmman(mman, &free_page);
      if (err) goto ONERR;
      mman->freemem  = memory_page_SIZE + (uint8_t*) free_page;
      mman->freesize = memory_page_FREESIZE;
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

int mfreelast_automatmman(struct automat_mman_t *mman, void *mem_addr)
{
   int err;
   size_t freesize = (size_t) (mman->freemem - (uint8_t*)mem_addr);
   size_t diff     = (freesize - mman->freesize);

   if (  mman->freemem - memory_page_FREESIZE <= (uint8_t*)mem_addr
         && (uint8_t*)mem_addr <= mman->freemem - mman->freesize
         && mman->allocated >= diff/*prevents deallocation of mman from first page*/) {
      mman->freesize   = freesize;
      mman->allocated -= diff;
   } else {
      err = EINVAL;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

void storestate_automatmman(struct automat_mman_t *mman, /*out*/automat_mman_state_t *state)
{
   state->last_page = last_pagelist(&mman->pagelist);
   state->freesize  = mman->freesize;
   state->allocated = mman->allocated;
   state->wasted    = mman->wasted;
}

void restore_automatmman(struct automat_mman_t *mman, const automat_mman_state_t *state)
{
   memory_page_t *lastpage = last_pagelist(&mman->pagelist);

   if (state->last_page != lastpage) {
      slist_t freelist;
      initsplit_pagelist(&freelist, &mman->pagelist, state->last_page);
      insertfirstPlist_pagelist(&mman->pagecache, &freelist);
   }

   mman->freemem   = memory_page_SIZE + (uint8_t*) state->last_page;
   mman->freesize  = state->freesize;
   mman->allocated = state->allocated;
   mman->wasted    = state->wasted;
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

   // TEST memory_page_FREESIZE
   TEST(memory_page_FREESIZE > 0);
   TEST(memory_page_FREESIZE < memory_page_SIZE);
   TEST(memory_page_FREESIZE == memory_page_SIZE - offsetof(memory_page_t, data));

   // === group helper types

   // TEST pagelist_t
   memory_page_t pageobj[10];
   slist_t       pagelist = slist_INIT;
   for (unsigned i = 0; i < lengthof(pageobj); ++i) {
      insertlast_pagelist(&pagelist, &pageobj[i]);
      TEST(&pageobj[i] == last_pagelist(&pagelist));
      TEST(&pageobj[0] == next_pagelist(&pageobj[i]));
      TEST(&pageobj[i] == next_pagelist(&pageobj[i ? i-1 : 0]));
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
   automat_mman_t *mman  = 0;
   automat_mman_t  mman2 = automat_mman_INIT;
   const size_t    oldsize = SIZEALLOCATED_PAGECACHE();
   const size_t    F = memory_page_SIZE - offsetof(memory_page_t, data) - sizeof(automat_mman_t);

   // === group lifetime

   // TEST automat_mman_INIT
   TEST( isempty_slist(&mman2.pagelist));
   TEST( isempty_slist(&mman2.pagecache));
   TEST( 0 == mman2.refcount);
   TEST( 0 == mman2.freemem);
   TEST( 0 == mman2.freesize);
   TEST( 0 == mman2.allocated);
   TEST( 0 == mman2.wasted);

   // TEST new_automatmman
   TEST( 0 == new_automatmman(&mman));
   // check mman
   uint8_t * P = mman->freemem - memory_page_SIZE + offsetof(memory_page_t, data);
   TEST( 0 != mman);
   TEST( P == (void*) mman);
   TEST( 0 != first_pagelist(&mman->pagelist));
   TEST( P == (void*) first_pagelist(&mman->pagelist)->data);
   TEST( P == (void*) last_pagelist(&mman->pagelist)->data);
   TEST( 0 == mman->refcount);
   TEST( 0 != mman->freemem);
   TEST( F == mman->freesize);
   TEST( 0 == mman->allocated);
   TEST( 0 == mman->wasted);
   // check pagecache
   TEST( oldsize + memory_page_SIZE == SIZEALLOCATED_PAGECACHE());

   // TEST delete_automatmman
   // prepare
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman->pagelist, page);
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman->pagecache, page);
   }
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + 7 * memory_page_SIZE);
   // test
   TEST( 0 == delete_automatmman(&mman));
   // check mman
   TEST( 0 == mman);
   // check pagecache
   TEST( oldsize == SIZEALLOCATED_PAGECACHE());

   // TEST delete_automatmman: double free
   TEST( 0 == delete_automatmman(&mman));
   TEST( 0 == mman);
   TEST( oldsize == SIZEALLOCATED_PAGECACHE());

   // TEST new_automatmman: simulated error
   init_testerrortimer(&s_automat_mman_errtimer, 1, 5);
   TEST( 5 == new_automatmman(&mman));
   TEST( 0 == mman);
   TEST( oldsize == SIZEALLOCATED_PAGECACHE());

   // TEST delete_automatmman: simulated error
   // prepare
   TEST(0 == new_automatmman(&mman));
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman->pagelist, page);
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman->pagecache, page);
   }
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 7 * memory_page_SIZE);
   init_testerrortimer(&s_automat_mman_errtimer, 2, 7);
   TEST( 7 == delete_automatmman(&mman));
   TEST( 0 == mman);
   TEST( oldsize == SIZEALLOCATED_PAGECACHE());

   // === group query

   // prepare
   TEST(0 == new_automatmman(&mman));

   // TEST refcount_automatmman
   TEST(0 == refcount_automatmman(mman));
   for (size_t i = 1; i; i <<= 1) {
      mman->refcount = i;
      TEST(i == refcount_automatmman(mman));
   }
   // reset
   mman->refcount = 0;

   // TEST sizeallocated_automatmman
   TEST(0 == sizeallocated_automatmman(mman));
   for (size_t i = 1; i; i <<= 1) {
      mman->allocated = i;
      TEST(i == sizeallocated_automatmman(mman));
   }
   // reset
   mman->allocated = 0;

   // TEST sizeallocated_automatmman
   TEST(0 == wasted_automatmman(mman));
   for (size_t i = 1; i; i <<= 1) {
      mman->wasted = i;
      TEST(i == wasted_automatmman(mman));
   }
   // reset
   mman->wasted = 0;

   // === group update

   // TEST incrwasted_automatmman
   for (unsigned i = 1, W = 1; i < 100; ++i, W += i) {
      automat_mman_t old;
      memcpy(&old, mman, sizeof(old));
      incrwasted_automatmman(mman, i);
      // check mman refcount
      TEST( W == mman->wasted);
      // check mman nothing changed except refcount
      old.wasted += i;
      TEST( 0 == memcmp(&old, mman, sizeof(old)));
   }

   // TEST incruse_automatmman
   for (unsigned i = 1; i < 100; ++i) {
      automat_mman_t old;
      memcpy(&old, mman, sizeof(old));
      incruse_automatmman(mman);
      // check mman refcount
      TEST( i == mman->refcount);
      // check mman nothing changed except refcount
      ++ old.refcount;
      TEST( 0 == memcmp(&old, mman, sizeof(old)));
   }

   // TEST decruse_automatmman
   for (size_t pl_size = 0; pl_size <= 3; pl_size += 3) {
      for (size_t pc_size = 0; pc_size <= 2; pc_size += 2) {
         memory_page_t * page[5] = { 0 };
         size_t          p_size  = 0;
         // prepare
         for (unsigned i = 0; i < pc_size; ++i, ++p_size) {
            TEST( 0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman->pagecache, page[p_size]);
         }
         for (unsigned i = 0; i < pl_size; ++i, ++p_size) {
            TEST(0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman->pagelist, page[p_size]);
         }
         mman->refcount  = 10;
         mman->freemem   = (void*)1;
         mman->freesize  = 9;
         mman->allocated = 8;
         mman->wasted    = 7;
         automat_mman_t old;
         memcpy(&old, mman, sizeof(old));
         for (unsigned i = 9; i; --i) {
            // test refcount > 1
            TEST( i == decruse_automatmman(mman));
            // check mman refcount
            TEST( i == mman->refcount);
            TEST( 1 == (uintptr_t)mman->freemem);
            TEST( 9 == mman->freesize);
            TEST( 8 == mman->allocated);
            TEST( 7 == mman->wasted);
            // check mman other field not changed
            -- old.refcount;
            TEST(0 == memcmp(&old, mman, sizeof(old)));
         }
         // test refcount == 1
         TEST( 0 == decruse_automatmman(mman));
         // check mman
         TEST( ! isempty_slist(&mman->pagelist));
         TEST( isempty_slist(&mman->pagecache) == (p_size == 0));
         TEST( mman->refcount == 0);
         TEST( mman->freemem  == (uint8_t*)mman + sizeof(automat_mman_t) + F);
         TEST( mman->freesize  == F);
         TEST( mman->allocated == 0);
         TEST( mman->wasted == 0);
         // check mman->pagelist content
         TEST( last_pagelist(&mman->pagelist) == first_pagelist(&mman->pagelist));
         TEST( last_pagelist(&mman->pagelist)->data == (void*) mman);
         // check mman->pagecache content
         slist_node_t * it = last_slist(&mman->pagecache);
         for (unsigned i = 0; i < p_size; ++i) {
            it = next_slist(it);
            TEST( it == (void*) page[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST reset_automatmman
   for (size_t pl_size = 0; pl_size <= 3; pl_size += 3) {
      for (size_t pc_size = 0; pc_size <= 2; pc_size += 2) {
         memory_page_t * page[5] = { 0 };
         size_t          p_size  = 0;
         // prepare
         for (unsigned i = 0; i < pc_size; ++i, ++p_size) {
            TEST( 0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman->pagecache, page[p_size]);
         }
         for (unsigned i = 0; i < pl_size; ++i, ++p_size) {
            TEST(0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman->pagelist, page[p_size]);
         }
         mman->freemem   = (void*)0;
         mman->freesize  = 9;
         mman->allocated = 8;
         mman->wasted    = 7;
         // test
         reset_automatmman(mman);
         // check mman
         TEST( ! isempty_slist(&mman->pagelist));
         TEST( isempty_slist(&mman->pagecache) == (p_size == 0));
         TEST( mman->refcount == 0);
         TEST( mman->freemem  == (uint8_t*)mman + sizeof(automat_mman_t) + F);
         TEST( mman->freesize  == F);
         TEST( mman->allocated == 0);
         TEST( mman->wasted == 0);
         // check mman->pagelist content
         TEST( last_pagelist(&mman->pagelist) == first_pagelist(&mman->pagelist));
         TEST( last_pagelist(&mman->pagelist)->data == (void*) mman);
         // check mman->pagecache content
         slist_node_t * it = last_slist(&mman->pagecache);
         for (unsigned i = 0; i < p_size; ++i) {
            it = next_slist(it);
            TEST( it == (void*) page[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // === group resource helper

   // prepare
   memory_page_t * page[10];

   // TEST getfreepage_automatmman: pagecache is empty
   page[0] = last_pagelist(&mman->pagelist);
   P = (uint8_t*) page[0]->data + sizeof(automat_mman_t);
   for (unsigned i = 1; i < lengthof(page); ++i) {
      TEST( 0 == getfreepage_automatmman(mman, &page[i]));
      // check page
      TEST( page[i] == next_pagelist(page[i-1]));
      TEST( page[0] == next_pagelist(page[i]));
      // check mman
      TEST( page[i] == last_pagelist(&mman->pagelist));
      TEST( isempty_slist(&mman->pagecache));
      TEST( mman->refcount == 0);
      TEST( mman->freemem  == P + F)
      TEST( mman->freesize  == F);
      TEST( mman->allocated == 0);
   }

   // TEST getfreepage_automatmman: pagecache is not empty
   // prepare
   insertlastPlist_slist(&mman->pagecache, &mman->pagelist);
   for (unsigned i = 0; i < lengthof(page); ++i) {
      memory_page_t * cached_page;
      TEST( 0 == getfreepage_automatmman(mman, &cached_page));
      // check cached_page
      TEST( page[i] == cached_page);
      TEST( page[0] == next_pagelist(cached_page));
      // check mman
      TEST( page[i] == last_pagelist(&mman->pagelist));
      if (i == lengthof(page)-1) {
         TEST( isempty_slist(&mman->pagecache));
      } else {
         TEST( page[i+1] == first_pagelist(&mman->pagecache));
      }
      TEST( mman->refcount == 0);
      TEST( mman->freemem  == P + F)
      TEST( mman->freesize  == F);
      TEST( mman->allocated == 0);
   }

   // TEST getfreepage_automatmman: simulated ERROR
   {
      automat_mman_t  old;
      memcpy(&old, mman, sizeof(old));
      TEST(isempty_slist(&mman->pagecache));
      // test
      memory_page_t * dummy = 0;
      init_testerrortimer(&s_automat_mman_errtimer, 1, 8);
      TEST( 8 == getfreepage_automatmman(mman, &dummy));
      // check out parameter
      TEST( 0 == dummy);
      // check mman: not changed
      TEST( 0 == memcmp(&old, mman, sizeof(old)));
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // reset
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}


static int test_allocate(void)
{
   automat_mman_t *mman = 0;
   const size_t    oldsize = SIZEALLOCATED_PAGECACHE();
   const size_t    F = memory_page_SIZE - offsetof(memory_page_t, data) - sizeof(automat_mman_t);
   void *          addr;
   memory_page_t * page;

   // prepare
   TEST( 0 == new_automatmman(&mman));
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + memory_page_SIZE);

   // TEST malloc_automatmman: size == 0/1
   for (unsigned size = 0; size <= 1; ++size) {
      TEST( 0 == malloc_automatmman(mman, (uint16_t)size, &addr));
      // check env
      TEST( SIZEALLOCATED_PAGECACHE() == oldsize + memory_page_SIZE);
      // check addr
      uint8_t * P = (uint8_t*)mman + sizeof(automat_mman_t);
      TEST( addr == P);
      // check mman
      TEST( first_pagelist(&mman->pagelist) != 0);
      TEST( first_pagelist(&mman->pagelist)->data == (void*)mman);
      TEST( last_pagelist(&mman->pagelist)->data  == (void*)mman);
      TEST( isempty_slist(&mman->pagecache));
      TEST( mman->refcount == 0);
      TEST( mman->freemem  == P + mman->freesize + size);
      TEST( mman->freesize == F - size);
      TEST( mman->allocated == size);
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST malloc_automatmman: allocate new page
   // prepare
   mman->freesize = 4;
   // test
   TEST( 0 == malloc_automatmman(mman, 5, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2*memory_page_SIZE);
   // check addr
   TEST( addr == last_pagelist(&mman->pagelist)->data);
   // check mman
   TEST( first_pagelist(&mman->pagelist) != 0);
   TEST( first_pagelist(&mman->pagelist)->data == (void*)mman);
   TEST( last_pagelist(&mman->pagelist)->data  != (void*)mman);
   TEST( isempty_slist(&mman->pagecache));
   TEST( mman->refcount == 0);
   TEST( mman->freemem  == (uint8_t*)addr + mman->freesize + 5);
   TEST( mman->freesize == memory_page_SIZE - offsetof(memory_page_t, data) - 5);
   TEST( mman->allocated == 5);
   // reset
   TEST(0 == delete_automatmman(&mman));
   TEST(0 == new_automatmman(&mman));

   // TEST malloc_automatmman: allocate cached page
   // prepare
   mman->freesize = 2;
   TEST(0 == new_memorypage(&page));
   insertlast_pagelist(&mman->pagecache, page);
   // test
   TEST( 0 == malloc_automatmman(mman, 3, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
   // check addr
   TEST( addr == page->data);
   // check mman
   TEST( last_pagelist(&mman->pagelist) == page);
   TEST( isempty_slist(&mman->pagecache));
   TEST( mman->refcount == 0);
   TEST( mman->freemem  == memory_page_SIZE + (uint8_t*) page);
   TEST( mman->allocated == 3);
   TEST( mman->freesize == memory_page_SIZE - 3 - offsetof(memory_page_t, data));

   // TEST malloc_automatmman: allocate from current page
   for (unsigned i = 0, off = 3, A = 3; i <= UINT16_MAX; A += i, off += i, ++i) {
      if (256 == i) i = UINT16_MAX-2;
      TESTP( 0 == malloc_automatmman(mman, (uint16_t) i, &addr), "i:%d", i);
      // check env
      TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
      // check addr
      TEST( addr == off + (uint8_t*) page->data);
      // check res
      TEST( last_pagelist(&mman->pagelist) == page);
      TEST( isempty_slist(&mman->pagecache));
      TEST( mman->refcount == 0);
      TEST( mman->freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman->pagelist));
      TEST( mman->freesize == memory_page_SIZE - off - i - offsetof(memory_page_t, data));
      TEST( mman->allocated == A + i);
   }
   // reset
   TEST(0 == delete_automatmman(&mman));
   TEST(0 == new_automatmman(&mman));

   // TEST mfreelast_automatmman: free last allocation
   // prepare
   automat_mman_t old = *mman;
   memcpy(&old, mman, sizeof(old));
   TEST(0 == malloc_automatmman(mman, 0, &addr));
   TEST(0 == memcmp(&old, mman, sizeof(old)));
   for (unsigned i = 0; i <= UINT16_MAX; i <<= 1, ++i) {
      // prepare
      TEST(0 == malloc_automatmman(mman, (uint16_t)i, &addr));
      // test
      TEST( 0 == mfreelast_automatmman(mman, addr));
      // check mman not changed
      TEST( 0 == memcmp(&old, mman, sizeof(old)));
   }

   // TEST mfreelast_automatmman: EINVAL
   TEST( EINVAL == mfreelast_automatmman(mman, (uint8_t*)addr -1));
   TEST( EINVAL == mfreelast_automatmman(mman, (uint8_t*)addr +1));

   // reset
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}

static int test_restore(void)
{
   automat_mman_t *mman = 0;
   automat_mman_t oldmman;
   automat_mman_state_t state;
   const size_t    oldsize = SIZEALLOCATED_PAGECACHE();
   void           *addr;
   memory_page_t  *first, *last;

   // prepare
   TEST( 0 == new_automatmman(&mman));
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + memory_page_SIZE);
   first = last = last_pagelist(&mman->pagelist);

   // TEST storestate_automatmman: single page
   for (size_t wasted = 0; wasted <= 256; wasted += 128) {
      for (size_t allocated = 0; allocated <= 256; allocated += 64) {
         // prepare
         TEST(0 == malloc_automatmman(mman, allocated, &addr));
         incrwasted_automatmman(mman, wasted);
         memcpy(&oldmman, mman, sizeof(oldmman));
         // test
         storestate_automatmman(mman, &state);
         // check mman not changed
         TEST( memcmp(&oldmman, mman, sizeof(oldmman)) == 0);
         TEST( last_pagelist(&mman->pagelist) == last);
         // check state
         TEST( state.last_page == last_pagelist(&mman->pagelist));
         TEST( state.freesize  == mman->freesize);
         TEST( state.allocated == mman->allocated);
         TEST( state.wasted    == mman->wasted);
         // check last
         TEST( next_pagelist(last) == last);

         // reset
         reset_automatmman(mman);
      }
   }

   // TEST restore_automatmman: on same page
   // prepare
   memcpy(&oldmman, mman, sizeof(oldmman));
   storestate_automatmman(mman, &state);
   for (size_t wasted = 0; wasted <= 1024; wasted *= 2, wasted += 1) {
      for (size_t allocated = 0; allocated < 65536; allocated *= 2, allocated += 1) {
         TEST(0 == malloc_automatmman(mman, allocated, &addr));
         incrwasted_automatmman(mman, wasted);
         // test
         restore_automatmman(mman, &state);
         // check mman restored
         TEST( memcmp(&oldmman, mman, sizeof(oldmman)) == 0);
         TEST( last_pagelist(&mman->pagelist) == last);
         // check page
         TEST( next_pagelist(last) == last);
      }
   }

   // TEST storestate_automatmman: multiple pages
   for (unsigned nrblock = 1; nrblock < 16; ++nrblock) {
      // prepare
      last = last_pagelist(&mman->pagelist);
      TEST(malloc_automatmman(mman, memory_page_FREESIZE-nrblock, &addr) == 0);
      for (size_t wasted = 0; wasted <= 1234; wasted += 1234) {
         incrwasted_automatmman(mman, wasted);
         memcpy(&oldmman, mman, sizeof(oldmman));
         // test
         storestate_automatmman(mman, &state);
         // check mman not changed
         TEST( memcmp(&oldmman, mman, sizeof(oldmman)) == 0);
         TEST( last_pagelist(&mman->pagelist)  != last);
         TEST( first_pagelist(&mman->pagelist) == first);
         // check state
         TEST( state.last_page == last_pagelist(&mman->pagelist));
         TEST( state.freesize  == mman->freesize);
         TEST( state.allocated == mman->allocated);
         TEST( state.wasted    == mman->wasted);
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST restore_automatmman: multiple pages
   for (unsigned nrbefore = 0; nrbefore < 15; ++nrbefore) {
      memory_page_t *pages[16];
      unsigned nrpages = 0;
      pages[nrpages++] = first;
      foreach (_pagelist, p, &mman->pagecache) {
         TEST(nrpages < lengthof(pages));
         pages[nrpages++] = p;
      }
      TEST(nrpages == 16);
      // prepare
      for (unsigned i = 0; i < nrbefore; ++i) {
         TEST(malloc_automatmman(mman, memory_page_FREESIZE-i, &addr) == 0);
      }
      oldmman = *mman;
      storestate_automatmman(mman, &state);
      for (unsigned nrblock = nrbefore+1; nrblock < 16; ++nrblock) {
         for (unsigned i = nrbefore; i < nrblock; ++i) {
            TEST(malloc_automatmman(mman, memory_page_FREESIZE-i, &addr) == 0);
         }
         incrwasted_automatmman(mman, 124);
         // test
         restore_automatmman(mman, &state);
         // check mman
         TEST( first_pagelist(&mman->pagelist)  == first);
         TEST( first_pagelist(&mman->pagecache) == pages[nrbefore+1]);
         TEST( mman->freesize  == oldmman.freesize);
         TEST( mman->refcount  == oldmman.refcount);
         TEST( mman->freemem   == oldmman.freemem);
         TEST( mman->freesize  == oldmman.freesize);
         TEST( mman->allocated == oldmman.allocated);
         TEST( mman->wasted    == oldmman.wasted);
         // check mman->pagelist
         unsigned i = 0;
         foreach (_pagelist, p, &mman->pagelist) {
            TEST( i <= nrbefore);
            TEST( p == pages[i++]);
         }
         TEST( i == nrbefore+1);
         // check mman->pagecache
         foreach (_pagelist, p, &mman->pagecache) {
            TEST( i < lengthof(pages));
            TEST( p == pages[i++]);
         }
         TEST( i == lengthof(pages));
      }
      // reset
      reset_automatmman(mman);
   }

   // reset
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}

int unittest_proglang_automat_mman()
{
   if (test_memorypage())  goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_allocate())    goto ONERR;
   if (test_restore())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
