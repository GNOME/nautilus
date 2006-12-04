/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2002, 2003, 2004, 2005  Soeren Sandmann (sandmann@daimi.au.dk)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>

#include "gsequence.h"

typedef struct _GSequenceNode GSequenceNode;

struct _GSequence
{
    GSequenceNode *	 end_node;
    GDestroyNotify	 data_destroy_notify;
    gboolean		 access_prohibited;
};

struct _GSequenceNode
{
    gint n_nodes;
    GSequenceNode *parent;    
    GSequenceNode *left;
    GSequenceNode *right;
    gpointer data;		/* For the end node, this field points
				 * to the sequence
				 */
};

static GSequenceNode *node_new           (gpointer           data);
static GSequenceNode *node_get_first     (GSequenceNode   *node);
static GSequenceNode *node_get_last      (GSequenceNode   *node);
static GSequenceNode *node_get_prev      (GSequenceNode   *node);
static GSequenceNode *node_get_next      (GSequenceNode   *node);
static gint             node_get_pos       (GSequenceNode   *node);
static GSequenceNode *node_get_by_pos    (GSequenceNode   *node,
					    gint               pos);
static GSequenceNode *node_find_closest  (GSequenceNode   *haystack,
					    GSequenceNode   *needle,
					    GSequenceNode   *end,
					    GSequenceIterCompareFunc cmp,
					    gpointer           user_data);
static gint             node_get_length    (GSequenceNode   *node);
static void             node_free          (GSequenceNode   *node,
					    GSequence       *seq);
static void             node_cut           (GSequenceNode   *split);
static void             node_insert_after  (GSequenceNode   *node,
					    GSequenceNode   *second);
static void             node_insert_before (GSequenceNode   *node,
					    GSequenceNode   *new);
static void             node_unlink        (GSequenceNode   *node);
static void             node_insert_sorted (GSequenceNode   *node,
					    GSequenceNode   *new,
					    GSequenceNode   *end,
					    GSequenceIterCompareFunc cmp_func,
					    gpointer           cmp_data);

static GSequence *
get_sequence (GSequenceNode *node)
{
    return (GSequence *)node_get_last (node)->data;
}

static void
check_seq_access (GSequence *seq)
{
    if (G_UNLIKELY (seq->access_prohibited))
    {
	g_warning ("Accessing a sequence while it is "
		   "being sorted is not allowed");
    }
}

static void
check_iter_access (GSequenceIter *iter)
{
    check_seq_access (get_sequence (iter));
}

static gboolean
is_end (GSequenceIter *iter)
{
    GSequence *seq = get_sequence (iter);
    
    return seq->end_node == iter;
}

/*
 * Public API
 */

/* GSequence */
GSequence *
g_sequence_new (GDestroyNotify data_destroy)
{
    GSequence *seq = g_new (GSequence, 1);
    seq->data_destroy_notify = data_destroy;
    
    seq->end_node = node_new (seq);
    
    seq->access_prohibited = FALSE;
    
    return seq;
}

void
g_sequence_free (GSequence *seq)
{
    g_return_if_fail (seq != NULL);
    
    check_seq_access (seq);
    
    node_free (seq->end_node, seq);
    
    g_free (seq);
}

void
g_sequence_foreach_range (GSequenceIter *begin,
			    GSequenceIter *end,
			    GFunc	     func,
			    gpointer	     data)
{
    GSequence *seq;
    GSequenceIter *iter;
    
    g_return_if_fail (func != NULL);
    g_return_if_fail (begin != NULL);
    g_return_if_fail (end != NULL);
    
    seq = get_sequence (begin);
    
    seq->access_prohibited = TRUE;
    
    iter = begin;
    while (iter != end)
    {
	GSequenceIter *next = node_get_next (iter);
	
	func (iter->data, data);
	
	iter = next;
    }
    
    seq->access_prohibited = FALSE;
}

