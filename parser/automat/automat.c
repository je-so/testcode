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

// === implemented types
struct memory_page_t;
struct automat_mman_t;
struct range_transition_t;
struct state_t;
struct multistate_node_t;
struct multistate_t;
struct automat_t;

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

/* struct: state_t
 * Beschreibt einen Zustand des Automaten. */
typedef struct state_t {
   uint8_t  type;  // value from state_e
   uint8_t  nrtrans;
   slist_node_t* next;
   union {
      empty_transition_t empty_transition[2/*nrtrans*/]; // case type == state_EMPTY
      range_transition_t range_transition[1/*nrtrans*/]; // case type == state_RANGE
   };
} state_t;

// group: constants

#define state_SIZE                     (offsetof(struct state_t, empty_transition))
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
   state->empty_transition[0] = target;
}

/* function: initempty2_state
 * Initialisiert state mit zwei "leeren" Übergangem zu target und target2.
 * Ein leerer Übergang wird immer verwendet und liest bzw. verbraucht keinen Buchstaben. */
static void initempty2_state(/*out*/state_t * state, state_t * target, state_t * target2)
{
   state->type = state_EMPTY;
   state->nrtrans = 2;
   state->empty_transition[0] = target;
   state->empty_transition[1] = target2;
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
      state->range_transition[i].state = target;
      state->range_transition[i].from = match_from[i];
      state->range_transition[i].to = match_to[i];
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
      state->range_transition[i].state = target;
      state->range_transition[i].from = match_from[i];
      state->range_transition[i].to = match_to[i];
   }
}


/* struct: multistate_node_t
 * Knoten, der Teil eines B-Tree ist. */
typedef struct multistate_node_t {
   uint8_t level;
   uint8_t size;
   union {
      struct { // level > 0
         state_t * key[3];
         struct multistate_node_t * node[4];
      };
      struct { // level == 0
         struct multistate_node_t * next;
         state_t * state[6];
      };
   };
} multistate_node_t;


