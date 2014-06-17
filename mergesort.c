/* title: MergeSort impl

   Implements <MergeSort>.

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

   file: C-kern/api/sort/mergsort.h
    Header file <MergeSort>.

   file: C-kern/sort/mergsort.c
    Implementation file <MergeSort impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/sort/mergesort.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/memory/vm.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: mergesort_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_mergesort_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_mergesort_errtimer = test_errortimer_FREE;
#endif

// group: constants

/* define: MIN_BLK_LEN
 * The minimum nr of elements moved as one block in a merge operation.
 * The merge operation is implemented in <merge_adjacent_slices> and
 * <rmerge_adjacent_slices>. */
#define MIN_BLK_LEN 7

/* define: MIN_SLICE_LEN
 * The minimum number of elements stored in a sorted slice.
 * The actual minimum length is computed by <compute_minslicelen>
 * but is always bigger or equal than this value except if the
 * whole array has a size less than MIN_SLICE_LEN.
 * Every slice is described with <mergesort_sortedslice_t>. */
#define MIN_SLICE_LEN 32

// group: memory-helper

/* function: alloctemp_mergesort
 * Reallocates <mergesort_t.temp> so it can store tempsize bytes.
 * The array is always reallocated. */
static int alloctemp_mergesort(mergesort_t * sort, size_t tempsize)
{
   int err;
   vmpage_t mblock;

   if (sort->temp != sort->tempmem) {
      mblock = (vmpage_t) vmpage_INIT(sort->tempsize, sort->temp);
      err = free_vmpage(&mblock);
      SETONERROR_testerrortimer(&s_mergesort_errtimer, &err);

      sort->temp     = sort->tempmem;
      sort->tempsize = sizeof(sort->tempmem);

      if (err) goto ONABORT;
   }

   // TODO: replace call to init_vmpage with call to temporary stack memory manager
   //       == implement temporary stack memory manager ==
   if (tempsize) {
      ONERROR_testerrortimer(&s_mergesort_errtimer, &err, ONABORT);
      err = init_vmpage(&mblock, tempsize);
      if (err) goto ONABORT;
      sort->temp     = mblock.addr;
      sort->tempsize = mblock.size;
   }

   return 0;
ONABORT:
   return err;
}

/* function: ensuretempsize
 * Ensures <mergesort_t.temp> can store tempsize bytes.
 * If the temporary array has less capacity it is reallocated.
 * */
static inline int ensuretempsize(mergesort_t * sort, size_t tempsize)
{
   return tempsize <= sort->tempsize ? 0 : alloctemp_mergesort(sort, tempsize);
}

// group: lifetime

void init_mergesort(/*out*/mergesort_t * sort)
{
    sort->compare  = 0;
    sort->cmpstate = 0;
    sort->elemsize = 0;
    sort->temp     = sort->tempmem;
    sort->tempsize = sizeof(sort->tempmem);
    sort->stacksize = 0;
}

