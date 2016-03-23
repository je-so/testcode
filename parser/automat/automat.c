/* title: FiniteStateMachine impl

   Implements <FiniteStateMachine>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/

// Compile with: gcc -oatm -DKONFIG_UNITTEST -std=gnu99 automat.c 
// Run with: ./atm

#include "config.h"
#include "automat.h"
#include "foreach.h"
#include "test_errortimer.h"

typedef uint32_t char32_t;

// === private types
struct memory_page_t;
struct range_transition_t;
union  transition_t;
struct state_t;
struct multistate_node_t;
struct multistate_t;
struct range_t;            // TODO:
struct rangeindex_node_t;  // TODO:
struct rangeindex_t;       // TODO:

/* enums: state_e
 * Beschreibt Typ eines <state_t>.
 *
 * state_EMPTY - Markiert einen neuen <state_t>, der leere Übergänge (ohne assozierten Character)
 *               beinhaltet, bis zu maximal 255 Stück. Weitere mit Charactern assoziierte Übergange
 *               können mittels state_RANGE_CONTINUE im nächsten <state_t> kodiert werden.
 * state_RANGE - Markiert einen neuen <state_t>, der mit Charactern assozierte Übergange verbunden ist.
 *               Bis zu maximal 255 Transitionen können gepsiehcert werden. Weitere mit Charactern assoziierte
 *               Übergange können mittels state_RANGE_CONTINUE im nächsten <state_t> kodiert werden.
 * state_RANGE_CONTINUE - Dieser <state_t> ist eine Erweiterung des vorherigen.
 *                        Ein state kann nur bis maximal 255 Transitionen unterbringen.
 *                        Mehr als 255 (Range) Transitionen werden in mehrere States
 *                        verteilt aufbewahrt.
 * */
typedef enum {
   state_EMPTY,
   state_RANGE,
   state_RANGE_CONTINUE
} state_e;

// forward
#ifdef KONFIG_UNITTEST
static test_errortimer_t s_automat_errtimer;
#endif


// struct: memory_page_t

typedef struct memory_page_t {
   /* variable: next
    * Verlinkt die Seiten zu einer Liste. */
   slist_node_t next;
   /* variable: data
    * Beginn der gespeicherten Daten. Diese sind auf den long Datentyp ausgerichtet. */
   long         data[1];
} memory_page_t;

// group: static variables

static size_t s_memory_page_sizeallocated = 0;

// group: constants

/* define: memory_page_SIZE
 * Die Größe in Bytes einer <memory_page_t>. */
#define memory_page_SIZE (256*1024)

// group: helper-types

slist_IMPLEMENT(_pagelist, memory_page_t, next.next)

// group: query

static inline size_t SIZEALLOCATED_PAGECACHE(void)
{
   return s_memory_page_sizeallocated;
}

// group: lifetime

/* function: new_memorypage
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von (*page)->data und (*page)->next sind undefiniert. */
static int new_memorypage(/*out*/memory_page_t ** page)
{
   int err;
   void * addr;

   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      addr = malloc(memory_page_SIZE);
      err = !addr ? ENOMEM : 0;
   }

   if (err) goto ONERR;

   s_memory_page_sizeallocated += memory_page_SIZE; 

   // set out
   *page = (memory_page_t*) addr;

   return 0;
ONERR:
   return err;
}

/* function: free_automatmman
 * Gibt eine Speicherseite frei. */
static int delete_memorypage(memory_page_t * page)
{
   int err = 0;
   free(page);
   s_memory_page_sizeallocated -= memory_page_SIZE; 
   (void) PROCESS_testerrortimer(&s_automat_errtimer, &err);
   return err;
}


// struct: automat_mman_t
struct automat_mman_t;

// group: lifetime

