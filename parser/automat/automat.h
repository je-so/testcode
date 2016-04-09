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
   size_t            allocated;
   /* variable: states
    * Liste aller Automaten-Zustände. */
   slist_t           states;
} automat_t;

// group: lifetime

/* define: automat_FREE
 * Static initializer. */
#define automat_FREE \
         { 0, 0, 0, slist_INIT }

/* function: free_automat
 * Verringert nur den Referenzzähler des verwendeten <automat_mman_t>.
 * Der allokierte Speicher selbst bleibt unverändert. Dies beschleunigt die
 * Freigabe von Speicher, der insgesamt mit Feigabe des besagten <automat_mman_t>
 * freigegeben wird. */
int free_automat(automat_t* ndfa);

/* function: initcopy_automat
 * Makes dest_ndfa a copy of src_ndfa.
 * The memory for building dest_ndfa is allocated from the
 * same memory heap as determined by use_mman.
 * This function is used internally cause a every state of a single automat must
 * be located on the same memory heap. Any operation applied to two different automat
 * makes sure that the result of the operation is located on the same heap. */
int initcopy_automat(/*out*/automat_t* dest_ndfa, automat_t* src_ndfa, const automat_t* use_mman);

/* function: initmove_automat
 * Moves content of src_ndfa to dest_ndfa.
 * dest_ndfa is set to freed state after return.
 * No allocated memory is touched. */
static inline void initmove_automat(/*out*/automat_t* dest_ndfa, automat_t* src_ndfa/*freed after return*/);

/* function: initmatch_automat
 * Erzeugt Automat ndfa = "".
 * Der Speicher wird vom selben Heap wie bei use_mman allokiert.
 * Falls use_mman == 0 wird ein neuer Heap angelegt. */
int initempty_automat(/*out*/automat_t* ndfa, struct automat_t* use_mman);

/* function: initmatch_automat
 * Erzeugt Automat ndfa = "[a-bc-de-f]", wobei a == from[0], b == to[0], c == from[1], usw.
 * Der Speicher wird vom selben Heap wie bei use_mman allokiert.
 * Falls use_mman == 0 wird ein neuer Heap angelegt. */
int initmatch_automat(/*out*/automat_t* ndfa, struct automat_t* use_mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);

/* function: initsequence_automat
 * Erzeugt Automat ndfa = "(ndfa1)(ndfa2)"
 * Der Speicher wird vom selben Heap wie bei ndfa1 allokiert.
 * TODO: copy ndfa2 if necessary */
int initsequence_automat(/*out*/automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/, automat_t* restrict ndfa2/*freed after return*/);

/* function: initrepeat_automat
 * Erzeugt Automat ndfa = "(ndfa1)*".
 * Der Speicher wird vom selben Heap wie bei ndfa1 allokiert. */
int initrepeat_automat(/*out*/automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/);

/* function: initor_automat
 * Erzeugt Automat ndfa = "(ndfa1)|(ndfa2)"
 * Der Speicher wird vom selben Heap wie bei ndfa1 allokiert.
 * TODO: copy ndfa2 if necessary */
int initor_automat(/*out*/automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/, automat_t* restrict ndfa2/*freed after return*/);

/* function: initand_automat
 * Erzeugt Automat ndfa = "(ndfa1) & (ndfa2)".
 * Der erzeugte Automat erkennt Zeichenfolgen, die von beiden AUtomaten gemeinsam erkannt werden.
 *
 * TODO: Implement opand_automat.
 * TODO: copy ndfa2 if necessary */
int initand_automat(automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: initandnot_automat
 * Erzeugt Automat ndfa = "(ndfa1) & !(ndfa2)".
 * Der erzeugte Automat erkennt Zeichenfolgen, die von ndfa aber nicht von ndfa2 erkannt werden.
 * TODO: Implement opandnot_automat
 * TODO: copy ndfa2 if necessary */
int initandnot_automat(automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/, automat_t* ndfa2/*freed after return*/);

/* function: initnot_automat
 * Erzeugt Automat ndfa = "!(ndfa1)" bzw. gleichbedeutend mit ndfa = "(.*) & !(ndfa1)".
 * Der Speicher wird vom selben Heap wie bei ndfa1 allokiert. */
int initnot_automat(automat_t* restrict ndfa, automat_t* restrict ndfa1/*freed after return*/);

// group: query

/* function: nrstate_automat
 * Gibt Anzahl Zustände des Automaten zurück. */
size_t nrstate_automat(const automat_t* ndfa);

/* function: matchchar32_automat
 *
 * Returns:
 * 0 - Either ndfa was initialized with <initempty_automat> or the str is not matched by ndfa.
 * L - The value L is > 0. The string str[0..L-1] is recognized (matched) by ndfa.
 *     If parameter matchLongest was set to true then L gives the longest possible match.
 *     If parameter matchLongest was set to false then L is the shortest possible match.
 */
size_t matchchar32_automat(const automat_t* ndfa, size_t len, const char32_t str[len], bool matchLongest);

// group: extend

/* function: extendmatch_automat
 * Erweitert den Automaten um weiter zu matchende Characters.
 * Wird nur dann verwendet, falls <initmatch_automat> nicht ausreicht,
 * um alle Characterbereiche aufzuzählen. Ein Aufruf in anderen Fällen führt
 * zu fehlerhaften Ergebnissen!
 *
 * Unchecked Precondition:
 * - ndfa initialized with initmatch_automat */
int extendmatch_automat(automat_t* ndfa, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);


// section: inline implementation

/* define: nrstate_automat
 * Implements <automat_t.nrstate_automat>. */
#define nrstate_automat(ndfa) \
         ((ndfa)->nrstate)

/* define: initmove_automat
 * Implements <automat_t.initmove_automat>. */
static inline void initmove_automat(/*out*/automat_t* dest_ndfa, automat_t* src_ndfa/*freed after return*/)
{
         *dest_ndfa = *src_ndfa;
         *src_ndfa  = (automat_t) automat_FREE;
}

#endif
