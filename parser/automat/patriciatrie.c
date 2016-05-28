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

// group: lifetime

int free_patriciatrie(patriciatrie_t *tree, delete_adapter_f delete_f)
{
   int err = removenodes_patriciatrie(tree, delete_f);

   tree->keyadapt = (getkey_adapter_t) getkey_adapter_INIT(0,0);

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: helper

static inline void * cast_object(patriciatrie_node_t *node, patriciatrie_t *tree)
{
   return ((uint8_t*)node - tree->keyadapt.nodeoffset);
}

// group: search

/* function: getbitinit
 * Ensures that precondition of getbit is met. */
static inline void getbitinit(patriciatrie_t *tree, getkey_data_t *key, size_t bitoffset)
{
   size_t byteoffset = bitoffset / 8;
   if (byteoffset < key->offset) {
      // PRECONDITION of getbit violated ==> go back in stream
      tree->keyadapt.getkey(key, byteoffset);
   }
}

/* function: getbit
 * Returns the bit value at bitoffset if bitoffset is in range [0..key->streamsize*8-1].
 * Every key has a virtual end marker byte 0xFF at offset key->streamsize.
 * Therefore a bitoffset in range [key->streamsize*8..key->streamsize*8+7]
 * always results into the value 1. For every bitoffset >= (key->streamsize+1)*8
 * a value of 0 is returned.
 *
 * Unchecked Precondition:
 * - key->offset <= bitoffset/8
 *
 * */
static inline int getbit(patriciatrie_t *tree, getkey_data_t *key, size_t bitoffset)
{
   size_t byteoffset = bitoffset / 8;
   if (byteoffset >= key->endoffset) {
      // end of streamed key ?
      if (byteoffset >= key->streamsize) {
         return (byteoffset == key->streamsize);
      }
      // go forward in stream
      tree->keyadapt.getkey(key, byteoffset);
   }
   return key->addr[byteoffset - key->offset] & (0x80>>(bitoffset%8));
}

/* function: get_first_different_bit
 * Determines the bit with the smallest offset which differs in foundkey and newkey.
 * Virtual end markers of 0xFF at end of each key are used to ensure that keys of different
 * length are always different.
 * If both keys are equal the function returns the error code EEXIST.
 * On success 0 is returned and *newkey_bit_offset contains the bit index
 * of the first differing bit starting from bitoffset 0.
 * The value *newkey_bit_value contains the value of the bit
 * from newkey at offset *newkey_bit_offset.
 *
 * Unchecked Precondition:
 * - foundobj == cast_object(foundnode, tree)
 * - newobj   == cast_object(newnode, tree)
 * */
static inline int get_first_different_bit(
      patriciatrie_t*tree,
      getkey_data_t *foundkey,
      getkey_data_t *newkey,
      /*out*/size_t *newkey_bit_offset,
      /*out*/uint8_t*newkey_bit_value)
{
   uint8_t bf;
   uint8_t bn;

   // reset both keys to offset 0
   ////
   if (foundkey->offset) {
      tree->keyadapt.getkey(foundkey, 0);
   }
   if (newkey->offset) {
      tree->keyadapt.getkey(newkey, 0);
   }

   // compare keys
   ////
   size_t offset = 0;
   uint8_t const *keyf = foundkey->addr;
   uint8_t const *keyn = newkey->addr;
   for (;;) {
      size_t endoffset = foundkey->endoffset < newkey->endoffset ? foundkey->endoffset : newkey->endoffset;
      for (; offset < endoffset; ++offset) {
         if (*keyf != *keyn) {
            bf = *keyf;
            bn = *keyn;
            goto FOUND_DIFFERENCE;
         }
         ++ keyf;
         ++ keyn;
      }
      if (offset == newkey->endoffset) {
         if (offset == newkey->streamsize) {
            if (offset == foundkey->endoffset) {
               if (offset == foundkey->streamsize) return EEXIST;
               tree->keyadapt.getkey(foundkey, offset);
               keyf = foundkey->addr;
            }
            if (*keyf != 0xFF/*end marker of newkey*/) {
               bf = *keyf;
               bn = 0xFF;
               goto FOUND_DIFFERENCE;
            }
            bn = 0; // artificial extension of newkey is 0
            ++offset;
            ++keyf;
            for (;;) {
               for (; offset < foundkey->endoffset; ++offset) {
                  if (*keyf != 0) {
                     bf = *keyf;
                     goto FOUND_DIFFERENCE;
                  }
                  ++ keyf;
               }
               if (offset == foundkey->streamsize) {
                  bf = 0xFF;/*end marker of foundkey*/
                  goto FOUND_DIFFERENCE;
               }
               tree->keyadapt.getkey(foundkey, offset);
               keyf = foundkey->addr;
            }
         }
         tree->keyadapt.getkey(newkey, offset);
         keyn = newkey->addr;
      }
      // == assert(offset < newkey->endoffset) ==
      if (offset == foundkey->endoffset) {
         if (offset == foundkey->streamsize) {
            if (*keyn != 0xFF/*end marker of foundkey*/) {
               bf = 0xFF;
               bn = *keyn;
               goto FOUND_DIFFERENCE;
            }
            bf = 0; // artificial extension of foundkey is 0
            ++offset;
            ++keyn;
            for (;;) {
               for (; offset < newkey->endoffset; ++offset) {
                  if (*keyn != 0) {
                     bn = *keyn;
                     goto FOUND_DIFFERENCE;
                  }
                  ++ keyn;
               }
               if (offset == newkey->streamsize) {
                  bn = 0xFF;/*end marker of newkey*/
                  goto FOUND_DIFFERENCE;
               }
               tree->keyadapt.getkey(newkey, offset);
               keyn = newkey->addr;
            }
         }
         tree->keyadapt.getkey(foundkey, offset);
         keyf = foundkey->addr;
      }
   }

FOUND_DIFFERENCE: // == (bf != bn) ==
   offset *= 8;
   bf ^= bn;
   unsigned mask = 0x80;
   for (; 0 == (mask & bf); mask >>= 1) {
      ++ offset;
   }

   *newkey_bit_offset = offset;
   *newkey_bit_value  = (uint8_t) (bn & mask);
   return 0;
}

static inline int is_key_equal(
      patriciatrie_t      *tree,
      patriciatrie_node_t *foundnode,
      getkey_data_t       *cmpkey)
{
   getkey_data_t foundkey;
   init1_getkeydata(&foundkey, tree->keyadapt.getkey, cast_object(foundnode, tree));

   if (foundkey.streamsize == cmpkey->streamsize) {
      if (cmpkey->offset) {
         tree->keyadapt.getkey(cmpkey, 0);
      }
      size_t offset = 0;
      uint8_t const *keyf = foundkey.addr;
      uint8_t const *keyc = cmpkey->addr;
      if (cmpkey->endoffset == cmpkey->streamsize) {
         for (;;) {
            for (; offset < foundkey.endoffset ; ++offset) {
               if (keyc[offset] != *keyf++) {
                  return false;
               }
            }
            if (offset == foundkey.streamsize) {
               return true/*equal*/;
            }
            tree->keyadapt.getkey(&foundkey, offset);
            keyf = foundkey.addr;
         }

      } else {
         for (;;) {
            size_t endoffset = foundkey.endoffset <= cmpkey->endoffset ? foundkey.endoffset : cmpkey->endoffset;
            for (; offset < endoffset; ++offset) {
               if (*keyc++ != *keyf++) {
                  return false;
               }
            }
            if (offset == foundkey.endoffset) {
               if (offset == foundkey.streamsize) {
                  return true/*equal*/;
               }
               tree->keyadapt.getkey(&foundkey, offset);
               keyf = foundkey.addr;
            }
            if (offset == cmpkey->endoffset) {
               tree->keyadapt.getkey(cmpkey, offset);
               keyc = cmpkey->addr;
            }
         }
      }
   }

   return false;
}

/* function: findnode
 * Returns found_parent and found_node, both !=0.
 * In case of a single node *found_node == *found_parent.
 * In case of an empty trie ESRCH is returned else 0.
 * Either (*found_node)->bit_offset < (*found_parent)->bit_offset),
 * or (*found_node)->bit_offset == (*found_parent)->bit_offset) in case of a single node.
 *
 * Unchecked Precondition:
 * - tree->root  != 0
 * - key->offset == 0
 * */
static void findnode(patriciatrie_t *tree, getkey_data_t *key, patriciatrie_node_t ** found_parent, patriciatrie_node_t ** found_node)
{
   patriciatrie_node_t *parent;
   patriciatrie_node_t *node = tree->root;

   do {
      parent = node;
      if (getbit(tree, key, node->bit_offset)) {
         node = node->right;
      } else {
         node = node->left;
      }
   } while (node->bit_offset > parent->bit_offset);

   *found_node   = node;
   *found_parent = parent;
}

int find_patriciatrie(patriciatrie_t *tree, size_t len, const uint8_t key[len], /*out*/patriciatrie_node_t ** found_node)
{
   int err;
   patriciatrie_node_t *node;
   patriciatrie_node_t *parent;

   VALIDATE_INPARAM_TEST((key != 0 || len == 0) && len < (((size_t)-1)/8), ONERR, );

   if (!tree->root) return ESRCH;

   getkey_data_t fullkey;
   initfullkey_getkeydata(&fullkey, len, key);
   findnode(tree, &fullkey, &parent, &node);

   if (! is_key_equal(tree, node, &fullkey)) {
      return ESRCH;
   }

   *found_node = node;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: change

int insert_patriciatrie(patriciatrie_t *tree, patriciatrie_node_t * newnode, /*err*/ patriciatrie_node_t** existing_node/*0 ==> not returned*/)
{
   int err;
   getkey_data_t newkey;
   init1_getkeydata(&newkey, tree->keyadapt.getkey, cast_object(newnode, tree));

   VALIDATE_INPARAM_TEST((newkey.addr != 0 || newkey.streamsize == 0) && newkey.streamsize < (((size_t)-1)/8), ONERR, );

   if (!tree->root) {
      tree->root = newnode;
      newnode->bit_offset = 0;
      newnode->right = newnode;
      newnode->left  = newnode;
      return 0;
   }

   // search node
   patriciatrie_node_t *parent;
   patriciatrie_node_t *node;
   findnode(tree, &newkey, &parent, &node);

   size_t  new_bitoffset;
   uint8_t new_bitvalue;
   {
      getkey_data_t foundkey;
      init1_getkeydata(&foundkey, tree->keyadapt.getkey, cast_object(node, tree));
      if (node == newnode || get_first_different_bit(tree, &foundkey, &newkey, &new_bitoffset, &new_bitvalue)) {
         if (existing_node) *existing_node = node;
         return EEXIST;   // found node has same key
      }
      // new_bitvalue == getbit(tree, &newkey, new_bitoffset)
   }

   // search position in tree where new_bitoffset belongs to
   if (new_bitoffset < parent->bit_offset) {
      node   = tree->root;
      parent = 0;
      getbitinit(tree, &newkey, node->bit_offset);
      while (node->bit_offset < new_bitoffset) {
         parent = node;
         if (getbit(tree, &newkey, node->bit_offset)) {
            node = node->right;
         } else {
            node = node->left;
         }
      }
   }

   // == assert(parent == 0 || parent->bit_offset < new_bitoffset || (tree->root == parent && parent == node)/*single node case)*/) ==

   if (node->right == node->left) {
      // node points to itself (bit_offset is unused) and is at the tree bottom (LEAF)
      // == assert(node->bit_offset == 0 && node->left == node) ==
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
         // == (tree->root == node) is possible therefore check for parent != 0 ==
         if (parent->right == node) {
            parent->right = newnode;
         } else {
            parent->left = newnode;
         }
      } else {
         // == (parent == 0) ==> (tree->root == node) ==
         tree->root = newnode;
      }
   }

   return 0;
ONERR:
   if (existing_node) *existing_node = 0; // err param
   TRACEEXIT_ERRLOG(err);
   return err;
}

int remove_patriciatrie(patriciatrie_t *tree, size_t len, const uint8_t key[len], /*out*/patriciatrie_node_t ** removed_node)
{
   int err;
   patriciatrie_node_t * node;
   patriciatrie_node_t * parent;

   VALIDATE_INPARAM_TEST((key != 0 || len == 0) && len < (((size_t)-1)/8), ONERR, );

   if (!tree->root) return ESRCH;

   getkey_data_t fullkey;
   initfullkey_getkeydata(&fullkey, len, key);
   findnode(tree, &fullkey, &parent, &node);

   if (! is_key_equal(tree, node, &fullkey)) {
      return ESRCH;
   }

   patriciatrie_node_t * const delnode = node;
   patriciatrie_node_t * replacednode = 0;
   patriciatrie_node_t * replacedwith;

   if (node->right == node->left) {
      // node points to itself (bit_offset is unused) and is at the tree bottom (LEAF)
      // == assert(node->left == node && 0 == node->bit_offset) ==
      if (tree->root == node) {
         tree->root = 0;
      } else if ( parent->left == parent
                  || parent->right == parent) {
         // parent is new LEAF
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      } else {
         // shift parents other child into parents position
         // other child or any child of this child points back to parent
         replacednode = parent;
         replacedwith = (parent->left == node ? parent->right : parent->left);
         // and make parent the new LEAF (only one such node in the whole trie)
         parent->bit_offset = 0;
         parent->left  = parent;
         parent->right = parent;
      }
   } else if (node->left == node || node->right == node) {
      // node points to itself
      // ==> (parent == node) && "node has only one child which does not point back to node"
      // node can be replaced by single child
      replacednode = node;
      replacedwith = (node->left == node ? node->right : node->left);
   } else {
      // node has two childs ==> one of the childs of node points back to node
      // ==> (parent != node)
      replacednode = node;
      replacedwith = parent;
      // find parent of replacedwith
      // node->bit_offset < parent->bit_offset ==> node higher in hierarchy than parent
      // NOT NEEDED(only single data block): getbitinit(tree, &fullkey, node->bit_offset)
      do {
         parent = node;
         if (getbit(tree, &fullkey, node->bit_offset)) {
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
         // NOT NEEDED(only single data block): getbitinit(tree, &fullkey, node->bit_offset)
         do {
            parent = node;
            if (getbit(tree, &fullkey, node->bit_offset)) {
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

int removenodes_patriciatrie(patriciatrie_t *tree, delete_adapter_f delete_f)
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
               void * object = cast_object(node, tree);
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

int initfirst_patriciatrieiterator(/*out*/patriciatrie_iterator_t *iter, patriciatrie_t *tree)
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

int initlast_patriciatrieiterator(/*out*/patriciatrie_iterator_t *iter, patriciatrie_t *tree)
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

bool next_patriciatrieiterator(patriciatrie_iterator_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
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

bool prev_patriciatrieiterator(patriciatrie_iterator_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * lower_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
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

int initfirst_patriciatrieprefixiter(/*out*/patriciatrie_prefixiter_t *iter, patriciatrie_t *tree, size_t len, const uint8_t prefixkey[len])
{
   patriciatrie_node_t * parent;
   patriciatrie_node_t * node = tree->root;
   size_t         prefix_bits = 8 * len;

   if (  (prefixkey || !len)
         && len < (((size_t)-1)/8)
         && node) {
      getkey_data_t prefk;
      initfullkey_getkeydata(&prefk, len, prefixkey);
      if (node->bit_offset < prefix_bits) {
         do {
            parent = node;
            if (getbit(tree, &prefk, node->bit_offset)) {
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
      init1_getkeydata(&key, tree->keyadapt.getkey, cast_object(node, tree));
      if (key.streamsize >= prefk.streamsize) {
         uint8_t const *key2 = key.addr;
         for (size_t offset = 0; offset < len; ++offset) {
            if (offset == key.endoffset) {
               tree->keyadapt.getkey(&key, offset);
               key2 = key.addr;
            }
            if (prefixkey[offset] != *key2++) goto NO_PREFIX;
         }

      } else { NO_PREFIX:
         node = 0;
      }
   }

   iter->next        = node;
   iter->tree        = tree;
   iter->prefix_bits = prefix_bits;
   return 0;
}

// group: iterate

bool next_patriciatrieprefixiter(patriciatrie_prefixiter_t *iter, /*out*/patriciatrie_node_t ** node)
{
   if (!iter->next) return false;

   *node = iter->next;

   getkey_data_t nextk;
   init1_getkeydata(&nextk, iter->tree->keyadapt.getkey, cast_object(iter->next, iter->tree));

   patriciatrie_node_t * next = iter->tree->root;
   patriciatrie_node_t * parent;
   patriciatrie_node_t * higher_branch_parent = 0;
   do {
      parent = next;
      if (getbit(iter->tree, &nextk, next->bit_offset)) {
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
