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

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libgnomevfs/gnome-vfs.h>


#include <stdio.h>

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;

	GtkWidget *tree;

	NautilusTreeModel *model;

	GHashTable *uri_to_node_map;
	GHashTable *uri_to_hack_node_map;

	gboolean show_hidden_files;
};

#define TREE_SPACING 5

static char         *nautilus_tree_view_uri_to_name      (const char       *uri);
static GtkCTreeNode *nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
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




#if 0
/* FIXME bugzilla.eazel.com 1523: this doesn't really do everything
   you might want to do when canonifying a URI. If it did, it might
   want to move to libnautilus-extensions, or gnome-vfs. What this
   should really do is parse the URI apart and reassemble it. */

static char *
nautilus_tree_view_get_canonical_uri (const char *uri)
{
	char *scheme_end;
	char *canonical_uri;

	scheme_end = strchr (uri, ':');

	g_assert (scheme_end != NULL);

	if (scheme_end[1] == '/' && scheme_end[2] != '/') {
		canonical_uri = g_malloc (strlen (uri) + 3);
		strncpy (canonical_uri, uri, (scheme_end - uri) + 1);
		strcpy (canonical_uri + (scheme_end - uri) + 1, "//");
		strcpy (canonical_uri + (scheme_end - uri) + 3, scheme_end + 1);
	} else {
		canonical_uri = g_strdup (uri);
	}

	return canonical_uri;
}

#endif



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

		view_node = g_hash_table_lookup (view->details->uri_to_node_map, uri);

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
		(char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
						      parent_view_node));
#endif


	text[0] = nautilus_file_get_name (file);
	text[1] = NULL;

	if (g_hash_table_lookup (view->details->uri_to_node_map, uri) == NULL) {
		
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
						   
						   /* FIXME bugzilla.eazel.com 1525: should 
						      remember what was
						      expanded last time and what
						      wasn't. On first start, we should
						      expand "/" and leave everything else
						      collapsed. */
						   FALSE);

		gtk_ctree_node_set_row_data_full (GTK_CTREE (view->details->tree),
						  view_node,
						  g_strdup (uri),
						  g_free);


		g_hash_table_insert (view->details->uri_to_node_map, uri, view_node); 
		
		if (nautilus_file_is_directory (file)) {
			/* Gratuitous hack so node can be expandable w/o
			   immediately inserting all the real children. */

			insert_hack_node (view, uri);
		}
	}

	g_free (text[0]);

	if (parent_view_node != NULL) {
		remove_hack_node (view, (char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
									      parent_view_node));
	}
}



static void
nautilus_tree_view_remove_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	
	file = nautilus_tree_node_get_file (node);

	uri = nautilus_file_get_uri (file);
	
	view_node = g_hash_table_lookup (view->details->uri_to_node_map, uri); 
	
	if (view_node != NULL) {
		gtk_ctree_remove_node (GTK_CTREE (view->details->tree),
				       view_node);
		
		/* FIXME: free the original key */
		g_hash_table_remove (view->details->uri_to_node_map, uri); 
	}

	g_free (uri);
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

	/* FIXME: leaks non-canonical URI */
	uri = nautilus_file_get_uri (file);

	view_node = g_hash_table_lookup (view->details->uri_to_node_map, uri); 

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
	} else {
		g_free (uri);
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
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
						
	view->details = g_new0 (NautilusTreeViewDetails, 1);

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
	/* FIXME bugzilla.eazel.com 1528: Should just change selection
           here. */
}
 
static void
tree_load_location_callback (NautilusView *nautilus_view, 
			     const char *location,
			     NautilusTreeView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	/* It's mandatory to send an underway message once the
	 * component starts loading, otherwise nautilus will assume it
	 * failed. In a real component, this will probably happen in
	 * some sort of callback from whatever loading mechanism it is
	 * using to load the data; this component loads no data, so it
	 * gives the progress update here.
	 */
	nautilus_view_report_load_underway (nautilus_view);
	
	/* Do the actual load. */
	nautilus_tree_view_load_uri (view, location);
	
	/* It's mandatory to call report_load_complete once the
	 * component is done loading successfully, or
	 * report_load_failed if it completes unsuccessfully. In a
	 * real component, this will probably happen in some sort of
	 * callback from whatever loading mechanism it is using to
	 * load the data; this component loads no data, so it gives
	 * the progress update here.
	 */
	nautilus_view_report_load_complete (nautilus_view);
}



static void
tree_expand_callback (GtkCTree         *ctree,
		      GtkCTreeNode     *node,
		      NautilusTreeView *view)
{
	const char *uri;

	uri = (const char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
							  node);

	freeze_if_have_hack_node (view, uri);

	nautilus_tree_model_monitor_node (view->details->model,
					  nautilus_tree_model_get_node (view->details->model,
									uri),
					  view);
}



static void
tree_collapse_callback (GtkCTree         *ctree,
			GtkCTreeNode     *node,
			NautilusTreeView *view)
{
	const char *uri;

	uri = (const char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
							  node);

	nautilus_tree_model_stop_monitoring_node (view->details->model,
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
	
	uri = (const char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
							  node);


	if (uri != NULL) {
		nautilus_view_open_location (NAUTILUS_VIEW (view->details->nautilus_view), uri);
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
	char *parent_uri;
	GtkCTreeNode *retval;

	node = nautilus_tree_model_get_node (view->details->model, uri);

	g_assert (node != NULL);

	if (nautilus_tree_node_get_parent (node) == NULL) {
		return NULL;
	}
	
	parent_uri = nautilus_file_get_uri 
		(nautilus_tree_node_get_file 
		 (nautilus_tree_node_get_parent (node)));

	retval = g_hash_table_lookup (view->details->uri_to_node_map, parent_uri);

	g_free (parent_uri);

	return retval;
}
