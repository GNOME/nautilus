/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2002  Soeren Sandmann (sandmann@daimi.au.dk)
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

struct _GSequence {
    GSequenceNode *node;	/* does not necessarily point to the root.
				 * You can splay it if you want it to
				 */
    GDestroyNotify data_destroy_notify;
};

struct _GSequenceNode {
    guint is_end  : 1;
    gint  n_nodes : 31;		/* number of nodes below this node,
				 * including this node
				 */
    GSequenceNode *parent;
    GSequenceNode *left;
    GSequenceNode *right;

    GSequence *sequence;
    
    gpointer data;
};

static GSequenceNode *g_sequence_node_new          (gpointer          data);
static GSequenceNode *g_sequence_node_find_first   (GSequenceNode    *node);
static GSequenceNode *g_sequence_node_find_last    (GSequenceNode    *node);
static GSequenceNode *g_sequence_node_find_by_pos  (GSequenceNode    *node,
						    gint pos);
static GSequenceNode *g_sequence_node_prev         (GSequenceNode    *node);
static GSequenceNode *g_sequence_node_next         (GSequenceNode    *node);
static gint           g_sequence_node_get_pos (GSequenceNode    *node);
static GSequence     *g_sequence_node_get_sequence (GSequenceNode    *node);
static GSequenceNode *g_sequence_node_find_closest (GSequenceNode    *node,
						    GSequenceNode    *other,
						    GCompareDataFunc  cmp,
						    gpointer          data);
static gint           g_sequence_node_get_length   (GSequenceNode    *node);
static void           g_sequence_node_free         (GSequenceNode    *node,
						    GDestroyNotify    destroy);
#if 0
static gboolean       g_sequence_node_is_singleton (GSequenceNode    *node);
#endif
static void           g_sequence_node_split        (GSequenceNode    *node,
						    GSequenceNode   **left,
						    GSequenceNode   **right);
static void           g_sequence_node_insert_before (GSequenceNode *node,
						     GSequenceNode *new);
static void           g_sequence_node_remove        (GSequenceNode *node);
static void           g_sequence_node_insert_sorted (GSequenceNode *node,
						     GSequenceNode *new,
						     GCompareDataFunc cmp_func,
						     gpointer cmp_data);


/* GSequence */
GSequence *
g_sequence_new                (GDestroyNotify           data_destroy)
{
    GSequence *seq = g_new (GSequence, 1);
    seq->data_destroy_notify = data_destroy;

    seq->node = g_sequence_node_new (NULL);
    seq->node->is_end = TRUE;
    seq->node->sequence = seq;
    
    return seq;
}

void
g_sequence_free               (GSequence               *seq)
{
    g_return_if_fail (seq != NULL);

    g_sequence_node_free (seq->node, seq->data_destroy_notify);

    g_free (seq);
}

#if 0
static void
flatten_nodes (GSequenceNode *node, GList **list)
{
    g_print ("flatten %p\n", node);
    if (!node)
	return;
    else if (g_sequence_node_is_singleton (node))
	*list = g_list_prepend (*list, node);
    else
    {
	GSequenceNode *left;
	GSequenceNode *right;

	g_sequence_node_split (node, &left, &right);

	flatten_nodes (left, list);
	flatten_nodes (right, list);
    }
}
#endif

typedef struct SortInfo SortInfo;
struct SortInfo {
    GCompareDataFunc cmp;
    gpointer data;
};

static gint
node_compare (gconstpointer n1, gconstpointer n2, gpointer data)
{
    SortInfo *info = data;
    const GSequenceNode *node1 = n1;
    const GSequenceNode *node2 = n2;

    if (node1->is_end)
	return 1;
    else if (node2->is_end)
	return -1;
    else
	return (* info->cmp) (node1->data, node2->data, info->data);
}

void
g_sequence_append             (GSequence               *seq,
			       gpointer                 data)
{
    GSequenceNode *node, *last;

    g_return_if_fail (seq != NULL);

    node = g_sequence_node_new (data);
    node->sequence = seq;
    last = g_sequence_node_find_last (seq->node);
    g_sequence_node_insert_before (last, node);
}

