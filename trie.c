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

typedef struct trie_node_t          trie_node_t;
typedef struct trie_nodedata_t      trie_nodedata_t;
typedef struct trie_subnode_t       trie_subnode_t;


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
 * header_SIZEMASK  - Mask to determine the value of one of the following 4 size configurations.
 * header_SIZESHIFT - Number of bits to shift right value of masked size field.
 *                    All header_SIZEX values are after the shift.
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
   trie_nodedata_t * child[256];
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
static inline trie_nodedata_t * child_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   return subnode->child[digit];
}

// group: change

/* function: setchild_triesubnode
 * Sets pointer to child node for digit. */
static inline void setchild_triesubnode(trie_subnode_t * subnode, uint8_t digit, trie_nodedata_t * child)
{
   subnode->child[digit] = child;
}

/* function: clearchild_triesubnode
 * Clears pointer for digit. */
static inline void clearchild_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   subnode->child[digit] = 0;
}


/* struct: trie_nodedata_t
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
struct trie_nodedata_t {
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
#define PTRALIGN  (offsetof(trie_nodedata_t, uservalue))

// group: query

static inline uint8_t * memaddr_trienodedata(trie_nodedata_t * nodedata)
{
   return (uint8_t*) nodedata;
}

// group: lifetime

/* function: delete_trienodedata
 * Frees allocated memory of subnode.
 * Referenced childs are not freed. */
static int delete_trienodedata(trie_nodedata_t ** nodedata, uint_fast16_t nodesize)
{
   int err;
   trie_nodedata_t * delnode = *nodedata;

   if (delnode) {
      *nodedata = 0;

      memblock_t mblock = memblock_INIT(nodesize, (uint8_t*)delnode);
      err = FREE_ERR_MM(&s_trie_errtimer, &mblock);
      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   return err;
}

/* function: new_trienodedata
 * Allocates a <trie_nodedata_t> of size nodesize in bytes.
 * The content is left uninitialized. */
static int new_trienodedata(/*out*/trie_nodedata_t ** nodedata, uint_fast16_t nodesize)
{
   int err;
   memblock_t mblock;

   err = ALLOC_ERR_MM(&s_trie_errtimer, nodesize, &mblock);
   if (err) goto ONABORT;

   // out param
   *nodedata = (trie_nodedata_t*) mblock.addr;

   return 0;
ONABORT:
   return err;
}



/* struct: trie_node_t
 * Stores decoded information about a <trie_nodedata_t> in memory.
 * A pointer to the memory data is stored in the <data> field.
 *
 * A <trie_nodedata_t> contains optional fields which do not
 * start at a fixed offset in memory.
 * This object calculates the offset information of every optional
 * field from the flags encoded in <trie_nodedata_t.header>.
 *
 * */
struct trie_node_t {
   // group: struct fields
   /* variable: data
    * Points to data in memory. */
   trie_nodedata_t * data;
   /* variable: off2_key
    * Offset in bytes from <data> to the encoded key.
    * The number 2 indicates the second optional field. */
   uint_fast16_t  off2_key;
   /* variable: off3_digit
    * Offset in bytes from <data> to the digit array.
    * The number 3 indicates the third optional field. */
   uint_fast16_t  off3_digit;
   /* variable: off4_uservalue
    * Offset in bytes from <data> to the optional user value.
    * The number 4 indicates the fourth optional field. */
   uint_fast16_t  off4_uservalue;
   /* variable: off5_child
    * Offset in bytes from <data> to the optional child array of void* pointer.
    * The number 5 indicates the fifth optional field. */
   uint_fast16_t  off5_child;
};

// group: constants

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
         (  (MAXSIZE-offsetof(trie_nodedata_t, keylen)) \
            / (sizeof(void*)+sizeof(uint8_t)))

/* define: MAXNROFCHILD_WITHUSERVALUE
 * The maximum nr of child pointer in child array of <trie_nodedata_t>.
 * The value is calculated with the assumption that no key is stored in the node
 * but an additional user value.
 * If a node needs to hold more child pointers it has to switch to a <trie_subnode_t>.
 * This value must be less than 256 else <trie_nodedata_t.nrchild> would overflow.
 * */
#define MAXNROFCHILD_WITHUSERVALUE \
         (  (MAXSIZE-offsetof(trie_nodedata_t, keylen)-sizeof(void*)) \
            / (sizeof(void*)+sizeof(uint8_t)))

// group: query-header

static inline int issubnode_trienode(const trie_node_t * node)
{
   return issubnode_header(node->data->header);
}

static inline int isuservalue_trienode(const trie_node_t * node)
{
   return isuservalue_header(node->data->header);
}

/* function: decodesize_trienode
 * Returns the size in bytes of a node decoded from its <header_t>.
 * */
static inline uint_fast16_t decodesize_trienode(const trie_node_t * node)
{
   return (2*sizeof(void*)) << sizeflags_header(node->data->header);
}

// group: query-fields

static inline trie_nodedata_t * data_trienode(const trie_node_t * node)
{
   return node->data;
}

static inline uint8_t * memaddr_trienode(const trie_node_t * node)
{
   return memaddr_trienodedata(data_trienode(node));
}

static inline uint_fast16_t off1_keylen_trienode(void)
{
   return offsetof(trie_nodedata_t, keylen);
}

static inline uint_fast16_t off2_key_trienode(const trie_node_t * node)
{
   return node->off2_key;
}

static inline uint_fast16_t off3_digit_trienode(const trie_node_t * node)
{
   return node->off3_digit;
}

static inline uint_fast16_t off4_uservalue_trienode(const trie_node_t * node)
{
   return node->off4_uservalue;
}

static inline uint_fast16_t off5_child_trienode(const trie_node_t * node)
{
   return node->off5_child;
}

static inline uint_fast16_t off6_free_trienode(const trie_node_t * node)
{
   return (uint_fast16_t) (
            off5_child_trienode(node)
            + ( issubnode_trienode(node)
                ? sizeof(void*)
                : (uint_fast16_t)node->data->nrchild * sizeof(void*))
         );
}

static inline uint8_t nrchild_trienode(const trie_node_t * node)
{
   return node->data->nrchild;
}

// group: query-helper

/* function: alignoffset_trienode
 * Aligns offset to architecture specific pointer alignment. */
static inline uint_fast16_t alignoffset_trienode(uint_fast16_t offset)
{
   static_assert(ispowerof2_int(PTRALIGN), "use bit mask to align value");
   return (uint_fast16_t) ((offset + PTRALIGN-1) & ~(PTRALIGN-1));
}

static inline void ** childs_trienode(trie_node_t * node)
{
   return (void**) (memaddr_trienode(node) + off5_child_trienode(node));
}

static inline uint_fast16_t childsize_trienode(const int issubnode, const uint8_t nrchild)
{
   return (uint_fast16_t) (
            issubnode ? sizeof(void*)
                      : (uint_fast16_t)nrchild * sizeof(void*)
         );
}

static inline uint8_t * digits_trienode(trie_node_t * node)
{
   return memaddr_trienode(node) + off3_digit_trienode(node);
}

static inline uint8_t keylen_trienode(const trie_node_t * node)
{
   return (uint8_t) (off3_digit_trienode(node) - off2_key_trienode(node));
}

/* function: subnode_trienode
 * Returns the pointer to the <trie_subnode_t>.
 *
 * Precondition:
 * The return value is only valid if <issubnode_trienode> returns true. */
static inline trie_subnode_t * subnode_trienode(trie_node_t * node)
{
   return *(void**) (memaddr_trienode(node) + off5_child_trienode(node));
}

/* function: usedsize_trienode
 * Returns number of bytes used in node. */
static uint_fast16_t usedsize_trienode(const trie_node_t * node)
{
   // TODO: remove function
   return (uint_fast16_t) (
            off6_free_trienode(node) - off4_uservalue_trienode(node)  /*size ptr*/
            + off3_digit_trienode(node)                               /*size byte array without digit array*/
            + (uint_fast16_t)
              (issubnode_trienode(node) ? 0 : nrchild_trienode(node)) /*size digit array*/
         );
}

/* function: sizeuservalue_trienode
 * Returns 0 or sizeof(void*). */
static inline unsigned sizeuservalue_trienode(const trie_node_t * node)
{
   unsigned size = off5_child_trienode(node) - off4_uservalue_trienode(node);
   return size;
}

// group: change-helper

static inline void addheaderflag_trienode(trie_node_t * node, uint8_t flag)
{
// TODO: test
   node->data->header = addflags_header(node->data->header, flag);
}

static inline void delheaderflag_trienode(trie_node_t * node, uint8_t flag)
{
// TODO: test
   node->data->header = delflags_header(node->data->header, flag);
}

static inline void encodekeylen_trienode(trie_node_t * node, const uint8_t keylen)
{
// TODO: test
   if (needkeylenbyte_header(node->data->header)) {
      node->data->header = encodekeylenbyte_header(node->data->header);
      node->data->keylen = keylen;
   } else {
      node->data->header = encodekeylen_header(node->data->header, keylen);
   }
}

/* function: addsubnode_trienode
 * Moves all pointers in child[] array to subnode.
 * Then all pointers in child[] and bytes in digit[]
 * are removed and a single pointer to subnode
 * is stored in node.
 *
 * Unchecked Precondition:
 * - The node contains digit[] and child[] arrays with MAXNROFCHILD_WITHUSERVALUE entries.
 *
 * */
static void addsubnode_trienode(trie_node_t * node, trie_subnode_t * subnode)
{
// TODO: test
   // copy child array into subnode
   const uint8_t nrchild = nrchild_trienode(node);
   const uint8_t* digits = digits_trienode(node);
   void * const*  childs = childs_trienode(node);
   for (uint8_t i = 0; i < nrchild; ++i) {
      setchild_triesubnode(subnode, digits[i], childs[i]);
   }

   // remove digit / child arrays from node
   addheaderflag_trienode(node, header_SUBNODE);
   -- node->data->nrchild;
   uint_fast16_t olduseroff = off4_uservalue_trienode(node);
   uint_fast16_t digitoff   = off3_digit_trienode(node);
   uint_fast16_t newuseroff = alignoffset_trienode(digitoff);
   uint_fast16_t newchildoff;
   uint8_t * memaddr = memaddr_trienode(node);
   if (isuservalue_trienode(node)) {
      newchildoff = newuseroff + sizeof(void*);
      memmove(memaddr + newuseroff, memaddr + olduseroff, sizeof(void*));
   } else {
      newchildoff = newuseroff;
   }
   node->off4_uservalue = newuseroff;
   node->off5_child     = newchildoff;
   if (0) (void) off6_free_trienode(node); // after childs there is no other field !

   // insert subnode
   *(void**)(memaddr + newchildoff) = subnode;
}

/* function: delsubnode_trienode
 * Moves all pointers from subnode into child[] array.
 * The pointer to subnode is returned in subnode.
 * Also a digit[] array is added to the node.
 *
 * Unchecked Precondition:
 * - The node contains a subnode with less than <MAXNROFCHILD_WITHUSERVALUE>/<MAXNROFCHILD_NOUSERVALUE> entries.
 * - The node is big enough to hold all digit[] and child[] entries
 *   after the pointer to subnode has been removed.
 * */
static trie_subnode_t * delsubnode_trienode(trie_node_t * node)
{
// TODO: test
   uint8_t *        memaddr = memaddr_trienode(node);
   trie_subnode_t * subnode = subnode_trienode(node);

   // make room for digit / child arrays
   delheaderflag_trienode(node, header_SUBNODE);
   uint8_t nrchild = ++ node->data->nrchild;
   uint_fast16_t olduseroff  = off4_uservalue_trienode(node);
   uint_fast16_t newuseroff  = alignoffset_trienode(off3_digit_trienode(node) + nrchild);
   node->off4_uservalue = newuseroff;
   if (isuservalue_trienode(node)) {
      node->off5_child = newuseroff + sizeof(void*);
      memmove(memaddr + newuseroff, memaddr + olduseroff, sizeof(void*));
   } else {
      node->off5_child = newuseroff;
   }
   if (0) (void) off6_free_trienode(node); // after childs there is no other field !

   // copy childs
   uint8_t* digits = digits_trienode(node);
   void **  childs = childs_trienode(node);
   for (uint8_t digit = 0, i = 0;;) {
      trie_nodedata_t * child = child_triesubnode(subnode, digit);
      if (child) {
         digits[i] = digit;
         childs[i] = child;
         ++i;
      }
      ++ digit;
      if (!digit) {
         assert(i == node->data->nrchild);
         break;
      }
   }

   return subnode;
}

/* function: copyoffsets_trienode
 * Copies offsets from src_node to dest_node.
 * The field <data> is not changed and therefore undefined.
 */
static inline void copyoffsets_trienode(
   /*out*/trie_node_t*  dest_node,
   const trie_node_t*   src_node)
{
// TODO: test
   dest_node->off2_key   = src_node->off2_key;
   dest_node->off3_digit = src_node->off3_digit;
   dest_node->off4_uservalue = src_node->off4_uservalue;
   dest_node->off5_child = src_node->off5_child;
}

/* function: computeoffsets_trienode
 * Computes new offsets and returns them in changed_node.
 * The offsets are computed from new_nrchild and new_keylen.
 * The field <data> is not changed and therefore undefined.
 *
 * Returns:
 * The used size in bytes for the new node layout. */
static inline unsigned computeoffsets_trienode(
   /*out*/trie_node_t*  changed_node,
   const int            issubnode,
   const unsigned       size_uservalue, /* either 0 or sizeof(void*) */
   const uint8_t        new_nrchild,
   const uint8_t        new_keylen)
{
// TODO: test
   // calculate new size
   unsigned offset = off1_keylen_trienode()
                   + needkeylenbyte_header((uint8_t)new_keylen);

   changed_node->off2_key = (uint_fast16_t)offset;
   offset += (unsigned) new_keylen;

   changed_node->off3_digit = (uint_fast16_t)offset;
   offset += issubnode ? 0u : new_nrchild;
   offset  = alignoffset_trienode((uint_fast16_t)offset);

   changed_node->off4_uservalue = (uint_fast16_t)offset;
   offset += size_uservalue;

   changed_node->off5_child = (uint_fast16_t)offset;
   offset += childsize_trienode(issubnode, new_nrchild);

   return offset;
}

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
   /*out*/trie_nodedata_t ** data,
   const header_t            nodeheader,
   unsigned                  oldsize,
   unsigned                  newsize)
{
// TODO: test
   int err;
   header_t sizeflags    = sizeflags_header(nodeheader);
   unsigned expandedsize = oldsize;

   do {
      expandedsize *= 2;
      ++ sizeflags;
   } while (expandedsize < newsize);

   err = new_trienodedata(data, expandedsize);
   if (err) return err;

   // set trie_nodedata_t.header
   (*data)->header = encodesizeflag_header(nodeheader, sizeflags);

   return 0;
}

