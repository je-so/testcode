/* title: FiniteStateMachine impl

   Implements <FiniteStateMachine>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/

// Compile with: gcc -oatm -DKONFIG_UNITTEST -std=gnu99 automat.c automat_mman.c  
// Run with: ./atm

#include "config.h"
#include "automat.h"
#include "automat_mman.h"
#include "foreach.h"
#include "test_errortimer.h"

typedef uint32_t char32_t;

// === private types
struct memory_page_t;
struct range_transition_t;
union  transition_t;
struct state_t;
struct depthstack_entry_t;
struct depthstack_t;
struct multistate_node_t;
struct multistate_t;
struct range_t;
struct rangemap_node_t;
struct rangemap_t;

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
   state_RANGE_CONTINUE // extends previous state_EMPTY or state_RANGE with addtional up to 255 range transitions
} state_e;


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


typedef struct depthstack_entry_t {
   void *   parent;
   unsigned ichild;
} depthstack_entry_t;

typedef struct depthstack_t {
   size_t             depth;
   depthstack_entry_t entry[bitsof(size_t)];
} depthstack_t;

// group: lifetime

static inline void init_depthstack(depthstack_t * stack)
{
   stack->depth = 0;
}

// group: update

static inline void push_depthstack(depthstack_t * stack, void * node, unsigned ichild)
{
   stack->entry[stack->depth] = (depthstack_entry_t) { node, ichild };
   ++ stack->depth;
}


/* struct: multistate_node_t
 * Der Datenknoten eines <multistate_t> B-Baumes. */
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

// group: lifetime

#define multistate_INIT \
         { 0, 0 }

// group: query

// TODO:

// group: update

