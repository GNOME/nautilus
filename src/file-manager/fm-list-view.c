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

#include <gtk/gtkhbox.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-list.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>

struct FMListViewDetails
{
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

#define LIST_VIEW_COLUMN_NONE		-1

#define LIST_VIEW_COLUMN_ICON		0
#define LIST_VIEW_COLUMN_EMBLEMS	1
#define LIST_VIEW_COLUMN_NAME		2
#define LIST_VIEW_COLUMN_SIZE		3
#define LIST_VIEW_COLUMN_MIME_TYPE	4
#define LIST_VIEW_COLUMN_DATE_MODIFIED	5
#define LIST_VIEW_COLUMN_COUNT		6

/* special values for get_data and set_data */

#define PENDING_USER_DATA_KEY		"pending user data"
#define SORT_INDICATOR_KEY		"sort indicator"
#define UP_INDICATOR_VALUE		1
#define DOWN_INDICATOR_VALUE		2

/* file attributes associated with columns */

#define LIST_VIEW_ICON_ATTRIBUTE		"icon"
#define LIST_VIEW_NAME_ATTRIBUTE		"name"
#define LIST_VIEW_EMBLEMS_ATTRIBUTE		"emblems"
#define LIST_VIEW_SIZE_ATTRIBUTE		"size"
#define LIST_VIEW_MIME_TYPE_ATTRIBUTE		"type"
#define LIST_VIEW_DATE_MODIFIED_ATTRIBUTE	"date_modified"

#define LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE	LIST_VIEW_NAME_ATTRIBUTE

/* Paths to use when creating & referring to bonobo menu items */
#define MENU_PATH_UNDO "/Edit/Undo"


/* forward declarations */
static int               add_to_list                              (FMListView         *list_view,
								   NautilusFile       *file);
static void              column_clicked_callback                  (GtkCList           *clist,
								   int                 column,
								   gpointer            user_data);
static int               compare_rows                             (GtkCList           *clist,
								   gconstpointer       ptr1,
								   gconstpointer       ptr2);
static void              context_click_selection_callback         (GtkCList           *clist,
								   FMListView         *list_view);
static void              context_click_background_callback        (GtkCList           *clist,
								   FMListView         *list_view);
static void		 create_list                              (FMListView         *list_view);
static void              list_activate_callback                   (NautilusList       *list,
								   GList              *file_list,
								   gpointer            data);
static void              list_selection_changed_callback          (NautilusList       *list,
								   gpointer            data);
static void              fm_list_view_add_file                    (FMDirectoryView    *view,
								   NautilusFile       *file);
static void		 fm_list_view_reset_row_height 		  (FMListView 	      *list_view);
static void              fm_list_view_file_changed                (FMDirectoryView    *view,
								   NautilusFile       *file);
static void              fm_list_view_begin_adding_files          (FMDirectoryView    *view);
static void              fm_list_view_begin_loading               (FMDirectoryView    *view);
static void              fm_list_view_bump_zoom_level             (FMDirectoryView    *view,
								   int                 zoom_increment);
static void              fm_list_view_zoom_to_level               (FMDirectoryView    *view,
								   int                 zoom_level);
static void              fm_list_view_restore_default_zoom_level  (FMDirectoryView    *view);
static gboolean          fm_list_view_can_zoom_in                 (FMDirectoryView    *view);
static gboolean          fm_list_view_can_zoom_out                (FMDirectoryView    *view);
static GtkWidget *       fm_list_view_get_background_widget       (FMDirectoryView    *view);
static void              fm_list_view_clear                       (FMDirectoryView    *view);
static guint             fm_list_view_get_icon_size               (FMListView         *list_view);
static GList *           fm_list_view_get_selection               (FMDirectoryView    *view);
static NautilusZoomLevel fm_list_view_get_zoom_level              (FMListView         *list_view);
static void              fm_list_view_initialize                  (gpointer            object,
								   gpointer            klass);
static void              fm_list_view_initialize_class            (gpointer            klass);
static void              fm_list_view_destroy                     (GtkObject          *object);
static void              fm_list_view_done_adding_files           (FMDirectoryView    *view);
static void              fm_list_view_select_all                  (FMDirectoryView    *view);
static void              fm_list_view_set_selection               (FMDirectoryView    *view, GList *selection);
static void              fm_list_view_set_zoom_level              (FMListView         *list_view,
								   NautilusZoomLevel   new_level,
			    					   gboolean            always_set_level);
static void              fm_list_view_sort_items                  (FMListView         *list_view,
								   int                 column,
								   gboolean            reversed);
const char *            get_attribute_from_column                 (int                 column);
int                     get_column_from_attribute                 (const char         *value);
int                     get_sort_column_from_attribute            (const char         *value);
static NautilusList *   get_list                                  (FMListView         *list_view);
static void             install_row_images                        (FMListView         *list_view,
								   guint               row);
static int              sort_criterion_from_column                (int                 column);
static void             update_icons                              (FMListView         *list_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMListView, fm_list_view, FM_TYPE_DIRECTORY_VIEW);

/* GtkObject methods.  */

static void
fm_list_view_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);
	widget_class = (GtkWidgetClass *) klass;

	
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

	object_class->destroy = fm_list_view_destroy;

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

	create_list (list_view);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_icons,
					       GTK_OBJECT (list_view));	
}

