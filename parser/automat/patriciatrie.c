/* title: Patricia-Trie impl
   Implements <Patricia-Trie>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2012 Jörg Seebohn

   file: C-kern/api/ds/inmem/patriciatrie.h
    Header file <Patricia-Trie>.

   file: C-kern/ds/inmem/patriciatrie.c
    Implementation file <Patricia-Trie impl>.
*/

#include "config.h"
#include "patriciatrie.h"


// section: patriciatrie_t

// group: helper

static inline void * cast_object(patriciatrie_node_t* node, size_t nodeoffset)
{
   return ((uint8_t*)node - nodeoffset);
}

// group: lifetime

int free_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f)
{
   int err = removenodes_patriciatrie(tree, delete_f);

   tree->keyadapt = (getkey_adapter_t) getkey_adapter_INIT(0,0);

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: search

/* define: GETBIT
 * Returns the bit value at bitoffset. Returns (key[0]&0x80) if bitoffset is 0 and (key[0]&0x40) if it is 1 and so on.
 * Every string has a virtual end marker of 0xFF.
 * If bitoffset/8 equals therefore length a value of 1 is returned.
 * If bitoffset is beyond string length and beyond virtual end marker a value of 0 is returned. */
#define GETBIT(key, length, bitoffset) \
   ( __extension__ ({                                       \
         size_t _byteoffset = (bitoffset) / 8;              \
         (_byteoffset < (length))                           \
         ? (0x80>>((bitoffset)%8)) & ((key)[_byteoffset])   \
         : ((_byteoffset == (length)) ? 1 : 0);             \
   }))

/* function: first_different_bit
 * Determines the first bit which differs in key1 and key2. Virtual end markers of 0xFF
 * at end of each string are considered. If both keys are equal the functions returns
 * the error code EINVAL. On success 0 is returned and *bit_offset contains the bit index
 * of the first differing bit beginning with 0. */
static int first_different_bit(
       const uint8_t * const key1,
       size_t        const length1,
       const uint8_t * const key2,
       size_t        const length2,
/*out*/size_t *      key2_bit_offset,
/*out*/uint8_t*      key2_bit_value)
{
   size_t length = (length1 < length2 ? length1 : length2);
   const uint8_t * k1 = key1;
   const uint8_t * k2 = key2;
   while (length && *k1 == *k2) {
      -- length;
      ++ k1;
      ++ k2;
   }

   uint8_t b1;
   uint8_t b2;
   size_t  result;
   if (length) {
      b1 = *k1;
      b2 = *k2;
      result = 8 * (size_t)(k1 - key1);
   } else {
      if (length1 == length2) return EINVAL;   // both keys are equal
      if (length1 < length2) {
         length = length2 - length1;
         b1 = 0xFF/*end marker of k1*/;
         b2 = *k2;
         if (b2 == b1) {
            b1 = 0x00; // artificial extension of k1
            do {
               ++ k2;
               -- length;
               if (! length) { b2 = 0xFF/*end marker*/; break; }
               b2 = *k2;
            } while (b2 == b1);
         }
         result = 8 * (size_t)(k2 - key2);
      } else {
         length = length1 - length2;
         b2 = 0xFF/*end marker of k2*/;
         b1 = *k1;
         if (b1 == b2) {
            b2 = 0x00; // artificial extension of k2
            do {
               ++ k1;
               -- length;
               if (! length) { b1 = 0xFF/*end marker*/; break; }
               b1 = *k1;
            } while (b1 == b2);
         }
         result = 8 * (size_t)(k1 - key1);
      }
   }

   // assert(b1 != b2);
   b1 ^= b2;
   unsigned mask = 0x80;
   if (!b1) return EINVAL; // should never happen (b1 != b2)
   for (; 0 == (mask & b1); mask >>= 1) {
      ++ result;
   }

   *key2_bit_offset = result;
   *key2_bit_value  = (uint8_t) (b2 & mask);
   return 0;
}

static int findnode(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_parent, patriciatrie_node_t ** found_node)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (!node) return ESRCH;

   do {
      parent = node;
      if (GETBIT(searchkey, keylength, node->bit_offset)) {
         node = node->right;
      } else {
         node = node->left;
      }
   } while (node->bit_offset > parent->bit_offset);

   *found_node   = node;
   *found_parent = parent;

   return 0;
}

int find_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** found_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONERR, );

   err = findnode(tree, keylength, searchkey, &parent, &node);
   if (err) return err;

   getkey_data_t key;
   tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &key);
   if (key.size != keylength || 0 != memcmp(key.addr, searchkey, keylength)) {
      return ESRCH;
   }

   *found_node = node;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: change

