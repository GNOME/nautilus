/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.

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
*/

#include <config.h>
#include "fm-list-view.h"

#include "fm-list-view-private.h"
#include "fm-properties-window.h"
#include <eel/eel-art-extensions.h>
#include <eel/eel-art-gtk-extensions.h>
#include <eel/eel-dnd.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-list.h>
#include <eel/eel-string.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-metadata.h>

struct FMListViewDetails {
	int sort_column;
	gboolean sort_reversed;

	guint zoom_level;
};

static NautilusFileSortType	default_sort_order_auto_value;
static gboolean			default_sort_reversed_auto_value;

/* 
 * Emblems should never get so small that they're illegible,
 * so we semi-arbitrarily choose a minimum size.
 */
#define LIST_VIEW_MINIMUM_EMBLEM_SIZE	NAUTILUS_ICON_SIZE_SMALLER

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	20


/* We hard-code that first column must contain an icon and the third
 * must contain emblems. The rest can be controlled by the subclass. 
 * Also, many details of these columns are controlled by the subclass; 
 * not too much is hard-coded.
 */
#define LIST_VIEW_COLUMN_NONE		(-1)
#define LIST_VIEW_COLUMN_ICON		0



/* special values for get_data and set_data */
#define PENDING_USER_DATA_KEY		"pending user data"
#define SORT_INDICATOR_KEY		"sort indicator"
#define UP_INDICATOR_VALUE		1
#define DOWN_INDICATOR_VALUE		2

/* forward declarations */
static void                 list_activate_callback                    (EelList       *list,
								       GList              *file_list,
								       gpointer            data);
static void                 list_selection_changed_callback           (EelList       *list,
								       gpointer            data);
static void                 fm_list_view_add_file                     (FMDirectoryView    *view,
								       NautilusFile       *file);
static void                 fm_list_view_remove_file                  (FMDirectoryView    *view,
								       NautilusFile       *file);
static void                 fm_list_view_file_changed                 (FMDirectoryView    *view,
								       NautilusFile       *file);
static void                 fm_list_view_reset_row_height             (FMListView         *list_view);
static void                 fm_list_view_adding_file                  (FMListView         *view,
								       NautilusFile       *file);
static void                 fm_list_view_removing_file                (FMListView         *view,
								       NautilusFile       *file);
static void                 fm_list_view_sort_files                   (FMDirectoryView    *view,
								       GList             **files);
static void                 fm_list_view_begin_file_changes           (FMDirectoryView    *view);
static void                 fm_list_view_begin_loading                (FMDirectoryView    *view);
static void                 fm_list_view_bump_zoom_level              (FMDirectoryView    *view,
								       int                 zoom_increment);
static void                 fm_list_view_zoom_to_level                (FMDirectoryView    *view,
								       int                 zoom_level);
static void                 fm_list_view_restore_default_zoom_level   (FMDirectoryView    *view);
static gboolean             fm_list_view_can_zoom_in                  (FMDirectoryView    *view);
static gboolean             fm_list_view_can_zoom_out                 (FMDirectoryView    *view);
static GtkWidget *          fm_list_view_get_background_widget        (FMDirectoryView    *view);
static void                 fm_list_view_clear                        (FMDirectoryView    *view);
static GList *              fm_list_view_get_selection                (FMDirectoryView    *view);
static NautilusZoomLevel    fm_list_view_get_zoom_level               (FMListView         *list_view);
static void                 fm_list_view_initialize                   (gpointer            object,
								       gpointer            klass);
static void                 fm_list_view_initialize_class             (gpointer            klass);
static void                 fm_list_view_destroy                      (GtkObject          *object);
static void                 fm_list_view_end_file_changes             (FMDirectoryView    *view);
static void                 fm_list_view_reset_to_defaults            (FMDirectoryView    *view);
static void                 fm_list_view_select_all                   (FMDirectoryView    *view);
static void                 fm_list_view_set_selection                (FMDirectoryView    *view,
								       GList              *selection);
static void                 fm_list_view_reveal_selection             (FMDirectoryView    *view);
static GArray *             fm_list_view_get_selected_icon_locations  (FMDirectoryView    *view);
static void                 fm_list_view_set_zoom_level               (FMListView         *list_view,
								       NautilusZoomLevel   new_level,
								       gboolean            always_set_level);
static void                 fm_list_view_sort_items                   (FMListView         *list_view,
								       int                 column,
								       gboolean            reversed);
static void                 fm_list_view_emblems_changed              (FMDirectoryView    *directory_view);
static void                 fm_list_view_update_smooth_graphics_mode  (FMDirectoryView    *directory_view);
static void                 fm_list_view_update_click_mode            (FMDirectoryView    *view);
static void                 fm_list_view_embedded_text_policy_changed (FMDirectoryView    *view);
static void                 fm_list_view_image_display_policy_changed (FMDirectoryView    *view);
static void                 install_row_images                        (FMListView         *list_view,
								       guint               row);
static void                 set_up_list                               (FMListView         *list_view);
static int                  get_column_from_attribute                 (FMListView         *list_view,
								       const char         *attribute);
static int                  get_sort_column_from_attribute            (FMListView         *list_view,
								       const char         *attribute);
static EelList *            get_list                                  (FMListView         *list_view);
static void                 update_icons                              (FMListView         *list_view);
static int                  get_number_of_columns                     (FMListView         *list_view);
static int                  get_emblems_column                        (FMListView         *list_view);
static int                  get_link_column                           (FMListView         *list_view);
static char *               get_default_sort_attribute                (FMListView         *list_view);
static void                 get_column_specification                  (FMListView         *list_view,
								       int                 column_number,
								       FMListViewColumn   *specification);
static const char **        get_column_titles                         (FMListView         *list_view);
static const char *         get_column_attribute                      (FMListView         *list_view,
								       int                 column_number);
static NautilusFileSortType get_column_sort_criterion                 (FMListView         *list_view,
								       int                 column_number);
static void                 real_adding_file                          (FMListView         *view,
								       NautilusFile       *file);
static void                 real_removing_file                        (FMListView         *view,
								       NautilusFile       *file);
static int                  real_get_number_of_columns                (FMListView         *list_view);
static int                  real_get_emblems_column                   (FMListView         *list_view);
static int                  real_get_link_column                      (FMListView         *list_view);
static char *               real_get_default_sort_attribute           (FMListView         *list_view);
static void                 real_get_column_specification             (FMListView         *list_view,
								       int                 column_number,
								       FMListViewColumn   *specification);
static gboolean             real_is_empty                             (FMDirectoryView    *view);
static void                 real_sort_directories_first_changed       (FMDirectoryView    *view);
static void                 real_start_renaming_item                  (FMDirectoryView    *view,
								       const char         *uri);
static void                 font_or_font_size_changed_callback        (gpointer            callback_data);
static void                 default_sort_criteria_changed_callback    (gpointer            callback_data);
static void                 default_zoom_level_changed_callback       (gpointer            callback_data);

EEL_DEFINE_CLASS_BOILERPLATE (FMListView,
				   fm_list_view,
				   FM_TYPE_DIRECTORY_VIEW)

/* GtkObject methods.  */

