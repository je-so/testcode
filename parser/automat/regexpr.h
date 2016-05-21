/* title: RegularExpression

   Erstellt aus einer textuellen Beschreibung eines
   regulären Ausdrucks eine strukturelle Beschreibung <regexpr_t>.

   Siehe <regexpr_t> für eine Definition von regülaren Ausdrücken.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: C-kern/api/proglang/regexpr.h
    Header file <RegularExpression>.

   file: C-kern/proglang/regexpr.c
    Implementation file <RegularExpression impl>.
*/
#ifndef CKERN_PROGLANG_REGEXPR_HEADER
#define CKERN_PROGLANG_REGEXPR_HEADER

#include "automat.h"

// == exported types
struct regexpr_t;
struct regexpr_err_t;



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_proglang_regexpr
 * Test <regexpr_t> functionality. */
int unittest_proglang_regexpr(void);
#endif

/* struct: regexpr_err_t
 * Speichert Fehlerbeschreibung, wenn bei der Syntaxanalyse in <init_regexpr> etwas schief geht. */
typedef struct regexpr_err_t {
   unsigned    type;
   char32_t    chr;
   const char* pos;
   const char* expect;
   char        unexpected[8];
} regexpr_err_t;

// group: log

/* function: log_regexprerr
 * Schreibt Fehlerbeschreibung auf ERRLOG.
 * Position des Fehlers Datei:Zeile:Spalte muss vom Aufrufer davor geloggt werden.
 * Parameter channel hat den Typ <log_channel_e>. */
void log_regexprerr(const regexpr_err_t *err, uint8_t channel);


/* struct: regexpr_t
 * Kapselt einen <automat_t>. Die Initialisierungsfunktion erzeugt
 * anhand einer regulären Beschreibungssprache einen Automaten.
 *
 * Der syntaktische Aufbau der regulären Sprache lautet:
 *
 * > re   = seq? ( ( '|' | '&' | '&!' ) seq? )* ;
 * > seq  = ( not? atom repeat? )* ;
 * > not  = '!' ; // operator not is applied after repeat operator
 * > repeat = ( '*' | '+' | '?' ) ; // operator repeat is applied before possible not operator
 * > atom = '(' re ')' | char | set ;
 * > set  = '[' '^'? ( char ( '-' char )? )+ ']' ; // ^ == match not the chars definied in the set
 * > char = '.' | no-special-char | '\' ( special-char | control-code ) ;
 * > special-char = '.' | '[' | ']' | '(' | ')' | '*' | '+' | '|' | '&' | ' ';
 * > control-code = 'n' | 'r' | 't' ;
 * > no-special-char = 'a' | 'A' | 'b' | 'B' ...
 *
 * Erklärung der Syntax:
 *
 * Die Regel "re =" definiert ein oder mehrere auch leere Sequenzen (seq),
 * die mit "oder" (|), "und" (&) bzw. "und nicht" (&!) verbunden sind.
 * Die Definition "A|B" erwartet ein "A" oder ein "B" als Eingabe.
 * Die Definition "[a-z] & x" erwartet einen Buchstaben aus [a..z] und ein "x",
 * was gleichbedeutend mit einem "x" ist.
 * Die Definition "[a-z] &! x" erwartet einen Buchstaben aus [a..z] und nicht ein "x",
 * was gleichbedeutend ist mit einem Buchstaben aus [a..w,y..z].
 *
 * Die Regel "seq =" definiert ein Sequenz aus einzelnen Atomen (atom).
 * Der Operator "!" vor dem Atom negiert es. D.h. "! a" erwartet alles außer
 * einem a. "!a" ist identisch mit ".* &! a".
 *
 * Ein '*', '+' nach dem Atom wiederholt es, wobei '*' das Atom auch 0 mal wiederholt,
 * d.h optional macht. Ein '?' nach dem Atom macht das Atom optional.
 *
 * - Der String "a*" erzeugt die Eingabesprache "", "a", "aa", "aaa", usw.
 * - Der String "a+" erzeugt die Eingabesprache "a", "aa", "aaa", usw.
 * - Der String "a?" erzeugt die Eingabesprache "" bzw. "a".
 *
 * Die Regel "atom =" definiert eine Menge (set), einen einzelnen Buchstaben (char)
 * bzw. einen weiteren regulären Ausdruck der in '(' und ')' eingeschlossen ist.
 * Der String "(a|b)" definiert also die Sprache "a" bzw. "b", die Klammern dienen
 * nur der zusammenfassung von "a|b" zu einer Einheit, auf die sich dann der "!"-Operator
 * bzw. die Operatoren "*","+" und "?" beziehen.
 *
 * Die Regel "set =" definiert ein Zeichen aus einer Menge.
 * "[abcdef]" ist die einfachste Form und listet alle mäglichen Zeichen einzeln auf.
 * Diese Schreibweise entspricht "(a|b|c|d|e|f)". Eine weitere Möglichkeit ist,
 * einen Bereich direkt anzugeben mittels "-". Beispielsweise definiert "[a-z0-9A-Z]"
 * ein Zeichen aus einer Menge von Kleinbuchstaben (a-z), oder der Menge der Zahlen (0-9)
 * oder der Menge der Großbuchstaben.
 *
 * Mit "^" als erstem Zeichen wird die Menge negiert. "[^0-9]" erwartet also ein einziges
 * Zeichen, das nicht eine Zahl ist.
 *
 * Die Regel "char =" definiert ein einziges Zeichen.
 * Der Punkt '.' steht dabei für ein beliebiges Unicode-Zeichen (0-\U7fffffff).
 * Die Zeichen "\\n", "\\r", und "\\t" werden ind die Control-Codes Newline (10),
 * Carriage-Return (13) und Tabulator (8) umgewandelt. Jejdes Zeichen kann mit "\\"
 * maskiert werden, so dass es sein besondere Bedeutung verliert und als normales
 * Eingabezeichen gelesen wird.
 *
 * Leerzeichen werden immer überlesen und sie können zur besseren Lesbarkeit überall
 * eingestreut werden. Nur Leerezeichen, die mit "\\" maskiert wurden, werden als
 * normales Eingabezeichen verstanden.
 * */
