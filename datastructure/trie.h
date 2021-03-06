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

   Copyright:
   This program is free software. See accompanying LICENSE file.

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
 * Manages an index which associates a binary key with a user defined pointer.
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
 * A single node consumes more nrbit > 1 bits and
 * has exactly pow(2,nrbits) possible child nodes.
 *
 * Path Compression (PC):
 * A node which has only 1 valid child stores nrbit
 * bits of the key which are used to select the child node
 * as prefix and removes the child. This is repeated until
 * a node has at least two childs or the node can not grow
 * any larger.
 *
 * */
struct trie_t {
   struct trie_node_t * root;
};

// group: lifetime

/* define: trie_INIT
 * Static initializer. */
#define trie_INIT \
         { 0 }

/* define: trie_INIT2
 * Static initializer. */
#define trie_INIT2(root) \
         { root }

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

// group: query

/* function: at_trie
 * Returns memory address of value of a previously stored (key, value) pair.
 * As long as trie is not changed the returned address is valid. It is allowed
 * to write a new value with *at_trie(...)=new_value;
 *
 * If there is no stored value the memory address 0 is returned. */
void ** at_trie(const trie_t * trie, uint16_t keylen, const uint8_t key[keylen]);

// group: foreach-support

void foreach_trie(trie_t * trie, IDNAME loopvar);

// TODO: add iterator support

// group: update

/* function: insert_trie
 * Insert a new (key, value) pair in the trie.
 * A copy of the key is stored in the trie so it can be freed after return.
 * The value pointer is stored in a node. If it points to an object
 * the object must exist as long the pair is not removed.
 *
 * Calls <insert2_trie> for its implementation with parameter islog set to true.
 *
 * Returns:
 * 0 - Insert was successful.
 * EEXIST - A value was already inserted with the given key.
 * ENOMEM - Out of memory. */
int insert_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * value);

/* function: tryinsert_trie
 * Same as <insert_trie> except error EEXIST is not logged.
 * Calls <insert2_trie> for its implementation with parameter islog set to false. */
int tryinsert_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * value);

/* function: remove_trie
 * Remove a (key, value) pair from the trie.
 * Calls <remove2_trie> for its implementation with parameter islog set to true.
 * The parameter contains the removed value after return.
 *
 * Returns:
 * 0 - Remove was successful.
 * ESRCH  - There is no value value associated with the given key. */
int remove_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** value);

/* function: tryremove_trie
 * Remove a (key, value) pair from the trie.
 * Same as <remove_trie> except that ESRCH is not logged. */
int tryremove_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** value);

// group: private-update

/* function: insert2_trie
 * Implements <insert_trie> and <tryinsert_trie>.
 * Parameter islog switches between the two. */
int insert2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * value, bool islog);

/* function: remove2_trie
 * Implements <remove_trie> and <tryremove_trie>.
 * Parameter islog switches between the two. */
int remove2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** value, bool islog);


// section: inline implementation

/* define: init_trie
 * Implements <trie_t.init_trie>. */
#define init_trie(trie) \
         (*(trie) = (trie_t) trie_INIT, 0)

/* define: insert_trie
 * Implements <trie_t.insert_trie>. */
#define insert_trie(trie, keylen, key, value) \
         (insert2_trie((trie), (keylen), (key), (value), true))

/* define: remove_trie
 * Implements <trie_t.remove_trie>. */
#define remove_trie(trie, keylen, key, value) \
         (remove2_trie((trie), (keylen), (key), (value), true))

/* define: tryinsert_trie
 * Implements <trie_t.tryinsert_trie>. */
#define tryinsert_trie(trie, keylen, key, value) \
         (insert2_trie((trie), (keylen), (key), (value), false))

/* define: tryremove_trie
 * Implements <trie_t.tryremove_trie>. */
#define tryremove_trie(trie, keylen, key, value) \
         (remove2_trie((trie), (keylen), (key), (value), false))


#endif
