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

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * Define compare function for objects stored on the heap.
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
 * Memory Alignment:
 * The implementation copies elements long by long if possible.
 * If your <elemsize> is set to a multiple of sizeof(long)
 * long copy is chosen else byte copy.
 * Make sure your element are aligned to a long boundary to make
 * this work. x86 does not generate an exception if elements are
 * not alignment other processors will do.
 *
 * In case elemsize is a multiple of sizeof(long):
 * 1. The array parameter in <init_heap> should be aligned.
 * 2. The elem parameter in <insert_heap> and <remove_heap> should be aligned.
 */
struct heap_t {
   // group: private fields
   /* variable: cmp
    * The function pointer pointing to the comparison function (see <heap_compare_f>). */
   heap_compare_f cmp;
   /* variable: cmpstate
    * Additional state given as first parameter to the comparison function. */
   void    * cmpstate;
   /* variable: elemsize
    * The size of stored elements in bytes. */
   uint8_t   elemsize;
   /* variable: array
    * The start address of the heap memory (lowest address).
    * The array is static and given as parameter in one of the init
    * functions. So do not free it as long as heap is in use. */
   uint8_t * array;
   /* variable: nrofelem
    * The number of elements stored on the heap memory. */
   size_t    nrofelem;
   /* variable: maxnrofelem
    * The maximum number of elements which could be stored on the heap. */
   size_t    maxnrofelem;
};

// group: lifetime

/* define: heap_FREE
 * Static initializer. */
#define heap_FREE \
         { 0, 0, 0, 0, 0, 0 }

/* function: free_heap
 * Does nothing at the moment (except for setting maxnrofelem to 0). */
int free_heap(heap_t * heap);

/* function: init_heap
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
 *
 * This is much faster than calling <init_heap> with nrofelem=0 and <insert_heap>.
 * Calling <insert_heap> nrofelem times is of time complexity O(nrofelem log nrofelem). */
int init_heap(/*out*/heap_t * heap, uint8_t elemsize, size_t nrofelem, size_t maxnrofelem, void * array/*[maxnrofelem*elemsize]*/, heap_compare_f cmp, void * cmpstate);

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
 * It states that every parent with index i has a higher
 * priority than its left (2*i+1) and right (2*i+2) child.
 * Root node has index 0 and the last node index <nrofelem_heap>-1. */
int invariant_heap(const heap_t * heap);

// group: foreach-support

/* function: foreach_heap
 * Macro which allows iterating over the whole heap.
 * The first element is always the largest. Following
 * elements are not iterated in any specific order.
 *
 * Changing Heap:
 * It is *not allowed* to change the heap during iteration.
 *
 * Parameter:
 * heap    - Pointer to heap which is iterated over.
 *           This parameter is evaluated more than once !
 *           So something like: heap_t*x=&heap_array[0]; foreach_heap(x++, elem)
 *           does not work. Use foreach_heap(x, elem)...;++x instead.
 * loopvar - is the name of the loop variable which has type void *.
 *           It points to the next element with size <elemsize_heap>.
 * */
void foreach_heap(const heap_t * heap, IDNAME loopvar);

// group: update

/* function: insert_heap
 * Insert elem of size elemsize into the heap.
 * elemsize bytes starting at elem are copied into an internal array.
 * The size of the element (elemsize) was set during the initialization
 * of the heap (see <init_heap>).
 *
 * The value returned by <nrofelem_heap> is incremented in case of success.
 * Returns ENOMEM in case <nrofelem_heap> is equal to <maxnrofelem_heap> and this
 * error is not logged into the error log. */
int insert_heap(heap_t * heap, const void * elem/*[elemsize]*/);

/* function: remove_heap
 * Remove maximum element of heap and copy it into elem.
 * elemsize bytes are copied from the heap to the memory starting
 * at elem. The size of the element (elemsize) was set during the initialization
 * of the heap (see <init_heap>).
 *
 * Equal elements could be returned in any order.
 *
 * The value returned by <nrofelem_heap> is decremented in case of success.
 * Returns ENODATA in case <nrofelem_heap> is already 0 and this
 * error is not logged into the error log. */
int remove_heap(heap_t * heap, /*out*/void * elem/*[elemsize]*/);


// section: inline implementation

// group: heap_t

/* define: elemsize_heap
 * Implements <heap_t.elemsize_heap>. */
#define elemsize_heap(heap) \
         ((heap)->elemsize)

/* define: foreach_heap
 * Implements <heap_t.foreach_heap>. */
#define foreach_heap(heap, loopvar) \
         for (void * loopvar = (heap)->array,                                                \
                   * loopvar ## _end = (heap)->array + (heap)->nrofelem * (heap)->elemsize;  \
              loopvar != loopvar ## _end; loopvar = (uint8_t*)loopvar + (heap)->elemsize)

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
