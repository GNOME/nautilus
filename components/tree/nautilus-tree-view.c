/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 
 *       Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-tree-view.c - tree sidebar panel
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-view-private.h"
#include <bonobo/bonobo-control.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <stdio.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500
#define TREE_SPACING 5
#define NAUTILUS_TREE_VIEW_MAX_CHANGE_BATCH 100

static void     notify_node_seen                     (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static gboolean ctree_is_node_expanded               (EelCTree         *ctree,
						      EelCTreeNode     *node);
static void     nautilus_tree_view_remove_model_node (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static void     reload_model_node                    (NautilusTreeView      *view,
						      NautilusTreeNode      *node,
						      gboolean               force_reload);
static void     reload_whole_tree                    (NautilusTreeView      *view,
						      gboolean               force_reload);
static void     tree_load_location_callback          (NautilusView          *nautilus_view,
						      const char            *location,
						      NautilusTreeView      *view);
static void     tree_expand_callback                 (EelCTree         *tree,
						      EelCTreeNode     *node,
						      NautilusTreeView      *view);
static void     tree_collapse_callback               (EelCTree         *tree,
						      EelCTreeNode     *node,
						      NautilusTreeView      *view);
static void     tree_select_row_callback             (EelCTree         *tree,
						      EelCTreeNode     *node,
						      gint                   column,
						      NautilusTreeView      *view);
static void     size_allocate_callback               (EelCTree         *tree,
						      GtkAllocation         *allocation,
						      gpointer               data);
static void     nautilus_tree_view_update_all_icons  (NautilusTreeView      *view);
static void     cancel_possible_activation           (NautilusTreeView      *view);
static void     nautilus_tree_view_update_model_node (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static void     nautilus_tree_view_initialize_class  (NautilusTreeViewClass *klass);
static void     nautilus_tree_view_initialize        (NautilusTreeView      *view);
static void     register_unparented_node             (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static void     forget_unparented_node               (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static void     insert_unparented_nodes              (NautilusTreeView      *view,
						      NautilusTreeNode      *node);
static void     expand_uri_sequence_and_select_end   (NautilusTreeView      *view);
static gboolean is_anti_aliased			     (NautilusTreeView	    *view);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTreeView,
				   nautilus_tree_view,
				   NAUTILUS_TYPE_VIEW)

static void
unlink_view_node_from_uri (NautilusTreeView *view,
			   EelCTreeNode *view_node)
{
	gpointer orig_key, value;

	if (g_hash_table_lookup_extended (view->details->view_node_to_uri_map,
					  view_node, &orig_key, &value)) {
		g_hash_table_remove (view->details->view_node_to_uri_map, view_node);
		g_free (value);
	}
}

/* URI will be g_free'd eventually */
static void
link_view_node_with_uri (NautilusTreeView *view,
			 EelCTreeNode *view_node,
			 const char *uri)
{
	unlink_view_node_from_uri (view, view_node);
	g_hash_table_insert (view->details->view_node_to_uri_map,
			     view_node, (gpointer) uri);
}

/* Returned string is only valid until next link or unlink of VIEW-NODE */
static const char *
map_view_node_to_uri (NautilusTreeView *view,
		      EelCTreeNode *view_node)
{
	gpointer value = g_hash_table_lookup (view->details->view_node_to_uri_map,
					      view_node);
	g_assert (value != NULL);
	return value;
}


static gboolean
nautilus_tree_view_should_skip_file (NautilusTreeView *view,
				     NautilusFile *file)
{
	if (nautilus_file_is_gone (file)) {
		return TRUE;
	}

	return !(nautilus_file_should_show (file, 
					    view->details->show_hidden_files, 
					    view->details->show_backup_files) &&
		 (view->details->show_non_directories || 
		  nautilus_file_is_directory (file)));
}

/* This is different to the should_skip function above, in that it
 * also searches all parent files of URI. It will return true iff
 * URI may be shown in the tree view display. Note that URI may
 * not exist when this is called.
 */
static gboolean
nautilus_tree_view_would_include_uri (NautilusTreeView *view,
				      const char *uri)
{
	char *copy, *component;

	/* The tree view currently only ever shows `file:' URIs */

	if (!eel_str_has_prefix (uri, "file:")) {
		return FALSE;
	}

	if (!view->details->show_hidden_files
	    || !view->details->show_backup_files) {
		copy = g_strdup (uri);
		while (1) {
			component = strrchr (copy, '/');
			if (component != NULL) {
				if ((!view->details->show_hidden_files
				     && nautilus_file_name_matches_hidden_pattern (component + 1))
				    || (!view->details->show_backup_files
					&& nautilus_file_name_matches_backup_pattern (component + 1))) {
					/* Don't show this file */
					g_free (copy);
					return FALSE;
				} else {
					/* Chop the bottom-most component from the uri */
					*component = 0;
				}
			} else {
				break;
			}
		}
		g_free (copy);
	}

	return TRUE;
}

static void
nautilus_tree_view_insert_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	NautilusTreeNode *parent_node;
	EelCTreeNode *parent_view_node;
 	EelCTreeNode *view_node;
	NautilusFile *file;
	char *text[2];
	GdkPixbuf *closed_pixbuf;
	GdkPixbuf *open_pixbuf;
	char *uri;

	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	file = nautilus_tree_node_get_file (node);

	if (nautilus_tree_view_should_skip_file (view, file)) {
		nautilus_tree_view_remove_model_node (view, node);
		return;
	}

#ifdef DEBUG_TREE
	printf ("Inserting URI into tree: %s\n", nautilus_file_get_uri (file));
#endif

	parent_node = nautilus_tree_node_get_parent (node);
	parent_view_node = (parent_node == NULL ? NULL
			    : nautilus_tree_view_model_node_to_view_node (view, parent_node));

#ifdef DEBUG_TREE
	printf ("parent_view_node 0x%x (%s)\n", (unsigned) parent_view_node, 
		nautilus_file_get_uri (nautilus_tree_view_node_to_file (view, parent_view_node)));
#endif

	if (parent_view_node == NULL && !nautilus_tree_node_is_toplevel (node)) {
		register_unparented_node (view, node);
	} else {
		text[0] = nautilus_file_get_display_name (file);
		text[1] = NULL;

		if (nautilus_tree_view_model_node_to_view_node (view, node) == NULL) {

			closed_pixbuf = nautilus_icon_factory_get_pixbuf_for_file
				(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS,
				 is_anti_aliased (view));
			open_pixbuf = nautilus_icon_factory_get_pixbuf_for_file
				(file, "accept", NAUTILUS_ICON_SIZE_FOR_MENUS,
				 is_anti_aliased (view));

			view->details->inserting_node = TRUE;
			EEL_CLIST_UNSET_FLAG (EEL_CLIST (view->details->tree),
					      CLIST_AUTO_SORT);
			view_node = eel_ctree_insert_node (EEL_CTREE (view->details->tree),
							   parent_view_node, 
							   NULL,
							   text,
							   TREE_SPACING,
							   closed_pixbuf, open_pixbuf,
							   ! nautilus_file_is_directory (file),
							   FALSE);
			EEL_CLIST_SET_FLAG (EEL_CLIST (view->details->tree),
					    CLIST_AUTO_SORT);
			view->details->inserting_node = FALSE;

			gdk_pixbuf_unref (closed_pixbuf);
			gdk_pixbuf_unref (open_pixbuf);


			eel_ctree_node_set_row_data (EEL_CTREE (view->details->tree),
						     view_node,
						     node);

			g_assert (g_hash_table_lookup (view->details->file_to_node_map, file) == NULL);

			nautilus_file_ref (file);
			g_hash_table_insert (view->details->file_to_node_map, file, view_node); 
		
			uri = nautilus_file_get_uri (file);
			link_view_node_with_uri (view, view_node, uri);

			if (nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
				if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
					if (!ctree_is_node_expanded (EEL_CTREE (view->details->tree),
								     view_node)) {
						eel_ctree_expand (EEL_CTREE (view->details->tree),
								  view_node);
					} 
				} else {
					if (ctree_is_node_expanded (EEL_CTREE (view->details->tree),
								    view_node)) {
						eel_ctree_collapse (EEL_CTREE (view->details->tree),
								    view_node);
					}
				}
			}

			eel_ctree_sort_single_node (EEL_CTREE (view->details->tree), view_node);

			insert_unparented_nodes (view, node);
		} else {
			nautilus_tree_view_update_model_node (view, node);
		}

		g_free (text[0]);
	}
	notify_node_seen (view, node);
}

static void
forget_view_node (NautilusTreeView *view,
		  EelCTreeNode *view_node)
{
	NautilusFile *file;
	NautilusTreeNode *node;

	file = nautilus_tree_view_node_to_file (view, view_node);
	node = nautilus_tree_view_node_to_model_node (view, view_node);

	g_return_if_fail (node != NULL);

	forget_unparented_node (view, node);

	eel_ctree_node_set_row_data (EEL_CTREE (view->details->tree),
				     view_node, NULL);

	g_hash_table_remove (view->details->file_to_node_map, file);
	nautilus_file_unref (file);

	unlink_view_node_from_uri (view, view_node);
}

static void
forget_view_node_and_children (NautilusTreeView *view,
			       EelCTreeNode *view_node)
{
	EelCTreeNode *child;

	for (child = EEL_CTREE_ROW (view_node)->children;
	     child != NULL;
	     child = EEL_CTREE_ROW (child)->sibling) {
		forget_view_node_and_children (view, child);
	}

	forget_view_node (view, view_node);
}

static void
nautilus_tree_view_remove_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	EelCTreeNode *view_node;
	NautilusFile *file;
	const char *uri;

	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	file = nautilus_tree_node_get_file (node);

#ifdef DEBUG_TREE
	printf ("XXX - Removing URI from tree: %s\n", nautilus_file_get_uri (file));
#endif


 	view_node = nautilus_tree_view_model_node_to_view_node (view, node);
 	if (view_node != NULL) {
		/* The URI associated with FILE may have been renamed by now,
		 * so using nautilus_file_get_uri () is no good (since it would
		 * give the new name, not the old name). Hence the extra hash
		 * table mapping view nodes to URIs..
		 *
		 * Note that it would be better to remove the expansion
		 * state of the children, but that breaks renaming..
		 */
		uri = map_view_node_to_uri (view, view_node);
		nautilus_tree_expansion_state_remove_node (view->details->expansion_state, uri);

		forget_view_node_and_children (view, view_node);
 		eel_ctree_remove_node (EEL_CTREE (view->details->tree),
				       view_node);
 	}

	nautilus_tree_model_stop_monitoring_node (view->details->model, node, view);

}




static gboolean
ctree_is_node_expanded (EelCTree     *ctree,
			EelCTreeNode *node)
{
	gchar     *text;
	guint8     spacing;
	GdkPixbuf *pixbuf_closed;
	GdkPixbuf *pixbuf_opened;
	gboolean   is_leaf;
	gboolean   expanded;

	eel_ctree_get_node_info (ctree, node,
				 &text, &spacing,
				 &pixbuf_closed,
				 &pixbuf_opened,
				 &is_leaf, &expanded);
	return expanded;
}

static void
nautilus_tree_view_update_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	EelCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *name;
	GdkPixbuf *closed_pixbuf;
	GdkPixbuf *open_pixbuf;
	
	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	file = nautilus_tree_node_get_file (node);

	if (nautilus_tree_view_should_skip_file (view, file)) {
		nautilus_tree_view_remove_model_node (view, node);
		return;
	}

#ifdef DEBUG_TREE
	printf ("XXX - Updating URI in tree: %s\n", nautilus_file_get_uri (file));
#endif

	view_node = nautilus_tree_view_model_node_to_view_node (view, node);

	if (view_node != NULL) {
		link_view_node_with_uri (view, view_node, nautilus_file_get_uri (file));

		closed_pixbuf = nautilus_icon_factory_get_pixbuf_for_file
			(file, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS,
			 is_anti_aliased (view));
		open_pixbuf = nautilus_icon_factory_get_pixbuf_for_file
			(file, "accept", NAUTILUS_ICON_SIZE_FOR_MENUS,
			 is_anti_aliased (view));

		name = nautilus_file_get_display_name (file);
	
		eel_ctree_set_node_info (EEL_CTREE (view->details->tree),
					 view_node,
					 name,
					 TREE_SPACING,
					 closed_pixbuf,
					 open_pixbuf,
					 ! nautilus_file_is_directory (file),
					 ctree_is_node_expanded (EEL_CTREE (view->details->tree),
								 view_node));

		g_free (name);

		gdk_pixbuf_unref (closed_pixbuf);
		gdk_pixbuf_unref (open_pixbuf);



		if (nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
			uri = nautilus_file_get_uri (file);

			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				if (!ctree_is_node_expanded (EEL_CTREE (view->details->tree),
							     view_node)) {
					eel_ctree_expand (EEL_CTREE (view->details->tree),
							  view_node);
				} 
			} else {
				if (ctree_is_node_expanded (EEL_CTREE (view->details->tree),
							    view_node)) {
					eel_ctree_collapse (EEL_CTREE (view->details->tree),
							    view_node);
				}
			}

			g_free (uri);
		}

		insert_unparented_nodes (view, node);
	} else {
		nautilus_tree_view_insert_model_node (view, node);
	}
}