/* function: deluservalue_trienode
 *
 * Unchecked Precondition:
 * - The node has a uservalue (<off4_uservalue_trienode>() == <off5_child_trienode>()-sizeof(void*))
 */
static inline void deluservalue_trienode(trie_node_t * node)
{
   // TODO: test
   unsigned newchildoff = off4_uservalue_trienode(node);
   unsigned oldchildoff = off5_child_trienode(node);
   node->off5_child = (uint_fast16_t) newchildoff;

   uint8_t * memaddr = memaddr_trienode(node);
   delheaderflag_trienode(node, header_USERVALUE);
   memmove(memaddr + newchildoff, memaddr + oldchildoff, childsize_trienode(issubnode_trienode(node), nrchild_trienode(node)));
   if (0) (void) off6_free_trienode(node); // after childs there is no other field !

}

/* function: tryadduservalue_trienode
 *
 * TODO: describe
 *
 * Unchecked Precondition:
 * - The node has no uservalue (<off4_uservalue_trienode> == <off5_child_trienode>) */
static int tryadduservalue_trienode(trie_node_t * node, void * uservalue)
{
// TODO: test
   int err;
   const uint_fast16_t off6 = off6_free_trienode(node);
   const unsigned      newoff6 = off6 + sizeof(void*);

   // could node of MAXSIZE hold it ?
   if (  MAXSIZE < newoff6) {
      return EINVAL;
   }

   uint8_t * destaddr = memaddr_trienode(node);
   unsigned  oldsize  = decodesize_trienode(node);

   if (oldsize < newoff6) {
      trie_nodedata_t * data;
      err = expandnode_trienode(&data, node->data->header, oldsize, newoff6);
      if (err) return err;

      uint8_t * srcaddr = destaddr;
      destaddr = memaddr_trienodedata(data);
      memcpy( destaddr + sizeof(data->header), srcaddr + sizeof(data->header),
              off4_uservalue_trienode(node) - sizeof(data->header));
      memcpy( destaddr + sizeof(void*) + off4_uservalue_trienode(node), srcaddr + off4_uservalue_trienode(node),
              off6 - off4_uservalue_trienode(node));

      (void) delete_trienodedata(&node->data, oldsize);
      node->data = data;

   } else {
      memmove( destaddr + sizeof(void*) + off4_uservalue_trienode(node), destaddr + off4_uservalue_trienode(node),
               off6 - off4_uservalue_trienode(node));
   }

   addheaderflag_trienode(node, header_USERVALUE);
   *(void**) (destaddr + off4_uservalue_trienode(node)) = uservalue;

   node->off5_child = (uint_fast16_t) (off4_uservalue_trienode(node) + sizeof(void*));

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
   uint8_t keylen = keylen_trienode(node);
   uint8_t new_keylen = (uint8_t) (keylen - prefixkeylen);

   trie_node_t changed_node;
   unsigned newsize = computeoffsets_trienode(  &changed_node,
                        issubnode_trienode(node),
                        sizeuservalue_trienode(node),
                        nrchild_trienode(node), new_keylen );

   uint8_t * memaddr = memaddr_trienode(node);

   encodekeylen_trienode(&changed_node, new_keylen);
   memmove( memaddr + off2_key_trienode(&changed_node),
            memaddr + off2_key_trienode(node) + prefixkeylen,
            new_keylen);
   memmove( memaddr + off3_digit_trienode(&changed_node),
            memaddr + off3_digit_trienode(node),
            off4_uservalue_trienode(node) - off3_digit_trienode(node)/*up to sizeof(void*)-1 bytes too much*/);
   memmove( memaddr + off4_uservalue_trienode(&changed_node),
            memaddr + off4_uservalue_trienode(node),
            newsize - off4_uservalue_trienode(&changed_node));

   copyoffsets_trienode(node, &changed_node);
}

/* function: tryaddkeyprefix_trienode
 * The key[prefixkeylen] is prepended to the key stored in node.
 * If node is resized the child pointer of the parent node has to be adapted!
 *
 * Returns EINVAL if a prefix of size prefixkeylen does not fit into the node
 * even after having been resized.
 * */
