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
   (C) 2013 Jörg Seebohn

   file: C-kern/api/ds/inmem/trie.h
    Header file <Trie>.

   file: C-kern/ds/inmem/trie.c
    Implementation file <Trie impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/ds/inmem/trie.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/test/errortimer.h"
#include "C-kern/api/test/mm/mm_test.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

typedef struct trie_node_t                trie_node_t ;

typedef struct trie_nodeoffsets_t         trie_nodeoffsets_t ;

typedef struct trie_subnode_t             trie_subnode_t ;

typedef struct trie_subnode2_t            trie_subnode2_t ;


/* enums: header_e
 * Bitmask which encodes the optional data members of <trie_node_t>.
 *
 * header_PREFIX_MASK - Mask to determine the value of one of the following 4 prefix configurations.
 * header_NOPREFIX    - No prefix[] member available
 * header_PREFIX1     - prefix[0]    == prefix digit
 * header_PREFIX2     - prefix[0..1] == prefix digits
 * header_PREFIX_LEN  - prefixlen    == length byte (>= 3) and
 *                      prefix[0..prefixlen-1] == prefix digits
 * header_USERVALUE   - If set indicates that uservalue member is available.
 * header_CHILD       - Child and digit array available. digit[x] contains next digit and child[x] points to next <trie_node_t>.
 * header_SUBNODE     - Subnode pointer is avialable and digit[0] counts the number of valid pointers to trie_node_t not null.
 *                      If a pointer in <trie_subnode_t> or <trie_subnode2_t> is null there is no entry with such a key.
 *                      The number of stored pointers is (digit[0]+1), i.e. it at least one pointer must be valid.
 * header_SIZENODE_MASK - Mask to determine the value of one of the following 4 size configurations.
 * header_SIZE1NODE     - The size of the node is 2 * sizeof(trie_node_t*)
 * header_SIZE2NODE     - The size of the node is 4 * sizeof(trie_node_t*)
 * header_SIZE3NODE     - The size of the node is 8 * sizeof(trie_node_t*)
 * header_SIZE4NODE     - The size of the node is 16 * sizeof(trie_node_t*)
 * header_SIZE5NODE     - The size of the node is 32 * sizeof(trie_node_t*)
 * */
typedef enum header_e {
   header_SIZENODE_MASK = 7,
   header_SIZE1NODE  = 0,
   header_SIZE2NODE  = 1,
   header_SIZE3NODE  = 2,
   header_SIZE4NODE  = 3,
   header_SIZE5NODE  = 4,
   // header_SIZE6NODE / header_SIZE7NODE / header_SIZE8NODE not used
   header_USERVALUE  = 8,
   header_CHILD      = 16,
   header_SUBNODE    = 32,
   header_PREFIX_MASK = 64+128,
   header_NOPREFIX   = 0,
   header_PREFIX1    = 64,
   header_PREFIX2    = 128,
   header_PREFIX_LEN = 64+128,
} header_e ;


/* struct: header_t
 * Support query functions on <trie_node_t->header> type. */
typedef uint8_t   header_t ;

// group: variables

#ifdef KONFIG_UNITTEST
/* variable: s_trie_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_trie_errtimer = test_errortimer_INIT_FREEABLE ;
#endif

// group: constants

/* define: SIZE1NODE
 * Size of <trie_node_t> if <header_t> contains <header_SIZE1NODE>. */
#define SIZE1NODE       (2*sizeof(trie_node_t*))
/* define: SIZE2NODE
 * Size of <trie_node_t> if <header_t> contains <header_SIZE2NODE>. */
#define SIZE2NODE       (4*sizeof(trie_node_t*))
/* define: SIZE3NODE
 * Size of <trie_node_t> if <header_t> contains <header_SIZE3NODE>. */
#define SIZE3NODE       (8*sizeof(trie_node_t*))
/* define: SIZE4NODE
 * Size of <trie_node_t> if <header_t> contains <header_SIZE4NODE>. */
#define SIZE4NODE       (16*sizeof(trie_node_t*))
/* define: SIZE5NODE
 * Size of <trie_node_t> if <header_t> contains <header_SIZE5NODE>. */
#define SIZE5NODE       (32*sizeof(trie_node_t*))
/* define: SIZEMAXNODE
 * Same as <SIZE5NODE>. */
#define SIZEMAXNODE     SIZE5NODE

// group: query

/* function: ischild_header
 * Return true in case child and digit array are encoded in trie_node_t. */
static inline int ischild_header(const header_t header)
{
   return 0 != (header_CHILD & header) ;
}

/* function: ischildorsubnode_header
 * Return true in case <ischild_header> or <issubnode_header> returns true. */
static inline int ischildorsubnode_header(const header_t header)
{
   return 0 != ((header_CHILD|header_SUBNODE) & header) ;
}


/* function: issubnode_header
 * Return true in case child array points to <trie_subnode_t>. */
static inline int issubnode_header(const header_t header)
{
   return 0 != (header_SUBNODE & header) ;
}

/* function: isuservalue_header
 * Return true in case node contains member uservalue. */
static inline int isuservalue_header(const header_t header)
{
   return 0 != (header_USERVALUE & header) ;
}

/* function: sizenode_header
 * Return size in bytes of node. */
static inline uint16_t sizenode_header(const header_t header)
{
   return (uint16_t) ((2*sizeof(trie_node_t*)) << (header & header_SIZENODE_MASK)) ;
}

// group: change

/* function: clear_header
 * Clears flags in header. */
static inline header_t clear_header(const header_t header, header_e flags)
{
   return (header_t) (header & ~(header_t)flags) ;
}


/* struct: trie_subnode2_t
 * Points to up to 8 <trie_node_t>s.
 *
 * unchecked Invariant:
 * o At least one pointer != 0 in child[]. */
struct trie_subnode2_t {
   trie_node_t * child[8] ;
} ;

// group: lifetime

/* function: new_triesubnode2
 * Allocates subnode and sets all child pointer to 0. */
static int new_triesubnode2(/*out*/trie_subnode2_t ** subnode)
{
   int err ;
   memblock_t mblock ;
   err = ALLOC_TEST(&s_trie_errtimer, sizeof(trie_subnode2_t), &mblock) ;
   if (err) return err ;
   memset(mblock.addr, 0, sizeof(trie_subnode2_t)) ;
   *subnode = (trie_subnode2_t*) mblock.addr ;
   return 0 ;
}

/* function: delete_triesubnode2
 * Frees memory of subnode and sets it to 0.
 * Nodes referenced from child array are not deleted. */
static int delete_triesubnode2(trie_subnode2_t ** subnode)
{
   int err ;
   trie_subnode2_t * delnode = *subnode ;
   if (!delnode) return 0 ;
   *subnode = 0 ;
   memblock_t mblock = memblock_INIT(sizeof(trie_subnode2_t), (uint8_t*)delnode) ;
   err = FREE_MM(&mblock) ;
   SETONERROR_testerrortimer(&s_trie_errtimer, &err) ;
   return err ;
}

// group: query

trie_node_t ** child_triesubnode2(trie_subnode2_t * subnode2, uint8_t digit)
{
   return &subnode2->child[digit&(lengthof(subnode2->child)-1)] ;
}


/* struct: trie_subnode_t
 * Points to up to 32 <trie_subnode2_t>s.
 * Exactly one <trie_subnode_t> is referenced from <trie_node_t>.
 *
 * unchecked Invariant:
 * o At least one pointer != 0 in child[]. */
struct trie_subnode_t {
   trie_subnode2_t * child[32] ;
} ;

// group: lifetime

/* function: delete_triesubnode
 * Frees memory of subnode and of all referenced <trie_subnode2_t>.
 * Subnode is set to 0. Nodes referenced from any <trie_subnode2_t> are not deleted. */
static int delete_triesubnode(trie_subnode_t ** subnode)
{
   int err = 0 ;
   int err2 ;
   trie_subnode_t * delnode = *subnode ;
   if (!delnode) return 0 ;
   *subnode = 0 ;
   for (unsigned i = 0 ; i < lengthof(delnode->child) ; ++i) {
      if (delnode->child[i]) {
         err2 = delete_triesubnode2(&delnode->child[i]) ;
         if (err2) err = err2 ;
      }
   }
   memblock_t mblock = memblock_INIT(sizeof(trie_subnode_t), (uint8_t*)delnode) ;
   err2 = FREE_MM(&mblock) ;
   SETONERROR_testerrortimer(&s_trie_errtimer, &err2) ;
   if (err2) err = err2 ;
   return err ;
}

/* function: new_triesubnode
 * Allocates new subnode and additional <trie_subnode2_t>.
 * Every pointer in child is stored into the corresponding child entry in the referenced <trie_subnode2_t>.
 * The correct place to store pointer child[x] is calculated from digit[x].
 *
 * Unchecked Precondition:
 * o nrchild <= 256
 * o digit array sorted in ascending order
 * o Forall 0 <= x < nrchild: child[x] != 0 */
static int new_triesubnode(/*out*/trie_subnode_t ** subnode, uint16_t nrchild, const uint8_t digit[nrchild], trie_node_t * const child[nrchild])
{
   int err ;
   memblock_t mblock ;
   err = ALLOC_TEST(&s_trie_errtimer, sizeof(trie_subnode_t), &mblock) ;
   if (err) return err ;
   memset(mblock.addr, 0, sizeof(trie_subnode_t)) ;
   trie_subnode_t * newnode = (trie_subnode_t*) mblock.addr ;
   if (nrchild) {
      unsigned i = nrchild ;
      uint8_t  d = digit[i-1] ;
      do {
         trie_subnode2_t * subnode2 ;
         static_assert(32 == lengthof(newnode->child), "32 childs") ;
         static_assert(8  == lengthof(subnode2->child), "8 childs") ;
         int ci = (d >> 3) ;  /* 0 <= ci <= 31 */
         err = new_triesubnode2(&newnode->child[ci]) ;
         if (err) goto ONABORT ;
         subnode2 = newnode->child[ci] ;
         child_triesubnode2(subnode2, d)[0] = child[i-1] ;
         for (ci <<= 3 ; 0 != (--i) ; ) {
            d = digit[i-1] ;
            if (d < ci) break ;
            child_triesubnode2(subnode2, d)[0] = child[i-1] ;
         }
      } while (i) ;
   }

   *subnode = newnode ;
   return 0 ;
ONABORT:
   delete_triesubnode(&newnode) ;
   return err ;
}

// group: query

trie_subnode2_t ** child_triesubnode(trie_subnode_t * subnode, uint8_t digit)
{
   static_assert(8 == 256/lengthof(subnode->child), "shift 3 right is ok") ;
   return &subnode->child[digit>>3] ;
}


// forward declaration needed in implementation of trie_nodeoffsets_t (if you change this adapt also other !)
struct trie_node_t {
   header_t    header ;
   uint8_t     prefixlen ;  // optional (fixed size)
   // uint8_t  prefix[] ;   // optional (variable size)
   // uint8_t  digit[] ;    // optional (variable size)
   void *      ptrdata[1] ;
   // void  *  uservalue ;  // optional (fixed size)
   // void  *  subnode ;    // optional (fixed size) of type trie_subnode_t*
   // void  *  child[] ;    // optional (variable size) of type trie_node_t*
} ;


/* struct: trie_nodeoffsets_t
 * Stores offsets of every possible member of <trie_node_t>.
 * Offsets point to valid data only if the following offset is greater.
 * nodesize gives the size of the whole node.
 * lenchild is no offset but gives the length of the child array.
 * In case of (1 == issubnode_header(offsets->header)) lenchild is 1 and
 * child[0] contains a single pointer to trie_subnode_t.
 * header is a bitmask which encodes the offset and size information.
 * */
struct trie_nodeoffsets_t {
   // other
   uint16_t    nodesize ;
   uint8_t     lenchild ;
   header_t    header ;
   // offsets
   uint8_t     prefix ;
   uint8_t     digit ;
   uint8_t     uservalue ;
   uint8_t     child ;      // same as subnode (either child or subnode) not both
} ;

// group: constants

/* define: HEADERSIZE
 * Offset to first data byte in <trie_node_t>. */
#define HEADERSIZE      (sizeof(header_t))

/* define: PTRALIGN
 * Alignment of the ptrdata in <trie_node_t>. The first byte in trie_node_t
 * which encodes the availability of the optional members is followed by byte data
 * which is in turn followed by pointer data (uservalue and/or child array).
 * This value is the alignment necessary for a pointer on this architecture.
 * This value must be a power of two. */
#define PTRALIGN        (sizeof(trie_node_t*))

/* define: LENCHILDMAX
 * The maximum length of child array in a <trie_node_t>.
 * If more than LENCHILDMAX pointers have to be stored then a single pointer
 * to a <trie_subnode_t> is stored in trie_node_t. */
#define LENCHILDMAX     ((SIZEMAXNODE-sizeof(void*)/*uservalue*/-HEADERSIZE-2/*prefix*/)/(sizeof(trie_node_t*)+1))

// group: helper

static inline uint8_t divideby5(uint8_t size)
{
   return (uint8_t) (((uint16_t)size * (uint8_t)(256/5) + (uint16_t)51) >> 8) ;
}

static inline uint8_t divideby9(uint8_t size)
{
   return (uint8_t) (((uint16_t)size * (uint8_t)(2048/9) + (uint16_t)140) >> 11) ;
}

static inline uint8_t dividebychilddigitsize(uint8_t size)
{
   static_assert(sizeof(trie_node_t*) == 4 || sizeof(trie_node_t*) == 8, "pointer 32 bit or 64 bit") ;
   return sizeof(trie_node_t*) == 4 ? divideby5(size) : divideby9(size) ;
}

// group: lifetime

/* function: init_trienodeoffsets
 * Initializes offsets from prefixlen, optional uservalue, and number of child pointers nrchild.
 * */
static void init_trienodeoffsets(/*out*/trie_nodeoffsets_t * offsets, uint16_t prefixlen, bool isuservalue, const uint16_t nrchild)
{
   header_t header     = isuservalue ? header_USERVALUE : 0 ;
   unsigned nextoffset = HEADERSIZE /* skips header */ ;
   unsigned nodesize   = (isuservalue ? sizeof(void*) : 0) ;
   uint16_t lenchild   = nrchild ;

   if (nrchild > LENCHILDMAX) {
      header   |= header_SUBNODE ;
      lenchild  = 1 ;
      nodesize += 1 + sizeof(trie_subnode_t*) ;
   } else if (nrchild) {    // (nrchild == LENCHILDMAX) ==> prefix of len == 2 will fit into SIZEMAXNODE
      header |= header_CHILD ;
      nodesize += nrchild/*digit size*/ + (size_t)(nrchild * sizeof(trie_node_t*)) ;
   }

   if (prefixlen > 2) {
      header |= header_PREFIX_LEN ;
      ++ nextoffset ;   // len of prefix is encoded in byte
      static_assert( (SIZEMAXNODE-HEADERSIZE-1) <= 255, "ensures: maxprefix <= 255") ;
      size_t maxprefix = (SIZEMAXNODE-HEADERSIZE-1/*prefixlen*/) - nodesize ;
      if (prefixlen > maxprefix) prefixlen = (uint16_t)maxprefix ;
      // (prefixlen <= 255) !!
   } else {
      header |= (header_t) ((prefixlen & 3) * header_PREFIX1) ;
   }

   offsets->prefix = (uint8_t) nextoffset ;
   nextoffset += prefixlen ;
   offsets->digit  = (uint8_t) nextoffset ;
   nodesize   += nextoffset ;
   if (nodesize <= SIZE3NODE) {
      if (nodesize <= SIZE1NODE) {
         header |= header_SIZE1NODE ;
         offsets->nodesize = SIZE1NODE ;
      } else if (nodesize <= SIZE2NODE) {
         header |= header_SIZE2NODE ;
         offsets->nodesize = SIZE2NODE ;
      } else {
         header |= header_SIZE3NODE ;
         offsets->nodesize = SIZE3NODE ;
      }
   } else {
      if (nodesize <= SIZE4NODE) {
         header |= header_SIZE4NODE ;
         offsets->nodesize = SIZE4NODE ;
      } else {
         static_assert( SIZE5NODE == SIZEMAXNODE, "support maximum size") ;
         header |= header_SIZE5NODE ;
         offsets->nodesize = SIZE5NODE ;
      }
   }

   // adapt lenchild to bigger nodesize
   size_t diff = offsets->nodesize - nodesize ;
   if ((header&header_CHILD) && diff >= (sizeof(trie_subnode_t*)+1)) {
      uint8_t incr = dividebychilddigitsize((uint8_t)diff) ;
      lenchild = (uint16_t) (lenchild + incr) ;
   }

   // set out param
   offsets->lenchild  = (uint8_t) lenchild ;
   offsets->header    = header ;
   nextoffset += lenchild ;
   nextoffset += PTRALIGN-1 ;    // align byte offset
   nextoffset &= ~(PTRALIGN-1) ; // -"-
   offsets->uservalue = (uint8_t) nextoffset ;
   nextoffset += isuservalue ? sizeof(void*) : 0 ;
   offsets->child     = (uint8_t) nextoffset ;

   return ;
}

/* function: initdecode_trienodeoffsets
 * Init offsets from decoded information stored in header.
 * A single byte in trienode is needed for the prefixlen if header
 * contains value <header_PREFIX_LEN>. */
static int initdecode_trienodeoffsets(/*out*/trie_nodeoffsets_t * offsets, const trie_node_t * node)
{
   unsigned nextoffset = HEADERSIZE /* skips header encoding optional members */ ;
   header_t header     = node->header ;

   offsets->nodesize  = sizenode_header(header) ;
   if (offsets->nodesize > SIZEMAXNODE) goto ONABORT ;
   offsets->header    = header ;

   uint8_t prefixlen  = 0 ;
   uint8_t prefixmask = header&header_PREFIX_MASK ;
   if (header_NOPREFIX != prefixmask) {
      if (header_PREFIX_LEN == prefixmask) {
         prefixlen = ((const uint8_t*)node)[nextoffset] ;
         ++ nextoffset ;
      } else {
         static_assert(0 == (header_PREFIX1&(header_PREFIX1-1)), "power of two") ;
         static_assert(header_PREFIX2 == 2*header_PREFIX1, "value 1 or 2") ;
         prefixlen = (uint8_t) (prefixmask / header_PREFIX1) ;
      }
   }

   offsets->prefix = (uint8_t) nextoffset ;
   nextoffset += prefixlen ;
   offsets->digit  = (uint8_t) nextoffset ;

   uint8_t  lenchild  = (uint8_t) issubnode_header(header) ;
   unsigned uservalue = isuservalue_header(header) ? sizeof(void*) : 0 ;
   if (ischild_header(header)) {
      if (lenchild) goto ONABORT ;
      // in case offsets->nodesize < (nextoffset + uservalue) ==> lenchild is wrong ==> size check at end aborts
      lenchild = dividebychilddigitsize((uint8_t) (offsets->nodesize - nextoffset - uservalue)) ;
      // header_CHILD ==> at least one child ==> size check at end aborts
      lenchild = (uint8_t) (lenchild + (lenchild == 0)) ;
   }

   offsets->lenchild  = lenchild ;
   nextoffset += lenchild ;      // digit size
   nextoffset += PTRALIGN-1 ;    // align byte offset
   nextoffset &= ~(PTRALIGN-1) ; // -"-
   offsets->uservalue = (uint8_t) nextoffset ;
   nextoffset += uservalue ;
   offsets->child     = (uint8_t) nextoffset ;
   nextoffset += lenchild * sizeof(trie_node_t*) ;

   if (nextoffset > offsets->nodesize) goto ONABORT ;

   return 0 ;
ONABORT:
   memset(offsets, 0, sizeof(offsets)) ;
   TRACEABORT_ERRLOG(EINVARIANT) ;
   return EINVARIANT ;
}

// group: query

static inline int compare_trienodeoffsets(const trie_nodeoffsets_t * loff, const trie_nodeoffsets_t * roff)
{
#define COMPARE(member) if (loff->member != roff->member) return loff->member - roff->member ;
   COMPARE(nodesize)
   COMPARE(lenchild)
   COMPARE(header)
   COMPARE(prefix)
   COMPARE(digit)
   COMPARE(uservalue)
   COMPARE(child)
#undef COMPARE
   return 0 ;
}

/* function: isexpandable_trienodeoffsets
 * Returns true if the nodesize is lower than maximum size. */
static inline int isexpandable_trienodeoffsets(const trie_nodeoffsets_t * offsets)
{
   return offsets->nodesize < SIZEMAXNODE ;
}

/* function: lenprefix_trienodeoffsets
 * Returns the number of bytes the encoded prefix uses. */
static inline uint8_t lenprefix_trienodeoffsets(const trie_nodeoffsets_t * offsets)
{
   return (uint8_t) (offsets->digit - offsets->prefix) ;
}

/* function: lenuservalue_trienodeoffsets
 * Returns sizeof(void*) if uservalue is available else 0. */
static inline uint8_t lenuservalue_trienodeoffsets(const trie_nodeoffsets_t * offsets)
{
   return (uint8_t) (offsets->child - offsets->uservalue) ;
}

/* TODO: refactor all query functions which need node pointer to trie_node_t
 *       ==> subnode_trienodeoffsets --rename->> subnode_trienode
 * */

/* function: uservalue_trienodeoffsets
 * Returns address of uservalue member. */
static inline void ** uservalue_trienodeoffsets(const trie_nodeoffsets_t * offsets, trie_node_t * node)
{
   static_assert(sizeof(trie_node_t*) == sizeof(void*), "pointer occupy same space ==> same alignment") ;
   return (void**)((uint8_t*)node + offsets->uservalue) ;
}

/* function: subnode_trienodeoffsets
 * Returns address of subnode member.
 * */
static inline trie_subnode_t ** subnode_trienodeoffsets(const trie_nodeoffsets_t * offsets, trie_node_t * node)
{
   static_assert(sizeof(trie_subnode_t*) == sizeof(trie_node_t*), "pointer occupy same space") ;
   return (trie_subnode_t**)((uint8_t*)node + offsets->child) ;
}