static void
fm_list_view_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;
	FMListViewClass *fm_list_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);
	fm_list_view_class = FM_LIST_VIEW_CLASS (klass);

	object_class->destroy = fm_list_view_destroy;

	fm_directory_view_class->add_file = fm_list_view_add_file;
	fm_directory_view_class->begin_file_changes = fm_list_view_begin_file_changes;
	fm_directory_view_class->begin_loading = fm_list_view_begin_loading;
	fm_directory_view_class->bump_zoom_level = fm_list_view_bump_zoom_level;
	fm_directory_view_class->zoom_to_level = fm_list_view_zoom_to_level;
	fm_directory_view_class->restore_default_zoom_level = fm_list_view_restore_default_zoom_level;
	fm_directory_view_class->can_zoom_in = fm_list_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_list_view_can_zoom_out;
	fm_directory_view_class->get_background_widget = fm_list_view_get_background_widget;
	fm_directory_view_class->clear = fm_list_view_clear;
	fm_directory_view_class->end_file_changes = fm_list_view_end_file_changes;
	fm_directory_view_class->file_changed = fm_list_view_file_changed;
	fm_directory_view_class->is_empty = real_is_empty;
	fm_directory_view_class->get_selection = fm_list_view_get_selection;
	fm_directory_view_class->reset_to_defaults = fm_list_view_reset_to_defaults;
	fm_directory_view_class->select_all = fm_list_view_select_all;
	fm_directory_view_class->set_selection = fm_list_view_set_selection;
	fm_directory_view_class->reveal_selection = fm_list_view_reveal_selection;
	fm_directory_view_class->sort_files = fm_list_view_sort_files;
	fm_directory_view_class->start_renaming_item = real_start_renaming_item;
	fm_directory_view_class->get_selected_icon_locations = fm_list_view_get_selected_icon_locations;
        fm_directory_view_class->click_policy_changed = fm_list_view_update_click_mode;
        fm_directory_view_class->embedded_text_policy_changed = fm_list_view_embedded_text_policy_changed;
        fm_directory_view_class->image_display_policy_changed = fm_list_view_image_display_policy_changed;
        fm_directory_view_class->smooth_graphics_mode_changed = fm_list_view_update_smooth_graphics_mode;
        fm_directory_view_class->emblems_changed = fm_list_view_emblems_changed;
        fm_directory_view_class->sort_directories_first_changed = real_sort_directories_first_changed;
	fm_directory_view_class->remove_file = fm_list_view_remove_file;

	fm_list_view_class->adding_file = real_adding_file;
	fm_list_view_class->removing_file = real_removing_file;
	fm_list_view_class->get_number_of_columns = real_get_number_of_columns;
	fm_list_view_class->get_emblems_column = real_get_emblems_column;
	fm_list_view_class->get_link_column = real_get_link_column;
	fm_list_view_class->get_column_specification = real_get_column_specification;
	fm_list_view_class->get_default_sort_attribute = real_get_default_sort_attribute;

	eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
					       (int *) &default_sort_order_auto_value);
	eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
					       &default_sort_reversed_auto_value);
}

static void
fm_list_view_initialize (gpointer object, gpointer klass)
{
	FMListView *list_view;
	
	g_return_if_fail (FM_IS_LIST_VIEW (object));
	g_return_if_fail (GTK_BIN (object)->child == NULL);

	list_view = FM_LIST_VIEW (object);

	list_view->details = g_new0 (FMListViewDetails, 1);

	/* These initial values are needed so the state is right when
	 * the metadata is read in later.
	 */
	list_view->details->zoom_level = NAUTILUS_ZOOM_LEVEL_SMALLER;
	list_view->details->sort_column = LIST_VIEW_COLUMN_NONE;
	
	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_icons,
					       GTK_OBJECT (list_view));	

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
						       font_or_font_size_changed_callback, 
						       list_view,
						       GTK_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
						       default_sort_criteria_changed_callback, 
						       list_view,
						       GTK_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
						       default_zoom_level_changed_callback, 
						       list_view,
						       GTK_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
						       default_sort_criteria_changed_callback, 
						       list_view,
						       GTK_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
						       font_or_font_size_changed_callback, 
						       list_view,
						       GTK_OBJECT (list_view));
	
	/* It's important to not create the EelList (with a call
	 * to create_list) until later, when the function pointers
	 * have been initialized by the subclass.
	 */
	/* FIXME bugzilla.gnome.org 42533: 
	 * This code currently relies on there being a call to
	 * get_list before the widget is shown to the user. It would
	 * be better to do something explicit, like connecting to
	 * "realize" instead of just relying on the various get_list
	 * callers.
	 */
}

