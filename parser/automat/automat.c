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
#include "patriciatrie.h"
#include "foreach.h"
#include "test_errortimer.h"

typedef uint32_t char32_t;

// === private types
struct memory_page_t;
struct range_transition_t;
struct empty_transition_t;
struct state_t;
struct statearray_block_t;
struct statearray_t;
struct statearray_iter_t;
struct depthstack_entry_t;
struct depthstack_t;
struct multistate_node_t;
struct multistate_t;
struct multistate_iter_t;
struct range_t;
struct rangemap_node_t;
struct rangemap_t;
struct rangemap_iter_t;
struct statevector_t;

#ifdef KONFIG_UNITTEST
// forward
static test_errortimer_t s_automat_errtimer;
#endif


/* struct: range_transition_t
 * Beschreibt Übergang von einem <state_t> zum nächsten.
 * Nur falls der Eingabecharacter zwischem from und to, beide Werte
 * mit eingeschlossen, liegt, darf der Übergang vollzogen werden. */
typedef struct range_transition_t {
   slist_node_t*  next;    // used in adapted slist_t  _rangelist
   struct state_t* state;  // goto state (only if char in [from .. to])
   char32_t from;          // inclusive
   char32_t to;            // inclusive
} range_transition_t;

// group: type support

/* define: YYY_rangelist
 * Implementiert <slist_t> für Objekte vom Typ <range_transition_t>. */
slist_IMPLEMENT(_rangelist, range_transition_t, next)


/* struct: empty_transition_t
 * Beschreibt unbedingten Übergang von einem <state_t> zum nächsten.
 * Der Übergang wird immer vollzogen ohne einen Buchstaben zu konsumieren. */
typedef struct empty_transition_t {
   slist_node_t*  next;    // used in adapted slist_t  _emptylist
   struct state_t* state;  // goto state (only if char in [from .. to])
} empty_transition_t;

// group: type support

/* define: YYY_emptylist
 * Implementiert <slist_t> für Objekte vom Typ <empty_transition_t>. */
slist_IMPLEMENT(_emptylist, empty_transition_t, next)


/* struct: state_t
 * Beschreibt einen Zustand des Automaten.
 * Ein Zustand besitzt nremptytrans leere Übergänge, verwaltet in emptylist
 * und nrrangetrans Zeichen erwartende Übergange, verwaltet in rangelist. */
typedef struct state_t {
   slist_node_t*  next;  // used in adapted slist_t  _statelist
   size_t   nremptytrans;
   size_t   nrrangetrans;
   slist_t  emptylist;
   slist_t  rangelist;
   union {
      uint8_t  isused; // used to mark a state as inserted or used
      size_t   nr;     // used to assign numbers to states for printing
      struct
      state_t* dest;   // used in copy operations
   };
} state_t;

// group: constants

#define state_SIZE                     (sizeof(struct state_t))
#define state_SIZE_EMPTYTRANS(nrtrans) ((nrtrans) * sizeof(empty_transition_t))
#define state_SIZE_RANGETRANS(nrtrans) ((nrtrans) * sizeof(range_transition_t))

// group: type support

/* define: YYY_statelist
 * Implementiert <slist_t> für Objekte vom Typ <state_t>. */
slist_IMPLEMENT(_statelist, state_t, next)

// group: lifetime

/* function: initempty_state
 * Initialisiert state mit nrtrans Übergangen zu target.
 * Ein leerer Übergang wird immer verwendet und liest bzw. verbraucht keinen Buchstaben. */
static void initempty_state(/*out*/state_t* state, state_t* target)
{
   empty_transition_t* trans = (void*) ((uintptr_t)state + state_SIZE);
   state->nremptytrans = 1;
   state->nrrangetrans = 0;
   initsingle_emptylist(&state->emptylist, trans);
   state->rangelist    = (slist_t) slist_INIT;
   trans[0].state = target;
}

/* function: initempty_state
 * Initialisiert state mit nrtrans Übergangen zu target.
 * Ein leerer Übergang wird immer verwendet und liest bzw. verbraucht keinen Buchstaben. */
static void initempty2_state(/*out*/state_t* state, state_t* target1, state_t* target2)
{
   empty_transition_t* trans = (void*) ((uintptr_t)state + state_SIZE);
   state->nremptytrans = 2;
   state->nrrangetrans = 0;
   initsingle_emptylist(&state->emptylist, &trans[1]);
   state->rangelist    = (slist_t) slist_INIT;
   insertfirst_emptylist(&state->emptylist, &trans[0]);
   trans[0].state = target1;
   trans[1].state = target2;
}

/* function: initrange_state
 * Initialisiert state mit nrmatch Übergangen zu target.
 * Diese testen den nächsten zu lesenden Character, ob er innerhalb
 * der nrmatch Bereiche [match_from..match_to] liegt. Wenn ja, wird der Character verbraucht
 * und der Übergang nach target ausgeführt. */
static void initrange_state(/*out*/state_t* state, state_t* target, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   state->nremptytrans = 0;
   state->nrrangetrans = nrmatch;
   state->emptylist    = (slist_t) slist_INIT;
   state->rangelist    = (slist_t) slist_INIT;
   range_transition_t* trans = (void*) ((uintptr_t)state + state_SIZE);
   for (uint8_t i = 0; i < nrmatch; ++i) {
      insertlast_rangelist(&state->rangelist, &trans[i]);
      trans[i].state = target;
      trans[i].from  = match_from[i];
      trans[i].to    = match_to[i];
   }
}

// group: update

/* function: extendmatch_state
 * Erweitert state mit nrmatch Übergangen zu target.
 * Diese testen den nächsten zu lesenden Character, ob er innerhalb
 * der nrmatch Bereiche [match_from..match_to] liegt. Wenn ja, wird der Character verbraucht
 * und der Übergang nach target ausgeführt. */
static void extendmatch_state(state_t* state, state_t* target, size_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch], range_transition_t trans[nrmatch])
{
   state->nrrangetrans += nrmatch;
   for (size_t i = 0; i < nrmatch; ++i) {
      insertlast_rangelist(&state->rangelist, &trans[i]);
      trans[i].state = target;
      trans[i].from  = match_from[i];
      trans[i].to    = match_to[i];
   }
}


/* struct: statearray_block_t
 * Speicherblock, in dem Zeiger auf <state_t> gespeichert werden.
 * Wird von <statearray_t> verwendet. */
typedef struct statearray_block_t {
   slist_node_t * next;
   size_t         nrstate;
   state_t *      state[/*nrstate <= length_of_block*/];
} statearray_block_t;

/* struct: statearray_t
 * Beschreibt zwei Mengen von unsortieren Zuständen.
 * Falls ein Zustand mehrfach hinzugefügt wird, wird dies nicht verhindert.
 * Die eine Menge dient dem destruktiven Lesen, die zweite dem Schreiben.
 * Am Ende werden die Rollen vertauscht. */
typedef struct statearray_t {
   automat_mman_t * mman;
   size_t   length_of_block;  // see statearray_block_t
   slist_t  addlist;          // list of statearray_block_t where new entries are added to the end
   slist_t  dellist;          // list of statearray_block_t where reading removes the first entry
   slist_t  freelist;         // list of free unsued statearray_block_t (cache)
   state_t** addnext;         // position where the new entry is added
   state_t** addend;          // end address of state array of statearray_block_t
   statearray_block_t * delblock;
   state_t** delnext;         // position where the old entry is removed
   state_t** delend;          // end address of state array of statearray_block_t
} statearray_t;

// group: type support

/* define: YYY_blocklist
 * Defines interface of type <slist_t> for <statearray_block_t>. */
slist_IMPLEMENT(_blocklist, statearray_block_t, next)

// group: helper

static uint16_t sizeblock_statearray(void)
{
   return (1<<14);
}

// group: lifetime

/* define: statearray_FREE
 * Static initializer. */
#define statearray_FREE \
         { 0, 0, slist_INIT, slist_INIT, slist_INIT, 0, 0, 0, 0, 0 }

static int init_statearray(statearray_t * arr)
{
   int err;
   automat_mman_t  *   mman = 0;
   size_t length_of_block = (sizeblock_statearray()-sizeof(statearray_block_t)) / sizeof(state_t*);

   err = new_automatmman(&mman);
   if (err) goto ONERR;

   arr->mman = mman;
   arr->length_of_block = length_of_block;
   arr->addlist  = (slist_t) slist_INIT;
   arr->dellist  = (slist_t) slist_INIT;
   arr->freelist = (slist_t) slist_INIT;
   arr->addnext = 0;
   arr->addend  = 0;
   arr->delblock = 0;
   arr->delnext = 0;
   arr->delend  = 0;

   return 0;
ONERR:
   delete_automatmman(&mman);
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int free_statearray(statearray_t * arr)
{
   int err;

   if (arr->mman) {
      err = delete_automatmman(&arr->mman);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: update

static int insert1_statearray(statearray_t * arr, state_t * state)
{
   int err;

   if (arr->addnext == arr->addend) {
      statearray_block_t * lastblock = last_blocklist(&arr->addlist);
      if (lastblock) lastblock->nrstate = (size_t) (arr->addnext - lastblock->state);
      statearray_block_t * newblock;
      if (isempty_blocklist(&arr->freelist)) {
         void * memblock;
         err = malloc_automatmman(arr->mman, (uint16_t) sizeblock_statearray(), &memblock);
         if (err) goto ONERR;
         newblock = memblock;
      } else {
         newblock = removefirst_blocklist(&arr->freelist);
      }
      insertlast_blocklist(&arr->addlist, newblock);
      newblock->nrstate = 0;
      arr->addnext = &newblock->state[0];
      arr->addend  = &newblock->state[arr->length_of_block];
   }

   *arr->addnext++ = state;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int remove2_statearray(statearray_t * arr, state_t ** state)
{
   while (arr->delnext == arr->delend) {
      if (arr->delblock) {
         insertlast_blocklist(&arr->freelist, arr->delblock);
         arr->delblock = 0;
      }
      if (isempty_blocklist(&arr->dellist)) {
         return ENODATA;
      }
      arr->delblock = removefirst_blocklist(&arr->dellist);
      arr->delnext = &arr->delblock->state[0];
      arr->delend  = &arr->delblock->state[arr->delblock->nrstate];
   }

   *state = *arr->delnext++;

   return 0;
}

/* function: swap1and2_statearray
 * Array added becomes array removed from and  vice versa. */
static void swap1and2_statearray(statearray_t * arr)
{
   if (arr->delblock) {
      insertlast_blocklist(&arr->freelist, arr->delblock);
   }
   insertlastPlist_blocklist(&arr->freelist, &arr->dellist);
   // dellist now empty
   statearray_block_t * lastblock = last_blocklist(&arr->addlist);
   if (lastblock) {
      lastblock->nrstate = (size_t) (arr->addnext - lastblock->state);
      arr->dellist = arr->addlist;
      arr->addlist = (slist_t) slist_INIT;
   }
   arr->addnext = 0;
   arr->addend  = 0;
   arr->delblock = 0;
   arr->delnext = 0;
   arr->delend  = 0;
}


/* struct: statearray_iter_t
 * ITerator der über eingefügte Zustände iteriert.
 * Wird von <statearray_t> verwendet. */
typedef struct statearray_iter_t {
   statearray_block_t * block;
   state_t** next;
   state_t** end;
} statearray_iter_t;

// group: lifetime

static void init_statearrayiter(/*out*/statearray_iter_t * iter, const statearray_t * arr)
{
   iter->block = first_blocklist(&arr->addlist);
   iter->next = iter->block ? &iter->block->state[0] : 0;
   iter->end  = iter->block ? &iter->block->state[iter->block->nrstate] : 0;
}

// group: iterate

static bool next_statearrayiter(statearray_iter_t * iter, const statearray_t * arr, /*out*/state_t ** state)
{
   while (iter->next == iter->end) {
      if (! iter->block) return false;
      if (iter->block == last_blocklist(&arr->addlist)) {
         if (iter->end == arr->addnext) return false;
         iter->end = arr->addnext;
      } else if (iter->end != &iter->block->state[iter->block->nrstate]) {
         iter->end = &iter->block->state[iter->block->nrstate];
      } else {
         iter->block = next_blocklist(iter->block);
         iter->next  = &iter->block->state[0];
         iter->end   = &iter->block->state[iter->block->nrstate];
      }
   }

   *state = *iter->next++;

   return true;
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
         state_t * key[7]; // key[i] == child[i+1]->child[/*(level > 0)*/0]->...->state[/*(level == 0)*/0]
         struct multistate_node_t * child[8];
      };
      struct { // level == 0
         struct multistate_node_t * next;
         state_t * state[14];
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

static int invariant2_multistate(multistate_node_t* node, unsigned level, state_t** from, state_t** to)
{
   int err;
   if (node->level != level) {
      return EINVARIANT;
   }

   if (     node->size < 2
         || node->size > (node->level ? multistate_NROFCHILD : multistate_NROFSTATE)) {
      return EINVARIANT;
   }

   if (node->level) {
      if (from && *from >= node->key[0]) {
         return EINVARIANT;
      }
      if (to && *to <= node->key[node->size-2]) {
         return EINVARIANT;
      }
      for (unsigned i = 0; i < node->size-2u; ++i) {
         if (node->key[i] >= node->key[i+1]) {
            return EINVARIANT;
         }
      }
      for (unsigned i = 0; i < node->size; ++i) {
         err = invariant2_multistate(node->child[i], level-1, i ? &node->key[i-1] : from, i < node->size-1u ? &node->key[i] : to);
         if (err) return err;
      }
   } else {
      if (from && *from != node->state[0]) {
         return EINVARIANT;
      }
      if (to && *to <= node->state[node->size-1]) {
         return EINVARIANT;
      }
      for (unsigned i = 0; i < node->size-1u; ++i) {
         if (node->state[i] >= node->state[i+1]) {
            return EINVARIANT;
         }
      }
   }

   return 0;
}

static int invariant_multistate(multistate_t* mst)
{
   int err;
   multistate_node_t* node = mst->root;

   if (mst->size <= 1) return 0;
   if (!node || node->level >= 32 || node->size < 2) return EINVARIANT;

   err = invariant2_multistate(node, node->level, 0, 0);

   return err;

}

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
            err = malloc_automatmman(mman, SIZE, &node2);
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
               err = malloc_automatmman(mman, SIZE, &node2);
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
                  for (unsigned i = (multistate_NROFCHILD-1)-low; i; --i, ++node_key, ++node2_key, ++node_child, ++node2_child) {
                     *node2_key = *node_key;
                     *node2_child = *node_child;
                  }
               }
            }
         }

         // increment depth by 1: alloc root node pointing to child and split child
         void * root;
         // ENOMEM ==> information in node2 is lost !! (corrupt data structure)
         err = malloc_automatmman(mman, SIZE, &root);
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
      if (mst->root == state) return EEXIST;
      err = malloc_automatmman(mman, SIZE, &node);
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

/* struct: multistate_iter_t
 * TODO: */
typedef struct multistate_iter_t {
   void*    next_node;
   uint8_t  next_state;
   uint8_t  is_single;
} multistate_iter_t;

// group: lifetime

static void init_multistateiter(multistate_iter_t* iter, const multistate_t* mst)
{
   iter->next_node = 0;
   iter->next_state = 0;
   iter->is_single = 0;

   if (mst->size == 1) {
      iter->next_node = mst->root;
      iter->is_single = 1;
   } else if (mst->size) {
      multistate_node_t* node = mst->root;
      for (unsigned level = node->level; (level--) > 0; ) {
         if (node->size > multistate_NROFCHILD || ! node->size) goto ONERR;
         node = node->child[0];
         if (node->level != level) goto ONERR;
      }
      iter->next_node = node;
   }

ONERR:
   return;
}

static bool next_multistateiter(multistate_iter_t* iter, state_t** state)
{
   if (iter->is_single) {
      *state = iter->next_node;
      iter->next_node = 0;
      iter->is_single = 0;
      return true;
   }

   multistate_node_t* node = iter->next_node;

   while (node) {
      if (iter->next_state < node->size) {
         *state = node->state[iter->next_state++];
         return true;
      }
      node = node->next;
      iter->next_node  = node;
      iter->next_state = 0;
   }

   return false;
}


/* struct: range_t
 * Beschreibt einen Übergangsbereich zu einem Zustand.
 * Wird für Optimierung des Automaten in einen DFA benötigt. */
typedef struct range_t {
   char32_t       from;    // inclusive
   char32_t       to;      // inclusive
   multistate_t   multistate;
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

static int invariant2_rangemap(rangemap_node_t* node, unsigned level, char32_t* from, char32_t to)
{
   int err;
   if (node->level != level) {
      return EINVARIANT;
   }

   if (     node->size < 2
         || node->size > (node->level ? rangemap_NROFCHILD : rangemap_NROFRANGE)) {
      return EINVARIANT;
   }

   if (node->level) {
      if (from && *from >= node->key[0]) {
         return EINVARIANT;
      }
      if (to <= node->key[node->size-2]) {
         return EINVARIANT;
      }
      for (unsigned i = 0; i < node->size-2u; ++i) {
         if (node->key[i] >= node->key[i+1]) {
            return EINVARIANT;
         }
      }
      for (unsigned i = 0; i < node->size; ++i) {
         err = invariant2_rangemap(node->child[i], level-1, i ? &node->key[i-1] : from, i < node->size-1u ? node->key[i]-1u : to);
         if (err) return err;
      }
   } else {
      if (from && *from != node->range[0].from) {
         return EINVARIANT;
      }
      if (to < node->range[node->size-1].to) {
         return EINVARIANT;
      }
      for (unsigned i = 0; i < node->size; ++i) {
         if (node->range[i].from > node->range[i].to) {
            return EINVARIANT;
         }
      }
      for (unsigned i = 0; i < node->size-1u; ++i) {
         if (node->range[i].to >= node->range[i+1].from) {
            return EINVARIANT;
         }
      }
   }

   return 0;
}

static int invariant_rangemap(rangemap_t * rmap)
{
   int err;
   rangemap_node_t* node = rmap->root;

   if (rmap->size <= 1) return 0;
   if (!node || node->level >= 32 || node->size < 2) return EINVARIANT;

   err = invariant2_rangemap(node, node->level, 0, (char32_t)-1);

   return err;
}

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
      err = malloc_automatmman(mman, SIZE, &memblock);
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
            to = nextFrom-1;
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
            err = malloc_automatmman(mman, SIZE, &memblock);
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
               err = malloc_automatmman(mman, SIZE, &memblock);
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
                  for (unsigned i = (rangemap_NROFCHILD-1)-low; i; --i, ++node_key, ++node2_key, ++node_child, ++node2_child) {
                     *node2_key = *node_key;
                     *node2_child = *node_child;
                  }
               }
            }
         }

         // increment depth by 1: alloc root node pointing to child and split child
         rangemap_node_t * root;
         // ENOMEM ==> information in node2 is lost !! (corrupt data structure)
         err = malloc_automatmman(mman, SIZE, &memblock);
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