static int tryaddkeyprefix_trienode(trie_node_t * node, uint8_t prefixkeylen, const uint8_t key[prefixkeylen])
{
// TODO: test
   int err;

   uint8_t node_keylen = keylen_trienode(node);

   unsigned newkeylen = (uint_fast16_t)node_keylen + prefixkeylen;

   // could node of MAXSIZE hold it ?
   if (255 < newkeylen) return EINVAL;

   trie_node_t changed_node;
   unsigned newsize = computeoffsets_trienode( &changed_node,
                        issubnode_trienode(node),
                        sizeuservalue_trienode(node),
                        nrchild_trienode(node), (uint8_t)newkeylen );

   if (newsize > MAXSIZE) return EINVAL;

   unsigned  oldsize  = decodesize_trienode(node);
   uint8_t * srcaddr  = memaddr_trienode(node);
   uint8_t * destaddr = srcaddr;

   trie_nodedata_t * data = 0;
   if (oldsize < newsize) {
      err = expandnode_trienode(&data, node->data->header, oldsize, newsize);
      if (err) return err;
      destaddr = memaddr_trienodedata(data);
   }

   // copy content
   if (0) (void) off6_free_trienode(node); // after childs there is no other field !
   memmove( destaddr + off4_uservalue_trienode(&changed_node), srcaddr + off4_uservalue_trienode(node),
            off6_free_trienode(&changed_node) - off4_uservalue_trienode(&changed_node));
   if (!issubnode_trienode(node)) {
      memmove( destaddr + off3_digit_trienode(&changed_node), srcaddr + off3_digit_trienode(node),
               nrchild_trienode(node));
   }
   memmove( destaddr + off2_key_trienode(&changed_node) + prefixkeylen, srcaddr + off2_key_trienode(node),
            node_keylen);
   memcpy(  destaddr + off2_key_trienode(&changed_node), key,
            prefixkeylen);

   // was node size expanded ?
   if (data) {
      data->nrchild = nrchild_trienode(node);
      (void) delete_trienodedata(&node->data, oldsize);
      node->data    = data;
   }

   encodekeylen_trienode(node, (uint8_t)newkeylen);

   copyoffsets_trienode(node, &changed_node);

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
 * - nrchild_trienode(node) < 255
 * - issubnode_trienode(node)  || childidx <= nrchild_trienode(node)
 * - !issubnode_trienode(node) || child_triesubnode(subnode, digit) == 0
 * */
static int tryaddchild_trienode(trie_node_t * node, uint8_t childidx/*0 - nrchild*/, uint8_t digit, trie_nodedata_t * child)
{
// TODO: test
   int err;

   if (issubnode_trienode(node)) {
      trie_subnode_t * subnode = subnode_trienode(node);
      setchild_triesubnode(subnode, digit, child);

   } else {
      trie_node_t changed_node;
      unsigned newsize = computeoffsets_trienode(
                           &changed_node, 0/*issubnode*/, sizeuservalue_trienode(node),
                           (uint8_t) (nrchild_trienode(node)+1), keylen_trienode(node));

      // could node of MAXSIZE hold it ?
      if (  MAXSIZE < newsize) {
         return EINVAL;
      }

      unsigned  oldsize  = decodesize_trienode(node);
      uint8_t * srcaddr  = memaddr_trienode(node);
      uint8_t * destaddr = srcaddr;

      trie_nodedata_t * data = 0;
      if (oldsize < newsize) {
         err = expandnode_trienode(&data, node->data->header, oldsize, newsize);
         if (err) return err;
         destaddr = memaddr_trienodedata(data);
      }

      // make room in child and digit arrays
      unsigned insoffset = childidx * sizeof(void*);
      // child array is last field
      if (0) off6_free_trienode(node);
      // child array after insoffset
      memmove( destaddr + sizeof(void*) + off5_child_trienode(&changed_node) + insoffset,
               srcaddr  + off5_child_trienode(node) + insoffset,
               newsize - sizeof(void*) - off5_child_trienode(&changed_node) - insoffset);
      *(void**)(destaddr + off5_child_trienode(&changed_node) + insoffset) = child;
      if (data || off4_uservalue_trienode(&changed_node) != off4_uservalue_trienode(node)) {
         // uservalue + child array before insoffset
         memmove( destaddr + off4_uservalue_trienode(&changed_node),
                  srcaddr  + off4_uservalue_trienode(node),
                  insoffset + sizeuservalue_trienode(node));
      }
      // digit array after childidx
      memmove( destaddr + sizeof(uint8_t) + off3_digit_trienode(&changed_node) + childidx,
               srcaddr  + off3_digit_trienode(node) + childidx,
               (size_t)nrchild_trienode(node) - childidx);
      destaddr[off3_digit_trienode(&changed_node) + childidx] = digit;

      // off3_digit_trienode(&changed_node) == off3_digit_trienode(node) ==> no copy in case of !data
      if (data) {
         // whole node except header before childidx
         memcpy( destaddr + sizeof(data->header),
                 srcaddr + sizeof(data->header),
                 childidx + off3_digit_trienode(node) - sizeof(data->header));
         (void) delete_trienodedata(&node->data, oldsize);
         node->data = data;
      }

      copyoffsets_trienode(node, &changed_node);
   }

   ++ node->data->nrchild;
   return 0;
}

/* function: tryaddchild_trienode
 * Inserts new child into node.
 * The position in the child array is given in childidx.
 * The value lies between 0 and <nrchild_trienode>-1 both inclusive.
 * If the node contains a <trie_subnode_t> parameter digit is used
 * instead of childidx.
 *
 * Unchecked Precondition:
 * - nrchild_trienode(node) > 0
 * - issubnode_trienode(node)  || childidx < nrchild_trienode(node)
 * - !issubnode_trienode(node) || child_triesubnode(subnode, digit) != 0
 * */
static void delchild_trienode(trie_node_t * node, uint8_t childidx/*0 - nrchild-1*/, uint8_t digit)
{
// TODO: test
   if (issubnode_trienode(node)) {
      trie_subnode_t * subnode = subnode_trienode(node);
      clearchild_triesubnode(subnode, digit);

   } else {
      trie_node_t changed_node;
      unsigned newsize = computeoffsets_trienode(
                           &changed_node, 0/*issubnode*/, sizeuservalue_trienode(node),
                           (uint8_t) (nrchild_trienode(node)-1), keylen_trienode(node));

      // remove entries in child and digit arrays
      unsigned deloffset = childidx * sizeof(void*);
      uint8_t * memaddr = memaddr_trienode(node);
      // digit array after childidx
      memmove( memaddr + off3_digit_trienode(node) + childidx,
               memaddr + off3_digit_trienode(node) + childidx+1,
               (size_t)nrchild_trienode(node)-childidx-1);
      if (off4_uservalue_trienode(&changed_node) != off4_uservalue_trienode(node)) {
         // uservalue + child array before insoffset
         memmove( memaddr + off4_uservalue_trienode(&changed_node),
                  memaddr + off4_uservalue_trienode(node),
                  deloffset + sizeuservalue_trienode(node));
      }
      // child array after insoffset
      memmove( memaddr + off5_child_trienode(&changed_node) + deloffset,
               memaddr + sizeof(void*) + off5_child_trienode(node) + deloffset,
               newsize - off5_child_trienode(&changed_node) - deloffset);
      // child array is last field
      if (0) off6_free_trienode(node);

      copyoffsets_trienode(node, &changed_node);
   }

   -- node->data->nrchild;
}


// group: lifetime

static int free_trienode(trie_node_t * node)
{
   int err;
   int err2;

   if (node->data) {
      err = 0;

      if (issubnode_trienode(node)) {
         trie_subnode_t * subnode = subnode_trienode(node);
         err = delete_triesubnode(&subnode);
      }

      err2 = delete_trienodedata(&node->data, decodesize_trienode(node));
      if (err2) err = err2;

      if (err) goto ONABORT;
   }

   return 0;
ONABORT:
   return err;
}

/* function: init_trienode
 * Initializes node and reserves space in a newly allocated <trie_nodedata_t>.
 * The content of the data node (user value, child pointers (+ digits) and key bytes)
 * are undefined after return.
 *
 * The reserved keylen will be shrunk if a node of size <MAXSIZE> can not hold the
 * whole key. Therefore check the length of the reserved key after return.
 *
 * The child array is either stored in the node or a subnode is allocated if
 * nrchild is bigger than <MAXNROFCHILD_WITHUSERVALUE> or <MAXNROFCHILD_NOUSERVALUE>
 * which depends on reserveUserValue being true or false.
 *
 * Parameter nrchild:
 * The parameter nrchild can encode only 255 child pointers.
 * A subnode supports up to 256 child pointers so you have to increment
 * by one after return.
 * In case of a subnode the value of <trie_nodedata_t.nrchild> is one less
 * then the provided value in parameter nrchild.
 *
 * */
static int init_trienode(
   /*out*/trie_node_t* node,
   const bool     reserveUserValue,
   uint8_t        nrchild,
   // keylen is reduced until it fits into a node of MAXSIZE with nrchild and a possible user value
   uint8_t        keylen
   )
{
   int err;
   size_t            size = reserveUserValue ? sizeof(void*) + off1_keylen_trienode() : off1_keylen_trienode();
   trie_subnode_t *  subnode = 0;

   if (nrchild > (reserveUserValue ? MAXNROFCHILD_WITHUSERVALUE : MAXNROFCHILD_NOUSERVALUE)) {
      size += childsize_trienode(1, nrchild);
      err = new_triesubnode(&subnode);
      if (err) goto ONABORT;
      -- nrchild; // subnode encodes one less

   } else {
      size += nrchild/*size digit array*/ + childsize_trienode(0, nrchild);
   }

   size += needkeylenbyte_header(keylen);
   size += keylen;

   size_t   nodesize;
   header_t header;

   if (size > MAXSIZE) {
      // nodesize == MAXSIZE
      size -= needkeylenbyte_header(keylen);
      size -= keylen;
      keylen = (uint8_t) (MAXSIZE - size);
      keylen = (uint8_t) (keylen - needkeylenbyte_header(keylen));
      size += needkeylenbyte_header(keylen);
      size += keylen;
      header = header_SIZEMAX<<header_SIZESHIFT;
      nodesize = MAXSIZE;

   } else if (size > MAXSIZE / 8) {
      // grow nodesize
      nodesize = MAXSIZE / 4;
      for (header = header_SIZEMAX-2; nodesize < size; ++header) {
         nodesize *= 2;
      }
      header = (header_t) (header << header_SIZESHIFT);

   } else {
      // shrink nodesize
      nodesize = MAXSIZE / 8;
      for (header = header_SIZEMAX-3; nodesize/2 >= size && header > header_SIZE0; --header) {
         nodesize /= 2;
      }
      header = (header_t) (header << header_SIZESHIFT);
   }

   trie_nodedata_t * data;
   err = new_trienodedata(&data, nodesize);
   if (err) goto ONABORT;

   header = addflags_header(header, reserveUserValue ? header_USERVALUE : 0);
   header = addflags_header(header, subnode ? header_SUBNODE : 0);

   unsigned offset = off1_keylen_trienode();

   if (needkeylenbyte_header(keylen)) {
      header = encodekeylenbyte_header(header);
      data->keylen = keylen;
      ++ offset;
   } else {
      header = encodekeylen_header(header, keylen);
   }

   data->header  = header;
   data->nrchild = nrchild;

   // set out param
   node->data = data;
   node->off2_key = (uint_fast16_t) offset;
   offset += keylen;
   node->off3_digit = (uint_fast16_t) offset;
   offset += subnode != 0 ? 0u : (unsigned)nrchild;
   offset  = alignoffset_trienode((uint_fast16_t)offset);
   node->off4_uservalue = (uint_fast16_t) offset;
   offset += reserveUserValue ? sizeof(void*) : 0;
   node->off5_child = (uint_fast16_t) offset;

   if (subnode) {
      * (void**) (memaddr_trienodedata(data) + offset) = subnode;
   }

   return 0;
ONABORT:
   (void) delete_triesubnode(&subnode);
   return err;
}


/* function: initdecode_trienode
 * Decodes data and initializes data and offsets in node.
 *
 * Returns EINVAL if decoding encounters an error.
 *
 * */
static int initdecode_trienode(/*out*/trie_node_t * node, /*in*/trie_nodedata_t * data)
{
   uint8_t keylen;
   header_t header = data->header;

   uint_fast16_t offset = off1_keylen_trienode();

   keylen = keylen_header(header);
   if (keylen == header_KEYLENBYTE) {
      keylen = data->keylen;
      ++offset;
   }

   node->data = data;
   node->off2_key = offset;
   offset += keylen;
   node->off3_digit = offset;
   offset += (uint_fast16_t) ((header & header_SUBNODE) ? 0 : data->nrchild);
   offset = alignoffset_trienode(offset);
   node->off4_uservalue = offset;
   offset += (header & header_USERVALUE) ? sizeof(void*) : 0;
   node->off5_child = offset;

   unsigned nodesize = decodesize_trienode(node);
   static_assert( 0 != (header_SIZEMAX << header_SIZESHIFT)
                  && header_SIZEMASK > (header_SIZEMAX << header_SIZESHIFT), "not all flags are in use");
   if (nodesize > MAXSIZE) goto ONABORT;

   if (nodesize < off6_free_trienode(node)) goto ONABORT;

   return 0;
ONABORT:
   node->data = 0;
   return EINVAL;
}


#if 0

/* function: new_trienode
 * Allocate new <trie_node_t> with optional prefix, optional usernode and optional childs.
 * If the prefix does not fit into a single trie_node_t a chain of trie_nodes is created.
 *
 * TODO: add size optimization for long prefix (two nodes instead of one, if two nodes occupy less space)
 *
 * unchecked Precondition:
 * o nrchild <= 256
 * o digit array is sorted in ascending order
 * o Every pointer in child != 0
 */
static int new_trienode(/*out*/trie_node_t * node, uint16_t prefixlen, const uint8_t prefix[prefixlen], void * const * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], trie_nodedata_t * const child[nrchild])
{
   int err;
   uint8_t       encodedlen;

   init_trienodeoffsets(&node->offsets, prefixlen, uservalue != 0, nrchild);
   encodedlen = lenprefix_trienodeoffsets(&node->offsets);
   prefixlen  = (uint16_t) (prefixlen - encodedlen);

   node->data = 0;
   err = newnode_trienode(&node->data, node->offsets.nodesize);
   if (err) goto ONABORT;

   node->data->header = node->offsets.header;
   if (encodedlen) {
      node->data->prefixlen = encodedlen;
      memcpy((uint8_t*)node->data + node->offsets.prefix, prefix+prefixlen, encodedlen);
   }

   if (nrchild) {
      if (ischild_header(node->offsets.header)) {
         memcpy((uint8_t*)node->data+node->offsets.digit, digit, nrchild);
         size_t sizechild = nrchild*sizeof(trie_node_t*);
         memcpy((uint8_t*)node->data+node->offsets.child, child, sizechild);
         memset((uint8_t*)node->data+node->offsets.child+sizechild, 0, (size_t)node->offsets.nodesize - node->offsets.child - sizechild);

      } else {
         trie_subnode_t * subnode = 0;
         err = new_triesubnode(&subnode, nrchild, digit, child);
         // works only if 0 < nrchild && nrchild <= 256
         digit_trienode(node)[0] = (uint8_t) (nrchild-1);
         subnode_trienodeoffsets(&node->offsets, node->data)[0] = subnode;
         if (err) goto ONABORT;
      }
   }

   if (uservalue) {
      *(void**)((uint8_t*)node->data + node->offsets.uservalue) = *uservalue;
   }

   while (prefixlen)  {

      // build chain of nodes !

      init_trienodeoffsets(&node->offsets, (uint16_t)(prefixlen-1), false, 1);
      encodedlen = lenprefix_trienodeoffsets(&node->offsets);

      do {
         // do not calculate offsets for same prefixlen
         -- prefixlen;

         trie_nodedata_t * newdata;
         trie_nodedata_t * prevdata = node->data;
         err = newnode_trienode(&newdata, node->offsets.nodesize);
         if (err) goto ONABORT;
         node->data = newdata;

         node->data->header    = node->offsets.header;
         node->data->prefixlen = encodedlen;
         memset(child_trienode(node), 0, (size_t)(node->offsets.nodesize - node->offsets.child));
         digit_trienode(node)[0] = prefix[prefixlen];
         child_trienode(node)[0] = prevdata;
         prefixlen  = (uint16_t) (prefixlen - encodedlen);
         memcpy(prefix_trienode(node), prefix+prefixlen, encodedlen);

      } while (encodedlen < prefixlen);
   }

   return 0;
ONABORT:
   delete_trienode(&node->data);
   TRACEABORT_ERRLOG(err);
   return err;
}

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
         && (  (child == 0 && sizeof(void*) <= shrinklen+sizefree_trienodeoffsets(&splitnode->offsets))
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

   trie_nodedata_t * parent = 0;
   trie_nodedata_t * data   = trie->root;
   trie_node_t       delnode;

   while (data) {

      // 1: decode data into delnode
      // ( data points to node which is encountered the first time
      //   parent points to parent of data )
      // if it has no childs: goto step 2
      // else: write parent in first child pointer
      //       and descend into first child by repeating step 1
      //

      // 2: delete delnode (no childs)

      // 3: decode parent into delnode
      // if it has more childs: copy next child into data and repeat step 1 (descend into child node)
      // else: read parent pointer from first child and goto step 2

      for (;;) {
         // step 1:
         void * firstchild  = 0;
         err2 = initdecode_trienode(&delnode, data);
         if (err2) {       // ! ignore ! corrupted memory
            err = err2;

         } else if (! issubnode_header(data->header)) {
            if (data->nrchild) {
               void ** childs = childs_trienode(&delnode);
               firstchild = childs[0];
               childs[0]  = parent;
            }

         } else {
            trie_subnode_t * subnode = subnode_trienode(&delnode);
            for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  data->nrchild = (uint8_t)i; // last index into subnode->childs
                  firstchild = subnode->child[i];
                  subnode->child[0] = parent;
                  break;
               }
            }
         }

         if (!firstchild) break;

         // repeat step 1
         parent = data;
         data   = firstchild;
      }

      for (;;) {
         // step 2:
         err2 = free_trienode(&delnode);
         if (err2) err = err2;

         data = 0;
         if (!parent) break; // (data == 0) ==> leave top loop: while (data) {}

         // step 3:
         err2 = initdecode_trienode(&delnode, parent);
         if (err2) { // ! ignore ! corrupted memory
            err = err2;
            break;      // (data == 0) ==> leave top loop: while (data) {}
         }

         if (! issubnode_header(parent->header)) {
            while (-- parent->nrchild) {
               data = childs_trienode(&delnode)[parent->nrchild];
               if (data) break;
            }
            if (data) break; // another child ==> repeat step 1
            parent = childs_trienode(&delnode)[0];

         } else {
            trie_subnode_t * subnode = subnode_trienode(&delnode);
            for (unsigned i = parent->nrchild+1u; i < lengthof(subnode->child); ++i) {
               if (subnode->child[i]) {
                  data->nrchild = (uint8_t)i; // last index into subnode->childs
                  data = subnode->child[i];
                  break;
               }
            }
            if (data) break; // another child ==> repeat step 1
            parent = subnode->child[0];

         }

         // delnode is initialized with former parent !
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

static int test_triesubnode2(void)
{
   trie_subnode2_t * subnode[16] = { 0 };
   size_t            allocsize   = SIZEALLOCATED_MM();

   // TEST new_triesubnode2
   for (unsigned i = 0; i < lengthof(subnode); ++i) {
      TEST(0 == new_triesubnode2(&subnode[i]));
      TEST(0 != subnode[i]);
      for (unsigned ci = 0; ci < lengthof(subnode[i]->child); ++ci) {
         TEST(0 == subnode[i]->child[ci]);
      }
      allocsize += sizeof(trie_subnode2_t);
      TEST(allocsize == SIZEALLOCATED_MM());
   }

   // TEST delete_triesubnode2
   for (unsigned i = 0; i < lengthof(subnode); ++i) {
      TEST(0 == delete_triesubnode2(&subnode[i]));
      TEST(0 == subnode[i]);
      allocsize -= sizeof(trie_subnode2_t);
      TEST(allocsize == SIZEALLOCATED_MM());
      TEST(0 == delete_triesubnode2(&subnode[i]));
      TEST(0 == subnode[i]);
      TEST(allocsize == SIZEALLOCATED_MM());
   }

   // TEST new_triesubnode2: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_triesubnode2(&subnode[0]));
   TEST(0 == subnode[0]);
   TEST(allocsize == SIZEALLOCATED_MM());

   // TEST delete_triesubnode2: ERROR
   TEST(0 == new_triesubnode2(&subnode[0]));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_triesubnode2(&subnode[0]));
   TEST(0 == subnode[0]);
   TEST(allocsize == SIZEALLOCATED_MM());

   // TEST child_triesubnode2
   TEST(0 == new_triesubnode2(&subnode[0]));
   for (unsigned i = 0, offset = 0; i < 256; ++i, ++offset, offset = (offset < lengthof(subnode[0]->child) ? offset : 0)) {
      TEST(&subnode[0]->child[offset] == child_triesubnode2(subnode[0], (uint8_t)i));
   }
   TEST(0 == delete_triesubnode2(&subnode[0]));

   return 0;
ONABORT:
   for (unsigned i = 0; i < lengthof(subnode); ++i) {
      delete_triesubnode2(&subnode[i]);
   }
   return EINVAL;
}