static void
register_unparented_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));
	g_return_if_fail (!nautilus_tree_node_is_toplevel (node));

	if (g_list_find (view->details->unparented_tree_nodes, node) == NULL) {
		gtk_object_ref (GTK_OBJECT (node));
		view->details->unparented_tree_nodes = g_list_prepend (view->details->unparented_tree_nodes, node);
	}
}

static void
forget_unparented_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	if (g_list_find (view->details->unparented_tree_nodes, node) != NULL) {
		view->details->unparented_tree_nodes = g_list_remove (view->details->unparented_tree_nodes, node);
		gtk_object_unref (GTK_OBJECT (node));
	}
}

static void
insert_unparented_nodes (NautilusTreeView *view, NautilusTreeNode *node)
{
	NautilusFile *file, *sub_file;
	NautilusDirectory *directory;
	GList *p, *to_add;
	NautilusTreeNode *sub_node;

	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

	file = nautilus_tree_node_get_file (node);

	if (nautilus_file_is_directory (file)) {
		directory = nautilus_tree_node_get_directory (node);
		if (directory != NULL) {
			to_add = NULL;
			for (p = view->details->unparented_tree_nodes; p != NULL; p = p->next) {
				sub_node = p->data;
				sub_file = nautilus_tree_node_get_file (sub_node);
				if (nautilus_directory_contains_file (directory, sub_file)) {
					to_add = g_list_prepend (to_add, sub_node);
				}
			}
			for (p = to_add; p != NULL; p = p->next) {
				sub_node = p->data;
				view->details->unparented_tree_nodes = g_list_remove (view->details->unparented_tree_nodes, sub_node);
				nautilus_tree_view_insert_model_node (view, sub_node);
				gtk_object_unref (GTK_OBJECT (sub_node));
			}
			g_list_free (to_add);
		}
	}
}

