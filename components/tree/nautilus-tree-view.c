/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-tree-view.c - tree content view
   component. This component displays a simple label of the URI
   and demonstrates merging menu items & toolbar buttons. 
   It should be a good basis for writing out-of-proc content views.
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-model.h"
#include "nautilus-tree-expansion-state.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libgnomevfs/gnome-vfs.h>


#include <stdio.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

typedef void (*TreeViewCallback) (NautilusTreeView *view);

/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;

	GtkWidget *tree;

	NautilusTreeModel *model;

	GHashTable *uri_to_node_map;
	GHashTable *uri_to_hack_node_map;

	gboolean show_hidden_files;

	NautilusTreeExpansionState *expansion_state;

	char *selected_uri;

	TreeViewCallback root_seen_callback;
	char *wait_uri;
	NautilusTreeNode *wait_node;
	TreeViewCallback uri_loaded_or_parent_done_loading;
        GList *in_progress_select_uris;
        gboolean root_seen;
};

#define TREE_SPACING 5

static void         notify_done_loading    (NautilusTreeView *view, 
					    NautilusTreeNode *node);
static void         notify_node_seen       (NautilusTreeView *view, 
					    NautilusTreeNode *node);

static char         *nautilus_tree_view_uri_to_name      (const char       *uri);
static GtkCTreeNode *nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
							  const char       *uri);

static gboolean      ctree_is_node_expanded              (GtkCTree     *ctree,
							  GtkCTreeNode *node);
static GtkCTreeNode *uri_to_view_node                    (NautilusTreeView *view,
							  const char *uri);
static GtkCTreeNode *model_node_to_view_node             (NautilusTreeView *view,
							  NautilusTreeNode *node);

static const char   *view_node_to_uri                    (NautilusTreeView *view,
							  GtkCTreeNode *node);


static void reload_node_for_uri                 (NautilusTreeView *view,
						 const char       *uri);
static void expand_node_for_uri                 (NautilusTreeView *view,
						 const char       *uri);
static void tree_load_location_callback         (NautilusView          *nautilus_view,
						 const char            *location,
						 NautilusTreeView      *view);
static void tree_expand_callback                (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 NautilusTreeView      *view);
static void tree_collapse_callback              (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 NautilusTreeView      *view);
static void tree_select_row_callback            (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 gint                   column,
						 NautilusTreeView      *view);
static void nautilus_tree_view_load_uri         (NautilusTreeView      *view,
						 const char            *uri);
static void nautilus_tree_view_update_all_icons (NautilusTreeView *view);


static void nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass);
static void nautilus_tree_view_initialize       (NautilusTreeView      *view);
static void nautilus_tree_view_destroy          (GtkObject             *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view, GTK_TYPE_SCROLLED_WINDOW)
     


static void
nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_view_destroy;
}


static gboolean
nautilus_tree_view_should_skip_file (NautilusTreeView *view,
				     NautilusFile *file)
{
	char *name;
	gboolean should_skip;

	should_skip = FALSE;

	/* FIXME: maybe this should track the "show hidden files" preference? */

	if (!view->details->show_hidden_files) {

		/* FIXME bugzilla.eazel.com 653: 
		 * Eventually this should become a generic filtering thingy. 
		 */
			
		name = nautilus_file_get_name (file);
			
		g_assert (name != NULL);
			
		if (nautilus_str_has_prefix (name, ".")) {
			should_skip = TRUE;
		}
		
		g_free (name);
	}
		
	return should_skip;
}


static void
insert_hack_node (NautilusTreeView *view, const char *uri)
{
 	GtkCTreeNode *view_node;
	GtkCTreeNode *hack_node;
	char *text[2];

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);

	if (hack_node == NULL) {
		text[0] = "...HACK...";
		text[1] = NULL;

		view_node = uri_to_view_node (view, uri);

		hack_node = gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
						   view_node, 
						   NULL,
						   text,
						   TREE_SPACING,
						   NULL, NULL, NULL, NULL,
						   FALSE,
						   FALSE);

		g_hash_table_insert (view->details->uri_to_hack_node_map, 
				     (char *) uri, hack_node);
	}
}