static int test_triesubnode(void)
{
   uint8_t           digit[256];
   trie_node_t *     child[256];
   trie_subnode_t *  subnode   = 0;
   trie_subnode2_t * subnode2;
   size_t            allocsize = SIZEALLOCATED_MM();

   // prepare
   for (unsigned i = 0; i < lengthof(digit); ++i) {
      digit[i] = (uint8_t) i;
   }
   for (unsigned i = 0; i < lengthof(child); ++i) {
      child[i] = (void*) (1+i);
   }

   static_assert(sizeof(trie_subnode_t) == SIZE5NODE, "fit into SIZE5NODE");

   // TEST new_triesubnode, delete_triesubnode
   for (unsigned i = 0; i < lengthof(child); ++i) {
      TEST(0 == new_triesubnode(&subnode, (uint16_t)i, digit, child));
      TEST(0 != subnode);
      for (unsigned ci = 0; ci < lengthof(subnode->child); ++ci) {
         if (ci * lengthof(subnode2->child) < i) {
            TEST(0 != subnode->child[ci]);
         } else {
            TEST(0 == subnode->child[ci]);
         }
      }
      unsigned alignedi = ((i + lengthof(subnode2->child)-1) / lengthof(subnode2->child)) * lengthof(subnode2->child);
      for (unsigned ci = 0; ci < alignedi; ++ci) {
         if (ci < i) {
            TEST(0 != subnode->child[ci/lengthof(subnode2->child)]->child[ci%lengthof(subnode2->child)]);
         } else {
            TEST(0 == subnode->child[ci/lengthof(subnode2->child)]->child[ci%lengthof(subnode2->child)]);
         }
      }
      size_t allocsize2 = allocsize + sizeof(trie_subnode_t) + alignedi/lengthof(subnode2->child) * sizeof(trie_subnode2_t);
      TEST(allocsize2 == SIZEALLOCATED_MM());
      TEST(0 == delete_triesubnode(&subnode));
      TEST(0 == subnode);
      TEST(allocsize == SIZEALLOCATED_MM());
      TEST(0 == delete_triesubnode(&subnode));
      TEST(0 == subnode);
      TEST(allocsize == SIZEALLOCATED_MM());
   }

   // TEST new_triesubnode: ERROR
   for (unsigned errcount = 1; errcount <= 1 + lengthof(subnode->child); ++errcount) {
      init_testerrortimer(&s_trie_errtimer, errcount, ENOMEM);
      TEST(ENOMEM == new_triesubnode(&subnode, lengthof(digit), digit, child));
      TEST(0 == subnode);
      TEST(allocsize == SIZEALLOCATED_MM());
   }
   init_testerrortimer(&s_trie_errtimer, 2 + lengthof(subnode->child), ENOMEM);
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child));
   free_testerrortimer(&s_trie_errtimer);
   TEST(0 == delete_triesubnode(&subnode));

   // TEST delete_triesubnode: ERROR
   for (unsigned errcount = 1; errcount <= 1 + lengthof(subnode->child); ++errcount) {
      TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child));
      init_testerrortimer(&s_trie_errtimer, errcount, EINVAL);
      TEST(EINVAL == delete_triesubnode(&subnode));
      TEST(0 == subnode);
      TEST(allocsize == SIZEALLOCATED_MM());
   }
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child));
   init_testerrortimer(&s_trie_errtimer, 2 + lengthof(subnode->child), EINVAL);
   TEST(0 == delete_triesubnode(&subnode));
   free_testerrortimer(&s_trie_errtimer);

   // TEST child_triesubnode
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child));
   for (unsigned i = 0; i < 256; ++i) {
      TEST(&subnode->child[i/(256/lengthof(subnode->child))] == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   return 0;
ONABORT:
   delete_triesubnode(&subnode);
   return EINVAL;
}

static int test_trienodeoffset(void)
{
   trie_nodeoffsets_t offsets;

   // ////
   // group constants

   // TEST HEADERSIZE
   static_assert( HEADERSIZE == offsetof(trie_node_t, prefixlen), "size of header");

   // TEST PTRALIGN
   static_assert( (PTRALIGN >= 1) && (PTRALIGN&(PTRALIGN-1)) == 0, "must be power of two value");
   static_assert( PTRALIGN == offsetof(trie_node_t, ptrdata), "alignment of pointer in struct");

   // ////
   // group helper

   // TEST divideby5, divideby8, dividebychilddigitsize
   for (uint32_t i = 0; i < 256; ++i) {
      uint8_t d5 = divideby5((uint8_t)i);
      uint8_t d9 = divideby9((uint8_t)i);
      uint8_t dc = dividebychilddigitsize((uint8_t)i);
      TEST(d5 == i/5);
      TEST(d9 == i/9);
      TEST(dc == i/(sizeof(trie_node_t*)+1));
   }

   // ////
   // group lifetime

   // TEST init_trienodeoffsets
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild) {
         for (uint32_t prefixlen = 0; prefixlen < 65536; prefixlen = (prefixlen >= 3 ? 2*prefixlen : prefixlen), ++prefixlen) {
            unsigned lenchild = (nrchild > LENCHILDMAX) ? 1u : nrchild;
            memset(&offsets, 0, sizeof(offsets));
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild);
            size_t  size    = HEADERSIZE + (isuser ? sizeof(void*) : 0) + lenchild * sizeof(trie_node_t*)
                              + (size_t) lenchild /*digit*/ + (size_t) (prefixlen > 2);
            uint32_t len    = prefixlen;
            unsigned header = 0;
            if (size + prefixlen > SIZE5NODE) {
               len  = SIZE5NODE - size;
               size = SIZE5NODE;
               header = header_SIZE5NODE;
            } else {
               size += prefixlen;
               size_t oldsize = size;
               if (size <= SIZE1NODE)        size = SIZE1NODE, header = header_SIZE0NODE;
               else if (size <= SIZE2NODE)   size = SIZE2NODE, header = header_SIZE2NODE;
               else if (size <= SIZE3NODE)   size = SIZE3NODE, header = header_SIZE3NODE;
               else if (size <= SIZE4NODE)   size = SIZE4NODE, header = header_SIZE4NODE;
               else                          size = SIZE5NODE, header = header_SIZE5NODE;
               if (0 < nrchild && nrchild <= LENCHILDMAX) {
                  // fill empty bytes with child pointer
                  for (; oldsize + 1 + sizeof(trie_node_t*) <= size; ++lenchild) {
                     oldsize += 1+sizeof(trie_node_t*);
                  }
               }
            }
            header |= prefixlen == 0 ? header_NOPREFIX : prefixlen == 1 ? header_PREFIX1 : prefixlen == 2 ? header_PREFIX2 : header_PREFIX_LEN;
            if (isuser)   header |= header_USERVALUE;
            if (nrchild > LENCHILDMAX) header |= header_SUBNODE;
            else if (nrchild)          header |= header_CHILD;
            static_assert(offsetof(trie_nodeoffsets_t, nodesize) == 0, "");
            TEST(offsets.nodesize  == size);
            TEST(offsets.lenchild  == lenchild);
            TEST(offsets.header    == header);
            TEST(offsets.prefix    == HEADERSIZE + (prefixlen > 2));
            TEST(offsets.digit     == offsets.prefix + len);
            size_t aligned = (offsets.digit + (size_t)lenchild + PTRALIGN-1) & ~(PTRALIGN-1);
            TEST(offsets.uservalue == aligned);
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
         }
      }
   }

   // TEST initdecode_trienodeoffsets
   static_assert(header_SIZENODE_MASK == 7, "encoded in first 3 bits");
   for (uint8_t sizemask = 0; sizemask <= header_SIZENODE_MASK; ++sizemask) {
      for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
         for (uint8_t ischild = 0; ischild <= 2; ++ischild) {  // ischild == 2 ==> subnode
            for (uint16_t prefixlen = 0; prefixlen < 256; ++prefixlen) {
               header_t header      = sizemask;
               size_t   needed_size = HEADERSIZE + (prefixlen > 2) + prefixlen
                                    + (isuser ? sizeof(void*) : 0) + (ischild == 2 ? 1+sizeof(trie_subnode_t*) : 0);
               size_t   lenchild     = ischild == 0 ? 0 : ischild == 2 ? 1
                                       : (sizenode_header(header)-needed_size)/(sizeof(trie_node_t*)+1);
               // encode additional header bits
               if (ischild == 1)          header |= header_CHILD;
               else if (ischild == 2)     header |= header_SUBNODE;
               if (isuser)                header |= header_USERVALUE;
               if (prefixlen == 1)        header |= header_PREFIX1;
               else if (prefixlen == 2)   header |= header_PREFIX2;
               else if (prefixlen >  2)   header |= header_PREFIX_LEN;
               trie_node_t dummy;
               memset(&dummy, 0, sizeof(dummy));
               dummy.header    = header;
               dummy.prefixlen = (uint8_t) prefixlen;
               // decode
               memset(&offsets, 255, sizeof(offsets));
               int err = initdecode_trienodeoffsets(&offsets, &dummy);
               if (  needed_size > sizenode_header(sizemask)
                     || sizenode_header(sizemask) > SIZEMAXNODE
                     || (1 == ischild && 0 == lenchild)) {
                  TEST(EINVARIANT == err);
                  break;
               }
               TEST(0 == err);
               // compare result
               TEST(offsets.nodesize  == sizenode_header(header));
               TEST(offsets.lenchild  == lenchild);
               TEST(offsets.header    == header);
               TEST(offsets.prefix    == HEADERSIZE       + (prefixlen > 2));
               TEST(offsets.digit     == offsets.prefix   + prefixlen);
               size_t aligned = (offsets.digit + lenchild + PTRALIGN-1) & ~(PTRALIGN-1);
               TEST(offsets.uservalue == aligned);
               TEST(offsets.child     == offsets.uservalue + isuser * sizeof(void*));
               TEST(offsets.nodesize  >= offsets.child + lenchild * sizeof(trie_node_t*));
               TEST(offsets.nodesize  <= offsets.child + (lenchild+1) * sizeof(trie_node_t*) || ischild != 1);
            }
         }
      }
   }

   // TEST initdecode_trienodeoffsets: header_CHILD and header_SUBNODE set
   TEST(EINVARIANT == initdecode_trienodeoffsets(&offsets, &(trie_node_t) { .header = header_SIZE5NODE|header_SUBNODE|header_CHILD } ));

   // ////
   // group query

   // TEST compare_trienodeoffsets
   trie_nodeoffsets_t offsets2;
   memset(&offsets, 0, sizeof(offsets));
   memset(&offsets2, 0, sizeof(offsets2));
   for (unsigned i = 0; i < 7; ++i) {
      for (int v = -1; v <= 0; ++v) {
         switch (i)
         {
         case 0: offsets.nodesize   = (typeof(offsets.nodesize))v;  break;
         case 1: offsets.lenchild   = (typeof(offsets.lenchild))v;  break;
         case 2: offsets.header     = (typeof(offsets.header))v;    break;
         case 3: offsets.prefix     = (typeof(offsets.prefix))v;    break;
         case 4: offsets.digit      = (typeof(offsets.digit))v;     break;
         case 5: offsets.uservalue  = (typeof(offsets.uservalue))v; break;
         case 6: offsets.child      = (typeof(offsets.child))v;     break;
         }
         if (v) {
            TEST(0 <  compare_trienodeoffsets(&offsets, &offsets2));
            TEST(0 >  compare_trienodeoffsets(&offsets2, &offsets));
         } else {
            TEST(0 == compare_trienodeoffsets(&offsets, &offsets2));
            TEST(0 == compare_trienodeoffsets(&offsets2, &offsets));
         }
      }
   }

   // TEST isexpandable_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets));
   for (uint16_t i = 0; i < SIZEMAXNODE; ++i) {
      offsets.nodesize = i;
      TEST(1 == isexpandable_trienodeoffsets(&offsets));
   }
   for (uint16_t i = SIZEMAXNODE; i < SIZEMAXNODE+3; ++i) {
      offsets.nodesize = i;
      TEST(0 == isexpandable_trienodeoffsets(&offsets));
   }

   // TEST lenprefix_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets));
   for (unsigned len = 0; len < 256; ++len) {
      for (unsigned s = 0; s < 256-len; ++s) {
         static_assert(offsetof(trie_nodeoffsets_t, digit) == sizeof(uint8_t) + offsetof(trie_nodeoffsets_t, prefix), "digit is next after prefix");
         offsets.prefix = (uint8_t) s;
         offsets.digit  = (uint8_t) (s + len);
         TEST(len == lenprefix_trienodeoffsets(&offsets));
      }
   }

   // TEST lenuservalue_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets));
   for (uint16_t i = 0; i < SIZEMAXNODE; ++i) {
      for (int isuser = 0; isuser <= 1; ++isuser) {
         static_assert(offsetof(trie_nodeoffsets_t, child) == sizeof(uint8_t) + offsetof(trie_nodeoffsets_t, uservalue), "child is next after uservalue");
         int len = isuser ? sizeof(void*) : 0;
         offsets.uservalue = (uint8_t) i;
         offsets.child     = (uint8_t) (i + len);
         TEST(len == lenuservalue_trienodeoffsets(&offsets));
      }
   }

   // TEST uservalue_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets));
   for (unsigned i = 0; i < SIZEMAXNODE/sizeof(trie_node_t*); ++i) {
      uint8_t array[SIZEMAXNODE] = { 0 };
      offsets.uservalue = (uint8_t) (i * sizeof(trie_node_t*));
      TEST(&((void**)array)[i] == uservalue_trienodeoffsets(&offsets, (trie_node_t*)array));
   }

   // TEST sizefree_trienodeoffsets, sizegrowprefix_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets));
   TEST(0 == sizegrowprefix_trienodeoffsets(&offsets));
   TEST(0 == sizefree_trienodeoffsets(&offsets));
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= LENCHILDMAX+1; ++nrchild) {
         for (uint16_t prefixlen = 0; prefixlen <= 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            if (lenprefix_trienodeoffsets(&offsets) != prefixlen) continue; // too big
            unsigned expect = (unsigned) offsets.nodesize
                              - (nrchild <= LENCHILDMAX ? nrchild*(sizeof(trie_node_t*)+1) : (sizeof(trie_node_t*)+1))
                              - prefixlen - (prefixlen>2) - HEADERSIZE - (isuser ? sizeof(void*) : 0);
            while (nrchild && nrchild <= LENCHILDMAX && expect >= (sizeof(trie_node_t*)+1)) {
               expect -= (sizeof(trie_node_t*)+1);
            }
            TEST(expect == sizefree_trienodeoffsets(&offsets));
            if ((expect+prefixlen) > 2 && (offsets.header&header_PREFIX_MASK) != header_PREFIX_LEN) -- expect;
            TEST(expect == sizegrowprefix_trienodeoffsets(&offsets));
         }
      }
   }

   // ////
   // group change

   // TEST convert2subnode_trienodeoffsets
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 1; nrchild <= 16; ++nrchild) {
         for (uint32_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild);
            TEST(1 == ischild_header(offsets.header));
            trie_nodeoffsets_t oldoff = offsets;
            convert2subnode_trienodeoffsets(&offsets);
            // check adapted offsets
            TEST(offsets.nodesize  == oldoff.nodesize);
            TEST(offsets.lenchild  == 0);
            TEST(offsets.header    == ((oldoff.header & ~(header_CHILD)) | header_SUBNODE));
            TEST(offsets.prefix    == oldoff.prefix);
            TEST(offsets.digit     == oldoff.digit);
            size_t aligned = (offsets.digit + 1u + PTRALIGN-1) & ~(PTRALIGN-1);
            TEST(offsets.uservalue == aligned);
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
            TEST(offsets.nodesize  >= offsets.child + sizeof(trie_subnode_t*));
         }
      }
   }

   // TEST shrinkprefix_trienodeoffsets
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1; prefixlen < 16; ++prefixlen) {
            for (uint8_t newprefixlen = 0; newprefixlen < prefixlen; ++newprefixlen) {
               init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild);
               trie_nodeoffsets_t oldoff = offsets;
               unsigned freesize = (unsigned) offsets.nodesize - sizeof(header_t)
                                 - (newprefixlen > 2) - newprefixlen
                                 - (isuser ? sizeof(void*) : 0);
               shrinkprefix_trienodeoffsets(&offsets, newprefixlen);
               // check adapted offsets
               TEST(offsets.nodesize  == oldoff.nodesize);
               TEST(offsets.lenchild  == (0 == nrchild ? 0 : nrchild <= LENCHILDMAX ? freesize/(sizeof(trie_node_t*)+1) : 1));
               switch (newprefixlen) {
               case 0:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_NOPREFIX));  break;
               case 1:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX1));   break;
               case 2:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX2));   break;
               default: TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX_LEN)); break;
               }
               TEST(offsets.prefix    == oldoff.prefix - (newprefixlen <= 2 && prefixlen > 2));
               TEST(offsets.digit     == offsets.prefix + newprefixlen);
               size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1);
               TEST(offsets.uservalue == aligned);
               TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
            }
         }
      }
   }

   // TEST changesize_trienodeoffsets
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint32_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild);
            for (header_t headersize = header_SIZE0NODE; headersize <= header_SIZE5NODE; ++ headersize) {
               if (offsets.child >= sizenode_header(headersize)) continue;
               trie_nodeoffsets_t oldoff = offsets;
               changesize_trienodeoffsets(&offsets, headersize);
               TEST(offsets.nodesize  == sizenode_header(headersize));
               TEST(offsets.header    == ((oldoff.header&~header_SIZENODE_MASK)|headersize));
               TEST(offsets.prefix    == oldoff.prefix);
               TEST(offsets.digit     == oldoff.digit);
               size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1);
               TEST(offsets.uservalue == aligned);
               TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*));
               TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*));
               if (ischild_header(offsets.header)) {
                  TEST(1 <= offsets.lenchild);
                  TEST(offsets.nodesize < offsets.child + offsets.lenchild * sizeof(trie_node_t*) + sizeof(trie_node_t*) + 1);
               } else if (issubnode_header(offsets.header)) {
                  TEST(1 == offsets.lenchild);
               } else {
                  TEST(0 == offsets.lenchild);
               }
            }
         }
      }
   }

   // TEST growprefix_trienodesoffsets: isshrinkchild = 0
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            unsigned growsize = sizegrowprefix_trienodeoffsets(&offsets);
            for (unsigned incr = 1; incr <= growsize; ++incr) {
               trie_nodeoffsets_t offsets3 = offsets;
               init_trienodeoffsets(&offsets2, (uint16_t)(prefixlen + incr), isuser, nrchild);
               growprefix_trienodesoffsets(&offsets3, (uint8_t) incr, 0);
               TEST(0 == compare_trienodeoffsets(&offsets2, &offsets3));
            }
         }
      }
   }

   // TEST growprefix_trienodesoffsets: isshrinkchild = 1
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 2; nrchild <= LENCHILDMAX; ++nrchild) {
         for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild);
            if (lenprefix_trienodeoffsets(&offsets) < prefixlen) continue; // not enough space
            unsigned growsize = sizegrowprefix_trienodeoffsets(&offsets);
            for (unsigned incr = 1; incr <= growsize+sizeof(trie_node_t*); ++incr) {
               trie_nodeoffsets_t offsets3 = offsets;
               uint16_t           nrchild2 = (uint16_t) ((offsets.lenchild > LENCHILDMAX ? LENCHILDMAX : offsets.lenchild) - (incr>growsize));
               init_trienodeoffsets(&offsets2, (uint16_t)(prefixlen + incr), isuser, nrchild2);
               growprefix_trienodesoffsets(&offsets3, (uint8_t) incr, (incr>growsize));
               TEST(0 == compare_trienodeoffsets(&offsets2, &offsets3));
            }
         }
      }
   }

   // TEST adduservalue_trienodeoffsets
   for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t) (nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      trie_node_t * nodebuffer[SIZEMAXNODE/sizeof(trie_node_t*)] = { 0 };
      trie_node_t * node = (trie_node_t*) nodebuffer;
      for (uint16_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
         init_trienodeoffsets(&offsets, prefixlen, 0/*no user*/, nrchild);
         bool isfreesize = (sizefree_trienodeoffsets(&offsets) >= sizeof(void*));
         if (! isfreesize && ! isfreechild_trienode(node, &offsets)) continue;
         offsets2 = offsets;
         adduservalue_trienodeoffsets(&offsets);
         TEST(offsets.nodesize == offsets2.nodesize);
         TEST(offsets.header == (offsets2.header | header_USERVALUE));
         TEST(offsets.prefix == offsets2.prefix);
         TEST(offsets.digit  == offsets2.digit);
         if (isfreesize) {
            TEST(offsets.lenchild  == offsets2.lenchild);
            TEST(offsets.uservalue == offsets2.uservalue);
            TEST(offsets.child     == offsets2.child + sizeof(void*));
         } else {
            TEST(nrchild <= LENCHILDMAX);
            TEST(2 <= offsets2.lenchild);
            TEST(offsets.lenchild  == offsets2.lenchild-1);
            TEST(offsets.uservalue == (((unsigned)offsets2.digit+offsets.lenchild+PTRALIGN-1)&~(PTRALIGN-1)));
            TEST(offsets.child     == offsets.uservalue + sizeof(void*));
            TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*));
            TEST(offsets.nodesize  <= offsets.child + (offsets.lenchild+1u) * sizeof(trie_node_t*));
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
      TEST(&nodebuffer[i] == digit_trienode(node, &offsets));
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
      TEST(&nodebuffer[i] == prefix_trienode(node, &offsets));
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
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
               trie_nodeoffsets_t oldoff = offsets;
               TEST(0 == shrinksize_trienode(&node, &offsets));
               TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));  // did nothing
               if (  SIZE1NODE < offsets.nodesize
                     && offsets.child < offsets.nodesize/2) {
                  const unsigned nrchildkept = isclearallchild ? 1 : (offsets.nodesize/2u - offsets.child) / sizeof(trie_node_t*);
                  if (isclearallchild) {  // clear all except first child
                     memset(child_trienode(node, &offsets)+1, 0, (unsigned)offsets.nodesize-offsets.child-sizeof(trie_node_t*));
                  } else {                // clear childs  from offset nodesize/2
                     memset(offsets.nodesize/2 + (uint8_t*)node, 0, offsets.nodesize/2);
                  }
                  TEST(0 == shrinksize_trienode(&node, &offsets));
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
               TEST(0 == delete_trienode(&node));
            }
         }
      }
   }

   // TEST shrinksize_trienode: ERROR
   TEST(0 == new_trienode(&node, &offsets, 15, key.addr, &uservalue, 16, digit, child));
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
   {
      trie_nodeoffsets_t oldoff = offsets;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
      TEST(ENOMEM == shrinksize_trienode(&node, &offsets));
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
      TEST(node->header == offsets.header);
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));
   }
   TEST(0 == delete_trienode(&node));

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
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
            TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
            trie_nodeoffsets_t oldoff = offsets;
            TEST(0 == expand_trienode(&node, &offsets));
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
            TEST(0 == deletenode_trienode(&node));
            TEST(allocsize == SIZEALLOCATED_MM());
         }
      }
   }

   // TEST expand_trienode: ERROR
   TEST(0 == new_trienode(&node, &offsets, 15, key.addr, &uservalue, 16, digit, child));
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child);
   {
      trie_nodeoffsets_t oldoff = offsets;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
      TEST(ENOMEM == expand_trienode(&node, &offsets));
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM());
      TEST(node->header == offsets.header);
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff));
   }
   TEST(0 == delete_trienode(&node));
   memset(child, 0, sizeof(child));

   // TEST tryextendprefix_trienode
   for (uint8_t isuser = 0; isuser <= 1; ++isuser) {
      for (uint16_t nrchild = 0; nrchild <= 256; ++nrchild, nrchild = (uint16_t)(nrchild == 20 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 0; prefixlen < 16; ++prefixlen) {
            uservalue = (void*) ((uintptr_t)100 * nrchild + 11u * prefixlen);
            for (unsigned i = 0; i < nrchild; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 0, 0, 0, 0, 0, 0));
            }
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
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
            TEST(0 == delete_trienode(&node));
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
         TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, 0, nrchild, digit, child));
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
         TEST(0 == delete_trienode(&node));
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
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit+11, child));
            // compare result
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, nrchild, digit+11, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
            // delete_trienode
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
            TEST(0 == delete_trienode(&node));
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
            TEST(0 == new_trienode(&node, &offsets, (uint16_t)prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, (uint8_t[]){13,15}, child));
            // compare content of chain
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, (uint16_t)prefixlen, key.addr, isuser, uservalue, nrchild, (uint8_t[]){13,15}, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
            // delete_trienode
            TEST(0 == delete_trienode(&node));
            TEST(0 == node);
            TEST(0 == delete_trienode(&node));
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
         TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, lengthof(child), digit, child));
         // compare result
         TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, lengthof(child), digit, expectchilds));
         TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0));
         TEST(0 == delete_trienode(&node));
         TEST(0 == node);
      }
   }

   // TEST delete_trienode: trie_subnode_t of trie_subnode_t
   TEST(0 == build_subnode_trie(&node, 2, 32));
   TEST(0 != node);
   initdecode_trienodeoffsets(&offsets, node);
   TEST(1 == issubnode_header(node->header));
   TEST(255 == digit_trienode(node, &offsets)[0]);
   TEST(0 == delete_trienode(&node));   // test delete with tree of subnode_t
   TEST(0 == node);

   // TEST delete_trienode: subnode == 0
   TEST(0 == build_subnode_trie(&node, 0, 256));
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
   TEST(0 == delete_trienode(&node));
   TEST(0 == node);

   // TEST delete_trienode: subnode with all childs == 0
   for (int isdelsub2 = 0; isdelsub2 <= 1; ++isdelsub2) {
      TEST(0 == build_subnode_trie(&node, 0, 256));
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
      TEST(0 == delete_trienode(&node));
      TEST(0 == node);
   }

   // TEST delete_trienode: subnode with only single child
   for (int isdelsub2 = 0; isdelsub2 <= 1; ++isdelsub2) {
      for (unsigned ci = 0; ci < 256; ++ci) {
         TEST(0 == build_subnode_trie(&node, 0, 256));
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
         TEST(0 == delete_trienode(&node));
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
                  TEST(0 == new_trienode(&node2, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
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
                  TEST(0 == newsplit_trienode(&node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0]));
                  // compare result
                  uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit };
                  expectchilds[0] = expectnode2;
                  TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds));
                  TEST(0 == compare_expectnode(expectnode, node, 0, 0, 1));
                  TEST(0 == delete_trienode(&node));
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
            TEST(0 == new_trienode(&node2, &offsets, prefixlen, key.addr, 0, 2, key.addr+prefixlen, child+2));
            // test newsplit_trienode
            child[0]  = 0;
            uservalue = (void*)(uintptr_t)(splitprefixlen*120+prefixlen);
            uint8_t splitdigit = (uint8_t) (key.addr[splitprefixlen] + (prefixlen%2 ? -1 : +1));
            if (splitparam) {
               trie_nodeoffsets_t offsets2;
               TEST(0 == new_trienode(&child[0], &offsets2, 3, key.addr+60, 0, 0, 0, 0));
               TEST(0 == new_expectnode(&expectchilds[1], &expectnode_memblock2, 3, key.addr+60, 0, 0, 0, 0, 0));
            }
            TEST(0 == newsplit_trienode(&node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0]));
            // compare result
            TEST(node == node2); // merged
            uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit };
            expectchilds[0] = expectnode2;
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds));
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 1, 0));
            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST new_trienode, delete_trienode: ERROR
   memset(child, 0, sizeof(child));
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child));
   TEST(0 == node);
   TEST(0 == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child));
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_trienode(&node));
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
      TEST(ENOMEM == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child));
      TEST(0 == node);
   }

   // TEST delete_trienode: ERROR (no log cause of overflow)
   CLEARBUFFER_ERRLOG();
   for (int issubnode = 0; issubnode < 2; ++issubnode) {
      for (uint32_t errcount = 1; errcount < 3; ++errcount) {
         if (issubnode) {
            TEST(0 == build_subnode_trie(&node, 0, 1));
         } else {
            TEST(0 == new_trienode(&node, &offsets, 2000, key.addr, 0, 0, 0, 0));
         }
         TEST(0 != node);
         init_testerrortimer(&s_trie_errtimer, errcount, EINVAL);
         TEST(EINVAL == delete_trienode(&node));
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
               TEST(0 == new_trienode(&node, &offsets, 3, (const uint8_t*)"123", 0, 0, 0, 0));
               digit[i] = (uint8_t) (prefixlen * 14u + i);
               child[i] = node;
            }
            uservalue = (void*)(uintptr_t)(10+prefixlen);
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
            trie_nodeoffsets_t oldoff = offsets;
            allocsize = SIZEALLOCATED_MM();
            TEST(0 == convertchild2sub_trienode(&node, &offsets));
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
            TEST(0 == delete_trienode(&node));
         }
      }
   }

   // TEST convertchild2sub_trienode: EINVAL
   TEST(0 == new_trienode(&node, &offsets, 1, key.addr, &uservalue, 0, 0, 0));
   TEST(0 == ischild_header(offsets.header));
   TEST(EINVAL == convertchild2sub_trienode(&node, &offsets));  // no childs
   TEST(0 == delete_trienode(&node));
   TEST(0 == new_trienode(&node, &offsets, 1, key.addr, &uservalue, 2, digit, child));
   TEST(1 == ischild_header(offsets.header));
   TEST(EINVAL == convertchild2sub_trienode(&node, &offsets));  // all child pointer set to 0
   TEST(0 == delete_trienode(&node));

   // TEST convertchild2sub_trienode: ENOMEM (subnode creation fails)
   allocsize = SIZEALLOCATED_MM();
   for (unsigned i = 0; i < 16; ++i) {
      TEST(0 == new_trienode(&child[i], &offsets, 0, 0, &uservalue, 0, 0, 0));
   }
   TEST(0 == new_trienode(&node, &offsets, 4, key.addr, &uservalue, 16, digit, child));
   // save old state
   size_t nodesize = SIZEALLOCATED_MM() - allocsize;
   memcpy(key.addr, &offsets, sizeof(offsets));
   memcpy(key.addr + sizeof(offsets), node, offsets.nodesize);
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == convertchild2sub_trienode(&node, &offsets));
   // test nothing changed
   TEST(allocsize+nodesize == SIZEALLOCATED_MM());
   TEST(0 == memcmp(key.addr, &offsets, sizeof(offsets)));
   TEST(0 == memcmp(key.addr + sizeof(offsets), node, offsets.nodesize));

   // TEST convertchild2sub_trienode: ENOMEM ignored (shrinksize_trienode fails)
   init_testerrortimer(&s_trie_errtimer, 4, ENOMEM);
   TEST(0 == convertchild2sub_trienode(&node, &offsets));
   nodesize += sizeof(trie_subnode_t) + 2 * sizeof(trie_subnode2_t);
   TEST(1 == issubnode_header(offsets.header));      // offsets changed
   TEST(node->header == offsets.header);            // also node
   TEST(allocsize+nodesize == SIZEALLOCATED_MM());   // but shrinksize_trienode failed
   TEST(0 == delete_trienode(&node));
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
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               // normal case
               shrinkprefixkeeptail_trienode(node, &offsets, newprefixlen);
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr+prefixlen-newprefixlen, isuser, uservalue, nrchild, digit, expectchilds));
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0));
               TEST(0 == delete_trienode(&node));
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
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child));
               // normal case
               shrinkprefixkeephead_trienode(node, &offsets, newprefixlen);
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr, isuser, uservalue, nrchild, digit, expectchilds));
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0));
               TEST(0 == delete_trienode(&node));
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
      subnode->child[i] = (trie_nodedata_t*) (i+1);
   }
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      trie_nodedata_t * child = (trie_nodedata_t*) (i+1);
      TEST(child == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   // == group: change ==

   // TEST setchild_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      setchild_triesubnode(subnode, (uint8_t)i, (trie_nodedata_t*) (i+1));
   }
   for (unsigned i = 0; i < lengthof(subnode->child); ++i) {
      trie_nodedata_t * child = (trie_nodedata_t*) (i+1);
      TEST(child == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   // TEST clearchild_triesubnode
   TEST(0 == new_triesubnode(&subnode));
   for (uintptr_t i = 0; i < lengthof(subnode->child); ++i) {
      setchild_triesubnode(subnode, (uint8_t)i, (trie_nodedata_t*) 100);
      clearchild_triesubnode(subnode, (uint8_t)i);
      TEST(0 == child_triesubnode(subnode, (uint8_t)i));
   }
   TEST(0 == delete_triesubnode(&subnode));

   return 0;
ONABORT:
   return EINVAL;
}

static int test_nodedata(void)
{
   trie_nodedata_t   data;
   trie_nodedata_t * node = 0;
   uint_fast16_t     size;
   size_t            size_allocated;

   // == group: constants ==

   // TEST PTRALIGN
   TEST(PTRALIGN + (uint8_t*)&data == (uint8_t*) &data.uservalue);

   // == group: query ==

   // TEST memaddr_trienodedata
   TEST(0 == memaddr_trienodedata(0));
   TEST((uint8_t*)&data == memaddr_trienodedata(&data));

   // == group: lifetime ==

   size_allocated = SIZEALLOCATED_MM();
   for (size = 8; size <= MAXSIZE; size *= 2) {
      // TEST new_trienodedata
      TEST(0 == new_trienodedata(&node, size));
      TEST(0 != node);
      TEST(size_allocated + size == SIZEALLOCATED_MM());

      // TEST delete_trienodedata
      TEST(0 == delete_trienodedata(&node, size));
      TEST(0 == node);
      TEST(size_allocated == SIZEALLOCATED_MM());
      TEST(0 == delete_trienodedata(&node, size));
      TEST(0 == node);
   }

   // TEST new_trienodedata: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   TEST(ENOMEM == new_trienodedata(&node, size));
   TEST(0 == node);
   TEST(size_allocated == SIZEALLOCATED_MM());

   // TEST delete_trienodedata: ERROR
   TEST(0 == new_trienodedata(&node, size));
   TEST(0 != node);
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL);
   TEST(EINVAL == delete_trienodedata(&node, size));
   TEST(0 == node);
   TEST(size_allocated == SIZEALLOCATED_MM());

   return 0;
ONABORT:
   (void) delete_trienodedata(&node, size);
   return EINVAL;
}

