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

	GHashTable *uri_to_node_map;
	GHashTable *uri_to_directory_map;

	GList *directory_read_queue;
	NautilusDirectory *current_directory;

	GList *pending_files_added;
	GList *pending_files_changed;

	guint display_pending_timeout_id;
	guint display_pending_idle_id;
	guint files_added_handler_id;
	guint files_changed_handler_id;

	gboolean show_hidden_files;
};

#define TREE_SPACING 3

static void nautilus_tree_view_insert_file (NautilusTreeView *view, NautilusFile *file);

static void nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass);
static void nautilus_tree_view_initialize       (NautilusTreeView      *view);
static void nautilus_tree_view_destroy          (GtkObject             *object);
static void tree_load_location_callback         (NautilusView          *nautilus_view,
						 const char            *location,
						 NautilusTreeView      *view);
static void tree_expand_callback                (NautilusView          *nautilus_view,
						 GtkCTreeNode          *node,
						 NautilusTreeView      *view);

static void nautilus_tree_view_load_uri         (NautilusTreeView      *view,
						 const char            *uri);

static void files_added_callback                (NautilusDirectory *directory,
						 GList *files,
						 gpointer callback_data);

static void files_changed_callback              (NautilusDirectory *directory,
						 GList *files,
						 gpointer callback_data);

static void done_loading_callback               (NautilusDirectory *directory,
						 gpointer callback_data);



NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view, GTK_TYPE_SCROLLED_WINDOW)
     

static void
nautilus_tree_view_file_added (NautilusTreeView *view, NautilusFile *file)
{
#ifdef DEBUG_TREE
	printf ("file added: %s\n", nautilus_file_get_uri (file));
#endif
	nautilus_tree_view_insert_file (view, file);

}

static void
nautilus_tree_view_file_changed (NautilusTreeView *view, NautilusFile *file)
{
#ifdef DEBUG_TREE
	printf ("file changed: %s\n", nautilus_file_get_uri (file));
#endif
	/* FIXME bugzilla.eazel.com 1522: should actually do something here. */
}


static void
nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_view_destroy;
}


static void
nautilus_tree_view_read_head_of_queue (NautilusTreeView *view)
{
	if (view->details->directory_read_queue == NULL) {
#ifdef DEBUG_TREE
		puts ("No queue");
#endif
		return;
	}

	view->details->current_directory = NAUTILUS_DIRECTORY (view->details->directory_read_queue->data);


#ifdef DEBUG_TREE
	printf ("XXX: Now scanning directory %s\n", nautilus_directory_get_uri (view->details->current_directory));
#endif

	/* Attach a handler to get any further files that show up as we
	 * load and sychronize. We won't miss any files because this
	 * signal is emitted from an idle routine and so we will be
	 * connected before the next time it is emitted.
	 */
    	view->details->files_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->current_directory),
		 "files_added",
		 GTK_SIGNAL_FUNC (files_added_callback),
		 view);
	view->details->files_changed_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->current_directory), 
		 "files_changed",
		 GTK_SIGNAL_FUNC (files_changed_callback),
		 view);
    	view->details->files_added_handler_id = gtk_signal_connect
		(GTK_OBJECT (view->details->current_directory),
		 "done_loading",
		 GTK_SIGNAL_FUNC (done_loading_callback),
		 view);

	nautilus_directory_file_monitor_add (view->details->current_directory, view,
					     NULL, FALSE, TRUE,
					     files_added_callback, view);




	view->details->directory_read_queue = view->details->directory_read_queue->next;
}