int free_automatmman(automat_mman_t * mman)
{
   int err = 0;
   int err2;

   // append mman->pagecache to mman->pagelist
   // and clear mman->pagecache
   insertlastPlist_pagelist(&mman->pagelist, &mman->pagecache);

   // free allocated pages
   foreach (_pagelist, page, &mman->pagelist) {
      err2 = delete_memorypage(page);
      if (err2) err = err2;
   }
   mman->pagelist = (slist_t) slist_INIT;
   mman->refcount = 0;
   mman->freemem  = 0;
   mman->freesize = 0;
   mman->allocated = 0;

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: query

size_t sizeallocated_automatmman(const automat_mman_t * mman)
{
   return mman->allocated;
}

// group: usage

/* function: incruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom aufrufenden <automat_t> benutzt. */
static inline void incruse_automatmman(automat_mman_t * mman)
{
   ++ mman->refcount;
}

/* function: decruse_automatmman
 * Markiert die Ressourcen, verwaltet von mman, als vom aufrufenden <automat_t> unbenutzt. */
static inline void decruse_automatmman(automat_mman_t * mman)
{
   assert(mman->refcount > 0);
   -- mman->refcount;

   if (0 == mman->refcount) {
      insertlastPlist_pagelist(&mman->pagecache, &mman->pagelist);
      mman->freemem  = 0;
      mman->freesize = 0;
      mman->allocated = 0;
   }
}

// group: resource helper

/* function: allocpage_automatmman
 * Weist page eine Speicherseite von <buffer_page_SIZE> Bytes zu.
 * Der Speicherinhalt von **page ist undefiniert. */
static inline int getfreepage_automatmman(automat_mman_t * mman, /*out*/memory_page_t ** free_page)
{
   int err;

   if (! isempty_slist(&mman->pagecache)) {
      err = removefirst_pagelist(&mman->pagecache, free_page);
   } else {
      err = new_memorypage(free_page);
   }
   if (err) goto ONERR;

   insertlast_pagelist(&mman->pagelist, *free_page);

   return 0;
ONERR:
   return err;
}

// group: allocate resources

static int allocmem_automatmman(automat_mman_t * mman, uint16_t mem_size, /*out*/void ** mem_addr)
{
   int err;
   memory_page_t * free_page;

   if (mman->freesize < mem_size) {
      #define FREE_PAGE_SIZE (memory_page_SIZE - offsetof(memory_page_t, data))
      if (  FREE_PAGE_SIZE < UINT16_MAX && mem_size > FREE_PAGE_SIZE) {
         // free page does not meet demand of mem_size bytes which could as big as UINT16_MAX
         err = ENOMEM;
         goto ONERR;
      }
      err = getfreepage_automatmman(mman, &free_page);
      if (err) goto ONERR;
      mman->freemem  = memory_page_SIZE + (uint8_t*) free_page;
      mman->freesize = FREE_PAGE_SIZE;
      #undef FREE_PAGE_SIZE
   }

   const size_t freesize = mman->freesize;
   mman->freesize = freesize - mem_size;
   mman->allocated += mem_size;

   // set out
   *mem_addr = mman->freemem - freesize;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


/* struct: range_transition_t
 * Beschreibt Übergang von einem <state_t> zum nächsten.
 * Nur falls der Eingabecharacter zwischem from und to, beide Werte
 * mit eingeschlossen, liegt, darf der Übergang vollzogen werden. */
typedef struct range_transition_t {
   struct state_t* state;  // goto state (only if char in [from .. to])
   char32_t from;          // inclusive
   char32_t to;            // inclusive
} range_transition_t;

/* typedef: empty_transition_t
 * Beschreibt unbedingten Übergang von einem <state_t> zum nächsten.
 * Der Übergang wird immer vollzogen ohne einen Buchstaben zu konsumieren. */
typedef struct state_t* empty_transition_t;

typedef union transition_t {
   empty_transition_t empty[3];  // case state_t.type == state_EMPTY
   range_transition_t range[3];  // case state_t.type == state_RANGE
} transition_t;

/* struct: state_t
 * Beschreibt einen Zustand des Automaten. */
typedef struct state_t {
   uint8_t  type;       // value from state_e
   uint8_t  nrtrans;
   slist_node_t* next;
   transition_t  trans;
} state_t;

// group: constants

#define state_SIZE                     (offsetof(struct state_t, trans))
#define state_SIZE_EMPTYTRANS(nrtrans) ((nrtrans) * sizeof(empty_transition_t))
#define state_SIZE_RANGETRANS(nrtrans) ((nrtrans) * sizeof(range_transition_t))

// group: type support

/* define: slist_IMPLEMENT__statelist
 * Implementiert <slist_t> für Objekte vom Typ <state_t>. */
slist_IMPLEMENT(_statelist, state_t, next)

// group: lifetime

/* function: initempty_state
 * Initialisiert state mit einem "leerem" Übergang zu target <state_t>.
 * Ein leerer Übergang wird immer verwendet und liest bzw. verbraucht keinen Buchstaben. */
static void initempty_state(/*out*/state_t * state, state_t * target)
{
   state->type = state_EMPTY;
   state->nrtrans = 1;
   state->trans.empty[0] = target;
}

/* function: initempty2_state
 * Initialisiert state mit zwei "leeren" Übergangem zu target und target2.
 * Ein leerer Übergang wird immer verwendet und liest bzw. verbraucht keinen Buchstaben. */
static void initempty2_state(/*out*/state_t * state, state_t * target, state_t * target2)
{
   state->type = state_EMPTY;
   state->nrtrans = 2;
   state->trans.empty[0] = target;
   state->trans.empty[1] = target2;
}

/* function: initrange_state
 * Initialisiert state mit einem einen Character verbrauchenden Übergang zu target <state_t>.
 * Ein <state_RANGE> Übergang testet den nächsten zu lesenden Character, ob er innerhalb
 * der nrmatch Bereiche [match_from..match_to] liegt. Wenn ja, wird der Character verbraucht
 * und der Übergang nach target <state_t> ausgeführt. */
static void initrange_state(/*out*/state_t * state, state_t * target, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   state->type = state_RANGE;
   state->nrtrans = nrmatch;
   for (unsigned i = 0; i < nrmatch; ++i) {
      state->trans.range[i].state = target;
      state->trans.range[i].from = match_from[i];
      state->trans.range[i].to = match_to[i];
   }
}

/* function: initcontinue_state
 * Initialisiert state mit einem einen Character verbrauchenden Übergang zu target <state_t>.
 * Ein <state_RANGE_CONTINUE> ist eine Erweiterung des aktuellen Zustandes um
 * weitere nrmatch Übergange. Diese testem den nächsten zu lesenden Character, ob er innerhalb
 * der nrmatch Bereiche [match_from..match_to] liegt. Wenn ja, wird der Character verbraucht
 * und der Übergang nach target <state_t> ausgeführt. */
static void initcontinue_state(/*out*/state_t * state, state_t * target, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   state->type = state_RANGE_CONTINUE;
   state->nrtrans = nrmatch;
   for (unsigned i = 0; i < nrmatch; ++i) {
      state->trans.range[i].state = target;
      state->trans.range[i].from = match_from[i];
      state->trans.range[i].to = match_to[i];
   }
}


/* struct: multistate_node_t
 * Knoten, der Teil eines B-Tree ist. */
typedef struct multistate_node_t {
   uint8_t level;
   uint8_t size;
   union {
      struct { // level > 0
         state_t * key[3]; // key[i] == child[i+1]->child[/*(level > 0)*/0]->...->state[/*(level == 0)*/0]
         struct multistate_node_t * child[4];
      };
      struct { // level == 0
         struct multistate_node_t * next;
         state_t * state[6];
      };
   };
} multistate_node_t;


typedef struct multistate_stack_entry_t {
   multistate_node_t * parent;
   unsigned            ichild;
} multistate_stack_entry_t;

typedef struct multistate_stack_t {
   size_t                   depth;
   multistate_stack_entry_t entry[bitsof(size_t)];
} multistate_stack_t;

// group: lifetime

static inline void init_multistatestack(multistate_stack_t * stack)
{
   stack->depth = 0;
}

// group: update

static inline void push_multistatestack(multistate_stack_t * stack, multistate_node_t * node, unsigned ichild)
{
   stack->entry[stack->depth] = (multistate_stack_entry_t) { node, ichild };
   ++ stack->depth;
}


/* struct: multistate_t
 * Verwaltet mehrere Pointer auf <state_t>.
 * Die gespeicherten Werte sind sortiert abgelegt, so dass
 * ein Vergleich zweier <multistate_t> auf Gleichheit möglich ist.*/
typedef struct multistate_t {
   size_t size;
   void * root;
} multistate_t;

// group: constants

/* define: multistate_NROFSTATE
 * Nr of states array <multistate_node_t->state> could store. */
#define multistate_NROFSTATE \
         (lengthof(((multistate_node_t*)0)->state))

/* define: multistate_NROFCHILD
 * Nr of pointer array <multistate_node_t->child> could store. */
#define multistate_NROFCHILD \
         (lengthof(((multistate_node_t*)0)->child))

/* define: multistate_NROFKEY
 * Nr of pointer array <multistate_node_t->key> could store. */
#define multistate_NROFKEY \
         (lengthof(((multistate_node_t*)0)->key))


// group: lifetime

#define multistate_INIT \
         { 0, 0 }

// group: query

// TODO:

// group: update

// returns:
// ENOMEM - Data structure may my be corrupt due to split operation (do not use it afterwards)
static int add_multistate(multistate_t * mst, /*in*/state_t * state, automat_mman_t * mman)
{
   int err;
   void * node;

   // === 3 cases ===
   // 1: 2 <= mst->size ==> insert into leaf (general case)
   // 2: 0 == mst->size ==> store state into root pointer (special case)
   // 3: 1 == mst->size ==> allocate leaf and store root + state into leaf (special case)

   if (1 < mst->size) {
      // === case 1 ===
      // search from mst->root node to leaf and store search path in stack
      multistate_stack_t   stack;
      init_multistatestack(&stack);
      node = mst->root;
      if (((multistate_node_t*)node)->level >= (int) lengthof(stack.entry)) {
         return EINVARIANT;
      }
      for (unsigned level = ((multistate_node_t*)node)->level; (level--) > 0; ) {
         if (((multistate_node_t*)node)->size > multistate_NROFCHILD
            || ((multistate_node_t*)node)->size < 2) {
            return EINVARIANT;
         }
         unsigned high = (((multistate_node_t*)node)->size -1u); // size >= 2 ==> high >= 1
         unsigned low  = 0; // low < high cause high >= 1
         // ((multistate_node_t*)node)->child[0..high] are valid
         // ((multistate_node_t*)node)->key[0..high-1] are valid
         for (unsigned mid = high / 2u; /*low < high*/; mid = (high + low) / 2u) {
            // search state in node->key
            if (((multistate_node_t*)node)->key[mid] <= state) {
               low = mid + 1; // low <= high
            } else {
               high = mid;
            }
            if (low == high) break;
         }
         push_multistatestack(&stack, node, low);
         node = ((multistate_node_t*)node)->child[low];
         if (((multistate_node_t*)node)->level != level) {
            return EINVARIANT;
         }
      }
      if (((multistate_node_t*)node)->size > multistate_NROFSTATE
         || ((multistate_node_t*)node)->size < 2) {
         return EINVARIANT;
      }
      // find state in leaf node. The index »low« is the offset where «state» has to be inserted !
      unsigned high = ((multistate_node_t*)node)->size;
      unsigned low  = 0; // 0 <= low <= ((multistate_node_t*)node)->size
      for (unsigned mid = high / 2u; /*low < high*/; mid = (high + low) / 2u) {
         // search state in node->key
         if (((multistate_node_t*)node)->state[mid] < state) {
            low = mid + 1;
         } else if (((multistate_node_t*)node)->state[mid] == state) {
            return EEXIST;
         } else {
            high = mid;
         }
         if (low == high) break;
      }
      if (multistate_NROFSTATE > ((multistate_node_t*)node)->size) {
         // insert into leaf
         for (unsigned i = ((multistate_node_t*)node)->size; i > low; --i) {
            ((multistate_node_t*)node)->state[i] = ((multistate_node_t*)node)->state[i-1];
         }
         ((multistate_node_t*)node)->state[low] = state;
         ++ ((multistate_node_t*)node)->size;
      } else {
         // split leaf
         void * node2;
         const uint16_t SIZE = sizeof(multistate_node_t);
         {
            err = allocmem_automatmman(mman, SIZE, &node2);
            if (err) goto ONERR;
            const unsigned NODE2_SIZE = ((multistate_NROFSTATE+1) / 2);
            const unsigned NODE_SIZE  = ((multistate_NROFSTATE+1) - NODE2_SIZE);
            ((multistate_node_t*)node2)->level = 0;
            ((multistate_node_t*)node2)->size  = (uint8_t) NODE2_SIZE;
            ((multistate_node_t*)node2)->next  = ((multistate_node_t*)node)->next;
            ((multistate_node_t*)node)->size   = (uint8_t) NODE_SIZE;
            ((multistate_node_t*)node)->next   = node2;
            // low <  multistate_NROFSTATE
            // ==> node->state[0], ..., state, node->state[low], ..., node->state[multistate_NROFSTATE-1]
            // low == multistate_NROFSTATE
            // ==> node->state[0], ..., node->state[multistate_NROFSTATE-1], state
            if (low < NODE_SIZE) {
               state_t ** node_state  = &((multistate_node_t*)node)->state[multistate_NROFSTATE];
               state_t ** node2_state = &((multistate_node_t*)node2)->state[NODE2_SIZE];
               for (unsigned i = NODE2_SIZE; i; --i) {
                  *(--node2_state) = *(--node_state);
               }
               for (unsigned i = NODE_SIZE-1-low; i > 0; --i, --node_state) {
                  *node_state = node_state[-1];
               }
               *node_state = state;
            } else {
               state_t ** node_state  = &((multistate_node_t*)node)->state[NODE_SIZE];
               state_t ** node2_state = &((multistate_node_t*)node2)->state[0];
               for (unsigned i = low-NODE_SIZE; i; --i, ++node_state, ++node2_state) {
                  *node2_state = *node_state;
               }
               *node2_state++ = state;
               for (unsigned i = multistate_NROFSTATE-low; i; --i, ++node_state, ++node2_state) {
                  *node2_state = *node_state;
               }
            }
         }
         // insert node2 into parent
         state_t * key2 = ((multistate_node_t*)node2)->state[0]; // TODO: use it !!!!
         while (stack.depth) {
            node = stack.entry[--stack.depth].parent;
            low  = stack.entry[stack.depth].ichild;
            if (multistate_NROFCHILD > ((multistate_node_t*)node)->size) {
               // insert into parent (node)
               for (unsigned i = ((multistate_node_t*)node)->size-1u; i > low; --i) {
                  ((multistate_node_t*)node)->key[i] = ((multistate_node_t*)node)->key[i-1];
               }
               ((multistate_node_t*)node)->key[low] = key2;
               ++ low;
               for (unsigned i = ((multistate_node_t*)node)->size; i > low; --i) {
                  ((multistate_node_t*)node)->child[i] = ((multistate_node_t*)node)->child[i-1];
               }
               ((multistate_node_t*)node)->child[low] = node2;
               ++ ((multistate_node_t*)node)->size;
               goto END_SPLIT;
            } else {
               // split node
               void * child = node2;
               state_t * child_key = key2;
               // ENOMEM ==> information in child(former node2) is lost !! (corrupt data structure)
               err = allocmem_automatmman(mman, SIZE, &node2);
               if (err) goto ONERR;
               const unsigned NODE2_SIZE = ((multistate_NROFCHILD+1) / 2);
               const unsigned NODE_SIZE  = ((multistate_NROFCHILD+1) - NODE2_SIZE);
               ((multistate_node_t*)node2)->level = ((multistate_node_t*)node)->level;
               ((multistate_node_t*)node2)->size  = (uint8_t) NODE2_SIZE;
               ((multistate_node_t*)node)->size   = (uint8_t) NODE_SIZE;
               if (low + 1 < NODE_SIZE) {
                  state_t ** node_key  = &((multistate_node_t*)node)->key[multistate_NROFKEY];
                  state_t ** node2_key = &((multistate_node_t*)node2)->key[NODE2_SIZE-1];
                  multistate_node_t ** node2_child = &((multistate_node_t*)node2)->child[NODE2_SIZE];
                  multistate_node_t ** node_child  = &((multistate_node_t*)node)->child[multistate_NROFCHILD];
                  for (unsigned i = NODE2_SIZE-1; i; --i) {
                     *(--node2_key) = *(--node_key);
                     *(--node2_child) = *(--node_child);
                  }
                  key2 = *(--node_key);
                  *(--node2_child) = *(--node_child);
                  for (unsigned i = NODE_SIZE-2-low; i; --i, --node_key, --node_child) {
                     *node_key  = node_key[-1];
                     *node_child  = node_child[-1];
                  }
                  *node_key = child_key;
                  *node_child = child;
               } else {
                  state_t ** node_key  = &((multistate_node_t*)node)->key[NODE_SIZE-1];
                  state_t ** node2_key = &((multistate_node_t*)node2)->key[0];
                  multistate_node_t ** node_child  = &((multistate_node_t*)node)->child[NODE_SIZE];
                  multistate_node_t ** node2_child = &((multistate_node_t*)node2)->child[0];
                  unsigned I = low-(NODE_SIZE-1);
                  if (I) {
                     key2 = *node_key++;
                     *node2_child++ = *node_child++;
                     for (unsigned i = I-1; i; --i, ++node_key, ++node2_key, ++node_child, ++node2_child) {
                        *node2_key = *node_key;
                        *node2_child = *node_child;
                     }
                     *node2_key++ = child_key;
                  } else {
                     // key2 = child_key; // already equal
                  }
                  *node2_child++ = child;
                  for (unsigned i = (multistate_NROFCHILD-1)-low; i; --i, ++node_child, ++node2_child) {
                     *node2_key = *node_key;
                     *node2_child = *node_child;
                  }
               }
            }
         }

         // increment depth by 1: alloc root node pointing to child and split child
         void * root;
         // ENOMEM ==> information in node2 is lost !! (corrupt data structure)
         err = allocmem_automatmman(mman, SIZE, &root);
         if (err) goto ONERR;
         ((multistate_node_t*)root)->level = (uint8_t) (((multistate_node_t*)node2)->level + 1);
         ((multistate_node_t*)root)->size  = 2;
         ((multistate_node_t*)root)->key[0]  = key2;
         ((multistate_node_t*)root)->child[0] = node;
         ((multistate_node_t*)root)->child[1] = node2;
         mst->root = root;

         END_SPLIT: ;
      }


   } else if (! mst->size) {
      // === case 2 ===
      mst->root = state;

   } else {
      // === case 3 ===
      const uint16_t SIZE = sizeof(multistate_node_t);
      err = allocmem_automatmman(mman, SIZE, &node);
      if (err) goto ONERR;
      ((multistate_node_t*)node)->level = 0;
      ((multistate_node_t*)node)->size  = 2;
      ((multistate_node_t*)node)->next  = 0;
      if ((state_t*)mst->root < state) {
         ((multistate_node_t*)node)->state[0] = mst->root;
         ((multistate_node_t*)node)->state[1] = state;
      } else {
         ((multistate_node_t*)node)->state[0] = state;
         ((multistate_node_t*)node)->state[1] = mst->root;
      }
      mst->root = node;
   }

   ++ mst->size;

   return 0;
ONERR:
   return err;
}


/* struct: range_t
 * Beschreibt einen Übergangsbereich zu einem Zustand.
 * Wird für Optimierung des Automaten in einen DFA benötigt. */
typedef struct range_t {
   char32_t       from;    // inclusive
   char32_t       to;      // inclusive
   multistate_t   state;
} range_t;

// group: lifetime

/* define: range_INIT
 * Initialisiert range mit Zeichenbereich [from..to]. */
#define range_INIT(from, to) \
         { from, to, multistate_INIT }



// section: automat_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_automat_errtimer
 * Simuliert Fehler in Funktionen für <automat_t> und <automat_mman_t>. */
static test_errortimer_t   s_automat_errtimer = test_errortimer_FREE;
#endif

// group: query

static inline const void* startstate_automat(const automat_t* ndfa)
{
   // documents how to access startstate
   return first_statelist(&ndfa->states);
}

static inline const void* endstate_automat(const automat_t* ndfa)
{
   // documents how to access endstate
   state_t * start = first_statelist(&ndfa->states);
   return start ? next_statelist(start) : 0;
}

// group: lifetime

int free_automat(automat_t* ndfa)
{
   if (ndfa->mman) {
      decruse_automatmman(ndfa->mman);
      *ndfa = (automat_t) automat_FREE;
   }

   return 0;
}

int initmatch_automat(/*out*/automat_t* ndfa, automat_mman_t * mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   int err;
   void * startstate;

   incruse_automatmman(mman);
   const uint16_t SIZE = (uint16_t) (3*state_SIZE + 2*state_SIZE_EMPTYTRANS(1) + 1*state_SIZE_RANGETRANS(nrmatch));
   err = allocmem_automatmman(mman, SIZE, &startstate);
   if (err) goto ONERR;

   state_t * endstate   = (void*) ((uint8_t*)startstate + 1*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t * matchstate = (void*) ((uint8_t*)startstate + 2*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   initempty_state(startstate, matchstate);
   initempty_state(endstate, endstate);
   initrange_state(matchstate, endstate, nrmatch, match_from, match_to);

   // set out
   ndfa->mman = mman;
   ndfa->nrstate = 3;
   initsingle_statelist(&ndfa->states, matchstate);
   insertfirst_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   return 0;
ONERR:
   decruse_automatmman(mman);
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initsequence_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/)
{
   int err;
   void * startstate;

   if (  ndfa1 == ndfa2 || ndfa1->mman != ndfa2->mman
         || ndfa1->nrstate < 2 || ndfa2->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + 2*state_SIZE_EMPTYTRANS(1));
   err = allocmem_automatmman(ndfa1->mman, SIZE, &startstate);
   if (err) goto ONERR;

   state_t * endstate = (void*) ((uint8_t*)startstate + 1*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t * first1   = first_statelist(&ndfa1->states);

   initempty_state(startstate, first1);
   initempty_state(endstate, endstate);

   state_t * last1  = next_statelist(first1);
   state_t * first2 = first_statelist(&ndfa2->states);
   last1->trans.empty[0] = first2;

   state_t * last2 = next_statelist(first2);
   last2->trans.empty[0] = endstate;

   // set out
   ndfa->mman = ndfa1->mman;
   ndfa->nrstate = 2 + ndfa1->nrstate + ndfa2->nrstate;
   initsingle_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);
   insertlastPlist_slist(&ndfa->states, &ndfa1->states);
   insertlastPlist_slist(&ndfa->states, &ndfa2->states);

   // fast free
   decruse_automatmman(ndfa1->mman);
   // 2nd decruse_automatmman not needed to avoid extra call to incruse_automatmman
   *ndfa1 = (automat_t) automat_FREE;
   *ndfa2 = (automat_t) automat_FREE;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initrepeat_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/)
{
   int err;
   void * startstate;

   if (  ndfa1->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + state_SIZE_EMPTYTRANS(2)+state_SIZE_EMPTYTRANS(1));
   err = allocmem_automatmman(ndfa1->mman, SIZE, &startstate);
   if (err) goto ONERR;

   state_t * endstate = (void*) ((uint8_t*)startstate + (state_SIZE + state_SIZE_EMPTYTRANS(2)));
   state_t * first1   = first_statelist(&ndfa1->states);

   initempty2_state(startstate, first1, endstate);
   initempty_state(endstate, endstate);

   state_t * last1  = next_statelist(first1);
   last1->trans.empty[0] = startstate;

   // set out
   ndfa->mman = ndfa1->mman;
   ndfa->nrstate = 2 + ndfa1->nrstate;
   initsingle_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);
   insertlastPlist_slist(&ndfa->states, &ndfa1->states);

   // fast free
   // decruse_automatmman not needed to avoid extra call to incruse_automatmman
   *ndfa1 = (automat_t) automat_FREE;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initor_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/)
{
   int err;
   void * startstate;

   if (  ndfa1 == ndfa2 || ndfa1->mman != ndfa2->mman
         || ndfa1->nrstate < 2 || ndfa2->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + state_SIZE_EMPTYTRANS(2) + state_SIZE_EMPTYTRANS(1));
   err = allocmem_automatmman(ndfa1->mman, SIZE, &startstate);
   if (err) goto ONERR;

   state_t * endstate = (void*) ((uint8_t*)startstate + 1*(state_SIZE + state_SIZE_EMPTYTRANS(2)));
   state_t * first1   = first_statelist(&ndfa1->states);
   state_t * first2   = first_statelist(&ndfa2->states);

   initempty2_state(startstate, first1, first2);
   initempty_state(endstate, endstate);

   state_t * last1 = next_statelist(first1);
   last1->trans.empty[0] = endstate;

   state_t * last2 = next_statelist(first2);
   last2->trans.empty[0] = endstate;

   // set out
   ndfa->mman = ndfa1->mman;
   ndfa->nrstate = 2 + ndfa1->nrstate + ndfa2->nrstate;
   initsingle_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);
   insertlastPlist_slist(&ndfa->states, &ndfa1->states);
   insertlastPlist_slist(&ndfa->states, &ndfa2->states);

   // fast free
   decruse_automatmman(ndfa1->mman);
   // 2nd decruse_automatmman not needed to avoid extra call to incruse_automatmman
   *ndfa1 = (automat_t) automat_FREE;
   *ndfa2 = (automat_t) automat_FREE;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: update

int addmatch_automat(automat_t* ndfa, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   int err;
   void * matchstate;

   if (ndfa->nrstate < 2 || nrmatch == 0) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (state_SIZE + state_SIZE_RANGETRANS(nrmatch));
   err = allocmem_automatmman(ndfa->mman, SIZE, &matchstate);
   if (err) goto ONERR;

   state_t * startstate = first_statelist(&ndfa->states);
   state_t * endstate = next_statelist(startstate);

   initcontinue_state(matchstate, endstate, nrmatch, match_from, match_to);

   // set out
   ndfa->nrstate += 1;
   insertlast_statelist(&ndfa->states, matchstate);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}




// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

/* function: build1_multistate
 * Build level 1 b-tree (root + nrchild leaves).
 *
 * Memory Layout:
 * addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... */
static int build1_multistate(
   /*out*/multistate_t * mst,
   automat_mman_t * mman,
   state_t state[],
   unsigned step, /*every stepth*/
   void * ENDMARKER,
   unsigned nrchild,
   /*out*/void* addr[nrchild+2],
   /*out*/void* child[nrchild])
{
   unsigned S = 0;
   const uint16_t SIZE = sizeof(multistate_node_t);
   TEST(nrchild >= 2);
   TEST(nrchild <= multistate_NROFCHILD);

   TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
   TEST(0 == allocmem_automatmman(mman, SIZE, &mst->root));
   mst->size = nrchild * multistate_NROFSTATE;
   TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
   ((multistate_node_t*)mst->root)->level = 1;
   ((multistate_node_t*)mst->root)->size  = (uint8_t) nrchild;

   void * prevchild = 0;
   for (unsigned i = 0; i < nrchild; ++i) {
      TEST(0 == allocmem_automatmman(mman, SIZE, &child[i]));
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[2+i]));
      if (i) ((multistate_node_t*)mst->root)->key[i-1] = &state[S];
      ((multistate_node_t*)mst->root)->child[i] = child[i];
      ((multistate_node_t*)child[i])->level = 0;
      ((multistate_node_t*)child[i])->size = multistate_NROFSTATE;
      ((multistate_node_t*)child[i])->next = 0;
      for (unsigned s = 0; s < multistate_NROFSTATE; ++s, S += step) {
         ((multistate_node_t*)child[i])->state[s] = &state[S];
      }
      if (prevchild) ((multistate_node_t*)prevchild)->next = child[i];
      prevchild = child[i];
   }

   for (unsigned i = 0; i < nrchild+2; ++i) {
      *((void**)addr[i]) = ENDMARKER;
   }

   return 0;
ONERR:
   return EINVAL;
}

/* function: build1_multistate
 * Build level 2 b-tree (root + nrchild nodes + full leaves).
 * Inserted states are generated beginning from (void*)0 up to
 * (void*)(nrchild*multistate_NROFCHILD*step*multistate_NROFSTATE). */
static int build2_multistate(
   /*out*/multistate_t * mst,
   automat_mman_t * mman,
   unsigned step, /*every stepth*/
   unsigned nrchild,
   /*out*/void* child[nrchild])
{
   const uint16_t SIZE = sizeof(multistate_node_t);
   size_t level1_size = multistate_NROFCHILD*multistate_NROFSTATE;
   TEST(nrchild >= 2);
   TEST(nrchild <= multistate_NROFCHILD);

   TEST(0 == allocmem_automatmman(mman, SIZE, &mst->root));
   mst->size = nrchild * level1_size;
   ((multistate_node_t*)mst->root)->level = 2;
   ((multistate_node_t*)mst->root)->size  = (uint8_t) nrchild;

   for (uintptr_t i = 0; i < nrchild; ++i) {
      TEST(0 == allocmem_automatmman(mman, SIZE, &child[i]));
      if (i) ((multistate_node_t*)mst->root)->key[i-1] = (void*)(i*step*level1_size);
      ((multistate_node_t*)mst->root)->child[i] = child[i];
      ((multistate_node_t*)child[i])->level = 1;
      ((multistate_node_t*)child[i])->size  = multistate_NROFCHILD;
   }

   uintptr_t statenr = 0;
   void * prevleaf = 0;
   for (unsigned i = 0; i < nrchild; ++i) {
      for (unsigned c = 0; c < multistate_NROFCHILD; ++c) {
         void * leaf;
         TEST(0 == allocmem_automatmman(mman, SIZE, &leaf));
         ((multistate_node_t*)child[i])->child[c] = leaf;
         if (c) ((multistate_node_t*)child[i])->key[c-1] = (void*)statenr;
         ((multistate_node_t*)leaf)->level = 0;
         ((multistate_node_t*)leaf)->size  = multistate_NROFSTATE;
         ((multistate_node_t*)leaf)->next  = 0;
         for (unsigned s = 0; s < multistate_NROFSTATE; ++s, statenr += step) {
            ((multistate_node_t*)leaf)->state[s] = (void*)statenr;
         }
         if (prevleaf) ((multistate_node_t*)prevleaf)->next = leaf;
         prevleaf = leaf;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_memorypage(void)
{
   memory_page_t * page[10] = { 0 };
   const size_t    oldsize = SIZEALLOCATED_PAGECACHE();

   // === group constants

   // TEST memory_page_SIZE
   TEST(0 < memory_page_SIZE);
   TEST(ispowerof2_int(memory_page_SIZE));


   // === group helper types

   // TEST pagelist_t
   memory_page_t pageobj[10];
   slist_t       pagelist = slist_INIT;
   for (unsigned i = 0; i < lengthof(pageobj); ++i) {
      insertlast_pagelist(&pagelist, &pageobj[i]);
      TEST(&pageobj[i].next == pagelist.last);
      TEST(pageobj[i].next.next == &pageobj[0].next);
      TEST(pageobj[i ? i-1 : 0].next.next == &pageobj[i].next);
   }

   // === group lifetime

   // TEST new_memorypage
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == new_memorypage(&page[i]));
      // check env
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + (i+1) * memory_page_SIZE);
      // check page
      TEST(0 != page[i]);
   }

   // TEST delete_memorypage
   for (unsigned i = lengthof(page)-1; i < lengthof(page); --i) {
      TEST(0 == delete_memorypage(page[i]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize + i * memory_page_SIZE);
   }

   // TEST new_memorypage: simulated ERROR
   for (unsigned i = 10; i < 13; ++i) {
      memory_page_t * errpage = 0;
      init_testerrortimer(&s_automat_errtimer, 1, (int)i);
      TEST((int)i == new_memorypage(&errpage));
      // check
      TEST(0 == errpage);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
   }

   // TEST delete_memorypage: simulated ERROR
   for (unsigned i = 10; i < 13; ++i) {
      TEST(0 == new_memorypage(&page[0]));
      init_testerrortimer(&s_automat_errtimer, 1, (int)i);
      TEST((int)i == delete_memorypage(page[0]));
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_automatmman(void)
{
   automat_mman_t mman = automat_mman_INIT;
   const size_t   oldsize = SIZEALLOCATED_PAGECACHE();
   void *         addr;

   // === group lifetime

   // TEST automat_mman_INIT
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);

   // TEST free_automatmman
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagelist, page);
      TEST(0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagecache, page);
   }
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + 6 * memory_page_SIZE);
   mman.freemem  = (void*) 1;
   mman.freesize = 10;
   mman.refcount = 1;
   mman.allocated = 1;
   TEST( 0 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);

   // TEST free_automatmman: double free
   TEST( 0 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);

   // TEST free_automatmman: simulated error
   for (unsigned i = 0; i < 3; ++i) {
      memory_page_t * page;
      TEST( 0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagelist, page);
      TEST( 0 == new_memorypage(&page));
      insertlast_pagelist(&mman.pagecache, page);
   }
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 6 * memory_page_SIZE);
   init_testerrortimer(&s_automat_errtimer, 2, 7);
   mman.freemem  = (void*) 2;
   mman.freesize = 2;
   mman.refcount = 2;
   mman.allocated = 2;
   TEST( 7 == free_automatmman(&mman));
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);
   // reset
   mman.refcount = 0;

   // === group query

   // TEST sizeallocated_automatmman
   TEST(0 == sizeallocated_automatmman(&mman));
   for (size_t i = 1; i; i <<= 1) {
      mman.allocated = i;
      TEST(i == sizeallocated_automatmman(&mman));
   }
   // reset
   mman.allocated = 0;

   // === group usage

   // TEST incruse_automatmman
   for (unsigned i = 1; i < 100; ++i) {
      automat_mman_t old;
      memcpy(&old, &mman, sizeof(mman));
      incruse_automatmman(&mman);
      // check mman refcount
      TEST( i == mman.refcount);
      // check mman nothing changed except refcount
      ++ old.refcount;
      TEST( 0 == memcmp(&old, &mman, sizeof(mman)));
   }

   // TEST decruse_automatmman
   for (size_t pl_size = 0; pl_size <= 3; pl_size += 3) {
      for (size_t pc_size = 0; pc_size <= 2; pc_size += 2) {
         memory_page_t * page[5] = { 0 };
         size_t          p_size  = 0;
         // prepare
         for (unsigned i = 0; i < pc_size; ++i, ++p_size) {
            TEST( 0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman.pagecache, page[p_size]);
         }
         for (unsigned i = 0; i < pl_size; ++i, ++p_size) {
            TEST(0 == new_memorypage(&page[p_size]));
            insertlast_pagelist(&mman.pagelist, page[p_size]);
         }
         mman.refcount = 10;
         mman.freemem  = (void*)1;
         mman.freesize = 9;
         mman.allocated = 8;
         automat_mman_t old;
         memcpy(&old, &mman, sizeof(mman));
         for (unsigned i = 9; i; --i) {
            // test refcount > 1
            decruse_automatmman(&mman);
            // check mman refcount
            TEST( i == mman.refcount);
            TEST( 1 == (uintptr_t)mman.freemem);
            TEST( 9 == mman.freesize);
            TEST( 8 == mman.allocated);
            // check mman other field not changed
            -- old.refcount;
            TEST(0 == memcmp(&old, &mman, sizeof(mman)));
         }
         // test refcount == 1
         decruse_automatmman(&mman);
         // check mman
         TEST( isempty_slist(&mman.pagelist));
         TEST( (p_size == 0) == isempty_slist(&mman.pagecache));
         TEST( 0 == mman.refcount);
         TEST( 0 == mman.freemem);
         TEST( 0 == mman.freesize);
         TEST( 0 == mman.allocated);
         slist_node_t * it = last_slist(&mman.pagecache);
         for (unsigned i = 0; i < p_size; ++i) {
            it = next_slist(it);
            TEST(it == (void*) page[i]);
         }
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   // === group resource helper

   // prepare
   mman = (automat_mman_t) automat_mman_INIT;
   memory_page_t * page[10];

   // TEST getfreepage_automatmman: pagecache is empty
   for (unsigned i = 0; i < lengthof(page); ++i) {
      TEST(0 == getfreepage_automatmman(&mman, &page[i]));
      // check page
      TEST( page[i] == (void*) next_slist(&page[i?i-1:0]->next));
      TEST( page[0] == (void*) next_slist(&page[i]->next));
      // check mman
      TEST( page[i] == (void*) last_slist(&mman.pagelist));
      TEST( isempty_slist(&mman.pagecache));
      TEST( 0 == mman.refcount);
      TEST( 0 == mman.freemem);
      TEST( 0 == mman.freesize);
      TEST( 0 == mman.allocated);
   }

   // TEST getfreepage_automatmman: pagecache is not empty
   // prepare
   insertlastPlist_slist(&mman.pagecache, &mman.pagelist);
   for (unsigned i = 0; i < lengthof(page); ++i) {
      memory_page_t * cached_page;
      TEST(0 == getfreepage_automatmman(&mman, &cached_page));
      // check cached_page
      TEST(page[i] == cached_page);
      TEST(page[0] == (void*) next_slist(&cached_page->next));
      // check mman
      TEST(page[i] == (void*) last_slist(&mman.pagelist));
      if (i == lengthof(page)-1) {
         TEST( isempty_slist(&mman.pagecache));
      } else {
         TEST( page[i+1] == (void*) first_slist(&mman.pagecache));
      }
      TEST( 0 == mman.refcount);
      TEST( 0 == mman.freemem);
      TEST( 0 == mman.freesize);
      TEST( 0 == mman.allocated);
   }

   // TEST getfreepage_automatmman: simulated ERROR
   {
      memory_page_t * dummy = 0;
      automat_mman_t  old;
      memcpy(&old, &mman, sizeof(mman));
      TEST( isempty_slist(&mman.pagecache));
      init_testerrortimer(&s_automat_errtimer, 1, 8);
      // test
      TEST( 8 == getfreepage_automatmman(&mman, &dummy));
      // check mman not changed
      TEST( 0 == memcmp(&old, &mman, sizeof(mman)));
      TEST( 0 == dummy);
   }

   // reset
   TEST( 0 == free_automatmman(&mman));
   mman = (automat_mman_t) automat_mman_INIT;

   // === group allocate resources

   // TEST allocmem_automatmman: size == 0 (no allocation)
   TEST( 0 == allocmem_automatmman(&mman, 0, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize);
   // check addr
   TEST( 0 == addr);
   // check mman
   TEST( isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( 0 == mman.refcount);
   TEST( 0 == mman.freemem);
   TEST( 0 == mman.freesize);
   TEST( 0 == mman.allocated);

   // TEST allocmem_automatmman: size == 1 (allocate new page)
   TEST( 0 == allocmem_automatmman(&mman, 1, &addr));
   // check env
   TEST(SIZEALLOCATED_PAGECACHE() == oldsize + memory_page_SIZE);
   // check addr
   TEST(addr == (void*) last_pagelist(&mman.pagelist)->data);
   // check res
   TEST(!isempty_slist(&mman.pagelist));
   TEST( isempty_slist(&mman.pagecache));
   TEST( mman.refcount == 0);
   TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
   TEST( mman.freesize == memory_page_SIZE - 1 - offsetof(memory_page_t, data));
   TEST( mman.allocated == 1);

   // TEST allocmem_automatmman: allocate cached page
   // prepare
   mman.freesize = 2;
   TEST(0 == new_memorypage(&page[0]));
   insertlast_pagelist(&mman.pagecache, page[0]);
   // test
   TEST( 0 == allocmem_automatmman(&mman, 3, &addr));
   // check env
   TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
   // check addr
   TEST( addr == (void*) page[0]->data);
   // check mman
   TEST( last_pagelist(&mman.pagelist) == page[0]);
   TEST( isempty_slist(&mman.pagecache));
   TEST( mman.refcount == 0);
   TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
   TEST( mman.allocated == 4);
   TEST( mman.freesize == memory_page_SIZE - 3 - offsetof(memory_page_t, data));

   // TEST allocmem_automatmman: allocate from current page
   for (unsigned i = 0, off = 3, S = 4; i <= UINT16_MAX; S += i, off += i, ++i) {
      if (256 == i) i = UINT16_MAX-2;
      TESTP( 0 == allocmem_automatmman(&mman, (uint16_t) i, &addr), "i:%d", i);
      // check env
      TEST( SIZEALLOCATED_PAGECACHE() == oldsize + 2 * memory_page_SIZE);
      // check addr
      TEST( addr == off + (uint8_t*) page[0]->data);
      // check res
      TEST( last_pagelist(&mman.pagelist) == page[0]);
      TEST( isempty_slist(&mman.pagecache));
      TEST( mman.refcount == 0);
      TEST( mman.freemem  == memory_page_SIZE + (uint8_t*) last_pagelist(&mman.pagelist));
      TEST( mman.freesize == memory_page_SIZE - off - i - offsetof(memory_page_t, data));
      TEST( mman.allocated == S + i);
   }

   // reset
   TEST(0 == free_automatmman(&mman));

   return 0;
ONERR:
   free_automatmman(&mman);
   return EINVAL;
}

static int test_state(void)
{
   const unsigned NROFSTATE = 256;
   state_t        state[NROFSTATE];
   char32_t       from[256];
   char32_t       to[256];

   // prepare
   memset(&state, 0, sizeof(state));
   for (unsigned r = 0; r < 256; ++r) {
      from[r] = r + 1;
      to[r] = r + 10;
   }

   // === constants

   // TEST state_SIZE
   static_assert( state_SIZE == sizeof(state_t) - sizeof(transition_t),
                  "nodesize without size of transition"
                  );

   // TEST state_SIZE_EMPTYTRANS
   static_assert( 0 == state_SIZE_EMPTYTRANS(0)
                  && sizeof(state_t*) == state_SIZE_EMPTYTRANS(1)
                  && 255*sizeof(state_t*) == state_SIZE_EMPTYTRANS(255),
                  "calcs size of empty transition"
                  );

   // TEST state_SIZE_RANGETRANS
   static_assert( 0 == state_SIZE_RANGETRANS(0)
                  && sizeof(range_transition_t) == state_SIZE_RANGETRANS(1)
                  && 255*sizeof(range_transition_t) == state_SIZE_RANGETRANS(255),
                  "calcs size of range transition"
                  );

   // === type support

   // TEST slist_IMPLEMENT: _statelist
   slist_t list = slist_INIT;
   // test only insertlast_statelist
   for (unsigned i = 0; i < NROFSTATE; ++i) {
      insertlast_statelist(&list, &state[i]);
      // check list
      TEST(&state[i] == last_statelist(&list));
   }
   // check state[]
   for (unsigned i = 0; i < NROFSTATE; ++i) {
      TEST(&state[(i+1)%256] == next_statelist(&state[i]));
      TEST((slist_node_t*)&state[(i+1)%256].next == state[i].next);
   }

   // === lifetime

   // TEST initempty_state
   memset(&state, 0, sizeof(state));
   state[0].next = (void*) &state[1].next;
   initempty_state(&state[0], &state[2]);
   TEST(state[0].type == state_EMPTY);
   TEST(state[0].nrtrans == 1);
   TEST(state[0].next == (void*) &state[1].next); // unchanged
   TEST(state[0].trans.empty[0] == &state[2]);
   TEST(state[0].trans.empty[1] == 0); // unchanged
   TEST(state[0].trans.empty[2] == 0); // unchanged
   // reset
   state[0].trans.empty[0] = 0;

   // TEST initempty2_state
   initempty2_state(&state[0], &state[2], &state[5]);
   TEST(state[0].type == state_EMPTY);
   TEST(state[0].nrtrans == 2);
   TEST(state[0].next == (void*) &state[1].next);  // unchanged
   TEST(state[0].trans.empty[0] == &state[2]);
   TEST(state[0].trans.empty[1] == &state[5]);
   TEST(state[0].trans.empty[2] == 0);             // unchanged
   // reset
   state[0].trans.empty[0] = 0;
   state[0].trans.empty[1] = 0;

   // TEST initrange_state
   static_assert( sizeof(state) > state_SIZE + state_SIZE_RANGETRANS(256),
                  "initrange_state does not overflow state array");
   for (unsigned i = 0; i < 256; ++i) {
      initrange_state(&state[0], &state[3], (uint8_t)i, from, to);
      TEST(state[0].type == state_RANGE);
      TEST(state[0].nrtrans == i);
      TEST(state[0].next == (void*) &state[1].next); // unchanged
      for (unsigned r = 255; r <= 255; --r) {
         TEST(state[0].trans.range[r].state == (r < i ? &state[3] : 0));
         TEST(state[0].trans.range[r].from  == (unsigned) (r < i ? r+1  : 0));
         TEST(state[0].trans.range[r].to    == (unsigned) (r < i ? r+10 : 0));
      }
      // reset
      memset(&state, 0, sizeof(state));
      state[0].next = (void*) &state[1].next;
   }

   // TEST initcontinue_state
   static_assert( sizeof(state) > state_SIZE + state_SIZE_RANGETRANS(256),
                  "initcontinue_state does not overflow state array");
   for (unsigned i = 0; i < 256; ++i) {
      initcontinue_state(&state[0], &state[3], (uint8_t)i, from, to);
      TEST(state[0].type == state_RANGE_CONTINUE);
      TEST(state[0].nrtrans == i);
      TEST(state[0].next == (void*) &state[1].next); // unchanged
      for (unsigned r = 255; r <= 255; --r) {
         TEST(state[0].trans.range[r].state == (r < i ? &state[3] : 0));
         TEST(state[0].trans.range[r].from  == (unsigned) (r < i ? r+1  : 0));
         TEST(state[0].trans.range[r].to    == (unsigned) (r < i ? r+10 : 0));
      }
      // reset
      memset(&state, 0, sizeof(state));
      state[0].next = (void*) &state[1].next;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_multistate(void)
{
   const unsigned NROFSTATE = 256;
   state_t        state[NROFSTATE];
   multistate_t   mst = multistate_INIT;
   automat_mman_t mman = automat_mman_INIT;
   void * const   ENDMARKER = (void*) (uintptr_t) 0x01234567;
   const unsigned LEVEL1_NROFSTATE = multistate_NROFSTATE * multistate_NROFCHILD;

   static_assert( 2*LEVEL1_NROFSTATE <= NROFSTATE,
                  "state array big enough to build root node with all possible number of leaves"
                  "and also supports interleaving (only every 2nd state to support inserting in between)"
                  );

   // prepare
   memset(&state, 0, sizeof(state));

   // TEST multistate_NROFSTATE
   TEST( lengthof(((multistate_node_t*)0)->state) == multistate_NROFSTATE);

   // TEST multistate_NROFCHILD
   TEST( lengthof(((multistate_node_t*)0)->child) == multistate_NROFCHILD);

   // TEST multistate_NROFKEY
   TEST( lengthof(((multistate_node_t*)0)->key)   == multistate_NROFKEY);

   // TEST multistate_INIT
   TEST(0 == mst.size);
   TEST(0 == mst.root);

   // TEST add_multistate: multistate_t.size == 0
   for (unsigned i = 0; i < NROFSTATE; ++i) {
      mst = (multistate_t) multistate_INIT;
      TEST( 0 == add_multistate(&mst, &state[i], &mman));
      // check mman: nothing allocated
      TEST( 0 == sizeallocated_automatmman(&mman));
      // check mst
      TEST( 1 == mst.size);
      TEST( &state[i] == mst.root);
   }

   // TEST add_multistate: multistate_t.size == 1
   for (unsigned i = 0; i < NROFSTATE-1; ++i) {
      for (unsigned order = 0; order <= 1; ++order) {
         // prepare
         mst = (multistate_t) multistate_INIT;
         TEST(0 == add_multistate(&mst, &state[i + !order], &mman));
         // test
         TEST( 0 == add_multistate(&mst, &state[i + order], &mman));
         // check mman
         TEST( sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
         // check mst
         TEST( 2 == mst.size);
         TEST( 0 != mst.root);
         // check mst.root content
         TEST( 0 == ((multistate_node_t*)mst.root)->level);
         TEST( 2 == ((multistate_node_t*)mst.root)->size);
         TEST( &state[i] == ((multistate_node_t*)mst.root)->state[0]);
         TEST( &state[i+1] == ((multistate_node_t*)mst.root)->state[1]);
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   // TEST add_multistate: single leaf && add states ascending/descending
   for (unsigned asc = 0; asc <= 1; ++asc) {
      void * addr = 0;
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         // test
         TEST( 0 == add_multistate(&mst, &state[asc ? i : multistate_NROFSTATE-1-i], &mman));
         if (1 == i) {  // set end marker at end of node
            TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr));
            *((void**)addr) = ENDMARKER;
            // check that end marker is effective !!
            TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         }
         // check mman
         TEST( (i > 0 ? sizeof(void*)+sizeof(multistate_node_t) : 0) == sizeallocated_automatmman(&mman));
         // check mst
         TEST( i+1 == mst.size);
         TEST( 0   != mst.root);
         // check mst.root content
         if (i >= 1) {
            TEST( 0   == ((multistate_node_t*)mst.root)->level);
            TEST( i+1 == ((multistate_node_t*)mst.root)->size);
            for (unsigned s = 0; s <= i; ++s) {
               TEST( &state[asc ? s : multistate_NROFSTATE-1-i+s] == ((multistate_node_t*)mst.root)->state[s]);
            }
            TEST( ENDMARKER == *((void**)addr)); // no overflow into following memory block
         }
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: single leaf && add states unordered
   for (unsigned S = 3; S < multistate_NROFSTATE; ++S) {
      for (unsigned pos = 0; pos < S; ++pos) {
         // prepare
         mst = (multistate_t) multistate_INIT;
         for (unsigned i = 0; i < S; ++i) {
            if (i != pos) { TEST(0 == add_multistate(&mst, &state[i], &mman)); }
         }
         void * addr = 0;
         TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr));
         *((void**)addr) = ENDMARKER;
         TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         // test
         TEST( 0 == add_multistate(&mst, &state[pos], &mman));
         // check: mman
         TEST( sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
         // check: mst
         TEST( S == mst.size);
         TEST( 0 != mst.root);
         // check: mst.root content
         TEST( 0 == ((multistate_node_t*)mst.root)->level);
         TEST( S == ((multistate_node_t*)mst.root)->size);
         for (unsigned i = 0; i < S; ++i) {
            TEST( &state[i] == ((multistate_node_t*)mst.root)->state[i]);
         }
         // check: no overflow into following memory block
         TEST( ENDMARKER == *((void**)addr));
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   // TEST add_multistate: single leaf: EEXIST
   {
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         TEST(0 == add_multistate(&mst, &state[i], &mman));
      }
      void * addr = 0;
      TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr));
      *((void**)addr) = ENDMARKER;
      TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         // test
         TEST( EEXIST == add_multistate(&mst, &state[i], &mman));
         // check mman
         TEST( sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
         // check mst
         TEST( multistate_NROFSTATE == mst.size);
         TEST( 0 != mst.root);
         // check mst.root content
         TEST( 0 == ((multistate_node_t*)mst.root)->level);
         TEST( multistate_NROFSTATE == ((multistate_node_t*)mst.root)->size);
         TEST( 0 == ((multistate_node_t*)mst.root)->next);
         for (unsigned s = 0; s < multistate_NROFSTATE; ++s) {
            TEST( &state[s] == ((multistate_node_t*)mst.root)->state[s]);
         }
         TEST( ENDMARKER == *((void**)addr)); // no overflow into following memory block
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: split leaf node (level 0) ==> build root (level 1) ==> 3 nodes total
   for (unsigned splitidx = 0; splitidx <= multistate_NROFSTATE; ++splitidx) {
      void * addr[2] = { 0 };
      TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0, next = 0; i < multistate_NROFSTATE; ++i, ++next) {
         if (next == splitidx) ++next;
         TEST(0 == add_multistate(&mst, &state[next], &mman));
      }
      TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr[1]));
      *((void**)addr[1]) = ENDMARKER;
      TEST(addr[1] == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
      TEST(addr[0] == &((void**)mst.root)[-1]);
      // test
      void * oldroot = mst.root;
      TEST( 2*sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
      TEST( 0 == add_multistate(&mst, &state[splitidx], &mman));
      // check: mman
      TEST( 2*sizeof(void*)+3*sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
      // check: mst
      TEST( multistate_NROFSTATE+1 == mst.size);
      TEST( mst.root != 0);
      TEST( mst.root != oldroot);
      TEST( mst.root == ((uint8_t*)addr[1]) + sizeof(void*) + sizeof(multistate_node_t));
      // check: mst.root content
      TEST( 1 == ((multistate_node_t*)mst.root)->level);
      TEST( 2 == ((multistate_node_t*)mst.root)->size);
      TEST( ((multistate_node_t*)mst.root)->child[0] == oldroot);
      TEST( ((multistate_node_t*)mst.root)->child[1] == (multistate_node_t*) (((uint8_t*)addr[1]) + sizeof(void*)));
      TEST( ((multistate_node_t*)mst.root)->key[0]   == ((multistate_node_t*)mst.root)->child[1]->state[0]);
      // check: leaf1 content
      multistate_node_t* leaf1 = ((multistate_node_t*)mst.root)->child[0];
      multistate_node_t* leaf2 = ((multistate_node_t*)mst.root)->child[1];
      TEST( 0 == leaf1->level);
      TEST( multistate_NROFSTATE/2 + 1 == leaf1->size);
      TEST( leaf2 == leaf1->next);
      for (unsigned i = 0; i < leaf1->size; ++i) {
         TEST( &state[i] == leaf1->state[i]);
      }
      // check: leaf2 content
      TEST( 0 == leaf2->level);
      TEST( multistate_NROFSTATE/2 == leaf2->size);
      TEST( 0 == leaf2->next);
      for (unsigned i = 0; i < leaf2->size; ++i) {
         TEST( &state[leaf1->size+i] == leaf2->state[i]);
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < lengthof(addr); ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: build root node (level 1) + many leafs (level 0) ascending/descending
   for (unsigned desc = 0; desc <= 1; ++desc) {
      void * child[multistate_NROFCHILD] = { 0 };
      void * addr[multistate_NROFCHILD] = { 0 };
      TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      child[0] = (uint8_t*)addr[0] + sizeof(void*);
      child[1] = (uint8_t*)child[0] + sizeof(multistate_node_t);
      void * root = (uint8_t*)child[1] + sizeof(multistate_node_t);
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE+1; ++i) {
         TEST(0 == add_multistate(&mst, &state[desc?NROFSTATE-1-i:i], &mman));
      }
      unsigned SIZE = multistate_NROFSTATE+1;
      TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr[1]));
      *((void**)addr[1]) = ENDMARKER;
      for (unsigned nrchild = 2; nrchild <= multistate_NROFCHILD; ) {
         for (unsigned nrstate = 1; nrstate <= multistate_NROFSTATE/2+(desc==0); ++nrstate) {
            const unsigned isSplit = (nrstate == multistate_NROFSTATE/2+(desc==0));
            if (isSplit && nrchild==multistate_NROFCHILD) {
               // fill only last but do not split it else root would overflow
               // this is checked in next test case
               ++nrchild;
               break;
            }
            // test add to last(desc==0)/first(desc==1) leaf
            TEST( 0 == add_multistate(&mst, &state[desc?NROFSTATE-1-SIZE:SIZE], &mman));
            if (isSplit) {
               if (desc) memmove(&child[2], &child[1], (nrchild-1)*sizeof(child[0]));
               child[desc?1:nrchild] = (uint8_t*)addr[nrchild-1] + sizeof(void*);
               TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr[nrchild]));
               *((void**)addr[nrchild]) = ENDMARKER;
               ++ nrchild;
            }
            // check: mman
            TEST( nrchild*sizeof(void*)+(nrchild+1)*sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
            // check: mst
            ++ SIZE;
            TEST( SIZE == mst.size);
            TEST( root == mst.root);
            // check mst.root content
            TEST( 1       == ((multistate_node_t*)mst.root)->level);
            TEST( nrchild == ((multistate_node_t*)mst.root)->size);
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST( child[i] == ((multistate_node_t*)mst.root)->child[i]);
               if (i) {
                  state_t * key = &state[ desc ? NROFSTATE-nrchild*(multistate_NROFSTATE/2) + i*(multistate_NROFSTATE/2)
                                : i*(multistate_NROFSTATE/2+1)];
                  TEST( key == ((multistate_node_t*)mst.root)->key[i-1]);
               }
            }
            // check: leaf content
            for (unsigned i = 0, istate=desc?NROFSTATE-SIZE:0; i < nrchild; ++i) {
               const unsigned isLast = desc?i==0:i==nrchild-1;
               const unsigned S = (multistate_NROFSTATE/2+(desc?isLast:(unsigned)!isLast)) + nrstate*isLast*(!isSplit);
               TEST( 0 == ((multistate_node_t*)child[i])->level);
               TEST( S == ((multistate_node_t*)child[i])->size);
               TEST( (i==nrchild-1?0:child[i+1]) == ((multistate_node_t*)child[i])->next);
               for (unsigned s = 0; s < S; ++s, ++istate) {
                  TEST( &state[istate] == ((multistate_node_t*)child[i])->state[s]);
               }
            }
            // check: no overflow into surrounding memory
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST( ENDMARKER == *((void**)addr[i]));
            }
         }
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: build root node (level 1) + many leafs (level 0) unordered
   for (unsigned nrchild=2; nrchild < multistate_NROFCHILD; ++nrchild) {
      for (unsigned pos = 0; pos < nrchild; ++pos) {
         void * child[multistate_NROFCHILD] = { 0 };
         void * addr[multistate_NROFCHILD+1] = { 0 };
         // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild
         TEST(0 == build1_multistate(&mst, &mman, state, 2/*every 2nd*/, ENDMARKER, nrchild, addr, child));
         void *   const root = mst.root;
         unsigned const SIZE = nrchild * multistate_NROFSTATE + 1;
         // test add single state ==> split check split
         TEST( 0 == add_multistate(&mst, &state[1+pos*(2*multistate_NROFSTATE)], &mman));
         // check: mman
         TEST( (nrchild+2)*sizeof(void*)+(nrchild+2)*sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
         // check: mst
         TEST( SIZE == mst.size);
         TEST( root == mst.root);
         // check mst.root content
         TEST( 1         == ((multistate_node_t*)mst.root)->level);
         TEST( nrchild+1 == ((multistate_node_t*)mst.root)->size);
         memmove(&child[pos+2], &child[pos+1], (nrchild-1-pos)*sizeof(child[0]));
         child[pos+1] = (uint8_t*)addr[1+nrchild] + sizeof(void*);
         for (unsigned i = 0; i < nrchild+1; ++i) {
            TEST( child[i] == ((multistate_node_t*)mst.root)->child[i]);
            if (i) {
               state_t * key = &state[(i-(i>pos))*(2*multistate_NROFSTATE)+(i==pos+1)*multistate_NROFSTATE];
               TEST( key == ((multistate_node_t*)mst.root)->key[i-1]);
            }
         }
         // check: leaf content
         for (unsigned i = 0, istate=0; i < nrchild+1; ++i) {
            unsigned const S = i == pos ? multistate_NROFSTATE/2+1 : i == pos+1 ? multistate_NROFSTATE/2 : multistate_NROFSTATE;
            TEST( 0 == ((multistate_node_t*)child[i])->level);
            TEST( S == ((multistate_node_t*)child[i])->size);
            TEST( (i==nrchild?0:child[i+1]) == ((multistate_node_t*)child[i])->next);
            for (unsigned s = 0; s < S; ++s, istate += (s<=2&&i==pos?1:2)) {
               TEST( &state[istate] == ((multistate_node_t*)child[i])->state[s]);
            }
         }
         // check: no overflow into surrounding memory
         for (unsigned i = 0; i < nrchild+2; ++i) {
            TEST( ENDMARKER == *((void**)addr[i]));
         }
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   // TEST add_multistate: split root node (level 1)
   for (unsigned pos = 0; pos < multistate_NROFCHILD; ++pos) {
      void * child[multistate_NROFCHILD+1] = { 0 };
      void * addr[multistate_NROFCHILD+2] = { 0 };
      // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild splitroot new-root
      TEST(0 == build1_multistate(&mst, &mman, state, 2/*every 2nd*/, ENDMARKER, multistate_NROFCHILD, addr, child));
      void *   const oldroot = mst.root;
      void *   const splitchild = (uint8_t*)addr[multistate_NROFCHILD+1] + sizeof(void*);
      void *   const splitroot = (uint8_t*)splitchild + sizeof(multistate_node_t);
      void *   const root = (uint8_t*)splitroot + sizeof(multistate_node_t);
      unsigned const SIZE = multistate_NROFCHILD * multistate_NROFSTATE + 1;
      state_t* const splitchild_key = &state[pos*(2*multistate_NROFSTATE)+multistate_NROFSTATE];
      state_t* const splitroot_key  = pos < multistate_NROFCHILD/2 ? &state[(multistate_NROFCHILD/2)*(2*multistate_NROFSTATE)]
                                    : pos == multistate_NROFCHILD/2 ? splitchild_key
                                    : &state[(multistate_NROFCHILD/2+1)*(2*multistate_NROFSTATE)];
      memmove(&child[pos+2], &child[pos+1], (multistate_NROFCHILD-1-pos)*sizeof(child[0]));
      child[pos+1] = splitchild;
      // test add single state ==> split check split
      TEST( 0 == add_multistate(&mst, &state[1+pos*(2*multistate_NROFSTATE)], &mman));
      // check: mman
      TEST( (multistate_NROFCHILD+2)*sizeof(void*)+(multistate_NROFCHILD+4)*sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
      // check: mst
      TEST( SIZE == mst.size);
      TEST( root == mst.root);
      // check mst.root content
      TEST( 2 == ((multistate_node_t*)mst.root)->level);
      TEST( 2 == ((multistate_node_t*)mst.root)->size);
      TEST( splitroot_key == ((multistate_node_t*)mst.root)->key[0]);
      TEST( oldroot   == ((multistate_node_t*)mst.root)->child[0]);
      TEST( splitroot == ((multistate_node_t*)mst.root)->child[1]);
      // check oldroot / splitroot content
      for (unsigned i = 0, ichild = 0; i < 2; ++i) {
         const unsigned S = multistate_NROFCHILD/2+1-i;
         TEST( 1 == ((multistate_node_t*)mst.root)->child[i]->level);
         TEST( S == ((multistate_node_t*)mst.root)->child[i]->size);
         for (unsigned s = 0; s < S; ++s, ++ichild) {
            TEST( child[ichild] == ((multistate_node_t*)mst.root)->child[i]->child[s]);
            if (s) {
               state_t * key = ((multistate_node_t*)child[ichild])->state[0];
               TEST( key == ((multistate_node_t*)mst.root)->child[i]->key[s-1]);
            }
         }
      }
      // check: leaf content
      for (unsigned i = 0, istate=0; i < multistate_NROFCHILD+1; ++i) {
         unsigned const S = i == pos ? multistate_NROFSTATE/2+1 : i == pos+1 ? multistate_NROFSTATE/2 : multistate_NROFSTATE;
         TEST( 0 == ((multistate_node_t*)child[i])->level);
         TEST( S == ((multistate_node_t*)child[i])->size);
         TEST( (i==multistate_NROFCHILD?0:child[i+1]) == ((multistate_node_t*)child[i])->next);
         for (unsigned s = 0; s < S; ++s, istate += (s<=2&&i==pos?1:2)) {
            TEST( &state[istate] == ((multistate_node_t*)child[i])->state[s]);
         }
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < multistate_NROFCHILD+2; ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: (level 2) split child(level 1) and add to root
   for (unsigned nrchild=2; nrchild < multistate_NROFCHILD; ++nrchild) {
      for (uintptr_t pos = 0; pos < nrchild; ++pos) {
         void * addr;
         void * child[multistate_NROFCHILD] = { 0 };
         size_t SIZE = nrchild * LEVEL1_NROFSTATE + 1;
         TEST(0 == build2_multistate(&mst, &mman, 2, nrchild, child));
         void * root = mst.root;
         TEST(0 == allocmem_automatmman(&mman, sizeof(void*), &addr));
         void * splitchild = (uint8_t*)addr + sizeof(void*) + sizeof(multistate_node_t)/*leaf*/;
         // test add single state ==> split check split
         TEST( 0 == add_multistate(&mst, (void*)(1+pos*(2*LEVEL1_NROFSTATE)), &mman));
         // check: mman
         TEST( sizeof(void*)+(1+2+nrchild+nrchild*multistate_NROFCHILD)*sizeof(multistate_node_t) == sizeallocated_automatmman(&mman));
         // check: mst
         TEST( SIZE == mst.size);
         TEST( root == mst.root);
         // check mst.root content
         TEST( 2         == ((multistate_node_t*)mst.root)->level);
         TEST( nrchild+1 == ((multistate_node_t*)mst.root)->size);
         state_t * key;
         for (uintptr_t i = 0; i <= pos; ++i) {
            key = (state_t*)(void*)(i*(2*LEVEL1_NROFSTATE));
            if (i) TEST( key == ((multistate_node_t*)mst.root)->key[i-1]);
            TEST( child[i] == ((multistate_node_t*)mst.root)->child[i]);
         }
         key = (state_t*)(void*)(pos*(2*LEVEL1_NROFSTATE)+LEVEL1_NROFSTATE);
         TEST( key == ((multistate_node_t*)mst.root)->key[pos]);
         TEST( splitchild == ((multistate_node_t*)mst.root)->child[pos+1]);
         for (uintptr_t i = pos+2; i < nrchild; ++i) {
            key = (state_t*)(void*)((i-1)*(2*LEVEL1_NROFSTATE));
            TEST( key == ((multistate_node_t*)mst.root)->key[i-1]);
            TEST( child[i-1] == ((multistate_node_t*)mst.root)->child[i]);
         }
         // check node/leaf content
         TEST( EEXIST == add_multistate(&mst, (void*)(1+pos*(2*LEVEL1_NROFSTATE)), &mman));
         for (uintptr_t i = 0; i < LEVEL1_NROFSTATE*nrchild; ++i) {
            TEST(EEXIST == add_multistate(&mst, (void*)(2*i), &mman));
         }
         // reset
         TEST(0 == free_automatmman(&mman));
      }
   }

   return 0;
ONERR:
   free_automatmman(&mman);
   return EINVAL;
}

static int helper_get_states(automat_t * ndfa, size_t maxsize, /*out*/state_t * states[maxsize])
{
   size_t i = 0;

   foreach (_statelist, s, &ndfa->states) {
      if (i == maxsize) return ENOMEM;
      states[i] = s;
      ++ i;
   }

   if (i != ndfa->nrstate) {
      return EINVAL;
   }

   return 0;
}

typedef struct {
   uint8_t type;  // value from state_e
   uint8_t nrtrans;
   size_t * target_state;  // [nrtrans]: array of state indizes beginning from 0
   char32_t * from;        // [nrtrans]: array of from chars describing range transtiions
   char32_t * to;          // [nrtrans]: array of to chars describing range transtiions
} helper_state_t;

static int helper_compare_states(automat_t * ndfa, size_t nrstate, const helper_state_t helperstate[nrstate])
{
   state_t * ndfa_state[258]; // allows indexing of states by number [0..nrstate-1]

   TEST(nrstate == ndfa->nrstate);
   TEST(0 == helper_get_states(ndfa, lengthof(ndfa_state), ndfa_state));

   for (size_t i = 0; i < nrstate; ++i) {
      TEST(helperstate[i].type == ndfa_state[i]->type);
      TEST(helperstate[i].nrtrans == ndfa_state[i]->nrtrans);
      // check transitions
      for (unsigned t = 0; t < helperstate[i].nrtrans; ++t) {
         const size_t state_idx = helperstate[i].target_state[t];
         TEST(state_idx < nrstate);
         if (helperstate[i].type == state_EMPTY) {
            TESTP(ndfa_state[state_idx] == ndfa_state[i]->trans.empty[t], "i:%d", i);

         } else if ( helperstate[i].type == state_RANGE
                     || helperstate[i].type == state_RANGE_CONTINUE) {
            TEST(ndfa_state[state_idx]  == ndfa_state[i]->trans.range[t].state);
            TEST(helperstate[i].from[t] == ndfa_state[i]->trans.range[t].from);
            TEST(helperstate[i].to[t]   == ndfa_state[i]->trans.range[t].to);

         } else {
            TEST(0/*not implemented*/);
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   automat_t      ndfa = automat_FREE;
   automat_t      ndfa1, ndfa2;
   size_t         S;
   automat_mman_t mman = automat_mman_INIT;
   automat_mman_t mman2 = automat_mman_INIT;
   char32_t       from[256];
   char32_t       to[256];
   helper_state_t helperstate[10];
   size_t         target_state[256];

   // TEST automat_FREE
   TEST( 0 == ndfa.mman);
   TEST( 0 == ndfa.nrstate);
   TEST( 1 == isempty_slist(&ndfa.states));

   // prepare
   for (unsigned i = 0; i < lengthof(target_state); ++i) {
      target_state[i] = 1;
   }

   for (size_t i = 0; i < 256; ++i) {
      // prepare
      for (unsigned r = 0; r < i; ++r) {
         from[r] = r;
         to[r] = 3*r;
      }

      // TEST initmatch_automat
      TESTP( 0 == initmatch_automat(&ndfa, &mman, (uint8_t) i, from, to), "i:%zd", i);
      // check mman
      TEST( 1 == mman.refcount);
      S = (unsigned) (3*(sizeof(state_t) - sizeof(transition_t)))
        + (unsigned) (2*sizeof(empty_transition_t) + i*sizeof(range_transition_t));
      TEST( S == sizeallocated_automatmman(&mman));
      // check ndfa
      TEST( &mman == ndfa.mman);
      TEST( 3     == ndfa.nrstate);
      TEST( 0     == isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 2 }, 0, 0 };
      helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      helperstate[2] = (helper_state_t) { state_RANGE, (uint8_t) i, target_state, from, to };
      TEST( 0 == helper_compare_states(&ndfa, 3, helperstate))

      // TEST free_automat
      TEST( 0 == free_automat(&ndfa));
      // check mman
      TEST( 0 == mman.refcount);
      // check ndfa
      TEST( 0 == ndfa.mman);
      TEST( 0 == ndfa.nrstate);
      TEST( 1 == isempty_slist(&ndfa.states));

      // TEST free_automat: double free
      TEST( 0 == free_automat(&ndfa));
      // check mman
      TEST( 0 == mman.refcount);
      // check ndfa
      TEST( 0 == ndfa.mman);
      TEST( 0 == ndfa.nrstate);
      TEST( 1 == isempty_slist(&ndfa.states));
   }

   // TEST initsequence_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(0 == initmatch_automat(&ndfa2, &mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
   TEST(2 == mman.refcount);
   S = sizeallocated_automatmman(&mman);
   // test
   TEST( 0 == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
   // check mman
   TEST( 1 == mman.refcount);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (2*sizeof(empty_transition_t));
   TEST( S == sizeallocated_automatmman(&mman));
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa2
   TEST( 0 == ndfa2.mman);
   // check ndfa
   TEST( &mman == ndfa.mman);
   TEST( 8     == ndfa.nrstate);
   TEST( 0     == isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 2 }, 0, 0 }; // ndfa
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 4 }, 0, 0 }; // ndfa1
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
   helperstate[4] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
   helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 7 }, 0, 0 }; // ndfa2
   helperstate[6] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[7] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 6 }, (char32_t[]) { 2 }, (char32_t[]) { 2 } };
   TEST(0 == helper_compare_states(&ndfa, 8, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));

   // TEST initrepeat_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(1 == mman.refcount);
   S = sizeallocated_automatmman(&mman);
   // test
   TEST( 0 == initrepeat_automat(&ndfa, &ndfa1));
   // check mman
   TEST( 1 == mman.refcount);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (3*sizeof(empty_transition_t));
   TEST( S == sizeallocated_automatmman(&mman));
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa
   TEST( &mman == ndfa.mman);
   TEST( 5     == ndfa.nrstate);
   TEST( 0     == isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 2, 1 }, 0, 0 }; // ndfa
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 4 }, 0, 0 }; // ndfa1
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 0 }, 0, 0 };
   helperstate[4] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
   TEST(0 == helper_compare_states(&ndfa, 5, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));

   // TEST initor_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(0 == initmatch_automat(&ndfa2, &mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
   TEST(2 == mman.refcount);
   S = sizeallocated_automatmman(&mman);
   // test
   TEST( 0 == initor_automat(&ndfa, &ndfa1, &ndfa2));
   // check mman
   TEST( 1 == mman.refcount);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (3*sizeof(empty_transition_t));
   TEST( S == sizeallocated_automatmman(&mman));
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa2
   TEST( 0 == ndfa2.mman);
   // check ndfa
   TEST( &mman == ndfa.mman);
   TEST( 8     == ndfa.nrstate);
   TEST( 0     == isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 2, 5 }, 0, 0 }; // ndfa
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 4 }, 0, 0 }; // ndfa1
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[4] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
   helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 7 }, 0, 0 }; // ndfa2
   helperstate[6] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[7] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 6 }, (char32_t[]) { 2 }, (char32_t[]) { 2 } };
   TEST(0 == helper_compare_states(&ndfa, 8, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automatmman(&mman));

   // === simulated ERROR / EINVAL

   for (unsigned err = 13; err < 15; ++err) {
      for (unsigned tc = 0, isDone = 0; !isDone; ++tc) {
         // prepare
         switch (tc) {
         case 0:  // TEST initmatch_automat: simulated ERROR
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  TEST( (int)err == initmatch_automat(&ndfa, &mman, 1, from, to));
                  break;
         case 1:  // TEST initsequence_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  mman.freesize = 0; // ==> new allocation needed ==> error
                  TEST( (int)err == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  // reset
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 2:  // TEST initor_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  mman.freesize = 0; // ==> new allocation needed ==> error
                  TEST( (int)err == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  // reset
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 3:  // TEST initsequence_automat: EINVAL empty ndfa
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  break;
         case 4:  // TEST initrepeat_automat: EINVAL empty ndfa
                  TEST( EINVAL == initrepeat_automat(&ndfa, &ndfa1));
                  break;
         case 5:  // TEST initor_automat: EINVAL empty ndfa
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  break;
         case 6:  // TEST initsequence_automat: EINVAL (sequence of same automat)
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa1));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 7:  // TEST initor_automat: EINVAL (sequence of same automat)
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa1));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 8:  // TEST initsequence_automat: EINVAL (different mman)
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman2, 1, from+2, to+2));
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  TEST(0 == free_automatmman(&mman2));
                  break;
         case 9:  // TEST initor_automat: EINVAL (different mman)
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman2, 1, from+2, to+2));
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  TEST(0 == free_automatmman(&mman2));
                  break;
         default: isDone = 1;
                  break;
         }
         if ( !isempty_slist(&mman.pagecache)) {
            TEST(last_pagelist(&mman.pagecache) == first_pagelist(&mman.pagecache));
            TEST(0 == delete_memorypage(last_pagelist(&mman.pagecache)));
            mman.pagecache = (slist_t) slist_INIT;
         }
         // check mman
         TEST( isempty_slist(&mman.pagelist));
         TEST( isempty_slist(&mman.pagecache));
         TEST( 0 == mman.refcount);
         TEST( 0 == mman.freemem);
         TEST( 0 == mman.freesize);
         // check ndfa
         TEST( 0 == ndfa.mman);
         TEST( 0 == ndfa.nrstate);
         TEST( 1 == isempty_slist(&ndfa.states));
      }
   }

   return 0;