void
g_sequence_prepend            (GSequence               *seq,
			       gpointer                 data)
{
    GSequenceNode *node, *second;

    g_return_if_fail (seq != NULL);
    
    node = g_sequence_node_new (data);
    node->sequence = seq;
    second = g_sequence_node_next (g_sequence_node_find_first (seq->node));
    
    g_sequence_node_insert_before (second, node);
}

void
g_sequence_insert             (GSequencePtr             ptr,
			       gpointer                 data)
{
    GSequenceNode *node;
    
    g_return_if_fail (ptr != NULL);

    node = g_sequence_node_new (data);
    node->sequence = ptr->sequence;

    g_sequence_node_insert_before (ptr, node);
}

static void
g_sequence_unlink (GSequence *seq,
		   GSequenceNode *node)
{
    g_assert (!node->is_end);

    seq->node = g_sequence_node_next (node);

    g_assert (seq->node);
    g_assert (seq->node != node);

    g_sequence_node_remove (node);
}

void
g_sequence_remove             (GSequencePtr             ptr)
{
    GSequence *seq;
    
    g_return_if_fail (ptr != NULL);
    g_return_if_fail (!ptr->is_end);

    seq = g_sequence_node_get_sequence (ptr); 
    g_sequence_unlink (seq, ptr);
    g_sequence_node_free (ptr, seq->data_destroy_notify);
}

void
g_sequence_sort               (GSequence               *seq,
			       GCompareDataFunc         cmp_func,
			       gpointer                 cmp_data)
{
    GSequence *tmp;
    GSequenceNode *begin, *end;

    g_return_if_fail (seq != NULL);
    g_return_if_fail (cmp_func != NULL);

    begin = g_sequence_get_begin_ptr (seq);
    end   = g_sequence_get_end_ptr (seq);

    g_sequence_remove_range (begin, end, &tmp);

    while (g_sequence_get_length (tmp) > 0)
    {
	GSequenceNode *node = g_sequence_get_begin_ptr (tmp);
	g_sequence_unlink (tmp, node);

	g_sequence_node_insert_sorted (seq->node, node, cmp_func, cmp_data);
    }

    g_sequence_free (tmp);
}

gpointer
g_sequence_ptr_get_data           (GSequencePtr             ptr)
{
    g_return_val_if_fail (ptr != NULL, NULL);
    g_return_val_if_fail (!ptr->is_end, NULL);

    return ptr->data;
}

GSequencePtr
g_sequence_insert_sorted      (GSequence               *seq,
			       gpointer                 data,
			       GCompareDataFunc         cmp_func,
			       gpointer                 cmp_data)
{
    GSequenceNode *new_node = g_sequence_node_new (data);
    new_node->sequence = seq;
    g_sequence_node_insert_sorted (seq->node, new_node, cmp_func, cmp_data);
    return new_node;
}

void
g_sequence_insert_sequence    (GSequencePtr             ptr,
			       GSequence               *other_seq)
{
    GSequenceNode *last;

    g_return_if_fail (other_seq != NULL);
    g_return_if_fail (ptr != NULL);

    last = g_sequence_node_find_last (other_seq->node);
    g_sequence_node_insert_before (ptr, last);
    g_sequence_node_remove (last);
    g_sequence_node_free (last, NULL);
    other_seq->node = NULL;
    g_sequence_free (other_seq);
}

void
g_sequence_concatenate        (GSequence               *seq1,
			       GSequence               *seq2)
{
    GSequenceNode *last;

    g_return_if_fail (seq1 != NULL);
    g_return_if_fail (seq2 != NULL);
    
    last = g_sequence_node_find_last (seq1->node);
    g_sequence_insert_sequence (last, seq2);
}

/*
 * The new sequence inherits the destroy notify from the sequence that
 * beign and end comes from
 */
