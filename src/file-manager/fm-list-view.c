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
#include <gtk/gtkhbox.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-list.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>

struct FMListViewDetails {
	int sort_column;
	gboolean sort_reversed;

	guint zoom_level;
	NautilusZoomLevel default_zoom_level;
};

/* 
 * Emblems should never get so small that they're illegible,
 * so we semi-arbitrarily choose a minimum size.
 */
#define LIST_VIEW_MINIMUM_EMBLEM_SIZE	NAUTILUS_ICON_SIZE_STANDARD

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	20

/* We hard-code that first column must contain an icon and the second
 * must contain emblems and the third must contain the name. The rest
 * can be controlled by the subclass. Also, many details of these
 * columns are controlled by the subclass; not too much is hard-coded.
 */
#define LIST_VIEW_COLUMN_NONE		(-1)
#define LIST_VIEW_COLUMN_ICON		0
#define LIST_VIEW_COLUMN_EMBLEMS	1

#define LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE "name"

/* special values for get_data and set_data */
#define PENDING_USER_DATA_KEY		"pending user data"
#define SORT_INDICATOR_KEY		"sort indicator"
#define UP_INDICATOR_VALUE		1
#define DOWN_INDICATOR_VALUE		2

/* forward declarations */
static void                 list_activate_callback                    (NautilusList      *list,
								       GList             *file_list,
								       gpointer           data);
static void                 list_selection_changed_callback           (NautilusList      *list,
								       gpointer           data);
static void                 fm_list_view_add_file                     (FMDirectoryView   *view,
								       NautilusFile      *file);
static void                 fm_list_view_reset_row_height             (FMListView        *list_view);
static void                 fm_list_view_file_changed                 (FMDirectoryView   *view,
								       NautilusFile      *file);
static void                 fm_list_view_begin_adding_files           (FMDirectoryView   *view);
static void                 fm_list_view_begin_loading                (FMDirectoryView   *view);
static void                 fm_list_view_bump_zoom_level              (FMDirectoryView   *view,
								       int                zoom_increment);
static void                 fm_list_view_zoom_to_level                (FMDirectoryView   *view,
								       int                zoom_level);
static void                 fm_list_view_restore_default_zoom_level   (FMDirectoryView   *view);
static gboolean             fm_list_view_can_zoom_in                  (FMDirectoryView   *view);
static gboolean             fm_list_view_can_zoom_out                 (FMDirectoryView   *view);
static GtkWidget *          fm_list_view_get_background_widget        (FMDirectoryView   *view);
static void                 fm_list_view_clear                        (FMDirectoryView   *view);
static GList *              fm_list_view_get_selection                (FMDirectoryView   *view);
static NautilusZoomLevel    fm_list_view_get_zoom_level               (FMListView        *list_view);
static void                 fm_list_view_initialize                   (gpointer           object,
								       gpointer           klass);
static void                 fm_list_view_initialize_class             (gpointer           klass);
static void                 fm_list_view_destroy                      (GtkObject         *object);
static void                 fm_list_view_done_adding_files            (FMDirectoryView   *view);
static void                 fm_list_view_select_all                   (FMDirectoryView   *view);
static void                 fm_list_view_font_family_changed          (FMDirectoryView   *view);
static void                 fm_list_view_set_selection                (FMDirectoryView   *view,
								       GList             *selection);
static void                 fm_list_view_set_zoom_level               (FMListView        *list_view,
								       NautilusZoomLevel  new_level,
								       gboolean           always_set_level);
static void                 fm_list_view_sort_items                   (FMListView        *list_view,
								       int                column,
								       gboolean           reversed);
static void                 fm_list_view_update_click_mode            (FMDirectoryView   *view);
static void                 fm_list_view_embedded_text_policy_changed (FMDirectoryView   *view);
static void                 fm_list_view_image_display_policy_changed (FMDirectoryView   *view);
static void                 install_row_images                        (FMListView        *list_view,
								       guint              row);
static void                 set_up_list                               (FMListView        *list_view);
static int                  get_column_from_attribute                 (FMListView        *list_view,
								       const char        *attribute);
static int                  get_sort_column_from_attribute            (FMListView        *list_view,
								       const char        *attribute);
static NautilusList *       get_list                                  (FMListView        *list_view);
static void                 update_icons                              (FMListView        *list_view);
static int                  get_number_of_columns                     (FMListView        *list_view);
static int                  get_link_column                           (FMListView        *list_view);
static void                 get_column_specification                  (FMListView        *list_view,
								       int                column_number,
								       FMListViewColumn  *specification);