static void
fm_list_view_destroy (GtkObject *object)
{
	g_free (FM_LIST_VIEW (object)->details);
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void 
column_clicked_callback (EelCList *clist, int column, gpointer user_data)
{
	FMListView *list_view;
	gboolean reversed;

	g_return_if_fail (EEL_IS_LIST (clist));
	g_return_if_fail (FM_IS_LIST_VIEW (user_data));
	g_return_if_fail (get_list (FM_LIST_VIEW (user_data)) == EEL_LIST (clist));

	list_view = FM_LIST_VIEW (user_data);

	if (column == list_view->details->sort_column) {
		reversed = !list_view->details->sort_reversed;
	} else {
		reversed = FALSE;
	}

	fm_list_view_sort_items (list_view, column, reversed);
}

/* EelCompareFunction-style compare function */
static int
list_view_compare_files_for_sort (gconstpointer a, gconstpointer b, gpointer callback_data)
{
	FMListView *list_view;
	NautilusFile *file1;
	NautilusFile *file2;
	int result;

	list_view = FM_LIST_VIEW (callback_data);
	file1 = NAUTILUS_FILE (a);
	file2 = NAUTILUS_FILE (b);

	result = nautilus_file_compare_for_sort (file1, file2,
						 get_column_sort_criterion (list_view, list_view->details->sort_column),
						 fm_directory_view_should_sort_directories_first (FM_DIRECTORY_VIEW (list_view)),
						 list_view->details->sort_reversed);

	/* Flip the sign in the reversed case since list widget will flip it back.
	 * See comment in fm_list_view_sort_items.
	 */
	if (list_view->details->sort_reversed) {
		result = -result;
	}

	return result;
}

/* CList-style compare function */
static int
fm_list_view_compare_rows (EelCList *clist,
			   gconstpointer ptr1,
			   gconstpointer ptr2)
{
	EelCListRow *row1;
	EelCListRow *row2;
	NautilusFile *file1;
	NautilusFile *file2;
	FMListView *list_view;
  
	g_assert (EEL_IS_LIST (clist));
	g_assert (clist->sort_column != LIST_VIEW_COLUMN_NONE);
	g_assert (ptr1 != NULL);
	g_assert (ptr2 != NULL);

	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;

	/* All of our rows have a NautilusFile in the row data. Therefore if
	 * the row data is NULL it must be a row that's being added, and hasn't
	 * had a chance to have its row data set yet. Use our special hack-o-rama
	 * static variable for that case.
	 */
	
	/* Don't do a type check here because these things may be NULL */
	file1 = (NautilusFile *) (row1->data);
	file2 = (NautilusFile *) (row2->data);

	if (file1 == NULL) {
		file1 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	if (file2 == NULL) {
		file2 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	
	list_view = FM_LIST_VIEW (GTK_WIDGET (clist)->parent);

	return list_view_compare_files_for_sort (file1, file2, list_view);
}

/* This is used by type-to-select code. It deliberately ignores the
 * folders-first and reversed-sort settings.
 */
static int
compare_rows_by_name (gconstpointer a, gconstpointer b, void *callback_data)
{
	EelCListRow *row1;
	EelCListRow *row2;

	g_assert (callback_data == NULL);

	row1 = (EelCListRow *) a;
	row2 = (EelCListRow *) b;

	return nautilus_file_compare_for_sort
		(NAUTILUS_FILE (row1->data),
		 NAUTILUS_FILE (row2->data),
		 NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
		 FALSE, FALSE);
}

static int
match_row_name (gconstpointer a, void *callback_data)
{
	EelCListRow *row;
	const char *pattern;
	
	row = (EelCListRow *) a;
	pattern = (const char *) callback_data;

	return nautilus_file_compare_display_name
		(NAUTILUS_FILE (row->data), pattern);
}

static void 
context_click_selection_callback (EelCList *clist, 
				  GdkEventButton *event,
				  FMListView *list_view)
{
	g_assert (EEL_IS_CLIST (clist));
	g_assert (FM_IS_LIST_VIEW (list_view));

	fm_directory_view_pop_up_selection_context_menu 
		(FM_DIRECTORY_VIEW (list_view), event);
}

static void 
context_click_background_callback (EelCList *clist,
				   GdkEventButton *event,
				   FMListView *list_view)
{
	g_assert (FM_IS_LIST_VIEW (list_view));

	fm_directory_view_pop_up_background_context_menu 
		(FM_DIRECTORY_VIEW (list_view), event);
}

static GPtrArray *
make_sorted_row_array (GtkWidget *widget)
{
	GPtrArray *array;

	if (EEL_CLIST (widget)->rows == 0)
		/* empty list, no work */
		return NULL;
		
	/* build an array of rows */
	array = eel_g_ptr_array_new_from_list (EEL_CLIST (widget)->row_list);

	/* sort the array by the names of the NautilusFile objects */
	eel_g_ptr_array_sort (array, compare_rows_by_name, NULL);

	return array;
}

static void
select_row_common (GtkWidget *widget, const GPtrArray *array, guint array_row_index)
{
	EelCListRow *row;
	int list_row_index;

	if (array_row_index >= array->len) {
		g_assert (array->len >= 1);
		array_row_index = array->len - 1;
	}

	row = g_ptr_array_index (array, array_row_index);

	g_assert (row != NULL);

	list_row_index = g_list_index (EEL_CLIST (widget)->row_list, row);
	g_assert (list_row_index >= 0);
	g_assert (list_row_index < EEL_CLIST (widget)->rows);

	/* select the matching row */
	eel_list_select_row (EEL_LIST (widget), list_row_index);
}

static void
select_matching_name_callback (GtkWidget *widget, const char *pattern, FMListView *list_view)
{
	GPtrArray *array;
	int array_row_index;

	g_assert (EEL_IS_LIST (widget));
	g_assert (gtk_object_get_data (GTK_OBJECT (widget), PENDING_USER_DATA_KEY) == NULL);
		
	/* build an array of rows, sorted by name */
	array = make_sorted_row_array (widget);
	if (array == NULL)
		return;

	/* Find the row that matches our search pattern or one after the
	 * closest match if the pattern does not match exactly.
	 */
	array_row_index = eel_g_ptr_array_search (array, match_row_name, 
						       (char *) pattern, FALSE);

	g_assert (array_row_index >= 0);
	select_row_common (widget, array, array_row_index);

	g_ptr_array_free (array, TRUE);
}

static void
select_previous_next_common (GtkWidget *widget, FMListView *list_view, gboolean next)
{
	GPtrArray *array;
	int array_row_index;
	guint index;
	int first_selected_row;
	int last_selected_row;

	g_assert (EEL_IS_LIST (widget));
	g_assert (gtk_object_get_data (GTK_OBJECT (widget), PENDING_USER_DATA_KEY) == NULL);

	/* build an array of rows */
	array = make_sorted_row_array (widget);
	if (array == NULL)
		return;

	/* sort the array by the names of the NautilusFile objects */
	eel_g_ptr_array_sort (array, compare_rows_by_name, NULL);

	/* find the index of the first and the last selected row */
	first_selected_row = -1;
	last_selected_row = -1;
	for (index = 0; index < array->len; index++) {
		if (((EelCListRow *) g_ptr_array_index (array, index))->state == GTK_STATE_SELECTED) {
			if (first_selected_row < 0) {
				first_selected_row = index;
			}
			last_selected_row = index;
		}
	}

	if (first_selected_row == -1 && last_selected_row == -1) {
		/* nothing selected, pick the first or the last */
		if (next) {
			array_row_index = 0;
		} else {
			array_row_index = array->len - 1;
		}
	} else if (first_selected_row != last_selected_row) {
		/* more than one item selected, pick the first or the last selected */
		if (next) {
			array_row_index = last_selected_row;
		} else {
			array_row_index = first_selected_row;
		}
	} else {
		/* one item selected, pick previous/next item */
		if (next) {
			array_row_index = last_selected_row + 1;
		} else {
			array_row_index = first_selected_row - 1;
		}
	}

 	if (array_row_index < 0) {
 		array_row_index = 0;
 	}

 	if (array_row_index >= (int) array->len) {
 		array_row_index = array->len - 1;
 	}

	select_row_common (widget, array, array_row_index);

	g_ptr_array_free (array, TRUE);
}

static void
select_previous_name_callback (GtkWidget *widget, FMListView *list_view)
{
	select_previous_next_common (widget, list_view, TRUE);
}

static void
select_next_name_callback (GtkWidget *widget, FMListView *list_view)
{
	select_previous_next_common (widget, list_view, FALSE);
}

static NautilusFile *
fm_list_nautilus_file_at (EelList *list, int x, int y)
{
	EelCListRow *row;

	row = eel_list_row_at (list, y);
	if (row == NULL) {
		return NULL;
	}
	return NAUTILUS_FILE (row->data);
}

static void
fm_list_receive_dropped_icons (EelList *list, 
			       int x, 
			       int y,
			       int action,
			       GList *drop_data,
			       FMListView *list_view)
{
	FMDirectoryView *directory_view;
	char *target_item_uri;
	NautilusFile *target_item;
	GList *source_uris, *p;
	GArray *dummy;

	target_item_uri = NULL;
	source_uris = NULL;
	directory_view = FM_DIRECTORY_VIEW (list_view);

	if (action == GDK_ACTION_ASK) {
		action = eel_drag_drop_action_ask 
			(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	}

	if (action > 0) {
		/* find the item we hit and figure out if it will take the dropped items */
		target_item = fm_list_nautilus_file_at (list, x, y);
		if (target_item != NULL 
		    && !nautilus_drag_can_accept_items (target_item, drop_data)) {
			target_item = NULL;
		}

		/* figure out the uri of the destination */
		if (target_item == NULL) {
			target_item_uri = fm_directory_view_get_uri (directory_view);
		} else {
			target_item_uri = nautilus_file_get_uri (target_item);
		}

		if (target_item_uri != NULL
			&& (action != GDK_ACTION_MOVE
				/* don't actually move the items if they are in the same directory */
				|| !eel_drag_items_local (target_item_uri, drop_data))) {
			/* build a list of URIs to copy */
			for (p = drop_data; p != NULL; p = p->next) {
				/* do a shallow copy of all the uri strings of the copied files */
				source_uris = g_list_prepend (source_uris, 
							      ((EelDragSelectionItem *)p->data)->uri);
			}
			source_uris = g_list_reverse (source_uris);
			
			/* pass in a 0-item array of icon positions */
			dummy = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
			/* start the copy */
			fm_directory_view_move_copy_items (source_uris, dummy,
							   target_item_uri,
							   action,
							   x, y,
							   directory_view);
		        g_array_free (dummy, TRUE);
		}
	}
	
	g_free (target_item_uri);
}

static void
fm_list_receive_dropped_keyword (EelList *list, char* keyword, int x, int y)
{
	NautilusFile *file;
	
	g_assert (keyword != NULL);

	file = fm_list_nautilus_file_at (list, x, y);

	nautilus_drag_file_receive_dropped_keyword (NAUTILUS_FILE (file), keyword);
}


static gboolean
fm_list_handle_dragged_items (EelList *list,
			      int action,
			      GList *drop_data,
			      int x, int y, guint info,
			      FMListView *list_view)
{
	gboolean ret_val;
	NautilusFile *target_item;

	switch (info) {
	case EEL_ICON_DND_GNOME_ICON_LIST:
	case EEL_ICON_DND_URI_LIST:
		/* Find the item we dragged over and determine if it can accept dropped items */
		target_item = fm_list_nautilus_file_at (list, x, y);
		if (target_item != NULL && nautilus_drag_can_accept_items (target_item, drop_data)) {
			ret_val = TRUE;
		} else {
			ret_val = FALSE;
		}
		
		break;
	case EEL_ICON_DND_KEYWORD:	
		ret_val = TRUE;
		break;
	case EEL_ICON_DND_COLOR:
	case EEL_ICON_DND_BGIMAGE:	
	default:
		ret_val = FALSE;
		break;
	}

	return ret_val;
}


static char *
eel_list_find_icon_list_drop_target (EelList *list,
					  int x, int y,
					  GList *selection_list,
					  FMListView *list_view)
{
	NautilusFile *file;
	char *uri;

	if (eel_list_rejects_dropped_icons (list)) {
		return NULL;
	}

	file = fm_list_nautilus_file_at (EEL_LIST (list), x, y);
	if (file == NULL) {
		uri = fm_directory_view_get_uri (FM_DIRECTORY_VIEW (list_view));
		return uri;
	}

	if ( !nautilus_drag_can_accept_items (file, 
					      selection_list)) {
		uri = fm_directory_view_get_uri (FM_DIRECTORY_VIEW (list_view));
	} else {
		
		uri = nautilus_file_get_uri (NAUTILUS_FILE (file));
	}

	
	return uri;	
}


static void
fm_list_get_default_action (EelList *list,
			    int *default_action,
			    int *non_default_action,
			    GdkDragContext *context,
			    GList *drop_data,
			    int x, int y, guint info,
			    FMListView *list_view)
{
	char *drop_target;

	g_assert (EEL_IS_LIST (list));

	/* FIXME bugzilla.gnome.org 42569: Too much code copied from nautilus-icon-dnd.c. Need to share more. */

	switch (info) {
	case EEL_ICON_DND_GNOME_ICON_LIST:
		if (drop_data == NULL) {
			*default_action = 0;
			*non_default_action = 0;
			return;
		}

		drop_target = eel_list_find_icon_list_drop_target (list, x, y,
									drop_data,
									list_view);
		if (drop_target == NULL) {
			*default_action = 0;
			*non_default_action = 0;
			return;
		}

		eel_drag_default_drop_action_for_icons (context, drop_target, 
							     drop_data, 
							     default_action, non_default_action);
		break;
		
	case EEL_ICON_DND_COLOR:
	case EEL_ICON_DND_BGIMAGE:
		*default_action = context->suggested_action;
		*non_default_action = context->suggested_action;
		break;
		
	case EEL_ICON_DND_KEYWORD:
		*default_action = context->suggested_action;
		*non_default_action = context->suggested_action;
		break;
	}
}


static void
fm_list_handle_dropped_items (EelList *list,
			      int action,
			      GList *drop_data,
			      int x, int y, guint info,
			      FMListView *list_view)
{
	/* FIXME bugzilla.gnome.org 41257:
	 * Merge this with nautilus_icon_container_receive_dropped_icons
	 */ 

	switch (info) {
	case EEL_ICON_DND_GNOME_ICON_LIST:
	case EEL_ICON_DND_URI_LIST:
		fm_list_receive_dropped_icons (list, x, y, 
					       action,
					       drop_data,
					       list_view);
		break;
	case EEL_ICON_DND_COLOR:
		break;
	case EEL_ICON_DND_BGIMAGE:	
		break;
	case EEL_ICON_DND_KEYWORD:	
		fm_list_receive_dropped_keyword (list, (char *)drop_data->data, x, y);
		break;
	default:
		break;
	}


}

/* iteration glue struct */
typedef struct {
	EelDragEachSelectedItemDataGet iteratee;
	EelList *list;
	gpointer iteratee_data;
} RowGetDataBinderContext;

static gboolean
row_get_data_binder (EelCListRow *row, int row_index, gpointer data)
{
	RowGetDataBinderContext *context;
	char *uri;
	ArtIRect icon_rect;
	GdkRectangle cell_rectangle;
	int drag_offset_x, drag_offset_y;

	context = (RowGetDataBinderContext *) data;
	

	uri = nautilus_file_get_uri (NAUTILUS_FILE (row->data));
	if (uri == NULL) {
		g_warning ("no URI for one of the iterated rows");
		return TRUE;
	}

	eel_list_get_cell_rectangle (context->list, row_index, 0, &cell_rectangle);
	
	eel_list_get_initial_drag_offset (context->list, &drag_offset_x, &drag_offset_y);

	/* adjust the icons to be vertically relative to the initial mouse click position */
	icon_rect = eel_art_irect_offset_by (eel_gdk_rectangle_to_art_irect (cell_rectangle),
		0, -drag_offset_y);

	/* horizontally just center the outline rectangles -- this will make the outlines align with
	 * the drag icon
	 */
	icon_rect = eel_art_irect_offset_to (icon_rect,
		- (icon_rect.x1 - icon_rect.x0) / 2, icon_rect.y0);
	
	/* add some extra spacing in between the icon outline rects */
	icon_rect = eel_art_irect_inset (icon_rect, 2, 2);

	/* pass the uri */
	(* context->iteratee) (uri,
		icon_rect.x0,
		icon_rect.y0,
		icon_rect.x1 - icon_rect.x0,
		icon_rect.y1 - icon_rect.y0,
		context->iteratee_data);

	g_free (uri);

	return TRUE;
}

/* Adaptor function used with nautilus_icon_container_each_selected_icon
 * to help iterate over all selected items, passing uris, x,y,w and h
 * values to the iteratee
 */
static void
each_icon_get_data_binder (EelDragEachSelectedItemDataGet iteratee, 
	gpointer iterator_context, gpointer data)
{
	RowGetDataBinderContext context;

	g_assert (EEL_IS_LIST (iterator_context));

	context.iteratee = iteratee;
	context.list = EEL_LIST (iterator_context);
	context.iteratee_data = data;
	eel_list_each_selected_row (EEL_LIST (iterator_context), 
		row_get_data_binder, &context);
}

static void
fm_list_drag_data_get (GtkWidget *widget, GdkDragContext *context,
		       GtkSelectionData *selection_data, guint info, guint time,
		       FMListView *list_view)
{
	g_assert (widget != NULL);
	g_assert (EEL_IS_LIST (widget));
	g_return_if_fail (context != NULL);

	/* Call common function from nautilus-drag that set's up
	 * the selection data in the right format. Pass it means to
	 * iterate all the selected icons.
	 */
	eel_drag_drag_data_get (widget, context, selection_data,
		info, time, widget, each_icon_get_data_binder);
}

static GdkPixbuf *
fm_list_get_drag_pixbuf (GtkWidget *widget, int row_index, FMListView *list_view)
{
	g_assert (FM_IS_LIST_VIEW (list_view));

	return eel_list_get_pixbuf (EEL_LIST (widget),
					 row_index,
					 LIST_VIEW_COLUMN_ICON);
}

static int
fm_list_get_sort_column_index (GtkWidget *widget, FMListView *list_view)
{
	g_assert (EEL_IS_LIST (widget));

	return FM_LIST_VIEW (list_view)->details->sort_column;
}

static void
fm_list_view_update_smooth_graphics_mode (FMDirectoryView *directory_view)
{
	EelList *list;
	gboolean smooth_graphics_mode, old_smooth_graphics_mode;

	g_assert (FM_IS_LIST_VIEW (directory_view));

	list = get_list (FM_LIST_VIEW (directory_view));
	g_assert (list != NULL);

	old_smooth_graphics_mode = eel_list_is_anti_aliased (list);
	smooth_graphics_mode = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);

	if (old_smooth_graphics_mode != smooth_graphics_mode) {
		eel_list_set_anti_aliased_mode (list, smooth_graphics_mode);
		update_icons (FM_LIST_VIEW (directory_view));
	}
}

static void
fm_list_view_emblems_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_LIST_VIEW (directory_view));
	
	update_icons (FM_LIST_VIEW (directory_view));
}


static void
real_sort_directories_first_changed (FMDirectoryView *directory_view)
{
	eel_clist_sort (EEL_CLIST (get_list (FM_LIST_VIEW (directory_view))));
}

static void
create_list (FMListView *list_view)
{
	int number_of_columns;
	const char **titles;
	EelList *list;
	EelCList *clist;
	int i;
	FMListViewColumn column;

	/* FIXME bugzilla.gnome.org 40666:
	 * title setup should allow for columns not being resizable at all,
	 * justification, editable or not, type/format,
	 * not being usable as a sort order criteria, etc.
	 * for now just set up name, min, max and current width
	 */

	number_of_columns = get_number_of_columns (list_view);
	titles = get_column_titles (list_view);
	list = EEL_LIST (eel_list_new_with_titles (get_number_of_columns (list_view),
							     titles));
	eel_list_initialize_dnd (list);
	g_free (titles);

	clist = EEL_CLIST (list);

	EEL_CLIST_SET_FLAG (clist, CLIST_SHOW_TITLES);

	for (i = 0; i < number_of_columns; ++i) {
		get_column_specification (list_view, i, &column);

		/* FIXME bugzilla.gnome.org 42532: Make a cover to do this trick. */
		eel_clist_set_column_max_width (clist, i, column.maximum_width);
		eel_clist_set_column_min_width (clist, i, column.minimum_width);
		/* work around broken EelCList that pins the max_width to be no less than
		 * the min_width instead of bumping min_width down too
		 */
		eel_clist_set_column_max_width (clist, i, column.maximum_width);
		eel_clist_set_column_width (clist, i, column.default_width);
		
		if (column.right_justified) {
			/* Hack around a problem where eel_clist_set_column_justification
			 * crashes if there is a column title but no
			 * column button (it should really be checking if it has a button instead)
			 * this is an easy, dirty fix for now, will get straightened out
			 * with a replacement list view (alternatively, we'd fix this in EelCList)
			 */
			char *saved_title = clist->column[i].title;
			clist->column[i].title = NULL;
			eel_clist_set_column_justification (clist, i, GTK_JUSTIFY_RIGHT);
			clist->column[i].title = saved_title;
		}

	}
	eel_clist_set_auto_sort (clist, TRUE);
	eel_clist_set_compare_func (clist, fm_list_view_compare_rows);

	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (list));
	
	set_up_list (list_view);
}