static int addrange_rangemap(rangemap_t* rmap, automat_mman_t* mman, char32_t from, char32_t to)
{
   int err;
   char32_t next_from = from;

   VALIDATE_INPARAM_TEST(from <= to, ONERR, );

   for (;;) {
      err = addrange2_rangemap(rmap, mman, next_from, to, &next_from);
      if (! err) break;
      if (err != EAGAIN) goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

static int addstate_rangemap(rangemap_t *rmap, automat_mman_t *mman, char32_t from, char32_t to, state_t *state)
{
   int err;
   rangemap_node_t *node;

   VALIDATE_INPARAM_TEST(from <= to, ONERR, );

   if (! rmap->size) {
      err = EINVAL;
      goto ONERR;
   }

   // !! copied from addrange2_rangemap !!
   node = rmap->root;
   for (unsigned level = node->level; (level--) > 0; ) {
      if (node->size > rangemap_NROFCHILD || node->size < 2) return EINVARIANT;
      unsigned high = node->size -1u;
      unsigned low  = 0;
      for (unsigned mid = high / 2u; /*low < high*/; mid = (high + low) / 2u) {
         if (node->key[mid] <= from) {
            low = mid + 1;
         } else {
            high = mid;
         }
         if (low == high) break;
      }
      node = node->child[low];
      if (node->level != level) return EINVARIANT;
   }
   if (node->size > rangemap_NROFRANGE) return EINVARIANT;
   unsigned high = node->size;
   unsigned low  = 0;
   for (unsigned mid = high / 2u; low < high; mid = (high + low) / 2u) {
      if (node->range[mid].to < from) {
         low = mid + 1;
      } else {
         high = mid;
      }
   }

   size_t expect = from;
   for (;;) {
      if (  low >= node->size
            || node->range[low].from != expect) {
         return EINVAL;
      }
      expect = node->range[low].to + 1;
      if (node->range[low].to > to) return EINVAL;
      err = add_multistate(&node->range[low].multistate, mman, state);
      if (err) goto ONERR;
      if (node->range[low].to == to) break;
      ++ low;
      if (low >= node->size) {
         low = 0;
         node = node->next;
         if (!node) return EINVAL;
      }
   }

   return 0;
ONERR:
   return err;
}

/* struct: rangemap_iter_t
 * Iteriert über alle in <rangemap_t> gespeicherten <range_t> in aufsteigender Reihenfolge.
 * Es gilt: range[i].to < range[i+1].from */
typedef struct rangemap_iter_t {
   void*    next_node;
   uint8_t  next_range;
} rangemap_iter_t;

// group: lifetime

static void init_rangemapiter(rangemap_iter_t* iter, const rangemap_t* rmap)
{
   iter->next_node  = 0;
   iter->next_range = 0;

   if (rmap->size) {
      rangemap_node_t* node = rmap->root;
      for (unsigned level = node->level; (level--) > 0; ) {
         if (node->size > rangemap_NROFCHILD || ! node->size) goto ONERR;
         node = node->child[0];
         if (node->level != level) goto ONERR;
      }
      iter->next_node = node;
   }

ONERR:
   return;
}

static bool next_rangemapiter(rangemap_iter_t* iter, range_t** range)
{
   rangemap_node_t* node = iter->next_node;

   while (node) {
      if (iter->next_range < node->size) {
         *range = &node->range[iter->next_range++];
         return true;
      }
      node = node->next;
      iter->next_node  = node;
      iter->next_range = 0;
   }

   return false;
}


/* struct: statevector_t
 * Ein Array von sortiertem Pointern auf <state_t>. */
typedef struct statevector_t {
   patriciatrie_node_t  index;   // permits storing in index of type patriciatrie_t
   slist_node_t       * next;    // permits storing in single linked list of type slist_t
   state_t            * dfa;     // points to computed state of the build dfa automaton
   size_t               nrstate;
   state_t*             state[/*nrstate*/];
} statevector_t;

// group: constants

/* define: statevector_MAX_NRSTATE
 * Definiert die maximale Anzahl an Zeigern (auf state_t), die in <statevector_t.state>
 * gespeichert werden können, so dass immer gilt: (sizeof(statevector_t) <= UINT16_MAX). */
#define statevector_MAX_NRSTATE \
         ((UINT16_MAX - sizeof(statevector_t)) / sizeof(state_t*))

// group: types

/* define: YYY_stateveclist
 * Verwaltet <statevector_t> als Liste. */
slist_IMPLEMENT(_stateveclist, statevector_t, next)

// group: query

/* function: getkey_statevector
 * Gibt Schlüssel zurück, über den der <statevector_t> indiziert wird. */
static void getkey_statevector(void *obj, getkey_data_t *key)
{
   // set out value
   key->addr = (void*) ((statevector_t*)obj)->state;
   key->size = ((statevector_t*)obj)->nrstate * sizeof(state_t*);
}

static inline getkey_adapter_t keyadapter_statevector(void)
{
   return (getkey_adapter_t) getkey_adapter_INIT(offsetof(statevector_t, index), &getkey_statevector);
}

static inline bool iscontained_statevector(statevector_t *svec, state_t *state)
{
   size_t high = svec->nrstate;
   size_t low  = 0;

   while (low < high) {
      size_t mid = (low + high)/2;
      if (svec->state[mid] < state) {
         low = mid+1;
      } else {
         high = mid;
      }
   }

   return (svec->nrstate > low) && (state == svec->state[low]);
}

// TODO: add test for isinuse12_statevector
static inline bool isinuse12_statevector(statevector_t *svec, bool isNeedValue2)
{
   size_t i;

   for (i = 0; i < svec->nrstate; ++i) {
      if (svec->state[i]->isused == 1) {
         if (! isNeedValue2) return true;
         for (++i; i < svec->nrstate; ++i) {
            if (svec->state[i]->isused == 2) return true;
         }
      } else if (svec->state[i]->isused == 2) {
         for (++i; i < svec->nrstate; ++i) {
            if (svec->state[i]->isused == 1) return true;
         }
      }
   }

   return false;
}

// group: lifetime

/* function: init_statevector
 * Allokiert neuen statevector_t und kopiert states von multistate nach *svec.
 * Die kopierten states sind in aufsteigend sortierter Reihenfolge gemäß ihrer Speicheradresse. */
static int init_statevector(/*out*/statevector_t **svec, automat_mman_t *mman, multistate_t* multistate)
{
   int err;
   void * newvec;
   multistate_iter_t iter;

   if (multistate->size > statevector_MAX_NRSTATE) {
      err = EOVERFLOW;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (sizeof(statevector_t) + multistate->size * sizeof(state_t*));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(mman, SIZE, &newvec);
   }
   if (err) goto ONERR;

   // copy states from multistate into newvec
   ((statevector_t*)newvec)->index   = (patriciatrie_node_t) patriciatrie_node_INIT;
   ((statevector_t*)newvec)->next    = 0;
   ((statevector_t*)newvec)->dfa     = 0;
   ((statevector_t*)newvec)->nrstate = multistate->size;
   init_multistateiter(&iter, multistate);
   size_t i;
   for (i = 0; next_multistateiter(&iter, &((statevector_t*)newvec)->state[i]); ++i) {
      /*returned states are in sorted order*/
      assert(i < multistate->size);
   }
   assert(i == multistate->size);

   // set out
   *svec = newvec;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
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

static inline void startend_automat(const automat_t* ndfa, /*out*/state_t ** start, /*out*/state_t ** end)
{
   // documents how to access start and end state
   state_t * last = last_statelist(&ndfa->states);
   *start = next_statelist(last);
   *end   = last;
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
   void * endstate;
   automat_mman_t * mman;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + 2*state_SIZE_EMPTYTRANS(1));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(mman, SIZE, &endstate);
   }
   if (err) goto ONERR;

   state_t * startstate = (void*) ((uint8_t*)endstate + (state_SIZE + state_SIZE_EMPTYTRANS(1)));
   initempty_state(endstate, endstate);
   initempty_state(startstate, endstate);

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
   void * endstate;
   automat_mman_t * mman;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(nrmatch));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(mman, SIZE, &endstate);
   }
   if (err) goto ONERR;

   state_t * startstate = (void*) ((uint8_t*)endstate + (state_SIZE + state_SIZE_EMPTYTRANS(1)));
   initempty_state(endstate, endstate);
   initrange_state(startstate, endstate, nrmatch, match_from, match_to);

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

int initcopy_automat(/*out*/automat_t* dest_ndfa, const automat_t* src_ndfa, const automat_t* use_mman)
{
   int err;
   automat_mman_t * mman;
   slist_t dest_states = slist_INIT;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   // allocate space for every state with transitions in dest_ndfa
   foreach (_statelist, src_state, &src_ndfa->states) {
      void * dest_state;
      if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
         err = malloc_automatmman(mman, state_SIZE, &dest_state);
      }
      if (err) goto ONERR;
      src_state->dest = dest_state;
      insertlast_statelist(&dest_states, (state_t*)dest_state);
      ((state_t*)dest_state)->nremptytrans = src_state->nremptytrans;
      ((state_t*)dest_state)->nrrangetrans = src_state->nrrangetrans;
      ((state_t*)dest_state)->emptylist    = (slist_t) slist_INIT;
      ((state_t*)dest_state)->rangelist    = (slist_t) slist_INIT;
      for (size_t i = 0; i < src_state->nremptytrans; ++i) {
         void* empty_trans;
         err = malloc_automatmman(mman, state_SIZE_EMPTYTRANS(1), &empty_trans);
         if (err) goto ONERR;
         insertlast_emptylist(&((state_t*)dest_state)->emptylist, (empty_transition_t*)empty_trans);
      }
      for (size_t i = 0; i < src_state->nrrangetrans; ++i) {
         void* range_trans;
         err = malloc_automatmman(mman, state_SIZE_RANGETRANS(1), &range_trans);
         if (err) goto ONERR;
         insertlast_rangelist(&((state_t*)dest_state)->rangelist, (range_transition_t*)range_trans);
      }

   }

   // copy transitions
   foreach (_statelist, src_state, &src_ndfa->states) {
      {
         empty_transition_t* src_trans  = last_emptylist(&src_state->emptylist);
         empty_transition_t* dest_trans = last_emptylist(&src_state->dest->emptylist);
         for (size_t i = 0; i < src_state->nremptytrans; ++i) {
            dest_trans->state = src_trans->state->dest;
            src_trans  = next_emptylist(src_trans);
            dest_trans = next_emptylist(dest_trans);
         }
      }
      {
         range_transition_t* src_trans  = last_rangelist(&src_state->rangelist);
         range_transition_t* dest_trans = last_rangelist(&src_state->dest->rangelist);
         for (size_t i = 0; i < src_state->nrrangetrans; ++i) {
            dest_trans->state = src_trans->state->dest;
            dest_trans->from  = src_trans->from;
            dest_trans->to    = src_trans->to;
            src_trans  = next_rangelist(src_trans);
            dest_trans = next_rangelist(dest_trans);
         }
      }
   }

   // set out
   incruse_automatmman(mman);
   dest_ndfa->mman    = mman;
   dest_ndfa->nrstate = src_ndfa->nrstate;
   dest_ndfa->allocated = src_ndfa->allocated;
   dest_ndfa->states  = dest_states;

   return 0;