void
g_sequence_foreach (GSequence *seq,
		      GFunc        func,
		      gpointer     data)
{
    GSequenceIter *begin, *end;
    
    check_seq_access (seq);
    
    begin = g_sequence_get_begin_iter (seq);
    end   = g_sequence_get_end_iter (seq);
    
    g_sequence_foreach_range (begin, end, func, data);
}

GSequenceIter *
g_sequence_range_get_midpoint (GSequenceIter *begin,
				 GSequenceIter *end)
{
    int begin_pos, end_pos, mid_pos;
    
    g_return_val_if_fail (begin != NULL, NULL);
    g_return_val_if_fail (end != NULL, NULL);
    g_return_val_if_fail (get_sequence (begin) == get_sequence (end), NULL);

    begin_pos = node_get_pos (begin);
    end_pos = node_get_pos (end);

    g_return_val_if_fail (end_pos >= begin_pos, NULL);
    
    mid_pos = begin_pos + (end_pos - begin_pos) / 2;

    return node_get_by_pos (begin, mid_pos);
}

gint
g_sequence_iter_compare (GSequenceIter *a,
			   GSequenceIter *b)
{
    gint a_pos, b_pos;
    
    g_return_val_if_fail (a != NULL, 0);
    g_return_val_if_fail (b != NULL, 0);
    g_return_val_if_fail (get_sequence (a) == get_sequence (b), 0);
    
    check_iter_access (a);
    check_iter_access (b);
    
    a_pos = node_get_pos (a);
    b_pos = node_get_pos (b);
    
    if (a_pos == b_pos)
	return 0;
    else if (a_pos > b_pos)
	return 1;
    else
	return -1;
}

GSequenceIter *
g_sequence_append (GSequence *seq,
		     gpointer     data)
{
    GSequenceNode *node;
    
    g_return_val_if_fail (seq != NULL, NULL);
    
    check_seq_access (seq);
    
    node = node_new (data);
    node_insert_before (seq->end_node, node);
    
    return node;
}

GSequenceIter *
g_sequence_prepend (GSequence *seq,
		      gpointer     data)
{
    GSequenceNode *node, *first;
    
    g_return_val_if_fail (seq != NULL, NULL);
    
    check_seq_access (seq);
    
    node = node_new (data);
    first = node_get_first (seq->end_node);
    
    node_insert_before (first, node);
    
    return node;
}

GSequenceIter *
g_sequence_insert_before (GSequenceIter *iter,
			    gpointer         data)
{
    GSequenceNode *node;
    
    g_return_val_if_fail (iter != NULL, NULL);
    
    check_iter_access (iter);
    
    node = node_new (data);
    
    node_insert_before (iter, node);
    
    return node;
}

void
g_sequence_remove (GSequenceIter *iter)
{
    GSequence *seq;
    
    g_return_if_fail (iter != NULL);
    g_return_if_fail (!is_end (iter));
    
    check_iter_access (iter);
    
    seq = get_sequence (iter); 
    
    node_unlink (iter);
    node_free (iter, seq);
}

void
g_sequence_remove_range (GSequenceIter *begin,
			   GSequenceIter *end)
{
    g_return_if_fail (get_sequence (begin) == get_sequence (end));

    check_iter_access (begin);
    check_iter_access (end);
    
    g_sequence_move_range (NULL, begin, end);
}

#if 0
static void
print_node (GSequenceNode *node, int level)
{
    int i;

    for (i = 0; i < level; ++i)
	g_print ("  ");

    g_print ("%p\n", node);

    if (!node)
	return;
    
    print_node (node->left, level + 1);
    print_node (node->right, level + 1);
}

static GSequenceNode *
get_root (GSequenceNode *node)
{
    GSequenceNode *root;

    root = node;
    while (root->parent)
	root = root->parent;
    return root;
}

static void
print_tree (GSequence *seq)
{
    print_node (get_root (seq->end_node), 0);
}
#endif