static int test_node_query(void)
{
   void            * buffer[2 + (MAXSIZE / sizeof(void*))] = { 0 };
   trie_nodedata_t * data = (trie_nodedata_t*) &buffer[1];
   trie_node_t       node;

   // prepare
   memset(buffer, 0, sizeof(buffer));
   memset(&node, 0, sizeof(node));
   node.data = data;

   // == group: constants ==

   // TEST MAXSIZE
   node.data->header = header_SIZEMAX << header_SIZESHIFT;
   TEST(MAXSIZE == decodesize_trienode(&node));

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
   data->header = header_SUBNODE;
   TEST(0 != issubnode_trienode(&node));
   data->header = (header_t) ~ header_SUBNODE;
   TEST(0 == issubnode_trienode(&node));
   data->header = 0;
   TEST(0 == issubnode_trienode(&node));

   // TEST isuservalue_trienode
   data->header = header_USERVALUE;
   TEST(0 != isuservalue_trienode(&node));
   data->header = (header_t) ~ header_USERVALUE;
   TEST(0 == isuservalue_trienode(&node));
   data->header = 0;
   TEST(0 == isuservalue_trienode(&node));

   // TEST decodesize_trienode
   node.data->header = header_SIZE0 << header_SIZESHIFT;
   TEST( 2*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZE1 << header_SIZESHIFT;
   TEST( 4*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZE2 << header_SIZESHIFT;
   TEST( 8*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZE3 << header_SIZESHIFT;
   TEST(16*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZE4 << header_SIZESHIFT;
   TEST(32*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZE5 << header_SIZESHIFT;
   TEST(64*sizeof(void*) == decodesize_trienode(&node));
   node.data->header = header_SIZEMAX << header_SIZESHIFT;
   TEST(64*sizeof(void*) == decodesize_trienode(&node));

   // == group: query-fields ==

   // TEST data_trienode
   TEST(data == data_trienode(&node));
   node.data = 0;
   TEST(0 == data_trienode(&node));
   node.data = data;
   TEST(data == data_trienode(&node));

   // TEST memaddr_trienode
   node.data = 0;
   TEST(0 == memaddr_trienode(&node));
   node.data = data;
   TEST((uint8_t*)data == memaddr_trienode(&node));

   // TEST off1_keylen_trienode
   TEST((unsigned)&((trie_nodedata_t*)0)->keylen == off1_keylen_trienode());

   // TEST off2_key_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off2_key = i;
      TEST(i == off2_key_trienode(&node));
   }

   // TEST off3_digit_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off3_digit = i;
      TEST(i == off3_digit_trienode(&node));
   }

   // TEST off4_uservalue_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off4_uservalue = i;
      TEST(i == off4_uservalue_trienode(&node));
   }

   // TEST off5_child_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off5_child = i;
      TEST(i == off5_child_trienode(&node));
   }

   // TEST off6_free_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off5_child = i;
      for (uint_fast16_t nrchild = 255; nrchild <= 255; --nrchild) {
         data->nrchild = (uint8_t) nrchild;
         data->header = header_SUBNODE;
         TEST(i + sizeof(void*) == off6_free_trienode(&node));
         data->header = (header_t) ~header_SUBNODE;
         TEST(i + nrchild * sizeof(void*) == off6_free_trienode(&node));
         data->header = 0;
         TEST(i + nrchild * sizeof(void*) == off6_free_trienode(&node));
      }
   }

   // TEST nrchild_trienode
   for (uint_fast16_t nrchild = 255; nrchild <= 255; --nrchild) {
      data->nrchild = (uint8_t) nrchild;
      data->header = header_SUBNODE;
      TEST(nrchild == nrchild_trienode(&node));
      data->header = 0;
      TEST(nrchild == nrchild_trienode(&node));
   }

   // == group: query-helper ==

   // TEST alignoffset_trienode
   TEST(0 == alignoffset_trienode(0));
   for (uint_fast16_t i = 0; i < 10; ++i) {
      unsigned offset = sizeof(void*)*i;
      for (uint_fast16_t b = 1; b <= sizeof(void*); ++b) {
         TEST(offset+sizeof(void*) == alignoffset_trienode(offset+b));
      }
   }

   // TEST childs_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off5_child = i;
      node.data = 0;
      TEST((void**)i == childs_trienode(&node));
      node.data = data;
      TEST((void**)(i+(uint8_t*)data) == childs_trienode(&node));
   }

   // TEST childsize_trienode
   for (uint_fast16_t nrchild = 255; nrchild <= 255; --nrchild) {
      data->nrchild = (uint8_t) nrchild;
      data->header = header_SUBNODE;
      TEST(sizeof(void*) == childsize_trienode(issubnode_trienode(&node), nrchild_trienode(&node)));
      data->header = (header_t) ~header_SUBNODE;
      TEST(nrchild * sizeof(void*) == childsize_trienode(issubnode_trienode(&node), nrchild_trienode(&node)));
      data->header = 0;
      TEST(nrchild * sizeof(void*) == childsize_trienode(issubnode_trienode(&node), nrchild_trienode(&node)));
   }

   // TEST digits_trienode
   for (uint_fast16_t i = 1024; i <= 1024; --i) {
      node.off3_digit = i;
      node.data = 0;
      TEST((uint8_t*)i == digits_trienode(&node));
      node.data = data;
      TEST(i+(uint8_t*)data == digits_trienode(&node));
   }

   // TEST keylen_trienode
   data->header = 0;
   data->keylen = 0;
   for (uint8_t i = 1; i; ++i) {
      node.off2_key   = 0;
      node.off3_digit = i;
      TEST(i == keylen_trienode(&node));
      node.off2_key   = 5;
      node.off3_digit = (uint_fast16_t) (i+5);
      TEST(i == keylen_trienode(&node));
   }
   node.off2_key   = 0;
   node.off3_digit = 0;
   TEST(0 == keylen_trienode(&node));

   // TEST subnode_trienode
   for (uint_fast16_t i = 0; i < MAXSIZE/sizeof(void*); ++i) {
      node.off5_child = i * sizeof(void*);
      ((void**)data)[i] = 0;
      TEST(0 == subnode_trienode(&node));
      ((void**)data)[i] = (void*)buffer;
      TEST((trie_subnode_t*)buffer == subnode_trienode(&node));
   }
   node.off5_child = 0;

   // TEST usedsize_trienode
   for (unsigned i = 1024; i <= 1024; --i) {
      node.off3_digit     = (uint_fast16_t) i;
      node.off4_uservalue = (uint_fast16_t) (node.off3_digit + 256 + 4*i);
      node.off5_child     = (uint_fast16_t) (node.off4_uservalue + (i % 2 ? sizeof(void*) : 0));
      for (unsigned nrchild = 255; nrchild <= 255; --nrchild) {
         size_t size = i + (i % 2 ? sizeof(void*) : 0);
         data->nrchild = (uint8_t) nrchild;
         data->header  = header_SUBNODE;
         TEST(size + sizeof(void*) == usedsize_trienode(&node));
         data->header  = 0;
         TEST(size + nrchild * (1+sizeof(void*)) == usedsize_trienode(&node));
      }
   }
   node.off3_digit     = 0;
   node.off4_uservalue = 0;
   node.off5_child     = 0;

   // TEST sizeuservalue_trienode
   for (uint_fast16_t i = 256; i <= 256; --i) {
      node.off4_uservalue = i;
      node.off5_child     = 2*i;
      TEST(i == sizeuservalue_trienode(&node));
   }

   return 0;