ONERR:
   if (! use_mman) {
      delete_automatmman(&mman);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

// TODO: test initreverse_automat
int initreverse_automat(/*out*/automat_t* dest_ndfa, const automat_t* src_ndfa, const automat_t* use_mman)
{
   int err;
   automat_mman_t * mman;
   slist_t  dest_states = slist_INIT;

   if (use_mman) {
      mman = use_mman->mman;
   } else {
      mman = 0;
      err = new_automatmman(&mman);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   if (src_ndfa->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   // dest_ndfa: allocate space for every state but without transitions
   foreach (_statelist, src_state, &src_ndfa->states) {
      void * dest_state;
      if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) { \
         err = malloc_automatmman(mman, state_SIZE, &dest_state); \
      } \
      if (err) goto ONERR; \
      src_state->dest = dest_state; \
      ((state_t*)dest_state)->nremptytrans = 0; \
      ((state_t*)dest_state)->nrrangetrans = 0; \
      ((state_t*)dest_state)->emptylist    = (slist_t) slist_INIT; \
      ((state_t*)dest_state)->rangelist    = (slist_t) slist_INIT;
      insertfirst_statelist(&dest_states, (state_t*)dest_state);
   }

   // copy transitions (unordered (slow memory access!))
   foreach (_statelist, src_state, &src_ndfa->states) {
      foreach (_emptylist, src_trans, &src_state->emptylist) {
         void* dest_trans;
         err = malloc_automatmman(mman, state_SIZE_EMPTYTRANS(1), &dest_trans);
         if (err) goto ONERR;
         state_t* dest_state = src_trans->state->dest;
         ++ dest_state->nremptytrans;
         insertlast_emptylist(&dest_state->emptylist, (empty_transition_t*)dest_trans);
         ((empty_transition_t*)dest_trans)->state = src_state->dest;
      }
      foreach (_rangelist, src_trans, &src_state->rangelist) {
         void* dest_trans;
         err = malloc_automatmman(mman, state_SIZE_RANGETRANS(1), &dest_trans);
         if (err) goto ONERR;
         state_t* dest_state = src_trans->state->dest;
         ++ dest_state->nrrangetrans;
         insertlast_rangelist(&dest_state->rangelist, (range_transition_t*)dest_trans);
         ((range_transition_t*)dest_trans)->state = src_state->dest;
         ((range_transition_t*)dest_trans)->from  = src_trans->from;
         ((range_transition_t*)dest_trans)->to    = src_trans->to;
      }
   }

   state_t* endstate = last_statelist(&dest_states);
   size_t   size_selftrans = 0;
   if (  !endstate->nremptytrans
         || last_emptylist(&endstate->emptylist)->state != endstate) {
      void* dest_trans;
      size_selftrans = state_SIZE_EMPTYTRANS(1);
      err = malloc_automatmman(mman, state_SIZE_EMPTYTRANS(1), &dest_trans);
      if (err) goto ONERR;
      ++ endstate->nremptytrans;
      insertlast_emptylist(&endstate->emptylist, (empty_transition_t*)dest_trans);
      ((empty_transition_t*)dest_trans)->state = endstate;
   }

   // set out
   incruse_automatmman(mman);
   dest_ndfa->mman    = mman;
   dest_ndfa->nrstate = src_ndfa->nrstate;
   dest_ndfa->allocated = src_ndfa->allocated + size_selftrans;
   dest_ndfa->states  = dest_states;

   return 0;
ONERR:
   if (! use_mman) {
      delete_automatmman(&mman);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: query

size_t matchchar32_automat(const automat_t* ndfa, size_t len, const char32_t str[len], bool matchLongest)
{
   int err;
   state_t * start;
   state_t * end;
   state_t * next;
   size_t    stroffset = 0;
   size_t    matchedlen = 0;
   statearray_t  states = statearray_FREE;
   statearray_iter_t iter;

   foreach (_statelist, s, &ndfa->states) {
      s->isused = 0;
   }

   startend_automat(ndfa, &start, &end);
   err = init_statearray(&states);
   if (err) goto ONERR;
   err = insert1_statearray(&states, start);
   if (err) goto ONERR;
   start->isused = 1;

   for (;;) {
      // === extend list of states with empty transition targets ===
      init_statearrayiter(&iter, &states);
      while (next_statearrayiter(&iter, &states, &next)) {
         foreach (_emptylist, empty_trans, &next->emptylist) {
            state_t * target = empty_trans->state;
            if (! target->isused) {
               target->isused = 1;
               err = insert1_statearray(&states, target);
               if (err) goto ONERR;
               // now target is returned by one of the next calls to next_statearrayiter
               // ==> it is followed too !
            }
         }
      }

      // set of states includes end state ?
      const unsigned isEnd = end->isused;

      // === reset: clear insert flags ===
      init_statearrayiter(&iter, &states);
      while (next_statearrayiter(&iter, &states, &next)) {
         next->isused = 0;
      }

      // === check end of match reached ===
      if (isEnd) {
         matchedlen = stroffset;
         if (! matchLongest) break; // use first match
      }

      // === check end of string reached ===
      if (stroffset >= len) break;

      // === match next single character ===
      swap1and2_statearray(&states);
      if (remove2_statearray(&states, &next)) break/*ENODATA*/;

      do {
         // loop over range-transitions of state (stored in variable next)
         // add range targets if they match str[stroffset]
         foreach (_rangelist, range_trans, &next->rangelist) {
            state_t * target = range_trans->state;
            if (! target->isused
               && range_trans->from <= str[stroffset] && str[stroffset] <= range_trans->to) {
               target->isused = 1;
               err = insert1_statearray(&states, target);
               if (err) goto ONERR;
            }
         }
      } while (0 == remove2_statearray(&states, &next));

      ++ stroffset;
   }

   err = free_statearray(&states);
   if (err) goto ONERR;

   return matchedlen;
ONERR:
   free_statearray(&states);
   TRACEEXIT_ERRLOG(err);
   return 0;
}

void print_automat(automat_t const* ndfa)
{
   size_t nr = 0;

   // assign numbers to states
   foreach (_statelist, s, &ndfa->states) {
      s->nr = nr++;
   }

   // print every transition of every state starting with start state
   printf("\n");
   foreach (_statelist, s, &ndfa->states) {
      int isErrorState = 1;
      foreach (_emptylist, trans, &s->emptylist) {
         isErrorState = 0;
         printf("%zd(%p) ''--> %zd(%p)\n", s->nr, (void*)s, trans->state->nr, (void*)trans->state);
      }
      foreach (_rangelist, trans, &s->rangelist) {
         isErrorState = 0;
         printf("%zd(%p) '", s->nr, (void*)s);
         if (' ' <= trans->from && trans->from <= 'z') {
            printf("%c", (char)trans->from);
         } else {
            printf("0x%02"PRIx32, trans->from);
         }
         if (trans->from != trans->to) {
            if (' ' <= trans->to && trans->to <= 'z') {
               printf("-%c", (char)trans->to);
            } else {
               printf("-0x%02"PRIx32, trans->to);
            }
         }
         printf("'--> %zd(%p)\n", trans->state->nr, (void*)trans->state);
      }
      if (isErrorState) {
         printf("%zd(%p) ------\n", s->nr, (void*)s);
      }
   }
}

// group: extend

int extendmatch_automat(automat_t* ndfa, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch])
{
   int err;
   void * rangetrans;

   if (ndfa->nrstate < 2 || nrmatch == 0) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (state_SIZE_RANGETRANS(nrmatch));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(ndfa->mman, SIZE, &rangetrans);
   }
   if (err) goto ONERR;

   state_t * startstate, * endstate;
   startend_automat(ndfa, &startstate, &endstate);

   extendmatch_state(startstate, endstate, nrmatch, match_from, match_to, (range_transition_t*)rangetrans);

   // update ndfa
   ndfa->allocated += SIZE;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: operations

int opsequence_automat(automat_t* ndfa, automat_t* ndfa2/*freed after return*/)
{
   int err;
   void * endstate;
   automat_t copy;
   automat_t * ndfa2cpy = ndfa2;

   if (ndfa->nrstate < 2 || ndfa2->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   if (ndfa->mman != ndfa2->mman) {
      err = initcopy_automat(&copy, ndfa2, ndfa);
      if (err) goto ONERR;
      ndfa2cpy = &copy; // (ndfa2cpy != ndfa2) ==> ndfa2 is considered freed
      err = free_automat(ndfa2);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(ndfa->mman, SIZE, &endstate);
   }
   if (err) goto ONERR;

   state_t *startstate = (void*) ((uint8_t*)endstate + (state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t *start1, *end1;
   startend_automat(ndfa, &start1, &end1);
   state_t *start2, *end2;
   startend_automat(ndfa2cpy, &start2, &end2);

   initempty_state(endstate, endstate);
   initempty_state(startstate, start1);
   last_emptylist(&end1->emptylist)->state = start2;
   last_emptylist(&end2->emptylist)->state = endstate;

   // set out
   ndfa->nrstate = 2 + ndfa->nrstate + ndfa2cpy->nrstate;
   ndfa->allocated = SIZE + ndfa->allocated + ndfa2cpy->allocated;
   insertlastPlist_slist(&ndfa->states, &ndfa2cpy->states);
   insertlast_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   // fast free
   decruse_automatmman(ndfa2cpy->mman);
   if (ndfa2cpy == ndfa2) *ndfa2 = (automat_t) automat_FREE;

   return 0;
ONERR:
   if (ndfa2cpy != ndfa2) {
      initmove_automat(ndfa2, ndfa2cpy);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int oprepeat_automat(/*out*/automat_t* ndfa)
{
   int err;
   void * endstate;

   if (ndfa->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + state_SIZE_EMPTYTRANS(3));
   err = malloc_automatmman(ndfa->mman, SIZE, &endstate);
   if (err) goto ONERR;

   state_t * startstate = (void*) ((uint8_t*)endstate + (state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t * start1, * end1;
   startend_automat(ndfa, &start1, &end1);

   initempty_state(endstate, endstate);
   initempty2_state(startstate, start1, endstate);
   last_emptylist(&end1->emptylist)->state = startstate;

   // set out
   ndfa->nrstate = 2 + ndfa->nrstate;
   ndfa->allocated = SIZE + ndfa->allocated;
   insertlast_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int opor_automat(/*out*/automat_t* ndfa, automat_t* ndfa2/*freed after return*/)
{
   int err;
   void * endstate;
   automat_t copy;
   automat_t * ndfa2cpy = ndfa2;

   if (ndfa->nrstate < 2 || ndfa2->nrstate < 2) {
      err = EINVAL;
      goto ONERR;
   }

   if (ndfa->mman != ndfa2->mman) {
      err = initcopy_automat(&copy, ndfa2, ndfa);
      if (err) goto ONERR;
      ndfa2cpy = &copy; // (ndfa2cpy != ndfa2) ==> copy is marked as used
      err = free_automat(ndfa2);
      PROCESS_testerrortimer(&s_automat_errtimer, &err);
      if (err) goto ONERR;
   }

   const uint16_t SIZE = (uint16_t) (2*state_SIZE + state_SIZE_EMPTYTRANS(3));
   if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
      err = malloc_automatmman(ndfa->mman, SIZE, &endstate);
   }
   if (err) goto ONERR;

   state_t * startstate = (void*) ((uint8_t*)endstate + (state_SIZE + state_SIZE_EMPTYTRANS(1)));
   state_t * start1, * end1;
   startend_automat(ndfa, &start1, &end1);
   state_t * start2, * end2;
   startend_automat(ndfa2cpy, &start2, &end2);

   initempty_state(endstate, endstate);
   initempty2_state(startstate, start1, start2);
   last_emptylist(&end1->emptylist)->state = endstate;
   last_emptylist(&end2->emptylist)->state = endstate;

   // set out
   ndfa->nrstate = 2 + ndfa->nrstate + ndfa2cpy->nrstate;
   ndfa->allocated = SIZE + ndfa->allocated + ndfa2cpy->allocated;
   insertlastPlist_slist(&ndfa->states, &ndfa2cpy->states);
   insertlast_statelist(&ndfa->states, endstate);
   insertfirst_statelist(&ndfa->states, startstate);

   // fast free
   decruse_automatmman(ndfa2cpy->mman);
   if (ndfa2cpy == ndfa2) *ndfa2 = (automat_t) automat_FREE;

   return 0;
ONERR:
   if (ndfa2cpy != ndfa2) {
      initmove_automat(ndfa2, ndfa2cpy);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: optimize

static int follow_empty_transition(multistate_t *multistate, automat_mman_t *mman)
{
   int err;
   statearray_t stateemptylist = statearray_FREE;
   state_t *next;

   err = init_statearray(&stateemptylist);
   if (err) goto ONERR;

   // === transfer states with empty transitions from multistate into stateemptylist
   {
      multistate_iter_t iter;
      init_multistateiter(&iter, multistate);
      while (next_multistateiter(&iter, &next)) {
         if (next->nremptytrans) {
            err = insert1_statearray(&stateemptylist, next);
            if (err) goto ONERR;
         }
      }
   }
   swap1and2_statearray(&stateemptylist);

   // === extend multistate with empty transition targets ===
   while (0 == remove2_statearray(&stateemptylist, &next)) {
      do {
         foreach (_emptylist, empty_trans, &next->emptylist) {
            err = add_multistate(multistate, mman, empty_trans->state);
            if (!err) {
               if (empty_trans->state->nremptytrans) {
                  err = insert1_statearray(&stateemptylist, empty_trans->state);
                  if (err) goto ONERR;
                  // now target is followed in next iteration after swap1and2_statearray
               }
            } else if (err != EEXIST) {
               goto ONERR;
            }
         }
      } while (0 == remove2_statearray(&stateemptylist, &next));
      swap1and2_statearray(&stateemptylist);
   }

   err = free_statearray(&stateemptylist);
   if (err) goto ONERR;

   return 0;
ONERR:
   free_statearray(&stateemptylist);
   TRACEEXIT_ERRLOG(err);
   return err;
}

static int build_rangemap_from_statevector(/*out*/rangemap_t *rmap, automat_mman_t *mman, statevector_t *svec)
{
   int err;

   *rmap = (rangemap_t) rangemap_INIT;

   // step 1: build non overlapping ranges stored in rmap, split them if necessary
   for (size_t i = 0; i < svec->nrstate; ++i) {
      state_t *state = svec->state[i];
      foreach (_rangelist, range_trans, &state->rangelist) {
         err = addrange_rangemap(rmap, mman, range_trans->from, range_trans->to);
         if (err) goto ONERR;
      }
   }

   // step 2: add states to non overlapping ranges
   for (size_t i = 0; i < svec->nrstate; ++i) {
      state_t *state = svec->state[i];
      foreach (_rangelist, range_trans, &state->rangelist) {
         err = addstate_rangemap(rmap, mman, range_trans->from, range_trans->to, range_trans->state);
         if (err) goto ONERR;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: makedfa_automat
 *
 * Wie funktioniert das?
 *
 *
 * */
int makedfa_automat(automat_t* ndfa)
{
   int err;
   void* addr;
   size_t          nrstate = 0;
   size_t          allocated = 0;
   slist_t         dfa_states = slist_INIT;
   multistate_t    multistate = multistate_INIT;
   statevector_t   * new_statevec = 0;
   automat_mman_t  * mman[4] = { 0 };
   slist_t         unprocessed;
   patriciatrie_t  svec_index;
   enum { DFA, STATEVEC, RANGEMAP, MULTISTATE };
   state_t *startstate, *endstate;


   if (! ndfa->mman) {
      err = EINVAL;
      goto ONERR;
   }

   // init local var
   init_patriciatrie(&svec_index, keyadapter_statevector());
   startend_automat(ndfa, &startstate, &endstate);
   for (unsigned i = 0; i < lengthof(mman); ++i) {
      err = new_automatmman(&mman[i]);
      if (err) goto ONERR;
   }

   // generate start state of type statevector_t
   err = add_multistate(&multistate, mman[MULTISTATE], startstate);
   if (err) goto ONERR;
   err = follow_empty_transition(&multistate, mman[MULTISTATE]);
   if (err) goto ONERR;
   // === convert type from multistate_t into statevector_t
   err = init_statevector(&new_statevec, mman[STATEVEC], &multistate);
   if (err) goto ONERR;
   initsingle_stateveclist(&unprocessed, new_statevec);
   err = insert_patriciatrie(&svec_index, &new_statevec->index, 0);
   if (err) goto ONERR;
   reset_automatmman(mman[MULTISTATE]);

   void* dfa_endstate;
   {
      const uint16_t SIZE = (uint16_t) (state_SIZE + state_SIZE_EMPTYTRANS(1));
      if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
         err = malloc_automatmman(mman[DFA], SIZE, &dfa_endstate);
      }
      if (err) goto ONERR;
      ++ nrstate;
      allocated += SIZE;
      initempty_state(dfa_endstate, dfa_endstate);
   }

   // process all unprocessed statevector_t
   //   (every statevecor_t is a single state in the new build dfa)
   // the first processed state is also the start state
   while (! isempty_slist(&unprocessed)) {
      statevector_t *statevec = removefirst_stateveclist(&unprocessed);

      // build rangemap_t from statevec
      // For every r in ranges:
      //  extend r.state with all reachable states by following empty transitions
      //  convert r.state of type multistate_t into type statevector_t
      //  add r as transition to new dfa-state with pointer to converted statevector

      rangemap_t rmap;
      err = build_rangemap_from_statevector(&rmap, mman[RANGEMAP], statevec);
      if (err) goto ONERR;

      const bool isendstate = iscontained_statevector(statevec, endstate);

      if (isendstate && rmap.size == 0 && nrstate != 1/*not start state*/) {
         // optimization if state is only an end state (except for start state)
         // do not generate state which contains only a single empty transition to end state
         statevec->dfa = dfa_endstate;
         continue; // while (! isempty_slist(&unprocessed))
      }

      state_t* dfastate;
      {
         const uint16_t SIZE = (uint16_t) (state_SIZE + (isendstate ? state_SIZE_EMPTYTRANS(1) : 0));
         if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
            err = malloc_automatmman(mman[DFA], SIZE, &addr);
         }
         if (err) goto ONERR;
         allocated += SIZE;
         dfastate = addr;
         if (isendstate) {
            initempty_state(dfastate, dfa_endstate);
         } else {
            initrange_state(dfastate, 0, 0, 0, 0);
         }
      }

      statevec->dfa = dfastate;
      ++ nrstate;
      insertlast_statelist(&dfa_states, dfastate);

      range_transition_t* prevtrans = 0;

      range_t* range;
      rangemap_iter_t iter;
      init_rangemapiter(&iter, &rmap);
      while (next_rangemapiter(&iter, &range)) {
         err = follow_empty_transition(&range->multistate, mman[MULTISTATE]);
         if (err) goto ONERR;
         err = init_statevector(&new_statevec, mman[STATEVEC], &range->multistate);
         if (err) goto ONERR;
         reset_automatmman(mman[MULTISTATE]);
         patriciatrie_node_t* existing_node;
         err = insert_patriciatrie(&svec_index, &new_statevec->index, &existing_node);
         if (! err) {
            insertlast_stateveclist(&unprocessed, new_statevec);
         } else {
            if (err != EEXIST) goto ONERR;
            mfreelast_automatmman(mman[STATEVEC], new_statevec);
            new_statevec = (void*) ((uintptr_t)existing_node - offsetof(statevector_t, index));
         }
         if (  prevtrans && (state_t*)new_statevec == prevtrans->state
               && range->from == prevtrans->to+1) {
            // transition optimizer
            prevtrans->to = range->to;
         } else {
            // add new transition
            const uint16_t SIZE = (uint16_t) (state_SIZE_RANGETRANS(1));
            if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
               err = malloc_automatmman(mman[DFA], SIZE, &addr);
            }
            if (err) goto ONERR;
            allocated += SIZE;
            prevtrans = addr;
            ++ dfastate->nrrangetrans;
            insertlast_rangelist(&dfastate->rangelist, prevtrans);
            prevtrans->state = (state_t*) new_statevec;
            prevtrans->from  = range->from;
            prevtrans->to    = range->to;
         }
      }
      reset_automatmman(mman[RANGEMAP]);
   }

   // set end state as last state in list
   insertlast_statelist(&dfa_states, dfa_endstate);

   // convert all pointers of transitions from dfa
   // from pointing to statevector_t into pointers to state_t
   foreach (_statelist, dfastate, &dfa_states) {
      foreach (_rangelist, range_trans, &dfastate->rangelist) {
         range_trans->state = ((statevector_t*)range_trans->state)->dfa;
      }
   }

   for (unsigned i = 0; i < lengthof(mman); ++i) {
      if (i == DFA) continue;
      err = delete_automatmman(&mman[i]);
      if (err) goto ONERR;
   }
   automat_mman_t * dfa_mman = mman[DFA];
   mman[DFA] = 0;

   // set out (change ndfa even in case of error to valid state)
   incruse_automatmman(dfa_mman);
   err = free_automat(ndfa);
   ndfa->mman = dfa_mman;
   ndfa->nrstate = nrstate;
   ndfa->allocated = allocated;
   ndfa->states = dfa_states;
   if (err) goto ONERR;

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(mman); ++i) {
      delete_automatmman(&mman[i]);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int minimize_automat(automat_t* ndfa)
{
   int err;
   automat_t ndfa2 = automat_FREE;
   automat_t ndfa3 = automat_FREE;

   err = initreverse_automat(&ndfa2, ndfa, 0);
   if (err) goto ONERR;
   err = makedfa_automat(&ndfa2);
   if (err) goto ONERR;
   err = initreverse_automat(&ndfa3, &ndfa2, 0);
   if (err) goto ONERR;
   err = free_automat(&ndfa2);
   if (err) goto ONERR;
   err = makedfa_automat(&ndfa3);
   if (err) goto ONERR;
   err = free_automat(ndfa);
   initmove_automat(ndfa, &ndfa3);
   if (err) goto ONERR;

   return 0;
ONERR:
   (void) free_automat(&ndfa2);
   (void) free_automat(&ndfa3);
   TRACEEXIT_ERRLOG(err);
   return err;
}

typedef enum { OP_AND, OP_AND_NOT } op_e;

static int makedfa2_automat(automat_t* ndfa, op_e op, const automat_t* ndfa2)
{
   int err;
   void* addr;
   size_t          nrstate = 0;
   size_t          allocated = 0;
   slist_t         dfa_states = slist_INIT;
   multistate_t    multistate = multistate_INIT;
   statevector_t   * new_statevec = 0;
   automat_mman_t  * mman[4] = { 0 };
   slist_t         unprocessed;
   patriciatrie_t  svec_index;
   enum { DFA, STATEVEC, RANGEMAP, MULTISTATE };
   state_t *startstate,  *endstate;
   state_t *startstate2, *endstate2;

   if (! ndfa->mman || !ndfa2->mman) {
      err = EINVAL;
      goto ONERR;
   }

   // init local var
   init_patriciatrie(&svec_index, keyadapter_statevector());
   startend_automat(ndfa, &startstate, &endstate);
   startend_automat(ndfa2, &startstate2, &endstate2);
   for (unsigned i = 0; i < lengthof(mman); ++i) {
      err = new_automatmman(&mman[i]);
      if (err) goto ONERR;
   }

   // flag used to discriminate between the owner automaton of the state
   foreach (_statelist, s, &ndfa->states) {
      s->isused = 1;
   }
   foreach (_statelist, s, &ndfa2->states) {
      s->isused = 2;
   }

   // generate start state of type statevector_t
   err = add_multistate(&multistate, mman[MULTISTATE], startstate);
   if (err) goto ONERR;
   err = add_multistate(&multistate, mman[MULTISTATE], startstate2);
   if (err) goto ONERR;
   err = follow_empty_transition(&multistate, mman[MULTISTATE]);
   if (err) goto ONERR;
   // === convert type from multistate_t into statevector_t
   err = init_statevector(&new_statevec, mman[STATEVEC], &multistate);
   if (err) goto ONERR;
   initsingle_stateveclist(&unprocessed, new_statevec);
   err = insert_patriciatrie(&svec_index, &new_statevec->index, 0);
   if (err) goto ONERR;
   reset_automatmman(mman[MULTISTATE]);

   void* dfa_endstate;
   {
      const uint16_t SIZE = (uint16_t) (state_SIZE + state_SIZE_EMPTYTRANS(1));
      if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
         err = malloc_automatmman(mman[DFA], SIZE, &dfa_endstate);
      }
      if (err) goto ONERR;
      ++ nrstate;
      allocated += SIZE;
      initempty_state(dfa_endstate, dfa_endstate);
   }

   // process all unprocessed statevector_t
   //   (every statevecor_t is a single state in the new build dfa)
   // the first processed state is also the start state
   while (! isempty_slist(&unprocessed)) {
      statevector_t *statevec = removefirst_stateveclist(&unprocessed);

      // build rangemap_t from statevec
      // For every r in ranges:
      //  extend r.state with all reachable states by following empty transitions
      //  convert r.state of type multistate_t into type statevector_t
      //  add r as transition to new dfa-state with pointer to converted statevector

      rangemap_t rmap;
      err = build_rangemap_from_statevector(&rmap, mman[RANGEMAP], statevec);
      if (err) goto ONERR;

      const bool isendstate = iscontained_statevector(statevec, endstate)
                              && ((op == OP_AND && iscontained_statevector(statevec, endstate2))
                                  || (op == OP_AND_NOT && ! iscontained_statevector(statevec, endstate2)));

      if (isendstate && rmap.size == 0 && nrstate != 1/*not start state*/) {
         // optimization if state is only an end state (except for start state)
         // do not generate state which contains only a single empty transition to end state
         statevec->dfa = dfa_endstate;
         continue; // while (! isempty_slist(&unprocessed))
      }

      state_t* dfastate;
      {
         const uint16_t SIZE = (uint16_t) (state_SIZE + (isendstate ? state_SIZE_EMPTYTRANS(1) : 0));
         if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
            err = malloc_automatmman(mman[DFA], SIZE, &addr);
         }
         if (err) goto ONERR;
         allocated += SIZE;
         dfastate = addr;
         if (isendstate) {
            initempty_state(dfastate, dfa_endstate);
         } else {
            initrange_state(dfastate, 0, 0, 0, 0);
         }
      }

      range_transition_t* prevtrans = 0;

      range_t* range;
      rangemap_iter_t iter;
      init_rangemapiter(&iter, &rmap);
      while (next_rangemapiter(&iter, &range)) {
         err = follow_empty_transition(&range->multistate, mman[MULTISTATE]);
         if (err) goto ONERR;
         err = init_statevector(&new_statevec, mman[STATEVEC], &range->multistate);
         if (err) goto ONERR;
         reset_automatmman(mman[MULTISTATE]);
         if (! isinuse12_statevector(new_statevec, op == OP_AND)) {
            // Optimization: end state can not be reached by new_statevec
            //               ==> do not add any transition
            mfreelast_automatmman(mman[STATEVEC], new_statevec);
            continue;
         }
         patriciatrie_node_t* existing_node;
         err = insert_patriciatrie(&svec_index, &new_statevec->index, &existing_node);
         if (! err) {
            insertlast_stateveclist(&unprocessed, new_statevec);
         } else {
            if (err != EEXIST) goto ONERR;
            mfreelast_automatmman(mman[STATEVEC], new_statevec);
            new_statevec = (void*) ((uintptr_t)existing_node - offsetof(statevector_t, index));
         }
         if (  prevtrans && (state_t*)new_statevec == prevtrans->state
               && range->from == prevtrans->to+1) {
            // transition optimizer
            prevtrans->to = range->to;
         } else {
            // add new transition
            const uint16_t SIZE = (uint16_t) (state_SIZE_RANGETRANS(1));
            if (! PROCESS_testerrortimer(&s_automat_errtimer, &err)) {
               err = malloc_automatmman(mman[DFA], SIZE, &addr);
            }
            if (err) goto ONERR;
            allocated += SIZE;
            prevtrans = addr;
            ++ dfastate->nrrangetrans;
            insertlast_rangelist(&dfastate->rangelist, prevtrans);
            prevtrans->state = (state_t*) new_statevec;
            prevtrans->from  = range->from;
            prevtrans->to    = range->to;
         }
      }
      reset_automatmman(mman[RANGEMAP]);
      if (! dfastate->nrrangetrans && isendstate && nrstate != 1/*not start state*/) {
         // remove empty state
         const uint16_t SIZE = (uint16_t) (state_SIZE + state_SIZE_EMPTYTRANS(1));
         statevec->dfa = dfa_endstate;
         mfreelast_automatmman(mman[DFA], dfastate);
         allocated -= SIZE;
      } else {
         statevec->dfa = dfastate;
         ++ nrstate;
         insertlast_statelist(&dfa_states, dfastate);
      }
   }

   // set end state as last state in list
   insertlast_statelist(&dfa_states, dfa_endstate);

   // convert all pointers of transitions from dfa
   // from pointing to statevector_t into pointers to state_t
   foreach (_statelist, dfastate, &dfa_states) {
      foreach (_rangelist, range_trans, &dfastate->rangelist) {
         range_trans->state = ((statevector_t*)range_trans->state)->dfa;
      }
   }

   for (unsigned i = 0; i < lengthof(mman); ++i) {
      if (i == DFA) continue;
      err = delete_automatmman(&mman[i]);
      if (err) goto ONERR;
   }
   automat_mman_t * dfa_mman = mman[DFA];
   mman[DFA] = 0;

   // set out (change ndfa even in case of error in free to valid state)
   incruse_automatmman(dfa_mman);
   err = free_automat(ndfa);
   ndfa->mman = dfa_mman;
   ndfa->nrstate = nrstate;
   ndfa->allocated = allocated;
   ndfa->states = dfa_states;
   if (err) goto ONERR;

   return 0;
ONERR:
   for (unsigned i = 0; i < lengthof(mman); ++i) {
      delete_automatmman(&mman[i]);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int opand_automat(automat_t* restrict ndfa, const automat_t* restrict ndfa2)
{
   int err;

   err = makedfa2_automat(ndfa, OP_AND, ndfa2);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int opandnot_automat(automat_t* restrict ndfa, const automat_t* restrict ndfa2)
{
   int err;

   err = makedfa2_automat(ndfa, OP_AND_NOT, ndfa2);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int opnot_automat(automat_t* restrict ndfa)
{
   int err;
   automat_t all = automat_FREE;

   if (ndfa->mman == 0) {
      err = EINVAL;
      goto ONERR;
   }

   err = initmatch_automat(&all, ndfa, 1, (char32_t[]){0}, (char32_t[]){(char32_t)-1});
   if (err) goto ONERR;
   err = makedfa2_automat(&all, OP_AND_NOT, ndfa);
   if (err) goto ONERR;
   err = free_automat(ndfa);
   initmove_automat(ndfa, &all);
   if (err) goto ONERR;

   return 0;
ONERR:
   (void) free_automat(&all);
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

   TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
   TEST(0 == malloc_automatmman(mman, SIZE, &mst->root));
   mst->size = nrchild * multistate_NROFSTATE;
   TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
   ((multistate_node_t*)mst->root)->level = 1;
   ((multistate_node_t*)mst->root)->size  = (uint8_t) nrchild;

   void * prevchild = 0;
   for (unsigned i = 0; i < nrchild; ++i) {
      TEST(0 == malloc_automatmman(mman, SIZE, &child[i]));
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[2+i]));
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

   TEST(0 == malloc_automatmman(mman, SIZE, &mst->root));
   mst->size = nrchild * level1_size;
   ((multistate_node_t*)mst->root)->level = 2;
   ((multistate_node_t*)mst->root)->size  = (uint8_t) nrchild;

   for (uintptr_t i = 0; i < nrchild; ++i) {
      TEST(0 == malloc_automatmman(mman, SIZE, &child[i]));
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
         TEST(0 == malloc_automatmman(mman, SIZE, &leaf));
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

   TEST(0 == malloc_automatmman(mman, SIZE, &node));
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
      TEST(0 == malloc_automatmman(mman, SIZE, &node));
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

   TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
   TEST(0 == malloc_automatmman(mman, SIZE, &node));
   rmap->root = node;
   rmap->size = nrchild * rangemap_NROFRANGE;
   TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
   rmap->root->level = 1;
   rmap->root->size  = (uint8_t) nrchild;

   for (unsigned i = 0, f = 0; i < nrchild; ++i) {
      TEST(0 == malloc_automatmman(mman, SIZE, &node));
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[2+i]));
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

   TEST(0 == malloc_automatmman(mman, SIZE, &node));
   rmap->root = node;
   rmap->size = nrchild * level1_size;
   rmap->root->level = 2;
   rmap->root->size  = (uint8_t) nrchild;

   for (unsigned i = 0; i < nrchild; ++i) {
      TEST(0 == malloc_automatmman(mman, SIZE, &node));
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
         TEST(0 == malloc_automatmman(mman, SIZE, &node));
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
   empty_transition_t* empty_trans = (void*)((uintptr_t)state + state_SIZE);
   range_transition_t* range_trans = (void*)((uintptr_t)state + state_SIZE);
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
   static_assert( state_SIZE == sizeof(state_t), "objectsize");

   // TEST state_SIZE_EMPTYTRANS
   static_assert( 0 == state_SIZE_EMPTYTRANS(0)
                  && sizeof(empty_transition_t) == state_SIZE_EMPTYTRANS(1)
                  && 255*sizeof(empty_transition_t) == state_SIZE_EMPTYTRANS(255),
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
   memset(&state, 255, sizeof(state[0]));
   state[0].next = (void*) &state[1].next;
   initempty_state(&state[0], &state[2]);
   TEST(state[0].next == (void*) &state[1].next); // unchanged
   TEST(state[0].nremptytrans == 1);
   TEST(state[0].nrrangetrans == 0);
   TEST(state[0].emptylist.last == (slist_node_t*)&empty_trans->next);
   TEST(state[0].rangelist.last == 0);
   TEST(state[0].isused    == 255); // unchanged
   TEST(empty_trans->next  == (slist_node_t*)&empty_trans->next);
   TEST(empty_trans->state == &state[2]);

   // TEST initempty2_state
   memset(&state, 255, sizeof(state[0]));
   state[0].next = (void*) &state[1].next;
   initempty2_state(&state[0], &state[2], &state[5]);
   TEST(state[0].next == (void*) &state[1].next); // unchanged
   TEST(state[0].nremptytrans == 2);
   TEST(state[0].nrrangetrans == 0);
   TEST(state[0].emptylist.last == (slist_node_t*)&empty_trans[1].next);
   TEST(state[0].rangelist.last == 0);
   TEST(state[0].isused      == 255); // unchanged
   TEST(empty_trans[0].next  == (slist_node_t*)&empty_trans[1].next);
   TEST(empty_trans[0].state == &state[2]);
   TEST(empty_trans[1].next  == (slist_node_t*)&empty_trans[0].next);
   TEST(empty_trans[1].state == &state[5]);

   // TEST initrange_state
   static_assert( sizeof(state) > state_SIZE + state_SIZE_RANGETRANS(256),
                  "initrange_state does not overflow state array");
   for (unsigned i = 0; i < 256; ++i) {
      memset(&state, 0, sizeof(state));
      memset(&state, 255, sizeof(state[0]));
      state[0].next = (void*) &state[1].next;
      initrange_state(&state[0], &state[3], (uint8_t)i, from, to);
      TEST(state[0].next == (void*) &state[1].next); // unchanged
      TEST(state[0].nremptytrans == 0);
      TEST(state[0].nrrangetrans == i);
      TEST(state[0].emptylist.last == 0);
      TEST(state[0].rangelist.last == (i ? (slist_node_t*)&range_trans[i-1].next : 0));
      TEST(state[0].isused == 255); // unchanged
      for (unsigned r = 0; r < i; ++r) {
         TEST(range_trans[r].next  == (slist_node_t*) &range_trans[r<i-1?r+1:0].next);
         TEST(range_trans[r].state == &state[3]);
         TEST(range_trans[r].from  == (unsigned) (r+1));
         TEST(range_trans[r].to    == (unsigned) (r+10));
      }
      for (unsigned r = i; r <= 255; ++r) {
         TEST(range_trans[r].next  == 0);
         TEST(range_trans[r].state == 0);
         TEST(range_trans[r].from  == 0);
         TEST(range_trans[r].to    == 0);
      }
   }

   // TEST initcontinue_state
   static_assert( sizeof(state) > state_SIZE + state_SIZE_RANGETRANS(256),
                  "initcontinue_state does not overflow state array");
   memset(&state, 0, sizeof(state));
   state[0].next = (void*) &state[1].next;
   state[0].nremptytrans = 2;
   state[0].emptylist.last = (void*)3;
   for (unsigned i = 0, s = 0; s+i < 256; ++i) {
      extendmatch_state(&state[0], &state[3], (uint8_t)i, from+s, to+s, range_trans+s);
      s += i;
      TEST(state[0].next == (void*) &state[1].next);  // unchanged
      TEST(state[0].nremptytrans == 2);               // unchanged
      TEST(state[0].nrrangetrans == s);
      TEST(state[0].emptylist.last == (void*)3);      // unchanged
      TEST(state[0].rangelist.last == (s ? (slist_node_t*)&range_trans[s-1].next : 0));
      for (unsigned r = 0; r < s; ++r) {
         TEST(range_trans[r].next  == (slist_node_t*) &range_trans[r<s-1?r+1:0].next);
         TEST(range_trans[r].state == &state[3]);
         TEST(range_trans[r].from  == (unsigned) (r+1));
         TEST(range_trans[r].to    == (unsigned) (r+10));
      }
      for (unsigned r = s; r <= 255; ++r) {
         TEST(range_trans[r].next  == 0);
         TEST(range_trans[r].state == 0);
         TEST(range_trans[r].from  == 0);
         TEST(range_trans[r].to    == 0);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_statearray(void)
{
   statearray_t arr = statearray_FREE;
   state_t    * state;
   size_t       size;
   size_t       L;
   statearray_iter_t iter;

   // prepare
   for(L = 0; ((L+1)*sizeof(state_t*) + sizeof(statearray_block_t)) <= sizeblock_statearray(); ++L)
      ;

   // TEST statearray_FREE
   TEST( 0 == arr.mman);
   TEST( 0 == arr.length_of_block);
   TEST( isempty_slist(&arr.addlist));
   TEST( isempty_slist(&arr.dellist));
   TEST( isempty_slist(&arr.freelist));
   TEST( 0 == arr.addnext);
   TEST( 0 == arr.addend);
   TEST( 0 == arr.delblock);
   TEST( 0 == arr.delnext);
   TEST( 0 == arr.delend);

   // TEST sizeblock_statearray
   TEST(16384 == sizeblock_statearray());

   // TEST init_statearray
   memset(&arr, 255, sizeof(arr));
   arr.mman = 0;
   size = SIZEALLOCATED_PAGECACHE();
   // test
   TEST( 0 == init_statearray(&arr));
   // check env
   TEST( size < SIZEALLOCATED_PAGECACHE());
   // check arr
   TEST( 0 != arr.mman);
   TEST( L == arr.length_of_block);
   TEST( isempty_slist(&arr.addlist));
   TEST( isempty_slist(&arr.dellist));
   TEST( isempty_slist(&arr.freelist));
   TEST( 0 == arr.addnext);
   TEST( 0 == arr.addend);
   TEST( 0 == arr.delblock);
   TEST( 0 == arr.delnext);
   TEST( 0 == arr.delend);

   // TEST free_statearray: frees only mman && double free
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST( 0 == free_statearray(&arr));
      // check env
      TEST( size == SIZEALLOCATED_PAGECACHE());
      // check arr
      TEST( 0 == arr.mman);
      TEST( L == arr.length_of_block); // untouched
      // ...
   }

   // TEST insert1_statearray: addnext == addend == 0 || (addnext < addend)
   TEST(0 == init_statearray(&arr));
   for (uintptr_t i = 1; i <= L; ++i) {
      // test
      TEST( 0 == insert1_statearray(&arr, (state_t*)i));
      // check mman
      TEST( sizeblock_statearray() == sizeallocated_automatmman(arr.mman));
      // check arr
      TEST( 0 != arr.mman);
      TEST( L == arr.length_of_block);
      statearray_block_t * block = first_blocklist(&arr.addlist);
      TEST( block != 0);
      TEST( block == first_blocklist(&arr.addlist));
      TEST( block == last_blocklist(&arr.addlist))
      TEST( isempty_slist(&arr.dellist));
      TEST( isempty_slist(&arr.freelist));
      TEST( &block->state[i] == arr.addnext);
      TEST( &block->state[L] == arr.addend);
      TEST( 0 == arr.delblock);
      TEST( 0 == arr.delnext);
      TEST( 0 == arr.delend);
      // check first_blocklist content
      TEST( block->next    == (slist_node_t*)block);
      TEST( block->nrstate == 0);
      TEST( block->state[i-1] == (state_t*)i);
      if (i == L) {
         for (uintptr_t i2 = 0; i2 < L; ++i2) {
            TEST( block->state[i2] == (state_t*)(1+i2));
         }
      }
   }

   // TEST insert1_statearray: addnext == addend
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   TEST(0 == insert1_statearray(&arr, (state_t*)(void*)1));
   statearray_block_t * block[6] = { first_blocklist(&arr.addlist), 0 };
   for (uintptr_t i = 2; i <= lengthof(block); ++i) {
      // test
      arr.addnext = arr.addend;
      TEST( 0 == insert1_statearray(&arr, (state_t*)i));
      // check mman
      TEST( i*sizeblock_statearray() == sizeallocated_automatmman(arr.mman));
      // check arr
      block[i-1] = last_blocklist(&arr.addlist);
      TEST( 0 != arr.mman);
      TEST( L == arr.length_of_block);
      TEST( block[0]   == first_blocklist(&arr.addlist));
      TEST( block[i-1] != block[0]);
      TEST( isempty_slist(&arr.dellist));
      TEST( isempty_slist(&arr.freelist));
      TEST( &block[i-1]->state[1] == arr.addnext);
      TEST( &block[i-1]->state[L] == arr.addend);
      TEST( 0 == arr.delblock);
      TEST( 0 == arr.delnext);
      TEST( 0 == arr.delend);
      // check previous last_blocklist content
      TEST( block[i-2]->next    == (slist_node_t*)block[i-1]);
      TEST( block[i-2]->nrstate == L);
      // check last_blocklist content
      TEST( block[i-1]->next     == (slist_node_t*)block[0]);
      TEST( block[i-1]->nrstate  == 0);
      TEST( block[i-1]->state[0] == (state_t*)i);
   }

   // prepare remove2_statearray
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   for (uintptr_t i = 0; i < lengthof(block); ++i) {
      for (uintptr_t i2 = 1; i2 <= 7*(i+1); ++i2) {
         TEST(0 == insert1_statearray(&arr, (state_t*)(i+i2)));
      }
      block[i] = last_blocklist(&arr.addlist);
      arr.addend = arr.addnext;
   }
   block[lengthof(block)-1]->nrstate = (size_t) (arr.addnext - &block[lengthof(block)-1]->state[0]);
   TEST(isempty_slist(&arr.dellist));
   TEST(isempty_slist(&arr.freelist));
   arr.addend  = 0;
   arr.addnext = 0;
   TEST(0 == arr.delblock);
   TEST(0 == arr.delnext);
   TEST(0 == arr.delend);
   arr.dellist = arr.addlist;
   arr.addlist = (slist_t) slist_INIT;

   // TEST remove2_statearray
   for (uintptr_t i = 0; i < lengthof(block); ++i) {
      for (uintptr_t i2 = 1; i2 <= 7*(i+1); ++i2) {
         // test
         state_t * removedstate = 0;
         TEST( 0 == remove2_statearray(&arr, &removedstate));
         // check mman
         TEST( lengthof(block)*sizeblock_statearray() == sizeallocated_automatmman(arr.mman));
         // check removedstate
         TEST( removedstate == (state_t*)(i+i2));
         // check arr
         TEST( 0 != arr.mman);
         TEST( L == arr.length_of_block);
         TEST( isempty_slist(&arr.addlist));
         if (i == lengthof(block)-1) {
            TEST( isempty_slist(&arr.dellist));
         } else {
            TEST( ! isempty_slist(&arr.dellist));
            TEST( block[i+1] == first_blocklist(&arr.dellist));
         }
         if (i == 0) {
            TEST( isempty_slist(&arr.freelist));
         } else {
            TEST( ! isempty_slist(&arr.freelist));
            TEST( block[i-1] == last_blocklist(&arr.freelist));
         }
         TEST( 0 == arr.addnext);
         TEST( 0 == arr.addend);
         TEST( block[i] == arr.delblock);
         TEST( arr.delnext == &arr.delblock->state[i2]);
         TEST( arr.delend  == &arr.delblock->state[7*(i+1)]);
      }
   }

   // TEST remove2_statearray: ENODATA (more than one time does not harm)
   for (unsigned rep = 0; rep < 5; ++rep) {
      state_t * removedstate = 0;
      TEST( ENODATA == remove2_statearray(&arr, &removedstate));
      // check mman
      TEST( lengthof(block)*sizeblock_statearray() == sizeallocated_automatmman(arr.mman));
      // check removedstate
      TEST( removedstate == 0);
      // check arr
      TEST( 0 != arr.mman);
      TEST( L == arr.length_of_block);
      TEST( isempty_slist(&arr.addlist));
      TEST( isempty_slist(&arr.dellist));
      TEST( ! isempty_slist(&arr.freelist));
      TEST( 0 == arr.addnext);
      TEST( 0 == arr.addend);
      TEST( 0 == arr.delblock);
      TEST( arr.delnext == arr.delend);
      TEST( arr.delend  == &block[lengthof(block)-1]->state[7*lengthof(block)]);
      // check freelist content
      unsigned i = 0;
      foreach (_blocklist, b, &arr.freelist) {
         TEST( i < lengthof(block));
         TEST( block[i++] == b);
      }
   }

   // prepare swap1and2_statearray
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   for (unsigned i = 0; i < lengthof(block); ++i) {
      for (uintptr_t i2 = 1; i2 <= 3; ++i2) {
         TEST(0 == insert1_statearray(&arr, (state_t*)(3*i+i2)));
      }
      block[i] = last_blocklist(&arr.addlist);
      arr.addend = arr.addnext;
   }
   for (unsigned i = 0; i < lengthof(block)/2; ++i) {
      statearray_block_t *firstblock;
      firstblock = removefirst_blocklist(&arr.addlist);
      insertlast_blocklist(&arr.dellist, firstblock);
   }
   for (uintptr_t i = 1; i <= 4; ++i) {
      TEST(0 == remove2_statearray(&arr, &state));
      TEST(i == (uintptr_t)state);
   }
   TEST(! isempty_slist(&arr.addlist));
   TEST(last_blocklist(&arr.addlist) == block[lengthof(block)-1]);
   TEST(0 == block[lengthof(block)-1]->nrstate);
   TEST(! isempty_slist(&arr.dellist));
   TEST(! isempty_slist(&arr.freelist));
   TEST(0 != arr.addend)
   TEST(0 != arr.addnext);
   TEST(0 != arr.delblock);
   TEST(0 != arr.delnext);
   TEST(0 != arr.delend);

   // TEST swap1and2_statearray
   swap1and2_statearray(&arr);
   // check mman
   TEST( lengthof(block)*sizeblock_statearray() == sizeallocated_automatmman(arr.mman));
   // check arr
   TEST( 0 != arr.mman);
   TEST( L == arr.length_of_block);
   TEST( isempty_slist(&arr.addlist));
   TEST( ! isempty_slist(&arr.dellist));
   TEST( ! isempty_slist(&arr.freelist));
   TEST( 0 == arr.addnext);
   TEST( 0 == arr.addend);
   TEST( 0 == arr.delblock);
   TEST( 0 == arr.delnext);
   TEST( 0 == arr.delend);
   // check freelist content (former dellist + delblock)
   {
      unsigned i = 0;
      foreach (_blocklist, b, &arr.freelist) {
         TEST( i < lengthof(block)/2);
         TEST( 3 == b->nrstate);
         TEST( b == block[i++]);
      }
      TEST( i == lengthof(block)/2);
   }
   // check dellist content (former addlist)
   {
      unsigned i = lengthof(block)/2;
      foreach (_blocklist, b, &arr.dellist) {
         TEST( i < lengthof(block));
         TEST( 3 == b->nrstate);
         TEST( b == block[i++]);
      }
      TEST( i == lengthof(block));
   }
   for (uintptr_t i = 3*lengthof(block)/2+1; i <= 3*lengthof(block); ++i) {
      TEST(0 == remove2_statearray(&arr, &state));
      TEST(i == (uintptr_t)state);
   }
   TEST(ENODATA == remove2_statearray(&arr, &state));

   // TEST init_statearrayiter: empty statearray
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   init_statearrayiter(&iter, &arr);
   // check iter
   TEST( 0 == iter.block);
   TEST( 0 == iter.next);
   TEST( 0 == iter.end);

   // TEST init_statearrayiter: single allocated page
   for (uintptr_t i = 0; i < arr.length_of_block; ++i) {
      TEST(0 == insert1_statearray(&arr, (state_t*)i));
      init_statearrayiter(&iter, &arr);
      // check iter
      TEST( iter.block == first_blocklist(&arr.addlist));
      TEST( iter.next  == first_blocklist(&arr.addlist)->state);
      TEST( iter.end   == first_blocklist(&arr.addlist)->state);
   }

   // TEST init_statearrayiter: multiple allocated pages
   TEST(0 == insert1_statearray(&arr, (state_t*)0));
   init_statearrayiter(&iter, &arr);
   // check iter
   TEST( iter.block == first_blocklist(&arr.addlist));
   TEST( iter.next  == first_blocklist(&arr.addlist)->state);
   TEST( iter.end   == first_blocklist(&arr.addlist)->state + arr.length_of_block);
   for (uintptr_t nrpage = 2; nrpage < 20; ++nrpage) {
      // prepare
      arr.addend = arr.addnext;
      TEST(0 == insert1_statearray(&arr, (state_t*)nrpage));
      // test
      init_statearrayiter(&iter, &arr);
      // check iter
      TEST( iter.block == first_blocklist(&arr.addlist));
      TEST( iter.next  == first_blocklist(&arr.addlist)->state);
      TEST( iter.end   == first_blocklist(&arr.addlist)->state + arr.length_of_block);
   }

   // TEST next_statearrayiter: empty statearray
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   init_statearrayiter(&iter, &arr);
   // test
   TEST( 0 == next_statearrayiter(&iter, &arr, &state));
   // check iter
   TEST( 0 == iter.block);
   TEST( 0 == iter.next);
   TEST( 0 == iter.end);

   // TEST next_statearrayiter: single page statearray
   for (unsigned len = 1, S = 1, S2 = 1; len < 20; ++len, S2 += len) {
      // prepare
      for (uintptr_t i = S; i <= S2; ++i) {
         TEST(0 == insert1_statearray(&arr, (state_t*)i));
      }
      if (len == 1) init_statearrayiter(&iter, &arr);
      for (uintptr_t i = 0; i < len; ++i, ++S) {
         // test
         TEST( 1 == next_statearrayiter(&iter, &arr, &state));
         // check state
         TEST( S == (uintptr_t)state);
         // check iter
         TEST( iter.block == first_blocklist(&arr.addlist));
         TEST( iter.next  == first_blocklist(&arr.addlist)->state + S);
         TEST( iter.end   == first_blocklist(&arr.addlist)->state + S2);
      }
      // test (ENODATA)
      TEST( 0 == next_statearrayiter(&iter, &arr, &state));
      // check state (not changed)
      TEST( S2 == (uintptr_t)state);
      // check iter (not changed)
      TEST( iter.block == first_blocklist(&arr.addlist));
      TEST( iter.next  == first_blocklist(&arr.addlist)->state + S2);
      TEST( iter.end   == first_blocklist(&arr.addlist)->state + S2);
   }

   // TEST next_statearrayiter: multiple page statearray
   TEST(0 == free_statearray(&arr));
   TEST(0 == init_statearray(&arr));
   TEST(0 == insert1_statearray(&arr, (state_t*)1));
   init_statearrayiter(&iter, &arr);
   block[0] = first_blocklist(&arr.addlist);
   for (uintptr_t nrpage = 2; nrpage < 20; ++nrpage, block[0] = next_blocklist(block[0])) {
      arr.addend = arr.addnext;
      TEST(0 == insert1_statearray(&arr, (state_t*)nrpage));
      // test
      TEST( 1 == next_statearrayiter(&iter, &arr, &state));
      // check state
      TEST( nrpage-1 == (uintptr_t)state);
      // check iter
      TEST( last_blocklist(&arr.addlist) == next_blocklist(block[0]));
      TEST( iter.block == block[0]);
      TEST( iter.next  == block[0]->state + 1);
      TEST( iter.end   == block[0]->state + 1);
   }
   for (unsigned tc = 1; tc <= 1; --tc) {
      // test (last + ENODATA)
      TEST( tc == next_statearrayiter(&iter, &arr, &state));
      // check state
      TEST( 19 == (uintptr_t)state);
      // check iter
      TEST( last_blocklist(&arr.addlist) == block[0]);
      TEST( iter.block == block[0]);
      TEST( iter.next  == block[0]->state + 1);
      TEST( iter.end   == block[0]->state + 1);
   }

   // reset
   TEST(0 == free_statearray(&arr));

   return 0;
ONERR:
   free_statearray(&arr);
   return EINVAL;
}

static int test_multistate(void)
{
   const unsigned NROFSTATE = 256;
   state_t        state[NROFSTATE];
   multistate_t   mst = multistate_INIT;
   automat_mman_t* mman = 0;
   multistate_iter_t iter;
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
      for (unsigned tc = 0; tc <= 1; ++tc) {
         // test
         TEST( (tc ? EEXIST : 0) == add_multistate(&mst, mman, &state[i]));
         // check mman: nothing allocated
         TEST( 0 == sizeallocated_automatmman(mman));
         // check mst
         TEST( 1 == mst.size);
         TEST( &state[i] == mst.root);
      }
   }

   // TEST add_multistate: multistate_t.size == 1
   for (unsigned i = 0; i < NROFSTATE-1; ++i) {
      for (unsigned order = 0; order <= 1; ++order) {
         // prepare
         mst = (multistate_t) multistate_INIT;
         TEST(0 == add_multistate(&mst, mman, &state[i + !order]));
         for (unsigned tc = 0; tc <= 1; ++tc) {
            // test
            TEST( (tc ? EEXIST : 0) == add_multistate(&mst, mman, &state[i + order]));
            // check mman
            TEST( sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
            // check mst
            TEST( 0 == invariant_multistate(&mst));
            TEST( 2 == mst.size);
            TEST( 0 != mst.root);
            // check mst.root content
            TEST( 0 == ((multistate_node_t*)mst.root)->level);
            TEST( 2 == ((multistate_node_t*)mst.root)->size);
            TEST( &state[i] == ((multistate_node_t*)mst.root)->state[0]);
            TEST( &state[i+1] == ((multistate_node_t*)mst.root)->state[1]);
         }
         // reset
         reset_automatmman(mman);
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
            TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr));
            *((void**)addr) = ENDMARKER;
            // check that end marker is effective !!
            TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         }
         // check mman
         TEST( (i > 0 ? sizeof(void*)+sizeof(multistate_node_t) : 0) == sizeallocated_automatmman(mman));
         // check mst
         TEST( 0 == invariant_multistate(&mst));
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
      reset_automatmman(mman);
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
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr));
         *((void**)addr) = ENDMARKER;
         TEST(addr == &((multistate_node_t*)mst.root)->state[multistate_NROFSTATE]);
         // test
         TEST( 0 == add_multistate(&mst, mman, &state[pos]));
         // check: mman
         TEST( sizeof(void*)+sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
         // check: mst
         TEST( 0 == invariant_multistate(&mst));
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
         reset_automatmman(mman);
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
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr));
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
      reset_automatmman(mman);
   }

   // TEST add_multistate: split leaf node (level 0) ==> build root (level 1) ==> 3 nodes total
   for (unsigned splitidx = 0; splitidx <= multistate_NROFSTATE; ++splitidx) {
      void * addr[2] = { 0 };
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      // prepare
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0, next = 0; i < multistate_NROFSTATE; ++i, ++next) {
         if (next == splitidx) ++next;
         TEST(0 == add_multistate(&mst, mman, &state[next]));
      }
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
      TEST( 0 == invariant_multistate(&mst));
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
      reset_automatmman(mman);
   }

   // TEST add_multistate: build root node (level 1) + many leafs (level 0) ascending/descending
   for (unsigned desc = 0; desc <= 1; ++desc) {
      void * child[multistate_NROFCHILD] = { 0 };
      void * addr[multistate_NROFCHILD] = { 0 };
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
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
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
               TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[nrchild]));
               *((void**)addr[nrchild]) = ENDMARKER;
               ++ nrchild;
            }
            // check: mman
            TEST( nrchild*sizeof(void*)+(nrchild+1)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
            // check: mst
            TEST( 0 == invariant_multistate(&mst));
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
      reset_automatmman(mman);
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
         TEST( 0 == invariant_multistate(&mst));
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
         malloc_automatmman(mman, 0, &addr[1]);
         memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
         reset_automatmman(mman);
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
      TEST( 0 == invariant_multistate(&mst));
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
      malloc_automatmman(mman, 0, &addr[1]);
      memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
      reset_automatmman(mman);
   }

   // TEST add_multistate: (level 2) split child(level 1) and add to root
   for (unsigned nrchild=2; nrchild < multistate_NROFCHILD; ++nrchild) {
      for (uintptr_t pos = 0; pos < nrchild; ++pos) {
         void * addr[2];
         void * child[multistate_NROFCHILD] = { 0 };
         size_t SIZE = nrchild * LEVEL1_NROFSTATE + 1;
         TEST(0 == malloc_automatmman(mman, 0, &addr[0]));
         TEST(0 == build2_multistate(&mst, mman, 2, nrchild, child));
         void * root = mst.root;
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
         void * splitchild = (uint8_t*)addr[1] + sizeof(void*) + sizeof(multistate_node_t)/*leaf*/;
         // test add single state ==> split check split
         TEST( 0 == add_multistate(&mst, mman, (void*)(1+pos*(2*LEVEL1_NROFSTATE))));
         // check: mman
         TEST( sizeof(void*)+(1+2+nrchild+nrchild*multistate_NROFCHILD)*sizeof(multistate_node_t) == sizeallocated_automatmman(mman));
         // check: mst
         TEST( 0 == invariant_multistate(&mst));
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
         malloc_automatmman(mman, 0, &addr[1]);
         memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
         reset_automatmman(mman);
      }
   }

   // TEST init_multistateiter: multistate_INIT
   mst = (multistate_t) multistate_INIT;
   memset(&iter, 255, sizeof(iter));
   init_multistateiter(&iter, &mst);
   // check iter
   TEST(0 == iter.next_node);
   TEST(0 == iter.next_state);
   TEST(0 == iter.is_single);

   // TEST next_multistateiter: multistate_INIT
   state_t * next = 0;
   TEST(0 == next_multistateiter(&iter, &next));
   // check next (unchanged)
   TEST(0 == next);
   // check iter
   TEST(0 == iter.next_node);
   TEST(0 == iter.next_state);
   TEST(0 == iter.is_single);

   // TEST init_multistateiter: single entry
   TEST(0 == add_multistate(&mst, mman, (void*)5));
   memset(&iter, 255, sizeof(iter));
   init_multistateiter(&iter, &mst);
   // check iter
   TEST(5 == (uintptr_t)iter.next_node);
   TEST(0 == iter.next_state);
   TEST(1 == iter.is_single);

   // TEST next_multistateiter: single entry
   // test
   TEST(1 == next_multistateiter(&iter, &next));
   // check next
   TEST(5 == (uintptr_t)next);
   // check iter
   TEST(0 == iter.next_node);
   TEST(0 == iter.next_state);
   TEST(0 == iter.is_single);

   multistate_node_t * F = 0;
   for (uintptr_t i=6; i <= 2*LEVEL1_NROFSTATE; ++i) {
      // TEST init_multistateiter: one or more pages
      TEST(0 == add_multistate(&mst, mman, (void*)i));
      if (i == 6) F = mst.root;
      memset(&iter, 255, sizeof(iter));
      init_multistateiter(&iter, &mst);
      // check iter
      TEST(F == iter.next_node);
      TEST(0 == iter.next_state);
      TEST(0 == iter.is_single);

      // TEST next_multistateiter: one or more pages
      multistate_node_t * N = F;
      for (uintptr_t i2 = 5, O = 1; i2 <= i; ++i2, O++) {
         if (O > N->size) { O = 1; N = N->next; }
         TEST(1 == next_multistateiter(&iter, &next));
         // check next
         TEST(i2 == (uintptr_t)next);
         // check iter
         TEST(N == iter.next_node);
         TEST(O == iter.next_state);
         TEST(0 == iter.is_single);
      }
      // test end of chain
      TEST(0 == next_multistateiter(&iter, &next));
      // check iter: reached end
      TEST(0 == iter.next_node);
      TEST(0 == iter.next_state);
      TEST(0 == iter.is_single);
   }

   // free resources
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}

static int test_rangemap(void)
{
   rangemap_t      rmap = rangemap_INIT;
   automat_mman_t *mman = 0;
   rangemap_iter_t iter;
   void * const    ENDMARKER = (void*) (uintptr_t) 0x01234567;

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
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
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
         TEST( 0  == rmap.root->range[0].multistate.size);
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         reset_automatmman(mman);
      }
   }

   // TEST addrange_rangemap: insert non-overlapping ranges into single node
   for (unsigned S = 2; S <= rangemap_NROFRANGE; ++S) {
      for (unsigned pos = 0; pos < S; ++pos) {
         // prepare
         void * addr[2];
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < S; ++i) {
            if (i == pos) continue;
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)i, (char32_t)i));
         }
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
            TEST( 0 == rmap.root->range[i].multistate.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         reset_automatmman(mman);
      }
   }

   // TEST addrange_rangemap: insert range overlapping with ranges and holes
   for (unsigned S = 1; S <= rangemap_NROFRANGE/2-1; ++S) {
      for (unsigned from = 0; from <= 2*S; ++from) {
         for (unsigned to = from; to <= 2*S; ++to) {
            // prepare
            void * addr[2];
            TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
            *(void**)addr[0] = ENDMARKER;
            rmap = (rangemap_t) rangemap_INIT;
            for (unsigned i = 0; i < S; ++i) {
               TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(1+2*i), (char32_t)(1+2*i)));
            }
            TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
               TEST( 0 == rmap.root->range[i].multistate.size);
            }
            // no overflow to adjacent block
            for (unsigned i = 0; i < lengthof(addr); ++i) {
               TEST( ENDMARKER == *(void**)addr[i]);
            }
            // reset
            reset_automatmman(mman);
         }
      }
   }

   // TEST addrange_rangemap: insert range fully overlapping with one or more ranges
   for (unsigned from = 0; from < rangemap_NROFRANGE; ++from) {
      for (unsigned to = from; to < rangemap_NROFRANGE; ++to) {
         const unsigned S = rangemap_NROFRANGE;
         // prepare
         void * addr[2];
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < rangemap_NROFRANGE; ++i) {
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(4*i), (char32_t)(3+4*i)));
         }
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
            TEST( 0 == rmap.root->range[i].multistate.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         reset_automatmman(mman);
      }
   }

   // TEST addrange_rangemap: insert range partially overlapping with one or more ranges
   for (unsigned from = 0; from < 3*4; ++from) {
      for (unsigned to = from; to < 3*4; ++to) {
         const unsigned S = 3u + (0 != from % 4) + (3 != to % 4);
         // prepare
         void * addr[2];
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
         *(void**)addr[0] = ENDMARKER;
         rmap = (rangemap_t) rangemap_INIT;
         for (unsigned i = 0; i < 3; ++i) {
            TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t)(4*i), (char32_t)(3+4*i)));
         }
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
            TEST( 0 == rmap.root->range[i].multistate.size);
         }
         // no overflow to adjacent block
         for (unsigned i = 0; i < lengthof(addr); ++i) {
            TEST( ENDMARKER == *(void**)addr[i]);
         }
         // reset
         reset_automatmman(mman);
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
            TEST( 0   == child->range[i].multistate.size);
         }
      }
      TEST( 0 == child);   // value from child->next
      // reset
      reset_automatmman(mman);
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
               TEST( 0   == child->range[i].multistate.size);
            }
         }
         TEST( 0 == child);   // value from child->next
         // reset
         reset_automatmman(mman);
      }
   }

   // TEST addrange_rangemap: split leaf node (level 0) ==> build root (level 1) ==> 3 nodes total
   for (unsigned pos = 0; pos <= rangemap_NROFRANGE; ++pos) {
      void * addr[2] = { 0 };
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[0]));
      *((void**)addr[0]) = ENDMARKER;
      // prepare
      rmap = (rangemap_t) rangemap_INIT;
      for (unsigned i = 0; i <= rangemap_NROFRANGE; ++i) {
         if (i == pos) continue;
         TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)i, (char32_t)i));
      }
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
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
         TEST( 0 == leaf1->range[i].multistate.size);
      }
      // check: leaf2 content
      TEST( leaf2->level == 0);
      TEST( leaf2->size  == rangemap_NROFRANGE/2);
      TEST( leaf2->next  == 0);
      for (unsigned i = 0, f = leaf1->size; i < leaf2->size; ++i, ++f) {
         TEST( f == leaf2->range[i].from);
         TEST( f == leaf2->range[i].to);
         TEST( 0 == leaf2->range[i].multistate.size);
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < lengthof(addr); ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      reset_automatmman(mman);
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
         TEST( 0 == invariant_rangemap(&rmap));
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
               TEST( child[i]->range[s].from == f);
               TEST( child[i]->range[s].to   == t);
               TEST( child[i]->range[s].multistate.size == 0);
            }
         }
         // check: no overflow into surrounding memory
         for (unsigned i = 0; i < nrchild+2; ++i) {
            TEST( ENDMARKER == *((void**)addr[i]));
         }
         // reset
         malloc_automatmman(mman, 0, &addr[1]);
         memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
         reset_automatmman(mman);
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
      TEST( 0 == invariant_rangemap(&rmap));
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
            TEST( child[i]->range[s].from == f);
            TEST( child[i]->range[s].to   == t);
            TEST( child[i]->range[s].multistate.size == 0);
         }
      }
      // check: no overflow into surrounding memory
      for (unsigned i = 0; i < rangemap_NROFCHILD+2; ++i) {
         TEST( ENDMARKER == *((void**)addr[i]));
      }
      // reset
      malloc_automatmman(mman, 0, &addr[1]);
      memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
      reset_automatmman(mman);
   }

   // TEST addrange_rangemap: (level 2) split child(level 1) and add to root
   for (unsigned nrchild=2; nrchild < rangemap_NROFCHILD; ++nrchild) {
      for (uintptr_t pos = 0; pos < nrchild; ++pos) {
         void * addr[2];
         rangemap_node_t * child[rangemap_NROFCHILD] = { 0 };
         const unsigned LEVEL1_NROFSTATE = rangemap_NROFRANGE * rangemap_NROFCHILD;
         size_t SIZE = nrchild * LEVEL1_NROFSTATE + 1;
         TEST(0 == malloc_automatmman(mman, 0, &addr[0]));
         TEST(0 == build2_rangemap(&rmap, mman, 2, nrchild, child));
         void * root = rmap.root;
         TEST(0 == malloc_automatmman(mman, sizeof(void*), &addr[1]));
         void * splitchild = (uint8_t*)addr[1] + sizeof(void*) + sizeof(rangemap_node_t)/*leaf*/;
         // test add single state ==> split
         char32_t r = (char32_t) (pos*(2*LEVEL1_NROFSTATE));
         TEST( 0 == addrange_rangemap(&rmap, mman, r, r));
         TEST( 0 == invariant_rangemap(&rmap));
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
         malloc_automatmman(mman, 0, &addr[1]);
         memset(addr[0], 0, (uintptr_t)addr[1] - (uintptr_t)addr[0]);
         reset_automatmman(mman);
      }
   }

   // TEST addstate_rangemap: add overlapping range from 0..MAX
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i <= 1000; ++i) {
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t) (3*i), (char32_t) (3*i+1)));
   }
   // test
   TEST( 0 == addrange_rangemap(&rmap, mman, 0, (char32_t)-1));
   TEST( 0 == invariant_rangemap(&rmap));
   // check rmap content
   {
      range_t* r;
      unsigned i = 0;
      bool isInBetween = false;
      init_rangemapiter(&iter, &rmap);
      while (next_rangemapiter(&iter, &r)) {
         if (isInBetween) {
            TEST(r->from == i);
            if (i == 3002) {
               TEST(r->to == (char32_t)-1);
               i = (char32_t)-1;
            } else {
               TEST(r->to == i);
               i += 1;
            }
         } else {
            TEST(r->from == i);
            TEST(r->to   == i+1);
            i += 2;
         }
         isInBetween = !isInBetween;
      }
      TEST(i == (char32_t)-1);
   }
   // reset
   reset_automatmman(mman);


   // TEST addstate_rangemap: add to single range
   // prepare
   unsigned const NRRANGE = 3 * rangemap_NROFCHILD * rangemap_NROFRANGE;
   rangemap_node_t *first = 0;
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i < NRRANGE; ++i) {
      TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t) (2*i), (char32_t) (2*i)));
      if (!i) first = rmap.root;
      TEST(first != 0);
      TEST(first->level == 0);
   }
   for (uintptr_t i = 0; i < NRRANGE; ++i) {
      rangemap_t old;
      // test
      memcpy(&old, &rmap, sizeof(old));
      TEST( 0 == addstate_rangemap(&rmap, mman, (char32_t) (2*i), (char32_t) (2*i), (state_t*)i));
      // check rmap not changed
      TEST( 0 == memcmp(&old, &rmap, sizeof(old)));
      // check rmap leaves content
      rangemap_node_t *next = first;
      for (uintptr_t i2 = 0, r = 0; i2 < NRRANGE; ++i2) {
         TEST(next != 0);
         TEST(next->size > r);
         TEST(next->range[r].from == (char32_t) (2*i2));
         TEST(next->range[r].to   == (char32_t) (2*i2));
         if (i2 <= i) {
            TEST(next->range[r].multistate.size == 1);
            TEST(next->range[r].multistate.root == (void*)i2);
         } else {
            TEST(next->range[r].multistate.size == 0);
            TEST(next->range[r].multistate.root == 0);
         }
         if ( (++r) >= next->size) {
            r = 0;
            next = next->next;
         }
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST addstate_rangemap: add to all ranges
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i < NRRANGE; ++i) {
      TEST( 0 == addrange_rangemap(&rmap, mman, (char32_t) (2*i), (char32_t) (2*i+1)));
      if (!i) first = rmap.root;
      TEST(first != 0);
      TEST(first->level == 0);
   }
   for (uintptr_t i = 1; i <= 3; ++i) {
      rangemap_t old;
      // test
      memcpy(&old, &rmap, sizeof(old));
      TEST( 0 == addstate_rangemap(&rmap, mman, 0, 2*NRRANGE-1, (state_t*)i));
      // check rmap not changed
      TEST( 0 == memcmp(&old, &rmap, sizeof(old)));
      // check rmap leaves content
      rangemap_node_t *next = first;
      for (uintptr_t i2 = 0, r = 0; i2 < NRRANGE; ++i2) {
         TEST(next != 0);
         TEST(next->size > r);
         TEST(next->range[r].from == (char32_t) (2*i2));
         TEST(next->range[r].to   == (char32_t) (2*i2+1));
         TEST(next->range[r].multistate.size == i);
         TEST(next->range[r].multistate.root != 0);
         if (i == 1) {
            TEST( (void*)i == next->range[r].multistate.root);
         } else {
            for (uintptr_t s = 1; s <= i; ++s) {
               TEST( (state_t*)s == ((multistate_node_t*)next->range[r].multistate.root)->state[s-1]);
            }
         }
         if ( (++r) >= next->size) {
            r = 0;
            next = next->next;
         }
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST addstate_rangemap: EINVAL (range[x].from < from && from < range[x].to)
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   TEST(0 == addrange_rangemap(&rmap, mman, 5, 9));
   // test
   for (unsigned from = 6; from <= 9; ++from) {
      TEST( EINVAL == addstate_rangemap(&rmap, mman, from, 9, 0));
      TEST( 1 == rmap.root->size);
      TEST( 0 == rmap.root->range[0].multistate.size);
   }

   // TEST addstate_rangemap: EINVAL (range[x].to < from && from < range[x+1].from)
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i <= 1; ++i) {
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)(3*i), (char32_t)(3*i+1)));
   }
   // single node
   TEST( EINVAL == addstate_rangemap(&rmap, mman, 2, 4, 0));
   TEST( 0 == rmap.root->range[0].multistate.size);
   TEST( 0 == rmap.root->range[1].multistate.size);
   TEST( EINVAL == addstate_rangemap(&rmap, mman, 0, 4, 0));
   TEST( 1 == rmap.root->range[0].multistate.size);
   TEST( 0 == rmap.root->range[1].multistate.size);
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i <= rangemap_NROFRANGE; ++i) {
      unsigned const off = (i == rangemap_NROFRANGE/2+1);
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)(2*i+off), (char32_t)(2*i+1)));
   }
   // split over two nodes
   TEST( EINVAL == addstate_rangemap(&rmap, mman, rangemap_NROFRANGE+2, rangemap_NROFRANGE+2, 0));
   for (unsigned c = 0; c <= 1; ++c) {
      for (unsigned i = 0; i < rmap.root->child[c]->size; ++i) {
         TEST( 0 == rmap.root->child[c]->range[i].multistate.size);
      }
   }
   TEST( EINVAL == addstate_rangemap(&rmap, mman, 0, 2*rangemap_NROFRANGE+1, 0));
   for (unsigned c = 0; c <= 1; ++c) {
      unsigned const S = (c == 0);
      for (unsigned i = 0; i < rmap.root->child[c]->size; ++i) {
         TEST( S == rmap.root->child[c]->range[i].multistate.size);
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST addstate_rangemap: EINVAL (range[x].from < to && to < range[x].to)
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   TEST(0 == addrange_rangemap(&rmap, mman, 5, 9));
   // test (single node)
   for (unsigned to = 5; to < 9; ++to) {
      TEST( EINVAL == addstate_rangemap(&rmap, mman, 5, to, 0));
      TEST( 1 == rmap.root->size);
      TEST( 0 == rmap.root->range[0].multistate.size);
   }
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i <= rangemap_NROFRANGE; ++i) {
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)(2*i), (char32_t)(2*i+1)));
   }
   // split over two nodes
   TEST( EINVAL == addstate_rangemap(&rmap, mman, (char32_t)0, (char32_t)(rangemap_NROFRANGE+2), 0));
   // check rmap
   for (unsigned c = 0; c <= 1; ++c) {
      unsigned const S = (c == 0);
      for (unsigned i = 0; i < rmap.root->child[c]->size; ++i) {
         TEST( S == rmap.root->child[c]->range[i].multistate.size);
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST addstate_rangemap: EINVAL (to > max(range[0..*].to))
   // prepare
   rmap = (rangemap_t) rangemap_INIT;
   for (unsigned i = 0; i <= rangemap_NROFRANGE; ++i) {
      TEST(0 == addrange_rangemap(&rmap, mman, (char32_t)(5*i), (char32_t)(5*i+4)));
   }
   // test
   TEST( EINVAL == addstate_rangemap(&rmap, mman, 0, 5*rangemap_NROFRANGE+4+1, 0));
   // check rmap
   for (unsigned c = 0; c <= 1; ++c) {
      for (unsigned i = 0; i < rmap.root->child[c]->size; ++i) {
         TEST( 1 == rmap.root->child[c]->range[i].multistate.size);
      }
   }
   // reset
   reset_automatmman(mman);

   // TEST init_rangemapiter: rangemap_INIT
   rmap = (rangemap_t) rangemap_INIT;
   memset(&iter, 255, sizeof(iter));
   init_rangemapiter(&iter, &rmap);
   // check iter
   TEST(0 == iter.next_node);
   TEST(0 == iter.next_range);

   // TEST next_rangemapiter: rangemap_INIT
   range_t * next = 0;
   TEST(0 == next_rangemapiter(&iter, &next));
   // check next (unchanged)
   TEST(0 == next);
   // check iter
   TEST(0 == iter.next_node);
   TEST(0 == iter.next_range);

   // TEST init_rangemapiter: single entry
   TEST(0 == addrange_rangemap(&rmap, mman, 1, 1));
   memset(&iter, 255, sizeof(iter));
   init_rangemapiter(&iter, &rmap);
   // check iter
   TEST(iter.next_node  == rmap.root);
   TEST(iter.next_range == 0);

   // TEST next_rangemapiter: single entry
   // test
   TEST(1 == next_rangemapiter(&iter, &next));
   // check next
   TEST(next == &rmap.root->range[0]);
   // check iter
   TEST(iter.next_node  == rmap.root);
   TEST(iter.next_range == 1);

   rangemap_node_t * F = rmap.root;
   for (unsigned i=2; i <= NRRANGE; ++i) {
      // TEST init_multistateiter: one or more pages
      // prepare
      TEST(0 == addrange_rangemap(&rmap, mman, i, i));
      memset(&iter, 255, sizeof(iter));
      // test
      init_rangemapiter(&iter, &rmap);
      // check iter
      TEST(iter.next_node  == F);
      TEST(iter.next_range == 0);

      // TEST next_rangemapiter: one or more pages
      rangemap_node_t* N = F;
      for (uintptr_t i2 = 1, r = 1; i2 <= i; ++i2, r++) {
         if (r > N->size) { r = 1; N = N->next; }
         TEST(1 == next_rangemapiter(&iter, &next));
         // check next
         TEST(next == &N->range[r-1]);
         // check iter
         TEST(iter.next_node  == N);
         TEST(iter.next_range == r);
      }
      // test end of chain
      TEST(0 == next_rangemapiter(&iter, &next));
      // check iter: reached end
      TEST(iter.next_node  == 0);
      TEST(iter.next_range == 0);
   }

   // free resources
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   delete_automatmman(&mman);
   return EINVAL;
}

static int test_statevector(void)
{
   automat_mman_t* mman = 0;
   statevector_t*  svec = 0;
   void*    const  MARKER = (void*) (uintptr_t) 0x718293a4;

   // prepare
   TEST(0 == new_automatmman(&mman));

   // === group constants

   // TEST statevector_MAX_NRSTATE: ensures statevector_t fits in uint16_t
   TEST( UINT16_MAX > sizeof(statevector_t) + statevector_MAX_NRSTATE  * sizeof(svec->state[0]));
   TEST( UINT16_MAX < sizeof(statevector_t) + (statevector_MAX_NRSTATE+1) * sizeof(svec->state[0]));

   // === group types
   {
      void* buffer[sizeof(statevector_t) * 128/sizeof(void*)] = { 0 };
      svec = (statevector_t*) buffer;

      // TEST slist_IMPLEMENT: (_stateveclist, statevector_t, next)
      slist_t list = slist_INIT;
      for (unsigned i = 0; i < 128; ++i) {
         // test insertlast
         insertlast_stateveclist(&list, &svec[i]);
         // check svec
         TEST( 0 == svec[i].index.bit_offset);
         TEST( 0 == svec[i].index.left);
         TEST( 0 == svec[i].index.right);
         TEST( 0 != svec[i].next);
         TEST( 0 == svec[i].nrstate);
      }
      // test foreach
      unsigned i = 0;
      foreach (_stateveclist, sv, &list) {
         TEST(sv == &svec[i++]);
      }
      TEST(i == 128);
   }

   // === group query

   // TEST getkey_statevector
   {
      getkey_data_t key = { 0, 0 };
      void* buffer[sizeof(statevector_t)] = { 0 };
      svec = (statevector_t*) buffer;
      for (size_t i = 0; i < 16; ++i) {
         // test
         svec->nrstate = i;
         getkey_statevector(svec, &key);
         // check key
         TEST( key.addr == (void*) svec->state);
         TEST( key.size == i * sizeof(void*));
      }
   }

   // TEST keyadapter_statevector
   {
      getkey_adapter_t adapter = keyadapter_statevector();
      TEST( adapter.nodeoffset == offsetof(statevector_t, index));
      TEST( adapter.getkey     == &getkey_statevector);
   }

   {
      void* buffer[sizeof(statevector_t) + 43] = { 0 };
      svec = (statevector_t*) buffer;

      // TEST iscontained_statevector: nrstate == 0
      TEST( 0 == iscontained_statevector(svec, (state_t*)0));
      // check svec
      TEST( 0 == svec->nrstate);

      // prepare
      for (uintptr_t i = 1; i <= 43; ++i) {
         svec->state[i-1] = (state_t*)(2*i);
      }

      // TEST iscontained_statevector: nrstate != 0
      for (unsigned nrstate = 1; nrstate <= 42; ++nrstate) {
         svec->nrstate = nrstate;
         TEST( 0 == iscontained_statevector(svec, (state_t*)0));
         TEST( 0 == iscontained_statevector(svec, (state_t*)(uintptr_t)1));
         for (unsigned i = 1; i <= nrstate; ++i) {
            TEST( 1 == iscontained_statevector(svec, (state_t*)(2*i)));
            TEST( 0 == iscontained_statevector(svec, (state_t*)(2*i+1)));
         }
         TEST( 0 == iscontained_statevector(svec, (state_t*)(2*nrstate+2)));
      }

   }

   // === group lifetime

   // TEST init_statevector
   for (unsigned nrstate = 1; nrstate <= statevector_MAX_NRSTATE; ++nrstate) {
      if (nrstate == 16) nrstate = statevector_MAX_NRSTATE-3;
      // prepare
      multistate_t mstate = multistate_INIT;
      for (unsigned s = 0; s <= 1; ++s) {
         for (uintptr_t i = s; i < nrstate; i += 2) {
            TEST(0 == add_multistate(&mstate, mman, (state_t*)i));
         }
      }
      void* marker[2] = { 0 };
      void* start_addr;
      size_t const S = sizeof(statevector_t) + nrstate * sizeof(svec->state[0]);
      TEST(0 == malloc_automatmman(mman, sizeof(void*), &marker[0]));
      *(void**)marker[0] = MARKER;
      TEST(0 == malloc_automatmman(mman, (uint16_t)(S + sizeof(void*)), &start_addr));
      marker[1] = S + (uint8_t*)start_addr;
      *(void**)marker[1] = MARKER;
      TEST(0 == mfreelast_automatmman(mman, start_addr));
      TEST((uint8_t*)marker[0] + sizeof(void*) + S == marker[1]);
      // test
      memset(start_addr, 255, S);
      svec = 0;
      TEST( 0 == init_statevector(&svec, mman, &mstate));
      // check svec
      TEST( svec == start_addr);
      // check svec content
      TEST( svec->index.bit_offset == 0);
      TEST( svec->index.left       == 0);
      TEST( svec->index.right      == 0);
      TEST( svec->next             == 0);
      TEST( svec->dfa              == 0);
      TEST( svec->nrstate          == nrstate);
      for (uintptr_t i = 0; i < nrstate; ++i) {
         TEST( svec->state[i] == (state_t*)i);
      }
      // check no overwrite into surrounding memory
      TEST(MARKER == *(void**)marker[0]);
      TEST(MARKER == *(void**)marker[1]);
      // reset
      reset_automatmman(mman);
   }

   // free resources
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
      if (i >= maxsize) return ENOMEM;
      states[i++] = s;
   }

   return (i == ndfa->nrstate) ? 0 : EINVAL;
}