static void
remove_hack_node (NautilusTreeView *view, const char *uri)
{
	GtkCTreeNode *hack_node;

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);
       
	if (hack_node != NULL) {
		gtk_ctree_remove_node (GTK_CTREE (view->details->tree),
				       hack_node);

		g_hash_table_remove (view->details->uri_to_hack_node_map, uri);

		gtk_clist_thaw (GTK_CLIST (view->details->tree));
	}
}


static void
freeze_if_have_hack_node (NautilusTreeView *view, const char *uri)
{
	GtkCTreeNode *hack_node;

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);

	if (hack_node != NULL) {
		gtk_clist_freeze (GTK_CLIST (view->details->tree));
	}
}

static void
nautilus_tree_view_insert_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *parent_view_node;
 	GtkCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *text[2];
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	if (nautilus_tree_view_should_skip_file (view, file)) {
		return;
	}

	uri = nautilus_file_get_uri (file);
	
#ifdef DEBUG_TREE
	printf ("Inserting URI into tree: %s\n", uri);
#endif

	parent_view_node = nautilus_tree_view_find_parent_node (view, uri);


#ifdef DEBUG_TREE
	printf ("parent_view_node 0x%x (%s)\n", (unsigned) parent_view_node, 
		view_node_to_uri (view, node);
#endif


	text[0] = nautilus_file_get_name (file);
	text[1] = NULL;

	if (model_node_to_view_node (view, node) == NULL) {
		
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);

		view_node = gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
						   parent_view_node, 
						   NULL,
						   text,
						   TREE_SPACING,
						   pixmap, mask, pixmap, mask,
						   FALSE,
						   FALSE);

		gtk_ctree_node_set_row_data_full (GTK_CTREE (view->details->tree),
						  view_node,
						  g_strdup (uri),
						  g_free);


		g_hash_table_insert (view->details->uri_to_node_map, uri, view_node); 
		
		if (nautilus_file_is_directory (file)) {
			/* Gratuitous hack so node can be expandable w/o
			   immediately inserting all the real children. */

			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				gtk_ctree_expand (GTK_CTREE (GTK_CTREE (view->details->tree)),
						  view_node);
			} else {
				insert_hack_node (view, uri);
			}
		}
	}

	g_free (text[0]);

	if (parent_view_node != NULL) {
		remove_hack_node (view, view_node_to_uri (view, parent_view_node));
	}
}



static void
nautilus_tree_view_remove_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	char *uri;

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	view_node = model_node_to_view_node (view, node); 
	
	if (view_node != NULL) {
		gtk_ctree_remove_node (GTK_CTREE (view->details->tree),
				       view_node);
		
		/* FIXME: free the original key */
		g_hash_table_remove (view->details->uri_to_node_map, uri); 
	}

	nautilus_tree_expansion_state_remove_node (view->details->expansion_state,
						   uri);

	g_free (uri);
}




static gboolean
ctree_is_node_expanded (GtkCTree     *ctree,
			GtkCTreeNode *node)
{
	gchar     *text;
	guint8     spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean   is_leaf;
	gboolean   expanded;

	gtk_ctree_get_node_info (ctree, node,
				 &text, &spacing,
				 &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened,
				 &is_leaf, &expanded);
	return expanded;
}

static void
nautilus_tree_view_update_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *name;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	uri = nautilus_file_get_uri (file);

	view_node = model_node_to_view_node (view, node);

	if (view_node != NULL) {

		name = nautilus_tree_view_uri_to_name (uri);
	
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);
		
		gtk_ctree_node_set_pixtext (GTK_CTREE (view->details->tree),
					    view_node,
					    0,
					    name,
					    TREE_SPACING,
					    pixmap,
					    mask);
		
#if 0
		/* FIXME: should switch to this call so we can set open/closed pixamps */
		void gtk_ctree_set_node_info  (GtkCTree     *ctree,
					       GtkCTreeNode *node,
					       const gchar  *text,
					       guint8        spacing,
					       GdkPixmap    *pixmap_closed,
					       GdkBitmap    *mask_closed,
					       GdkPixmap    *pixmap_opened,
					       GdkBitmap    *mask_opened,
					       gboolean      is_leaf,
					       gboolean      expanded);