static void
fm_list_view_destroy (GtkObject *object)
{
	g_return_if_fail (FM_IS_LIST_VIEW (object));

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
compare_rows (GtkCList *clist,
	      gconstpointer ptr1,
	      gconstpointer ptr2)
{
	GtkCListRow *row1;
	GtkCListRow *row2;
	NautilusFile *file1;
	NautilusFile *file2;
	int sort_criterion;
  
	g_return_val_if_fail (NAUTILUS_IS_LIST (clist), 0);
	g_return_val_if_fail (clist->sort_column != LIST_VIEW_COLUMN_NONE, 0);

	row1 = (GtkCListRow *) ptr1;
	row2 = (GtkCListRow *) ptr2;

	file1 = (NautilusFile *) row1->data;
	file2 = (NautilusFile *) row2->data;

	/* All of our rows have a NautilusFile in the row data. Therefore if
	 * the row data is NULL it must be a row that's being added, and hasn't
	 * had a chance to have its row data set yet. Use our special hack-o-rama
	 * static variable for that case.
	 */
	g_assert (file1 != NULL || file2 != NULL);
	if (file1 == NULL) {
		file1 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	} else if (file2 == NULL) {
		file2 = gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	g_assert (file1 != NULL && file2 != NULL);
	
	sort_criterion = sort_criterion_from_column (clist->sort_column);

	return nautilus_file_compare_for_sort (file1, file2, sort_criterion);
}

static int
compare_rows_by_name (gconstpointer a, gconstpointer b, void *unused)
{
	GtkCListRow *row1;
	GtkCListRow *row2;

	row1 = (GtkCListRow *) a;
	row2 = (GtkCListRow *) b;

	g_assert ((NautilusFile *)row1->data != NULL);
	g_assert ((NautilusFile *)row2->data != NULL);

	return nautilus_file_compare_for_sort ((NautilusFile *)row1->data, 
		(NautilusFile *)row2->data, NAUTILUS_FILE_SORT_BY_NAME);
}

static int
match_row_name (gconstpointer a, void *context)
{
	GtkCListRow *row;
	const char *pattern;
	
	row = (GtkCListRow *) a;
	pattern = (const char *)context;

	g_assert ((NautilusFile *)row->data != NULL);
	
	return nautilus_file_compare_name ((NautilusFile *)row->data,
		pattern);
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
		(char *)pattern, FALSE);

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
	return (NautilusFile *)row->data;
}

static void
fm_list_handle_dropped_icons (NautilusList *list, GList *drop_data, int x, int y, 
	int action, FMListView *list_view)
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

	context = (RowGetDataBinderContext *)data;

	uri = nautilus_file_get_uri ((NautilusFile *)row->data);
	if (uri == NULL) {
		g_warning ("no URI for one of the iterated rows");
		return TRUE;
	}

	/* pass the uri */
	context->iteratee (uri, 0, 0, 0, 0, context->iteratee_data);

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
create_list (FMListView *list_view)
{
	NautilusList *list;
	GtkCList *clist;

	/* FIXME bugzilla.eazel.com 666:
	 * title setup should allow for columns not being resizable at all,
	 * justification, editable or not, type/format,
	 * not being usable as a sort order criteria, etc.
	 * for now just set up name, min, max and current width
	 */	
	const char * const titles[] = {
		NULL,		/* Icon */
		NULL,		/* Emblems */
		_("Name"),
		_("Size"),
		_("Type"),
		_("Date Modified"),
	};
	
	guint widths[] = {
		 fm_list_view_get_icon_size (list_view),	/* Icon */
		 40,	/* Emblems */
		130,	/* Name */
		 55,	/* Size */
		 95,	/* Type */
		100,	/* Modified */
	};

	guint min_widths[] = {
		 fm_list_view_get_icon_size (list_view),	/* Icon */
		 20,	/* Emblems */
		 30,	/* Name */
		 20,	/* Size */
		 20,	/* Type */
		 30,	/* Modified */
	};

	guint max_widths[] = {
		fm_list_view_get_icon_size (list_view),	/* Icon */
		300,	/* Emblems */
		300,	/* Name */
		 80,	/* Size */
		200,	/* Type */
		200,	/* Modified */
	};

	int i;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));

	list = NAUTILUS_LIST (nautilus_list_new_with_titles (LIST_VIEW_COLUMN_COUNT, titles));
	clist = GTK_CLIST (list);

	for (i = 0; i < LIST_VIEW_COLUMN_COUNT; ++i) {
		gboolean right_justified;

		right_justified = (i == LIST_VIEW_COLUMN_SIZE);

		gtk_clist_set_column_max_width (clist, i, max_widths[i]);
		gtk_clist_set_column_min_width (clist, i, min_widths[i]);
		/* work around broken GtkCList that pins the max_width to be no less than
		 * the min_width instead of bumping min_width down too
		 */
		gtk_clist_set_column_max_width (clist, i, max_widths[i]);
		gtk_clist_set_column_width (clist, i, widths[i]);


		if (right_justified) {
			/* hack around a problem where gtk_clist_set_column_justification
			 * crashes if there is a column title but now
			 * column button (it should really be checking if it has a button instead)
			 * this is an easy, dirty fix for now, will get straightened out
			 * with a replacement list view (alternatively, we'd fix this in GtkCList)
			 */
			char *tmp_title = clist->column[i].title;
			clist->column[i].title = NULL;
			gtk_clist_set_column_justification (clist, i, GTK_JUSTIFY_RIGHT);
			clist->column[i].title = tmp_title;
		}

	}

	gtk_clist_set_auto_sort (clist, TRUE);
	gtk_clist_set_compare_func (clist, compare_rows);
	
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


	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (list));

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
	int new_row;
	int column;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW (list_view), -1);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), -1);

	nautilus_file_ref (file);

	/* One extra slot so it's NULL-terminated */
	text = g_new0 (char *, LIST_VIEW_COLUMN_COUNT+1);

	for (column = 0; column < LIST_VIEW_COLUMN_COUNT; ++column) {
		/* No text in icon column */
		if (column != LIST_VIEW_COLUMN_ICON) {
			text[column] = 
				nautilus_file_get_string_attribute_with_default 
					(file, get_attribute_from_column (column));
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
	nautilus_list_mark_cell_as_link (list, new_row, LIST_VIEW_COLUMN_NAME);
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, NULL);

	install_row_images (list_view, new_row);

	g_strfreev (text);

	return new_row;
}

