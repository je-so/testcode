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
 *   Beschreibe Struktur der Knoten
 *
 *   Beschreibe ReadCursor(+ Update Value), InsertCursor, DeleteCursor !!
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
 * header_KEYLEN1     - trie_node_t.key[0]    == binary key digit
 * header_KEYLEN2     - trie_node_t.key[0..1] == binary key digits
 * header_KEYLEN3     - trie_node_t.key[0..2] == binary key digits
 * header_KEYLEN4     - trie_node_t.key[0..3] == binary key digits
 * header_KEYLEN5     - trie_node_t.key[0..4] == binary key digits
 * header_KEYLEN6     - trie_node_t.key[0..5] == binary key digits
 * header_KEYLENBYTE  - trie_node_t.keylen    == contains key length
 *                      trie_node_t.key[0..keylen-1] == binary key digits
 * header_VALUE       - If set indicates that a value member (type void*) is available.
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
   header_VALUE      = 8,
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

static inline int isvalue_header(const header_t header)
{
   return (header & header_VALUE);
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
 * Points to 256 childs of type <trie_node_t>.
 * If child[i] is 0 it means there is no child
 * for 8-bit binary digit i at a certain offset in the search key. */
struct trie_subnode_t {
   // group: struct fields
   /* variable: child
    * An array of 256 pointer to <trie_node_t>.
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

/* function: childaddr_triesubnode
 * Returns child pointer for digit. */
static inline trie_node_t ** childaddr_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   return &subnode->child[digit];
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
   // TODO: remove function
   subnode->child[digit] = 0;
}


/* struct: trie_node_t
 * Describes a flexible structure of trie node data stored in memory.
 * Two fields <header> and <nrchild> are followed by optional data fields.
 * The optional fields are: (sub)key, value, digit and child arrays.
 * A subnode (pointer to <trie_subnode_t>) could be stored instead of
 * the digit and child arrays.
 *
 * Memory Size:
 * The size of the structure is flexible.
 * It can use up to <trie_node_t.MAXSIZE> bytes. */
struct trie_node_t {
   // group: struct fields
   /* variable: header
    * Flags which describes content of <trie_node_t>. See <header_t>. */
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
   /* variable: value
    * Start of ptr-aligned data.
    * Contains an optional stored value.
    * The value is associated with the whole key starting with the root node. */
   void     *  value;  // optional (fixed size)
   // void  *  child_or_subnode[];     // child:   optional (variable size) points to trie_node_t
                                       // subnode: optional (size 1) points to trie_subnode_t
};

// group: constants

/* define: PTRALIGN
 * Alignment of <trie_node_t.value>. The first byte in trie_node_t
 * which encodes the availability of the optional members is followed by
 * byte aligned data which is in turn followed by pointer aligned data.
 * This value is the alignment necessary for a pointer on this architecture.
 * This value must be a power of two. */
#define PTRALIGN  (offsetof(trie_node_t, value))

/* define: MAXSIZE
 * The maximum size in bytes used by a <trie_node_t>. */
#define MAXSIZE   (64*sizeof(void*))

/* define: MINSIZE
 * The minumum size in bytes used by a <trie_node_t>. */
#define MINSIZE   (2*sizeof(void*))

/* define: MAXNROFCHILD
 * The maximum nr of child pointer in child array of <trie_node_t>.
 * The value is calculated with the assumption that no key is stored in the node
 * but with an additional value.
 * If a node needs to hold more child pointers it has to switch to a <trie_subnode_t>.
 * This value must be less than 256 else <trie_node_t.nrchild> would overflow.
 * */
#define MAXNROFCHILD \
         (  (MAXSIZE-offsetof(trie_node_t, keylen)-sizeof(void*)) \
            / (sizeof(trie_node_t*)+sizeof(uint8_t)))

/* define: COMPUTEKEYLEN
 * Used to implement <NOSPLITKEYLEN> and <MAXKEYLEN>. */
#define COMPUTEKEYLEN(NODESIZE) \
         ((NODESIZE)-offsetof(trie_node_t, keylen) \
            -(sizeof(void*) >= sizeof(trie_node_t*) ? sizeof(void*) : sizeof(trie_node_t*)) /*child or value*/ \
            -sizeof(uint8_t) /*keylenbyte*/)

/* define: NOSPLITKEYLEN
 * Up to this keylen keys are not split over several nodes.
 * The function assumes a node with a single value or a single child. */
#define NOSPLITKEYLEN COMPUTEKEYLEN(4*sizeof(void*))

/* define: MAXKEYLEN
 * This is the maximum keylen storable in a node with a single value. */
#define MAXKEYLEN     COMPUTEKEYLEN(256)

// group: query-header

static inline int issubnode_trienode(const trie_node_t * node)
{
   return issubnode_header(node->header);
}

static inline int isvalue_trienode(const trie_node_t * node)
{
   return isvalue_header(node->header);
}

/* function: nodesize_trienode
 * Returns the size in bytes of a node decoded from its <header_t>.
 * */
static inline unsigned nodesize_trienode(const trie_node_t * node)
{
   return (2*sizeof(void*)) << sizeflags_header(node->header);
}

// group: query-helper

/* function: splitkeylen_trienode
 * Computes the correct size of the key in case of splitting it over several nodes.
 * Use macro <NOSPLITKEYLEN> to determine if you need to call this function.
 * In case of (keylen<=NOSPLITKEYLEN) you do not need to call this function.
 * This functions returns <MAXKEYLEN> if (keylen>=MAXKEYLEN).
 * */
uint8_t splitkeylen_trienode(uint16_t keylen)
{
#define SPLITKEYLEN5  COMPUTEKEYLEN(64*sizeof(void*))
#define SPLITKEYLEN4  COMPUTEKEYLEN(32*sizeof(void*))
#define SPLITKEYLEN3  COMPUTEKEYLEN(16*sizeof(void*))
#define SPLITKEYLEN2  COMPUTEKEYLEN(8*sizeof(void*))
#define SPLITKEYLEN1  COMPUTEKEYLEN(4*sizeof(void*))
#define SPLITKEYLEN0  COMPUTEKEYLEN(2*sizeof(void*))

   static_assert( 2*sizeof(void*) == MINSIZE
                  && 64*sizeof(void*) == MAXSIZE,
                  "error case ==> adapt parameter values in SPLITKEYLEN? macros");

   static_assert( MAXKEYLEN == SPLITKEYLEN5     /*either: 32 bit architecure*/
                  || MAXKEYLEN == SPLITKEYLEN4, /*or:     64 bit architecure*/
                  "error case ==> redefine MAXKEYLEN to match your nodesize");

   if (keylen >= MAXKEYLEN) return MAXKEYLEN;
   if (keylen <= NOSPLITKEYLEN) return (uint8_t) keylen;
   // !! NOSPLITKEYLEN < keylen && keylen < MAXKEYLEN && keylen < SPLITKEYLEN5
   if (SPLITKEYLEN3 < MAXKEYLEN && keylen >= SPLITKEYLEN3) {
      if (SPLITKEYLEN4 < MAXKEYLEN && keylen >= SPLITKEYLEN4) {
         if (SPLITKEYLEN5 <= MAXKEYLEN && keylen > SPLITKEYLEN4+SPLITKEYLEN3) return (uint8_t) keylen;
         return SPLITKEYLEN4;
      }
      if (SPLITKEYLEN4 <= MAXKEYLEN && keylen > SPLITKEYLEN3+SPLITKEYLEN2) return (uint8_t) keylen;
      return SPLITKEYLEN3;
   }
   // !! keylen < SPLITKEYLEN3
   if (SPLITKEYLEN2 < MAXKEYLEN && keylen >= SPLITKEYLEN2) {
      if (SPLITKEYLEN3 <= MAXKEYLEN  && keylen > SPLITKEYLEN2+SPLITKEYLEN1) return (uint8_t) keylen;
      return SPLITKEYLEN2;
   }
   // !! keylen < SPLITKEYLEN2
   if (SPLITKEYLEN2 <= MAXKEYLEN && keylen > SPLITKEYLEN1+SPLITKEYLEN0) return (uint8_t) keylen;

   static_assert(SPLITKEYLEN1 == NOSPLITKEYLEN, "error case: adapt function to match NOSPLITKEYLEN as last case");

   return SPLITKEYLEN1;

#undef SPLITKEYLEN5
#undef SPLITKEYLEN4
#undef SPLITKEYLEN3
#undef SPLITKEYLEN2
#undef SPLITKEYLEN1
#undef SPLITKEYLEN0
}

/* function: alignoffset_trienode
 * Aligns offset to architecture specific pointer alignment. */
static inline unsigned alignoffset_trienode(unsigned offset)
{
   static_assert(ispowerof2_int(PTRALIGN), "use bit mask to align value");
   return (offset + PTRALIGN-1) & ~(PTRALIGN-1);
}

/* function: valuesize_trienode
 * Returns 0 or sizeof(void*). */
static inline unsigned valuesize_trienode(int isvalue)
{
   return isvalue ? sizeof(void*) : 0u;
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

static inline unsigned off4_child_trienode(unsigned off3_digit, unsigned digitsize)
{
   return alignoffset_trienode(off3_digit + digitsize);
}

static inline unsigned off5_value_trienode(const unsigned off4_child, const unsigned childsize)
{
   return off4_child + childsize;
}

/* function: off6_size_trienode
 * Returns the size of used bytes in <trienode_t>.
 * The size is calculated from the offset of the optional field value and its size. */
static inline unsigned off6_size_trienode(const unsigned off5_value, const unsigned valuesize)
{
   return off5_value + valuesize;
}

static inline uint8_t nrchild_trienode(const trie_node_t * node)
{
   return node->nrchild;
}

static inline trie_node_t ** childs_trienode(trie_node_t * node, unsigned off4_child)
{
   return (trie_node_t**) (memaddr_trienode(node) + off4_child);
}

static inline unsigned childsize_trienode(const int issubnode, const uint8_t nrchild)
{
   return issubnode ? sizeof(void*) : nrchild * sizeof(trie_node_t*);
}

static inline uint8_t * digits_trienode(trie_node_t * node, unsigned off3_digit)
{
   return memaddr_trienode(node) + off3_digit;
}

static inline unsigned digitsize_trienode(const int issubnode, const uint8_t nrchild)
{
   return issubnode ? 0u : (unsigned)nrchild;
}

/* function: keylen_trienode
 * Returns keylen calculated from flags in <header_t> and optional <trie_node_t.keylen>. */
static inline uint8_t keylen_trienode(const trie_node_t * node)
{
   unsigned keylen = keylen_header(node->header);
   return (uint8_t) (keylen == header_KEYLENBYTE ? node->keylen : keylen);
}

/* function: keylenoff_trienode
 * Calculates length of key from two adjacent offsets instead of decoding it from header. */
static inline unsigned keylenoff_trienode(unsigned off2_key, unsigned off3_digit)
{
   return off3_digit - off2_key;
}

/* function: subnode_trienode
 * Returns the pointer to the <trie_subnode_t>.
 *
 * Precondition:
 * The return value is only valid if <issubnode_trienode> returns true. */
static inline trie_subnode_t * subnode_trienode(trie_node_t * node, unsigned off4_child)
{
   return *(void**) (memaddr_trienode(node) + off4_child);
}

/* function: value_trienode
 * Returned value is only valid if node contains a value. */
static inline void * value_trienode(trie_node_t * node, unsigned off5_value)
{
   return *(void**) (memaddr_trienode(node) + off5_value);
}

static inline unsigned childoff4_trienode(const trie_node_t * node)
{
   uint8_t keylen = keylen_trienode(node);
   unsigned  off2 = off2_key_trienode(needkeylenbyte_header(keylen));
   unsigned  off3 = off3_digit_trienode(off2, keylen);
   return off4_child_trienode(off3, digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
}

/* function: findchild_trienode
 * Searches in digits array for digit.
 * The found index is returned in childidx.
 * A return value of true indicates that digit is found
 * else the digit is not found and childidx contains the index
 * where digit should be inserted. */
static inline int findchild_trienode(uint8_t digit, uint8_t nrchild, const uint8_t digits[nrchild], /*out*/uint8_t * childidx)
{
   unsigned high   = nrchild;
   unsigned low    = 0;
   unsigned middle = high >> 1;

   while (high > low) {
      if (digit == digits[middle]) {
         *childidx = (uint8_t) middle;
         return true;
      } else if (digit < digits[middle])
         high = middle;
      else
         low = middle+1;
      middle = (high+low) >> 1;
   }

   *childidx = (uint8_t) high;
   return false;
}

// group: change-helper

/* function: subnode_trienode
 * Sets the pointer to <trie_subnode_t>.
 * Call this function onlyif you know that the node contains space for a subnode. */
static inline void setsubnode_trienode(trie_node_t * node, unsigned off4_child, trie_subnode_t * subnode)
{
   *(void**) (memaddr_trienode(node) + off4_child) = subnode;
}

/* function: setvalue_trienode
 * The set value does not overwrite any other if the node contains a value. */
static inline void setvalue_trienode(trie_node_t * node, unsigned off5_value, void * value)
{
   *(void**) (memaddr_trienode(node) + off5_value) = value;
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

/* function: shrinknode_trienode
 * Allocates a new node of at least size newsize.
 * Only <trie_node_t.header> of data is initialized to the correct value.
 * All other fields of data must be set by the caller.
 *
 * Unchecked Precondition:
 * - oldsize == nodesize_trienode(node)
 * - oldsize >  MINSIZE
 * - newsize <= oldsize/2
 * */
static inline int shrinknode_trienode(
   /*out*/trie_node_t ** data,
   const header_t       nodeheader,
   unsigned             oldsize,
   unsigned             newsize)
{
   int err;
   header_t sizeflags    = sizeflags_header(nodeheader);
   unsigned shrunkensize = oldsize;

   if (newsize < MINSIZE) newsize = MINSIZE;

   do {
      shrunkensize /= 2;
      -- sizeflags;
   } while (shrunkensize/2 >= newsize);

   err = allocmemory_trienode(data, shrunkensize);
   if (err) return err;

   // only size flag is adapted
   (*data)->header = encodesizeflag_header(nodeheader, sizeflags);

   return 0;
}

/* function: expandnode_trienode
 * Allocates a new node of at least size newnode.
 * Only <trie_node_t.header> of data is initialized to the correct value.
 * All other fields of data must be set by the caller.
 *
 * Unchecked Precondition:
 * - oldsize == nodesize_trienode(node)
 * - oldsize < newsize
 * - newsize <= MAXSIZE
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

   // only size flag is adapted
   (*data)->header = encodesizeflag_header(nodeheader, sizeflags);

   return 0;
}

/* function: addsubnode_trienode
 * Moves all pointers in child[] array to subnode.
 * Then all pointers in child[] and bytes in digit[]
 * are removed and a single pointer to subnode
 * is stored in node.
 *
 * The node is resized to a smaller size if the new size plus reservebytes
 * allows it.
 *
 * Unchecked Precondition:
 * - The node contains no subnode
 * - nrchild_trienode(node) >= 1
 * - (*trienode)->nrchild * (1+sizeof(trie_node_t*)) - sizeof(void*) >= reservebytes
 * */
static int addsubnode_trienode(trie_node_t ** trienode, unsigned off3_digit, uint16_t reservebytes)
{
   int err;
   trie_node_t * node = *trienode;
   trie_subnode_t * subnode;

   err = new_triesubnode(&subnode);
   if (err) return err;

   unsigned off4_child    = off4_child_trienode(off3_digit, digitsize_trienode(0, nrchild_trienode(node)));
   unsigned dst_off4child = off4_child_trienode(off3_digit, 0);
   unsigned valuesize     = valuesize_trienode(isvalue_trienode(node));

   unsigned oldsize = nodesize_trienode(node);
   unsigned newsize = off4_child_trienode(off3_digit, reservebytes) + valuesize + childsize_trienode(1, nrchild_trienode(node));
   trie_node_t * newnode = node;
   if (newsize <= oldsize/2 && oldsize > MINSIZE) {
      err = shrinknode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) goto ONABORT;
      memcpy( memaddr_trienode(newnode) + sizeof(header_t),
              memaddr_trienode(node) + sizeof(header_t),
              off3_digit - sizeof(header_t));
   }

   // copy child array into subnode
   const uint8_t   nrchild = nrchild_trienode(node);
   const uint8_t * digits  = digits_trienode(node, off3_digit);
   trie_node_t  ** childs  = childs_trienode(node, off4_child);
   for (uint8_t i = 0; i < nrchild; ++i) {
      setchild_triesubnode(subnode, digits[i], childs[i]);
   }

   // remove digit / child arrays from node
   memmove(memaddr_trienode(newnode) + off5_value_trienode(dst_off4child, childsize_trienode(1, nrchild_trienode(node))),
           memaddr_trienode(node)    + off5_value_trienode(off4_child, childsize_trienode(0, nrchild_trienode(node))),
           valuesize);
   setsubnode_trienode(newnode, dst_off4child, subnode);

   addheaderflag_trienode(newnode, header_SUBNODE);
   -- newnode->nrchild;

   if (newnode != node) {
      (void) freememory_trienode(node, oldsize);
      // adapt inout param
      *trienode = newnode;
   }

   return 0;
ONABORT:
   (void) delete_triesubnode(&subnode);
   return err;
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
   uint8_t  nrchild       = (uint8_t) (node->nrchild + 1);
   unsigned src_off4child = off4_child_trienode(off3_digit, digitsize_trienode(1, nrchild));
   unsigned dst_off4child = off4_child_trienode(off3_digit, digitsize_trienode(0, nrchild));
   unsigned valuesize     = valuesize_trienode(isvalue_trienode(node));
   unsigned dst_off5value = off5_value_trienode(dst_off4child, childsize_trienode(0, nrchild));
   unsigned newsize       = off6_size_trienode(dst_off5value, valuesize);

   if (newsize > MAXSIZE || nrchild == 0/*overflow*/) return EINVAL;

   trie_subnode_t * subnode = subnode_trienode(node, src_off4child);

   // make room for digit / child arrays
   unsigned oldsize = nodesize_trienode(node);
   if (newsize > oldsize) {
      trie_node_t * newnode;
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;
      memcpy(memaddr_trienode(newnode) + sizeof(node->header),
             memaddr_trienode(node)    + sizeof(node->header),
             off3_digit - sizeof(node->header));
      memcpy(memaddr_trienode(newnode) + dst_off5value,
             memaddr_trienode(node)    + off5_value_trienode(src_off4child, childsize_trienode(1, nrchild)),
             valuesize);

      (void) freememory_trienode(node, oldsize);
      node = newnode;

      // adapt inout param
      *trienode = node;

   } else {
      memmove(memaddr_trienode(node) + dst_off5value,
              memaddr_trienode(node) + off5_value_trienode(src_off4child, childsize_trienode(1, nrchild)),
              valuesize);
   }

   delheaderflag_trienode(node, header_SUBNODE);
   ++ node->nrchild;

   // copy childs
   uint8_t      * digits = digits_trienode(node, off3_digit);
   trie_node_t ** childs = childs_trienode(node, dst_off4child);
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

/* function: delvalue_trienode
 * Removes the value from the node.
 *
 * Unchecked Precondition:
 * - The node has a value.
 */
static inline void delvalue_trienode(trie_node_t * node)
{
   delheaderflag_trienode(node, header_VALUE);
}

/* function: tryaddvalue_trienode
 * Adds a value to the node.
 * The node is resized if it is necessary.
 * The unlogged error value EINVAL is returned
 * if the node size would be bigger than <MAXSIZE>.
 *
 * Unchecked Precondition:
 * - The node has no value */
static int tryaddvalue_trienode(trie_node_t ** trienode, unsigned off4_child, void * value)
{
   int err;
   trie_node_t *  node       = *trienode;
   const unsigned childsize  = childsize_trienode(issubnode_trienode(node), nrchild_trienode(node));
   const unsigned off5_value = off5_value_trienode(off4_child, childsize);
   const unsigned newsize    = off6_size_trienode(off5_value, valuesize_trienode(true));

   if (MAXSIZE < newsize) return EINVAL;

   unsigned  oldsize = nodesize_trienode(node);

   if (oldsize < newsize) {
      trie_node_t * newnode;
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;

      memcpy( memaddr_trienode(newnode) + sizeof(node->header),
              memaddr_trienode(node)    + sizeof(node->header),
              off5_value - sizeof(node->header));

      (void) freememory_trienode(node, oldsize);
      node = newnode;

      // adapt inout param
      *trienode = newnode;
   }

   addheaderflag_trienode(node, header_VALUE);
   setvalue_trienode(node, off5_value, value);

   return 0;
 }

/* function: delkeyprefix_trienode
 * The first prefixkeylen bytes of key in node are removed.
 *
 * The node is resized to a smaller size if the new size plus reservebytes
 * allows it.
 *
 * Unchecked Precondition:
 * - keylen_trienode(node) >= prefixkeylen
 * - prefixkeylen >= reservebytes
 * */
static int delkeyprefix_trienode(trie_node_t ** trienode, unsigned off2_key, unsigned off3_digit, uint8_t prefixkeylen, uint16_t reservebytes)
{
   int err;
   trie_node_t * node = *trienode;
   unsigned dst_keylen   = keylenoff_trienode(off2_key, off3_digit) - prefixkeylen;
   unsigned dst_keyoff   = off2_key_trienode(needkeylenbyte_header((uint8_t)dst_keylen));
   unsigned dst_digitoff = off3_digit_trienode(dst_keyoff, dst_keylen);
   int      issubnode    = issubnode_trienode(node);
   unsigned digitsize    = digitsize_trienode(issubnode, nrchild_trienode(node));
   unsigned src_off4child = off4_child_trienode(off3_digit, digitsize);
   unsigned dst_off4child = off4_child_trienode(dst_digitoff, digitsize);
   unsigned valuesize    = valuesize_trienode(isvalue_trienode(node));
   unsigned childsize    = childsize_trienode(issubnode, nrchild_trienode(node));

   unsigned oldsize = nodesize_trienode(node);
   unsigned newsize = off4_child_trienode(dst_digitoff, digitsize+reservebytes) + valuesize + childsize;
   trie_node_t * newnode = node;
   if (newsize <= oldsize/2 && oldsize > MINSIZE) {
      err = shrinknode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) goto ONABORT;
      newnode->nrchild = node->nrchild;
   }

   encodekeylen_trienode(newnode, (uint8_t)dst_keylen);
   // copy key + digit array !!
   memmove( memaddr_trienode(newnode) + dst_keyoff, memaddr_trienode(node) + off2_key + prefixkeylen, dst_keylen + digitsize);
   // copy child array or subnode + value
   memmove( memaddr_trienode(newnode) + dst_off4child, memaddr_trienode(node) + src_off4child, childsize + valuesize);

   if (newnode != node) {
      (void) freememory_trienode(node, oldsize);
      // adapt inout param
      *trienode = newnode;
   }

   return 0;
ONABORT:
   return err;
}

/* function: tryaddkeyprefix_trienode
 * The key[prefixkeylen] is prepended to the key stored in node.
 * If node is resized the child pointer of the parent node has to be adapted!
 *
 * Returns EINVAL if a prefix of size prefixkeylen does not fit into the node
 * even after having been resized or if the length of key is > 255.
 * */
static int tryaddkeyprefix_trienode(trie_node_t ** trienode, unsigned off2_key, unsigned off3_digit, uint8_t prefixkeylen, const uint8_t key[prefixkeylen])
{
   int err;

   trie_node_t  * node = *trienode;
   unsigned dst_keylen = keylenoff_trienode(off2_key, off3_digit) + prefixkeylen;

   if (255 < dst_keylen) return EINVAL;

   unsigned dst_keyoff  = off2_key_trienode(needkeylenbyte_header((uint8_t)dst_keylen));
   unsigned dst_digitoff= off3_digit_trienode(dst_keyoff, dst_keylen);
   int      issubnode   = issubnode_trienode(node);
   unsigned digitsize   = digitsize_trienode(issubnode, nrchild_trienode(node));
   unsigned src_off4child = off4_child_trienode(off3_digit, digitsize);
   unsigned dst_off4child = off4_child_trienode(dst_digitoff, digitsize);
   unsigned ptrsize     = childsize_trienode(issubnode, nrchild_trienode(node))
                        + valuesize_trienode(isvalue_trienode(node));

   unsigned newsize = dst_off4child + ptrsize;
   if (newsize > MAXSIZE) return EINVAL;

   unsigned      oldsize = nodesize_trienode(node);
   trie_node_t * newnode = node;
   if (oldsize < newsize) {
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;
      newnode->nrchild = nrchild_trienode(node);

      // adapt inout param
      *trienode = newnode;
   }

   // copy content
   memmove( memaddr_trienode(newnode) + dst_off4child, memaddr_trienode(node) + src_off4child, ptrsize);
   memmove( memaddr_trienode(newnode) + dst_digitoff, memaddr_trienode(node) + off3_digit, digitsize);
   memmove( memaddr_trienode(newnode) + dst_keyoff + prefixkeylen, memaddr_trienode(node) + off2_key, dst_keylen - prefixkeylen);
   memcpy( memaddr_trienode(newnode) + dst_keyoff, key, prefixkeylen);

   // was node expanded ?
   if (newnode != node) {
      (void) freememory_trienode(node, oldsize);
      node = newnode;
   }

   encodekeylen_trienode(node, (uint8_t)dst_keylen);

   return 0;
}

/* function: tryaddchild_trienode
 * Insert new child into node at position childidx.
 * The value of childidx lies between 0 and <nrchild_trienode> both inclusive.
 *
 *
 * Unchecked Precondition:
 * - ! issubnode_trienode(node)
 * - nrchild_trienode(node) < 255
 * - childidx <= nrchild_trienode(node)
 * */
static int tryaddchild_trienode(trie_node_t ** trienode, unsigned off3_digit, unsigned off4_child, uint8_t childidx/*0 - nrchild*/, uint8_t digit, trie_node_t * child)
{
   int err;
   trie_node_t * node   = *trienode;
   unsigned valuesize   = valuesize_trienode(isvalue_trienode(node));
   unsigned newnrchild  = nrchild_trienode(node) + 1u;
   unsigned dst_off4child = off4_child_trienode(off3_digit, digitsize_trienode(0, (uint8_t)newnrchild));
   unsigned ptrsize     = childsize_trienode(0, (uint8_t) newnrchild) + valuesize;
   unsigned newsize     = dst_off4child + ptrsize;

   if (MAXSIZE < newsize) return EINVAL;

   unsigned      oldsize = nodesize_trienode(node);
   trie_node_t * newnode = node;
   if (oldsize < newsize) {
      err = expandnode_trienode(&newnode, node->header, oldsize, newsize);
      if (err) return err;
      memcpy( memaddr_trienode(newnode) + sizeof(newnode->header),
              memaddr_trienode(node) + sizeof(newnode->header),
              off3_digit + childidx - sizeof(newnode->header));

      // adapt inout param
      *trienode = newnode;
   }

   // make room in child and digit arrays
   unsigned insoffset = childsize_trienode(0, childidx);
   // child array after insoffset + value
   unsigned dst_insoff = dst_off4child + insoffset;
   memmove( memaddr_trienode(newnode) + dst_insoff + sizeof(trie_node_t*),
            memaddr_trienode(node)  + off4_child + insoffset,
            newsize  - dst_insoff - sizeof(trie_node_t*));
   *(trie_node_t**)(memaddr_trienode(newnode) + dst_insoff) = child;
   // child array before insoffset
   memmove( memaddr_trienode(newnode) + dst_off4child, memaddr_trienode(node) + off4_child, insoffset);
   // digit array after childidx
   unsigned digitoff = off3_digit + childidx;
   memmove( memaddr_trienode(newnode) + digitoff + sizeof(uint8_t),
            memaddr_trienode(node)  + digitoff,
            digitsize_trienode(0, nrchild_trienode(node)) - childidx);
   memaddr_trienode(newnode)[digitoff] = digit;

   if (newnode != node) {
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
static void delchild_trienode(trie_node_t ** trienode, unsigned off3_digit, unsigned off4_child, uint8_t childidx/*0 - nrchild-1*/)
{
   int err;
   trie_node_t * node = *trienode;
   unsigned valuesize   = valuesize_trienode(isvalue_trienode(node));
   unsigned newnrchild  = -- node->nrchild;
   unsigned dst_digitsize = digitsize_trienode(0, (uint8_t) newnrchild);
   unsigned dst_off4child = off4_child_trienode(off3_digit, dst_digitsize);
   unsigned ptrsize     = childsize_trienode(0, (uint8_t) newnrchild) + valuesize;
   unsigned newsize     = dst_off4child + ptrsize;

   unsigned      oldsize = nodesize_trienode(node);
   trie_node_t * newnode = node;
   if (newsize <= oldsize/2 && oldsize > MINSIZE) {
      err = shrinknode_trienode(&newnode, node->header, oldsize, newsize);
      // ignore any error
      if (!err) {
         memcpy( memaddr_trienode(newnode) + sizeof(newnode->header),
                 memaddr_trienode(node) + sizeof(newnode->header),
                 off3_digit + childidx - sizeof(newnode->header));

         // adapt inout param
         *trienode = newnode;
      }
   }

   // remove entries in child and digit arrays
   unsigned  deloffset = childsize_trienode(0, childidx);
   // digit array after childidx
   memmove( memaddr_trienode(newnode) + off3_digit + childidx,
            memaddr_trienode(node) + off3_digit + childidx + 1,
            dst_digitsize - childidx);
   // child array before deloffset
   memmove( memaddr_trienode(newnode) + dst_off4child,
            memaddr_trienode(node) + off4_child, deloffset);
   // child array after deloffset + value
   unsigned dst_deloff = dst_off4child + deloffset;
   memmove( memaddr_trienode(newnode) + dst_deloff,
            memaddr_trienode(node) + off4_child + deloffset + sizeof(trie_node_t*),
            newsize - dst_deloff);

   if (newnode != node) {
      (void) freememory_trienode(node, oldsize);
   }
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
         trie_subnode_t * subnode = subnode_trienode(delnode, childoff4_trienode(delnode));
         err = delete_triesubnode(&subnode);
      }

      err2 = freememory_trienode(delnode, nodesize_trienode(delnode));
      if (err2) err = err2;

      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   return err;
}

/* function: new_trienode
 * Initializes node and reserves space in a newly allocated <trie_node_t>.
 * The content of the node (value, child pointers (+ digits) and key bytes)
 * is undefined after return.
 *
 * The reserved keylen will be shrunk if a node of size <MAXSIZE> can not hold the
 * whole key. Therefore check the length of the reserved key after return.
 *
 * The child array is either stored in the node or a subnode is allocated if
 * nrchild is bigger than <MAXNROFCHILD>.
 *
 * Parameter nrchild:
 * The parameter nrchild can encode only 255 child pointers.
 * A subnode supports up to 256 child pointers so you have to increment
 * by one after return.
 * In case of a subnode the value of <trie_node_t.nrchild> is one less
 * then the provided value in parameter nrchild.
 *
 * Unchecked Precondition:
 * - The digit array must be sorted in ascending order
 *   (x < y && 0 <= x && y < nrchild) ==> digit[x] < digit[y]
 * */
static int new_trienode(
   /*out*/trie_node_t** node,
   uint8_t        keylen,
   uint8_t        nrchild,
   const uint8_t  key[keylen],
   const uint8_t  digit[nrchild],
   trie_node_t *  child[nrchild],
   void **        value)
{
   int err;
   unsigned          size = off1_keylen_trienode();
   trie_subnode_t *  subnode = 0;

   size += valuesize_trienode(value != 0);
   size += keylen + needkeylenbyte_header(keylen);

   if (nrchild > MAXNROFCHILD) {
      size += childsize_trienode(1, nrchild);
      if (size > MAXSIZE) return EINVAL;

      err = new_triesubnode(&subnode);
      if (err) goto ONABORT;

      for (unsigned i = 0; i < nrchild; ++i) {
         subnode->child[digit[i]] = child[i];
      }

      -- nrchild; // subnode encodes one less
   } else {
      size += digitsize_trienode(0, nrchild) + childsize_trienode(0, nrchild);
      if (size > MAXSIZE) return EINVAL;
   }

   header_t header;
   unsigned nodesize;

   if (size > MAXSIZE / 8) {
      // grow nodesize
      nodesize = MAXSIZE / 4;
      for (header = header_SIZEMAX-2; nodesize < size; ++header) {
         nodesize *= 2;
      }

   } else {
      // shrink nodesize
      nodesize = MAXSIZE / 8;
      for (header = header_SIZEMAX-3; nodesize/2 >= size && header > header_SIZE0; --header) {
         nodesize /= 2;
      }
   }

   header = (header_t) (header << header_SIZESHIFT);

   trie_node_t * newnode;
   err = allocmemory_trienode(&newnode, nodesize);
   if (err) goto ONABORT;

   header = addflags_header(header, (value != 0) ? header_VALUE : 0);
   header = addflags_header(header, subnode ? header_SUBNODE : 0);

   unsigned offset = off1_keylen_trienode();

   if (needkeylenbyte_header(keylen)) {
      header = encodekeylenbyte_header(header);
      newnode->keylen = keylen;
      ++ offset;
   } else {
      header = encodekeylen_header(header, keylen);
   }

   newnode->header  = header;
   newnode->nrchild = nrchild;

   // off2_key == offset;
   if (keylen) memcpy(memaddr_trienode(newnode) + offset, key, keylen);
   offset += keylen;

   // off3_digit == offset;
   if (subnode) {
      offset = off4_child_trienode(offset, digitsize_trienode(1, nrchild));
      setsubnode_trienode(newnode, offset, subnode);
      offset = off5_value_trienode(offset, childsize_trienode(1, nrchild));

   } else {
      uint8_t * dst_digit = digits_trienode(newnode, offset);
      memcpy(dst_digit, digit, digitsize_trienode(0, nrchild));
      offset = off4_child_trienode(offset, digitsize_trienode(0, nrchild));
      trie_node_t ** dst_child = childs_trienode(newnode, offset);
      unsigned childsize = childsize_trienode(0, nrchild);
      offset = off5_value_trienode(offset, childsize);
      memcpy(dst_child, child, childsize);
   }

   if (value != 0) {
      setvalue_trienode(newnode, offset, *value);
   }

   // set out param
   *node = newnode;

   return 0;
ONABORT:
   (void) delete_triesubnode(&subnode);
   return err;
}


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
            trie_node_t ** childs = childs_trienode(delnode, childoff4_trienode(delnode));
            for (unsigned i = 0; i < delnode->nrchild; ++i) {
               if (childs[i]) {
                  firstchild = childs[i];
                  ((uint8_t*)childs)[-1] = (uint8_t) i;  // save last index
                                                         // overwrite possible value or digit array
                                                         // nrchild is used in offset calculation
                  childs[0] = parent;
                  break;
               }
            }

         } else {
            trie_subnode_t * subnode = subnode_trienode(delnode, childoff4_trienode(delnode));
            for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  firstchild = subnode->child[i];
                  delnode->nrchild = (uint8_t)i; // save last index (nrchild not used in offset calc due to subnode)
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
            trie_node_t ** childs = childs_trienode(delnode, childoff4_trienode(delnode));
            for (unsigned i = 1u+((uint8_t*)childs)[-1]; i < delnode->nrchild; ++i) {
               if (childs[i]) {
                  delnode = childs[i];
                  ((uint8_t*)childs)[-1] = (uint8_t) i;
                  break;
               }
            }
            if (delnode != parent) break; // another child ==> repeat step 1
            parent = childs[0];

         } else {
            trie_subnode_t * subnode = subnode_trienode(delnode, childoff4_trienode(delnode));
            for (unsigned i = 1u+delnode->nrchild/*restore last*/; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  delnode->nrchild = (uint8_t)i; // save last index
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

// group: private-update

/* function: restructnode_trie
 * The function restructures a node to make space for a value or a new child.
 * The noe is resized to a smaller size if possible.
 *
 * Parameter parentchild must point to the entry of the child array or trie_subnode_t
 * from which the value *trienode was read.
 *
 * If the key stored in trienode is extracted into a new parent node or if trienode
 * is resized after the child array has been extracted into its own trie_subnode_t
 * the (former) parent node of trienode must be adapted.
 * To automate this task parameter *parentchild is set to point to either the
 * newly created parent node or (resized or not) trienode.
 *
 * After return trienode points to the resized node or else is not changed.
 *
 * The parameter isChild determines the number of bytes reserved in trienode after
 * restructuring. If isChild is false sizeof(void*) bytes are reserved for a value
 * else (in case of true) either sizeof(trie_node_t*)+sizeof(uint8_t) are reserved
 * (if key is extracted) or 0 (if child array is converted into a <trie_subnode_t>).
 *
 * Parameter off3_digit and off4_child contains new values which are adapted to the new
 * structure of the node.
 *
 * Unchecked Preconditions:
 * - nodesize_trienode(node) == MAXSIZE
 * */
static int restructnode_trie(trie_node_t ** trienode, /*out*/trie_node_t ** parentchild, bool isChild, unsigned off2_key, /*inout*/unsigned * off3_digit, /*inout*/unsigned * off4_child)
{
   int err;
   trie_node_t * parent;
   trie_node_t * node    = *trienode;
   unsigned      keylen  = keylenoff_trienode(off2_key, *off3_digit);

   if (keylen < 2*sizeof(trie_node_t*)) {    // == convert child array into subnode
      if (issubnode_trienode(node)) return EINVAL;

      err = addsubnode_trienode(&node, *off3_digit, isChild ? 0 : sizeof(void*));
      if (err) return err;
      *off4_child = off4_child_trienode(*off3_digit, digitsize_trienode(1, nrchild_trienode(node)));

      parent = 0;

   } else {                            // == extract key
      err = new_trienode(  &parent, (uint8_t) (keylen-1), 1, memaddr_trienode(node) + off2_key,
                           memaddr_trienode(node) + off2_key + keylen-1, &node, 0);
      if (err) return err;

      trie_node_t * oldnode = node;
      err = delkeyprefix_trienode(&node, off2_key, *off3_digit, (uint8_t) keylen, isChild ? sizeof(trie_node_t*)+sizeof(uint8_t) : sizeof(void*));
      if (err) goto ONABORT;
      *off3_digit = off3_digit_trienode(off2_key_trienode(0), 0);
      *off4_child = off4_child_trienode(
                              off3_digit_trienode(off2_key_trienode(0), 0),
                              digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));

      if (oldnode != node) {
         childs_trienode(parent, off4_child_trienode( off3_digit_trienode(off2_key_trienode(needkeylenbyte_header((uint8_t)(keylen-1))), (uint8_t)(keylen-1)),
                                                      digitsize_trienode(0, 1))
                        )[0] = node;
      }
   }

   // set out parameter
   // off3_digit and off4_child are already set
   *trienode    = node;
   *parentchild = parent ? parent : node;

   return 0;
ONABORT:
   delete_trienode(&parent);
   return err;
}

/* function: build_nodechain_trienode
 * Creates one or more nodes which holds the whole key.
 * The last node of the node chain contains the value.
 * A node can store a key part of up to <MAXKEYLEN> bytes.
 * The head of the chain is returned in node.
 * */
static int build_nodechain_trienode(/*out*/trie_node_t ** node, uint16_t keylen, const uint8_t key[keylen], void * value)
{
   int err;
   unsigned    offset = keylen;
   trie_node_t * head;

   uint8_t splitlen = splitkeylen_trienode(keylen);
   offset -= splitlen;

   // build last node in chain first
   err = new_trienode(&head, splitlen, 0, key + offset, 0, 0, &value);
   if (err) return err;

   // build chain of nodes
   while (offset) {
      splitlen = splitkeylen_trienode((uint16_t) offset);
      offset  -= splitlen;
      err = new_trienode(&head, (uint8_t) (splitlen-1), 1, key + offset, key+offset+splitlen-1/*digits*/, &head/*childs*/, 0);
      if (err) goto ONABORT;
   }

   // set out
   *node = head;

   return 0;
ONABORT: ;
   trie_t undotrie = trie_INIT2(head);
   (void) free_trie(&undotrie);
   return err;
}

/* function: build_splitnode_trienode
 * Creates one or two nodes.
 * In case of one node splitparent contains the same value
 * as splitnode. In case of two nodes splitparent is the
 * parent of splitnode.
 *
 * The function creates a possible parent node with a key and a single child pointer
 * to splitnode. It creates also a sliptnode which contains two child pointer or
 * a single child pointer and a value. The configuration is determined by parameter value.
 * If value is 0 splitnode contains two childs. If value is != 0 splkitnode contains a single
 * child and a value.
 *
 * Unchecked Preconditions:
 * - (value != 0)  ==> digit[0] and child[0] are valid
 * - (value == 0)  ==> digit[0], digit[1] and child[0], child[1] are valid
 * */
static inline int build_splitnode_trienode(
   /*out*/trie_node_t ** splitnode,
   /*out*/trie_node_t *** childs,
   uint8_t        keylen,
   const uint8_t  key[keylen],
   const uint8_t  digit[/*1 or 2*/],
   trie_node_t *  child[/*1 or 2*/],
   void       **  value)
{
   int err;

#define NODE_MAXKEYLEN  (COMPUTEKEYLEN(128) - sizeof(trie_node_t*) - 2*sizeof(uint8_t))

   if (keylen > NODE_MAXKEYLEN) {
      unsigned node_keylen  = (keylen < COMPUTEKEYLEN(128) ? keylen : COMPUTEKEYLEN(128));
      unsigned child_keylen = keylen - node_keylen;
      trie_node_t * splitchild;
      err = new_trienode(&splitchild, (uint8_t)child_keylen, value ? 1 : 2, key + node_keylen, digit, child, value);
      if (err) return err;
      err = new_trienode(splitnode, (uint8_t)(node_keylen-1), 1, key, key + node_keylen-1, &splitchild, 0);
      if (err) {
         (void) delete_trienode(&splitchild);
         return err;
      }
      unsigned off4_child = off4_child_trienode(
                              off3_digit_trienode(off2_key_trienode(needkeylenbyte_header((uint8_t)child_keylen)), child_keylen),
                              digitsize_trienode(0, (value != 0 ? 1 : 2)));
      *childs = childs_trienode(splitchild, off4_child);

   } else {
      err = new_trienode(splitnode, keylen, value ? 1 : 2, key, digit, child, value);
      if (err) return err;
      unsigned off4_child = off4_child_trienode(
                              off3_digit_trienode(off2_key_trienode(needkeylenbyte_header(keylen)), keylen),
                              digitsize_trienode(0, (value != 0 ? 1 : 2)));
      *childs = childs_trienode(*splitnode, off4_child);
   }

   return 0;
}

/* function: insert2_trie
 * Implements <insert_trie> and <tryinsert_trie>.
 *
 * Description:
 *
 * Searches from root node to the correct node for insertion.
 * If a node is found which matches the full key either value is inserted
 * or EEXIST is returned if the node contains an already inserted value.
 *
 * If only a prefix of the key of the found node matches a split node parent
 * is created and the found node is marked for key prefix deletion.
 *
 * Now a new node is created which contains the unmatched part of the key
 * and the value. If the unmatched key part is too big a whole chain of nodes
 * is created where the root node of the chain becomes the new node.
 *
 * The new node is inserted into the found node or the split parent.
 * If the new child pointer does not fit into the found node (split parent always works)
 * the found node is transformed (the key is extracted itno its own node or a child array
 * is converted into a <trie_subnode_t>) and then the child pointer is inserted.
 *
 * After that the parent node of the found node is adapted to point to either a split parent
 * node or a transformed found node else it is not changed.
 *
 * If the found node was marked for prefix deletion it is deleted as last (no error possible).
 * */
int insert2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * value, bool islog)
{
   int err;
   trie_node_t  * node;        // node marks the current position in the trie
   trie_node_t ** parentchild; // points to child pointer (child array, subnode, or root) where node was read from
   trie_node_t  * child = 0;   // created node (chain) containing value which will be added to node
   unsigned       matched_keylen = 0;

   node = trie->root;

   if (!node) {
      /* empty root */
      err = build_nodechain_trienode(&trie->root, keylen, key, value);
      if (err) goto ONABORT;

   } else {
      parentchild = &trie->root;

      for (;;) {  // follow node path from root to matching child
         uint8_t  node_keylen = keylen_trienode(node);
         unsigned off2_key    = off2_key_trienode(needkeylenbyte_header(node_keylen));
         unsigned off3_digit  = off3_digit_trienode(off2_key, node_keylen);

         // match key fully or split node in case of partial match
         if (  node_keylen + matched_keylen > keylen
               || 0 != memcmp(key+matched_keylen, memaddr_trienode(node) + off2_key, node_keylen)) {
            // partial match ==> split node
            unsigned splitkeylen;
            unsigned keylen2 = node_keylen > (keylen - matched_keylen)
                             ? (keylen - matched_keylen) : node_keylen;
            const uint8_t * lkey = key+matched_keylen;
            const uint8_t * rkey = memaddr_trienode(node) + off2_key;
            for (splitkeylen = 0; splitkeylen < keylen2; ++splitkeylen) {
               if (lkey[splitkeylen] != rkey[splitkeylen]) break;
            }
            matched_keylen += splitkeylen;
            trie_node_t ** splitnodechild;
            if (matched_keylen < keylen) {
               // splitnode has child pointer to node with value
               ++ matched_keylen;
               err = build_nodechain_trienode(&child, (uint16_t) (keylen - matched_keylen), key + matched_keylen, value);
               if (err) goto ONABORT;
               bool childidx = (lkey[splitkeylen] > rkey[splitkeylen]);
               uint8_t       digits[2];
               trie_node_t * childs[2];
               digits[childidx]  = lkey[splitkeylen];
               digits[!childidx] = rkey[splitkeylen];
               childs[childidx]  = child;
               childs[!childidx] = node;
               err = build_splitnode_trienode(&child, &splitnodechild, (uint8_t)splitkeylen, rkey, digits, childs, 0);
               if (err) goto ONABORT;
               splitnodechild += !childidx;

            } else {
               // splitnode contains value
               err = build_splitnode_trienode(&child, &splitnodechild, (uint8_t)splitkeylen, rkey, rkey+splitkeylen, &node, &value);
               if (err) goto ONABORT;
            }
            // assert (*splitnodechild == node);
            err = delkeyprefix_trienode(splitnodechild, off2_key, off3_digit, (uint8_t)(splitkeylen+1), 0);
            if (err) {
               *splitnodechild = 0; // do not delete node in error handling
               goto ONABORT;
            }
            *parentchild = child;
            return 0; // DONE
         }

         matched_keylen += node_keylen;

         int      issubnode = issubnode_trienode(node);
         unsigned off4_child = off4_child_trienode(off3_digit, digitsize_trienode(issubnode, nrchild_trienode(node)));

         if (matched_keylen == keylen) {
            // found node which matches full key ==> add value to existing node
            if (isvalue_trienode(node)) {
               err = EEXIST;
               goto ONABORT;
            }
            err = tryaddvalue_trienode(parentchild, off4_child, value);
            if (err) {
               if (err != EINVAL) goto ONABORT;
               err = restructnode_trie(&node, parentchild, false, off2_key, &off3_digit, &off4_child);
               if (err) goto ONABORT;
               // node will be not resized cause of reservedbytes==sizeof(void*)
               err = tryaddvalue_trienode(&node, off4_child, value);
               if (err) goto ONABORT;
            }
            return 0; // DONE
         }

         // follow path to next child (either child array or subnode)

         uint8_t digit = key[matched_keylen++];

         if (issubnode) {  /* subnode case */
            trie_subnode_t * subnode = subnode_trienode(node, off4_child);
            parentchild = childaddr_triesubnode(subnode, digit);

            if (! *parentchild) {
               // insert child into subnode
               err = build_nodechain_trienode(parentchild, (uint16_t) (keylen - matched_keylen), key + matched_keylen, value);
               if (err) goto ONABORT;
               ++ node->nrchild;
               return 0; // DONE
            }

         } else {          /* child array case */
            uint8_t * digits = digits_trienode(node, off3_digit);
            uint8_t   childidx;

            if (!findchild_trienode(digit, nrchild_trienode(node), digits, &childidx)) {
               // insert child into child array (childidx is index of insert position)
               err = build_nodechain_trienode(&child, (uint16_t) (keylen - matched_keylen), key + matched_keylen, value);
               if (err) goto ONABORT;
               err = tryaddchild_trienode(parentchild, off3_digit, off4_child, childidx, digit, child);
               if (err) {
                  if (err != EINVAL) goto ONABORT;
                  err = restructnode_trie(&node, parentchild, true, off2_key, &off3_digit, &off4_child);
                  if (err) goto ONABORT;
                  if (issubnode_trienode(node)) {
                     trie_subnode_t * subnode = subnode_trienode(node, off4_child);
                     setchild_triesubnode(subnode, digit, child);
                     ++ node->nrchild;
                  } else {
                     // node will be not resized cause of reservedbytes==sizeof(uint8_t)+sizeof(trie_node_t*)
                     err = tryaddchild_trienode(&node, off3_digit, off4_child, childidx, digit, child);
                     if (err) goto ONABORT;
                  }
               }
               return 0; // DONE
            }

            trie_node_t ** childs = childs_trienode(node, off4_child);
            parentchild = &childs[childidx];
         }
         // goto child node at depth + 1
         node = *parentchild;
      }
   }

   return 0;
ONABORT: ;
   if (child) {
      trie_t undotrie = trie_INIT2(child);
      (void) free_trie(&undotrie);
   }
   if (islog || err != EEXIST) {
      TRACEABORT_ERRLOG(err);
   }
   return err;
}

int remove2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** value, bool islog)
{
   int err;
   trie_node_t  * node;        // node marks the current position in the trie
   // chainroot is parent of chain of nodes with only one child
   trie_node_t ** chainrootchild; // points to child pointer (child array, subnode, or root) where node was read from
   trie_node_t ** parentchild; // points to child pointer (child array, subnode, or root) where node was read from
   trie_node_t ** chainroot_parentchild; // points to child pointer of parent node where chainroot was read from
   unsigned       matched_keylen;
   unsigned       chainroot_off3 = 0; // the index into the child array of chainroot
   unsigned       chainroot_off4 = 0; // the index into the child array of chainroot
   uint8_t        chainroot_childidx = 0; // the index into the child array of chainroot

   chainroot_parentchild = 0;
   chainrootchild = &trie->root;
   parentchild    = &trie->root;
   matched_keylen = 0;

   node = trie->root;

   for (;;) {
      if (!node) {
         err = ESRCH;
         goto ONABORT;
      }

      uint8_t  node_keylen = keylen_trienode(node);
      unsigned off2_key    = off2_key_trienode(needkeylenbyte_header(node_keylen));

      // match key
      if (  node_keylen + matched_keylen > keylen
            || 0 != memcmp(key+matched_keylen, memaddr_trienode(node) + off2_key, node_keylen)) {
         err = ESRCH;   // partial match
         goto ONABORT;
      }
      matched_keylen += node_keylen;

      int      issubnode  = issubnode_trienode(node);
      unsigned off3_digit = off3_digit_trienode(off2_key, node_keylen);
      unsigned off4_child = off4_child_trienode(off3_digit, digitsize_trienode(issubnode, nrchild_trienode(node)));

      if (matched_keylen == keylen) {
         // found node which matches full key
         if (! isvalue_trienode(node)) {
            err = ESRCH;
            goto ONABORT;
         }

         unsigned off5_value = off5_value_trienode(off4_child, childsize_trienode(issubnode, nrchild_trienode(node)));
         *value = value_trienode(node, off5_value);

         delvalue_trienode(node);

         if (!issubnode && 0 == nrchild_trienode(node)) {
            // delete node chain ?
            if (parentchild == chainrootchild) {
               (void) delete_trienode(chainrootchild);
            } else {
               trie_t nodechain = trie_INIT2(*chainrootchild);
               (void) free_trie(&nodechain);
               *chainrootchild = 0;
            }

            if (chainroot_parentchild) {
               if (issubnode_trienode(*chainroot_parentchild)) {
                  if (((*chainroot_parentchild)->nrchild--) < MAXNROFCHILD-2) {
                     // try to restructure subnode into child array !
                     (void) trydelsubnode_trienode(chainroot_parentchild, chainroot_off3);
                  }
               } else {
                  (void) delchild_trienode(chainroot_parentchild, chainroot_off3, chainroot_off4, chainroot_childidx);
               }
            }
         }

         return 0; // DONE
      }

      // follow path to next child (either child array or subnode)
      uint8_t digit = key[matched_keylen++];

      if (issubnode) {  /* subnode case */

         trie_subnode_t * subnode = subnode_trienode(node, off4_child);
         chainroot_parentchild = parentchild;
         chainrootchild = childaddr_triesubnode(subnode, digit);
         chainroot_off3 = off3_digit;
         // chainroot_off4,chainroot_childidx not used in subnode case of branch (matched_keylen == keylen)
         parentchild = chainrootchild;
         node = *parentchild;

      } else {          /* child array case */
         uint8_t * digits = digits_trienode(node, off3_digit);
         uint8_t   childidx;

         if (!findchild_trienode(digit, nrchild_trienode(node), digits, &childidx)) {
            err = ESRCH;
            goto ONABORT;
         }

         trie_node_t ** child = childs_trienode(node, off4_child) + childidx;

         if (nrchild_trienode(node) > 1 || isvalue_trienode(node)) {
            chainroot_parentchild = parentchild;
            chainrootchild = child;
            chainroot_off3 = off3_digit;
            chainroot_off4 = off4_child;
            chainroot_childidx = childidx;
         }

         parentchild = child;
         node = *parentchild;
      }

   }

   return 0;
ONABORT: ;
   if (islog || err != ESRCH) {
      TRACEABORT_ERRLOG(err);
   }
   return err;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

typedef struct nodeoffsets_t nodeoffsets_t;

struct nodeoffsets_t {
   unsigned off2_key;
   unsigned off3_digit;
   unsigned off4_child;
   unsigned off5_value;
   unsigned off6_size;
};

void init_nodeoffsets(/*out*/nodeoffsets_t * offsets, const trie_node_t * node)
{
   uint8_t  keylen = keylen_trienode(node);
   unsigned offset = off2_key_trienode(needkeylenbyte_header(keylen));
   offsets->off2_key = offset;
   offset = off3_digit_trienode(offset, keylen);
   offsets->off3_digit = offset;
   offset = off4_child_trienode(offset, digitsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
   offsets->off4_child = offset;
   offset = off5_value_trienode(offset, childsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
   offsets->off5_value = offset;
   offset = off6_size_trienode(offset, valuesize_trienode(isvalue_trienode(node)));
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

   // TEST isvalue_header
   TEST(0 == isvalue_header((header_t)~header_VALUE));
   TEST(0 != isvalue_header(header_VALUE));
   TEST(0 != isvalue_header((header_t)~0));

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

   // TEST new_triesubnode: ENOMEM
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_triesubnode(&subnode));
   TEST(0 == subnode);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST delete_triesubnode: EINVAL
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

   // TEST childaddr_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      subnode->child[i] = (trie_node_t*) (i+1);
   }
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      trie_node_t * child = (trie_node_t*) (i+1);
      TEST(0     != childaddr_triesubnode(subnode, (uint8_t)i));
      TEST(child == childaddr_triesubnode(subnode, (uint8_t)i)[0]);
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
   TEST(PTRALIGN == offsetof(trie_node_t, value));
   TEST(PTRALIGN <= sizeof(void*));
   TEST(0 != ispowerof2_int(PTRALIGN));

   // TEST MAXSIZE
   node->header = encodesizeflag_header(0, header_SIZEMAX);
   TEST(MAXSIZE == nodesize_trienode(node));

   // TEST MINSIZE
   node->header = encodesizeflag_header(0, header_SIZE0);
   TEST(MINSIZE == nodesize_trienode(node));

   // TEST MAXNROFCHILD
   static_assert( 32 < MAXNROFCHILD
                  && 64 > MAXNROFCHILD
                  && MAXSIZE/(sizeof(trie_node_t*)+1) > MAXNROFCHILD,
                  && MAXSIZE/(sizeof(trie_node_t*)+1) <= MAXNROFCHILD+2
                  "MAXNROFCHILD depends on MAXSIZE");
   {
      unsigned off6_size = off6_size_trienode(
                              off5_value_trienode(
                                 off4_child_trienode( off3_digit_trienode(off2_key_trienode(0), 0),
                                                      digitsize_trienode(0, MAXNROFCHILD)),
                                 childsize_trienode(0, MAXNROFCHILD)),
                              valuesize_trienode(1/*contains value*/));
      TEST(MAXSIZE >= off6_size);
      TEST(MAXSIZE <  off6_size + (1+sizeof(trie_node_t)));
   }

   // TEST COMPUTEKEYLEN
   for (unsigned i = MINSIZE; i <= MAXSIZE; ++i) {
      unsigned sizeused = off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
                        + valuesize_trienode(1)
                        - (off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
                           - off3_digit_trienode(off2_key_trienode(1), 0))/*unused alignment*/;
      unsigned keylen = i - sizeused;
      TEST(keylen == COMPUTEKEYLEN(i));
   }

   // TEST NOSPLITKEYLEN
   static_assert( NOSPLITKEYLEN > MINSIZE
                  && NOSPLITKEYLEN < 2*MINSIZE,
                  "keylen is stored unsplitted into a node of size <= header_SIZE1");
   TEST(1 == needkeylenbyte_header(NOSPLITKEYLEN));
   unsigned sizeused = off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
                     + valuesize_trienode(1)
                     - (off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
                        - off3_digit_trienode(off2_key_trienode(1), 0))/*unused alignment*/;
   unsigned nosplitkeylen = 2*MINSIZE;
   nosplitkeylen -= sizeused;
   TEST(NOSPLITKEYLEN == nosplitkeylen);

   // TEST MAXKEYLEN
   static_assert( MAXKEYLEN < 255 && MAXKEYLEN > 128,
                  "maximum keylen storable in a node; must be less than 255;");
   // compute MAXKEYLEN with offset functions
   sizeused = off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
            + valuesize_trienode(1)
            - (off4_child_trienode(off3_digit_trienode(off2_key_trienode(1), 0), 0)
               - off3_digit_trienode(off2_key_trienode(1), 0))/*unused alignment*/;
   unsigned maxkeylen = MAXSIZE;
   while (maxkeylen > 256) maxkeylen /= 2;   // nodes with size > 256 can not be used cause typeof(keylen) == uint8_t
   maxkeylen -= sizeused;
   TEST(MAXKEYLEN == maxkeylen);

   // == group: query-header ==

   // TEST issubnode_trienode
   node->header = header_SUBNODE;
   TEST(0 != issubnode_trienode(node));
   node->header = (header_t) ~ header_SUBNODE;
   TEST(0 == issubnode_trienode(node));
   node->header = 0;
   TEST(0 == issubnode_trienode(node));

   // TEST isvalue_trienode
   node->header = header_VALUE;
   TEST(0 != isvalue_trienode(node));
   node->header = (header_t) ~ header_VALUE;
   TEST(0 == isvalue_trienode(node));
   node->header = 0;
   TEST(0 == isvalue_trienode(node));

   // TEST nodesize_trienode
   node->header = header_SIZE0 << header_SIZESHIFT;
   TEST( 2*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZE1 << header_SIZESHIFT;
   TEST( 4*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZE2 << header_SIZESHIFT;
   TEST( 8*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZE3 << header_SIZESHIFT;
   TEST(16*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZE4 << header_SIZESHIFT;
   TEST(32*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZE5 << header_SIZESHIFT;
   TEST(64*sizeof(void*) == nodesize_trienode(node));
   node->header = header_SIZEMAX << header_SIZESHIFT;
   TEST(64*sizeof(void*) == nodesize_trienode(node));

   // == group: query-helper ==

   // TEST splitkeylen_trienode: returns unchanged value for <= NOSPLITKEYLEN
   for (uint16_t i = 0; i <= NOSPLITKEYLEN; ++i) {
      TEST(i == splitkeylen_trienode(i));
   }

   // TEST splitkeylen_trienode: returns MAXKEYLEN for values >= MAXKEYLEN
   for (uint16_t i = MAXKEYLEN; i >= MAXKEYLEN; ++i) {
      TEST(MAXKEYLEN == splitkeylen_trienode(i));
   }

   // TEST splitkeylen_trienode: adapt to memory efficient value
   for (uint16_t i = NOSPLITKEYLEN+1; i < MAXKEYLEN; ++i) {
      unsigned keylen = splitkeylen_trienode(i);
      TEST(keylen <= i);
      unsigned size;
      for (size = MINSIZE; size < MAXSIZE; size *= 2) {
         if (COMPUTEKEYLEN(size*2) > i) break;
      }
      // is it more memory efficient to split key into nodes of size and size/2 ?
      if (COMPUTEKEYLEN(size) + COMPUTEKEYLEN(size/2) < keylen) size *= 2;
      if (COMPUTEKEYLEN(size) >= i) {
         TEST(keylen == i);
      } else {
         TEST(keylen == COMPUTEKEYLEN(size));
      }
   }

   // TEST alignoffset_trienode
   TEST(0 == alignoffset_trienode(0));
   for (unsigned i = 0; i < 10; ++i) {
      unsigned offset = sizeof(void*)*i;
      for (unsigned b = 1; b <= sizeof(void*); ++b) {
         TEST(offset+sizeof(void*) == alignoffset_trienode(offset+b));
      }
   }

   // TEST valuesize_trienode
   TEST(sizeof(void*) == valuesize_trienode(1));
   TEST(sizeof(void*) == valuesize_trienode(header_VALUE));
   TEST(0 == valuesize_trienode(0));

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

   // TEST off4_child_trienode
   for (unsigned off3_digit = 0; off3_digit <= 512; ++off3_digit) {
      for (unsigned digitsize  = 0; digitsize <= 32; ++digitsize) {
         unsigned off4 = off4_child_trienode(off3_digit, digitsize);
         TEST(off4 >= off3_digit+digitsize);
         TEST(off4 <  off3_digit+digitsize+sizeof(void*));
         TEST(off4 % sizeof(void*) == 0);
      }
   }

   // TEST off5_value_trienode
   for (unsigned off4_child = 0; off4_child <= 512; ++off4_child) {
      for (unsigned childsize = 0; childsize <= 32; ++childsize) {
         TEST(off5_value_trienode(off4_child, childsize) == off4_child + childsize);
      }
   }

   // TEST off6_size_trienode
   for (unsigned off5_value = 0; off5_value <= 256; ++off5_value) {
      TEST(off6_size_trienode(off5_value, valuesize_trienode(1)) == off5_value + sizeof(void*));
      TEST(off6_size_trienode(off5_value, valuesize_trienode(0)) == off5_value);
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
   for (unsigned off4_child = 0; off4_child <= 512; ++off4_child) {
      TEST(childs_trienode(node, off4_child) == (trie_node_t**) ((uint8_t*)buffer + off4_child));
   }

   // TEST childsize_trienode
   for (unsigned nrchild = 0; nrchild <= 255; ++nrchild) {
      TEST(childsize_trienode(header_SUBNODE, (uint8_t) nrchild) == sizeof(void*));
      TEST(childsize_trienode(1, (uint8_t) nrchild) == sizeof(void*));
      TEST(childsize_trienode(0, (uint8_t) nrchild) == nrchild * sizeof(trie_node_t*));
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

   // TEST keylenoff_trienode
   for (unsigned off2_key = 0; off2_key <= 255; ++off2_key) {
      for (unsigned off3_digit = off2_key; off3_digit <= off2_key+255; ++off3_digit) {
         TEST(off3_digit - off2_key == keylenoff_trienode(off2_key, off3_digit));
      }
   }

   // TEST subnode_trienode
   for (unsigned off4_child = 0; off4_child < MAXSIZE; off4_child += sizeof(void*)) {
      buffer[off4_child/sizeof(void*)] = (void*)buffer;
      TEST(subnode_trienode(node, off4_child) == (trie_subnode_t*)buffer);
      buffer[off4_child/sizeof(void*)] = 0;
      TEST(subnode_trienode(node, off4_child) == 0);
   }

   // TEST value_trienode
   for (unsigned off5_value = 0; off5_value < MAXSIZE; off5_value += sizeof(void*)) {
      buffer[off5_value/sizeof(void*)] = buffer;
      TEST(value_trienode(node, off5_value) == (void*)buffer);
      buffer[off5_value/sizeof(void*)] = 0;
      TEST(value_trienode(node, off5_value) == 0);
   }

   // TEST childoff4_trienode
   for (int issubnode = 0; issubnode <= 1; ++issubnode) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         for (unsigned nrchild = 0; nrchild <= 255; ++nrchild) {
            node->header  = (header_t) (issubnode ? header_SUBNODE : 0);
            node->nrchild = (uint8_t) nrchild;
            encodekeylen_trienode(node, (uint8_t) keylen);
            unsigned expect = alignoffset_trienode(off2_key_trienode(needkeylenbyte_header((uint8_t)keylen))
                                                   + keylen + (issubnode ? 0 : nrchild));
            TEST(childoff4_trienode(node) == expect);
         }
      }
   }
   node->header  = 0;
   node->nrchild = 0;

   // TEST findchild_trienode: empty digit array
   uint8_t childidx = 1;
   memset(buffer, 0, sizeof(buffer));
   TEST(0 == findchild_trienode(0, 0, (const uint8_t*)buffer, &childidx));
   TEST(0 == childidx);

   // TEST findchild_trienode: no empty digit array
   for (uint8_t size = 1; size <= 16; ++size ) {
      uint8_t * digit = (uint8_t*) buffer;
      for (uint8_t first = 0; first <= 16; ++first) {
         for (uint8_t i = 0; i < size; ++i) {
            digit[i] = (uint8_t) (first + 3*i);
         }
         for (uint8_t i = 0; i < size; ++i) {
            childidx = (uint8_t) (i+1u);
            TEST(1 == findchild_trienode((uint8_t)(first+3*i), size, (const uint8_t*)buffer, &childidx));
            TEST(i == childidx);
            TEST(0 == findchild_trienode((uint8_t)(first+3*i+1), size, (const uint8_t*)buffer, &childidx));
            TEST(i == childidx-1);
            if (i || first) {
               TEST(0 == findchild_trienode((uint8_t)(first+3*i-1), size, (const uint8_t*)buffer, &childidx));
               TEST(i == childidx);
            }
         }
      }
   }

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

static unsigned calc_used_size(unsigned keylen, unsigned nrchild, bool isvalue)
{
   const bool issubnode = (nrchild > MAXNROFCHILD);
   unsigned off2_key   = off2_key_trienode(needkeylenbyte_header((uint8_t)keylen));
   unsigned off3_digit = off3_digit_trienode(off2_key, (uint8_t)keylen);
   unsigned off4_child = off3_digit + digitsize_trienode(issubnode, (uint8_t)nrchild);
   unsigned off5_value = off5_value_trienode(off4_child, childsize_trienode(issubnode, (uint8_t)nrchild));
   unsigned off6_size  = off6_size_trienode(off5_value, valuesize_trienode(isvalue));

   return off6_size/*not aligned*/;
}

static int test_node_lifetime(void)
{
   trie_node_t *  node;
   size_t         size_allocated = SIZEALLOCATED_MM();
   uint8_t        key[256];
   uint8_t        digit[256];
   trie_node_t *  child[256];
   void        *  value = &node;

   // prepare
   for (unsigned i = 0; i < lengthof(key); ++i) {
      key[i] = (uint8_t) (0x80 | i);
   }
   for (uintptr_t i = 0; i < lengthof(digit); ++i) {
      digit[i] = (uint8_t) (47 + i);
      child[i] = (trie_node_t*) (i&1 ? ~i : i << 16);
   }

   // == group: lifetime ==

   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         for (uint8_t nrchild = 0; nrchild <= MAXNROFCHILD; ++nrchild) {

            unsigned size = calc_used_size(keylen, nrchild, isvalue);

            if (size > MAXSIZE) {
               trie_node_t * dummy = (void*)0x1234;
               TEST(EINVAL == new_trienode(&dummy, (uint8_t)keylen, nrchild, key, digit, child, isvalue ? &value : 0));
               TEST(dummy == (void*)0x1234);
               TEST(size_allocated == SIZEALLOCATED_MM());
               break;
            }

            header_t header;
            unsigned nodesize;
            get_node_size(size, &nodesize, &header);
            if (isvalue) header = addflags_header(header, header_VALUE);
            header = (header_t) (needkeylenbyte_header((uint8_t)keylen)
                                 ? encodekeylenbyte_header(header)
                                 : encodekeylen_header(header, (uint8_t)keylen));

            // TEST new_trienode: child array
            node = 0;
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == new_trienode(&node, (uint8_t)keylen, nrchild, key, digit, child, isvalue ? &value : 0));
            TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
            TEST(0 != node);
            TEST(header  == node->header);
            TEST(nrchild == node->nrchild);
            TEST(keylen  == keylen_trienode(node));
            nodeoffsets_t off;
            init_nodeoffsets(&off, node);
            // compare copied content
            TEST(0 == memcmp(memaddr_trienode(node)+off.off2_key, key, keylen));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off3_digit, digit, nrchild));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off4_child, child, nrchild * sizeof(trie_node_t*)));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off5_value, &value, valuesize_trienode(isvalue)));

            // TEST delete_trienode: child array
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
         }

         for (uint8_t nrchild = MAXNROFCHILD+1; nrchild >= MAXNROFCHILD+1; ++nrchild) {

            unsigned size = calc_used_size(keylen, nrchild, isvalue);

            if (size > MAXSIZE) {
               trie_node_t * dummy = (void*)0x1234;
               TEST(EINVAL == new_trienode(&dummy, (uint8_t)keylen, nrchild, key, digit, child, isvalue ? &value : 0));
               TEST(dummy == (void*)0x1234);
               TEST(size_allocated == SIZEALLOCATED_MM());
               break;
            }

            header_t header;
            unsigned nodesize;
            get_node_size(size, &nodesize, &header);
            header = addflags_header(header, header_SUBNODE);
            if (isvalue) header = addflags_header(header, header_VALUE);
            header = (header_t) (needkeylenbyte_header((uint8_t)keylen)
                                 ? encodekeylenbyte_header(header)
                                 : encodekeylen_header(header, (uint8_t)keylen));

            // TEST new_trienode: subnode
            node = 0;
            TEST(0 == new_trienode(&node, (uint8_t)keylen, nrchild, key, digit, child, isvalue ? &value : 0));
            TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
            TEST(0 != node);
            TEST(header  == node->header);
            TEST(nrchild == node->nrchild+1);
            TEST(keylen  == keylen_trienode(node));
            nodeoffsets_t off;
            init_nodeoffsets(&off, node);

            // compare copied content
            TEST(0 == memcmp(memaddr_trienode(node)+off.off2_key, key, keylen));
            TEST(0 == memcmp(memaddr_trienode(node)+off.off5_value, &value, valuesize_trienode(isvalue)));
            trie_subnode_t * subnode = subnode_trienode(node, off.off4_child);
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(subnode->child[digit[i]] == child[i]);
            }
            for (unsigned i = nrchild; i <= 255; ++i) {
               TEST(subnode->child[digit[i]] == 0);
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

   // TEST new_trienode: ENOMEM
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   // no subnode
   node = 0;
   TEST(ENOMEM == new_trienode(&node, 3, 0, (const uint8_t*)"key", 0, 0, &value));
   TEST(0 == node);
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      TEST(ENOMEM == new_trienode(&node, 0, MAXNROFCHILD+1, 0, digit, child, &value));
      TEST(0 == node);
   }

   // TEST delete_trienode: EINVAL
   // no subnode
   TEST(0 == new_trienode(&node, 3, 0, (const uint8_t*)"key", 0, 0, &value));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_trienode(&node));
   TEST(0 == node);
   TEST(size_allocated == SIZEALLOCATED_MM());
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == new_trienode(&node, 0, MAXNROFCHILD+1, 0, digit, child, (void*)0));
      init_testerrortimer(&s_trie_errtimer, i, EINVAL);
      TEST(EINVAL == delete_trienode(&node));
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
   header_t          header,
   unsigned          keylen,
   unsigned          nrchild,
   uint8_t           key[keylen],
   uint8_t           digit[nrchild],
   trie_node_t *     child[nrchild],
   void *            value)
{
   nodeoffsets_t off;
   init_nodeoffsets(&off, node);
   TEST(header == node->header);
   TEST(keylen == keylen_trienode(node));
   TEST(0 == memcmp(memaddr_trienode(node)+off.off2_key, key, keylen));
   TEST(0 == memcmp(memaddr_trienode(node)+off.off5_value, &value, valuesize_trienode(isvalue_trienode(node))));
   if (issubnode_trienode(node)) {
      TEST(nrchild-1 == nrchild_trienode(node));
      trie_subnode_t subnode2;
      memset(&subnode2, 0, sizeof(subnode2));
      for (unsigned i = 0; i < nrchild; ++i) {
         TEST(0 == subnode2.child[digit[i]]);
         subnode2.child[digit[i]] = child[i];
      }
      trie_subnode_t * subnode = subnode_trienode(node, off.off4_child);
      TEST(0 == memcmp(subnode->child, subnode2.child, sizeof(subnode2.child)));

   } else {
      TEST(nrchild == nrchild_trienode(node));
      TEST(0 == memcmp(memaddr_trienode(node)+off.off3_digit, digit, nrchild));
      TEST(0 == memcmp(memaddr_trienode(node)+off.off4_child, child, childsize_trienode(0, (uint8_t)nrchild)));
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
   uint8_t        key[256];
   trie_node_t *  child[256];
   uint8_t        digit[256];
   void        *  value = (void*)0x33558998;
   size_t         size_allocated = SIZEALLOCATED_MM();

   // prepare
   memset(buffer, 0, sizeof(buffer));
   for (unsigned i = 0; i < lengthof(key); ++i) {
      key[i] = (uint8_t) ~ i;
   }
   for (unsigned i = 0; i < lengthof(child); ++i) {
      digit[i] = (uint8_t) i;
      child[i] = (void*) (~ ((uintptr_t)i << 16) ^ i);
   }

   // == group: change-helper ==

   // TEST setsubnode_trienode
   for (unsigned off4_child = 0; off4_child < MAXSIZE; off4_child += sizeof(void*)) {
      buffer[off4_child/sizeof(void*)] = 0;
      setsubnode_trienode(node, off4_child, (trie_subnode_t*)buffer);
      TEST(buffer[off4_child/sizeof(void*)] == (trie_subnode_t*)buffer);
      setsubnode_trienode(node, off4_child, (trie_subnode_t*)0);
      TEST(buffer[off4_child/sizeof(void*)] == 0);
   }

   // TEST setvalue_trienode
   for (unsigned off5_value = 0; off5_value < MAXSIZE; off5_value += sizeof(void*)) {
      buffer[off5_value/sizeof(void*)] = 0;
      setvalue_trienode(node, off5_value, (void*)buffer);
      TEST(buffer[off5_value/sizeof(void*)] == (void*)buffer);
      setvalue_trienode(node, off5_value, (void*)0);
      TEST(buffer[off5_value/sizeof(void*)] == 0);
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

   // TEST allocmemory_trienode: ENOMEM
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == allocmemory_trienode(&node, MAXSIZE));
   TEST(node == (void*)buffer);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST freememory_trienode: EINVAL
   TEST(0 == allocmemory_trienode(&node, MAXSIZE));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == freememory_trienode(node, MAXSIZE));
   TEST(size_allocated == SIZEALLOCATED_MM());
   node = (trie_node_t*)buffer;

   // TEST shrinknode_trienode
   for (unsigned i = header_SIZE1; i < header_SIZEMAX; ++i) {
      for (unsigned i2 = i-1; i2 <= header_SIZE0; --i2) {
         node->header = encodesizeflag_header(0, (header_t)i);
         node->header = addflags_header(node->header, header_SUBNODE|header_KEYLENBYTE);
         unsigned  oldsize = nodesize_trienode(node);
         unsigned  newsize = oldsize >> (i-i2);
         trie_node_t * newnode = 0;
         TEST(0 == shrinknode_trienode(&newnode, node->header, oldsize, newsize/2+1));
         TEST(0 != newnode);
         TEST(size_allocated + newsize == SIZEALLOCATED_MM());
         // only header field is set in shrinknode_trienode
         TEST(newnode->header == encodesizeflag_header(node->header, (header_t)i2));
         TEST(0 == freememory_trienode(newnode, newsize));
         TEST(size_allocated == SIZEALLOCATED_MM());
      }
   }

   // TEST shrinknode_trienode: MINSIZE is the lower limit
   {
      trie_node_t * newnode = 0;
      TEST(0 == shrinknode_trienode(&newnode, encodesizeflag_header(0, header_SIZE1), 2*MINSIZE, 1));
      TEST(size_allocated + MINSIZE == SIZEALLOCATED_MM());
      TEST(newnode->header == encodesizeflag_header(0, header_SIZE0));
      TEST(0 == freememory_trienode(newnode, MINSIZE));
   }

   // TEST shrinknode_trienode: ENOMEM
   node->header = encodesizeflag_header(0, header_SIZE1);
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == expandnode_trienode(&node, node->header, nodesize_trienode(node), MINSIZE));
   TEST(node == (trie_node_t*)buffer);
   TEST(node->header == encodesizeflag_header(0, header_SIZE1));
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST expandnode_trienode
   for (unsigned i = header_SIZE0; i < header_SIZEMAX; ++i) {
      for (unsigned i2 = i+1; i2 <= header_SIZEMAX; ++i2) {
         node->header = encodesizeflag_header(0, (header_t)i);
         node->header = addflags_header(node->header, header_SUBNODE|header_VALUE|header_KEYLEN4);
         unsigned  oldsize = nodesize_trienode(node);
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

   // TEST expandnode_trienode: ENOMEM
   node->header = encodesizeflag_header(0, header_SIZE1);
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == expandnode_trienode(&node, node->header, nodesize_trienode(node), MAXSIZE));
   TEST(node == (trie_node_t*)buffer);
   TEST(node->header == encodesizeflag_header(0, header_SIZE1));
   TEST(size_allocated == SIZEALLOCATED_MM());

   // addsubnode_trienode, trydelsubnode_trienode
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         for (unsigned nrchild = 1; nrchild <= 255; ++nrchild) {

            if (MAXSIZE < calc_used_size(keylen, nrchild, isvalue)) break;

            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key, digit, child, isvalue ? &value : 0));
            unsigned nodesize = nodesize_trienode(node);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;
            init_nodeoffsets(&off, node);

            if (!issubnode_trienode(node)) {

               // calculate if node will be reallocated to a smaller size
               unsigned needbytes = calc_used_size(keylen, MAXNROFCHILD+1, isvalue);
               unsigned nodesize2;
               header_t sizeflags;
               get_node_size(needbytes, &nodesize2, &sizeflags);
               unsigned reservebytes = nodesize2 == nodesize ? 0u : nodesize;
               TEST(nodesize2 <= nodesize);

               // TEST addsubnode_trienode: ENOMEM
               for (unsigned i = 1; i <= 2; ++i) {
                  init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
                  TEST(ENOMEM == addsubnode_trienode(&node, off.off3_digit, 0));
                  TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
                  TEST(oldnode == node);
                  TEST(0 == compare_content(node, oldheader,
                                 keylen, nrchild, key, digit, child, value));
                  if (nodesize2 == nodesize) break; // second allocation takes place only if node is resized
               }

               // TEST addsubnode_trienode: no reallocation
               TEST(0 == addsubnode_trienode(&node, off.off3_digit, (uint16_t) reservebytes));
               // subnode is allocated
               TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
               // no reallocation of node
               TEST(nodesize == nodesize_trienode(node));
               TEST(oldnode  == node);
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_child == alignoffset_trienode(off.off3_digit));
               TEST(off2.off5_value == off2.off4_child+sizeof(void*));
               TEST(off2.off6_size  == off2.off5_value+off.off6_size-off.off5_value);
               // compare moved content
               TEST(0 == compare_content(node, addflags_header(oldheader, header_SUBNODE),
                                         keylen, nrchild, key, digit, child, value));

               // TEST trydelsubnode_trienode: no reallocation
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);  // free error is ignored !!
               TEST(0 == trydelsubnode_trienode(&node, off2.off3_digit));
               // subnode is freed + no reallocation of node
               TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
               TEST(nodesize == nodesize_trienode(node));
               TEST(oldnode  == node);
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare content
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

            } else if (issubnode_trienode(node) && nodesize_trienode(node) < MAXSIZE) {

               unsigned freesize = nodesize - calc_used_size(keylen, 0, isvalue);
               unsigned minnrchild = freesize / (1 + sizeof(trie_node_t*));
               unsigned maxnrchild = (MAXSIZE - nodesize + freesize) / (1 + sizeof(trie_node_t*));
               unsigned nrchild2 = nrchild // nrchild > MAXNROFCHILD !!
                                 - MAXNROFCHILD
                                 + minnrchild; // ==> realloc

               // TEST trydelsubnode_trienode: EINVAL (> MAXIZSE)
               TEST(EINVAL == trydelsubnode_trienode(&node, off.off3_digit));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

               // TEST trydelsubnode_trienode: EINVAL (nrchild overflows)
               node->nrchild = 255;
               TEST(EINVAL == trydelsubnode_trienode(&node, off.off3_digit));
               TEST(oldnode == node);
               TEST(node->nrchild == 255);
               node->nrchild = (uint8_t) (nrchild-1);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

               if (nrchild2 <= maxnrchild) {
                  // clear child which does not fit in reallocated node
                  trie_subnode_t * subnode = subnode_trienode(node, off.off4_child);
                  for (unsigned i = nrchild2; i <= nrchild; ++i) {
                     subnode->child[i] = 0;
                  }
                  node->nrchild = (uint8_t) (nrchild2-1);
                  unsigned newsize;
                  header_t sizeflags;
                  get_node_size(nodesize - freesize + nrchild2 * (1+sizeof(trie_node_t*)), &newsize, &sizeflags);
                  TEST(newsize > nodesize);

                  // TEST trydelsubnode_trienode: ENOMEM
                  init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
                  TEST(ENOMEM == trydelsubnode_trienode(&node, off.off3_digit));
                  TEST(oldnode == node);
                  TEST(0 == compare_content(node, oldheader,
                                 keylen, nrchild2, key, digit, child, value));

                  // TEST trydelsubnode_trienode: node is reallocated (expanded)
                  TEST(0 == trydelsubnode_trienode(&node, off.off3_digit));
                  // subnode is freed + reallocation
                  TEST(size_allocated + newsize == SIZEALLOCATED_MM());
                  TEST(newsize == nodesize_trienode(node));
                  // offsets ok
                  init_nodeoffsets(&off2, node);
                  TEST(off2.off2_key   == off.off2_key);
                  TEST(off2.off3_digit == off.off3_digit);
                  TEST(off2.off4_child == alignoffset_trienode(off.off3_digit + nrchild2));
                  TEST(off2.off5_value == off2.off4_child + nrchild2 * sizeof(trie_node_t*));
                  TEST(off2.off6_size  == off2.off5_value + off.off6_size - off.off5_value);
                  // compare moved content
                  TEST(0 == compare_content(node, addflags_header(delflags_header(oldheader, header_SIZEMASK|header_SUBNODE),sizeflags),
                                 keylen, nrchild2, key, digit, child, value));

                  // TEST addsubnode_trienode: node is reallocated (shrunken)
                  init_testerrortimer(&s_trie_errtimer, 3, EINVAL); // free memory error is ignored !
                  TEST(0 == addsubnode_trienode(&node, off.off3_digit, 0));
                  // subnode is allocated + node reallocated
                  TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
                  TEST(nodesize == nodesize_trienode(node));
                  // offsets ok
                  init_nodeoffsets(&off2, node);
                  TEST(0 == memcmp(&off2, &off, sizeof(off)));
                  // compare moved content
                  TEST(0 == compare_content(node, oldheader,
                                 keylen, nrchild2, key, digit, child, value));
               }

            }

            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST addsubnode_trienode: reservebytes
   TEST(0 == new_trienode(&node, 0, 4, 0, digit, child, 0));
   TEST(8*sizeof(void*) == nodesize_trienode(node));
   init_nodeoffsets(&off, node);
   TEST(0 == addsubnode_trienode(&node, off.off3_digit, sizeof(uint8_t)));
   TEST(2*sizeof(void*) == nodesize_trienode(node));
   TEST(0 == delete_trienode(&node));

   // delvalue_trienode, tryaddvalue_trienode
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         for (unsigned nrchild = 0; nrchild <= MAXNROFCHILD+2; ++nrchild) {

            if (MAXSIZE < calc_used_size(keylen, nrchild, isvalue)) break;

            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key, digit, child, isvalue ? &value : 0));
            init_nodeoffsets(&off, node);
            unsigned    nodesize  = nodesize_trienode(node);
            unsigned    subsize   = (issubnode_trienode(node) ? sizeof(trie_subnode_t) : 0);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;

            if (isvalue) {

               // TEST delvalue_trienode
               delvalue_trienode(node);
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated + nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_child == off.off4_child);
               TEST(off2.off5_value == off.off5_value);
               TEST(off2.off6_size  == off.off6_size - sizeof(void*));
               // compare moved content
               TEST(0 == isvalue_trienode(node));
               TEST(0 == compare_content(node, delflags_header(oldheader, header_VALUE),
                              keylen, nrchild, key, digit, child, value));

               // TEST tryaddvalue_trienode: no reallocation
               TEST(0 == tryaddvalue_trienode(&node, off.off4_child, value));
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated + nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare moved content
               TEST(0 != isvalue_trienode(node));
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

            } else if (MAXSIZE == nodesize && nodesize < off.off6_size + sizeof(void*)) {
               TEST(EINVAL == tryaddvalue_trienode(&node, off.off4_child, value));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

            } else if (nodesize < off.off6_size + sizeof(void*)) {
               // TEST tryaddvalue_trienode: ENOMEM
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
               TEST(ENOMEM == tryaddvalue_trienode(&node, off.off4_child, value));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key, digit, child, value));

               // TEST tryaddvalue_trienode: reallocation
               TEST(0 == tryaddvalue_trienode(&node, off.off4_child, value));
               TEST(0 != node);
               // reallocation
               TEST(size_allocated + 2*nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_child == off.off4_child);
               TEST(off2.off5_value == off.off5_value);
               TEST(off2.off6_size  == off.off6_size + sizeof(void*));
               // compare moved content
               TEST(0 != isvalue_trienode(node));
               TEST(0 == compare_content(node, encodesizeflag_header(addflags_header(oldheader, header_VALUE), (header_t)(sizeflags_header(oldheader)+1)),
                              keylen, nrchild, key, digit, child, value));

            }

            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // tryaddkeyprefix_trienode, delkeyprefix_trienode
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         if (30 <= keylen && keylen <= 240) { keylen = 240; continue; }
         for (unsigned nrchild = 0; nrchild <= MAXNROFCHILD+2; ++nrchild) {
            if (6 == nrchild) nrchild = MAXNROFCHILD-2;

            if (MAXSIZE < calc_used_size(keylen, nrchild, isvalue)) break;

            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key+lengthof(key)-keylen, digit, child, isvalue ? &value : 0));
            init_nodeoffsets(&off, node);
            unsigned    nodesize  = nodesize_trienode(node);
            unsigned    subsize   = (issubnode_trienode(node) ? sizeof(trie_subnode_t) : 0);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;

            for (unsigned preflen = 0; preflen <= keylen; ++preflen) {
               if (5 == preflen && keylen > 10) preflen = keylen-5;

               // TEST delkeyprefix_trienode: no reallocation
               TEST(0 == delkeyprefix_trienode(&node, off.off2_key, off.off3_digit, (uint8_t)preflen, (uint16_t)(preflen+1/*lenbyte*/)));
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated + nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key-(needkeylenbyte_header((uint8_t)keylen) && !needkeylenbyte_header((uint8_t)(keylen-preflen))));
               TEST(off2.off3_digit == off2.off2_key+keylen-preflen);
               TEST(off2.off4_child == off4_child_trienode(off2.off3_digit, digitsize_trienode(subsize!=0, (uint8_t)nrchild)));
               TEST(off2.off5_value == off2.off4_child + off.off5_value - off.off4_child);
               TEST(off2.off6_size  == off2.off5_value + off.off6_size - off.off5_value);
               // compare moved content
               header_t header2 = addflags_header(delflags_header(oldheader, header_KEYLENMASK), keylen_header(node->header));
               TEST(0 == compare_content(node, header2,
                              keylen-preflen, nrchild, key + lengthof(key) - (keylen-preflen),
                              digit, child, value));

               // TEST tryaddkeyprefix_trienode: no reallocation
               TEST(0 == tryaddkeyprefix_trienode(&node, off2.off2_key, off2.off3_digit, (uint8_t)preflen, key + lengthof(key) - keylen));
               // no reallocation
               TEST(oldnode == node);
               TEST(size_allocated + nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare moved content
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + lengthof(key) - keylen,
                              digit, child, value));

            }

            // TEST tryaddkeyprefix_trienode: EINVAL (keylen overflow)
            if (keylen > 0) {
               TEST(EINVAL == tryaddkeyprefix_trienode(&node, off.off2_key, off.off3_digit, (uint8_t)(256-keylen), key));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + lengthof(key) - keylen,
                              digit, child, value));
            }

            // TEST tryaddkeyprefix_trienode: EINVAL (nodesize > MAXSIZE)
            if (off.off6_size + 255-keylen > MAXSIZE) {
               TEST(EINVAL == tryaddkeyprefix_trienode(&node, off.off2_key, off.off3_digit, (uint8_t)(255-keylen), key));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + lengthof(key) - keylen,
                              digit, child, value));
            }

            if (nodesize < MAXSIZE && nodesize + sizeof(void*)/*alignment*/ <= off.off6_size + (255-keylen)) {
               /*resize possible*/
               uint8_t preflen = (uint8_t) (nodesize + sizeof(void*) - off.off6_size);

               // TEST tryaddkeyprefix_trienode: ENOMEM
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
               TEST(ENOMEM == tryaddkeyprefix_trienode(&node, off.off2_key, off.off3_digit, preflen, key));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + lengthof(key) - keylen,
                              digit, child, value));

               if ((keylen & 1)) {
                  // switch from next bigger size to biggest size
                  preflen = (uint8_t) (off.off6_size+(255-keylen) < MAXSIZE ? (255 - keylen) : MAXSIZE - 1/*lenbyte*/ - off.off6_size);
               }

               // TEST tryaddkeyprefix_trienode: with reallocation (expanded)
               init_testerrortimer(&s_trie_errtimer, 2, EINVAL); // free memory error is ignored !
               TEST(0 == tryaddkeyprefix_trienode(&node, off.off2_key, off.off3_digit, preflen, key + lengthof(key) - keylen - preflen));
               // with reallocation
               TEST(oldnode != node);
               TEST(size_allocated + nodesize_trienode(node) + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key+(!needkeylenbyte_header((uint8_t)keylen) && needkeylenbyte_header((uint8_t)(keylen+preflen))));
               TEST(off2.off3_digit == off2.off2_key+keylen+preflen);
               TEST(off2.off4_child == off4_child_trienode(off2.off3_digit, digitsize_trienode(subsize!=0, (uint8_t)nrchild)));
               TEST(off2.off5_value == off2.off4_child + off.off5_value - off.off4_child);
               TEST(off2.off6_size  == off2.off5_value + off.off6_size - off.off5_value);
               TEST(off2.off6_size  >  nodesize_trienode(node)/2);
               // compare moved content
               header_t header2 = encodesizeflag_header(oldheader, sizeflags_header(node->header));
               header2 = addflags_header(delflags_header(header2, header_KEYLENMASK), keylen_header(node->header));
               TEST(0 == compare_content(node, header2,
                              keylen+preflen, nrchild, key + lengthof(key) - keylen - preflen,
                              digit, child, value));


               // TEST delkeyprefix_trienode: ENOMEM
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
               oldnode = node;
               TEST(ENOMEM == delkeyprefix_trienode(&node, off2.off2_key, off2.off3_digit, (uint8_t)preflen, 0));
               TEST(node == oldnode);
               TEST(size_allocated + nodesize_trienode(node) + subsize == SIZEALLOCATED_MM());
               TEST(0 == compare_content(node, header2,
                              keylen+preflen, nrchild, key + lengthof(key) - keylen - preflen,
                              digit, child, value));

               // TEST delkeyprefix_trienode: with reallocation (shrunken)
               init_testerrortimer(&s_trie_errtimer, 2, EINVAL); // free memory error is ignored !
               TEST(0 == delkeyprefix_trienode(&node, off2.off2_key, off2.off3_digit, (uint8_t)preflen, 0));
               // reallocation
               TEST(node != oldnode);
               TEST(size_allocated + nodesize + subsize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off2, &off, sizeof(off)));
               // compare moved content
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + lengthof(key) - keylen,
                              digit, child, value));

            }

            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST delkeyprefix_trienode: reservebytes
   TEST(0 == new_trienode(&node, 2*sizeof(void*), 0, key, 0, 0, &value));
   TEST(4*sizeof(void*) == nodesize_trienode(node));
   init_nodeoffsets(&off, node);
   TEST(0 == delkeyprefix_trienode(&node, off.off2_key, off.off3_digit, 2*sizeof(void*), sizeof(uint8_t)));
   TEST(2*sizeof(void*) == nodesize_trienode(node));
   TEST(0 == delete_trienode(&node));

   // tryaddchild_trienode, delchild_trienode
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= 255; ++keylen) {
         for (unsigned nrchild = 0; nrchild <= MAXNROFCHILD; ++nrchild) {

            if (MAXSIZE < calc_used_size(keylen, nrchild, isvalue)) break;

            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key, digit, child, isvalue ? &value : 0));
            init_nodeoffsets(&off, node);
            unsigned    nodesize  = nodesize_trienode(node);
            header_t    oldheader = node->header;
            trie_node_t * oldnode = node;

            if (nrchild && calc_used_size(keylen, nrchild-1, isvalue) > nodesize/2/*no reallocation*/) {
               for (uint8_t childidx = 0; childidx < nrchild; ++childidx) {

                  uint8_t       expect_digit[nrchild];
                  trie_node_t * expect_child[nrchild];
                  memcpy(expect_digit, digit, childidx);
                  memcpy(expect_child, child, childidx*sizeof(trie_node_t*));
                  memcpy(expect_digit+childidx, digit+childidx+1, nrchild-1-childidx);
                  memcpy(expect_child+childidx, child+childidx+1, (nrchild-1-childidx)*sizeof(trie_node_t*));

                  // TEST delchild_trienode
                  delchild_trienode(&node, off.off3_digit, off.off4_child, childidx);
                  // no reallocation
                  TEST(oldnode == node);
                  TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
                  // offsets ok
                  init_nodeoffsets(&off2, node);
                  TEST(off2.off2_key   == off.off2_key);
                  TEST(off2.off3_digit == off.off3_digit);
                  TEST(off2.off4_child == off4_child_trienode(off2.off3_digit, digitsize_trienode(0, (uint8_t)(nrchild-1))));
                  TEST(off2.off5_value == off2.off4_child + off.off5_value - off.off4_child - sizeof(trie_node_t*));
                  TEST(off2.off6_size  == off2.off5_value + off.off6_size - off.off5_value);
                  // compare moved content
                  TEST(0 == compare_content(node, oldheader,
                                 keylen, nrchild-1, key + keylen - keylen,
                                 expect_digit, expect_child, value));

                  // TEST tryaddchild_trienode
                  TEST(0 == tryaddchild_trienode(&node, off2.off3_digit, off2.off4_child, childidx, digit[childidx], child[childidx]));
                  // no reallocation
                  TEST(oldnode == node);
                  TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
                  // offsets ok
                  init_nodeoffsets(&off2, node);
                  TEST(0 == memcmp(&off, &off2, sizeof(off)));
                  // compare moved content
                  TEST(0 == compare_content(node, oldheader,
                                 keylen, nrchild, key + keylen - keylen,
                                 digit, child, value));

               }
            }

            unsigned freesize = nodesize - calc_used_size(keylen, nrchild, isvalue);

            if (nodesize == MAXSIZE && freesize <= sizeof(trie_node_t*)) {
               // TEST tryaddchild_trienode: EINVAL (nodesize overflow)
               TEST(EINVAL == tryaddchild_trienode(&node, off.off3_digit, off.off4_child, 0, 255, (trie_node_t*)0x1234));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + keylen - keylen,
                              digit, child, value));

            }

            if (nodesize < MAXSIZE && freesize <= sizeof(trie_node_t*)) {

               // TEST tryaddchild_trienode: ENOMEM
               init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
               TEST(ENOMEM == tryaddchild_trienode(&node, off.off3_digit, off.off4_child, 0, 255, (trie_node_t*)0x1234));
               TEST(oldnode == node);
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + keylen - keylen,
                              digit, child, value));

               uint8_t childidx = (uint8_t) (keylen % (nrchild + 1));
               uint8_t       expect_digit[nrchild+1];
               trie_node_t * expect_child[nrchild+1];
               memcpy(expect_digit, digit, childidx);
               memcpy(expect_child, child, childidx*sizeof(trie_node_t*));
               memcpy(expect_digit+childidx+1, digit+childidx, nrchild-childidx);
               memcpy(expect_child+childidx+1, child+childidx, (nrchild-childidx)*sizeof(trie_node_t*));
               expect_child[childidx] = (trie_node_t*)0x1234;
               expect_digit[childidx] = 255;

               // TEST tryaddchild_trienode: with reallocation
               TEST(0 == tryaddchild_trienode(&node, off.off3_digit, off.off4_child, childidx, 255, (trie_node_t*)0x1234));
               // with reallocation
               TEST(oldnode != node);
               TEST(size_allocated + 2*nodesize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(off2.off2_key   == off.off2_key);
               TEST(off2.off3_digit == off.off3_digit);
               TEST(off2.off4_child == off4_child_trienode(off2.off3_digit, digitsize_trienode(0, (uint8_t)(1+nrchild))));
               TEST(off2.off5_value == off2.off4_child + off.off5_value - off.off4_child + sizeof(trie_node_t*));
               TEST(off2.off6_size  == off2.off5_value + off.off6_size - off.off5_value);
               // compare moved content
               TEST(0 == compare_content(node, encodesizeflag_header(oldheader, (header_t)(sizeflags_header(oldheader)+1)),
                              keylen, nrchild+1, key + keylen - keylen,
                              expect_digit, expect_child, value));

               // TEST delchild_trienode: with reallocation
               oldnode = node;
               delchild_trienode(&node, off2.off3_digit, off2.off4_child, childidx);
               // with reallocation
               TEST(oldnode != node);
               TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
               // offsets ok
               init_nodeoffsets(&off2, node);
               TEST(0 == memcmp(&off, &off2, sizeof(off)));
               // compare moved content
               TEST(0 == compare_content(node, oldheader,
                              keylen, nrchild, key + keylen - keylen,
                              digit, child, value));

            }

            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST delchild_trienode: ENOMEM
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == new_trienode(&node, 0, 1, 0, digit, child, &value));
      init_nodeoffsets(&off, node);
      trie_node_t * oldnode = node;
      header_t sizeflags;
      unsigned nodesize = nodesize_trienode(node);
      get_node_size(i == 1 ? nodesize : nodesize/2, &nodesize, &sizeflags);
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      delchild_trienode(&node, off.off3_digit, off.off4_child, 0);
      TEST(nrchild_trienode(node) == 0);
      if (i == 1) {
         TEST(oldnode == node); // allocation failed
      } else {
         TEST(oldnode != node); // free failed
      }
      TEST(0 == compare_content(node, (header_t) (header_VALUE + sizeflags),
                                0, 0, 0, 0, 0, value));
      TEST(0 == delete_trienode(&node));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   return 0;
