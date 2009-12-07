/*
 * Copyright (c) 2008, 2009 Dan Kruchinin <dan.kruchinin@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * @file ttree.h
 * @author Dan Kruchinin
 * @brief T*-tree API defenitions and constants
 *
 * For more information about T- and T*-trees see:
 * 1) Tobin J. Lehman , Michael J. Carey,
 *    A Study of Index Structures for Main Memory Database Management Systems
 * 2) Kong-Rim Choi , Kyung-Chang Kim,
 *    T*-tree: a main memory database index structure for real time applications
 */

#ifndef __TTREE_H__
#define __TTREE_H__

#include <stdint.h>
#include <sys/types.h>

#define TTREE_DEFAULT_NUMKEYS 8         /**< Default number of keys per T*-tree node */
#define TNODE_ITEMS_MIN       2         /**< Minimum allowable number of keys per T*-tree node */
#define TNODE_ITEMS_MAX       (1 << 11) /**< Maximum allowable numebr of keys per T*-tree node */

enum {
    TNODE_UNDEF = -1, /**< T*-tree node side is undefined */
    TNODE_LEFT,       /**< Left side */
    TNODE_RIGHT,      /**< Right side */
};

enum ttree_cursor_state {
    TT_CSR_UNTIED = 0,
    TT_CSR_TIED,
    TT_CSR_PENDING,
};

#define TNODE_ROOT  TNODE_UNDEF /**< T*-tree node is root */
#define TNODE_BOUND TNODE_UNDEF /**< T*-tree node bounds searhing value */

/**
 * @struct TtreeNode
 * @brief T*-tree node structure
 */
typedef struct ttree_node {
    struct ttree_node *parent;     /**< Pointer to node's parent */
    struct ttree_node *successor;  /**< Pointer to node's soccussor */
    union {
        struct ttree_node *sides[2];
        struct  {
            struct ttree_node *left;   /**< Pointer to node's left child  */
            struct ttree_node *right;  /**< Pointer to node's right child */
        };
    };
    union {
        uint32_t pad;
        struct {
            signed min_idx     :12;      /**< Index of minimum item in node's array */
            signed max_idx     :12;      /**< Index of maximum item in node's array */
            signed bfc         :4;       /**< Node's balance factor */
            unsigned node_side :4;       /**< Node's side(TNODE_LEFT, TNODE_RIGHT or TNODE_ROOT) */
        };
    };
    void *keys[2];                   /**< First two items of T*-tree node keys array */
} TtreeNode;

/**
 * @typedef int (*ttree_cmp_func_t)(void *data1, void *data2)
 * @brief T*-tree keys comparing function.
 * Must return negative value if @a key1 is leaster than @a key2,
 * positive if @a key1 is greater than @a key2 and 0 when they're equal.
 */
typedef int (*ttree_cmp_func_fn)(void *key1, void *key2);

/**
 * @struct Ttree
 * @brief T*-tree main structure.
 */
typedef struct ttree {
    TtreeNode *root;            /**< A pointer to T*-tree root node */
    ttree_cmp_func_fn cmp_func; /**< User-defined key comparing function */
    size_t key_offs;            /**< Offset from item to its key(may be 0) */
    int keys_per_tnode;         /**< Number of keys per each T*-tree node */
} Ttree;

/**
 * @struct tnode_meta_t
 * @brief This structure is used to describe T*-tree node meta information.
 *
 * T*-tree node meta information helps to tie particular key with a value
 * holding in a T*-tree. Using tnode_meta_t user may know what exactly node
 * holds given key and which index this key has.
 *
 * @see TtreeNode
 */
typedef struct ttree_cursor {
    Ttree *ttree;
    TtreeNode *tnode; /**< A pointer to T*-tree node */
    int idx;             /**< Particular index in a T*-tree node array */
    int side;            /**< T*-tree node side. Used when item is inserted. */
    enum ttree_cursor_state state;
} TtreeCursor;

