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
 * The minimum nr of elements moved as one block in merge. */
#define MIN_BLK_LEN 7

/* define: MIN_SLICE_LEN
 * The minum number of elements (length) of a sorted sub-array.
 * Every presorted slice is described in <mergesort_sortedslice_t>.
 * All such slices are stored on a stack. */
#define MIN_SLICE_LEN 32

#define mergesort_IMPL_ISPOINTER   1


#if 1

#define NAME(name)   name ## _ptr
#define ELEMTYPE     void*
#define INDEXMULT    1
#define ELEMSIZE     (sizeof(void*))
#define ADDR
#define ELEM(addr)   ((void**)(addr))[0]

#else

#define NAME(name)   name ## _blob
#define ELEMTYPE     uint8_t
#define INDEXMULT    ELEMSIZE
#define ELEMSIZE     (sort->elemsize)
#define ADDR         &
#define ELEM(addr)   &((addr)[0])
#define

#endif

#define search_greatequal  NAME(search_greatequal)
#define rsearch_greatequal NAME(rsearch_greatequal)
#define search_greater     NAME(search_greater)
#define rsearch_greater    NAME(rsearch_greater)
#define merge_adjacent_slices     NAME(merge_adjacent_slices)
#define rmerge_adjacent_slices    NAME(rmerge_adjacent_slices)
#define merge_topofstack          NAME(merge_topofstack)
#define establish_stack_invariant NAME(establish_stack_invariant)
#define merge_all          NAME(merge_all)
#define insertsort         NAME(insertsort)
#define reverse_elements   NAME(reverse_elements)
#define count_presorted    NAME(count_presorted)

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
 * Ensures <mergesort_t.temp> can store templen * <mergesort_t.elemsize> bytes.
 * If the temporary array has a less capacity it is reallocated.
 *
 * Unchecked Precondition:
 * - templen * <mergesort_t.elemsize> <= (size_t)-1
 * */
static inline int ensuretempsize(mergesort_t * sort, size_t templen)
{
   assert(sort->elemsize == sizeof(void*));
   size_t tempsize = templen * sort->elemsize;
   return tempsize <= sort->tempsize ? 0 : alloctemp_mergesort(sort, tempsize);
}

// group: lifetime