typedef enum {
   state_EMPTY,
   state_RANGE,
   state_RANGE_ENDSTATE,
} state_type_e;

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
      if (helperstate[i].type == state_EMPTY) {
         TEST(helperstate[i].nrtrans == ndfa_state[i]->nremptytrans);
         TEST(0 == ndfa_state[i]->nrrangetrans);
      } else if (helperstate[i].type == state_RANGE) {
         TEST(0 == ndfa_state[i]->nremptytrans);
         TEST(helperstate[i].nrtrans == ndfa_state[i]->nrrangetrans);
      } else { // type == state_RANGE_ENDSTATE
         TEST(1 == ndfa_state[i]->nremptytrans);
         TEST(helperstate[i].nrtrans == ndfa_state[i]->nrrangetrans);
      }
      // check transitions
      empty_transition_t* empty_trans = first_emptylist(&ndfa_state[i]->emptylist);
      range_transition_t* range_trans = first_rangelist(&ndfa_state[i]->rangelist);
      for (unsigned t = 0; t < helperstate[i].nrtrans; ++t) {
         const size_t state_idx = helperstate[i].target_state[t];
         TEST(state_idx < nrstate);
         if (helperstate[i].type == state_EMPTY) {
            TESTP(ndfa_state[state_idx] == empty_trans->state, "i:%d", i);
            empty_trans = next_emptylist(empty_trans);
         } else {
            TEST(ndfa_state[state_idx]  == range_trans->state);
            TEST(helperstate[i].from[t] == range_trans->from);
            TEST(helperstate[i].to[t]   == range_trans->to);
            range_trans = next_rangelist(range_trans);
            if (helperstate[i].type == state_RANGE_ENDSTATE) {
               TESTP(ndfa_state[nrstate-1] == empty_trans->state, "expected endstate i:%d", i);
               TEST(empty_trans == next_emptylist(empty_trans));
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int helper_compare_copy(automat_t * dest_ndfa, automat_t * src_ndfa)
{
   void*     end_addr;
   size_t    nrstates = 0;
   size_t    allocated = 0;
   state_t * d = first_statelist(&dest_ndfa->states);

   TEST(0 == malloc_automatmman(dest_ndfa->mman, 0, &end_addr));

   foreach (_statelist, s, &src_ndfa->states) {
      TEST(d == (void*) ((uintptr_t)end_addr + allocated - dest_ndfa->allocated));
      ++ nrstates;
      TEST(d->nremptytrans == s->nremptytrans);
      TEST(d->nrrangetrans == s->nrrangetrans);
      TEST(isempty_slist(&d->emptylist) ==isempty_slist(&s->emptylist));
      TEST(isempty_slist(&d->rangelist) ==isempty_slist(&s->rangelist));
      TEST(d == s->dest);
      allocated += state_SIZE
                 + state_SIZE_EMPTYTRANS(d->nremptytrans)
                 + state_SIZE_RANGETRANS(d->nrrangetrans);
      empty_transition_t* d_empty_trans = last_emptylist(&d->emptylist);
      foreach (_emptylist, s_empty_trans, &s->emptylist) {
         d_empty_trans = next_emptylist(d_empty_trans);
         TEST(s_empty_trans->state->dest == d_empty_trans->state);
      }
      range_transition_t* d_range_trans = last_rangelist(&d->rangelist);
      foreach (_rangelist, s_range_trans, &s->rangelist) {
         d_range_trans = next_rangelist(d_range_trans);
         TEST(s_range_trans->state->dest == d_range_trans->state);
         TEST(s_range_trans->from == d_range_trans->from);
         TEST(s_range_trans->to   == d_range_trans->to);
      }
      d = next_statelist(d);
   }
   TEST(d == first_statelist(&dest_ndfa->states));
   TEST(dest_ndfa->mman != src_ndfa->mman);
   TEST(allocated == sizeallocated_automatmman(dest_ndfa->mman));
   TEST(nrstates == src_ndfa->nrstate);
   TEST(nrstates == dest_ndfa->nrstate);
   TEST(allocated == src_ndfa->allocated);
   TEST(allocated == dest_ndfa->allocated);

   return 0;
ONERR:
   return EINVAL;
}

static int helper_compare_reverse(automat_t* dest_ndfa, automat_t* src_ndfa, automat_t* use_mman)
{
   void*  end_addr;
   size_t nrstates = 0;
   size_t allocated = 0;

   TEST(0 == malloc_automatmman(dest_ndfa->mman, 0, &end_addr));

   // check mman
   if (use_mman) {
      TEST(dest_ndfa->mman == use_mman->mman);
      TEST(refcount_automatmman(dest_ndfa->mman) >= 2);
      TEST(dest_ndfa->allocated <= sizeallocated_automatmman(dest_ndfa->mman));
   } else {
      TEST(dest_ndfa->mman != src_ndfa->mman);
      TEST(refcount_automatmman(dest_ndfa->mman) == 1);
      TEST(dest_ndfa->allocated == sizeallocated_automatmman(dest_ndfa->mman));
   }
   // start and end state swapped
   state_t *dstart, *sstart, *dend, *send;
   startend_automat(dest_ndfa, &dstart, &dend);
   startend_automat(src_ndfa, &sstart, &send);
   TEST(dstart == send->dest);
   TEST(dend   == sstart->dest);
   // contains empty self trans as last entry
   TEST(dend->nremptytrans > 0);
   TEST(dend == last_emptylist(&dend->emptylist)->state);
   // check allocation order of states of dest_ndfa and calculate size of memory
   void* start_addr = (void*) ((uintptr_t)end_addr - dest_ndfa->allocated);
   void* trans_addr = (void*) ((uintptr_t)start_addr + dest_ndfa->nrstate * state_SIZE);
   foreach (_statelist, d, &dest_ndfa->states) {
      d->dest = 0; // set to defined value (unset in initreverse)
      nrstates  += 1;
      allocated += state_SIZE + state_SIZE_EMPTYTRANS(d->nremptytrans) + state_SIZE_RANGETRANS(d->nrrangetrans);
      TEST( d == (void*) ((uintptr_t)trans_addr - nrstates * state_SIZE));
      size_t trans_count = 0;
      foreach (_emptylist, trans, &d->emptylist) {
         ++trans_count;
         TEST(trans_addr <= (void*)trans && (void*)trans < end_addr);
         TEST(start_addr <= (void*)trans->state && (void*)trans->state < trans_addr);
         TEST(((uintptr_t)trans->state - (uintptr_t)start_addr) % state_SIZE == 0);
      }
      TEST(trans_count == d->nremptytrans);
      trans_count = 0;
      foreach (_rangelist, trans, &d->rangelist) {
         ++trans_count;
         TEST(trans_addr <= (void*)trans && (void*)trans < end_addr);
         TEST(start_addr <= (void*)trans->state && (void*)trans->state < trans_addr);
         TEST(((uintptr_t)trans->state - (uintptr_t)start_addr) % state_SIZE == 0);
      }
      TEST(trans_count == d->nrrangetrans);
   }
   // check size of memory
   TEST(nrstates == src_ndfa->nrstate);
   TEST(nrstates == dest_ndfa->nrstate);
   TEST(allocated == src_ndfa->allocated + state_SIZE_EMPTYTRANS(1));
   TEST(allocated == dest_ndfa->allocated);
   // check src_state->dest is ok
   size_t statenr = 0;
   foreach (_statelist, s, &src_ndfa->states) {
      TEST( s->dest == (void*) ((uintptr_t)start_addr + statenr * state_SIZE));
      statenr ++;
      s->dest->dest = s; // set back pointer
   }
   // check every transition in src is in dest
   foreach (_statelist, s, &src_ndfa->states) {
      foreach (_emptylist, trans, &s->emptylist) {
         state_t* d = trans->state->dest;
         size_t found = 0;
         foreach (_emptylist, dtrans, &d->emptylist) {
            found += (dtrans->state->dest == s);
         }
         TEST( 1 <= found);
      }
      foreach (_rangelist, trans, &s->rangelist) {
         state_t* d = trans->state->dest;
         size_t found = 0;
         foreach (_rangelist, dtrans, &d->rangelist) {
            found += (dtrans->state->dest == s && dtrans->from == trans->from && dtrans->to == trans->to);
         }
         TEST( 1 <= found);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   automat_t      ndfa  = automat_FREE;
   automat_t      ndfa1 = automat_FREE;
   automat_t      ndfa2 = automat_FREE;
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
   incruse_automatmman(mman);
   incruse_automatmman(mman2);
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
         TESTP( 0 == initmatch_automat(&ndfa, tc ? &use_mman : 0, (uint8_t) i, from, to), "i:%zd", i);
         automat_mman_t * mm = ndfa.mman;
         TEST( 0 != mm);
         // check mman
         TEST( (1+tc) == refcount_automatmman(mm));
         S = (unsigned) (2*sizeof(state_t))
           + (unsigned) (sizeof(empty_transition_t) + i*sizeof(range_transition_t));
         TEST( S == sizeallocated_automatmman(mm));
         // check ndfa
         TEST( ndfa.mman      == (tc ? mman : mm));
         TEST( ndfa.nrstate   == 2);
         TEST( ndfa.allocated == S);
         TEST( ! isempty_slist(&ndfa.states));
         // check ndfa.states
         helperstate[0] = (helper_state_t) { state_RANGE, (uint8_t) i, target_state, from, to };
         helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
         TEST( 0 == helper_compare_states(&ndfa, 2, helperstate))

         // TEST free_automat: free and double free
         // prepare
         if (mm != mman) incruse_automatmman(mm);
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
            TEST( ndfa.allocated == 0);
            TEST( isempty_slist(&ndfa.states));
         }

         if (mm != mman) {
            // TEST free_automat: free deletes memory manager if refcount == 0
            ndfa.mman = mm;
            // test
            TEST( 0 == free_automat(&ndfa));
            // check ndfa
            TEST( 0 == ndfa.mman);
            // check env (mm freed)
            TEST( SIZE_PAGE == SIZEALLOCATED_PAGECACHE());
         }
         // reset
         reset_automatmman(mman);
      }
   }

   // TEST free_automat: simulated ERROR
   decruse_automatmman(mman);
   for (unsigned tc = 0; tc <= 1; ++tc) {
      size_t const SIZE_PAGE = SIZEALLOCATED_PAGECACHE();
      TEST( 0 == initmatch_automat(&ndfa, tc ? &use_mman : 0, 3, from, to));
      automat_mman_t * mm = ndfa.mman;
      // test
      init_testerrortimer(&s_automat_errtimer, 1u, EINVAL);
      TEST( EINVAL == free_automat(&ndfa));
      // check env
      TEST( 0 == refcount_automatmman(mm));
      if (mm != mman) TEST(0 == delete_automatmman(&mm))/*simulated error prevented deletion*/;
      TEST( SIZE_PAGE == SIZEALLOCATED_PAGECACHE());
      // check ndfa
      TEST( ndfa.mman      == 0);
      TEST( ndfa.nrstate   == 0);
      TEST( ndfa.allocated == 0);
      TEST( isempty_slist(&ndfa.states));
   }
   // reset
   incruse_automatmman(mman);

   // TEST initempty_automat
   for (unsigned tc = 0; tc <= 1; ++tc) {
      // test
      TEST( 0 == initempty_automat(&ndfa, tc ? &use_mman : 0));
      automat_mman_t * mm = ndfa.mman;
      TEST( 0 != mm);
      // check mman
      TEST( (1+tc) == refcount_automatmman(mm));
      S = (unsigned) (2*sizeof(state_t))
        + (unsigned) (2*sizeof(empty_transition_t));
      TEST( S == sizeallocated_automatmman(mm));
      // check ndfa
      TEST( ndfa.mman      == (tc ? mman : mm));
      TEST( ndfa.nrstate   == 2);
      TEST( ndfa.allocated == S);
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      TEST( 0 == helper_compare_states(&ndfa, 2, helperstate))
      // reset
      TEST( 0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // === simulated ERROR

   TEST(0 == initempty_automat(&ndfa2, &use_mman2));
   for (unsigned tc = 0; tc <= 8; ++tc) {
      // prepare
      int err = (int) (tc+4);
      S = sizeallocated_automatmman(mman);
      init_testerrortimer(&s_automat_errtimer, 1, (int) err);
      switch (tc) {
      case 0:  // TEST initmatch_automat: simulated ERROR
               TEST( err == initempty_automat(&ndfa, &use_mman));
               break;
      case 1:  // TEST initmatch_automat: simulated ERROR
               TEST( err == initempty_automat(&ndfa, 0));
               break;
      case 2:  // TEST initmatch_automat: simulated ERROR
               TEST( err == initmatch_automat(&ndfa, &use_mman, 1, from, to));
               break;
      case 3:  // TEST initmatch_automat: simulated ERROR
               TEST( err == initmatch_automat(&ndfa, 0, 1, from, to));
               break;
      case 4:  // TEST initcopy_automat: simulated ERROR
               TEST( err == initcopy_automat(&ndfa, &ndfa2, &use_mman));
               break;
      case 5:  // TEST initcopy_automat: simulated ERROR
               TEST( err == initcopy_automat(&ndfa, &ndfa2, 0));
               break;
      case 6:  // TEST initreverse_automat: simulated ERROR
               TEST( err == initreverse_automat(&ndfa, &ndfa2, &use_mman));
               break;
      case 7:  // TEST initreverse_automat: simulated ERROR
               TEST( err == initreverse_automat(&ndfa, &ndfa2, 0));
               break;
      case 8:  // TEST initreverse_automat: EINVAL
               free_testerrortimer(&s_automat_errtimer);
               TEST( EINVAL == initreverse_automat(&ndfa, &ndfa, 0));
               break;
      }
      // check ndfa
      TEST( ndfa.mman    == 0);
      TEST( ndfa.nrstate == 0);
      TEST( ndfa.allocated == 0);
      TEST( isempty_slist(&ndfa.states));
      // check mman
      TEST( 0 == wasted_automatmman(mman));
      TEST( S == sizeallocated_automatmman(mman));
      TEST( 1 == refcount_automatmman(mman));
      // reset
      reset_automatmman(mman);
   }
   TEST(0 == free_automat(&ndfa2));

   // TEST initmove_automat
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 3000 } ));
   // test
   S = sizeallocated_automatmman(mman);
   initmove_automat(&ndfa, &ndfa1);
   // check mman
   TEST( 2 == refcount_automatmman(mman));
   TEST( S == sizeallocated_automatmman(mman));
   // check ndfa1
   TEST( 0 == ndfa1.mman);
   TEST( 0 == ndfa1.nrstate);
   TEST( 0 == ndfa1.allocated);
   TEST( isempty_slist(&ndfa1.states));
   // check ndfa
   TEST( ndfa.mman      == mman);
   TEST( ndfa.nrstate   == 2);
   TEST( ndfa.allocated == S);
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { 1 }, (char32_t[]) { 3000 } };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 2, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);

   // TEST initcopy_automat
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
   TEST(0 == extendmatch_automat(&ndfa2, 1, (char32_t[]) { 3 }, (char32_t[]) { 3 } ));
   TEST(0 == opor_automat(&ndfa, &ndfa2));
   S = sizeallocated_automatmman(ndfa.mman);
   for (unsigned tc = 0; tc <= 1; ++tc) {
      const size_t SIZE_PAGE = SIZEALLOCATED_PAGECACHE();
      // test
      TEST(0 == initcopy_automat(&ndfa2, &ndfa, tc ? &use_mman2 : 0));
      // check env
      if (!tc) {
         TEST( SIZE_PAGE < SIZEALLOCATED_PAGECACHE());
      }
      // check ndfa
      TEST( S == sizeallocated_automatmman(ndfa.mman));
      // check ndfa2
      TEST( ndfa2.mman == (tc ? mman2 : ndfa2.mman));
      TEST( 0 == helper_compare_copy(&ndfa2, &ndfa));
      TEST( (1+tc) == refcount_automatmman(ndfa2.mman));
      helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 1, 3 }, 0, 0 }; // ndfa
      // ndfa1
      helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
      helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      // ndfa2
      helperstate[3] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 4, 4 }, (char32_t[]) { 2, 3 }, (char32_t[]) { 2, 3 } };
      helperstate[4] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 }; // ndfa
      TEST( 0 == helper_compare_states(&ndfa2, 6, helperstate))
      // reset
      TEST(0 == free_automat(&ndfa2));
      reset_automatmman(mman2);
      TEST(SIZE_PAGE == SIZEALLOCATED_PAGECACHE());
   }
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);

   // TEST initreverse_automat: empty automat
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST(0 == initempty_automat(&ndfa, &use_mman));
      // test
      TEST( 0 == initreverse_automat(&ndfa2, &ndfa, tc ? &use_mman : 0));
      // check ndfa2
      TEST( 0 == helper_compare_reverse(&ndfa2, &ndfa, tc ? &use_mman : 0));
      helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 1, 0 }, 0, 0 }; // start state contains empty self trans of former end state
      helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };    // added empty self trans to end state
      TEST( 0 == helper_compare_states(&ndfa2, 2, helperstate))
      // reset
      TEST(0 == free_automat(&ndfa2));
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // TEST initreverse_automat: more complex automat
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
      for (unsigned i = 1; i < 10; ++i) {
         TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]) { 3*i }, (char32_t[]) { 4*i } ));
         TEST(0 == opor_automat(&ndfa, &ndfa2));
         TEST(0 == oprepeat_automat(&ndfa));
      }
      // test
      TEST( 0 == initreverse_automat(&ndfa2, &ndfa, tc ? &use_mman : 0));
      // check ndfa2
      TEST( 0 == helper_compare_reverse(&ndfa2, &ndfa, tc ? &use_mman : 0));
      // reset
      TEST(0 == free_automat(&ndfa2));
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // reset
   TEST(0 == delete_automatmman(&mman));
   TEST(0 == delete_automatmman(&mman2));

   return 0;