static void
fm_list_view_update_font (FMListView *list_view)
{
 	/* Note that these deltas aren't exactly the same as those used
 	 * in the icon view, on purpose.
 	 */
	static int font_size_delta_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1] = {
		-4, -2, 0, 0, +2, +6, +6 };
	
	char *font_name;
	int standard_font_size;
	int font_size;

	GdkFont *font;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));

	font_name = eel_preferences_get (NAUTILUS_PREFERENCES_LIST_VIEW_FONT);
	standard_font_size = eel_preferences_get_integer (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE);
	
	font_size = standard_font_size + font_size_delta_table[list_view->details->zoom_level];

	font = nautilus_font_factory_get_font_by_family (font_name, font_size);
	g_assert (font != NULL);
	eel_gtk_widget_set_font (GTK_WIDGET (get_list (list_view)), font);
	gdk_font_unref (font);
	g_free (font_name);
}

static void
font_or_font_size_changed_callback (gpointer callback_data)
{
	g_return_if_fail (FM_IS_LIST_VIEW (callback_data));

	fm_list_view_update_font (FM_LIST_VIEW (callback_data));
}

static int
measure_width_callback (const char *string, void *context)
{
	return gdk_string_width ((GdkFont *)context, string);
}

static char *
truncate_middle_callback (const char *string, int width, void *context)
{
	return eel_string_ellipsize (string, (GdkFont *)context, width, EEL_ELLIPSIZE_MIDDLE);
}