/* function: sizefree_trienodeoffsets
 * Calculates unused bytes in node which corresponds to offset. */
static uint8_t sizefree_trienodeoffsets(const trie_nodeoffsets_t * offsets)
{
   static_assert(SIZEMAXNODE-HEADERSIZE <= 255, "fits in 8 bit") ;
   const unsigned alignedwaste = (unsigned) offsets->uservalue - offsets->digit - offsets->lenchild ;
   const unsigned childwaste   = (unsigned) offsets->nodesize - offsets->child - offsets->lenchild * sizeof(trie_node_t*) ;
   unsigned sizefree = alignedwaste + childwaste ;
   return (uint8_t) sizefree ;
}

/* function: sizegrowprefix_trienodeoffsets
 * Calculates size the prefix could grow without growing the node size. */
static uint8_t sizegrowprefix_trienodeoffsets(const trie_nodeoffsets_t * offsets)
{
   unsigned sizegrow  = sizefree_trienodeoffsets(offsets) ;
   unsigned prefixlen = lenprefix_trienodeoffsets(offsets) ;
   sizegrow -= (unsigned) ((prefixlen <= 2) & ((prefixlen+sizegrow) > 2)) ;
   return (uint8_t) sizegrow ;
}

// group: change

/* function: convert2subnode_trienodeoffsets
 * Switch header flags from <header_CHILD> to <header_SUBNODE>.
 * The function returns the number of bytes in use after conversion.
 * Values in offsets are adapted and members in node are moved if necessary.
 * The header in node is changed and the subnode pointer is set.
 *
 * Unchecked Precondition:
 * o true == ischild_header(offsets->header) */
static void convert2subnode_trienodeoffsets(trie_nodeoffsets_t * offsets)
{
   offsets->header   = clear_header(offsets->header, header_CHILD) | header_SUBNODE ;
   offsets->lenchild = 0 ;

   unsigned nextoffset = offsets->digit + 1u ;
   nextoffset += PTRALIGN-1 ;    // align byte offset
   nextoffset &= ~(PTRALIGN-1) ; // -"-
   unsigned uservaluesize = lenuservalue_trienodeoffsets(offsets) ;
   offsets->uservalue = (uint8_t) nextoffset ;
   offsets->child     = (uint8_t) (nextoffset + uservaluesize) ;
}

/* function: shrinkprefix_trienodeoffsets
 * Adapts offsets to newprefixlen.
 *
 * Unchecked Precondition:
 * o newprefixlen < lenprefix_trienodeoffsets(offsets) */
static void shrinkprefix_trienodeoffsets(trie_nodeoffsets_t * offsets, uint8_t newprefixlen)
{
   if (newprefixlen <= 2) {
      offsets->header = (header_t) ((offsets->header&~header_PREFIX_MASK) | ((newprefixlen & 3) * header_PREFIX1)) ;
   }

   unsigned uservaluesize = lenuservalue_trienodeoffsets(offsets) ;

   if (ischild_header(offsets->header)) {
      unsigned freesize = (unsigned)offsets->nodesize - HEADERSIZE  // free
                        - (newprefixlen > 2) - newprefixlen         // prefix len
                        - uservaluesize ;
      offsets->lenchild = dividebychilddigitsize((uint8_t)freesize) ;
   }

   offsets->prefix = (uint8_t) (HEADERSIZE + (newprefixlen > 2)) ;
   offsets->digit  = (uint8_t) (offsets->prefix + newprefixlen) ;
   unsigned alignedoffset = (unsigned) offsets->digit + offsets->lenchild ;
   alignedoffset += PTRALIGN-1 ;    // align byte offset
   alignedoffset &= ~(PTRALIGN-1) ; // -"-
   offsets->uservalue = (uint8_t) alignedoffset ;
   offsets->child     = (uint8_t) (alignedoffset + uservaluesize) ;
}

/* function: changesize_trienodeoffsets
 * Sets header and nodesize of offset to smaller or bigger size.
 * Also offsets members lenchild, uservalue and child are recalculated in case of
 * ischild_header(offsets->header) returns true.
 *
 * Unchecked Precondition:
 * o 0 == (headersize & ~header_SIZENODE_MASK)
 * o sizenode_header(headersize) <= SIZEMAXNODE
 * o //"either growing or offsets->child fits in smaller size"
 *   offsets.nodesize <= sizenode_header(headersize)
 *   || offsets->child < sizenode_header(headersize)
 *   || (offsets->child == sizenode_header(headersize) && !ischildorsubnode_header(offsets->header))
 * */
static void changesize_trienodeoffsets(trie_nodeoffsets_t * offsets, header_t headersize)
{
   offsets->header   = (header_t) ((offsets->header & ~header_SIZENODE_MASK) | headersize) ;
   offsets->nodesize = sizenode_header(headersize) ;

   if (ischild_header(offsets->header)) {
      unsigned uservaluesize = lenuservalue_trienodeoffsets(offsets) ;
      unsigned freesize = (unsigned) offsets->nodesize - offsets->digit - uservaluesize ;
      offsets->lenchild = dividebychilddigitsize((uint8_t)freesize) ;

      unsigned alignedoffset = (unsigned) offsets->digit + offsets->lenchild ;
      alignedoffset += PTRALIGN-1 ;    // align byte offset
      alignedoffset &= ~(PTRALIGN-1) ; // -"-
      offsets->uservalue = (uint8_t) alignedoffset ;
      offsets->child     = (uint8_t) (alignedoffset + uservaluesize) ;
   }
}

/* function: growprefix_trienodesoffsets
 * Adds increment to length of prefix.
 *
 * Unchecked Precondition:
 * o sizegrowprefix_trienodeoffsets(offsets) >= increment || (usefreechild && sizegrowprefix_trienodeoffsets(offsets)+sizeof(trie_node_t*) >= increment)
 * o lenprefix_trienodeoffsets(offsets) + increment <= 255 // is valid cause SIZEMAXNODE-HEADERSIZE <= 255 !
 * o ! usefreechild || offsets.lenchild >= 2
 * */
static void growprefix_trienodesoffsets(trie_nodeoffsets_t * offsets, uint8_t increment, bool usefreechild)
{
   const uint8_t oldlen = lenprefix_trienodeoffsets(offsets) ;
   const uint8_t newlen = (uint8_t) (oldlen + increment) ;

   offsets->lenchild = (uint8_t) (offsets->lenchild - usefreechild) ;
   offsets->header   = (header_t) ((offsets->header & ~header_PREFIX_MASK) | (((newlen & 3) | (newlen > 2 ? 3 : 0)) * header_PREFIX1)) ;

   unsigned nextoffset = HEADERSIZE + (unsigned) (newlen > 2) ;
   offsets->prefix = (uint8_t) nextoffset ;
   nextoffset += newlen ;
   offsets->digit  = (uint8_t) nextoffset ;
   nextoffset += offsets->lenchild ;
   nextoffset += PTRALIGN-1 ;    // align byte offset
   nextoffset &= ~(PTRALIGN-1) ; // -"-
   unsigned offset = nextoffset - offsets->uservalue ;
   offsets->uservalue = (uint8_t) nextoffset ;
   offsets->child     = (uint8_t) (offsets->child + offset) ;
}

/* function: adduservalue_trienodeoffsets
 * Add header_USERVALUE to offsets->header and adapts offsets.
 *
 * Unchecked Preconditions:
 * (node points to trie_node_t* which corresponds to offset)
 * o ! isuservalue_header(offsets->header)
 * o sizeof(void*) <= sizefree_trienodeoffsets(offsets)
 *   || offsets.lenchild >= 2 && "last child in node is 0"
 * */
static void adduservalue_trienodeoffsets(trie_nodeoffsets_t * offsets)
{
   static_assert(sizeof(trie_node_t*) == sizeof(void*), "pointer occupy same space ==> can be stored in child[] entry") ;

   offsets->header |= header_USERVALUE ;

   if (offsets->child + offsets->lenchild * sizeof(trie_node_t*) < offsets->nodesize) {
      offsets->child = (uint8_t) (offsets->child + sizeof(void*)) ;

   } else {
      -- offsets->lenchild ;
      unsigned nextoffset = (unsigned) offsets->digit + offsets->lenchild ;
      nextoffset += PTRALIGN-1 ;    // align byte offset
      nextoffset &= ~(PTRALIGN-1) ; // -"-
      offsets->uservalue = (uint8_t) nextoffset ;
      offsets->child     = (uint8_t) (nextoffset + sizeof(void*)) ;
   }
}


#if 0 // declared at top of module
/* struct: trie_node_t
 * Describes a node in the trie.
 * It is a flexible data structure which can hold an optional string prefix,
 * an optional user pointer, and an optional array of pointers to child nodes
 * or instead a pointer to a <trie_subnode_t>. The subnode and child members
 * exclude each other.
 *
 * A <trie_node_t> can use 2*sizeof(trie_node_t*) up to 32*sizeof(trie_node_t*) bytes.
 *
 * */
struct trie_node_t {
   // group: variables
   /* variable: header
    * Flags which describes content of trie_node_t. See <header_t>. */
   header_t    header ;
   /* variable: prefixlen
    * Start of data and (optional) contains size of prefix key. */
   uint8_t     prefixlen ;  // optional (fixed size)
   // uint8_t  prefix[] ;   // optional (variable size)
   // uint8_t  digit[] ;    // optional (variable size)
   /* variable: ptrdata
    * Contains optional data which is pointer aligned. */
   void *      ptrdata[1] ;
   // void  *  uservalue ;  // optional (fixed size)
   // void  *  subnode ;    // optional (fixed size) of type trie_subnode_t*
   // void  *  child[] ;    // optional (variable size) of type trie_node_t*
} ;
#endif

// group: query-helper

/* function: child_trienode
 * Returns ptr to child array. */
static inline trie_node_t ** child_trienode(trie_node_t * node, const trie_nodeoffsets_t * offsets)
{
   return (trie_node_t**)((uint8_t*)node + offsets->child) ;
}

/* function: digit_trienode
 * Returns ptr to digit array. */
static inline uint8_t * digit_trienode(trie_node_t * node, const trie_nodeoffsets_t * offsets)
{
   return (uint8_t*)node + offsets->digit ;
}

/* function: isfreechild_trienode
 * Uses node to check if last child entry is empty. */
static inline bool isfreechild_trienode(trie_node_t * node, const trie_nodeoffsets_t * offsets)
{
   return offsets->lenchild > 1 && 0 == child_trienode(node, offsets)[offsets->lenchild-1] ;
}

/* function: prefix_trienode
 * Returns ptr to digit array. */
static inline uint8_t * prefix_trienode(trie_node_t * node, const trie_nodeoffsets_t * offsets)
{
   return (uint8_t*)node + offsets->prefix ;
}

// group: helper

/* funcion: newnode_trienode
 * Allocates new trie_node_t of size nodesize and returns pointer in node. */
static int newnode_trienode(/*out*/trie_node_t ** node, uint16_t nodesize)
{
   int err ;
   memblock_t mblock ;
#ifdef KONFIG_UNITTEST
   err = process_testerrortimer(&s_trie_errtimer) ;
   if (err) return err ;
#endif
   err = ALLOC_MM(nodesize, &mblock) ;
   if (err) return err ;
   *node = (trie_node_t*) mblock.addr ;
   return 0 ;
}

/* funcion: deletenode_trienode
 * Free memory node points to and sets node to 0. */
static int deletenode_trienode(trie_node_t ** node)
{
   int err ;
   trie_node_t * delnode = *node ;
   if (!delnode) return 0 ;
   memblock_t mblock = memblock_INIT(sizenode_header(delnode->header), (uint8_t*)delnode) ;
   err = FREE_MM(&mblock) ;
   *node = 0 ;
   SETONERROR_testerrortimer(&s_trie_errtimer, &err) ;
   return err ;
}

/* function: shrinksize_trienode
 * Resize node to a smaller size.
 * The header of node and offsets are adapted and also lenchild, digit, uservalue and child array of node.
 * A smallest value for nodesize is chosen for which offsets->child <= nodesize and all childs zero at offset nodsize.
 * */
static int shrinksize_trienode(trie_node_t ** node, trie_nodeoffsets_t * offsets)
{
   int err ;
   trie_node_t * shrinknode = *node ;
   header_t      headersize = (offsets->header & header_SIZENODE_MASK) ;
   unsigned      nodesize   = offsets->nodesize / 2 ;

   // check PRECONDITION for changesize_trienodeoffsets
   while (  (offsets->child < nodesize || (offsets->child == nodesize && !ischildorsubnode_header(offsets->header)))
            && SIZE1NODE <= nodesize
            && (!ischild_header(offsets->header) || 0 == *(trie_node_t**)((uint8_t*)shrinknode + nodesize))) {
      -- headersize ;
      nodesize /= 2 ;
   }
   nodesize *= 2 ;

   if (nodesize == offsets->nodesize) return 0 ;

   ONERROR_testerrortimer(&s_trie_errtimer, ONABORT) ;
   memblock_t mblock = memblock_INIT(offsets->nodesize, (uint8_t*)shrinknode) ;
   err = RESIZE_MM(nodesize, &mblock) ;
   if (err) goto ONABORT ;

   shrinknode = (trie_node_t*) mblock.addr ;

   // save old offsets
   unsigned olduservalue = offsets->uservalue ;

   // adapt offsets ; precondition is OK
   changesize_trienodeoffsets(offsets, headersize) ;

   // move content of node
   shrinknode->header = offsets->header ;
   if (ischild_header(offsets->header)) {
      // uservalue + childs
      unsigned size      = offsets->nodesize - olduservalue ;
      uint8_t* uservalue = (uint8_t*) uservalue_trienodeoffsets(offsets, shrinknode) ;
      memmove(uservalue, olduservalue + (uint8_t*)shrinknode, size) ;
      memset(uservalue+size, 0, olduservalue - offsets->uservalue) ;
   }

   *node = shrinknode ;

   return 0 ;
ONABORT:
   return err ;
}

/* function: expand_trienode
 * Doubles the size of the node.
 * The header of node and offsets is adapted. offsets->nodesize is also adapted.
 *
 * TODO: add functionality to increase lenchild !
 *
 * Unchecked Precondition:
 * o 1 == isexpandable_trienodeoffsets(offsets)
 * */
static int expand_trienode(trie_node_t ** node, trie_nodeoffsets_t * offsets)
{
   int err ;
   trie_node_t * expandnode  = *node ;
   unsigned      oldnodesize = offsets->nodesize ;

   ONERROR_testerrortimer(&s_trie_errtimer, ONABORT) ;
   memblock_t mblock = memblock_INIT(oldnodesize, (uint8_t*) *node) ;
   err = RESIZE_MM(2*oldnodesize, &mblock) ;
   if (err) goto ONABORT ;

   expandnode = (trie_node_t*) mblock.addr ;

   // save old offsets
   unsigned olduservalue = offsets->uservalue ;

   header_t headersize = (header_t) ((offsets->header & header_SIZENODE_MASK) + 1) ;
   changesize_trienodeoffsets(offsets, headersize) ;

   // move content of node
   expandnode->header = offsets->header ;
   if (ischild_header(offsets->header)) {
      // uservalue + childs
      unsigned size      = oldnodesize - olduservalue ;
      uint8_t* uservalue = (uint8_t*) uservalue_trienodeoffsets(offsets, expandnode) ;
      memmove(uservalue, olduservalue + (uint8_t*)expandnode, size) ;
      memset(uservalue+size, 0, (size_t)offsets->nodesize - offsets->uservalue - size) ;
   }

   *node = expandnode ;

   return 0 ;
ONABORT:
   return err ;
}

/* function: shrinkprefixkeeptail_trienode
 * Keeps last newprefixlen bytes of the key prefix.
 *
 * Unchecked Precondition:
 * o newprefixlen < prefixlen */
static void shrinkprefixkeeptail_trienode(trie_node_t * node, trie_nodeoffsets_t * offsets, uint8_t newprefixlen)
{
   uint8_t   prefixlen    = lenprefix_trienodeoffsets(offsets) ;
   uint8_t   lenchild     = offsets->lenchild ;
   uint8_t * oldprefix    = prefix_trienode(node, offsets) + prefixlen - newprefixlen ;
   void **   olduservalue = uservalue_trienodeoffsets(offsets, node) ;
   unsigned  size         = lenuservalue_trienodeoffsets(offsets) + lenchild * sizeof(trie_node_t*)/*same size as sizeof(trie_subnode_t*)*/ ;

   shrinkprefix_trienodeoffsets(offsets, newprefixlen) ;
   // change node
   node->header    = offsets->header ;
   // newprefixlen could overwrite first byte of old prefix
   node->prefixlen = newprefixlen ;
   // copy prefix + digit array
   memmove(prefix_trienode(node,offsets), oldprefix, (unsigned)newprefixlen + lenchild) ;
   // copy uservalue + child array/subnode
   uint8_t * uservalue = (uint8_t*) uservalue_trienodeoffsets(offsets, node) ;
   memmove(uservalue, olduservalue, size) ;
   memset(uservalue+size, 0, (unsigned) offsets->nodesize - offsets->uservalue - size) ;
}

/* function: shrinkprefixkeephead_trienode
 * Keeps first newprefixlen bytes of the key prefix.
 *
 * Unchecked Precondition:
 * o newprefixlen < prefixlen */
static void shrinkprefixkeephead_trienode(trie_node_t * node, trie_nodeoffsets_t * offsets, uint8_t newprefixlen)
{
   uint8_t   prefixlen    = lenprefix_trienodeoffsets(offsets) ;
   uint8_t   lenchild     = offsets->lenchild ;
   uint8_t * oldprefix    = prefix_trienode(node, offsets) ;
   void **   olduservalue = uservalue_trienodeoffsets(offsets, node) ;
   unsigned  size         = lenuservalue_trienodeoffsets(offsets) + lenchild * sizeof(trie_node_t*)/*same size as sizeof(trie_subnode_t*)*/ ;

   shrinkprefix_trienodeoffsets(offsets, newprefixlen) ;
   // change node
   node->header = offsets->header ;
   // prefixlen and prefix[]
   if (newprefixlen > 2)   node->prefixlen = newprefixlen ;
   else if (prefixlen > 2) memmove(oldprefix-1, oldprefix, (size_t)newprefixlen) /*newprefixlen <= 2*/ ;
   // digit[]
   memmove(digit_trienode(node, offsets), oldprefix+prefixlen, (size_t)lenchild) ;
   // copy uservalue + child array/subnode
   uint8_t * uservalue = (uint8_t*) uservalue_trienodeoffsets(offsets, node) ;
   memmove(uservalue, olduservalue, size) ;
   memset(uservalue+size, 0, (unsigned) offsets->nodesize - offsets->uservalue - size) ;
}

/* function: tryextendprefix_trienode
 * Extends prefix with new head prefix1[len-1] + prefix2.
 * The new prefix is prefix1[len-1] + prefix2 + oldprefix.
 * Returns ENOMEM if node has not enough free space.
 *
 * Unchecked Preconditions:
 * o len > 0 && len <= sizeof(trie_node_t*) */
static int tryextendprefix_trienode(trie_node_t * node, trie_nodeoffsets_t * offsets, uint8_t len, uint8_t prefix1[len-1], uint8_t prefix2/*single digit*/)
{
   uint8_t growsize     = sizegrowprefix_trienodeoffsets(offsets) ;
   bool    usefreechild = false ;

   if (len > growsize && !(usefreechild = isfreechild_trienode(node, offsets))) return ENOMEM ;

   // INVARIANT
   //    len <= growsize                             ==> extra byte already considered in calc. growsize
   // || usefreechild && len <= sizeof(trie_node_t*) ==> digit[] byte can be used for additional prefixlen byte

   uint8_t* oldprefix    = prefix_trienode(node, offsets) ;
   void  ** olduservalue = uservalue_trienodeoffsets(offsets, node) ;
   growprefix_trienodesoffsets(offsets, len, usefreechild) ;
   // olduservalue <= uservalue_trienodeoffsets(&offsets, node) ; uservalue + child[]
   memmove(uservalue_trienodeoffsets(offsets, node), olduservalue, (unsigned)offsets->nodesize - offsets->uservalue) ;
   uint8_t prefixlen = lenprefix_trienodeoffsets(offsets) ;
   uint8_t * prefix  = prefix_trienode(node, offsets) ;
   // prefix[] + digit[]
   memmove(prefix + len, oldprefix, (unsigned)prefixlen - len + offsets->lenchild) ;
   node->prefixlen = prefixlen ; // if not used it is overwritten
   memcpy(prefix, prefix1, len-1u) ;
   prefix[len-1] = prefix2 ;
   node->header = offsets->header ;

   return 0 ;
}

/* function: adduservalue_trienode
 * Add uservalue to node and adapt offset.
 *
 * Unchecked Preconditions:
 * o ! isuservalue_header(node->header)
 * o sizeof(void*) <= sizefree_trienodeoffsets(offsets) || isfreechild_trienode(node, offsets)
 * o
 * */
static void adduservalue_trienode(trie_node_t * node, trie_nodeoffsets_t * offsets, void * uservalue)
{
   trie_node_t ** oldchild = child_trienode(node, offsets) ;
   adduservalue_trienodeoffsets(offsets) ;
   // adapt header
   node->header = offsets->header ;
   // only child[] or subnode moved and lenchild decremented by one (if needed)
   void ** addruservalue = uservalue_trienodeoffsets(offsets, node) ;
   memmove(addruservalue+1, oldchild, offsets->lenchild * sizeof(trie_node_t*)) ;
   *addruservalue = uservalue ;
}

// group: lifetime

/* function: delete_trienode
 * Frees memory of node and all of its child nodes.
 * The tree is traversed in depth first order.
 * During traversal a special delete header is written to the node. */