/**
 * g_sequence_move_range:
 * @dest: 
 * @begin: 
 * @end: 
 * 
 * Insert a range at the destination pointed to by ptr. The @begin and
 * @end iters must point into the same sequence. It is allowed for @dest to
 * point to a different sequence than the one pointed into by @begin and
 * @end. If @dest is NULL, the range indicated by @begin and @end is
 * removed from the sequence. If @dest iter points to a place within
 * the (@begin, @end) range, the range stays put.
 * 
 * Since: 2.12
 **/
void
g_sequence_move_range (GSequenceIter *dest,
			 GSequenceIter *begin,
			 GSequenceIter *end)
{
    GSequence *src_seq;
    GSequenceNode *first;
    
    g_return_if_fail (begin != NULL);
    g_return_if_fail (end != NULL);
    
    check_iter_access (begin);
    check_iter_access (end);
    if (dest)
	check_iter_access (dest);
    
    src_seq = get_sequence (begin);
    
    g_return_if_fail (src_seq == get_sequence (end));

#if 0
    if (dest && get_sequence (dest) == src_seq)
    {
	g_return_if_fail ((g_sequence_iter_compare (dest, begin) <= 0)  ||
			  (g_sequence_iter_compare (end, dest) <= 0));
    }
#endif

    /* Dest points to begin or end? */
    if (dest == begin || dest == end)
	return;

    /* begin comes after end? */
    if (g_sequence_iter_compare (begin, end) >= 0)
	return;

    /* dest points somewhere in the (begin, end) range? */
    if (dest && get_sequence (dest) == src_seq &&
	g_sequence_iter_compare (dest, begin) > 0 &&
	g_sequence_iter_compare (dest, end) < 0)
    {
	return;
    }
    
    src_seq = get_sequence (begin);

    first = node_get_first (begin);

    node_cut (begin);

    node_cut (end);

    if (first != begin)
	node_insert_after (node_get_last (first), end);

    if (dest)
	node_insert_before (dest, begin);
    else
	node_free (begin, src_seq);
}

typedef struct
{
    GCompareDataFunc    cmp_func;
    gpointer		cmp_data;
    GSequenceNode	*end_node;
} SortInfo;

/* This function compares two iters using a normal compare
 * function and user_data passed in in a SortInfo struct
 */
static gint
iter_compare (GSequenceIter *node1,
	      GSequenceIter *node2,
	      gpointer data)
{
    const SortInfo *info = data;
    gint retval;
    
    if (node1 == info->end_node)
	return 1;
    
    if (node2 == info->end_node)
	return -1;
    
    retval = info->cmp_func (node1->data, node2->data, info->cmp_data);
    
    /* If the nodes are different, but the user supplied compare function
     * compares them equal, then force an arbitrary (but consistent) order
     * on them, so that our sorts will be stable
     */
    if (retval == 0 && node1 != node2)
    {
	if (node1 > node2)
	    return 1;
	else
	    return -1;
    }
    
    return retval;
}

void
g_sequence_sort (GSequence      *seq,
		   GCompareDataFunc  cmp_func,
		   gpointer          cmp_data)
{
    SortInfo info = { cmp_func, cmp_data, seq->end_node };
    
    check_seq_access (seq);
    
    g_sequence_sort_iter (seq, iter_compare, &info);
}

/**
 * g_sequence_insert_sorted:
 * @seq: a #GSequence
 * @data: the data to insert
 * @cmp_func: the #GCompareDataFunc used to compare elements in the queue. It is
 *     called with two elements of the @seq and @user_data. It should
 *     return 0 if the elements are equal, a negative value if the first
 *     element comes before the second, and a positive value if the second
 *     element comes before the first.
 * @cmp_data: user data passed to @cmp_func.
 * 
 * Inserts @data into @queue using @func to determine the new position.
 * 
 * Since: 2.10
 **/
