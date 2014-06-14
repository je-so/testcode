
// section: mergesort_t

#undef search_greatequal
#undef rsearch_greatequal
#undef search_greater
#undef rsearch_greater
#undef merge_adjacent_slices
#undef rmerge_adjacent_slices
#undef merge_topofstack
#undef establish_stack_invariant
#undef merge_all
#undef insertsort
#undef reverse_elements
#undef count_presorted
#undef NAME
#undef ELEMSIZE
#undef ELEM
#undef INITFASTCOPY
#undef COPYINCR_1
#undef COPYDECR_1
#undef COPYINCR
#undef COPYDECR
#undef SWAP

// every selected mergesort_IMPL_TYPE has its own namespace

#define search_greatequal         NAME(search_greatequal)
#define rsearch_greatequal        NAME(rsearch_greatequal)
#define search_greater            NAME(search_greater)
#define rsearch_greater           NAME(rsearch_greater)
#define merge_adjacent_slices     NAME(merge_adjacent_slices)
#define rmerge_adjacent_slices    NAME(rmerge_adjacent_slices)
#define merge_topofstack          NAME(merge_topofstack)
#define establish_stack_invariant NAME(establish_stack_invariant)
#define merge_all                 NAME(merge_all)
#define insertsort                NAME(insertsort)
#define reverse_elements          NAME(reverse_elements)
#define count_presorted           NAME(count_presorted)

/* group: generic-macros
 *
 * These macros differentiate the functionality of the different functions.
 * All generic implementations are equal except for the macros. */

// ==== mergesort_TYPE_POINTER ====

#if (mergesort_IMPL_TYPE == mergesort_TYPE_POINTER)

/* define: NAME
 * Maps name to a specific implementation name.
 * For example merge_all is mapped to merge_all_ptr, merge_all_long, or merge_all_bytes. */
#define NAME(name)   name ## _ptr
/* define: ELEMSIZE
 * The size of the element. This value is used as optimization in
 * case of mergesort_IMPL_TYPE==mergesort_TYPE_POINTER. In the pointer
 * case it is defined as constant sizeof(void*) instead of variable <mergesort_t->elemsize>. */
#define ELEMSIZE     (sizeof(void*))
/* define: ELEM
 * Access of single element from its address.
 * The return value is either addr or the content of the element
 * in case of type void*. The compare function uses this value. */
#define ELEM(addr)   ((void**)(addr))[0]
/* define: INITFASTCOPY
 * Initializes variable fastcopy. Which is true in case
 * of a single byte or long value. In the pointer case where the elementsize
 * is known (sizeof(void*)) it is not necessary and therefore left empty. */
#define INITFASTCOPY

/* define: COPYINCR_1
 * Copies a single element from address src to address dest.
 * The pointer src and dest (type uint8_t*) are incremented afterwards. */
#define COPYINCR_1(dest, src) \
{ \
   *(void**)(dest) = *(void**)(src); \
   dest += sizeof(void*); \
   src  += sizeof(void*); \
}

/* define: COPYDECR_1
 * Copies a single element from address src to address dest.
 * The pointer src and dest (type uint8_t*) are decremented before the copy operation. */
#define COPYDECR_1(dest, src) \
{ \
   dest -= sizeof(void*); \
   src  -= sizeof(void*); \
   *(void**)(dest) = *(void**)(src); \
}

/* define: COPYINCR
 * Copies nr elements from address src to address dest.
 * The pointer src and dest (type uint8_t*) are incremented after every single element
 * by the size of the element.
 *
 * Unchecked Precondition:
 * - dest < src || memory_does_not_overlap(src, dest, nr*ELEMSIZE) */
#define COPYINCR(dest, src, nr) \
{ \
   size_t _n = (nr); \
   do { \
      *(void**)(dest) = *(void**)(src); \
      dest += sizeof(void*); \
      src  += sizeof(void*); \
   } while (--_n); \
}

/* define: COPYDECR
 * Copies nr elements from address src to address dest.
 * The pointer src and dest (type uint8_t*) are decremented before copying
 * every single element by the size of the element.
 *
 * Unchecked Precondition:
 * - dest > src || memory_does_not_overlap(src, dest, nr*ELEMSIZE) */