static int delete_trienode(trie_node_t ** node)
{
   int err = 0 ;
   int err2 ;

   typedef struct delheader_t delheader_t ;
   struct delheader_t {
      uint8_t  header ;
      uint8_t  nodesize ;     // nodesize * PTRALIGN    == nodesize in bytes
      uint8_t  childoffset ;  // childoffset * PTRALIGN == offset in bytes into child array / 1 .. 255 in case of subnode
      void *   parent ;       // points to parent or subnode (in case of subnode parent pointer is stored in subnode->child[0]->child[0])
   } ;

   static_assert( offsetof(delheader_t, header) == offsetof(trie_node_t, header)
                  && sizeof(((delheader_t*)0)->header) == sizeof(((trie_node_t*)0)->header), "header is compatible") ;
   static_assert(offsetof(delheader_t, parent) <= PTRALIGN, "parent overwrites first child if uservalue is not present") ;
   static_assert(sizeof(delheader_t) <= SIZE1NODE, "minmum nodesize can hold delheader_t") ;
   static_assert(SIZEMAXNODE / sizeof(trie_node_t*) < 255, "childoffset and nodesize fit into 8 bit") ;

   delheader_t * delheader = 0 ;
   trie_node_t * delnode   = *node ;
   trie_nodeoffsets_t offsets ;

   while (delnode) {
      // 1: decode delnode !!
      // write delheader_t to node and set delheader to it
      // if first child set delnode to first child => repeat 1

      // 2: if delheader is 0 break this loop
      // move next child of delheader into delnode => repeat 1
      // If no next child delete node (if it contains pointer to subnodes delete all subnodes first)
      // set delheader to delheader->parent (before deletion !) => repeat 2

      // step 1:
      do {
         void *   firstchild  = 0 ;
         uint8_t  childoffset = 0 ;
         err2 = initdecode_trienodeoffsets(&offsets, delnode) ;
         if (err2) {       // ! ignore corrupted delnode !
            err = err2 ;
         } else {
            void *  parent  = delheader ;
            if (ischild_header(offsets.header)) {
               // delheader_t->parent could overlap with child[0]
               firstchild  = child_trienode(delnode, &offsets)[0] ;
               childoffset = (uint8_t) (1 + (offsets.child / sizeof(trie_node_t*))) ;
            } else if (issubnode_header(offsets.header)) {
               // delheader_t->parent could overlap with subnode
               trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, delnode)[0] ;
               if (subnode) {
                  for (unsigned i = 0 ; i < lengthof(subnode->child) ; ++i) {
                     if (subnode->child[i]) {
                        trie_subnode2_t * subnode2 = subnode->child[i] ;
                        for (unsigned i2 = 0 ; i2 < lengthof(subnode2->child) ; ++i2) {
                           if (subnode2->child[i2]) {
                              firstchild = subnode2->child[i2] ;
                              subnode2->child[i2] = 0 ;
                              subnode2->child[0]  = (trie_node_t*) delheader ;   // real parent
                              subnode->child[i] = subnode->child[0] ;
                              subnode->child[0] = subnode2 ;
                              parent = subnode ;                  // parent points to subnode
                              goto FOUND_CHILD_IN_SUBNODE ;
                           }
                        }
                     }
                  }
                  // delete subnode cause of no childs ==> subnode == 0
                  err2 = delete_triesubnode(&subnode) ;
                  if (err2) err = err2 ;
               }
               // change header cause subnode == 0
               delnode->header = clear_header(delnode->header, header_SUBNODE) ;

               FOUND_CHILD_IN_SUBNODE: ;
               //    (subnode != 0 && firschild != 0)
               //    || (subnode == 0 && firstchild == 0 && !issubnode_header(delnode->header))
            }
            ((delheader_t*)delnode)->nodesize    = (uint8_t) (offsets.nodesize / sizeof(trie_node_t*)) ;
            ((delheader_t*)delnode)->childoffset = childoffset ;
            ((delheader_t*)delnode)->parent      = parent ;
            delheader = (delheader_t*) delnode ;
         }
         delnode = firstchild ;
      } while (delnode) ;

      // step 2:
      delheader_t * delparent ;
      for ( ; delheader ; delheader = delparent) {
         if (ischildorsubnode_header(delheader->header)) {
            // visit childs first
            if (issubnode_header(delheader->header)) {
               trie_subnode_t * subnode = delheader->parent ;
               while (delheader->childoffset != 255) {
                  trie_subnode2_t * subnode2 ;
                  ++ delheader->childoffset ; // first value is 1
                  subnode2 = child_triesubnode(subnode, delheader->childoffset)[0] ;
                  if (subnode2) {
                     delnode = child_triesubnode2(subnode2, delheader->childoffset)[0] ;
                     if (delnode) break ;
                  } else {
                     delheader->childoffset |= (lengthof(subnode2->child)-1) ;
                  }
               }

            } else if (delheader->nodesize > delheader->childoffset) {
               delnode = ((trie_node_t**)((uint8_t*)delheader + delheader->childoffset * sizeof(trie_node_t*)))[0] ;
               ++ delheader->childoffset ;
            }
            if (delnode) break ; // another child (delnode != 0) ? => handle it first
         }
         // ////
         // delete node delheader !
         delparent = delheader->parent ;
         if (issubnode_header(delheader->header)) {
            trie_subnode_t * subnode = delheader->parent ;
            delparent = (delheader_t*) subnode->child[0]->child[0] ;
            err2 = delete_triesubnode(&subnode) ;
            if (err2) err = err2 ;
         }
         err2 = deletenode_trienode((trie_node_t**)&delheader) ;
         if (err2) err = err2 ;
      }
   }

   // set inout param
   *node = 0 ;

   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

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
static int new_trienode(/*out*/trie_node_t ** node, /*out*/trie_nodeoffsets_t * offsets, uint16_t prefixlen, const uint8_t prefix[prefixlen], void * const * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], trie_node_t * const child[nrchild])
{
   int err ;
   trie_node_t * newnode = 0 ;
   uint8_t       encodedlen ;

   init_trienodeoffsets(offsets, prefixlen, uservalue != 0, nrchild) ;
   encodedlen = lenprefix_trienodeoffsets(offsets) ;
   prefixlen  = (uint16_t) (prefixlen - encodedlen) ;

   err = newnode_trienode(&newnode, offsets->nodesize) ;
   if (err) goto ONABORT ;

   newnode->header = offsets->header ;
   if (encodedlen) {
      newnode->prefixlen = encodedlen ;
      memcpy((uint8_t*)newnode + offsets->prefix, prefix+prefixlen, encodedlen) ;
   }

   if (nrchild) {
      if (ischild_header(offsets->header)) {
         memcpy((uint8_t*)newnode+offsets->digit, digit, nrchild) ;
         size_t sizechild = nrchild*sizeof(trie_node_t*) ;
         memcpy((uint8_t*)newnode+offsets->child, child, sizechild) ;
         memset((uint8_t*)newnode+offsets->child+sizechild, 0, (size_t)offsets->nodesize - offsets->child - sizechild) ;

      } else {
         trie_subnode_t * subnode = 0 ;
         err = new_triesubnode(&subnode, nrchild, digit, child) ;
         // works only if 0 < nrchild && nrchild <= 256
         digit_trienode(newnode, offsets)[0]   = (uint8_t) (nrchild-1) ;
         subnode_trienodeoffsets(offsets, newnode)[0] = subnode ;
         if (err) goto ONABORT ;
      }
   }

   if (uservalue) {
      *(void**)((uint8_t*)newnode + offsets->uservalue) = *uservalue ;
   }

   while (prefixlen)  {

      // build chain of nodes !

      init_trienodeoffsets(offsets, (uint16_t)(prefixlen-1), false, 1) ;
      encodedlen = lenprefix_trienodeoffsets(offsets) ;

      do {
         // do not calculate offsets for same prefixlen
         -- prefixlen ;

         trie_node_t * newnode2 ;
         err = newnode_trienode(&newnode2, offsets->nodesize) ;
         if (err) goto ONABORT ;

         newnode2->header    = offsets->header ;
         newnode2->prefixlen = encodedlen ;
         memset(child_trienode(newnode2, offsets), 0, (size_t)(offsets->nodesize - offsets->child)) ;
         digit_trienode(newnode2, offsets)[0] = prefix[prefixlen] ;
         child_trienode(newnode2, offsets)[0] = newnode ;
         prefixlen  = (uint16_t) (prefixlen - encodedlen) ;
         memcpy(prefix_trienode(newnode2, offsets), prefix+prefixlen, encodedlen) ;

         newnode = newnode2 ;
      } while (encodedlen < prefixlen) ;
   }

   // out
   *node = newnode ;

   return 0 ;
ONABORT:
   delete_trienode(&newnode) ;
   TRACEABORT_ERRLOG(err) ;
   return err ;
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
static int newsplit_trienode(/*out*/trie_node_t ** node, trie_node_t * splitnode, trie_nodeoffsets_t * splitnodeoffsets, uint8_t splitlen, void * uservalue, uint8_t digit, trie_node_t * child)
{
   int err ;
   uint8_t prefixlen = lenprefix_trienodeoffsets(splitnodeoffsets) ;
   uint8_t shrinklen = (uint8_t) (prefixlen-splitlen) ;

   if (  shrinklen <= sizeof(trie_node_t*)
         && !isuservalue_header(splitnodeoffsets->header)
         // only single child ?
         && ischild_header(splitnodeoffsets->header)
         && (1 == splitnodeoffsets->lenchild || 0 == child_trienode(splitnode, splitnodeoffsets)[1])
         // is enough space for uservalue or child (precondition for adduservalue_trienode)
         && (  (child == 0 && sizeof(void*) <= shrinklen+sizefree_trienodeoffsets(splitnodeoffsets))
               || splitnodeoffsets->lenchild > 1 /*same as (1 == isfreechild_trienode(...))*/)
         ) {
      trie_node_t       * mergenode = child_trienode(splitnode, splitnodeoffsets)[0] ;
      trie_nodeoffsets_t  mergeoffsets ;
      initdecode_trienodeoffsets(&mergeoffsets, mergenode) ;

      if (0 == tryextendprefix_trienode(  mergenode, &mergeoffsets, shrinklen,
                                          prefix_trienode(splitnode, splitnodeoffsets)+splitlen+1,
                                          digit_trienode(splitnode, splitnodeoffsets)[0])) {

         digit_trienode(splitnode, splitnodeoffsets)[0] = prefix_trienode(splitnode, splitnodeoffsets)[splitlen] ;
         shrinkprefixkeephead_trienode(splitnode, splitnodeoffsets, splitlen) ;

         if (child) {   // addchild
            static_assert(sizeof(void*) == sizeof(trie_node_t*), "size calculation is valid") ;
            uint8_t     * splitdigit = digit_trienode(splitnode, splitnodeoffsets) ;
            trie_node_t** splitchild = child_trienode(splitnode, splitnodeoffsets) ;
            if (digit > digit_trienode(splitnode, splitnodeoffsets)[0]) {
               splitdigit[1] = digit ;
               splitchild[1] = child ;
            } else {
               splitdigit[1] = splitdigit[0] ;
               splitdigit[0] = digit ;
               splitchild[1] = splitchild[0] ;
               splitchild[0] = child ;
            }
         } else {
            adduservalue_trienode(splitnode, splitnodeoffsets, uservalue) ;
         }

         *node = splitnode ;
         return 0 ;
      }
   }

   // normal split (cause merge with single child not possible)
   trie_nodeoffsets_t offsets ;
   uint8_t          * prefix = prefix_trienode(splitnode, splitnodeoffsets) ;
   trie_node_t      * child2[2] ;
   uint8_t            digit2[2] ;
   uint8_t            childindex ;
   if (!child || prefix[splitlen] < digit) {
      child2[0] = splitnode ;
      child2[1] = child ;
      digit2[0] = prefix[splitlen] ;
      digit2[1] = digit ;
      childindex = 0 ;
   } else {
      child2[0] = child ;
      child2[1] = splitnode ;
      digit2[0] = digit ;
      digit2[1] = prefix[splitlen] ;
      childindex = 1 ;
   }

   err = new_trienode(node, &offsets, splitlen, prefix, child ? 0 : &uservalue, (uint16_t) (1 + (child != 0)), digit2, child2) ;
   if (err) goto ONABORT ;
   // precondition OK: prefixlen-1u-splitlen < lenprefix_trienodeoffsets(splitnodeoffsets)
   shrinkprefixkeeptail_trienode(splitnode, splitnodeoffsets, (uint8_t)(shrinklen-1)) ;
   // ignore error
   (void) shrinksize_trienode(&child_trienode(*node, &offsets)[childindex], splitnodeoffsets) ;

   return 0 ;
ONABORT:
   TRACEABORT_ERRLOG(err) ;
   return err ;
}

// group: change

/* function: convertchild2sub_trienode
 * TODO: describe
 * TODO: remove shrinknode
 * */
static int convertchild2sub_trienode(trie_node_t ** node, trie_nodeoffsets_t * offsets)
{
   int err ;
   trie_node_t *  oldnode = *node ;
   trie_node_t ** child   = child_trienode(oldnode, offsets) ;

   if (!ischild_header(offsets->header) || ! child[0]) return EINVAL ;

   unsigned nrchild ;
   for (nrchild = 1 ; nrchild < offsets->lenchild ; ++nrchild) {
      if (!child[nrchild]) break ;
   }

   trie_subnode_t * subnode ;
   err = new_triesubnode(&subnode, (uint16_t) nrchild, digit_trienode(oldnode, offsets), child) ;
   if (err) return err ;

   void * olduservalue = uservalue_trienodeoffsets(offsets, oldnode)[0] ;
   convert2subnode_trienodeoffsets(offsets) ;
   // move content of oldnode
   oldnode->header = offsets->header ;
   digit_trienode(oldnode, offsets)[0] = (uint8_t) (nrchild-1) ;
   // even if no uservalue then olduservalue contains first child pointer but is overwritten by subnode
   uservalue_trienodeoffsets(offsets, oldnode)[0] = olduservalue ;
   subnode_trienodeoffsets(offsets, oldnode)[0]   = subnode ;

   // ignore shrinksize_trienode error (cause node already changed and fail of shrink is no problem)
   (void) shrinksize_trienode(node, offsets) ;
   return 0 ;
}

// TODO: document + test
#if 0 // TODO: remove line
static int convertsub2child_trienode(trie_node_t ** node, /*out*/trie_node_t ** head, trie_nodeoffsets_t * offsets)
{
   int err ;

   if (!issubnode_header(offsets->header)) return EINVAL ;

   unsigned          nrchild = 0 ;
   trie_subnode_t *  subnode = subnode_trienodeoffsets(offsets, *node)[0] ;

   for (unsigned ci = 0 ; ci < lengthof(subnode->child) ; ++ci) {
      trie_subnode2_t * subnode2 = subnode->child[ci] ;
      if (subnode2) {
         for (unsigned si = 0 ; si < lengthof(subnode2->child) ; ++si) {
            nrchild += (subnode2->child[si] != 0) ;
         }
         if (nrchild > LENCHILDMAX) return E2BIG ;
      }
   }

   // 1. calculate new size of node
   // 2. split node if prefix does not fit + expand size of node
   // 3. copy childs into expanded node
   // 4. return out param

   trie_node_t * splitnode   = 0 ;
   unsigned unused_alignment = (unsigned) offsets->uservalue - offsets->digit ;
   unsigned newsize          = (offsets->child - unused_alignment + nrchild * (sizeof(trie_node_t*)+1u)) ;
   unsigned prefixlen        = 0 ;
   if (newsize > SIZEMAXNODE) {
      // remove prefix
      prefixlen = lenprefix_trienodeoffsets(offsets) ;
      // split
      // TODO:
      assert(0) ;
   }

   if (newsize > offsets->nodesize) {
      // expand node
      // TODO:
      assert(0) ;
   }

   // out param
   *head = splitnode ;

   return ENOSYS ;
ONABORT:
   if (splitnode) {
      // TODO: merge node
      assert(0) ;
   }
   return err ;
}
#endif // TODO: remove line

/* function: insertchild_trienode
 * Inserts child into node. offsets must be the corresponding <trie_nodeoffsets_t> of node.
 * childindex is considered only valid if ischild_header(offsets->header) returns true.
 * digit is a single digit associated with key. After the prefix of node matched every child pointer
 * in node is associated with a unique digit to determine the child node to follow.
 *
 * Unchecked Preconditions:
 * o ! ischild_header(offsets->header)
 *     || ( forall(i < childindex): digit_trienode(*node, offsets)[i] < digit
 *          && forall(i >= childindex && i < offsets.lenchild): digit_trienode(*node, offsets)[i] > digit)
 * o ! ischild_header(offsets->header) || child_trienode(*node, offsets)[0] != 0
 * o ! ischild_header(offsets->header) || childindex == 0 || child_trienode(*node, offsets)[childindex-1] != 0
 * o ! ischild_header(offsets->header) || (0 <= childindex && childindex <= offsets.lenchild)
 */
static int insertchild_trienode(trie_node_t ** node, trie_nodeoffsets_t * offsets, uint8_t digit, trie_node_t * child, uint8_t childindex)
{
   int err ;
   trie_node_t * insertnode = *node ;

   // 1. isschild
   // 1.1 && enough space     ==> add child to array
   // 1.2 && not enough space ==> convert child array to subnode -- go to step 2
   // 2. issubnode ==> add to trie_subnode_t/trie_subnode2_t
   // 3. not enough space ==> split node (reduce prefix size) goto step 4
   // 4. enough space ==> extend node with child array and add child

   if (ischildorsubnode_header(offsets->header)) {
      if (ischild_header(offsets->header)) {
         trie_node_t ** insertchild = child_trienode(insertnode, offsets) ;

         if (  0 == insertchild[offsets->lenchild-1]
               || isexpandable_trienodeoffsets(offsets)) {

            unsigned endindex = offsets->lenchild ;

            if (0 == insertchild[endindex-1]) {
               // at least on entry in child[] free
               -- endindex ;
               while (0 == insertchild[endindex-1]) --endindex ;  // first child always != 0 ==> loop ends

            } else {
               // resize child array to a bigger size
               err = expand_trienode(node, offsets) ;
               if (err) goto ONABORT ;
            }

            uint8_t * insertdigit = digit_trienode(insertnode, offsets) ;
            if (endindex > childindex) {
               memmove(insertdigit+childindex+1, insertdigit+childindex, (endindex-childindex)*sizeof(insertdigit[0])) ;
               memmove(insertchild+childindex+1, insertchild+childindex, (endindex-childindex)*sizeof(insertchild[0])) ;
            }
            insertdigit[childindex] = digit ;
            insertchild[childindex] = child ;
            return 0 ;
         }

         err = convertchild2sub_trienode(node, offsets) ;
         if (err) goto ONABORT ;
      }

      // handle subnode
      trie_subnode_t  * subnode  = subnode_trienodeoffsets(offsets, insertnode)[0] ;
      trie_subnode2_t * subnode2 = child_triesubnode(subnode, digit)[0] ;
      if (!subnode2) {
         err = new_triesubnode2(&subnode2) ;
         if (err) goto ONABORT ;
         child_triesubnode(subnode, digit)[0] = subnode2 ;
      }
      child_triesubnode2(subnode2, digit)[0] = child ;
      // increment child count
      ++ digit_trienode(insertnode, offsets)[0] ;

   } else {
      // check for enough space

      // need split node ?

      // extend with child array

      // TODO: add child to child array of node
   }

   return 0 ;
ONABORT:
   return err ;
}

// static int removechild_trienode(trie_node_t ** node, uint8_t digit)
// {
// TODO: implement
// }

// TODO: if predecessor of node is node with prefix and 1 child try merging splitnode with predecessor
// TODO: build this merging nodes into removechild / split / convertsub2child and possible other !!!




// section: trie_t

// group: lifetime

int free_trie(trie_t * trie)
{
   int err ;

   err = delete_trienode(&trie->root) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   TRACEABORTFREE_ERRLOG(err) ;
   return err ;
}

// group: query

typedef struct trie_findresult_t   trie_findresult_t ;

struct trie_findresult_t {
   trie_nodeoffsets_t offsets ;
   // parent of node ; 0 ==> node is root node
   trie_node_t *  parent ;
   // points to entry in child[] array (=> *child != 0) || points to entry in trie_subnode2_t (*child could be 0 => node == 0)
   trie_node_t ** parent_child ;
   // node == 0 ==> trie_t.root == 0 ; node != 0 ==> trie_t.root != 0
   trie_node_t *  node ;
   // points to node who contains child which starts prefix chain (chain of nodes with prefix + 1 child pointer ; last node has no child pointer)
   trie_node_t *  chain_parent ;
   // points to entry in trie_node_t->child[] or into trie_subnode2_t->child[] of chain_parent
   trie_node_t ** chain_child ;
   // number of bytes of key prefix which could be matched
   // (is_split == false ==> prefixlen of node contained ; is_split == true ==> prefixlen of node not contained)
   uint16_t       matchkeylen ;
   // points to child[childindex] whose digit[childindex] is bigger than key[matchkeylen] ;
   // only valid if returnalue == ESRCH && is_split == 0 && ischild_header(node->header)
   uint8_t        childindex ;
   // only valid if is_split ; gives the number of matched bytes in node
   uint8_t        splitlen ;
   // false: whole prefix stored in node matched ; true: node matched only partially (or not at all)
   bool           is_split ;
} ;

/* function: findnode_trie
 * Finds node in trie who matches the given key fully or partially.
 * The returned result contains information if a node was found which matched full or
 * at least partially. */
static int findnode_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/trie_findresult_t * result)
{
   int err ;
   result->parent       = 0 ;
   result->parent_child = &trie->root ;
   result->node         = trie->root ;
   result->chain_parent = 0 ;
   result->chain_child  = &trie->root ;
   result->matchkeylen  = 0 ;
   result->is_split     = 0 ;

   if (!trie->root) return ESRCH ;

   for ( ; ;) {
      // parent == 0 || parent matched fully

      err = initdecode_trienodeoffsets(&result->offsets, result->node) ;
      if (err) return err ;

      // match prefix
      uint8_t prefixlen = lenprefix_trienodeoffsets(&result->offsets) ;
      if (prefixlen) {
         const uint8_t * prefix  = prefix_trienode(result->node, &result->offsets) ;
         const uint8_t * key2    = key + result->matchkeylen ;
         const bool      issplit = ((uint32_t)prefixlen + result->matchkeylen) > keylen ;
         if (issplit || 0 != memcmp(key2, prefix, prefixlen)/*do not match*/) {
            uint8_t splitlen = 0 ;
            if (issplit) {
               unsigned maxlen = (unsigned) keylen - result->matchkeylen ;
               while (splitlen < maxlen && key2[splitlen] == prefix[splitlen]) ++ splitlen ;
            } else {
               while (key2[splitlen] == prefix[splitlen]) ++ splitlen ;
            }
            result->is_split = 1 ;
            result->splitlen = splitlen ;
            break ;
         }
      }
      result->matchkeylen = (uint16_t) (result->matchkeylen + prefixlen) ;

      if (keylen == result->matchkeylen) return 0 ;  // isfound ?  (is_split == 0)

      if (! ischildorsubnode_header(result->offsets.header))   break ; // no more childs ?

      uint8_t d = key[result->matchkeylen] ;

      // find child
      if (ischild_header(result->offsets.header)) {
         // search in child[] array
         trie_node_t ** child = child_trienode(result->node, &result->offsets) ;
         uint8_t *      digit = digit_trienode(result->node, &result->offsets) ;
         unsigned low  = 0 ;
         unsigned high = result->offsets.lenchild ;
         while (low < high) {
            unsigned mid = (low+high)/2 ;
            if (!child[mid] || digit[mid] > d) {
               high = mid ;
            } else if (digit[mid] < d) {
               low = mid + 1 ;
            } else  {
               low = mid ;
               goto FOUND_CHILD ;
            }
         }
         result->childindex = (uint8_t) low ;
         break ;
         FOUND_CHILD:
         result->parent       = result->node ;
         result->parent_child = &child[low] ;
         result->node         = child[low] ;
         if (  0 != low || (1 < result->offsets.lenchild && 0 != child[1]) // more than one valid child pointer ?
               || isuservalue_header(result->offsets.header)) {            // uservalue ?
            result->chain_parent = result->parent ;
            result->chain_child  = result->parent_child ;
         }

      } else {
         // search in subnode->child[] array
         trie_subnode_t  * subnode  = subnode_trienodeoffsets(&result->offsets, result->node)[0] ;
         trie_subnode2_t * subnode2 = child_triesubnode(subnode, d)[0] ;
         trie_node_t    ** pchild   = child_triesubnode2(subnode2, d) ;
         if (!subnode2 || !(*pchild)) break ;
         result->parent       = result->node ;
         result->parent_child = pchild ;
         result->node         = pchild[0] ;
         result->chain_parent = result->parent ;
         result->chain_child  = result->parent_child ;
      }
      ++ result->matchkeylen ;
   }

   return ESRCH ;
}

