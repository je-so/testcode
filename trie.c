/* title: Trie impl

   Implements <Trie>.

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

#include "C-kern/konfig.h"
// TODO: insert next line and see if all functions are in use
// #undef KONFIG_UNITTEST // TODO: remove line
#include "C-kern/api/ds/inmem/trie.h"
#include "C-kern/api/err.h"
#include "C-kern/api/math/int/power2.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/ptr.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/test/mm/err_macros.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// forward
#ifdef KONFIG_UNITTEST
static test_errortimer_t   s_trie_errtimer;
#endif

/*
 * Implementierung:
 *
 *   TODO: Beschreibe Struktur der Knoten
 *
 *   TODO: Beschreibe ReadCursor(+ Update UserValue), InsertCursor, DeleteCursor !!
 *
 */

// TODO: implement ReadCursor, InsertCursor, DeleteCursor

typedef struct trie_node_t    trie_node_t;
typedef struct trie_subnode_t trie_subnode_t;


/* struct: header_t
 * Stores bit values of type <header_e>. */
typedef uint8_t header_t;

// group: types

/* enums: header_e
 * Bitvalues which encodes the optional data members of <trie_node_t>.
 * These values are stored in a data field of type <header_t>.
 *
 * header_KEYLENMASK  - Mask to determine the value of the following KEY configurations.
 * header_KEYLEN0     - No key[] member available
 * header_KEYLEN1     - trie_nodedata_t.key[0]    == binary key digit
 * header_KEYLEN2     - trie_nodedata_t.key[0..1] == binary key digits
 * header_KEYLEN3     - trie_nodedata_t.key[0..2] == binary key digits
 * header_KEYLEN4     - trie_nodedata_t.key[0..3] == binary key digits
 * header_KEYLEN5     - trie_nodedata_t.key[0..4] == binary key digits
 * header_KEYLEN6     - trie_nodedata_t.key[0..5] == binary key digits
 * header_KEYLENBYTE  - trie_nodedata_t.keylen    == contains key length
 *                      trie_nodedata_t.key[0..keylen-1] == binary key digits
 * header_USERVALUE   - If set indicates that uservalue member is available.
 * header_CHILD       - Child and digit array available. digit[x] contains next digit and child[x] points to next <trie_node_t>.
 * header_SUBNODE     - Subnode pointer is avialable and digit[0] counts the number of valid pointers to trie_node_t not null.
 *                      If a pointer in <trie_subnode_t> or <trie_subnode2_t> is null there is no entry with such a key.
 *                      The number of stored pointers is (digit[0]+1), i.e. it at least one pointer must be valid.
 * header_SIZEMASK  - Mask to determine the value of one of the following 6 size configurations.
 * header_SIZESHIFT - Number of bits to shift a masked size value to the right
 *                    All header_SIZEX values must be shifted left before they could be stored in the header.
 * header_SIZE0     - The size of the node is 2 * sizeof(void*)
 * header_SIZE1     - The size of the node is 4 * sizeof(void*)
 * header_SIZE2     - The size of the node is 8 * sizeof(void*)
 * header_SIZE3     - The size of the node is 16 * sizeof(void*)
 * header_SIZE4     - The size of the node is 32 * sizeof(void*)
 * header_SIZE5     - The size of the node is 64 * sizeof(void*)
 * */
enum header_e {
   header_KEYLENMASK = 7,
   header_KEYLEN0    = 0,
   header_KEYLEN1    = 1,
   header_KEYLEN2    = 2,
   header_KEYLEN3    = 3,
   header_KEYLEN4    = 4,
   header_KEYLEN5    = 5,
   header_KEYLEN6    = 6,
   header_KEYLENBYTE = 7,
   header_USERVALUE  = 8,
   header_SUBNODE    = 16,
   header_SIZEMASK  = 32+64+128,
   header_SIZESHIFT = 5,
   header_SIZE0    = 0,
   header_SIZE1    = 1,
   header_SIZE2    = 2,
   header_SIZE3    = 3,
   header_SIZE4    = 4,
   header_SIZE5    = 5,
   // header_SIZE6 / header_SIZE7 not used
   header_SIZEMAX  = header_SIZE5,
};

typedef enum header_e header_e;

// group: query

static inline unsigned needkeylenbyte_header(const uint8_t keylen)
{
   static_assert( header_KEYLEN0 == 0 && header_KEYLEN6 == 6
                  && header_KEYLENBYTE == 7 && header_KEYLENMASK == 7, "use 1 byte for keylength >= 7");
   return (keylen >= header_KEYLENBYTE);
}

static inline header_t keylen_header(const header_t header)
{
   return (header_t) (header & header_KEYLENMASK);
}

static inline header_t sizeflags_header(const header_t header)
{
   return (header_t) ((header & header_SIZEMASK) >> header_SIZESHIFT);
}

static inline int issubnode_header(const header_t header)
{
   return (header & header_SUBNODE);
}

static inline int isuservalue_header(const header_t header)
{
   return (header & header_USERVALUE);
}

// group: change

static inline header_t addflags_header(const header_t header, const header_t flags)
{
   return (header_t) (header | flags);
}

static inline header_t delflags_header(const header_t header, const header_t flags)
{
   return (header_t) (header & ~flags);
}

static inline header_t encodekeylenbyte_header(header_t header)
{
   static_assert( header_KEYLENBYTE == header_KEYLENMASK, "oring value is enough");
   return addflags_header( header, header_KEYLENBYTE);
}

static inline header_t encodekeylen_header(header_t header, const uint8_t keylen)
{
   static_assert( header_KEYLEN0 == 0 && header_KEYLEN6 == 6 && header_KEYLENMASK == 7, "encode keylen directly");
   return addflags_header( delflags_header(header, header_KEYLENMASK), keylen);
}

static inline header_t encodesizeflag_header(header_t header, const header_t sizeflag)
{
   return addflags_header(delflags_header(header, header_SIZEMASK), (header_t) (sizeflag << header_SIZESHIFT));
}


/* struct: trie_subnode_t
 * Points to 256 childs of type <trie_nodedata_t>.
 * If child[i] is 0 it means there is no child
 * for 8-bit binary digit i at a certain offset in the search key. */
struct trie_subnode_t {
   // group: struct fields
   /* variable: child
    * An array of 256 pointer to <trie_nodedata_t>.
    * If there is no child at a given key digit the pointer is set to 0. */
   trie_node_t * child[256];
};

// group: lifetime

/* function: delete_triesubnode
 * Frees allocated memory of subnode.
 * Referenced childs are not freed. */
static int delete_triesubnode(trie_subnode_t ** subnode)
{
   int err;
   trie_subnode_t * delnode = *subnode;

   if (delnode) {
      *subnode = 0;

      memblock_t mblock = memblock_INIT(sizeof(trie_subnode_t), (uint8_t*)delnode);
      err = FREE_ERR_MM(&s_trie_errtimer, &mblock);
      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   return err;
}

/* function: new_triesubnode
 * Allocates a single subnode.
 * All 256 pointer to child nodes are set to 0. */
static int new_triesubnode(/*out*/trie_subnode_t ** subnode)
{
   int err;
   memblock_t mblock;

   err = ALLOC_ERR_MM(&s_trie_errtimer, sizeof(trie_subnode_t), &mblock);
   if (err) goto ONABORT;
   memset(mblock.addr, 0, sizeof(trie_subnode_t));

   // out param
   *subnode = (trie_subnode_t*)mblock.addr;

   return 0;
ONABORT:
   return err;
}

// group: query

/* function: child_triesubnode
 * Returns child pointer for digit. */
static inline trie_node_t * child_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   return subnode->child[digit];
}

// group: change

/* function: setchild_triesubnode
 * Sets pointer to child node for digit. */
static inline void setchild_triesubnode(trie_subnode_t * subnode, uint8_t digit, trie_node_t * child)
{
   subnode->child[digit] = child;
}

/* function: clearchild_triesubnode
 * Clears pointer for digit. */
static inline void clearchild_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   subnode->child[digit] = 0;
}


/* struct: trie_node_t
 * Describes a flexible structure of trie node data stored in memory.
 * Two fields <header> and <nrchild> are followed by optional data fields.
 * The optional fields are a part of the key (prefix/subkey), a user pointer,
 * and optional digit and child arrays. Instead of digit and child arrays
 * a single pointer to a <trie_subnode_t> could be stored.
 *
 * The size of the structure is flexible.
 * It can use up to <trie_node_t.MAXSIZE> bytes.
 *
 * */
struct trie_node_t {
   // group: struct fields
   /* variable: header
    * Flags which describes content of <trie_nodedata_t>. See <header_t>. */
   header_t    header;
   /* variable: nrchild
    * Nr of childs stored in optional child[] array or <trie_subnode_t>.
    * The subnode can store up to 256 childs and the number of childs in a subnode is always >= 1.
    * In the subnode case the stored value is one less than the real number of child
    * to be able to count up to 256. */
   uint8_t     nrchild;
   /* variable: keylen
    * Start of byte-aligned data.
    * Contains optional byte size of key. */
   uint8_t     keylen;     // optional (fixed size)
   // uint8_t  key[keylen];   // optional (variable size)
   // uint8_t  digit[];    // optional (variable size)
   /* variable: uservalue
    * Start of ptr-aligned data.
    * Contains an optional user value. */
   void     *  uservalue;  // optional (fixed size)
   // void  *  child_or_subnode[];     // child:   optional (variable size) points to trie_nodedata_t
                                       // subnode: optional (size 1) points to trie_subnode_t
};

// group: constants

/* define: PTRALIGN
 * Alignment of <trie_nodedata_t.uservalue>. The first byte in trie_node_t
 * which encodes the availability of the optional members is followed by
 * byte aligned data which is in turn followed by pointer aligned data.
 * This value is the alignment necessary for a pointer on this architecture.
 * This value must be a power of two. */
#define PTRALIGN  (offsetof(trie_node_t, uservalue))

/* define: MAXSIZE
 * The maximum size in bytes used by a <trie_nodedata_t>. */
#define MAXSIZE   (64*sizeof(void*))

/* define: MAXNROFCHILD_NOUSERVALUE
 * The maximum nr of child pointer in child array of <trie_nodedata_t>.
 * The value is calculated with the assumption that no key and no user value are stored in the node.
 * If a node needs to hold more child pointers it has to switch to a <trie_subnode_t>.
 * This value must be less than 256 else <trie_nodedata_t.nrchild> would overflow.
 * */
#define MAXNROFCHILD_NOUSERVALUE \
         (  (MAXSIZE-offsetof(trie_node_t, keylen)) \
            / (sizeof(void*)+sizeof(uint8_t)))

/* define: MAXNROFCHILD_WITHUSERVALUE
 * The maximum nr of child pointer in child array of <trie_nodedata_t>.
 * The value is calculated with the assumption that no key is stored in the node
 * but an additional user value.
 * If a node needs to hold more child pointers it has to switch to a <trie_subnode_t>.
 * This value must be less than 256 else <trie_nodedata_t.nrchild> would overflow.
 * */
#define MAXNROFCHILD_WITHUSERVALUE \
         (  (MAXSIZE-offsetof(trie_node_t, keylen)-sizeof(void*)) \
            / (sizeof(void*)+sizeof(uint8_t)))

// group: query-header

static inline int issubnode_trienode(const trie_node_t * node)
{
   return issubnode_header(node->header);
}

static inline int isuservalue_trienode(const trie_node_t * node)
{
   return isuservalue_header(node->header);
}

/* function: decodesize_trienode
 * Returns the size in bytes of a node decoded from its <header_t>.
 * */
static inline unsigned decodesize_trienode(const trie_node_t * node)
{
   return (2*sizeof(void*)) << sizeflags_header(node->header);
}

// group: query-helper

/* function: alignoffset_trienode
 * Aligns offset to architecture specific pointer alignment. */
static inline unsigned alignoffset_trienode(unsigned offset)
{
   static_assert(ispowerof2_int(PTRALIGN), "use bit mask to align value");
   return (offset + PTRALIGN-1) & ~(PTRALIGN-1);
}

/* function: sizeuservalue_trienode
 * Returns 0 or sizeof(void*). */
static inline unsigned sizeuservalue_trienode(int isuservalue)
{
   return isuservalue ? sizeof(void*) : 0u;
}

static inline uint8_t * memaddr_trienode(trie_node_t * node)
{
   return (uint8_t*) node;
}

static inline unsigned off1_keylen_trienode(void)
{
   return offsetof(trie_node_t, keylen);
}

static inline unsigned off2_key_trienode(const unsigned islenbyte/*0 or 1*/)
{
   return off1_keylen_trienode() + islenbyte;
}

static inline unsigned off3_digit_trienode(const unsigned off2_key, const unsigned keylen)
{
   return off2_key + keylen;
}

static inline unsigned off4_uservalue_trienode(unsigned off3_digit, unsigned digitsize)
{
   return alignoffset_trienode(off3_digit + digitsize);
}

static inline unsigned off5_child_trienode(const unsigned off4_uservalue, const unsigned sizeuservalue)
{
   return off4_uservalue + sizeuservalue;
}

static inline unsigned off6_size_trienode(const unsigned off5_child, const unsigned childsize)
{
   return off5_child + childsize;
}

static inline uint8_t nrchild_trienode(const trie_node_t * node)
{
   return node->nrchild;
}

static inline void ** childs_trienode(trie_node_t * node, unsigned off5_child)
{
   return (void**) (memaddr_trienode(node) + off5_child);
}

static inline unsigned childsize_trienode(const int issubnode, const uint8_t nrchild)
{
   return issubnode ? sizeof(void*) : nrchild * sizeof(void*);
}

static inline uint8_t * digits_trienode(trie_node_t * node, unsigned off3_digit)
{
   return memaddr_trienode(node) + off3_digit;
}

static inline unsigned digitsize_trienode(const int issubnode, const uint8_t nrchild)
{
   return issubnode ? 0u : (unsigned)nrchild;
}

static inline uint8_t keylen_trienode(const trie_node_t * node)
{
   unsigned keylen = keylen_header(node->header);
   unsigned mask   = (keylen == header_KEYLENBYTE) ? 0 : 255;
   return (uint8_t) ((keylen & mask) + (node->keylen & ~mask));
}

/* function: subnode_trienode
 * Returns the pointer to the <trie_subnode_t>.
 *
 * Precondition:
 * The return value is only valid if <issubnode_trienode> returns true. */
static inline trie_subnode_t * subnode_trienode(trie_node_t * node, unsigned off5_child)
{
   return *(void**) (memaddr_trienode(node) + off5_child);
}

/* function: uservalue_trienode
 * Returned value is only valid if node contains a uservalue. */
static inline void * uservalue_trienode(trie_node_t * node, unsigned off4_uservalue)
{
   return *(void**) (memaddr_trienode(node) + off4_uservalue);
}