#define COPYDECR(dest, src, nr) \
{ \
   size_t _n = (nr); \
   do { \
      dest -= sizeof(void*); \
      src  -= sizeof(void*); \
      *(void**)(dest) = *(void**)(src); \
   } while (--_n); \
}

/* define: SWAP
 * Swaps the element at lo with element at hi.
 * Pointer lo and hi (type uint8_t*) must point
 * directly to the element. They are not changed after return. */
#define SWAP(lo, hi) \
{ \
   void * t = *(void**)lo; \
   *(void**)lo = *(void**)hi; \
   *(void**)hi = t; \
}

// ==== mergesort_TYPE_LONG ====

#elif (mergesort_IMPL_TYPE == mergesort_TYPE_LONG)

#define NAME(name)   name ## _long
#define ELEMSIZE     (sort->elemsize)
#define ELEM(addr)   (addr)
#define INITFASTCOPY const int fastcopy = (ELEMSIZE == sizeof(long));

#define COPYINCR_1(dest, src) \
{ \
   if (fastcopy) { \
      *(long*)(dest) = *(long*)(src); \
      dest += sizeof(long); \
      src  += sizeof(long); \
   } else { \
      COPYINCR(dest, src, 1u); \
   } \
}

#define COPYDECR_1(dest, src) \
{ \
   if (fastcopy) { \
      dest -= sizeof(long); \
      src  -= sizeof(long); \
      *(long*)(dest) = *(long*)(src); \
   } else { \
      COPYDECR(dest, src, 1u); \
   } \
}

#define COPYINCR(dest, src, nr) \
{ \
   size_t _n = (nr) * ELEMSIZE / sizeof(long); \
   do { \
      *(long*)(dest) = *(long*)(src); \
      dest += sizeof(long); \
      src  += sizeof(long); \
   } while (--_n); \
}

#define COPYDECR(dest, src, nr) \
{ \
   size_t _n = (nr) * ELEMSIZE / sizeof(long); \
   do { \
      dest -= sizeof(long); \
      src  -= sizeof(long); \
      *(long*)(dest) = *(long*)(src); \
   } while (--_n); \
}

#define SWAP(lo, hi) \
{ \
   if (fastcopy) { \
      long t = *(long*) lo; \
      *(long*) lo = *(long*) hi; \
      *(long*) hi = t; \
   } else { \
      long * _l = (long*) lo; \
      long * _h = (long*) hi; \
      size_t _n = ELEMSIZE / sizeof(long); \
      do { \
         long t = *_l; \
         *_l = *_h; \
         *_h = t; \
         ++ _l; \
         ++ _h; \
      } while (--_n); \
   } \
}

// ==== mergesort_TYPE_BYTES ====

#elif (mergesort_IMPL_TYPE == mergesort_TYPE_BYTES)

#define NAME(name)   name ## _bytes
#define ELEMSIZE     (sort->elemsize)
#define ELEM(addr)   (addr)
#define INITFASTCOPY const int fastcopy = (ELEMSIZE == 1);

#define COPYINCR_1(dest, src) \
{ \
   if (fastcopy) { \
      *(dest) = *(src); \
      ++ dest; \
      ++ src; \
   } else { \
      COPYINCR(dest, src, 1u); \
   } \
}

#define COPYDECR_1(dest, src) \
{ \
   if (fastcopy) { \
      -- dest; \
      -- src; \
      *(dest) = *(src); \
   } else { \
      COPYDECR(dest, src, 1u); \
   } \
}

#define COPYINCR(dest, src, nr) \
{ \
   size_t _n = (nr) * ELEMSIZE; \
   do { \
      *(dest) = *(src); \
      ++ dest; \
      ++ src; \
   } while (--_n); \
}

#define COPYDECR(dest, src, nr) \
{ \
   size_t _n = (nr) * ELEMSIZE; \
   do { \
      -- dest; \
      -- src; \
      *(dest) = *(src); \
   } while (--_n); \
}