typedef struct regexpr_t {
   automat_t matcher;
} regexpr_t;

// group: lifetime

/* define: regexpr_FREE
 * Static initializer. */
#define regexpr_FREE \
         { automat_FREE }

/* function: init_regexpr
 * Initiailisiert regex mit der strukturelle Repräsentation des durch definition in Textform definierten regulären Ausdrucks.
 * Die Syntax eines regulären Ausdruck ist in <regexpr_t> beschrieben.
 *
 * Beispiele für Parameter definition:
 * "[a-zA-Z_][0-9a-zA-Z_]*" - Beschreibt einen Identifier, der mit einem Buchstaben oder '_' beginnt.
 * ".*" - Kein oder beliebig viele Zeichen inklusive Newline.
 * "[^\n]*"  - Kein oder beliebig viele Zeichen außer Newline. Das '\n' wird vom Compiler
 *             in das Newline Zeichen umesetzt.
 * "[^\\n]*" - Kein oder beliebig viele Zeichen außer Newline. Das '\\n' wird vom Compiler
 *             in die Zeichenfolge '\' und 'n' umgesetzt. Zeichenfolgen beginnend mit '\'
 *             werden als Controlcodes (\n == Newline, \t == Tab, \r == Carriage Return) bzw.
 *             als Escapesequenz zur MAskierung der Sonderfunktion des Zeichens betrachtet
 *             (z.B. \* == Das '*' Zeichen selbst, wird nicht als Wiederholungsoperator gesehen).
 *
 * Returns:
 * ESYNTAX - definition[len] contains a syntax error (error is not logged), errdescr is set.
 * EILSEQ  - definition[len] contains an illegaly encoded utf8 character (error is not logged).
 *           errdescr is set.
 * */
int init_regexpr(/*out*/regexpr_t* regex, size_t len, const char definition[len], /*err*/regexpr_err_t *errdescr);

/* function: free_regexpr
 * Gibt von regex belegten Speicher frei. */
int free_regexpr(regexpr_t* regex);

// group: query

// TODO: add query functions to regexpr_t


// section: inline implementation


#endif