typedef struct multistate_stack_entry_t {
   multistate_node_t * node;
   unsigned idx_node;
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

static inline void push_multistatestack(multistate_stack_t * stack, multistate_node_t * node, unsigned idx_node)
{
   stack->entry[stack->depth] = (multistate_stack_entry_t) { node, idx_node };
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

// group: lifetime

#define multistate_INIT \
         { 0, 0 }

// group: query

// TODO:

// group: update

static int add_multistate(multistate_t * mst, /*in*/state_t * state, automat_mman_t * mman)
{
   int err;
   void * node;

   // 3 cases
   // 1: 2 <= mst->size ==> insert into leaf
   // 2: 0 == mst->size ==> store state into root pointer
   // 3: 1 == mst->size ==> allocate leaf and store root + state into leaf

   if (1 < mst->size) { // case 1
      // search from mst->root node to leaf and store search path in stack
      multistate_stack_t   stack;
      init_multistatestack(&stack);
      node = mst->root;
      if (  ((multistate_node_t*)node)->level >= (int) lengthof(stack.entry)
            || ((multistate_node_t*)node)->size < 2) {
         return EINVARIANT;
      }
      for (int level = ((multistate_node_t*)node)->level; (level--) > 0; ) {
         if (((multistate_node_t*)node)->size > lengthof(((multistate_node_t*)node)->node)) {
            return EINVARIANT;
         }
         unsigned high = (((multistate_node_t*)node)->size -1u); // size >= 2 ==> high >= 1
         unsigned low  = 0; // low < high cause high >= 1
         // ((multistate_node_t*)node)->node[high] is valid
         // ((multistate_node_t*)node)->key[high-1] is valid
         for (unsigned mid = high / 2u; /*low < high*/; mid = (high + low) / 2u) {
            // search state in node->key
            if (((multistate_node_t*)node)->key[mid] < state) {
               low = mid + 1; // low <= high
            } else {
               high = mid;
            }
            if (low == high) break;
         }
         push_multistatestack(&stack, node, low);
         node = ((multistate_node_t*)node)->node[low];
         if (  ((multistate_node_t*)node)->level != level
               || ((multistate_node_t*)node)->size < 2
               || ((multistate_node_t*)node)->size > lengthof(((multistate_node_t*)node)->node)) {
            return EINVARIANT;
         }
      }
      if (((multistate_node_t*)node)->size > lengthof(((multistate_node_t*)node)->state)) {
         return EINVARIANT;
      }
      // find state in leaf node. low is the index where new state has to be inserted !
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
      if (lengthof(((multistate_node_t*)node)->state) > ((multistate_node_t*)node)->size) {
         // insert into leaf
         memmove(&((multistate_node_t*)node)->state[low+1], &((multistate_node_t*)node)->state[low], sizeof(((multistate_node_t*)node)->state[0]) * (((multistate_node_t*)node)->size - low));
         ((multistate_node_t*)node)->state[low] = state;
         ++ ((multistate_node_t*)node)->size;
      } else {
         // split node
         // ENOMEM ==> splitting failed ==> possible half of the information is lost
         //        ==> structure corrupt afterwards ==> ignored cause user sees only ENOMEM and never content of this structure
         assert(0); // TODO: implement
      }

   } else if (! mst->size) {  // case 2
      mst->root = state;

   } else { // case 3
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


// section: automat_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_automat_errtimer
 * Simuliert Fehler in Funktionen für <automat_t> und <automat_mman_t>. */
static test_errortimer_t   s_automat_errtimer = test_errortimer_FREE;
#endif

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
   last1->empty_transition[0] = first2;

   state_t * last2 = next_statelist(first2);
   last2->empty_transition[0] = endstate;

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
   last1->empty_transition[0] = startstate;

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
   last1->empty_transition[0] = endstate;

   state_t * last2 = next_statelist(first2);
   last2->empty_transition[0] = endstate;

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

// group: query

const void* startstate_automat(const automat_t* ndfa)
{
   return first_statelist(&ndfa->states);
}

/* function: endstate_automat
 * Endzustand des Automaten. Im Falle eines optimierten Automaten,
 * der mehrere Endzustände hat, verweisen diese per zuätzlicher "empty transition"
 * auf den einzigen und nicht optimierten Endzustand. */
const void* endstate_automat(const automat_t* ndfa)
{
   state_t * start = first_statelist(&ndfa->states);
   return start ? next_statelist(start) : 0;
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
   for (int i = 10; i < 13; ++i) {
      memory_page_t * errpage = 0;
      init_testerrortimer(&s_automat_errtimer, 1, i);
      TEST(i == new_memorypage(&errpage));
      // check
      TEST(0 == errpage);
      TEST(SIZEALLOCATED_PAGECACHE() == oldsize);
   }

   // TEST delete_memorypage: simulated ERROR
   for (int i = 10; i < 13; ++i) {
      TEST(0 == new_memorypage(&page[0]));
      init_testerrortimer(&s_automat_errtimer, 1, i);
      TEST(i == delete_memorypage(page[0]));
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
   for (int i = 0; i < 3; ++i) {
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
   for (int i = 0; i < 3; ++i) {
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
   state_t state[256];

   // === constants

   // TEST state_SIZE
   static_assert( state_SIZE == sizeof(state_t) - sizeof(range_transition_t),
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
   for (unsigned i = 0; i < 256; ++i) {
      insertlast_statelist(&list, &state[i]);
      // check list
      TEST(&state[i] == last_statelist(&list));
   }
   // check state[]
   for (unsigned i = 0; i < 256; ++i) {
      TEST(&state[(i+1)%256] == next_statelist(&state[i]));
      TEST((slist_node_t*)&state[(i+1)%256].next == state[i].next);
   }

   // === lifetime

   // TEST initempty_state
   initempty_state(&state[0], &state[2]);
   TEST(state[0].type == state_EMPTY);
   TEST(state[0].nrtrans == 1);
   TEST(state[0].next == (void*) &state[1].next); // unchanged
   TEST(state[0].empty_transition[0] == &state[2]);
   TEST(state[0].range_transition[0].state == &state[2]);
   TEST(state[0].range_transition[0].from  == 0); // unchanged
   TEST(state[0].range_transition[0].to    == 0); // unchanged
   // reset
   state[0].empty_transition[0] = 0;

   // TEST initempty2_state
   initempty2_state(&state[0], &state[2], &state[5]);
   TEST(state[0].type == state_EMPTY);
   TEST(state[0].nrtrans == 2);
   TEST(state[0].next == (void*) &state[1].next);  // unchanged
   TEST(state[0].empty_transition[0] == &state[2]);
   TEST(state[0].empty_transition[1] == &state[5]);
   TEST(state[0].empty_transition[2] == 0);        // unchanged
   // reset
   state[0].empty_transition[0] = 0;
   state[0].empty_transition[1] = 0;


   // prepare
   char32_t from[256];
   char32_t to[256];
   for (unsigned r = 0; r < 256; ++r) {
      from[r] = r + 1;
      to[r] = r + 10;
   }

   // TEST initrange_state
   for (unsigned i = 0; i < 256; ++i) {
      initrange_state(&state[0], &state[3], (uint8_t)i, from, to);
      TEST(state[0].type == state_RANGE);
      TEST(state[0].nrtrans == i);
      TEST(state[0].next == (void*) &state[1].next); // unchanged
      for (unsigned r = 0; r < i; ++r) {
         TEST(state[0].range_transition[r].state == (r < i ? &state[3] : 0));
         TEST(state[0].range_transition[r].from  == (r < i ? r+1  : 0));
         TEST(state[0].range_transition[r].to    == (r < i ? r+10 : 0));
      }
      // reset
      memset(state, 0, sizeof(state));
      state[0].next = (void*) &state[1].next;
   }

   // TEST initcontinue_state
   for (unsigned i = 0; i < 256; ++i) {
      initcontinue_state(&state[0], &state[3], (uint8_t)i, from, to);
      TEST(state[0].type == state_RANGE_CONTINUE);
      TEST(state[0].nrtrans == i);
      TEST(state[0].next == (void*) &state[1].next); // unchanged
      for (unsigned r = 0; r < i; ++r) {
         TEST(state[0].range_transition[r].state == (r < i ? &state[3] : 0));
         TEST(state[0].range_transition[r].from  == (r < i ? r+1  : 0));
         TEST(state[0].range_transition[r].to    == (r < i ? r+10 : 0));
      }
      // reset
      memset(state, 0, sizeof(state));
      state[0].next = (void*) &state[1].next;
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_multistate(void)
{
   state_t        state[256];
   multistate_t   mst = multistate_INIT;
   automat_mman_t mman = automat_mman_INIT;

   // TEST multistate_INIT
   TEST(0 == mst.size);
   TEST(0 == mst.root);

   // TEST add_multistate: multistate_t.size == 0
   for (unsigned i = 0; i < lengthof(state); ++i) {
      mst = (multistate_t) multistate_INIT;
      TEST( 0 == add_multistate(&mst, &state[i], &mman));
      // check mman: nothing allocated
      TEST( 0 == sizeallocated_automatmman(&mman));
      // check mst
      TEST( 1 == mst.size);
      TEST( &state[i] == mst.root);
   }

   // TEST add_multistate: multistate_t.size == 1
   for (unsigned i = 0; i < lengthof(state)-1; ++i) {
      for (unsigned order = 0; order <= 1; ++order) {

         mst = (multistate_t) multistate_INIT;
         TEST(0 == add_multistate(&mst, &state[i + !order], &mman));
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

   void * const ENDMARKER = (void*) (uintptr_t) 0x01234567;

   // TEST add_multistate: single node && add states ascending/descending
   for (int asc = 1; asc >= 0; --asc) {
      const unsigned Lstate = lengthof(((multistate_node_t*)0)->state);
      void * addr = 0;
      mst = (multistate_t) multistate_INIT;
      for (unsigned i = 0; i < Lstate; ++i) {
         // test
         TEST( 0 == add_multistate(&mst, &state[asc ? i : Lstate-1-i], &mman));
         if (1 == i) {  // set end marker at end of node
            allocmem_automatmman(&mman, sizeof(void*), &addr);
            *((void**)addr) = ENDMARKER;
            // check that end marker is effective !!
            TEST(addr == &((multistate_node_t*)mst.root)->state[Lstate]);
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
               TEST( &state[asc ? s : Lstate-1-i+s] == ((multistate_node_t*)mst.root)->state[s]);
            }
            TEST(ENDMARKER == *((void**)addr)); // no overflow into following memory block
         }
      }
      // reset
      TEST(0 == free_automatmman(&mman));
   }

   // TEST add_multistate: single node && add states unordered
   // TODO:


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
            TESTP(ndfa_state[state_idx] == ndfa_state[i]->empty_transition[t], "i:%d", i);

         } else if ( helperstate[i].type == state_RANGE
                     || helperstate[i].type == state_RANGE_CONTINUE) {
            TEST(ndfa_state[state_idx]  == ndfa_state[i]->range_transition[t].state);
            TEST(helperstate[i].from[t] == ndfa_state[i]->range_transition[t].from);
            TEST(helperstate[i].to[t]   == ndfa_state[i]->range_transition[t].to);

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
      S = (unsigned) (3*(sizeof(state_t) - sizeof(range_transition_t)))
        + (unsigned) (2*sizeof(state_t*) + i*sizeof(range_transition_t));
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
   S += (unsigned) (2*(sizeof(state_t) - sizeof(range_transition_t)))
      + (unsigned) (2*sizeof(state_t*));
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
   S += (unsigned) (2*(sizeof(state_t) - sizeof(range_transition_t)))
      + (unsigned) (3*sizeof(state_t*));
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
   S += (unsigned) (2*(sizeof(state_t) - sizeof(range_transition_t)))
      + (unsigned) (3*sizeof(state_t*));
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

   for (int err = 13; err < 15; ++err) {
      for (int tc = 0, isDone = 0; !isDone; ++tc) {
         // prepare
         switch (tc) {
         case 0:  // TEST initmatch_automat: simulated ERROR
                  init_testerrortimer(&s_automat_errtimer, 1, err);
                  TEST( err == initmatch_automat(&ndfa, &mman, 1, from, to));
                  break;
         case 1:  // TEST initsequence_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, err);
                  mman.freesize = 0; // ==> new allocation needed ==> error
                  TEST( err == initsequence_automat(&ndfa, &ndfa1, &ndfa2));
                  // reset
                  TEST(0 == free_automat(&ndfa2));
                  TEST(0 == free_automat(&ndfa1));
                  break;
         case 2:  // TEST initor_automat: simulated ERROR
                  TEST(0 == initmatch_automat(&ndfa1, &mman, 1, from+1, to+1));
                  TEST(0 == initmatch_automat(&ndfa2, &mman, 1, from+2, to+2));
                  init_testerrortimer(&s_automat_errtimer, 1, err);
                  mman.freesize = 0; // ==> new allocation needed ==> error
                  TEST( err == initor_automat(&ndfa, &ndfa1, &ndfa2));
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
      S += sizeof(state_t) + (i-1) * sizeof(range_transition_t);
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