#define SWAP(lo, hi) \
{ \
   if (fastcopy) { \
      uint8_t t = *(lo); \
      *(lo) = *(hi); \
      *(hi) = t; \
   } else { \
      uint8_t * _l = lo; \
      uint8_t * _h = hi; \
      size_t _n = ELEMSIZE; \
      do { \
         uint8_t t = *_l; \
         *_l = *_h; \
         *_h = t; \
         ++ _l; \
         ++ _h; \
      } while (--_n); \
   } \
}

#else

   #error "Define mergesort_IMPL_TYPE has wrong value"
   #error "Supported values: mergesort_TYPE_POINTER, mergesort_TYPE_LONG, mergesort_TYPE_BYTES"

#endif

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
   size_t    minblklen = 2*MIN_BLK_LEN;
   size_t    lblklen;   /* # of times element in left is smallest */
   size_t    rblklen;   /* # of times element in right is smallest */
   INITFASTCOPY;

   /*
    * All elements in left[0..lblklen-1] <= right[0] are already in place.
    */
   lblklen = search_greater(sort, ELEM(right), llen, left);
   left += lblklen * ELEMSIZE;
   llen -= lblklen;
   if (llen == 0) return 0;

   err = ensuretempsize(sort, (size_t) (right - left));
   if (err) return err;

   memcpy(sort->temp, left, (size_t) (right - left));
   dest = left;
   left = sort->temp;

   COPYINCR_1(dest, right);
   --rlen;
   if (rlen == 0) goto DONE;

   for (;;) {
      lblklen = 0;
      rblklen = 0;

     /*
      * Copy lowest element from left or right to dest.
      */
      for (;;) {
         if (sort->compare(sort->cmpstate, ELEM(right), ELEM(left)) < 0) {
            COPYINCR_1(dest, right);
            --rlen;
            if (rlen == 0) goto DONE;
            ++rblklen;
            lblklen = 0;
            if (rblklen >= minblklen)
               break;

         } else {
            COPYINCR_1(dest, left);
            --llen;
            if (llen == 0) goto DONE;
            ++lblklen;
            rblklen = 0;
            if (lblklen >= minblklen)
               break;
         }
      }

     /*
      * minblklen elements were copied from one array en bloc.
      * So try to copy blocks of elements, and continue until
      * the blocksize is less than MIN_BLK_LEN.
      */
      do {
         // lower blockmode barrier
         minblklen -= (MIN_BLK_LEN != 1);
         lblklen = search_greater(sort, ELEM(right), llen, left);
         if (lblklen) {
            COPYINCR(dest, left, lblklen);
            llen -= lblklen;
            if (llen == 0) goto DONE;
         }
         COPYINCR_1(dest, right);
         --rlen;
         if (rlen == 0) goto DONE;

         rblklen = search_greatequal(sort, ELEM(left), rlen, right);
         if (rblklen) {
            COPYINCR(dest, right, rblklen);
            rlen -= rblklen;
            if (rlen == 0) goto DONE;
         }
         COPYINCR_1(dest, left);
         --llen;
         if (llen == 0) goto DONE;
      } while (lblklen >= MIN_BLK_LEN || rblklen >= MIN_BLK_LEN);
      minblklen += 2; // raise blockmode barrier if less than two runs
   }

DONE:
   if (llen) memcpy(dest, left, llen * ELEMSIZE);
   return 0;
}

/* function: rmerge_adjacent_slices
 * Merge the llen elements starting at left with the rlen elements starting at right
 * in a stable way. After success the array left contains
 * (llen+rlen) elements in sorted order.
 * Return 0 if successful.
 *
 * The merging is done in reverse order from higher to lower values.
 *
 * Unchecked Precondition:
 * - Array slice left is followed by slice right.
 *   (left + llen * ELEMSIZE) == right
 * - Both sub-arrays are sorted.
 * - rlen <= llen
 * - llen >= 1 && rlen >= 1 */