GSequenceIter *
g_sequence_insert_sorted (GSequence       *seq,
			    gpointer           data,
			    GCompareDataFunc   cmp_func,
			    gpointer           cmp_data)
{
    SortInfo info = { cmp_func, cmp_data, NULL };
    
    g_return_val_if_fail (seq != NULL, NULL);
    g_return_val_if_fail (cmp_func != NULL, NULL);
    
    info.end_node = seq->end_node;
    check_seq_access (seq);
    
    return g_sequence_insert_sorted_iter (seq, data, iter_compare, &info);
}

void
g_sequence_sort_changed (GSequenceIter  *iter,
			   GCompareDataFunc  cmp_func,
			   gpointer          cmp_data)
{
    SortInfo info = { cmp_func, cmp_data, NULL };
    
    g_return_if_fail (!is_end (iter));
    
    info.end_node = get_sequence (iter)->end_node;
    check_iter_access (iter);
    
    g_sequence_sort_changed_iter (iter, iter_compare, &info);
}

void
g_sequence_sort_iter (GSequence                *seq,
			GSequenceIterCompareFunc  cmp_func,
			gpointer		    cmp_data)
{
    GSequence *tmp;
    GSequenceNode *begin, *end;
    
    g_return_if_fail (seq != NULL);
    g_return_if_fail (cmp_func != NULL);
    
    check_seq_access (seq);
    
    begin = g_sequence_get_begin_iter (seq);
    end   = g_sequence_get_end_iter (seq);
    
    tmp = g_sequence_new (NULL);
    
    g_sequence_move_range (g_sequence_get_begin_iter (tmp), begin, end);
    
    tmp->access_prohibited = TRUE;
    seq->access_prohibited = TRUE;
    
    while (g_sequence_get_length (tmp) > 0)
    {
	GSequenceNode *node = g_sequence_get_begin_iter (tmp);
	
	node_unlink (node);
	
	node_insert_sorted (seq->end_node, node, seq->end_node, cmp_func, cmp_data);
    }
    
    tmp->access_prohibited = FALSE;
    seq->access_prohibited = FALSE;
    
    g_sequence_free (tmp);
}

void
g_sequence_sort_changed_iter (GSequenceIter            *iter,
				GSequenceIterCompareFunc  iter_cmp,
				gpointer		    cmp_data)
{
    GSequence *seq;
    
    g_return_if_fail (!is_end (iter));
    
    check_iter_access (iter);
    
    seq = get_sequence (iter);
    
    seq->access_prohibited = TRUE;
    
    node_unlink (iter);
    node_insert_sorted (seq->end_node, iter, seq->end_node, iter_cmp, cmp_data);
    
    seq->access_prohibited = FALSE;
}

GSequenceIter *
g_sequence_insert_sorted_iter   (GSequence                *seq,
				   gpointer                    data,
				   GSequenceIterCompareFunc  iter_cmp,
				   gpointer		       cmp_data)
{
    GSequenceNode *new_node;
    
    check_seq_access (seq);
    
    new_node = node_new (data);
    node_insert_sorted (seq->end_node, new_node, seq->end_node, iter_cmp, cmp_data);
    return new_node;
}

GSequenceIter *
g_sequence_search_iter (GSequence                *seq,
			  gpointer                    data,
			  GSequenceIterCompareFunc  cmp_func,
			  gpointer                    cmp_data)
{
    GSequenceNode *node;
    GSequenceNode *dummy;
    
    g_return_val_if_fail (seq != NULL, NULL);
    
    check_seq_access (seq);
    
    seq->access_prohibited = TRUE;

    dummy = node_new (data);
    
    node = node_find_closest (seq->end_node, dummy, seq->end_node, cmp_func, cmp_data);

    node_free (dummy, NULL);
    
    seq->access_prohibited = FALSE;
    
    return node;
}

