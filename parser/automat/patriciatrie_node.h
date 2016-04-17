/* title: PatriciaTrie-Node

   Defines node type <patriciatrie_node_t> which can be stored in tries
   of type <patriciatrie_t>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/node/patriciatrie_node.h
    Header file <PatriciaTrie-Node>.
*/
#ifndef CKERN_DS_INMEM_NODE_PATRICIATRIE_NODE_HEADER
#define CKERN_DS_INMEM_NODE_PATRICIATRIE_NODE_HEADER

/* typedef: struct patriciatrie_node_t
 * Export <patriciatrie_node_t> into global namespace. */
typedef struct patriciatrie_node_t           patriciatrie_node_t ;


/* struct: patriciatrie_node_t
 * Management overhead of objects which wants to be stored in a <patriciatrie_t>.
 * >
 * >                ╭───────╮
 * >                │ node  │
 * >            left├───────┤right
 * > (bit at off-╭──┤ offset├──╮ (bit at
 * >  set is 0)  │  ╰───────╯  │  offset is 1)
 * >        ╭────∇──╮       ╭──∇────╮
 * >        │ left  │       │ right │
 * >        ├───────┤       ├───────┤
 * >        │ offset│       │ offset│
 * >        ╰┬─────┬╯       ╰┬─────┬╯
 * >        left  right    left  right */
struct patriciatrie_node_t {
   /* variable: bit_offset
    * The bit offset of the bit to test. The bit with offset 0 is bit 0x80 of the first byte of the key. */
   size_t               bit_offset ;
   /* variable: left
    * Follow left pointer if testet bit at <bit_offset> is 0.  */
   patriciatrie_node_t  * left ;
   /* variable: right
    * Follow right pointer if testet bit at <bit_offset> is 1.  */
   patriciatrie_node_t  * right ;
} ;

// group: lifetime

/* define: patriciatrie_node_INIT
 * Static initializer. */
#define patriciatrie_node_INIT { 0, 0, 0 }


#endif