static int rmerge_adjacent_slices(
   mergesort_t * sort,
   uint8_t * left,  size_t llen,
   uint8_t * right, size_t rlen)
{
   int err;
   uint8_t * dest;
   uint8_t * lend = right;
   uint8_t * rend;
   size_t    minblklen = 2*MIN_BLK_LEN;
   size_t    lblklen;   /* # of times element in left is smallest */
   size_t    rblklen;   /* # of times element in right is smallest */
   size_t    nrofbytes;
   INITFASTCOPY;

   /*
    * All elements in right[0..rblklen-1] >= left[(llen-1)*ELEMSIZE] are already in place.
    */
   rblklen = rsearch_greatequal(sort, ELEM(lend - ELEMSIZE), rlen, right);
   rlen -= rblklen;
   if (rlen == 0) return 0;

   nrofbytes = rlen * ELEMSIZE;
   err = ensuretempsize(sort, nrofbytes);
   if (err) return err;

   memcpy(sort->temp, right, nrofbytes);
   dest  = right + nrofbytes;
   right = sort->temp;
   rend  = right + nrofbytes;

   COPYDECR_1(dest, lend);
   --llen;
   if (llen == 0) goto DONE;

   for (;;) {
      lblklen = 0;
      rblklen = 0;

     /*
      * Copy highest element from left or right to dest.
      */
      for (;;) {
         if (sort->compare(sort->cmpstate, ELEM(rend - ELEMSIZE), ELEM(lend - ELEMSIZE)) < 0) {
            COPYDECR_1(dest, lend);
            --llen;
            if (llen == 0) goto DONE;
            ++lblklen;
            rblklen = 0;
            if (lblklen >= minblklen)
               break;

         } else {
            COPYDECR_1(dest, rend);
            --rlen;
            if (rlen == 0) goto DONE;
            ++rblklen;
            lblklen = 0;
            if (rblklen >= minblklen)
               break;
         }
      }

     /*
      * minblklen elements were copied from one array en bloc.
      * So try to copy blocks of elements, and continue until
      * the blocksize is less than MIN_BLK_LEN.
      */
      do {
         // lower blockmode barrier
         minblklen -= (minblklen != 1);
         lblklen = rsearch_greater(sort, ELEM(rend - ELEMSIZE), llen, left);
         if (lblklen) {
            COPYDECR(dest, lend, lblklen);
            llen -= lblklen;
            if (llen == 0) goto DONE;
         }
         COPYDECR_1(dest, rend);
         --rlen;
         if (rlen == 0) goto DONE;

         rblklen = rsearch_greatequal(sort, ELEM(lend - ELEMSIZE), rlen, right);
         if (rblklen) {
            COPYDECR(dest, rend, rblklen);
            rlen -= rblklen;
            if (rlen == 0) goto DONE;
         }
         COPYDECR_1(dest, lend);
         --llen;
         if (llen == 0) goto DONE;
      } while (lblklen >= MIN_BLK_LEN || rblklen >= MIN_BLK_LEN);
      minblklen += 2; // raise blockmode barrier if less than two runs
   }

DONE:
   // rlen != 0 ==> dest - rlen * ELEMSIZE == left
   if (rlen) memcpy(left, right, rlen * ELEMSIZE);
   return 0;
}

/* function: merge_topofstack
 * Merges two sorted sub-arrays at top of stack.
 * If isSecondTop is true the two arrays at stack indices -3 and -2
 * are merged else at stack indices -2 and -1.
 *
 * This function calls either <merge_adjacent_slices> or <rmerge_adjacent_slices>
 * depending on the length of the left or right slice.
 *
 * This helps to minimize the size of the temporary array used during the merge
 * operation.
 *
 * */
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
      return rmerge_adjacent_slices(sort, left, llen, right, rlen);
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
 * Merge all slices on the stack until the whole arrray is sorted.
 * This is used at the end of the mergesort.
 *
 */
static inline int merge_all(mergesort_t * sort)
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
 * It has O(n log n) compares, but has O(n * n) data movements
 * in the worst case.
 *
 * Uses <mergesort_t.tempmem>.
 *
 * Unchecked Precondition:
 * - start > 0 && start <= len
 * - a[0 .. start-1] is already sorted