/**
 * g_sequence_search:
 * @seq: 
 * @data: 
 * @cmp_func: 
 * @cmp_data: 
 * 
 * Returns an iterator pointing to the position where @data would
 * be inserted according to @cmp_func and @cmp_data.
 * 
 * Return value: 
 * 
 * Since: 2.6
 **/
GSequenceIter *
g_sequence_search (GSequence      *seq,
		     gpointer          data,
		     GCompareDataFunc  cmp_func,
		     gpointer          cmp_data)
{
    SortInfo info = { cmp_func, cmp_data, NULL };
    
    g_return_val_if_fail (seq != NULL, NULL);
    
    info.end_node = seq->end_node;
    check_seq_access (seq);
    
    return g_sequence_search_iter (seq, data, iter_compare, &info);
}

GSequence *
g_sequence_iter_get_sequence (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, NULL);
    
    return get_sequence (iter);
}

gpointer
g_sequence_get (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (!is_end (iter), NULL);
    
    return iter->data;
}

void
g_sequence_set (GSequenceIter *iter,
		  gpointer         data)
{
    GSequence *seq;
    
    g_return_if_fail (iter != NULL);
    g_return_if_fail (!is_end (iter));
    
    seq = get_sequence (iter);

    /* If @data is identical to iter->data, it is destroyed
     * here. This will work right in case of ref-counted objects. Also
     * it is similar to what ghashtables do.
     *
     * For non-refcounted data it's a little less convenient, but
     * code relying on self-setting not destroying would be
     * pretty dubious anyway ...
     */
    
    if (seq->data_destroy_notify)
	seq->data_destroy_notify (iter->data);
    
    iter->data = data;
}

gint
g_sequence_get_length (GSequence *seq)
{
    return node_get_length (seq->end_node) - 1;
}

GSequenceIter *
g_sequence_get_end_iter (GSequence *seq)
{
    g_return_val_if_fail (seq != NULL, NULL);
    
    g_assert (is_end (seq->end_node));
    
    return seq->end_node;
}

GSequenceIter *
g_sequence_get_begin_iter (GSequence *seq)
{
    g_return_val_if_fail (seq != NULL, NULL);
    return node_get_first (seq->end_node);
}

static int
clamp_position (GSequence *seq,
		int          pos)
{
    gint len = g_sequence_get_length (seq);
    
    if (pos > len || pos < 0)
	pos = len;
    
    return pos;
}

/*
 * if pos > number of items or -1, will return end pointer
 */
GSequenceIter *
g_sequence_get_iter_at_pos (GSequence *seq,
			      gint         pos)
{
    g_return_val_if_fail (seq != NULL, NULL);
    
    pos = clamp_position (seq, pos);
    
    return node_get_by_pos (seq->end_node, pos);
}

void
g_sequence_move (GSequenceIter *src,
		   GSequenceIter *dest)
{
    g_return_if_fail (src != NULL);
    g_return_if_fail (dest != NULL);
    g_return_if_fail (!is_end (src));
    
    if (src == dest)
	return;
    
    node_unlink (src);
    node_insert_before (dest, src);
}

/* GSequenceIter * */
gboolean
g_sequence_iter_is_end (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, FALSE);
    
    return is_end (iter);
}

gboolean
g_sequence_iter_is_begin (GSequenceIter *iter)
{
    return (node_get_prev (iter) == iter);
}

gint
g_sequence_iter_get_position (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, -1);
    
    return node_get_pos (iter);
}

GSequenceIter *
g_sequence_iter_next (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, NULL);
    
    return node_get_next (iter);
}

GSequenceIter *
g_sequence_iter_prev (GSequenceIter *iter)
{
    g_return_val_if_fail (iter != NULL, NULL);
    
    return node_get_prev (iter);
}

GSequenceIter *
g_sequence_iter_move (GSequenceIter *iter,
			gint             delta)
{
    gint new_pos;
    
    g_return_val_if_fail (iter != NULL, NULL);
    
    new_pos = node_get_pos (iter) + delta;
    
    new_pos = clamp_position (get_sequence (iter), new_pos);
    
    return node_get_by_pos (iter, new_pos);
}

