/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Darin Adler
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
 *       Anders Carlsson <andersca@gnu.org>
 *       Darin Adler <darin@bentspoon.com>
 */

/* nautilus-tree-view.c - tree sidebar panel
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-model.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-tree-view-drag-dest.h>

#define NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE "tree-sidebar-panel/expansion_state"

struct NautilusTreeViewDetails {
	GtkWidget *scrolled_window;
	GtkTreeView *tree_widget;
	GtkTreeModelSort *sort_model;
	NautilusTreeModel *child_model;

	NautilusFile *activation_file;
	GHashTable   *expanded_uris;

	NautilusTreeViewDragDest *drag_dest;
};

typedef struct {
	GList *uris;
	NautilusTreeView *view;
} PrependURIParameters;

BONOBO_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

/*
 *   The expansion state storage is pretty broken
 * conceptually we have a gconf key, but we can't
 * listen on it, since we don't want to sync all
 * tree views. We want to load the stored state per
 * new tree view we instantiate, and keep a track of
 * what nodes we are expanding.
 *
 *   We then arbitrarily serialize all the tree
 * view's expansion state - and the last one to shut
 * wins the GConf key value - it sucks, but it's what
 * happened in Nautilus 1.0
 *
 * - Michael Meeks (23/5/2002)
 */

static void
populate_expansion_hash (const char *string,
			 gpointer callback_data)
{
	char *key = g_strdup (string);

	g_hash_table_insert (callback_data, key, key);
}

static void
load_expansion_state (NautilusTreeView *view)
{
	EelStringList *uris;

	uris = eel_preferences_get_string_list (
		NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE);

	eel_string_list_for_each (uris, populate_expansion_hash,
				  view->details->expanded_uris);

	eel_string_list_free (uris);
}

static void
expand_row_if_stored (NautilusTreeView *view,
		      GtkTreePath      *path,
		      const char       *uri)
{
	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));
	g_return_if_fail (view->details != NULL);

	if (g_hash_table_lookup (view->details->expanded_uris, uri)) {
		if (!gtk_tree_view_expand_row (
			view->details->tree_widget, path, FALSE)) {
			g_warning ("Error expanding row '%s' '%s'", uri,
				   gtk_tree_path_to_string (path));
		}
		g_hash_table_remove (view->details->expanded_uris, uri);
	}
}

static void
row_inserted_expand_node_callback (GtkTreeModel     *tree_model,
				   GtkTreePath      *path,
				   GtkTreeIter      *iter,
				   NautilusTreeView *view)
{
	char *uri;
	GtkTreeIter  parent;
	GtkTreePath *sort_path, *parent_path;
	NautilusFile *file;

	file = nautilus_tree_model_iter_get_file (view->details->child_model, iter);

	if (file) {
		/*
		 *   We can't expand a node as it's created,
		 * we need to wait for the dummy child to be
		 * made, so it has children, so 'expand_node'
		 * doesn't fail.
		 */
		nautilus_file_unref (file);
		return;
	}

	if (!gtk_tree_model_iter_parent (tree_model, &parent, iter)) {
		g_warning ("Un-parented tree node");
		return;
	}

	file = nautilus_tree_model_iter_get_file (view->details->child_model, &parent);

	uri = nautilus_file_get_uri (file);
	g_return_if_fail (uri != NULL);

	parent_path = gtk_tree_model_get_path (tree_model, &parent);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(view->details->sort_model, parent_path);

	expand_row_if_stored (view, sort_path, uri);

	gtk_tree_path_free (sort_path);

	g_free (uri);

	nautilus_file_unref (file);
}

static NautilusFile *
sort_model_iter_to_file (NautilusTreeView *view, GtkTreeIter *iter)
{
	GtkTreeIter child_iter;

	gtk_tree_model_sort_convert_iter_to_child_iter (view->details->sort_model, &child_iter, iter);
	return nautilus_tree_model_iter_get_file (view->details->child_model, &child_iter);
}

static NautilusFile *
sort_model_path_to_file (NautilusTreeView *view, GtkTreePath *path)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->sort_model), &iter, path)) {
		return NULL;
	}
	return sort_model_iter_to_file (view, &iter);
}

static void
prepend_one_uri (GtkTreeView *tree_view,
		 GtkTreePath *path,
		 gpointer callback_data)
{
	PrependURIParameters *p;
	NautilusFile *file;

	p = callback_data;
	file = sort_model_path_to_file (p->view, path);
	if (file == NULL) {
		return;
	}
	p->uris = g_list_prepend (p->uris, nautilus_file_get_uri (file));
	nautilus_file_unref (file);
}

