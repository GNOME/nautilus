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
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-tree-view-drag-dest.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-volume-monitor.h>

struct NautilusTreeViewDetails {
	GtkWidget *scrolled_window;
	GtkTreeView *tree_widget;
	GtkTreeModelSort *sort_model;
	NautilusTreeModel *child_model;

	NautilusFile *activation_file;

	NautilusTreeViewDragDest *drag_dest;

	char *selection_location;
	gboolean selecting;
};

typedef struct {
	GList *uris;
	NautilusTreeView *view;
} PrependURIParameters;

BONOBO_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static gboolean
show_iter_for_file (NautilusTreeView *view, NautilusFile *file, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	NautilusFile *parent_file;
	GtkTreeIter parent_iter;
	GtkTreePath *path, *sort_path;
	GtkTreeIter cur_iter;

	if (view->details->child_model == NULL) {
		return FALSE;
	}
	model = GTK_TREE_MODEL (view->details->child_model);

	/* check if file is visible in the same root as the currently selected folder is */
	gtk_tree_view_get_cursor (view->details->tree_widget, &path, NULL);
	if (path != NULL) {
		if (gtk_tree_model_get_iter (model, &cur_iter, path)) {
			if (nautilus_tree_model_file_get_iter (view->details->child_model,
							       iter, file, &cur_iter)) {
				return TRUE;
			}
		}
	}
	/* check if file is visible at all */
	if (nautilus_tree_model_file_get_iter (view->details->child_model,
					       iter, file, NULL)) {
		return TRUE;
	}

	parent_file = nautilus_file_get_parent (file);

	if (parent_file == NULL) {
		return FALSE;
	}
	if (!show_iter_for_file (view, parent_file, &parent_iter)) {
		nautilus_file_unref (parent_file);
		return FALSE;
	}
	nautilus_file_unref (parent_file);

	if (parent_iter.user_data == NULL || parent_iter.stamp == 0) {
		return FALSE;
	}
	path = gtk_tree_model_get_path (model, &parent_iter);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(view->details->sort_model, path);
	gtk_tree_path_free (path);
	gtk_tree_view_expand_row (view->details->tree_widget, sort_path, FALSE);
	gtk_tree_path_free (sort_path);

	return FALSE;
}

static gboolean
show_selection_idle_callback (gpointer callback_data)
{
	NautilusTreeView *view;
	NautilusFile *file, *old_file;
	GtkTreeIter iter;
	GtkTreePath *path, *sort_path;

	view = NAUTILUS_TREE_VIEW (callback_data);

	file = nautilus_file_get (view->details->selection_location);
	if (file == NULL) {
		return FALSE;
	}

	if (!nautilus_file_is_directory (file)) {
		old_file = file;
		file = nautilus_file_get_parent (file);
		nautilus_file_unref (old_file);
		if (file == NULL) {
			return FALSE;
		}
	}
	
	view->details->selecting = TRUE;
	if (!show_iter_for_file (view, file, &iter)) {
		nautilus_file_unref (file);
		return FALSE;
	}
	view->details->selecting = FALSE;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->child_model), &iter);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(view->details->sort_model, path);
	gtk_tree_path_free (path);
	gtk_tree_view_set_cursor (view->details->tree_widget, sort_path, NULL, FALSE);
	gtk_tree_view_scroll_to_cell (view->details->tree_widget, sort_path, NULL, FALSE, 0, 0);
	gtk_tree_path_free (sort_path);

	nautilus_file_unref (file);

	return FALSE;
}

static void
row_loaded_callback (GtkTreeModel     *tree_model,
		     GtkTreeIter      *iter,
		     NautilusTreeView *view)
{
	NautilusFile *file, *tmp_file, *selection_file;

	if (view->details->selection_location == NULL
	    || !view->details->selecting
	    || iter->user_data == NULL || iter->stamp == 0) {
		return;
	}

	file = nautilus_tree_model_iter_get_file (view->details->child_model, iter);
	if (file == NULL) {
		return;
	}
	if (!nautilus_file_is_directory (file)) {
		nautilus_file_unref(file);
		return;
	}

	/* if iter is ancestor of wanted selection_location then update selection */
	selection_file = nautilus_file_get (view->details->selection_location);
	while (selection_file != NULL) {
		if (file == selection_file) {
			nautilus_file_unref (file);
			nautilus_file_unref (selection_file);

			g_idle_add (show_selection_idle_callback, view);
			return;
		}
		tmp_file = nautilus_file_get_parent (selection_file);
		nautilus_file_unref (selection_file);
		selection_file = tmp_file;
	}
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
		if (view->details->selection_location == NULL ||
		    strcmp (uri, view->details->selection_location) != 0) {
			if (view->details->selection_location != NULL) {
				g_free (view->details->selection_location);
			}
			view->details->selection_location = g_strdup (uri);
			nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (view), uri);
		}
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
	NautilusFileAttributes attributes;
	GtkTreeIter iter;

        cancel_activation (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	view->details->activation_file = sort_model_iter_to_file (view, &iter);
	if (view->details->activation_file == NULL) {
		return;
	}
		
	attributes = NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI;
	nautilus_file_call_when_ready (view->details->activation_file, attributes,
				       got_activation_uri_callback, view);
}