static inline unsigned childoff5_trienode(const trie_node_t * node)
{
   uint8_t keylen = keylen_trienode(node);
   unsigned  off2 = off2_key_trienode(needkeylenbyte_header(keylen));
   unsigned  off3 = off3_digit_trienode(off2, keylen);
   unsigned  off4 = off4_uservalue_trienode(off3, digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
   return off5_child_trienode(off4, sizeuservalue_trienode(isuservalue_trienode(node)));
}

// group: change-helper

/* function: subnode_trienode
 * Sets the pointer to <trie_subnode_t>.
 * Call this function onlyif you know that the node contains space for a subnode. */
static inline void setsubnode_trienode(trie_node_t * node, unsigned off5_child, trie_subnode_t * subnode)
{
   *(void**) (memaddr_trienode(node) + off5_child) = subnode;
}

/* function: setuservalue_trienode
 * Returned value is only valid if node contains a uservalue. */
static inline void setuservalue_trienode(trie_node_t * node, unsigned off4_uservalue, void * uservalue)
{
   *(void**) (memaddr_trienode(node) + off4_uservalue) = uservalue;
}

static inline void addheaderflag_trienode(trie_node_t * node, uint8_t flag)
{
   node->header = addflags_header(node->header, flag);
}

static inline void delheaderflag_trienode(trie_node_t * node, uint8_t flag)
{
   node->header = delflags_header(node->header, flag);
}

static inline void encodekeylen_trienode(trie_node_t * node, const uint8_t keylen)
{
   if (needkeylenbyte_header(keylen)) {
      node->header = encodekeylenbyte_header(node->header);
      node->keylen = keylen;
   } else {
      node->header = encodekeylen_header(node->header, keylen);
   }
}

static inline int allocmemory_trienode(/*out*/trie_node_t ** memaddr, unsigned memsize)
{
   int err;
   memblock_t mblock;

   err = ALLOC_ERR_MM(&s_trie_errtimer, memsize, &mblock);
   if (err) return err;

   // out param
   *memaddr = (trie_node_t*) mblock.addr;
   return 0;
}

static inline int freememory_trienode(trie_node_t * memaddr, unsigned memsize)
{
   memblock_t mblock = memblock_INIT(memsize, (uint8_t*)memaddr);
   return FREE_ERR_MM(&s_trie_errtimer, &mblock);
}

// TODO: !! implement shrinknode_trienode !!
// TODO: introduce policy which determines when to shrink a node !

/* function: expandnode_trienode
 * Allocates a new node of at least size newnode.
 * Only <trie_nodedata_t.header> of data is initialized to the correct value.
 * All other fields of data must be set by the caller.
 *
 * Unchecked Precondition:
 * - oldsize == decodesize_trienode(node)
 * - oldsize < MAXSIZE && newsize <= MAXSIZE
 * - oldsize < newsize
 * */
static inline int expandnode_trienode(
   /*out*/trie_node_t ** data,
   const header_t       nodeheader,
   unsigned             oldsize,
   unsigned             newsize)
{
   int err;
   header_t sizeflags    = sizeflags_header(nodeheader);
   unsigned expandedsize = oldsize;

   do {
      expandedsize *= 2;
      ++ sizeflags;
   } while (expandedsize < newsize);

   err = allocmemory_trienode(data, expandedsize);
   if (err) return err;

   // set trie_nodedata_t.header
   (*data)->header = encodesizeflag_header(nodeheader, sizeflags);

   return 0;
}

/* function: addsubnode_trienode
 * Moves all pointers in child[] array to subnode.
 * Then all pointers in child[] and bytes in digit[]
 * are removed and a single pointer to subnode
 * is stored in node.
 *
 * Unchecked Precondition:
 * - The node contains no subnode
 * - The node contains at least one child
 * */
static int addsubnode_trienode(trie_node_t * node, unsigned off3_digit)
{
   int err;
   trie_subnode_t * subnode;

   err = new_triesubnode(&subnode);
   if (err) return err;

   unsigned src_useroff = off4_uservalue_trienode(off3_digit, digitsize_trienode(0, nrchild_trienode(node)));
   unsigned dst_useroff = off4_uservalue_trienode(off3_digit, digitsize_trienode(1, nrchild_trienode(node)));
   unsigned usersize    = sizeuservalue_trienode(isuservalue_trienode(node));

   // copy child array into subnode
   const uint8_t nrchild = nrchild_trienode(node);
   const uint8_t* digits = digits_trienode(node, off3_digit);
   void * const*  childs = childs_trienode(node, off5_child_trienode(src_useroff, usersize));
   for (uint8_t i = 0; i < nrchild; ++i) {
      setchild_triesubnode(subnode, digits[i], childs[i]);
   }

   // remove digit / child arrays from node
   addheaderflag_trienode(node, header_SUBNODE);
   -- node->nrchild;
   memmove(memaddr_trienode(node) + dst_useroff, memaddr_trienode(node) + src_useroff, usersize);

   setsubnode_trienode(node, off5_child_trienode(dst_useroff, usersize), subnode);

   return 0;
}

/* function: trydelsubnode_trienode
 * Moves all pointers from subnode into digit[]/child[] array.
 * The node is reallocated if necessary.
 *
 * The error value EINVAL is returned in case if there is not
 * enough space for the digit and child arrays in a node of
 * size <MAXSIZE>. In case of error nothing is changed.
 *
 * Unchecked Precondition:
 * - The node contains a subnode
 * */
static int trydelsubnode_trienode(trie_node_t ** trienode, unsigned off3_digit)
{
   int err;
   trie_node_t *   node = *trienode;
   uint8_t  nrchild     = (uint8_t) (node->nrchild + 1);
   unsigned src_useroff = off4_uservalue_trienode(off3_digit, digitsize_trienode(1, nrchild));
   unsigned dst_useroff = off4_uservalue_trienode(off3_digit, digitsize_trienode(0, nrchild));
   unsigned usersize    = sizeuservalue_trienode(isuservalue_trienode(node));
   unsigned newsize     = dst_useroff + usersize + childsize_trienode(0, nrchild);

   if (newsize > MAXSIZE || nrchild == 0/*overflow*/) return EINVAL;

   trie_subnode_t * subnode = subnode_trienode(node, off5_child_trienode(src_useroff, usersize));

   // make room for digit / child arrays
   unsigned oldsize = decodesize_trienode(node);
   if (newsize > oldsize) {
      uint8_t *     srcaddr = memaddr_trienode(node);
      trie_node_t * oldnode = node;
      err = expandnode_trienode(&node, node->header, oldsize, newsize);
      if (err) return err;
      memcpy(memaddr_trienode(node) + sizeof(node->header), srcaddr + sizeof(node->header), off3_digit - sizeof(node->header));
      memcpy(memaddr_trienode(node) + dst_useroff, srcaddr + src_useroff, usersize);

      (void) freememory_trienode(oldnode, oldsize);

      // adapt inout param
      *trienode = node;

   } else {
      uint8_t * memaddr = memaddr_trienode(node);
      memmove(memaddr + dst_useroff, memaddr + src_useroff, usersize);
   }

   delheaderflag_trienode(node, header_SUBNODE);
   ++ node->nrchild;

   // copy childs
   uint8_t* digits = digits_trienode(node, off3_digit);
   void **  childs = childs_trienode(node, off5_child_trienode(dst_useroff, usersize));
   unsigned i = 0;
   for (unsigned digit = 0; digit <= 255; ++digit) {
      trie_node_t * child = child_triesubnode(subnode, (uint8_t)digit);
      if (child) {
         digits[i] = (uint8_t) digit;
         childs[i] = child;
         ++i;
      }
   }
   assert(i == node->nrchild);

   (void) delete_triesubnode(&subnode);

   return 0;
}

/* function: deluservalue_trienode
 * Removes the uservalue from the node.
 *
 * Unchecked Precondition:
 * - The node has a uservalue.
 */
static inline void deluservalue_trienode(trie_node_t * node, unsigned off4_uservalue)
{
   // TODO: test
   delheaderflag_trienode(node, header_USERVALUE);

   uint8_t * memaddr = memaddr_trienode(node);
   unsigned off5_child = off5_child_trienode(off4_uservalue, sizeuservalue_trienode(1));
   memmove( memaddr + off4_uservalue, memaddr + off5_child,
            off6_size_trienode(off5_child - off5_child, childsize_trienode(issubnode_trienode(node), nrchild_trienode(node))));
}

/* function: tryadduservalue_trienode
 * Adds a new user value to the node.
 * The node is resized if it is necessary.
 * The unlogged error value EINVAL is returned
 * if the node size would be bigger than <MAXSIZE>.
 *
 * Unchecked Precondition:
 * - The node has no uservalue */
static int tryadduservalue_trienode(trie_node_t ** trienode, unsigned off4_uservalue, void * uservalue)
{
// TODO: test
   int err;
   trie_node_t *  node       = *trienode;
   const unsigned childsize  = childsize_trienode(issubnode_trienode(node), nrchild_trienode(node));
   const unsigned off5_child = off5_child_trienode(off4_uservalue, sizeuservalue_trienode(1));
   const unsigned newsize    = off6_size_trienode(off5_child, childsize);

   if (MAXSIZE < newsize) return EINVAL;

   unsigned  oldsize = decodesize_trienode(node);

   if (oldsize < newsize) {
      trie_node_t * oldnode = node;
      uint8_t *     srcaddr = memaddr_trienode(node);
      err = expandnode_trienode(&node, node->header, oldsize, newsize);
      if (err) return err;

      uint8_t * destaddr = memaddr_trienode(node);
      memcpy( destaddr + sizeof(node->header), srcaddr + sizeof(node->header),
              off4_uservalue - sizeof(node->header));
      memcpy( destaddr + off5_child, srcaddr + off4_uservalue, childsize);

      (void) freememory_trienode(oldnode, oldsize);

      // adapt inout param
      *trienode = node;

   } else {
      uint8_t * memaddr = memaddr_trienode(node);
      memmove( memaddr + off5_child, memaddr + off4_uservalue, childsize);
   }

   addheaderflag_trienode(node, header_USERVALUE);
   setuservalue_trienode(node, off4_uservalue, uservalue);

   return 0;
 }

/* function: delkeyprefix_trienode
 * The first prefixkeylen bytes of key in node are removed.
 *
 * Unchecked Precondition:
 * - keylen_trienode(node) >= prefixkeylen
 * */
static void delkeyprefix_trienode(trie_node_t * node, uint8_t prefixkeylen)
{
// TODO: test

   // TODO: move calculation of offsets to top
   // then call childs_trienode with new parameter / same with digits_trienode

   uint8_t src_keylen   = keylen_trienode(node);
   unsigned dst_keylen  = (unsigned) src_keylen - prefixkeylen;
   unsigned src_keyoff  = off2_key_trienode(needkeylenbyte_header(src_keylen));
   unsigned dst_keyoff  = off2_key_trienode(needkeylenbyte_header((uint8_t)dst_keylen));
   unsigned src_digitoff= off3_digit_trienode(src_keyoff, src_keylen);
   unsigned dst_digitoff= off3_digit_trienode(dst_keyoff, dst_keylen);
   unsigned digitsize   = digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node));
   unsigned src_useroff = off4_uservalue_trienode(src_digitoff, digitsize);
   unsigned dst_useroff = off4_uservalue_trienode(dst_digitoff, digitsize);
   unsigned usersize    = sizeuservalue_trienode(isuservalue_trienode(node));
   unsigned childsize   = childsize_trienode(issubnode_trienode(node), nrchild_trienode(node));

   uint8_t * memaddr = memaddr_trienode(node);

   encodekeylen_trienode(node, (uint8_t)dst_keylen);
   // copy key + digit array !!
   memmove( memaddr + dst_keyoff, memaddr + src_keyoff + prefixkeylen, dst_keylen + nrchild_trienode(node));
   // copy uservalue + child array or subnode
   memmove( memaddr + dst_useroff, memaddr + src_useroff, usersize + childsize);
}

/* function: tryaddkeyprefix_trienode
 * The key[prefixkeylen] is prepended to the key stored in node.
 * If node is resized the child pointer of the parent node has to be adapted!
 *
 * Returns EINVAL if a prefix of size prefixkeylen does not fit into the node
 * even after having been resized.
 * */
static int tryaddkeyprefix_trienode(trie_node_t ** trienode, uint8_t prefixkeylen, const uint8_t key[prefixkeylen])
{
// TODO: test
   int err;

   trie_node_t  * node = *trienode;
   uint8_t  src_keylen = keylen_trienode(node);
   unsigned dst_keylen = (unsigned) src_keylen + prefixkeylen;

   // could node of MAXSIZE hold it ?
   if (255 < dst_keylen) return EINVAL;

   unsigned src_keyoff  = off2_key_trienode(needkeylenbyte_header(src_keylen));
   unsigned dst_keyoff  = off2_key_trienode(needkeylenbyte_header((uint8_t)dst_keylen));
   unsigned src_digitoff= off3_digit_trienode(src_keyoff, src_keylen);
   unsigned dst_digitoff= off3_digit_trienode(dst_keyoff, dst_keylen);
   unsigned digitsize   = digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node));
   unsigned src_useroff = off4_uservalue_trienode(src_digitoff, digitsize);
   unsigned dst_useroff = off4_uservalue_trienode(dst_digitoff, digitsize);
   unsigned ptrsize     = sizeuservalue_trienode(isuservalue_trienode(node))
                        + childsize_trienode(issubnode_trienode(node), nrchild_trienode(node));

   unsigned newsize = dst_useroff + ptrsize;
   if (newsize > MAXSIZE) return EINVAL;

   unsigned  oldsize  = decodesize_trienode(node);
   uint8_t * srcaddr  = memaddr_trienode(node);
   uint8_t * destaddr = srcaddr;

   trie_node_t * newnode = 0;
   if (oldsize < newsize) {
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;
      destaddr = memaddr_trienode(newnode);
      newnode->nrchild = nrchild_trienode(node);

      // adapt inout param
      *trienode = newnode;
   }

   // copy content
   memmove( destaddr + dst_useroff, srcaddr + src_useroff, ptrsize);
   if (!issubnode_trienode(node)) {
      memmove( destaddr + dst_digitoff, srcaddr + src_digitoff, nrchild_trienode(node));
   }
   memmove( destaddr + dst_keyoff + prefixkeylen, srcaddr + src_keyoff, src_keylen);
   memcpy( destaddr + dst_keyoff, key, prefixkeylen);

   // was node size expanded ?
   if (newnode) {
      (void) freememory_trienode(node, oldsize);
      node = newnode;
   }

   encodekeylen_trienode(node, (uint8_t)dst_keylen);

   return 0;
}

/* function: tryaddchild_trienode
 * Inserts new child into node.
 * The position in the child array is given in childidx.
 * The value lies between 0 and <nrchild_trienode> both inclusive.
 * If the node contains a <trie_subnode_t> parameter digit is used
 * instead of childidx.
 *
 * Unchecked Precondition:
 * - ! issubnode_trienode(node)
 * - nrchild_trienode(node) < 255
 * - childidx <= nrchild_trienode(node)
 * */
static int tryaddchild_trienode(trie_node_t * node, uint8_t childidx/*0 - nrchild*/, uint8_t digit, trie_node_t * child)
{
// TODO: test
   int err;
// TODO: pass offsets as parameter (keylen) !!
   uint8_t  keylen      = keylen_trienode(node);
   unsigned off2_key    = off2_key_trienode(needkeylenbyte_header(keylen));
   unsigned off3_digit  = off3_digit_trienode(off2_key, keylen);
   unsigned digitsize   = digitsize_trienode(0, nrchild_trienode(node));
   unsigned src_useroff = off4_uservalue_trienode(off3_digit, digitsize);
   unsigned usersize    = sizeuservalue_trienode(isuservalue_trienode(node));
   unsigned newnrchild  = nrchild_trienode(node) + 1u;
   unsigned dst_useroff = off4_uservalue_trienode(off3_digit, digitsize_trienode(0, (uint8_t)newnrchild));
   unsigned ptrsize     = usersize + childsize_trienode(0, (uint8_t) newnrchild);
   unsigned newsize     = dst_useroff + ptrsize;

   if (MAXSIZE < newsize) return EINVAL;

   unsigned  oldsize  = decodesize_trienode(node);
   uint8_t * srcaddr  = memaddr_trienode(node);
   uint8_t * destaddr = srcaddr;

   trie_node_t * newnode = 0;
   if (oldsize < newsize) {
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;
      destaddr = memaddr_trienode(newnode);
      memcpy( destaddr + sizeof(newnode->header),
              srcaddr + sizeof(newnode->header),
              off3_digit + childidx - sizeof(newnode->header));
   }

   // make room in child and digit arrays
   unsigned insoffset = childsize_trienode(0, childidx) + usersize;
   // child array after insoffset
   unsigned dst_insoff = dst_useroff + insoffset;
   memmove( destaddr + dst_insoff + sizeof(void*),
            srcaddr  + src_useroff + insoffset,
            newsize  - dst_insoff - sizeof(void*));
   *(void**)(destaddr + dst_insoff) = child;
   // uservalue + child array before insoffset
   memmove( destaddr + dst_useroff, srcaddr + src_useroff, insoffset);
   // digit array after childidx
   unsigned digitoff = off3_digit + childidx;
   memmove( destaddr + digitoff + sizeof(uint8_t),
            srcaddr  + digitoff,
            digitsize - childidx);
   destaddr[digitoff] = digit;

   if (newnode) {
      (void) freememory_trienode(node, oldsize);
      node = newnode;
   }

   ++ node->nrchild;

   return 0;
}

/* function: delchild_trienode
 * Inserts new child into node.
 * The position in the child array is given in childidx.
 * The value lies between 0 and <nrchild_trienode>-1 both inclusive.
 * If the node contains a <trie_subnode_t> parameter digit is used
 * instead of childidx.
 *
 * Unchecked Precondition:
 * - ! issubnode_trienode(node)
 * - 0 < nrchild_trienode(node)
 * - childidx < nrchild_trienode(node)
 * */
static void delchild_trienode(trie_node_t * node, uint8_t childidx/*0 - nrchild-1*/)
{
// TODO: test
// TODO: pass offsets as parameter (keylen) !!
   uint8_t  keylen      = keylen_trienode(node);
   unsigned off2_key    = off2_key_trienode(needkeylenbyte_header(keylen));
   unsigned off3_digit  = off3_digit_trienode(off2_key, keylen);
   unsigned digitsize   = digitsize_trienode(0, nrchild_trienode(node));
   unsigned src_useroff = off4_uservalue_trienode(off3_digit, digitsize);
   unsigned usersize    = sizeuservalue_trienode(isuservalue_trienode(node));
   unsigned dst_nrchild = nrchild_trienode(node)-1u;
   unsigned dst_digsize = digitsize_trienode(0, (uint8_t) dst_nrchild);
   unsigned dst_useroff = off4_uservalue_trienode(off3_digit, dst_digsize);
   unsigned ptrsize     = usersize + childsize_trienode(0, (uint8_t) dst_nrchild);
   unsigned newsize     = dst_useroff + ptrsize;

   // remove entries in child and digit arrays
   unsigned  deloffset = childsize_trienode(0, childidx) + usersize;
   uint8_t * memaddr   = memaddr_trienode(node);
   // digit array after childidx
   memmove( memaddr + off3_digit + childidx,
            memaddr + off3_digit + childidx + 1,
            dst_digsize - childidx);
   if (dst_useroff != src_useroff) {
      // uservalue + child array before deloffset
      memmove( memaddr + dst_useroff, memaddr + src_useroff, deloffset);
   }
   // child array after deloffset
   unsigned dst_deloff = dst_useroff + deloffset;
   memmove( memaddr + dst_deloff,
            memaddr + src_useroff + deloffset + sizeof(void*),
            newsize - dst_deloff);

   -- node->nrchild;
}


// group: lifetime