int free_mergesort(mergesort_t * sort)
{
   int err;

   if (sort->temp) {
      err = alloctemp_mergesort(sort, 0);

      sort->temp      = 0;
      sort->tempsize  = 0;
      sort->stacksize = 0;

      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}

// group: query

/* function: compute_minslicelen
 * Compute a good value for the minimum sub-array length;
 * presorted sub-arrays shorter than this are extended via <insertsort>.
 *
 * If n < 64, return n (it's too small to bother with fancy stuff).
 * Else if n is an exact power of 2, return 32.
 * Else return an int k, 32 <= k <= 64, such that n/k is close to, but
 * strictly less than, an exact power of 2.
 *
 * The implementation chooses the 6 most sign. bits of n as minsize.
 * And it increments minsize by one if n/minsize is not an exact power
 * of two.
 */
static uint8_t compute_minslicelen(size_t n)
{
   // becomes 1 if any 1 bits are shifted off
   unsigned r = 0;

   while (n >= 64) {
      r |= n & 1;
      n >>= 1;
   }

   return (uint8_t) ((unsigned)n + r);
}

// group: set

/* function: setsortstate
 * Prepares <mergesort_t> with additional information before sorting can commence.
 *
 * 1. Sets the compare function.
 * 2. Sets elemsize
 *
 * An error is returned if cmp is invalid or if elemsize*array_len overflows size_t type. */
static int setsortstate(
   mergesort_t  * sort,
   sort_compare_f cmp,
   void         * cmpstate,
   const uint8_t  elemsize,
   const size_t   array_len)
{
   if (cmp == 0 || elemsize == 0 || array_len > ((size_t)-1 / elemsize)) {
      return EINVAL;
   }

   sort->compare  = cmp;
   sort->cmpstate = cmpstate;
   sort->elemsize = elemsize;
   return 0;
}

// group: generic

#define mergesort_TYPE_POINTER   1
#define mergesort_TYPE_LONG      2
#define mergesort_TYPE_BYTES     4

//////////////////

#define mergesort_IMPL_TYPE      mergesort_TYPE_BYTES
#include "mergesort_generic_impl.h"

#undef  mergesort_IMPL_TYPE
#define mergesort_IMPL_TYPE      mergesort_TYPE_LONG
#include "mergesort_generic_impl.h"

#undef  mergesort_IMPL_TYPE
#define mergesort_IMPL_TYPE      mergesort_TYPE_POINTER
#include "mergesort_generic_impl.h"

//////////////////

int sortblob_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, void * a/*uint8_t[len*elemsize]*/, sort_compare_f cmp, void * cmpstate)
{
   // use long copy if element size is multiple of long and array is aligned to long
   // on x86 the alignment of the array is not necessary ; long copy would work without it
   if (0 == (uintptr_t)a % sizeof(long) && 0 == elemsize % sizeof(long)) {
      return sortlong_mergesort(sort, elemsize, len, a, cmp, cmpstate);

   } else {
      // adds at least 50% runtime overhead (byte copy is really slow)
      return sortbytes_mergesort(sort, elemsize, len, a, cmp, cmpstate);
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_stacksize(void)
{
   double   phi = (1+sqrt(5))/2; // golden ratio
   unsigned stacksize;

   // stack contains set whose length form qa sequence of Fibonnacci numbers
   // stack[0].len == MIN_SLICE_LEN, stack[1].len == MIN_SLICE_LEN, stack[2].len >= stack[0].len + stack[1].len
   // Fibonacci F1 = 1, F2 = 1, F3 = 2, ..., Fn = F(n-1) + F(n-2)
   // The sum of the first Fn is: F1 + F2 + ... + Fn = F(n+2) -1
   // A stack of depth n describes MIN_SLICE_LEN * (F1 + ... + F(n)) = MIN_SLICE_LEN * (F(n+2) -1) entries.

   // A stack which could describe up to (size_t)-1 entries must have a depth n
   // where MIN_SLICE_LEN*F(n+2) > (size_t)-1. That means the nth Fibonacci number must overflow
   // the type size_t.

   // loga(x) == log(x) / log(a)
   // 64 == log2(2**64) == log(2**64) / log(2) ; ==> log(2**64) == 64 * log(2)
   // pow(2, bitsof(size_t)) overflows size_t and pow(2, bitsof(size_t))/MIN_SLICE_LEN overflows (size_t)-1 / MIN_SLICE_LEN
   // ==> F(n+2) > (size_t)-1 / MIN_SLICE_LEN

   //  The value of the nth Fibonnaci Fn for large numbers of n can be calculated as
   //  Fn ~ 1/sqrt(5) * golden_ratio**n;
   //  (pow(golden_ratio, n) == golden_ratio**n)
   //  ==> n = logphi(Fn*sqrt(5)) == (log(Fn) + log(sqrt(5))) / log(phi)

   // TEST lengthof(stack): size_t
   {
      unsigned ln_phi_SIZEMAX = (unsigned) ((bitsof(size_t)*log(2) - log(MIN_SLICE_LEN) + log(sqrt(5))) / log(phi) + 0.5);
      size_t size1 = 1*MIN_SLICE_LEN; // size of previous set F1
      size_t size2 = 1*MIN_SLICE_LEN; // size of next set     F2
      size_t next;

      for (stacksize = 0; size2 >= size1; ++stacksize, size1=size2, size2=next) {
         next = size1 + size2; // next == MIN_SLICE_LEN*F(n+2) && n-1 == stacksize
      }

      TEST(stacksize == ln_phi_SIZEMAX-2);
      TEST(stacksize <= lengthof(((mergesort_t*)0)->stack));
   }

   // TEST stacksize == 85: big enough to sort array of size uint64_t
   {
      unsigned ln_phi_SIZEMAX = (unsigned) ((64*log(2) - log(MIN_SLICE_LEN) +  log(sqrt(5))) / log(phi) + 0.5);
      uint64_t size1 = 1*MIN_SLICE_LEN; // size of previous set F1
      uint64_t size2 = 1*MIN_SLICE_LEN; // size of next set     F2
      uint64_t next;

      for (stacksize = 0; size2 >= size1; ++stacksize, size1=size2, size2=next) {
         next = size1 + size2; // next == MIN_SLICE_LEN*F(n+2) && n-1 == stacksize
      }

      TEST(stacksize == 85);
      TEST(stacksize == ln_phi_SIZEMAX-2);
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_memhelper(void)
{
   mergesort_t sort;
   vmpage_t    vmpage;

   // TEST alloctemp_mergesort: size == 0
   memset(&sort, 0, sizeof(sort));
   for (unsigned i = 0; i < 2; ++i) {
      TEST(0 == alloctemp_mergesort(&sort, 0));
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
   }

   // TEST alloctemp_mergesort: size > 0
   TEST(0 == alloctemp_mergesort(&sort, 1));
   TEST(0 != sort.temp);
   TEST(pagesize_vm() == sort.tempsize);
   vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
   TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));

   // TEST alloctemp_mergesort: free (size == 0)
   TEST(0 == alloctemp_mergesort(&sort, 0));
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));

   // alloctemp_mergesort: different sizes
   for (unsigned i = 1; i <= 10; ++i) {
      TEST(0 == alloctemp_mergesort(&sort, i * pagesize_vm()));
      TEST(0 != sort.temp);
      TEST(i*pagesize_vm() == sort.tempsize);
      vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   }

   // TEST alloctemp_mergesort: ERROR
   init_testerrortimer(&s_mergesort_errtimer, 1, ENOMEM);
   TEST(ENOMEM == alloctemp_mergesort(&sort, 1));
   // freed (reset to tempmem)
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));

   // TEST ensuretempsize: no reallocation
   for (unsigned i = 0; i <= sort.tempsize; ++i) {
      TEST(0 == ensuretempsize(&sort, i));
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
   }

   // TEST ensuretempsize: reallocation
   for (unsigned i = 10; i <= 11; ++i) {
      // reallocation
      TEST(0 == ensuretempsize(&sort, i * pagesize_vm()));
      TEST(0 != sort.temp);
      TEST(i*pagesize_vm() == sort.tempsize);
      vmpage = (vmpage_t) vmpage_INIT(sort.tempsize, sort.temp);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
      // no reallocation
      TEST(0 == ensuretempsize(&sort, 0));
      TEST(0 == ensuretempsize(&sort, i * pagesize_vm()-1));
      TEST(vmpage.addr == sort.temp);
      TEST(vmpage.size == sort.tempsize);
      TEST(0 != ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   }

   // TEST ensuretempsize: ERROR
   for (unsigned i = 1; i <= 2; ++i) {
      if (sort.tempmem == sort.temp) {
         TEST(0 == ensuretempsize(&sort, sort.tempsize+1));
      }
      init_testerrortimer(&s_mergesort_errtimer, i, ENOMEM);
      TEST(ENOMEM == ensuretempsize(&sort, sort.tempsize+1));
      // freed (reset to tempmem)
      TEST(sort.tempmem == sort.temp);
      TEST(sizeof(sort.tempmem) == sort.tempsize);
      TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_initfree(void)
{
   mergesort_t sort = mergesort_FREE;

   // TEST mergesort_FREE
   TEST(0 == sort.compare);
   TEST(0 == sort.cmpstate);
   TEST(0 == sort.elemsize);
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST init_mergesort
   memset(&sort, 255, sizeof(sort));
   init_mergesort(&sort);
   TEST(0 == sort.compare);
   TEST(0 == sort.cmpstate);
   TEST(0 == sort.elemsize);
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: sort.tempmem == sort.temp
   sort.stacksize = 1;
   TEST(0 == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: sort.tempmem != sort.temp
   alloctemp_mergesort(&sort, pagesize_vm());
   sort.stacksize = 1;
   TEST(sort.temp != 0);
   TEST(sort.temp != sort.tempmem);
   TEST(sort.tempsize == pagesize_vm());
   TEST(0 == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   // TEST free_mergesort: ERROR
   alloctemp_mergesort(&sort, pagesize_vm());
   sort.stacksize = 1;
   init_testerrortimer(&s_mergesort_errtimer, 1, EINVAL);
   TEST(EINVAL == free_mergesort(&sort));
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);

   return 0;
ONABORT:
   return EINVAL;
}

static uint64_t s_compare_count = 0;

static int test_compare_ptr(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   // works cause all pointer values are positive integer values ==> no overflow if subtracted
   if ((uintptr_t)left < (uintptr_t)right)
      return -1;
   else if ((uintptr_t)left > (uintptr_t)right)
      return +1;
   return 0;
}

static int test_compare_long(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   if (*(const long*)left < *(const long*)right)
      return -1;
   else if (*(const long*)left > *(const long*)right)
      return +1;
   return 0;
}

static int test_compare_bytes(void * cmpstate, const void * left, const void * right)
{
   (void) cmpstate;
   ++s_compare_count;
   int lk = ((const uint8_t*)left)[0] * 256 + ((const uint8_t*)left)[1];
   int rk = ((const uint8_t*)right)[0] * 256 + ((const uint8_t*)right)[1];
   return lk - rk;
}


static int test_compare2(const void * left, const void * right) // TODO: remove
{
   ++s_compare_count;
   if (*(const uintptr_t*)left < *(const uintptr_t*)right)
      return -1;
   else if (*(const uintptr_t*)left > *(const uintptr_t*)right)
      return +1;
   return 0;
}


static int test_query(void)
{
   // TEST compute_minslicelen: >= MIN_SLICE_LEN except if arraysize < MIN_SLICE_LEN
   TEST(MIN_SLICE_LEN == compute_minslicelen(64));
   for (size_t i = 0; i < 64; ++i) {
      TEST(i == compute_minslicelen(i));
   }

   // TEST compute_minslicelen: minlen * pow(2, shift)
   for (size_t i = 32; i < 64; ++i) {
      for (size_t shift = 1; shift <= bitsof(size_t)-6; ++shift) {
         TEST(i == compute_minslicelen(i << shift));
      }
   }

   // TEST compute_minslicelen: minlen * pow(2, shift) + DELTA < minlen * pow(2, shift+1)
   for (size_t i = 32; i < 64; ++i) {
      for (size_t shift = 1; shift <= bitsof(size_t)-6; ++shift) {
         for (size_t delta = 0; delta < shift; ++delta) {
            TEST(i+1 == compute_minslicelen((i << shift) + ((size_t)1 << delta)/*set single bit to 1*/));
            if (delta) {
               TEST(i+1 == compute_minslicelen((i << shift) + ((size_t)1 << delta)-1/*set all lower bits to 1*/));
            }
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_set(void)
{
   mergesort_t sort = mergesort_FREE;

   // TEST setsortstate
   TEST(0 == setsortstate(&sort, &test_compare_ptr, (void*)3, 5, 15/*only used to check for EINVAL*/));
   TEST(sort.compare  == &test_compare_ptr);
   TEST(sort.cmpstate == (void*)3);
   TEST(sort.elemsize == 5);
   TEST(0 == setsortstate(&sort, &test_compare_long, (void*)0, 16, SIZE_MAX/16/*only used to check for EINVAL*/));
   TEST(sort.compare  == &test_compare_long);
   TEST(sort.cmpstate == 0);
   TEST(sort.elemsize == 16);

   // TEST setsortstate: EINVAL (cmp == 0)
   TEST(EINVAL == setsortstate(&sort, 0, (void*)1, 1, 1));

   // TEST setsortstate: EINVAL (elemsize == 0)
   TEST(EINVAL == setsortstate(&sort, &test_compare_ptr, (void*)1, 0, 1));

   // TEST setsortstate: EINVAL (elemsize * array_len > (size_t)-1)
   TEST(EINVAL == setsortstate(&sort, &test_compare_ptr, (void*)1, 8, SIZE_MAX/8 + 1u));

   return 0;
ONABORT:
   return EINVAL;
}

static void * s_compstate = 0;
static const void * s_left  = 0;
static const void * s_right = 0;

static int test_compare_save(void * cmpstate, const void * left, const void * right)
{
   s_compstate = cmpstate;
   s_left      = left;
   s_right     = right;
   return -1;
}

static int test_compare_save2(void * cmpstate, const void * left, const void * right)
{
   s_compstate = cmpstate;
   s_left      = left;
   s_right     = right;
   return +1;
}

static int test_searchgrequal(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST search_greatequal: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, 3, alen));
                        TEST(alen == search_greatequal_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[3*(alen-1)]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == search_greatequal_long(&sort, (void*)key, alen, (uint8_t*)larray));
                        TEST(s_left == (void*)&larray[alen-1]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == search_greatequal_ptr(&sort, (void*)key, alen, (uint8_t*)parray));
                        TEST(s_left == (void*)parray[alen-1]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST search_greatequal: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // alen is returned in case no value in array >= key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*i + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i == search_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i == search_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*)larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i == search_greatequal_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*)parray)); break;
               }
            }
         }
      }
   }

   // TEST search_greatequal: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*multi*i);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == search_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == search_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*)larray)); break;
               }
            }
         }
      }
   }

   // TEST search_greatequal: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == search_greatequal_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == search_greatequal_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_rsearchgrequal(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST rsearch_greatequal: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, 3, alen));
                        TEST(alen == rsearch_greatequal_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[0]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == rsearch_greatequal_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[0]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == rsearch_greatequal_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[0]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST rsearch_greatequal: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // 0 is returned in case no value in array >= key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*(alen-i) + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i == rsearch_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i == rsearch_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i == rsearch_greatequal_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greatequal: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*(alen-i)*multi);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == rsearch_greatequal_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == rsearch_greatequal_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greatequal: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save2, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == rsearch_greatequal_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == rsearch_greatequal_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_searchgreater(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long)  (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST search_greater: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, 3, alen));
                        TEST(alen == search_greater_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[3*(alen-1)]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == search_greater_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[alen-1]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == search_greater_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[alen-1]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST search_greater: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // alen is returned in case no value in array > key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*i + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               unsigned i2 = i + (i == alen ? 0 : kadd);
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i2 == search_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i2 == search_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i2 == search_greater_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST search_greater: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*multi*i);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == search_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == search_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST search_greater: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == search_greater_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == search_greater_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }


   return 0;
