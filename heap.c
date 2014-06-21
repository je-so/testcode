/* title: Heap impl

   Implements <Heap>.

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

   file: C-kern/api/ds/inmem/heap.h
    Header file <Heap>.

   file: C-kern/ds/inmem/heap.c
    Implementation file <Heap impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/heap.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/memory/vm.h" // TODO: remove
#endif


// section: heap_t

// group: macros

// TODO: change implementation of swaptype (measure time!!)

#define INITSWAPTYPE \
         const int swaptype = ((heap->elemsize == sizeof(long)) ? 0 : (heap->elemsize % sizeof(long)) ? 2 : 1)

// TODO: change implementation of copytype (measure time!!)

#define INITCOPYTYPE \
         const int copytype = 2

#define ELEM(offset) \
         &heap->array[(offset)]

/* define: COPY
 * TODO: */
#define COPY(dest, src) \
         if (copytype == 0) { \
            *(long*)(dest) = *(const long*)(src); \
         } else if (copytype == 1) { \
            unsigned _nr = heap->elemsize / sizeof(long); \
            for (unsigned _i = 0; _i < _nr; ++_i) { \
               ((long*)(dest))[_i] = ((const long*)(src))[_i]; \
            } \
         } else { \
            unsigned _nr = heap->elemsize; \
            for (unsigned _i = 0; _i < _nr; ++_i) { \
               ((uint8_t*)(dest))[_i] = ((const uint8_t*)(src))[_i]; \
            } \
         }

/* define: SWAP
 * TODO: describe
 * TODO: change implementation of byte swap (measure time !!)
 * */
#define SWAP(parent, child) \
         if (swaptype == 0) { \
            long temp; \
            temp = *((long*)(heap->array+parent)); \
            *((long*)(heap->array+parent)) = *((long*)(heap->array+child)); \
            *((long*)(heap->array+child))  = temp; \
         } else if (swaptype == 1) { \
            unsigned _nr = heap->elemsize / sizeof(long); \
            uint8_t * _parent = (heap->array + (parent)); \
            size_t  _childoff = ((child) - (parent)); \
            do { \
               long temp = *(long*)_parent; \
               *(long*)_parent = *(long*)(_parent + _childoff); \
               *(long*)(_parent + _childoff) = temp; \
               _parent += sizeof(long); \
            } while (--_nr); \
         } else { \
            unsigned _nr = heap->elemsize; \
            uint8_t * _parent = (heap->array + (parent)); \
            size_t  _childoff = ((child) - (parent)); \
            do { \
               uint8_t temp = *_parent; \
               *_parent = *(_parent + _childoff); \
               *(_parent + _childoff) = temp; \
               ++_parent; \
            } while (--_nr); \
         }

/* define: ISSWAP
 * TODO: */
#define ISSWAP(parent, child) \
         ((heap->cmp(heap->cmpstate, parent, child) < 0) ^ isminheap)


// group: query

int invariant_heap(const heap_t * heap)
{
   if (heap->nrofelem > heap->maxnrofelem) goto ONERR;

   if (heap->nrofelem <= 1) return 0;

   const int isminheap = heap->isminheap;
   size_t cmaxoff = heap->nrofelem * heap->elemsize;
   size_t pmaxoff = (heap->nrofelem / 2) * heap->elemsize;
   size_t parent  = pmaxoff - heap->elemsize;
   size_t child;

   child = 2 * (parent + heap->elemsize);
   if (child < cmaxoff && ISSWAP(ELEM(parent), ELEM(child))) goto ONERR;

   child -= heap->elemsize;
   if (ISSWAP(ELEM(parent), ELEM(child))) goto ONERR;

   while (parent) {
      child   = 2 * parent;
      parent -= heap->elemsize;
      if (ISSWAP(ELEM(parent), ELEM(child))) goto ONERR;

      child -= heap->elemsize;
      if (ISSWAP(ELEM(parent), ELEM(child))) goto ONERR;
   }

   return 0;
ONERR:
   TRACEABORT_ERRLOG(EINVARIANT);
   return EINVARIANT;
}

// group: update