static int delete_trienode(trie_node_t ** node)
{
   int err;
   int err2;
   trie_node_t * delnode = *node;

   if (delnode) {
      *node = 0;

      err = 0;
      if (issubnode_trienode(delnode)) {
         trie_subnode_t * subnode = subnode_trienode(delnode, childoff5_trienode(delnode));
         err = delete_triesubnode(&subnode);
      }

      err2 = freememory_trienode(delnode, decodesize_trienode(delnode));
      if (err2) err = err2;

      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   return err;
}

/* function: new_trienode
 * Initializes node and reserves space in a newly allocated <trie_nodedata_t>.
 * The content of the data node (user value, child pointers (+ digits) and key bytes)
 * are undefined after return.
 *
 * The reserved keylen will be shrunk if a node of size <MAXSIZE> can not hold the
 * whole key. Therefore check the length of the reserved key after return.
 *
 * The child array is either stored in the node or a subnode is allocated if
 * nrchild is bigger than <MAXNROFCHILD_WITHUSERVALUE> or <MAXNROFCHILD_NOUSERVALUE>
 * which depends on isuservalue being true or false.
 *
 * Parameter nrchild:
 * The parameter nrchild can encode only 255 child pointers.
 * A subnode supports up to 256 child pointers so you have to increment
 * by one after return.
 * In case of a subnode the value of <trie_nodedata_t.nrchild> is one less
 * then the provided value in parameter nrchild.
 *
 * Unchecked Precondition:
 * - The digit array must be sorted in ascending order
 * - A node without uservalue and without childs is not allowed: (isuservalue != 0) || (nrchild > 0)
 * */
static int new_trienode(
   /*out*/trie_node_t** node,
   const int      isuservalue,
   uint8_t        nrchild,
   // keylen is reduced until it fits into a node of MAXSIZE with nrchild and a possible user value
   uint8_t        keylen,
   // TODO: make parameter optional (void*) [] ?? !!
   void *         uservalue,
   // Unchecked Precondition: (0 <= x < y < nrchild) ==> digit[x] < digit[y]
   const uint8_t  digit[nrchild],
   void *         child[nrchild],
   const uint8_t  key[keylen])
{
   int err;
   unsigned          size = isuservalue ? sizeof(void*) : 0;
   trie_subnode_t *  subnode = 0;

   size += off1_keylen_trienode();

   if (nrchild > (isuservalue ? MAXNROFCHILD_WITHUSERVALUE : MAXNROFCHILD_NOUSERVALUE)) {
      size += childsize_trienode(1, nrchild);
      err = new_triesubnode(&subnode);
      if (err) goto ONABORT;

      for (unsigned i = 0; i < nrchild; ++i) {
         subnode->child[digit[i]] = child[i];
      }

      -- nrchild; // subnode encodes one less
   } else {
      size += (size_t)nrchild/*size digit array*/ + childsize_trienode(0, nrchild);
   }

   unsigned iskeylenbyte = (unsigned) needkeylenbyte_header(keylen);
   size += keylen + iskeylenbyte;

   unsigned nodesize;
   header_t header;
   unsigned keyoffset;

   if (size > MAXSIZE) {
      // nodesize == MAXSIZE
      keyoffset = keylen;
      size -= keylen + iskeylenbyte;
      keylen = (uint8_t) (MAXSIZE - size);
      keylen = (uint8_t) (keylen - needkeylenbyte_header(keylen));
      iskeylenbyte = (unsigned) needkeylenbyte_header(keylen);
      size += keylen + iskeylenbyte;
      keyoffset -= keylen;
      header = header_SIZEMAX<<header_SIZESHIFT;
      nodesize = MAXSIZE;

   } else if (size > MAXSIZE / 8) {
      // grow nodesize
      nodesize = MAXSIZE / 4;
      for (header = header_SIZEMAX-2; nodesize < size; ++header) {
         nodesize *= 2;
      }
      header = (header_t) (header << header_SIZESHIFT);
      keyoffset = 0;

   } else {
      // shrink nodesize
      nodesize = MAXSIZE / 8;
      for (header = header_SIZEMAX-3; nodesize/2 >= size && header > header_SIZE0; --header) {
         nodesize /= 2;
      }
      header = (header_t) (header << header_SIZESHIFT);
      keyoffset = 0;
   }

   trie_node_t * newnode;
   err = allocmemory_trienode(&newnode, nodesize);
   if (err) goto ONABORT;

   header = addflags_header(header, isuservalue ? header_USERVALUE : 0);
   header = addflags_header(header, subnode ? header_SUBNODE : 0);

   unsigned offset = off1_keylen_trienode();

   if (iskeylenbyte) {
      header = encodekeylenbyte_header(header);
      newnode->keylen = keylen;
      ++ offset;
   } else {
      header = encodekeylen_header(header, keylen);
   }

   newnode->header  = header;
   newnode->nrchild = nrchild;

   // off2_key == offset;
   if (keylen) memcpy(memaddr_trienode(newnode) + offset, key+keyoffset, keylen);
   offset += keylen;
   unsigned off3_digit = offset;
   offset = off4_uservalue_trienode(offset, digitsize_trienode(subnode != 0, nrchild));
   // if (!isuservalue) ==> overwrites first child or subnode cause nrchild > 0 in this case
   setuservalue_trienode(newnode, offset, uservalue);
   offset = off5_child_trienode(offset, sizeuservalue_trienode(isuservalue));

   if (subnode) {
      setsubnode_trienode(newnode, offset, subnode);

   } else {
      uint8_t * dst_digit = digits_trienode(newnode, off3_digit);
      void   ** dst_child = childs_trienode(newnode, offset);
      memcpy(dst_digit, digit, digitsize_trienode(0, nrchild));
      memcpy(dst_child, child, childsize_trienode(0, nrchild));
   }

   // set out param
   *node = newnode;

   return 0;
ONABORT:
   (void) delete_triesubnode(&subnode);
   return err;
}


#if 0

/* function: newsplit_trienode
 * The prefix of splitnode is split.
 * A new node is created which contains the first splitlen bytes of prefix in splitnode.
 * The prefix in splitnode is shrunk to the last lenprefix_trienodeoffsets(splitnodeoffsets)-splitlen-1 bytes.
 *
 * Merge Case:
 * In case lenprefix_trienodeoffsets(splitnodeoffsets)-splitlen <= sizeof(trie_node_t*) it is possible that
 * the last part of the prefix of splitnode is merged into its child (if exactly one child and no uservalue).
 * In this case node is not allocated but the prefix in splitnode is adapted to new size splitlen and splitnode
 * is returned in node.
 *
 * Attention:
 * Do not use splitnode after return. It may be resized and therefore points to invalid memory.
 *
 * Unchecked Preconditions:
 * o child != 0 ==> uservalue not used (must be invalid)
 * o child == 0 ==> uservalue used (must be valid)
 * o child == 0 || digit != prefix_trienode(splitnode, splitnodeoffsets)[splitlen]
 * o splitlen < prefixlen
 * */
static int newsplit_trienode(/*out*/trie_node_t * node, trie_node_t * splitnode, uint8_t splitlen, void * uservalue, uint8_t digit, trie_nodedata_t * child)
{
   int err;
   uint8_t prefixlen = lenprefix_trienodeoffsets(&splitnode->offsets);
   uint8_t shrinklen = (uint8_t) (prefixlen-splitlen);

   if (  shrinklen <= sizeof(trie_node_t*)
         && !isuservalue_header(splitnode->offsets.header)
         // only single child ?
         && ischild_header(splitnode->offsets.header)
         && (1 == splitnode->offsets.lenchild || 0 == child_trienode(splitnode)[1])
         // is enough space for uservalue or child (precondition for adduservalue_trienode)
         && (  (child == 0 && sizeof(void*) <= shrinklen+sizedelete_trienodeoffsets(&splitnode->offsets))
               || splitnode->offsets.lenchild > 1 /*same as (1 == isfreechild_trienode(...))*/)
         ) {
      trie_node_t  mergenode = { .data = child_trienode(splitnode)[0] } ;
      initdecode_trienodeoffsets(&mergenode.offsets, mergenode.data);

      if (0 == tryextendprefix_trienode(  &mergenode, shrinklen,
                                          prefix_trienode(splitnode)+splitlen+1,
                                          digit_trienode(splitnode)[0])) {

         digit_trienode(splitnode)[0] = prefix_trienode(splitnode)[splitlen];
         shrinkprefixkeephead_trienode(splitnode, splitlen);

         if (child) {   // addchild
            static_assert(sizeof(void*) == sizeof(trie_node_t*), "size calculation is valid");
            uint8_t  * splitdigit = digit_trienode(splitnode);
            void    ** splitchild = child_trienode(splitnode);
            if (digit > digit_trienode(splitnode)[0]) {
               splitdigit[1] = digit;
               splitchild[1] = child;
            } else {
               splitdigit[1] = splitdigit[0];
               splitdigit[0] = digit;
               splitchild[1] = splitchild[0];
               splitchild[0] = child;
            }
         } else {
            adduservalue_trienode(splitnode, uservalue);
         }

         *node = *splitnode;
         return 0;
      }
   }

   // normal split (cause merge with single child not possible)
   uint8_t          * prefix = prefix_trienode(splitnode);
   trie_nodedata_t  * child2[2];
   uint8_t            digit2[2];
   uint8_t            childindex;
   if (!child || prefix[splitlen] < digit) {
      child2[0] = splitnode->data;
      child2[1] = child;
      digit2[0] = prefix[splitlen];
      digit2[1] = digit;
      childindex = 0;
   } else {
      child2[0] = child;
      child2[1] = splitnode->data;
      digit2[0] = digit;
      digit2[1] = prefix[splitlen];
      childindex = 1;
   }

   err = new_trienode(node, splitlen, prefix, child ? 0 : &uservalue, (uint16_t) (1 + (child != 0)), digit2, child2);
   if (err) goto ONABORT;
   // precondition OK: prefixlen-1u-splitlen < lenprefix_trienodeoffsets(splitnodeoffsets)
   shrinkprefixkeeptail_trienode(splitnode, (uint8_t)(shrinklen-1));
   // ignore error
   (void) shrinksize_trienode(splitnode);
   child_trienode(node)[childindex] = splitnode->data;

   return 0;
ONABORT:
   TRACEABORT_ERRLOG(err);
   return err;
}

// group: query

typedef struct trie_findresult_t   trie_findresult_t;

struct trie_findresult_t {
   trie_nodeoffsets_t offsets;
   // parent of node; 0 ==> node is root node
   trie_nodedata_t *  parent;
   // points to entry in child[] array (=> *child != 0) || points to entry in trie_subnode2_t (*child could be 0 => node == 0)
   trie_nodedata_t ** parent_child;
   // node == 0 ==> trie_t.root == 0; node != 0 ==> trie_t.root != 0
   trie_nodedata_t *  node;
   // points to node who contains child which starts prefix chain (chain of nodes with prefix + 1 child pointer; last node has no child pointer)
   trie_nodedata_t *  chain_parent;
   // points to entry in trie_nodedata_t->child[] or into trie_subnode2_t->child[] of chain_parent
   trie_nodedata_t ** chain_child;
   // number of bytes of key prefix which could be matched
   // (is_split == false ==> prefixlen of node contained; is_split == true ==> prefixlen of node not contained)
   uint16_t       matchkeylen;
   // points to child[childindex] whose digit[childindex] is bigger than key[matchkeylen];
   // only valid if returnalue == ESRCH && is_split == 0 && ischild_header(node->header)
   uint8_t        childindex;
   // only valid if is_split; gives the number of matched bytes in node
   uint8_t        splitlen;
   // false: whole prefix stored in node matched; true: node matched only partially (or not at all)
   bool           is_split;
};

/* function: findnode_trie
 * Finds node in trie who matches the given key fully or partially.
 * The returned result contains information if a node was found which matched full or
 * at least partially. */
static int findnode_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/trie_findresult_t * result)
{
   int err;
   result->parent       = 0;
   result->parent_child = &trie->root;
   result->node         = trie->root;
   result->chain_parent = 0;
   result->chain_child  = &trie->root;
   result->matchkeylen  = 0;
   result->is_split     = 0;

   if (!trie->root) return ESRCH;

   for (;;) {
      // parent == 0 || parent matched fully

      err = initdecode_trienodeoffsets(&result->offsets, result->node);
      if (err) return err;

      // match prefix
      uint8_t prefixlen = lenprefix_trienodeoffsets(&result->offsets);
      if (prefixlen) {
         const uint8_t * prefix  = prefix_trienode(result->node, &result->offsets);
         const uint8_t * key2    = key + result->matchkeylen;
         const bool      issplit = ((uint32_t)prefixlen + result->matchkeylen) > keylen;
         if (issplit || 0 != memcmp(key2, prefix, prefixlen)/*do not match*/) {
            uint8_t splitlen = 0;
            if (issplit) {
               unsigned maxlen = (unsigned) keylen - result->matchkeylen;
               while (splitlen < maxlen && key2[splitlen] == prefix[splitlen]) ++ splitlen;
            } else {
               while (key2[splitlen] == prefix[splitlen]) ++ splitlen;
            }
            result->is_split = 1;
            result->splitlen = splitlen;
            break;
         }
      }
      result->matchkeylen = (uint16_t) (result->matchkeylen + prefixlen);

      if (keylen == result->matchkeylen) return 0;  // isfound ?  (is_split == 0)

      if (! ischildorsubnode_header(result->offsets.header))   break; // no more childs ?

      uint8_t d = key[result->matchkeylen];

      // find child
      if (ischild_header(result->offsets.header)) {
         // search in child[] array
         trie_node_t ** child = child_trienode(result->node, &result->offsets);
         uint8_t *      digit = digit_trienode(result->node, &result->offsets);
         unsigned low  = 0;
         unsigned high = result->offsets.lenchild;
         while (low < high) {
            unsigned mid = (low+high)/2;
            if (!child[mid] || digit[mid] > d) {
               high = mid;
            } else if (digit[mid] < d) {
               low = mid + 1;
            } else  {
               low = mid;
               goto FOUND_CHILD;
            }
         }
         result->childindex = (uint8_t) low;
         break;
         FOUND_CHILD:
         result->parent       = result->node;
         result->parent_child = &child[low];
         result->node         = child[low];
         if (  0 != low || (1 < result->offsets.lenchild && 0 != child[1]) // more than one valid child pointer ?
               || isuservalue_header(result->offsets.header)) {            // uservalue ?
            result->chain_parent = result->parent;
            result->chain_child  = result->parent_child;
         }

      } else {
         // search in subnode->child[] array
         trie_subnode_t  * subnode  = subnode_trienodeoffsets(&result->offsets, result->node)[0];
         trie_subnode2_t * subnode2 = child_triesubnode(subnode, d)[0];
         trie_node_t    ** pchild   = child_triesubnode2(subnode2, d);
         if (!subnode2 || !(*pchild)) break;
         result->parent       = result->node;
         result->parent_child = pchild;
         result->node         = pchild[0];
         result->chain_parent = result->parent;
         result->chain_child  = result->parent_child;
      }
      ++ result->matchkeylen;
   }

   return ESRCH;
}

void ** at_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen])
{
   int err;
   trie_findresult_t findresult;

   err = findnode_trie(trie, keylen, key, &findresult);
   if (err || ! isuservalue_header(findresult.offsets.header)) return 0;

   return uservalue_trienodeoffsets(&findresult.offsets, findresult.node);
}

// group: update

// TODO: implement & test
int insert2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * uservalue, bool islog)
{
   int err;
   trie_node_t      * newchild = 0;
   trie_nodeoffsets_t offsets;
   trie_findresult_t  findresult;

   if (! trie->root) {
      // add to root
      err = new_trienode(&trie->root, &offsets, keylen, key, &uservalue, 0, 0, 0);
      if (err) goto ONABORT;

   } else {

      err = findnode_trie(trie, keylen, key, &findresult);
      if (err != ESRCH) {
         if (0 == err) err = EEXIST;
         goto ONABORT;
      }
      // findresult.matchkeylen < keylen

      if (findresult.is_split) {
         uint8_t  digit     = 0;
         uint16_t keyoffset = (uint16_t) (findresult.matchkeylen + findresult.splitlen);

         if (keylen > keyoffset) {
            err = new_trienode(&newchild, &offsets, (uint16_t) (keylen - keyoffset -1), key + keyoffset + 1, &uservalue, 0, 0, 0);
            if (err) goto ONABORT;
            digit = key[keyoffset];
         } else {
            // uservalue is added cause newchild == 0
         }

         // split node
         err = newsplit_trienode(&findresult.node, findresult.node, &findresult.offsets, findresult.splitlen, uservalue, digit, newchild);
         if (err) goto ONABORT;

      } else {
         // findresult.node != 0 ==> add to child[] or subnode / resize or split node if not enough space

         err = new_trienode(&newchild, &offsets, (uint16_t) (keylen - findresult.matchkeylen -1), key + findresult.matchkeylen +1, &uservalue, 0, 0, 0);
         if (err) goto ONABORT;

         uint8_t digit = key[findresult.matchkeylen];

         err = insertchild_trienode(&findresult.node, &findresult.offsets, digit, newchild, findresult.childindex);
         if (err) goto ONABORT;
      }

      // adapt parent
      *findresult.parent_child = findresult.node;
   }

   return 0;
ONABORT:
   if (islog || EEXIST != err) {
      TRACEABORT_ERRLOG(err);
   }
   (void) delete_trienode(&newchild);
   return err;
}

// TODO: implement & test
int remove2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** uservalue, bool islog)
{
   int err;

   trie_findresult_t findresult;

   err = findnode_trie(trie, keylen, key, &findresult);
   if (err) goto ONABORT;

   if (! isuservalue_header(findresult.offsets.header)) {
      err = ESRCH;
      goto ONABORT;
   }

   // out param
   *uservalue = uservalue_trienodeoffsets(&findresult.offsets, findresult.node)[0];

   if (ischildorsubnode_header(findresult.offsets.header)) {
      // TODO: remove user value // + add test //
      assert(0);
   } else {
      // remove node
      err = delete_trienode(findresult.chain_child);
      if (findresult.chain_parent) {
         if (ischild_header(findresult.chain_parent->header)) {
            // TODO: adapt child[] and digit[] array // + add test //
         } else {
            // TODO: decrement child count // convert to child[] array // + add test //
         }
      } else {
         // chain_child points to trie->root
      }
      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   if (islog || ESRCH != err) {
      TRACEABORT_ERRLOG(err);
   }
   return err;
}

#endif // #if 0


// section: trie_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_trie_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_trie_errtimer = test_errortimer_FREE;
#endif

// group: lifetime

int free_trie(trie_t * trie)
{
   int err = 0;
   int err2;

   trie_node_t * parent  = 0;
   trie_node_t * delnode = trie->root;

   while (delnode) {

      // 1: delnode points to node which is encountered the first time
      //    parent points to parent of delnode
      // if (delnode has no childs) goto step 2
      // else  write parent in first child pointer
      //       and descend into first child by repeating step 1
      //

      // 2: delete delnode (no childs)

      // 3: decode parent into delnode
      // if (it has more childs) copy next child into delnode and repeat step 1 (descend into child node)
      // else  read parent pointer from first child and goto step 2

      for (;;) {
         // step 1:
         void * firstchild  = 0;
         if (! issubnode_header(delnode->header)) {
            if (delnode->nrchild) {
               void ** childs = childs_trienode(delnode, childoff5_trienode(delnode));
               firstchild = childs[0];
               childs[0]  = parent;
            }

         } else {
            trie_subnode_t * subnode = subnode_trienode(delnode, childoff5_trienode(delnode));
            for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  delnode->nrchild = (uint8_t)i; // last index into subnode->childs
                  firstchild = subnode->child[i];
                  subnode->child[0] = parent;
                  break;
               }
            }
         }

         if (!firstchild) break;

         // repeat step 1
         parent  = delnode;
         delnode = firstchild;
      }

      for (;;) {
         // step 2:
         err2 = delete_trienode(&delnode);
         if (err2) err = err2;

         // step 3:
         delnode = parent;
         if (!delnode) break; // (delnode == 0) ==> leave top loop: while (delnode) {}

         if (! issubnode_header(delnode->header)) {
            void ** childs = childs_trienode(delnode, childoff5_trienode(delnode));
            while (-- delnode->nrchild) {
               if (childs[delnode->nrchild]) {
                  delnode = childs[delnode->nrchild];
                  break;
               }
            }
            if (delnode != parent) break; // another child ==> repeat step 1
            parent = childs[0];

         } else {
            trie_subnode_t * subnode = subnode_trienode(delnode, childoff5_trienode(delnode));
            for (unsigned i = delnode->nrchild+1u; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  delnode->nrchild = (uint8_t)i; // last index into subnode->childs
                  delnode = subnode->child[i];
                  break;
               }
            }
            if (delnode != parent) break; // another child ==> repeat step 1
            parent = subnode->child[0];

         }

         // repeat step 2
      }

   }

   // set inout param
   trie->root = 0;

   if (err) goto ONABORT;

   return 0;
ONABORT:
   TRACEABORTFREE_ERRLOG(err);
   return err;
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#if 0

typedef struct expect_node_t  expect_node_t;

struct expect_node_t {
   uint8_t  keylen;
   uint8_t  issubnode;
   uint8_t  isuservalue;
   uint8_t  key[255];
   uint16_t nrchild;
   void   * uservalue;
   expect_node_t * child[256];
};

static size_t nodesize_expectnode(uint16_t prefixlen, bool isuservalue, uint16_t nrchild)
{
   size_t size = sizeof(header_t) + (isuservalue ? sizeof(void*) : 0) + (prefixlen > 2) + prefixlen;
   if (nrchild > LENCHILDMAX)
      size += sizeof(trie_subnode_t*) + 1;
   else
      size += nrchild * (sizeof(trie_node_t*)+1);
   return size;
}

static int alloc_expectnode(/*out*/expect_node_t ** expectnode, memblock_t * memblock, uint8_t prefixlen, const uint8_t prefix[prefixlen], bool isuservalue, void * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], expect_node_t * child[nrchild])
{
   TEST(memblock->size >= sizeof(expect_node_t));
   *expectnode = (expect_node_t*) memblock->addr;
   memblock->addr += sizeof(expect_node_t);
   memblock->size += sizeof(expect_node_t);

   (*expectnode)->prefixlen = prefixlen;
   memcpy((*expectnode)->prefix, prefix, prefixlen);
   (*expectnode)->isuservalue = isuservalue;
   (*expectnode)->uservalue   = uservalue;
   (*expectnode)->nrchild     = nrchild;
   memset((*expectnode)->child, 0, sizeof((*expectnode)->child));
   for (unsigned i = 0; i < nrchild; ++i) {
      (*expectnode)->child[digit[i]] = child[i];
   }

   return 0;
ONABORT:
   return ENOMEM;
}

static int new_expectnode(/*out*/expect_node_t ** expectnode, memblock_t * memblock, uint16_t prefixlen, const uint8_t prefix[prefixlen], bool isuservalue, void * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], expect_node_t * child[nrchild])
{
   TEST(nrchild <= 256);
   size_t nodesize = nodesize_expectnode(0, isuservalue, nrchild);
   size_t freesize = SIZEMAXNODE - nodesize;
   TEST(nodesize < SIZEMAXNODE);
   static_assert(SIZEMAXNODE <= 255, "freesize < 255 !");

   freesize -= (freesize > 2);
   if (prefixlen <= freesize) {
      // fit
      TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)prefixlen, prefix, isuservalue, uservalue, nrchild, digit, child));
   } else {
      // does not fit
      TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)freesize, prefix + (size_t)prefixlen - freesize, isuservalue, uservalue, nrchild, digit, child));
      prefixlen = (uint16_t) (prefixlen - freesize);
      do {
         -- prefixlen;
         nodesize = nodesize_expectnode(0, false, 1);
         freesize = SIZEMAXNODE - nodesize;
         freesize -= (freesize > 2);
         if (freesize > prefixlen) freesize = prefixlen;
         TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)freesize, prefix + prefixlen - freesize, false, 0, 1, (uint8_t[]){ prefix[prefixlen] }, (expect_node_t*[]) { *expectnode }));
         prefixlen = (uint16_t) (prefixlen - freesize);
      } while (prefixlen);
   }

   return 0;
ONABORT:
   return EINVAL;
}

/* function: compare_expectnode
 * Compares expect with node. If nodeoffsets != 0 then node and nodeoffsets must match.
 * If cmpnodesize == 0 then the nodesize of node must be the minimum size.
 * If cmpnodesize == 1 then the nodesize of node must be the minimum size or double in size (needed for splitting).
 * If cmpnodesize == 2 then the nodesize of node must be >= the minimum size.
 * The value cmpsubnodesize is inheritedas cmpnodesize for childs.
 * */