static void
awaited_node_or_uri_found (NautilusTreeView *view)
{
	view->details->wait_node = NULL;
	g_free (view->details->wait_uri);
	view->details->wait_uri = NULL;
	
	expand_uri_sequence_and_select_end (view);
}

static void
notify_done_loading (NautilusTreeView *view,
		     NautilusTreeNode *node)
{
	if (view->details->wait_node == node) {
		awaited_node_or_uri_found (view);
	}
}

static void
notify_node_seen (NautilusTreeView *view,
		  NautilusTreeNode *node)
{
	char *uri;

	if (!view->details->root_seen) {
		view->details->root_seen = TRUE;
		expand_uri_sequence_and_select_end (view);
	}

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (eel_strcmp (uri, view->details->wait_uri) == 0) {
		awaited_node_or_uri_found (view);
	}
	
	g_free (uri);
}

static int
get_selected_row (EelCTree *ctree)
{
	EelCList *clist;
	EelCTreeNode *node;

	clist = EEL_CLIST (ctree);
	if (clist->selection == NULL) {
		return -1;
	}
	node = clist->selection->data;
	return g_list_position (clist->row_list, &node->list);
}

static int
get_selected_row_position (EelCTree *ctree)
{
	EelCList *clist;
	int selected_row, top_row, column;

	clist = EEL_CLIST (ctree);
	selected_row = get_selected_row (ctree);
	if (eel_clist_row_is_visible (clist,
				      selected_row) == GTK_VISIBILITY_NONE) {
		return -1;
	}
	if (!eel_clist_get_selection_info (clist, 0, 0,
					   &top_row, &column)) {
		return -1;
	}
	return selected_row - top_row;
}

