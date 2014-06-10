/* title: MergeSort

   Offers implementation of a stable merge sort algorithm
   with complexity O(n log n).

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

   file: C-kern/api/sort/mergesort.h
    Header file <MergeSort>.

   file: C-kern/sort/mergesort.c
    Implementation file <MergeSort impl>.
*/
#ifndef CKERN_SORT_MERGESORT_HEADER
#define CKERN_SORT_MERGESORT_HEADER

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
/* function: unittest_sort_mergesort
 * Test <mergesort_t> functionality. */
int unittest_sort_mergesort(void);
#endif

// TODO: rename into MIN_FAST_SEARCH
#define MIN_GALLOP 7


/* struct: mergesort_subarray_t
 * Describes a sub-array.
 * Len gives the number of elements stored in the array.
 * Base is the start address of the array (lower address). */
struct mergesort_subarray_t {
    void * base;
    size_t len;
};

/* struct: mergesort_t
 * Implementation of a stable mergesort.
 *
 * Complexity O(n log n).
 *
 * The implementation is an adaption of TimSort.
 * See http://bugs.python.org/file4451/timsort.txt for a description
 * of how the algorithm works.
 *
 * The implementation can use up to (nr_of_elements/2*element_size)
 * bytes of additional memory.
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
   void    * temp;
   /* variable: tempsize
    * The size in bytes of the allocated temporary storage. */
   size_t    tempsize;

   /* variable: pending
    * TODO: rename and describe
    * A stack of stacksize pending runs yet to be merged.  Run #i starts at
    * address base[i] and extends for len[i] elements.  It's always
    * true (so long as the indices are in bounds) that
    *
    *     pending[i].base + pending[i].len == pending[i+1].base
    *
    * so we could cut the storage for this, but it's a minor amount,
    * and keeping all the info explicit simplifies the code.
    */
   struct mergesort_subarray_t pending[85/*big enough to sort arrays of size (uint64_t)-1*/];
   /* variable: stacksize
    * The number of entries of stack <pending>. */
   size_t stacksize;

   /* variable: tempmem
    * Temporary memory block. LArge enough to hold 256 pointers. */
   uint8_t  tempmem[256 * sizeof(void*)];
};

// group: lifetime

/* define: mergesort_FREE
 * Static initializer. */
#define mergesort_FREE \
         { 0, 0, 0, 0, 0, { { 0, 0 } }, 0, { 0 } }

/* function: init_mergesort
 * TODO: Describe Initializes object. */
int init_mergesort(/*out*/mergesort_t * sort);

/* function: free_mergesort
 * TODO: Describe Frees all associated resources. */
int free_mergesort(mergesort_t * sort);

// group: sort

/* function: sortptr_mergesort
 * Sorts the array a which contains len pointers.
 * Returns 0 on success, != 0 on error.
 * The array contains pointer to objects and the comparison function cmp
 * is called with these pointers values.
 *
 * Undo Violation:
 * In case of error, the incomplete permutation of elements
 * in array a will not be undone! */
int sortptr_mergesort(mergesort_t * sort, size_t len, void * a[len], sort_compare_f cmp, void * cmpstate);

/* function: sortblob_mergesort
 * Sorts the array a which contains len elements with size elemsize each.
 * Returns 0 on success, != 0 on error.
 * The comparison function cmp is called with the address of the elements
 * located in the array. Mergesort uses a temporary array during merging
 * of sub-arrays of elements therefor the given address could also point
 * to a location in the temporary array.
 *
 * Undo Violation:
 * In case of error, the incomplete permutation of elements
 * in array a will not be undone! */
int sortblob_mergesort(mergesort_t * sort, uint8_t elemsize, size_t len, void * a/*uint8_t[len]*/, sort_compare_f cmp, void * cmpstate);


// section: inline implementation

// TODO: remove !!
/* define: init_mergesort
 * Implements <mergesort_t.init_mergesort>. */


#endif