static const char **        get_column_titles                         (FMListView        *list_view);
static const char *         get_column_attribute                      (FMListView        *list_view,
								       int                column_number);
static NautilusFileSortType get_column_sort_criterion                 (FMListView        *list_view,
								       int                column_number);
static int                  real_get_number_of_columns                (FMListView        *list_view);
static int                  real_get_link_column                      (FMListView        *list_view);
static void                 real_get_column_specification             (FMListView        *list_view,
								       int                column_number,
								       FMListViewColumn  *specification);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMListView,
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
	fm_directory_view_class->begin_adding_files = fm_list_view_begin_adding_files;
	fm_directory_view_class->begin_loading = fm_list_view_begin_loading;
	fm_directory_view_class->bump_zoom_level = fm_list_view_bump_zoom_level;
	fm_directory_view_class->zoom_to_level = fm_list_view_zoom_to_level;
	fm_directory_view_class->restore_default_zoom_level = fm_list_view_restore_default_zoom_level;
	fm_directory_view_class->can_zoom_in = fm_list_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_list_view_can_zoom_out;
	fm_directory_view_class->get_background_widget = fm_list_view_get_background_widget;
	fm_directory_view_class->clear = fm_list_view_clear;
	fm_directory_view_class->done_adding_files = fm_list_view_done_adding_files;
	fm_directory_view_class->file_changed = fm_list_view_file_changed;
	fm_directory_view_class->get_selection = fm_list_view_get_selection;
	fm_directory_view_class->select_all = fm_list_view_select_all;
	fm_directory_view_class->set_selection = fm_list_view_set_selection;
        fm_directory_view_class->click_policy_changed = fm_list_view_update_click_mode;
        fm_directory_view_class->embedded_text_policy_changed = fm_list_view_embedded_text_policy_changed;
        fm_directory_view_class->image_display_policy_changed = fm_list_view_image_display_policy_changed;
        fm_directory_view_class->font_family_changed = fm_list_view_font_family_changed;

	fm_list_view_class->get_number_of_columns = real_get_number_of_columns;
	fm_list_view_class->get_link_column = real_get_link_column;
	fm_list_view_class->get_column_specification = real_get_column_specification;
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
	list_view->details->default_zoom_level = NAUTILUS_ZOOM_LEVEL_SMALLER;
	
	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_icons,
					       GTK_OBJECT (list_view));	

	/* It's important to not create the NautilusList (with a call
	 * to create_list) until later, when the function pointers
	 * have been initialized by the subclass.
	 */
	/* FIXME: This code currently relies on there being a call to
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
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void 
column_clicked_callback (GtkCList *clist, int column, gpointer user_data)
{
	FMListView *list_view;
	gboolean reversed;

	g_return_if_fail (NAUTILUS_IS_LIST (clist));
	g_return_if_fail (FM_IS_LIST_VIEW (user_data));
	g_return_if_fail (get_list (FM_LIST_VIEW (user_data)) == NAUTILUS_LIST (clist));

	list_view = FM_LIST_VIEW (user_data);

	if (column == list_view->details->sort_column) {
		reversed = !list_view->details->sort_reversed;
	} else {
		reversed = FALSE;
	}

	fm_list_view_sort_items (list_view, column, reversed);
}

static int
fm_list_view_compare_rows (GtkCList *clist,
			   gconstpointer ptr1,
			   gconstpointer ptr2)
{
	GtkCListRow *row1;
	GtkCListRow *row2;
	NautilusFile *file1;
	NautilusFile *file2;
	NautilusFileSortType sort_criterion;
	FMListView *list_view;
  
	g_return_val_if_fail (NAUTILUS_IS_LIST (clist), 0);
	g_return_val_if_fail (clist->sort_column != LIST_VIEW_COLUMN_NONE, 0);

	row1 = (GtkCListRow *) ptr1;
	row2 = (GtkCListRow *) ptr2;

	/* All of our rows have a NautilusFile in the row data. Therefore if
	 * the row data is NULL it must be a row that's being added, and hasn't
	 * had a chance to have its row data set yet. Use our special hack-o-rama
	 * static variable for that case.
	 */
	
	/* Don't do a type check here because these things may be NULL */
	file1 = (NautilusFile *) (row1->data);
	file2 = (NautilusFile *) (row2->data);

	g_assert (file1 != NULL || file2 != NULL);
	if (file1 == NULL) {
		file1 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	} else if (file2 == NULL) {
		file2 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	g_assert (file1 != NULL && file2 != NULL);
	
	list_view = FM_LIST_VIEW (GTK_WIDGET (clist)->parent);
	sort_criterion = get_column_sort_criterion (list_view, clist->sort_column);
	return nautilus_file_compare_for_sort (file1, file2, sort_criterion);
}