void ** at_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen])
{
   int err ;
   trie_findresult_t findresult ;

   err = findnode_trie(trie, keylen, key, &findresult) ;
   if (err || ! isuservalue_header(findresult.offsets.header)) return 0 ;

   return uservalue_trienodeoffsets(&findresult.offsets, findresult.node) ;
}

// group: update

// TODO: implement & test
int insert2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], void * uservalue, bool islog)
{
   int err ;
   trie_node_t      * newchild = 0 ;
   trie_nodeoffsets_t offsets ;
   trie_findresult_t  findresult ;

   if (! trie->root) {
      // add to root
      err = new_trienode(&trie->root, &offsets, keylen, key, &uservalue, 0, 0, 0) ;
      if (err) goto ONABORT ;

   } else {

      err = findnode_trie(trie, keylen, key, &findresult) ;
      if (err != ESRCH) {
         if (0 == err) err = EEXIST ;
         goto ONABORT ;
      }
      // findresult.matchkeylen < keylen

      if (findresult.is_split) {
         uint8_t  digit     = 0 ;
         uint16_t keyoffset = (uint16_t) (findresult.matchkeylen + findresult.splitlen) ;

         if (keylen > keyoffset) {
            err = new_trienode(&newchild, &offsets, (uint16_t) (keylen - keyoffset -1), key + keyoffset + 1, &uservalue, 0, 0, 0) ;
            if (err) goto ONABORT ;
            digit = key[keyoffset] ;
         } else {
            // uservalue is added cause newchild == 0
         }

         // split node
         err = newsplit_trienode(&findresult.node, findresult.node, &findresult.offsets, findresult.splitlen, uservalue, digit, newchild) ;
         if (err) goto ONABORT ;

      } else {
         // findresult.node != 0 ==> add to child[] or subnode / resize or split node if not enough space

         err = new_trienode(&newchild, &offsets, (uint16_t) (keylen - findresult.matchkeylen -1), key + findresult.matchkeylen +1, &uservalue, 0, 0, 0) ;
         if (err) goto ONABORT ;

         uint8_t digit = key[findresult.matchkeylen] ;

         err = insertchild_trienode(&findresult.node, &findresult.offsets, digit, newchild, findresult.childindex) ;
         if (err) goto ONABORT ;
      }

      // adapt parent
      *findresult.parent_child = findresult.node ;
   }

   return 0 ;
ONABORT:
   if (islog || EEXIST != err) {
      TRACEABORT_ERRLOG(err) ;
   }
   (void) delete_trienode(&newchild) ;
   return err ;
}

// TODO: implement & test
int remove2_trie(trie_t * trie, uint16_t keylen, const uint8_t key[keylen], /*out*/void ** uservalue, bool islog)
{
   int err ;

   trie_findresult_t findresult ;

   err = findnode_trie(trie, keylen, key, &findresult) ;
   if (err) goto ONABORT ;

   if (! isuservalue_header(findresult.offsets.header)) {
      err = ESRCH ;
      goto ONABORT ;
   }

   // out param
   *uservalue = uservalue_trienodeoffsets(&findresult.offsets, findresult.node)[0] ;

   if (ischildorsubnode_header(findresult.offsets.header)) {
      // TODO: remove user value // + add test //
      assert(0) ;
   } else {
      // remove node
      err = delete_trienode(findresult.chain_child) ;
      if (findresult.chain_parent) {
         if (ischild_header(findresult.chain_parent->header)) {
            // TODO: adapt child[] and digit[] array // + add test //
         } else {
            // TODO: decrement child count // convert to child[] array // + add test //
         }
      } else {
         // chain_child points to trie->root
      }
      if (err) goto ONABORT ;
   }

   return 0 ;
ONABORT:
   if (islog || ESRCH != err) {
      TRACEABORT_ERRLOG(err) ;
   }
   return err ;
}


// group: test

#ifdef KONFIG_UNITTEST

typedef struct expect_node_t  expect_node_t ;

struct expect_node_t {
   uint8_t  prefixlen ;
   uint8_t  prefix[255] ;
   uint8_t  isuservalue ;
   void *   uservalue ;
   uint16_t nrchild ;
   expect_node_t * child[256] ;
} ;

static size_t nodesize_expectnode(uint16_t prefixlen, bool isuservalue, uint16_t nrchild)
{
   size_t size = sizeof(header_t) + (isuservalue ? sizeof(void*) : 0) + (prefixlen > 2) + prefixlen ;
   if (nrchild > LENCHILDMAX)
      size += sizeof(trie_subnode_t*) + 1 ;
   else
      size += nrchild * (sizeof(trie_node_t*)+1) ;
   return size ;
}

static int alloc_expectnode(/*out*/expect_node_t ** expectnode, memblock_t * memblock, uint8_t prefixlen, const uint8_t prefix[prefixlen], bool isuservalue, void * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], expect_node_t * child[nrchild])
{
   TEST(memblock->size >= sizeof(expect_node_t)) ;
   *expectnode = (expect_node_t*) memblock->addr ;
   memblock->addr += sizeof(expect_node_t) ;
   memblock->size += sizeof(expect_node_t) ;

   (*expectnode)->prefixlen = prefixlen ;
   memcpy((*expectnode)->prefix, prefix, prefixlen) ;
   (*expectnode)->isuservalue = isuservalue ;
   (*expectnode)->uservalue   = uservalue ;
   (*expectnode)->nrchild     = nrchild ;
   memset((*expectnode)->child, 0, sizeof((*expectnode)->child)) ;
   for (unsigned i = 0 ; i < nrchild ; ++i) {
      (*expectnode)->child[digit[i]] = child[i] ;
   }

   return 0 ;
ONABORT:
   return ENOMEM ;
}

static int new_expectnode(/*out*/expect_node_t ** expectnode, memblock_t * memblock, uint16_t prefixlen, const uint8_t prefix[prefixlen], bool isuservalue, void * uservalue, uint16_t nrchild, const uint8_t digit[nrchild], expect_node_t * child[nrchild])
{
   TEST(nrchild <= 256) ;
   size_t nodesize = nodesize_expectnode(0, isuservalue, nrchild) ;
   size_t freesize = SIZEMAXNODE - nodesize ;
   TEST(nodesize < SIZEMAXNODE) ;
   static_assert(SIZEMAXNODE <= 255, "freesize < 255 !") ;

   freesize -= (freesize > 2) ;
   if (prefixlen <= freesize) {
      // fit
      TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)prefixlen, prefix, isuservalue, uservalue, nrchild, digit, child)) ;
   } else {
      // does not fit
      TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)freesize, prefix + (size_t)prefixlen - freesize, isuservalue, uservalue, nrchild, digit, child)) ;
      prefixlen = (uint16_t) (prefixlen - freesize) ;
      do {
         -- prefixlen ;
         nodesize = nodesize_expectnode(0, false, 1) ;
         freesize = SIZEMAXNODE - nodesize ;
         freesize -= (freesize > 2) ;
         if (freesize > prefixlen) freesize = prefixlen ;
         TEST(0 == alloc_expectnode(expectnode, memblock, (uint8_t)freesize, prefix + prefixlen - freesize, false, 0, 1, (uint8_t[]){ prefix[prefixlen] }, (expect_node_t*[]) { *expectnode })) ;
         prefixlen = (uint16_t) (prefixlen - freesize) ;
      } while (prefixlen) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
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
      TEST(0 == expect && 0 == node) ;
   } else {
      trie_nodeoffsets_t offsets ;
      TEST(0 == initdecode_trienodeoffsets(&offsets, node)) ;
      if (nodeoffsets) {
         TEST(0 == compare_trienodeoffsets(&offsets, nodeoffsets)) ;
      }
      size_t expectsize = nodesize_expectnode(expect->prefixlen, expect->isuservalue, expect->nrchild) ;
      if (expectsize <= SIZE1NODE)      expectsize = SIZE1NODE ;
      else if (expectsize <= SIZE2NODE) expectsize = SIZE2NODE ;
      else if (expectsize <= SIZE3NODE) expectsize = SIZE3NODE ;
      else if (expectsize <= SIZE4NODE) expectsize = SIZE4NODE ;
      else                              expectsize = SIZE5NODE ;
      switch (cmpnodesize) {
      case 0:  TEST(offsets.nodesize == expectsize) ; break ;
      case 1:  TEST(offsets.nodesize == expectsize || offsets.nodesize == 2*expectsize) ; break ;
      default: TEST(offsets.nodesize >= expectsize) ; break ;
      }
      switch (expect->prefixlen) {
      case 0:
         TEST(header_NOPREFIX   == (offsets.header&header_PREFIX_MASK)) ;
         break ;
      case 1:
         TEST(header_PREFIX1    == (offsets.header&header_PREFIX_MASK)) ;
         break ;
      case 2:
         TEST(header_PREFIX2    == (offsets.header&header_PREFIX_MASK)) ;
         break ;
      default:
         TEST(header_PREFIX_LEN == (offsets.header&header_PREFIX_MASK)) ;
         break ;
      }
      TEST(expect->prefixlen == lenprefix_trienodeoffsets(&offsets)) ;
      TEST(expect->prefixlen <= 2 || node->prefixlen == expect->prefixlen) ;
      TEST(0 == memcmp(expect->prefix, prefix_trienode(node, &offsets), expect->prefixlen)) ;
      TEST(expect->isuservalue == (0 != (offsets.header & header_USERVALUE))) ;
      if (expect->isuservalue) {
         TEST(expect->uservalue == uservalue_trienodeoffsets(&offsets, node)[0]) ;
      }
      if (0 == expect->nrchild) {
         // has no childs
         TEST(0 == offsets.lenchild) ;
         TEST(0 == (offsets.header & header_CHILD)) ;
         TEST(0 == (offsets.header & header_SUBNODE)) ;
      } else {
         // has childs (either header_CHILD or header_SUBNODE set)
         TEST((0 != (offsets.header & header_CHILD)) == (0 == (offsets.header & header_SUBNODE))) ;
         if (0 != (offsets.header & header_CHILD)) {
            // encoded in child[] array
            TEST(expect->nrchild <= offsets.lenchild) ;
            for (unsigned i = expect->nrchild ; i < offsets.lenchild ; ++i) {
               TEST(0 == child_trienode(node, &offsets)[i]) ;
            }
            for (unsigned i = 0, ei = 0 ; i < expect->nrchild ; ++i, ++ei) {
               for ( ; ei < 256 ; ++ei) {
                  if (expect->child[ei]) break ;
               }
               TEST(ei < 256) ;
               TEST(ei == digit_trienode(node, &offsets)[i]) ;
               TEST(0  == compare_expectnode(expect->child[ei], child_trienode(node, &offsets)[i], 0, cmpsubnodesize, cmpsubnodesize)) ;
            }
         } else {
            // encoded in subnode
            TEST(1 == offsets.lenchild) ;
            TEST(expect->nrchild == 1+digit_trienode(node, &offsets)[0]) ;
            trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0] ;
            for (unsigned i = 0 ; i < 256 ; ++i) {
               trie_subnode2_t * subnode2 = child_triesubnode(subnode, (uint8_t)i)[0] ;
               if (expect->child[i]) {
                  TEST(0 != subnode2) ;
                  TEST(0 == compare_expectnode(expect->child[i], child_triesubnode2(subnode2, (uint8_t)i)[0], 0, cmpsubnodesize, cmpsubnodesize)) ;
               } else {
                  TEST(0 == subnode2 || 0 == child_triesubnode2(subnode2, (uint8_t)i)[0]) ;
               }
            }
         }
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_header_enum(void)
{
   static_assert(0 == (header_SIZENODE_MASK& (header_USERVALUE|header_PREFIX_MASK|header_CHILD|header_SUBNODE)), "no overlap") ;
   static_assert(0 == (header_PREFIX_MASK  & (header_SIZENODE_MASK|header_USERVALUE|header_CHILD|header_SUBNODE)), "no overlap") ;
   static_assert(0 == (header_USERVALUE     & (header_SIZENODE_MASK|header_PREFIX_MASK|header_CHILD|header_SUBNODE)), "no overlap") ;
   static_assert(0 == (header_CHILD        & (header_SIZENODE_MASK|header_PREFIX_MASK|header_USERVALUE|header_SUBNODE)), "no overlap") ;
   static_assert(0 == (header_SUBNODE      & (header_SIZENODE_MASK|header_PREFIX_MASK|header_USERVALUE|header_CHILD)), "no overlap") ;
   static_assert(0 != header_SIZENODE_MASK, "valid value") ;
   static_assert(0 != header_PREFIX_MASK, "valid value") ;
   static_assert(0 != header_USERVALUE,    "valid value") ;
   static_assert(0 != header_CHILD,       "valid value") ;
   static_assert(0 != header_SUBNODE,     "valid value") ;
   static_assert(0 == header_SIZE1NODE,   "valid value") ;
   static_assert(0 != header_SIZE2NODE,   "valid value") ;
   static_assert(0 != header_SIZE3NODE,   "valid value") ;
   static_assert(0 != header_SIZE4NODE,   "valid value") ;
   static_assert(0 != header_SIZE5NODE,   "valid value") ;
   static_assert(0 == header_NOPREFIX,    "valid value") ;
   static_assert(0 != header_PREFIX1,     "valid value") ;
   static_assert(0 != header_PREFIX2,     "valid value") ;
   static_assert(0 != header_PREFIX_LEN,  "valid value") ;
   static_assert(header_SIZE1NODE == (header_SIZE1NODE & header_SIZENODE_MASK), "header_SIZENODE_MASK is mask") ;
   static_assert(header_SIZE2NODE == (header_SIZE2NODE & header_SIZENODE_MASK), "header_SIZENODE_MASK is mask") ;
   static_assert(header_SIZE3NODE == (header_SIZE3NODE & header_SIZENODE_MASK), "header_SIZENODE_MASK is mask") ;
   static_assert(header_SIZE4NODE == (header_SIZE4NODE & header_SIZENODE_MASK), "header_SIZENODE_MASK is mask") ;
   static_assert(header_SIZE5NODE == (header_SIZE5NODE & header_SIZENODE_MASK), "header_SIZENODE_MASK is mask") ;
   static_assert(header_SIZENODE_MASK == (header_SIZE1NODE|header_SIZE2NODE|header_SIZE3NODE|header_SIZE4NODE|header_SIZE5NODE), "header_SIZENODE_MASK is mask") ;
   static_assert(0 == (header_USERVALUE & (header_USERVALUE-1)), "power of 2") ;
   static_assert(0 == (header_CHILD & (header_CHILD-1)),       "power of 2") ;
   static_assert(0 == (header_SUBNODE & (header_SUBNODE-1)),   "power of 2") ;
   static_assert(header_NOPREFIX == (header_NOPREFIX & header_PREFIX_MASK), "header_PREFIX_MASK is mask") ;
   static_assert(header_PREFIX1  == (header_PREFIX1 & header_PREFIX_MASK), "header_PREFIX_MASK is mask") ;
   static_assert(header_PREFIX2  == (header_PREFIX2 & header_PREFIX_MASK), "header_PREFIX_MASK is mask") ;
   static_assert(header_PREFIX_LEN == (header_PREFIX_LEN & header_PREFIX_MASK), "header_PREFIX_MASK is mask") ;
   static_assert(header_PREFIX_MASK == (header_NOPREFIX|header_PREFIX1|header_PREFIX2|header_PREFIX_LEN), "header_PREFIX_MASK is mask") ;
   return 0 ;
}

static int test_header(void)
{
   // ////
   // group constants

   // TEST SIZE1NODE, SIZE2NODE, SIZE3NODE, SIZE4NODE, SIZE5NODE, SIZEMAXNODE
   static_assert( SIZE1NODE == sizeof(trie_node_t),     "sizeof(trie_node_t) == sizeof 2 pointers") ;
   static_assert( SIZE1NODE == 2*sizeof(trie_node_t*),  "sizeof(trie_node_t) == sizeof 2 pointers") ;
   static_assert( SIZE2NODE == 4*sizeof(trie_node_t*),  "double size") ;
   static_assert( SIZE3NODE == 8*sizeof(trie_node_t*),  "double size") ;
   static_assert( SIZE4NODE == 16*sizeof(trie_node_t*), "double size") ;
   static_assert( SIZE5NODE == 32*sizeof(trie_node_t*), "double size") ;
   static_assert( SIZEMAXNODE == SIZE5NODE,             "maximum supported size is 32 pointers") ;
   static_assert( SIZEMAXNODE <= 256
                  && SIZEMAXNODE-sizeof(header_t) <= 255, "size without header fits in 8 bit") ;

   // ////
   // group query

   // TEST ischild_header
   for (unsigned header = 0 ; header <= header_CHILD ; header += header_CHILD) {
      int ischild = (header != 0) ;
      TEST(ischild == ischild_header((header_t) header)) ;
      TEST(ischild == ischild_header((header_t) (header|~(unsigned)header_CHILD))) ;
   }

   // TEST ischildorsubnode_header
   static_assert(2*header_CHILD == header_SUBNODE, "for loop produces all 4 states") ;
   for (unsigned header = 0 ; header <= (header_CHILD|header_SUBNODE) ; header += header_CHILD) {
      int ischild = (header != 0) ;
      TEST(ischild == ischildorsubnode_header((header_t) header)) ;
      TEST(ischild == ischildorsubnode_header((header_t) (header|~(unsigned)(header_CHILD|header_SUBNODE)))) ;
   }

   // TEST issubnode_header
   for (unsigned header = 0 ; header <= header_SUBNODE ; header += header_SUBNODE) {
      int issubnode = (header != 0) ;
      TEST(issubnode == issubnode_header((header_t) header)) ;
      TEST(issubnode == issubnode_header((header_t) (header|~(unsigned)header_SUBNODE))) ;
   }

   // TEST isuservalue_header
   for (unsigned header = 0 ; header <= header_USERVALUE ; header += header_USERVALUE) {
      int isuservalue = (header != 0) ;
      TEST(isuservalue == isuservalue_header((header_t) header)) ;
      TEST(isuservalue == isuservalue_header((header_t) (header|~(unsigned)header_USERVALUE))) ;
   }

   // TEST sizenode_header
   static_assert(header_SIZE1NODE == 0, "allows simple shift") ;
   static_assert(header_SIZE1NODE + 1 == header_SIZE2NODE, "allows simple shift") ;
   static_assert(header_SIZE2NODE + 1 == header_SIZE3NODE, "allows simple shift") ;
   static_assert(header_SIZE3NODE + 1 == header_SIZE4NODE, "allows simple shift") ;
   static_assert(header_SIZE4NODE + 1 == header_SIZE5NODE, "allows simple shift") ;
   static_assert(SIZEMAXNODE == SIZE5NODE, "every bit considered") ;
   TEST(SIZE1NODE == sizenode_header(header_SIZE1NODE)) ;
   TEST(SIZE2NODE == sizenode_header(header_SIZE2NODE)) ;
   TEST(SIZE3NODE == sizenode_header(header_SIZE3NODE)) ;
   TEST(SIZE4NODE == sizenode_header(header_SIZE4NODE)) ;
   TEST(SIZE5NODE == sizenode_header(header_SIZE5NODE)) ;
   TEST(SIZE5NODE <  sizenode_header(header_SIZENODE_MASK)) ;
   TEST(SIZE1NODE == sizenode_header((header_t)(header_SIZE1NODE|~header_SIZENODE_MASK))) ;
   TEST(SIZE2NODE == sizenode_header((header_t)(header_SIZE2NODE|~header_SIZENODE_MASK))) ;
   TEST(SIZE3NODE == sizenode_header((header_t)(header_SIZE3NODE|~header_SIZENODE_MASK))) ;
   TEST(SIZE4NODE == sizenode_header((header_t)(header_SIZE4NODE|~header_SIZENODE_MASK))) ;
   TEST(SIZE5NODE == sizenode_header((header_t)(header_SIZE5NODE|~header_SIZENODE_MASK))) ;
   TEST(SIZE5NODE <  sizenode_header((header_t)(header_SIZENODE_MASK|~header_SIZENODE_MASK))) ;

   // ////
   // group change

   // TEST clear_header
   for (header_t i = 1 ; i ; i = (header_t)(i<<1)) {
      header_t header = (header_t)-1 ;
      header_t result = (header_t)(~i) ;
      TEST(result == clear_header(header, (header_e)i)) ;
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_triesubnode2(void)
{
   trie_subnode2_t * subnode[16] = { 0 } ;
   size_t            allocsize   = SIZEALLOCATED_MM() ;

   // TEST new_triesubnode2
   for (unsigned i = 0 ; i < lengthof(subnode) ; ++i) {
      TEST(0 == new_triesubnode2(&subnode[i])) ;
      TEST(0 != subnode[i]) ;
      for (unsigned ci = 0 ; ci < lengthof(subnode[i]->child) ; ++ci) {
         TEST(0 == subnode[i]->child[ci]) ;
      }
      allocsize += sizeof(trie_subnode2_t) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }

   // TEST delete_triesubnode2
   for (unsigned i = 0 ; i < lengthof(subnode) ; ++i) {
      TEST(0 == delete_triesubnode2(&subnode[i])) ;
      TEST(0 == subnode[i]) ;
      allocsize -= sizeof(trie_subnode2_t) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
      TEST(0 == delete_triesubnode2(&subnode[i])) ;
      TEST(0 == subnode[i]) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }

   // TEST new_triesubnode2: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == new_triesubnode2(&subnode[0])) ;
   TEST(0 == subnode[0]) ;
   TEST(allocsize == SIZEALLOCATED_MM()) ;

   // TEST delete_triesubnode2: ERROR
   TEST(0 == new_triesubnode2(&subnode[0])) ;
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL) ;
   TEST(EINVAL == delete_triesubnode2(&subnode[0])) ;
   TEST(0 == subnode[0]) ;
   TEST(allocsize == SIZEALLOCATED_MM()) ;

   // TEST child_triesubnode2
   TEST(0 == new_triesubnode2(&subnode[0])) ;
   for (unsigned i = 0, offset = 0 ; i < 256 ; ++i, ++offset, offset = (offset < lengthof(subnode[0]->child) ? offset : 0)) {
      TEST(&subnode[0]->child[offset] == child_triesubnode2(subnode[0], (uint8_t)i)) ;
   }
   TEST(0 == delete_triesubnode2(&subnode[0])) ;

   return 0 ;
ONABORT:
   for (unsigned i = 0 ; i < lengthof(subnode) ; ++i) {
      delete_triesubnode2(&subnode[i]) ;
   }
   return EINVAL ;
}

static int test_triesubnode(void)
{
   uint8_t           digit[256] ;
   trie_node_t *     child[256] ;
   trie_subnode_t *  subnode   = 0 ;
   trie_subnode2_t * subnode2 ;
   size_t            allocsize = SIZEALLOCATED_MM() ;

   // prepare
   for (unsigned i = 0 ; i < lengthof(digit) ; ++i) {
      digit[i] = (uint8_t) i ;
   }
   for (unsigned i = 0 ; i < lengthof(child) ; ++i) {
      child[i] = (void*) (1+i) ;
   }

   static_assert(sizeof(trie_subnode_t) == SIZE5NODE, "fit into SIZE5NODE") ;

   // TEST new_triesubnode, delete_triesubnode
   for (unsigned i = 0 ; i < lengthof(child) ; ++i) {
      TEST(0 == new_triesubnode(&subnode, (uint16_t)i, digit, child)) ;
      TEST(0 != subnode) ;
      for (unsigned ci = 0 ; ci < lengthof(subnode->child) ; ++ci) {
         if (ci * lengthof(subnode2->child) < i) {
            TEST(0 != subnode->child[ci]) ;
         } else {
            TEST(0 == subnode->child[ci]) ;
         }
      }
      unsigned alignedi = ((i + lengthof(subnode2->child)-1) / lengthof(subnode2->child)) * lengthof(subnode2->child) ;
      for (unsigned ci = 0 ; ci < alignedi ; ++ci) {
         if (ci < i) {
            TEST(0 != subnode->child[ci/lengthof(subnode2->child)]->child[ci%lengthof(subnode2->child)]) ;
         } else {
            TEST(0 == subnode->child[ci/lengthof(subnode2->child)]->child[ci%lengthof(subnode2->child)]) ;
         }
      }
      size_t allocsize2 = allocsize + sizeof(trie_subnode_t) + alignedi/lengthof(subnode2->child) * sizeof(trie_subnode2_t) ;
      TEST(allocsize2 == SIZEALLOCATED_MM()) ;
      TEST(0 == delete_triesubnode(&subnode)) ;
      TEST(0 == subnode) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
      TEST(0 == delete_triesubnode(&subnode)) ;
      TEST(0 == subnode) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }

   // TEST new_triesubnode: ERROR
   for (unsigned errcount = 1 ; errcount <= 1 + lengthof(subnode->child) ; ++errcount) {
      init_testerrortimer(&s_trie_errtimer, errcount, ENOMEM) ;
      TEST(ENOMEM == new_triesubnode(&subnode, lengthof(digit), digit, child)) ;
      TEST(0 == subnode) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }
   init_testerrortimer(&s_trie_errtimer, 2 + lengthof(subnode->child), ENOMEM) ;
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child)) ;
   free_testerrortimer(&s_trie_errtimer) ;
   TEST(0 == delete_triesubnode(&subnode)) ;

   // TEST delete_triesubnode: ERROR
   for (unsigned errcount = 1 ; errcount <= 1 + lengthof(subnode->child) ; ++errcount) {
      TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child)) ;
      init_testerrortimer(&s_trie_errtimer, errcount, EINVAL) ;
      TEST(EINVAL == delete_triesubnode(&subnode)) ;
      TEST(0 == subnode) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child)) ;
   init_testerrortimer(&s_trie_errtimer, 2 + lengthof(subnode->child), EINVAL) ;
   TEST(0 == delete_triesubnode(&subnode)) ;
   free_testerrortimer(&s_trie_errtimer) ;

   // TEST child_triesubnode
   TEST(0 == new_triesubnode(&subnode, lengthof(digit), digit, child)) ;
   for (unsigned i = 0 ; i < 256 ; ++i) {
      TEST(&subnode->child[i/(256/lengthof(subnode->child))] == child_triesubnode(subnode, (uint8_t)i)) ;
   }
   TEST(0 == delete_triesubnode(&subnode)) ;

   return 0 ;
