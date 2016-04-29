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
int initcopy_automat(/*out*/automat_t* restrict dest_ndfa, const automat_t* restrict src_ndfa, const automat_t* restrict use_mman);

/* function: initmove_automat
 * Moves content of src_ndfa to dest_ndfa.
 * dest_ndfa is set to freed state after return.
 * No allocated memory is touched. */
static inline void initmove_automat(/*out*/automat_t* restrict dest_ndfa, automat_t* restrict src_ndfa/*freed after return*/);

/* function: initmatch_automat
 * Erzeugt Automat ndfa = "".
 * Der Speicher wird vom selben Heap wie bei use_mman allokiert.
 * Falls use_mman == 0 wird ein neuer Heap angelegt. */
int initempty_automat(/*out*/automat_t* restrict ndfa, struct automat_t* restrict use_mman);

/* function: initmatch_automat
 * Erzeugt Automat ndfa = "[a-bc-de-f]", wobei a == from[0], b == to[0], c == from[1], usw.
 * Der Speicher wird vom selben Heap wie bei use_mman allokiert.
 * Falls use_mman == 0 wird ein neuer Heap angelegt. */
int initmatch_automat(/*out*/automat_t* restrict ndfa, struct automat_t* restrict use_mman, uint8_t nrmatch, char32_t match_from[nrmatch], char32_t match_to[nrmatch]);

/* function: initreverse_automat
 * Sei ndfa2 = "ABC" ==> erzeugt Automat ndfa = "CBA".
 * Alle Transitionen werden umgekehrt und zeigen vom ehemaligen Zielzustand
 * zum vormaligen Ausgangszustand. Der Startzustand wird zum Endzustand und umgekehrt.
 * Der Speicher wird vom selben Heap wie bei use_mman allokiert.
 * Falls use_mman == 0 wird ein neuer Heap angelegt. */
int initreverse_automat(/*out*/automat_t* restrict ndfa, const automat_t* ndfa2, const automat_t* use_mman);

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

/* function: print_automat
 * Gibt ein Folge von Zeilen der Form "a(0xaddrA): 'a-z'--> b(0xaddrB)" aus.
 * Ein '' steht für einen leeren Übergang(Transition), der keinen Buchstaben erwartet. */
void print_automat(automat_t const* ndfa);

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

// group: operations

/* function: opsequence_automat
 * Erzeugt Automat ndfa = "(ndfa)(ndfa2)"
 * Der Speicher wird vom Heap von ndfa allokiert.
 * Falls ndfa2 nicht denselben Heap benutzt, wird der Inhalt kopiert. */
int opsequence_automat(automat_t* restrict ndfa, automat_t* restrict ndfa2/*freed after return*/);

/* function: oprepeat_automat
 * Erzeugt Automat ndfa = "(ndfa)*". */
int oprepeat_automat(/*out*/automat_t* ndfa);

/* function: opor_automat
 * Erzeugt Automat ndfa = "(ndfa)|(ndfa2)"
 * Der Speicher wird vom Heap von ndfa allokiert.
 * Falls ndfa2 nicht denselben Heap benutzt, wird der Inhalt kopiert. */
int opor_automat(/*out*/automat_t* restrict ndfa, automat_t* restrict ndfa2/*freed after return*/);

/* function: opand_automat
 * Erzeugt Automat ndfa = "(ndfa) & (ndfa2)".
 * Der erzeugte Automat erkennt Zeichenfolgen, die von beiden AUtomaten gemeinsam erkannt werden. */
int opand_automat(automat_t* restrict ndfa, const automat_t* restrict ndfa2);

/* function: opandnot_automat
 * Erzeugt Automat ndfa = "(ndfa) & !(ndfa2)".
 * Der erzeugte Automat erkennt Zeichenfolgen, die von ndfa aber nicht von ndfa2 erkannt werden. */
int opandnot_automat(automat_t* restrict ndfa, const automat_t* restrict ndfa2);

/* function: opnot_automat
 * Erzeugt Automat ndfa = "!(ndfa)" bzw. gleichbedeutend mit ndfa = "([\x00-\xffffffff]*) & !(ndfa)". */
int opnot_automat(automat_t* restrict ndfa);

// group: optimize

/* function: makedfa_automat
 * Wandelt ndfa in gleichwertigen deterministischen endlichen Automaten um.
 * Leere Übergange (die keine Eingaben erwarten) und mehrdeutige Übergange
 * werden eliminiert.
 *
 * Das folgende Beispiel zeigt die Transformation:
 * nfa ( start: ""->m1; m1: "a"->m2, "a"->m3; m2: "b"->e; m3: "c"->e; e: <endstate> )
 * dfa ( start: "a"->m23; m23: "b"->e, "c"->e; e: <endstate> )
 * */
int makedfa_automat(automat_t* ndfa);

/* function: minimize_automat
 * Generiert einen auf minimale Anzahl an Zuständen optimierten deterministischen endlichen Automaten.
 *
 * Funktionsweise:
 * Der Automat ndfa wird umgekehrt, d.h. der Startzustand wird zum Endzustand und umgekehrt und alle
 * Übergange werden umgekehrt, zeigen dann vom vormaligen Zielzustand aud den vormaligen Ausgangszustand.
 * Danach wird der umgekehrte Automat in einen DFA verwandelt, damit alle Zustände,
 * die über gleiche Eingabesequenzen erreicht werden, zu einem zusammengefasst werden.
 * D.h. alle Zustände des Ausgangsautomaten, die dasselbe Suffix produzieren, werden
 * im umgekehrten Automaten zu einem einzigen zusammengefasst.
 * Danach wird der umgekehrte Automat wiederum umgekehrt und dann nochmals in einen DFA verwandelt.
 * Dieser DFA ist minimal, da Zustände, die keine zwei verschiedene Zustände gleiche Suffixe produzieren.
 * */
int minimize_automat(automat_t* ndfa);


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