static int
compare_rows_by_name (gconstpointer a, gconstpointer b, void *callback_data)
{
	GtkCListRow *row1;
	GtkCListRow *row2;

	g_assert (callback_data == NULL);

	row1 = (GtkCListRow *) a;
	row2 = (GtkCListRow *) b;

	return nautilus_file_compare_for_sort
		(NAUTILUS_FILE (row1->data),
		 NAUTILUS_FILE (row2->data),
		 NAUTILUS_FILE_SORT_BY_NAME);
}

static int
match_row_name (gconstpointer a, void *callback_data)
{
	GtkCListRow *row;
	const char *pattern;
	
	row = (GtkCListRow *) a;
	pattern = (const char *) callback_data;

	return nautilus_file_compare_name
		(NAUTILUS_FILE (row->data), pattern);
}

static void 
context_click_selection_callback (GtkCList *clist, FMListView *list_view)
{
	g_assert (GTK_IS_CLIST (clist));
	g_assert (FM_IS_LIST_VIEW (list_view));

	fm_directory_view_pop_up_selection_context_menu (FM_DIRECTORY_VIEW (list_view));
}

static void 
context_click_background_callback (GtkCList *clist, FMListView *list_view)
{
	g_assert (FM_IS_LIST_VIEW (list_view));

	fm_directory_view_pop_up_background_context_menu (FM_DIRECTORY_VIEW (list_view));
}

static GPtrArray *
make_sorted_row_array (GtkWidget *widget)
{
	GPtrArray *array;

	if (GTK_CLIST (widget)->rows == 0)
		/* empty list, no work */
		return NULL;
		
	/* build an array of rows */
	array = nautilus_g_ptr_array_new_from_list (GTK_CLIST (widget)->row_list);

	/* sort the array by the names of the NautilusFile objects */
	nautilus_g_ptr_array_sort (array, compare_rows_by_name, NULL);

	return array;
}

static void
select_row_common (GtkWidget *widget, const GPtrArray *array, int array_row_index)
{
	GtkCListRow *row;
	int list_row_index;

	if (array_row_index >= array->len)
		array_row_index = array->len - 1;

	g_assert (array_row_index >= 0);
	row = g_ptr_array_index (array, array_row_index);

	g_assert (row != NULL);

	list_row_index = g_list_index (GTK_CLIST (widget)->row_list, row);
	g_assert (list_row_index >= 0);
	g_assert (list_row_index < GTK_CLIST (widget)->rows);

	/* select the matching row */
	nautilus_list_select_row (NAUTILUS_LIST (widget), list_row_index);
}

static void
select_matching_name_callback (GtkWidget *widget, const char *pattern, FMListView *list_view)
{
	GPtrArray *array;
	int array_row_index;

	g_assert (NAUTILUS_IS_LIST (widget));
	g_assert (gtk_object_get_data (GTK_OBJECT (widget), PENDING_USER_DATA_KEY) == NULL);
		
	/* build an array of rows, sorted by name */
	array = make_sorted_row_array (widget);
	if (array == NULL)
		return;

	/* Find the row that matches our search pattern or one after the
	 * closest match if the pattern does not match exactly.
	 */
	array_row_index = nautilus_g_ptr_array_search (array, match_row_name, 
						       (char *) pattern, FALSE);

	select_row_common (widget, array, array_row_index);

	g_ptr_array_free (array, TRUE);
}