void
g_sequence_remove_range       (GSequencePtr             begin,
			       GSequencePtr             end,
			       GSequence              **removed)
{
    GSequence *seq;
    GSequenceNode *s1, *s2, *s3;

    seq = g_sequence_node_get_sequence (begin);

    g_assert (end != NULL);
    
    g_return_if_fail (seq == g_sequence_node_get_sequence (end));

    g_sequence_node_split (begin, &s1, &s2);
    g_sequence_node_split (end, NULL, &s3);

    if (s1)
	g_sequence_node_insert_before (s3, s1);

    seq->node = s3;

    if (removed)
    {
	*removed = g_sequence_new (seq->data_destroy_notify);
	g_sequence_node_insert_before ((*removed)->node, s2);
    }
    else
    {
	g_sequence_node_free (s2, seq->data_destroy_notify);
    }
}

gint
g_sequence_get_length         (GSequence               *seq)
{
    return g_sequence_node_get_length (seq->node) - 1;
}

GSequencePtr
g_sequence_get_end_ptr        (GSequence               *seq)
{
    g_return_val_if_fail (seq != NULL, NULL);
    return g_sequence_node_find_last (seq->node);
}

GSequencePtr
g_sequence_get_begin_ptr      (GSequence               *seq)
{
    g_return_val_if_fail (seq != NULL, NULL);
    return g_sequence_node_find_first (seq->node);
}

/*
 * if pos > number of items or -1, will return end pointer
 */
GSequencePtr
g_sequence_get_ptr_at_pos     (GSequence               *seq,
			       gint                     pos)
{
    gint len;
    
    g_return_val_if_fail (seq != NULL, NULL);

    len = g_sequence_get_length (seq);

    if (pos > len || pos == -1)
	pos = len;

    return g_sequence_node_find_by_pos (seq->node, pos);
}


/* GSequencePtr */
gboolean
g_sequence_ptr_is_end         (GSequencePtr             ptr)
{
    g_return_val_if_fail (ptr != NULL, FALSE);
    return ptr->is_end;
}

gboolean
g_sequence_ptr_is_begin       (GSequencePtr             ptr)
{
    return (g_sequence_node_prev (ptr) == NULL);
}

gint
g_sequence_ptr_get_position   (GSequencePtr             ptr)
{
    g_return_val_if_fail (ptr != NULL, -1);

    return g_sequence_node_get_pos (ptr);
}

GSequencePtr
g_sequence_ptr_next           (GSequencePtr             ptr)
{
    g_return_val_if_fail (ptr != NULL, NULL);

    return g_sequence_node_next (ptr);
}

GSequencePtr
g_sequence_ptr_prev           (GSequencePtr             ptr)
{
    g_return_val_if_fail (ptr != NULL, NULL);

    return g_sequence_node_prev (ptr);
}

GSequencePtr
g_sequence_ptr_move           (GSequencePtr             ptr,
			       guint                    delta)
{
    gint new_pos;
    
    g_return_val_if_fail (ptr != NULL, NULL);

    new_pos = g_sequence_node_get_pos (ptr) + delta;
    
    return g_sequence_node_find_by_pos (ptr, new_pos);
}

void
g_sequence_ptr_sort_changed  (GSequencePtr	     ptr,
			      GCompareDataFunc	     cmp_func,
			      gpointer		     cmp_data)
{
    GSequence *seq;
    
    g_return_if_fail (!ptr->is_end);
    
    seq = g_sequence_node_get_sequence (ptr); 
    g_sequence_unlink (seq, ptr);
    g_sequence_node_insert_sorted (seq->node, ptr, cmp_func, cmp_data);
}

/* search
 *
 * The only restriction on the search function is that it
 * must not delete any nodes. It is permitted to insert new nodes,
 * but the caller should "know what he is doing"
 */