static int compare_expectnode(expect_node_t * expect, trie_node_t * node, trie_nodeoffsets_t * nodeoffsets/*could be 0*/, uint8_t cmpnodesize, uint8_t cmpsubnodesize)
{
   if (expect == 0 || node == 0) {
      TEST(0 == expect && 0 == node);
   } else {
      trie_nodeoffsets_t offsets;
      TEST(0 == initdecode_trienodeoffsets(&offsets, node));
      if (nodeoffsets) {
         TEST(0 == compare_trienodeoffsets(&offsets, nodeoffsets));
      }
      size_t expectsize = nodesize_expectnode(expect->prefixlen, expect->isuservalue, expect->nrchild);
      if (expectsize <= SIZE1NODE)      expectsize = SIZE1NODE;
      else if (expectsize <= SIZE2NODE) expectsize = SIZE2NODE;
      else if (expectsize <= SIZE3NODE) expectsize = SIZE3NODE;
      else if (expectsize <= SIZE4NODE) expectsize = SIZE4NODE;
      else                              expectsize = SIZE5NODE;
      switch (cmpnodesize) {
      case 0:  TEST(offsets.nodesize == expectsize); break;
      case 1:  TEST(offsets.nodesize == expectsize || offsets.nodesize == 2*expectsize); break;
      default: TEST(offsets.nodesize >= expectsize); break;
      }
      switch (expect->prefixlen) {
      case 0:
         TEST(header_NOPREFIX   == (offsets.header&header_PREFIX_MASK));
         break;
      case 1:
         TEST(header_PREFIX1    == (offsets.header&header_PREFIX_MASK));
         break;
      case 2:
         TEST(header_PREFIX2    == (offsets.header&header_PREFIX_MASK));
         break;
      default:
         TEST(header_PREFIX_LEN == (offsets.header&header_PREFIX_MASK));
         break;
      }
      TEST(expect->prefixlen == lenprefix_trienodeoffsets(&offsets));
      TEST(expect->prefixlen <= 2 || node->prefixlen == expect->prefixlen);
      TEST(0 == memcmp(expect->prefix, prefix_trienode(node, &offsets), expect->prefixlen));
      TEST(expect->isuservalue == (0 != (offsets.header & header_USERVALUE)));
      if (expect->isuservalue) {
         TEST(expect->uservalue == uservalue_trienodeoffsets(&offsets, node)[0]);
      }
      if (0 == expect->nrchild) {
         // has no childs
         TEST(0 == offsets.lenchild);
         TEST(0 == (offsets.header & header_CHILD));
         TEST(0 == (offsets.header & header_SUBNODE));
      } else {
         // has childs (either header_CHILD or header_SUBNODE set)
         TEST((0 != (offsets.header & header_CHILD)) == (0 == (offsets.header & header_SUBNODE)));
         if (0 != (offsets.header & header_CHILD)) {
            // encoded in child[] array
            TEST(expect->nrchild <= offsets.lenchild);
            for (unsigned i = expect->nrchild; i < offsets.lenchild; ++i) {
               TEST(0 == child_trienode(node, &offsets)[i]);
            }
            for (unsigned i = 0, ei = 0; i < expect->nrchild; ++i, ++ei) {
               for (; ei < 256; ++ei) {
                  if (expect->child[ei]) break;
               }
               TEST(ei < 256);
               TEST(ei == digit_trienode(node, &offsets)[i]);
               TEST(0  == compare_expectnode(expect->child[ei], child_trienode(node, &offsets)[i], 0, cmpsubnodesize, cmpsubnodesize));
            }
         } else {
            // encoded in subnode
            TEST(1 == offsets.lenchild);
            TEST(expect->nrchild == 1+digit_trienode(node, &offsets)[0]);
            trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0];
            for (unsigned i = 0; i < 256; ++i) {
               trie_subnode2_t * subnode2 = child_triesubnode(subnode, (uint8_t)i)[0];
               if (expect->child[i]) {
                  TEST(0 != subnode2);
                  TEST(0 == compare_expectnode(expect->child[i], child_triesubnode2(subnode2, (uint8_t)i)[0], 0, cmpsubnodesize, cmpsubnodesize));
               } else {
                  TEST(0 == subnode2 || 0 == child_triesubnode2(subnode2, (uint8_t)i)[0]);
               }
            }
         }
      }
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int build_subnode_trie(/*out*/trie_node_t ** root, uint8_t depth, uint16_t nrchild)
{
   int err;
   uint8_t              digit[256];
   trie_node_t        * child[256];
   trie_nodeoffsets_t   offsets;

   for (unsigned i = 0; i < 256; ++i) {
      digit[i] = (uint8_t) i;
      child[i] = 0;
   }

   if (0 == depth) {
      for (unsigned i = 0; i < nrchild && i < 256; ++i) {
         void * uservalue = 0;
         err = new_trienode(&child[i], &offsets, 0, 0, &uservalue, 0, 0, 0);
         if (err) goto ONABORT;
      }
   } else {
      for (unsigned i = 0; i < 256; i += 127) {
         err = build_subnode_trie(&child[i], (uint8_t) (depth-1), nrchild);
         if (err) goto ONABORT;
      }
   }

   err = new_trienode(root, &offsets, 0, 0, 0, 256, digit, child);
   if (err) goto ONABORT;

   return 0;
ONABORT:
   return err;
}

static int test_trienode(void)
{
   trie_node_t *  node;
   trie_node_t *  node2;
   size_t         allocsize;
   uint8_t        digit[256];
   trie_node_t *  child[256] = { 0 };
   void *         uservalue;
   trie_nodeoffsets_t offsets;
   memblock_t     key = memblock_FREE;
   memblock_t     expectnode_memblock = memblock_FREE;
   memblock_t     expectnode_memblock2;
   expect_node_t* expectnode;
   expect_node_t* expectnode2;

   // prepare
   TEST(0 == ALLOC_MM(1024*1024, &expectnode_memblock));
   TEST(0 == ALLOC_MM(65536, &key));
   for (unsigned i = 0; i < 65536; ++i) {
      key.addr[i] = (uint8_t) (29*i);
   }
   for (unsigned i = 0; i < 256; ++i) {
      digit[i] = (uint8_t) i;
   }
   allocsize = SIZEALLOCATED_MM();

   // ////
   // group query-helper

   // TEST child_trienode
   memset(&offsets, 0, sizeof(offsets));
   for (unsigned i = 0; i < SIZEMAXNODE/sizeof(trie_node_t*); ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 };
      node = (trie_node_t*)nodebuffer;
      offsets.child = (uint8_t) (i * sizeof(trie_node_t*));
      TEST(&((trie_node_t**)node)[i] == child_trienode(node, &offsets));
   }

   // TEST digit_trienode
   memset(&offsets, 0, sizeof(offsets));
   for (uint16_t i = 0; i < SIZEMAXNODE; ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 };
      node = (trie_node_t*)nodebuffer;
      offsets.digit = (uint8_t) i;
      TEST(nodebuffer[i] == digit_trienode(node, &offsets));
   }

   // TEST isfreechild_trienode
   for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t)(nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
         for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
            uint8_t * nodebuffer[SIZEMAXNODE/sizeof(uint8_t*)] = { 0 };
            node = (trie_node_t*)nodebuffer;
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            memset(child_trienode(node, &offsets), 255, offsets.lenchild*sizeof(trie_node_t*));
            TEST(0 == isfreechild_trienode(node, &offsets));
            if (nrchild && nrchild <= LENCHILDMAX) {
               child_trienode(node, &offsets)[offsets.lenchild-1] = 0;
               TEST((offsets.lenchild > 1) == isfreechild_trienode(node, &offsets));
            }
         }
      }
   }

   // TEST prefix_trienode
   memset(&offsets, 0, sizeof(offsets));
   for (unsigned i = 0; i < SIZEMAXNODE; ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 };
      node = (trie_node_t*)nodebuffer;
      offsets.prefix = (uint8_t) i;
      TEST(nodebuffer[i] == prefix_trienode(node, &offsets));
   }

   // ////
   // group helper

   // TEST newnode_trienode
   for (unsigned i = 0; i < header_SIZENODE_MASK; ++i) {
      header_t header = (header_t) i;
      uint16_t size   = sizenode_header(header);
      TEST(0 == newnode_trienode(&child[i], size));
      TEST(0 != child[i]);
      child[i]->header = header;
      allocsize += size;
      TEST(allocsize == SIZEALLOCATED_MM());
   }

   // TEST deletenode_trienode
   for (unsigned i = 0; i < header_SIZENODE_MASK; ++i) {
      uint16_t size = sizenode_header((header_t)i);
      TEST(0 == deletenode_trienode(&child[i]));
      TEST(0 == child[i]);
      allocsize -= size;
      TEST(allocsize == SIZEALLOCATED_MM());
      TEST(0 == deletenode_trienode(&child[i]));
      TEST(0 == child[i]);
      TEST(allocsize == SIZEALLOCATED_MM());
   }

   // TEST newnode_trienode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == newnode_trienode(&child[0], SIZEMAXNODE));
   TEST(0 == child[0]);
   TEST(allocsize == SIZEALLOCATED_MM());

   // TEST deletenode_trienode: ERROR
   TEST(0 == newnode_trienode(&child[0], SIZE5NODE));
   child[0]->header = header_SIZE5NODE;
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == deletenode_trienode(&child[0]));
   TEST(0 == child[0]);
   TEST(allocsize == SIZEALLOCATED_MM());

   // TEST shrinksize_trienode
   for (uintptr_t i = 0; i < lengthof(child); ++i) {
      child[i] = (trie_node_t*) (i+1);
   }
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 16; ++nrchild) {
         for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            for (unsigned isclearallchild = 0; isclearallchild <= 1; ++isclearallchild) {
               uservalue = (void*)(nrchild * (uintptr_t)100 + 1000u * prefixlen);
               TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
               trie_nodeoffsets_t oldoff = offsets;
               TEST(0 == shrinksize_trienode(node, &offsets));
               TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));  // did nothing
               if (  SIZE1NODE < offsets.nodesize
                     && offsets.child < offsets.nodesize/2) {
                  const unsigned nrchildkept = isclearallchild ? 1 : (offsets.nodesize/2u - offsets.child) / sizeof(trie_node_t*);
                  if (isclearallchild) {  // clear all except first child
                     memset(child_trienode(node, &offsets)+1, 0, (unsigned)offsets.nodesize-offsets.child-sizeof(trie_node_t*));
                  } else {                // clear childs  from offset nodesize/2
                     memset(offsets.nodesize/2 + (uint8_t*)node, 0, offsets.nodesize/2);
                  }
                  TEST(0 == shrinksize_trienode(node, &offsets));
                  TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
                  uint8_t diff = 1;
                  while (offsets.nodesize != (oldoff.nodesize>>diff)) ++ diff;
                  TEST(diff == 1 || isclearallchild);
                  TEST(offsets.nodesize  == (oldoff.nodesize>>diff));
                  TEST(offsets.lenchild  <  oldoff.lenchild);
                  TEST(offsets.lenchild  >= nrchildkept);
                  TEST(offsets.header    == ((oldoff.header&~header_SIZENODE_MASK)|((oldoff.header&header_SIZENODE_MASK)-diff)));
                  TEST(offsets.prefix    == oldoff.prefix);
                  TEST(offsets.digit     == oldoff.digit);
                  size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1);
                  TEST(offsets.uservalue == aligned);
                  TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
                  TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*));
                  if (ischild_header(offsets.header)) {
                     TEST(offsets.nodesize < offsets.child + offsets.lenchild * sizeof(trie_node_t*) + sizeof(trie_node_t*) + 1);
                  }
                  TEST(node->header == offsets.header);
                  TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, prefixlen));
                  if (isuser) {
                     TEST(uservalue == uservalue_trienodeoffsets(&offsets, node)[0]);
                  }
                  if (ischild_header(node->header)) {
                     for (uintptr_t i = 0; i < nrchildkept; ++i) {
                        TEST(digit_trienode(node, &offsets)[i] == i);
                        TEST(child_trienode(node, &offsets)[i] == (trie_node_t*)(i+1));
                     }
                     for (uintptr_t i = nrchildkept; i < offsets.lenchild; ++i) {
                        TEST(child_trienode(node, &offsets)[i] == 0);
                     }
                  }
               }
               memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
               TEST(0 == delete_trienode(node));
            }
         }
      }
   }

   // TEST shrinksize_trienode: ERROR
   TEST(0 == new_trienode(node, &offsets, 15, key.addr, &uservalue, 16, digit, child));
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
   {
      trie_nodeoffsets_t oldoff = offsets;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
      TEST(ENOMEM == shrinksize_trienode(node, &offsets));
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
      TEST(node->header == offsets.header);
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));
   }
   TEST(0 == delete_trienode(node));

   // TEST expand_trienode
   for (uintptr_t i = 0; i < LENCHILDMAX; ++i) {
      child[i] = (trie_node_t*) (i+1);
   }
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= LENCHILDMAX; ++nrchild) {
         for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            if (!isexpandable_trienodeoffsets(&offsets)) break; // already max size
            uservalue = (void*)((uintptr_t)nrchild * 1234 + prefixlen);
            TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
            TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
            trie_nodeoffsets_t oldoff = offsets;
            TEST(0 == expand_trienode(node, &offsets));
            TEST(allocsize + oldoff.nodesize*2u == SIZEALLOCATED_MM());
            // compare offsets
            if (oldoff.lenchild) {
            oldoff.lenchild  = (uint8_t) ((sizefree_trienodeoffsets(&oldoff) + oldoff.lenchild * (1+sizeof(trie_node_t*)) + oldoff.nodesize)
                               / (1+sizeof(trie_node_t*)));
            }
            oldoff.nodesize  = (uint16_t) (oldoff.nodesize * 2);
            oldoff.header    = (header_t) (oldoff.header + header_SIZE2NODE);
            oldoff.uservalue = (uint8_t) (((unsigned)oldoff.digit + oldoff.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1u));
            oldoff.child     = (uint8_t) (oldoff.uservalue + (isuser ? sizeof(void*) : 0));
            TEST(0 == compare_trienodeoffsets(&oldoff, &offsets));
            // compare node content
            TEST(node->header == offsets.header);
            TEST(prefixlen <= 2 || node->prefixlen == prefixlen);
            TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, prefixlen));
            TEST(0 == memcmp(digit_trienode(node, &offsets), digit, nrchild));
            TEST(!isuser || uservalue == uservalue_trienodeoffsets(&offsets, node)[0]);
            TEST(0 == memcmp(child_trienode(node, &offsets), child, nrchild));
            for (unsigned i = nrchild; i < offsets.lenchild; ++i) {
               TEST(0 == child_trienode(node, &offsets)[i]);
            }
            memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
            TEST(0 == deletenode_trienode(node));
            TEST(allocsize == SIZEALLOCATED_MM());
         }
      }
   }

   // TEST expand_trienode: ERROR
   TEST(0 == new_trienode(node, &offsets, 15, key.addr, &uservalue, 16, digit, child));
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
   {
      trie_nodeoffsets_t oldoff = offsets;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
      TEST(ENOMEM == expand_trienode(node, &offsets));
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
      TEST(node->header == offsets.header);
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));
   }
   TEST(0 == delete_trienode(node));
   memset(child, 0, sizeof(child));

   // TEST tryextendprefix_trienode
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t)(nrchild == 20 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            uservalue = (void*) ((uintptr_t)100 * nrchild + 11u * prefixlen);
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 0, 0, 0, 0, 0, 0));
            }
            TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
            for (unsigned i = nrchild; i < offsets.lenchild; ++i) {
               trie_nodeoffsets_t offsets2;
               TEST(0 == new_trienode(&child[i], &offsets2, 0, 0, 0, 0, 0, 0));
               digit_trienode(node, &offsets)[i] = digit[i];
               child_trienode(node, &offsets)[i] = child[i];
            }
            uint8_t sizegrow = sizegrowprefix_trienodeoffsets(&offsets);
            TEST(ENOMEM == tryextendprefix_trienode(node, &offsets, (uint8_t)(sizegrow+1), key.addr+16, key.addr[sizegrow+16]));
            trie_nodeoffsets_t offsets2 = offsets;
            trie_subnode_t *   subnode  = nrchild > LENCHILDMAX ? subnode_trienodeoffsets(&offsets, node)[0] : 0;
            for (uint8_t i = 1; i <= sizegrow+sizeof(trie_node_t*); ++i) {
               bool uselastchild = (i > sizegrow);
               if (uselastchild && offsets.lenchild < 2) break;
               if (uselastchild) {
                  // last child is free
                  TEST(0 == delete_trienode(child_trienode(node, &offsets)+offsets.lenchild-1));
               } else {
                  // all childs in use
               }
               TEST(0 == tryextendprefix_trienode(node, &offsets, i, key.addr+16, key.addr[i+15]));
               // offsets
               TEST(offsets.nodesize  == offsets2.nodesize);
               TEST(offsets.lenchild  == offsets2.lenchild-(i > sizegrow));
               TEST(offsets.prefix    == HEADERSIZE + (prefixlen+i > 2));
               TEST(offsets.digit     == offsets.prefix + (prefixlen+i));
               TEST(offsets.uservalue == ((PTRALIGN-1u + offsets.digit + offsets.lenchild) & ~(PTRALIGN-1u)));
               TEST(offsets.child     == offsets.uservalue + (isuser ? sizeof(void*) : 0));
               unsigned nodesize = offsets.child + offsets.lenchild * sizeof(trie_node_t*);
               TEST(offsets.nodesize  >= nodesize && (i != sizegrow || offsets.nodesize == nodesize));
               // node
               TEST(node->header == offsets.header);
               TEST((prefixlen+i <= 2) || (prefixlen+i == node->prefixlen));
               TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr+16, i));
               TEST(0 == memcmp(prefix_trienode(node, &offsets)+i, key.addr, prefixlen));
               if (subnode) {
                  TEST(nrchild == 1+digit_trienode(node, &offsets)[0]);
                  TEST(subnode == subnode_trienodeoffsets(&offsets, node)[0]);
               } else {
                  TEST(0 == memcmp(digit_trienode(node, &offsets), digit, offsets.lenchild));
                  TEST(0 == memcmp(child_trienode(node, &offsets), child, sizeof(trie_node_t*)*offsets.lenchild));
               }
               TEST(!isuser || uservalue == uservalue_trienodeoffsets(&offsets, node)[0]);
               shrinkprefixkeeptail_trienode(node, &offsets, prefixlen);
               TEST(0 == compare_trienodeoffsets(&offsets, &offsets2));
            }
            memset(child, 0, sizeof(child));
            TEST(0 == delete_trienode(node));
         }
      }
   }

   // TEST adduservalue_trienode
   for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t)(nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      for (uint8_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
         init_trienodeoffsets(&offsets, prefixlen, 0, nrchild);
         if (lenprefix_trienodeoffsets(&offsets) != prefixlen) break; // does not fit in node
         uservalue = (void*) ((uintptr_t)100 * nrchild + 11u * prefixlen);
         for (unsigned i = 0; i < nrchild; ++i) {
            TEST(0 == new_trienode(&child[i], &offsets, 0, 0, 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, 0, nrchild, digit, child));
         trie_subnode_t   * subnode = nrchild > LENCHILDMAX ? subnode_trienodeoffsets(&offsets, node)[0] : 0;
         trie_nodeoffsets_t oldoff  = offsets;
         bool issizefree = sizefree_trienodeoffsets(&offsets) >= sizeof(void*);
         TEST(subnode || nrchild <= offsets.lenchild);
         if (issizefree || isfreechild_trienode(node, &offsets)) {
            adduservalue_trienode(node, &offsets, uservalue);
            // compare offsets
            oldoff.lenchild  = (uint8_t)  (oldoff.lenchild - (! issizefree));
            oldoff.header    = (header_t) (oldoff.header | header_USERVALUE);
            oldoff.uservalue = (uint8_t)  (((unsigned)oldoff.digit + offsets.lenchild + PTRALIGN-1) & ~(PTRALIGN-1));
            oldoff.child     = (uint8_t)  (oldoff.uservalue + sizeof(void*));
            TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));
            // compare node
            TEST(node->header == offsets.header);
            TEST(prefixlen <= 2 || prefixlen == node->prefixlen);
            TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, lenprefix_trienodeoffsets(&offsets)));
            if (subnode) {          // compare digit[0] and subnode
               TEST(nrchild == (uint16_t)1 + digit_trienode(node, &offsets)[0]);
               TEST(subnode == subnode_trienodeoffsets(&offsets, node)[0]);
            } else if (nrchild) {   // compare digit[] and child[]
               TEST(1 <= offsets.lenchild && nrchild-1 <= offsets.lenchild);
               TEST(0 == memcmp(digit_trienode(node, &offsets), digit, nrchild));
               for (unsigned i = 0; i < nrchild; ++i) {
                  TEST(child[i] == child_trienode(node, &offsets)[i]);
               }
               for (unsigned i = nrchild; i < offsets.lenchild; ++i) {
                  TEST(0 == child_trienode(node, &offsets)[i]);
               }
            }
         }
         TEST(0 == delete_trienode(node));
      }
   }

   // ////
   // group lifetime

   // TEST new_trienode, delete_trienode: single node with childs
   for (uint8_t nrchild = 0; nrchild <= 2; ++nrchild) {
      for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
         for (uint8_t prefixlen = 0; prefixlen <= 16; ++prefixlen) {
            // new_trienode
            uservalue = (void*) ((uintptr_t)prefixlen + 200);
            expectnode_memblock2 = expectnode_memblock;
            expect_node_t * expectchilds[2] = { 0 };
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 33, key.addr+10, 0, 0, 0, 0));
               TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 33, key.addr+10, 0, 0, 0, 0, 0));
            }
            TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit+11, child));
            // compare result
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, nrchild, digit+11, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
            // delete_trienode
            TEST(0 == delete_trienode(node));
            TEST(0 == node);
            TEST(0 == delete_trienode(node));
            TEST(0 == node);
         }
      }
   }

   // TEST new_trienode, delete_trienode: prefix chain
   for (uint8_t nrchild = 0; nrchild <= 2; ++nrchild) {
      for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
         for (uint32_t prefixlen = 0; prefixlen < 65536; ++prefixlen, prefixlen = (prefixlen < 1024 || prefixlen >= 65530 ? prefixlen : 65530)) {
            uservalue = 0;
            expectnode_memblock2 = expectnode_memblock;
            expect_node_t * expectchilds[2] = { 0 };
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 34, key.addr+3, 0, 0, 0, 0));
               TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 34, key.addr+3, 0, 0, 0, 0, 0));
            }
            // new_trienode
            TEST(0 == new_trienode(node, &offsets, (uint16_t)prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, (uint8_t[]){13,15}, child));
            // compare content of chain
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, (uint16_t)prefixlen, key.addr, isuser, uservalue, nrchild, (uint8_t[]){13,15}, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
            // delete_trienode
            TEST(0 == delete_trienode(node));
            TEST(0 == node);
            TEST(0 == delete_trienode(node));
            TEST(0 == node);
         }
      }
   }

   // TEST new_trienode, delete_trienode: trie_subnode_t
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
         expect_node_t * expectchilds[lengthof(child)] = { 0 };
         uservalue            = (void*) (isuser + 10u + prefixlen);
         expectnode_memblock2 = expectnode_memblock;
         for (unsigned i = 0; i < lengthof(child); ++i) {
            TEST(0 == new_trienode(&child[i], &offsets, 3, key.addr, 0, 0, 0, 0));
            TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 3, key.addr, 0, 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, lengthof(child), digit, child));
         // compare result
         TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, lengthof(child), digit, expectchilds));
         TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
         TEST(0 == delete_trienode(node));
         TEST(0 == node);
      }
   }

   // TEST delete_trienode: trie_subnode_t of trie_subnode_t
   TEST(0 == build_subnode_trie(node, 2, 32));
   TEST(0 != node);
   initdecode_trienodeoffsets(&offsets, node);
   TEST(1 == issubnode_header(node->header));
   TEST(255 == digit_trienode(node, &offsets)[0]);
   TEST(0 == delete_trienode(node));   // test delete with tree of subnode_t
   TEST(0 == node);

   // TEST delete_trienode: subnode == 0
   TEST(0 == build_subnode_trie(node, 0, 256));
   initdecode_trienodeoffsets(&offsets, node);
   TEST(1 == issubnode_header(node->header));
   TEST(255 == digit_trienode(node, &offsets)[0]);
   {  // set subnode == 0
      trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0];
      subnode_trienodeoffsets(&offsets, node)[0] = 0;
      for (unsigned i = 0; i < 256; ++i) {
         TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i)));
      }
      delete_triesubnode(&subnode);
   }
   TEST(0 == delete_trienode(node));
   TEST(0 == node);

   // TEST delete_trienode: subnode with all childs == 0
   for (int isdelsub2 = 0; isdelsub2 <= 1; ++isdelsub2) {
      TEST(0 == build_subnode_trie(node, 0, 256));
      initdecode_trienodeoffsets(&offsets, node);
      TEST(1 == issubnode_header(node->header));
      TEST(255 == digit_trienode(node, &offsets)[0]);
      trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0];
      for (unsigned i = 0; i < 256; ++i) {
         TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i)));
      }
      for (unsigned i = 0; isdelsub2 && i < lengthof(subnode->child); ++i) {
         TEST(0 == delete_triesubnode2(&subnode->child[i]));
      }
      TEST(0 == delete_trienode(node));
      TEST(0 == node);
   }

   // TEST delete_trienode: subnode with only single child
   for (int isdelsub2 = 0; isdelsub2 <= 1; ++isdelsub2) {
      for (unsigned ci = 0; ci < 256; ++ci) {
         TEST(0 == build_subnode_trie(node, 0, 256));
         initdecode_trienodeoffsets(&offsets, node);
         TEST(1 == issubnode_header(node->header));
         TEST(255 == digit_trienode(node, &offsets)[0]);
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0];
         for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
            trie_subnode2_t * subnode2 = subnode->child[i];
            bool              isdel    = true;
            for (unsigned i2 = 0; i2 < lengthof(subnode2->child); ++i2) {
               if (ci != i*lengthof(subnode2->child) + i2) {
                  TEST(0 == delete_trienode(&subnode2->child[i2]));
               } else {
                  isdel = false;
               }
            }
            if (isdelsub2 && isdel) {
               TEST(0 == delete_triesubnode2(&subnode->child[i]));
            }
         }
         TEST(0 == delete_trienode(node));
         TEST(0 == node);
      }
   }

   // TEST newsplit_trienode: no merge with following node
   for (uint8_t splitparam = 0; splitparam <= 1; ++splitparam) {
      // splitparam == 0: uservalue / splitparam == 1: child param
      for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
         for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
            for (uint8_t prefixlen = 1; prefixlen < 16; ++prefixlen) {
               for (uint8_t splitprefixlen = 0; splitprefixlen < prefixlen; ++splitprefixlen) {
                  const uint8_t newprefixlen = (uint8_t) (prefixlen-1-splitprefixlen);
                  expectnode_memblock2 = expectnode_memblock;
                  expect_node_t * expectchilds[256] = { 0 };
                  for (unsigned i = 0; i < nrchild; ++i) {
                     // make sure that merge with following node is not possible
                     TEST(0 == new_trienode(&child[i], &offsets, 6, key.addr, 0, 0, 0, 0));
                     TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 6, key.addr, 0, 0, 0, 0, 0));
                  }
                  uservalue = (void*)(uintptr_t)(1000+nrchild);
                  TEST(0 == new_trienode(node2, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
                  TEST(0 == new_expectnode(&expectnode2, &expectnode_memblock2, newprefixlen, key.addr+prefixlen-newprefixlen, isuser, uservalue, nrchild, digit, expectchilds));
                  // test newsplit_trienode
                  child[0]  = 0;
                  uservalue = (void*)(uintptr_t)(2000+nrchild);
                  uint8_t splitdigit = (uint8_t) (key.addr[splitprefixlen] + (splitprefixlen%2 ? -1 : +1));
                  if (splitparam) {
                     trie_nodeoffsets_t offsets2;
                     TEST(0 == new_trienode(&child[0], &offsets2, 3, key.addr+60, 0, 0, 0, 0));
                     TEST(0 == new_expectnode(&expectchilds[1], &expectnode_memblock2, 3, key.addr+60, 0, 0, 0, 0, 0));
                  }
                  TEST(0 == newsplit_trienode(node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0]));
                  // compare result
                  uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit };
                  expectchilds[0] = expectnode2;
                  TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds));
                  TEST(0 == compare_expectnode(expectnode, node, 0, 0, 1));
                  TEST(0 == delete_trienode(node));
               }
            }
         }
      }
   }

   // TEST newsplit_trienode: merge with following node
   for (uint8_t splitparam = 0; splitparam <= 1; ++splitparam) {
      // splitparam == 0: uservalue / splitparam == 1: child param
      for (uint8_t splitprefixlen = 0; splitprefixlen < 16; ++splitprefixlen) {
         for (uint8_t prefixlen = (uint8_t)(splitprefixlen+1); prefixlen <= (splitprefixlen+sizeof(trie_node_t*)); ++prefixlen) {
            const uint8_t mergelen = (uint8_t) (prefixlen-splitprefixlen);
            expectnode_memblock2 = expectnode_memblock;
            expect_node_t * expectchilds[2] = { 0 };
            // make sure that merge with following node is possible
            uservalue = (void*)(uintptr_t)(splitprefixlen*100+prefixlen);
            TEST(0 == new_trienode(&child[0], &offsets, 0, 0, &uservalue, 0, 0, 0));
            TEST(0 == new_trienode(&child[1], &offsets, 0, 0, &uservalue, 0, 0, 0));
            TEST(0 == new_trienode(&child[2], &offsets, 3, key.addr+prefixlen+1, 0, 2, digit, child));
            child[3] = 0;
            TEST(0 == delete_trienode(child_trienode(child[2], &offsets)+1)); // delete child[1]
            TEST(0 == new_expectnode(&expectchilds[0], &expectnode_memblock2, 0, 0, 1, uservalue, 0, 0, 0));
            TEST(0 == new_expectnode(&expectnode2, &expectnode_memblock2, (uint16_t)(3+mergelen), key.addr+prefixlen+1-mergelen, 0, 0, 1, digit, expectchilds));
            // child[3] is empty ==> insert of child is possible ==> merge is possible
            TEST(0 == new_trienode(node2, &offsets, prefixlen, key.addr, 0, 2, key.addr+prefixlen, child+2));
            // test newsplit_trienode
            child[0]  = 0;
            uservalue = (void*)(uintptr_t)(splitprefixlen*120+prefixlen);
            uint8_t splitdigit = (uint8_t) (key.addr[splitprefixlen] + (prefixlen%2 ? -1 : +1));
            if (splitparam) {
               trie_nodeoffsets_t offsets2;
               TEST(0 == new_trienode(&child[0], &offsets2, 3, key.addr+60, 0, 0, 0, 0));
               TEST(0 == new_expectnode(&expectchilds[1], &expectnode_memblock2, 3, key.addr+60, 0, 0, 0, 0, 0));
            }
            TEST(0 == newsplit_trienode(node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0]));
            // compare result
            TEST(node == node2); // merged
            uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit };
            expectchilds[0] = expectnode2;
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 1, 0));
            TEST(0 == delete_trienode(node));
         }
      }
   }

   // TEST new_trienode, delete_trienode: ERROR
   memset(child, 0, sizeof(child));
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_trienode(node, &offsets, 20000, key.addr, 0, 256, digit, child));
   TEST(0 == node);
   TEST(0 == new_trienode(node, &offsets, 20000, key.addr, 0, 256, digit, child));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_trienode(node));
   TEST(0 == node);

   // store log
   char * logbuffer;
   {
      uint8_t *logbuffer2;
      size_t   logsize;
      GETBUFFER_ERRLOG(&logbuffer2, &logsize);
      logbuffer = strdup((char*)logbuffer2);
      TEST(logbuffer);
   }

   // TEST new_trienode: ERROR (no log cause of overflow)
   CLEARBUFFER_ERRLOG();
   for (uint32_t errcount = 1; errcount < 50; ++errcount) {
      init_testerrortimer(&s_trie_errtimer, errcount, ENOMEM);
      TEST(ENOMEM == new_trienode(node, &offsets, 20000, key.addr, 0, 256, digit, child));
      TEST(0 == node);
   }

   // TEST delete_trienode: ERROR (no log cause of overflow)
   CLEARBUFFER_ERRLOG();
   for (int issubnode = 0; issubnode < 2; ++issubnode) {
      for (uint32_t errcount = 1; errcount < 3; ++errcount) {
         if (issubnode) {
            TEST(0 == build_subnode_trie(node, 0, 1));
         } else {
            TEST(0 == new_trienode(node, &offsets, 2000, key.addr, 0, 0, 0, 0));
         }
         TEST(0 != node);
         init_testerrortimer(&s_trie_errtimer, errcount, EINVAL);
         TEST(EINVAL == delete_trienode(node));
         TEST(0 == node);
      }
   }

   // restore log
   CLEARBUFFER_ERRLOG();
   PRINTF_ERRLOG("%s", logbuffer);
   free(logbuffer);

   // ////
   // group change

   // TEST convertchild2sub_trienode
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t prefixlen = 0; prefixlen < 8; ++prefixlen) {
         for (uint8_t nrchild = 1; nrchild < LENCHILDMAX; ++nrchild) {
            // test memory
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            if (lenprefix_trienodeoffsets(&offsets) != prefixlen) break;  // prefix does not fit
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(0 == new_trienode(node, &offsets, 3, (const uint8_t*)"123", 0, 0, 0, 0));
               digit[i] = (uint8_t) (prefixlen * 14u + i);
               child[i] = node;
            }
            uservalue = (void*)(uintptr_t)(10+prefixlen);
            TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
            trie_nodeoffsets_t oldoff = offsets;
            allocsize = SIZEALLOCATED_MM();
            TEST(0 == convertchild2sub_trienode(node, &offsets));
            // test offsets
            TEST(offsets.nodesize  <= oldoff.nodesize);
            TEST(offsets.lenchild  == 0);
            TEST(offsets.header    == ((oldoff.header & ~(header_CHILD|header_SIZENODE_MASK)) | (header_SUBNODE|(offsets.header&header_SIZENODE_MASK))));
            TEST(offsets.prefix    == oldoff.prefix);
            TEST(offsets.digit     == oldoff.digit);
            size_t aligned = (offsets.digit + 1u + PTRALIGN-1) & ~(PTRALIGN-1);
            TEST(offsets.uservalue == aligned);
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
            size_t   newsize = offsets.child + sizeof(trie_subnode_t*);
            header_t headsize;
            if      (newsize <= SIZE1NODE) newsize = SIZE1NODE, headsize = header_SIZE0NODE;
            else if (newsize <= SIZE2NODE) newsize = SIZE2NODE, headsize = header_SIZE2NODE;
            else if (newsize <= SIZE3NODE) newsize = SIZE3NODE, headsize = header_SIZE3NODE;
            else if (newsize <= SIZE4NODE) newsize = SIZE4NODE, headsize = header_SIZE4NODE;
            else                           newsize = SIZE5NODE, headsize = header_SIZE5NODE;
            TEST(headsize == (offsets.header&header_SIZENODE_MASK));
            // test node
            TEST(offsets.header == node->header);
            TEST(nrchild        == 1u + digit_trienode(node, &offsets)[0]);
            if (isuser) {
               TEST(uservalue == uservalue_trienodeoffsets(&offsets, node)[0]);
            }
            // test content of subnode
            trie_subnode2_t * subnode2;
            trie_subnode_t *  subnode = subnode_trienodeoffsets(&offsets, node)[0];
            bool              issubnode[lengthof(subnode->child)] = { 0 };
            for (unsigned i = 0; i < nrchild; ++i) {
               issubnode[digit[i]/lengthof(subnode2->child)] = 1;
            }
            unsigned nrsubnode = 0;
            for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
               TEST(issubnode[i] == (0 != subnode->child[i]));
               nrsubnode += issubnode[i];
            }
            for (unsigned i = 0; i < nrchild; ++i) {
               uint8_t d = digit[i];
               subnode2  = subnode->child[d/lengthof(subnode2->child)];
               TEST(child[i] == child_triesubnode2(subnode2, d)[0]);
               digit[i] = (uint8_t) i;   // reset
               child[i] = 0;             // reset
            }
            TEST(SIZEALLOCATED_MM() == allocsize - oldoff.nodesize + newsize + sizeof(trie_subnode_t) + nrsubnode * sizeof(trie_subnode2_t));
            TEST(0 == delete_trienode(node));
         }
      }
   }

   // TEST convertchild2sub_trienode: EINVAL
   TEST(0 == new_trienode(node, &offsets, 1, key.addr, &uservalue, 0, 0, 0));
   TEST(0 == ischild_header(offsets.header));
   TEST(EINVAL == convertchild2sub_trienode(node, &offsets));  // no childs
   TEST(0 == delete_trienode(node));
   TEST(0 == new_trienode(node, &offsets, 1, key.addr, &uservalue, 2, digit, child));
   TEST(1 == ischild_header(offsets.header));
   TEST(EINVAL == convertchild2sub_trienode(node, &offsets));  // all child pointer set to 0
   TEST(0 == delete_trienode(node));

   // TEST convertchild2sub_trienode: ENOMEM (subnode creation fails)
   allocsize = SIZEALLOCATED_MM();
   for (unsigned i = 0; i < 16; ++i) {
      TEST(0 == new_trienode(&child[i], &offsets, 0, 0, &uservalue, 0, 0, 0));
   }
   TEST(0 == new_trienode(node, &offsets, 4, key.addr, &uservalue, 16, digit, child));
   // save old state
   size_t nodesize = SIZEALLOCATED_MM() - allocsize;
   memcpy(key.addr, &offsets, sizeof(offsets));
   memcpy(key.addr + sizeof(offsets), node, offsets.nodesize);
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == convertchild2sub_trienode(node, &offsets));
   // test nothing changed
   TEST(allocsize+nodesize == SIZEALLOCATED_MM());
   TEST(0 == memcmp(key.addr, &offsets, sizeof(offsets)));
   TEST(0 == memcmp(key.addr + sizeof(offsets), node, offsets.nodesize));

   // TEST convertchild2sub_trienode: ENOMEM ignored (shrinksize_trienode fails)
   init_testerrortimer(&s_trie_errtimer, 4, ENOMEM);
   TEST(0 == convertchild2sub_trienode(node, &offsets));
   nodesize += sizeof(trie_subnode_t) + 2 * sizeof(trie_subnode2_t);
   TEST(1 == issubnode_header(offsets.header));      // offsets changed
   TEST(node->header == offsets.header);            // also node
   TEST(allocsize+nodesize == SIZEALLOCATED_MM());   // but shrinksize_trienode failed
   TEST(0 == delete_trienode(node));
   TEST(allocsize == SIZEALLOCATED_MM());

   // TEST shrinkprefixkeeptail_trienode: normal (precondition not violated)
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1; prefixlen < 16; ++prefixlen) {
            for (uint8_t newprefixlen = 0; newprefixlen < prefixlen; ++newprefixlen) {
               expectnode_memblock2 = expectnode_memblock;
               expect_node_t * expectchilds[256] = { 0 };
               for (unsigned i = 0; i < 16 && i < nrchild; ++i) {
                  TEST(0 == new_trienode(&child[i], &offsets, 5, key.addr+3, 0, 0, 0, 0));
                  TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 5, key.addr+3, 0, 0, 0, 0, 0));
               }
               uservalue = (void*)(uintptr_t)(1000+nrchild);
               TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               // normal case
               shrinkprefixkeeptail_trienode(node, &offsets, newprefixlen);
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr+prefixlen-newprefixlen, isuser, uservalue, nrchild, digit, expectchilds));
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0));
               TEST(0 == delete_trienode(node));
               memset(child, 0, sizeof(child));
            }
         }
      }
   }

   // TEST shrinkprefixkeephead_trienode
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1; prefixlen < 16; ++prefixlen) {
            for (uint8_t newprefixlen = 0; newprefixlen < prefixlen; ++newprefixlen) {
               expectnode_memblock2 = expectnode_memblock;
               expect_node_t * expectchilds[256] = { 0 };
               for (unsigned i = 0; i < 16 && i < nrchild; ++i) {
                  TEST(0 == new_trienode(&child[i], &offsets, 5, key.addr+3, 0, 0, 0, 0));
                  TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 5, key.addr+3, 0, 0, 0, 0, 0));
               }
               uservalue = (void*)((uintptr_t)123+prefixlen);
               TEST(0 == new_trienode(node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               // normal case
               shrinkprefixkeephead_trienode(node, &offsets, newprefixlen);
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr, isuser, uservalue, nrchild, digit, expectchilds));
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0));
               TEST(0 == delete_trienode(node));
               memset(child, 0, sizeof(child));
            }
         }
      }
   }

   // TEST insertchild_trienode: add to child array
   // TODO: TEST insertchild_trienode

   // TEST insertchild_trienode: convert child array into subnode
   // TODO: TEST insertchild_trienode

   // TEST insertchild_trienode: add to subnode
   // TODO: TEST insertchild_trienode

   // TEST insertchild_trienode: add child array
   // TODO: TEST insertchild_trienode

   // TEST insertchild_trienode: split node && add child array
   // TODO: TEST insertchild_trienode

   // unprepare
   TEST(0 == FREE_MM(&expectnode_memblock));
   TEST(0 == FREE_MM(&key));

   return 0;
