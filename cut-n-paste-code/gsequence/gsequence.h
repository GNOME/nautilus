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

#ifndef __GSEQUENCE_H__
#define __GSEQUENCE_H__

typedef struct _GSequence      GSequence;
typedef struct _GSequenceNode *GSequencePtr;

/* GSequence */
GSequence *  g_sequence_new                (GDestroyNotify           data_destroy);
void         g_sequence_free               (GSequence               *seq);
void         g_sequence_sort               (GSequence               *seq,
					    GCompareDataFunc         cmp_func,
					    gpointer                 cmp_data);
void         g_sequence_append             (GSequence               *seq,
					    gpointer                 data);
void         g_sequence_prepend            (GSequence               *seq,
					    gpointer                 data);
void         g_sequence_insert             (GSequencePtr             ptr,
					    gpointer                 data);
void         g_sequence_remove             (GSequencePtr             ptr);
GSequencePtr g_sequence_insert_sorted      (GSequence               *seq,
					    gpointer                 data,
					    GCompareDataFunc         cmp_func,
					    gpointer                 cmp_data);
void         g_sequence_insert_sequence    (GSequencePtr             ptr,
					    GSequence               *other_seq);
void         g_sequence_concatenate        (GSequence               *seq1,
					    GSequence               *seq);
void         g_sequence_remove_range       (GSequencePtr             begin,
					    GSequencePtr             end,
					    GSequence              **removed);
gint	     g_sequence_get_length         (GSequence               *seq);
GSequencePtr g_sequence_get_end_ptr        (GSequence               *seq);
GSequencePtr g_sequence_get_begin_ptr      (GSequence               *seq);
GSequencePtr g_sequence_get_ptr_at_pos     (GSequence               *seq,
					    gint                     pos);

/* GSequencePtr */
gboolean     g_sequence_ptr_is_end         (GSequencePtr             ptr);
gboolean     g_sequence_ptr_is_begin       (GSequencePtr             ptr);
gint         g_sequence_ptr_get_position   (GSequencePtr             ptr);
GSequencePtr g_sequence_ptr_next           (GSequencePtr             ptr);
GSequencePtr g_sequence_ptr_prev           (GSequencePtr             ptr);
GSequencePtr g_sequence_ptr_move           (GSequencePtr             ptr,
					    guint                    leap);
void         g_sequence_ptr_sort_changed   (GSequencePtr	     ptr,
					    GCompareDataFunc	     cmp_func,
					    gpointer		     cmp_data);
gpointer     g_sequence_ptr_get_data       (GSequencePtr             ptr);

/* search */

/* return TRUE if you want to be called again with two
 * smaller segments
 */
typedef gboolean (* GSequenceSearchFunc) (GSequencePtr begin,
					  GSequencePtr end,
					  gpointer     data);

void         g_sequence_search             (GSequence               *seq,
					    GSequenceSearchFunc      f,
					    gpointer                 data);

/* debug */
gint         g_sequence_calc_tree_height   (GSequence               *seq);

#endif /* __GSEQUENCE_H__ */
