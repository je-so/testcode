/* title: SingleLinkedList-Node

   Defines node type <slist_node_t> which can be stored in a
   list of type <slist_t>. Management overhead of objects which
   wants to be stored in a <slist_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/slist_node.h
    Header file of <SingleLinkedList-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_SLIST_NODE_HEADER
#define CKERN_DS_INMEM_NODE_SLIST_NODE_HEADER

// === exported types
struct slist_node_t;


/* struct: slist_node_t
 * Provides the means for linking an object to another of the same type.
 * This kind of object is managed by the single linked list object <slist_t>.
 * The next node can be reached from this node in O(1).
 * An object which wants to be member of a list must inherit from <slist_node_t>. */
typedef struct slist_node_t {
   // group: public fields
   /* variable: next
    * Points to next node in the list.
    * If this node is currently not part of any list this value is set to NULL. */
   struct slist_node_t * next;
} slist_node_t;

// group: lifetime

/* define: slist_node_INIT
 * Static initializer.
 * Before inserting a node into a list do not forget to initialize the next pointer with NULL.
 *
 * Note:
 * The next pointer is checked against NULL in the precondition of every insert function
 * of every list implementation.
 * This ensures that a node is not inserted in more than one list by mistake. */
#define slist_node_INIT { 0 }

// group: generic

/* define: slist_node_EMBED
 * Allows to embed members of <slist_node_t> into another structure.
 *
 * Parameter:
 * nextID - The name of the embedded <slist_node_t.next> member.
 *
 * Your object must inherit or embed <slist_node_t> to be manageable by <slist_t>.
 * With macro <slist_node_EMBED> you can do
 *
 * > struct object_t {
 * >    ...
 * >    slist_node_EMBED(next); // declares: slist_node_t * next;
 * >    ...
 * > };
 *
 * instead of
 *
 * > struct object_t {
 * >    ...
 * >    slist_node_t listnode;
 * >    ...
 * > }; */
#define slist_node_EMBED(nextID) \
   slist_node_t * nextID


#endif