void
g_sequence_search             (GSequence               *seq,
			       GSequenceSearchFunc      f,
			       gpointer                 data)
{
    GQueue *intervals = g_queue_new ();

    g_queue_push_tail (intervals, g_sequence_node_find_first (seq->node));
    g_queue_push_tail (intervals, g_sequence_node_find_last (seq->node));

    while (!g_queue_is_empty (intervals))
    {
	GSequenceNode *begin = g_queue_pop_head (intervals);
	GSequenceNode *end   = g_queue_pop_head (intervals);
	
	if (f (begin, end, data))
	{
	    gint begin_pos = g_sequence_node_get_pos (begin);
	    gint end_pos   = g_sequence_node_get_pos (end);

	    if (end_pos - begin_pos > 1)
	    {
		GSequenceNode *mid;
		gint mid_pos;

		mid_pos = begin_pos + (end_pos - begin_pos) / 2;
		mid = g_sequence_node_find_by_pos (begin, mid_pos);

		g_queue_push_tail (intervals, begin);
		g_queue_push_tail (intervals, mid);

		g_queue_push_tail (intervals, mid);
		g_queue_push_tail (intervals, end);
	    }
	}
    }

    g_queue_free (intervals);
}



#if 0
/* aggregates */
void
g_sequence_add_aggregate      (GSequence               *seq,
			       const gchar             *aggregate,
			       GSequenceAggregateFunc   f,
			       gpointer                 data,
			       GDestroyNotify           destroy)
{
    /* FIXME */
}

void
g_sequence_remove_aggregate   (GSequence               *seq,
			       const gchar              aggregate)
{
    /* FIXME */

}

void
g_sequence_set_aggregate_data (GSequencePtr             ptr,
			       const gchar             *aggregate,
			       gpointer                 data)
{
    /* FIXME */
    
}

gpointer
g_sequence_get_aggregate_data (GSequencePtr             begin,
			       GSequencePtr             end,
			       const gchar             *aggregate)
{
    g_assert_not_reached();
    return NULL;
}
#endif


/* Nodes
 */
static void
g_sequence_node_update_fields (GSequenceNode *node)
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
g_sequence_node_rotate (GSequenceNode *node)
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
    
    g_sequence_node_update_fields (old);
    g_sequence_node_update_fields (node);
}

static GSequenceNode *
splay (GSequenceNode *node)
{
    while (node->parent)
    {
	if (!node->parent->parent)
	{
	    /* zig */
	    g_sequence_node_rotate (node);
	}
	else if ((NODE_LEFT_CHILD (node) && NODE_LEFT_CHILD (node->parent)) ||
		 (NODE_RIGHT_CHILD (node) && NODE_RIGHT_CHILD (node->parent)))
	{
	    /* zig-zig */
	    g_sequence_node_rotate (node->parent);
	    g_sequence_node_rotate (node);
	}
	else
	{
	    /* zig-zag */
	    g_sequence_node_rotate (node);
	    g_sequence_node_rotate (node);
	}
    }

    return node;
}

