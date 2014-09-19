/* title: Heap impl

   Implements <Heap>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/time/timevalue.h"
#include "C-kern/api/time/systimer.h"
#endif


// section: heap_t

// group: macros

/* define: INITCOPYTYPE
 * Initializes copytype to values 0, 1, or 2.
 * The value 0 is chosen if heap->elemsize == sizeof(long).
 * The value 1 is chosen if heap->elemsize != sizeof(long) && 0 == (heap->elemsize % sizeof(long)).
 * The value 2 is chosen for all other cases. */
#define INITCOPYTYPE \
         const int copytype = ((heap->elemsize == sizeof(long)) ? 0 : (heap->elemsize % sizeof(long)) ? 2 : 1)

/* define: ELEM
 * Returns the address of the element at byte offset in <heap_t.array>. */
#define ELEM(offset) \
         &heap->array[(offset)]

/* define: COPY
 * Copies <heap_t.elemsize> bytes from memory address src to dest.
 * The pointer src and dest must point to the start address (lowest).
 * The copy function copies byte or long values depending on the value of copytype. */
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
 * Swaps <heap_t.elemsize> bytes between src to dest.
 * The pointer src and dest must point to the start address (lowest).
 * The swap function swaps byte or long values depending on the value of copytype. */
#define SWAP(parent, child) \
         if (copytype == 0) { \
            long temp; \
            temp = *((long*)(heap->array+parent)); \
            *((long*)(heap->array+parent)) = *((long*)(heap->array+child)); \
            *((long*)(heap->array+child))  = temp; \
         } else if (copytype == 1) { \
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
 * Returns true if the parent is lower than the child. */
#define ISSWAP(parent, child) \
         (heap->cmp(heap->cmpstate, parent, child) < 0)

// group: query

int invariant_heap(const heap_t * heap)
{
   if (heap->nrofelem > heap->maxnrofelem) goto ONERR;

   if (heap->nrofelem <= 1) return 0;

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
   TRACEEXIT_ERRLOG(EINVARIANT);
   return EINVARIANT;
}

// group: update

/* function: build_heap
 * Builds heap structure according to heap condition.
 *
 * (Max) Heap Condition:
 * For all 0 <= parent && (2*parent+1) < nrofelem
 * where (2*parent+1) is the index of the left and (2*parent+2) the index of right child.
 * > array[parent*elemsize] >= array[(2*parent+1)*elemsize]
 * > && ((2*parent+2) >= nrofelem || array[parent*elemsize] >= array[(2*parent+2)*elemsize]
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

   INITCOPYTYPE;
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

int insert_heap(heap_t * heap, const void * elem)
{
   if (heap->nrofelem == heap->maxnrofelem) return ENOMEM;

   INITCOPYTYPE;
   unsigned i = heap->nrofelem ++;
   unsigned child = i * heap->elemsize;

   while (i > 0) {
      i = (i - 1);

      unsigned parent = child - heap->elemsize;
      if (i&1) parent -= heap->elemsize;
      i /= 2;
      parent /= 2;

      if (! ISSWAP(ELEM(parent), elem)) break;

      COPY(heap->array+child, heap->array+parent);
      child = parent;
   }

   COPY(heap->array+child, elem);

   return 0;
}

int remove_heap(heap_t * heap, /*out*/void * elem)
{
   if (heap->nrofelem == 0) return ENODATA;

   INITCOPYTYPE;

   // remove max (first) element
   COPY(elem, heap->array);

   if (-- heap->nrofelem) {

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

int init_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate)
{
   int err;

   VALIDATE_INPARAM_TEST(cmp != 0 && elemsize > 0 && nrofelem <= maxnrofelem && 0 < maxnrofelem, ONERR, );
   VALIDATE_INPARAM_TEST(maxnrofelem <= (size_t)-1 / elemsize && (uintptr_t)array + maxnrofelem * elemsize > (uintptr_t)array, ONERR, );

   heap->cmp = cmp;
   heap->cmpstate = cmpstate;
   heap->elemsize = elemsize;
   heap->array = array;
   heap->nrofelem = nrofelem;
   heap->maxnrofelem = maxnrofelem;

   build_heap(heap);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
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

static int compare_long_revert(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   long l = *(const long*)left;
   long r = *(const long*)right;
   return (l < r) ? +1 : (l > r) ? -1 : 0;
}

static int compare_byte_revert(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   uint8_t l = *(const uint8_t*)left;
   uint8_t r = *(const uint8_t*)right;
   return (l < r) ? +1 : (l > r) ? -1 : 0;
}

static int test_initfree(void)
{
   heap_t heap = heap_FREE;

   // TEST heap_FREE
   TEST(0 == heap.cmp);
   TEST(0 == heap.cmpstate);
   TEST(0 == heap.elemsize);
   TEST(0 == heap.array);
   TEST(0 == heap.nrofelem);
   TEST(0 == heap.maxnrofelem);

   // TEST free_heap
   memset(&heap, 255, sizeof(heap));
   TEST(0 == free_heap(&heap));
   // only this field is cleared
   TEST(0 == heap.maxnrofelem);

   // TEST init_heap: EINVAL
   {
      uint8_t elemsize = 0;
      uint8_t nrelem    = 2;
      uint8_t maxnrelem = 1;
      // compare function == 0
      TEST(EINVAL == init_heap(&heap, 1, 0, 1, (void*)1, 0, (void*)1));
      // elemsize == 0
      TEST(EINVAL == init_heap(&heap, elemsize, 0, 1, (void*)1, &compare_long, (void*)1));
      // nrelem > maxnrelem
      TEST(nrelem > maxnrelem);
      TEST(EINVAL == init_heap(&heap, 1, nrelem, maxnrelem, (void*)1, &compare_long, (void*)1));
      // maxnrelem == 0
      TEST(EINVAL == init_heap(&heap, 1, 0, 0, (void*)1, &compare_long, (void*)1));
      // maxnrelem * elemsize overflows size_t
      TEST(EINVAL == init_heap(&heap, 3, 0, SIZE_MAX/3+1, (void*)1, &compare_long, (void*)1));
      // array + maxnrelem * elemsize overflows
      TEST(EINVAL == init_heap(&heap, 1, 0, SIZE_MAX, (void*)1, &compare_long, (void*)1));
   }

   // TEST init_heap: init data field
   for (unsigned i = 1; i < 256; ++i) {
      memset(&heap, 0, sizeof(heap));
      TEST(0 == init_heap(&heap, (uint8_t)i, 0, 1+i, (void*)i, &compare_long, (void*)(2*i)));
      TEST(heap.cmp == &compare_long);
      TEST(heap.cmpstate == (void*)(2*i));
      TEST(heap.elemsize == i);
      TEST(heap.array    == (void*)i);
      TEST(heap.nrofelem == 0);
      TEST(heap.maxnrofelem == 1+i);
   }

   // TEST init_heap: build heap from ascending / descending elements
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned len = 1; len <= 255; ++len) {
               for (unsigned isasc = 0; isasc <= 1;  ++isasc) {
                  long array[5*255];
                  memset(array, 0, sizeof(array));
                  for (unsigned i = 0, val = isasc ? 0 : len-1; i < 255; ++i, val += (unsigned)(isasc ? 1 : -1)) {
                     if (basesize == 1) {
                        ((uint8_t*)array)[i*elemsize] = (uint8_t) val;
                     } else {
                        array[i*elemsize/sizeof(long)] = (long) val;
                     }
                  }
                  TEST(0 == init_heap(&heap, (uint8_t)elemsize, len, len, array,
                                          ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                                : (basesize == 1 ? &compare_byte : &compare_long)
                                          , 0));
                  long expect = (long) (ismin ? 0 : len-1);
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

   // TEST init_heap: build heap from random order
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            long array[5*255];
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
            TEST(0 == init_heap(&heap, (uint8_t)elemsize, 255, 255, array,
                                    ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                          : (basesize == 1 ? &compare_byte : &compare_long)
                                    , 0));
            long expect = (long) (ismin ? 0 : 254);
            if (basesize == 1) {
               TEST(expect == ((uint8_t*)array)[0]);
            } else {
               TEST(expect == array[0]);
            }
            TEST(0 == invariant_heap(&heap));
         }
      }
   }

   // TEST init_heap: build heap from equal elements
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned isasc = 0; isasc <= 1;  ++isasc) {
               long array[5*256];
               memset(array, 0, sizeof(array));
               for (unsigned i = 0, val = isasc ? 0 : 255; i < 256; ++i, val += (unsigned)(isasc ? 1 : -1)) {
                  if (basesize == 1) {
                     ((uint8_t*)array)[i*elemsize] = (uint8_t) (val/2);
                  } else {
                     array[i*elemsize/sizeof(long)] = (long) (val/2);
                  }
               }
               TEST(0 == init_heap(&heap, (uint8_t)elemsize, 256, 256, array,
                                       ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                             : (basesize == 1 ? &compare_byte : &compare_long)
                                       , 0));
               long expect = (long) (ismin ? 0 : 127);
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

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   heap_t heap = heap_FREE;
   long   array[255];

   // TEST elemsize_heap
   for (unsigned i = 255; i < 256; --i) {
      heap.elemsize = (uint8_t) i;
      TEST(i == elemsize_heap(&heap));
   }

   // TEST maxnrofelem_heap
   for (size_t i = 1; i; i <<= 1) {
      heap.maxnrofelem = i;
      TEST(i == maxnrofelem_heap(&heap));
   }
   heap.maxnrofelem = 0;
   TEST(0 == maxnrofelem_heap(&heap));

   // TEST nrofelem_heap
   for (size_t i = 1; i; i <<= 1) {
      heap.nrofelem = i;
      TEST(i == nrofelem_heap(&heap));
   }
   heap.nrofelem = 0;
   TEST(0 == nrofelem_heap(&heap));

   // TEST invariant_heap: ascending, descending, equal
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned elemsize = 1; elemsize <= sizeof(long); elemsize += sizeof(long)-1) {
         for (unsigned isasc = 0; isasc <= 1; ++isasc) {
            unsigned len = lengthof(array);
            memset(array, 0, sizeof(array));
            for (unsigned i = 0, val = isasc ? 0 : len-1; i < len; ++i, val += (unsigned)(isasc ? 1 : -1)) {
               if (elemsize == 1) {
                  ((uint8_t*)array)[i*elemsize] = (uint8_t) val;
               } else {
                  array[i*elemsize/sizeof(long)] = (long) val;
               }
            }
            TEST(0 == init_heap(&heap, (uint8_t)elemsize, len, len, array,
                              ismin ? (elemsize == 1 ? &compare_byte_revert : &compare_long_revert)
                                    : (elemsize == 1 ? &compare_byte : &compare_long)
                              , 0));
            TEST(0 == invariant_heap(&heap));

            // all elements are equal
            memset(array, 0, sizeof(array));
            TEST(0 == invariant_heap(&heap));
         }
      }
   }

   // TEST invariant_heap: EINVARIANT (heap.nrofelem > heap.maxnrofelem)
   heap.nrofelem    = heap.maxnrofelem+1;
   TEST(EINVARIANT == invariant_heap(&heap));

   // TEST invariant_heap: EINVARIANT
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned elemsize = 1; elemsize <= sizeof(long); elemsize += sizeof(long)-1) {
         for (unsigned isasc = 0; isasc <= 1; ++isasc) {
            for (unsigned len = lengthof(array)-5; len <= lengthof(array); ++len) {
               memset(array, 0, sizeof(array));
               for (unsigned i = 0, val = isasc ? 0 : len-1; i < len; ++i, val += (unsigned)(isasc ? 1 : -1)) {
                  if (elemsize == 1) {
                     ((uint8_t*)array)[i*elemsize] = (uint8_t) val;
                  } else {
                     array[i*elemsize/sizeof(long)] = (long) val;
                  }
               }
               TEST(0 == init_heap(&heap, (uint8_t)elemsize, len, len, array,
                                 ismin ? (elemsize == 1 ? &compare_byte_revert : &compare_long_revert)
                                       : (elemsize == 1 ? &compare_byte : &compare_long)
                                 , 0));

               uint8_t * logbuf;
               size_t    logsize, logsize2;
               GETBUFFER_ERRLOG(&logbuf, &logsize);
               for (unsigned i = 0; i < len; ++i) {
                  for (unsigned child = 2*i+1; child < len && child <= (2*i+2); ++child) {
                     for (int iserr = 1; iserr >= 0; --iserr) {
                        // swap parent with child
                        if (elemsize == 1) {
                           uint8_t temp = ((uint8_t*)array)[i*elemsize];
                           ((uint8_t*)array)[i*elemsize]     = ((uint8_t*)array)[child*elemsize];
                           ((uint8_t*)array)[child*elemsize] = temp;
                        } else {
                           long temp = array[i*elemsize/sizeof(long)];
                           array[i*elemsize/sizeof(long)]     = array[child*elemsize/sizeof(long)];
                           array[child*elemsize/sizeof(long)] = temp;
                        }
                        if (iserr) {
                           TEST(EINVARIANT == invariant_heap(&heap));
                           GETBUFFER_ERRLOG(&logbuf, &logsize2);
                           TEST(logsize2 > logsize);
                           TRUNCATEBUFFER_ERRLOG(logsize);
                        } else {
                           TEST(0 == invariant_heap(&heap));
                        }
                     }
                  }
               }
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_foreach(void)
{
   heap_t heap = heap_FREE;
   long   array[5*255];

   // TEST foreach_heap: empty heap
   {
      unsigned i = 0;
      foreach_heap(&heap, elem) {
         ++i;
         TEST(0);
      }
      TEST(i == 0);
   }

   // TEST foreach_heap: different element sizes
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned len = 1; len <= 255; ++len) {
               // fill array
               memset(array, 0, sizeof(array));
               for (unsigned i = 0, val = ismin ? 0 : 254; i < len; ++i, val += (unsigned)(ismin ? 1 : -1)) {
                  if (basesize == 1) {
                     ((uint8_t*)array)[i*elemsize] = (uint8_t) (val / 2);
                  } else {
                     array[i*elemsize/sizeof(long)] = (long) (val / 2);
                  }
               }
               TEST(0 == init_heap(&heap, (uint8_t)elemsize, len, 255, array,
                                 ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                       : (basesize == 1 ? &compare_byte : &compare_long)
                                 , 0));
               unsigned i = 0, val = ismin ? 0 : 254;
               foreach_heap(&heap, elem) {
                  void * expect = heap.array + i * elemsize;
                  TEST(expect == elem);
                  if (basesize == 1) {
                     TEST(*((uint8_t*)elem) == (uint8_t) (val / 2));
                  } else {
                     TEST(*((long*)elem)    == (long) (val / 2));
                  }
                  ++i;
                  val += (unsigned) (ismin ? 1 : -1);
               }
               TEST(len == i);
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   heap_t heap = heap_FREE;
   long   array[5*255];
   long   zero[5] = { 0 };
   long   elem[5];

   // TEST insert_heap, remove_heap: ascending, descending
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned len = 1; len <= 255; ++len) {
               if (len == 32) len = 240;
               for (unsigned isasc = 0; isasc <= 1;  ++isasc) {
                  TEST(0 == init_heap(&heap, (uint8_t)elemsize, 0, len, array,
                                       ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                             : (basesize == 1 ? &compare_byte : &compare_long)
                                       , 0));
                  // insert
                  TEST(0 == nrofelem_heap(&heap));
                  memset(elem, 0, sizeof(elem));
                  for (unsigned i = 1; i <= len; ++i) {
                     long val = (long) (ismin ? i-1 : len-i);
                     if (basesize == 1) {
                        ((uint8_t*)elem)[0] = (uint8_t) val;
                     } else {
                        elem[0] = val;
                     }
                     TEST(0 == insert_heap(&heap, elem));
                     TEST(i == nrofelem_heap(&heap));
                     TEST(0 == invariant_heap(&heap));
                  }

                  // remove
                  for (unsigned i = 1; i <= len; ++i) {
                     long val = (long) (ismin ? i-1 : len-i);
                     memset(elem, 255, sizeof(elem));
                     TEST(0 == remove_heap(&heap, elem));
                     TEST(len-i == nrofelem_heap(&heap));
                     TEST(0 == invariant_heap(&heap));
                     if (basesize == 1) {
                        TEST(val == ((uint8_t*)elem)[0]);
                        ((uint8_t*)elem)[0] = 0;
                     } else {
                        TEST(val == elem[0]);
                        elem[0] = 0;
                     }
                     TEST(0 == memcmp(elem, zero, elemsize));
                  }

                  TEST(len == maxnrofelem_heap(&heap));
               }
            }
         }
      }
   }

   // TEST insert_heap, remove_heap: equal elements
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned elemsize = 1; elemsize <= sizeof(long); elemsize += sizeof(long)-1) {
         for (unsigned len = 240; len <= 250; ++len) {
            for (unsigned isasc = 0; isasc <= 1;  ++isasc) {
               TEST(0 == init_heap(&heap, (uint8_t)elemsize, 0, len, array,
                                    ismin ? (elemsize == 1 ? &compare_byte_revert : &compare_long_revert)
                                          : (elemsize == 1 ? &compare_byte : &compare_long)
                                    , 0));
               // insert
               TEST(0 == nrofelem_heap(&heap));
               memset(elem, 0, sizeof(elem));
               for (unsigned i = 1; i <= len; ++i) {
                  long val = (long) ((ismin ? i-1 : len-i)/2);
                  if (elemsize == 1) {
                     ((uint8_t*)elem)[0] = (uint8_t) val;
                  } else {
                     elem[0] = val;
                  }
                  TEST(0 == insert_heap(&heap, elem));
                  TEST(i == nrofelem_heap(&heap));
                  TEST(0 == invariant_heap(&heap));
               }

               // remove
               for (unsigned i = 1; i <= len; ++i) {
                  long val = (long) ((ismin ? i-1 : len-i)/2);
                  memset(elem, 255, sizeof(elem));
                  TEST(0 == remove_heap(&heap, elem));
                  TEST(len-i == nrofelem_heap(&heap));
                  TEST(0 == invariant_heap(&heap));
                  if (elemsize == 1) {
                     TEST(val == ((uint8_t*)elem)[0]);
                     ((uint8_t*)elem)[0] = 0;
                  } else {
                     TEST(val == elem[0]);
                     elem[0] = 0;
                  }
                  TEST(0 == memcmp(elem, zero, elemsize));
               }

               TEST(len == maxnrofelem_heap(&heap));
            }
         }
      }
   }

   // TEST insert_heap, remove_heap: random
   for (unsigned ismin = 0; ismin <= 1; ++ismin) {
      for (unsigned basesize = 1; basesize <= sizeof(long); basesize += sizeof(long)-1) {
         for (unsigned elemsize = basesize; elemsize <= 5*basesize; elemsize += basesize) {
            for (unsigned len = 250; len <= 255; ++len) {
               TEST(0 == init_heap(&heap, (uint8_t)elemsize, 0, len, array,
                                       ismin ? (basesize == 1 ? &compare_byte_revert : &compare_long_revert)
                                             : (basesize == 1 ? &compare_byte : &compare_long)
                                       , 0));
               unsigned vals[255];
               for (unsigned i = 0; i < len; ++i) {
                  vals[i] = i;
               }
               for (unsigned i = 0; i < len; ++i) {
                  unsigned r = ((unsigned) random()) % len;
                  unsigned temp = vals[r];
                  vals[r] = vals[i];
                  vals[i] = temp;
               }
               // insert
               TEST(0 == nrofelem_heap(&heap));
               memset(elem, 0, sizeof(elem));
               for (unsigned i = 0; i < len; ++i) {
                  if (basesize == 1) {
                     ((uint8_t*)elem)[0] = (uint8_t) vals[i];
                  } else {
                     elem[0] = (long) vals[i];
                  }
                  TEST(0 == insert_heap(&heap, elem));
                  TEST(i+1 == nrofelem_heap(&heap));
                  TEST(0 == invariant_heap(&heap));
               }

               // remove
               for (unsigned i = 1; i <= len; ++i) {
                  long val = (long) (ismin ? i-1 : len-i);
                  memset(elem, 255, sizeof(elem));
                  TEST(0 == remove_heap(&heap, elem));
                  TEST(len-i == nrofelem_heap(&heap));
                  TEST(0 == invariant_heap(&heap));
                  if (basesize == 1) {
                     TEST(val == ((uint8_t*)elem)[0]);
                     ((uint8_t*)elem)[0] = 0;
                  } else {
                     TEST(val == elem[0]);
                     elem[0] = 0;
                  }
                  TEST(0 == memcmp(elem, zero, elemsize));
               }

               TEST(len == maxnrofelem_heap(&heap));
            }
         }
      }
   }

   // TEST insert_heap: ENOMEM
   for (unsigned i = 255; i < 255; --i) {
      heap.nrofelem    = i;
      heap.maxnrofelem = i;
      TEST(ENOMEM == insert_heap(&heap, elem));
   }

   // TEST remove_heap: ENODATA
   TEST(0 == nrofelem_heap(&heap));
   TEST(ENODATA == remove_heap(&heap, elem));
   TEST(0 == nrofelem_heap(&heap));

   // unprepare
   TEST(0 == free_heap(&heap));

   return 0;