#ifndef CONFIG_DEBUG_TTREE
#define TT_ASSERT_DBG(cond)
#define __validate_cursor_dbg(cursor)
#else
#define TT_ASSERT_DBG(cond) ASSERT(cond)
#define __validate_cursor_dbg(cursor)                       \
    do {                                                    \
        TT_ASSERT_DBG((cursor)->ttree != NULL);             \
        TT_ASSERT_DBG((cursor)->tnode != NULL);             \
        TT_ASSERT_DBG(!tnode_is_empty((cursor)->tnode));    \
        TT_ASSERT_DBG((cursor)->state != TT_CSR_UNTIED);    \
    } while (0)
#endif /* CONFIG_DEBUG_TTREE */


/**
 * @brief Get real size of T*-tree node in bytes
 * @paran ttree - A pointer to T*-tree
 * @return T*-tree node size
 */
#define tnode_size(ttree)                                               \
    (sizeof(TtreeNode) + (ttree->keys_per_tnode - 2) * sizeof(void *))

/**
 * @brief Get current number of keys holding in T*-tree node
 * @param ttree - A pointer to T*-tree node
 * @return Number of keys in node
 */
#define tnode_num_keys(tnode)                   \
    (((tnode)->max_idx - (tnode)->min_idx) + 1)

/**
 * @brief Check if T*-tree node is empty
 * @param tnode - A pointer to node
 * @return Bool.
 */
#define tnode_is_empty(tnode)                   \
    (!tnode_num_keys(tnode))

/**
 * @brief Check if T*-tree node is full.
 * @param ttree - A pointer to T*-tree
 * @param tnode - A pointer to target node.
 * @return Bool.
 */
#define tnode_is_full(ttree, tnode)                     \
    (tnode_num_keys(tnode) == (ttree)->keys_per_tnode)

/**
 * @brief Get item address from its key
 * @param ttree - A pointer to T*-tree
 * @param key   - A poineter to key
 * @return An address of item
 */
#define ttree_key2item(ttree, key)                  \
    ((void *)((char *)(key) - (ttree)->key_offs))

/**
 * @brief Get key address by an item.
 * @param ttree - A pointer to T*-tree
 * @param item  - A pointer to item
 * @return An address of key field of @a item.
 */
#define ttree_item2key(ttree, item)                 \
    ((void *)((char *)(item) + (ttree)->key_offs))

/**
 * @brief Fetch key from T*-tree node by key's index
 * @param tnode - A pointer to target node.
 * @param idx   - Key index
 * @return An address of key holding by @a idx.
 */
#define tnode_key(tnode, idx)                   \
    ((tnode)->keys[(idx)])

/**
 * @brief Get an address of minimum key in T*-tree node
 * @param tnode - A pointer to target node.
 * @return An address of min. key in a node.
 */
#define tnode_key_min(tnode) tnode_key(tnode, (tnode)->min_idx)

/**
 * @brief Get an address of maximum key in T*-tree node
 * @param tnode - A pointer to target node.
 * @return An address of its max. key.
 */
#define tnode_key_max(tnode) tnode_key(tnode, (tnode)->max_idx)

/**
 * @brief Get greatest lower bound node of a given node.
 * @param tnode - A pointer to subtree
 * @return A pointer to node holding greatest lower bound.
 */
#define Ttreenode_glb(tnode)                    \
    __tnode_get_bound(tnode, TNODE_LEFT)

/**
 * @brief Get least upper bound node of a given node.
 * @param tnode - A pointer to subtree.
 * @return A pointer to node holding least upper bound of given subtree.
 */
#define Ttreenode_lub(tnode)                    \
    __tnode_get_bound(tnode, TNODE_RIGHT)

/**
 * @brief Get subtree's leftmost node
 * @param tnode - A pointer to subtree
 * @return Its leftmost node
 */
#define Ttreenode_leftmost(tnode)               \
    __tnode_sidemost(tnode, TNODE_LEFT)

/**
 * @brief Get subtree's rightmost node
 * @param tnode - A pointer to subtree.
 * @return Its rightmost node.
 */
#define Ttreenode_rightmost(tnode)              \
    __tnode_sidemost(tnode, TNODE_RIGHT)

/**
 * @brief Iterate through each non-empty index in a T*-tree node
 * @param tnode - A pointer to target node
 * @param iter  - An integer using for iteration.
 * @see tnode_key
 */
