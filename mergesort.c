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
 * but is always bigger or equal than this value.
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
 * The implementation choses the 6 most sign. bits of n as minsize.
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
   if (0 == (uintptr_t)a % sizeof(long) && 0 == elemsize % sizeof(long)) {
      return sortlong_mergesort(sort, elemsize, len, a, cmp, cmpstate);

   } else {
      return sortbytes_mergesort(sort, elemsize, len, a, cmp, cmpstate);
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_constants(void)
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

   // TEST lengthof(stack): size_t == uint64_t
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
   init_testerrortimer(&s_mergesort_errtimer, 1, ENOMEM);
   TEST(ENOMEM == ensuretempsize(&sort, sort.tempsize+1));
   // freed (reset to tempmem)
   TEST(sort.tempmem == sort.temp);
   TEST(sizeof(sort.tempmem) == sort.tempsize);
   TEST(0 == ismapped_vm(&vmpage, accessmode_RDWR_PRIVATE));

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

   // TODO:

   return 0;
ONABORT:
   return EINVAL;
}

static uint64_t s_compare_count = 0;

static int test_compare(void * cmpstate, const void * left, const void * right)
{
   ++s_compare_count;
   (void) cmpstate;
   // works cause all pointer values are positive integer values ==> no overflow if subtracted
   if ((uintptr_t)left < (uintptr_t)right)
      return -1;
   else if ((uintptr_t)left > (uintptr_t)right)
      return +1;
   return 0;
}

static int test_compare_blob(void * cmpstate, const void * left, const void * right)
{
   ++s_compare_count;
   (void) cmpstate;
   if (*(const uintptr_t*)left < *(const uintptr_t*)right)
      return -1;
   else if (*(const uintptr_t*)left > *(const uintptr_t*)right)
      return +1;
   return 0;
}

static int test_compare2(const void * left, const void * right)
{
   ++s_compare_count;
   if (*(const uintptr_t*)left < *(const uintptr_t*)right)
      return -1;
   else if (*(const uintptr_t*)left > *(const uintptr_t*)right)
      return +1;
   return 0;
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
   // TEST(0 == sortptr_mergesort(&sort, len, a, &test_compare, 0));
   TEST(0 == sortblob_mergesort(&sort, sizeof(void*), len, a, &test_compare_blob, 0));
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
   if (test_constants())      goto ONABORT;
   if (test_memhelper())      goto ONABORT;
   if (test_initfree())       goto ONABORT;
   if (test_sort())           goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