int init_mergesort(/*out*/mergesort_t * sort)
{
    sort->compare  = 0;
    sort->cmpstate = 0;
    sort->elemsize = 0;
    sort->temp     = sort->tempmem;
    sort->tempsize = sizeof(sort->tempmem);
    sort->stacksize = 0;
    return 0;
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

// group: merge-helper

/* function: search_greatequal
 * Returns index x such that key <= a[x] && (x == 0 || a[x-1] < key).
 * If all values in a are lower than key, index value n is returned.
 *
 * Unchecked Precondition:
 * - n >= 1
 * - Values in array a are sorted in ascending order.
 * */
static inline size_t search_greatequal(mergesort_t * sort, void * key, size_t n, uint8_t * a/*[n*ELEMSIZE]*/)
{
   if (sort->compare(sort->cmpstate, ELEM(a), key) >= 0) return 0;

   // a[0] < key

   // increase index by doubling it until
   // a[lastidx] < key <= a[idx]
   // lastindex < idx

   size_t lastidx;
   size_t idx = 1;
   size_t off = ELEMSIZE;
   size_t n_2 = n >> 1;

   while (idx <= n_2) {
      if (sort->compare(sort->cmpstate, ELEM(a + off), key) >= 0) {
         lastidx = idx >> 1;
         goto DoBinarySearch;
      }
      idx = (idx << 1);
      off = (off << 1);
   }
   lastidx = idx >> 1;
   idx = n;

DoBinarySearch:

   // a[lastidx] < key
   ++ lastidx;
   // a[lastidx-1] < key

   while (lastidx < idx) {
      // (lastidx + idx) could overflow size_t type !
      size_t mid = lastidx + ((idx - lastidx) >> 1);
      if (sort->compare(sort->cmpstate, ELEM(a + mid * ELEMSIZE), key) >= 0)
         idx = mid;        // key <= a[idx]
      else
         lastidx = mid+1;  // a[lastidx-1] < key
   }

   return idx; // 0 < idx && idx <= n && a[idx-1] < key && (idx == n || key <= a[idx])
}

/* function: rsearch_greatequal
 * Returns index x such that key <= a[n-x] && (x == n || a[n-x-1] < key).
 * If all values in a are lower than key, index value 0 is returned.
 *
 * Unchecked Precondition:
 * - n >= 1
 * - Values in array a are sorted in ascending order.
 * */
static inline size_t rsearch_greatequal(mergesort_t * sort, void * key, size_t n, uint8_t * a/*[n*ELEMSIZE]*/)
{
   uint8_t * enda = a + n * ELEMSIZE;
   size_t    off  = ELEMSIZE;

   if (sort->compare(sort->cmpstate, ELEM(enda - off), key) < 0) return 0;

   // key <= a[n-1]

   // increase index by doubling it until
   // a[n-idx] < key <= a[n-lastindex]
   // lastindex < idx

   size_t lastidx;
   size_t idx = 2;
   size_t n_2 = n >> 1;

   while (idx <= n_2) {
      off = (off << 1);
      if (sort->compare(sort->cmpstate, ELEM(enda - off), key) < 0) {
         lastidx = idx >> 1;
         goto DoBinarySearch;
      }
      idx = (idx << 1);
   }
   lastidx = idx >> 1;
   idx = n+1;

DoBinarySearch:

   // a[n-idx] < key
   -- idx;
   // a[n-idx-1] < key

   while (lastidx < idx) {
      // (lastidx + idx) could overflow size_t type !
      size_t mid = lastidx + ((idx - lastidx +1) >> 1);
      if (sort->compare(sort->cmpstate, ELEM(enda - mid*ELEMSIZE), key) < 0)
         idx = mid-1;   // a[n-idx-1] < key
      else
         lastidx = mid; // key <= a[n-lastidx]
   }

   return idx; // 0 < idx && idx <= n && (idx == n || a[n-idx-1] < key) && key <= a[n-idx]
}

/* function: search_greater
 * Returns index x such that key < a[x] && (x == 0 || a[x-1] <= key).
 * If all values in a are lower than key, index value n is returned.
 *
 * Unchecked Precondition:
 * - n >= 1
 * - Values in array a are sorted in ascending order.
 * */
static inline size_t search_greater(mergesort_t * sort, void * key, size_t n, uint8_t * a/*[n*ELEMSIZE]*/)
{
   if (sort->compare(sort->cmpstate, ELEM(a), key) > 0) return 0;

   // a[0] <= key

   // increase index by doubling it until
   // a[lastidx] <= key < a[idx]
   // lastindex < idx

   size_t lastidx;
   size_t idx = 1;
   size_t off = ELEMSIZE;
   size_t n_2 = n >> 1;

   while (idx <= n_2) {
      if (sort->compare(sort->cmpstate, ELEM(a + off), key) > 0) {
         lastidx = idx >> 1;
         goto DoBinarySearch;
      }
      idx = (idx << 1);
      off = (off << 1);
   }
   lastidx = idx >> 1;
   idx = n;

DoBinarySearch:

   // a[lastidx] <= key
   ++ lastidx;
   // a[lastidx-1] <= key

   while (lastidx < idx) {
      // (lastidx + idx) could overflow size_t type !
      size_t mid = lastidx + ((idx - lastidx) >> 1);
      if (sort->compare(sort->cmpstate, ELEM(a + mid*ELEMSIZE), key) > 0)
         idx = mid;        // key < a[idx]
       else
         lastidx = mid+1;  // a[lastidx-1] <= key
   }

   return idx; // 0 < idx && idx <= n && a[idx-1] <= key && (idx == n || key < a[idx])
}

/* function: rsearch_greater
 * Returns index x such that key < a[n-x] && (x == n || a[n-x-1] <= key).
 * If all values in a are lower than key, index value 0 is returned.
 *
 * Unchecked Precondition:
 * - n >= 1
 * - Values in array a are sorted in ascending order.
 * */
static inline size_t rsearch_greater(mergesort_t * sort, void * key, size_t n, uint8_t * a/*[n*ELEMSIZE]*/)
{
   uint8_t * enda = a + n * ELEMSIZE;
   size_t    off  = ELEMSIZE;

   if (sort->compare(sort->cmpstate, ELEM(enda - off), key) <= 0) return 0;

   // key < a[n-1]

   // increase index by doubling it until
   // a[n-idx] <= key < a[n-lastindex]
   // lastindex < idx

   size_t lastidx;
   size_t idx = 2;
   size_t n_2 = n >> 1;

   while (idx <= n_2) {
      off = (off << 1);
      if (sort->compare(sort->cmpstate, ELEM(enda - off), key) <= 0) {
         lastidx = idx >> 1;
         goto DoBinarySearch;
      }
      idx = (idx << 1);
   }
   lastidx = idx >> 1;
   idx = n+1;

DoBinarySearch:

   // a[n-idx] <= key
   -- idx;
   // a[n-idx-1] <= key

   while (lastidx < idx) {
      // size_t mid = (lastidx + idx) >> 1; is wrong
      // (lastidx + idx) could overflow size_t type !
      size_t mid = lastidx + ((idx - lastidx +1) >> 1);
      if (sort->compare(sort->cmpstate, ELEM(enda - mid*ELEMSIZE), key) <= 0)
         idx = mid-1;   // a[n-idx-1] <= key
      else
         lastidx = mid; // key < a[n-lastidx]
   }

   return idx; // 0 < idx && idx <= n && (idx == n || a[n-idx-1] <= key) && key < a[n-idx]
}

/* function: merge_adjacent_slices
 * Merge the llen elements starting at left with the rlen elements starting at right
 * in a stable way. After success the array left contains
 * (llen+rlen) elements in sorted order.
 * Return 0 if successful.
 *
 * The merging is done from lower to higher values.
 *
 *
 * Unchecked Precondition:
 * - Array slice left is followed by slice right.
 *   (left + llen * ELEMSIZE) == right
 * - Both sub-arrays are sorted.
 * - llen <= rlen
 * - llen >= 1 && rlen >= 1
 */
static int merge_adjacent_slices(
   mergesort_t * sort,
   uint8_t  * left,  size_t llen,
   uint8_t  * right, size_t rlen)
{
   int err;
   uint8_t * dest;
   // TODO:
   size_t    minblklen = 3*MIN_BLK_LEN;
   size_t    lblklen;   /* # of times element in left is smallest */
   size_t    rblklen;   /* # of times element in right is smallest */
   size_t    nrofbytes;

   /*
    * All elements in left[0..lblklen-1] <= b[0] are already in place.
    */
   lblklen = search_greater(sort, ELEM(right), llen, left); // TODO: remove cast
   left += lblklen * ELEMSIZE;
   llen -= lblklen;
   if (llen == 0) return 0;

   err = ensuretempsize(sort, llen);
   if (err) return err;

   memcpy(sort->temp, left, llen * ELEMSIZE);
   dest = left;
   left = sort->temp;

#define COPY(dest, src, nr) \
         nrofbytes = (nr)*ELEMSIZE; \
         memcpy(dest, src, nrofbytes); \
         dest += nrofbytes; \
         src  += nrofbytes

#define MOVE(dest, src, nr) \
         nrofbytes = (nr)*ELEMSIZE; \
         memmove(dest, src, nrofbytes); \
         dest += nrofbytes; \
         src  += nrofbytes

   COPY(dest, right, 1);
   --rlen;
   if (rlen == 0) goto DONE;

   for (;;) {
      lblklen = 0;
      rblklen = 0;

     /* Do the straightforward thing until (if ever) one run
      * appears to win consistently.
      */
      for (;;) {
         assert(llen > 0 && rlen > 0);
         if (sort->compare(sort->cmpstate, ELEM(right), ELEM(left)) < 0) {
            COPY(dest, right, 1);
            ++rblklen;
            lblklen = 0;
            --rlen;
            if (rlen == 0) goto DONE;
            if (rblklen >= minblklen)
               break;

         } else {
            COPY(dest, left, 1);
            ++lblklen;
            rblklen = 0;
            --llen;
            if (llen == 0) goto DONE;
            if (lblklen >= minblklen)
               break;
         }
      }

     /* One run is winning so consistently that galloping may
      * be a huge win.  So try that, and continue galloping until
      * (if ever) neither run appears to be winning consistently
      * anymore.
      */
      ++minblklen;
      do {
         assert(llen > 0 && rlen > 0);
         minblklen -= (MIN_BLK_LEN != 1);
         lblklen = search_greater(sort, ELEM(right), llen, left);
         if (lblklen) {
            COPY(dest, left, lblklen);
            llen -= lblklen;
            if (llen == 0) goto DONE;
         }
         COPY(dest, right, 1);
         --rlen;
         if (rlen == 0) goto DONE;

         rblklen = search_greatequal(sort, ELEM(left), rlen, right);
         if (rblklen) {
            MOVE(dest, right, rblklen);
            rlen -= rblklen;
            if (rlen == 0) goto DONE;
         }
         COPY(dest, left, 1);
         --llen;
         if (llen == 0) goto DONE;
      } while (lblklen >= MIN_BLK_LEN || rblklen >= MIN_BLK_LEN);
      ++minblklen;           /* penalize it for leaving galloping mode */
   }

#undef COPY
#undef MOVE

DONE:
   if (llen) memcpy(dest, left, llen * ELEMSIZE);
   return 0;
}

/* function: rmerge_adjacent_slices
 * TODO:
 * Merge the llen elements starting at pa with the rlen elements starting at pb
 * in a stable way, in-place.  llen and rlen must be > 0, and pa + llen == pb.
 * Must also have that *pb < *pa, that pa[llen-1] belongs at the end of the
 * merge, and should have llen >= rlen.  See listsort.txt for more info.
 * Return 0 if successful.
 *
 * The merging is done in reverse order from higher to lower values.
 *
 * Unchecked Precondition:
 * - Array slice left is followed by slice pb.
 *   (left + llen * ELEMSIZE) == (uint8_t*)pb
 * - Both sub-arrays are sorted.
 * - rlen <= llen
 * - llen >= 1 && rlen >= 1 */
static int rmerge_adjacent_slices(
   mergesort_t * sort,
   ELEMTYPE * pa, size_t llen,
   ELEMTYPE * pb, size_t rlen)
{
   int err;
   ELEMTYPE * dest;
   ELEMTYPE * basea;
   ELEMTYPE * baseb;
   // TODO:
   size_t     minblklen = 3*MIN_BLK_LEN;
   size_t    lblklen;   /* # of times element in left is smallest */
   size_t    rblklen;   /* # of times element in right is smallest */

   assert(pa && pb && llen > 0 && rlen > 0 && pa + llen == pb);

   /* Where does a end in b?  Elements in b after that can be
    * ignored (already in place).
    */
   rblklen = rsearch_greatequal(sort, ADDR pa[(llen-1)*INDEXMULT], rlen, (uint8_t*) pb); // TODO: remove cast to (uint8_t*)
   rlen -= rblklen;
   if (rlen == 0) return 0;

   err = ensuretempsize(sort, rlen);
   if (err) return err;

   dest  = pb;
   dest += rlen*INDEXMULT;
   memcpy(sort->temp, pb, rlen * ELEMSIZE);
   basea = pa;
   baseb = sort->temp;
   pb = sort->temp;
   pb += rlen*INDEXMULT;
   pa += llen*INDEXMULT;

#define COPY(dest, src, nr) \
         dest -= (nr)*INDEXMULT; \
         src  -= (nr)*INDEXMULT; \
         memcpy(dest, src, (nr)*ELEMSIZE)

#define MOVE(dest, src, nr) \
         dest -= (nr)*INDEXMULT; \
         src  -= (nr)*INDEXMULT; \
         memmove(dest, src, (nr)*ELEMSIZE)

   COPY(dest, pa, 1);
   --llen;
   if (llen == 0) goto DONE;

   for (;;) {
      lblklen = 0;
      rblklen = 0;

      /* Do the straightforward thing until (if ever) one run
       * appears to win consistently.
       */
      for (;;) {
         assert(llen > 0 && rlen > 0);
         if (sort->compare(sort->cmpstate, ADDR pb[-1*INDEXMULT], ADDR pa[-1*INDEXMULT]) < 0) {
            COPY(dest, pa, 1);
            ++lblklen;
            rblklen = 0;
            --llen;
            if (llen == 0) goto DONE;
            if (lblklen >= minblklen)
               break;

         } else {
            COPY(dest, pb, 1);
            ++rblklen;
            lblklen = 0;
            --rlen;
            if (rlen == 0) goto DONE;
            if (rblklen >= minblklen)
               break;
         }
      }

      /* One run is winning so consistently that galloping may
       * be a huge win.  So try that, and continue galloping until
       * (if ever) neither run appears to be winning consistently
       * anymore.
       */
      ++minblklen;
      do {
         assert(llen > 0 && rlen > 0);
         minblklen -= (minblklen != 1);
         lblklen = rsearch_greater(sort, ADDR pb[-1*INDEXMULT], llen, (uint8_t*)basea); // TODO: remove cast
         if (lblklen) {
            MOVE(dest, pa, lblklen);
            llen -= lblklen;
            if (llen == 0) goto DONE;
         }
         COPY(dest, pb, 1);
         --rlen;
         if (rlen == 0) goto DONE;

         rblklen = rsearch_greatequal(sort, ADDR pa[-1*INDEXMULT], rlen, (uint8_t*)baseb); // TODO: remove cast to (uint8_t*)
         if (rblklen) {
            COPY(dest, pb, rblklen);
            rlen -= rblklen;
            if (rlen == 0) goto DONE;
         }
         COPY(dest, pa, 1);
         --llen;
         if (llen == 0) goto DONE;
      } while (lblklen >= MIN_BLK_LEN || rblklen >= MIN_BLK_LEN);
      ++minblklen;           /* penalize it for leaving galloping mode */
   }

DONE:
   if (rlen) memcpy(dest-rlen*INDEXMULT, baseb, rlen * sizeof(void*));
   return 0;
}

/* function: merge_topofstack
 * Merges two sorted sub-arrays at top of stack.
 * If isSecondTop is true the two arrays at stack indices -3 and -2
 * are merged else at stack indices -2 and -1. */
static int merge_topofstack(
   mergesort_t * sort,
   bool        isSecondTop)
{
   size_t n = (-- sort->stacksize) - isSecondTop;

   uint8_t * left  = sort->stack[n-1].base;
   size_t    llen  = sort->stack[n-1].len;
   uint8_t * right = sort->stack[n].base;
   size_t    rlen  = sort->stack[n].len;
   sort->stack[n-1].len = llen + rlen;
   if (isSecondTop) {
      sort->stack[n] = sort->stack[n+1];
   }

   // Minimize size of temp array: min(llen, rlen)
   if (llen <= rlen)
      return merge_adjacent_slices(sort, left, llen, right, rlen);
   else
      return rmerge_adjacent_slices(sort, (ELEMTYPE*)left, llen, (ELEMTYPE*)right, rlen);
}

/* function: establish_stack_invariant
 * Merge adjacent sub-arrays on top of stack until invariant is re-established.
 *
 * Size Invariant:
 * The abstract function len(index) determines the length of the pushed
 * sub-array on the stack. The index -1 is the top element.
 * - len(-3) > len(-2) + len(-1)
 * - len(-2) > len(-1)
 *
 * Returns 0 on success, != 0 on error.
 */
static int establish_stack_invariant(mergesort_t * sort)
{
   int err;
   struct mergesort_sortedslice_t * slice = sort->stack;

   while (sort->stacksize > 1) {
      bool   isSecondTop = false;
      size_t n = sort->stacksize;

      if (n > 2 && slice[n-3].len <= slice[n-2].len + slice[n-1].len) {
         if (slice[n-3].len <= slice[n-1].len) {
            isSecondTop = true;
         }

      } else if (slice[n-2].len > slice[n-1].len) {
         break;
      }

      err = merge_topofstack(sort, isSecondTop);
      if (err) return err;
   }

   return 0;
}

/* function: merge_all
 *
 * Regardless of invariants, merge all runs on the stack until only one
 * remains.  This is used at the end of the mergesort.
 *
 * Returns 0 on success, -1 on error.
 */
static int merge_all(mergesort_t * sort)
{
   int err;

   while (sort->stacksize > 1) {
      err = merge_topofstack(sort, false);
      if (err) return err;
   }

   return 0;
}

/* function: insertsort
 * Sorts array a of length len.
 * insertsort is a stable sort for sorting small arrays.
 * It has O(n log n) compares, but can do O(n * n) data movements
 * in the worst case.
 *
 * Unchecked Precondition:
 * - start > 0 && start <= len
 * - a[0 .. start-1] is already sorted
*/
static int insertsort(mergesort_t * sort, uint8_t start, uint8_t len, void * a[len])
{
   sort_compare_f compare  = sort->compare;
   void         * cmpstate = sort->cmpstate;

   for (unsigned i = start; i < len; ++i) {
      /* Invariant: i > 0 && a[0 .. i-1] sorted */
      unsigned l = 0;
      unsigned r = i;
      void * next = a[i];
      do {
         unsigned mid = (r + l) >> 1;
         if (compare(cmpstate, next, a[mid]) < 0)
            r = mid;
         else
            l = mid+1;
      } while (l < r);
      for (r = i; r > l; --r)
         a[r] = a[r-1];
      a[l] = next;
   }

   return 0;
}

/* function: reverse_elements
 * Reverse the elements of an array from a[0] up to a[len-1].
 *
 * Unchecked Precondition:
 * - len >= 2 */
static inline void reverse_elements(size_t len, void * a[len])
{
   size_t lo = 0;
   size_t hi = len-1;
   do {
      void * t = a[lo];
      a[lo++] = a[hi];
      a[hi--] = t;
   } while (lo < hi);
}

/* function: count_presorted
 * Return the length of an already sorted sub-array beginning at a[0].
 * An already sorted set is the longest ascending sequence, with
 *
 *  a[0] <= a[1] <= a[2] <= ...
 *
 * or the longest descending sequence, with
 *
 *  a[0] > a[1] > a[2] > ...
 *
 * A descending sequence is transormed into an ascending sequence
 * by calling reverse_elements.
 *
 * In the descending case the strictness property a[0] > a[1] is needed instead of a[0] >= a[1].
 * Calling reverse_elements would violate the stability property in case of equal elements. */
static inline size_t count_presorted(mergesort_t * sort, size_t len, void * a[len])
{
   size_t n;

   if (len <= 1)
      return len;

   n = 1;
   if (sort->compare(sort->cmpstate, a[n], a[n-1]) < 0) {
      for (n = n + 1; n < len; ++n) {
         if (sort->compare(sort->cmpstate, a[n], a[n-1]) < 0)
            continue;
         break;
      }

      reverse_elements(n, a);

   } else {
      for (n = n + 1; n < len; ++n) {
         if (sort->compare(sort->cmpstate, a[n], a[n-1]) < 0)
            break;
      }
   }

   return n;
}

// group: sort

#if (mergesort_IMPL_ISPOINTER == 1)
int sortptr_mergesort(mergesort_t * sort, size_t len, void * a[len], sort_compare_f cmp, void * cmpstate)

#elif (mergesort_IMPL_ISPOINTER == 0)
int sortblob_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, void * a/*uint8_t[len]*/, sort_compare_f cmp, void * cmpstate)