ONABORT:
   delete_triesubnode(&subnode) ;
   return EINVAL ;
}

static int test_trienodeoffset(void)
{
   trie_nodeoffsets_t offsets ;

   // ////
   // group constants

   // TEST HEADERSIZE
   static_assert( HEADERSIZE == offsetof(trie_node_t, prefixlen), "size of header") ;

   // TEST PTRALIGN
   static_assert( (PTRALIGN >= 1) && (PTRALIGN&(PTRALIGN-1)) == 0, "must be power of two value") ;
   static_assert( PTRALIGN == offsetof(trie_node_t, ptrdata), "alignment of pointer in struct") ;

   // ////
   // group helper

   // TEST divideby5, divideby8, dividebychilddigitsize
   for (uint32_t i = 0 ; i < 256 ; ++i) {
      uint8_t d5 = divideby5((uint8_t)i) ;
      uint8_t d9 = divideby9((uint8_t)i) ;
      uint8_t dc = dividebychilddigitsize((uint8_t)i) ;
      TEST(d5 == i/5) ;
      TEST(d9 == i/9) ;
      TEST(dc == i/(sizeof(trie_node_t*)+1)) ;
   }

   // ////
   // group lifetime

   // TEST init_trienodeoffsets
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild) {
         for (uint32_t prefixlen = 0 ; prefixlen < 65536 ; prefixlen = (prefixlen >= 3 ? 2*prefixlen : prefixlen), ++prefixlen) {
            unsigned lenchild = (nrchild > LENCHILDMAX) ? 1u : nrchild ;
            memset(&offsets, 0, sizeof(offsets)) ;
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild) ;
            size_t  size    = HEADERSIZE + (isuser ? sizeof(void*) : 0) + lenchild * sizeof(trie_node_t*)
                              + (size_t) lenchild /*digit*/ + (size_t) (prefixlen > 2) ;
            uint32_t len    = prefixlen ;
            unsigned header = 0 ;
            if (size + prefixlen > SIZE5NODE) {
               len  = SIZE5NODE - size ;
               size = SIZE5NODE ;
               header = header_SIZE5NODE ;
            } else {
               size += prefixlen ;
               size_t oldsize = size ;
               if (size <= SIZE1NODE)        size = SIZE1NODE, header = header_SIZE1NODE ;
               else if (size <= SIZE2NODE)   size = SIZE2NODE, header = header_SIZE2NODE ;
               else if (size <= SIZE3NODE)   size = SIZE3NODE, header = header_SIZE3NODE ;
               else if (size <= SIZE4NODE)   size = SIZE4NODE, header = header_SIZE4NODE ;
               else                          size = SIZE5NODE, header = header_SIZE5NODE ;
               if (0 < nrchild && nrchild <= LENCHILDMAX) {
                  // fill empty bytes with child pointer
                  for ( ; oldsize + 1 + sizeof(trie_node_t*) <= size ; ++lenchild) {
                     oldsize += 1+sizeof(trie_node_t*) ;
                  }
               }
            }
            header |= prefixlen == 0 ? header_NOPREFIX : prefixlen == 1 ? header_PREFIX1 : prefixlen == 2 ? header_PREFIX2 : header_PREFIX_LEN ;
            if (isuser)   header |= header_USERVALUE ;
            if (nrchild > LENCHILDMAX) header |= header_SUBNODE ;
            else if (nrchild)          header |= header_CHILD ;
            static_assert(offsetof(trie_nodeoffsets_t, nodesize) == 0, "") ;
            TEST(offsets.nodesize  == size) ;
            TEST(offsets.lenchild  == lenchild) ;
            TEST(offsets.header    == header) ;
            TEST(offsets.prefix    == HEADERSIZE + (prefixlen > 2)) ;
            TEST(offsets.digit     == offsets.prefix + len) ;
            size_t aligned = (offsets.digit + (size_t)lenchild + PTRALIGN-1) & ~(PTRALIGN-1) ;
            TEST(offsets.uservalue == aligned) ;
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
         }
      }
   }

   // TEST initdecode_trienodeoffsets
   static_assert(header_SIZENODE_MASK == 7, "encoded in first 3 bits") ;
   for (uint8_t sizemask = 0 ; sizemask <= header_SIZENODE_MASK ; ++sizemask) {
      for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
         for (uint8_t ischild = 0 ; ischild <= 2 ; ++ischild) {  // ischild == 2 ==> subnode
            for (uint16_t prefixlen = 0 ; prefixlen < 256 ; ++prefixlen) {
               header_t header      = sizemask ;
               size_t   needed_size = HEADERSIZE + (prefixlen > 2) + prefixlen
                                    + (isuser ? sizeof(void*) : 0) + (ischild == 2 ? 1+sizeof(trie_subnode_t*) : 0) ;
               size_t   lenchild     = ischild == 0 ? 0 : ischild == 2 ? 1
                                       : (sizenode_header(header)-needed_size)/(sizeof(trie_node_t*)+1) ;
               // encode additional header bits
               if (ischild == 1)          header |= header_CHILD ;
               else if (ischild == 2)     header |= header_SUBNODE ;
               if (isuser)                header |= header_USERVALUE ;
               if (prefixlen == 1)        header |= header_PREFIX1 ;
               else if (prefixlen == 2)   header |= header_PREFIX2 ;
               else if (prefixlen >  2)   header |= header_PREFIX_LEN ;
               trie_node_t dummy ;
               memset(&dummy, 0, sizeof(dummy)) ;
               dummy.header    = header ;
               dummy.prefixlen = (uint8_t) prefixlen ;
               // decode
               memset(&offsets, 255, sizeof(offsets)) ;
               int err = initdecode_trienodeoffsets(&offsets, &dummy) ;
               if (  needed_size > sizenode_header(sizemask)
                     || sizenode_header(sizemask) > SIZEMAXNODE
                     || (1 == ischild && 0 == lenchild)) {
                  TEST(EINVARIANT == err) ;
                  break ;
               }
               TEST(0 == err) ;
               // compare result
               TEST(offsets.nodesize  == sizenode_header(header)) ;
               TEST(offsets.lenchild  == lenchild) ;
               TEST(offsets.header    == header) ;
               TEST(offsets.prefix    == HEADERSIZE       + (prefixlen > 2)) ;
               TEST(offsets.digit     == offsets.prefix   + prefixlen) ;
               size_t aligned = (offsets.digit + lenchild + PTRALIGN-1) & ~(PTRALIGN-1) ;
               TEST(offsets.uservalue == aligned) ;
               TEST(offsets.child     == offsets.uservalue + isuser * sizeof(void*)) ;
               TEST(offsets.nodesize  >= offsets.child + lenchild * sizeof(trie_node_t*)) ;
               TEST(offsets.nodesize  <= offsets.child + (lenchild+1) * sizeof(trie_node_t*) || ischild != 1) ;
            }
         }
      }
   }

   // TEST initdecode_trienodeoffsets: header_CHILD and header_SUBNODE set
   TEST(EINVARIANT == initdecode_trienodeoffsets(&offsets, &(trie_node_t) { .header = header_SIZE5NODE|header_SUBNODE|header_CHILD } )) ;

   // ////
   // group query

   // TEST compare_trienodeoffsets
   trie_nodeoffsets_t offsets2 ;
   memset(&offsets, 0, sizeof(offsets)) ;
   memset(&offsets2, 0, sizeof(offsets2)) ;
   for (unsigned i = 0 ; i < 7 ; ++i) {
      for (int v = -1 ; v <= 0 ; ++v) {
         switch (i)
         {
         case 0: offsets.nodesize   = (typeof(offsets.nodesize))v ;  break ;
         case 1: offsets.lenchild   = (typeof(offsets.lenchild))v ;  break ;
         case 2: offsets.header     = (typeof(offsets.header))v ;    break ;
         case 3: offsets.prefix     = (typeof(offsets.prefix))v ;    break ;
         case 4: offsets.digit      = (typeof(offsets.digit))v ;     break ;
         case 5: offsets.uservalue  = (typeof(offsets.uservalue))v ; break ;
         case 6: offsets.child      = (typeof(offsets.child))v ;     break ;
         }
         if (v) {
            TEST(0 <  compare_trienodeoffsets(&offsets, &offsets2)) ;
            TEST(0 >  compare_trienodeoffsets(&offsets2, &offsets)) ;
         } else {
            TEST(0 == compare_trienodeoffsets(&offsets, &offsets2)) ;
            TEST(0 == compare_trienodeoffsets(&offsets2, &offsets)) ;
         }
      }
   }

   // TEST isexpandable_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   for (uint16_t i = 0 ; i < SIZEMAXNODE ; ++i) {
      offsets.nodesize = i ;
      TEST(1 == isexpandable_trienodeoffsets(&offsets)) ;
   }
   for (uint16_t i = SIZEMAXNODE ; i < SIZEMAXNODE+3 ; ++i) {
      offsets.nodesize = i ;
      TEST(0 == isexpandable_trienodeoffsets(&offsets)) ;
   }

   // TEST lenprefix_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   for (unsigned len = 0 ; len < 256 ; ++len) {
      for (unsigned s = 0 ; s < 256-len ; ++s) {
         static_assert(offsetof(trie_nodeoffsets_t, digit) == sizeof(uint8_t) + offsetof(trie_nodeoffsets_t, prefix), "digit is next after prefix") ;
         offsets.prefix = (uint8_t) s ;
         offsets.digit  = (uint8_t) (s + len) ;
         TEST(len == lenprefix_trienodeoffsets(&offsets)) ;
      }
   }

   // TEST lenuservalue_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   for (uint16_t i = 0 ; i < SIZEMAXNODE ; ++i) {
      for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
         static_assert(offsetof(trie_nodeoffsets_t, child) == sizeof(uint8_t) + offsetof(trie_nodeoffsets_t, uservalue), "child is next after uservalue") ;
         int len = isuser ? sizeof(void*) : 0 ;
         offsets.uservalue = (uint8_t) i ;
         offsets.child     = (uint8_t) (i + len) ;
         TEST(len == lenuservalue_trienodeoffsets(&offsets)) ;
      }
   }

   // TEST uservalue_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   for (unsigned i = 0 ; i < SIZEMAXNODE/sizeof(trie_node_t*) ; ++i) {
      uint8_t array[SIZEMAXNODE] = { 0 } ;
      offsets.uservalue = (uint8_t) (i * sizeof(trie_node_t*)) ;
      TEST(&((void**)array)[i] == uservalue_trienodeoffsets(&offsets, (trie_node_t*)array)) ;
   }

   // TEST subnode_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   for (unsigned i = 0 ; i < SIZEMAXNODE/sizeof(trie_node_t*) ; ++i) {
      uint8_t array[SIZEMAXNODE] = { 0 } ;
      offsets.child = (uint8_t) (i * sizeof(trie_node_t*)) ;
      TEST(&((trie_subnode_t**)array)[i] == subnode_trienodeoffsets(&offsets, (trie_node_t*)array)) ;
   }

   // TEST sizefree_trienodeoffsets, sizegrowprefix_trienodeoffsets
   memset(&offsets, 0, sizeof(offsets)) ;
   TEST(0 == sizegrowprefix_trienodeoffsets(&offsets)) ;
   TEST(0 == sizefree_trienodeoffsets(&offsets)) ;
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= LENCHILDMAX+1 ; ++nrchild) {
         for (uint16_t prefixlen = 0 ; prefixlen <= 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            if (lenprefix_trienodeoffsets(&offsets) != prefixlen) continue ; // too big
            unsigned expect = (unsigned) offsets.nodesize
                              - (nrchild <= LENCHILDMAX ? nrchild*(sizeof(trie_node_t*)+1) : (sizeof(trie_node_t*)+1))
                              - prefixlen - (prefixlen>2) - HEADERSIZE - (isuser ? sizeof(void*) : 0) ;
            while (nrchild && nrchild <= LENCHILDMAX && expect >= (sizeof(trie_node_t*)+1)) {
               expect -= (sizeof(trie_node_t*)+1) ;
            }
            TEST(expect == sizefree_trienodeoffsets(&offsets)) ;
            if ((expect+prefixlen) > 2 && (offsets.header&header_PREFIX_MASK) != header_PREFIX_LEN) -- expect ;
            TEST(expect == sizegrowprefix_trienodeoffsets(&offsets)) ;
         }
      }
   }

   // ////
   // group change

   // TEST convert2subnode_trienodeoffsets
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 1 ; nrchild <= 16 ; ++nrchild) {
         for (uint32_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild) ;
            TEST(1 == ischild_header(offsets.header)) ;
            trie_nodeoffsets_t oldoff = offsets ;
            convert2subnode_trienodeoffsets(&offsets) ;
            // check adapted offsets
            TEST(offsets.nodesize  == oldoff.nodesize) ;
            TEST(offsets.lenchild  == 0) ;
            TEST(offsets.header    == ((oldoff.header & ~(header_CHILD)) | header_SUBNODE)) ;
            TEST(offsets.prefix    == oldoff.prefix) ;
            TEST(offsets.digit     == oldoff.digit) ;
            size_t aligned = (offsets.digit + 1u + PTRALIGN-1) & ~(PTRALIGN-1) ;
            TEST(offsets.uservalue == aligned) ;
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
            TEST(offsets.nodesize  >= offsets.child + sizeof(trie_subnode_t*)) ;
         }
      }
   }

   // TEST shrinkprefix_trienodeoffsets
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1 ; prefixlen < 16 ; ++prefixlen) {
            for (uint8_t newprefixlen = 0 ; newprefixlen < prefixlen ; ++newprefixlen) {
               init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild) ;
               trie_nodeoffsets_t oldoff = offsets ;
               unsigned freesize = (unsigned) offsets.nodesize - sizeof(header_t)
                                 - (newprefixlen > 2) - newprefixlen
                                 - (isuser ? sizeof(void*) : 0) ;
               shrinkprefix_trienodeoffsets(&offsets, newprefixlen) ;
               // check adapted offsets
               TEST(offsets.nodesize  == oldoff.nodesize) ;
               TEST(offsets.lenchild  == (0 == nrchild ? 0 : nrchild <= LENCHILDMAX ? freesize/(sizeof(trie_node_t*)+1) : 1)) ;
               switch (newprefixlen) {
               case 0:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_NOPREFIX)) ;  break ;
               case 1:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX1)) ;   break ;
               case 2:  TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX2)) ;   break ;
               default: TEST(offsets.header    == ((oldoff.header & ~header_PREFIX_MASK) | header_PREFIX_LEN)) ; break ;
               }
               TEST(offsets.prefix    == oldoff.prefix - (newprefixlen <= 2 && prefixlen > 2)) ;
               TEST(offsets.digit     == offsets.prefix + newprefixlen) ;
               size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1) ;
               TEST(offsets.uservalue == aligned) ;
               TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
            }
         }
      }
   }

   // TEST changesize_trienodeoffsets
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint32_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, (uint16_t)prefixlen, isuser, nrchild) ;
            for (header_t headersize = header_SIZE1NODE ; headersize <= header_SIZE5NODE ; ++ headersize) {
               if (offsets.child >= sizenode_header(headersize)) continue ;
               trie_nodeoffsets_t oldoff = offsets ;
               changesize_trienodeoffsets(&offsets, headersize) ;
               TEST(offsets.nodesize  == sizenode_header(headersize)) ;
               TEST(offsets.header    == ((oldoff.header&~header_SIZENODE_MASK)|headersize)) ;
               TEST(offsets.prefix    == oldoff.prefix) ;
               TEST(offsets.digit     == oldoff.digit) ;
               size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1) ;
               TEST(offsets.uservalue == aligned) ;
               TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
               TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*)) ;
               if (ischild_header(offsets.header)) {
                  TEST(1 <= offsets.lenchild) ;
                  TEST(offsets.nodesize < offsets.child + offsets.lenchild * sizeof(trie_node_t*) + sizeof(trie_node_t*) + 1) ;
               } else if (issubnode_header(offsets.header)) {
                  TEST(1 == offsets.lenchild) ;
               } else {
                  TEST(0 == offsets.lenchild) ;
               }
            }
         }
      }
   }

   // TEST growprefix_trienodesoffsets: isshrinkchild = 0
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            unsigned growsize = sizegrowprefix_trienodeoffsets(&offsets) ;
            for (unsigned incr = 1 ; incr <= growsize ; ++incr) {
               trie_nodeoffsets_t offsets3 = offsets ;
               init_trienodeoffsets(&offsets2, (uint16_t)(prefixlen + incr), isuser, nrchild) ;
               growprefix_trienodesoffsets(&offsets3, (uint8_t) incr, 0) ;
               TEST(0 == compare_trienodeoffsets(&offsets2, &offsets3)) ;
            }
         }
      }
   }

   // TEST growprefix_trienodesoffsets: isshrinkchild = 1
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 2 ; nrchild <= LENCHILDMAX ; ++nrchild) {
         for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            if (lenprefix_trienodeoffsets(&offsets) < prefixlen) continue ; // not enough space
            unsigned growsize = sizegrowprefix_trienodeoffsets(&offsets) ;
            for (unsigned incr = 1 ; incr <= growsize+sizeof(trie_node_t*) ; ++incr) {
               trie_nodeoffsets_t offsets3 = offsets ;
               uint16_t           nrchild2 = (uint16_t) ((offsets.lenchild > LENCHILDMAX ? LENCHILDMAX : offsets.lenchild) - (incr>growsize)) ;
               init_trienodeoffsets(&offsets2, (uint16_t)(prefixlen + incr), isuser, nrchild2) ;
               growprefix_trienodesoffsets(&offsets3, (uint8_t) incr, (incr>growsize)) ;
               TEST(0 == compare_trienodeoffsets(&offsets2, &offsets3)) ;
            }
         }
      }
   }

   // TEST adduservalue_trienodeoffsets
   for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      trie_node_t * nodebuffer[SIZEMAXNODE/sizeof(trie_node_t*)] = { 0 } ;
      trie_node_t * node = (trie_node_t*) nodebuffer ;
      for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
         init_trienodeoffsets(&offsets, prefixlen, 0/*no user*/, nrchild) ;
         bool isfreesize = (sizefree_trienodeoffsets(&offsets) >= sizeof(void*)) ;
         if (! isfreesize && ! isfreechild_trienode(node, &offsets)) continue ;
         offsets2 = offsets ;
         adduservalue_trienodeoffsets(&offsets) ;
         TEST(offsets.nodesize == offsets2.nodesize) ;
         TEST(offsets.header == (offsets2.header | header_USERVALUE)) ;
         TEST(offsets.prefix == offsets2.prefix) ;
         TEST(offsets.digit  == offsets2.digit) ;
         if (isfreesize) {
            TEST(offsets.lenchild  == offsets2.lenchild) ;
            TEST(offsets.uservalue == offsets2.uservalue) ;
            TEST(offsets.child     == offsets2.child + sizeof(void*)) ;
         } else {
            TEST(nrchild <= LENCHILDMAX) ;
            TEST(2 <= offsets2.lenchild) ;
            TEST(offsets.lenchild  == offsets2.lenchild-1) ;
            TEST(offsets.uservalue == (((unsigned)offsets2.digit+offsets.lenchild+PTRALIGN-1)&~(PTRALIGN-1))) ;
            TEST(offsets.child     == offsets.uservalue + sizeof(void*)) ;
            TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*)) ;
            TEST(offsets.nodesize  <= offsets.child + (offsets.lenchild+1u) * sizeof(trie_node_t*)) ;
         }
      }
   }

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int build_subnode_trie(/*out*/trie_node_t ** root, uint8_t depth, uint16_t nrchild)
{
   int err ;
   uint8_t              digit[256] ;
   trie_node_t        * child[256] ;
   trie_nodeoffsets_t   offsets ;

   for (unsigned i = 0 ; i < 256 ; ++i) {
      digit[i] = (uint8_t) i ;
      child[i] = 0 ;
   }

   if (0 == depth) {
      for (unsigned i = 0 ; i < nrchild && i < 256 ; ++i) {
         void * uservalue = 0 ;
         err = new_trienode(&child[i], &offsets, 0, 0, &uservalue, 0, 0, 0) ;
         if (err) goto ONABORT ;
      }
   } else {
      for (unsigned i = 0 ; i < 256 ; i += 127) {
         err = build_subnode_trie(&child[i], (uint8_t) (depth-1), nrchild) ;
         if (err) goto ONABORT ;
      }
   }

   err = new_trienode(root, &offsets, 0, 0, 0, 256, digit, child) ;
   if (err) goto ONABORT ;

   return 0 ;
ONABORT:
   return err ;
}