static void
select_previous_next_common (GtkWidget *widget, FMListView *list_view, gboolean next)
{
	GPtrArray *array;
	int array_row_index;
	int index;
	int first_selected_row;
	int last_selected_row;

	g_assert (NAUTILUS_IS_LIST (widget));
	g_assert (gtk_object_get_data (GTK_OBJECT (widget), PENDING_USER_DATA_KEY) == NULL);

	/* build an array of rows */
	array = make_sorted_row_array (widget);
	if (array == NULL)
		return;

	/* sort the array by the names of the NautilusFile objects */
	nautilus_g_ptr_array_sort (array, compare_rows_by_name, NULL);

	/* find the index of the first and the last selected row */
	first_selected_row = -1;
	last_selected_row = -1;
	for (index = 0; index < array->len; index++) {
		if (((GtkCListRow *) g_ptr_array_index (array, index))->state == GTK_STATE_SELECTED) {
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

 	if (array_row_index >= array->len) {
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
fm_list_nautilus_file_at (NautilusList *list, int x, int y)
{
	GtkCListRow *row;

	row = nautilus_list_row_at (list, y);
	if (row == NULL) {
		return NULL;
	}
	return NAUTILUS_FILE (row->data);
}

static void
fm_list_handle_dropped_icons (NautilusList *list,
			      GList *drop_data,
			      int x, int y, 
			      int action,
			      FMListView *list_view)
{
	/* FIXME bugzilla.eazel.com 1257:
	 * Merge this with nautilus_icon_container_receive_dropped_icons
	 */ 
	FMDirectoryView *directory_view;
	char *list_view_uri;
	char *target_item_uri;
	NautilusFile *target_item;
	GList *source_uris, *p;

	target_item_uri = NULL;
	source_uris = NULL;
	directory_view = FM_DIRECTORY_VIEW (list_view);

	if (action == GDK_ACTION_ASK) {
		action = nautilus_drag_drop_action_ask 
			(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	}

	if (action >= 0) {
		/* find the item we hit and figure out if it will take the dropped items */
		target_item = fm_list_nautilus_file_at (list, x, y);
		if (target_item != NULL 
			&& !nautilus_drag_can_accept_items (target_item, drop_data)) {
			target_item = NULL;
		}
		
		list_view_uri = fm_directory_view_get_uri (directory_view);
		if (target_item != NULL 
			|| action != GDK_ACTION_MOVE
			|| !nautilus_drag_items_local (list_view_uri, drop_data)) {

			/* build a list of URIs to copy */
			for (p = drop_data; p != NULL; p = p->next) {
				/* do a shallow copy of all the uri strings of the copied files */
				source_uris = g_list_prepend (source_uris, 
					((DragSelectionItem *)p->data)->uri);
			}
			source_uris = g_list_reverse (source_uris);

			/* figure out the uri of the destination */
			if (target_item != NULL) {
				target_item_uri = nautilus_file_get_uri (target_item);
			} else {
				target_item_uri = g_strdup (list_view_uri);
			}

			/* start the copy */
			fm_directory_view_move_copy_items (source_uris, NULL,
				target_item_uri, action, x, y, directory_view);

		}
	}

	g_free (target_item_uri);
	g_free (list_view_uri);
}

/* iteration glue struct */
typedef struct {
	NautilusDragEachSelectedItemDataGet iteratee;
	gpointer iteratee_data;
} RowGetDataBinderContext;

static gboolean
row_get_data_binder (GtkCListRow * row, gpointer data)
{
	RowGetDataBinderContext *context;
	char *uri;

	context = (RowGetDataBinderContext *) data;

	uri = nautilus_file_get_uri (NAUTILUS_FILE (row->data));
	if (uri == NULL) {
		g_warning ("no URI for one of the iterated rows");
		return TRUE;
	}

	/* pass the uri */
	(* context->iteratee) (uri, 0, 0, 0, 0, context->iteratee_data);

	g_free (uri);

	return TRUE;
}

/* Adaptor function used with nautilus_icon_container_each_selected_icon
 * to help iterate over all selected items, passing uris, x,y,w and h
 * values to the iteratee
 */
static void
each_icon_get_data_binder (NautilusDragEachSelectedItemDataGet iteratee, 
	gpointer iterator_context, gpointer data)
{
	RowGetDataBinderContext context;

	g_assert (NAUTILUS_IS_LIST (iterator_context));

	context.iteratee = iteratee;
	context.iteratee_data = data;
	nautilus_list_each_selected_row (NAUTILUS_LIST (iterator_context), 
		row_get_data_binder, &context);
}

static void
fm_list_drag_data_get (GtkWidget *widget, GdkDragContext *context,
		       GtkSelectionData *selection_data, guint info, guint time,
		       FMListView *list_view)
{
	g_assert (widget != NULL);
	g_assert (NAUTILUS_IS_LIST (widget));
	g_return_if_fail (context != NULL);

	/* Call common function from nautilus-drag that set's up
	 * the selection data in the right format. Pass it means to
	 * iterate all the selected icons.
	 */
	nautilus_drag_drag_data_get (widget, context, selection_data,
		info, time, widget, each_icon_get_data_binder);
}

static void
fm_list_get_drag_pixmap (GtkWidget *widget, int row_index, GdkPixmap **pixmap, 
			 GdkBitmap **mask, FMListView *list_view)
{
	GtkCList *clist;
	GtkCListRow *row;

	g_assert (NAUTILUS_IS_LIST (widget));

	clist = GTK_CLIST (widget);
	row = ROW_ELEMENT (clist, row_index)->data;

	g_assert (row != NULL);

	*pixmap = gdk_pixmap_ref (GTK_CELL_PIXMAP (row->cell[LIST_VIEW_COLUMN_ICON])->pixmap);
	*mask = gdk_bitmap_ref (GTK_CELL_PIXMAP (row->cell[LIST_VIEW_COLUMN_ICON])->mask);
}

static int
fm_list_get_sort_column_index (GtkWidget *widget, FMListView *list_view)
{
	g_assert (NAUTILUS_IS_LIST (widget));
	g_assert (FM_IS_LIST_VIEW (list_view));

	return FM_LIST_VIEW (list_view)->details->sort_column;
}

static void
create_list (FMListView *list_view)
{
	int number_of_columns;
	const char **titles;
	NautilusList *list;
	GtkCList *clist;
	int i;
	FMListViewColumn column;

	/* FIXME bugzilla.eazel.com 666:
	 * title setup should allow for columns not being resizable at all,
	 * justification, editable or not, type/format,
	 * not being usable as a sort order criteria, etc.
	 * for now just set up name, min, max and current width
	 */

	number_of_columns = get_number_of_columns (list_view);
	titles = get_column_titles (list_view);
	list = NAUTILUS_LIST (nautilus_list_new_with_titles (get_number_of_columns (list_view),
							     titles));
	g_free (titles);

	clist = GTK_CLIST (list);

	for (i = 0; i < number_of_columns; ++i) {
		get_column_specification (list_view, i, &column);

		/* FIXME: Make a cover to do this trick. */
		gtk_clist_set_column_max_width (clist, i, column.maximum_width);
		gtk_clist_set_column_min_width (clist, i, column.minimum_width);
		/* work around broken GtkCList that pins the max_width to be no less than
		 * the min_width instead of bumping min_width down too
		 */
		gtk_clist_set_column_max_width (clist, i, column.maximum_width);
		gtk_clist_set_column_width (clist, i, column.default_width);
		
		if (column.right_justified) {
			/* Hack around a problem where gtk_clist_set_column_justification
			 * crashes if there is a column title but no
			 * column button (it should really be checking if it has a button instead)
			 * this is an easy, dirty fix for now, will get straightened out
			 * with a replacement list view (alternatively, we'd fix this in GtkCList)
			 */
			char *saved_title = clist->column[i].title;
			clist->column[i].title = NULL;
			gtk_clist_set_column_justification (clist, i, GTK_JUSTIFY_RIGHT);
			clist->column[i].title = saved_title;
		}

	}
	gtk_clist_set_auto_sort (clist, TRUE);
	gtk_clist_set_compare_func (clist, fm_list_view_compare_rows);

	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (list));
	
	set_up_list (list_view);
}

void
set_up_list (FMListView *list_view)
{
	NautilusList *list;
	
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
			    "handle_dropped_icons",
			    fm_list_handle_dropped_icons,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "drag_data_get",
			    fm_list_drag_data_get,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_drag_pixmap",
			    fm_list_get_drag_pixmap,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "get_sort_column_index",
			    GTK_SIGNAL_FUNC (fm_list_get_sort_column_index),
			    list_view);

	/* Make height tall enough for icons to look good.
	 * This must be done after the list widget is realized, due to
	 * a bug/design flaw in gtk_clist_set_row_height. Connecting to
	 * the "realize" signal is slightly too early, so we connect to
	 * "map".
	 */
	gtk_signal_connect (GTK_OBJECT (list_view),
			    "map",
			    fm_list_view_reset_row_height,
			    NULL);

	fm_list_view_update_click_mode (FM_DIRECTORY_VIEW (list_view));
	fm_list_view_font_family_changed (FM_DIRECTORY_VIEW (list_view));

	gtk_widget_show (GTK_WIDGET (list));
}

static void
list_activate_callback (NautilusList *list,
			GList *file_list,
			gpointer data)
{
	g_return_if_fail (NAUTILUS_IS_LIST (list));
	g_return_if_fail (FM_IS_LIST_VIEW (data));
	g_return_if_fail (file_list != NULL);

	fm_directory_view_activate_files (FM_DIRECTORY_VIEW (data), file_list);
}

static void
list_selection_changed_callback (NautilusList *list,
				 gpointer data)
{
	g_return_if_fail (FM_IS_LIST_VIEW (data));
	g_return_if_fail (list == get_list (FM_LIST_VIEW (data)));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (data));
}

static int
add_to_list (FMListView *list_view, NautilusFile *file)
{
	NautilusList *list;
	GtkCList *clist;
	char **text;
	int number_of_columns;
	int new_row;
	int column;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (list_view), -1);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), -1);

	nautilus_file_ref (file);

	/* One extra slot so it's NULL-terminated */
	number_of_columns = get_number_of_columns (list_view);
	text = g_new0 (char *, number_of_columns + 1);
	for (column = 0; column < number_of_columns; ++column) {
		/* No text in icon and emblem columns. */
		if (column != LIST_VIEW_COLUMN_ICON
		    && column != LIST_VIEW_COLUMN_EMBLEMS) {
			text[column] = nautilus_file_get_string_attribute_with_default 
				(file, get_column_attribute (list_view, column));
		}
	}

	list = get_list (list_view);
	clist = GTK_CLIST (list);

	/* Temporarily set user data value as hack for the problem
	 * that compare_rows is called before the row data can be set.
	 */
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, file);
	/* Note that since list is auto-sorted new_row isn't necessarily last row. */
	new_row = gtk_clist_append (clist, text);

	gtk_clist_set_row_data (clist, new_row, file);
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, NULL);

	/* Mark one column as a link. */
	nautilus_list_mark_cell_as_link (list, new_row, get_link_column (list_view));

	install_row_images (list_view, new_row);

	g_strfreev (text);

	return new_row;
}