ONERR:
   free_automat(&ndfa);
   free_automat(&ndfa1);
   free_automat(&ndfa2);
   delete_automatmman(&mman);
   delete_automatmman(&mman2);
   return EINVAL;
}

static int check_dfa_endstate(automat_t* ndfa, /*out*/void** end_addr2)
{
   state_t* start_state;
   state_t* end_state;
   void*    end_addr;

   TEST( 1 == refcount_automatmman(ndfa->mman));
   TEST( 0 == malloc_automatmman(ndfa->mman, 0, &end_addr));
   startend_automat(ndfa, &start_state, &end_state);

   TEST( end_state   == (void*) ((uintptr_t)end_addr - ndfa->allocated));
   TEST( start_state == (void*) ((uintptr_t)end_state + (state_SIZE + state_SIZE_EMPTYTRANS(1))));
   TEST( end_state->nremptytrans == 1);
   TEST( end_state->nrrangetrans == 0);
   TEST( ! isempty_slist(&end_state->emptylist));
   TEST( isempty_slist(&end_state->rangelist));
   TEST( last_emptylist(&end_state->emptylist)->next == last_slist(&end_state->emptylist));
   TEST( last_emptylist(&end_state->emptylist)->state == end_state);

   if (end_addr2) *end_addr2 = end_addr;

   return 0;
ONERR:
   return EINVAL;
}