ONABORT:
   return EINVAL;
}

static void get_node_size(unsigned size, /*out*/unsigned * nodesize, /*out*/header_t * sizeflag)
{
   if (size <= MAXSIZE/32) {
      *nodesize = MAXSIZE/32;
      *sizeflag = header_SIZE0;
   } else if (size <= MAXSIZE/16) {
      *nodesize = MAXSIZE/16;
      *sizeflag = header_SIZE1;
   } else if (size <= MAXSIZE/8) {
      *nodesize = MAXSIZE/8;
      *sizeflag = header_SIZE2;
   } else if (size <= MAXSIZE/4) {
      *nodesize = MAXSIZE/4;
      *sizeflag = header_SIZE3;
   } else if (size <= MAXSIZE/2) {
      *nodesize = MAXSIZE/2;
      *sizeflag = header_SIZE4;
   } else {
      *nodesize = MAXSIZE;
      *sizeflag = header_SIZE5;
   }
}

static int test_node_lifetime(void)
{
   trie_node_t       node;
   trie_node_t       node2;
   trie_nodedata_t * data = 0;
   size_t            size_allocated = SIZEALLOCATED_MM();

   memset(&node, 0, sizeof(node));
   memset(&node2, 0, sizeof(node2));

   // == group: lifetime ==

   for (uint8_t isuservalue = 0, maxchild = MAXNROFCHILD_NOUSERVALUE+1; isuservalue <= 1; ++isuservalue, maxchild = MAXNROFCHILD_WITHUSERVALUE+1) {
      const unsigned usersize = isuservalue ? sizeof(void*) : 0;

      for (uint8_t nrchild = 0; nrchild < maxchild; ++nrchild) {

         const unsigned childsize = nrchild * sizeof(void*);

         for (unsigned keylen = 0; keylen <= 255; ++keylen) {

            header_t header = isuservalue ? header_USERVALUE : 0;
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

            header = (header_t) (lenbyte ? encodekeylenbyte_header(header) : encodekeylen_header(header, (uint8_t)keysize));

            header_t sizeflag;
            unsigned nodesize;

            get_node_size(size, &nodesize, &sizeflag);
            header = encodesizeflag_header(header, sizeflag);

            // TEST init_trienode: child array
            TEST(0 == init_trienode(&node, isuservalue!=0, nrchild, (uint8_t)keylen));
            TEST(size_allocated + nodesize == SIZEALLOCATED_MM());
            TEST(0 != node.data);
            TEST(header  == node.data->header);
            TEST(nrchild == node.data->nrchild);
            TEST(keysize == keylen_trienode(&node));
            unsigned off = off1_keylen_trienode() + lenbyte;
            TEST(off == node.off2_key);
            off += keysize;
            TEST(off == node.off3_digit);
            off += nrchild;
            if (off % PTRALIGN) off += PTRALIGN - (off % PTRALIGN);
            TEST(off == node.off4_uservalue);
            off += usersize;
            TEST(off == node.off5_child);
            off += childsize;
            TEST(off == off6_free_trienode(&node));

            // TEST initdecode_trienode
            TEST(0 == initdecode_trienode(&node2, node.data));
            TEST(0 == memcmp(&node2, &node, sizeof(node)));
            TEST(header  == node2.data->header);
            TEST(nrchild == node2.data->nrchild);
            TEST(keysize == keylen_trienode(&node2));

            // TEST free_trienode: child array
            TEST(0 == free_trienode(&node));
            TEST(0 == node.data);
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == free_trienode(&node));
            TEST(0 == node.data);
         }
      }

      for (uint8_t nrchild = maxchild; nrchild >= maxchild; ++nrchild) {

         for (int keylen = 0; keylen <= 255; ++keylen) {

            header_t header = (header_t) (isuservalue ? header_USERVALUE|header_SUBNODE : header_SUBNODE);
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

            header = (header_t) (lenbyte ? encodekeylenbyte_header(header) : encodekeylen_header(header, (uint8_t)keysize));

            header_t sizeflag;
            unsigned nodesize;

            get_node_size(size, &nodesize, &sizeflag);
            header = encodesizeflag_header(header, sizeflag);

            // TEST init_trienode: subnode
            TEST(0 == init_trienode(&node, isuservalue!=0, nrchild, (uint8_t)keylen));
            TEST(size_allocated + nodesize + sizeof(trie_subnode_t) == SIZEALLOCATED_MM());
            TEST(0 != node.data);
            TEST(header  == node.data->header);
            TEST(nrchild-1 == node.data->nrchild);
            TEST(keysize == keylen_trienode(&node));
            unsigned off = off1_keylen_trienode() + lenbyte;
            TEST(off == node.off2_key);
            off += keysize;
            TEST(off == node.off3_digit);
            if (off % PTRALIGN) off += PTRALIGN - (off % PTRALIGN);
            TEST(off == node.off4_uservalue);
            off += usersize;
            TEST(off == node.off5_child);
            off += sizeof(void*);
            TEST(off == off6_free_trienode(&node));

            // TEST initdecode_trienode
            TEST(0 == initdecode_trienode(&node2, node.data));
            TEST(0 == memcmp(&node2, &node, sizeof(node)));
            TEST(header  == node2.data->header);
            TEST(nrchild-1 == node2.data->nrchild);
            TEST(keysize == keylen_trienode(&node2));

            // TEST free_trienode: subnode
            TEST(0 == free_trienode(&node));
            TEST(0 == node.data);
            TEST(size_allocated == SIZEALLOCATED_MM());
            TEST(0 == free_trienode(&node));
            TEST(0 == node.data);
         }
      }
   }

   // TEST init_trienode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM);
   // no subnode
   TEST(ENOMEM == init_trienode(&node, 0, 1, 1));
   TEST(0 == node.data);
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      init_testerrortimer(&s_trie_errtimer, i, ENOMEM);
      TEST(ENOMEM == init_trienode(&node, 1, MAXNROFCHILD_WITHUSERVALUE+1, 100));
      TEST(0 == node.data);
   }

   // TEST initdecode_trienode: ERROR
   TEST(0 == init_trienode(&node, 0, MAXNROFCHILD_NOUSERVALUE, 1));
   // content is bigger than nodesize
   node.data->header -= 1 << header_SIZESHIFT;
   TEST(EINVAL == initdecode_trienode(&node2, node.data));
   TEST(0 == node2.data);
   // nodesize is bigger than MAXSIZE
   node.data->header += 2 << header_SIZESHIFT;
   TEST(EINVAL == initdecode_trienode(&node2, node.data));
   TEST(0 == node2.data);
   node.data->header -= 1 << header_SIZESHIFT;
   TEST(0 == free_trienode(&node));

   // TEST free_trienode: ERROR
   // no subnode
   TEST(0 == init_trienode(&node, 0, 1, 1));
   init_testerrortimer(&s_trie_errtimer, 1, EPERM);
   TEST(EPERM == free_trienode(&node));
   TEST(0 == node.data);
   TEST(size_allocated == SIZEALLOCATED_MM());
   // with subnode
   for (unsigned i = 1; i <= 2; ++i) {
      TEST(0 == init_trienode(&node, 0, MAXNROFCHILD_NOUSERVALUE+1, 0));
      init_testerrortimer(&s_trie_errtimer, i, EPERM);
      TEST(EPERM == free_trienode(&node));
      TEST(0 == node.data);
      TEST(size_allocated == SIZEALLOCATED_MM());
   }

   // TEST free_trienode: wrong size (EINVAL)
   TEST(0 == new_trienodedata(&data, MAXSIZE));
   TEST(size_allocated + MAXSIZE == SIZEALLOCATED_MM());
   node.data    = data;
   data->header = encodesizeflag_header(0, header_SIZE0);
   // test memory manager checks correct size of free memory block and does nothing!
   TEST(EINVAL == free_trienode(&node));
   TEST(0 == node.data);
   // nothing freed
   TEST(size_allocated + MAXSIZE == SIZEALLOCATED_MM());
   node.data    = data;
   data->header = encodesizeflag_header(0, header_SIZEMAX);
   data = 0;
   TEST(0 == free_trienode(&node));
   TEST(0 == node.data);
   TEST(size_allocated == SIZEALLOCATED_MM());

   return 0;