int insert_patriciatrie(patriciatrie_t * tree, patriciatrie_node_t * newnode)
{
   int err;
   getkey_data_t key;
   tree->keyadapt.getkey(cast_object(newnode, tree->keyadapt.nodeoffset), &key);

   VALIDATE_INPARAM_TEST((key.addr != 0 || key.size == 0) && key.size < (((size_t)-1)/8), ONERR, );

   if (!tree->root) {
      tree->root = newnode;
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      return 0;
   }

   // search node
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node;
   (void) findnode(tree, key.size, key.addr, &parent, &node);

   size_t  new_bitoffset;
   uint8_t new_bitvalue;
   if (node == newnode) {
      return EEXIST;      // found node already inserted
   } else {
      getkey_data_t nodek;
      tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &nodek);
      if (first_different_bit(nodek.addr, nodek.size, key.addr, key.size, &new_bitoffset, &new_bitvalue)) {
         return EEXIST;   // found node has same key
      }
      // new_bitvalue == GETBIT(key.addr, key.size, new_bitoffset)
   }

   // search position in tree where new_bitoffset belongs to
   if (new_bitoffset < parent->bit_offset) {
      node   = tree->root;
      parent = 0;
      while (node->bit_offset < new_bitoffset) {
         parent = node;
         if (GETBIT(key.addr, key.size, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      }
   }

   assert(parent == 0 || parent->bit_offset < new_bitoffset || tree->root == parent);

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->bit_offset == 0 && node->left == node);
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      node->bit_offset = new_bitoffset;
      if (new_bitvalue) {
         node->right = newnode;
      } else {
         node->left  = newnode;
      }
   } else {
      newnode->bit_offset = new_bitoffset;
      if (new_bitvalue) {
         newnode->right = newnode;
         newnode->left  = node;
      } else {
         newnode->right = node;
         newnode->left  = newnode;
      }
      if (parent) {
         if (parent->right == node) {
            parent->right = newnode;
         } else {
            assert(parent->left == node);
            parent->left = newnode;
         }
      } else {
         assert(tree->root == node);
         tree->root = newnode;
      }
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int remove_patriciatrie(patriciatrie_t * tree, size_t keylength, const uint8_t searchkey[keylength], patriciatrie_node_t ** removed_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((searchkey != 0 || keylength == 0) && keylength < (((size_t)-1)/8), ONERR, );

   err = findnode(tree, keylength, searchkey, &parent, &node);
   if (err) return err;

   getkey_data_t nodek;
   tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &nodek);

   if (  nodek.size != keylength
         || memcmp(nodek.addr, searchkey, keylength)) {
      return ESRCH;
   }

   patriciatrie_node_t * const delnode = node;
   patriciatrie_node_t * replacednode = 0;
   patriciatrie_node_t * replacedwith;

   if (node->right == node->left) {
      // LEAF points to itself (bit_offset is unused) and is at the tree bottom
      assert(node->left == node);
      assert(0==node->bit_offset);
      if (node == parent) {
         assert(tree->root == node);
         tree->root = 0;
      } else if ( parent->left == parent
                  || parent->right == parent) {
         // parent is new LEAF
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      } else {
         // shift parents other child into parents position
         replacednode = parent;
         replacedwith = (parent->left == node ? parent->right : parent->left);
         assert(replacedwith != node && replacedwith != parent);
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      }
   } else if ( node->left == node
               || node->right == node) {
      // node can be removed (cause only one child)
      assert(parent == node);
      replacednode = node;
      replacedwith = (node->left == node ? node->right : node->left);
      assert(replacedwith != node);
   } else {
      // node has two childs
      assert(parent != node);
      replacednode = node;
      replacedwith = parent;
      // find parent of replacedwith
      do {
         parent = node;
         if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      } while (node != replacedwith);
      // now let parent of replacedwith point to other child of replacedwith instead to replacedwith
      if (parent->left == node)
         parent->left = (node->left == replacednode ? node->right : node->left);
      else
         parent->right = (node->left == replacednode ? node->right : node->left);
      // copy replacednode into replacedwith
      node->bit_offset = replacednode->bit_offset;
      node->left       = replacednode->left;
      node->right      = replacednode->right;
   }

   // find parent of replacednode and let it point to replacedwith
   if (replacednode) {
      node = tree->root;
      if (node == replacednode) {
         tree->root = replacedwith;
      } else {
         do {
            parent = node;
            if (GETBIT(nodek.addr, nodek.size, node->bit_offset)) {
               node = node->right;
            } else {
               node = node->left;
            }
         } while (node != replacednode);
         if (parent->left == replacednode)
            parent->left = replacedwith;
         else
            parent->right = replacedwith;
      }
   }

   delnode->bit_offset = 0;
   delnode->left       = 0;
   delnode->right      = 0;

   *removed_node = delnode;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int removenodes_patriciatrie(patriciatrie_t * tree, delete_adapter_f delete_f)
{
   int err;
   patriciatrie_node_t * parent = 0;
   patriciatrie_node_t * node   = tree->root;

   tree->root = 0;

   if (node) {

      const bool isDelete = (delete_f != 0);

      err = 0;

      for (;;) {
         while (  node->left->bit_offset > node->bit_offset
                  || (node->left != node && node->left->left == node->left && node->left->right == node->left) /*node->left has unused bit_offset*/) {
            patriciatrie_node_t * nodeleft = node->left;
            node->left = parent;
            parent = node;
            node   = nodeleft;
         }
         if (  node->right->bit_offset > node->bit_offset
               || (node->right != node && node->right->left == node->right && node->right->right == node->right) /*node->right has unused bit_offset*/) {
            patriciatrie_node_t * noderight = node->right;
            node->left = parent;
            parent = node;
            node   = noderight;
         } else {
            node->bit_offset = 0;
            node->left       = 0;
            node->right      = 0;
            if (isDelete) {
               void * object = cast_object(node, tree->keyadapt.nodeoffset);
               int err2 = delete_f(object);
               if (err2) err = err2;
            }
            if (!parent) break;
            if (parent->right == node) {
               parent->right = parent;
            }
            node   = parent;
            parent = node->left;
            node->left = node;
         }
      }

      if (err) goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}


// section: patriciatrie_iterator_t

// group: lifetime

int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (node) {
      do {
         parent = node;
         node   = node->left;
      } while (node->bit_offset > parent->bit_offset);
   }

   iter->next = node;
   iter->tree = tree;
   return 0;
}

int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t * iter, patriciatrie_t * tree)
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;

   if (node) {
      do {
         parent = node;
         node = node->right;
      } while (node->bit_offset > parent->bit_offset);
   }

   iter->next = node;
   iter->tree = tree;
   return 0;
}