static int test_operations(void)
{
   automat_t      ndfa  = automat_FREE;
   automat_t      ndfa1 = automat_FREE;
   automat_t      ndfa2 = automat_FREE;
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
   incruse_automatmman(mman);
   incruse_automatmman(mman2);
   for (unsigned i = 0; i < lengthof(target_state); ++i) {
      target_state[i] = 0;
   }
   for (unsigned i = 0; i < lengthof(from); ++i) {
      from[i] = i;
      to[i]   = 3*i;
   }

   // TEST opsequence_automat
   // prepare
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
      TEST(0 == initmatch_automat(&ndfa2, tc ? &use_mman2 : &use_mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
      S = sizeallocated_automatmman(mman) + sizeallocated_automatmman(mman2);
      // test
      TEST( 0 == opsequence_automat(&ndfa, &ndfa2));
      // check mman
      TEST( 2 == refcount_automatmman(mman));
      TEST( 1 == refcount_automatmman(mman2)); // is copied or not used
      S += (unsigned) (2*sizeof(state_t))
         + (unsigned) (2*sizeof(empty_transition_t));
      TEST( S == sizeallocated_automatmman(mman));
      // check ndfa2
      TEST( 0 == ndfa2.mman);
      // check ndfa
      TEST( ndfa.mman      == mman);
      TEST( ndfa.nrstate   == 6);
      TEST( ndfa.allocated == S);
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      // ndfa1
      helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
      helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 3 }, 0, 0 };
      // ndfa2
      helperstate[3] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 4 }, (char32_t[]) { 2 }, (char32_t[]) { 2 } };
      helperstate[4] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };  // ndfa2
      helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      TEST(0 == helper_compare_states(&ndfa, 6, helperstate))
      // reset
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
      reset_automatmman(mman2);
   }

   // TEST oprepeat_automat
   // prepare
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
   TEST(2 == refcount_automatmman(mman));
   S = sizeallocated_automatmman(mman);
   // test
   TEST( 0 == oprepeat_automat(&ndfa));
   // check mman
   TEST( 2 == refcount_automatmman(mman));
   S += (unsigned) (2*sizeof(state_t))
      + (unsigned) (3*sizeof(empty_transition_t));
   TEST( S == sizeallocated_automatmman(mman));
   // check ndfa
   TEST( ndfa.mman      == mman);
   TEST( ndfa.nrstate   == 4);
   TEST( ndfa.allocated == S);
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 1, 3 }, 0, 0 };
   helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
   helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 0 }, 0, 0 };
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 3 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 4, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);

   // TEST opor_automat
   // prepare
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
      TEST(0 == initmatch_automat(&ndfa2, tc ? &use_mman2 : &use_mman, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
      S = sizeallocated_automatmman(mman) + sizeallocated_automatmman(mman2);
      // test
      TEST( 0 == opor_automat(&ndfa, &ndfa2));
      // check mman
      TEST( 2 == refcount_automatmman(mman));
      TEST( 1 == refcount_automatmman(mman2)); // is copied or not used
      S += (unsigned) (2*sizeof(state_t))
         + (unsigned) (3*sizeof(empty_transition_t));
      TEST( S == sizeallocated_automatmman(mman));
      // check ndfa1
      TEST( 0 == ndfa1.mman);
      // check ndfa2
      TEST( 0 == ndfa2.mman);
      // check ndfa
      TEST( ndfa.mman      == mman);
      TEST( ndfa.nrstate   == 6);
      TEST( ndfa.allocated == S);
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[0] = (helper_state_t) { state_EMPTY, 2, (size_t[]) { 1, 3 }, 0, 0 };
      // ndfa1
      helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
      helperstate[2] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      // ndfa2
      helperstate[3] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 4 }, (char32_t[]) { 2 }, (char32_t[]) { 2 } };
      helperstate[4] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
      TEST(0 == helper_compare_states(&ndfa, 6, helperstate))
      // reset
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
      reset_automatmman(mman2);
   }

   // TEST opand_automat: empty automaton
   TEST(0 == initempty_automat(&ndfa, &use_mman));
   TEST(0 == initempty_automat(&ndfa1, &use_mman));
   // test
   TEST( 0 == opand_automat(&ndfa, &ndfa1));
   // check ndfa1 not changed
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa1, 2, helperstate))
   // check mman released
   TEST( 2 == refcount_automatmman(mman));
   // check ndfa
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 2);
   TEST( ndfa.allocated == 2*state_SIZE + 2*state_SIZE_EMPTYTRANS(1));
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   TEST( 0 == check_dfa_endstate(&ndfa, 0));
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 2, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   reset_automatmman(mman);

   // TEST opand_automat: ab*c & abc
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]){ 'a' }, (char32_t[]){ 'a' }));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'b' }, (char32_t[]){ 'b' }));
   TEST(0 == oprepeat_automat(&ndfa2));
   TEST(0 == opsequence_automat(&ndfa, &ndfa2));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'c' }, (char32_t[]){ 'c' }));
   TEST(0 == opsequence_automat(&ndfa, &ndfa2));   // ab*c
   TEST(4 == matchchar32_automat(&ndfa, 4, U"abbc", false));
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]){ 'a' }, (char32_t[]){ 'a' }));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'b' }, (char32_t[]){ 'b' }));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'c' }, (char32_t[]){ 'c' }));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));   // abc
   // test
   TEST( 0 == opand_automat(&ndfa, &ndfa1));
   // check ndfa1 not changed
   TEST( 3 == matchchar32_automat(&ndfa1, 3, U"abc", false));
   // check mman released
   TEST( 2 == refcount_automatmman(mman));
   // check ndfa
   TEST( 0 == matchchar32_automat(&ndfa, 4, U"abbc", false));
   TEST( 3 == matchchar32_automat(&ndfa, 3, U"abc", false));
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 4);
   TEST( ndfa.allocated == 4*state_SIZE + state_SIZE_EMPTYTRANS(1) + 3*state_SIZE_RANGETRANS(1));
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   TEST( 0 == check_dfa_endstate(&ndfa, 0));
   helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { 'a' }, (char32_t[]) { 'a' } };
   helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 'b' }, (char32_t[]) { 'b' } };
   helperstate[2] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 'c' }, (char32_t[]) { 'c' } };
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 3 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 4, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   reset_automatmman(mman);

   // TEST opandnot_automat
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]){ 'a' }, (char32_t[]){ 'a' }));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'b' }, (char32_t[]){ 'b' }));
   TEST(0 == oprepeat_automat(&ndfa2));
   TEST(0 == opsequence_automat(&ndfa, &ndfa2));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'c' }, (char32_t[]){ 'c' }));
   TEST(0 == opsequence_automat(&ndfa, &ndfa2));   // ab*c
   TEST(4 == matchchar32_automat(&ndfa, 4, U"abbc", false));
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]){ 'a' }, (char32_t[]){ 'a' }));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'b' }, (char32_t[]){ 'b' }));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){ 'c' }, (char32_t[]){ 'c' }));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));   // abc
   // test
   TEST( 0 == opandnot_automat(&ndfa, &ndfa1));
   // check ndfa1 not changed
   TEST( 3 == matchchar32_automat(&ndfa1, 3, U"abc", false));
   // check mman released
   TEST( 2 == refcount_automatmman(mman));
   // check ndfa
   TEST( 4 == matchchar32_automat(&ndfa, 4, U"abbc", false));
   TEST( 2 == matchchar32_automat(&ndfa, 2, U"ac", false));
   TEST( 0 == matchchar32_automat(&ndfa, 3, U"abc", false));
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 6);
   TEST( ndfa.allocated == 6*state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(7));
   TEST( ! isempty_slist(&ndfa.states));
   // check ndfa.states
   TEST( 0 == check_dfa_endstate(&ndfa, 0));
   helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { 'a' }, (char32_t[]) { 'a' } };
   helperstate[1] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 2, 5 }, (char32_t[]) { 'b', 'c' }, (char32_t[]) { 'b', 'c' } };
   helperstate[2] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 3, 4 }, (char32_t[]) { 'b', 'c' }, (char32_t[]) { 'b', 'c' } };
   helperstate[3] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 3, 5 }, (char32_t[]) { 'b', 'c' }, (char32_t[]) { 'b', 'c' } };
   helperstate[4] = (helper_state_t) { state_RANGE, 0, 0, 0, 0 }; // error state
   helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 6, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   reset_automatmman(mman);

   // TODO: test initandnot_automat
   //       use extension made in initand_automat
   //       => extend second ndfa further with an additional error transition state
   //       => error state (dummy state) loops to itself with every input
   //       => check first end state to be true and second one to be false to become new endstate

   // TEST initnot_automat
   // TODO: test initnot_automat (same as initandnot_automat with ndfa1 = ".*")



   // === simulated ERROR in copy operation

   for (unsigned count = 1; count <= 3; ++ count) {
      for (unsigned tc = 0; tc <= 1; ++tc) {
         // prepare
         int err = (int) (3+count);
         TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 1 }, (char32_t[]) { 1 } ));
         TEST(0 == initmatch_automat(&ndfa2, &use_mman2, 1, (char32_t[]) { 2 }, (char32_t[]) { 2 } ));
         S = sizeallocated_automatmman(mman);
         init_testerrortimer(&s_automat_errtimer, count, err);
         switch (tc) {
         case 0:  // TEST opsequence_automat: ERROR in copy operation
                  TEST( err == opsequence_automat(&ndfa, &ndfa2));
                  break;
         case 1:  // TEST opor_automat: ERROR in copy operation
                  TEST( err == opor_automat(&ndfa, &ndfa2));
                  break;
         }
         // check ndfa: not changed
         helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { 1 }, (char32_t[]) { 1 } };
         helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
         TEST( 0 == helper_compare_states(&ndfa, 2, helperstate))
         // check ndfa2: not changed
         helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { 2 }, (char32_t[]) { 2 } };
         helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
         TEST( 0 == helper_compare_states(&ndfa2, 2, helperstate))
         // check mman, mman2
         TEST( refcount_automatmman(mman)      == (count <= 2 ? 2 : 3));
         TEST( sizeallocated_automatmman(mman) <= 2*S);
         TEST( wasted_automatmman(mman)        == 0);
         TEST( refcount_automatmman(mman2)     == (count <= 2 ? 2 : 1));
         TEST( sizeallocated_automatmman(mman2) == S);
         TEST( wasted_automatmman(mman2)       == (count <= 2 ? 0 : S));
         // reset
         TEST(0 == free_automat(&ndfa));
         TEST(0 == free_automat(&ndfa2));
         reset_automatmman(mman);
         reset_automatmman(mman2);
      }
   }

   // === simulated ERROR (no copy)

   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, from+1, to+1));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, from+2, to+2));
   S = sizeallocated_automatmman(mman);
   for (unsigned tc = 0; tc <= 4; ++tc) {
      // prepare
      int err = (int) (tc+4);
      init_testerrortimer(&s_automat_errtimer, 1, (int) err);
      switch (tc) {
      case 0:  // TEST opsequence_automat: simulated ERROR
               TEST( err == opsequence_automat(&ndfa, &ndfa2));
               break;
      case 1:  // TEST opor_automat: simulated ERROR
               TEST( err == opor_automat(&ndfa, &ndfa2));
               break;
      case 2:  // TEST opand_automat: simulated ERROR
               TEST( err == opand_automat(&ndfa, &ndfa2));
               break;
      case 3:  // TEST opandnot_automat: simulated ERROR
               TEST( err == opandnot_automat(&ndfa, &ndfa2));
               break;
      case 4:  // TEST opnot_automat: simulated ERROR
               TEST( err == opnot_automat(&ndfa));
               break;
      }
      // check ndfa: not changed
      helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { from[1] }, (char32_t[]) { to[1] } };
      helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      TEST( 0 == helper_compare_states(&ndfa, 2, helperstate))
      // check ndfa2: not changed
      helperstate[0] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 1 }, (char32_t[]) { from[2] }, (char32_t[]) { to[2] } };
      helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
      TEST( 0 == helper_compare_states(&ndfa2, 2, helperstate))
      // check mman
      TEST( 0 == wasted_automatmman(mman));
      TEST( S == sizeallocated_automatmman(mman));
      TEST( 3 == refcount_automatmman(mman));
   }
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa2));
   reset_automatmman(mman);

   // === EINVAL

   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, from+1, to+1));
   S = sizeallocated_automatmman(mman);
   for (unsigned tc = 0; tc <= 5; ++tc) {
      // prepare
      switch (tc) {
      case 0:  // TEST opsequence_automat: EINVAL empty ndfa
               TEST( EINVAL == opsequence_automat(&ndfa,  &ndfa2));
               TEST( EINVAL == opsequence_automat(&ndfa2, &ndfa));
               break;
      case 1:  // TEST oprepeat_automat: EINVAL empty ndfa
               TEST( EINVAL == oprepeat_automat(&ndfa));
               break;
      case 2:  // TEST opor_automat: EINVAL empty ndfa
               TEST( EINVAL == opor_automat(&ndfa,  &ndfa2));
               TEST( EINVAL == opor_automat(&ndfa2, &ndfa));
               break;
      case 3:  // TEST opand_automat: EINVAL empty ndfa
               TEST( EINVAL == opand_automat(&ndfa,  &ndfa2));
               TEST( EINVAL == opand_automat(&ndfa2, &ndfa));
               break;
      case 4:  // TEST opandnot_automat: EINVAL empty ndfa
               TEST( EINVAL == opandnot_automat(&ndfa,  &ndfa2));
               TEST( EINVAL == opandnot_automat(&ndfa2, &ndfa));
               break;
      case 5:  // TEST opnot_automat: EINVAL empty ndfa
               TEST( EINVAL == opnot_automat(&ndfa));
               break;
      }
      // check mman
      TEST( 0 == wasted_automatmman(mman));
      TEST( S == sizeallocated_automatmman(mman));
      TEST( 2 == refcount_automatmman(mman));
      // check ndfa
      TEST( ndfa.mman    == 0);
      TEST( ndfa.nrstate == 0);
      TEST( ndfa.allocated == 0);
      TEST( isempty_slist(&ndfa.states));
   }
   // reset
   TEST(0 == free_automat(&ndfa2));
   reset_automatmman(mman);

   // reset
   TEST(0 == delete_automatmman(&mman));
   TEST(0 == delete_automatmman(&mman2));

   return 0;