ONABORT:
   if (node != (void*)buffer) {
      delete_trienode(&node);
   }
   return EINVAL;
}

static int build_trie(/*out*/trie_node_t ** node, unsigned depth, unsigned type)
{
   // type: [  0 -> child array, 1 -> subnode, 2 -> child array + value,
   //          3 -> subnode + value, 4 -> only value ]
   TEST(type <= 4);

   uint8_t       digits[256];
   trie_node_t * childs[256];
   memset(digits, 128, sizeof(digits));
   memset(childs, 0, sizeof(childs));

   if (depth > 0 && type < 4) {
      for (unsigned i = 1; i <= 5; ++i) {
         digits[i] = (uint8_t) (i == 4 ? 255 : (depth&1) + 17*i);
         TEST(0 == build_trie(&childs[i], depth-1, i-1));
      }
   }

   void * value = (void*)1;
   TEST(0 == new_trienode(node, 3, (uint8_t) (type&1 ? 255 : type != 4 ? 6 : 0), (const uint8_t*)"key", digits, childs, type >= 2 ? &value : 0));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_initfree(void)
{
   trie_t trie = trie_INIT;
   size_t size_allocated = SIZEALLOCATED_MM();

   // TEST trie_INIT
   TEST(0 == trie.root);

   // TEST trie_INIT2
   trie = (trie_t) trie_INIT2((trie_node_t*)1);
   TEST(trie.root == (trie_node_t*)1);
   trie = (trie_t) trie_INIT2(0);
   TEST(trie.root == 0);

   // TEST init_trie
   trie.root = (void*)1;
   TEST(0 == init_trie(&trie));
   TEST(0 == trie.root);

   // TEST trie_FREE
   trie = (trie_t) trie_FREE;
   TEST(0 == trie.root);

   // TEST free_trie: free already freed trie
   TEST(0 == free_trie(&trie));
   TEST(0 == trie.root);

   // TEST free_trie: free single trie node
   for (unsigned type = 0; type <= 4; ++type) {
      TEST(0 == build_trie(&trie.root, 0, type));
      TEST(0 != trie.root);
      TEST(SIZEALLOCATED_MM() > size_allocated);
      TEST(0 == free_trie(&trie));
      TEST(0 == trie.root);
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // TEST free_trie: free trie nodes recursiveley
   for (unsigned type = 0; type <= 3; ++type) {
      TEST(0 == build_trie(&trie.root, 5, type));
      TEST(0 != trie.root);
      TEST(SIZEALLOCATED_MM() >  size_allocated);
      TEST(0 == free_trie(&trie));
      TEST(0 == trie.root);
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // TEST free_trie: ERROR
   for (unsigned i = 11; i <= 14; ++i) {
      TEST(0 == build_trie(&trie.root, 3, i&0x3));
      init_testerrortimer(&s_trie_errtimer, i, EINVAL);
      TEST(EINVAL == free_trie(&trie));
      TEST(0 == trie.root);
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   return 0;
ONABORT:
   free_trie(&trie);
   return EINVAL;
}

static int compare_nodechain(
   trie_node_t *  chainstart,
   size_t         memorysize,
   unsigned       keylen,
   uint8_t        key[keylen],
   void        *  value)
{
   unsigned nrnodesmax = keylen / MAXKEYLEN;
   unsigned keylen_remain = keylen % MAXKEYLEN;
   unsigned nrnodes;
   unsigned keyoffset = 0;
   size_t   nodesize = 0;
   uint8_t  splitkeylen[5];
   trie_node_t * node = chainstart;
   trie_node_t * child;

   for (nrnodes = 0; keylen_remain; ++nrnodes) {
      TEST(nrnodes < lengthof(splitkeylen));
      splitkeylen[nrnodes] = splitkeylen_trienode((uint16_t)keylen_remain);
      keylen_remain -= splitkeylen[nrnodes];
   }

   // 1. compare chain of nodes (having a single child)
   for (node = chainstart; (nrnodes + nrnodesmax) > 1; node = child) {
      uint8_t keysize = (uint8_t) ((nrnodes > 0) ? splitkeylen[-- nrnodes] : (--nrnodesmax, MAXKEYLEN));
      TEST(keylen_trienode(node)  == keysize-1);
      TEST(nrchild_trienode(node) == 1);
      nodesize += nodesize_trienode(node);
      nodeoffsets_t off;
      init_nodeoffsets(&off, node);
      child = childs_trienode(node, childoff4_trienode(node))[0];
      TEST(0 == compare_content(node, (header_t)(node->header & (header_SIZEMASK|header_KEYLENMASK)),
                     keysize-1u, 1, key + keyoffset,
                     key+keyoffset+keysize-1, &child, value));
      keyoffset += keysize;
   }

   // 2. compare last node having only a value
   uint8_t keysize = (uint8_t) ((nrnodes > 0) ? splitkeylen[-- nrnodes] : nrnodesmax ? MAXKEYLEN : 0);
   TEST(keylen_trienode(node)  == keysize);
   TEST(nrchild_trienode(node) == 0);
   nodesize += nodesize_trienode(node);
   nodeoffsets_t off;
   init_nodeoffsets(&off, node);
   TEST(0 == compare_content(node, (header_t) ((node->header & (header_SIZEMASK|header_KEYLENMASK)) | header_VALUE),
                  keysize, 0, key + keyoffset,
                  0, 0, value));
   keyoffset += keysize;

   TEST(keyoffset == keylen);
   TEST(nodesize == memorysize);

   return 0;
ONABORT:
   return EINVAL;
}

static int test_inserthelper(void)
{
   trie_node_t *  node = 0;
   trie_t         trie = trie_INIT;
   memblock_t     key;
   uint8_t        digit[256];
   trie_node_t *  child[256];
   size_t         size_allocated;
   header_t       sizeflags;
   unsigned       nodesize;
   void        *  value = (void*)0x0405abff;
   nodeoffsets_t  off;
   nodeoffsets_t  off2;

   TEST(0 == ALLOC_MM(UINT16_MAX, &key));
   for (unsigned i = 0; i < UINT16_MAX; ++i) {
      key.addr[i] = (uint8_t) (i * 23);
   }
   for (unsigned i = 0; i < lengthof(digit); ++i) {
      digit[i] = (uint8_t) i;
      child[i] = (void*) (3+i);
   }
   size_allocated = SIZEALLOCATED_MM();

   // TEST restructnode_trie: extract childs into trie_subnode_t
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (int isChild = 0; isChild <= 1; ++isChild) {
         for (unsigned keylen = 0; keylen < 2*sizeof(trie_node_t*); ++keylen) {
            unsigned usedsize = calc_used_size(keylen, 0, isvalue);
            unsigned nrchild = (MAXSIZE - usedsize) / (sizeof(uint8_t) + sizeof(trie_node_t*));
            unsigned reservesize = isChild ? 0 : sizeof(void*);
            usedsize = calc_used_size(keylen, MAXNROFCHILD+1, isvalue) + reservesize;
            get_node_size(usedsize, &nodesize, &sizeflags);
            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit, child, isvalue ? &value : 0));
            TEST(MAXSIZE == nodesize_trienode(node));
            TEST(keylen  == keylen_trienode(node));
            init_nodeoffsets(&off, node);
            header_t      oldheader = node->header;
            trie_node_t * oldnode = node;
            trie_node_t * parentchild = 0;
            memcpy(&off2, &off, sizeof(off2));
            // restructnode_trie
            TEST(0 == restructnode_trie(&node, &parentchild, isChild, off.off2_key, &off.off3_digit, &off.off4_child));
            TEST(off.off3_digit == off2.off3_digit);
            TEST(off.off4_child == off4_child_trienode(off.off3_digit, digitsize_trienode(1, (uint8_t)nrchild)));
            TEST(node != oldnode);
            TEST(node == parentchild);
            TEST(0 == compare_content(node, delflags_header(oldheader, header_SIZEMASK)|header_SUBNODE|sizeflags,
                                       keylen, nrchild, key.addr, digit, child, value));
            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST restructnode_trie: ENOMEM (extract childs into trie_subnode_t)
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == new_trienode(&node, 0, MAXNROFCHILD, 0, digit, child, 0));
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      header_t      oldheader = node->header;
      trie_node_t * oldnode   = node;
      init_nodeoffsets(&off, node);
      memcpy(&off2, &off, sizeof(off2));
      TEST(ENOMEM == restructnode_trie(&node, 0, false, off.off2_key, &off.off3_digit, &off.off4_child));
      TEST(0 == memcmp(&off2, &off, sizeof(off2)));
      TEST(oldnode == node);
      TEST(0 == compare_content(node, oldheader,
                                0, MAXNROFCHILD, 0, digit, child, 0));
      TEST(0 == delete_trienode(&node));
   }

   // TEST restructnode_trie: extract key into parent node
   for (uint8_t isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned isChild = 0; isChild <= 1; ++isChild) {
         for (unsigned keylen = 2*sizeof(trie_node_t*); keylen <= MAXKEYLEN; ++keylen) {
            unsigned usedsize = calc_used_size(keylen, 0, isvalue);
            unsigned nrchild  = (MAXSIZE - usedsize) / (sizeof(uint8_t) + sizeof(trie_node_t*));
            unsigned reservesize = isChild ? (1+sizeof(trie_node_t*)) : sizeof(void*);
            usedsize = calc_used_size(0, nrchild, isvalue) + reservesize;
            get_node_size(usedsize, &nodesize, &sizeflags);
            TEST(0 == new_trienode(&node, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit, child, isvalue ? &value : 0));
            TEST(MAXSIZE == nodesize_trienode(node));
            TEST(keylen  == keylen_trienode(node));
            init_nodeoffsets(&off, node);
            header_t      oldheader   = node->header;
            trie_node_t * parentchild = 0;
            // restructnode_trie
            TEST(0 == restructnode_trie(&node, &parentchild, isChild, off.off2_key, &off.off3_digit, &off.off4_child));
            TEST(off.off3_digit == off3_digit_trienode(off2_key_trienode(0), 0));
            TEST(off.off4_child == off4_child_trienode(off.off3_digit, digitsize_trienode(0, (uint8_t)nrchild)));
            TEST(0 != parentchild);
            TEST(node != parentchild);
            TEST(0 == compare_content(node, delflags_header(oldheader, header_SIZEMASK|header_KEYLENMASK)|sizeflags,
                                      0, nrchild, 0, digit, child, value));
            usedsize = calc_used_size(keylen-1, 1, false);
            get_node_size(usedsize, &nodesize, &sizeflags);
            TEST(0 == compare_content(parentchild, addflags_header(sizeflags,header_KEYLENBYTE),
                                      keylen-1, 1, key.addr, key.addr+keylen-1, &node, 0));
            TEST(0 == delete_trienode(&node));
            TEST(0 == delete_trienode(&parentchild));
         }
      }
   }

   // TEST restructnode_trie: ENOMEM (extract key into parent node)
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == new_trienode(&node, MAXKEYLEN, 0, key.addr, 0, 0, &value));
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      header_t      oldheader = node->header;
      trie_node_t * oldnode   = node;
      init_nodeoffsets(&off, node);
      memcpy(&off2, &off, sizeof(off2));
      TEST(ENOMEM == restructnode_trie(&node, 0, false, off.off2_key, &off.off3_digit, &off.off4_child));
      TEST(0 == memcmp(&off2, &off, sizeof(off2)));
      TEST(oldnode == node);
      TEST(0 == compare_content(node, oldheader,
                                MAXKEYLEN, 0, key.addr, 0, 0, value));
      TEST(0 == delete_trienode(&node));
   }
   TEST(size_allocated == SIZEALLOCATED_MM());

   // build_nodechain_trienode
   for (unsigned keylen = 0; keylen <= UINT16_MAX; ++keylen) {
      if (keylen == 4*MAXKEYLEN) keylen = UINT16_MAX - 5;
      value = (void*)(0x01020304 + keylen);

      // TEST build_nodechain_trienode
      TEST(0 == build_nodechain_trienode(&trie.root, (uint16_t)keylen, key.addr, value));
      TEST(0 != trie.root);
      TEST(SIZEALLOCATED_MM() > size_allocated);
      TEST(0 == compare_nodechain(trie.root, SIZEALLOCATED_MM() - size_allocated, keylen, key.addr, value));
      TEST(0 == free_trie(&trie));
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // TEST build_nodechain_trienode: ENOMEM (single node)
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == build_nodechain_trienode(&trie.root, NOSPLITKEYLEN, key.addr, 0));
   TEST(SIZEALLOCATED_MM() == size_allocated);
   TEST(0 == trie.root);

   // TEST build_nodechain_trienode: ENOMEM (complete chain)
   init_testerrortimer(&s_trie_errtimer, 1 + UINT16_MAX / MAXKEYLEN, ENOMEM);
   TEST(ENOMEM == build_nodechain_trienode(&trie.root, UINT16_MAX, key.addr, 0));
   TEST(SIZEALLOCATED_MM() == size_allocated);
   TEST(0 == trie.root);

   // TEST build_splitnode_trienode: no parent
   for (int isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen <= (COMPUTEKEYLEN(128)-sizeof(trie_node_t*)-2); ++keylen) {
         trie_node_t ** childs = 0;
         get_node_size(calc_used_size(keylen, isvalue?1:2, isvalue), &nodesize, &sizeflags);
         if (isvalue) sizeflags = (header_t) (sizeflags | header_VALUE);
         TEST(0 == build_splitnode_trienode(&node, &childs, (uint8_t)keylen, key.addr, digit+keylen, child+keylen, isvalue ? &value : 0));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize);
         TEST(0 == compare_content(node, (header_t)(sizeflags|(node->header&header_KEYLENMASK)),
                                   keylen, isvalue?1:2, key.addr,
                                   digit+keylen, child+keylen, value));
         TEST(childs == childs_trienode(node, childoff4_trienode(node)));
         TEST(0 == delete_trienode(&node));
      }
   }

   // TEST build_splitnode_trienode: with parent
   for (int isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = (COMPUTEKEYLEN(128)-sizeof(trie_node_t*)-2)+1; keylen <= MAXKEYLEN; ++keylen) {
         trie_node_t ** childs = 0;
         unsigned parent_keylen = keylen < COMPUTEKEYLEN(128) ? keylen : COMPUTEKEYLEN(128);
         unsigned node_keylen   = keylen - parent_keylen;
         header_t parentsizeflags;
         get_node_size(128, &nodesize, &parentsizeflags);
         get_node_size(calc_used_size(node_keylen, isvalue?1:2, isvalue), &nodesize, &sizeflags);
         if (isvalue) sizeflags = (header_t) (sizeflags | header_VALUE);
         TEST(0 == build_splitnode_trienode(&trie.root, &childs, (uint8_t)keylen, key.addr, digit+keylen/2, child+keylen/2, isvalue ? &value : 0));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + 128/*parent*/);
         node = childs_trienode(trie.root, childoff4_trienode(trie.root))[0];
         TEST(0 == compare_content(trie.root, (header_t)(parentsizeflags|(trie.root->header&header_KEYLENMASK)),
                                   parent_keylen-1, 1, key.addr,
                                   key.addr+parent_keylen-1, &node, 0));
         TEST(0 == compare_content(node, (header_t)(sizeflags|(node->header&header_KEYLENMASK)),
                                   node_keylen, isvalue?1:2, key.addr+parent_keylen,
                                   digit+keylen/2, child+keylen/2, value));
         TEST(childs == childs_trienode(node, childoff4_trienode(node)));
         TEST(0 == delete_trienode(&trie.root));
         TEST(0 == delete_trienode(&node));
      }
   }

   // TEST build_splitnode_trienode: ENOMEM (no parent)
   trie_node_t * dummy = (trie_node_t*)0x1234;
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == build_splitnode_trienode(&node, 0, 1, key.addr, digit, child, 0));
   TEST(SIZEALLOCATED_MM() == size_allocated);
   TEST(dummy == (trie_node_t*)0x1234);

   // TEST build_splitnode_trienode: ENOMEM (with parent)
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      TEST(ENOMEM == build_splitnode_trienode(&node, 0, COMPUTEKEYLEN(128), key.addr, digit, child, &value));
      TEST(SIZEALLOCATED_MM() == size_allocated);
      TEST(dummy == (trie_node_t*)0x1234);
   }

   // unprepare
   TEST(0 == FREE_MM(&key));

   return 0;
