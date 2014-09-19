/* title: MergeSort

   Offers implementation of a stable merge sort algorithm
   with worst case complexity O(n log n).

   When to choose mergesort?:

   If you need a stable algorithm, in cases where you sort
   by one criterion first and after that by another.

   If you know that your data set contains large blocks
   of presorted data, this implementation is very fast,
   meaning O(n) complexity in the best case.

   If additional memory of up to (nr_of_elements * element_size / 2)
   bytes is of no problem.

   But the implementation is much more complex than for example quicksort.
   So if you know your data is random, and do not need a stable algorithm,
   and want to conserve memory, use quicksort.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/ds/sort/mergesort.h
    Header file <MergeSort>.

   file: C-kern/ds/sort/mergesort.c
    Implementation file <MergeSort impl>.
*/
#ifndef CKERN_DS_SORT_MERGESORT_HEADER
#define CKERN_DS_SORT_MERGESORT_HEADER

/* typedef: sort_compare_f
 * Define compare function for arbitrary objects.
 * The first parameter cmpstate is a variable which
 * points to additional shared compare state beyond
 * the two objects left and right.
 *
 * Returns:
 * The returned values -1 or +1 stand for any negative
 * or positive number. So an implementation is free to choose
 * values larger than +1 or smaller than -1.
 * -1 - left  < right
 * 0  - left == right
 * +1 - left  > right
 */
typedef int (* sort_compare_f) (void * cmpstate, const void * left, const void * right);

/* typedef: struct mergesort_t
 * Export <mergesort_t> into global namespace. */
typedef struct mergesort_t mergesort_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_sort_mergesort
 * Test <mergesort_t> functionality. */
int unittest_ds_sort_mergesort(void);
#endif


/* struct: mergesort_sortedslice_t
 * Describes a part of an array which contains sorted data.
 * Len gives the number of elements stored in the sub-array.
 * Base is the start address of the sub-array (lower address). */
struct mergesort_sortedslice_t {
    void * base;
    size_t len;
};

/* struct: mergesort_t
 * Implementation of a stable mergesort.
 * Sorts the elements in ascending order.
 *
 * Complexity O(n log n).
 *
 * The implementation is an adaption of TimSort.
 * See http://bugs.python.org/file4451/timsort.txt for a description
 * of how the algorithm works.
 *
 * The implementation can use up to (nr_of_elements * elemsize / 2)
 * bytes of additional memory.
 *
 * Description:
 * First the whole array is scanned for presorted slices of the array.
 * Slices with descending order are reversed.
 *
 * If a slice contains less than compute_minslicelen() presorted elements
 * additional elements are added until compute_minslicelen() number of elements
 * are reached. The extended slice is sorted with a stable insertsort.
 *
 * The reference to already scanned and sorted slices are pushed on a stack.
 * The length of the pushed slices must satisfy an invariant.
 * This invariant states that the length of slices forms a Finonacci sequence
 * that is length(n-2) > length(n-1) + length(n) where n is the top of stack.
 * This invariant ensures balanced merges. If the two topmost slices are merged
 * the length of the following slice has at least the size of the two other.
 * Balanced merges are responsible for the log(n) factor in the runtime.
 *
 * The value returned from compute_minslicelen() is chosen between 32 and 64
 * so that the length of the whole array divided by minlen is close to
 * a power of two. This ensures also balanced merges.
 *
 * The stack invariant is established by merging adjacent slices together
 * during the whole array scan.
 *
 * Once the whole array has been scanned all slices on the stack are merged
 * from top to bottom.
 *
 */
struct mergesort_t {
   // group: private fields
   /* variable: compare
    * The comparison function to compare two elements. See <sort_compare_f>. */
   sort_compare_f compare;
   /* variable: cmpstate
    * Additional state given as first parameter to the comparison function <compare>. See <sort_compare_f>. */
   void         * cmpstate;
   /* variable: elemsize
    * Size of an element stored in the array to be sorted. */
   uint8_t        elemsize;

   /* variable: temp
    * Temporary storage to help with merges.
    * It contains room for <tempsize> / <elemsize> entries. */
   uint8_t * temp;
   /* variable: tempsize
    * The size in bytes of the allocated temporary storage. */
   size_t    tempsize;

   /* variable: stack
    * A stack of stacksize sorted slices (see <mergesort_sortedslice_t>) waiting to be merged.
    *
    * Invariants:
    * For all 0 <= i && i < stacksize-1:
    * > stack[i].base + elemsize * stack[i].len == stack[i+1].base
    * > && stack[0].base == a
    * Value a is the array given int eh call to <sortptr_mergesort> or <sortblob_mergesort>.
    *
    * The slice length grow at least as fast as the Fibonacci sequence;
    * for all 0 <= i && i < stacksize-1:
    *
    * For all 0 <= i && i < stacksize-2:
    * > stack[i].len > stack[i+1].len + stack[i+2].len
    * > && stack[stacksize-2].len > stack[stacksize-1].len
    *
    */
   struct mergesort_sortedslice_t stack[85/*big enough to sort arrays of size (uint64_t)-1*/];
   /* variable: stacksize
    * The number of entries of <stack>. */
   size_t    stacksize;

   /* variable: tempmem
    * Temporary memory block. Large enough to hold 256 pointers.
    * Must always be large enough to hold at least one element (255 bytes max).
    * Used in internal insertsort implementation and during merge operations of small arrays. */
   uint8_t   tempmem[256 * sizeof(void*)];
};

// group: lifetime

/* define: mergesort_FREE
 * Static initializer. */
#define mergesort_FREE \
         { 0, 0, 0, 0, 0, { { 0, 0 } }, 0, { 0 } }

/* function: init_mergesort
 * Initializes sort so that calling <sortptr_mergesort> or <sortblob_mergesort> will work. */
void init_mergesort(/*out*/mergesort_t * sort);

/* function: free_mergesort
 * Frees any allocated temporary storage.
 * After return of <sortptr_mergesort> or <sortblob_mergesort>
 * the used temporary memory is not freed. It can be reused for
 * another call to sort. Call this function to free this memory. */
int free_mergesort(mergesort_t * sort);

// group: sort

/* function: sortptr_mergesort
 * Sort array a which contains len pointers.
 * The sorting is done in ascending order.
 * Returns 0 on success, ENOMEM (or another value) on error.
 * The array contains pointer to objects and the comparison function cmp
 * is called with these pointer values.
 *
 * Undo Violation:
 * In case of error, the incomplete permutation of elements
 * in array a will not be undone! */
int sortptr_mergesort(mergesort_t * sort, size_t len, void * a[len], sort_compare_f cmp, void * cmpstate);

/* function: sortblob_mergesort
 * Sorts the array a which contains len elements of elemsize bytes each.
 * The sorting is done in ascending order.
 * Returns 0 on success, ENOMEM (or another value) on error.
 * The comparison function cmp is called with the address of the elements
 * located in the array. Mergesort uses a temporary array during merging
 * of sorted subsets therefore the comparison function could also
 * be called with an address pointing into the temporary array.
 *
 * Undo Violation:
 * In case of error, the incomplete permutation of elements
 * in array a will not be undone! */
int sortblob_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, void * a/*uint8_t[len*elemsize]*/, sort_compare_f cmp, void * cmpstate);


#endif