static char *
get_cell_text (GtkWidget *widget, int column_index, int cell_width,
	EelCListRow *row, GdkFont *font, FMListView *list_view)
{
	const char *cell_text;
	EelCList *clist;
	FMListViewColumn specification;
	
	clist = EEL_CLIST (widget);

	get_column_specification (list_view, column_index, &specification);

	if (strcmp (specification.attribute, "date_modified") == 0) {
		/* special handling of dates */
		return nautilus_file_fit_modified_date_as_string (
								  NAUTILUS_FILE (row->data),
								  cell_width,
								  measure_width_callback, truncate_middle_callback,
								  font);
	}

	switch ((EelCellType)row->cell[column_index].type) {
	case EEL_CELL_PIXTEXT:
		cell_text = EEL_CELL_PIXTEXT (row->cell[column_index])->text;
		break;
	case EEL_CELL_TEXT:
	case EEL_CELL_LINK_TEXT:
		cell_text = EEL_CELL_TEXT (row->cell[column_index])->text;
		break;
	default:
		g_assert_not_reached ();
		cell_text = NULL;
		break;
	}
		
	return eel_string_ellipsize (cell_text, font, cell_width, EEL_ELLIPSIZE_MIDDLE);
}


void
set_up_list (FMListView *list_view)
{
	EelList *list;
	
	g_return_if_fail (FM_IS_LIST_VIEW (list_view));
	
	list = get_list (list_view);

	GTK_WIDGET_SET_FLAGS (list, GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (list),
			    "activate",
			    GTK_SIGNAL_FUNC (list_activate_callback),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (list_selection_changed_callback),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "click_column",
			    column_clicked_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "context_click_selection",
			    context_click_selection_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "context_click_background",
			    context_click_background_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "select_matching_name",
			    select_matching_name_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "select_previous_name",
			    select_previous_name_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "select_next_name",
			    select_next_name_callback,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "handle_dragged_items",
			    GTK_SIGNAL_FUNC(fm_list_handle_dragged_items),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "handle_dropped_items",
			    fm_list_handle_dropped_items,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_default_action",
			    fm_list_get_default_action,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "drag_data_get",
			    fm_list_drag_data_get,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_drag_pixbuf",
			    GTK_SIGNAL_FUNC (fm_list_get_drag_pixbuf),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_sort_column_index",
			    GTK_SIGNAL_FUNC (fm_list_get_sort_column_index),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_cell_text",
			    GTK_SIGNAL_FUNC (get_cell_text),
			    list_view);	

	/* Make height tall enough for icons to look good.
	 * This must be done after the list widget is realized, due to
	 * a bug/design flaw in eel_clist_set_row_height. Connecting to
	 * the "realize" signal is slightly too early, so we connect to
	 * "map".
	 */
	gtk_signal_connect (GTK_OBJECT (list_view),
			    "map",
			    fm_list_view_reset_row_height,
			    NULL);

	fm_list_view_update_click_mode (FM_DIRECTORY_VIEW (list_view));
	fm_list_view_update_font (list_view);
	fm_list_view_update_smooth_graphics_mode (FM_DIRECTORY_VIEW (list_view));

	/* Don't even try to accept dropped icons if the view says not to.
	 * This is used only for views in which accepting file-drops is never
	 * allowed, such as the search results view.
	 */
	eel_list_set_rejects_dropped_icons 
		(list, 
		 !fm_directory_view_accepts_dragged_files 
		 	(FM_DIRECTORY_VIEW (list_view)));

	gtk_widget_show (GTK_WIDGET (list));
}

static void
list_activate_callback (EelList *list,
			GList *file_list,
			gpointer data)
{
	g_return_if_fail (EEL_IS_LIST (list));
	g_return_if_fail (FM_IS_LIST_VIEW (data));
	g_return_if_fail (file_list != NULL);

	fm_directory_view_activate_files (FM_DIRECTORY_VIEW (data), file_list);
}

static void
list_selection_changed_callback (EelList *list,
				 gpointer data)
{
	g_return_if_fail (FM_IS_LIST_VIEW (data));
	g_return_if_fail (list == get_list (FM_LIST_VIEW (data)));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (data));
}

static int
add_to_list (FMListView *list_view, NautilusFile *file)
{
	EelList *list;
	EelCList *clist;
	char **text;
	int number_of_columns;
	int new_row;
	int column;
	int emblems_column;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (list_view), -1);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), -1);

	fm_list_view_adding_file (list_view, file);

	/* One extra slot so it's NULL-terminated */
	number_of_columns = get_number_of_columns (list_view);
	text = g_new0 (char *, number_of_columns);
	emblems_column = get_emblems_column (list_view);
	for (column = 0; column < number_of_columns; ++column) {
		/* No text in icon and emblem columns. */
		if (column != LIST_VIEW_COLUMN_ICON
		    && column != emblems_column) {
			text[column] = nautilus_file_get_string_attribute_with_default 
				(file, get_column_attribute (list_view, column));
		}
	}

	list = get_list (list_view);
	clist = EEL_CLIST (list);

	/* Temporarily set user data value as hack for the problem
	 * that compare_rows is called before the row data can be set.
	 */
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, file);
	/* Note that since list is auto-sorted new_row isn't necessarily last row. */
	new_row = eel_clist_append (clist, text);

	eel_clist_set_row_data (clist, new_row, file);
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, NULL);

	/* Mark one column as a link. */
	eel_list_mark_cell_as_link (list, new_row, get_link_column (list_view));

	install_row_images (list_view, new_row);

	/* Free the column text. */
	for (column = 0; column < number_of_columns; ++column) {
		g_free (text[column]);
	}
	g_free (text);

	return new_row;
}