ONABORT:
   FREE_MM(&expectnode_memblock);
   FREE_MM(&key);
   return EINVAL;
}

static int test_initfree(void)
{
   trie_t               trie = trie_FREE;
   trie_nodeoffsets_t   offsets;
   uint8_t              digit[256];

   // prepare
   for (unsigned i = 0; i < lengthof(digit); ++i) {
      digit[i] = (uint8_t) i;
   }

   // TEST trie_FREE
   TEST(0 == trie.root);

   // TEST trie_INIT
   trie = (trie_t) trie_INIT;
   TEST(0 == trie.root);

   // TEST init_trie
   memset(&trie, 255 ,sizeof(trie));
   TEST(0 == init_trie(&trie));
   TEST(0 == trie.root);

   // TEST free_trie
   trie_node_t *  topchilds[16]   = { 0 };
   trie_node_t *  leafchilds[256] = { 0 };
   for (unsigned ti = 0; ti < lengthof(topchilds); ++ti) {
      void * uservalue = (void*) ti;
      for (unsigned li = 0; li < lengthof(leafchilds); ++li) {
         TEST(0 == new_trienode(&leafchilds[li], &offsets, 3, (const uint8_t*)"123", &uservalue, 0, 0, 0));
      }
      TEST(0 == new_trienode(&topchilds[ti], &offsets, 0, 0, 0, 256, digit, leafchilds));
      for (unsigned ci = 0; ci < 10; ++ci) {
         trie_node_t * childs[1] = { topchilds[ti] };
         TEST(0 == new_trienode(&topchilds[ti], &offsets, 5, (const uint8_t*)"12345", &uservalue, 1, digit, childs));
      }
   }
   TEST(0 == new_trienode(&trie.root, &offsets, 4, (const uint8_t*)"root", 0, lengthof(topchilds), digit, topchilds));
   TEST(0 == free_trie(&trie));
   TEST(0 == trie.root);
   TEST(0 == free_trie(&trie));
   TEST(0 == trie.root);

   return 0;
ONABORT:
   return EINVAL;
}