static void
set_selected_row_position (EelCTree *ctree,
			   int position)
{
	int current_position, top_row;

	if (position <= 0) {
		return;
	}
	current_position = get_selected_row_position (ctree);
	if (current_position == position) {
		return;
	}
	top_row = get_selected_row (ctree) - position;
	if (top_row < 0) {
		top_row = 0;
	}
	eel_clist_moveto (EEL_CLIST (ctree), top_row, -1, 0, 0);
}

static gboolean
dequeue_pending_idle_callback (gpointer data)
{
	NautilusTreeView *view;
	int i;
	NautilusTreeChange *change;
	gboolean done_early;
	int position;
	
	view = NAUTILUS_TREE_VIEW (data);
	done_early = FALSE;

	eel_clist_freeze (EEL_CLIST (view->details->tree));

	position = get_selected_row_position (EEL_CTREE (view->details->tree));

	for (i = 0; i < NAUTILUS_TREE_VIEW_MAX_CHANGE_BATCH; i++) {
		change = nautilus_tree_change_queue_dequeue 
			(view->details->change_queue);
		
		if (change == NULL) {
			done_early = TRUE;
			break;
		}

		switch (change->change_type) {
		case NAUTILUS_TREE_CHANGE_TYPE_CHANGED:
			nautilus_tree_view_update_model_node (view, change->node);
			break;
		case NAUTILUS_TREE_CHANGE_TYPE_REMOVED:
			nautilus_tree_view_remove_model_node (view, change->node);
			break;
		case NAUTILUS_TREE_CHANGE_TYPE_DONE_LOADING:
			notify_done_loading (view, change->node);
		}

		nautilus_tree_change_free (change);
	}

	set_selected_row_position (EEL_CTREE (view->details->tree),
				   position);

	eel_clist_thaw (EEL_CLIST (view->details->tree));

	if (done_early) {
		view->details->pending_idle_id = 0;
		return FALSE;
	} else {
		return TRUE;
	}
}

static void
schedule_pending_idle_callback (NautilusTreeView *view)
{
	if (view->details->pending_idle_id == 0
	    && view->details->tree != NULL
	    && GTK_WIDGET_MAPPED (view->details->tree)) {
		view->details->pending_idle_id =
			gtk_idle_add_priority (GTK_PRIORITY_LOW,
					       dequeue_pending_idle_callback,
					       view);
	}
}

static void
unschedule_pending_idle_callback (NautilusTreeView *view)
{
	if (view->details->pending_idle_id != 0) {
		gtk_idle_remove (view->details->pending_idle_id);
		view->details->pending_idle_id = 0;
	}
}


static void
nautilus_tree_view_enqueue_change (NautilusTreeView *view,
				   NautilusTreeChangeType change_type,
				   NautilusTreeNode  *node)
{
	nautilus_tree_change_queue_enqueue (view->details->change_queue,
					    change_type,
					    node);
	schedule_pending_idle_callback (view);
}

static void
nautilus_tree_view_model_node_changed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	nautilus_tree_view_enqueue_change (NAUTILUS_TREE_VIEW (callback_data),
					   NAUTILUS_TREE_CHANGE_TYPE_CHANGED,
					   node);
}

static void
nautilus_tree_view_model_node_removed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	nautilus_tree_view_enqueue_change (NAUTILUS_TREE_VIEW (callback_data),
					   NAUTILUS_TREE_CHANGE_TYPE_REMOVED,
					   node);
}

static void
nautilus_tree_view_model_node_renamed_callback (NautilusTreeModel *model,
						const char	  *old_uri,
						const char 	  *new_uri,
						gpointer           callback_data)
{
	NautilusTreeView *view;

	/* Propagate the expansion state of the old name to the new name */

	view = NAUTILUS_TREE_VIEW (callback_data);

	if (nautilus_tree_view_would_include_uri (view, new_uri)) {
		if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, old_uri)) {
			nautilus_tree_expansion_state_expand_node (view->details->expansion_state, new_uri);
		} else {
			nautilus_tree_expansion_state_collapse_node (view->details->expansion_state, new_uri);
		}
	}

	nautilus_tree_expansion_state_remove_node (view->details->expansion_state, old_uri);
}

static void
nautilus_tree_view_model_done_loading_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	nautilus_tree_view_enqueue_change (NAUTILUS_TREE_VIEW (callback_data),
					   NAUTILUS_TREE_CHANGE_TYPE_DONE_LOADING,
					   node);
}