#endif	

		if (nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				if (!ctree_is_node_expanded (GTK_CTREE (GTK_CTREE (view->details->tree)),
							     view_node)) {
					gtk_ctree_expand (GTK_CTREE (GTK_CTREE (view->details->tree)),
							  view_node);
				} else {
					reload_node_for_uri (view, uri);
				}
			} else {
				insert_hack_node (view, uri);
			}
		}
	} else {
		g_free (uri);
	}
}


static void
notify_done_loading (NautilusTreeView *view,
		     NautilusTreeNode *node)
{
	TreeViewCallback callback;

	if (view->details->uri_loaded_or_parent_done_loading != NULL &&
	    view->details->wait_node == node) {
		callback = view->details->uri_loaded_or_parent_done_loading;
		view->details->wait_node = NULL;
		g_free (view->details->wait_uri);
		view->details->wait_uri = NULL;

		(*callback) (view);
	}
}

static void
notify_node_seen (NautilusTreeView *view,
		  NautilusTreeNode *node)
{
	TreeViewCallback root_callback;
	TreeViewCallback callback;
	char *uri;

	if (!view->details->root_seen) {
		view->details->root_seen = TRUE;

		if (view->details->root_seen_callback != NULL) {
			root_callback = view->details->root_seen_callback;
			view->details->root_seen_callback = NULL;
			
			(*root_callback) (view);
		}
	}

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (nautilus_strcmp (uri, view->details->wait_uri) == 0) {
		callback = view->details->uri_loaded_or_parent_done_loading;
		view->details->wait_node = NULL;
		g_free (view->details->wait_uri);
		view->details->wait_uri = NULL;

		(*callback) (view);
	}
}





static void
nautilus_tree_view_model_node_added_callback (NautilusTreeModel *model,
					      NautilusTreeNode  *node,
					      gpointer           callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	nautilus_tree_view_insert_model_node (view, node);
	notify_node_seen (view, node);
}





static void
nautilus_tree_view_model_node_changed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	/* Assume file did not change location - we will model that as a remove + an add */
	nautilus_tree_view_update_model_node (view, node);
}

static void
nautilus_tree_view_model_node_removed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);

	nautilus_tree_view_remove_model_node (view, node);
}


static void
nautilus_tree_view_model_done_loading_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;
	char *uri;

	view = NAUTILUS_TREE_VIEW (callback_data);

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));

	remove_hack_node (view, uri);

	g_free (uri);

	notify_done_loading (view, node);
}

static void
nautilus_tree_view_load_from_filesystem (NautilusTreeView *view)
{
	view->details->model = nautilus_tree_model_new ("file:///");

	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_added_callback,
					 view);

	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_added",
			    nautilus_tree_view_model_node_added_callback,
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
			    "done_loading_children",
			    nautilus_tree_view_model_done_loading_callback,
			    view);

}



static void
nautilus_tree_view_initialize (NautilusTreeView *view)
{
	/* set up scrolled window */
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
						
	view->details = g_new0 (NautilusTreeViewDetails, 1);

	/* set up expansion state */
	view->details->expansion_state = nautilus_tree_expansion_state_new ();
	

	/* set up ctree */
	view->details->tree = gtk_ctree_new (1, 0);

        gtk_clist_set_selection_mode (GTK_CLIST (view->details->tree), GTK_SELECTION_BROWSE);
	gtk_clist_set_auto_sort (GTK_CLIST (view->details->tree), TRUE);
	gtk_clist_set_sort_type (GTK_CLIST (view->details->tree), GTK_SORT_ASCENDING);
	gtk_clist_set_column_auto_resize (GTK_CLIST (view->details->tree), 0, TRUE);
	gtk_clist_columns_autosize (GTK_CLIST (view->details->tree));

	gtk_clist_set_reorderable (GTK_CLIST (view->details->tree), FALSE);

	gtk_ctree_set_expander_style (GTK_CTREE (view->details->tree), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_ctree_set_line_style (GTK_CTREE (view->details->tree), GTK_CTREE_LINES_NONE);

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

	/* set up view */
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (tree_load_location_callback), 
			    view);

	view->details->uri_to_node_map = g_hash_table_new (g_str_hash, g_str_equal);
	view->details->uri_to_hack_node_map = g_hash_table_new (g_str_hash, g_str_equal);
	
	nautilus_tree_view_load_from_filesystem (view);

	gtk_signal_connect_object_while_alive
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 nautilus_tree_view_update_all_icons,
		 GTK_OBJECT (view));	

	gtk_widget_show (view->details->tree);

	gtk_container_add (GTK_CONTAINER (view), view->details->tree);

	gtk_widget_show (GTK_WIDGET (view));
}