void
g_sequence_swap (GSequenceIter *a,
		   GSequenceIter *b)
{
    GSequenceNode *leftmost, *rightmost, *rightmost_next;
    int a_pos, b_pos;
    
    g_return_if_fail (!g_sequence_iter_is_end (a));
    g_return_if_fail (!g_sequence_iter_is_end (b));
    
    if (a == b)
	return;
    
    a_pos = g_sequence_iter_get_position (a);
    b_pos = g_sequence_iter_get_position (b);
    
    if (a_pos > b_pos)
    {
	leftmost = b;
	rightmost = a;
    }
    else
    {
	leftmost = a;
	rightmost = b;
    }
    
    rightmost_next = node_get_next (rightmost);
    
    /* Situation is now like this:
     *
     *     ..., leftmost, ......., rightmost, rightmost_next, ...
     *
     */
    g_sequence_move (rightmost, leftmost);
    g_sequence_move (leftmost, rightmost_next);
}

#if 0
/* aggregates */
void
g_sequence_set_aggregate (GSequence               *seq,
			    GSequenceAggregateFunc   f,
			    gpointer                   data,
			    GDestroyNotify             destroy)
{
    /* FIXME */
}

void
g_sequence_set_aggregate_data (GSequenceIter *            iter,
				 const gchar             *aggregate,
				 gpointer                 data)
{
    /* FIXME */
    
}

gpointer
g_sequence_get_aggregate_data (GSequenceIter *            begin,
				 GSequenceIter *            end,
				 const gchar             *aggregate)
{
    g_assert_not_reached();
    return NULL;
}
#endif



/*
 * Implementation of the node_* methods
 */
static void
node_update_fields (GSequenceNode *node)
{
    g_assert (node != NULL);
    
    node->n_nodes = 1;
    
    if (node->left)
	node->n_nodes += node->left->n_nodes;
    
    if (node->right)
	node->n_nodes += node->right->n_nodes;
    
#if 0
    if (node->left || node->right)
	g_assert (node->n_nodes > 1);
#endif
}

#define NODE_LEFT_CHILD(n)  (((n)->parent) && ((n)->parent->left) == (n))
#define NODE_RIGHT_CHILD(n) (((n)->parent) && ((n)->parent->right) == (n))

static void
node_rotate (GSequenceNode *node)
{
    GSequenceNode *tmp, *old;
    
    g_assert (node->parent);
    g_assert (node->parent != node);
    
    if (NODE_LEFT_CHILD (node))
    {
	/* rotate right */
	tmp = node->right;
	
	node->right = node->parent;
	node->parent = node->parent->parent;
	if (node->parent)
	{
	    if (node->parent->left == node->right)
		node->parent->left = node;
	    else
		node->parent->right = node;
	}
	
	g_assert (node->right);
	
	node->right->parent = node;
	node->right->left = tmp;
	
	if (node->right->left)
	    node->right->left->parent = node->right;
	
	old = node->right;
    }
    else
    {
	/* rotate left */
	tmp = node->left;
	
	node->left = node->parent;
	node->parent = node->parent->parent;
	if (node->parent)
	{
	    if (node->parent->right == node->left)
		node->parent->right = node;
	    else
		node->parent->left = node;
	}
	
	g_assert (node->left);
	
	node->left->parent = node;
	node->left->right = tmp;
	
	if (node->left->right)
	    node->left->right->parent = node->left;
	
	old = node->left;
    }
    
    node_update_fields (old);
    node_update_fields (node);
}

static GSequenceNode *
splay (GSequenceNode *node)
{
    while (node->parent)
    {
	if (!node->parent->parent)
	{
	    /* zig */
	    node_rotate (node);
	}
	else if ((NODE_LEFT_CHILD (node) && NODE_LEFT_CHILD (node->parent)) ||
		 (NODE_RIGHT_CHILD (node) && NODE_RIGHT_CHILD (node->parent)))
	{
	    /* zig-zig */
	    node_rotate (node->parent);
	    node_rotate (node);
	}
	else
	{
	    /* zig-zag */
	    node_rotate (node);
	    node_rotate (node);
	}
    }
    
    return node;
}

