/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
*/

#include <config.h>
#include "fm-list-view.h"

#include "fm-list-model.h"
#include <eel/eel-cell-renderer-pixbuf-list.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>

struct FMListViewDetails {
	GtkTreeView *tree_view;
	FMListModel *model;
};

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL }
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28

static NautilusFileSortType	default_sort_order_auto_value;
static gboolean			default_sort_reversed_auto_value;

static GList *     fm_list_view_get_selection         (FMDirectoryView *view);

GNOME_CLASS_BOILERPLATE (FMListView, fm_list_view,
			 FMDirectoryView, FM_TYPE_DIRECTORY_VIEW)

static void
list_selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (user_data);

	fm_directory_view_notify_selection_changed (view);
}

static void
list_activate_callback (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	FMDirectoryView *view;
	GList *file_list;
	
	view = FM_DIRECTORY_VIEW (user_data);
	
	file_list = fm_list_view_get_selection (view);
	fm_directory_view_activate_files (view, file_list);
	nautilus_file_list_free (file_list);
}

/* Move these to eel? */

static void
tree_selection_foreach_set_boolean (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    gpointer callback_data)
{
	* (gboolean *) callback_data = TRUE;
}

static gboolean
tree_selection_not_empty (GtkTreeSelection *selection)
{
	gboolean not_empty;

	not_empty = FALSE;
	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_foreach_set_boolean,
					     &not_empty);
	return not_empty;
}

static gboolean
tree_view_has_selection (GtkTreeView *view)
{
	return tree_selection_not_empty (gtk_tree_view_get_selection (view));
}

static void
event_after_callback (GtkWidget *widget, GdkEventAny *event, gpointer callback_data)
{
	/* Put up the right kind of menu if we right click in the tree view. */
	if (event->type == GDK_BUTTON_PRESS
	    && event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))
	    && ((GdkEventButton *) event)->button == 3) {
		if (tree_view_has_selection (GTK_TREE_VIEW (widget))) {
			fm_directory_view_pop_up_selection_context_menu
				(FM_DIRECTORY_VIEW (callback_data), (GdkEventButton *) event);
		} else {
			fm_directory_view_pop_up_background_context_menu
				(FM_DIRECTORY_VIEW (callback_data), (GdkEventButton *) event);
		}
	}
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	/* Deselect if people click outside any row. */
	if (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))
	    && !gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y, NULL, NULL, NULL, NULL)) {
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)));
	}

	/* Let the default code run in any case; it won't reselect anything. */
	return FALSE;
}