ONABORT:
   (void) delete_trienode(&node);
   (void) free_trie(&trie);
   (void) FREE_MM(&key);
   return EINVAL;
}

static int build_depthx_trie(/*out*/trie_t * trie, /*out*/trie_node_t *** parentchild, /*out*/unsigned * keylen, unsigned index, unsigned depth, uint8_t key[10*depth])
{
   trie_node_t * parent;
   unsigned keylen2 = 1u + 5 * ((unsigned)random() % 4u);

   if (depth) {
      TEST(0 == build_depthx_trie(trie, parentchild, keylen, index+1, depth-1, key+keylen2+1));
      int idx = key[keylen2] > (uint8_t)(key[keylen2]-1);
      uint8_t digits[2];
      trie_node_t * childs[2];
      digits[idx]  = key[keylen2];
      digits[!idx] = (uint8_t)(key[keylen2]-1);
      childs[idx]  = trie->root;
      childs[!idx] = 0;
      TEST(0 == new_trienode(&parent, (uint8_t)keylen2, 2, key, digits, childs, 0));

   } else {
      uint8_t digit = (uint8_t) (key[keylen2] + 1);
      // subnode needs at least one child
      TEST(0 == new_trienode(&parent, (uint8_t)keylen2, 1, key, &digit, (trie_node_t*[]){0}, 0));
   }

   if (0 != (index&1)) {
      nodeoffsets_t off;
      init_nodeoffsets(&off, parent);
      TEST(0 == addsubnode_trienode(&parent, off.off3_digit, 0));
   }

   if (!depth) *keylen = 0;
   if (depth == 1) {
      if (issubnode_trienode(parent)) {
         *parentchild = childaddr_triesubnode(subnode_trienode(parent, childoff4_trienode(parent)), key[keylen2]);
      } else {
         trie_node_t ** childs = childs_trienode(parent, childoff4_trienode(parent));
         *parentchild = childs[0] ? childs : childs+1;
      }
   }

   *keylen += keylen2+(depth!=0);
   trie->root = parent;

   return 0;
ONABORT:
   return EINVAL;
}