static void
disconnect_model_handlers (NautilusTreeView *view)
{
	/* stop monitoring all the nodes, then stop monitoring the
           whole */
}


static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);
	
	disconnect_model_handlers (view);


	nautilus_tree_expansion_state_save (view->details->expansion_state);

	gtk_object_unref (GTK_OBJECT (view->details->expansion_state));


	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * nautilus_tree_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusTreeView to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_tree_view_get_nautilus_view (NautilusTreeView *view)
{
	return view->details->nautilus_view;
}


static GtkCTreeNode *
uri_to_view_node (NautilusTreeView *view,
		  const char *uri)
{
	return g_hash_table_lookup (view->details->uri_to_node_map, uri);
}


static GtkCTreeNode *
model_node_to_view_node (NautilusTreeView *view,
			 NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	char *uri;

	if (node == NULL) {
		return NULL;
	}

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	view_node = uri_to_view_node (view, uri);
	g_free (uri);

	return view_node;
}

static const char *
view_node_to_uri (NautilusTreeView *view,
		  GtkCTreeNode *node)
{
	return (const char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
							   node);
}

static GList *
get_uri_sequence_to_root (char *uri_text)
{
	GList *retval;
	GnomeVFSURI *uri;
	GnomeVFSURI *parent_uri;

	retval = NULL;

	uri = gnome_vfs_uri_new (uri_text);

	retval = g_list_prepend (retval, uri_text);
	
	if (uri == NULL) {
		return retval;
	}

	while (1) {
		parent_uri = gnome_vfs_uri_get_parent (uri);
		
		gnome_vfs_uri_unref (uri);

		if  (parent_uri == NULL) {
			return retval;
		}

		uri = parent_uri;
		uri_text = gnome_vfs_uri_to_string (uri, 
						    GNOME_VFS_URI_HIDE_NONE);

		retval = g_list_prepend (retval, uri_text);
	}
}


static void
call_when_root_seen (NautilusTreeView *view, 
		     TreeViewCallback callback)
{
	view->details->root_seen_callback = callback;
}

static void
call_when_uri_loaded_or_parent_done_loading (NautilusTreeView *view,
					     const char *uri, 
					     NautilusTreeNode *node,
					     TreeViewCallback callback)
{
	view->details->wait_uri = g_strdup (uri);
	view->details->wait_node = node;

	view->details->uri_loaded_or_parent_done_loading = callback;
}


static void
cancel_selection_in_progress (NautilusTreeView *view)
{
	nautilus_g_list_free_deep (view->details->in_progress_select_uris);
	view->details->in_progress_select_uris = NULL;

	view->details->root_seen_callback = NULL;

	g_free (view->details->wait_uri);
	view->details->wait_uri = NULL;
	view->details->wait_node = NULL;

	view->details->uri_loaded_or_parent_done_loading = NULL;
}

static void
expand_uri_sequence_and_select_end (NautilusTreeView *view)
{
	const char *uri;
	GList *p;
	GList *old_sequence;
	GtkCTreeNode *view_node;
	gboolean at_least_one_found;

	at_least_one_found = FALSE;

	if (!view->details->root_seen) {
		call_when_root_seen (view, expand_uri_sequence_and_select_end);
		return;
	}

	for (p = view->details->in_progress_select_uris; p != NULL; p = p->next) {
		uri = (char *) p->data;

		view_node = uri_to_view_node (view, uri);

		if (view_node == NULL) {
			break;
		}

		at_least_one_found = TRUE;

		if (p->next != NULL) {
			gtk_ctree_expand (GTK_CTREE (view->details->tree),
					  view_node);
		} else {
			g_free (view->details->selected_uri);
			view->details->selected_uri = g_strdup (uri);
			gtk_ctree_select (GTK_CTREE (view->details->tree),
					  view_node);
		}
	}
		
	if (!at_least_one_found) {
		/* The target URI just isn't in the tree at all; don't
                   expand, load or select anything */
		
		cancel_selection_in_progress (view);

		return;
	}

	if (p != NULL) {
		/* Not all the nodes existed yet, damn */
		
		old_sequence = view->details->in_progress_select_uris;
		
		view->details->in_progress_select_uris = p;

		call_when_uri_loaded_or_parent_done_loading (view, uri, 
							     nautilus_tree_model_get_node (view->details->model,
											   (char *) p->prev->data),
							     expand_uri_sequence_and_select_end);


		p->prev->next = NULL;
		p->prev = NULL;
		nautilus_g_list_free_deep (old_sequence);

		return;
	}

	/* We're all done, clean up potential remaining junk. */

	cancel_selection_in_progress (view);
}