static void
nautilus_tree_view_load_from_filesystem (NautilusTreeView *view)
{
	view->details->model = nautilus_tree_model_new ("file:///");

	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_changed_callback,
					 view);

	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_changed",
			    nautilus_tree_view_model_node_changed_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_removed",
			    nautilus_tree_view_model_node_removed_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_being_renamed",
			    nautilus_tree_view_model_node_renamed_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "done_loading_children",
			    nautilus_tree_view_model_done_loading_callback,
			    view);
}



static void
filtering_changed_callback (gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	view->details->show_hidden_files = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	
	view->details->show_backup_files = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);

	view->details->show_non_directories = 
		! eel_preferences_get_boolean (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES);


	/* Reload the whole tree so that the filtering changes take place. */

	if (view->details->root_seen) {
		reload_whole_tree (view, FALSE);
	}
}

static void
update_smooth_graphics_mode (NautilusTreeView *view)
{
	gboolean aa_mode, old_aa_mode;

	if (view->details->tree != NULL) {
		aa_mode = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);
		old_aa_mode = is_anti_aliased (view);

		if (old_aa_mode != aa_mode) {
			eel_list_set_anti_aliased_mode (EEL_LIST (view->details->tree), aa_mode);

			/* FIXME: refetch icons using correct aa mode... */
		}
	}
}

static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	update_smooth_graphics_mode (view);
}

static gboolean
is_anti_aliased (NautilusTreeView *view)
{
	return eel_list_is_anti_aliased (EEL_LIST (view->details->tree));
}

static gpointer compare_cached_key, compare_cached_value;

static gint
ctree_compare_rows (EelCList *clist,
		    gconstpointer  ptr1,
		    gconstpointer  ptr2)
{
	NautilusTreeView *view;
	NautilusTreeNode *node1, *node2;
	NautilusFile *file1, *file2;
	gint result;

	/* Default to returning -1 in unknown cases. This magically
	 * ensures that inserted nodes are initially placed at the
	 * head of the list, instead of wasting time comparing
	 * against all files for no reason (makes expanding /dev/
	 * actually bearable)
	 */
	result = -1;

	/* Avoid fetching the view from the object data more than once
	 * per sort.
	 */
	if (compare_cached_key == clist) {
		view = compare_cached_value;
	} else {
		view = gtk_object_get_data (GTK_OBJECT (clist), "tree_view");
		compare_cached_key = clist;
		compare_cached_value = view;
	}
	g_assert (view != NULL);

	if (!view->details->inserting_node) {
		node1 = ((EelCTreeRow *) ptr1)->row.data;
		node2 = ((EelCTreeRow *) ptr2)->row.data;

		file1 = node1 ? nautilus_tree_node_get_file (node1) : NULL;
		file2 = node2 ? nautilus_tree_node_get_file (node2) : NULL;

		if (file1 != NULL && file2 != NULL) {
			result = nautilus_file_compare_for_sort (file1, file2, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE);
		}
	}

	return result;
}

static void
create_tree (NautilusTreeView *view)
{
	/* set up expansion state */
	view->details->expansion_state = nautilus_tree_expansion_state_new ();

	/* set up change queue */
	view->details->change_queue = nautilus_tree_change_queue_new ();

	/* set up ctree */
	view->details->tree = eel_ctree_new (1, 0);

	gtk_object_set_data (GTK_OBJECT (view->details->tree), "tree_view", (gpointer) view);
	gtk_widget_add_events (GTK_WIDGET (view->details->tree), GDK_POINTER_MOTION_MASK);
	
        eel_clist_set_selection_mode (EEL_CLIST (view->details->tree), GTK_SELECTION_SINGLE);
	eel_clist_set_auto_sort (EEL_CLIST (view->details->tree), TRUE);
	eel_clist_set_sort_type (EEL_CLIST (view->details->tree), GTK_SORT_ASCENDING);

	update_smooth_graphics_mode (view);

	/* FIXME bugzilla.gnome.org 46820:
	 * Using the NautilusFile comparison function to sort by
	 * is way too slow when opening large directories (those with
	 * 1000s of files). So to fix bug 6988 we're reverting back
	 * to using the standard clist comparison function (a strcmp)
	 */
	eel_clist_set_compare_func (EEL_CLIST (view->details->tree),
				    ctree_compare_rows);

	eel_clist_set_column_auto_resize (EEL_CLIST (view->details->tree), 0, TRUE);
	eel_clist_columns_autosize (EEL_CLIST (view->details->tree));
	eel_clist_set_reorderable (EEL_CLIST (view->details->tree), FALSE);
	eel_clist_set_row_height (EEL_CLIST (view->details->tree),
				  MAX (NAUTILUS_ICON_SIZE_FOR_MENUS,
				       view->details->tree->style->font->ascent
				       + view->details->tree->style->font->descent));
        eel_ctree_set_indent (EEL_CTREE (view->details->tree), 12);

	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_expand",
			    GTK_SIGNAL_FUNC (tree_expand_callback), 
			    view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_collapse",
			    GTK_SIGNAL_FUNC (tree_collapse_callback), 
			    view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_select_row",
			    GTK_SIGNAL_FUNC (tree_select_row_callback), 
			    view);
	
	gtk_signal_connect_after (GTK_OBJECT (view->details->tree),
				  "size_allocate",
				  GTK_SIGNAL_FUNC (size_allocate_callback), 
				  view);
	
	/* Keep track of changes in these prefs to filter files accordingly. */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
				      filtering_changed_callback,
				      view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
				      filtering_changed_callback,
				      view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
				      filtering_changed_callback,
				      view);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
				      smooth_graphics_mode_changed_callback, 
				      view);

	view->details->file_to_node_map = g_hash_table_new (NULL, NULL);
	view->details->view_node_to_uri_map = g_hash_table_new (NULL, NULL);
	
	nautilus_tree_view_load_from_filesystem (view);

	gtk_signal_connect_object_while_alive
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 nautilus_tree_view_update_all_icons,
		 GTK_OBJECT (view));

	nautilus_tree_view_init_dnd (view);

	gtk_widget_show (view->details->tree);

	gtk_container_add (GTK_CONTAINER (view->details->scrolled_window),
			   view->details->tree);
}