static NautilusList *
get_list (FMListView *list_view)
{
	GtkWidget *child;

	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NULL);

	child = GTK_BIN (list_view)->child;
	if (child == NULL) {
		create_list (list_view);
		child = GTK_BIN (list_view)->child;
	}
	
	return NAUTILUS_LIST (child);
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

	if (zoom_increment < 0 && 0 - zoom_increment > old_level) {
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

static void
fm_list_view_restore_default_zoom_level (FMDirectoryView *view)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	/* The list view is using NAUTILUS_ZOOM_LEVEL_SMALLER as default */
	fm_list_view_set_zoom_level (list_view, NAUTILUS_ZOOM_LEVEL_SMALLER, FALSE);
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
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	/* Clear away the existing list items. */
	gtk_clist_clear (GTK_CLIST (get_list (FM_LIST_VIEW (view))));
}

static void
fm_list_view_begin_adding_files (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	gtk_clist_freeze (GTK_CLIST (get_list (FM_LIST_VIEW (view))));
}

static void
fm_list_view_begin_loading (FMDirectoryView *view)
{
	NautilusDirectory *directory;
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	directory = fm_directory_view_get_model (view);
	list_view = FM_LIST_VIEW (view);

	/* Set up the background color from the metadata. */
	nautilus_connect_background_to_directory_metadata (GTK_WIDGET (get_list (list_view)),
							   directory);

	fm_list_view_set_zoom_level (
		list_view,
		nautilus_directory_get_integer_metadata (
			directory, 
			NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
			list_view->details->default_zoom_level),
		TRUE);

	fm_list_view_sort_items (
		list_view,
		get_sort_column_from_attribute (list_view,
						nautilus_directory_get_metadata (directory,
										 NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
										 LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE)),
		nautilus_directory_get_boolean_metadata (
			directory,
			NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
			FALSE));
}