static NautilusList *
get_list (FMListView *list_view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (list_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_LIST (GTK_BIN (list_view)->child), NULL);

	return NAUTILUS_LIST (GTK_BIN (list_view)->child);
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
		get_sort_column_from_attribute (
			nautilus_directory_get_metadata (
				directory,
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

	g_return_val_if_fail (old_row >= 0, FALSE);
	
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
	int row;
	int new_width;

	g_return_if_fail (FM_IS_LIST_VIEW (list_view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (new_level == fm_list_view_get_zoom_level (list_view)) {
		if (always_set_level) {
			fm_directory_view_set_zoom_level (&list_view->parent, new_level);
		}
		return;
	}
	
	list_view->details->zoom_level = new_level;
	nautilus_directory_set_integer_metadata
		(fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view)), 
		 NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
		 list_view->details->default_zoom_level,
		 new_level);

	fm_directory_view_set_zoom_level (&list_view->parent, new_level);

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
	gtk_clist_set_column_min_width (clist,
					LIST_VIEW_COLUMN_ICON,
					fm_list_view_get_icon_size (list_view));
	gtk_clist_set_column_max_width (clist,
					LIST_VIEW_COLUMN_ICON,
					fm_list_view_get_icon_size (list_view));
	gtk_clist_set_column_min_width (clist,
					LIST_VIEW_COLUMN_ICON,
					fm_list_view_get_icon_size (list_view));
	gtk_clist_set_column_width (clist,
				    LIST_VIEW_COLUMN_ICON,
				    fm_list_view_get_icon_size (list_view));
	
	for (row = 0; row < clist->rows; ++row) {
		install_row_images (list_view, row);
	}

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
	nautilus_directory_set_metadata (
		directory,
		NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
		LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE,
		get_attribute_from_column (column));
	nautilus_directory_set_boolean_metadata (
		directory,
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
 * Get the attribute name associated with the column. These are stored in
 * the metadata and also used to look up the text to display.
 * Note that these are not localized on purpose, so that the metadata files
 * can be shared.
 * @column: The column index.
 * 
 * Return value: The string to be saved in the metadata.
 */
const char *
get_attribute_from_column (int column)
{
	switch (column) {
	case LIST_VIEW_COLUMN_ICON:
		return LIST_VIEW_ICON_ATTRIBUTE;
	case LIST_VIEW_COLUMN_NAME:
		return LIST_VIEW_NAME_ATTRIBUTE;
	case LIST_VIEW_COLUMN_EMBLEMS:
		return LIST_VIEW_EMBLEMS_ATTRIBUTE;
	case LIST_VIEW_COLUMN_SIZE:
		return LIST_VIEW_SIZE_ATTRIBUTE;
	case LIST_VIEW_COLUMN_MIME_TYPE:
		return LIST_VIEW_MIME_TYPE_ATTRIBUTE;
	case LIST_VIEW_COLUMN_DATE_MODIFIED:
		return LIST_VIEW_DATE_MODIFIED_ATTRIBUTE;
	default:
		g_assert_not_reached ();
		return NULL;
	}
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
int
get_column_from_attribute (const char *value)
{
	if (strcmp (LIST_VIEW_ICON_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_ICON;

	if (strcmp (LIST_VIEW_NAME_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_NAME;

	if (strcmp (LIST_VIEW_EMBLEMS_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_EMBLEMS;

	if (strcmp (LIST_VIEW_SIZE_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_SIZE;

	if (strcmp (LIST_VIEW_MIME_TYPE_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_MIME_TYPE;

	if (strcmp (LIST_VIEW_DATE_MODIFIED_ATTRIBUTE, value) == 0)
		return LIST_VIEW_COLUMN_DATE_MODIFIED;

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
int
get_sort_column_from_attribute (const char *value)
{
	int result;

	result = get_column_from_attribute (value);
	if (result == LIST_VIEW_COLUMN_NONE)
		result = get_column_from_attribute (LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE);

	return result;
}

static GList *
fm_list_view_get_emblem_pixbufs_for_file (FMListView *list_view, 
					  NautilusFile *file)
{
	GList *emblem_icons, *emblem_pixbufs, *p;
	GdkPixbuf *emblem_pixbuf;
	int emblem_size;

	emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file);
	emblem_pixbufs = NULL;
	emblem_size = MAX (LIST_VIEW_MINIMUM_EMBLEM_SIZE, 
			   fm_list_view_get_icon_size (list_view));
	for (p = emblem_icons; p != NULL; p = p->next) {
		emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data, 
			 emblem_size, emblem_size,
			 emblem_size, emblem_size);
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
static void
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
		 fm_list_view_get_icon_size (list_view),
		 &pixmap, &bitmap);
	gtk_clist_set_pixmap (clist, row, LIST_VIEW_COLUMN_ICON, pixmap, bitmap);

	/* Install any emblems for this file. */
	nautilus_list_set_pixbuf_list (list, row, LIST_VIEW_COLUMN_EMBLEMS, 
				       fm_list_view_get_emblem_pixbufs_for_file (list_view, file));
}

static int
sort_criterion_from_column (int column)
{
	switch (column)	{
	case LIST_VIEW_COLUMN_ICON:	
		return NAUTILUS_FILE_SORT_BY_TYPE;
	case LIST_VIEW_COLUMN_NAME:
		return NAUTILUS_FILE_SORT_BY_NAME;
	case LIST_VIEW_COLUMN_EMBLEMS:
		return NAUTILUS_FILE_SORT_BY_EMBLEMS;
	case LIST_VIEW_COLUMN_SIZE:
		return NAUTILUS_FILE_SORT_BY_SIZE;
	case LIST_VIEW_COLUMN_DATE_MODIFIED:
		return NAUTILUS_FILE_SORT_BY_MTIME;
	case LIST_VIEW_COLUMN_MIME_TYPE:
		return NAUTILUS_FILE_SORT_BY_TYPE;
	default: 
		return NAUTILUS_FILE_SORT_NONE;
	}
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
