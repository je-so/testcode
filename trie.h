/* title: Trie

   Implements a trie, a compact prefix tree.
   The trie stores binary strings with characters of type uint8_t.
   The nodes in the tree do not store the keys themselves but the
   key is derived from the position of the node in the tree hierarchy.
   Every node in a subtree shares the same prefix key as the root node
   of the subtree.

   The trie is compact in that it allows to label an edge between two nodes with more than one character.
   Therefore a chain of nodes with only one child (n1-"x"->n2-"y"->n3-"z"->n4) can be compressed
   into one node (n1-"xyz"->n2).

   A trie node has a dynamic size and is reallocated in case of insertions or deletions.
   Therefore the trie does not store keys with associated user nodes but only keys
   with associated pointers which can point to user defined values.

   >            n1
   >    "a"/    |"b"   \"c"
   >      n2    n3      n4
   >  "c"/ \"d"     "x"/ \"y"
   >    n5  n6        n7  n8
   >
   > Keys of nodes:
   > n1: ""
   > n2: "a"
   > n3: "b"
   > n4: "c"
   > n5: "ac"
   > n6: "ad"
   > n7: "cx"
   > n8: "cy"

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
struct trie_nodedata_t;

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
 * Manages an index which associates  abinary key with a user defined pointer.
 * The index is implemented as a trie.
 * The data structure contains a single pointer to the root node.
 *
 * Searching a value:
 * A node has at least two possible childs. The value of the first
 * bit of the key selects the left (0) or the right child.
 * The next bit of the key selects the left or right child of the
 * child node and so on until the last bit of the key has bin processed.
 * If the found child node contains a stored user value it is returned.
 *
 * Level Compression (LC):
 * A single node consumes more than nrbit > 1 bits and
 * has exactly pow(2,nrbits) possible child nodes.
 *
 * Path Compression (PC):
 * A node which has only 1 valid child stores a
 * the bit of the key as prefix and removes the child.
 * This is repeated until a node has at least two childs.
 *
 * */
struct trie_t {
   struct trie_nodedata_t * root;
};

// group: lifetime

/* define: trie_INIT
 * Static initializer. */
#define trie_INIT \
         { 0 }

/* define: trie_FREE
 * Static initializer. */
#define trie_FREE \
         { 0 }

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
