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

// forward
struct automat_mman_t;

// === exported types
struct automat_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_automat
 * Test <automat_t> functionality. */
int unittest_proglang_automat(void);
#endif


/* struct: automat_t
 * Verwaltet (nicht-)deterministische endliche Automaten.
 * Siehe auch <Zustandsdiagramm at https://de.wikipedia.org/wiki/Zustandsdiagramm_(UML)>
 *
 * Der Automat hat genau einen Startzustand und genau einen Endzustand.
 * Im Falle eines optimierten Automaten, der optimalerweise mehrere Endzustände hätte,
 * verweisen diese per zusätzlicher "empty transition" auf den einzigen nicht
 * wegoptimierten Endzustand.
 *
 * */
typedef struct automat_t {
   // group: private
   struct
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
int initempty_automat(/*out*/automat_t* ndfa);

/* function: initmatch_automat
 * TODO: Describe Initializes object. */
int initmatch_automat(/*out*/automat_t* ndfa, struct automat_mman_t * mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);

/* function: initcopy_automat
 * Makes dest_ndfa a copy of src_ndfa.
 * The memory for building dest_ndfa is allocated from the
 * same memory heap as determined by use_mman_from.
 * This function is used internally cause a every state of a single automat must
 * be located on the same memory heap. Any operation applied to two different automat
 * makes sure that the result of the operation is located on the same heap. */
int initcopy_automat(/*out*/automat_t* dest_ndfa, automat_t* src_ndfa, const automat_t* use_mman_from);

// group: update

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