ONERR:
   free_automat(&ndfa);
   free_automat(&ndfa1);
   free_automat(&ndfa2);
   delete_automatmman(&mman);
   delete_automatmman(&mman2);
   return EINVAL;
}

static void set_isuse(automat_t * ndfa)
{
   foreach (_statelist, s, &ndfa->states) {
      s->isused = 1;
   }
}

static int check_isuse(automat_t * ndfa)
{
   foreach (_statelist, s, &ndfa->states) {
      TEST(0 == s->isused);
   }
   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   automat_t ndfa  = automat_FREE;
   automat_t ndfa2[5] = { automat_FREE } ;
   unsigned  minchainlen = sizeblock_statearray() / (unsigned) sizeof(state_t*);

   // TEST nrstate_automat: automat_FREE
   TEST(0 == nrstate_automat(&ndfa));

   // TEST nrstate_automat: returns value of nrstate
   for (size_t i = 1; i; i <<= 1) {
      ndfa.nrstate = i;
      TEST(i == nrstate_automat(&ndfa));
   }

   for (unsigned l = 1; l < 4; ++l) {
      // prepare
      state_t states[4];
      for (unsigned i = 0; i < l; ++i) {
         insertlast_statelist(&ndfa.states, &states[i]);
      }
      state_t * start;
      state_t * end;

      // TEST startend_automat: empty statelist
      startend_automat(&ndfa, &start, &end);
      // check start
      TEST(start == &states[0]);
      // check end
      TEST(end   == &states[l-1]);
   }

   // prepare ndfa with "( | [a-b][a-b]* )"
   {
      enum { EMPTY, MATCH1, MATCH2 };
      TEST(0 == initempty_automat(&ndfa2[EMPTY], 0));
      TEST(0 == initmatch_automat(&ndfa2[MATCH1], &ndfa2[EMPTY], 1, (char32_t[]){ 'a' }, (char32_t[]){ 'b' }));
      TEST(0 == initmatch_automat(&ndfa2[MATCH2], &ndfa2[EMPTY], 1, (char32_t[]){ 'a' }, (char32_t[]){ 'b' }));
      TEST(0 == oprepeat_automat(&ndfa2[MATCH2]));
      TEST(0 == opsequence_automat(&ndfa2[MATCH1], &ndfa2[MATCH2]));
      TEST(0 == opor_automat(&ndfa2[EMPTY], &ndfa2[MATCH1]));
      initmove_automat(&ndfa, &ndfa2[EMPTY]);
   }

   // TEST matchchar32_automat: match empty transition as shortest match
   set_isuse(&ndfa);
   TEST( 0 == matchchar32_automat(&ndfa, 1, U"a", false));
   // check ndfa isuse cleared
   TEST( 0 == check_isuse(&ndfa));

   // TEST matchchar32_automat: match longest string
   for (unsigned len = 0; len <= 10; ++len) {
      set_isuse(&ndfa);
      TEST( len == matchchar32_automat(&ndfa, len, U"ababababab", true));
      // check ndfa isuse cleared
      TEST( 0 == check_isuse(&ndfa));
   }

   // TEST matchchar32_automat: follow very long empty states
   // prepare
   TEST(0 == free_automat(&ndfa));
   TEST(0 == initmatch_automat(&ndfa, 0, 1, (char32_t[]){ 0 }, (char32_t[]){ 0 }));
   for (unsigned i = 1; i < 2*minchainlen; ++i) {
      TEST(0 == initmatch_automat(&ndfa2[0], &ndfa, 1, (char32_t[]){ i }, (char32_t[]){ i }));
      TEST(0 == opor_automat(&ndfa, &ndfa2[0]));
   }
   for (unsigned i = 0; i < 2*minchainlen; i += minchainlen/3) {
      // test
      set_isuse(&ndfa);
      char32_t c = (char32_t)i;
      TEST( 1 == matchchar32_automat(&ndfa, 1, &c, false));
      // check ndfa isuse cleared
      TEST( 0 == check_isuse(&ndfa));
   }
   for (unsigned i = 2*minchainlen; i <= 4*minchainlen; i += minchainlen) {
      // test (no match for other chars)
      set_isuse(&ndfa);
      char32_t c = (char32_t)i;
      TEST( 0 == matchchar32_automat(&ndfa, 1, &c, false));
      // check ndfa isuse cleared (if no error)
      TEST( 0 == check_isuse(&ndfa));
   }

   // TEST matchchar32_automat: state_RANGE_CONTINUE
   TEST(0 == free_automat(&ndfa));
   TEST(0 == initmatch_automat(&ndfa, 0, 2, (char32_t[]){ 0, 1 }, (char32_t[]){ 0, 1 }));
   for (unsigned i = 2; i < 2*minchainlen; i += 2) {
      TEST(0 == extendmatch_automat(&ndfa, 2, (char32_t[]){ i, i+1 }, (char32_t[]){ i, i+1 }));
   }
   for (unsigned i = 0; i < 2*minchainlen; i += minchainlen/4) {
      // test
      set_isuse(&ndfa);
      char32_t c = (char32_t)i;
      TEST( 1 == matchchar32_automat(&ndfa, 1, &c, false));
      // check ndfa isuse cleared
      TEST( 0 == check_isuse(&ndfa));
   }

   // reset
   TEST(0 == free_automat(&ndfa));
   for (unsigned i = 0; i < lengthof(ndfa2); ++i) {
      TEST(0 == free_automat(&ndfa2[i]));
   }

   return 0;
ONERR:
   free_automat(&ndfa);
   for (unsigned i = 0; i < lengthof(ndfa2); ++i) {
      free_automat(&ndfa2[i]);
   }
   return EINVAL;
}

