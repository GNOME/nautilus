/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-search-list-view.c - implementation of list view of a virtual directory,
   based on FMListView.

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

   Authors: Rebecca Schulman <rebecka@eazel.com>
*/

/* Perhaps this would be better off as a set of actual special cases rather than a subclass? 
   We haven't changed much, and there is a lot of copied code (duplicate functionality)*/

#include <config.h>
#include <glib.h>
#include <gtk/gtkclist.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libgnome/gnome-i18n.h>

#include <stdio.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-list.h>

#include "fm-list-view.h"
#include "fm-list-view-private.h"
#include "fm-search-list-view.h"

#define PENDING_USER_DATA_KEY		"pending user data"

#define SEARCH_LIST_VIEW_COLUMN_NONE    	-1

#define SEARCH_LIST_VIEW_COLUMN_ICON		0
#define SEARCH_LIST_VIEW_COLUMN_EMBLEMS 	1
#define SEARCH_LIST_VIEW_COLUMN_NAME		3
#define SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH     2
#define SEARCH_LIST_VIEW_COLUMN_SIZE		4
#define SEARCH_LIST_VIEW_COLUMN_MIME_TYPE	5
#define SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED	6
#define SEARCH_LIST_VIEW_COLUMN_COUNT		7

#define SEARCH_LIST_VIEW_ICON_ATTRIBUTE 		"icon"
#define SEARCH_LIST_VIEW_EMBLEMS_ATTRIBUTE		"emblems"
#define SEARCH_LIST_VIEW_ACTUAL_PATH_ATTRIBUTE          "real_directory"
#define SEARCH_LIST_VIEW_NAME_ATTRIBUTE	        	"real_name"
#define SEARCH_LIST_VIEW_SIZE_ATTRIBUTE		        "size"
#define SEARCH_LIST_VIEW_MIME_TYPE_ATTRIBUTE		"type"
#define SEARCH_LIST_VIEW_DATE_MODIFIED_ATTRIBUTE	"date_modified"

#define LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE	LIST_VIEW_NAME_ATTRIBUTE

struct FMSearchListViewDetails {
	
};


static void              fm_search_list_view_initialize                 (gpointer            object,
									 gpointer            klass);
static void              fm_search_list_view_initialize_class           (gpointer            klass);
static void              fm_search_list_view_destroy                    (GtkObject *object);

const char *             fm_search_list_view_get_attribute_from_column        (int column);
static gboolean          fm_search_list_view_column_is_right_justified        (int column);
static int               fm_search_list_view_compare_rows                     (GtkCList *clist,
									       gconstpointer ptr1,
									       gconstpointer ptr2);
static int               fm_search_list_view_get_sort_criterion_from_column   (int column);
NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMSearchListView, fm_search_list_view, FM_TYPE_LIST_VIEW);



static void
fm_search_list_view_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;
	FMListViewClass *fm_list_view_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);
	fm_list_view_class = FM_LIST_VIEW_CLASS (klass);
  
	fm_list_view_class->compare_rows = fm_search_list_view_compare_rows;
	fm_list_view_class->get_attribute_from_column = fm_search_list_view_get_attribute_from_column;
	fm_list_view_class->column_is_right_justified = fm_search_list_view_column_is_right_justified;



	
	object_class->destroy = fm_search_list_view_destroy;
  
}