static gboolean
display_pending_files (NautilusTreeView *view)
{
	GList *files_added, *files_changed, *p;
	NautilusFile *file;

	if (view->details->current_directory != NULL
	    && nautilus_directory_are_all_files_seen (view->details->current_directory)) {
		/* done_loading (view); */
	}

	files_added = view->details->pending_files_added;
	files_changed = view->details->pending_files_changed;

	if (files_added == NULL && files_changed == NULL) {
		return FALSE;
	}

	view->details->pending_files_added = NULL;
	view->details->pending_files_changed = NULL;

	/* XXX gtk_signal_emit (GTK_OBJECT (view), signals[BEGIN_ADDING_FILES]); */

	for (p = files_added; p != NULL; p = p->next) {
		file = p->data;
		
		/* FIXME bugzilla.eazel.com 1520: Need to do a better test to see if the info is up to date. */
#if 0
		if (nautilus_directory_contains_file (view->details->current_directory, file)) {
#endif
			nautilus_tree_view_file_added (view, file);
#if 0
		}
#endif
	}

	for (p = files_changed; p != NULL; p = p->next) {
		file = p->data;

		nautilus_tree_view_file_changed (view, file);
	}

	/* gtk_signal_emit (GTK_OBJECT (view), signals[DONE_ADDING_FILES]); */

	nautilus_file_list_free (files_added);
	nautilus_file_list_free (files_changed);

	return TRUE;
}


static gboolean
display_pending_idle_callback (gpointer data)
{
	/* Don't do another idle until we receive more files. */

	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (data);

	view->details->display_pending_idle_id = 0;

	display_pending_files (view);

	return FALSE;
}


static gboolean
display_pending_timeout_callback (gpointer data)
{
	/* Do another timeout if we displayed some files.
	 * Once we get all the files, we'll start using
	 * idle instead.
	 */

	NautilusTreeView *view;
	gboolean displayed_some;

	view = NAUTILUS_TREE_VIEW (data);

	displayed_some = display_pending_files (view);
	if (displayed_some) {
		return TRUE;
	}

	view->details->display_pending_timeout_id = 0;
	return FALSE;
}



static void
unschedule_timeout_display_of_pending_files (NautilusTreeView *view)
{
	/* Get rid of timeout if it's active. */
	if (view->details->display_pending_timeout_id != 0) {
		g_assert (view->details->display_pending_idle_id == 0);
		gtk_timeout_remove (view->details->display_pending_timeout_id);
		view->details->display_pending_timeout_id = 0;
	}
}

static void
schedule_idle_display_of_pending_files (NautilusTreeView *view)
{
	/* No need to schedule an idle if there's already one pending. */
	if (view->details->display_pending_idle_id != 0) {
		return;
	}

	/* An idle takes precedence over a timeout. */
	unschedule_timeout_display_of_pending_files (view);

	view->details->display_pending_idle_id =
		gtk_idle_add (display_pending_idle_callback, view);
}

static void
schedule_timeout_display_of_pending_files (NautilusTreeView *view)
{
	/* No need to schedule a timeout if there's already one pending. */
	if (view->details->display_pending_timeout_id != 0) {
		return;
	}

	/* An idle takes precedence over a timeout. */
	if (view->details->display_pending_idle_id != 0) {
		return;
	}

	view->details->display_pending_timeout_id =
		gtk_timeout_add (DISPLAY_TIMEOUT_INTERVAL_MSECS,
				 display_pending_timeout_callback, view);
}

static void
unschedule_idle_display_of_pending_files (NautilusTreeView *view)
{
	/* Get rid of idle if it's active. */
	if (view->details->display_pending_idle_id != 0) {
		g_assert (view->details->display_pending_timeout_id == 0);
		gtk_idle_remove (view->details->display_pending_idle_id);
		view->details->display_pending_idle_id = 0;
	}
}

static void
unschedule_display_of_pending_files (NautilusTreeView *view)
{
	unschedule_idle_display_of_pending_files (view);
	unschedule_timeout_display_of_pending_files (view);
}

static void
queue_pending_files (NautilusTreeView *view,
		     GList *files,
		     GList **pending_list)
{
	GList *filtered_files = NULL;
	GList *files_iterator;
	NautilusFile *file;
	char * name;

	/* Filter out hidden files if needed */
	if (!view->details->show_hidden_files) {
		/* FIXME bugzilla.eazel.com 653: 
		 * Eventually this should become a generic filtering thingy. 
		 */
		for (files_iterator = files; 
		     files_iterator != NULL; 
		     files_iterator = files_iterator->next) {
			file = NAUTILUS_FILE (files_iterator->data);
			
			g_assert (file != NULL);
			
			name = nautilus_link_get_display_name (nautilus_file_get_name (file));
			
			g_assert (name != NULL);
			
			if (!nautilus_str_has_prefix (name, ".")) {
				filtered_files = g_list_append (filtered_files, file);
			}
			
			g_free (name);
		}
		
		files = filtered_files;
	}

	/* Put the files on the pending list if there are any. */
	if (files != NULL) {
		nautilus_file_list_ref (files);
		*pending_list = g_list_concat (*pending_list, g_list_copy (files));
	
		/* If we haven't see all the files yet, then we'll wait for the
		 * timeout to fire. If we have seen all the files, then we'll use
		 * an idle instead.
		 */
		if (nautilus_directory_are_all_files_seen (view->details->current_directory)) {
			schedule_idle_display_of_pending_files (view);
		} else {
			schedule_timeout_display_of_pending_files (view);
		}
	}
}