static int test_query(void)
{
   trie_t             trie = trie_INIT;
   memblock_t         key  = memblock_FREE;
   void             * uservalue;
   trie_findresult_t  findresult;
   trie_findresult_t  findresult2;
   trie_node_t      * childs[256] = { 0 };
   trie_nodeoffsets_t offsets;

   // prepare
   memset(&offsets, 0, sizeof(offsets));
   memset(&findresult, 0, sizeof(findresult));
   memset(&findresult2, 0, sizeof(findresult2));
   TEST(0 == ALLOC_MM(1024, &key));
   for (unsigned i = 0; i < key.size; ++i) {
         key.addr[i] = (uint8_t) i;
   }

   // TEST findnode_trie, at_trie: empty trie
   for (uint16_t keylen = 0; keylen <= 16; ++keylen) {
      TEST(ESRCH == findnode_trie(&trie, keylen, key.addr, &findresult));
      TEST(findresult.parent       == 0)
      TEST(findresult.parent_child == &trie.root);
      TEST(findresult.node         == trie.root);
      TEST(findresult.chain_parent == 0);
      TEST(findresult.chain_child  == &trie.root);
      TEST(findresult.matchkeylen  == 0);
      TEST(findresult.is_split     == 0);
      TEST(0 == at_trie(&trie, (uint16_t)key.size, key.addr));
   }

   // TEST findnode_trie, at_trie: single node / node chain (root is chain_parent)
   for (int isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t keylen = 0; keylen <= key.size; keylen = (uint16_t) (keylen <= 16 ? keylen+1 : 2*keylen)) {
         uservalue = (void*) (11*keylen + 13*isuser);
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, 0, 0, 0));
         findresult.parent = (void*)2; findresult.parent_child = 0; findresult.node = 0;
         TEST(0 == findnode_trie(&trie, keylen, key.addr, &findresult));
         TEST(findresult.parent       != (void*)2)
         TEST(findresult.parent_child != 0);
         TEST(findresult.node         != 0);
         TEST(isuser == isuservalue_header(findresult.node->header))
         findresult2 = (trie_findresult_t) {
            .parent = findresult.parent, .parent_child = findresult.parent_child, .node = findresult.parent_child[0],
            .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = keylen
         };
         initdecode_trienodeoffsets(&findresult2.offsets, findresult.node);
         TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
         if (isuser) {
            TEST(at_trie(&trie, keylen, key.addr)[0] == uservalue);
         } else {
            TEST(at_trie(&trie, keylen, key.addr)    == 0);
         }
         if (keylen < key.size) {
            TEST(0 == at_trie(&trie, (uint16_t) (keylen+1), key.addr));
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) (keylen+1), key.addr, &findresult));
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: node with childs followed (begin of chain_parent)
   for (int isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t keylen = 0; keylen <= 3; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser);
         for (unsigned i = 0; i < LENCHILDMAX-1; ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, LENCHILDMAX-1, key.addr, childs));
         for (unsigned i = 0; i < LENCHILDMAX-1; ++i) {
            uint8_t            skey[2*keylen+1];
            memcpy(skey, key.addr, sizeof(skey));
            skey[keylen] = key.addr[i];
            // test find
            TEST(0 == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult));
            findresult2 = (trie_findresult_t) {
               .parent = trie.root, .parent_child = child_trienode(trie.root, &offsets)+i,
               .node = childs[i], .chain_parent = trie.root, .chain_child = child_trienode(trie.root, &offsets)+i,
               .matchkeylen = (uint16_t) sizeof(skey), .is_split = 0
            };
            initdecode_trienodeoffsets(&findresult2.offsets, findresult.node);
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
            if (isuser) {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == uservalue_trienodeoffsets(&findresult.offsets, findresult.node));
            } else {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == 0);
            }
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: split node
   for (int isuser = 0; isuser <= 1; ++isuser) {
      findresult2 = (trie_findresult_t) {
         .parent = 0, .parent_child = &trie.root, .node = trie.root,
         .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = 0, .is_split = 1
      };
      for (uint16_t keylen = 1; keylen <= 16; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser);
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, 0, 0, 0));
         findresult2.node = trie.root;
         initdecode_trienodeoffsets(&findresult2.offsets, trie.root);
         for (uint8_t splitlen = 0; splitlen < keylen; ++splitlen) {
            // keysize < prefixlen in node
            TEST(0 == at_trie(&trie, splitlen, key.addr));
            TEST(ESRCH == findnode_trie(&trie, splitlen, key.addr, &findresult));
            findresult2.splitlen = splitlen;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));

            // keysize >= prefixlen but key does not match
            uint8_t oldkey = key.addr[splitlen];
            key.addr[splitlen] = (uint8_t) (oldkey + 1);
            TEST(0 == at_trie(&trie, keylen, key.addr));
            TEST(ESRCH == findnode_trie(&trie, keylen, key.addr, &findresult));
            findresult2.splitlen = splitlen;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
            key.addr[splitlen] = oldkey;
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: node with childs not followed
   memset(&findresult, 0, sizeof(findresult));
   for (int isuser = 0; isuser <= 1; ++isuser) {
      uint8_t digits[LENCHILDMAX-1];
      for (unsigned i = 0; i < sizeof(digits); ++i) {
         digits[i] = (uint8_t) (5+5*i);
      }
      for (uint16_t keylen = 0; keylen <= 3; ++ keylen) {
         uint8_t skey[keylen+1];
         memcpy(skey, key.addr, sizeof(skey));
         uservalue = (void*) (7*keylen + 23*isuser);
         for (unsigned i = 0; i < sizeof(digits); ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, 1, key.addr+10, 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, sizeof(digits), digits, childs));
         findresult2 = (trie_findresult_t) {
               .parent = 0, .parent_child = &trie.root,
               .node = trie.root, .chain_parent = 0, .chain_child = &trie.root,
               .matchkeylen = keylen, .is_split = 0
         };
         initdecode_trienodeoffsets(&findresult2.offsets, trie.root);
         for (unsigned i = 0; i < LENCHILDMAX-1; ++i) {
            skey[keylen] = (uint8_t) (digits[i] + 1);
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult));
            findresult2.childindex = (uint8_t) (i+1);
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
            skey[keylen] = (uint8_t) (digits[i] - 2);
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult));
            findresult2.childindex = (uint8_t) i;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: node with subnode followed (begin of chain_parent)
   memset(&findresult, 0, sizeof(findresult));
   for (int isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t keylen = 0; keylen <= 3; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser);
         for (unsigned i = 0; i < lengthof(childs); ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, lengthof(childs), key.addr, childs));
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, trie.root)[0];
         for (unsigned i = 0; i < lengthof(childs); ++i) {
            uint8_t            skey[2*keylen+1];
            memcpy(skey, key.addr, sizeof(skey));
            skey[keylen] = key.addr[i];
            // test find no split
            TEST(0 == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult));
            findresult2 = (trie_findresult_t) {
               .parent = trie.root, .parent_child = child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i),
               .node = childs[i], .chain_parent = trie.root, .chain_child = child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i),
               .matchkeylen = (uint16_t) sizeof(skey), .is_split = 0
            };
            initdecode_trienodeoffsets(&findresult2.offsets, findresult.node);
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
            if (isuser) {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == uservalue_trienodeoffsets(&findresult.offsets, findresult.node));
            } else {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == 0);
            }
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: node with subnode not followed
   for (int isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t keylen = 0; keylen <= 3; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser);
         for (unsigned i = 0; i < lengthof(childs); ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0));
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, lengthof(childs), key.addr, childs));
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, trie.root)[0];
         for (unsigned i = 0; i < lengthof(childs); ++i) {
            uint8_t            skey[2*keylen+1];
            memcpy(skey, key.addr, sizeof(skey));
            skey[keylen] = key.addr[i];
            TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i)));
            if (0 == child_triesubnode(subnode, (uint8_t)i)[0]->child[lengthof(((trie_subnode2_t*)0)->child)-1]) {
               TEST(0 == delete_triesubnode2(child_triesubnode(subnode, (uint8_t)i)));
            }
            TEST(0 == at_trie(&trie, (uint16_t)sizeof(skey), skey));
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult));
            findresult2 = (trie_findresult_t) {
                  .parent = 0, .parent_child = &trie.root, .node = trie.root,
                  .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = keylen
            };
            initdecode_trienodeoffsets(&findresult2.offsets, trie.root);
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
         }
         TEST(0 == free_trie(&trie));
      }
   }

   // TEST findnode_trie, at_trie: chain of nodes with uservalue (uservalue ==> begin of chain_parent)
   for (uint16_t keylen = 1; keylen <= 6; ++ keylen) {
      for (uintptr_t i = 0; i < 4; ++i) {
         uservalue = (void*)i;
         TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+(3u-i)*(keylen+1u), &uservalue, (uint16_t) (i != 0), key.addr+(4u-i)*(keylen+1u)-1u, &childs[i-1]));
      }
      trie.root = childs[3];
      for (uintptr_t i = 0; i < 4; ++i) {
         uint16_t skeylen = (uint16_t)((i+1)*keylen+i);
         TEST((void*)(3-i) == at_trie(&trie, skeylen, key.addr)[0]);
         TEST(0 == findnode_trie(&trie, skeylen, key.addr, &findresult));
         if (i) initdecode_trienodeoffsets(&offsets, childs[4-i]);
         findresult2 = (trie_findresult_t) {
               .parent = i ? childs[4-i] : 0, .parent_child = i ? child_trienode(childs[4-i], &offsets) : &trie.root, .node = childs[3-i],
               .chain_parent = i ? childs[4-i] : 0, .chain_child = i ? child_trienode(childs[4-i], &offsets) : &trie.root, .matchkeylen = skeylen
         };
         initdecode_trienodeoffsets(&findresult2.offsets, childs[3-i]);
         TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult)));
      }
      TEST(0 == free_trie(&trie));
   }

   // unprepare
   TEST(0 == free_trie(&trie));
   TEST(0 == FREE_MM(&key));

   return 0;
ONABORT:
   FREE_MM(&key);
   free_trie(&trie);
   return EINVAL;
}

static int test_insertremove(void)
{
   trie_t            trie = trie_INIT;
   memblock_t        key  = memblock_FREE;
   memblock_t        expectnode_memblock = memblock_FREE;
   memblock_t        memblock;
   expect_node_t *   expectnode;
   expect_node_t *   expectnode2;
   void *            uservalue;

   // prepare
   TEST(0 == ALLOC_MM(1024*1024, &expectnode_memblock));
   TEST(0 == ALLOC_MM(65536, &key));
   for (unsigned i = 0; i < 65536; ++i) {
      key.addr[i] = (uint8_t) (11*i);
   }

   // TEST insert_trie, remove_trie: empty trie <-> (single node or chain of nodes storing prefix)
   for (uint32_t keylen= 0; keylen <= 65535; ++keylen, keylen= (keylen== 10*SIZEMAXNODE ? 65400 : keylen)) {
      // insert_trie
      uservalue = (void*) (2+keylen);
      memblock = expectnode_memblock;
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, uservalue));
      // compare expected result
      TEST(0 != trie.root);
      TEST(0 == new_expectnode(&expectnode, &memblock, (uint16_t)keylen, key.addr, true, uservalue, 0, 0, 0));
      TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0, 0));
      // remove_trie
      uservalue = 0;
      TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &uservalue));
      // compare expected result
      TEST((void*)(2+keylen) == uservalue); // out value
      TEST(0 == trie.root);                 // node freed
   }

#if 0 // TODO: remove line
   // TEST insert_trie, remove_trie: split node, merge node
   for (uint16_t keylen = 1; keylen <= SIZEMAXNODE-HEADERSIZE-sizeof(void*)-1; ++keylen) {
      for (uint16_t splitoffset = 0; splitoffset < keylen; ++splitoffset) {
         memblock  = expectnode_memblock;
         uservalue = (void*) (1u + (uintptr_t)splitoffset);
         TEST(0 == insert_trie(&trie, keylen, key.addr, uservalue));
         // insert_trie: split
         uservalue = (void*) ((uintptr_t)splitoffset);
         TEST(0 == insert_trie(&trie, splitoffset, key.addr, uservalue));
         // compare expected result
         TEST(0 == new_expectnode(&expectnode2, &memblock, (uint16_t)(keylen-splitoffset-1), key.addr+splitoffset+1, true, (void*)(1+(uintptr_t)splitoffset), 0, 0, 0));
         TEST(0 == new_expectnode(&expectnode, &memblock, splitoffset, key.addr, true, uservalue, 1, key.addr+splitoffset, &expectnode));
         TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0));
         // remove_trie: merge with following node
         uservalue = 0;
         TEST(0 == remove_trie(&trie, splitoffset, key.addr, &uservalue));
         // compare expected result
         TEST(0 == new_expectnode(&expectnode,  &memblock, keylen, key.addr, true, (void*)(1+(uintptr_t)splitoffset), 0, 0, 0));
         TEST(splitoffset == (uintptr_t)uservalue);                 // out param
         TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0));// trie structure
         TEST(0 == free_trie(&trie));
      }
   }
#endif // TODO: remove line

   // TEST change into subnode

   // TEST change from subnode into node with childs (add digit[0] which contains number of childs -1: 0 => 1 child ... 255 => 256 childs)

   // TEST prefix chain

   // TEST merge node

   // unprepare
   TEST(0 == free_trie(&trie));
   TEST(0 == FREE_MM(&key));
   TEST(0 == FREE_MM(&expectnode_memblock));

   return 0;
ONABORT:
   free_trie(&trie);
   FREE_MM(&key);
   FREE_MM(&expectnode_memblock);
   return EINVAL;
}

#endif // #if 0

typedef struct nodeoffsets_t nodeoffsets_t;

struct nodeoffsets_t {
   unsigned off2_key;
   unsigned off3_digit;
   unsigned off4_uservalue;
   unsigned off5_child;
   unsigned off6_size;
};