static GSequenceNode *
g_sequence_node_new (gpointer          data)
{
    GSequenceNode *node = g_new0 (GSequenceNode, 1);

    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;

    node->data = data;
    node->is_end = FALSE;
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
g_sequence_node_find_first   (GSequenceNode    *node)
{
    return splay (find_min (node));
}

static GSequenceNode *
g_sequence_node_find_last    (GSequenceNode    *node)
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
g_sequence_node_find_by_pos  (GSequenceNode    *node,
			      gint              pos)
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
g_sequence_node_prev         (GSequenceNode    *node)
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
g_sequence_node_next         (GSequenceNode    *node)
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
g_sequence_node_get_pos (GSequenceNode    *node)
{
    splay (node);

    return get_n_nodes (node->left);
}

static GSequence *
g_sequence_node_get_sequence (GSequenceNode    *node)
{
    splay (node);

    return node->sequence;
}

static GSequenceNode *
g_sequence_node_find_closest (GSequenceNode    *node,
			      GSequenceNode    *other,
			      GCompareDataFunc  cmp,
			      gpointer          data)
{
    GSequenceNode *best;
    gint c;

    splay (node);
    
    do
    {
	best = node;

	if ((c = cmp (node, other, data)) != 0)
	{
	    if (c < 0)
		node = node->right;
	    else
		node = node->left;
	}
    }
    while (c != 0 && node != NULL);

    return best;
}

static void
g_sequence_node_free         (GSequenceNode    *node,
			      GDestroyNotify    destroy)
{
    /* FIXME:
     *
     * This is to avoid excessively deep recursions. A splay tree is not necessarily
     * balanced at all.
     *
     * I _think_ this is still linear in the number of nodes, but I'd like to
     * do something more efficient.
     */

    while (node)
    {
	GSequenceNode *next;

	node = splay (find_min (node));
	next = node->right;
	if (next)
	    next->parent = NULL;

	if (destroy && !node->is_end)
	    destroy (node->data);
	g_free (node);
	
	node = next;
    }
}

#if 0
static gboolean
g_sequence_node_is_singleton (GSequenceNode    *node)
{
    splay (node);

    if (node->left || node->right)
	return FALSE;

    return TRUE;
}
#endif

static void
g_sequence_node_split        (GSequenceNode    *node,
			      GSequenceNode   **left,
			      GSequenceNode   **right)
{
    GSequenceNode *left_tree;
    
    splay (node);

    left_tree = node->left;
    if (left_tree)
    {
	left_tree->parent = NULL;
	g_sequence_node_update_fields (left_tree);
    }
    
    node->left = NULL;
    g_sequence_node_update_fields (node);

    if (left)
	*left = left_tree;

    if (right)
	*right = node;
}

static void
g_sequence_node_insert_before (GSequenceNode *node,
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

    g_sequence_node_update_fields (new);
    g_sequence_node_update_fields (node);
}

static gint
g_sequence_node_get_length (GSequenceNode    *node)
{
    g_assert (node != NULL);
    
    splay (node);
    return node->n_nodes;
}

static void
g_sequence_node_remove        (GSequenceNode *node)
{
    GSequenceNode *right, *left;
    
    splay (node);

    left = node->left;
    right = node->right;

    node->left = node->right = NULL;

    if (right)
    {
	right->parent = node->parent;
	right->left = left;
	if (left)
	    left->parent = right;
    }
    else if (left)
	left->parent = node->parent;
}


#if 0
/* debug func */
static gint
g_sequence_node_calc_height (GSequenceNode *node)
{
    /* breadth first traversal */
    gint height = 0;
    GQueue *nodes = g_queue_new ();

    g_queue_push_tail (nodes, node);

    while (!g_queue_is_empty (nodes))
    {
	GQueue *tmp = g_queue_new ();

	height++;
	while (!g_queue_is_empty (nodes))
	{
	    GSequenceNode *node = g_queue_pop_head (nodes);
	    if (node->left)
		g_queue_push_tail (tmp, node->left);
	    if (node->right)
		g_queue_push_tail (tmp, node->right);
	}

	g_queue_free (nodes);
	
	nodes = tmp;
    }
    g_queue_free (nodes);

    return height;
}
#endif

static void
g_sequence_node_insert_sorted (GSequenceNode *node,
			       GSequenceNode *new,
			       GCompareDataFunc cmp_func,
			       gpointer cmp_data)
{
    SortInfo info;
    GSequenceNode *closest;
    info.cmp = cmp_func;
    info.data = cmp_data;
    
    closest =
	g_sequence_node_find_closest (node, new, node_compare, &info);

    if (node_compare (new, closest, &info) > 0)
	closest = g_sequence_node_next (closest);

    /* this can never fail since we have a bigger-than-everything
     * end-node
     */
    g_assert (node_compare (new, closest, &info) <= 0);
    g_sequence_node_insert_before (closest, new);
}

static gint
g_sequence_node_calc_height (GSequenceNode *node)
{
  gint left_height;
  gint right_height;

  if (node)
  {
      left_height = 0;
      right_height = 0;

      if (node->left)
	left_height = g_sequence_node_calc_height (node->left);

      if (node->right)
	right_height = g_sequence_node_calc_height (node->right);

      return MAX (left_height, right_height) + 1;
  }

  return 0;
}

gint
g_sequence_calc_tree_height   (GSequence               *seq)
{
    GSequenceNode *node = seq->node;
    gint r, l;
    while (node->parent)
	node = node->parent;

    if (node)
    {
	r = g_sequence_node_calc_height (node->right);
	l = g_sequence_node_calc_height (node->left);

	return MAX (r, l) + 1;
    }
    else
	return 0;
}