// group: iterate

bool next_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         next = next->right;
      } else {
         higher_branch_parent = parent;
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (higher_branch_parent) {
      parent = higher_branch_parent;
      next = parent->right;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->left;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}

bool prev_patriciatrieiterator(patriciatrie_iterator_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * lower_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         lower_branch_parent = parent;
         next = next->right;
      } else {
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (lower_branch_parent) {
      parent = lower_branch_parent;
      next = parent->left;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->right;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}

// section: patriciatrie_prefixiter_t

// group: lifetime

int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t * iter, patriciatrie_t * tree, size_t keylength, const uint8_t prefixkey[keylength])
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;
   size_t         prefix_bits = 8 * keylength;

   if (  (prefixkey || !keylength)
         && keylength < (((size_t)-1)/8)
         && node) {
      if (node->bit_offset < prefix_bits) {
         do {
            parent = node;
            if (GETBIT(prefixkey, keylength, node->bit_offset)) {
               node = node->right;
            } else {
               node = node->left;
            }
         } while (node->bit_offset > parent->bit_offset && node->bit_offset < prefix_bits);
      } else {
         parent = node;
         node = node->left;
      }
      while (node->bit_offset > parent->bit_offset) {
         parent = node;
         node = node->left;
      }
      getkey_data_t key;
      tree->keyadapt.getkey(cast_object(node, tree->keyadapt.nodeoffset), &key);
      bool isPrefix =   key.size >= keylength
                        && ! memcmp(key.addr, prefixkey, keylength);
      if (!isPrefix) node = 0;
   }

   iter->next        = node;
   iter->tree        = tree;
   iter->prefix_bits = prefix_bits;
   return 0;
}

// group: iterate

bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t * iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   iter->tree->keyadapt.getkey(cast_object(iter->next, iter->tree->keyadapt.nodeoffset), &nextk);

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (GETBIT(nextk.addr, nextk.size, next->bit_offset)) {
         next = next->right;
      } else {
         higher_branch_parent = parent;
         next = next->left;
      }
   } while (next->bit_offset > parent->bit_offset);

   if (  higher_branch_parent
         && higher_branch_parent->bit_offset >= iter->prefix_bits) {
      parent = higher_branch_parent;
      next = parent->right;
      while (next->bit_offset > parent->bit_offset) {
         parent = next;
         next = next->left;
      }
      iter->next = next;
   } else {
      iter->next = 0;
   }

   return true;
}