static int test_extend(void)
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
   helperstate[0] = (helper_state_t) { state_RANGE, 15, target, from, to };
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0 };
   TEST(0 == helper_compare_states(&ndfa, 2, helperstate))
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
   for (unsigned i = 1, off=15; off+i < lengthof(from); off += i, ++i) {
      TEST( 0 == extendmatch_automat(&ndfa, (uint8_t)i, from+off, to+off))
      // check mman
      S += i * sizeof(range_transition_t);
      TEST( 1 == refcount_automatmman(mman));
      TEST( S == sizeallocated_automatmman(mman));
      // check ndfa
      TEST( mman == ndfa.mman);
      TEST( 2    == ndfa.nrstate);
      TEST( 0    == isempty_slist(&ndfa.states));
      // check ndfa.states
      helperstate[0] = (helper_state_t) { state_RANGE, (uint8_t)(off+i), target, from, to };
      TESTP( 0 == helper_compare_states(&ndfa, ndfa.nrstate, helperstate), "i:%d", i);
   }

   // reset
   TEST(0 == free_automat(&ndfa));

   return 0;
ONERR:
   free_automat(&ndfa);
   return EINVAL;
}

static int test_optimize(void)
{
   automat_t      ndfa  = automat_FREE;
   automat_t      ndfa1 = automat_FREE;
   automat_t      ndfa2 = automat_FREE;
   automat_mman_t*mman = 0;
   automat_t      use_mman;
   void *         end_addr;
   void *         next_addr;
   state_t *      start_state;
   state_t *      end_state;
   helper_state_t helperstate[10];

   // prepare
   TEST(0 == initempty_automat(&use_mman, 0));
   mman = use_mman.mman;

   // === convert to deterministic automaton (dfa)

   // prepare
   TEST(0 == initmatch_automat(&ndfa1, 0, 1, (char32_t[]){ 0 }, (char32_t[]){ (char32_t)-1 }));
   TEST(0 == oprepeat_automat(&ndfa1));  // ndfa1 matches ".*"

   // TEST makedfa_automat: 1 empty transition
   for (unsigned tc = 0; tc <= 1; ++tc) {
      TEST(0 == initempty_automat(&ndfa, &use_mman));
      TEST(2 == refcount_automatmman(mman));
      // test
      if (tc == 0) {
         TEST( 0 == makedfa_automat(&ndfa));
      } else {
         TEST( 0 == makedfa2_automat(&ndfa, OP_AND, &ndfa1));
      }
      // check mman released
      TEST( 1 == refcount_automatmman(mman));
      // check ndfa
      TEST( ndfa.mman      != mman);
      TEST( ndfa.nrstate   == 2);
      TEST( ndfa.allocated == 2*(state_SIZE + state_SIZE_EMPTYTRANS(1)));
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      startend_automat(&ndfa, &start_state, &end_state);
      TEST( end_state == next_statelist(start_state)); // only 2 in list (end == start->next)
      TEST( 0 == check_dfa_endstate(&ndfa, &end_addr));
      TEST( start_state->nremptytrans == 1);
      TEST( start_state->nrrangetrans == 0);
      TEST( ! isempty_slist(&start_state->emptylist));
      TEST( isempty_slist(&start_state->rangelist));
      TEST( last_emptylist(&start_state->emptylist)->next == last_slist(&start_state->emptylist));
      TEST( last_emptylist(&start_state->emptylist)->state == end_state);
      // reset
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // TEST makedfa_automat: 1 range && optimize continuous ranges into single transition
   for (unsigned tc = 0; tc <= 1; ++tc) {
      // prepare
      TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (uint32_t[]) { 0 }, (uint32_t[]) { 4 }));
      for (unsigned i = 1; i < 200; ++i) {
         TEST(0 == extendmatch_automat(&ndfa, 1, (uint32_t[]) { 5*i }, (uint32_t[]) { 5*i+4 }));
      }
      // test
      if (tc == 0) {
         TEST( 0 == makedfa_automat(&ndfa));
      } else {
         TEST( 0 == makedfa2_automat(&ndfa, OP_AND, &ndfa1));
      }
      // check mman released
      TEST( 1 == refcount_automatmman(mman));
      // check ndfa
      TEST( ndfa.mman      != mman);
      TEST( ndfa.nrstate   == 2);
      TEST( ndfa.allocated == 2 * state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(1));
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      startend_automat(&ndfa, &start_state, &end_state);
      TEST( 0 == check_dfa_endstate(&ndfa, &end_addr));
      TEST( start_state->nremptytrans == 0);
      TEST( start_state->nrrangetrans == 1);
      TEST( isempty_slist(&start_state->emptylist));
      TEST( ! isempty_slist(&start_state->rangelist));
      TEST( last_rangelist(&start_state->rangelist)->next == last_slist(&start_state->rangelist));
      TEST( last_rangelist(&start_state->rangelist)->state == (tc ? next_statelist(start_state) : end_state));
      TEST( last_rangelist(&start_state->rangelist)->from  == 0);
      TEST( last_rangelist(&start_state->rangelist)->to    == 5*200-1);
      // reset
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // TEST makedfa_automat: or'ed states
   for (unsigned tc = 0; tc <= 1; ++tc) {
      // prepare
      TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]) { 0 }, (char32_t[]) { 1 }));
      for (unsigned i = 1; i < 4*255; ++i) {
         TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]) { 3*i }, (char32_t[]) { 3*i+1 }));
         TEST(0 == opor_automat(&ndfa, &ndfa2));
      }
      // test
      if (tc == 0) {
         TEST( 0 == makedfa_automat(&ndfa));
      } else {
         TEST( 0 == makedfa2_automat(&ndfa, OP_AND, &ndfa1));
      }
      // check mman released
      TEST( 1 == refcount_automatmman(mman));
      // check ndfa
      TEST( ndfa.mman      != mman);
      TEST( ndfa.nrstate   == 2);
      TEST( ndfa.allocated == 2 * state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(4*255));
      TEST( ! isempty_slist(&ndfa.states));
      // check ndfa.states
      startend_automat(&ndfa, &start_state, &end_state);
      TEST( end_state == next_statelist(start_state)); // only 2 in list (end == start->next)
      TEST( 0 == check_dfa_endstate(&ndfa, &end_addr));
      TEST( start_state->nremptytrans == 0);
      TEST( start_state->nrrangetrans == 4*255);
      TEST( isempty_slist(&start_state->emptylist));
      TEST( ! isempty_slist(&start_state->rangelist));
      range_transition_t* range_trans = first_rangelist(&start_state->rangelist);
      next_addr = (uint8_t*)start_state + state_SIZE;
      for (unsigned i = 0; i < 4*255; ++i) {
         TEST( range_trans == next_addr);
         TEST( range_trans <  (range_transition_t*) end_addr);
         TEST( range_trans->state == end_state);
         TEST( range_trans->from  == 3*i);
         TEST( range_trans->to    == 3*i+1);
         next_addr   = &range_trans[1];
         range_trans = next_rangelist(range_trans);
      }
      TEST( end_addr == next_addr);
      TEST( range_trans == first_rangelist(&start_state->rangelist));
      // reset
      TEST(0 == free_automat(&ndfa));
      reset_automatmman(mman);
   }

   // reset
   TEST(0 == free_automat(&ndfa1));

   // === minimize states of automaton (reverse + dfa + reverse + dfa)

   // TEST minimize_automat: empty automaton
   TEST(0 == initempty_automat(&ndfa, &use_mman));
   // test
   TEST( 0 == minimize_automat(&ndfa));
   // check ndfa
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 2);
   TEST( ndfa.allocated == 2 * state_SIZE + state_SIZE_EMPTYTRANS(2));
   TEST( ! isempty_slist(&ndfa.states));
   TEST( 1 == refcount_automatmman(ndfa.mman));
   helperstate[0] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0};
   helperstate[1] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 1 }, 0, 0};
   TEST(0 == helper_compare_states(&ndfa, 2, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);


   // TEST minimize_automat: dfa+reverse+dfa+reverse could generate an nfa
   //                        ==> minimize_automat must call makedfa as last op
   // prepare
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]){'x'}, (char32_t[]){'x'}));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'b'}, (char32_t[]){'b'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // xb
   TEST(0 == oprepeat_automat(&ndfa1));            // (xb)*
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'x'}, (char32_t[]){'x'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // (xb)*x
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'n'}, (char32_t[]){'n'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // (xb)*xn
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]){'b'}, (char32_t[]){'b'}));
   TEST(0 == opsequence_automat(&ndfa, &ndfa1));   // b(xb)*xn
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]){'x'}, (char32_t[]){'x'}));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'c'}, (char32_t[]){'c'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // xc
   TEST(0 == oprepeat_automat(&ndfa1));            // (xc)*
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'x'}, (char32_t[]){'x'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // (xc)*x
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'n'}, (char32_t[]){'n'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));  // (xc)*xn
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'c'}, (char32_t[]){'c'}));
   TEST(0 == opsequence_automat(&ndfa2, &ndfa1));  // c(xc)*xn
   TEST(0 == opor_automat(&ndfa, &ndfa2));         // b(xb)*xn|c(xc)*xn
   TEST(0 == initcopy_automat(&ndfa1, &ndfa, 0));  // b(xb)*xn|c(xc)*xn
   // test (simulate minimize_automat) without calling makedfa_automat as last operation
   TEST( 0 == makedfa_automat(&ndfa));
   TEST( 0 == initreverse_automat(&ndfa2, &ndfa, &use_mman));
   TEST( 0 == free_automat(&ndfa));
   TEST( 0 == makedfa_automat(&ndfa2));
   TEST( 0 == initreverse_automat(&ndfa, &ndfa2, &use_mman));
   TEST( 0 == free_automat(&ndfa2));
   // check ndfa is type nfa and not dfa
   next_addr = first_statelist(&ndfa.states);
   for (unsigned i = 0; i < 3; ++i) {
      next_addr = next_statelist(next_addr);
   }
   // check ndfa: 4th state has 2 transition with 'c' ==> no dfa but nfa
   TEST(2 == ((state_t*)next_addr)->nrrangetrans);
   TEST((char32_t)'c' == last_rangelist(&((state_t*)next_addr)->rangelist)->from);
   TEST((char32_t)'c' == first_rangelist(&((state_t*)next_addr)->rangelist)->from);
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);

   // TEST minimize_automat: b(xb)*xn|c(xc)*xn
   TEST(0 == initcopy_automat(&ndfa, &ndfa1, &use_mman));
   TEST(0 == free_automat(&ndfa1));
   // test
   TEST( 0 == minimize_automat(&ndfa));
   // check ndfa
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 6);
   TEST( ndfa.allocated == 6 * state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(8));
   TEST( ! isempty_slist(&ndfa.states));
   TEST( 1 == refcount_automatmman(ndfa.mman));
   helperstate[0] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 1, 2 }, (char32_t[]) { 'b', 'c' }, (char32_t[]) { 'b', 'c' }};
   helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { 'x' }, (char32_t[]) { 'x' }};
   helperstate[2] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 4 }, (char32_t[]) { 'x' }, (char32_t[]) { 'x' }};
   helperstate[3] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 1, 5 }, (char32_t[]) { 'b', 'n' }, (char32_t[]) { 'b', 'n' }};
   helperstate[4] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 2, 5 }, (char32_t[]) { 'c', 'n' }, (char32_t[]) { 'c', 'n' }};
   helperstate[5] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 5 }, 0, 0};
   TEST(0 == helper_compare_states(&ndfa, 6, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   reset_automatmman(mman);

   // TEST minimize_automat:  a(a12345|b12345)|c12345
   TEST(0 == initempty_automat(&ndfa1, &use_mman));
   for (unsigned i = '1'; i <= '5'; ++i) {
      TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){i}, (char32_t[]){i}));
      TEST(0 == opsequence_automat(&ndfa1, &ndfa2));
   }
   automat_t nabc[3];
   for (unsigned i = 'a'; i <= 'c'; ++i) {
      TEST(0 == initmatch_automat(&nabc[i-'a'], &use_mman, 1, (char32_t[]){i}, (char32_t[]){i}));
      TEST(0 == initcopy_automat(&ndfa2, &ndfa1, &use_mman));
      TEST(0 == opsequence_automat(&nabc[i-'a'], &ndfa2));
   }
   TEST(0 == opor_automat(&nabc[0], &nabc[1]));
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]){'a'}, (char32_t[]){'a'}));
   TEST(0 == opsequence_automat(&ndfa, &nabc[0]));
   TEST(0 == opor_automat(&ndfa, &nabc[2]));
   // test
   TEST( 0 == minimize_automat(&ndfa));
   // check ndfa
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 8);
   TEST( ndfa.allocated == 8 * state_SIZE + state_SIZE_EMPTYTRANS(1) + state_SIZE_RANGETRANS(8));
   TEST( ! isempty_slist(&ndfa.states));
   TEST( 1 == refcount_automatmman(ndfa.mman));
   helperstate[0] = (helper_state_t) { state_RANGE, 2, (size_t[]) { 1, 2 }, (char32_t[]) { 'a', 'c' }, (char32_t[]) { 'a', 'c' }};
   helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 'a' }, (char32_t[]) { 'b' }};
   helperstate[2] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 3 }, (char32_t[]) { '1' }, (char32_t[]) { '1' }};
   helperstate[3] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 4 }, (char32_t[]) { '2' }, (char32_t[]) { '2' }};
   helperstate[4] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 5 }, (char32_t[]) { '3' }, (char32_t[]) { '3' }};
   helperstate[5] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 6 }, (char32_t[]) { '4' }, (char32_t[]) { '4' }};
   helperstate[6] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 7 }, (char32_t[]) { '5' }, (char32_t[]) { '5' }};
   helperstate[7] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 7 }, 0, 0};
   TEST(0 == helper_compare_states(&ndfa, 8, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   reset_automatmman(mman);

   // TEST minimize_automat:  (xy(zy)*)*
   TEST(0 == initmatch_automat(&ndfa, &use_mman, 1, (char32_t[]){'x'}, (char32_t[]){'x'}));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'y'}, (char32_t[]){'y'}));
   TEST(0 == opsequence_automat(&ndfa, &ndfa2));
   TEST(0 == initmatch_automat(&ndfa1, &use_mman, 1, (char32_t[]){'z'}, (char32_t[]){'z'}));
   TEST(0 == initmatch_automat(&ndfa2, &use_mman, 1, (char32_t[]){'y'}, (char32_t[]){'y'}));
   TEST(0 == opsequence_automat(&ndfa1, &ndfa2));
   TEST(0 == oprepeat_automat(&ndfa1));
   TEST(0 == opsequence_automat(&ndfa, &ndfa1));
   TEST(0 == oprepeat_automat(&ndfa));
   // test
   TEST( 0 == minimize_automat(&ndfa));
   // check ndfa
   TEST( ndfa.mman      != mman);
   TEST( ndfa.nrstate   == 4);
   TEST( ndfa.allocated == 4 * state_SIZE + state_SIZE_EMPTYTRANS(3) + state_SIZE_RANGETRANS(4));
   TEST( ! isempty_slist(&ndfa.states));
   TEST( 1 == refcount_automatmman(ndfa.mman));
   helperstate[0] = (helper_state_t) { state_RANGE_ENDSTATE, 1, (size_t[]) { 1 }, (char32_t[]) { 'x' }, (char32_t[]) { 'x' }};
   helperstate[1] = (helper_state_t) { state_RANGE, 1, (size_t[]) { 2 }, (char32_t[]) { 'y' }, (char32_t[]) { 'y' }};
   helperstate[2] = (helper_state_t) { state_RANGE_ENDSTATE, 2, (size_t[]) { 1, 1 }, (char32_t[]) { 'x', 'z' }, (char32_t[]) { 'x', 'z' }};
   helperstate[3] = (helper_state_t) { state_EMPTY, 1, (size_t[]) { 3 }, 0, 0};
   TEST(0 == helper_compare_states(&ndfa, 4, helperstate))
   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   reset_automatmman(mman);

   // reset
   TEST(0 == free_automat(&ndfa));
   TEST(0 == free_automat(&ndfa1));
   TEST(0 == free_automat(&ndfa2));
   TEST(0 == delete_automatmman(&mman));

   return 0;
ONERR:
   free_automat(&ndfa);
   free_automat(&ndfa1);
   free_automat(&ndfa2);
   delete_automatmman(&mman);
   return EINVAL;
}

int unittest_proglang_automat()
{
   if (test_state())       goto ONERR;
   if (test_statearray())  goto ONERR;
   if (test_multistate())  goto ONERR;
   if (test_rangemap())    goto ONERR;
   if (test_statevector()) goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_operations())  goto ONERR;
   if (test_query())       goto ONERR;
   if (test_extend())      goto ONERR;
   if (test_optimize())    goto ONERR;

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
