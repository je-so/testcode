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

static int test_merge(void)
{
   mergesort_t sort;

   // prepare
   init_mergesort(&sort);

   // TODO: ?

   // TEST merge_adjacent_slices
   // TEST rmerge_adjacent_slices

   // TEST merge_topofstack
   // TODO: ?

   // TEST establish_stack_invariant
   // TODO: ?

   // TEST merge_all
   // TODO: ?

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

   // prepare
   init_mergesort(&sort);

   // TEST insertsort
   // TODO: ?

   // TEST reverse_elements
   // TODO: ?

   // TEST count_presorted
   // TODO: ?

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

   return 0;
ONABORT:
   return EINVAL;
}

#endif