static void
save_expansion_state_callback (GtkTreeView      *tree_widget,
			       NautilusTreeView *view)
{
	PrependURIParameters p;
        EelStringList *uris;

	g_return_if_fail (NAUTILUS_IS_TREE_VIEW (view));

	p.uris = NULL;
	p.view = view;
        gtk_tree_view_map_expanded_rows (tree_widget, prepend_one_uri, &p);
        p.uris = g_list_sort (p.uris, eel_strcmp_compare_func);
        uris = eel_string_list_new_from_g_list (p.uris, TRUE);
	eel_g_list_free_deep (p.uris);
        eel_preferences_set_string_list (NAUTILUS_PREFERENCES_TREE_VIEW_EXPANSION_STATE, uris);
        eel_string_list_free (uris);
}

static void
got_activation_uri_callback (NautilusFile *file, gpointer callback_data)
{
        char *uri, *file_uri;
        NautilusTreeView *view;
	GdkScreen *screen;
	
        view = NAUTILUS_TREE_VIEW (callback_data);

	screen = gtk_widget_get_screen (GTK_WIDGET (view->details->tree_widget));

        g_assert (file == view->details->activation_file);

	/* FIXME: reenable && !eel_uris_match_ignore_fragments (view->details->current_main_view_uri, uri) */

	uri = nautilus_file_get_activation_uri (file);
	if (uri != NULL
	    && eel_str_has_prefix (uri, NAUTILUS_COMMAND_SPECIFIER)) {

		uri += strlen (NAUTILUS_COMMAND_SPECIFIER);
		nautilus_launch_application_from_command (screen, NULL, uri, NULL, FALSE);

	} else if (uri != NULL
	    	   && eel_str_has_prefix (uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER)) {
		   
		file_uri = nautilus_file_get_uri (file);
		nautilus_launch_desktop_file (screen, file_uri, NULL, NULL);
		g_free (file_uri);
		
	} else if (uri != NULL
		   && nautilus_file_is_executable (file)
		   && nautilus_file_can_execute (file)
		   && !nautilus_file_is_directory (file)) {	
		   
		file_uri = gnome_vfs_get_local_path_from_uri (uri);

		/* Non-local executables don't get launched. They act like non-executables. */
		if (file_uri == NULL) {
			nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), uri);
		} else {
			nautilus_launch_application_from_command (screen, NULL, file_uri, NULL, FALSE);
			g_free (file_uri);
		}
		   
	} else if (uri != NULL) {	
		nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), uri);
	}

	g_free (uri);
	nautilus_file_unref (view->details->activation_file);
	view->details->activation_file = NULL;
}

static void
cancel_activation (NautilusTreeView *view)
{
        if (view->details->activation_file == NULL) {
		return;
	}
	
	nautilus_file_cancel_call_when_ready
		(view->details->activation_file,
		 got_activation_uri_callback, view);
	nautilus_file_unref (view->details->activation_file);
        view->details->activation_file = NULL;
}

static void
selection_changed_callback (GtkTreeSelection *selection,
			    NautilusTreeView *view)
{
	GList *attrs;
	GtkTreeIter iter;

        cancel_activation (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	view->details->activation_file = sort_model_iter_to_file (view, &iter);
	if (view->details->activation_file == NULL) {
		return;
	}
		
	attrs = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_call_when_ready (view->details->activation_file, attrs,
				       got_activation_uri_callback, view);
	g_list_free (attrs);
}

static int
compare_rows (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer callback_data)
{
	NautilusFile *file_a, *file_b;
	int result;

	file_a = nautilus_tree_model_iter_get_file (NAUTILUS_TREE_MODEL (model), a);
	file_b = nautilus_tree_model_iter_get_file (NAUTILUS_TREE_MODEL (model), b);

	if (file_a == file_b) {
		result = 0;
	} else if (file_a == NULL) {
		result = -1;
	} else if (file_b == NULL) {
		result = +1;
	} else {
		result = nautilus_file_compare_for_sort (file_a, file_b,
							 NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
							 FALSE, FALSE);
	}

	nautilus_file_unref (file_a);
	nautilus_file_unref (file_b);

	return result;
}


static char *
get_root_uri_callback (NautilusTreeViewDragDest *dest,
		       gpointer user_data)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (user_data);

	return g_strdup ("file:///");
}

static NautilusFile *
get_file_for_path_callback (NautilusTreeViewDragDest *dest,
			    GtkTreePath *path,
			    gpointer user_data)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (user_data);

	return sort_model_path_to_file (view, path);
}

static void
move_copy_items_callback (NautilusTreeViewDragDest *dest,
			  const GList *item_uris,
			  const char *target_uri,
			  guint action,
			  int x,
			  int y,
			  gpointer user_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (user_data);

	nautilus_file_operations_copy_move
		(item_uris,
		 NULL,
		 target_uri,
		 action,
		 GTK_WIDGET (view->details->tree_widget),
		 NULL, NULL);
}

