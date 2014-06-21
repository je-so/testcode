/* title: Heap

   Der Heap verwaltet eine Menge von Elementen,
   teil-sortiert nach einem Schlüssel.

   Er bildet die Grundlage für Priority-Queues,
   bei denen Elemente der Priorität nach verwaltet werden;
   <queue_t> verwaltet Elemente der Einfügereihenfolge nach.

   Dieser spezielle Heap erwartet die Elemente abgelegt in einem Array.
   Es wäre auch möglich, Elemente in Form eines Baumes (heaptree_t?)
   abzulegen.

   Der Heap erlaubt das Löschen des kleinsten (größten) Elements
   bzw. das Einfügen eine Elementes in O(log n) und das Erstellen eines
   Heapes in O(n) für n unsortierte Elemente.

   Dieser Heap verwaltet einen fixen Speicherbereich (Array) und damit eine
   fixe Anzahl an Elementen.

   Verwendung:
   - Für Prioritätswarteschlangen, bei denen Elemente der Wichtigkeit nach
     verarbeitet werden sollen.
   - Für die Ermittlung der k kleinsten bzw. k größten Elemente einer Menge
     in O(n + k log n). O(n) um einen Heap aus einem Array aufzubauen und
     k mal O(log n) Zeit, um k Elemente herauszunehmen.


   Siehe auch http://de.wikipedia.org/wiki/Heap_%28Datenstruktur%29

   For a description of a heap see http://en.wikipedia.org/wiki/Heap_%28data_structure%29

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
#ifndef CKERN_DS_INMEM_HEAP_HEADER
#define CKERN_DS_INMEM_HEAP_HEADER

/* typedef: struct heap_t
 * Export <heap_t> into global namespace. */
typedef struct heap_t heap_t;

/* typedef: heap_compare_f
 * Define compare function for objects stored in the heap.
 * The first parameter cmpstate is a variable which
 * points to additional shared compare state beyond
 * the two objects left and right.
 *
 * Returns:
 * The returned values -1 or +1 stand for any negative
 * or positive number. So an implementation is free to choose
 * values larger than +1 or smaller than -1.
 * -1 - left  < right
 *  0 - left == right
 * +1 - left  > right
 */
typedef int (*heap_compare_f) (void * cmpstate, const void * left, const void * right);


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_heap
 * Test <heap_t> functionality. */
int unittest_ds_inmem_heap(void);
#endif


/* struct: heap_t
 * Manages an array of elements of size elemsize
 * with the lowest or highest priority element stored at index 0.
 *
 */
struct heap_t {
   heap_compare_f cmp;
   void    * cmpstate;
   uint8_t   elemsize;
   /* variable: isminheap
    * If true array[0..elemsize-1] contains the lowest
    * priority element else the highest. */
   uint8_t   isminheap;
   uint8_t * array;
   size_t    nrofelem;
   size_t    maxnrofelem;
};

// group: lifetime

/* define: heap_FREE
 * Static initializer. */
#define heap_FREE \
         { 0, 0, 0, 0, 0, 0, 0 }

/* function: initmin_heap
 * Builds heap stucture in existing array.
 * The provided must have a size of elemsize * maxnrofelem bytes.
 * Every element occupies elemsize bytes and nrofelem elements are stored
 * continuously beginning from index 0 in the array.
 * The parameter cmp is the comparison function to compare elements according their priority.
 * cmpstate is an additional value passed through as first parameter to the compare function.
 * After return ((uint8_t*)array)[0..elemsize-1] contains the element with the lowest priority.
 *
 * The content of the array is not copied so it is not allowed to free array as long as heap is not freed.
 *
 * Time O(n):
 * The building of a heap structure from an existing set of elements costs O(nrofelem).
 * This is much faster than calling <initmin_heap> or <initmax_heap> with nrofelem=0 and
 * calling <insert_heap> nrofelem times which is of time complexity O(nrofelem log nrofelem).
 */
int initmin_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate);

/* function: initmax_heap
 * Same as <initmin_heap> except ((uint8_t*)array)[0..elemsize-1] contains element with the highest priority. */
int initmax_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate);

/* function: free_heap
 * Does nothing at the moment (except for setting maxnrofelem to 0). */
int free_heap(heap_t * heap);

// group: query

/* function: elemsize_heap
 * Returns the element size in bytes of the stored elements. */
uint8_t elemsize_heap(const heap_t * heap);

/* function: maxnrofelem_heap
 * Returns the maximum number of elements the heap can store
 * in the fixed size array. */
size_t maxnrofelem_heap(const heap_t * heap);

/* function: nrofelem_heap
 * Returns the number of elements currently stored on the heap. */
size_t nrofelem_heap(const heap_t * heap);

/* function: invariant_heap
 * Checks the heap condition.
 * It states that every parent i has a lower (bigger)
 * priority than its childs (2*i+1) and (2*i+2).
 * Root parent node has index 0. */
int invariant_heap(const heap_t * heap);

// group: update

// TODO:
int insert_heap(heap_t * heap, const void * new_elem/*[elemsize]*/);

// TODO:
int remove_heap(heap_t * heap, /*out*/void * removed_elem/*[elemsize]*/);


// section: inline implementation

/* define: elemsize_heap
 * Implements <heap_t.elemsize_heap>. */
#define elemsize_heap(heap) \
         ((heap)->elemsize)

/* define: free_heap
 * Implements <heap_t.free_heap>. */
#define free_heap(heap) \
         ((heap)->maxnrofelem = 0, 0)

/* define: maxnrofelem_heap
 * Implements <heap_t.maxnrofelem_heap>. */
#define maxnrofelem_heap(heap) \
         ((heap)->maxnrofelem)

/* define: nrofelem_heap
 * Implements <heap_t.nrofelem_heap>. */
#define nrofelem_heap(heap) \
         ((heap)->nrofelem)


#endif