ONABORT:
   return EINVAL;
}

static int test_rsearchgreater(void)
{
   mergesort_t sort = mergesort_FREE;
   void *      parray[512];
   long        larray[512];
   uint8_t     barray[3*512];

   // prepare
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      parray[i] = (void*) (3*i+1);
      larray[i] = (long) (3*i+1);
   }
   for (unsigned i = 0; i < lengthof(barray); i += 3) {
      barray[i]   = (uint8_t) ((i+1) / 256);
      barray[i+1] = (uint8_t) (i+1);
      barray[i+2] = 0;
   }

   // TEST rsearch_greater: called with correct arguments!
   for (int type = 0; type < 3; ++type) {
      for (uintptr_t cmpstate = 0; cmpstate <= 0x1000; cmpstate += 0x1000) {
         for (uintptr_t key = 0; key <= 10; ++key) {
            for (unsigned alen = 1; alen <= 8; ++alen) {
               TEST(0 == setsortstate(&sort, &test_compare_save, (void*)cmpstate, sizeof(long), alen));
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, 3, alen));
                        TEST(alen == rsearch_greater_bytes(&sort, (void*)key, alen, barray));
                        TEST(s_left == (void*)&barray[0]); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(long), alen));
                        TEST(alen == rsearch_greater_long(&sort, (void*)key, alen, (uint8_t*) larray));
                        TEST(s_left == (void*)&larray[0]); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_save2, (void*)cmpstate, sizeof(void*), alen));
                        TEST(alen == rsearch_greater_ptr(&sort, (void*)key, alen, (uint8_t*) parray));
                        TEST(s_left == (void*)parray[0]); break;
               }
               TEST(s_compstate == (void*)cmpstate);
               TEST(s_right     == (void*)key);
            }
         }
      }
   }

   // TEST rsearch_greater: find all elements in array
   for (int type = 0; type < 3; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned alen = 1; alen <= lengthof(larray); ++alen) {
         for (unsigned i = 0; i <= alen; ++i) {
            // 0 is returned in case no value in array > key
            for (unsigned kadd = 0; kadd <= 1; ++kadd) {
               lkey = (long) (3*(alen-i) + kadd);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               unsigned i2 = i - (i == 0 ? 0 : kadd);
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, 3, alen));
                        TEST(i2 == rsearch_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, sizeof(long), alen));
                        TEST(i2 == rsearch_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               case 2:  TEST(0 == setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), alen));
                        TEST(i2 == rsearch_greater_ptr(&sort, (void*)(uintptr_t)lkey, alen, (uint8_t*) parray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greater: multiple of long / bytes elements
   for (int type = 0; type < 2; ++type) {
      long    lkey;
      uint8_t bkey[2];
      for (unsigned multi = 2; multi <= 5; ++multi) {
         for (unsigned alen = 1; alen <= lengthof(larray)/multi; ++alen) {
            for (unsigned i = 0; i <= alen; ++i) {
               lkey = (long) (3*(alen-i)*multi);
               bkey[0] = (uint8_t) (lkey/256);
               bkey[1] = (uint8_t) lkey;
               switch (type) {
               case 0:  TEST(0 == setsortstate(&sort, &test_compare_bytes, 0, (uint8_t) (3 * multi), alen));
                        TEST(i == rsearch_greater_bytes(&sort, (void*)bkey, alen, barray)); break;
               case 1:  TEST(0 == setsortstate(&sort, &test_compare_long, 0, (uint8_t) (sizeof(long) * multi), alen));
                        TEST(i == rsearch_greater_long(&sort, (void*)&lkey, alen, (uint8_t*) larray)); break;
               }
            }
         }
      }
   }

   // TEST rsearch_greater: size_t does not overflow
   for (int type = 0; type < 2; ++type) {
      for (unsigned multi = 1; multi <= 5; multi += 4) {
         uint8_t elemsize = 0;
         switch (type) {
         case 0: elemsize = (uint8_t) multi;                break;
         case 1: elemsize = (uint8_t) (sizeof(long)*multi); break;
         }
         TEST(0 == setsortstate(&sort, &test_compare_save2, 0, elemsize, SIZE_MAX/elemsize));
         switch (type) {
         case 0: TEST(SIZE_MAX/elemsize == rsearch_greater_bytes(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         case 1: TEST(SIZE_MAX/elemsize == rsearch_greater_long(&sort, 0, SIZE_MAX/elemsize, 0)); break;
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static void set_value(uintptr_t value, uint8_t * barray, long * larray, void ** parray)
{
   parray[0] = (void*) value;
   larray[0] = (long) value;
   barray[0] = (uint8_t) (value / 256);
   barray[1] = (uint8_t) value;
   barray[2] = 0;
}

static int compare_value(int type, uintptr_t value, unsigned i, uint8_t * barray, long * larray, void ** parray)
{
   switch (type) {
   case 0: TEST(barray[3*i]   == (uint8_t) (value / 256));
           TEST(barray[3*i+1] == (uint8_t) value );
           TEST(barray[3*i+2] == 0); break;
   case 1: TEST(larray[i] == (long) value);  break;
   case 2: TEST(parray[i] == (void*) value); break;
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int compare_content(int type, uint8_t * barray, long * larray, void ** parray, unsigned len)
{
   for (unsigned i = 0; i < len; ++i) {
      uintptr_t key = 5*i;
      switch (type) {
      case 0: TEST(barray[3*i]   == (uint8_t) (key / 256));
              TEST(barray[3*i+1] == (uint8_t) key);
              TEST(barray[3*i+2] == 0); break;
      case 1: TEST(larray[i] == (long) key);  break;
      case 2: TEST(parray[i] == (void*) key); break;
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_merge(void)
{
   mergesort_t sort;
   void *      parray[128];
   long        larray[128];
   uint8_t     barray[3*128];
   typedef int (*merge_slices_f) (mergesort_t * sort, uint8_t * left, size_t llen, uint8_t * right, size_t rlen);
   merge_slices_f merge_slices [3][2] = {
      { &merge_adjacent_slices_bytes, &rmerge_adjacent_slices_bytes },
      { &merge_adjacent_slices_long,  &rmerge_adjacent_slices_long },
      { &merge_adjacent_slices_ptr,   &rmerge_adjacent_slices_ptr },
   };

   // prepare
   init_mergesort(&sort);
   for (unsigned i = 0; i < lengthof(larray); ++i) {
      uintptr_t val = 5*i;
      set_value(val, barray+3*i, larray+i, parray+i);
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: allocate enough temporary memory
   for (unsigned nrpage = 2; nrpage <= 10; nrpage += 2) {
      vmpage_t vmpage;
      TEST(0 == init_vmpage(&vmpage, pagesize_vm()*nrpage));
      for (int type = 0; type < 3; ++type) {
         for (int reverse = 0; reverse <= 1; ++reverse) {
            for (size_t lsize = pagesize_vm(); lsize < vmpage.size; lsize += pagesize_vm()) {
               switch (type) {
               case 0: setsortstate(&sort, &test_compare_bytes, 0, 2, 1); break;
               case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); break;
               case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); break;
               }
               memset(vmpage.addr, 1, vmpage.size);
               memset(vmpage.addr + lsize, 0, vmpage.size - lsize);
               TEST(0 == merge_slices[type][reverse](&sort,
                                 vmpage.addr, lsize / sort.elemsize,
                                 vmpage.addr+lsize, (vmpage.size - lsize) / sort.elemsize));
               TEST(0 != sort.temp);
               TEST(sort.tempmem  != sort.temp);
               TEST(sort.tempsize == (size_t) (reverse ? vmpage.size - lsize : lsize));
               TEST(0 == alloctemp_mergesort(&sort, 0));
               TEST(sort.tempmem  == sort.temp);
            }
         }
      }
      TEST(0 == free_vmpage(&vmpage));
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: all elements are already in place
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned llen = 1; llen < lengthof(larray); ++llen) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + llen * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, llen, right, lengthof(larray)-llen));
            // nothing changed
            sort.tempsize = 0;
            TEST(0 == compare_content(type, barray, larray, parray, lengthof(larray)));
            // no tempmem used
            TEST(sort.temp == sort.tempmem);
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: alternating left right
   for (int type = 0; type < 3; ++type) {
      for (int reverse = 0; reverse <= 1; ++reverse) {
         for (unsigned off = 0; off <= 1; ++off) {
            for (unsigned i = 0, ki = 0; i < lengthof(larray); ++i, ki += 2) {
               uintptr_t val = 5u*((ki%lengthof(larray)) + (ki>=lengthof(larray) ? off : (unsigned)!off));
               set_value(val, barray+3*i, larray+i, parray+i);
            }
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
            }
            uint8_t * right = left + lengthof(larray)/2 * sort.elemsize;
            TEST(0 == merge_slices[type][reverse](&sort, left, lengthof(larray)/2, right, lengthof(larray)/2));
            TEST(0 == compare_content(type, barray, larray, parray, lengthof(larray)));
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: block modes
   size_t blocksize[][3][2] = {
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 1 }, { 0, 0 } }, // triggers first if (llen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 1 }, { 2*MIN_BLK_LEN, 0 } }, // triggers second if (rlen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 2*MIN_BLK_LEN, 0 } }, // triggers third if (rlen == 0) goto DONE;
      { { 1, 2*MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, MIN_BLK_LEN } }, // triggers fourth if (llen == 0) goto DONE;
      // reverse
      { { 0, 2*MIN_BLK_LEN}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse first if (llen == 0) goto DONE;
      { { 2*MIN_BLK_LEN, 1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse second if (rlen == 0) goto DONE;
      { { 2*MIN_BLK_LEN, MIN_BLK_LEN+1}, { 2*MIN_BLK_LEN, 2*MIN_BLK_LEN }, { 1, 1 } }, // triggers reverse third if (rlen == 0) goto DONE;
      { { 0, 1 }, { 1, 2*MIN_BLK_LEN }, { 2*MIN_BLK_LEN+1, 1 } }, // triggers reverse fourth if (llen == 0) goto DONE;
   };
   for (unsigned ti = 0; ti < lengthof(blocksize); ++ti) {
      unsigned llen = 0;
      unsigned rlen = 0;
      for (unsigned bi = 0; bi < lengthof(blocksize[ti]); ++bi) {
         llen += blocksize[ti][bi][0];
         rlen += blocksize[ti][bi][1];
      }
      for (int type = 0; type < 3; ++type) {
         for (int reverse = 0; reverse <= 1; ++reverse) {
            for (unsigned isswap = 0; isswap <= 1; ++isswap) {
               unsigned ki = 0, li = 0, ri = llen;
               for (unsigned bi = 0; bi < lengthof(blocksize[ti]); ++bi) {
                  unsigned lk = ki + (!isswap ? 0 : blocksize[ti][bi][1]);
                  unsigned rk = ki + (isswap  ? 0 : blocksize[ti][bi][0]);
                  ki += blocksize[ti][bi][0] + blocksize[ti][bi][1];
                  for (unsigned i = 0; i < blocksize[ti][bi][0]; ++i, ++lk, ++li) {
                     set_value(5*lk, barray+3*li, larray+li, parray+li);
                  }
                  for (unsigned i = 0; i < blocksize[ti][bi][1]; ++i, ++rk, ++ri) {
                     set_value(5*rk, barray+3*ri, larray+ri, parray+ri);
                  }
               }
               uint8_t * left;
               switch (type) {
               case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, lengthof(barray)); left = barray; break;
               case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), lengthof(larray)); left = (uint8_t*)larray; break;
               case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), lengthof(parray)); left = (uint8_t*)parray; break;
               }
               uint8_t * right = left + llen * sort.elemsize;
               TEST(0 == merge_slices[type][reverse](&sort, left, llen, right, rlen));
               TEST(0 == compare_content(type, barray, larray, parray, llen+rlen));
            }
         }
      }
   }

   // TEST merge_adjacent_slices, rmerge_adjacent_slices: stable
   // TODO: test stable merge !!

   // TEST merge_topofstack: stacksize + isSecondTop
   memset(barray, 0, sizeof(barray));
   memset(larray, 0, sizeof(larray));
   memset(parray, 0, sizeof(parray));
   for (unsigned stacksize = 2; stacksize <= 10; ++stacksize) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned isSecondTop = 0; isSecondTop <= 1; ++isSecondTop) {
            if (isSecondTop && stacksize < 3) continue;
            for (unsigned s = 1; s <= 3; ++s) {
               unsigned top = stacksize-1u-isSecondTop;
               memset(sort.stack, 0, sizeof(sort.stack));
               sort.stack[top-1].base = left;
               sort.stack[top-1].len  = s*lengthof(larray)/4;
               sort.stack[top].base = left + s*lengthof(larray)/4 * sort.elemsize;
               sort.stack[top].len  = lengthof(larray) - s*lengthof(larray)/4;
               sort.stack[top+1].base = (void*)s;
               sort.stack[top+1].len  = SIZE_MAX/s;
               sort.stacksize = stacksize;
               switch (type) {
               case 0: TEST(0 == merge_topofstack_bytes(&sort, isSecondTop)); break;
               case 1: TEST(0 == merge_topofstack_long(&sort, isSecondTop)); break;
               case 2: TEST(0 == merge_topofstack_ptr(&sort, isSecondTop)); break;
               }
               TEST(sort.stacksize == stacksize-1);
               TEST(sort.stack[top-1].base == left);
               TEST(sort.stack[top-1].len  == lengthof(larray));
               TEST(sort.stack[top+1].base == (void*)s);
               TEST(sort.stack[top+1].len  == SIZE_MAX/s);
               if (isSecondTop) {
                  TEST(sort.stack[top].base == (void*)s);
                  TEST(sort.stack[top].len  == SIZE_MAX/s);
               } else {
                  TEST(sort.stack[top].base == left + s*lengthof(larray)/4 * sort.elemsize);
                  TEST(sort.stack[top].len  == lengthof(larray) - s*lengthof(larray)/4);
               }
               for (unsigned i = 0; i < lengthof(larray); ++i) {
                  TEST(0 == barray[3*i] && 0 == barray[3*i+1] && 0 == barray[3*i+2]);
                  TEST(0 == larray[i]);
                  TEST(0 == parray[i]);
               }
            }
         }
      }
   }

   // TEST merge_topofstack: tempsize has size of smaller slice
   for (unsigned nrpage = 5; nrpage <= 10; nrpage += 5) {
      vmpage_t vmpage;
      TEST(0 == init_vmpage(&vmpage, pagesize_vm()*nrpage));
      for (int type = 0; type < 3; ++type) {
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); break;
         }
         for (unsigned lsize = 1; lsize < nrpage; lsize += nrpage/2) {
            memset(vmpage.addr, 1, vmpage.size);
            memset(vmpage.addr + lsize*pagesize_vm(), 0, vmpage.size - lsize*pagesize_vm());
            sort.stack[0].base = vmpage.addr;
            sort.stack[0].len  = lsize*pagesize_vm() / sort.elemsize;
            sort.stack[1].base = vmpage.addr + lsize*pagesize_vm();
            sort.stack[1].len  = vmpage.size / sort.elemsize - sort.stack[0].len;
            sort.stacksize = 2;
            switch (type) {
            case 0: TEST(0 == merge_topofstack_bytes(&sort, false)); break;
            case 1: TEST(0 == merge_topofstack_long(&sort, false)); break;
            case 2: TEST(0 == merge_topofstack_ptr(&sort, false)); break;
            }
            TEST(sort.stacksize == 1);
            TEST(sort.stack[0].base == vmpage.addr);
            TEST(sort.stack[0].len  == vmpage.size / sort.elemsize);
            TEST(sort.tempsize == pagesize_vm() * (lsize == 1 ? lsize : nrpage - lsize));
            TEST(0 == alloctemp_mergesort(&sort, 0));
         }
      }
      TEST(0 == free_vmpage(&vmpage));
   }

   // TEST establish_stack_invariant: does nothing if invariant ok
   memset(barray, 0, sizeof(barray));
   memset(larray, 0, sizeof(larray));
   memset(parray, 0, sizeof(parray));
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/2; stackoffset += lengthof(sort.stack)/2) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            // 3 entries
            if (stackoffset) {
               sort.stack[stackoffset-1].base = left;
               sort.stack[stackoffset-1].len  = (2*size+2);
            }
            sort.stack[stackoffset+0].base = left + (2*size+2)*sort.elemsize;
            sort.stack[stackoffset+0].len  = size+1;
            sort.stack[stackoffset+1].base = left + (3*size+3)*sort.elemsize;
            sort.stack[stackoffset+1].len  = size;
            sort.stacksize = stackoffset+2;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            TEST(sort.stacksize == stackoffset+2);
            if (stackoffset) {
               TEST(sort.stack[stackoffset-1].base == left);
               TEST(sort.stack[stackoffset-1].len  == (2*size+2));
            }
            TEST(sort.stack[stackoffset+0].base == left + (2*size+2)*sort.elemsize);
            TEST(sort.stack[stackoffset+0].len  == size+1);
            TEST(sort.stack[stackoffset+1].base == left + (3*size+3)*sort.elemsize);
            TEST(sort.stack[stackoffset+1].len  == size);
         }
      }
   }

   // TEST establish_stack_invariant: merge if top[-2].len <= top[-1].len
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/3; stackoffset += lengthof(sort.stack)/3) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            if (stackoffset) {
               sort.stack[stackoffset-2].base = 0;
               sort.stack[stackoffset-2].len  = SIZE_MAX;
               sort.stack[stackoffset-1].base = 0;
               sort.stack[stackoffset-1].len  = SIZE_MAX/2;
            }
            sort.stack[stackoffset+0].base = left;
            sort.stack[stackoffset+0].len  = size;
            sort.stack[stackoffset+1].base = left + size*sort.elemsize;
            sort.stack[stackoffset+1].len  = 9;
            sort.stacksize = stackoffset+2;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            if (size == 10) {
               TEST(sort.stacksize == stackoffset+2);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size);
               TEST(sort.stack[stackoffset+1].base == left + size*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 9);
            } else {
               TEST(sort.stacksize == stackoffset+1);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+9);
            }
         }
      }
   }

   // TEST establish_stack_invariant: merge if top[-3].len <= top[-2].len + top[-1].len
   for (unsigned stackoffset = 0; stackoffset <= lengthof(sort.stack)/2; stackoffset += lengthof(sort.stack)/2) {
      for (unsigned size = 1; size <= 10; ++size) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            if (stackoffset) {
               sort.stack[stackoffset-2].base = 0;
               sort.stack[stackoffset-2].len  = SIZE_MAX;
               sort.stack[stackoffset-1].base = 0;
               sort.stack[stackoffset-1].len  = SIZE_MAX/2;
            }
            sort.stack[stackoffset+0].base = left;
            sort.stack[stackoffset+0].len  = size;
            sort.stack[stackoffset+1].base = left + size*sort.elemsize;
            sort.stack[stackoffset+1].len  = 5;
            sort.stack[stackoffset+2].base = left + (5+size)*sort.elemsize;
            sort.stack[stackoffset+2].len  = 4;
            sort.stacksize = stackoffset+3;
            switch (type) {
            case 0: TEST(0 == establish_stack_invariant_bytes(&sort)); break;
            case 1: TEST(0 == establish_stack_invariant_long(&sort)); break;
            case 2: TEST(0 == establish_stack_invariant_ptr(&sort)); break;
            }
            if (size == 10) {
               TEST(sort.stacksize == stackoffset+3);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size);
               TEST(sort.stack[stackoffset+1].base == left + size*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 5);
               TEST(sort.stack[stackoffset+2].base = left + (5+size)*sort.elemsize);
               TEST(sort.stack[stackoffset+2].len  = 4);
            } else if (size <= 4) {
               // first merge [-2] and [-1] which then satisfy invariant ([-3].len <= [-1].len)
               TEST(sort.stacksize == stackoffset+2);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+5);
               TEST(sort.stack[stackoffset+1].base == left + (5+size)*sort.elemsize);
               TEST(sort.stack[stackoffset+1].len  == 4);

            } else {
               // first merge [-1] and [-2] and then with [-3]
               TEST(sort.stacksize == stackoffset+1);
               TEST(sort.stack[stackoffset+0].base == left);
               TEST(sort.stack[stackoffset+0].len  == size+9);
            }
         }
      }
   }

   // TEST merge_all
   for (unsigned stacksize = 0; stacksize <= 5 && stacksize < lengthof(sort.stack); ++stacksize) {
      for (unsigned incr = 0; incr <= 1; ++incr) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            unsigned total = 0;
            for (unsigned i = 0, s = 1 + (1-incr)*(stacksize-1); i < stacksize; ++i, total += s, s += (unsigned)-1 + 2*incr) {
               sort.stack[i].base = left + total * sort.elemsize;
               sort.stack[i].len  = s;
            }
            sort.stacksize = stacksize;
            switch (type) {
            case 0: TEST(0 == merge_all_bytes(&sort)); break;
            case 1: TEST(0 == merge_all_long(&sort)); break;
            case 2: TEST(0 == merge_all_ptr(&sort)); break;
            }
            if (stacksize == 0) {
               TEST(sort.stacksize == 0);
            } else {
               TEST(sort.stacksize == 1);
               TEST(sort.stack[0].base == left);
               TEST(sort.stack[0].len  == total);
            }
         }
      }
   }

   // unprepare
   TEST(0 == free_mergesort(&sort));

   return 0;