static void
fm_list_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	/* We are allowed to get the same icon twice, so don't re-add it. */
	if (gtk_clist_find_row_from_data (GTK_CLIST (get_list (FM_LIST_VIEW (view))), file) < 0) {
		add_to_list (FM_LIST_VIEW (view), file);
	}
}

static gboolean
remove_from_list (FMListView *list_view, NautilusFile *file)
{
	NautilusList *list;
	int old_row;
	gboolean was_selected;

	list = get_list (list_view);
	old_row = gtk_clist_find_row_from_data (GTK_CLIST (list), file);

	/* Just ignore this.  For some reason this is possible to
	 * happen that multiple file_changed calls will be emitted for
	 * the same file.   Thus this means we'll get called on a file
	 * which no longer exists in the list. */
	if (old_row < 0) {
		return FALSE;
	}
	
	/* Keep this item selected if necessary. */
	was_selected = nautilus_list_is_row_selected (list, old_row);

	/* Remove and re-add file to get new text/icon values and sort correctly. */
	gtk_clist_remove (GTK_CLIST (list), old_row);
	nautilus_file_unref (file);

	return was_selected;
}

static void
fm_list_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	FMListView *list_view;
	GtkCList *clist;
	int new_row;
	gboolean was_selected;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	/* This handles both changes to an existing file and the
	 * existing file going away.
	 */
	list_view = FM_LIST_VIEW (view);
	clist = GTK_CLIST (get_list (list_view));

	/* Ref it here so it doesn't go away entirely after we remove it
	 * but before we reinsert it.
	 */
	nautilus_file_ref (file);
	
	gtk_clist_freeze (clist);

	was_selected = remove_from_list (list_view, file);	

	if (nautilus_directory_contains_file (fm_directory_view_get_model (view), file)) {
		new_row = add_to_list (list_view, file);

		if (was_selected) {
			gtk_clist_select_row (clist, new_row, -1);
		}
	}

	gtk_clist_thaw (clist);

	/* Unref to match our keep-it-alive-for-this-function ref. */
	nautilus_file_unref (file);
}