static GSequenceNode *
node_new (gpointer          data)
{
    GSequenceNode *node = g_new0 (GSequenceNode, 1);
    
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
    
    node->data = data;
    node->n_nodes = 1;
    
    return node;
}

static GSequenceNode *
find_min (GSequenceNode *node)
{
    splay (node);
    
    while (node->left)
	node = node->left;
    
    return node;
}

static GSequenceNode *
find_max (GSequenceNode *node)
{
    splay (node);
    
    while (node->right)
	node = node->right;
    
    return node;
}

static GSequenceNode *
node_get_first   (GSequenceNode    *node)
{
    return splay (find_min (node));
}

static GSequenceNode *
node_get_last    (GSequenceNode    *node)
{
    return splay (find_max (node));
}

static gint
get_n_nodes (GSequenceNode *node)
{
    if (node)
	return node->n_nodes;
    else
	return 0;
}

static GSequenceNode *
node_get_by_pos  (GSequenceNode *node,
		  gint             pos)
{
    gint i;
    
    g_assert (node != NULL);
    
    splay (node);
    
    while ((i = get_n_nodes (node->left)) != pos)
    {
	if (i < pos)
	{
	    node = node->right;
	    pos -= (i + 1);
	}
	else
	{
	    node = node->left;
	    g_assert (node->parent != NULL);
	}
    }
    
    return splay (node);
}

static GSequenceNode *
node_get_prev  (GSequenceNode    *node)
{
    splay (node);
    
    if (node->left)
    {
	node = node->left;
	while (node->right)
	    node = node->right;
    }
    
    return splay (node);
}

static GSequenceNode *
node_get_next         (GSequenceNode    *node)
{
    splay (node);
    
    if (node->right)
    {
	node = node->right;
	while (node->left)
	    node = node->left;
    }
    
    return splay (node);
}

static gint
node_get_pos (GSequenceNode    *node)
{
    splay (node);
    
    return get_n_nodes (node->left);
}

/* Return closest node bigger than @needle (does always exist because there
 * is an end_node)
 */
static GSequenceNode *
node_find_closest (GSequenceNode	      *haystack,
		   GSequenceNode	      *needle,
		   GSequenceNode            *end,
		   GSequenceIterCompareFunc  cmp_func,
		   gpointer		       cmp_data)
{
    GSequenceNode *best;
    gint c;
    
    g_assert (haystack);
    
    haystack = splay (haystack);
    
    do
    {
	best = haystack;
	
	if (haystack == end)
	    c = 1;
	else
	    c = cmp_func (haystack, needle, cmp_data);
	
	if (c > 0)
	    haystack = haystack->left;
	else if (c < 0)
	    haystack = haystack->right;
    }
    while (c != 0 && haystack != NULL);
    
    /* If the best node is smaller than the data, then move one step
     * to the right
     */
    if (!is_end (best) && c < 0)
	best = node_get_next (best);
    
    return best;
}

static void
node_free (GSequenceNode *node,
	   GSequence     *seq)
{
    GQueue *stack = g_queue_new ();

    splay (node);
    
    g_queue_push_head (stack, node);
    
    while (!g_queue_is_empty (stack))
    {
	node = g_queue_pop_head (stack);
	
	if (node)
	{
	    g_queue_push_head (stack, node->right);
	    g_queue_push_head (stack, node->left);
	    
	    if (seq && seq->data_destroy_notify && node != seq->end_node)
		seq->data_destroy_notify (node->data);
	    
	    g_free (node);
	}
    }
    
    g_queue_free (stack);
}