static void
fm_search_list_view_initialize (gpointer object,
				gpointer klass)
{
	FMListView *list_view;
	int i;


	g_return_if_fail (FM_IS_SEARCH_LIST_VIEW (object));
	g_return_if_fail (GTK_BIN (object)->child == NULL);

	list_view = FM_LIST_VIEW (object);

	/* FIXME:  This would probably be neater with get a set
	   functions, but this will do for now */

	/* These have already been allocated, so free the old values before
	   overwriting new ones */
	for (i = 0; i < list_view->details->number_of_columns; i++) {
		if (list_view->details->column_titles[i] != NULL) {
			g_free (list_view->details->column_titles[i]);
		}
	}
	g_free (list_view->details->column_titles);
	g_free (list_view->details->column_width);
	g_free (list_view->details->maximum_column_width);
	g_free (list_view->details->minimum_column_width);

	list_view->details->number_of_columns = SEARCH_LIST_VIEW_COLUMN_COUNT;
	list_view->details->column_titles = g_new0 (char *, list_view->details->number_of_columns);
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_ICON] = NULL;
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_EMBLEMS] = NULL;
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_NAME] = _("Name");
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH] = _("Directory");
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_SIZE] = _("Size");
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_MIME_TYPE] = 	_("Type");
	list_view->details->column_titles[SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED] = _("Date Modified");

	list_view->details->column_width = g_new0 (int, list_view->details->number_of_columns);
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_ICON] = fm_list_view_get_icon_size (FM_LIST_VIEW (list_view));
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_EMBLEMS] = 40;
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_NAME] = 200;
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH] = 130;
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_SIZE] = 55;
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_MIME_TYPE] = 95;
	list_view->details->column_width[SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED] = 95;

	list_view->details->minimum_column_width = g_new0 (int, list_view->details->number_of_columns);
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_ICON] = fm_list_view_get_icon_size (FM_LIST_VIEW (list_view));
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_EMBLEMS] = 20;
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_NAME] = 30;
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH] = 30;
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_SIZE] = 20;
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_MIME_TYPE] = 20;
	list_view->details->minimum_column_width[SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED] = 30;

	list_view->details->maximum_column_width = g_new0 (int, list_view->details->number_of_columns);
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_ICON] = fm_list_view_get_icon_size (FM_LIST_VIEW (list_view));
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_EMBLEMS] =  300;
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_NAME] = 300;
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH] = 500;
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_SIZE] = 80;
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_MIME_TYPE] = 200;
	list_view->details->maximum_column_width[SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED] = 200;
}





static int
fm_search_list_view_get_sort_criterion_from_column (int column)
{
	switch (column)	{
	case SEARCH_LIST_VIEW_COLUMN_ICON:	
		return NAUTILUS_FILE_SORT_BY_TYPE;
	case SEARCH_LIST_VIEW_COLUMN_NAME:
		return NAUTILUS_FILE_SORT_BY_NAME;
	case SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH:
		return NAUTILUS_FILE_SORT_BY_DIRECTORY;
	case SEARCH_LIST_VIEW_COLUMN_EMBLEMS:
		return NAUTILUS_FILE_SORT_BY_EMBLEMS;
	case SEARCH_LIST_VIEW_COLUMN_SIZE:
		return NAUTILUS_FILE_SORT_BY_SIZE;
	case SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED:
		return NAUTILUS_FILE_SORT_BY_MTIME;
	case SEARCH_LIST_VIEW_COLUMN_MIME_TYPE:
		return NAUTILUS_FILE_SORT_BY_TYPE;
	default: 
		return NAUTILUS_FILE_SORT_NONE;
	}
}

const char *
fm_search_list_view_get_attribute_from_column (int column)
{
	switch (column) {
	case SEARCH_LIST_VIEW_COLUMN_ICON:
		return SEARCH_LIST_VIEW_ICON_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_NAME:
		return SEARCH_LIST_VIEW_NAME_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH:
		return SEARCH_LIST_VIEW_ACTUAL_PATH_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_EMBLEMS:
		return SEARCH_LIST_VIEW_EMBLEMS_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_SIZE:
		return SEARCH_LIST_VIEW_SIZE_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_MIME_TYPE:
		return SEARCH_LIST_VIEW_MIME_TYPE_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_DATE_MODIFIED:
		return SEARCH_LIST_VIEW_DATE_MODIFIED_ATTRIBUTE;
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static int
fm_search_list_view_compare_rows (GtkCList *clist,
				  gconstpointer ptr1,
				  gconstpointer ptr2)
{
	GtkCListRow *row1;
	GtkCListRow *row2;
	NautilusFile *file1;
	NautilusFile *file2;
	int sort_criterion;
  
	g_return_val_if_fail (NAUTILUS_IS_LIST (clist), 0);
	g_return_val_if_fail (clist->sort_column != SEARCH_LIST_VIEW_COLUMN_NONE, 0);

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
	
	sort_criterion = fm_search_list_view_get_sort_criterion_from_column (clist->sort_column);

	return nautilus_file_compare_for_sort (file1, file2, sort_criterion);
}


static gboolean          
fm_search_list_view_column_is_right_justified (int column)
{
	return column == SEARCH_LIST_VIEW_COLUMN_SIZE;
}

static void
fm_search_list_view_destroy (GtkObject *object)
{
	g_return_if_fail (FM_IS_SEARCH_LIST_VIEW (object));

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}