static void
create_and_set_up_tree_view (FMListView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	
	view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	g_signal_connect (gtk_tree_view_get_selection (view->details->tree_view),
			  "changed", G_CALLBACK (list_selection_changed_callback), view);

	g_signal_connect (view->details->tree_view, "row_activated",
			  G_CALLBACK (list_activate_callback), view);
	g_signal_connect (view->details->tree_view, "event-after",
			  G_CALLBACK (event_after_callback), view);
	g_signal_connect (view->details->tree_view, "button_press_event",
			  G_CALLBACK (button_press_callback), view);
	
	view->details->model = g_object_new (FM_TYPE_LIST_MODEL, NULL);
	gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
	g_object_unref (view->details->model);
	
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (view->details->tree_view, TRUE);

	gtk_tree_view_enable_model_drag_source (view->details->tree_view, 0, drag_types, G_N_ELEMENTS (drag_types),
						GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	
	/* Create the file name column */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_renderer_set_fixed_size (cell, -1, LIST_VIEW_MINIMUM_ROW_HEIGHT);
	
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_sort_column_id (column, FM_LIST_MODEL_NAME_COLUMN);
	gtk_tree_view_column_set_title (column, _("File name"));
	
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "pixbuf", FM_LIST_MODEL_ICON_COLUMN,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", FM_LIST_MODEL_NAME_COLUMN,
					     NULL);
	gtk_tree_view_append_column (view->details->tree_view, column);

	/* Create the size column */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (cell),
		      "xalign", 1.0,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Size"),
							   cell,
							   "text", FM_LIST_MODEL_SIZE_COLUMN,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, FM_LIST_MODEL_SIZE_COLUMN);
	gtk_tree_view_append_column (view->details->tree_view, column);

	/* Create the type column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"),
							   cell,
							   "text", FM_LIST_MODEL_TYPE_COLUMN,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, FM_LIST_MODEL_TYPE_COLUMN);	
	gtk_tree_view_append_column (view->details->tree_view, column);

	/* Create the date modified column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Date Modified"),
							   cell,
							   "text", FM_LIST_MODEL_DATE_MODIFIED_COLUMN,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, FM_LIST_MODEL_DATE_MODIFIED_COLUMN);
	gtk_tree_view_append_column (view->details->tree_view, column);

	gtk_widget_show (GTK_WIDGET (view->details->tree_view));
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->tree_view));
}

static void
fm_list_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	fm_list_model_add_file (FM_LIST_VIEW (view)->details->model, file);
}

static void
set_sort_order_from_metadata_and_preferences (FMListView *list_view)
{
	char *sort_attribute;
	int sort_column_id;
	NautilusFile *file;
	gboolean sort_reversed;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
	sort_attribute = nautilus_file_get_metadata (file,
						     NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
						     NULL);
	sort_column_id = fm_list_model_get_sort_column_id_from_attribute (sort_attribute);
	g_free (sort_attribute);

	if (sort_column_id == -1) {
		sort_column_id = fm_list_model_get_sort_column_id_from_sort_type (default_sort_order_auto_value);
	}

	sort_reversed = nautilus_file_get_boolean_metadata (file,
							    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
							    default_sort_reversed_auto_value);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
					      sort_column_id,
					      sort_reversed ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
}

static void
fm_list_view_begin_loading (FMDirectoryView *view)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	nautilus_connect_background_to_file_metadata (GTK_WIDGET (list_view->details->tree_view),
						      fm_directory_view_get_directory_as_file (view));

	set_sort_order_from_metadata_and_preferences (list_view);
}

static void
fm_list_view_clear (FMDirectoryView *view)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);
	
	fm_list_model_clear (list_view->details->model);
}

static void
fm_list_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	fm_list_model_file_changed (FM_LIST_VIEW (view)->details->model, file);
}

static GtkWidget *
fm_list_view_get_background_widget (FMDirectoryView *view)
{
	return GTK_WIDGET (view);
}

static void
fm_list_view_get_selection_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **list;
	NautilusFile *file;
	
	list = data;

	gtk_tree_model_get (model, iter,
			    FM_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	nautilus_file_ref (file);
	(* list) = g_list_prepend ((* list), file);
}

static GList *
fm_list_view_get_selection (FMDirectoryView *view)
{
	GList *list;

	list = NULL;

	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view),
					     fm_list_view_get_selection_foreach_func, &list);

	return list;
}

static gboolean
fm_list_view_is_empty (FMDirectoryView *view)
{
	return fm_list_model_is_empty (FM_LIST_VIEW (view)->details->model);
}

static void
fm_list_view_remove_file (FMDirectoryView *view, NautilusFile *file)
{
	fm_list_model_remove_file (FM_LIST_VIEW (view)->details->model, file);
}

static void
fm_list_view_set_selection (FMDirectoryView *view, GList *selection)
{
	FMListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GtkTreeIter iter;
	NautilusFile *file;
	
	list_view = FM_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	gtk_tree_selection_unselect_all (tree_selection);
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		if (fm_list_model_get_tree_iter_from_file (list_view->details->model, file, &iter)) {
			gtk_tree_selection_select_iter (tree_selection, &iter);
		}
	}
}

static void
fm_list_view_select_all (FMDirectoryView *view)
{
	gtk_tree_selection_select_all (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view));
}

/* Reset sort criteria and zoom level to match defaults */
static void
fm_list_view_reset_to_defaults (FMDirectoryView *view)
{
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file (view);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN, NULL, NULL);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED, NULL, NULL);

	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (FM_LIST_VIEW (view)->details->model),
		 fm_list_model_get_sort_column_id_from_sort_type (default_sort_order_auto_value),
		 default_sort_reversed_auto_value ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
        fm_directory_view_restore_default_zoom_level (view);
}