/* Splits into two trees, left and right. 
 * @node will be part of the right tree
 */

static void
node_cut (GSequenceNode *node)
{
    splay (node);

    g_assert (node->parent == NULL);
    
    if (node->left)
	node->left->parent = NULL;
    
    node->left = NULL;
    node_update_fields (node);
}

static void
node_insert_before (GSequenceNode *node,
		    GSequenceNode *new)
{
    g_assert (node != NULL);
    g_assert (new != NULL);
    
    splay (node);
    
    new = splay (find_min (new));
    g_assert (new->left == NULL);
    
    if (node->left)
	node->left->parent = new;
    
    new->left = node->left;
    new->parent = node;
    
    node->left = new;
    
    node_update_fields (new);
    node_update_fields (node);
}

static void
node_insert_after (GSequenceNode *node,
		   GSequenceNode *new)
{
    g_assert (node != NULL);
    g_assert (new != NULL);
    
    splay (node);
    
    new = splay (find_max (new));
    g_assert (new->right == NULL);
    g_assert (node->parent == NULL);
    
    if (node->right)
	node->right->parent = new;
    
    new->right = node->right;
    new->parent = node;
    
    node->right = new;
    
    node_update_fields (new);
    node_update_fields (node);
}

static gint
node_get_length (GSequenceNode    *node)
{
    g_assert (node != NULL);
    
    splay (node);
    return node->n_nodes;
}

static void
node_unlink (GSequenceNode *node)
{
    GSequenceNode *right, *left;
    
    splay (node);
    
    left = node->left;
    right = node->right;
    
    node->parent = node->left = node->right = NULL;
    node_update_fields (node);
    
    if (right)
    {
	right->parent = NULL;
	
	right = node_get_first (right);
	g_assert (right->left == NULL);
	
	right->left = left;
	if (left)
	{
	    left->parent = right;
	    node_update_fields (right);
	}
    }
    else if (left)
    {
	left->parent = NULL;
    }
}

static void
node_insert_sorted (GSequenceNode *node,
		    GSequenceNode *new,
		    GSequenceNode *end,
		    GSequenceIterCompareFunc cmp_func,
		    gpointer cmp_data)
{
    GSequenceNode *closest;
    
    closest = node_find_closest (node, new, end, cmp_func, cmp_data);
    
    node_insert_before (closest, new);
}

static gint
node_calc_height (GSequenceNode *node)
{
    gint left_height;
    gint right_height;
    
    if (node)
    {
	left_height = 0;
	right_height = 0;
	
	if (node->left)
	    left_height = node_calc_height (node->left);
	
	if (node->right)
	    right_height = node_calc_height (node->right);
	
	return MAX (left_height, right_height) + 1;
    }
    
    return 0;
}

gint
g_sequence_calc_tree_height   (GSequence               *seq)
{
    GSequenceNode *node = seq->end_node;
    gint r, l;
    while (node->parent)
	node = node->parent;
    
    if (node)
    {
	r = node_calc_height (node->right);
	l = node_calc_height (node->left);
	
	return MAX (r, l) + 1;
    }
    else
	return 0;
}

static void
check_node (GSequenceNode *node)
{
    if (node)
    {
	g_assert (node->parent != node);
	g_assert (node->n_nodes ==
		  1 + get_n_nodes (node->left) + get_n_nodes (node->right));
	check_node (node->left);
	check_node (node->right);
    }
}

void
g_sequence_self_test (GSequence *seq)
{
    GSequenceNode *node = splay (seq->end_node);
    
    check_node (node);
}

#if 0
void
g_sequence_set_aggregator   (GSequence                  *seq,
			       GSequenceAggregateFunction  func,
			       gpointer			     data,
			       GDestroyNotify                destroy)
{
    
}

gconstpointer g_sequence_get_aggregate    (GSequenceIter *              begin,
					     GSequenceIter *              end);
void          g_sequence_update_aggregate (GSequenceIter *              iter);
#endif