static EelList *
get_list_if_exists (FMListView *list_view)
{
	GtkWidget *child;

	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NULL);

	child = GTK_BIN (list_view)->child;
	if (child == NULL) {
		return NULL;
	}
	
	return EEL_LIST (child);
}

static EelList *
get_list (FMListView *list_view)
{
	GtkWidget *child;

	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NULL);

	child = GTK_BIN (list_view)->child;
	if (child == NULL) {
		create_list (list_view);
		child = GTK_BIN (list_view)->child;
	}
	
	return EEL_LIST (child);
}

static void
fm_list_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
	FMListView *list_view;
	NautilusZoomLevel old_level;
	NautilusZoomLevel new_level;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);
	old_level = fm_list_view_get_zoom_level (list_view);

	if (zoom_increment < 0 && 0 - zoom_increment > (int) old_level) {
		new_level = NAUTILUS_ZOOM_LEVEL_SMALLEST;
	} else {
		new_level = MIN (old_level + zoom_increment,
				 NAUTILUS_ZOOM_LEVEL_LARGEST);
	}

	fm_list_view_set_zoom_level (list_view, new_level, FALSE);
}

static void
fm_list_view_zoom_to_level (FMDirectoryView *view, int zoom_level)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	fm_list_view_set_zoom_level (list_view, zoom_level, FALSE);
}

static NautilusZoomLevel default_zoom_level = NAUTILUS_ZOOM_LEVEL_SMALLER;

static NautilusZoomLevel
get_default_zoom_level (void)
{
	static gboolean auto_storage_added = FALSE;

	if (!auto_storage_added) {
		auto_storage_added = TRUE;
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
						       (int *) &default_zoom_level);
	}

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
fm_list_view_restore_default_zoom_level (FMDirectoryView *view)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	fm_list_view_set_zoom_level (list_view, get_default_zoom_level (), FALSE);
}

static gboolean 
fm_list_view_can_zoom_in (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

	return fm_list_view_get_zoom_level (FM_LIST_VIEW (view))
		< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
fm_list_view_can_zoom_out (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

	return fm_list_view_get_zoom_level (FM_LIST_VIEW (view))
		> NAUTILUS_ZOOM_LEVEL_SMALLEST;
}


static GtkWidget * 
fm_list_view_get_background_widget (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

	return GTK_WIDGET (get_list (FM_LIST_VIEW (view)));
}

static void
fm_list_view_clear (FMDirectoryView *view)
{
	EelList *list;
	EelCList *clist;
	int row;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list = get_list_if_exists (FM_LIST_VIEW (view));
	if (list == NULL) {
		return;
	}

	/* Clear away the existing list items. */
	clist = EEL_CLIST (list);
	for (row = 0; row < clist->rows; ++row) {
		fm_list_view_removing_file
			(FM_LIST_VIEW (view), NAUTILUS_FILE (eel_clist_get_row_data (clist, row)));
	}
	eel_clist_clear (clist);
}

static void
fm_list_view_begin_file_changes (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	eel_clist_freeze (EEL_CLIST (get_list (FM_LIST_VIEW (view))));
}

static void
set_sort_order_from_metadata_and_preferences (FMListView *list_view)
{
	char *default_sort_attribute;
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

	default_sort_attribute = get_default_sort_attribute (list_view);
	fm_list_view_sort_items (
		list_view,
		get_sort_column_from_attribute (list_view,
						nautilus_file_get_metadata (file,
									    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
									    default_sort_attribute)),
		nautilus_file_get_boolean_metadata (file,
						    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
						    default_sort_reversed_auto_value));
	g_free (default_sort_attribute);
}

static void
set_zoom_level_from_metadata_and_preferences (FMListView *list_view)
{
	fm_list_view_set_zoom_level (
		list_view,
		nautilus_file_get_integer_metadata (
			fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view)), 
			NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
			get_default_zoom_level ()),
		TRUE);
}

static void
default_sort_criteria_changed_callback (gpointer callback_data)
{
	g_return_if_fail (FM_IS_LIST_VIEW (callback_data));

	set_sort_order_from_metadata_and_preferences (FM_LIST_VIEW (callback_data));
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	g_return_if_fail (FM_IS_LIST_VIEW (callback_data));

	set_zoom_level_from_metadata_and_preferences (FM_LIST_VIEW (callback_data));
}

static void
fm_list_view_begin_loading (FMDirectoryView *view)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	/* Set up the background color from the metadata. */
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (get_list (list_view)),
						      fm_directory_view_get_directory_as_file (view));

	set_zoom_level_from_metadata_and_preferences (list_view);
	set_sort_order_from_metadata_and_preferences (list_view);
}

static void
fm_list_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	/* We are allowed to get the same icon twice, so don't re-add it. */
	if (eel_clist_find_row_from_data (EEL_CLIST (get_list (FM_LIST_VIEW (view))), file) < 0) {
		add_to_list (FM_LIST_VIEW (view), file);
	}
}

static void
remove_from_list (FMListView *list_view, 
		  NautilusFile *file, 
		  gboolean *was_in_list, 
		  gboolean *was_selected)
{
	EelList *list;
	int old_row;

	g_assert (was_in_list != NULL);
	g_assert (was_selected != NULL);

	list = get_list (list_view);
	old_row = eel_clist_find_row_from_data (EEL_CLIST (list), file);

	/* Sometimes this might be called on files that are no longer in
	 * the list. This happens when a NautilusFile has file_changed called
	 * on it after the file has already been marked as gone (which is legal).
	 * The file was removed from the list the first time, so with the second
	 * file_changed it's not there anymore. Also, note that the search-list-view
	 * subclass relies on this behavior to ignore file_changed calls on the
	 * search-result-symbolic-link files that aren't actually the ones in
	 * the list.
	 */
	if (old_row < 0) {
		*was_in_list = FALSE;
		*was_selected = FALSE;
		return;
	}
	
	*was_in_list = TRUE;
	*was_selected = eel_list_is_row_selected (list, old_row);

	eel_clist_remove (EEL_CLIST (list), old_row);
	fm_list_view_removing_file (list_view, file);
}

static void
real_adding_file (FMListView *view, NautilusFile *file)
{
	nautilus_file_ref (file);
}

static void
fm_list_view_adding_file (FMListView *view, NautilusFile *file)
{
	EEL_CALL_METHOD (FM_LIST_VIEW_CLASS, view,
			 adding_file, (view, file));
}

static void
real_removing_file (FMListView *view, NautilusFile *file)
{
	nautilus_file_unref (file);
}

static void
fm_list_view_removing_file (FMListView *view, NautilusFile *file)
{
	EEL_CALL_METHOD (FM_LIST_VIEW_CLASS, view,
			 removing_file, (view, file));
}

static void
fm_list_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	FMListView *list_view;
	EelCList *clist;
	int new_row;
	gboolean was_in_list;
	gboolean was_selected;

	list_view = FM_LIST_VIEW (view);
	clist = EEL_CLIST (get_list (list_view));

	/* Ref it here so it doesn't go away entirely after we remove it
	 * but before we reinsert it.
	 */
	nautilus_file_ref (file);

	remove_from_list (list_view, file, &was_in_list, &was_selected);
	if (was_in_list) {
		new_row = add_to_list (list_view, file);
		if (was_selected) {
			eel_clist_select_row (clist, new_row, -1);
		}
	}

	/* Unref to match our keep-it-alive-for-this-function ref. */
	nautilus_file_unref (file);
}

static void
fm_list_view_remove_file (FMDirectoryView *view, NautilusFile *file)
{
	gboolean was_in_list;
	gboolean was_selected;

	remove_from_list (FM_LIST_VIEW (view), file, &was_in_list, &was_selected);
}

static void
fm_list_view_end_file_changes (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	eel_clist_thaw (EEL_CLIST (get_list (FM_LIST_VIEW (view))));
}

static guint
fm_list_view_get_icon_size (FMListView *list_view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NAUTILUS_ICON_SIZE_STANDARD);

	return nautilus_get_icon_size_for_zoom_level
		(fm_list_view_get_zoom_level (list_view));
}

static gboolean
real_is_empty (FMDirectoryView *view)
{
	g_assert (FM_IS_LIST_VIEW (view));

	return EEL_CLIST (get_list (FM_LIST_VIEW (view)))->rows == 0;
}