static void
tree_map_callback (GtkWidget *widget,
		   gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);
	g_assert (widget == view->details->scrolled_window);

	if (view->details->tree == NULL) {
		create_tree (view);
	}
	nautilus_tree_model_set_defer_notifications (view->details->model, FALSE);
	schedule_pending_idle_callback (view);
	expand_uri_sequence_and_select_end (view);
}

static void
tree_unmap_callback (GtkWidget *widget,
		     gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);
	g_assert (widget == view->details->scrolled_window);

	nautilus_tree_model_set_defer_notifications (view->details->model, TRUE);
	unschedule_pending_idle_callback (view);
}

static void
nautilus_tree_view_initialize (NautilusTreeView *view)
{
	view->details = g_new0 (NautilusTreeViewDetails, 1);

	/* Obtain the filtering preferences */
	view->details->show_hidden_files = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

	view->details->show_backup_files = 
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);

	view->details->show_non_directories = 
		! eel_preferences_get_boolean (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES);

	gtk_signal_connect (GTK_OBJECT (view),
			    "load_location",
			    GTK_SIGNAL_FUNC (tree_load_location_callback),
			    view);

	view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
						
	gtk_widget_show (view->details->scrolled_window);

	nautilus_view_construct (NAUTILUS_VIEW (view),
				 view->details->scrolled_window);

	gtk_signal_connect (GTK_OBJECT (view->details->scrolled_window),
			    "map",
			    tree_map_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->scrolled_window),
			    "unmap",
			    tree_unmap_callback,
			    view);
}

static void
disconnect_model_handlers (NautilusTreeView *view)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node (view->details->model, "file:///");

	if (node != NULL) {
		nautilus_tree_model_monitor_remove (view->details->model, view);
	}
}

static void
free_file_to_node_map_entry (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (callback_data == NULL);

	nautilus_file_unref (NAUTILUS_FILE (key));
}

static void
free_view_node_to_uri_map_entry (gpointer key, gpointer value, gpointer callback_data)
{
	g_free (value);			/* the URI */
}