static void
create_tree (NautilusTreeView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	
	view->details->child_model = nautilus_tree_model_new (NULL);
	view->details->sort_model = GTK_TREE_MODEL_SORT
		(gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (view->details->child_model)));
	view->details->tree_widget = GTK_TREE_VIEW
		(gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->details->sort_model)));
	g_object_unref (view->details->sort_model);
	g_signal_connect_object
		(view->details->child_model, "row_inserted",
		 G_CALLBACK (row_inserted_expand_node_callback),
		 view, G_CONNECT_AFTER);
	nautilus_tree_model_set_root_uri (view->details->child_model, "file:///");
	g_object_unref (view->details->child_model);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
						 compare_rows, view, NULL);

	gtk_tree_view_set_headers_visible (view->details->tree_widget, FALSE);

	g_signal_connect_object (view->details->tree_widget, "destroy",
				 G_CALLBACK (save_expansion_state_callback), view, 0);

	view->details->drag_dest = 
		nautilus_tree_view_drag_dest_new (view->details->tree_widget);
	g_signal_connect_object (view->details->drag_dest, 
				 "get_root_uri",
				 G_CALLBACK (get_root_uri_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest, 
				 "get_file_for_path",
				 G_CALLBACK (get_file_for_path_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "move_copy_items",
				 G_CALLBACK (move_copy_items_callback),
				 view, 0);

	/* Create column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "pixbuf", NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
					     "pixbuf_expander_closed", NAUTILUS_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
					     "pixbuf_expander_open", NAUTILUS_TREE_MODEL_OPEN_PIXBUF_COLUMN,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", NAUTILUS_TREE_MODEL_DISPLAY_NAME_COLUMN,
					     "style", NAUTILUS_TREE_MODEL_FONT_STYLE_COLUMN,
					     NULL);

	gtk_tree_view_append_column (view->details->tree_widget, column);

	gtk_widget_show (GTK_WIDGET (view->details->tree_widget));

	gtk_container_add (GTK_CONTAINER (view->details->scrolled_window),
			   GTK_WIDGET (view->details->tree_widget));

	g_signal_connect_object (gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_widget)), "changed",
				 G_CALLBACK (selection_changed_callback), view, 0);
}

static void
update_filtering_from_preferences (NautilusTreeView *view)
{
	if (view->details->child_model == NULL) {
		return;
	}

	nautilus_tree_model_set_show_hidden_files
		(view->details->child_model,
		 eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES));
	nautilus_tree_model_set_show_backup_files
		(view->details->child_model,
		 eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES));
	nautilus_tree_model_set_show_only_directories
		(view->details->child_model,
		 eel_preferences_get_boolean (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES));
}

static void
tree_activate_callback (BonoboControl *control, gboolean activating, gpointer user_data)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (user_data);

	if (activating && view->details->tree_widget == NULL) {
		load_expansion_state (view);
		create_tree (view);
		update_filtering_from_preferences (view);
	}
}

static void
filtering_changed_callback (gpointer callback_data)
{
	update_filtering_from_preferences (NAUTILUS_TREE_VIEW (callback_data));
}

static void
nautilus_tree_view_instance_init (NautilusTreeView *view)
{
	BonoboControl *control;
	
	view->details = g_new0 (NautilusTreeViewDetails, 1);
	
	view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	view->details->expanded_uris = g_hash_table_new_full
		(g_str_hash, g_str_equal, g_free, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_widget_show (view->details->scrolled_window);

	control = bonobo_control_new (view->details->scrolled_window);
	g_signal_connect_object (control, "activate",
				 G_CALLBACK (tree_activate_callback), view, 0);

	nautilus_view_construct_from_bonobo_control (NAUTILUS_VIEW (view), control);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
				      filtering_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
				      filtering_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
				      filtering_changed_callback, view);
}

static void
nautilus_tree_view_dispose (GObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);
	
	if (view->details->drag_dest) {
		g_object_unref (view->details->drag_dest);
		view->details->drag_dest = NULL;
	}
}

static void
nautilus_tree_view_finalize (GObject *object)
{
	NautilusTreeView *view;

	view = NAUTILUS_TREE_VIEW (object);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					 filtering_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
					 filtering_changed_callback, view);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
					 filtering_changed_callback, view);

	cancel_activation (view);

	g_free (view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_tree_view_class_init (NautilusTreeViewClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_tree_view_dispose;
	G_OBJECT_CLASS (class)->finalize = nautilus_tree_view_finalize;
}