void init_nodeoffsets(nodeoffsets_t * offsets, const trie_node_t * node)
{
   uint8_t  keylen = keylen_trienode(node);
   unsigned offset = off1_keylen_trienode();

   offset += needkeylenbyte_header(keylen);
   offsets->off2_key = offset;
   offset += keylen;
   offsets->off3_digit = offset;
   offset = off4_uservalue_trienode(offset, digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
   offsets->off4_uservalue = offset;
   offset += sizeuservalue_trienode(isuservalue_trienode(node));
   offsets->off5_child = offset;
   offset += childsize_trienode(issubnode_trienode(node), nrchild_trienode(node));
   offsets->off6_size = offset;
}

static int test_header(void)
{
   // TEST header_SIZEMASK
   static_assert( header_SIZEMAX  == header_SIZE5, "only 6 different sizes supported");
   static_assert( header_SIZEMASK == (7 << header_SIZESHIFT), "need 3 bits for encoding");

   // TEST header_SIZE0, header_SIZE1, ..., header_SIZE5
   static_assert( header_SIZE0 == 0
                  && header_SIZE1 == 1
                  && header_SIZE2 == 2
                  && header_SIZE3 == 3
                  && header_SIZE4 == 4
                  && header_SIZE5 == 5,
                  "values after shift"
                  );

   // TEST header_KEYLENMASK
   static_assert( header_KEYLENMASK == 7, "3 bits for encoding");

   // TEST header_KEYLEN0, header_KEYLEN1, ..., header_KEYLEN6, header_KEYLENBYTE
   static_assert( header_KEYLEN0 == 0 && header_KEYLEN1 == 1 && header_KEYLEN2 == 2
                  && header_KEYLEN3 == 3 && header_KEYLEN4 == 4 && header_KEYLEN5 == 5
                  && header_KEYLEN6 == 6 && header_KEYLENBYTE == 7,
                  "all bits are used");

   // == group: query ==

   // TEST needkeylenbyte_header
   static_assert( header_KEYLEN0 == 0 && header_KEYLEN6 == 6 && header_KEYLENBYTE == 7, "used in for loop");
   for (uint8_t i = header_KEYLEN0; i <= header_KEYLEN6; ++i) {
      TEST(0 == needkeylenbyte_header(i));
   }
   for (uint8_t i = header_KEYLENBYTE; i >= header_KEYLENBYTE; ++i) {
      TEST(1 == needkeylenbyte_header(i));
   }

   // TEST keylen_header: returns the bit values masked with header_KEYLENMASK
   for (uint8_t i = header_KEYLEN0; i <= header_KEYLENMASK; ++i) {
      TEST(i == keylen_header(i));
      TEST(i == keylen_header((header_t)(i|~header_KEYLENMASK)));
   }

   // TEST sizeflags_header
   static_assert( header_SIZE0 == 0 && header_SIZEMAX > 0
                  && (header_SIZEMASK>>header_SIZESHIFT) > header_SIZEMAX, "used in for loop");
   for (uint8_t i = header_SIZE0; i <= (header_SIZEMASK>>header_SIZESHIFT); ++i) {
      uint8_t header = (uint8_t) (i << header_SIZESHIFT);
      TEST(i == sizeflags_header(header));
      header = (header_t) (header | ~header_SIZEMASK);
      TEST(i == sizeflags_header(header));
   }

   // TEST issubnode_header
   TEST(0 == issubnode_header((header_t)~header_SUBNODE));
   TEST(0 != issubnode_header(header_SUBNODE));
   TEST(0 != issubnode_header((header_t)~0));

   // TEST isuservalue_header
   TEST(0 == isuservalue_header((header_t)~header_USERVALUE));
   TEST(0 != isuservalue_header(header_USERVALUE));
   TEST(0 != isuservalue_header((header_t)~0));

   // == group: change ==

   // TEST addflags_header
   TEST(0 == addflags_header(0, 0));
   for (header_t i = 1; i; i = (header_t) (i << 1)) {
      TEST(i == addflags_header(0, i));
      TEST((i|1) == addflags_header(1, i));
      TEST((i|128) == addflags_header(128, i));
   }

   // TEST delflags_header
   TEST(0 == delflags_header(0, 0));
   TEST(0 == delflags_header(0, 255));
   TEST(0 == delflags_header(255, 255));
   TEST(255 == delflags_header(255, 0));
   for (header_t i = 1; i; i = (header_t) (i << 1)) {
      TEST((255-i) == delflags_header(255, i));
      TEST((254|i)-i == delflags_header(254, i));
      TEST((127|i)-i == delflags_header(127, i));
   }

   // TEST encodekeylenbyte_header
   TEST(header_KEYLENBYTE == encodekeylenbyte_header(0));
   TEST((header_t)~0      == encodekeylenbyte_header((header_t)~header_KEYLENBYTE));

   // TEST encodekeylen_header
   for (uint8_t i = header_KEYLEN0; i < header_KEYLENMASK; ++i) {
      TEST(i == encodekeylen_header(0, i));
      header_t h = (header_t) ( i | ~header_KEYLENMASK);
      TEST(h == encodekeylen_header((header_t)~header_KEYLENMASK, i));
   }

   // TEST encodesizeflag_header
   for (uint8_t i = header_SIZE0; i <= (header_SIZEMASK>>header_SIZESHIFT); ++i) {
      header_t h = (header_t) (i << header_SIZESHIFT);
      TEST(h == encodesizeflag_header(0, i));
      h = (header_t) (h | ~header_SIZEMASK);
      TEST(h == encodesizeflag_header((header_t)~header_SIZEMASK, i));
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_subnode(void)
{
   trie_subnode_t *  subnode = 0;
   size_t            size_allocated;

   // == group: lifetime ==

   // TEST new_triesubnode
   size_allocated = SIZEALLOCATED_MM();
   TEST(0 == new_triesubnode(&subnode));
   TEST(0 != subnode);
   TEST(size_allocated + sizeof(*subnode) == SIZEALLOCATED_MM());
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      TEST(0 == subnode->child[i]);
   }

   // TEST delete_triesubnode
   TEST(0 == delete_triesubnode(&subnode));
   TEST(0 == subnode);
   TEST(size_allocated == SIZEALLOCATED_MM());
   TEST(0 == delete_triesubnode(&subnode));
   TEST(0 == subnode);

   // TEST new_triesubnode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_triesubnode(&subnode));
   TEST(0 == subnode);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST delete_triesubnode: ERROR
   TEST(0 == new_triesubnode(&subnode));
   TEST(0 != subnode);
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_triesubnode(&subnode));
   TEST(0 == subnode);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // == group: query ==

   // TEST child_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      TEST(0 == child_triesubnode(subnode, (uint8_t)i));
   }
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      subnode->child[i] = (trie_node_t*) (i+1);
   }
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      trie_node_t * child = (trie_node_t*) (i+1);
      TEST(child == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   // == group: change ==

   // TEST setchild_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      setchild_triesubnode(subnode, (uint8_t)i, (trie_node_t*) (i+1));
   }
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      trie_node_t * child = (trie_node_t*) (i+1);
      TEST(child == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   // TEST clearchild_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      setchild_triesubnode(subnode, (uint8_t)i, (trie_node_t*) 100);
      clearchild_triesubnode(subnode, (uint8_t)i);
      TEST(0 == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_node_query(void)
{
   void        * buffer[MAXSIZE / sizeof(void*)];
   trie_node_t * node = (trie_node_t*) buffer;

   // prepare
   memset(buffer, 0, sizeof(buffer));

   // == group: constants ==

   // TEST PTRALIGN
   TEST(PTRALIGN == offsetof(trie_node_t, uservalue));
   TEST(PTRALIGN <= sizeof(void*));

   // TEST MAXSIZE
   node->header = header_SIZEMAX << header_SIZESHIFT;
   TEST(MAXSIZE == decodesize_trienode(node));

   // TEST MAXNROFCHILD_NOUSERVALUE
   static_assert( 32 < MAXNROFCHILD_NOUSERVALUE
                  && 64 > MAXNROFCHILD_NOUSERVALUE
                  && 64*sizeof(void*)/(sizeof(void*)+1) <= MAXNROFCHILD_NOUSERVALUE+1
                  && 64*sizeof(void*)/(sizeof(void*)+1) >= MAXNROFCHILD_NOUSERVALUE,
                  "MAXNROFCHILD_NOUSERVALUE depends on MAXSIZE");

   // TEST MAXNROFCHILD_WITHUSERVALUE
   static_assert( MAXNROFCHILD_WITHUSERVALUE <= MAXNROFCHILD_NOUSERVALUE
                  && MAXNROFCHILD_WITHUSERVALUE >= MAXNROFCHILD_NOUSERVALUE-1,
                  "MAXNROFCHILD_WITHUSERVALUE is the same or one less than MAXNROFCHILD_NOUSERVALUE");

   // == group: query-header ==

   // TEST issubnode_trienode
   node->header = header_SUBNODE;
   TEST(0 != issubnode_trienode(node));
   node->header = (header_t) ~ header_SUBNODE;
   TEST(0 == issubnode_trienode(node));
   node->header = 0;
   TEST(0 == issubnode_trienode(node));

   // TEST isuservalue_trienode
   node->header = header_USERVALUE;
   TEST(0 != isuservalue_trienode(node));
   node->header = (header_t) ~ header_USERVALUE;
   TEST(0 == isuservalue_trienode(node));
   node->header = 0;
   TEST(0 == isuservalue_trienode(node));

   // TEST decodesize_trienode
   node->header = header_SIZE0 << header_SIZESHIFT;
   TEST( 2*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZE1 << header_SIZESHIFT;
   TEST( 4*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZE2 << header_SIZESHIFT;
   TEST( 8*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZE3 << header_SIZESHIFT;
   TEST(16*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZE4 << header_SIZESHIFT;
   TEST(32*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZE5 << header_SIZESHIFT;
   TEST(64*sizeof(void*) == decodesize_trienode(node));
   node->header = header_SIZEMAX << header_SIZESHIFT;
   TEST(64*sizeof(void*) == decodesize_trienode(node));

   // == group: query-helper ==

    // TEST alignoffset_trienode
   TEST(0 == alignoffset_trienode(0));
   for (unsigned i = 0; i < 10; ++i) {
      unsigned offset = sizeof(void*)*i;
      for (unsigned b = 1; b <= sizeof(void*); ++b) {
         TEST(offset+sizeof(void*) == alignoffset_trienode(offset+b));
      }
   }

   // TEST sizeuservalue_trienode
   TEST(sizeof(void*) == sizeuservalue_trienode(1));
   TEST(sizeof(void*) == sizeuservalue_trienode(header_USERVALUE));
   TEST(0 == sizeuservalue_trienode(0));

   // TEST memaddr_trienode
   TEST(0 == memaddr_trienode(0));
   TEST((uint8_t*)buffer == memaddr_trienode(node));

   // TEST off1_keylen_trienode
   TEST((unsigned)&((trie_node_t*)0)->keylen == off1_keylen_trienode());

   // TEST off2_key_trienode
   for (uint8_t i = 0; i < header_KEYLENBYTE; ++i) {
      TEST(off1_keylen_trienode() == off2_key_trienode(needkeylenbyte_header(i)));
   }
   for (uint8_t i = header_KEYLENBYTE; i >= header_KEYLENBYTE; ++i) {
      TEST(off1_keylen_trienode()+1u == off2_key_trienode(needkeylenbyte_header(i)));
   }

   // TEST off3_digit_trienode
   for (unsigned i = 0; i <= 256; ++i) {
      for (unsigned i2 = 0; i2 <= 256; ++i2) {
         TEST(i+i2 == off3_digit_trienode(i, i2));
      }
   }

   // TEST off4_uservalue_trienode
   for (unsigned off3_digit = 0; off3_digit <= 512; ++off3_digit) {
      for (unsigned digitsize  = 0; digitsize <= 32; ++digitsize) {
         unsigned off4 = off4_uservalue_trienode(off3_digit, digitsize);
         TEST(off4 >= off3_digit+digitsize);
         TEST(off4 <  off3_digit+digitsize+sizeof(void*));
         TEST(off4 % sizeof(void*) == 0);
      }
   }

   // TEST off5_child_trienode
   for (unsigned off4_uservalue = 0; off4_uservalue <= 1024; ++ off4_uservalue) {
      TEST(off5_child_trienode(off4_uservalue, sizeuservalue_trienode(1)) == off4_uservalue+sizeof(void*));
      TEST(off5_child_trienode(off4_uservalue, sizeuservalue_trienode(0)) == off4_uservalue);
   }

   // TEST off6_size_trienode
   for (unsigned off5_child = 0; off5_child <= 256; ++ off5_child) {
      for (unsigned childsize = 0; childsize < 64; childsize += sizeof(void*)) {
         TEST(off6_size_trienode(off5_child, childsize) == off5_child + childsize);
      }
   }

   // TEST nrchild_trienode
   for (unsigned nrchild = 255; nrchild <= 255; --nrchild) {
      node->nrchild = (uint8_t) nrchild;
      node->header = header_SUBNODE;
      TEST(nrchild == nrchild_trienode(node));
      node->header = 0;
      TEST(nrchild == nrchild_trienode(node));
   }

   // TEST childs_trienode
   for (unsigned off5_child = 0; off5_child <= 512; ++off5_child) {
      TEST(childs_trienode(node, off5_child) == (void**) ((uint8_t*)buffer + off5_child));
   }

   // TEST childsize_trienode
   for (unsigned nrchild = 0; nrchild <= 255; ++nrchild) {
      TEST(childsize_trienode(header_SUBNODE, (uint8_t) nrchild) == sizeof(void*));
      TEST(childsize_trienode(1, (uint8_t) nrchild) == sizeof(void*));
      TEST(childsize_trienode(0, (uint8_t) nrchild) == nrchild * sizeof(void*));
   }

   // TEST digits_trienode
   for (unsigned off3_digit = 0; off3_digit <= 512; ++off3_digit) {
      TEST(digits_trienode(node, off3_digit) == (uint8_t*)buffer + off3_digit);
   }

   // TEST digitsize_trienode
   for (unsigned nrchild = 0; nrchild <= 255; ++nrchild) {
      TEST(digitsize_trienode(header_SUBNODE, (uint8_t) nrchild) == 0);
      TEST(digitsize_trienode(1, (uint8_t) nrchild) == 0);
      TEST(digitsize_trienode(0, (uint8_t) nrchild) == nrchild);
   }

   // TEST keylen_trienode
   for (unsigned i = 255; i <= 255; --i) {
      if (needkeylenbyte_header((uint8_t)i)) {
         node->header = header_KEYLENBYTE;
         node->keylen = (uint8_t) i;
      } else {
         node->header = (uint8_t) i;
         node->keylen = 255;
      }
      TEST(i == keylen_trienode(node));
   }
   node->header = 0;
   node->keylen = 0;
   TEST(0 == keylen_trienode(node));

   // TEST subnode_trienode
   for (unsigned off5_child = 0; off5_child < MAXSIZE; off5_child += sizeof(void*)) {
      buffer[off5_child/sizeof(void*)] = buffer;
      TEST(subnode_trienode(node, off5_child) == (trie_subnode_t*)buffer);
      buffer[off5_child/sizeof(void*)] = 0;
      TEST(subnode_trienode(node, off5_child) == 0);
   }

   // TEST uservalue_trienode
   for (unsigned off4_uservalue = 0; off4_uservalue < MAXSIZE; off4_uservalue += sizeof(void*)) {
      buffer[off4_uservalue/sizeof(void*)] = buffer;
      TEST(uservalue_trienode(node, off4_uservalue) == (void*)buffer);
      buffer[off4_uservalue/sizeof(void*)] = 0;
      TEST(uservalue_trienode(node, off4_uservalue) == 0);
   }

   // TEST childoff5_trienode
   for (int isuservalue = 0; isuservalue <= 1; ++isuservalue) {
      for (int issubnode = 0; issubnode <= 1; ++issubnode) {
         for (unsigned keylen = 0; keylen <= 255; ++keylen) {
            for (unsigned nrchild = 0; nrchild <= 255; ++nrchild) {
               node->header  = addflags_header(0, isuservalue ? header_USERVALUE : 0);
               node->header  = addflags_header(node->header, issubnode ? header_SUBNODE : 0);
               node->nrchild = (uint8_t) nrchild;
               encodekeylen_trienode(node, (uint8_t) keylen);
               unsigned off5 = childoff5_trienode(node);
               unsigned expect = (isuservalue ? sizeof(void*) : 0);
               expect += alignoffset_trienode(  off2_key_trienode(needkeylenbyte_header((uint8_t)keylen))
                                                + keylen
                                                + (issubnode ? 0 : nrchild));
               TEST(off5 == expect);
            }
         }
      }
   }
   node->header  = 0;
   node->nrchild = 0;

   return 0;
ONABORT:
   return EINVAL;
}

static void get_node_size(unsigned size, /*out*/unsigned * nodesize, /*out*/header_t * header)
{
   if (size <= MAXSIZE/32) {
      *nodesize = MAXSIZE/32;
      *header = header_SIZE0<<header_SIZESHIFT;
   } else if (size <= MAXSIZE/16) {
      *nodesize = MAXSIZE/16;
      *header = header_SIZE1<<header_SIZESHIFT;
   } else if (size <= MAXSIZE/8) {
      *nodesize = MAXSIZE/8;
      *header = header_SIZE2<<header_SIZESHIFT;
   } else if (size <= MAXSIZE/4) {
      *nodesize = MAXSIZE/4;
      *header = header_SIZE3<<header_SIZESHIFT;
   } else if (size <= MAXSIZE/2) {
      *nodesize = MAXSIZE/2;
      *header = header_SIZE4<<header_SIZESHIFT;
   } else {
      *nodesize = MAXSIZE;
      *header = header_SIZE5<<header_SIZESHIFT;
   }
}

static int test_node_lifetime(void)
{
   trie_node_t *  node;
   size_t         size_allocated = SIZEALLOCATED_MM();

   // == group: lifetime ==

   for (uint8_t isuservalue = 0, maxchild = MAXNROFCHILD_NOUSERVALUE+1; isuservalue <= 1; ++isuservalue, maxchild = MAXNROFCHILD_WITHUSERVALUE+1) {
      uint8_t key[256];
      uint8_t digit[256];
      void  * child[256];
      void  * uservalue = key;
      unsigned usersize = isuservalue ? sizeof(void*) : 0;

      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         digit[0] = (uint8_t) keylen;
         child[0] = (void*) 0x11002200;
         key[keylen] = (uint8_t) (0x80 | (keylen&0x7f));

         for (uint8_t nrchild = !isuservalue; nrchild < maxchild; ++nrchild) {
            digit[nrchild] = (uint8_t) (keylen+nrchild);
            child[nrchild] = (void*) (nrchild&1 ? ~(uintptr_t)nrchild : (uintptr_t)nrchild << 16);

            const unsigned childsize = nrchild * sizeof(void*);
            unsigned keysize = keylen;
            unsigned lenbyte = (keylen >= header_KEYLENBYTE);
            unsigned size = usersize + childsize + keysize + lenbyte + nrchild
                          + off1_keylen_trienode();

            if (size > MAXSIZE) {
               size -= keysize + lenbyte;
               TEST(size <= MAXSIZE);
               keysize = MAXSIZE - size;
               keysize -= (keysize >= header_KEYLENBYTE);
               lenbyte = (keysize >= header_KEYLENBYTE);
               size += keysize + lenbyte;
            }

            header_t header;
            unsigned nodesize;
            get_node_size(size, &nodesize, &header);
            if (isuservalue) header = addflags_header(header, header_USERVALUE);
            header = (header_t) (lenbyte ? encodekeylenbyte_header(header) : encodekeylen_header(header, (uint8_t)keysize));

            // TEST new_trienode: child array
            node = 0;
            TEST(0 == new_trienode(&node, isuservalue, nrchild, (uint8_t)keylen, uservalue, digit, child, key));
            TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
            TEST(0 != node);
            TEST(header  == node->header);
            TEST(nrchild == node->nrchild);
            TEST(keysize == keylen_trienode(node));
            nodeoffsets_t off;
            init_nodeoffsets(&off, node);
            // compare copied content
            TEST(0 == memcmp(memaddr_trienode(node)+off.off2_key, key + keylen - keysize, keysize));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off3_digit, digit, nrchild));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off4_uservalue, &uservalue, usersize));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off5_child, child, childsize));

            // TEST delete_trienode: child array
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
         }

         for (uint8_t nrchild = maxchild; nrchild >= maxchild; ++nrchild) {

            digit[nrchild] = (uint8_t) (keylen+nrchild);
            child[nrchild] = (void*) (nrchild&1 ? ~(uintptr_t)nrchild : (uintptr_t)nrchild << 16);

            unsigned keysize = keylen;
            unsigned lenbyte = (keylen >= header_KEYLENBYTE);
            unsigned size = usersize + sizeof(void*)/*subnode*/ + keysize + lenbyte
                          + off1_keylen_trienode();

            if (size > MAXSIZE) {
               size -= keysize + lenbyte;
               TEST(size <= MAXSIZE);
               keysize = MAXSIZE - size;
               keysize -= (keysize >= header_KEYLENBYTE);
               lenbyte = (keysize >= header_KEYLENBYTE);
               size += keysize + lenbyte;
            }

            header_t header;
            unsigned nodesize;
            get_node_size(size, &nodesize, &header);
            header = addflags_header(header, header_SUBNODE);
            if (isuservalue) header = addflags_header(header, header_USERVALUE);
            header = (header_t) (lenbyte ? encodekeylenbyte_header(header) : encodekeylen_header(header, (uint8_t)keysize));

            // TEST new_trienode: subnode
            node = 0;
            TEST(0 == new_trienode(&node, isuservalue, nrchild, (uint8_t)keylen, uservalue, digit, child, key));
            TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
            TEST(0 != node);
            TEST(header  == node->header);
            TEST(nrchild-1 == node->nrchild);
            TEST(keysize == keylen_trienode(node));
            nodeoffsets_t off;
            init_nodeoffsets(&off, node);

            // compare copied content
            TEST(0 == memcmp(memaddr_trienode(node)+off.off2_key, key + keylen - keysize, keysize));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off4_uservalue, &uservalue, usersize));
            trie_subnode_t * subnode = *(void**)(memaddr_trienode(node)+off.off5_child);
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(subnode->child[digit[i]] == child[i]);
            }
            for (unsigned i = nrchild; i <= 255; ++i) {
               TEST(subnode->child[(uint8_t)(keylen+i)] == 0);
            }

            // TEST delete_trienode: subnode
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
         }

      }
   }

   // TEST new_trienode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   // no subnode
   node = 0;
   TEST(ENOMEM == new_trienode(&node, 1, 0, 3, (void*)1, 0, 0, (const uint8_t*)"key"));
   TEST(0 == node);
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      uint8_t digit[MAXNROFCHILD_WITHUSERVALUE+1] = {0};
      void  * child[MAXNROFCHILD_WITHUSERVALUE+1] = {0};
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      TEST(ENOMEM == new_trienode(&node, 1, MAXNROFCHILD_WITHUSERVALUE+1, 0, (void*)1, digit, child, 0));
      TEST(0 == node);
   }

   // TEST delete_trienode: ERROR
   // no subnode
   TEST(0 == new_trienode(&node, 1, 0, 3, (void*)1, 0, 0, (const uint8_t*)"key"));
   init_testerrortimer(&s_trie_errtimer, 1, EPERM);
   TEST(EPERM == delete_trienode(&node));
   TEST(0 == node);
   TEST(size_allocated == SIZEALLOCATED_MM());
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      uint8_t digit[MAXNROFCHILD_NOUSERVALUE+1] = {0};
      void  * child[MAXNROFCHILD_NOUSERVALUE+1] = {0};
      TEST(0 == new_trienode(&node, 0, MAXNROFCHILD_NOUSERVALUE+1, 0, (void*)0, digit, child, 0));
      init_testerrortimer(&s_trie_errtimer, i, EPERM);
      TEST(EPERM == delete_trienode(&node));
      TEST(0 == node);
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST delete_trienode: wrong size (EINVAL)
   TEST(0 == allocmemory_trienode(&node, MAXSIZE));
   TEST(size_allocated + MAXSIZE == SIZEALLOCATED_MM());
   node->header = encodesizeflag_header(0, header_SIZE0);
   // test memory manager checks correct size of free memory block and does nothing!
   void * oldnode = node;
   TEST(EINVAL == delete_trienode(&node));
   TEST(0 == node);
   // nothing freed
   TEST(size_allocated + MAXSIZE == SIZEALLOCATED_MM());
   node = oldnode;
   node->header = encodesizeflag_header(0, header_SIZEMAX);
   TEST(0 == delete_trienode(&node));
   TEST(0 == node);
   TEST(size_allocated == SIZEALLOCATED_MM());

   return 0;