static void
fm_list_view_done_adding_files (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_LIST_VIEW (view));

	gtk_clist_thaw (GTK_CLIST (get_list (FM_LIST_VIEW (view))));
}

static guint
fm_list_view_get_icon_size (FMListView *list_view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NAUTILUS_ICON_SIZE_STANDARD);

	return nautilus_get_icon_size_for_zoom_level
		(fm_list_view_get_zoom_level (list_view));
}

static GList *
fm_list_view_get_selection (FMDirectoryView *view)
{
	GList *list;

	g_return_val_if_fail (FM_IS_LIST_VIEW (view), NULL);

	list = nautilus_list_get_selection (get_list (FM_LIST_VIEW (view)));
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
	GtkCList *clist;
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
	nautilus_directory_set_integer_metadata
		(fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view)), 
		 NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
		 list_view->details->default_zoom_level,
		 new_level);

	fm_directory_view_set_zoom_level (FM_DIRECTORY_VIEW (list_view), new_level);

	/* Reset default to new level; this way any change in zoom level
	 * will "stick" until the user visits a directory that had its zoom
	 * level set explicitly earlier.
	 */
	list_view->details->default_zoom_level = new_level;	

	clist = GTK_CLIST (get_list (list_view));
	
	gtk_clist_freeze (clist);

	fm_list_view_reset_row_height (list_view);

	new_width = fm_list_view_get_icon_size (list_view);
	
	/* This little dance is necessary due to bugs in GtkCList.
	 * Must set min, then max, then min, then actual width.
	 */
	/* FIXME: Make a cover to do this trick. */
	gtk_clist_set_column_min_width (clist,
					LIST_VIEW_COLUMN_ICON,
					new_width);
	gtk_clist_set_column_max_width (clist,
					LIST_VIEW_COLUMN_ICON,
					new_width);
	gtk_clist_set_column_min_width (clist,
					LIST_VIEW_COLUMN_ICON,
					new_width);
	gtk_clist_set_column_width (clist,
				    LIST_VIEW_COLUMN_ICON,
				    new_width);
	
	update_icons (list_view);

	gtk_clist_thaw (clist);
}

static void
fm_list_view_reset_row_height (FMListView *list_view)
{
	gtk_clist_set_row_height (GTK_CLIST (get_list (list_view)), 
				  MAX (fm_list_view_get_icon_size (list_view),
				       LIST_VIEW_MINIMUM_ROW_HEIGHT));
}

/* select all of the items in the view */
static void
fm_list_view_select_all (FMDirectoryView *view)
{
	GtkCList *clist;
	g_return_if_fail (FM_IS_LIST_VIEW (view));
	
        clist = GTK_CLIST (get_list (FM_LIST_VIEW(view)));
        gtk_clist_select_all (clist);
}

/* select all of the items in the view */
static void
fm_list_view_set_selection (FMDirectoryView *view, GList *selection)
{
	NautilusList *nlist;
	g_return_if_fail (FM_IS_LIST_VIEW (view));
	
        nlist = NAUTILUS_LIST (get_list (FM_LIST_VIEW(view)));

        nautilus_list_set_selection (nlist, selection);
}


static void
fm_list_view_sort_items (FMListView *list_view, 
			 int column, 
			 gboolean reversed)
{
	NautilusList *list;
	GtkCList *clist;
	NautilusDirectory *directory;
	
	g_return_if_fail (FM_IS_LIST_VIEW (list_view));
	g_return_if_fail (column >= 0);
	g_return_if_fail (column < GTK_CLIST (get_list (list_view))->columns);

	if (column == list_view->details->sort_column &&
	    reversed == list_view->details->sort_reversed)
	{
		return;
	}

	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view));
	nautilus_directory_set_metadata
		(directory,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
		 LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE,
		 get_column_attribute (list_view, column));
	nautilus_directory_set_boolean_metadata
		(directory,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
		 FALSE,
		 reversed);

	list = get_list (list_view);
	clist = GTK_CLIST (list);

	list_view->details->sort_column = column;

	if (reversed != list_view->details->sort_reversed)
	{
		gtk_clist_set_sort_type (clist, reversed
					 ? GTK_SORT_DESCENDING
					 : GTK_SORT_ASCENDING);
		list_view->details->sort_reversed = reversed;
	}

	gtk_clist_set_sort_column (clist, column);
	gtk_clist_sort (clist);
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
	
	result = get_column_from_attribute (list_view, value);
	if (result == LIST_VIEW_COLUMN_NONE)
		result = get_column_from_attribute (list_view,
						    LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE);

	return result;
}