// returns:
// ENOMEM - Data structure may be corrupt due to split operation (do not use it afterwards)
static int add_multistate(multistate_t * mst, struct automat_mman_t * mman, /*in*/state_t * state)
{
   int err;
   uint16_t const SIZE = sizeof(multistate_node_t);
   void * node;

   // === 3 cases ===
   // 1: 2 <= mst->size ==> insert into leaf (general case)
   // 2: 0 == mst->size ==> store state into root pointer (special case)
   // 3: 1 == mst->size ==> allocate leaf and store root + state into leaf (special case)

   if (1 < mst->size) {
      // === case 1 ===
      // search from mst->root node to leaf and store search path in stack
      depthstack_t   stack;
      init_depthstack(&stack);
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
               low = mid + 1; // low <= high (key[x] is first entry of leaf reachable from child[x+1])
            } else {
               high = mid;
            }
            if (low == high) break;
         }
         push_depthstack(&stack, node, low);
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
         // search state in node->state[]
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
         state_t * key2 = ((multistate_node_t*)node2)->state[0];
         while (stack.depth) {
            node = stack.entry[--stack.depth].parent;
            low  = stack.entry[stack.depth].ichild;
            if (((multistate_node_t*)node)->size < multistate_NROFCHILD) {
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
                  state_t ** node_key  = &((multistate_node_t*)node)->key[multistate_NROFCHILD-1];
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


/* struct: rangemap_node_t
 * Der Datenknoten eines <rangemap_t> B-Baumes. */
typedef struct rangemap_node_t {
   uint8_t level;
   uint8_t size;
   union {
      struct { // level > 0  (inner node)
         char32_t key[19]; // key[i] == child[i+1]->child[/*(level > 0)*/0]->...->range.from[/*(level == 0)*/0]
         struct rangemap_node_t * child[20];
      };
      struct { // level == 0 (leaf node)
         struct rangemap_node_t * next;
         range_t range[10];
      };
   };
} rangemap_node_t;


/* struct: rangemap_t
 * Eine Map (B-Tree), der range_t auf multistate_t abbildet.
 * Wird für Optimierung des Automaten in einen DFA benötigt. */
typedef struct rangemap_t {
   size_t            size;
   rangemap_node_t * root;
} rangemap_t;

// group: constants

/* define: rangemap_NROFRANGE
 * Nr of ranges array <rangemap_node_t->range> could store. */
#define rangemap_NROFRANGE \
         (lengthof(((rangemap_node_t*)0)->range))

/* define: rangemap_NROFCHILD
 * Nr of pointer array <rangemap_node_t->child> could store. */
#define rangemap_NROFCHILD \
         (lengthof(((rangemap_node_t*)0)->child))

// group: lifetime

/* define: rangemap_INIT
 * Initialisiert eine <rangemap_t>. */
#define rangemap_INIT \
         { 0, (void*)0 }

// group: query

// TODO:

// group: update

// returns:
// ENOMEM - Data structure may be corrupt due to split operation (do not use it afterwards)
// EAGAIN - Work is done for range [from..next_from-1] but function has to be
//          called again with range set to [next_from..to]
static int addrange2_rangemap(rangemap_t * rmap, automat_mman_t * mman, char32_t from, char32_t to, /*err*/char32_t * next_from)
{
   int err;
   uint16_t const SIZE = sizeof(rangemap_node_t);
   char32_t nextFrom = to + 1;
   bool  isNextFrom = false;
   void* memblock;
   rangemap_node_t * node;

   // === 2 cases ===
   // 1: 0 == mst->size ==> allocate root leaf and store range into leaf
   // 2: 1 <= mst->size ==> insert into leaf (general case)

   if (! rmap->size) {
      // === case 1
      err = allocmem_automatmman(mman, SIZE, &memblock);
      if (err) goto ONERR;
      node = memblock;
      node->level = 0;
      node->size  = 1;
      node->next  = 0;
      node->range[0] = (range_t) range_INIT(from, to);
      rmap->root = node;

   } else {
      // === case 2
      depthstack_t   stack;
      init_depthstack(&stack);
      node = rmap->root;
      if (node->level >= (int) lengthof(stack.entry)) {
         return EINVARIANT;
      }
      for (unsigned level = node->level; (level--) > 0; ) {
         if (node->size > rangemap_NROFCHILD || node->size < 2) return EINVARIANT;
         unsigned high = node->size -1u; // size >= 2 ==> high >= 1
         unsigned low  = 0; // low < high cause high >= 1
         // node->child[0..high] are valid
         // node->key[0..high-1] are valid
         for (unsigned mid = high / 2u; /*low < high*/; mid = (high + low) / 2u) {
            // search from in node->key
            if (node->key[mid] <= from) {
               low = mid + 1; // low <= high (key[x] is first entry of leaf reachable from child[x+1])
            } else {
               high = mid;
            }
            if (low == high) break;
         }
         if (low < node->size-1u && node->key[low] <= to) {
            isNextFrom = true;
            nextFrom = node->key[low];
            to = node->key[low]-1;
         }
         push_depthstack(&stack, node, low);
         node = node->child[low];
         if (node->level != level) return EINVARIANT;
      }
      if (node->size > rangemap_NROFRANGE) return EINVARIANT;
      // find range in leaf node. The index »low« is the offset where «from..to» has to be inserted !
      unsigned high = node->size;
      unsigned low  = 0; // 0 <= low <= node->size
      for (unsigned mid = high / 2u; low < high; mid = (high + low) / 2u) {
         if (node->range[mid].to < from) {
            low = mid + 1;
         } else {
            high = mid;
         }
      }
      while (low < node->size && node->range[low].from <= from) {
         if (node->range[low].from == from) {
            if (to >= node->range[low].to) {
               if (to == node->range[low].to) goto DONE_NO_INSERT;
               from = node->range[low].to+1; // from <= to
               ++low; // low <= node->size
            } else {
               // split node->range[low] insert from..to
               node->range[low].from = to+1; // to+1 <= [].to ==> [].from <= [].to
               goto SKIP_2nd_CHECK;
            }
         } else {
            // split node->range[low] insert [].from..from
            isNextFrom = true;
            nextFrom = from;
            from = node->range[low].from;
            to = nextFrom-1;
            node->range[low].from = nextFrom; // from <= [].to ==> [].from <= [].to
            goto SKIP_2nd_CHECK;
         }
      }
      if (low < node->size && node->range[low].from <= to) {
         isNextFrom = true;
         nextFrom = node->range[low].from;
         to = node->range[low].from-1; // from < node->range[low].from ==> from <= to
      }
      SKIP_2nd_CHECK:
      if (node->size < rangemap_NROFRANGE) {
         // insert into leaf
         for (unsigned i = node->size; i > low; --i) {
            node->range[i] = node->range[i-1];
         }
         node->range[low] = (range_t) range_INIT(from, to);
         ++ node->size;
      } else {
         // split leaf
         rangemap_node_t * node2;
         {
            err = allocmem_automatmman(mman, SIZE, &memblock);
            if (err) goto ONERR;
            node2 = memblock;
            const unsigned NODE2_SIZE = ((rangemap_NROFRANGE+1) / 2);
            const unsigned NODE_SIZE  = ((rangemap_NROFRANGE+1) - NODE2_SIZE);
            node2->level = 0;
            node2->size  = (uint8_t) NODE2_SIZE;
            node2->next  = node->next;
            node->size   = (uint8_t) NODE_SIZE;
            node->next   = node2;
            // low <  rangemap_NROFRANGE
            // ==> node->range[0], ..., from..to, node->range[low], ..., node->range[rangemap_NROFRANGE-1]
            // low == rangemap_NROFRANGE
            // ==> node->range[0], ..., node->range[rangemap_NROFRANGE-1], from..to
            if (low < NODE_SIZE) {
               range_t * node_range  = &node->range[rangemap_NROFRANGE];
               range_t * node2_range = &node2->range[NODE2_SIZE];
               for (unsigned i = NODE2_SIZE; i; --i) {
                  *(--node2_range) = *(--node_range);
               }
               for (unsigned i = NODE_SIZE-1-low; i > 0; --i, --node_range) {
                  *node_range = node_range[-1];
               }
               *node_range = (range_t) range_INIT(from, to);
            } else {
               range_t * node_range  = &node->range[NODE_SIZE];
               range_t * node2_range = &node2->range[0];
               for (unsigned i = low-NODE_SIZE; i; --i, ++node_range, ++node2_range) {
                  *node2_range = *node_range;
               }
               *node2_range++ = (range_t) range_INIT(from, to);
               for (unsigned i = rangemap_NROFRANGE-low; i; --i, ++node_range, ++node2_range) {
                  *node2_range = *node_range;
               }
            }
         }
         // insert node2 into parent
         char32_t key2 = node2->range[0].from;
         while (stack.depth) {
            node = stack.entry[--stack.depth].parent;
            low  = stack.entry[stack.depth].ichild;
            if (node->size < rangemap_NROFCHILD) {
               // insert into parent (node)
               for (unsigned i = node->size-1u; i > low; --i) {
                  node->key[i] = node->key[i-1];
               }
               node->key[low] = key2;
               ++ low;
               for (unsigned i = node->size; i > low; --i) {
                  node->child[i] = node->child[i-1];
               }
               node->child[low] = node2;
               ++ node->size;
               goto DONE_INSERT;
            } else {
               // split node
               rangemap_node_t * child = node2;
               char32_t      child_key = key2;
               // ENOMEM ==> information in child(former node2) is lost !! (corrupt data structure)
               err = allocmem_automatmman(mman, SIZE, &memblock);
               if (err) goto ONERR;
               node2 = memblock;
               const unsigned NODE2_SIZE = ((rangemap_NROFCHILD+1) / 2);
               const unsigned NODE_SIZE  = ((rangemap_NROFCHILD+1) - NODE2_SIZE);
               node2->level = node->level;
               node2->size  = (uint8_t) NODE2_SIZE;
               node->size   = (uint8_t) NODE_SIZE;
               if (low + 1 < NODE_SIZE) {
                  char32_t * node_key  = &node->key[rangemap_NROFCHILD-1];
                  char32_t * node2_key = &node2->key[NODE2_SIZE-1];
                  rangemap_node_t ** node2_child = &node2->child[NODE2_SIZE];
                  rangemap_node_t ** node_child  = &node->child[rangemap_NROFCHILD];
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
                  char32_t * node_key  = &node->key[NODE_SIZE-1];
                  char32_t * node2_key = &node2->key[0];
                  rangemap_node_t ** node_child  = &node->child[NODE_SIZE];
                  rangemap_node_t ** node2_child = &node2->child[0];
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
                  for (unsigned i = (rangemap_NROFCHILD-1)-low; i; --i, ++node_child, ++node2_child) {
                     *node2_key = *node_key;
                     *node2_child = *node_child;
                  }
               }
            }
         }

         // increment depth by 1: alloc root node pointing to child and split child
         rangemap_node_t * root;
         // ENOMEM ==> information in node2 is lost !! (corrupt data structure)
         err = allocmem_automatmman(mman, SIZE, &memblock);
         if (err) goto ONERR;
         root = memblock;
         root->level = (uint8_t) (node2->level + 1);
         root->size  = 2;
         root->key[0]   = key2;
         root->child[0] = node;
         root->child[1] = node2;
         rmap->root = root;
      }
   }

DONE_INSERT:
   ++ rmap->size;

DONE_NO_INSERT:
   if (isNextFrom) *next_from = nextFrom;
   return isNextFrom ? EAGAIN : 0;
ONERR:
   return err;
}

static int addrange_rangemap(rangemap_t * rmap, automat_mman_t * mman, char32_t from, char32_t to)
{
   int err;
   char32_t next_from = from;

   if (from > to) { err = EINVAL; goto ONERR; }

   for (;;) {
      err = addrange2_rangemap(rmap, mman, next_from, to, &next_from);
      if (! err) break;
      if (err != EAGAIN) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}


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
   int err;
   if (ndfa->mman) {
      err = 0;
      incrwasted_automatmman(ndfa->mman, ndfa->allocated);
      if (0 == decruse_automatmman(ndfa->mman)) {
         if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
            err = delete_automatmman(&ndfa->mman);
         }
      }
      *ndfa = (automat_t) automat_FREE;

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int initempty_automat(/*out*/automat_t* ndfa, struct automat_t* use_mman)
{
   int err;
   void * startstate;
   automat_mman_t * mman;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + 2*state_SIZE_EMPTYTRANS(1));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = allocmem_automatmman(mman, SIZE, &startstate);
   }
   if (err) goto ONERR;

   state_t * endstate = (void*) ((uint8_t*)startstate + 1*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   initempty_state(startstate, endstate);
   initempty_state(endstate, endstate);

   // set out
   incruse_automatmman(mman);
   ndfa->mman = mman;
   ndfa->nrstate = 2;
   ndfa->allocated = SIZE;
   initsingle_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   return 0;
ONERR:
   if (! use_mman) {
      delete_automatmman(&mman);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initmatch_automat(/*out*/automat_t* ndfa, struct automat_t* use_mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   int err;
   void * startstate;
   automat_mman_t * mman;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (3*state_SIZE + 2*state_SIZE_EMPTYTRANS(1) + 1*state_SIZE_RANGETRANS(nrmatch));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = allocmem_automatmman(mman, SIZE, &startstate);
   }
   if (err) goto ONERR;

   state_t * endstate   = (void*) ((uint8_t*)startstate + 1*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t * matchstate = (void*) ((uint8_t*)startstate + 2*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   initempty_state(startstate, matchstate);
   initempty_state(endstate, endstate);
   initrange_state(matchstate, endstate, nrmatch, match_from, match_to);

   // set out
   incruse_automatmman(mman);
   ndfa->mman = mman;
   ndfa->nrstate = 3;
   ndfa->allocated = SIZE;
   initsingle_statelist(&ndfa->states, matchstate);
   insertfirst_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   return 0;
ONERR:
   if (! use_mman) {
      delete_automatmman(&mman);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

// TODO: int initcopy_automat(/*out*/automat_t* dest_ndfa, automat_t* src_ndfa, const automat_t* use_mman_from);
// TODO: implement
// TODO: TEST

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
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = allocmem_automatmman(ndfa1->mman, SIZE, &startstate);
   }
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
   ndfa->allocated = SIZE + ndfa1->allocated + ndfa2->allocated;
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
   ndfa->allocated = SIZE + ndfa1->allocated;
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
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = allocmem_automatmman(ndfa1->mman, SIZE, &startstate);
   }
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
   ndfa->allocated = SIZE + ndfa1->allocated + ndfa2->allocated;
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

int extendmatch_automat(automat_t* ndfa, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   int err;
   void * matchstate;

   if (ndfa->nrstate < 2 || nrmatch == 0) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (state_SIZE + state_SIZE_RANGETRANS(nrmatch));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = allocmem_automatmman(ndfa->mman, SIZE, &matchstate);
   }
   if (err) goto ONERR;

   state_t * startstate = first_statelist(&ndfa->states);
   state_t * endstate = next_statelist(startstate);

   initcontinue_state(matchstate, endstate, nrmatch, match_from, match_to);

   // set out
   ndfa->nrstate += 1;
   ndfa->allocated += SIZE;
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

static int build_recursive(
   /*out*/rangemap_node_t ** root,
   automat_mman_t * mman,
   unsigned keyoffset,
   unsigned level,
   unsigned interval_per_child,
   /*inout*/rangemap_node_t ** child)
{
   void * node;
   const uint16_t SIZE = sizeof(rangemap_node_t);
   rangemap_node_t * parent;
   TEST(level >= 1);

   TEST(0 == allocmem_automatmman(mman, SIZE, &node));
   *root  = node;
   parent = node;
   parent->level = (uint8_t) level;
   parent->size  = 4;

   if (level > 1) {
      for (unsigned i = 0; i < 4; ++i) {
         unsigned keyoffset2 = (1u<<(2*(level-1))) * i * interval_per_child;
         if (i) parent->key[i-1] = keyoffset + keyoffset2;
         TEST(0 == build_recursive(&parent->child[i], mman, keyoffset + keyoffset2, level-1, interval_per_child, child));
      }
   } else {
      for (unsigned i = 0; i < 4; ++i) {
         if (i) parent->key[i-1] = keyoffset + i * interval_per_child;
         parent->child[i] = *child;
         *child = (*child)->next;
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

/* function: build_rangemap
 * Builds level b-tree.
 * Every node has 4 childs and a leaf a single range entry copied from from[], to[].
 * The interval_per_child is the total interval every leaf is considered
 * to occupy. This value is used to build the key values in the node entries. */
static int build_rangemap(
   /*out*/rangemap_t * rmap,
   automat_mman_t * mman,
   unsigned level,
   unsigned from[1<<(2*level)],
   unsigned to[1<<(2*level)],
   unsigned interval_per_child,
   /*out*/rangemap_node_t ** first_child)
{
   void * node;
   const uint16_t SIZE = sizeof(rangemap_node_t);
   const unsigned S = 1u<<(2*level);

   rangemap_node_t * child;
   rangemap_node_t * prev_child = 0;
   for (unsigned i = 0; i < S; ++i, prev_child = child) {
      TEST(0 == allocmem_automatmman(mman, SIZE, &node));
      child = node;
      if (!i) *first_child = child;
      else    prev_child->next = child;
      child->level = 0;
      child->size  = 1;
      child->next  = 0;
      child->range[0] = (range_t) range_INIT(from[i], to[i]);
      prev_child = child;
   }

   child = *first_child;
   TEST(0 == build_recursive(&rmap->root, mman, 0, level, interval_per_child, &child));
   TEST(0 == child);
   rmap->size = S;

   return 0;
ONERR:
   return EINVAL;
}

/* function: build1_rangemap
 * Build level 1 b-tree (root + nrchild leaves).
 *
 * Memory Layout:
 * addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... */
static int build1_rangemap(
   /*out*/rangemap_t * rmap,
   automat_mman_t * mman,
   unsigned range_width,
   void * ENDMARKER,
   unsigned nrchild,
   /*out*/void* addr[nrchild+2],
   /*out*/rangemap_node_t* child[nrchild])
{
   void * node;
   const uint16_t SIZE = sizeof(rangemap_node_t);
   TEST(range_width >= 1);
   TEST(nrchild >= 2);
   TEST(nrchild <= rangemap_NROFCHILD);

   TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
   TEST(0 == allocmem_automatmman(mman, SIZE, &node));
   rmap->root = node;
   rmap->size = nrchild * rangemap_NROFRANGE;
   TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
   rmap->root->level = 1;
   rmap->root->size  = (uint8_t) nrchild;

   for (unsigned i = 0, f = 0; i < nrchild; ++i) {
      TEST(0 == allocmem_automatmman(mman, SIZE, &node));
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[2+i]));
      child[i] = node;
      if (i) {
         rmap->root->key[i-1] = f;
         child[i-1]->next = child[i];
      }
      rmap->root->child[i] = child[i];
      child[i]->level = 0;
      child[i]->size  = rangemap_NROFRANGE;
      child[i]->next  = 0;
      for (unsigned r = 0; r < rangemap_NROFRANGE; ++r, f += range_width) {
         child[i]->range[r] = (range_t) range_INIT(f, f + range_width-1);
      }
   }

   for (unsigned i = 0; i < nrchild+2; ++i) {
      *((void**)addr[i]) = ENDMARKER;
   }

   return 0;
ONERR:
   return EINVAL;
}

/* function: build2_rangemap
 * Build level 2 b-tree (root + nrchild nodes + full leaves).
 * Inserted ranges are generated beginning from 0 up to
 * (nrchild*range_width*rangemap_NROFCHILD*rangemap_NROFRANGE). */
static int build2_rangemap(
   /*out*/rangemap_t * rmap,
   automat_mman_t * mman,
   unsigned range_width,
   unsigned nrchild,
   /*out*/rangemap_node_t* child[nrchild])
{
   void * node;
   const uint16_t SIZE = sizeof(rangemap_node_t);
   size_t level1_size = rangemap_NROFCHILD*rangemap_NROFRANGE;
   TEST(nrchild >= 2);
   TEST(nrchild <= rangemap_NROFCHILD);

   TEST(0 == allocmem_automatmman(mman, SIZE, &node));
   rmap->root = node;
   rmap->size = nrchild * level1_size;
   rmap->root->level = 2;
   rmap->root->size  = (uint8_t) nrchild;

   for (unsigned i = 0; i < nrchild; ++i) {
      TEST(0 == allocmem_automatmman(mman, SIZE, &node));
      child[i] = node;
      if (i) rmap->root->key[i-1] = (char32_t) (i*range_width*level1_size);
      rmap->root->child[i] = child[i];
      child[i]->level = 1;
      child[i]->size  = rangemap_NROFCHILD;
   }

   unsigned f = 0;
   rangemap_node_t * prevleaf = 0;
   for (unsigned i = 0; i < nrchild; ++i) {
      for (unsigned c = 0; c < rangemap_NROFCHILD; ++c) {
         TEST(0 == allocmem_automatmman(mman, SIZE, &node));
         rangemap_node_t * leaf = node;
         if (c) child[i]->key[c-1] = f;
         child[i]->child[c] = leaf;
         leaf->level = 0;
         leaf->size  = rangemap_NROFRANGE;
         leaf->next  = 0;
         for (unsigned r = 0; r < rangemap_NROFRANGE; ++r, f += range_width) {
            leaf->range[r] = (range_t) range_INIT(f, f+range_width-1);
         }
         if (prevleaf) prevleaf->next = leaf;
         prevleaf = leaf;
      }
   }

   return 0;
ONERR:
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
   automat_mman_t* mman = 0;
   void * const   ENDMARKER = (void*) (uintptr_t) 0x01234567;
   const unsigned LEVEL1_NROFSTATE = multistate_NROFSTATE * multistate_NROFCHILD;

   static_assert( 2*LEVEL1_NROFSTATE <= NROFSTATE,
                  "state array big enough to build root node with all possible number of leaves"
                  "and also supports interleaving (only every 2nd state to support inserting in between)"
                  );

   // prepare
   memset(&state, 0, sizeof(state));
   TEST(0 == new_automatmman(&mman));

   // TEST multistate_NROFSTATE
   TEST( lengthof(((multistate_node_t*)0)->state) == multistate_NROFSTATE);

   // TEST multistate_NROFCHILD
   TEST( lengthof(((multistate_node_t*)0)->child) == multistate_NROFCHILD);

   // TEST multistate_INIT
   TEST(0 == mst.size);
   TEST(0 == mst.root);

   // TEST add_multistate: multistate_t.size == 0
   for (unsigned i = 0; i < NROFSTATE; ++i) {
      mst = (multistate_t) multistate_INIT;
      TEST( 0 == add_multistate(&mst, mman, &state[i]));
      // check mman: nothing allocated
      TEST( 0 == sizeallocated_automatmman(mman));
      // check mst
      TEST( 1 == mst.size);
      TEST( &state[i] == mst.root);
   }

   // TEST add_multistate: multistate_t.size == 1
   for (unsigned i = 0; i < NROFSTATE-1; ++i) {
      for (unsigned order = 0; order <= 1; ++order) {
         // prepare
         mst = (multistate_t) multistate_INIT;
         TEST(0 == add_multistate(&mst, mman, &state[i + !order]));
         // test
         TEST( 0 == add_multistate(&mst, mman, &state[i + order]));
         // check mman
         TEST( sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
         // check mst
         TEST( 2 == mst.size);
         TEST( 0 != mst.root);
         // check mst.root content
         TEST( 0 == ((multistate_node_t*)mst.root)->level);
         TEST( 2 == ((multistate_node_t*)mst.root)->size);
         TEST( &state[i] == ((multistate_node_t*)mst.root)->state[0]);
         TEST( &state[i+1] == ((multistate_node_t*)mst.root)->state[1]);
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST add_multistate: single leaf && add states ascending/descending
   for (unsigned asc = 0; asc <= 1; ++asc) {
      void * addr = 0;
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         // test
         TEST( 0 == add_multistate(&mst, mman, &state[asc ? i : multistate_NROFSTATE-1-i]));
         if (1 == i) {  // set end marker at end of node
            TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr));
            *((void**)addr) = ENDMARKER;
            // check that end marker is effective !!
            TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         }
         // check mman
         TEST( (i > 0 ? sizeof(void*)+sizeof(multistate_node_t) : 0) == sizeallocated_automatmman(mman));
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
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST add_multistate: single leaf && add states unordered
   for (unsigned S = 3; S < multistate_NROFSTATE; ++S) {
      for (unsigned pos = 0; pos < S; ++pos) {
         // prepare
         mst = (multistate_t) multistate_INIT;
         for (unsigned i = 0; i < S; ++i) {
            if (i != pos) { TEST(0 == add_multistate(&mst, mman, &state[i])); }
         }
         void * addr = 0;
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr));
         *((void**)addr) = ENDMARKER;
         TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         // test
         TEST( 0 == add_multistate(&mst, mman, &state[pos]));
         // check: mman
         TEST( sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
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
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST add_multistate: single leaf: EEXIST
   {
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         TEST(0 == add_multistate(&mst, mman, &state[i]));
      }
      void * addr = 0;
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr));
      *((void**)addr) = ENDMARKER;
      TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
      for (unsigned i = 0; i < multistate_NROFSTATE; ++i) {
         // test
         TEST( EEXIST == add_multistate(&mst, mman, &state[i]));
         // check mman
         TEST( sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
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
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST add_multistate: split leaf node (level 0) ==> build root (level 1) ==> 3 nodes total
   for (unsigned splitidx = 0; splitidx <= multistate_NROFSTATE; ++splitidx) {
      void * addr[2] = { 0 };
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0, next = 0; i < multistate_NROFSTATE; ++i, ++next) {
         if (next == splitidx) ++next;
         TEST(0 == add_multistate(&mst, mman, &state[next]));
      }
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
      *((void**)addr[1]) = ENDMARKER;
      TEST(addr[1] == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
      TEST(addr[0] == &((void**)mst.root)[-1]);
      // test
      void * oldroot = mst.root;
      TEST( 2*sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
      TEST( 0 == add_multistate(&mst, mman, &state[splitidx]));
      // check: mman
      TEST( 2*sizeof(void*)+3*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
      // check: mst
      TEST( mst.size == multistate_NROFSTATE+1);
      TEST( mst.root != 0);
      TEST( mst.root != oldroot);
      TEST( mst.root == ((uint8_t*)addr[1]) + sizeof(void*) + sizeof(multistate_node_t));
      // check: mst.root content
      TEST( ((multistate_node_t*)mst.root)->level == 1);
      TEST( ((multistate_node_t*)mst.root)->size  == 2);
      TEST( ((multistate_node_t*)mst.root)->child[0] == oldroot);
      TEST( ((multistate_node_t*)mst.root)->child[1] == (multistate_node_t*) (((uint8_t*)addr[1]) + sizeof(void*)));
      TEST( ((multistate_node_t*)mst.root)->key[0]   == ((multistate_node_t*)mst.root)->child[1]->state[0]);
      // check: leaf1 content
      multistate_node_t* leaf1 = ((multistate_node_t*)mst.root)->child[0];
      multistate_node_t* leaf2 = ((multistate_node_t*)mst.root)->child[1];
      TEST( leaf1->level == 0);
      TEST( leaf1->size  == multistate_NROFSTATE/2 + 1);
      TEST( leaf1->next  == leaf2);
      for (unsigned i = 0; i < leaf1->size; ++i) {
         TEST( &state[i] == leaf1->state[i]);
      }
      // check: leaf2 content
      TEST( leaf2->level == 0);
      TEST( leaf2->size  == multistate_NROFSTATE/2);
      TEST( leaf2->next  == 0);
      for (unsigned i = 0; i < leaf2->size; ++i) {
         TEST( &state[leaf1->size+i] == leaf2->state[i]);
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < lengthof(addr); ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST add_multistate: build root node (level 1) + many leafs (level 0) ascending/descending
   for (unsigned desc = 0; desc <= 1; ++desc) {
      void * child[multistate_NROFCHILD] = { 0 };
      void * addr[multistate_NROFCHILD] = { 0 };
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      child[0] = (uint8_t*)addr[0] + sizeof(void*);
      child[1] = (uint8_t*)child[0] + sizeof(multistate_node_t);
      void * root = (uint8_t*)child[1] + sizeof(multistate_node_t);
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < multistate_NROFSTATE+1; ++i) {
         TEST(0 == add_multistate(&mst, mman, &state[desc?NROFSTATE-1-i:i]));
      }
      unsigned SIZE = multistate_NROFSTATE+1;
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
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
            TEST( 0 == add_multistate(&mst, mman, &state[desc?NROFSTATE-1-SIZE:SIZE]));
            if (isSplit) {
               if (desc) memmove(&child[2], &child[1], (nrchild-1)*sizeof(child[0]));
               child[desc?1:nrchild] = (uint8_t*)addr[nrchild-1] + sizeof(void*);
               TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[nrchild]));
               *((void**)addr[nrchild]) = ENDMARKER;
               ++ nrchild;
            }
            // check: mman
            TEST( nrchild*sizeof(void*)+(nrchild+1)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
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
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST add_multistate: build root node (level 1) + many leafs (level 0) unordered
   for (unsigned nrchild=2; nrchild < multistate_NROFCHILD; ++nrchild) {
      for (unsigned pos = 0; pos < nrchild; ++pos) {
         void * child[multistate_NROFCHILD] = { 0 };
         void * addr[multistate_NROFCHILD+1] = { 0 };
         // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild
         TEST(0 == build1_multistate(&mst, mman, state, 2/*every 2nd*/, ENDMARKER, nrchild, addr, child));
         void *   const root = mst.root;
         unsigned const SIZE = nrchild * multistate_NROFSTATE + 1;
         // test add single state ==> split check split
         TEST( 0 == add_multistate(&mst, mman, &state[1+pos*(2*multistate_NROFSTATE)]));
         // check: mman
         TEST( (nrchild+2)*sizeof(void*)+(nrchild+2)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
         // check: mst
         TEST( SIZE == mst.size);
         TEST( root == mst.root);
         // check mst.root content
         TEST( 1         == ((multistate_node_t*)mst.root)->level);
         TEST( nrchild+1 == ((multistate_node_t*)mst.root)->size);
         memmove(&child[pos+2], &child[pos+1], (nrchild-1-pos)*sizeof(child[0]));
         child[pos+1] = (uint8_t*)addr[1+nrchild] + sizeof(void*);
         TEST( child[0] == ((multistate_node_t*)mst.root)->child[0]);
         for (unsigned i = 1; i < nrchild+1; ++i) {
            state_t * key = &state[(i-(i>pos))*(2*multistate_NROFSTATE)+(i==pos+1)*multistate_NROFSTATE];
            TEST( ((multistate_node_t*)mst.root)->key[i-1] == key);
            TEST( ((multistate_node_t*)mst.root)->child[i] == child[i]);
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
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST add_multistate: split root node (level 1)
   for (unsigned pos = 0; pos < multistate_NROFCHILD; ++pos) {
      void * child[multistate_NROFCHILD+1] = { 0 };
      void * addr[multistate_NROFCHILD+2] = { 0 };
      // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild splitroot new-root
      TEST(0 == build1_multistate(&mst, mman, state, 2/*every 2nd*/, ENDMARKER, multistate_NROFCHILD, addr, child));
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
      TEST( 0 == add_multistate(&mst, mman, &state[1+pos*(2*multistate_NROFSTATE)]));
      // check: mman
      TEST( (multistate_NROFCHILD+2)*sizeof(void*)+(multistate_NROFCHILD+4)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
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
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST add_multistate: (level 2) split child(level 1) and add to root
   for (unsigned nrchild=2; nrchild < multistate_NROFCHILD; ++nrchild) {
      for (uintptr_t pos = 0; pos < nrchild; ++pos) {
         void * addr;
         void * child[multistate_NROFCHILD] = { 0 };
         size_t SIZE = nrchild * LEVEL1_NROFSTATE + 1;
         TEST(0 == build2_multistate(&mst, mman, 2, nrchild, child));
         void * root = mst.root;
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr));
         void * splitchild = (uint8_t*)addr + sizeof(void*) + sizeof(multistate_node_t)/*leaf*/;
         // test add single state ==> split check split
         TEST( 0 == add_multistate(&mst, mman, (void*)(1+pos*(2*LEVEL1_NROFSTATE))));
         // check: mman
         TEST( sizeof(void*)+(1+2+nrchild+nrchild*multistate_NROFCHILD)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
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
         TEST( EEXIST == add_multistate(&mst, mman, (void*)(1+pos*(2*LEVEL1_NROFSTATE))));
         for (uintptr_t i = 0; i < LEVEL1_NROFSTATE*nrchild; ++i) {
            TEST(EEXIST == add_multistate(&mst, mman, (void*)(2*i)));
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // reset
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
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

static int test_rangemap(void)
{
   rangemap_t     rmap = rangemap_INIT;
   automat_mman_t*mman = 0;
   void * const   ENDMARKER = (void*) (uintptr_t) 0x01234567;

   // prepare
   TEST(0 == new_automatmman(&mman));

   // TEST rangemap_NROFRANGE
   TEST( lengthof(((rangemap_node_t*)0)->range) == rangemap_NROFRANGE);

   // TEST rangemap_NROFCHILD
   TEST( lengthof(((rangemap_node_t*)0)->child) == rangemap_NROFCHILD);

   // TEST rangemap_INIT
   TEST(0 == rmap.size);
   TEST(0 == rmap.root);

   // TEST addrange_rangemap: EINVAL
   TEST( EINVAL == addrange_rangemap(&rmap, mman, 1, 0));
   TEST( EINVAL == addrange_rangemap(&rmap, mman, UINT32_MAX, 0));

   // TEST addrange_rangemap: multistate_t.size == 0
   for (unsigned from = 0; from < 256; from += 16) {
      for (unsigned to = from; to < 256; to += 32) {
         void * addr[2];
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
         // assumes big blocks + no free block header stored in free memory
         addr[1] = (uint8_t*)addr[0] + sizeof(rangemap_node_t);
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            *(void**)addr[i] = ENDMARKER;
         }
         // test
         rmap = (rangemap_t) rangemap_INIT;
         TEST( 0 == addrange_rangemap(&rmap, mman, from, to));
         // check mman: root allocated
         TEST( sizeof(void*) + sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check rmap
         TEST( rmap.size == 1);
         TEST( rmap.root == (rangemap_node_t*)((uint8_t*)addr[0] + sizeof(void*)));
         // check mst.root content
         TEST( 0 == rmap.root->level);
         TEST( 1 == rmap.root->size);
         TEST( 0 == rmap.root->next);
         TEST( from == rmap.root->range[0].from);
         TEST( to == rmap.root->range[0].to);
         TEST( 0  == rmap.root->range[0].state.size);
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: insert non-overlapping ranges into single node
   for (unsigned S = 2; S <= rangemap_NROFRANGE; ++S) {
      for (unsigned pos = 0; pos < S; ++pos) {
         // prepare
         void * addr[2];
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < S; ++i) {
            if (i == pos) continue;
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)i, (char32_t)i));
         }
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
         *(void**)addr[1] = ENDMARKER;
         // test
         TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)pos, (char32_t)pos));
         // check mman: only root allocated
         TEST( 2*sizeof(void*) + sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check rmap
         TEST( rmap.size == S);
         TEST( rmap.root == (rangemap_node_t*)((uint8_t*)addr[0] + sizeof(void*)));
         // check mst.root content
         TEST( 0 == rmap.root->level);
         TEST( S == rmap.root->size);
         TEST( 0 == rmap.root->next);
         for (unsigned i = 0; i < S; ++i) {
            TEST( i == rmap.root->range[i].from);
            TEST( i == rmap.root->range[i].to);
            TEST( 0 == rmap.root->range[i].state.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: insert range overlapping with ranges and holes
   for (unsigned S = 1; S <= rangemap_NROFRANGE/2-1; ++S) {
      for (unsigned from = 0; from <= 2*S; ++from) {
         for (unsigned to = from; to <= 2*S; ++to) {
            // prepare
            void * addr[2];
            TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
            *(void**)addr[0] = ENDMARKER;
            rmap = (rangemap_t) rangemap_INIT;
            for (unsigned i = 0; i < S; ++i) {
               TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(1+2*i), (char32_t)(1+2*i)));
            }
            TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
            *(void**)addr[1] = ENDMARKER;
            // test
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)from, (char32_t)to));
            // check mman
            TEST( 2*sizeof(void*) + sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
            // check rmap
            unsigned const D = to-from+1;
            unsigned const S2 = S + D/2 + (D&1?!(from&1):0);
            TESTP( rmap.size == S2, "rmap.size:%zd S2:%d S:%d to:%d from:%d", rmap.size, S2, S, to, from);
            TEST( rmap.root == (rangemap_node_t*)((uint8_t*)addr[0] + sizeof(void*)));
            // check mst.root content
            TEST( 0  == rmap.root->level);
            TEST( S2 == rmap.root->size);
            TEST( 0  == rmap.root->next);
            for (unsigned i = 0, N = from < 1 ? from : 1; i < S2; ++i, ++N) {
               N += !(N&1) && (N < from || N > to);
               TESTP( N == rmap.root->range[i].from, "from:%d to:%d N:%d != [%d]:%d", from, to, N, i, rmap.root->range[i].from);
               TEST( N == rmap.root->range[i].to);
               TEST( 0 == rmap.root->range[i].state.size);
            }
            // no overflow to adjacent block
            for (unsigned i = 0; i < lengthof(addr); ++i) {
               TEST( ENDMARKER == *(void**)addr[i]);
            }
            // reset
            TEST(0 == delete_automatmman(&mman));
            TEST(0 == new_automatmman(&mman));
         }
      }
   }

   // TEST addrange_rangemap: insert range fully overlapping with one or more ranges
   for (unsigned from = 0; from < rangemap_NROFRANGE; ++from) {
      for (unsigned to = from; to < rangemap_NROFRANGE; ++to) {
         const unsigned S = rangemap_NROFRANGE;
         // prepare
         void * addr[2];
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < rangemap_NROFRANGE; ++i) {
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(4*i), (char32_t)(3+4*i)));
         }
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
         *(void**)addr[1] = ENDMARKER;
         // test
         TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(4*from), (char32_t)(4*to+3)));
         // check mman: unchanged
         TEST( 2*sizeof(void*) + sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check rmap: unchanged
         TEST( rmap.size == S);
         TEST( rmap.root == (rangemap_node_t*)((uint8_t*)addr[0] + sizeof(void*)));
         // check mst.root content: unchanged
         TEST( 0 == rmap.root->level);
         TEST( S == rmap.root->size);
         TEST( 0 == rmap.root->next);
         for (unsigned i = 0; i < S; ++i) {
            TEST( 4*i   == rmap.root->range[i].from);
            TEST( 4*i+3 == rmap.root->range[i].to);
            TEST( 0 == rmap.root->range[i].state.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: insert range partially overlapping with one or more ranges
   for (unsigned from = 0; from < 3*4; ++from) {
      for (unsigned to = from; to < 3*4; ++to) {
         const unsigned S = 3u + (0 != from % 4) + (3 != to % 4);
         // prepare
         void * addr[2];
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < 3; ++i) {
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(4*i), (char32_t)(3+4*i)));
         }
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
         *(void**)addr[1] = ENDMARKER;
         // test
         TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)from, (char32_t)to));
         // check mman: unchanged
         TEST( 2*sizeof(void*) + sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check rmap: unchanged
         TEST( rmap.size == S);
         TEST( rmap.root == (rangemap_node_t*)((uint8_t*)addr[0] + sizeof(void*)));
         // check mst.root content: unchanged
         TEST( 0 == rmap.root->level);
         TEST( S == rmap.root->size);
         TEST( 0 == rmap.root->next);
         for (unsigned i = 0, f = 0, next_t = 3, t; i < S; ++i, f = t+1, next_t += (t == next_t)?4:0) {
            t = (f < from && from <= next_t) ? from-1 : (f <= to && to < next_t) ? to : next_t;
            TEST( f == rmap.root->range[i].from);
            TEST( t == rmap.root->range[i].to);
            TEST( 0 == rmap.root->range[i].state.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: insert overlapping range in tree with root->level > 0
   for (unsigned level = 1; level <= 4; ++level) {
      // range 0..8 overlaps 3..5
      unsigned const S = 1u<<(2*level);
      unsigned from[256];
      unsigned to[256];
      for (unsigned i = 0; i < S; ++i) {
         from[i] = 3 + 9*i;
         to[i]   = 5 + 9*i;
      }
      rangemap_node_t * child = 0;
      TEST(0 == build_rangemap(&rmap, mman, level, from, to, 9, &child));
      void * const root = rmap.root;
      size_t const MMSIZE = sizeallocated_automatmman(mman);
      TEST(S == rmap.size);
      // test
      TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)0, (char32_t)(S*9-1)));
      // check mman: unchanged
      TEST( MMSIZE == sizeallocated_automatmman(mman));
      // check rmap
      TEST( rmap.size == 3/*two additional range per child node*/*S);
      TEST( rmap.root == root);
      // check chain of child content
      for (unsigned c = 0; c < S; ++c, child = child->next) {
         TEST( 0 != child); // value from child->next
         TEST( 0 == child->level);
         TEST( 3 == child->size);
         for (unsigned i = 0, f = 9*c; i < 3; ++i, f += 3) {
            TEST( f   == child->range[i].from);
            TEST( f+2 == child->range[i].to);
            TEST( 0   == child->range[i].state.size);
         }
      }
      TEST( 0 == child);   // value from child->next
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST addrange_rangemap: insert overlapping range in tree with root->level > 0
   for (unsigned level = 1; level <= 4; ++level) {
      for (unsigned tc = 1; tc <= 2; ++tc) {
         // range 0..9 overlaps 0..4 or 5..9
         unsigned const S = 1u<<(2*level);
         unsigned from[256];
         unsigned to[256];
         for (unsigned i = 0; i < S; ++i) {
            from[i] = (tc==1?0:5) + 10*i;
            to[i]   = (tc==1?4:9) + 10*i;
         }
         rangemap_node_t * child = 0;
         TEST(0 == build_rangemap(&rmap, mman, level, from, to, 10, &child));
         void * const root = rmap.root;
         size_t const MMSIZE = sizeallocated_automatmman(mman);
         TEST(S == rmap.size);
         // test
         TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)0, (char32_t)(S*10-1)));
         // check mman: unchanged
         TEST( MMSIZE == sizeallocated_automatmman(mman));
         // check rmap
         TEST( rmap.size == 2/*additional range per child node*/*S);
         TEST( rmap.root == root);
         // check chain of child content
         for (unsigned c = 0; c < S; ++c, child = child->next) {
            TEST( 0 != child); // value from child->next
            TEST( 0 == child->level);
            TEST( 2 == child->size);
            for (unsigned i = 0, f = 10*c; i < 2; ++i, f += 5) {
               TEST( f   == child->range[i].from);
               TEST( f+4 == child->range[i].to);
               TEST( 0   == child->range[i].state.size);
            }
         }
         TEST( 0 == child);   // value from child->next
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: split leaf node (level 0) ==> build root (level 1) ==> 3 nodes total
   for (unsigned pos = 0; pos <= rangemap_NROFRANGE; ++pos) {
      void * addr[2] = { 0 };
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      // prepare
      rmap = (rangemap_t) rangemap_INIT;
      for (unsigned i = 0; i <= rangemap_NROFRANGE; ++i) {
         if (i == pos) continue;
         TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)i, (char32_t)i));
      }
      TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr[1]));
      *((void**)addr[1]) = ENDMARKER;
      TEST(addr[1] == &rmap.root->range[rangemap_NROFRANGE]);
      TEST(addr[0] == &((void**)rmap.root)[-1]);
      // test
      void * oldroot = rmap.root;
      TEST( 2*sizeof(void*)+sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)pos, (char32_t)pos));
      // check: mman
      TEST( 2*sizeof(void*)+3*sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
      // check: rmap
      TEST( rmap.size == rangemap_NROFRANGE+1);
      TEST( rmap.root != 0);
      TEST( rmap.root != oldroot);
      TEST( rmap.root == (rangemap_node_t*) (((uint8_t*)addr[1]) + sizeof(void*) + sizeof(rangemap_node_t)));
      // check: rmap.root content
      TEST( rmap.root->level == 1);
      TEST( rmap.root->size  == 2);
      TEST( rmap.root->child[0] == oldroot);
      TEST( rmap.root->child[1] == (rangemap_node_t*) (((uint8_t*)addr[1]) + sizeof(void*)));
      TEST( rmap.root->key[0]   == rmap.root->child[1]->range[0].from);
      // check: leaf1 content
      rangemap_node_t* leaf1 = rmap.root->child[0];
      rangemap_node_t* leaf2 = rmap.root->child[1];
      TEST( leaf1->level == 0);
      TEST( leaf1->size  == rangemap_NROFRANGE/2 + 1);
      TEST( leaf1->next  == leaf2);
      for (unsigned i = 0; i < leaf1->size; ++i) {
         TEST( i == leaf1->range[i].from);
         TEST( i == leaf1->range[i].to);
         TEST( 0 == leaf1->range[i].state.size);
      }
      // check: leaf2 content
      TEST( leaf2->level == 0);
      TEST( leaf2->size  == rangemap_NROFRANGE/2);
      TEST( leaf2->next  == 0);
      for (unsigned i = 0, f = leaf1->size; i < leaf2->size; ++i, ++f) {
         TEST( f == leaf2->range[i].from);
         TEST( f == leaf2->range[i].to);
         TEST( 0 == leaf2->range[i].state.size);
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < lengthof(addr); ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST addrange_rangemap: build root node (level 1) + many leafs (level 0) unordered
   for (unsigned nrchild=2; nrchild < rangemap_NROFCHILD; ++nrchild) {
      for (unsigned pos = 0; pos < nrchild; ++pos) {
         rangemap_node_t * child[rangemap_NROFCHILD+1/*last entry is alwys 0*/] = { 0 };
         void * addr[rangemap_NROFCHILD+1] = { 0 };
         // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild
         TEST(0 == build1_rangemap(&rmap, mman, 4/*range-width*/, ENDMARKER, nrchild, addr, child));
         void *   const root = rmap.root;
         unsigned const SIZE = nrchild * rangemap_NROFRANGE + 1;
         // test add single state ==> split check split
         char32_t r = (char32_t) (pos*(4*rangemap_NROFRANGE));
         TEST( 0 == addrange_rangemap(&rmap, mman, r, r+1));
         // check: mman
         TEST( (nrchild+2)*sizeof(void*)+(nrchild+2)*sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check: rmap
         TEST( rmap.size == SIZE);
         TEST( rmap.root == root);
         // check rmap.root content
         TEST( rmap.root->level == 1);
         TEST( rmap.root->size  == nrchild+1);
         memmove(&child[pos+2], &child[pos+1], (nrchild-1-pos)*sizeof(child[0]));
         child[pos+1] = (rangemap_node_t*) ((uint8_t*)addr[1+nrchild] + sizeof(void*));
         TEST( rmap.root->child[0] == child[0]);
         for (unsigned i = 1; i < nrchild+1; ++i) {
            TEST( rmap.root->child[i] == child[i]);
            TEST( rmap.root->key[i-1] == ((i-(i>pos))*(4*rangemap_NROFRANGE)+(i==pos+1)*(2*rangemap_NROFRANGE)));
         }
         // check: leaf content
         for (unsigned i = 0, f=0; i < nrchild+1; ++i) {
            unsigned const S = i == pos ? rangemap_NROFRANGE/2+1 : i == pos+1 ? rangemap_NROFRANGE/2 : rangemap_NROFRANGE;
            TEST( child[i]->level == 0);
            TEST( child[i]->size  == S);
            TEST( child[i]->next  == child[i+1]/*last entry 0*/);
            for (unsigned s = 0, t; s < S; ++s, f = t+1) {
               t = f + (s<2&&i==pos?1:3);
               TEST( child[i]->range[s].from       == f);
               TEST( child[i]->range[s].to         == t);
               TEST( child[i]->range[s].state.size == 0);
            }
         }
         // check: no overflow into surrounding memory
         for (unsigned i = 0; i < nrchild+2; ++i) {
            TEST( ENDMARKER == *((void**)addr[i]));
         }
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // TEST addrange_rangemap: split root node (level 1)
   for (unsigned pos = 0; pos < rangemap_NROFCHILD; ++pos) {
      rangemap_node_t* child[rangemap_NROFCHILD+2/*last entry is 0*/] = { 0 };
      void * addr[rangemap_NROFCHILD+2] = { 0 };
      // addr[0] root addr[1] child[0] addr[2] child[1] addr[3] ... splitchild splitroot new-root
      TEST(0 == build1_rangemap(&rmap, mman, 4/*range-width*/, ENDMARKER, rangemap_NROFCHILD, addr, child));
      void *   const oldroot = rmap.root;
      void *   const splitchild = (uint8_t*)addr[rangemap_NROFCHILD+1] + sizeof(void*);
      void *   const splitroot = (uint8_t*)splitchild + sizeof(rangemap_node_t);
      void *   const root = (uint8_t*)splitroot + sizeof(rangemap_node_t);
      unsigned const SIZE = rangemap_NROFCHILD * rangemap_NROFRANGE + 1;
      char32_t const splitchild_key = (char32_t) (pos*(4*rangemap_NROFRANGE)+2*rangemap_NROFRANGE);
      char32_t const splitroot_key  = (char32_t) (pos < rangemap_NROFCHILD/2 ? (rangemap_NROFCHILD/2)*(4*rangemap_NROFRANGE)
                                    : pos == rangemap_NROFCHILD/2 ? splitchild_key
                                    : (rangemap_NROFCHILD/2+1)*(4*rangemap_NROFRANGE));
      memmove(&child[pos+2], &child[pos+1], (rangemap_NROFCHILD-1-pos)*sizeof(child[0]));
      child[pos+1] = splitchild;
      // test add single state ==> split check split
      char32_t r = (char32_t) (pos*(4*rangemap_NROFRANGE));
      TEST( 0 == addrange_rangemap(&rmap, mman, r, r+1));
      // check: mman
      TEST( (rangemap_NROFCHILD+2)*sizeof(void*)+(rangemap_NROFCHILD+4)*sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
      // check rmap
      TEST( rmap.size == SIZE);
      TEST( rmap.root == root);
      // check rmap.root content
      TEST( rmap.root->level == 2);
      TEST( rmap.root->size  == 2);
      TEST( rmap.root->key[0] == splitroot_key);
      TEST( rmap.root->child[0] == oldroot);
      TEST( rmap.root->child[1] == splitroot);
      // check oldroot / splitroot content
      for (unsigned i = 0, ichild = 0; i < 2; ++i) {
         const unsigned S = rangemap_NROFCHILD/2+1-i;
         TEST( rmap.root->child[i]->level == 1);
         TEST( rmap.root->child[i]->size  == S);
         TEST( rmap.root->child[i]->child[0] == child[ichild++]);
         for (unsigned s = 1; s < S; ++s) {
            TEST( rmap.root->child[i]->key[s-1] == child[ichild]->range[0].from);
            TEST( rmap.root->child[i]->child[s] == child[ichild++]);
         }
      }
      // check: leaf content
      for (unsigned i = 0, f=0; i < rangemap_NROFCHILD+1; ++i) {
         unsigned const S = i == pos ? rangemap_NROFRANGE/2+1 : i == pos+1 ? rangemap_NROFRANGE/2 : rangemap_NROFRANGE;
         TEST( child[i]->level == 0);
         TEST( child[i]->size  == S);
         TEST( child[i]->next  == child[i+1]/*last entry 0*/);
         for (unsigned s = 0, t; s < S; ++s, f = t+1) {
            t = f + (s<2&&i==pos?1:3);
            TEST( child[i]->range[s].from       == f);
            TEST( child[i]->range[s].to         == t);
            TEST( child[i]->range[s].state.size == 0);
         }
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < rangemap_NROFCHILD+2; ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      TEST(0 == delete_automatmman(&mman));
      TEST(0 == new_automatmman(&mman));
   }

   // TEST addrange_rangemap: (level 2) split child(level 1) and add to root
   for (unsigned nrchild=2; nrchild < rangemap_NROFCHILD; ++nrchild) {
      for (uintptr_t pos = 0; pos < nrchild; ++pos) {
         rangemap_node_t * child[rangemap_NROFCHILD] = { 0 };
         const unsigned LEVEL1_NROFSTATE = rangemap_NROFRANGE * rangemap_NROFCHILD;
         size_t SIZE = nrchild * LEVEL1_NROFSTATE + 1;
         TEST(0 == build2_rangemap(&rmap, mman, 2, nrchild, child));
         void * root = rmap.root;
         void * addr;
         TEST(0 == allocmem_automatmman(mman, sizeof(void*), &addr));
         void * splitchild = (uint8_t*)addr + sizeof(void*) + sizeof(rangemap_node_t)/*leaf*/;
         // test add single state ==> split
         char32_t r = (char32_t) (pos*(2*LEVEL1_NROFSTATE));
         TEST( 0 == addrange_rangemap(&rmap, mman, r, r));
         // check: mman
         TEST( sizeof(void*)+(1+2+nrchild+nrchild*rangemap_NROFCHILD)*sizeof(rangemap_node_t) == sizeallocated_automatmman(mman));
         // check rmap
         TEST( rmap.size == SIZE);
         TEST( rmap.root == root);
         // check rmap.root content
         TEST( rmap.root->level == 2);
         TEST( rmap.root->size  == nrchild+1);
         TEST( rmap.root->child[0] == child[0]);
         for (uintptr_t i = 1; i <= pos; ++i) {
            TEST( rmap.root->key[i-1] == (char32_t) (i*(2*LEVEL1_NROFSTATE)));
            TEST( rmap.root->child[i] == child[i]);
         }
         TEST( rmap.root->key[pos]     == (char32_t) (pos*(2*LEVEL1_NROFSTATE)+LEVEL1_NROFSTATE));
         TEST( rmap.root->child[pos+1] == splitchild);
         for (unsigned i = pos+2; i < nrchild; ++i) {
            TEST( rmap.root->key[i-1] == (char32_t) ((i-1)*(2*LEVEL1_NROFSTATE)));
            TEST( rmap.root->child[i] == child[i-1]);
         }
         // skip check other node/leaf content
         // reset
         TEST(0 == delete_automatmman(&mman));
         TEST(0 == new_automatmman(&mman));
      }
   }

   // reset
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}

static int test_initfree(void)
{
   automat_t      ndfa = automat_FREE;
   automat_t      ndfa1, ndfa2;
   size_t         S;
   automat_mman_t*mman = 0;
   automat_mman_t*mman2 = 0;
   automat_t      use_mman = automat_FREE;
   automat_t      use_mman2 = automat_FREE;
   char32_t       from[256];
   char32_t       to[256];
   helper_state_t helperstate[10];
   size_t         target_state[256];

   // prepare
   TEST(0 == new_automatmman(&mman));
   TEST(0 == new_automatmman(&mman2));
   use_mman.mman  = mman;
   use_mman2.mman = mman2;
   for (unsigned i = 0; i < lengthof(target_state); ++i) {
      target_state[i] = 1;
   }
   for (unsigned i = 0; i < lengthof(from); ++i) {
      from[i] = i;
      to[i]   = 3*i;
   }

   // TEST automat_FREE
   TEST( 0 == ndfa.mman);
   TEST( 0 == ndfa.nrstate);
   TEST( 0 == ndfa.allocated);
   TEST( 1 == isempty_slist(&ndfa.states));

   for (size_t i = 0; i < 256; ++i) {
      for (unsigned tc = 0; tc <= 1; ++tc) {
         size_t const SIZE_PAGE = SIZEALLOCATED_PAGECACHE();

         // TEST initmatch_automat
         TESTP( 0 == initmatch_automat(&ndfa, tc ? 0 : &use_mman, (uint8_t) i, from, to), "i:%zd", i);
         automat_mman_t * mm = ndfa.mman;
         TEST( 0 != mm);
         // check mman
         TEST( 1 == refcount_automatmman(mm));
         S = (unsigned) (3*(sizeof(state_t) - sizeof(transition_t)))
           + (unsigned) (2*sizeof(empty_transition_t) + i*sizeof(range_transition_t));
         TEST( sizeallocated_automatmman(mm) == S);
         // check ndfa
         TEST( ndfa.mman      == (tc ? mm : mman));
         TEST( ndfa.nrstate   == 3);
         TEST( ndfa.allocated == S);
         TEST( ! isempty_slist(&ndfa.states));
         // check ndfa.states
         helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 2 }, 0, 0 };
         helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
         helperstate[2] = (helper_state_t) { state_RANGE, (uint8_t) i, target_state, from, to };
         TEST( 0 == helper_compare_states(&ndfa, 3, helperstate))

         // TEST free_automat: free and double free
         // prepare
         incruse_automatmman(mm);
         TEST( 2 == refcount_automatmman(mm));
         for (unsigned r = 0; r <= 1; ++r) {
            // test
            TEST( 0 == free_automat(&ndfa));
            // check mman
            TEST( refcount_automatmman(mm) == 1);
            TEST( sizeallocated_automatmman(mm) == S);
            TEST( wasted_automatmman(mm) == S);
            // check ndfa
            TEST( ndfa.mman    == 0);
            TEST( ndfa.nrstate == 0);
            TEST( isempty_slist(&ndfa.states));
         }

         if (mm != mman) {
            // TEST free_automat: free deletes memory manager if refcount == 0
            TEST( 1 == refcount_automatmman(mm));
            ndfa.mman = mm;
            // test
            TEST( 0 == free_automat(&ndfa));
            // check ndfa
            TEST( 0 == ndfa.mman);
            // check env (mm freed)
            TEST( SIZE_PAGE == SIZEALLOCATED_PAGECACHE());
         } else {
            // reset
            decruse_automatmman(mm);
         }
      }
   }

   // TEST free_automat: simulated ERROR
   // TODO: TEST initempty_automat


   // TEST initempty_automat
   // TODO: TEST initempty_automat

   // TEST initsequence_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
   TEST(2 == refcount_automatmman(mman));
   S = sizeallocated_automatmman(mman);
   // test
   TEST( 0 == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
   // check mman
   TEST( refcount_automatmman(mman) == 1);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (2*sizeof(empty_transition_t));
   TEST( sizeallocated_automatmman(mman) == S);
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa2
   TEST( 0 == ndfa2.mman);
   // check ndfa
   TEST( ndfa.mman    == mman);
   TEST( ndfa.nrstate == 8);
   TEST( ndfa.allocated == S);
   TEST( ! isempty_slist(&ndfa.states));
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
   incruse_automatmman(mman);
   TEST(0 == free_automat(&ndfa));
   decruse_automatmman(mman);

   // TEST initrepeat_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(1 == refcount_automatmman(mman));
   S = sizeallocated_automatmman(mman);
   // test
   TEST( 0 == initrepeat_automat(&ndfa, &ndfa1));
   // check mman
   TEST( refcount_automatmman(mman) == 1);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (3*sizeof(empty_transition_t));
   TEST( sizeallocated_automatmman(mman) == S);
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa
   TEST( ndfa.mman    == mman);
   TEST( ndfa.nrstate == 5);
   TEST( ndfa.allocated == S);
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 2, 1 }, 0, 0 }; // ndfa
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 4 }, 0, 0 }; // ndfa1
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 0 }, 0, 0 };
   helperstate[4] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
   TEST(0 == helper_compare_states(&ndfa, 5, helperstate))
   // reset
   incruse_automatmman(mman);
   TEST(0 == free_automat(&ndfa));
   decruse_automatmman(mman);


   // TEST initor_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
   TEST(2 == refcount_automatmman(mman));
   S = sizeallocated_automatmman(mman);
   // test
   TEST( 0 == initor_automat(&ndfa, &ndfa1, &ndfa2));
   // check mman
   TEST( refcount_automatmman(mman) == 1);
   S += (unsigned) (2*(sizeof(state_t) - sizeof(transition_t)))
      + (unsigned) (3*sizeof(empty_transition_t));
   TEST( sizeallocated_automatmman(mman) == S);
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   // check ndfa2
   TEST( 0 == ndfa2.mman);
   // check ndfa
   TEST( ndfa.mman    == mman);
   TEST( ndfa.nrstate == 8);
   TEST( ndfa.allocated == S);
   TEST( ! isempty_slist(&ndfa.states));
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
   incruse_automatmman(mman);
   TEST(0 == free_automat(&ndfa));
   decruse_automatmman(mman);

   // === simulated ERROR / EINVAL

   for (unsigned err = 13; err < 15; ++err) {
      for (unsigned tc = 0, isDone = 0; !isDone; ++tc) {
         // prepare
         incruse_automatmman(mman);
         incruse_automatmman(mman2);
         switch (tc) {
         case 0:  // TEST initmatch_automat: simulated ERROR
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  TEST( (int)err == initmatch_automat(&ndfa, &use_mman, 1, from, to));
                  TEST( 0 == wasted_automatmman(mman));
                  break;
         case 1:  // TEST initsequence_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  TEST( (int)err == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  // reset
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 2:  // TEST initor_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, (int) err);
                  TEST( (int)err == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  // reset
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 3:  // TEST initsequence_automat: EINVAL empty ndfa
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  break;
         case 4:  // TEST initrepeat_automat: EINVAL empty ndfa
                  TEST( EINVAL == initrepeat_automat(&ndfa, &ndfa1));
                  TEST( 0 == wasted_automatmman(mman));
                  break;
         case 5:  // TEST initor_automat: EINVAL empty ndfa
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  break;
         case 6:  // TEST initsequence_automat: EINVAL (sequence of same automat)
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa1));
                  TEST( 0 == wasted_automatmman(mman));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 7:  // TEST initor_automat: EINVAL (sequence of same automat)
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa1));
                  TEST( 0 == wasted_automatmman(mman));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 8:  // TEST initsequence_automat: EINVAL (different mman)
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &use_mman2, 1, from+2, to+2));
                  TEST( EINVAL == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  TEST( 0 == wasted_automatmman(mman2));
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 9:  // TEST initor_automat: EINVAL (different mman)
                  TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &use_mman2, 1, from+2, to+2));
                  TEST( EINVAL == initor_automat(&ndfa, &ndfa1, &ndfa2));
                  TEST( 0 == wasted_automatmman(mman));
                  TEST( 0 == wasted_automatmman(mman2));
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         default: isDone = 1;
                  break;
         }
         // check mman
         decruse_automatmman(mman);
         decruse_automatmman(mman2);
         TEST( 0 == refcount_automatmman(mman));
         TEST( 0 == sizeallocated_automatmman(mman));
         TEST( 0 == refcount_automatmman(mman2));
         TEST( 0 == sizeallocated_automatmman(mman2));
         // check ndfa
         TEST( ndfa.mman    == 0);
         TEST( ndfa.nrstate == 0);
         TEST( ndfa.allocated == 0);
         TEST( isempty_slist(&ndfa.states));
      }
   }

   // reset
   TEST(0 == delete_automatmman(&mman));
   TEST(0 == delete_automatmman(&mman2));

   return 0;
ONERR:
   delete_automatmman(&mman);
   delete_automatmman(&mman2);
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
   automat_mman_t*mman = 0;
   char32_t       from[256];
   char32_t       to[256];
   size_t         target[255];
   helper_state_t helperstate[3+255];
   size_t         S;

   // prepare
   for (unsigned i = 0; i < lengthof(target); ++i) {
      target[i] = 1;
   }
   for (unsigned i = 0; i < lengthof(from); ++i) {
      from[i] = 1 + i;
      to[i]   = 1 + 2*i;
   }
   TEST(0 == initmatch_automat(&ndfa, 0, 15, from, to));
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 2 }, 0, 0 };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[2] = (helper_state_t) { state_RANGE, 15, target, from, to };
   TEST(0 == helper_compare_states(&ndfa, 3, helperstate))
   mman = ndfa.mman;
   S = sizeallocated_automatmman(mman);

   // TEST extendmatch_automat: EINVAL (freed ndfa)
   TEST( EINVAL == extendmatch_automat(&ndfa2, 1, from, to));
   TEST( 1 == refcount_automatmman(mman));
   TEST( S == sizeallocated_automatmman(mman));

   // TEST extendmatch_automat: EINVAL (nrmatch == 0)
   TEST( EINVAL == extendmatch_automat(&ndfa, 0, from, to));
   TEST( mman == ndfa.mman);
   TEST( 1 == refcount_automatmman(mman));
   TEST( S == sizeallocated_automatmman(mman));

   // TEST extendmatch_automat: ENOMEM
   init_testerrortimer(&s_automat_errtimer, 1, ENOMEM);
   TEST( ENOMEM == extendmatch_automat(&ndfa, 1, from, to));
   TEST( mman == ndfa.mman);
   TEST( 1 == refcount_automatmman(mman));
   TEST( S == sizeallocated_automatmman(mman));

   // TEST extendmatch_automat
   for (unsigned i = 1; i < lengthof(from); ++i) {
      TEST( 0 == extendmatch_automat(&ndfa, (uint8_t)i, from, to))
      // check mman
      S += state_SIZE + i * sizeof(range_transition_t);
      TEST( 1 == refcount_automatmman(mman));
      TEST( S == sizeallocated_automatmman(mman));
      // check ndfa
      TEST( mman == ndfa.mman);
      TEST( 3+i  == ndfa.nrstate);
      TEST( 0    == isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[2+i] = (helper_state_t) { state_RANGE_CONTINUE, (uint8_t) i, target, from, to };
      TESTP( 0 == helper_compare_states(&ndfa, ndfa.nrstate, helperstate), "i:%d", i);
   }

   // reset
   TEST(0 == free_automat(&ndfa));

   return 0;
ONERR:
   free_automat(&ndfa);
   return EINVAL;
}

int unittest_proglang_automat()
{
   if (test_state())       goto ONERR;
   if (test_multistate())  goto ONERR;
   if (test_rangemap())    goto ONERR;
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
   if (unittest_proglang_automat_mman()
       || unittest_proglang_automat()) {
      printf("RUN unittest_proglang_automat: *** ERROR ***\n");
   } else {
      printf("RUN unittest_proglang_automat: *** OK ***\n");
   }
   return 0;
}
#endif