#if GNOME2_CONVERSION_COMPLETE

/* This is needed when writing out the sort to metadata. But we don't
 * need this list here. We can probably do this using the table inside
 * FMListModel.
 */

static char *
get_attribute_from_sort_type (NautilusFileSortType sort_type)
{
	switch (sort_type) {
	case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
		return g_strdup ("name");
	case NAUTILUS_FILE_SORT_BY_SIZE:
		return g_strdup ("size");
	case NAUTILUS_FILE_SORT_BY_TYPE:
		return g_strdup ("type");
	case NAUTILUS_FILE_SORT_BY_MTIME:
		return g_strdup ("date_modified");
	case NAUTILUS_FILE_SORT_BY_EMBLEMS:
		return g_strdup ("emblems");
	default:
		g_warning ("unknown sort type %d in get_attribute_from_sort_type", sort_type);
		return g_strdup ("name");
	}
}

static char *
real_get_default_sort_attribute (FMListView *view)
{
	return get_attribute_from_sort_type (default_sort_order_auto_value);
}

#endif

static void
fm_list_view_sort_directories_first_changed (FMDirectoryView *view)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	fm_list_model_set_should_sort_directories_first (list_view->details->model,
							 fm_directory_view_should_sort_directories_first (view));
}

static void
fm_list_view_finalize (GObject *object)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (object);

	g_free (list_view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
fm_list_view_emblems_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_LIST_VIEW (directory_view));

#if GNOME2_CONVERSION_COMPLETE
	/* FIXME: This needs to update the emblems of the icons, since
	 * relative emblems may have changed.
	 */
#endif
}


static void
fm_list_view_class_init (FMListViewClass *class)
{
	FMDirectoryViewClass *fm_directory_view_class;

	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->finalize = fm_list_view_finalize;

	fm_directory_view_class->add_file = fm_list_view_add_file;
	fm_directory_view_class->begin_loading = fm_list_view_begin_loading;
#if GNOME2_CONVERSION_COMPLETE
	fm_directory_view_class->bump_zoom_level = xxx;
	fm_directory_view_class->can_zoom_in = xxx;
	fm_directory_view_class->can_zoom_out = xxx;
#endif
	fm_directory_view_class->clear = fm_list_view_clear;
	fm_directory_view_class->file_changed = fm_list_view_file_changed;
	fm_directory_view_class->get_background_widget = fm_list_view_get_background_widget;
	fm_directory_view_class->get_selection = fm_list_view_get_selection;
	fm_directory_view_class->is_empty = fm_list_view_is_empty;
	fm_directory_view_class->reset_to_defaults = fm_list_view_reset_to_defaults;
#if GNOME2_CONVERSION_COMPLETE
	fm_directory_view_class->restore_default_zoom_level = xxx;
#endif
	fm_directory_view_class->remove_file = fm_list_view_remove_file;
	fm_directory_view_class->select_all = fm_list_view_select_all;
	fm_directory_view_class->set_selection = fm_list_view_set_selection;
        fm_directory_view_class->emblems_changed = fm_list_view_emblems_changed;
	fm_directory_view_class->sort_directories_first_changed = fm_list_view_sort_directories_first_changed;
	
	eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
				       (int *) &default_sort_order_auto_value);
	eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
					  &default_sort_reversed_auto_value);
}

static void
fm_list_view_instance_init (FMListView *list_view)
{
	list_view->details = g_new0 (FMListViewDetails, 1);

	create_and_set_up_tree_view (list_view);

	fm_list_view_sort_directories_first_changed (FM_DIRECTORY_VIEW (list_view));
}