static void
files_added_callback (NautilusDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	NautilusTreeView *view;

#ifdef DEBUG_TREE
	puts ("XXX - files_added_callback");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_added);
}

static void
done_loading_callback (NautilusDirectory *directory,
		       gpointer callback_data)
{
	NautilusTreeView *view;

#ifdef DEBUG_TREE
	puts ("XXX - files_added_callback");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);

	if (nautilus_directory_are_all_files_seen (directory)) {
#ifdef DEBUG_TREE
		puts ("XXX - should move head of queue ahead here");
#endif		

#ifdef DEBUG_TREE
		printf ("Just finished reading '%s', moving queue ahead to '%s'\n",
			nautilus_directory_get_uri (directory),
			(view->details->directory_read_queue == NULL ? "none" : nautilus_directory_get_uri (NAUTILUS_DIRECTORY (view->details->directory_read_queue->data))));
#endif DEBUG_TREE


		view->details->current_directory = NULL;
		nautilus_tree_view_read_head_of_queue (view);
	}
}


static void
files_changed_callback (NautilusDirectory *directory,
			GList *files,
			gpointer callback_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (callback_data);
	queue_pending_files (view, files, &view->details->pending_files_changed);
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
nautilus_tree_view_find_parent_node (NautilusTreeView *view, const char *uri)
{
	GnomeVFSURI *vfs_uri;
	GnomeVFSURI *parent_uri;
	char *parent_uri_text;
	GtkCTreeNode *retval;

	vfs_uri = gnome_vfs_uri_new (uri);
	
	parent_uri = gnome_vfs_uri_get_parent (vfs_uri);

	if (parent_uri == NULL) {
		gnome_vfs_uri_unref (vfs_uri);
		return NULL;
	}
	
	parent_uri_text = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);

#ifdef DEBUG_TREE
	printf ("XXX - looking for parent node name: %s\n", parent_uri_text);
#endif

	retval = g_hash_table_lookup (view->details->uri_to_node_map, parent_uri_text);

	g_free (parent_uri_text);
	gnome_vfs_uri_unref (parent_uri);
	gnome_vfs_uri_unref (vfs_uri);

	return retval;
}

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




static void
nautilus_tree_view_insert_file (NautilusTreeView *view, NautilusFile *file)
{
	GtkCTreeNode *parent_node;
	GtkCTreeNode *node;
	GtkCTreeNode *previous_sibling_node;
	char *name;
	char *text[2];
	char *canonical_uri;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	canonical_uri = nautilus_tree_view_get_canonical_uri (nautilus_file_get_uri (file));

#ifdef DEBUG_TREE
	printf ("Inserting URI into tree: %s\n", canonical_uri);
#endif


	parent_node = nautilus_tree_view_find_parent_node (view, canonical_uri);
	previous_sibling_node = NULL;

	name = nautilus_tree_view_uri_to_name (canonical_uri);

	text[0] = name;
	text[1] = NULL;

	node = g_hash_table_lookup (view->details->uri_to_node_map, canonical_uri);

	if (node == NULL) {
		
		/* FIXME bugzilla.eazel.com 1524: still need to
		   respond to icon theme changes. */


		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);

		node = gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
					      parent_node, 
					      previous_sibling_node,
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
		if (nautilus_file_is_directory (file)) {
			/* Gratuitous hack so node can be expandable w/o
			   immediately inserting all the real children. */
			
			text[0] = "...HACK...";
			text[1] = NULL;
			gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
					       node, 
					       NULL,
					       text,
					       TREE_SPACING,
					       NULL, NULL, NULL, NULL,
					       FALSE,
					       FALSE);
		}
	}



	gtk_ctree_node_set_row_data_full (GTK_CTREE (view->details->tree),
					  node,
					  g_strdup (canonical_uri),
					  g_free);