#define tnode_for_each_index(tnode, iter)                               \
    for ((iter) = (tnode)->min_idx; (iter) <= (tnode)->max_idx; (iter)++)

/**
 * @brief Set T*-tree node side to @a side
 * @param tnode - A pointer to target node
 * @param side  - Node side to set
 * @see tnode_get_side
 */
static inline void tnode_set_side(TtreeNode *tnode, int side)
{
    tnode->node_side &= ~0x3;
    tnode->node_side |= (side + 1);
}

/**
 * @brief Get T*-tree node side.
 * @param tnode - A pointer to target node.
 * @return Side of @a tnode.
 * @see tnode_set_side
 */
static inline int tnode_get_side(TtreeNode *tnode)
{
    return ((tnode->node_side & 0x03) - 1);
}

/**
 * @brief Check if T*-tree is absolutely empty
 * @param ttree - A pointer to T*-tree
 * @return Bool.
 */
#define ttree_is_empty(ttree)                   \
    (!(ttree)->root)

/**
 * @brief Initialize new T*-tree.
 * @param ttree[out]  - A pointer to T*-tree to initialize
 * @param cmpf        - A pointer to user-defined compare function
 * @param data_struct - Item's structure
 * @param key_field   - Field in a @a data_struct that is used as a key.
 * @see Ttree
 */
#define ttree_init(ttree, cmpf, data_struct, key_field)         \
    __ttree_init(ttree, cmpf, offsetof(data_struct, key_field))

/**
 * @brief More detailed T*-tree initialization.
 * @param ttree[out] - A pointer to T*-tree to initialize
 * @param cmpf       - User defined compare function
 * @param key_offs   - Offset from item structure start to its key field.
 * @see Ttree
 */
void __ttree_init(Ttree *ttree, ttree_cmp_func_fn cmpf, size_t key_offs);

/**
 * @brief Destroy whole T*-tree
 * @param ttree - A pointer to tree to destroy.
 * @see ttree_init
 */
void ttree_destroy(Ttree *ttree);

/**
 * @brief Find an item by its key in a tree.
 *
 * This function allows to find an item in a tree by item's key.
 * Also it is used for searching a place where new item with a key @a key
 * should be inserted. All necessary information are stored in a @a tnode_meta
 * if meta is specified.
 *
 * @param ttree           - A pointer to T*-tree where to search.
 * @param key             - A pointer to search key.
 * @param tnode_meta[out] - A pointer to T*-tree node meta, where results meta is searched.(may be NULL)
 * @return A pointer to found item or NULL if item wasn't found.
 * @see tnode_meta_t
 */
void *ttree_lookup(Ttree *ttree, void *key, TtreeCursor *cursor);

/**
 * @brief Insert an item @a item in a T*-tree
 * @param ttree - A pointer to tree.
 * @param item  - A pointer to item to insert.
 * @return 0 if all is ok, negative value if item is duplicate.
 */
int ttree_insert(Ttree *ttree, void *item);

/**
 * @brief Delete an item from a T*-tree by item's key.
 * @param ttree - A pointer to tree.
 * @param key   - A pointer to item's key.
 * @return A pointer to removed item or NULL item with key @a key wasn't found.
 */
void *ttree_delete(Ttree *ttree, void *key);

/**
 * @brief "Placeful" item insertion in a T*-tree.
 *
 * This function allows to insert new intems in a T*-tree specifing
 * they precise places using T*-tree node meta information.
 * T*-tree nodes meta may be known by searching a key position in a tree
 * using ttree_lookup function. Even if key is not found, tnode_meta_t
 * will hold its *possible* place. For example ttree_insert dowsn't allow
 * to insert the duplicates. But using a boundle of ttree_lookup and ttree_insert_placeful
 * this limitation can be wiped out. After ttree_lookup is done, metainformation is
 * saved in tnode_meta and this meta may be used for placeful insertion.
 * Metainformation after insertion metainformation(idx with tnode) may be used for
 * accessing to item or its key in a tree. Note that ttree_insert_placeful *may*
 * change the meta if and only if item position is changed during insertion proccess.
 * @warning @a tnode_meta shouldn't be modified directly.
 *
 * @param ttree           - A pointer to T*-tree where to insert.
 * @param tnode_meta[out] - A pointer to T*-tree node meta information(filled by ttree_lookup)
 * @param item            - An item to insert.
 *
 * @see tnode_meta_t
 * @see ttree_lookup
 * @see ttree_delete_placeful
 */