*/
static int insertsort(mergesort_t * sort, uint8_t start, uint8_t len, uint8_t * a/*[len*ELEMSIZE]*/)
{
   INITFASTCOPY;
   uint8_t * next = (uint8_t*)a + start * ELEMSIZE;
   for (unsigned i = start; i < len; ++i, next += ELEMSIZE) {
      /* Invariant: i > 0 && a[0 .. i-1] sorted */
      unsigned l = 0;
      unsigned r = i;
      do {
         unsigned mid = (r + l) >> 1;
         if (sort->compare(sort->cmpstate, ELEM(next), ELEM(a + mid*ELEMSIZE)) < 0)
            r = mid;
         else
            l = mid+1;
      } while (l < r);
      if (i != l) {
         uint8_t * dest = sort->tempmem;
         uint8_t * src  = next;
         COPYINCR_1(dest, src);
         dest = next;
         COPYDECR(src, dest, (size_t)(i-l))
         src = sort->tempmem;
         COPYINCR_1(dest, src);
      }
   }

   return 0;
}

/* function: reverse_elements
 * Reverse the elements of an array from a[0] up to a[len-1].
 *
 * Unchecked Precondition:
 * - lo < hi
 * - lo points to first element a[0]
 * - hi points to last element a[(n-1)*ELEMSIZE] */
static inline void reverse_elements(mergesort_t * sort, uint8_t * lo, uint8_t * hi)
{
   (void) sort; // not necessary in ptr case
   INITFASTCOPY;
   do {
      SWAP(lo, hi);
      lo += ELEMSIZE;
      hi -= ELEMSIZE;
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
static inline size_t count_presorted(mergesort_t * sort, size_t len, uint8_t * a/*[len*ELEMSIZE]*/)
{
   if (len <= 1)
      return len;

   uint8_t * next = a;
   size_t    n;
   if (sort->compare(sort->cmpstate, ELEM(next + ELEMSIZE), ELEM(next)) < 0) {
      next += ELEMSIZE;
      for (n = len - 2; n; --n, next += ELEMSIZE) {
         if (sort->compare(sort->cmpstate, ELEM(next + ELEMSIZE), ELEM(next)) < 0)
            continue;
         break;
      }

      reverse_elements(sort, a, next);

   } else {
      for (n = len - 2; n; --n) {
         next += ELEMSIZE;
         if (sort->compare(sort->cmpstate, ELEM(next + ELEMSIZE), ELEM(next)) < 0)
            break;
      }
   }

   return len - n;
}

// group: sort

#if (mergesort_IMPL_TYPE == mergesort_TYPE_POINTER)
int sortptr_mergesort(mergesort_t * sort, size_t len, void * a[len], sort_compare_f cmp, void * cmpstate)

#elif (mergesort_IMPL_TYPE == mergesort_TYPE_LONG)
static int sortlong_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, uint8_t * a/*[len*elemsize]*/, sort_compare_f cmp, void * cmpstate)

#elif (mergesort_IMPL_TYPE == mergesort_TYPE_BYTES)
static int sortbytes_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, uint8_t * a/*[len*elemsize]*/, sort_compare_f cmp, void * cmpstate)

#endif
{
   int err;

   if (len < 2) return 0;

   #if (mergesort_IMPL_TYPE == mergesort_TYPE_POINTER)
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
   uint8_t   minlen  = compute_minslicelen(len);
   uint8_t * next    = (uint8_t*) a;
   size_t    nextlen = len;

   do {
      /* get size of next presorted sub array */
      size_t slice_len = count_presorted(sort, nextlen, next);

      /* extend size to min(minlen, nextlen). */
      if (slice_len < minlen) {
         const uint8_t extlen = (uint8_t) ( nextlen <= minlen
                              ? nextlen : (unsigned)minlen);
         err = insertsort(sort, (uint8_t)slice_len, extlen, next);
         if (err) goto ONABORT;
         slice_len = extlen;
      }

      /* push presorted slice onto stack of unmerged slices */
      if (sort->stacksize == lengthof(sort->stack)) {
         // stack size chosen too small for size_t type
         err = merge_topofstack(sort, false);
         if (err) goto ONABORT;
      }
      sort->stack[sort->stacksize].base = next;
      sort->stack[sort->stacksize].len  = slice_len;
      ++sort->stacksize;

      /* merge sorted slices until stack invariant is established */
      err = establish_stack_invariant(sort);
      if (err) goto ONABORT;

      /* advance */
      next    += slice_len * ELEMSIZE;
      nextlen -= slice_len;
   } while (nextlen);

   err = merge_all(sort);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}