static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);

	cancel_possible_activation (view);

	if (view->details->pending_idle_id != 0) {
		gtk_idle_remove (view->details->pending_idle_id);
	}

	if (view->details->tree != NULL) {
		gtk_object_unref (GTK_OBJECT (view->details->change_queue));
		
		if (compare_cached_key == view->details->tree) {
			compare_cached_key = NULL;
		}

		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
						 filtering_changed_callback,
						 view);
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
						 filtering_changed_callback,
						 view);
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
						 filtering_changed_callback,
						 view);

		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
						 smooth_graphics_mode_changed_callback,
						 view);
		
		g_hash_table_foreach (view->details->file_to_node_map,
				      free_file_to_node_map_entry,
				      NULL);
		g_hash_table_destroy (view->details->file_to_node_map);
		
		g_hash_table_foreach (view->details->view_node_to_uri_map,
				      free_view_node_to_uri_map_entry,
				      NULL);
		g_hash_table_destroy (view->details->view_node_to_uri_map);
		
		nautilus_tree_view_free_dnd (view);
		
		disconnect_model_handlers (view);
		gtk_object_unref (GTK_OBJECT (view->details->model));
		
		nautilus_tree_expansion_state_save (view->details->expansion_state);
		gtk_object_unref (GTK_OBJECT (view->details->expansion_state));
	}

	eel_gtk_object_list_free (view->details->unparented_tree_nodes);
	
	g_free (view->details->current_main_view_uri);
	g_free (view->details->selected_uri);

	g_free (view->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static EelCTreeNode *
file_to_view_node (NautilusTreeView *view,
		   NautilusFile     *file)
{
	return g_hash_table_lookup (view->details->file_to_node_map, file);
}


EelCTreeNode *
nautilus_tree_view_model_node_to_view_node (NautilusTreeView *view,
					    NautilusTreeNode *node)
{
	EelCTreeNode *view_node;
	NautilusFile *file;

	if (node == NULL) {
		return NULL;
	}

	file = nautilus_tree_node_get_file (node);
	view_node = file_to_view_node (view, file);

	return view_node;
}

NautilusTreeNode *
nautilus_tree_view_node_to_model_node (NautilusTreeView *view,
				       EelCTreeNode *node)
{
	NautilusTreeNode *tree_node;

	tree_node = (NautilusTreeNode *) eel_ctree_node_get_row_data (EEL_CTREE (view->details->tree),
								      node);

	return tree_node;
}

NautilusFile *
nautilus_tree_view_node_to_file (NautilusTreeView *view,
				 EelCTreeNode *node)
{
	NautilusTreeNode *tree_node;

	tree_node = nautilus_tree_view_node_to_model_node (view, node);

	if (tree_node == NULL) {
		return NULL;
	}

	return nautilus_tree_node_get_file (tree_node);
}

static GList *
get_uri_sequence_to_root (char *uri_text)
{
	GList *result;
	GnomeVFSURI *uri;
	GnomeVFSURI *parent_uri;

	result = NULL;

	uri = gnome_vfs_uri_new (uri_text);

	result = g_list_prepend (result, uri_text);
	
	if (uri == NULL) {
		return result;
	}

	while (1) {
		parent_uri = gnome_vfs_uri_get_parent (uri);
		
		gnome_vfs_uri_unref (uri);

		if (parent_uri == NULL) {
			return result;
		}
		
		uri = parent_uri;
		uri_text = gnome_vfs_uri_to_string (uri, 
						    GNOME_VFS_URI_HIDE_NONE);
		
		result = g_list_prepend (result, uri_text);
	}
}


static void
call_when_uri_loaded_or_parent_done_loading (NautilusTreeView *view,
					     const char *uri, 
					     NautilusTreeNode *node)
{
	view->details->wait_uri = g_strdup (uri);
	view->details->wait_node = node;
}


static void
cancel_selection_in_progress (NautilusTreeView *view)
{
	eel_g_list_free_deep (view->details->in_progress_select_uris);
	view->details->in_progress_select_uris = NULL;

	g_free (view->details->wait_uri);
	view->details->wait_uri = NULL;
	view->details->wait_node = NULL;
}

static void
expand_uri_sequence_and_select_end (NautilusTreeView *view)
{
	const char *uri;
	GList *p;
	GList *old_sequence;
	EelCTreeNode *view_node;
	EelCTreeNode *last_valid_view_node;
	NautilusFile *file;
	NautilusTreeNode *node;

	view_node = NULL;
	last_valid_view_node = NULL;

	uri = NULL;

	if (!view->details->root_seen
	    || !GTK_WIDGET_MAPPED (view->details->tree)) {
		return;
	}

	for (p = view->details->in_progress_select_uris; p != NULL; p = p->next) {
		uri = (char *) p->data;

		file = nautilus_file_get (uri);
		view_node = file_to_view_node (view, file);
		nautilus_file_unref (file);

		if (view_node == NULL) {
			break;
		}

		last_valid_view_node = view_node;

		if (p->next != NULL) {
			/* We don't want to expand the node if it's
			 * already expanded, as that might trigger a deep force reload, which we
			 * don't want.
			 */

			if (!ctree_is_node_expanded (EEL_CTREE (view->details->tree), view_node)) {
				eel_ctree_expand (EEL_CTREE (view->details->tree),
						  view_node);
			}
		} else {
			g_free (view->details->selected_uri);
			view->details->selected_uri = g_strdup (uri);
			eel_ctree_select (EEL_CTREE (view->details->tree),
					  view_node);
		}
	}
		
	if (p == NULL || last_valid_view_node == NULL) {
		/* We already found it, or the the target URI just
		   isn't in the tree at all. Clean up. */
		
		cancel_selection_in_progress (view);

		return;
	}

	/* Not all the nodes existed yet, damn */
	
	old_sequence = view->details->in_progress_select_uris;
	
	view->details->in_progress_select_uris = p;
	
	/* Force a shallow reload; might have triggered deep reload already, 
	   but we can't be sure */
	reload_model_node (view,
			   nautilus_tree_view_node_to_model_node (view,
								  last_valid_view_node),
			   TRUE);

	node = nautilus_tree_model_get_node (view->details->model, (char *) p->prev->data);
	if (node != NULL) {
		call_when_uri_loaded_or_parent_done_loading (view, uri, node);
	}
	
	p->prev->next = NULL;
	p->prev = NULL;
	eel_g_list_free_deep (old_sequence);
}

static void
select_current_location (NautilusTreeView *view)
{
	cancel_selection_in_progress (view);

	if (view->details->selected_uri == NULL ||
	    !eel_uris_match_ignore_fragments (view->details->current_main_view_uri,
					      view->details->selected_uri)) {
		view->details->in_progress_select_uris = get_uri_sequence_to_root
			(g_strdup (view->details->current_main_view_uri));
		expand_uri_sequence_and_select_end (view);
	}
}

static void
tree_load_location_callback (NautilusView *nautilus_view, 
			     const char *location,
			     NautilusTreeView *view)
{
	g_assert (nautilus_view == NAUTILUS_VIEW (view));
	
	nautilus_view_report_load_complete (nautilus_view);

	g_free (view->details->current_main_view_uri);
	view->details->current_main_view_uri = eel_make_uri_canonical (location);

	select_current_location (view);
}


static void
reload_model_node (NautilusTreeView *view,
		   NautilusTreeNode *node,
		   gboolean          force_reload)
{
	GList *p;
	char *uri;

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));

	if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
		nautilus_tree_model_monitor_node (view->details->model, node, view, force_reload);
		
		for (p = nautilus_tree_node_get_children (node); p != NULL; p = p->next) {
			nautilus_tree_view_enqueue_change (view,
							   NAUTILUS_TREE_CHANGE_TYPE_CHANGED,
							   (NautilusTreeNode *) p->data);
		}
	}
	
	g_free (uri);
}