static int test_trienode(void)
{
   trie_node_t *  node ;
   trie_node_t *  node2 ;
   size_t         allocsize ;
   uint8_t        digit[256] ;
   trie_node_t *  child[256] = { 0 } ;
   void *         uservalue ;
   trie_nodeoffsets_t offsets ;
   memblock_t     key = memblock_INIT_FREEABLE ;
   memblock_t     expectnode_memblock = memblock_INIT_FREEABLE ;
   memblock_t     expectnode_memblock2 ;
   expect_node_t* expectnode ;
   expect_node_t* expectnode2 ;

   // prepare
   TEST(0 == ALLOC_MM(1024*1024, &expectnode_memblock)) ;
   TEST(0 == ALLOC_MM(65536, &key)) ;
   for (unsigned i = 0 ; i < 65536 ; ++i) {
      key.addr[i] = (uint8_t) (29*i) ;
   }
   for (unsigned i = 0 ; i < 256 ; ++i) {
      digit[i] = (uint8_t) i ;
   }
   allocsize = SIZEALLOCATED_MM() ;

   // ////
   // group query-helper

   // TEST child_trienode
   memset(&offsets, 0, sizeof(offsets)) ;
   for (unsigned i = 0 ; i < SIZEMAXNODE/sizeof(trie_node_t*) ; ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 } ;
      node = (trie_node_t*)nodebuffer ;
      offsets.child = (uint8_t) (i * sizeof(trie_node_t*)) ;
      TEST(&((trie_node_t**)node)[i] == child_trienode(node, &offsets)) ;
   }

   // TEST digit_trienode
   memset(&offsets, 0, sizeof(offsets)) ;
   for (uint16_t i = 0 ; i < SIZEMAXNODE ; ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 } ;
      node = (trie_node_t*)nodebuffer ;
      offsets.digit = (uint8_t) i ;
      TEST(&nodebuffer[i] == digit_trienode(node, &offsets)) ;
   }

   // TEST isfreechild_trienode
   for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t)(nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
         for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
            uint8_t * nodebuffer[SIZEMAXNODE/sizeof(uint8_t*)] = { 0 } ;
            node = (trie_node_t*)nodebuffer ;
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            memset(child_trienode(node, &offsets), 255, offsets.lenchild*sizeof(trie_node_t*)) ;
            TEST(0 == isfreechild_trienode(node, &offsets)) ;
            if (nrchild && nrchild <= LENCHILDMAX) {
               child_trienode(node, &offsets)[offsets.lenchild-1] = 0 ;
               TEST((offsets.lenchild > 1) == isfreechild_trienode(node, &offsets)) ;
            }
         }
      }
   }

   // TEST prefix_trienode
   memset(&offsets, 0, sizeof(offsets)) ;
   for (unsigned i = 0 ; i < SIZEMAXNODE ; ++i) {
      uint8_t nodebuffer[SIZEMAXNODE] = { 0 } ;
      node = (trie_node_t*)nodebuffer ;
      offsets.prefix = (uint8_t) i ;
      TEST(&nodebuffer[i] == prefix_trienode(node, &offsets)) ;
   }

   // ////
   // group helper

   // TEST newnode_trienode
   for (unsigned i = 0 ; i < header_SIZENODE_MASK ; ++i) {
      header_t header = (header_t) i ;
      uint16_t size   = sizenode_header(header) ;
      TEST(0 == newnode_trienode(&child[i], size)) ;
      TEST(0 != child[i]) ;
      child[i]->header = header ;
      allocsize += size ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }

   // TEST deletenode_trienode
   for (unsigned i = 0 ; i < header_SIZENODE_MASK ; ++i) {
      uint16_t size = sizenode_header((header_t)i) ;
      TEST(0 == deletenode_trienode(&child[i])) ;
      TEST(0 == child[i]) ;
      allocsize -= size ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
      TEST(0 == deletenode_trienode(&child[i])) ;
      TEST(0 == child[i]) ;
      TEST(allocsize == SIZEALLOCATED_MM()) ;
   }

   // TEST newnode_trienode: ERROR
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == newnode_trienode(&child[0], SIZEMAXNODE)) ;
   TEST(0 == child[0]) ;
   TEST(allocsize == SIZEALLOCATED_MM()) ;

   // TEST deletenode_trienode: ERROR
   TEST(0 == newnode_trienode(&child[0], SIZE5NODE)) ;
   child[0]->header = header_SIZE5NODE ;
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL) ;
   TEST(EINVAL == deletenode_trienode(&child[0])) ;
   TEST(0 == child[0]) ;
   TEST(allocsize == SIZEALLOCATED_MM()) ;

   // TEST shrinksize_trienode
   for (uintptr_t i = 0 ; i < lengthof(child) ; ++i) {
      child[i] = (trie_node_t*) (i+1) ;
   }
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 16 ; ++nrchild) {
         for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            for (unsigned isclearallchild = 0 ; isclearallchild <= 1 ; ++isclearallchild) {
               uservalue = (void*)(nrchild * (uintptr_t)100 + 1000u * prefixlen) ;
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
               TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
               trie_nodeoffsets_t oldoff = offsets ;
               TEST(0 == shrinksize_trienode(&node, &offsets)) ;
               TEST(0 == compare_trienodeoffsets(&offsets, &oldoff)) ;  // did nothing
               if (  SIZE1NODE < offsets.nodesize
                     && offsets.child < offsets.nodesize/2) {
                  const unsigned nrchildkept = isclearallchild ? 1 : (offsets.nodesize/2u - offsets.child) / sizeof(trie_node_t*) ;
                  if (isclearallchild) {  // clear all except first child
                     memset(child_trienode(node, &offsets)+1, 0, (unsigned)offsets.nodesize-offsets.child-sizeof(trie_node_t*)) ;
                  } else {                // clear childs  from offset nodesize/2
                     memset(offsets.nodesize/2 + (uint8_t*)node, 0, offsets.nodesize/2) ;
                  }
                  TEST(0 == shrinksize_trienode(&node, &offsets)) ;
                  TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
                  uint8_t diff = 1 ;
                  while (offsets.nodesize != (oldoff.nodesize>>diff)) ++ diff ;
                  TEST(diff == 1 || isclearallchild) ;
                  TEST(offsets.nodesize  == (oldoff.nodesize>>diff)) ;
                  TEST(offsets.lenchild  <  oldoff.lenchild) ;
                  TEST(offsets.lenchild  >= nrchildkept) ;
                  TEST(offsets.header    == ((oldoff.header&~header_SIZENODE_MASK)|((oldoff.header&header_SIZENODE_MASK)-diff))) ;
                  TEST(offsets.prefix    == oldoff.prefix) ;
                  TEST(offsets.digit     == oldoff.digit) ;
                  size_t aligned = (offsets.digit + (unsigned)offsets.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1) ;
                  TEST(offsets.uservalue == aligned) ;
                  TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
                  TEST(offsets.nodesize  >= offsets.child + offsets.lenchild * sizeof(trie_node_t*)) ;
                  if (ischild_header(offsets.header)) {
                     TEST(offsets.nodesize < offsets.child + offsets.lenchild * sizeof(trie_node_t*) + sizeof(trie_node_t*) + 1) ;
                  }
                  TEST(node->header == offsets.header) ;
                  TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, prefixlen)) ;
                  if (isuser) {
                     TEST(uservalue == uservalue_trienodeoffsets(&offsets, node)[0]) ;
                  }
                  if (ischild_header(node->header)) {
                     for (uintptr_t i = 0 ; i < nrchildkept ; ++i) {
                        TEST(digit_trienode(node, &offsets)[i] == i) ;
                        TEST(child_trienode(node, &offsets)[i] == (trie_node_t*)(i+1)) ;
                     }
                     for (uintptr_t i = nrchildkept ; i < offsets.lenchild ; ++i) {
                        TEST(child_trienode(node, &offsets)[i] == 0) ;
                     }
                  }
               }
               memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child) ;
               TEST(0 == delete_trienode(&node)) ;
            }
         }
      }
   }

   // TEST shrinksize_trienode: ERROR
   TEST(0 == new_trienode(&node, &offsets, 15, key.addr, &uservalue, 16, digit, child)) ;
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child) ;
   {
      trie_nodeoffsets_t oldoff = offsets ;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
      TEST(ENOMEM == shrinksize_trienode(&node, &offsets)) ;
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
      TEST(node->header == offsets.header) ;
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff)) ;
   }
   TEST(0 == delete_trienode(&node)) ;

   // TEST expand_trienode
   for (uintptr_t i = 0 ; i < LENCHILDMAX ; ++i) {
      child[i] = (trie_node_t*) (i+1) ;
   }
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= LENCHILDMAX ; ++nrchild) {
         for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            if (!isexpandable_trienodeoffsets(&offsets)) break ; // already max size
            uservalue = (void*)((uintptr_t)nrchild * 1234 + prefixlen) ;
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
            TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
            trie_nodeoffsets_t oldoff = offsets ;
            TEST(0 == expand_trienode(&node, &offsets)) ;
            TEST(allocsize + oldoff.nodesize*2u == SIZEALLOCATED_MM()) ;
            // compare offsets
            if (oldoff.lenchild) {
            oldoff.lenchild  = (uint8_t) ((sizefree_trienodeoffsets(&oldoff) + oldoff.lenchild * (1+sizeof(trie_node_t*)) + oldoff.nodesize)
                               / (1+sizeof(trie_node_t*))) ;
            }
            oldoff.nodesize  = (uint16_t) (oldoff.nodesize * 2) ;
            oldoff.header    = (header_t) (oldoff.header + header_SIZE2NODE) ;
            oldoff.uservalue = (uint8_t) (((unsigned)oldoff.digit + oldoff.lenchild + PTRALIGN-1u) & ~(PTRALIGN-1u)) ;
            oldoff.child     = (uint8_t) (oldoff.uservalue + (isuser ? sizeof(void*) : 0)) ;
            TEST(0 == compare_trienodeoffsets(&oldoff, &offsets)) ;
            // compare node content
            TEST(node->header == offsets.header) ;
            TEST(prefixlen <= 2 || node->prefixlen == prefixlen) ;
            TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, prefixlen)) ;
            TEST(0 == memcmp(digit_trienode(node, &offsets), digit, nrchild)) ;
            TEST(!isuser || uservalue == uservalue_trienodeoffsets(&offsets, node)[0]) ;
            TEST(0 == memcmp(child_trienode(node, &offsets), child, nrchild)) ;
            for (unsigned i = nrchild ; i < offsets.lenchild ; ++i) {
               TEST(0 == child_trienode(node, &offsets)[i]) ;
            }
            memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child) ;
            TEST(0 == deletenode_trienode(&node)) ;
            TEST(allocsize == SIZEALLOCATED_MM()) ;
         }
      }
   }

   // TEST expand_trienode: ERROR
   TEST(0 == new_trienode(&node, &offsets, 15, key.addr, &uservalue, 16, digit, child)) ;
   TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
   memset(child_trienode(node, &offsets), 0, (size_t)offsets.nodesize - offsets.child) ;
   {
      trie_nodeoffsets_t oldoff = offsets ;
      init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
      TEST(ENOMEM == expand_trienode(&node, &offsets)) ;
      // nothing changed
      TEST(allocsize + offsets.nodesize == SIZEALLOCATED_MM()) ;
      TEST(node->header == offsets.header) ;
      TEST(0 == compare_trienodeoffsets(&offsets, &oldoff)) ;
   }
   TEST(0 == delete_trienode(&node)) ;
   memset(child, 0, sizeof(child)) ;

   // TEST tryextendprefix_trienode
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t)(nrchild == 20 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
            uservalue = (void*) ((uintptr_t)100 * nrchild + 11u * prefixlen) ;
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 0, 0, 0, 0, 0, 0)) ;
            }
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
            for (unsigned i = nrchild ; i < offsets.lenchild ; ++i) {
               trie_nodeoffsets_t offsets2 ;
               TEST(0 == new_trienode(&child[i], &offsets2, 0, 0, 0, 0, 0, 0)) ;
               digit_trienode(node, &offsets)[i] = digit[i] ;
               child_trienode(node, &offsets)[i] = child[i] ;
            }
            uint8_t sizegrow = sizegrowprefix_trienodeoffsets(&offsets) ;
            TEST(ENOMEM == tryextendprefix_trienode(node, &offsets, (uint8_t)(sizegrow+1), key.addr+16, key.addr[sizegrow+16])) ;
            trie_nodeoffsets_t offsets2 = offsets ;
            trie_subnode_t *   subnode  = nrchild > LENCHILDMAX ? subnode_trienodeoffsets(&offsets, node)[0] : 0 ;
            for (uint8_t i = 1 ; i <= sizegrow+sizeof(trie_node_t*) ; ++i) {
               bool uselastchild = (i > sizegrow) ;
               if (uselastchild && offsets.lenchild < 2) break ;
               if (uselastchild) {
                  // last child is free
                  TEST(0 == delete_trienode(child_trienode(node, &offsets)+offsets.lenchild-1)) ;
               } else {
                  // all childs in use
               }
               TEST(0 == tryextendprefix_trienode(node, &offsets, i, key.addr+16, key.addr[i+15])) ;
               // offsets
               TEST(offsets.nodesize  == offsets2.nodesize) ;
               TEST(offsets.lenchild  == offsets2.lenchild-(i > sizegrow)) ;
               TEST(offsets.prefix    == HEADERSIZE + (prefixlen+i > 2)) ;
               TEST(offsets.digit     == offsets.prefix + (prefixlen+i)) ;
               TEST(offsets.uservalue == ((PTRALIGN-1u + offsets.digit + offsets.lenchild) & ~(PTRALIGN-1u))) ;
               TEST(offsets.child     == offsets.uservalue + (isuser ? sizeof(void*) : 0)) ;
               unsigned nodesize = offsets.child + offsets.lenchild * sizeof(trie_node_t*) ;
               TEST(offsets.nodesize  >= nodesize && (i != sizegrow || offsets.nodesize == nodesize)) ;
               // node
               TEST(node->header == offsets.header) ;
               TEST((prefixlen+i <= 2) || (prefixlen+i == node->prefixlen)) ;
               TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr+16, i)) ;
               TEST(0 == memcmp(prefix_trienode(node, &offsets)+i, key.addr, prefixlen)) ;
               if (subnode) {
                  TEST(nrchild == 1+digit_trienode(node, &offsets)[0]) ;
                  TEST(subnode == subnode_trienodeoffsets(&offsets, node)[0]) ;
               } else {
                  TEST(0 == memcmp(digit_trienode(node, &offsets), digit, offsets.lenchild)) ;
                  TEST(0 == memcmp(child_trienode(node, &offsets), child, sizeof(trie_node_t*)*offsets.lenchild)) ;
               }
               TEST(!isuser || uservalue == uservalue_trienodeoffsets(&offsets, node)[0]) ;
               shrinkprefixkeeptail_trienode(node, &offsets, prefixlen) ;
               TEST(0 == compare_trienodeoffsets(&offsets, &offsets2)) ;
            }
            memset(child, 0, sizeof(child)) ;
            TEST(0 == delete_trienode(&node)) ;
         }
      }
   }

   // TEST adduservalue_trienode
   for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t)(nrchild == LENCHILDMAX+1 ? 256 : nrchild)) {
      for (uint8_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
         init_trienodeoffsets(&offsets, prefixlen, 0, nrchild) ;
         if (lenprefix_trienodeoffsets(&offsets) != prefixlen) break ; // does not fit in node
         uservalue = (void*) ((uintptr_t)100 * nrchild + 11u * prefixlen) ;
         for (unsigned i = 0 ; i < nrchild ; ++i) {
            TEST(0 == new_trienode(&child[i], &offsets, 0, 0, 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, 0, nrchild, digit, child)) ;
         trie_subnode_t   * subnode = nrchild > LENCHILDMAX ? subnode_trienodeoffsets(&offsets, node)[0] : 0 ;
         trie_nodeoffsets_t oldoff  = offsets ;
         bool issizefree = sizefree_trienodeoffsets(&offsets) >= sizeof(void*) ;
         TEST(subnode || nrchild <= offsets.lenchild) ;
         if (issizefree || isfreechild_trienode(node, &offsets)) {
            adduservalue_trienode(node, &offsets, uservalue) ;
            // compare offsets
            oldoff.lenchild  = (uint8_t)  (oldoff.lenchild - (! issizefree)) ;
            oldoff.header    = (header_t) (oldoff.header | header_USERVALUE) ;
            oldoff.uservalue = (uint8_t)  (((unsigned)oldoff.digit + offsets.lenchild + PTRALIGN-1) & ~(PTRALIGN-1)) ;
            oldoff.child     = (uint8_t)  (oldoff.uservalue + sizeof(void*)) ;
            TEST(0 == compare_trienodeoffsets(&offsets, &oldoff)) ;
            // compare node
            TEST(node->header == offsets.header) ;
            TEST(prefixlen <= 2 || prefixlen == node->prefixlen) ;
            TEST(0 == memcmp(prefix_trienode(node, &offsets), key.addr, lenprefix_trienodeoffsets(&offsets))) ;
            if (subnode) {          // compare digit[0] and subnode
               TEST(nrchild == (uint16_t)1 + digit_trienode(node, &offsets)[0]) ;
               TEST(subnode == subnode_trienodeoffsets(&offsets, node)[0]) ;
            } else if (nrchild) {   // compare digit[] and child[]
               TEST(1 <= offsets.lenchild && nrchild-1 <= offsets.lenchild) ;
               TEST(0 == memcmp(digit_trienode(node, &offsets), digit, nrchild)) ;
               for (unsigned i = 0 ; i < nrchild ; ++i) {
                  TEST(child[i] == child_trienode(node, &offsets)[i]) ;
               }
               for (unsigned i = nrchild ; i < offsets.lenchild ; ++i) {
                  TEST(0 == child_trienode(node, &offsets)[i]) ;
               }
            }
         }
         TEST(0 == delete_trienode(&node)) ;
      }
   }

   // ////
   // group lifetime

   // TEST new_trienode, delete_trienode: single node with childs
   for (uint8_t nrchild = 0 ; nrchild <= 2 ; ++nrchild) {
      for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
         for (uint8_t prefixlen = 0 ; prefixlen <= 16 ; ++prefixlen) {
            // new_trienode
            uservalue = (void*) ((uintptr_t)prefixlen + 200) ;
            expectnode_memblock2 = expectnode_memblock ;
            expect_node_t * expectchilds[2] = { 0 } ;
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 33, key.addr+10, 0, 0, 0, 0)) ;
               TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 33, key.addr+10, 0, 0, 0, 0, 0)) ;
            }
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit+11, child)) ;
            // compare result
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, nrchild, digit+11, expectchilds)) ;
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0)) ;
            // delete_trienode
            TEST(0 == delete_trienode(&node)) ;
            TEST(0 == node) ;
            TEST(0 == delete_trienode(&node)) ;
            TEST(0 == node) ;
         }
      }
   }

   // TEST new_trienode, delete_trienode: prefix chain
   for (uint8_t nrchild = 0 ; nrchild <= 2 ; ++nrchild) {
      for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
         for (uint32_t prefixlen = 0 ; prefixlen < 65536 ; ++prefixlen, prefixlen = (prefixlen < 1024 || prefixlen >= 65530 ? prefixlen : 65530)) {
            uservalue = 0 ;
            expectnode_memblock2 = expectnode_memblock ;
            expect_node_t * expectchilds[2] = { 0 } ;
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               TEST(0 == new_trienode(&child[i], &offsets, 34, key.addr+3, 0, 0, 0, 0)) ;
               TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 34, key.addr+3, 0, 0, 0, 0, 0)) ;
            }
            // new_trienode
            TEST(0 == new_trienode(&node, &offsets, (uint16_t)prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, (uint8_t[]){13,15}, child)) ;
            // compare content of chain
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, (uint16_t)prefixlen, key.addr, isuser, uservalue, nrchild, (uint8_t[]){13,15}, expectchilds)) ;
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0)) ;
            // delete_trienode
            TEST(0 == delete_trienode(&node)) ;
            TEST(0 == node) ;
            TEST(0 == delete_trienode(&node)) ;
            TEST(0 == node) ;
         }
      }
   }

   // TEST new_trienode, delete_trienode: trie_subnode_t
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t prefixlen = 0 ; prefixlen < 16 ; ++prefixlen) {
         expect_node_t * expectchilds[lengthof(child)] = { 0 } ;
         uservalue            = (void*) (isuser + 10u + prefixlen) ;
         expectnode_memblock2 = expectnode_memblock ;
         for (unsigned i = 0 ; i < lengthof(child) ; ++i) {
            TEST(0 == new_trienode(&child[i], &offsets, 3, key.addr, 0, 0, 0, 0)) ;
            TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 3, key.addr, 0, 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, lengthof(child), digit, child)) ;
         // compare result
         TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, prefixlen, key.addr, isuser, uservalue, lengthof(child), digit, expectchilds)) ;
         TEST(0 == compare_expectnode(expectnode, node, &offsets, 0, 0)) ;
         TEST(0 == delete_trienode(&node)) ;
         TEST(0 == node) ;
      }
   }

   // TEST delete_trienode: trie_subnode_t of trie_subnode_t
   TEST(0 == build_subnode_trie(&node, 2, 32)) ;
   TEST(0 != node) ;
   initdecode_trienodeoffsets(&offsets, node) ;
   TEST(1 == issubnode_header(node->header)) ;
   TEST(255 == digit_trienode(node, &offsets)[0]) ;
   TEST(0 == delete_trienode(&node)) ;   // test delete with tree of subnode_t
   TEST(0 == node) ;

   // TEST delete_trienode: subnode == 0
   TEST(0 == build_subnode_trie(&node, 0, 256)) ;
   initdecode_trienodeoffsets(&offsets, node) ;
   TEST(1 == issubnode_header(node->header)) ;
   TEST(255 == digit_trienode(node, &offsets)[0]) ;
   {  // set subnode == 0
      trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0] ;
      subnode_trienodeoffsets(&offsets, node)[0] = 0 ;
      for (unsigned i = 0 ; i < 256 ; ++i) {
         TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i))) ;
      }
      delete_triesubnode(&subnode) ;
   }
   TEST(0 == delete_trienode(&node)) ;
   TEST(0 == node) ;

   // TEST delete_trienode: subnode with all childs == 0
   for (int isdelsub2 = 0 ; isdelsub2 <= 1 ; ++isdelsub2) {
      TEST(0 == build_subnode_trie(&node, 0, 256)) ;
      initdecode_trienodeoffsets(&offsets, node) ;
      TEST(1 == issubnode_header(node->header)) ;
      TEST(255 == digit_trienode(node, &offsets)[0]) ;
      trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0] ;
      for (unsigned i = 0 ; i < 256 ; ++i) {
         TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i))) ;
      }
      for (unsigned i = 0 ; isdelsub2 && i < lengthof(subnode->child) ; ++i) {
         TEST(0 == delete_triesubnode2(&subnode->child[i])) ;
      }
      TEST(0 == delete_trienode(&node)) ;
      TEST(0 == node) ;
   }

   // TEST delete_trienode: subnode with only single child
   for (int isdelsub2 = 0 ; isdelsub2 <= 1 ; ++isdelsub2) {
      for (unsigned ci = 0 ; ci < 256 ; ++ci) {
         TEST(0 == build_subnode_trie(&node, 0, 256)) ;
         initdecode_trienodeoffsets(&offsets, node) ;
         TEST(1 == issubnode_header(node->header)) ;
         TEST(255 == digit_trienode(node, &offsets)[0]) ;
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, node)[0] ;
         for (unsigned i = 0 ; i < lengthof(subnode->child) ; ++i) {
            trie_subnode2_t * subnode2 = subnode->child[i] ;
            bool              isdel    = true ;
            for (unsigned i2 = 0 ; i2 < lengthof(subnode2->child) ; ++i2) {
               if (ci != i*lengthof(subnode2->child) + i2) {
                  TEST(0 == delete_trienode(&subnode2->child[i2])) ;
               } else {
                  isdel = false ;
               }
            }
            if (isdelsub2 && isdel) {
               TEST(0 == delete_triesubnode2(&subnode->child[i])) ;
            }
         }
         TEST(0 == delete_trienode(&node)) ;
         TEST(0 == node) ;
      }
   }

   // TEST newsplit_trienode: no merge with following node
   for (uint8_t splitparam = 0 ; splitparam <= 1 ; ++splitparam) {
      // splitparam == 0: uservalue / splitparam == 1: child param
      for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
         for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
            for (uint8_t prefixlen = 1 ; prefixlen < 16 ; ++prefixlen) {
               for (uint8_t splitprefixlen = 0 ; splitprefixlen < prefixlen ; ++splitprefixlen) {
                  const uint8_t newprefixlen = (uint8_t) (prefixlen-1-splitprefixlen) ;
                  expectnode_memblock2 = expectnode_memblock ;
                  expect_node_t * expectchilds[256] = { 0 } ;
                  for (unsigned i = 0 ; i < nrchild ; ++i) {
                     // make sure that merge with following node is not possible
                     TEST(0 == new_trienode(&child[i], &offsets, 6, key.addr, 0, 0, 0, 0)) ;
                     TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 6, key.addr, 0, 0, 0, 0, 0)) ;
                  }
                  uservalue = (void*)(uintptr_t)(1000+nrchild) ;
                  TEST(0 == new_trienode(&node2, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
                  TEST(0 == new_expectnode(&expectnode2, &expectnode_memblock2, newprefixlen, key.addr+prefixlen-newprefixlen, isuser, uservalue, nrchild, digit, expectchilds)) ;
                  // test newsplit_trienode
                  child[0]  = 0 ;
                  uservalue = (void*)(uintptr_t)(2000+nrchild) ;
                  uint8_t splitdigit = (uint8_t) (key.addr[splitprefixlen] + (splitprefixlen%2 ? -1 : +1)) ;
                  if (splitparam) {
                     trie_nodeoffsets_t offsets2 ;
                     TEST(0 == new_trienode(&child[0], &offsets2, 3, key.addr+60, 0, 0, 0, 0)) ;
                     TEST(0 == new_expectnode(&expectchilds[1], &expectnode_memblock2, 3, key.addr+60, 0, 0, 0, 0, 0)) ;
                  }
                  TEST(0 == newsplit_trienode(&node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0])) ;
                  // compare result
                  uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit } ;
                  expectchilds[0] = expectnode2 ;
                  TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds)) ;
                  TEST(0 == compare_expectnode(expectnode, node, 0, 0, 1)) ;
                  TEST(0 == delete_trienode(&node)) ;
               }
            }
         }
      }
   }

   // TEST newsplit_trienode: merge with following node
   for (uint8_t splitparam = 0 ; splitparam <= 1 ; ++splitparam) {
      // splitparam == 0: uservalue / splitparam == 1: child param
      for (uint8_t splitprefixlen = 0 ; splitprefixlen < 16 ; ++splitprefixlen) {
         for (uint8_t prefixlen = (uint8_t)(splitprefixlen+1) ; prefixlen <= (splitprefixlen+sizeof(trie_node_t*)) ; ++prefixlen) {
            const uint8_t mergelen = (uint8_t) (prefixlen-splitprefixlen) ;
            expectnode_memblock2 = expectnode_memblock ;
            expect_node_t * expectchilds[2] = { 0 } ;
            // make sure that merge with following node is possible
            uservalue = (void*)(uintptr_t)(splitprefixlen*100+prefixlen) ;
            TEST(0 == new_trienode(&child[0], &offsets, 0, 0, &uservalue, 0, 0, 0)) ;
            TEST(0 == new_trienode(&child[1], &offsets, 0, 0, &uservalue, 0, 0, 0)) ;
            TEST(0 == new_trienode(&child[2], &offsets, 3, key.addr+prefixlen+1, 0, 2, digit, child)) ;
            child[3] = 0 ;
            TEST(0 == delete_trienode(child_trienode(child[2], &offsets)+1)) ; // delete child[1]
            TEST(0 == new_expectnode(&expectchilds[0], &expectnode_memblock2, 0, 0, 1, uservalue, 0, 0, 0)) ;
            TEST(0 == new_expectnode(&expectnode2, &expectnode_memblock2, (uint16_t)(3+mergelen), key.addr+prefixlen+1-mergelen, 0, 0, 1, digit, expectchilds)) ;
            // child[3] is empty ==> insert of child is possible ==> merge is possible
            TEST(0 == new_trienode(&node2, &offsets, prefixlen, key.addr, 0, 2, key.addr+prefixlen, child+2)) ;
            // test newsplit_trienode
            child[0]  = 0 ;
            uservalue = (void*)(uintptr_t)(splitprefixlen*120+prefixlen) ;
            uint8_t splitdigit = (uint8_t) (key.addr[splitprefixlen] + (prefixlen%2 ? -1 : +1)) ;
            if (splitparam) {
               trie_nodeoffsets_t offsets2 ;
               TEST(0 == new_trienode(&child[0], &offsets2, 3, key.addr+60, 0, 0, 0, 0)) ;
               TEST(0 == new_expectnode(&expectchilds[1], &expectnode_memblock2, 3, key.addr+60, 0, 0, 0, 0, 0)) ;
            }
            TEST(0 == newsplit_trienode(&node, node2, &offsets, splitprefixlen, uservalue, splitdigit, child[0])) ;
            // compare result
            TEST(node == node2) ; // merged
            uint8_t digit2[2] = { key.addr[splitprefixlen], splitdigit } ;
            expectchilds[0] = expectnode2 ;
            TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, splitprefixlen, key.addr, !splitparam, uservalue, (uint16_t)(1+splitparam), digit2, expectchilds)) ;
            TEST(0 == compare_expectnode(expectnode, node, &offsets, 1, 0)) ;
            TEST(0 == delete_trienode(&node)) ;
         }
      }
   }

   // TEST new_trienode, delete_trienode: ERROR
   memset(child, 0, sizeof(child)) ;
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child)) ;
   TEST(0 == node) ;
   TEST(0 == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child)) ;
   init_testerrortimer(&s_trie_errtimer, 1, EINVAL) ;
   TEST(EINVAL == delete_trienode(&node)) ;
   TEST(0 == node) ;

   // store log
   char * logbuffer ;
   {
      uint8_t *logbuffer2 ;
      size_t   logsize ;
      GETBUFFER_ERRLOG(&logbuffer2, &logsize) ;
      logbuffer = strdup((char*)logbuffer2) ;
      TEST(logbuffer) ;
   }

   // TEST new_trienode: ERROR (no log cause of overflow)
   CLEARBUFFER_ERRLOG() ;
   for (uint32_t errcount = 1 ; errcount < 50 ; ++errcount) {
      init_testerrortimer(&s_trie_errtimer, errcount, ENOMEM) ;
      TEST(ENOMEM == new_trienode(&node, &offsets, 20000, key.addr, 0, 256, digit, child)) ;
      TEST(0 == node) ;
   }

   // TEST delete_trienode: ERROR (no log cause of overflow)
   CLEARBUFFER_ERRLOG() ;
   for (int issubnode = 0 ; issubnode < 2 ; ++issubnode) {
      for (uint32_t errcount = 1 ; errcount < 3 ; ++errcount) {
         if (issubnode) {
            TEST(0 == build_subnode_trie(&node, 0, 1)) ;
         } else {
            TEST(0 == new_trienode(&node, &offsets, 2000, key.addr, 0, 0, 0, 0)) ;
         }
         TEST(0 != node) ;
         init_testerrortimer(&s_trie_errtimer, errcount, EINVAL) ;
         TEST(EINVAL == delete_trienode(&node)) ;
         TEST(0 == node) ;
      }
   }

   // restore log
   CLEARBUFFER_ERRLOG() ;
   PRINTF_ERRLOG("%s", logbuffer) ;
   free(logbuffer) ;

   // ////
   // group change

   // TEST convertchild2sub_trienode
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t prefixlen = 0 ; prefixlen < 8 ; ++prefixlen) {
         for (uint8_t nrchild = 1 ; nrchild < LENCHILDMAX ; ++nrchild) {
            // test memory
            init_trienodeoffsets(&offsets, prefixlen, isuser, nrchild) ;
            if (lenprefix_trienodeoffsets(&offsets) != prefixlen) break ;  // prefix does not fit
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               TEST(0 == new_trienode(&node, &offsets, 3, (const uint8_t*)"123", 0, 0, 0, 0)) ;
               digit[i] = (uint8_t) (prefixlen * 14u + i) ;
               child[i] = node ;
            }
            uservalue = (void*)(uintptr_t)(10+prefixlen) ;
            TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
            trie_nodeoffsets_t oldoff = offsets ;
            allocsize = SIZEALLOCATED_MM() ;
            TEST(0 == convertchild2sub_trienode(&node, &offsets)) ;
            // test offsets
            TEST(offsets.nodesize  <= oldoff.nodesize) ;
            TEST(offsets.lenchild  == 0) ;
            TEST(offsets.header    == ((oldoff.header & ~(header_CHILD|header_SIZENODE_MASK)) | (header_SUBNODE|(offsets.header&header_SIZENODE_MASK)))) ;
            TEST(offsets.prefix    == oldoff.prefix) ;
            TEST(offsets.digit     == oldoff.digit) ;
            size_t aligned = (offsets.digit + 1u + PTRALIGN-1) & ~(PTRALIGN-1) ;
            TEST(offsets.uservalue == aligned) ;
            TEST(offsets.child     == offsets.uservalue  + isuser * sizeof(void*)) ;
            size_t   newsize = offsets.child + sizeof(trie_subnode_t*) ;
            header_t headsize ;
            if      (newsize <= SIZE1NODE) newsize = SIZE1NODE, headsize = header_SIZE1NODE ;
            else if (newsize <= SIZE2NODE) newsize = SIZE2NODE, headsize = header_SIZE2NODE ;
            else if (newsize <= SIZE3NODE) newsize = SIZE3NODE, headsize = header_SIZE3NODE ;
            else if (newsize <= SIZE4NODE) newsize = SIZE4NODE, headsize = header_SIZE4NODE ;
            else                           newsize = SIZE5NODE, headsize = header_SIZE5NODE ;
            TEST(headsize == (offsets.header&header_SIZENODE_MASK)) ;
            // test node
            TEST(offsets.header == node->header) ;
            TEST(nrchild        == 1u + digit_trienode(node, &offsets)[0]) ;
            if (isuser) {
               TEST(uservalue == uservalue_trienodeoffsets(&offsets, node)[0]) ;
            }
            // test content of subnode
            trie_subnode2_t * subnode2 ;
            trie_subnode_t *  subnode = subnode_trienodeoffsets(&offsets, node)[0] ;
            bool              issubnode[lengthof(subnode->child)] = { 0 } ;
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               issubnode[digit[i]/lengthof(subnode2->child)] = 1 ;
            }
            unsigned nrsubnode = 0 ;
            for (unsigned i = 0 ; i < lengthof(subnode->child) ; ++i) {
               TEST(issubnode[i] == (0 != subnode->child[i])) ;
               nrsubnode += issubnode[i] ;
            }
            for (unsigned i = 0 ; i < nrchild ; ++i) {
               uint8_t d = digit[i] ;
               subnode2  = subnode->child[d/lengthof(subnode2->child)] ;
               TEST(child[i] == child_triesubnode2(subnode2, d)[0]) ;
               digit[i] = (uint8_t) i ;   // reset
               child[i] = 0 ;             // reset
            }
            TEST(SIZEALLOCATED_MM() == allocsize - oldoff.nodesize + newsize + sizeof(trie_subnode_t) + nrsubnode * sizeof(trie_subnode2_t)) ;
            TEST(0 == delete_trienode(&node)) ;
         }
      }
   }

   // TEST convertchild2sub_trienode: EINVAL
   TEST(0 == new_trienode(&node, &offsets, 1, key.addr, &uservalue, 0, 0, 0)) ;
   TEST(0 == ischild_header(offsets.header)) ;
   TEST(EINVAL == convertchild2sub_trienode(&node, &offsets)) ;  // no childs
   TEST(0 == delete_trienode(&node)) ;
   TEST(0 == new_trienode(&node, &offsets, 1, key.addr, &uservalue, 2, digit, child)) ;
   TEST(1 == ischild_header(offsets.header)) ;
   TEST(EINVAL == convertchild2sub_trienode(&node, &offsets)) ;  // all child pointer set to 0
   TEST(0 == delete_trienode(&node)) ;

   // TEST convertchild2sub_trienode: ENOMEM (subnode creation fails)
   allocsize = SIZEALLOCATED_MM() ;
   for (unsigned i = 0 ; i < 16 ; ++i) {
      TEST(0 == new_trienode(&child[i], &offsets, 0, 0, &uservalue, 0, 0, 0)) ;
   }
   TEST(0 == new_trienode(&node, &offsets, 4, key.addr, &uservalue, 16, digit, child)) ;
   // save old state
   size_t nodesize = SIZEALLOCATED_MM() - allocsize ;
   memcpy(key.addr, &offsets, sizeof(offsets)) ;
   memcpy(key.addr + sizeof(offsets), node, offsets.nodesize) ;
   init_testerrortimer(&s_trie_errtimer, 1, ENOMEM) ;
   TEST(ENOMEM == convertchild2sub_trienode(&node, &offsets)) ;
   // test nothing changed
   TEST(allocsize+nodesize == SIZEALLOCATED_MM()) ;
   TEST(0 == memcmp(key.addr, &offsets, sizeof(offsets))) ;
   TEST(0 == memcmp(key.addr + sizeof(offsets), node, offsets.nodesize)) ;

   // TEST convertchild2sub_trienode: ENOMEM ignored (shrinksize_trienode fails)
   init_testerrortimer(&s_trie_errtimer, 4, ENOMEM) ;
   TEST(0 == convertchild2sub_trienode(&node, &offsets)) ;
   nodesize += sizeof(trie_subnode_t) + 2 * sizeof(trie_subnode2_t) ;
   TEST(1 == issubnode_header(offsets.header)) ;      // offsets changed
   TEST(node->header == offsets.header) ;            // also node
   TEST(allocsize+nodesize == SIZEALLOCATED_MM()) ;   // but shrinksize_trienode failed
   TEST(0 == delete_trienode(&node)) ;
   TEST(allocsize == SIZEALLOCATED_MM()) ;

   // TEST shrinkprefixkeeptail_trienode: normal (precondition not violated)
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1 ; prefixlen < 16 ; ++prefixlen) {
            for (uint8_t newprefixlen = 0 ; newprefixlen < prefixlen ; ++newprefixlen) {
               expectnode_memblock2 = expectnode_memblock ;
               expect_node_t * expectchilds[256] = { 0 } ;
               for (unsigned i = 0 ; i < 16 && i < nrchild ; ++i) {
                  TEST(0 == new_trienode(&child[i], &offsets, 5, key.addr+3, 0, 0, 0, 0)) ;
                  TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 5, key.addr+3, 0, 0, 0, 0, 0)) ;
               }
               uservalue = (void*)(uintptr_t)(1000+nrchild) ;
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
               // normal case
               shrinkprefixkeeptail_trienode(node, &offsets, newprefixlen) ;
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr+prefixlen-newprefixlen, isuser, uservalue, nrchild, digit, expectchilds)) ;
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0)) ;
               TEST(0 == delete_trienode(&node)) ;
               memset(child, 0, sizeof(child)) ;
            }
         }
      }
   }

   // TEST shrinkprefixkeephead_trienode
   for (uint8_t isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t nrchild = 0 ; nrchild <= 256 ; ++nrchild, nrchild = (uint16_t) (nrchild == 17 ? 256 : nrchild)) {
         for (uint8_t prefixlen = 1 ; prefixlen < 16 ; ++prefixlen) {
            for (uint8_t newprefixlen = 0 ; newprefixlen < prefixlen ; ++newprefixlen) {
               expectnode_memblock2 = expectnode_memblock ;
               expect_node_t * expectchilds[256] = { 0 } ;
               for (unsigned i = 0 ; i < 16 && i < nrchild ; ++i) {
                  TEST(0 == new_trienode(&child[i], &offsets, 5, key.addr+3, 0, 0, 0, 0)) ;
                  TEST(0 == new_expectnode(&expectchilds[i], &expectnode_memblock2, 5, key.addr+3, 0, 0, 0, 0, 0)) ;
               }
               uservalue = (void*)((uintptr_t)123+prefixlen) ;
               TEST(0 == new_trienode(&node, &offsets, prefixlen, key.addr, isuser ? &uservalue : 0, nrchild, digit, child)) ;
               // normal case
               shrinkprefixkeephead_trienode(node, &offsets, newprefixlen) ;
               // compare result
               TEST(0 == new_expectnode(&expectnode, &expectnode_memblock2, newprefixlen, key.addr, isuser, uservalue, nrchild, digit, expectchilds)) ;
               TEST(0 == compare_expectnode(expectnode, node, &offsets, 2, 0)) ;
               TEST(0 == delete_trienode(&node)) ;
               memset(child, 0, sizeof(child)) ;
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
   TEST(0 == FREE_MM(&expectnode_memblock)) ;
   TEST(0 == FREE_MM(&key)) ;

   return 0 ;
ONABORT:
   FREE_MM(&expectnode_memblock) ;
   FREE_MM(&key) ;
   return EINVAL ;
}

static int test_initfree(void)
{
   trie_t               trie = trie_INIT_FREEABLE ;
   trie_nodeoffsets_t   offsets ;
   uint8_t              digit[256] ;

   // prepare
   for (unsigned i = 0 ; i < lengthof(digit) ; ++i) {
      digit[i] = (uint8_t) i ;
   }

   // TEST trie_INIT_FREEABLE
   TEST(0 == trie.root) ;

   // TEST trie_INIT
   trie = (trie_t) trie_INIT ;
   TEST(0 == trie.root) ;

   // TEST init_trie
   memset(&trie, 255 ,sizeof(trie)) ;
   TEST(0 == init_trie(&trie)) ;
   TEST(0 == trie.root) ;

   // TEST free_trie
   trie_node_t *  topchilds[16]   = { 0 } ;
   trie_node_t *  leafchilds[256] = { 0 } ;
   for (unsigned ti = 0 ; ti < lengthof(topchilds) ; ++ti) {
      void * uservalue = (void*) ti ;
      for (unsigned li = 0 ; li < lengthof(leafchilds) ; ++li) {
         TEST(0 == new_trienode(&leafchilds[li], &offsets, 3, (const uint8_t*)"123", &uservalue, 0, 0, 0)) ;
      }
      TEST(0 == new_trienode(&topchilds[ti], &offsets, 0, 0, 0, 256, digit, leafchilds)) ;
      for (unsigned ci = 0 ; ci < 10 ; ++ci) {
         trie_node_t * childs[1] = { topchilds[ti] } ;
         TEST(0 == new_trienode(&topchilds[ti], &offsets, 5, (const uint8_t*)"12345", &uservalue, 1, digit, childs)) ;
      }
   }
   TEST(0 == new_trienode(&trie.root, &offsets, 4, (const uint8_t*)"root", 0, lengthof(topchilds), digit, topchilds)) ;
   TEST(0 == free_trie(&trie)) ;
   TEST(0 == trie.root) ;
   TEST(0 == free_trie(&trie)) ;
   TEST(0 == trie.root) ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

static int test_query(void)
{
   trie_t             trie = trie_INIT ;
   memblock_t         key  = memblock_INIT_FREEABLE ;
   void             * uservalue ;
   trie_findresult_t  findresult ;
   trie_findresult_t  findresult2 ;
   trie_node_t      * childs[256] = { 0 } ;
   trie_nodeoffsets_t offsets ;

   // prepare
   memset(&offsets, 0, sizeof(offsets)) ;
   memset(&findresult, 0, sizeof(findresult)) ;
   memset(&findresult2, 0, sizeof(findresult2)) ;
   TEST(0 == ALLOC_MM(1024, &key)) ;
   for (unsigned i = 0 ; i < key.size ; ++i) {
         key.addr[i] = (uint8_t) i ;
   }

   // TEST findnode_trie, at_trie: empty trie
   for (uint16_t keylen = 0 ; keylen <= 16 ; ++keylen) {
      TEST(ESRCH == findnode_trie(&trie, keylen, key.addr, &findresult)) ;
      TEST(findresult.parent       == 0)
      TEST(findresult.parent_child == &trie.root) ;
      TEST(findresult.node         == trie.root) ;
      TEST(findresult.chain_parent == 0) ;
      TEST(findresult.chain_child  == &trie.root) ;
      TEST(findresult.matchkeylen  == 0) ;
      TEST(findresult.is_split     == 0) ;
      TEST(0 == at_trie(&trie, (uint16_t)key.size, key.addr)) ;
   }

   // TEST findnode_trie, at_trie: single node / node chain (root is chain_parent)
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t keylen = 0 ; keylen <= key.size ; keylen = (uint16_t) (keylen <= 16 ? keylen+1 : 2*keylen)) {
         uservalue = (void*) (11*keylen + 13*isuser) ;
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, 0, 0, 0)) ;
         findresult.parent = (void*)2 ; findresult.parent_child = 0 ; findresult.node = 0 ;
         TEST(0 == findnode_trie(&trie, keylen, key.addr, &findresult)) ;
         TEST(findresult.parent       != (void*)2)
         TEST(findresult.parent_child != 0) ;
         TEST(findresult.node         != 0) ;
         TEST(isuser == isuservalue_header(findresult.node->header))
         findresult2 = (trie_findresult_t) {
            .parent = findresult.parent, .parent_child = findresult.parent_child, .node = findresult.parent_child[0],
            .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = keylen
         } ;
         initdecode_trienodeoffsets(&findresult2.offsets, findresult.node) ;
         TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
         if (isuser) {
            TEST(at_trie(&trie, keylen, key.addr)[0] == uservalue) ;
         } else {
            TEST(at_trie(&trie, keylen, key.addr)    == 0) ;
         }
         if (keylen < key.size) {
            TEST(0 == at_trie(&trie, (uint16_t) (keylen+1), key.addr)) ;
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) (keylen+1), key.addr, &findresult)) ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: node with childs followed (begin of chain_parent)
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t keylen = 0 ; keylen <= 3 ; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser) ;
         for (unsigned i = 0 ; i < LENCHILDMAX-1 ; ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, LENCHILDMAX-1, key.addr, childs)) ;
         for (unsigned i = 0 ; i < LENCHILDMAX-1 ; ++i) {
            uint8_t            skey[2*keylen+1] ;
            memcpy(skey, key.addr, sizeof(skey)) ;
            skey[keylen] = key.addr[i] ;
            // test find
            TEST(0 == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult)) ;
            findresult2 = (trie_findresult_t) {
               .parent = trie.root, .parent_child = child_trienode(trie.root, &offsets)+i,
               .node = childs[i], .chain_parent = trie.root, .chain_child = child_trienode(trie.root, &offsets)+i,
               .matchkeylen = (uint16_t) sizeof(skey), .is_split = 0
            } ;
            initdecode_trienodeoffsets(&findresult2.offsets, findresult.node) ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
            if (isuser) {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == uservalue_trienodeoffsets(&findresult.offsets, findresult.node)) ;
            } else {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == 0) ;
            }
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: split node
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      findresult2 = (trie_findresult_t) {
         .parent = 0, .parent_child = &trie.root, .node = trie.root,
         .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = 0, .is_split = 1
      } ;
      for (uint16_t keylen = 1 ; keylen <= 16 ; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser) ;
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, 0, 0, 0)) ;
         findresult2.node = trie.root ;
         initdecode_trienodeoffsets(&findresult2.offsets, trie.root) ;
         for (uint8_t splitlen = 0 ; splitlen < keylen ; ++splitlen) {
            // keysize < prefixlen in node
            TEST(0 == at_trie(&trie, splitlen, key.addr)) ;
            TEST(ESRCH == findnode_trie(&trie, splitlen, key.addr, &findresult)) ;
            findresult2.splitlen = splitlen ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;

            // keysize >= prefixlen but key does not match
            uint8_t oldkey = key.addr[splitlen] ;
            key.addr[splitlen] = (uint8_t) (oldkey + 1) ;
            TEST(0 == at_trie(&trie, keylen, key.addr)) ;
            TEST(ESRCH == findnode_trie(&trie, keylen, key.addr, &findresult)) ;
            findresult2.splitlen = splitlen ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
            key.addr[splitlen] = oldkey ;
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: node with childs not followed
   memset(&findresult, 0, sizeof(findresult)) ;
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      uint8_t digits[LENCHILDMAX-1] ;
      for (unsigned i = 0 ; i < sizeof(digits) ; ++i) {
         digits[i] = (uint8_t) (5+5*i) ;
      }
      for (uint16_t keylen = 0 ; keylen <= 3 ; ++ keylen) {
         uint8_t skey[keylen+1] ;
         memcpy(skey, key.addr, sizeof(skey)) ;
         uservalue = (void*) (7*keylen + 23*isuser) ;
         for (unsigned i = 0 ; i < sizeof(digits) ; ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, 1, key.addr+10, 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, sizeof(digits), digits, childs)) ;
         findresult2 = (trie_findresult_t) {
               .parent = 0, .parent_child = &trie.root,
               .node = trie.root, .chain_parent = 0, .chain_child = &trie.root,
               .matchkeylen = keylen, .is_split = 0
         } ;
         initdecode_trienodeoffsets(&findresult2.offsets, trie.root) ;
         for (unsigned i = 0 ; i < LENCHILDMAX-1 ; ++i) {
            skey[keylen] = (uint8_t) (digits[i] + 1) ;
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult)) ;
            findresult2.childindex = (uint8_t) (i+1) ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
            skey[keylen] = (uint8_t) (digits[i] - 2) ;
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult)) ;
            findresult2.childindex = (uint8_t) i ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: node with subnode followed (begin of chain_parent)
   memset(&findresult, 0, sizeof(findresult)) ;
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t keylen = 0 ; keylen <= 3 ; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser) ;
         for (unsigned i = 0 ; i < lengthof(childs) ; ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, lengthof(childs), key.addr, childs)) ;
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, trie.root)[0] ;
         for (unsigned i = 0 ; i < lengthof(childs) ; ++i) {
            uint8_t            skey[2*keylen+1] ;
            memcpy(skey, key.addr, sizeof(skey)) ;
            skey[keylen] = key.addr[i] ;
            // test find no split
            TEST(0 == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult)) ;
            findresult2 = (trie_findresult_t) {
               .parent = trie.root, .parent_child = child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i),
               .node = childs[i], .chain_parent = trie.root, .chain_child = child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i),
               .matchkeylen = (uint16_t) sizeof(skey), .is_split = 0
            } ;
            initdecode_trienodeoffsets(&findresult2.offsets, findresult.node) ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
            if (isuser) {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == uservalue_trienodeoffsets(&findresult.offsets, findresult.node)) ;
            } else {
               TEST(at_trie(&trie, (uint16_t)sizeof(skey), skey) == 0) ;
            }
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: node with subnode not followed
   for (int isuser = 0 ; isuser <= 1 ; ++isuser) {
      for (uint16_t keylen = 0 ; keylen <= 3 ; ++ keylen) {
         uservalue = (void*) (7*keylen + 23*isuser) ;
         for (unsigned i = 0 ; i < lengthof(childs) ; ++i) {
            TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+keylen+1, isuser ? &uservalue : 0, 0, 0, 0)) ;
         }
         TEST(0 == new_trienode(&trie.root, &offsets, keylen, key.addr, isuser ? &uservalue : 0, lengthof(childs), key.addr, childs)) ;
         trie_subnode_t * subnode = subnode_trienodeoffsets(&offsets, trie.root)[0] ;
         for (unsigned i = 0 ; i < lengthof(childs) ; ++i) {
            uint8_t            skey[2*keylen+1] ;
            memcpy(skey, key.addr, sizeof(skey)) ;
            skey[keylen] = key.addr[i] ;
            TEST(0 == delete_trienode(child_triesubnode2(child_triesubnode(subnode, (uint8_t)i)[0], (uint8_t)i))) ;
            if (0 == child_triesubnode(subnode, (uint8_t)i)[0]->child[lengthof(((trie_subnode2_t*)0)->child)-1]) {
               TEST(0 == delete_triesubnode2(child_triesubnode(subnode, (uint8_t)i))) ;
            }
            TEST(0 == at_trie(&trie, (uint16_t)sizeof(skey), skey)) ;
            TEST(ESRCH == findnode_trie(&trie, (uint16_t) sizeof(skey), skey, &findresult)) ;
            findresult2 = (trie_findresult_t) {
                  .parent = 0, .parent_child = &trie.root, .node = trie.root,
                  .chain_parent = 0, .chain_child = &trie.root, .matchkeylen = keylen
            } ;
            initdecode_trienodeoffsets(&findresult2.offsets, trie.root) ;
            TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
         }
         TEST(0 == free_trie(&trie)) ;
      }
   }

   // TEST findnode_trie, at_trie: chain of nodes with uservalue (uservalue ==> begin of chain_parent)
   for (uint16_t keylen = 1 ; keylen <= 6 ; ++ keylen) {
      for (uintptr_t i = 0 ; i < 4 ; ++i) {
         uservalue = (void*)i ;
         TEST(0 == new_trienode(&childs[i], &offsets, keylen, key.addr+(3u-i)*(keylen+1u), &uservalue, (uint16_t) (i != 0), key.addr+(4u-i)*(keylen+1u)-1u, &childs[i-1])) ;
      }
      trie.root = childs[3] ;
      for (uintptr_t i = 0 ; i < 4 ; ++i) {
         uint16_t skeylen = (uint16_t)((i+1)*keylen+i) ;
         TEST((void*)(3-i) == at_trie(&trie, skeylen, key.addr)[0]) ;
         TEST(0 == findnode_trie(&trie, skeylen, key.addr, &findresult)) ;
         if (i) initdecode_trienodeoffsets(&offsets, childs[4-i]) ;
         findresult2 = (trie_findresult_t) {
               .parent = i ? childs[4-i] : 0, .parent_child = i ? child_trienode(childs[4-i], &offsets) : &trie.root, .node = childs[3-i],
               .chain_parent = i ? childs[4-i] : 0, .chain_child = i ? child_trienode(childs[4-i], &offsets) : &trie.root, .matchkeylen = skeylen
         } ;
         initdecode_trienodeoffsets(&findresult2.offsets, childs[3-i]) ;
         TEST(0 == memcmp(&findresult, &findresult2, sizeof(findresult))) ;
      }
      TEST(0 == free_trie(&trie)) ;
   }

   // unprepare
   TEST(0 == free_trie(&trie)) ;
   TEST(0 == FREE_MM(&key)) ;

   return 0 ;