ONABORT:
   free_mergesort(&sort);
   return EINVAL;
}

static int test_presort(void)
{
   mergesort_t sort;
   void *      parray[64];
   long        larray[64];
   uint8_t     barray[3*64];

   // prepare
   init_mergesort(&sort);

   // TEST insertsort: stable (equal elements keep position relative to themselves)
   for (unsigned i = 0; i < lengthof(larray); i += 2) {
      set_value(1+(i < lengthof(larray)/2), barray+3*i, larray+i, parray+i);
      set_value(i, barray+3*i+3, larray+i+1, parray+i+1);
   }
   for (int type = 0; type < 2; ++type) {
      uint8_t   len = lengthof(larray)/2;
      uint8_t * left;
      switch (type) {
      case 0: setsortstate(&sort, &test_compare_bytes, 0, 2*3, 1); left = barray; break;
      case 1: setsortstate(&sort, &test_compare_long, 0, 2*sizeof(long), 1); left = (uint8_t*) larray; break;
      }
      switch (type) {
      case 0: insertsort_bytes(&sort, 1, len, left); break;
      case 1: insertsort_long(&sort, 1, len, left); break;
      }
      for (unsigned i = 0; i < lengthof(larray)/2; i += 2) { // sorted
         TEST(0 == compare_value(type, 1, i, barray, larray, parray));
         TEST(0 == compare_value(type, i+lengthof(larray)/2, i+1, barray, larray, parray));
         TEST(0 == compare_value(type, 2, i+lengthof(larray)/2, barray, larray, parray));
         TEST(0 == compare_value(type, i, i+lengthof(larray)/2+1, barray, larray, parray));
      }
   }

   // TEST insertsort: elements before start are not sorted
   for (uint8_t len = 1; len < lengthof(larray); ++len) {
      for (uint8_t start = 1; start <= len; ++start) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            for (unsigned i = 0; i < start; ++i) {
               set_value(start-i, barray+3*i, larray+i, parray+i);
            }
            for (unsigned i = start; i < len; ++i) {
               set_value(len-i+start, barray+3*i, larray+i, parray+i);
            }
            switch (type) {
            case 0: insertsort_bytes(&sort, start, len, left); break;
            case 1: insertsort_long(&sort, start, len, left); break;
            case 2: insertsort_ptr(&sort, start, len, left); break;
            }
            for (unsigned i = 0; i < start; ++i) { // not sorted
               TEST(0 == compare_value(type, start-i, i, barray, larray, parray));
            }
            for (unsigned i = start; i < len; ++i) { // sorted
               TEST(0 == compare_value(type, i+1, i, barray, larray, parray));
            }
         }
      }
   }

   // TEST insertsort: sort ascending & descending
   for (unsigned startval = 1; startval <= lengthof(larray); ++startval) {
      for (unsigned step = 1; step < lengthof(larray); step += 2) {
         for (int type = 0; type < 3; ++type) {
            uint8_t * left;
            switch (type) {
            case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
            case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
            case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
            }
            for (unsigned i = 0; i < lengthof(larray); ++i) {
               unsigned val = (startval + i * step) % lengthof(larray);
               set_value(val, barray+3*i, larray+i, parray+i);
            }
            switch (type) {
            case 0: insertsort_bytes(&sort, 1, lengthof(larray), left); break;
            case 1: insertsort_long(&sort, 1, lengthof(larray), left); break;
            case 2: insertsort_ptr(&sort, 1, lengthof(larray), left); break;
            }
            for (unsigned i = 0; i < lengthof(larray); ++i) {
               TEST(0 == compare_value(type, i, i, barray, larray, parray));
            }
         }
      }
   }

   // TEST reverse_elements
   for (unsigned len = 1; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < len; ++i) {
            set_value(i, barray+3*i, larray+i, parray+i);
         }
         switch (type) {
         case 0: reverse_elements_bytes(&sort, left, left+(len-1)*sort.elemsize); break;
         case 1: reverse_elements_long(&sort, left, left+(len-1)*sort.elemsize); break;
         case 2: reverse_elements_ptr(&sort, left, left+(len-1)*sort.elemsize); break;
         }
         for (unsigned i = 0; i < len; ++i) {
            TEST(0 == compare_value(type, i, len-1-i, barray, larray, parray));
         }
      }
   }

   // TEST count_presorted: ascending (or equal)
   for (unsigned len = 2; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < lengthof(larray); ++i) {
            set_value(1+i/2, barray+3*i, larray+i, parray+i);
         }
         if (len < lengthof(larray)) {
            set_value(len/2, barray+3*len, larray+len, parray+len);
         }
         switch (type) {
         case 0: TEST(len == count_presorted_bytes(&sort, len, left)); break;
         case 1: TEST(len == count_presorted_long(&sort, len, left)); break;
         case 2: TEST(len == count_presorted_ptr(&sort, len, left)); break;
         }
      }
   }

   // TEST count_presorted: descending (order is reversed before return)
   for (unsigned len = 2; len <= lengthof(larray); ++len) {
      for (int type = 0; type < 3; ++type) {
         uint8_t * left;
         switch (type) {
         case 0: setsortstate(&sort, &test_compare_bytes, 0, 3, 1); left = barray; break;
         case 1: setsortstate(&sort, &test_compare_long, 0, sizeof(long), 1); left = (uint8_t*) larray; break;
         case 2: setsortstate(&sort, &test_compare_ptr, 0, sizeof(void*), 1); left = (uint8_t*) parray; break;
         }
         for (unsigned i = 0; i < lengthof(larray); ++i) {
            set_value(SIZE_MAX-i, barray+3*i, larray+i, parray+i);
         }
         if (len < lengthof(larray)) {
            set_value(SIZE_MAX, barray+3*len, larray+len, parray+len);
         }
         switch (type) {
         case 0: TEST(len == count_presorted_bytes(&sort, len, left)); break;
         case 1: TEST(len == count_presorted_long(&sort, len, left)); break;
         case 2: TEST(len == count_presorted_ptr(&sort, len, left)); break;
         }
         for (unsigned i = 0; i < len; ++i) {
            TEST(0 == compare_value(type, SIZE_MAX-i, len-1-i, barray, larray, parray));
         }
      }
   }

   // unprepare
   TEST(0 == free_mergesort(&sort));

   return 0;