/* function: build_heap
 * Builds heap structure according to heap condition.
 *
 * Max Heap Condition:
 * For all 0 <= parent && (2*parent+1) < nrofelem
 * - ( array[parent*elemsize] >= array[(2*parent+1)*elemsize]
 *     && ((2*parent+2) > nrofelem || array[parent*elemsize] >= array[(2*parent+2)*elemsize])
 *
 * Min Heap Condition:
 * For all 0 <= parent && (2*parent+1) < nrofelem
 * - ( array[parent*elemsize] <= array[(2*parent+1)*elemsize]
 *     && ((2*parent+2) > nrofelem || array[parent*elemsize] <= array[(2*parent+2)*elemsize])
 *
 * Geometric Series:
 * The sum of the inifinite series
 * > s = 1 + r + r*r + r*r*r + ...
 * > s = pow(r, 0) + pow(r, 1) + pow(r, 2) + ... + pow(r, h) + ...
 * converges to (if abs(r) < 1 && h goes to inifinity):  1/(1-r)
 *
 * You can derive s (d/dr): s = 1 + 2*r + 3*r*r + ... + h * pow(r, h-1) + ...
 * and the sum converges to (same condition as above): 1/((1-r)*(1-r))
 *
 * Time O(n):
 * > time = 0;
 * > for (height = 0; height < log2(n); ++height) {
 * >    time += height * pow(2, log2(n)-1-height);
 * > }
 * >
 * > // with:  pow(2, log2(n)-1-height)
 * > // == n / pow(2, 1+height)
 * > // == n / 4 / pow(2, height-1)
 * > // == (n / 4) * pow(1.0/2.0, height-1)
 * >
 * > time = 0;
 * > for (height = 0; height < log2(n); ++height) {
 * >    time += height * pow(1.0/2.0, height-1);
 * > }
 * > time *= n / 4;
 *
 * With the knowledge of the geometric series the resulting
 * time is always less than (height goes to inifinity):
 * > (n / 4) * (1 / ((1-1/2)*(1-1/2))) == n
 *
 * */
static void build_heap(heap_t * heap)
{
   if (heap->nrofelem <= 1) return;

   INITSWAPTYPE;
   const int isminheap = heap->isminheap;
   const size_t cmaxoff = heap->nrofelem * heap->elemsize;
   const size_t pmaxoff = (heap->nrofelem / 2) * heap->elemsize;
   size_t parent   = pmaxoff - heap->elemsize;
   size_t swapelem = parent;
   size_t child;

   child = 2 * (parent + heap->elemsize);
   if (child < cmaxoff && ISSWAP(ELEM(parent), ELEM(child))) {
      swapelem  = child;
   }

   child -= heap->elemsize;
   if (ISSWAP(ELEM(swapelem), ELEM(child))) {
      swapelem = child;
   }

   if (parent != swapelem) {
      SWAP(parent, swapelem);
   }

   while (parent) {
      child   = 2 * parent;
      parent -= heap->elemsize;
      swapelem = parent;
      if (ISSWAP(ELEM(parent), ELEM(child))) {
         swapelem = child;
      }

      child -= heap->elemsize;
      if (ISSWAP(ELEM(swapelem), ELEM(child))) {
         swapelem = child;
      }

      size_t parent2 = parent;
      while (parent2 != swapelem) {
         SWAP(parent2, swapelem);

         parent2 = swapelem;
         if (parent2 >= pmaxoff) break;

         child = 2 * (parent2 + heap->elemsize);
         swapelem = parent2;
         if (child < cmaxoff && ISSWAP(ELEM(parent2), ELEM(child))) {
            swapelem = child;
         }

         child -= heap->elemsize;
         if (ISSWAP(ELEM(swapelem), ELEM(child))) {
            swapelem = child;
         }
      }
   }
}

int insert_heap(heap_t * heap, const void * new_elem)
{
   if (heap->nrofelem == heap->maxnrofelem) return ENOMEM;

   INITCOPYTYPE;
   const int isminheap = heap->isminheap;

   unsigned i = heap->nrofelem ++;
   unsigned child = i * heap->elemsize;

   while (i > 0) {
      i = (i - 1) / 2;

      unsigned parent = i * heap->elemsize;
      if (! ISSWAP(ELEM(parent), new_elem)) break;

      COPY(heap->array+child, heap->array+parent);
      child = parent;
   }

   memcpy(heap->array+child, new_elem, heap->elemsize);

   return 0;
}

int remove_heap(heap_t * heap, /*out*/void * removed_elem)
{
   if (heap->nrofelem == 0) return ENODATA;

   // remove first element (min or max)
   memcpy(removed_elem, heap->array, heap->elemsize);

   if (-- heap->nrofelem) {

      INITCOPYTYPE;
      const int isminheap = heap->isminheap;
      const size_t cmaxoff = heap->nrofelem * heap->elemsize;
      const size_t pmaxoff = (heap->nrofelem / 2) * heap->elemsize;

      unsigned parent = 0;

      while (parent < pmaxoff) {
         unsigned child = (2 * parent) + heap->elemsize;

         size_t swapelem = cmaxoff;
         if (ISSWAP(ELEM(swapelem), ELEM(child))) {
            swapelem = child;
         }

         child += heap->elemsize;
         if (child < cmaxoff && ISSWAP(ELEM(swapelem), ELEM(child))) {
            swapelem = child;
         }

         if (swapelem == cmaxoff) break;

         void * dest = heap->array + parent;
         void * src  = heap->array + swapelem;
         parent = swapelem;
         COPY(dest, src);
      }

      void * dest = heap->array + parent;
      void * src  = heap->array + cmaxoff;
      COPY(dest, src);
   }

   return 0;
}