ONABORT:
   delete_trienode(&node);
   return EINVAL;
}

static int compare_content(
   trie_node_t *     node,
   nodeoffsets_t *   off,
   header_t          header,
   unsigned          keylen,
   uint8_t           key[keylen],
   void *            uservalue,
   unsigned          nrchild,
   uint8_t           digit[nrchild],
   void *            child[nrchild])
{
   TEST(header == node->header);
   TEST(keylen == keylen_trienode(node));
   TEST(0 == memcmp(memaddr_trienode(node)+off->off2_key, key, keylen));
   TEST(0 == memcmp(memaddr_trienode(node)+off->off4_uservalue, &uservalue, sizeuservalue_trienode(isuservalue_trienode(node))));
   if (issubnode_trienode(node)) {
      TEST(nrchild-1 == nrchild_trienode(node));
      trie_subnode_t subnode2;
      memset(&subnode2, 0, sizeof(subnode2));
      for (unsigned i = 0; i < nrchild; ++i) {
         TEST(0 == subnode2.child[digit[i]]);
         subnode2.child[digit[i]] = child[i];
      }
      trie_subnode_t * subnode = subnode_trienode(node, off->off5_child);
      TEST(0 == memcmp(subnode->child, subnode2.child, sizeof(subnode2.child)));

   } else {
      TEST(nrchild == nrchild_trienode(node));
      TEST(0 == memcmp(memaddr_trienode(node)+off->off3_digit, digit, nrchild));
      TEST(0 == memcmp(memaddr_trienode(node)+off->off5_child, child, childsize_trienode(0, (uint8_t)nrchild)));
   }

   return 0;
ONABORT:
   return EINVAL;
}

static int test_node_change(void)
{
   void        *  buffer[MAXSIZE / sizeof(void*)];
   trie_node_t *  node  = (trie_node_t*) buffer;
   nodeoffsets_t  off;
   nodeoffsets_t  off2;
   size_t         size_allocated = SIZEALLOCATED_MM();

   // prepare
   memset(buffer, 0, sizeof(buffer));

   // == group: change-helper ==

   // TEST setsubnode_trienode
   for (unsigned off5_child = 0; off5_child < MAXSIZE; off5_child += sizeof(void*)) {
      buffer[off5_child/sizeof(void*)] = 0;
      setsubnode_trienode(node, off5_child, (trie_subnode_t*)buffer);
      TEST(buffer[off5_child/sizeof(void*)] == (trie_subnode_t*)buffer);
      setsubnode_trienode(node, off5_child, (trie_subnode_t*)0);
      TEST(buffer[off5_child/sizeof(void*)] == 0);
   }

   // TEST setuservalue_trienode
   for (unsigned off4_uservalue = 0; off4_uservalue < MAXSIZE; off4_uservalue += sizeof(void*)) {
      buffer[off4_uservalue/sizeof(void*)] = 0;
      setuservalue_trienode(node, off4_uservalue, (void*)buffer);
      TEST(buffer[off4_uservalue/sizeof(void*)] == (void*)buffer);
      setuservalue_trienode(node, off4_uservalue, (void*)0);
      TEST(buffer[off4_uservalue/sizeof(void*)] == 0);
   }

   for (unsigned i = 0; i <= 255; ++i) {
      // TEST addheaderflag_trienode: header == 0
      node->header = 0;
      addheaderflag_trienode(node, (uint8_t)i);
      TEST(i == node->header);

      // TEST addheaderflag_trienode: header != 0
      node->header = (header_t) ~i;
      addheaderflag_trienode(node, (uint8_t)i);
      TEST(255 == node->header);

      // TEST delheaderflag_trienode: header == 0
      node->header = (header_t) i;
      delheaderflag_trienode(node, (uint8_t)i);
      TEST(0 == node->header);

      // TEST delheaderflag_trienode: header != 0
      node->header = 255;
      delheaderflag_trienode(node, (uint8_t)i);
      TEST((header_t)~i == node->header);
   }

   for (int i = 0; i <= 255; ++i) {
      // TEST encodekeylen_trienode: header == 0
      node->header = 0;
      node->keylen = 0;
      encodekeylen_trienode(node, (uint8_t)i);
      if (i >= header_KEYLENBYTE) {
         TEST(header_KEYLENBYTE == node->header);
         TEST(i == node->keylen);
      } else {
         TEST(i == node->header);
         TEST(0 == node->keylen);
      }

      // TEST encodekeylen_trienode: header != 0
      node->header = (header_t) ~header_KEYLENMASK;
      node->keylen = 0;
      encodekeylen_trienode(node, (uint8_t)i);
      if (i >= header_KEYLENBYTE) {
         TEST(255 == node->header);
         TEST(i == node->keylen);
      } else {
         header_t h = (header_t) (i|~header_KEYLENMASK);
         TEST(h == node->header);
         TEST(0 == node->keylen);
      }

   }

   for (unsigned nodesize = 8; nodesize < MAXSIZE; nodesize *= 2) {
      trie_node_t * newnode = 0;

      // TEST allocmemory_trienode
      TEST(0 == allocmemory_trienode(&newnode, nodesize));
      TEST(0 != newnode);
      TEST(size_allocated + nodesize == SIZEALLOCATED_MM());

      // TEST freememory_trienode
      TEST(0 == freememory_trienode(newnode, nodesize));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST allocmemory_trienode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == allocmemory_trienode(&node, MAXSIZE));
   TEST(node == (void*)buffer);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST freememory_trienode: ERROR
   TEST(0 == allocmemory_trienode(&node, MAXSIZE));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == freememory_trienode(node, MAXSIZE));
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST expandnode_trienode
   node = (trie_node_t*)buffer;
   for (unsigned i = header_SIZE0; i < header_SIZEMAX; ++i) {
      for (unsigned i2 = i+1; i2 <= header_SIZEMAX; ++i2) {
         node->header = encodesizeflag_header(0, (header_t)i);
         node->header = addflags_header(node->header, header_SUBNODE|header_USERVALUE|header_KEYLEN4);
         unsigned  oldsize = decodesize_trienode(node);
         unsigned  newsize = oldsize << (i2-i);
         trie_node_t * newnode = 0;
         TEST(0 == expandnode_trienode(&newnode, node->header, oldsize, newsize/2+1/*only 1 byte bigger*/));
         TEST(0 != newnode);
         TEST(size_allocated + newsize == SIZEALLOCATED_MM());
         // only header field is set in expandnode_trienode
         TEST(newnode->header == encodesizeflag_header(node->header, (header_t)i2));
         TEST(0 == freememory_trienode(newnode, newsize));
         TEST(size_allocated == SIZEALLOCATED_MM());
      }
   }

   // TEST expandnode_trienode: ERROR
   node->header = encodesizeflag_header(0, header_SIZE1);
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == expandnode_trienode(&node, node->header, decodesize_trienode(node), MAXSIZE));
   TEST(node == (trie_node_t*)buffer);
   TEST(node->header == encodesizeflag_header(0, header_SIZE1));
   TEST(size_allocated == SIZEALLOCATED_MM());

   // addsubnode_trienode, trydelsubnode_trienode
   for (uint8_t isuservalue = 0; isuservalue <= 1; ++isuservalue) {
      uint8_t key[256];
      uint8_t digit[256] = { 0 };
      void  * child[256] = { (void*) 0x10203040, 0 };
      void  * uservalue = buffer;

      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         key[keylen] = (uint8_t) (0x80 | (keylen&0x7f));

         for (unsigned nrchild = !isuservalue; nrchild <= 255; ++nrchild) {
            digit[nrchild] = (uint8_t) nrchild;
            child[nrchild] = (void*) ~ (uintptr_t) (nrchild << 16);

            TEST(0 == new_trienode(&node, isuservalue, (uint8_t)nrchild, (uint8_t)keylen, uservalue, digit, child, key));
            unsigned nodesize = decodesize_trienode(node);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;
            init_nodeoffsets(&off, node);
            unsigned keysize = off.off3_digit - off.off2_key;

            if (nrchild > 0 && !issubnode_trienode(node)) {

               // TEST addsubnode_trienode: ERROR
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
               TEST(ENOMEM == addsubnode_trienode(node, off.off3_digit));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, &off, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

               // TEST addsubnode_trienode
               TEST(0 == addsubnode_trienode(node, off.off3_digit));
               // subnode is allocated + no reallocation of node
               TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
               TEST(nodesize == decodesize_trienode(node));
               TEST(oldnode  == node);
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_uservalue == alignoffset_trienode(off.off3_digit));
               TEST(off2.off5_child-off2.off4_uservalue == off.off5_child-off.off4_uservalue);
               TEST(off2.off6_size == off2.off5_child+sizeof(void*));
               // compare moved content
               TEST(0 != issubnode_trienode(node));
               TEST(0 == compare_content(node, &off2, addflags_header(oldheader, header_SUBNODE),
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

               // TEST trydelsubnode_trienode: no reallocation
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);  // free error is ignored !!
               TEST(0 == trydelsubnode_trienode(&node, off2.off3_digit));
               // subnode is freed + no reallocation of node
               TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
               TEST(nodesize == decodesize_trienode(node));
               TEST(oldnode  == node);
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare content
               TEST(oldheader == node->header);
               TEST(0 == compare_content(node, &off2, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

            } else if (issubnode_trienode(node) && decodesize_trienode(node) < MAXSIZE) {

               unsigned freesize = nodesize - off.off6_size
                                 + (off.off4_uservalue - off.off3_digit) + sizeof(void*)/*subnode*/;
               unsigned minnrchild = freesize / (1 + sizeof(void*));
               unsigned maxnrchild = (MAXSIZE - nodesize + freesize) / (1 + sizeof(void*));
               unsigned nrchild2 = nrchild // nrchild > MAXNROFCHILD !!
                                 - (isuservalue ? MAXNROFCHILD_WITHUSERVALUE : MAXNROFCHILD_NOUSERVALUE)
                                 + minnrchild; // ==> realloc

               // TEST trydelsubnode_trienode: EINVAL (> MAXIZSE)
               TEST(EINVAL == trydelsubnode_trienode(&node, off.off3_digit));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, &off, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

               // TEST trydelsubnode_trienode: EINVAL (nrchild overflows)
               node->nrchild = 255;
               TEST(EINVAL == trydelsubnode_trienode(&node, off.off3_digit));
               TEST(oldnode == node);
               TEST(node->nrchild == 255);
               node->nrchild = (uint8_t) (nrchild-1);
               TEST(0 == compare_content(node, &off, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

               if (nrchild2 <= maxnrchild) {
                  // clear child which does not fit in reallocated node
                  trie_subnode_t * subnode = subnode_trienode(node, off.off5_child);
                  for (unsigned i = nrchild2; i <= nrchild; ++i) {
                     subnode->child[i] = 0;
                  }
                  node->nrchild = (uint8_t) (nrchild2-1);
                  unsigned newsize;
                  header_t sizeflags;
                  get_node_size(nodesize - freesize + nrchild2 * (1+sizeof(void*)), &newsize, &sizeflags);

                  // TEST trydelsubnode_trienode: ERROR
                  init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
                  TEST(ENOMEM == trydelsubnode_trienode(&node, off.off3_digit));
                  TEST(oldnode == node);
                  TEST(0 == compare_content(node, &off, oldheader,
                                 keysize, key + keylen - keysize,
                                 uservalue, nrchild2, digit, child));

                  // TEST trydelsubnode_trienode: node is reallocated
                  TEST(0 == trydelsubnode_trienode(&node, off.off3_digit));
                  // subnode is freed + reallocation
                  TEST(size_allocated + newsize == SIZEALLOCATED_MM());
                  TEST(newsize == decodesize_trienode(node));
                  // offsets ok
                  init_nodeoffsets(&off2, node);
                  TEST(off2.off2_key   == off.off2_key);
                  TEST(off2.off3_digit == off.off3_digit);
                  TEST(off2.off4_uservalue == alignoffset_trienode(off.off3_digit + nrchild2));
                  TEST(off2.off5_child-off2.off4_uservalue == off.off5_child-off.off4_uservalue);
                  TEST(off2.off6_size == off2.off5_child + nrchild2 * sizeof(void*));
                  // compare moved content
                  TEST(0 == issubnode_header(node->header));
                  TEST(0 == compare_content(node, &off2,
                                 (header_t)(delflags_header(oldheader, header_SIZEMASK|header_SUBNODE)|sizeflags),
                                 keysize, key + keylen - keysize,
                                 uservalue, nrchild2, digit, child));
               }

            }

            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // deluservalue_trienode, tryadduservalue_trienode
   for (uint8_t isuservalue = 0; isuservalue <= 1; ++isuservalue) {
      uint8_t key[256];
      uint8_t digit[256] = { 0 };
      void  * child[256] = { (void*) 0x10203040, 0 };
      void  * uservalue = buffer;

      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         key[keylen] = (uint8_t) ~keylen;

         for (unsigned nrchild = !isuservalue; nrchild <= MAXNROFCHILD_NOUSERVALUE+2; ++nrchild) {
            digit[nrchild] = (uint8_t) nrchild;
            child[nrchild] = (void*) ~ (uintptr_t) (nrchild << 16);

            TEST(0 == new_trienode(&node, isuservalue, (uint8_t)nrchild, (uint8_t)keylen, uservalue, digit, child, key));
            init_nodeoffsets(&off, node);
            unsigned    keysize   = off.off3_digit - off.off2_key;
            unsigned    nodesize  = decodesize_trienode(node);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;

            size_allocated = SIZEALLOCATED_MM();

            if (isuservalue) {

               // TEST deluservalue_trienode
               deluservalue_trienode(node, off.off4_uservalue);
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_uservalue == off.off4_uservalue);
               TEST(off2.off5_child     == off.off4_uservalue);
               TEST(off2.off6_size      == off.off6_size - sizeof(void*));
               // compare moved content
               TEST(0 == isuservalue_trienode(node));
               TEST(0 == compare_content(node, &off2,
                              delflags_header(oldheader, header_USERVALUE),
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

               // TEST tryadduservalue_trienode: no reallocation
               TEST(0 == tryadduservalue_trienode(&node, off.off4_uservalue, uservalue));
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare moved content
               TEST(0 != isuservalue_trienode(node));
               TEST(0 == compare_content(node, &off2, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

            } else if (MAXSIZE == nodesize && nodesize < off.off6_size + sizeof(void*)) {
               TEST(EINVAL == tryadduservalue_trienode(&node, off.off4_uservalue, uservalue));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, &off, oldheader,
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

            } else if (nodesize < off.off6_size + sizeof(void*)) {
               // TEST tryadduservalue_trienode: reallocation
               TEST(0 == tryadduservalue_trienode(&node, off.off4_uservalue, uservalue));
               TEST(0 != node);
               // reallocation
               TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_uservalue == off.off4_uservalue);
               TEST(off2.off5_child     == off.off5_child + sizeof(void*));
               TEST(off2.off6_size      == off.off6_size + sizeof(void*));
               // compare moved content
               TEST(0 != isuservalue_trienode(node));
               TEST(0 == compare_content(node, &off2,
                              encodesizeflag_header(addflags_header(oldheader, header_USERVALUE), (header_t)(sizeflags_header(oldheader)+1)),
                              keysize, key + keylen - keysize,
                              uservalue, nrchild, digit, child));

            }

            TEST(0 == delete_trienode(&node));
            if (keysize < keylen) break;
         }
      }
   }

   // delkeyprefix_trienode, delkeyprefix_trienode

   // TEST tryaddkeyprefix_trienode

   // TEST tryaddkeyprefix_trienode


   // tryaddchild_trienode, delchild_trienode

   // TEST tryaddchild_trienode

   // TEST delchild_trienode

   return 0;
ONABORT:
   if (node != (void*)buffer) {
      delete_trienode(&node);
   }
   return EINVAL;
}

int unittest_ds_inmem_trie()
{
   if (test_header())         goto ONABORT;
   if (test_subnode())        goto ONABORT;
   if (test_node_query())     goto ONABORT;
   if (test_node_lifetime())  goto ONABORT;
   if (test_node_change())    goto ONABORT;

   // TODO: if (test_initfree())          goto ONABORT;
   // TODO: if (test_query())             goto ONABORT;
   // TODO: if (test_insertremove())      goto ONABORT;
   // TODO: if (test_iterator())          goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