static void
real_start_renaming_item  (FMDirectoryView *view, const char *uri)
{
	NautilusFile *file;

	/* call parent class to make sure the right icon is selected */
	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, start_renaming_item, (view, uri));
	/* Show properties window since we don't do in-place renaming here */
	file = nautilus_file_get (uri);
	fm_properties_window_present (file, view);
	nautilus_file_unref (file);
}

static GList *
fm_list_view_get_selection (FMDirectoryView *view)
{
	GList *list;

	g_return_val_if_fail (FM_IS_LIST_VIEW (view), NULL);

	list = eel_list_get_selection (get_list (FM_LIST_VIEW (view)));
	nautilus_file_list_ref (list);
	return list;
}

static NautilusZoomLevel
fm_list_view_get_zoom_level (FMListView *list_view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view),
			      NAUTILUS_ZOOM_LEVEL_STANDARD);

	return list_view->details->zoom_level;
}

static void
fm_list_view_set_zoom_level (FMListView *list_view,
			     NautilusZoomLevel new_level,
			     gboolean always_set_level)
{
	EelCList *clist;
	int new_width;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (new_level == fm_list_view_get_zoom_level (list_view)) {
		if (always_set_level) {
			fm_directory_view_set_zoom_level (FM_DIRECTORY_VIEW (list_view), new_level);
		}
		return;
	}
	
	list_view->details->zoom_level = new_level;
	nautilus_file_set_integer_metadata
		(fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view)), 
		 NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
		 get_default_zoom_level (),
		 new_level);

	fm_directory_view_set_zoom_level (FM_DIRECTORY_VIEW (list_view), new_level);

	fm_list_view_update_font (list_view);

	clist = EEL_CLIST (get_list (list_view));
	
	eel_clist_freeze (clist);

	fm_list_view_reset_row_height (list_view);

	new_width = fm_list_view_get_icon_size (list_view);
	
	/* This little dance is necessary due to bugs in EelCList.
	 * Must set min, then max, then min, then actual width.
	 */
	/* FIXME bugzilla.gnome.org 42532: Make a cover to do this
	 * trick, or fix EelCList now that we have a copy of the
	 * code here in Nautilus.
	 */
	eel_clist_set_column_min_width (clist, LIST_VIEW_COLUMN_ICON, new_width);
	eel_clist_set_column_max_width (clist, LIST_VIEW_COLUMN_ICON, new_width);
	eel_clist_set_column_min_width (clist, LIST_VIEW_COLUMN_ICON, new_width);
	eel_clist_set_column_width (clist, LIST_VIEW_COLUMN_ICON, new_width);
	
	update_icons (list_view);

	eel_clist_thaw (clist);
}

static void
fm_list_view_reset_row_height (FMListView *list_view)
{
	eel_clist_set_row_height (EEL_CLIST (get_list (list_view)), 
				  MAX (fm_list_view_get_icon_size (list_view),
				       LIST_VIEW_MINIMUM_ROW_HEIGHT));
}

/* Reset sort criteria and zoom level to match defaults */
static void
fm_list_view_reset_to_defaults (FMDirectoryView *view)
{
	FMListView *list_view;
	char *default_sort_attribute;
	
	list_view = FM_LIST_VIEW (view);

	default_sort_attribute = get_default_sort_attribute (list_view);
	fm_list_view_sort_items (list_view, 
				 get_sort_column_from_attribute (list_view, default_sort_attribute), 
				 default_sort_reversed_auto_value);
	g_free (default_sort_attribute);

	fm_list_view_restore_default_zoom_level (view);
}

/* select all of the items in the view */
static void
fm_list_view_select_all (FMDirectoryView *view)
{
	EelCList *clist;
	g_return_if_fail (FM_IS_LIST_VIEW (view));
	
        clist = EEL_CLIST (get_list (FM_LIST_VIEW (view)));
        eel_clist_select_all (clist);
}

/* select items in the view */
static void
fm_list_view_set_selection (FMDirectoryView *view, GList *selection)
{
	EelList *nlist;
	g_return_if_fail (FM_IS_LIST_VIEW (view));
	
        nlist = EEL_LIST (get_list (FM_LIST_VIEW(view)));

        eel_list_set_selection (nlist, selection);
}

static void
fm_list_view_reveal_selection (FMDirectoryView *view)
{
	EelList *nlist;
	GList *selection;

	g_return_if_fail (FM_IS_LIST_VIEW (view));
	
	selection = fm_directory_view_get_selection (view);
	/* Make sure at least one of the selected items is scrolled into view */
        if (selection != NULL) {
	        nlist = EEL_LIST (get_list (FM_LIST_VIEW(view)));
	        eel_list_reveal_row 
	        	(nlist, eel_list_get_first_selected_row (nlist));
        }

        nautilus_file_list_free (selection);
}

static void
fm_list_view_sort_files (FMDirectoryView *view,
			 GList **files)
{
	EelCList *clist;
	NautilusFileSortType sort_criterion;

	clist = EEL_CLIST (get_list (FM_LIST_VIEW (view)));	
	sort_criterion = get_column_sort_criterion (FM_LIST_VIEW (view), clist->sort_column);

	/* Sort the added items before displaying them, so that
	 * they'll be added in order, and items won't move around.
	 */
	*files = eel_g_list_sort_custom (*files, list_view_compare_files_for_sort, view);
}

static GArray *
fm_list_view_get_selected_icon_locations (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), NULL);

	/* icon locations are not stored in the list view,
	 * just return an empty list
	 */
	return g_array_new (FALSE, TRUE, sizeof (GdkPoint));
}

static void
fm_list_view_sort_items (FMListView *list_view, 
			 int column, 
			 gboolean reversed)
{
	EelList *list;
	EelCList *clist;
	NautilusFile *file;
	char *default_sort_attribute;
	
	g_return_if_fail (FM_IS_LIST_VIEW (list_view));
	g_return_if_fail (column >= 0);
	g_return_if_fail (column < EEL_CLIST (get_list (list_view))->columns);

	if (column == list_view->details->sort_column &&
	    reversed == list_view->details->sort_reversed) {
		return;
	}

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
	default_sort_attribute = get_default_sort_attribute (list_view);
	nautilus_file_set_metadata
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
		 default_sort_attribute,
		 get_column_attribute (list_view, column));
	g_free (default_sort_attribute);
	
	nautilus_file_set_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
		 default_sort_reversed_auto_value,
		 reversed);

	list = get_list (list_view);
	clist = EEL_CLIST (list);

	list_view->details->sort_column = column;

	if (reversed != list_view->details->sort_reversed) {
		/* Set DESCENDING or ASCENDING so the sort-order triangle
		 * in the column header draws correctly; we have to play
		 * games with the sort order in list_view_compare_files_for_sort
		 * to make up for this.
		 */
		eel_list_set_sort_type (list, reversed
					      ? GTK_SORT_DESCENDING
					      : GTK_SORT_ASCENDING);
		list_view->details->sort_reversed = reversed;
	}
	
	eel_list_set_sort_column (list, column);
	eel_clist_sort (clist);
}

/**
 * Get the column number given the attribute name associated with the column.
 * Some day the columns might move around, forcing this function to get
 * more complicated.
 * @value: The attribute name associated with this column.
 * 
 * Return value: The column index, or LIST_VIEW_COLUMN_NONE if attribute
 * name does not match any column.
 */
static int
get_column_from_attribute (FMListView *list_view, const char *value)
{
	int number_of_columns, i;

	if (value == NULL) {
		return LIST_VIEW_COLUMN_NONE;
	}

	number_of_columns = get_number_of_columns (list_view);
	for (i = 0; i < number_of_columns; i++) {
		if (strcmp (get_column_attribute (list_view, i), value) == 0) {
			return i;
		}
	}
	
	return LIST_VIEW_COLUMN_NONE;
}

/**
 * Get the column number to use for sorting given an attribute name.
 * This returns the same result as get_column_for_attribute, except
 * that it chooses a default column for unknown attributes.
 * @value: An attribute name, typically one supported by 
 * nautilus_file_get_string_attribute.
 * 
 * Return value: The column index to use for sorting.
 */