ONERR:
   return EINVAL;
}

static int test_time(void)
{
   heap_t     heap  = heap_FREE;
   systimer_t timer = systimer_FREE;
   size_t     len   = 100000;
   long *     a;
   memblock_t mblock = memblock_FREE;

   // prepare
   TEST(0 == ALLOC_MM(len * sizeof(long), &mblock));
   a = (long*) mblock.addr;

   // measure time init_heap
   for (long i = 0; i < (long)len; ++i) {
      a[i] = i;
   }
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   TEST(0 == init_heap(&heap, sizeof(long), len, len, a, &compare_long, 0));
   uint64_t time1_ms;
   TEST(0 == expirationcount_systimer(timer, &time1_ms));
   TEST(0 == invariant_heap(&heap));

   // measure time insert_heap
   TEST(0 == init_heap(&heap, sizeof(long), 0, len, a, &compare_long, 0));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   for (long i = 0; i < (long)len; ++i) {
      TEST(0 == insert_heap(&heap, &i));
   }
   uint64_t time2_ms;
   TEST(0 == expirationcount_systimer(timer, &time2_ms));
   TEST(0 == invariant_heap(&heap));

   // measure time for remove_heap
   for (long i = 0; i < (long)len; ++i) {
      a[i] = i;
   }
   TEST(0 == init_heap(&heap, sizeof(long), len, len, a, &compare_long, 0));
   TEST(0 == startinterval_systimer(timer, &(struct timevalue_t){.nanosec = 1000000}));
   for (size_t i = 0; i < len; ++i) {
      long v[2];
      TEST(0 == remove_heap(&heap, &v));
   }
   uint64_t time3_ms;
   TEST(0 == expirationcount_systimer(timer, &time3_ms));
   TEST(0 == invariant_heap(&heap));

   if (time1_ms > time2_ms/2) {
      logwarning_unittest("init_heap not really faster than insert_heap");
   }

   if (time3_ms <= time2_ms) {
      logwarning_unittest("remove_heap faster than insert_heap");
   }

   // unprepare
   TEST(0 == free_heap(&heap));
   TEST(0 == free_systimer(&timer));
   TEST(0 == FREE_MM(&mblock));

   return 0;
ONERR:
   FREE_MM(&mblock);
   free_systimer(&timer);
   return EINVAL;
}

int unittest_ds_inmem_heap()
{
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_foreach())     goto ONERR;
   if (test_update())      goto ONERR;
   if (test_time())        goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
