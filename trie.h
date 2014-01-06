/* title: Trie

   Implements a trie, a compact prefix tree.
   The trie stores binary strings with characters of type uint8_t.
   The nodes in the tree do not store the keys themselves but the
   key is dervied from the position of the node in the tree.
   Every node in the subtree of a node shares the same prefix key.

   The trie is compact in that it allows to label an edge between two nodes with more than one character.
   Therefore a chain of nodes with only one child (n1-"x"->n2-"y"->n3-"z"->n4) can be compressed
   into one node (n1-"xyz"->n2).

   A trie node is of flexible size and is reallocated in case of insertions or deletions.
   Therefore the trie does not store keys with associated user nodes but only keys
   with associated pointers which can point to user defined values.

   >       n1
   >    a/    \b    \c
   >   n2      n3    n8
   >  c/ \d   x/ \y
   >  n4  n5  n6  n7
   >
   > Keys of nodes:
   > n1: ""
   > n2: "a"
   > n3: "b"
   > n4: "ac"
   > n5: "ad"
   > n6: "bx"
   > n7: "by"
   > n8: "c"

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/ds/inmem/trie.h
    Header file <Trie>.

   file: C-kern/ds/inmem/trie.c
    Implementation file <Trie impl>.
*/
#ifndef CKERN_DS_INMEM_TRIE_HEADER
#define CKERN_DS_INMEM_TRIE_HEADER

// forward
struct trie_node_t;

/* typedef: struct trie_t
 * Export <trie_t> into global namespace. */
typedef struct trie_t trie_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_ds_inmem_trie
 * Test <trie_t> functionality. */
int unittest_ds_inmem_trie(void);
#endif


/* struct: trie_t
 * TODO: describe type */
struct trie_t {
   struct trie_node_t * root;
};

// group: lifetime

/* define: trie_INIT
 * Static initializer. */
#define trie_INIT                         { 0 }

/* define: trie_INIT_FREEABLE
 * Static initializer. */
#define trie_INIT_FREEABLE                { 0 }

/* function: init_trie
 * Initializes trie with 0 pointer. */
int init_trie(/*out*/trie_t * trie);

/* function: free_trie
 * Frees all nodes and their associated memory.
 * If you need to free any objects which are referenced by the stored user pointers
 * you have to iterate over the stored pointers and free them before calling <free_trie>. */
int free_trie(trie_t * trie);

// group: foreach-support

// TODO: add iterator support

// group: query

/* function: at_trie
 * TODO: describe
 * During trie traversal two nodes are merged into one if this saves space. Therefore trie is not declared as const.
 * */
void ** at_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen]);

// group: update

/* function: insert_trie
 * TODO: describe */
int insert_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * uservalue);

/* function: tryinsert_trie
 * TODO: describe */
int tryinsert_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * uservalue);

/* function: remove_trie
 * TODO: describe */
int remove_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** uservalue);

/* function: tryremove_trie
 * TODO: describe */
int tryremove_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** uservalue);

// group: private

/* function: insert2_trie
 * TODO: describe */
int insert2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * uservalue, bool islog);

/* function: remove2_trie
 * TODO: describe */
int remove2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** uservalue, bool islog);


// section: inline implementation

/* define: init_trie
 * Implements <trie_t.init_trie>. */
#define init_trie(trie) \
         (*(trie) = (trie_t) trie_INIT, 0)

/* define: insert_trie
 * Implements <trie_t.insert_trie>. */
#define insert_trie(trie, keylen, key, uservalue) \
         (insert2_trie((trie), (keylen), (key), (uservalue), true))

/* define: remove_trie
 * Implements <trie_t.remove_trie>. */
#define remove_trie(trie, keylen, key, uservalue) \
         (remove2_trie((trie), (keylen), (key), (uservalue), true))

/* define: tryinsert_trie
 * Implements <trie_t.tryinsert_trie>. */
#define tryinsert_trie(trie, keylen, key, uservalue) \
         (insert2_trie((trie), (keylen), (key), (uservalue), false))

/* define: tryremove_trie
 * Implements <trie_t.tryremove_trie>. */
#define tryremove_trie(trie, keylen, key, uservalue) \
         (remove2_trie((trie), (keylen), (key), (uservalue), false))


#endif