/* function: test_insert
 * Test insert functionality of <trie_t>.
 *
 * The following is tested:
 * == depth 0 ==
 * 1. Test insert into empty trie. Root node is created.
 * Even a chain of nodes is created if the key is too long
 * to store it into a single node.
 * == depth 1 ==
 * 2. Test insert of value into an already existing node with no value.
 *    2.1 If expansion not possible
 *        2.1.1 child array is converted into a subnode
 *        2.1.2 part of key is removed from node and put into its own node
 * 3. Create nodes (or chain) and add them to an existing node
 *    3.1 Add child to child array (expand child array)
 *        3.1.2 If expansion not possible
 *              3.1.2.1 child array is converted into a subnode
 *              3.1.2.2 part of key is removed from node and put into its own node
 *    3.2 Add child to subnode
 * 4. Split a key stored in node and add new child and node to
 *    newly created splitnode.
 * == depth X: follow node chain ==
 * 5. Now test, that insert finds the correct node and applies all transformations
 *    of depth 1 correctly to the found node.
 * == Error Codes ==
 * 6. Test error codes of insert (no change of trie).
 * 7. Test logging and non logging of EEXIST of insert and tryinsert.
 */
static int test_insert(void)
{
   trie_t         trie = trie_INIT;
   size_t         size_allocated;
   size_t         size_used;
   memblock_t     key;
   uint8_t        digit[256];
   trie_node_t *  child[256];
   uint8_t        key2[MAXKEYLEN+1];
   uint8_t        digit2[256];
   trie_node_t *  child2[256];
   header_t       sizeflags;
   header_t       oldheader;
   unsigned       nodesize;
   uint8_t     *  logbuffer;
   size_t         logsize1;
   size_t         logsize2;
   void        *  value = (void*) 0x01020304;
   nodeoffsets_t  off;

   // prepare
   TEST(0 == ALLOC_MM(UINT16_MAX, &key));
   for (unsigned i = 0; i < UINT16_MAX; ++i) {
      key.addr[i] = (uint8_t) (i * 47);
   }
   for (unsigned i = 0; i < lengthof(digit); ++i) {
      digit[i] = (uint8_t) i;
      child[i] = (void*) (0xf0000f + (i << 16));
   }
   size_allocated = SIZEALLOCATED_MM();

   // == depth 0 ==

   // TEST insert_trie: (depth 0)
   for (unsigned keylen = 0; keylen <= 3*MAXKEYLEN; ++keylen) {
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)(0x01020304 + keylen)));
      size_used = SIZEALLOCATED_MM() - size_allocated;
      TEST(0 != trie.root);
      TEST(0 != size_used);
      TEST(0 == compare_nodechain(trie.root, size_used, keylen, key.addr, (void*)(0x01020304 + keylen)));

      // EEXIST is logged
      if (keylen == 10) {
         GETBUFFER_ERRLOG(&logbuffer, &logsize1);
         TEST(EEXIST == insert_trie(&trie, (uint16_t)keylen, key.addr, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize2 > logsize1);
      }

      TEST(0 == free_trie(&trie));
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // TEST insert_trie: ENOMEM (depth 0)
   for (unsigned keylen = 0; keylen <= UINT16_MAX; keylen += UINT16_MAX) {
      init_testerrortimer(&s_trie_errtimer, keylen != UINT16_MAX ? 1 : 1 + UINT16_MAX / MAXKEYLEN, ENOMEM);
      TEST(ENOMEM == insert_trie(&trie, (uint16_t)keylen, key.addr, 0));
      TEST(SIZEALLOCATED_MM() == size_allocated);
      TEST(0 == trie.root);
   }

   // == depth 1 ==

   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   for (unsigned keylen = 0, isENOMEM = 0; keylen <= 2*sizeof(void*); ++keylen) {
      for (unsigned nrchild = 1; nrchild <= MAXNROFCHILD+1; ++nrchild) {
         if (nrchild == 4) nrchild = MAXNROFCHILD+1;
         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit, child, 0));
         get_node_size(calc_used_size(keylen, nrchild, true), &nodesize, &sizeflags);
         oldheader = trie.root->header;
         header_t expectheader = addflags_header(delflags_header(oldheader, header_SIZEMASK),(header_t)(sizeflags|header_VALUE));

         // TEST insert_trie: ENOMEM add value (expand node if necessary) (depth 1)
         if (expectheader != oldheader && isENOMEM == 0) {
            isENOMEM = 1;
            size_t oldsize = SIZEALLOCATED_MM();
            init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
            TEST(ENOMEM == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)8));
            TEST(SIZEALLOCATED_MM() == oldsize);
            TEST(0 == compare_content( trie.root, oldheader,
                                       keylen, (uint8_t)nrchild, key.addr, digit, child, 0));
            GETBUFFER_ERRLOG(&logbuffer, &logsize2);
            TEST(logsize1 < logsize2); // log
            logsize1 = logsize2;
         }

         // TEST insert_trie: add value (expand node if necessary) (depth 1)
         TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)0x12345));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + (nrchild > MAXNROFCHILD?sizeof(trie_subnode_t):0));
         TEST(0 == compare_content( trie.root, expectheader,
                                    keylen, nrchild, key.addr, digit, child, (void*)0x12345));

         // TEST tryinsert_trie: EEXIST
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)keylen, key.addr, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 == logsize2); // no log

         TEST(0 == delete_trienode(&trie.root));
      }
   }

   for (unsigned i = 0; i < 2; ++i) {
      uint8_t keylen[2]  = { sizeof(void*),(uint8_t)(2*sizeof(void*)-off1_keylen_trienode()) };
      uint8_t nrchild[2] = { MAXNROFCHILD,  MAXNROFCHILD-1 };
      TEST(0 == new_trienode(&trie.root, keylen[i], nrchild[i], key.addr, digit, child, 0));
      TEST(MAXSIZE == nodesize_trienode(trie.root));
      get_node_size(calc_used_size(keylen[i], MAXNROFCHILD+1, true), &nodesize, &sizeflags);
      sizeflags = (header_t) (sizeflags|(trie.root->header&header_KEYLENMASK));

      // TEST insert_trie: ENOMEM add value / restructure into subnode (depth 1)
      if (i == 0) {
         size_t oldsize = SIZEALLOCATED_MM();
         oldheader = trie.root->header;
         init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
         TEST(ENOMEM == insert_trie(&trie, keylen[i], key.addr, (void*)8));
         TEST(SIZEALLOCATED_MM() == oldsize);
         TEST(0 == compare_content( trie.root, oldheader,
                                    keylen[i], nrchild[i], key.addr, digit, child, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 < logsize2); // log
         logsize1 = logsize2;
      }

      // TEST insert_trie: add value / restructure into subnode (depth 1)
      TEST(0 == insert_trie(&trie, keylen[i], key.addr, (void*)(keylen[i]+1)));
      TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + sizeof(trie_subnode_t));
      TEST(0 == compare_content( trie.root, (header_t)(sizeflags|header_VALUE|header_SUBNODE),
                                 keylen[i], nrchild[i], key.addr, digit, child, (void*)(keylen[i]+1)));

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, keylen[i], key.addr, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log

      TEST(0 == delete_trienode(&trie.root));
   }

   for (unsigned i = 0; i < 2; ++i) {
      uint8_t keylen[2]  = { MAXKEYLEN-1, MAXKEYLEN };
      uint8_t nrchild[2] = { 1,           MAXNROFCHILD+1 };
      TEST(0 == new_trienode(&trie.root, keylen[i], nrchild[i], key.addr, digit, child, 0));
      TEST(MAXSIZE == nodesize_trienode(trie.root));
      get_node_size(calc_used_size(0, nrchild[i], true), &nodesize, &sizeflags);
      sizeflags = (header_t) (sizeflags|(nrchild[i] > MAXNROFCHILD?header_SUBNODE:0));

      // TEST insert_trie: ENOMEM add value / restructure extract key (depth 1)
      if (i == 0) {
         size_t oldsize = SIZEALLOCATED_MM();
         oldheader = trie.root->header;
         init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
         TEST(ENOMEM == insert_trie(&trie, keylen[i], key.addr, (void*)8));
         TEST(SIZEALLOCATED_MM() == oldsize);
         TEST(0 == compare_content( trie.root, oldheader,
                                    keylen[i], nrchild[i], key.addr, digit, child, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 < logsize2); // log
         logsize1 = logsize2;
      }

      // TEST insert_trie: add value / restructure extract key (depth 1)
      TEST(0 == insert_trie(&trie, keylen[i], key.addr, (void*)(keylen[i]+1)));
      TEST(SIZEALLOCATED_MM() == size_allocated + MAXSIZE + nodesize + (nrchild[i] > MAXNROFCHILD?sizeof(trie_subnode_t):0));
      trie_node_t * node = childs_trienode(trie.root, childoff4_trienode(trie.root))[0];
      TEST(0 == compare_content( trie.root, (header_t)((header_SIZEMAX<<header_SIZESHIFT)|header_KEYLENBYTE),
                                 keylen[i]-1u, 1, key.addr, key.addr+keylen[i]-1, &node, 0));
      TEST(0 == compare_content( node, (header_t)(sizeflags|header_VALUE),
                                 0, nrchild[i], 0, digit, child, (void*)(keylen[i]+1)));

      TEST(0 == delete_trienode(&node));
      TEST(0 == delete_trienode(&trie.root));
   }

   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   for (unsigned nrchild = 0, isENOMEM = 0; nrchild < MAXNROFCHILD; nrchild += (nrchild>4?5:1)) {
      for (unsigned keylen = 0; keylen <= 2*sizeof(void*); keylen += 3) {
         unsigned childidx[3] = { 0, nrchild/2, nrchild };
         for (unsigned i = 0; i < 3; ++i) {
            if (i && childidx[i-1] == childidx[i]) continue;
            memcpy(digit2, digit, childidx[i]);
            memcpy(digit2+childidx[i], digit+childidx[i]+1, nrchild-childidx[i]);
            memcpy(child2, child, childidx[i]*sizeof(child[0]));
            memcpy(child2+childidx[i], child+childidx[i]+1, (nrchild-childidx[i])*sizeof(child[0]));
            memcpy(key2, key.addr, keylen);
            key2[keylen] = digit[childidx[i]];
            TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
            TEST(keylen == keylen_trienode(trie.root));
            get_node_size(calc_used_size(keylen, nrchild+1, false), &nodesize, &sizeflags);
            oldheader = trie.root->header;
            header_t expectheader = addflags_header(delflags_header(oldheader, header_SIZEMASK), sizeflags);

            // TEST insert_trie: ENOMEM add child to child array (expand if necessary) (depth 1)
            if (expectheader != oldheader && isENOMEM == 0) {
               isENOMEM = 1;
               for (unsigned e = 1; e <= 2; ++e) {
                  size_t oldsize = SIZEALLOCATED_MM();
                  init_testerrortimer(&s_trie_errtimer, e, ENOMEM);
                  TEST(ENOMEM == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)8));
                  TEST(SIZEALLOCATED_MM() == oldsize);
                  TEST(0 == compare_content( trie.root, oldheader,
                                             keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
                  GETBUFFER_ERRLOG(&logbuffer, &logsize2);
                  TEST(logsize1 < logsize2); // log
                  logsize1 = logsize2;
               }
            }

            // TEST insert_trie: add child to child array (expand if necessary) (depth 1)
            TEST(0 == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)(keylen+3)));
            TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + MINSIZE);
            memcpy(child2, child, (nrchild+1)*sizeof(child[0]));
            trie_node_t * node = childs_trienode(trie.root, childoff4_trienode(trie.root))[childidx[i]];
            child2[childidx[i]] = node;
            TEST(0 == compare_content( trie.root, expectheader,
                                       keylen, nrchild+1, key.addr, digit, child2, 0));
            TEST(0 == compare_content( node, header_VALUE,
                                       0, 0, 0, 0, 0, (void*)(keylen+3)));

            // TEST tryinsert_trie: EEXIST
            TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen+1), key2, 0));
            GETBUFFER_ERRLOG(&logbuffer, &logsize2);
            TEST(logsize1 == logsize2); // no log

            TEST(0 == delete_trienode(&node));
            TEST(0 == delete_trienode(&trie.root));
         }
      }
   }

   for (unsigned keylen = 0; keylen < 2*sizeof(void*); ++keylen) {
      unsigned nrchild = (MAXSIZE-off1_keylen_trienode()-keylen-needkeylenbyte_header((uint8_t)keylen))
                       / (1+sizeof(void*));
      unsigned childidx[4] = { 0, nrchild/2-2, nrchild };
      for (unsigned i = 0; i < 4; ++i) {
         memcpy(digit2, digit, childidx[i]);
         memcpy(digit2+childidx[i], digit+childidx[i]+1, nrchild-childidx[i]);
         memcpy(child2, child, childidx[i]*sizeof(child[0]));
         memcpy(child2+childidx[i], child+childidx[i]+1, (nrchild-childidx[i])*sizeof(child[0]));
         memcpy(key2, key.addr, keylen);
         key2[keylen] = digit[childidx[i]];
         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
         TEST(keylen == keylen_trienode(trie.root));
         TEST(MAXSIZE == nodesize_trienode(trie.root));
         get_node_size(calc_used_size(keylen, MAXNROFCHILD+1, false), &nodesize, &sizeflags);
         oldheader = trie.root->header;

         // TEST insert_trie: ENOMEM add child to child array / restructure into subnode (depth 1)
         if (keylen == sizeof(void*) && i == 2) {
            for (unsigned e = 1; e <= 2; ++e) {
               size_t oldsize = SIZEALLOCATED_MM();
               oldheader = trie.root->header;
               init_testerrortimer(&s_trie_errtimer, e, ENOMEM);
               TEST(ENOMEM == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)8));
               TEST(SIZEALLOCATED_MM() == oldsize);
               TEST(0 == compare_content( trie.root, oldheader,
                                          keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
               GETBUFFER_ERRLOG(&logbuffer, &logsize2);
               TEST(logsize1 < logsize2); // log
               logsize1 = logsize2;
            }
         }

         // TEST insert_trie: add child to child array / restructure into subnode (depth 1)
         TEST(0 == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)(keylen+3)));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + MINSIZE + sizeof(trie_subnode_t));
         header_t expectheader = addflags_header(delflags_header(oldheader, header_SIZEMASK),sizeflags|header_SUBNODE);
         memcpy(child2, child, (nrchild+1)*sizeof(child[0]));
         trie_node_t * node = child_triesubnode(subnode_trienode(trie.root, childoff4_trienode(trie.root)), digit[childidx[i]]);
         child2[childidx[i]] = node;
         TEST(0 == compare_content( trie.root, expectheader,
                                    keylen, nrchild+1, key.addr, digit, child2, 0));
         TEST(0 == compare_content( node, header_VALUE,
                                    0, 0, 0, 0, 0, (void*)(keylen+3)));

         // TEST tryinsert_trie: EEXIST
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen+1), key2, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 == logsize2); // no log

         TEST(0 == delete_trienode(&node));
         TEST(0 == delete_trienode(&trie.root));
      }
   }

   for (unsigned keylen = 2*sizeof(void*); keylen <= MAXKEYLEN; ++keylen) {
      unsigned nrchild = (MAXSIZE-calc_used_size(keylen, 0, false)) / (1+sizeof(trie_node_t*));
      unsigned childidx[3] = { 0, nrchild/2, nrchild };
      for (unsigned i = 0; i < 3; ++i) {
         if (i && childidx[i-1] == childidx[i]) continue;
         memcpy(digit2, digit, childidx[i]);
         memcpy(digit2+childidx[i], digit+childidx[i]+1, nrchild-childidx[i]);
         memcpy(child2, child, childidx[i]*sizeof(child[0]));
         memcpy(child2+childidx[i], child+childidx[i]+1, (nrchild-childidx[i])*sizeof(child[0]));
         memcpy(key2, key.addr, keylen);
         key2[keylen] = digit[childidx[i]];
         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
         TEST(keylen  == keylen_trienode(trie.root));
         TEST(MAXSIZE == nodesize_trienode(trie.root));
         get_node_size(calc_used_size(0, nrchild+1, false), &nodesize, &sizeflags);
         header_t root_sizeflags;
         unsigned root_nodesize;
         get_node_size(calc_used_size(keylen-1, 1, false), &root_nodesize, &root_sizeflags);

         // TEST insert_trie: ENOMEM add child to child array / restructure extract key (depth 1)
         if (keylen == 5*sizeof(void*) && i == 1) {
            for (unsigned e = 1; e <= 2; ++e) {
               size_t oldsize = SIZEALLOCATED_MM();
               oldheader = trie.root->header;
               init_testerrortimer(&s_trie_errtimer, e, ENOMEM);
               TEST(ENOMEM == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)(keylen+13)));
               TEST(SIZEALLOCATED_MM() == oldsize);
               TEST(0 == compare_content( trie.root, oldheader,
                                          keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
               GETBUFFER_ERRLOG(&logbuffer, &logsize2);
               TEST(logsize1 < logsize2); // log
               logsize1 = logsize2;
            }
         }

         // TEST insert_trie: add child to child array / restructure extract key (depth 1)
         TEST(0 == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)(keylen+13)));
         TEST(SIZEALLOCATED_MM() == size_allocated + root_nodesize + nodesize + MINSIZE);
         trie_node_t * node = childs_trienode(trie.root, childoff4_trienode(trie.root))[0];
         TEST(0 == compare_content( trie.root, (header_t)(root_sizeflags|header_KEYLENBYTE),
                                    keylen-1, 1, key.addr, key.addr+keylen-1, &node, 0));
         trie_node_t * node_value = childs_trienode(node, childoff4_trienode(node))[childidx[i]];
         memcpy(child2, child, (nrchild+1)*sizeof(child[0]));
         child2[childidx[i]] = node_value;
         TEST(0 == compare_content( node, sizeflags,
                                    0, nrchild+1, 0, digit, child2, 0));
         TEST(0 == compare_content( node_value, header_VALUE,
                                    0, 0, 0, 0, 0, (void*)(keylen+13)));

         // TEST tryinsert_trie: EEXIST
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen+1), key2, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 == logsize2); // no log

         TEST(0 == delete_trienode(&node_value));
         TEST(0 == delete_trienode(&node));
         TEST(0 == delete_trienode(&trie.root));
      }
   }

   for (unsigned childidx = 0; childidx <= 255; ++childidx) {
      unsigned keylen  = (childidx % 5*sizeof(void*));
      unsigned nrchild = 255;
      memcpy(digit2, digit, childidx);
      memcpy(digit2+childidx, digit+childidx+1, nrchild-childidx);
      memcpy(child2, child, childidx*sizeof(child[0]));
      memcpy(child2+childidx, child+childidx+1, (nrchild-childidx)*sizeof(child[0]));
      memcpy(key2, key.addr, keylen);
      key2[keylen] = digit[childidx];
      TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digit2, child2, 0));
      TEST(0 != issubnode_trienode(trie.root));
      nodesize  = nodesize_trienode(trie.root) + sizeof(trie_subnode_t);
      oldheader = trie.root->header;

      // TEST insert_trie: ENOMEM add child to subnode (depth 1)
      if (childidx == 2) {
         init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
         TEST(ENOMEM == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)2));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize);
         TEST(0 == compare_content( trie.root, oldheader,
                                    keylen, nrchild, key.addr, digit2, child2, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 < logsize2); // log
         logsize1 = logsize2;
      }

      // TEST insert_trie: add child to subnode (depth 1)
      TEST(0 == insert_trie(&trie, (uint16_t)(keylen+1), key2, (void*)(keylen+13)));
      TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + MINSIZE);
      trie_node_t * node = child_triesubnode(subnode_trienode(trie.root, childoff4_trienode(trie.root)), digit[childidx]);
      memcpy(child2, child, (nrchild+1)*sizeof(child[0]));
      child2[childidx] = node;
      TEST(0 == compare_content( trie.root, oldheader,
                                 keylen, nrchild+1, key.addr, digit, child2, 0));
      TEST(0 == compare_content( node, header_VALUE,
                                 0, 0, 0, 0, 0, (void*)(keylen+13)));

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen+1), key2, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log

      TEST(0 == delete_trienode(&node));
      TEST(0 == delete_trienode(&trie.root));
   }

   for (unsigned keylen = 1; keylen <= MAXKEYLEN; ++keylen) {
      for (unsigned splitkeylen = 0; splitkeylen < keylen; ++splitkeylen) {
         if (splitkeylen == 10 && keylen > COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*)) splitkeylen = COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*);
         if (splitkeylen == COMPUTEKEYLEN(128)+2 && splitkeylen < keylen-2) splitkeylen = keylen-2;

         unsigned splitparent_keylen = (splitkeylen <= COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*))
                                     ? 0 : splitkeylen > COMPUTEKEYLEN(128)
                                           ? COMPUTEKEYLEN(128) : splitkeylen;
         unsigned splitnode_keylen   = splitkeylen - splitparent_keylen;

         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, 0, key.addr, 0, 0, &value));

         // TEST insert_trie: ENOMEM add value to splitted node (depth 1)
         if (  (keylen == MAXKEYLEN && splitkeylen == MAXKEYLEN-1)
               || (keylen == COMPUTEKEYLEN(128) && splitkeylen == COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*))) {
            nodesize = nodesize_trienode(trie.root);
            for (unsigned i = 1; i <= 2+(splitkeylen == MAXKEYLEN-1); ++i) {
               init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
               TEST(ENOMEM == insert_trie(&trie, (uint16_t)splitkeylen, key.addr, (void*)0x02030405));
               TEST(SIZEALLOCATED_MM() == size_allocated + nodesize);
            }
            GETBUFFER_ERRLOG(&logbuffer, &logsize1);
         }

         header_t splitnode_sizeflags;
         header_t splitparent_sizeflags;
         unsigned splitnode_size;
         unsigned splitparent_size = 0;
         get_node_size( calc_used_size(keylen-1-splitkeylen, 0, true), &nodesize, &sizeflags);
         get_node_size( calc_used_size(splitnode_keylen, 1, true), &splitnode_size, &splitnode_sizeflags);
         if (splitparent_keylen) get_node_size( 128, &splitparent_size, &splitparent_sizeflags);

         // TEST insert_trie: add value to splitted node (depth 1)
         TEST(0 == insert_trie(&trie, (uint16_t)splitkeylen, key.addr, (void*)0x02030405));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + splitnode_size + splitparent_size);
         trie_node_t * splitnode = trie.root;
         if (splitparent_size) {
            splitnode = childs_trienode(trie.root, childoff4_trienode(trie.root))[0];
            TEST(0 == compare_content( trie.root, (header_t)((trie.root->header&header_KEYLENMASK)|splitparent_sizeflags),
                                       splitparent_keylen-1, 1, key.addr, key.addr+splitparent_keylen-1, &splitnode, 0));
         }
         trie_node_t * node = childs_trienode(splitnode, childoff4_trienode(splitnode))[0];
         TEST(0 == compare_content( splitnode, (header_t)((splitnode->header&header_KEYLENMASK)|splitnode_sizeflags|header_VALUE),
                                    splitnode_keylen, 1, key.addr+splitparent_keylen, key.addr+splitkeylen, &node, (void*)0x02030405));
         TEST(0 == compare_content( node, (header_t)((node->header&header_KEYLENMASK)|sizeflags|header_VALUE),
                                    keylen-1-splitkeylen, 0, key.addr+splitkeylen+1, 0, 0, value));

         // TEST tryinsert_trie: EEXIST
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)splitkeylen, key.addr, 0));
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)keylen, key.addr, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 == logsize2); // no log

         TEST(0 == free_trie(&trie));
         TEST(SIZEALLOCATED_MM() == size_allocated);
      }
   }

   for (unsigned keylen = 1; keylen <= MAXKEYLEN; ++keylen) {
      for (unsigned splitkeylen = 1; splitkeylen <= keylen; ++splitkeylen) {
         if (splitkeylen == 10 && keylen > COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*)) splitkeylen = COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*);
         if (splitkeylen == COMPUTEKEYLEN(128)+2 && splitkeylen < keylen-2) splitkeylen = keylen-2;

         unsigned splitparent_keylen = ((splitkeylen-1) <= COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*))
                                     ? 0 : (splitkeylen-1) > COMPUTEKEYLEN(128)
                                           ? COMPUTEKEYLEN(128) : (splitkeylen-1);
         unsigned splitnode_keylen   = splitkeylen - 1 - splitparent_keylen;

         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, 0, key.addr, 0, 0, &value));
         // key2 differs in last digit
         memcpy(key2, key.addr, splitkeylen);
         key2[splitkeylen-1] = (uint8_t) (key2[splitkeylen-1] + ((splitkeylen&1) ? +1 : -1));

         // TEST insert_trie: ENOMEM add child to splitted node (depth 1)
         if (  (keylen == MAXKEYLEN && splitkeylen == MAXKEYLEN-1)
               || (keylen == COMPUTEKEYLEN(128) && splitkeylen == COMPUTEKEYLEN(128)-2-sizeof(trie_node_t*))) {
            nodesize = nodesize_trienode(trie.root);
            for (unsigned i = 1; i <= 3+(splitkeylen == MAXKEYLEN-1); ++i) {
               init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
               TEST(ENOMEM == insert_trie(&trie, (uint16_t)splitkeylen, key2, (void*)0x02030405));
               TEST(SIZEALLOCATED_MM() == size_allocated + nodesize);
            }
            GETBUFFER_ERRLOG(&logbuffer, &logsize1);
         }

         header_t splitnode_sizeflags;
         header_t splitparent_sizeflags;
         unsigned splitnode_size;
         unsigned splitparent_size = 0;
         get_node_size( calc_used_size(keylen-splitkeylen, 0, true), &nodesize, &sizeflags);
         get_node_size( calc_used_size(splitnode_keylen, 2, false), &splitnode_size, &splitnode_sizeflags);
         if (splitparent_keylen) get_node_size( 128, &splitparent_size, &splitparent_sizeflags);

         // TEST insert_trie: add child to splitted node (depth 1)
         TEST(0 == insert_trie(&trie, (uint16_t)splitkeylen, key2, (void*)0x02030405));
         TEST(SIZEALLOCATED_MM() == size_allocated + nodesize + splitnode_size + splitparent_size + MINSIZE);
         trie_node_t * splitnode = trie.root;
         if (splitparent_size) {
            splitnode = childs_trienode(trie.root, childoff4_trienode(trie.root))[0];
            TEST(0 == compare_content( trie.root, (header_t)((trie.root->header&header_KEYLENMASK)|splitparent_sizeflags),
                                       splitparent_keylen-1, 1, key.addr, key.addr+splitparent_keylen-1, &splitnode, 0));
         }
         trie_node_t **splitchilds = childs_trienode(splitnode, childoff4_trienode(splitnode));
         uint8_t       splitdigits[2];
         uint8_t       valueidx = (key2[splitkeylen-1] > key.addr[splitkeylen-1]);
         splitdigits[valueidx]  = key2[splitkeylen-1];
         splitdigits[!valueidx] = key.addr[splitkeylen-1];
         TEST(0 == compare_content( splitnode, (header_t)((splitnode->header&header_KEYLENMASK)|splitnode_sizeflags),
                                    splitnode_keylen, 2, key.addr+splitparent_keylen, splitdigits, splitchilds, 0));
         trie_node_t * node    = splitchilds[!valueidx];
         trie_node_t * valnode = splitchilds[valueidx];
         TEST(0 == compare_content( node, (header_t)((node->header&header_KEYLENMASK)|sizeflags|header_VALUE),
                                    keylen-splitkeylen, 0, key.addr+splitkeylen, 0, 0, value));
         TEST(0 == compare_content( valnode, (header_t)(header_VALUE),
                                    0, 0, 0, 0, 0, (void*)0x02030405));

         // TEST tryinsert_trie: EEXIST
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)splitkeylen, key2, 0));
         TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)keylen, key.addr, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TEST(logsize1 == logsize2); // no log

         TEST(0 == free_trie(&trie));
         TEST(SIZEALLOCATED_MM() == size_allocated);
      }
   }

   // == depth X: follow node chain ==

   // * 5. Now test, that insert finds the correct node and applies all transformations
   // *    of depth 1 correctly to the found node.

   // TEST insert_trie: add value (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)0x56789abc));
      TEST(0 != isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      TEST((void*)0x56789abc == value_trienode(*parentchild, off.off5_value));

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)keylen, key.addr, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST insert_trie: add child to child array or subnode (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      trie_node_t  * node;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      TEST(0 == insert_trie(&trie, (uint16_t)(keylen+1), key.addr, (void*)0x56789abc));
      TEST(0 == isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      if (issubnode_trienode(*parentchild)) {
         TEST(1 == nrchild_trienode(*parentchild));
         trie_subnode_t * subnode = subnode_trienode(*parentchild, off.off4_child);
         node = child_triesubnode(subnode, key.addr[keylen]);
      } else {
         TEST(2 == nrchild_trienode(*parentchild));
         if (key.addr[keylen] == digits_trienode(*parentchild, off.off3_digit)[0]) {
            node = childs_trienode(*parentchild, off.off4_child)[0];
         } else {
            TEST(key.addr[keylen] == digits_trienode(*parentchild, off.off3_digit)[1]);
            node = childs_trienode(*parentchild, off.off4_child)[1];
         }
      }
      init_nodeoffsets(&off, node);
      TEST(0 != isvalue_trienode(node));
      TEST((void*)0x56789abc == value_trienode(node, off.off5_value));

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen+1), key.addr, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST insert_trie: add value to splitted node (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      unsigned node_keylen = keylen_trienode(*parentchild);
      TEST(0 == insert_trie(&trie, (uint16_t)(keylen-node_keylen), key.addr, (void*)0x56789abc));
      TEST(0 != isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      TEST(1 == nrchild_trienode(*parentchild));
      TEST((void*)0x56789abc == value_trienode(*parentchild, off.off5_value));
      trie_node_t * node = childs_trienode(*parentchild, off.off4_child)[0];
      TEST(key.addr[keylen-node_keylen] == digits_trienode(*parentchild, off.off3_digit)[0]);
      init_nodeoffsets(&off, node);
      TEST(0 == isvalue_trienode(node));
      TEST(node_keylen == keylen_trienode(node)+1u);

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen-node_keylen), key.addr, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST insert_trie: add child to splitted node (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      unsigned node_keylen = keylen_trienode(*parentchild);
      ++ key.addr[keylen-node_keylen];
      TEST(0 == insert_trie(&trie, (uint16_t)(keylen-node_keylen+1), key.addr, (void*)0x56789abc));
      TEST(0 == isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      TEST(2 == nrchild_trienode(*parentchild));
      trie_node_t * node = childs_trienode(*parentchild, off.off4_child)[1];
      if (key.addr[keylen-node_keylen] == digits_trienode(*parentchild, off.off3_digit)[0]) {
         node = childs_trienode(*parentchild, off.off4_child)[0];
      } else {
         TEST(key.addr[keylen-node_keylen] == digits_trienode(*parentchild, off.off3_digit)[1]);
      }
      init_nodeoffsets(&off, node);
      TEST(0 != isvalue_trienode(node));
      TEST((void*)0x56789abc == value_trienode(node, off.off5_value));

      // TEST tryinsert_trie: EEXIST
      TEST(EEXIST == tryinsert_trie(&trie, (uint16_t)(keylen-node_keylen+1), key.addr, 0));
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log
      -- key.addr[keylen-node_keylen];

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // unprepare
   TEST(0 == FREE_MM(&key));

   return 0;
ONABORT:
   delete_trienode(&trie.root);
   FREE_MM(&key);
   return EINVAL;
}

/* function: test_remove
 * Test remove (key,value) pair from <trie_t>.
 *
 * The following is tested:
 * == depth 0 ==
 * 1. Test remove from an empty trie and which results in an empty trie.
 *    1.1 EEXIST is returned in the first case and nothing else happens.
 *    1.2 Root points to a single chain of nodes. Removing the value from the last node
 *        deletes the whole chain of nodes. The trie is empty afterwards.
 * == depth 1 ==
 * 2. A chain of nodes begins at child pointer of root node.
 *    Removing value at end of chain removes chain root from child array or subnode.
 *    2.1 A root node with a single child and a value keeps its value and nrchild is 0 afterwards.
 *        Root node is not removed as part of node chain.
 *    2.2 A root node with childs and a value removed keeps its childs afterwards.
 *    2.3 A root node with child array is resized if it fits in a smaller node size.
 *    2.4 A root node with subnode is converted into a node with child array if subnode contains less MAXNROFCHILD-2
 * == depth X: follow node chain ==
 * 3. Now test, that remove finds the correct node and applies all transformations
 *    of depth 1 correctly to the found node.
 * == Error Codes ==
 * 4. Test error codes of remove (no change of trie).
 * 5. Test logging and non logging of ESRCH of remove and tryremove
 */
static int test_remove(void)
{
   trie_t      trie  = trie_INIT;
   void      * value;
   memblock_t  key   = memblock_FREE;
   uint8_t   * logbuffer;
   trie_node_t * childs[256] = { 0 };
   uint8_t     digits[256];
   size_t      logsize1;
   size_t      logsize2;
   size_t      size_allocated;
   nodeoffsets_t off;
   unsigned    nodesize;
   header_t    sizeflags;
   header_t    oldheader;

   // prepare
   TEST(0 == ALLOC_MM(UINT16_MAX, &key));
   for (unsigned i = 0; i < UINT16_MAX; ++i) {
      key.addr[i] = (uint8_t) (i * 69);
   }
   for (unsigned i = 0; i < 256; ++i) {
      digits[i] = (uint8_t)i;
      childs[i] = (void*) (1+i);
   }
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   size_allocated = SIZEALLOCATED_MM();

   // == depth 0 ==

   // TEST tryremove_trie: empty trie
   value = (void*)0x12345;
   for (unsigned keylen = 0; keylen < MAXKEYLEN; ++keylen) {
      TEST(ESRCH == tryremove_trie(&trie, (uint16_t)keylen, (const uint8_t*)""/*not used if root == 0*/, &value))
      TEST(trie.root == 0);
      TEST(value     == (void*)0x12345);
      GETBUFFER_ERRLOG(&logbuffer, &logsize2);
      TEST(logsize1 == logsize2); // no log written in case of ESRCH
   }

   // TEST remove_trie: single node or chain of nodes
   for (unsigned keylen = 0; keylen < 5*MAXKEYLEN; ++keylen) {
      if (keylen >= MAXKEYLEN) keylen += MAXKEYLEN;
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)(keylen+0x54321)));

      // ESRCH
      if (keylen == 2*MAXKEYLEN) {
         for (unsigned keylen2 = 0; keylen2 < keylen; ++keylen2) {
            value = (void*)keylen2;
            TEST(ESRCH == tryremove_trie(&trie, (uint16_t)keylen2, key.addr, &value));
            ++ key.addr[keylen2-(keylen2 > 0)];
            TEST(ESRCH == tryremove_trie(&trie, (uint16_t)keylen2, key.addr, &value));
            -- key.addr[keylen2-(keylen2 > 0)];
            TEST(value == (void*)keylen2/*not changed*/);
            GETBUFFER_ERRLOG(&logbuffer, &logsize2);
            TEST(logsize1 == logsize2); // no log written in case of ESRCH
         }
      }

      // free error is ignored
      init_testerrortimer(&s_trie_errtimer, keylen/MAXKEYLEN, EINVAL);
      TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &value));
      TEST(0 == isenabled_testerrortimer(&s_trie_errtimer));
      TEST(trie.root == 0);
      TEST(value     == (void*)(keylen+0x54321));
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // == depth 1 ==

   // TEST remove_trie: root node has value + 1 child ==> child removed ==> root is kept
   for (unsigned keylen = MAXKEYLEN+1; keylen < 5*MAXKEYLEN; ++keylen) {
      if (keylen >= MAXKEYLEN) keylen += MAXKEYLEN;
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, (void*)(keylen+0x54321)));
      init_nodeoffsets(&off, trie.root);
      TEST(0 == tryaddvalue_trienode(&trie.root, off.off4_child, (void*)keylen));
      TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &value));
      TEST(value == (void*)(keylen+0x54321));
      TEST(0 != trie.root);
      TEST(0 == nrchild_trienode(trie.root));
      TEST(0 != isvalue_trienode(trie.root));
      TEST(0 == remove_trie(&trie, keylen_trienode(trie.root), key.addr, &value));
      TEST(trie.root == 0);
      TEST(value     == (void*)keylen);
      TEST(SIZEALLOCATED_MM() == size_allocated);
   }

   // TEST remove_trie: root node has value + childs ==> value of root removed ==> root is kept
   for (unsigned keylen = 0; keylen <= 2*sizeof(void*); ++keylen) {
      for (unsigned nrchild = 1; nrchild <= MAXNROFCHILD-2; ++nrchild) {
         value = (void*) (keylen+nrchild);
         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digits, childs, &value));
         oldheader = trie.root->header;
         value = 0;
         TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &value));
         TEST(0 != trie.root);
         TEST(value == (void*)(keylen+nrchild));
         TEST(0 == compare_content(trie.root, delflags_header(oldheader, header_VALUE),
                                   keylen, nrchild, key.addr, digits, childs, 0));
         TEST(0 == delete_trienode(&trie.root));
         TEST(SIZEALLOCATED_MM() == size_allocated);
      }
   }

   // TEST remove_trie: child array + possible resize
   for (int isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen < MAXKEYLEN; ++keylen) {
         for (unsigned nrchild = 2; nrchild <= MAXNROFCHILD; ++nrchild) {
            if (calc_used_size(keylen, nrchild, isvalue) > MAXSIZE) break;
            get_node_size(calc_used_size(keylen, nrchild-1, isvalue), &nodesize, &sizeflags); // check for resize
            for (unsigned childidx = 0; childidx < nrchild; ++childidx) {
               value = (void*)(keylen + childidx);
               TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, (uint8_t)nrchild, key.addr, digits, childs, isvalue ? &value : 0));
               uint8_t expect_digits[lengthof(digits)];
               trie_node_t * expect_childs[lengthof(childs)];
               memcpy(expect_digits, digits, sizeof(expect_digits));
               memcpy(expect_childs, childs, sizeof(expect_childs));
               memcpy(expect_digits+childidx, digits+childidx+1, sizeof(uint8_t)*(lengthof(expect_digits)-childidx-1));
               memcpy(expect_childs+childidx, childs+childidx+1, sizeof(trie_node_t*)*(lengthof(expect_childs)-childidx-1));
               init_nodeoffsets(&off, trie.root);
               TEST(0 == build_nodechain_trienode(&childs_trienode(trie.root, off.off4_child)[childidx], (uint16_t) (childidx*32), key.addr+keylen+1, (void*)childidx));
               uint8_t old = key.addr[keylen];
               key.addr[keylen] = digits[childidx];
               TEST(0 == remove_trie(&trie, (uint16_t)(keylen+1+32*childidx), key.addr, &value));
               key.addr[keylen] = old;
               TEST(value == (void*)childidx);
               TEST(0 == compare_content(trie.root, addflags_header(delflags_header(trie.root->header, header_SIZEMASK), sizeflags),
                                         keylen, nrchild-1, key.addr, expect_digits, expect_childs, (void*)(keylen + childidx)));
               TEST(0 == delete_trienode(&trie.root));
               TEST(SIZEALLOCATED_MM() == size_allocated);
            }
         }
      }
   }

   // TEST remove_trie: ENOMEM child array and resize ==> no resize
   get_node_size(calc_used_size(1, 1, true), &nodesize, &sizeflags);
   TEST(calc_used_size(1, 0, true) <= nodesize/2);
   TEST(0 == build_nodechain_trienode(&childs[0], 1, key.addr+2, (void*)0x33));
   TEST(0 == new_trienode(&trie.root, 1, 1, key.addr, key.addr+1, childs, &value));
   init_testerrortimer(&s_trie_errtimer, 2, ENOMEM);
   TEST(0 == remove_trie(&trie, 3, key.addr, &value));
   TEST(0 == isenabled_testerrortimer(&s_trie_errtimer));
   TEST(value == (void*)0x33);
   TEST(nodesize == nodesize_trienode(trie.root)); // no resize
   TEST(0 == delete_trienode(&trie.root));

   // TEST remove_trie: subnode + possible conversion into child array
   GETBUFFER_ERRLOG(&logbuffer, &logsize1);
   for (int isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned keylen = 0; keylen < 2*sizeof(void*); ++keylen) {
         value = (void*)(5 * keylen + 1);
         TEST(0 == new_trienode(&trie.root, (uint8_t)keylen, 255, key.addr, digits, childs, isvalue ? &value : 0));
         init_nodeoffsets(&off, trie.root);
         trie_subnode_t * subnode = subnode_trienode(trie.root, off.off4_child);
         oldheader = trie.root->header;
         uint8_t old = key.addr[keylen];
         for (unsigned childidx = 254; childidx >= MAXNROFCHILD-2; --childidx) {
            // no conversion
            TEST(0 == build_nodechain_trienode(&subnode->child[childidx], (uint16_t) (childidx*5), key.addr+keylen+1, (void*)childidx));
            key.addr[keylen] = (uint8_t) childidx;
            TEST(0 == remove_trie(&trie, (uint16_t)(keylen+1+5*childidx), key.addr, &value));
            TEST(value == (void*)childidx);
            TEST(0 == subnode->child[childidx]);
            TEST(0 == compare_content(trie.root, oldheader,
                                      keylen, childidx, key.addr, digits, childs, (void*)(5*keylen + 1)));
            // ESRCH
            TEST(ESRCH == tryremove_trie(&trie, (uint16_t)(keylen+1), key.addr, &value));
            key.addr[keylen] = old;
            GETBUFFER_ERRLOG(&logbuffer, &logsize2);
            TEST(logsize1 == logsize2); // no log written in case of ESRCH
         }
         // conversion with ENOMEM ==> no conversion into child
         TEST(0 == build_nodechain_trienode(&subnode->child[MAXNROFCHILD-3], 2, key.addr+keylen+1, (void*)0x44881));
         key.addr[keylen] = MAXNROFCHILD-3;
         init_testerrortimer(&s_trie_errtimer, 2, ENOMEM);
         TEST(0 == remove_trie(&trie, (uint16_t)(keylen+1+2), key.addr, &value));
         TEST(value == (void*)0x44881);
         TEST(0 == compare_content(trie.root, oldheader,
                                   keylen, MAXNROFCHILD-3, key.addr, digits, childs, (void*)(5*keylen + 1)));
         ++trie.root->nrchild;

         // conversion
         TEST(0 == build_nodechain_trienode(&subnode->child[MAXNROFCHILD-3], 2*MAXNROFCHILD+12, key.addr+keylen+1, (void*)0x44321));
         key.addr[keylen] = MAXNROFCHILD-3;
         TEST(0 == remove_trie(&trie, (uint16_t)(keylen+1+2*MAXNROFCHILD+12), key.addr, &value));
         TEST(value == (void*)0x44321);
         TEST(0 == issubnode_trienode(trie.root));
         TEST(0 == compare_content(trie.root, (header_t) ((header_SIZEMAX<<header_SIZESHIFT)|(oldheader&(header_KEYLENMASK|header_VALUE))),
                                   keylen, MAXNROFCHILD-3, key.addr, digits, childs, (void*)(5*keylen + 1)));

         key.addr[keylen] = old;
         TEST(0 == delete_trienode(&trie.root));
         TEST(SIZEALLOCATED_MM() == size_allocated);
      }
   }

   // TEST remove_trie: subnode with a single child (error case) does not crash
   for (int isvalue = 0; isvalue <= 1; ++isvalue) {
      for (unsigned nrchild = 1; nrchild < 5; ++nrchild) {
         value = (void*)(5 * nrchild + 1);
         TEST(0 == new_trienode(&trie.root, 0, (uint8_t)nrchild, 0, digits, childs, isvalue ? &value : 0));
         init_nodeoffsets(&off, trie.root);
         TEST(0 == addsubnode_trienode(&trie.root, off.off3_digit, 0));
         init_nodeoffsets(&off, trie.root);
         trie_subnode_t * subnode = subnode_trienode(trie.root, off.off4_child);
         oldheader = trie.root->header;
         // conversion
         TEST(0 == build_nodechain_trienode(&subnode->child[digits[0]], 0, 0, (void*)0x4455321));
         TEST(0 == remove_trie(&trie, 1, &digits[0], &value));
         TEST(value == (void*)0x4455321);
         if (nrchild == 1) {
            TEST(0   != issubnode_trienode(trie.root));
            TEST(255 == nrchild_trienode(trie.root));
            for (unsigned i = 0; i < 256; ++i) {
               TEST(0 == child_triesubnode(subnode, (uint8_t)i));
            }
            trie.root->nrchild = 0;
            setchild_triesubnode(subnode, digits[1], childs[1]);
            TEST(0 == compare_content(trie.root, trie.root->header,
                                      0, 1, 0, digits+1, childs+1, (void*)(5*nrchild + 1)));
         } else {
            TEST(0 == issubnode_trienode(trie.root));
            TEST(0 == compare_content(trie.root, trie.root->header,
                                      0, nrchild-1, 0, digits+1, childs+1, (void*)(5*nrchild + 1)));
         }

         TEST(0 == delete_trienode(&trie.root));
         TEST(SIZEALLOCATED_MM() == size_allocated);
      }
   }

   // == depth X: follow node chain ==

   // 3. Now test, that remove finds the correct node and applies all transformations
   //    of depth 1 correctly to the found node.

   // TEST remove_trie: node has value + 1 child ==> child removed ==> node is kept (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      TEST(0 == isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      TEST(0 == tryaddvalue_trienode(parentchild, off.off4_child, (void*)1));
      init_nodeoffsets(&off, *parentchild);
      if (issubnode_trienode(*parentchild)) {
         trie_subnode_t * subnode = subnode_trienode(*parentchild, off.off4_child);
         TEST(0 == build_nodechain_trienode(childaddr_triesubnode(subnode, key.addr[keylen]), 2, key.addr+keylen+1, (void*)depth));
      } else {
         // build_depthx_trie adds one child with digit (key.addr[keylen]+1)
         digits_trienode(*parentchild, off.off3_digit)[0] = key.addr[keylen];
         TEST(0 == build_nodechain_trienode(childs_trienode(*parentchild, off.off4_child), 2, key.addr+keylen+1, (void*)depth));
      }

      // ESRCH
      if (depth == 8) {
         for (unsigned keylen2 = 0; keylen2 < keylen+3; ++keylen2) {
            if (keylen2 == keylen) continue; // this node contains a value
            value = (void*)keylen2;
            TEST(ESRCH == tryremove_trie(&trie, (uint16_t)keylen2, key.addr, &value));
            ++ key.addr[keylen2-(keylen2 > 0)];
            TEST(ESRCH == tryremove_trie(&trie, (uint16_t)keylen2, key.addr, &value));
            -- key.addr[keylen2-(keylen2 > 0)];
            TEST(value == (void*)keylen2/*not changed*/);
            GETBUFFER_ERRLOG(&logbuffer, &logsize2);
            TEST(logsize1 == logsize2); // no log written in case of ESRCH
         }
      }

      TEST(0 == remove_trie(&trie, (uint16_t)(keylen+3), key.addr, &value));
      TEST(value == (void*)depth);
      init_nodeoffsets(&off, *parentchild);
      if (issubnode_trienode(*parentchild)) {
         TEST(255/*underflow*/ == nrchild_trienode(*parentchild));
      } else {
         TEST(0 == nrchild_trienode(*parentchild));
      }

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST remove_trie: node has value + childs ==> value of node removed ==> node is kept  (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      TEST(0 == isvalue_trienode(*parentchild));
      init_nodeoffsets(&off, *parentchild);
      TEST(0 == tryaddvalue_trienode(parentchild, off.off4_child, (void*)(0x33+depth)));
      int issubnode = issubnode_trienode(*parentchild);

      TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &value));
      TEST(value == (void*)(0x33+depth));
      TEST(nrchild_trienode(*parentchild)   == 1-(issubnode != 0));
      TEST(issubnode_trienode(*parentchild) == issubnode);

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST remove_trie: child array with possible resize (depth X)
   // TEST remove_trie: subnode with possible conversion into child array (depth X)
   for (unsigned depth = 2; depth <= 8; ++depth) {
      trie_node_t * old;
      bool     isresize = false;
      unsigned keylen;
      trie_node_t ** parentchild;
      TEST(0 == build_depthx_trie(&trie, &parentchild, &keylen, 0, depth, key.addr));
      init_nodeoffsets(&off, *parentchild);
      if (issubnode_trienode(*parentchild)) {
         trie_subnode_t * subnode = subnode_trienode(*parentchild, off.off4_child);
         TEST(0 == build_nodechain_trienode(childaddr_triesubnode(subnode, key.addr[keylen]), 12, key.addr+keylen+1, (void*)depth));
         setchild_triesubnode(subnode, (uint8_t)(key.addr[keylen]+1), (void*)1); // not empty: ensures correct conversion
         ++ (*parentchild)->nrchild;
      } else {
         // build_depthx_trie adds one child with digit (key.addr[keylen]+1)
         uint8_t idx = key.addr[keylen] > digits_trienode(*parentchild, off.off3_digit)[0];
         old = *parentchild;
         TEST(0 == tryaddchild_trienode(parentchild, off.off3_digit, off.off4_child, idx, key.addr[keylen], childs[0]));
         isresize = (old != *parentchild);
         init_nodeoffsets(&off, *parentchild); // possible resize
         TEST(0 == build_nodechain_trienode(childs_trienode(*parentchild, off.off4_child)+idx, 12, key.addr+keylen+1, (void*)depth));
      }

      old = *parentchild;
      TEST(0 == remove_trie(&trie, (uint16_t)(keylen+13), key.addr, &value));
      TEST(value == (void*)depth);
      TEST(1 == nrchild_trienode(*parentchild));
      TEST(0 == issubnode_trienode(*parentchild));
      TEST(!isresize || old != *parentchild);
      init_nodeoffsets(&off, *parentchild);
      childs_trienode(*parentchild, off.off4_child)[0] = 0; // no segmentation fault in delete

      TEST(0 == free_trie(&trie));
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST remove_trie: ESRCH
   TEST(ESRCH == remove_trie(&trie, 10, key.addr, &value));
   GETBUFFER_ERRLOG(&logbuffer, &logsize2);
   TEST(logsize1 < logsize2); // log written !

   // unprepare
   TEST(0 == FREE_MM(&key));

   return 0;
ONABORT:
   delete_trienode(&trie.root);
   FREE_MM(&key);
   return EINVAL;
}

int unittest_ds_inmem_trie()
{
   int i = 0;
   printf("%d\n", 10/i);
   // #if 0 // TODO: remove line
   // header_t
   if (test_header())         goto ONABORT;
   // trie_subnode_t
   if (test_subnode())        goto ONABORT;
   // trie_node_t
   if (test_node_query())     goto ONABORT;
   if (test_node_lifetime())  goto ONABORT;
   if (test_node_change())    goto ONABORT;
   // trie_t
   if (test_initfree())       goto ONABORT;
   if (test_inserthelper())   goto ONABORT;
   // #endif // TODO: remove line
   if (test_insert())         goto ONABORT;
   if (test_remove())         goto ONABORT;

   // TODO: if (test_query())          goto ONABORT;
   // TODO: if (test_iterator())       goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