#else
#error "Expect (mergesort_IMPL_ISPOINTER == 0) || (mergesort_IMPL_ISPOINTER == 1)"
#endif
{
   int err;

   if (len < 2) return 0;

   #if (mergesort_IMPL_ISPOINTER == 1)
   err = setsortstate(sort, cmp, cmpstate, sizeof(void*), len);
   if (err) goto ONABORT;
   #else
   err = setsortstate(sort, cmp, cmpstate, elemsize, len);
   if (err) goto ONABORT;
   #endif

  /*
   * Scanning array left to right, finding presorted sub arrays,
   * and extending short sub arrays to minsize elements.
   */
   uint8_t    minlen = compute_minslicelen(len);
   ELEMTYPE * next   = a;
   size_t     nextlen = len;

   do {
      /* get size of next presorted sub array */
      size_t subarray_len = count_presorted(sort, nextlen, next);

      /* extend size to min(minlen, nextlen). */
      if (subarray_len < minlen) {
         const uint8_t extlen = (uint8_t) ( nextlen <= minlen
                              ? nextlen : (unsigned)minlen);
         err = insertsort(sort, (uint8_t)subarray_len, extlen, next);
         if (err) goto ONABORT;
         subarray_len = extlen;
      }

      /* push sub-array onto stack of unmerged sub-arrays */
      if (sort->stacksize == lengthof(sort->stack)) {
         // stack size chosen too small for size_t type
         err = merge_topofstack(sort, false);
         if (err) goto ONABORT;
      }
      sort->stack[sort->stacksize].base = next;
      sort->stack[sort->stacksize].len  = subarray_len;
      ++sort->stacksize;

      /* merge sub-array on stack until stack invariant is established */
      err = establish_stack_invariant(sort);
      if (err) goto ONABORT;

      /* advance */
      next    += subarray_len*INDEXMULT;
      nextlen -= subarray_len;
   } while (nextlen);

   err = merge_all(sort);
   if (err) goto ONABORT;

   // TODO:
   assert(sort->stacksize == 1);
   assert(sort->stack[0].base == a);
   assert(sort->stack[0].len  == len);

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
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

static int test_compare(void * cmpstate, const void * left, const void * right);

static int test_initfree(void)
{
   mergesort_t sort = mergesort_FREE;

   // TEST sort_FREE
   // TODO:
   TEST(0 == sort.compare);
   TEST(0 == sort.cmpstate);
   TEST(0 == sort.elemsize);
   TEST(0 == sort.temp);
   TEST(0 == sort.tempsize);
   TEST(0 == sort.stacksize);


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

static int test_compare2(const void * left, const void * right)
{
   ++s_compare_count;
   // works cause all pointer values are positive integer values ==> no overflow if subtracted
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
   s_compare_count = 0;
   TEST(0 == init_vmpage(&vmpage, len * sizeof(void*)));
   a = (void**) vmpage.addr;

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

#if 0
   qsort(a, len, sizeof(void*), &test_compare2);
#else
   TEST(0 == init_mergesort(&sort))
   TEST(0 == sortptr_mergesort(&sort, len, a, &test_compare, 0));
   TEST(0 == free_mergesort(&sort));
#endif

   time_t end = time(0);

   printf("time = %u\n", (unsigned) (end - start));

   for (uintptr_t i = 0; i < len; ++i) {
      TEST(a[i] == (void*) i);
   }

   printf("compare_count = %llu\n", s_compare_count);

exit(0);

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
   if (test_initfree())       goto ONABORT;
   if (test_sort())           goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