ONABORT:
   free_mergesort(&sort);
   return EINVAL;
}

static int test_sort(void)
{
   mergesort_t    sort   = mergesort_FREE;
   vmpage_t       vmpage;
   void        ** a      = 0;
   const unsigned len    = 50000000;

   // prepare
   TEST(0 == init_vmpage(&vmpage, len * sizeof(void*)));
   a = (void**) vmpage.addr;
   s_compare_count = 0;

   for (uintptr_t i = 0; i < len; ++i) {
      a[i] = (void*) i;
   }

   for (unsigned i = 0; i < len; ++i) {
      unsigned r = ((unsigned) random()) % len;
      void * t = a[r];
      a[r] = a[i];
      a[i] = t;
   }

   time_t start = time(0);

   extern void qsortx(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *));

#if 0
   qsortx(a, len, sizeof(void*), &test_compare2);
#else
   init_mergesort(&sort);
   // TEST(0 == sortptr_mergesort(&sort, len, a, &test_compare_ptr, 0));
   TEST(0 == sortblob_mergesort(&sort, sizeof(void*), len, a, &test_compare_long, 0));
   TEST(0 == free_mergesort(&sort));
#endif

   time_t end = time(0);

   printf("time = %u\n", (unsigned) (end - start));

   for (uintptr_t i = 0; i < len; ++i) {
      TEST(a[i] == (void*) i);
   }

   printf("compare_count = %llu\n", s_compare_count);

   // unprepare
   a = 0;
   TEST(0 == free_vmpage(&vmpage));

   return 0;
ONABORT:
   free_mergesort(&sort);
   if (a) free_vmpage(&vmpage);
   return EINVAL;
}

int unittest_sort_mergesort()
{
   if (test_merge())          goto ONABORT; // TODO: remove
   if (test_presort())        goto ONABORT; // TODO: remove
   if (test_sort())           goto ONABORT; // TODO: remove
   #if 0 // TODO: remove
   if (test_stacksize())      goto ONABORT;
   if (test_memhelper())      goto ONABORT;
   if (test_initfree())       goto ONABORT;
   if (test_query())          goto ONABORT;
   if (test_set())            goto ONABORT;
   if (test_searchgrequal())  goto ONABORT;
   if (test_rsearchgrequal()) goto ONABORT;
   if (test_searchgreater())  goto ONABORT;
   if (test_rsearchgreater()) goto ONABORT;
   if (test_merge())          goto ONABORT;
   if (test_presort())        goto ONABORT;
   if (test_sort())           goto ONABORT;
   #endif // TODO: remove

   return 0;
ONABORT:
   return EINVAL;
}

#endif