static int
get_sort_column_from_attribute (FMListView *list_view,
				const char *value)
{
	int result;
	char *default_sort_attribute;
	
	result = get_column_from_attribute (list_view, value);
	if (result == LIST_VIEW_COLUMN_NONE) {
		default_sort_attribute = get_default_sort_attribute (list_view);
		result = get_column_from_attribute (list_view,
						    default_sort_attribute);
		g_free (default_sort_attribute);
	}
				

	return result;
}

static GList *
fm_list_view_get_emblem_pixbufs_for_file (FMListView *list_view, 
					  NautilusFile *file)
{
	GList *emblem_icons, *emblem_pixbufs, *p;
	GdkPixbuf *emblem_pixbuf;
	int emblem_size;
	EelStringList *emblems_to_ignore;
	gboolean anti_aliased;

	anti_aliased = eel_list_is_anti_aliased (get_list (list_view));

	emblems_to_ignore =  fm_directory_view_get_emblem_names_to_exclude 
		(FM_DIRECTORY_VIEW (list_view));
	emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file, emblems_to_ignore);
	eel_string_list_free (emblems_to_ignore);
	
	emblem_pixbufs = NULL;
	emblem_size = MAX (LIST_VIEW_MINIMUM_EMBLEM_SIZE, 
			   fm_list_view_get_icon_size (list_view));
	for (p = emblem_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data, 
			 emblem_size, emblem_size,
			 emblem_size, emblem_size,
			 anti_aliased,
			 NULL, anti_aliased);
		if (emblem_pixbuf != NULL) {
			emblem_pixbufs = g_list_prepend
				(emblem_pixbufs, emblem_pixbuf);
		}
	}
	emblem_pixbufs = g_list_reverse (emblem_pixbufs);

	nautilus_scalable_icon_list_free (emblem_icons);

	return emblem_pixbufs;
}

/**
 * install_row_images:
 *
 * Put the icon and emblems for a file into the specified row.
 * @list_view: FMDirectoryView in which to install icon.
 * @row: target row index
 * 
 **/
void
install_row_images (FMListView *list_view, guint row)
{
	EelList *list;
	EelCList *clist;
	NautilusFile *file;
	GdkPixbuf *icon;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));

	list = get_list (list_view);
	clist = EEL_CLIST (list);
	file = eel_clist_get_row_data (clist, row);

	g_return_if_fail (file != NULL);

	/* Install the icon for this file. */
	icon = (nautilus_icon_factory_get_pixbuf_for_file
		(file, NULL, fm_list_view_get_icon_size (list_view),
		 eel_list_is_anti_aliased (list)));
	eel_list_set_pixbuf (list, row, LIST_VIEW_COLUMN_ICON, icon);

	/* Install any emblems for this file. */
	eel_list_set_pixbuf_list (list, row, get_emblems_column (list_view), 
				       fm_list_view_get_emblem_pixbufs_for_file (list_view, file));
}

static void 
update_icons (FMListView *list_view)
{
	EelList *list;
	int row;

	list = get_list (list_view);

	for (row = 0; row < EEL_CLIST (list)->rows; ++row) {
		install_row_images (list_view, row);	
	}
}

static void
fm_list_view_update_click_mode (FMDirectoryView *view)
{
	int click_mode;

	g_assert (FM_IS_LIST_VIEW (view));
	g_assert (get_list (FM_LIST_VIEW (view)) != NULL);

	click_mode = eel_preferences_get_integer (NAUTILUS_PREFERENCES_CLICK_POLICY);

	eel_list_set_single_click_mode (get_list (FM_LIST_VIEW (view)), 
		click_mode == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
fm_list_view_embedded_text_policy_changed (FMDirectoryView *view)
{
	update_icons (FM_LIST_VIEW (view));
}

static void
fm_list_view_image_display_policy_changed (FMDirectoryView *view)
{
	update_icons (FM_LIST_VIEW (view));
}

static int
get_number_of_columns (FMListView *list_view)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_LIST_VIEW_CLASS, list_view,
		 get_number_of_columns, (list_view));
}

static int
get_link_column (FMListView *list_view)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_LIST_VIEW_CLASS, list_view,
		 get_link_column, (list_view));
}

static int
get_emblems_column (FMListView *list_view)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_LIST_VIEW_CLASS, list_view,
		 get_emblems_column, (list_view));
}

static char *
get_default_sort_attribute (FMListView *list_view)
{
	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(FM_LIST_VIEW_CLASS, list_view,
		 get_default_sort_attribute, (list_view));
}

static void
get_column_specification (FMListView *list_view,
			  int column_number,
			  FMListViewColumn *specification)
{
	guint icon_size;

	EEL_CALL_METHOD (FM_LIST_VIEW_CLASS, list_view,
			      get_column_specification,
			      (list_view, column_number, specification));
	
	/* We have a special case for the width of the icons
	 * column.
	 */
	if (column_number == LIST_VIEW_COLUMN_ICON) {
		g_assert (specification->minimum_width == 0);
		g_assert (specification->default_width == 0);
		g_assert (specification->maximum_width == 0);

		icon_size = fm_list_view_get_icon_size (list_view);

		specification->minimum_width = icon_size;
		specification->default_width = icon_size;
		specification->maximum_width = icon_size;
	}
}

static const char **
get_column_titles (FMListView *list_view)
{
	int number_of_columns, i;
	const char **titles;
	FMListViewColumn specification;

	number_of_columns = get_number_of_columns (list_view);

	titles = g_new (const char *, number_of_columns);

	for (i = 0; i < number_of_columns; i++) {
		get_column_specification (list_view, i, &specification);
		titles[i] = specification.title;
	}

	return titles;
}

static const char *
get_column_attribute (FMListView *list_view,
		      int column_number)
{
	FMListViewColumn specification;
	
	get_column_specification (list_view, column_number, &specification);
	return specification.attribute;
}

static NautilusFileSortType
get_column_sort_criterion (FMListView *list_view,
			   int column_number)
{
	FMListViewColumn specification;
	
	get_column_specification (list_view, column_number, &specification);
	return specification.sort_criterion;
}

void
fm_list_view_column_set (FMListViewColumn *column,
			 const char *attribute,
			 const char *title,
			 NautilusFileSortType sort_criterion,
			 int minimum_width,
			 int default_width,
			 int maximum_width,
			 gboolean right_justified)
{
	column->attribute = attribute;
	column->title = title;
	column->sort_criterion = sort_criterion;
	column->minimum_width = minimum_width;
	column->default_width = default_width;
	column->maximum_width = maximum_width;
	column->right_justified = right_justified;
}

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

static int
real_get_number_of_columns (FMListView *view)
{
	return 6;
}

static int
real_get_emblems_column (FMListView *view)
{
	return 2;
}

static int
real_get_link_column (FMListView *view)
{
	return 1;
}

static void
real_get_column_specification (FMListView *view,
			       int column_number,
			       FMListViewColumn *specification)
{
	switch (column_number) {
	case 0:
		fm_list_view_column_set (specification,
					 "icon", NULL,
					 NAUTILUS_FILE_SORT_BY_TYPE,
					 0, 0, 0, FALSE);
		break;
	case 1:
		fm_list_view_column_set (specification,
					 "name", _("Name"),
					 NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
					 30, 170, 300, FALSE);
		break;
	case 2:
		fm_list_view_column_set (specification,
					 "emblems", _("Emblems"),
					 NAUTILUS_FILE_SORT_BY_EMBLEMS,
					 20, 52, 300, FALSE);
		break;
	case 3:
		fm_list_view_column_set (specification,
					 "size", _("Size"),
					 NAUTILUS_FILE_SORT_BY_SIZE,
					 20, 55, 80, TRUE);
		break;
	case 4:
		fm_list_view_column_set (specification,
					 "type", _("Type"),
					 NAUTILUS_FILE_SORT_BY_TYPE,
					 20, 135, 200, FALSE);
		break;
	case 5:
		fm_list_view_column_set (specification,
					 "date_modified", _("Date Modified"),
					 NAUTILUS_FILE_SORT_BY_MTIME,
					 30, 100, 200, FALSE);
		break;
	default:
		g_assert_not_reached ();
	}
}