static GList *
fm_list_view_get_emblem_pixbufs_for_file (FMListView *list_view, 
					  NautilusFile *file)
{
	GList *emblem_icons, *emblem_pixbufs, *p;
	GdkPixbuf *emblem_pixbuf;
	int emblem_size;

	emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file, FALSE);
	emblem_pixbufs = NULL;
	emblem_size = MAX (LIST_VIEW_MINIMUM_EMBLEM_SIZE, 
			   fm_list_view_get_icon_size (list_view));
	for (p = emblem_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data, 
			 emblem_size, emblem_size,
			 emblem_size, emblem_size,
			 NULL);
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
	NautilusList *list;
	GtkCList *clist;
	NautilusFile *file;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));

	list = get_list (list_view);
	clist = GTK_CLIST (list);
	file = gtk_clist_get_row_data (clist, row);

	g_return_if_fail (file != NULL);

	/* Install the icon for this file. */
	nautilus_icon_factory_get_pixmap_and_mask_for_file
		(file,
		 NULL,
		 fm_list_view_get_icon_size (list_view),
		 &pixmap, &bitmap);
	gtk_clist_set_pixmap (clist, row, LIST_VIEW_COLUMN_ICON, pixmap, bitmap);

	/* Install any emblems for this file. */
	nautilus_list_set_pixbuf_list (list, row, LIST_VIEW_COLUMN_EMBLEMS, 
				       fm_list_view_get_emblem_pixbufs_for_file (list_view, file));
}

static void 
update_icons (FMListView *list_view)
{
	NautilusList *list;
	int row;

	list = get_list (list_view);

	for (row = 0; row < GTK_CLIST (list)->rows; ++row) {
		install_row_images (list_view, row);	
	}
}

static void
fm_list_view_update_click_mode (FMDirectoryView *view)
{
	int click_mode;

	g_assert (FM_IS_LIST_VIEW (view));
	g_assert (get_list (FM_LIST_VIEW (view)) != NULL);

	click_mode = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY,
						    NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_list_set_single_click_mode (get_list (FM_LIST_VIEW (view)), 
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

static void
fm_list_view_font_family_changed (FMDirectoryView *view)
{
	GdkFont *font;

	/* FIXME: need to use the right font size for a given zoom level here */
	font = nautilus_font_factory_get_font_from_preferences (10);
	g_assert (font != NULL);
	nautilus_gtk_widget_set_font (GTK_WIDGET (get_list (FM_LIST_VIEW (view))), font);
	gdk_font_unref (font);
}

static int
get_number_of_columns (FMListView *list_view)
{
	/* For now, we just use the function pointer.
	 * It might be better to use a signal later.
	 */
	return (* FM_LIST_VIEW_CLASS (GTK_OBJECT (list_view)->klass)
		->get_number_of_columns) (list_view);
}

static int
get_link_column (FMListView *list_view)
{
	/* For now, we just use the function pointer.
	 * It might be better to use a signal later.
	 */
	return (* FM_LIST_VIEW_CLASS (GTK_OBJECT (list_view)->klass)
		->get_link_column) (list_view);
}

static void
get_column_specification (FMListView *list_view,
			  int column_number,
			  FMListViewColumn *specification)
{
	guint icon_size;

	/* For now, we just use the function pointer.
	 * It might be better to use a signal later.
	 */
	(* FM_LIST_VIEW_CLASS (GTK_OBJECT (list_view)->klass)
	 ->get_column_specification) (list_view,
				      column_number,
				      specification);

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

static int
real_get_number_of_columns (FMListView *view)
{
	return 6;
}

static int
real_get_link_column (FMListView *view)
{
	return 2;
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
					 "emblems", NULL,
					 NAUTILUS_FILE_SORT_BY_EMBLEMS,
					 20, 40, 300, FALSE);
		break;
	case 2:
		fm_list_view_column_set (specification,
					 "name", _("Name"),
					 NAUTILUS_FILE_SORT_BY_NAME,
					 30, 130, 300, FALSE);
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
					 20, 95, 200, FALSE);
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