static int
compare_rows (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer callback_data)
{
	NautilusFile *file_a, *file_b;
	int result;

	if (a->user_data == NULL) {
		return -1;
	}
	else if (b->user_data == NULL) {
		return -1;
	}

	/* don't sort root nodes */
	if (nautilus_tree_model_iter_is_root (NAUTILUS_TREE_MODEL (model), a)
	    || nautilus_tree_model_iter_is_root (NAUTILUS_TREE_MODEL (model), b)) {
		return 0;
	}

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

	/* Don't allow drops on background */
	return NULL;
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
theme_changed_callback (GObject *icon_factory, gpointer callback_data)
{
        NautilusTreeView *view; 

        view = NAUTILUS_TREE_VIEW (callback_data); 
        nautilus_tree_model_set_theme (NAUTILUS_TREE_MODEL (view->details->child_model));  
}

static void
add_root_for_volume (NautilusTreeView *view,
		     const NautilusVolume *volume)
{
	char *icon, *mount_uri, *name;

	if (nautilus_volume_is_in_removable_blacklist (volume)) {
		return;
	}

	if (!nautilus_volume_is_removable (volume)) {
		return;
	}
	
	/* Name uniqueness is handled by nautilus-desktop-link-monitor.c... */
	
	icon = nautilus_volume_get_icon (volume);
	mount_uri = nautilus_volume_get_target_uri (volume);
	name = nautilus_volume_get_name (volume);
	
	nautilus_tree_model_add_root_uri (view->details->child_model,
					  mount_uri, name, icon);

	g_free (icon);
	g_free (name);
	g_free (mount_uri);
	
}

static void
volume_mounted_callback (NautilusVolumeMonitor *volume_monitor,
			 NautilusVolume *volume,
			 NautilusTreeView *view)
{
	add_root_for_volume (view, volume);
}

static gboolean
add_one_volume_root (const NautilusVolume *volume, gpointer callback_data)
{
	add_root_for_volume (NAUTILUS_TREE_VIEW (callback_data), volume);
}

static void
volume_unmounted_callback (NautilusVolumeMonitor *volume_monitor,
			   NautilusVolume *volume,
			   NautilusTreeView *view)
{
	char *mount_uri;
	
	mount_uri = nautilus_volume_get_target_uri (volume);
	nautilus_tree_model_remove_root_uri (view->details->child_model,
					     mount_uri);
	g_free (mount_uri);
}


static void
create_tree (NautilusTreeView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	NautilusVolumeMonitor *volume_monitor;
	char *home_uri;
	
	view->details->child_model = nautilus_tree_model_new ();
	view->details->sort_model = GTK_TREE_MODEL_SORT
		(gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (view->details->child_model)));
	view->details->tree_widget = GTK_TREE_VIEW
		(gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->details->sort_model)));
	g_object_unref (view->details->sort_model);
	g_signal_connect_object
		(view->details->child_model, "row_loaded",
		 G_CALLBACK (row_loaded_callback),
		 view, G_CONNECT_AFTER);
	home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	nautilus_tree_model_add_root_uri (view->details->child_model, home_uri, _("Home Folder"), "gnome-home");
	g_free (home_uri);
	nautilus_tree_model_add_root_uri (view->details->child_model, "file:///", _("Filesystem"), "gnome-folder");
#ifdef NOT_YET_USABLE
	nautilus_tree_model_add_root_uri (view->details->child_model, "network:///", _("Network Neighbourhood"), "gnome-fs-network");
#endif
	
	volume_monitor = nautilus_volume_monitor_get ();
	nautilus_volume_monitor_each_mounted_volume (volume_monitor,
					     	     add_one_volume_root,
						     view);
	
	g_signal_connect_object (volume_monitor, "volume_mounted",
				 G_CALLBACK (volume_mounted_callback), view, 0);
	g_signal_connect_object (volume_monitor, "volume_unmounted",
				 G_CALLBACK (volume_unmounted_callback), view, 0);
	
	g_object_unref (view->details->child_model);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
						 compare_rows, view, NULL);

	gtk_tree_view_set_headers_visible (view->details->tree_widget, FALSE);

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
					     "weight", NAUTILUS_TREE_MODEL_FONT_WEIGHT_COLUMN,
					     NULL);

	gtk_tree_view_append_column (view->details->tree_widget, column);

	gtk_widget_show (GTK_WIDGET (view->details->tree_widget));

	gtk_container_add (GTK_CONTAINER (view->details->scrolled_window),
			   GTK_WIDGET (view->details->tree_widget));

	g_signal_connect_object (gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_widget)), "changed",
				 G_CALLBACK (selection_changed_callback), view, 0);

	g_idle_add (show_selection_idle_callback, view);
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
load_location_callback (NautilusTreeView *view, char *location)
{
	if (view->details->selection_location != NULL) {
		g_free (view->details->selection_location);
	}
	view->details->selection_location = g_strdup (location);

	g_idle_add (show_selection_idle_callback, view);
}

static void
nautilus_tree_view_instance_init (NautilusTreeView *view)
{
	BonoboControl *control;
	
	view->details = g_new0 (NautilusTreeViewDetails, 1);
	
	view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_widget_show (view->details->scrolled_window);

	control = bonobo_control_new (view->details->scrolled_window);
	g_signal_connect_object (control, "activate",
				 G_CALLBACK (tree_activate_callback), view, 0);

	nautilus_view_construct_from_bonobo_control (NAUTILUS_VIEW (view), control);

	view->details->selection_location = NULL;
	g_signal_connect_object (view, "load_location",
				 G_CALLBACK (load_location_callback), view, 0);
	view->details->selecting = FALSE;

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
				      filtering_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
				      filtering_changed_callback, view);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
				      filtering_changed_callback, view);

	g_signal_connect_object (nautilus_icon_factory_get(), "icons_changed",
				 G_CALLBACK (theme_changed_callback), view, 0);  

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

	if (view->details->selection_location != NULL) {
		g_free (view->details->selection_location);
	}

	g_free (view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_tree_view_class_init (NautilusTreeViewClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_tree_view_dispose;
	G_OBJECT_CLASS (class)->finalize = nautilus_tree_view_finalize;
}