// group: lifetime

static int init2_heap(/*out*/heap_t * heap, uint8_t ismin, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate)
{
   int err;

   VALIDATE_INPARAM_TEST(cmp != 0 && elemsize > 0 && nrofelem <= maxnrofelem && 0 < maxnrofelem, ONERR, );
   VALIDATE_INPARAM_TEST(maxnrofelem <= (size_t)-1 / elemsize && (uintptr_t)array + maxnrofelem * elemsize > (uintptr_t)array, ONERR, );

   heap->cmp = cmp;
   heap->cmpstate = cmpstate;
   heap->elemsize = elemsize;
   heap->isminheap = ismin;
   heap->array = array;
   heap->nrofelem = nrofelem;
   heap->maxnrofelem = maxnrofelem;

   build_heap(heap);

   return 0;
ONERR:
   return err;
}

int initmin_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate)
{
   int err;

   err = init2_heap(heap, true, elemsize, nrofelem, maxnrofelem, array, cmp, cmpstate);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEABORT_ERRLOG(err);
   return err;
}

int initmax_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate)
{
   int err;

   err = init2_heap(heap, false, elemsize, nrofelem, maxnrofelem, array, cmp, cmpstate);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEABORT_ERRLOG(err);
   return err;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int compare_long(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   long l = *(const long*)left;
   long r = *(const long*)right;
   return (l < r) ? -1 : (l > r) ? +1 : 0;
}

static int compare_byte(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   uint8_t l = *(const uint8_t*)left;
   uint8_t r = *(const uint8_t*)right;
   return (l < r) ? -1 : (l > r) ? +1 : 0;
}

static int test_initfree(void)
{
   heap_t heap = heap_FREE;

   // TEST heap_FREE
   TEST(0 == heap.cmp);
   TEST(0 == heap.cmpstate);
   TEST(0 == heap.elemsize);
   TEST(0 == heap.isminheap);
   TEST(0 == heap.array);
   TEST(0 == heap.nrofelem);
   TEST(0 == heap.maxnrofelem);

   // TEST free_heap
   memset(&heap, 255, sizeof(heap));
   TEST(0 == free_heap(&heap));
   // only this field is cleared
   TEST(0 == heap.maxnrofelem);

   // TEST initmin_heap, initmax_heap: EINVAL
   {
      uint8_t elemsize = 0;
      uint8_t nrelem    = 2;
      uint8_t maxnrelem = 1;
      // compare function == 0
      TEST(EINVAL == initmax_heap(&heap, 1, 0, 1, (void*)1, 0, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, 1, 0, 1, (void*)1, 0, (void*)1));
      // elemsize == 0
      TEST(EINVAL == initmax_heap(&heap, elemsize, 0, 1, (void*)1, &compare_long, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, elemsize, 0, 1, (void*)1, &compare_long, (void*)1));
      // nrelem > maxnrelem
      TEST(nrelem > maxnrelem);
      TEST(EINVAL == initmax_heap(&heap, 1, nrelem, maxnrelem, (void*)1, &compare_long, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, 1, nrelem, maxnrelem, (void*)1, &compare_long, (void*)1));
      // maxnrelem == 0
      TEST(EINVAL == initmax_heap(&heap, 1, 0, 0, (void*)1, &compare_long, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, 1, 0, 0, (void*)1, &compare_long, (void*)1));
      // maxnrelem * elemsize overflows size_t
      TEST(EINVAL == initmax_heap(&heap, 3, 0, SIZE_MAX/3+1, (void*)1, &compare_long, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, 3, 0, SIZE_MAX/3+1, (void*)1, &compare_long, (void*)1));
      // array + maxnrelem * elemsize overflows
      TEST(EINVAL == initmax_heap(&heap, 1, 0, SIZE_MAX, (void*)1, &compare_long, (void*)1));
      TEST(EINVAL == initmin_heap(&heap, 1, 0, SIZE_MAX, (void*)1, &compare_long, (void*)1));
   }

   // TEST initmin_heap, initmax_heap: init data field
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned i = 1; i < 256; ++i) {
         memset(&heap, 0, sizeof(heap));
         switch (ismin) {
         case 0: TEST(0 == initmax_heap(&heap, (uint8_t)i, 0, 1+i, (void*)i, &compare_long, (void*)(2*i))); break;
         case 1: TEST(0 == initmin_heap(&heap, (uint8_t)i, 0, 1+i, (void*)i, &compare_long, (void*)(2*i))); break;
         }
         TEST(heap.cmp == &compare_long);
         TEST(heap.cmpstate == (void*)(2*i));
         TEST(heap.elemsize == i);
         TEST(heap.isminheap == ismin);
         TEST(heap.array    == (void*)i);
         TEST(heap.nrofelem == 0);
         TEST(heap.maxnrofelem == 1+i);
      }
   }

   // TEST initmin_heap, initmax_heap: build heap from ascending / descending elements
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      long array[5*255];
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned len = 1; len <= 255; ++len) {
               for (unsigned isasc = 0; isasc <= 1;  ++isasc) {
                  memset(array, 0, sizeof(array));
                  for (unsigned i = 0, val = isasc ? 0 : 254; i < 255; ++i, val += (unsigned)(isasc ? 1 : -1)) {
                     if (basesize == 1) {
                        ((uint8_t*)array)[i*elemsize] = (uint8_t) val;
                     } else {
                        array[i*elemsize/sizeof(long)] = (long) val;
                     }
                  }
                  switch (ismin) {
                  case 0: TEST(0 == initmax_heap(&heap, (uint8_t)elemsize, len, len, array, basesize == 1 ? &compare_byte : &compare_long, 0)); break;
                  case 1: TEST(0 == initmin_heap(&heap, (uint8_t)elemsize, len, len, array, basesize == 1 ? &compare_byte : &compare_long, 0)); break;
                  }
                  long expect = (long) (ismin ? (isasc ? 0 : 255-len) : (isasc ? len-1 : 254));
                  if (basesize == 1) {
                     TEST(expect == ((uint8_t*)array)[0]);
                  } else {
                     TEST(expect == array[0]);
                  }
                  TEST(0 == invariant_heap(&heap));
               }
            }
         }
      }
   }

   // TEST initmin_heap, initmax_heap: build heap from random order
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      long array[5*255];
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            memset(array, 0, sizeof(array));
            for (unsigned i = 0; i < 255; ++i) {
               if (basesize == 1) {
                  ((uint8_t*)array)[i*elemsize] = (uint8_t)i;
               } else {
                  array[i*elemsize/sizeof(long)] = (long)i;
               }
            }
            for (unsigned i = 0; i < 255; ++i) {
               uint8_t * a = (uint8_t*)array;
               long temp[5];
               unsigned r = ((unsigned) random()) % 255;
               memcpy(temp, a+elemsize*r, elemsize);
               memcpy(a+elemsize*r, a+elemsize*i, elemsize);
               memcpy(a+elemsize*i, temp, elemsize);
            }
            switch (ismin) {
            case 0: TEST(0 == initmax_heap(&heap, (uint8_t)elemsize, 255, 255, array, basesize == 1 ? &compare_byte : &compare_long, 0)); break;
            case 1: TEST(0 == initmin_heap(&heap, (uint8_t)elemsize, 255, 255, array, basesize == 1 ? &compare_byte : &compare_long, 0)); break;
            }
            long expect = (ismin ? 0 : 254);
            if (basesize == 1) {
               TEST(expect == ((uint8_t*)array)[0]);
            } else {
               TEST(expect == array[0]);
            }
            TEST(0 == invariant_heap(&heap));
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_update(void)
{
   heap_t heap = heap_FREE;
   long   array[5*255];

   // TEST insert_heap


   // TEST remove_heap

   // unprepare
   TEST(0 == free_heap(&heap));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_time(void)
{
   heap_t     heap  = heap_FREE;
   systimer_t timer = systimer_FREE;
   size_t     len   = 5000000;
   long *     a;
   vmpage_t   vmpage = vmpage_FREE;

   // prepare
   TEST(0 == init_vmpage(&vmpage, len * sizeof(long)));
   a = (long*) vmpage.addr;

   // measure time for SWAP memcpy and own bytecopy !!
   for (size_t i = 0; i < len; ++i) {
      a[i] = (long) (len-i);
   }
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   TEST(0 == initmin_heap(&heap, sizeof(long), len, len, a, &compare_long, 0));
   uint64_t time1_ms;
   TEST(0 == expirationcount_systimer(timer, &time1_ms));
   TEST(0 == invariant_heap(&heap));

   // measure time for INITCOPYTYPE (insert + remove)


   // unprepare
   TEST(0 == free_heap(&heap));
   TEST(0 == free_systimer(&timer));
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONABORT:
   free_vmpage(&vmpage);
   free_systimer(&timer);
   return EINVAL;
}

int unittest_ds_inmem_heap()
{
   if (test_initfree())       goto ONABORT;
   if (test_update())         goto ONABORT;
   if (test_time())           goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
