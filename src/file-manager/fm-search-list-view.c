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

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-list.h>

#include "fm-list-view.h"
#include "fm-list-view-private.h"
#include "fm-search-list-view.h"

#define PENDING_USER_DATA_KEY		"pending user data"

#define SEARCH_LIST_VIEW_COLUMN_NONE    	-1

#define SEARCH_LIST_VIEW_COLUMN_ICON		0
#define SEARCH_LIST_VIEW_COLUMN_EMBLEMS 	1
#define SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH     2
#define SEARCH_LIST_VIEW_COLUMN_NAME		3
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


static void              fm_search_list_view_create_list                (FMListView *list_view);

static void              fm_search_list_view_initialize                 (gpointer            object,
									 gpointer            klass);
static void              fm_search_list_view_initialize_class           (gpointer            klass);
static void              fm_search_list_view_add_file                   (FMDirectoryView *list_view,
									 NautilusFile *file);
static void              fm_search_list_view_destroy                    (GtkObject *object);
static int               compare_rows                                   (GtkCList *clist,
									 gconstpointer ptr1,
									 gconstpointer ptr2);
static NautilusList *    get_list                                       (FMSearchListView         *list_view);
static int               add_to_list                                    (FMSearchListView *list_view, 
									 NautilusFile *file);
static int               sort_criterion_from_column                     (int column);
const char *             get_attribute_from_column                      (int column);


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
  
	/* Override list view's add_file method */
	fm_directory_view_class->add_file = fm_search_list_view_add_file;
	fm_list_view_class->create_list = fm_search_list_view_create_list;
	
	object_class->destroy = fm_search_list_view_destroy;
  
}


static void
fm_search_list_view_initialize (gpointer object,
				gpointer klass)
{

}


static void
fm_search_list_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	g_return_if_fail (FM_IS_SEARCH_LIST_VIEW (view));

	/* We are allowed to get the same icon twice, so don't re-add it. */
	if (gtk_clist_find_row_from_data (GTK_CLIST (get_list (FM_SEARCH_LIST_VIEW (view))), file) < 0) {
		add_to_list (FM_SEARCH_LIST_VIEW (view), file);
	}
}


static int
add_to_list (FMSearchListView *list_view, NautilusFile *file)
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
	text = g_new0 (char *, SEARCH_LIST_VIEW_COLUMN_COUNT+1);

	for (column = 0; column < SEARCH_LIST_VIEW_COLUMN_COUNT; ++column) {
		/* No text in icon column */
		if (column != SEARCH_LIST_VIEW_COLUMN_ICON) {
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
	nautilus_list_mark_cell_as_link (list, new_row, SEARCH_LIST_VIEW_COLUMN_NAME);
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, NULL);

	fm_list_view_install_row_images (FM_LIST_VIEW (list_view), new_row);

	g_strfreev (text);

	return new_row;
}


static NautilusList *
get_list (FMSearchListView *list_view)
{

	g_return_val_if_fail (FM_IS_SEARCH_LIST_VIEW (list_view), NULL);
	if (fm_list_view_list_is_instantiated (FM_LIST_VIEW (list_view)) == FALSE) {
		fm_search_list_view_create_list (FM_LIST_VIEW (list_view));
	}
	g_return_val_if_fail (NAUTILUS_IS_LIST (GTK_BIN (list_view)->child), NULL);

	return NAUTILUS_LIST (GTK_BIN (list_view)->child);
}


static void
fm_search_list_view_create_list (FMListView *search_list_view)
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
		_("Directory"),
		_("Name"),
		_("Size"),
		_("Type"),
		_("Date Modified"),
	};
	
	guint widths[] = {
		fm_list_view_get_icon_size (FM_LIST_VIEW (search_list_view)),	/* Icon */
		40,	/* Emblems */
		200,   /* Directory Name */
		130,	/* File Name */
		55,	/* Size */
		95,	/* Type */
		100,	/* Modified */
	};

	guint min_widths[] = {
		fm_list_view_get_icon_size (FM_LIST_VIEW (search_list_view)),	/* Icon */
		20,	/* Emblems */
		30,    /* Directory Name */
		30,	/* File Name */
		20,	/* Size */
		20,	/* Type */
		30,	/* Modified */
	};

	guint max_widths[] = {
		fm_list_view_get_icon_size (FM_LIST_VIEW (search_list_view)),	/* Icon */
		300,	/* Emblems */
		300,    /* Directory Name */
		300,	/* File Name */
		80,	/* Size */
		200,	/* Type */
		200,	/* Modified */
	};

	int i;

	g_return_if_fail (FM_IS_SEARCH_LIST_VIEW (search_list_view));

	list = NAUTILUS_LIST (nautilus_list_new_with_titles (SEARCH_LIST_VIEW_COLUMN_COUNT, titles));
	clist = GTK_CLIST (list);

	for (i = 0; i < SEARCH_LIST_VIEW_COLUMN_COUNT; ++i) {
		gboolean right_justified;

		right_justified = (i == SEARCH_LIST_VIEW_COLUMN_SIZE);

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
	gtk_container_add (GTK_CONTAINER (search_list_view), GTK_WIDGET (list));

	gtk_clist_set_auto_sort (clist, TRUE);
	gtk_clist_set_compare_func (clist, compare_rows);
	fm_list_view_set_instantiated (FM_LIST_VIEW (search_list_view));

	fm_list_view_setup_list (FM_LIST_VIEW (search_list_view));
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
	
	sort_criterion = sort_criterion_from_column (clist->sort_column);
	return nautilus_file_compare_for_sort (file1, file2, sort_criterion);
}


static int
sort_criterion_from_column (int column)
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
get_attribute_from_column (int column)
{
	switch (column) {
	case SEARCH_LIST_VIEW_COLUMN_ICON:
		return SEARCH_LIST_VIEW_ICON_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_NAME:
		return SEARCH_LIST_VIEW_NAME_ATTRIBUTE;
	case SEARCH_LIST_VIEW_COLUMN_ACTUAL_PATH:
		/* FIXME: Darin added this just to get rid of the core
		 * dump. Need to add real code here.
		 */
		return SEARCH_LIST_VIEW_NAME_ATTRIBUTE;
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

static void
fm_search_list_view_destroy (GtkObject *object)
{
	g_return_if_fail (FM_IS_SEARCH_LIST_VIEW (object));

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}
