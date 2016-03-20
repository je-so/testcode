/* title: FiniteStateMachine

   Type <automat_t> supports construction of finite state machines.

   Non-deterministic and deterministic automaton are supported.

   See also <nondeterministic_finite_automaton at https://en.wikipedia.org/wiki/Nondeterministic_finite_automaton>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
#ifndef CKERN_PROGLANG_AUTOMAT_HEADER
#define CKERN_PROGLANG_AUTOMAT_HEADER

#include "slist.h"

// === exported types
struct automat_t;
struct automat_mman_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_automat
 * Test <automat_t> functionality. */
int unittest_proglang_automat(void);
#endif


/* struct: automat_mman_t
 *
 * TODO: replace automat_mman_t with general stream_memory_manager_t
 *
 * */
typedef struct automat_mman_t {
   slist_t  pagelist;   // list of allocated memory pages
   slist_t  pagecache;  // list of free pages not freed, waiting to be reused
   size_t   refcount;   // number of <automat_t> which use resources managed by this object
   uint8_t* freemem;    // freemem points to end of memory_page_t
                        // addr_next_free_memblock == freemem - freesize
   size_t   freesize;   // 0 <= freesize <= memory_page_SIZE - offsetof(memory_page_t, data)
   size_t   allocated;  // total amount of allocated memory
} automat_mman_t;

// group: lifetime

/* define: automat_mman_INIT
 * Statischer Initialisierer. */
#define automat_mman_INIT \
         { slist_INIT, slist_INIT, 0, 0, 0, 0 }

/* function: free_automatmman
 * Gibt alle belegten (Speicher-)Ressourcen frei. */
int free_automatmman(automat_mman_t * mman);

// group: query

/* function: sizeallocated_automatmman
 * Gibt insgesamt allokierten Speicher in Bytes zurück. */
size_t sizeallocated_automatmman(const automat_mman_t * mman);


/* struct: automat_t
 * Verwaltet (nicht-)deterministische endliche Automaten.
 * Siehe auch <Zustandsdiagramm at https://de.wikipedia.org/wiki/Zustandsdiagramm_(UML)>
 *
 * */
typedef struct automat_t {
   // group: private
   automat_mman_t *  mman;
   size_t            nrstate;
   /* variable: states
    * Liste aller Automaten-Zustände. */
   slist_t           states;
} automat_t;

// group: lifetime

/* define: automat_FREE
 * Static initializer. */
#define automat_FREE \
         { 0, 0, slist_INIT }

/* function: initmatch_automat
 * TODO: Describe Initializes object. */
int initmatch_automat(/*out*/automat_t* ndfa, automat_mman_t * mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);

/* function: initsequence_automat
 * TODO: Describe Initializes object.
 *
 * Precondition:
 * - ndfa1 != ndfa2 && "both objects use same <automat_mman_t>". */
int initsequence_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: initrepeat_automat
 * TODO: Describe Initializes object. */
int initrepeat_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/);

/* function: initor_automat
 * TODO: Describe Initializes object.
 *
 * Precondition:
 * - ndfa1 != ndfa2 && "both objects use same <automat_mman_t>". */
int initor_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: initand_automat
 * TODO: Not implemented.
 *
 * Precondition:
 * - ndfa1 != ndfa2 && "both objects use same <automat_mman_t>". */
int initand_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: initandnot_automat
 * TODO: Not implemented.
 *
 * Precondition:
 * - ndfa1 != ndfa2 && "both objects use same <automat_mman_t>". */
int initandnot_automat(/*out*/automat_t* ndfa, automat_t* ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: free_automat
 * Verringert nur den Referenzzähler des verwendeten <automat_mman_t>.
 * Der allokierte Speicher selbst bleibt unverändert. Dies beschleunigt die
 * Freigabe von Speicher, der insgesamt mit Feigabe des besagten <automat_mman_t>
 * freigegeben wird. */
int free_automat(automat_t* ndfa);

// group: query

/* function: nrstate_automat
 * Gibt Anzahl Zustände des Automaten zurück. */
size_t nrstate_automat(const automat_t* ndfa);

/* function: startstate_automat
 * Gibt den einzigen Startzustand des automaten zurück. */
const void* startstate_automat(const automat_t* ndfa);

/* function: endstate_automat
 * Endzustand des Automaten. Im Falle eines optimierten Automaten,
 * der mehrere Endzustände hat, verweisen diese per zuätzlicher "empty transition"
 * auf den einzigen und nicht optimierten Endzustand. */
const void* endstate_automat(const automat_t* ndfa);

// group: update

/* function: addmatch_automat
 * Erweitert den Automaten um weiter zu matchende Characters.
 * Wird nur dann verwendet, falls <initmatch_automat> nicht ausreicht,
 * um alle Characterbereiche aufzuzählen. Ein Aufruf in anderen Fällen führt
 * zu fehlerhaften Ergebnissen!
 *
 * Precondition:
 * - ndfa initialized with initmatch_automat */
int addmatch_automat(automat_t* ndfa, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);


// section: inline implementation

/* define: nrstate_automat
 * Implements <automat_t.nrstate_automat>. */
#define nrstate_automat(ndfa) \
         ((ndfa)->nrstate)

#endif