ONERR:
   free_automatmman(&mman);
   free_automatmman(&mman2);
   return EINVAL;
}

static int test_query(void)
{
   automat_t ndfa = automat_FREE;

   // TEST nrstate_automat: automat_FREE
   TEST(0 == nrstate_automat(&ndfa));

   // TEST nrstate_automat: returns value of nrstate
   for (size_t i = 1; i; i <<= 1) {
      ndfa.nrstate = i;
      TEST(i == nrstate_automat(&ndfa));
   }

   for (unsigned l = 0; l < 3; ++l) {
      // prepare
      state_t states[3];
      for (unsigned i = 0; i < l; ++i) {
         insertlast_statelist(&ndfa.states, &states[i]);
      }

      // TEST startstate_automat: empty statelist
      TEST((l ? &states[0] : 0) == startstate_automat(&ndfa));

      // TEST endstate_automat: empty statelist
      TEST((l ? &states[(l>1)] : 0) == endstate_automat(&ndfa));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   automat_t      ndfa = automat_FREE;
   automat_t      ndfa2 = automat_FREE;
   automat_mman_t mman = automat_mman_INIT;
   char32_t       from[256];
   char32_t       to[256];
   size_t         target[255];
   helper_state_t helperstate[3+255];

   // prepare
   for (unsigned i = 0; i < lengthof(target); ++i) {
      target[i] = 1;
   }
   for (unsigned i = 0; i < lengthof(from); ++i) {
      from[i] = 1 + i;
      to[i]   = 1 + 2*i;
   }
   TEST(0 == initmatch_automat(&ndfa, &mman, 15, from, to));
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 2 }, 0, 0 };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_RANGE, 15, target, from, to };
   TEST(0 == helper_compare_states(&ndfa, 3, helperstate))

   // TEST addmatch_automat: EINVAL (freed ndfa)
   TEST( EINVAL == addmatch_automat(&ndfa2, 1, from, to));

   // TEST addmatch_automat: EINVAL (nrmatch == 0)
   TEST( EINVAL == addmatch_automat(&ndfa, 0, from, to));

   // TEST addmatch_automat
   size_t S = sizeallocated_automatmman(&mman);
   for (unsigned i = 1; i < lengthof(from); ++i) {
      TEST( 0 == addmatch_automat(&ndfa, (uint8_t)i, from, to))
      // check mman
      S += state_SIZE + i * sizeof(range_transition_t);
      TEST( 1 == mman.refcount);
      TEST( S == sizeallocated_automatmman(&mman));
      // check ndfa
      TEST( &mman == ndfa.mman);
      TEST( 3+i   == ndfa.nrstate);
      TEST( 0     == isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[2+i] = (helper_state_t) { state_RANGE_CONTINUE, (uint8_t) i, target, from, to };
      TESTP( 0 == helper_compare_states(&ndfa, ndfa.nrstate, helperstate), "i:%d", i);
   }

   // reset
   TEST(0 == free_automatmman(&mman));

   return 0;
ONERR:
   free_automatmman(&mman);
   return EINVAL;
}

int unittest_proglang_automat()
{
   if (test_memorypage())  goto ONERR;
   if (test_automatmman()) goto ONERR;
   if (test_state())       goto ONERR;
   if (test_multistate())  goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_update())      goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

int main(void)
{
   printf("RUN unittest_proglang_automat\n");
   if (unittest_proglang_automat()) {
      printf("RUN unittest_proglang_automat: *** ERROR ***\n");
   } else {
      printf("RUN unittest_proglang_automat: *** OK ***\n");
   }
   return 0;
}
#endif