ONABORT:
   FREE_MM(&key) ;
   free_trie(&trie) ;
   return EINVAL ;
}

static int test_insertremove(void)
{
   trie_t            trie = trie_INIT ;
   memblock_t        key  = memblock_INIT_FREEABLE ;
   memblock_t        expectnode_memblock = memblock_INIT_FREEABLE ;
   memblock_t        memblock ;
   expect_node_t *   expectnode ;
   expect_node_t *   expectnode2 ;
   void *            uservalue ;

   // prepare
   TEST(0 == ALLOC_MM(1024*1024, &expectnode_memblock)) ;
   TEST(0 == ALLOC_MM(65536, &key)) ;
   for (unsigned i = 0 ; i < 65536 ; ++i) {
      key.addr[i] = (uint8_t) (11*i) ;
   }

   // TEST insert_trie, remove_trie: empty trie <-> (single node or chain of nodes storing prefix)
   for (uint32_t keylen= 0 ; keylen <= 65535 ; ++keylen, keylen= (keylen== 10*SIZEMAXNODE ? 65400 : keylen)) {
      // insert_trie
      uservalue = (void*) (2+keylen) ;
      memblock = expectnode_memblock ;
      TEST(0 == insert_trie(&trie, (uint16_t)keylen, key.addr, uservalue)) ;
      // compare expected result
      TEST(0 != trie.root) ;
      TEST(0 == new_expectnode(&expectnode, &memblock, (uint16_t)keylen, key.addr, true, uservalue, 0, 0, 0)) ;
      TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0, 0)) ;
      // remove_trie
      uservalue = 0 ;
      TEST(0 == remove_trie(&trie, (uint16_t)keylen, key.addr, &uservalue)) ;
      // compare expected result
      TEST((void*)(2+keylen) == uservalue) ; // out value
      TEST(0 == trie.root) ;                 // node freed
   }