#ifdef DEBUG_TREE
	printf ("XXX - putting node in hash_table: %s\n", canonical_uri);
#endif

	g_hash_table_insert (view->details->uri_to_node_map, canonical_uri, node); 
}



static void
nautilus_tree_view_enqueue_directory (NautilusTreeView *view,
				      NautilusDirectory *directory)
{
	/* FIXME bugzilla.eazel.com 1526: check if it's already on the
           queue first. */

	view->details->directory_read_queue = g_list_append (view->details->directory_read_queue, 
							     directory);

#ifdef DEBUG_TREE
	printf ("just enqueue'd: 0x%x (%s)\n", (unsigned) directory, nautilus_directory_get_uri (directory));
	printf ("queue length: %d\n", g_list_length (view->details->directory_read_queue));
#endif

	if (view->details->current_directory == NULL) {
#ifdef DEBUG_TREE
		printf ("reading head of queue");
#endif
		nautilus_tree_view_read_head_of_queue (view);
	}
}

static void
nautilus_tree_view_load_uri_children (NautilusTreeView *view, const char *uri, gboolean is_root)
{
 	NautilusDirectory *directory;
 	NautilusFile *file;

	char *canonical_uri;
	
	canonical_uri = nautilus_tree_view_get_canonical_uri (uri);

	file = nautilus_file_get (canonical_uri);

	/* FIXME bugzilla.eazel.com 1519: remove gone files when
	   rescaning directory. To implement it here, we should remove all
	   children of the node for the dir before we load it. */

	if (is_root || nautilus_file_is_directory (file)) {
		directory = nautilus_directory_get (canonical_uri);

		/* Should clear out all children first */
		
		nautilus_tree_view_enqueue_directory (view,
						      directory);
	}

	nautilus_file_unref (file);
}



static void
nautilus_tree_view_load_from_filesystem (NautilusTreeView *view)
{
	NautilusFile *file;
	/* Start loading. */

	file = nautilus_file_get ("file:/");

	nautilus_tree_view_insert_file (view, file);
	nautilus_tree_view_load_uri_children (view, "file:/", TRUE);

	nautilus_file_unref (file);
}




static void
nautilus_tree_view_initialize (NautilusTreeView *view)
{
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);
						
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
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (tree_load_location_callback), 
			    view);

	view->details->uri_to_node_map = g_hash_table_new (g_str_hash, g_str_equal);

	nautilus_tree_view_load_from_filesystem (view);

	gtk_widget_show (view->details->tree);

	gtk_container_add (GTK_CONTAINER (view), view->details->tree);

	gtk_widget_show (GTK_WIDGET (view));
}


static void
disconnect_handler (NautilusTreeView *view, int *id)
{
	if (*id != 0) {
		gtk_signal_disconnect (GTK_OBJECT (view->details->current_directory), *id);
		*id = 0;
	}
}

static void
disconnect_model_handlers (NautilusTreeView *view)
{
	/* FIXME bugzilla.eazel.com 1527: this is bogus, we need to
	   disconnect from all NautilusDirectories, not just the most
	   recent one. */

	disconnect_handler (view, &view->details->files_added_handler_id);
	disconnect_handler (view, &view->details->files_changed_handler_id);
	if (view->details->current_directory != NULL) {
		nautilus_directory_file_monitor_remove (view->details->current_directory, view);
	}
}


static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);
	

	if (view->details->current_directory != NULL) {
		disconnect_model_handlers (view);
		nautilus_directory_unref (view->details->current_directory);
	}

	unschedule_display_of_pending_files (view);

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
nautilus_tree_expand_uri (NautilusTreeView *view, const char *uri)
{
	/* Refresh children */
	nautilus_tree_view_load_uri_children (view, uri, FALSE);
}


static void
tree_expand_callback (NautilusView     *nautilus_view,
		      GtkCTreeNode     *node,
		      NautilusTreeView *view)
{
	char *uri;

	uri = (char *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
						    node);

	nautilus_tree_expand_uri (view, uri);
}