ONABORT:
   free_trienode(&node);
   delete_trienodedata(&data, MAXSIZE);
   return EINVAL;
}

static int test_node_change(void)
{
   trie_node_t node = { .data = 0 };

   // == group: change-helper ==

   // TEST addheaderflag_trienode

   // TEST delheaderflag_trienode(trie_node_t * node, uint8_t flag)

   // TEST encodekeylen_trienode(trie_node_t * node, const uint8_t keylen)

   // TEST addsubnode_trienode

   return 0;
ONABORT:
   free_trienode(&node);
   return EINVAL;
}

int unittest_ds_inmem_trie()
{
   if (test_header())         goto ONABORT;
   if (test_subnode())        goto ONABORT;
   if (test_nodedata())       goto ONABORT;
   if (test_node_query())     goto ONABORT;
   if (test_node_lifetime())  goto ONABORT;
   if (test_node_change())    goto ONABORT;

   // TODO: if (test_triesubnode2())      goto ONABORT;
   // TODO: if (test_triesubnode())       goto ONABORT;
   // TODO: if (test_trienodeoffset())    goto ONABORT;
   // TODO: if (test_trienode())          goto ONABORT;
   // TODO: if (test_initfree())          goto ONABORT;
   // TODO: if (test_query())             goto ONABORT;
   // TODO: if (test_insertremove())      goto ONABORT;
   // TODO: if (test_iterator())          goto ONABORT;

   return 0;
ONABORT:
   return EINVAL;
}

#endif