void ttree_insert_placeful(TtreeCursor *cursor, void *item);

/**
 * @brief "Placeful" item deletion from a T*-tree.
 *
 * ttree_delete_placeful use similar to ttree_insert_placeful approach. I.e. it uses
 * metainformation structure filled by ttree_lookup for item deletion. Since tnode_meta_t
 * contains target T*-tree node and precise place of item in that node, this information
 * may be used for deletion.
 *
 * @param ttree      - A pointer to a T*-tree item will be removed from.
 * @param tnode_meta - T*-tree node metainformation structure filled by ttree_lookup.
 * @return A pointer to removed item.
 *
 * @see tnode_meta_t
 * @see ttree_lookup
 * @see ttree_delete_placeful
 */
void *ttree_delete_placeful(TtreeCursor *cursor);

/**
 * @brief Replace an item saved in a T*-tree by a key @a key.
 * It's an atomic operation that doesn't requires any rebalancing.
 *
 * @param ttree    - A pointer to a T*-tree.
 * @param key      - A pointer to key whose item will be replaced.
 * @param new_item - A pointer to new item that'll replace previous one.
 * @return 0 if all is ok or negative value if @a key wasn't found.
 */
int ttree_replace(Ttree *ttree, void *key, void *new_item);

void ttree_cursor_init(Ttree *ttree, TtreeCursor *cursor);
int ttree_cursor_next(TtreeCursor *cursor);
int ttree_cursor_prev(TtreeCursor *cursor);

#define ttree_cursor_copy(csr_dst, csr_src)         \
    memcpy(csr_dst, csr_src, sizeof(*(csr_src)))

static inline void *ttree_key_from_cursor(TtreeCursor *cursor)
{
    __validate_cursor_dbg(cursor);
    if (likely(cursor->side == TNODE_BOUND))
        return tnode_key(cursor->tnode, cursor->idx);

    return NULL;
}

static inline void *ttree_item_from_cursor(TtreeCursor *cursor)
{
    void *key = ttree_key_from_cursor(cursor);
    if (!key)
        return NULL;

    return ttree_key2item(cursor->ttree, key);
}

/**
 * @brief Display T*-tree structure on a screen.
 * @param ttree - A pointer to a T*-tree.
 * @paran fn    - A pointer to function used for displaing T-tree node items
 * @warning Recursive function.
 */
void ttree_print(Ttree *ttree, void (*fn)(TtreeNode *tnode));

#ifdef CONFIG_DEBUG_TTREE
/**
 * @brief This debugging function allows to check T*-tree for balance.
 *
 * Function panics if a tree is totally unbalanced or one of its subtree
 * has invalid balance or balance factor.
 * @warning Recursive function.
 *
 * @param tnode - A pointer to subtree to check.
 * @see Ttree
 * @see TtreeNode
 */
int ttree_check_depth_dbg(TtreeNode *tnode);
#endif /* CONFIG_DEBUG_TTREE */

/*
 * Internal T*-tree functions.
 * Not invented for public usage.
 */
static inline TtreeNode *__tnode_sidemost(TtreeNode *tnode, int side)
{
    if (!tnode)
        return NULL;
    else {
        TtreeNode *n;

        for (n = tnode; n->sides[side]; n = n->sides[side]);
        return n;
    }
}

static inline TtreeNode *__tnode_get_bound(TtreeNode *tnode, int side)
{
    if (!tnode)
        return NULL;
    else {
        if (!tnode->sides[side])
            return NULL;
        else {
            TtreeNode *bnode;

            for (bnode = tnode->sides[side]; bnode->sides[!side];
                 bnode = bnode->sides[!side]);
            return bnode;
        }
    }
}

#endif /* !__TTREE_H__ */