#if 0 // TODO: remove line
   // TEST insert_trie, remove_trie: split node, merge node
   for (uint16_t keylen = 1 ; keylen <= SIZEMAXNODE-HEADERSIZE-sizeof(void*)-1 ; ++keylen) {
      for (uint16_t splitoffset = 0 ; splitoffset < keylen ; ++splitoffset) {
         memblock  = expectnode_memblock ;
         uservalue = (void*) (1u + (uintptr_t)splitoffset) ;
         TEST(0 == insert_trie(&trie, keylen, key.addr, uservalue)) ;
         // insert_trie: split
         uservalue = (void*) ((uintptr_t)splitoffset) ;
         TEST(0 == insert_trie(&trie, splitoffset, key.addr, uservalue)) ;
         // compare expected result
         TEST(0 == new_expectnode(&expectnode2, &memblock, (uint16_t)(keylen-splitoffset-1), key.addr+splitoffset+1, true, (void*)(1+(uintptr_t)splitoffset), 0, 0, 0)) ;
         TEST(0 == new_expectnode(&expectnode, &memblock, splitoffset, key.addr, true, uservalue, 1, key.addr+splitoffset, &expectnode)) ;
         TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0)) ;
         // remove_trie: merge with following node
         uservalue = 0 ;
         TEST(0 == remove_trie(&trie, splitoffset, key.addr, &uservalue)) ;
         // compare expected result
         TEST(0 == new_expectnode(&expectnode,  &memblock, keylen, key.addr, true, (void*)(1+(uintptr_t)splitoffset), 0, 0, 0)) ;
         TEST(splitoffset == (uintptr_t)uservalue) ;                 // out param
         TEST(0 == compare_expectnode(expectnode, trie.root, 0, 0)) ;// trie structure
         TEST(0 == free_trie(&trie)) ;
      }
   }
#endif // TODO: remove line

   // TEST change into subnode

   // TEST change from subnode into node with childs (add digit[0] which contains number of childs -1: 0 => 1 child ... 255 => 256 childs)

   // TEST prefix chain

   // TEST merge node

   // unprepare
   TEST(0 == free_trie(&trie)) ;
   TEST(0 == FREE_MM(&key)) ;
   TEST(0 == FREE_MM(&expectnode_memblock)) ;

   return 0 ;
ONABORT:
   free_trie(&trie) ;
   FREE_MM(&key) ;
   FREE_MM(&expectnode_memblock) ;
   return EINVAL ;
}

int unittest_ds_inmem_trie()
{
   // sub types
   if (test_header_enum())       goto ONABORT ;
   if (test_header())            goto ONABORT ;
   if (test_triesubnode2())      goto ONABORT ;
   if (test_triesubnode())       goto ONABORT ;
   if (test_trienodeoffset())    goto ONABORT ;
   if (test_trienode())          goto ONABORT ;
   // trie_t
   if (test_initfree())          goto ONABORT ;
   if (test_query())             goto ONABORT ;
   if (test_insertremove())      goto ONABORT ;
   // TODO: if (test_iterator())          goto ONABORT ;

   return 0 ;
ONABORT:
   return EINVAL ;
}

#endif