static void
reload_model_node_recursive (NautilusTreeView *view,
			     NautilusTreeNode *node,
			     gboolean          force_reload)
{
	GList *p;
	char *uri;

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));

	if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
		nautilus_tree_model_monitor_node (view->details->model, node, view, force_reload);
		
		for (p = nautilus_tree_node_get_children (node); p != NULL; p = p->next) {
			nautilus_tree_view_enqueue_change (view,
							   NAUTILUS_TREE_CHANGE_TYPE_CHANGED,
							   (NautilusTreeNode *) p->data);
			reload_model_node_recursive (view, (NautilusTreeNode *) p->data, force_reload);
		}
	}
	
	g_free (uri);
}


static void
reload_whole_tree (NautilusTreeView *view,
		   gboolean          force_reload)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node (view->details->model, "file:///");
	if (node != NULL) {
		reload_model_node_recursive (view, node, force_reload);
	}
}



static void
expand_node_for_file (NautilusTreeView *view,
		      NautilusFile     *file)
{
	char *uri;
	gboolean ever_expanded;
	NautilusTreeNode *node;

	uri = nautilus_file_get_uri (file);
	ever_expanded = nautilus_tree_expansion_state_was_ever_expanded (view->details->expansion_state, 
									 uri);
	nautilus_tree_expansion_state_expand_node (view->details->expansion_state,
						   uri);
	g_free (uri);

	node = nautilus_tree_model_get_node_from_file (view->details->model, file);
	if (node != NULL) {
		reload_model_node_recursive (view, node, ever_expanded);
	}
}

static void
tree_expand_callback (EelCTree         *ctree,
		      EelCTreeNode     *node,
		      NautilusTreeView      *view)
{
	NautilusFile *file;

	file = nautilus_tree_view_node_to_file (view, node);

	expand_node_for_file (view, file);
}



static void
tree_collapse_callback (EelCTree         *ctree,
			EelCTreeNode     *node,
			NautilusTreeView      *view)
{
	char *uri;
	
	uri = nautilus_file_get_uri (nautilus_tree_view_node_to_file (view, node));
	nautilus_tree_expansion_state_collapse_node (view->details->expansion_state,
						     uri);
	g_free (uri);
	
	nautilus_tree_model_stop_monitoring_node_recursive (view->details->model,
							    nautilus_tree_view_node_to_model_node (view, node),
							    view);
}

static void
ctree_show_node (EelCTree *tree,
		 EelCTreeNode *node)
{
	if (eel_ctree_node_is_visible (tree, node) != GTK_VISIBILITY_FULL) {
		eel_ctree_node_moveto (tree, node, 0, 0.5, 0);
		if (eel_ctree_node_is_visible (tree, node) != GTK_VISIBILITY_FULL) {
			eel_ctree_node_moveto (tree, node, 0, 0.5, 0.5);
		}
	}
}


static void
got_activation_uri_callback (NautilusFile *file,
			     gpointer callback_data)
{
	char *uri;
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	if (file == view->details->activation_uri_wait_file) {
		uri = nautilus_file_get_activation_uri (file);
		
		if (uri != NULL &&
		    !eel_uris_match_ignore_fragments (view->details->current_main_view_uri, uri) &&
		    strncmp (uri, "command:", strlen ("command:")) != 0) {
			nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), uri);
			g_free (view->details->selected_uri);
			view->details->selected_uri = g_strdup (uri);
		}
		
		ctree_show_node (EEL_CTREE (view->details->tree), 
				 file_to_view_node (view, file));
		
		g_free (uri);

		nautilus_file_unref (view->details->activation_uri_wait_file);
		view->details->activation_uri_wait_file = NULL;
	}
}

static void
cancel_possible_activation (NautilusTreeView *view)
{
	if (view->details->activation_uri_wait_file != NULL) {
		nautilus_file_cancel_call_when_ready 
			(view->details->activation_uri_wait_file,
			 got_activation_uri_callback,
			 view);
		nautilus_file_unref (view->details->activation_uri_wait_file);
	}

	view->details->activation_uri_wait_file = NULL;
}

static void
tree_select_row_callback (EelCTree              *tree,
			  EelCTreeNode          *node,
			  gint                        column,
			  NautilusTreeView           *view)
{
	GList *attributes;

	cancel_possible_activation (view);

	view->details->activation_uri_wait_file = nautilus_tree_view_node_to_file (view,
										   node);
	nautilus_file_ref (view->details->activation_uri_wait_file);

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	
	nautilus_file_call_when_ready (view->details->activation_uri_wait_file,
				       attributes,
				       got_activation_uri_callback,
				       view);
 
}

static EelCTreeNode *
ctree_get_first_selected_node (EelCTree *tree)
{
	if (EEL_CLIST (tree)->selection == NULL) {
		return NULL;
	}

	return EEL_CTREE_NODE (EEL_CLIST (tree)->selection->data);
}

static void
size_allocate_callback (EelCTree  *tree,
			GtkAllocation  *allocation,
			gpointer        data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (data);

	if (!view->details->got_first_size_allocate) {
		if (ctree_get_first_selected_node (tree)) {
			ctree_show_node (tree, 
					 ctree_get_first_selected_node (tree));
		}

		view->details->got_first_size_allocate = TRUE;
	}
}



static void 
nautilus_tree_view_update_all_icons (NautilusTreeView *view)
{
	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_changed_callback,
					 view);
}

static void
nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_view_destroy;
}