/**
 * nautilus_tree_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * 
 **/
void
nautilus_tree_view_load_uri (NautilusTreeView *view,
			     const char       *uri)
{
	char *canonical_uri;

	cancel_selection_in_progress (view);

	canonical_uri = nautilus_make_uri_canonical (uri);

	if (nautilus_strcmp (canonical_uri, view->details->selected_uri) == 0) {
		g_free (canonical_uri);
		return;
	}

	view->details->in_progress_select_uris = get_uri_sequence_to_root (canonical_uri);
			
	expand_uri_sequence_and_select_end (view);
}
 
static void
tree_load_location_callback (NautilusView *nautilus_view, 
			     const char *location,
			     NautilusTreeView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	/* Do the actual load. */
	nautilus_tree_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}


static void
reload_node_for_uri (NautilusTreeView *view,
		     const char *uri)
{
	GList *p;

	nautilus_tree_model_monitor_node (view->details->model,
					  nautilus_tree_model_get_node (view->details->model,
									uri),
					  view);

	for (p = nautilus_tree_node_get_children 
		(nautilus_tree_model_get_node (view->details->model, uri)); p != NULL; p = p->next) {
		nautilus_tree_view_update_model_node (view, (NautilusTreeNode *) p->data);
	}
}

static void
expand_node_for_uri (NautilusTreeView *view,
		     const char *uri)
{

	freeze_if_have_hack_node (view, uri);

	nautilus_tree_expansion_state_expand_node (view->details->expansion_state,
						   uri);

	reload_node_for_uri (view, uri);
}

static void
tree_expand_callback (GtkCTree         *ctree,
		      GtkCTreeNode     *node,
		      NautilusTreeView *view)
{
	const char *uri;

	uri = view_node_to_uri (view, node);

	expand_node_for_uri (view, uri);
}



static void
tree_collapse_callback (GtkCTree         *ctree,
			GtkCTreeNode     *node,
			NautilusTreeView *view)
{
	const char *uri;

	uri = view_node_to_uri (view, node);

	nautilus_tree_expansion_state_collapse_node (view->details->expansion_state,
						     uri);

	nautilus_tree_model_stop_monitoring_node_recursive (view->details->model,
							    nautilus_tree_model_get_node (view->details->model,
											  uri),
							    view);
}




static void
tree_select_row_callback (GtkCTree              *tree,
			  GtkCTreeNode          *node,
			  gint                   column,
			  NautilusTreeView      *view)
{
	const char *uri;
	
	uri = view_node_to_uri (view, node);
	
	if (uri != NULL &&
	    nautilus_strcmp (view->details->selected_uri, uri) != 0) {
		nautilus_view_open_location (NAUTILUS_VIEW (view->details->nautilus_view), uri);
		
		g_free (view->details->selected_uri);
		view->details->selected_uri = g_strdup (uri);
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


static char *
nautilus_tree_view_uri_to_name (const char *uri)
{
	GnomeVFSURI *gnome_vfs_uri;
	char *name;
	
	gnome_vfs_uri = gnome_vfs_uri_new (uri);
	name = g_strdup (gnome_vfs_uri_extract_short_path_name (gnome_vfs_uri));
	gnome_vfs_uri_unref (gnome_vfs_uri);

	return name;
}

static GtkCTreeNode *
nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
				     const char *uri)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node (view->details->model, uri);

	g_assert (node != NULL);

	return model_node_to_view_node (view, nautilus_tree_node_get_parent (node));
}
