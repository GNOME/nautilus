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
#include "fm-search-list-view.h"

#include "fm-directory-view.h"
#include "fm-list-view-private.h"
#include <libnautilus-extensions/nautilus-search-bar-criterion.h>

#include <libgnome/gnome-i18n.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

static void fm_search_list_view_initialize       (gpointer          object,
						  gpointer          klass);
static void fm_search_list_view_initialize_class (gpointer          klass);
static int  real_get_number_of_columns           (FMListView       *list_view);
static int  real_get_link_column                 (FMListView       *list_view);
static void real_get_column_specification        (FMListView       *list_view,
						  int               column_number,
						  FMListViewColumn *specification);
static void begin_loading_callback               (FMDirectoryView *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMSearchListView,
				   fm_search_list_view,
				   FM_TYPE_LIST_VIEW)
static void
begin_loading_callback (FMDirectoryView *view)
{
	char *human_string;
	NautilusView *nautilus_view;
	char *uri;
	
	uri = fm_directory_view_get_uri (view);

	nautilus_view = fm_directory_view_get_nautilus_view (view);
	
	human_string = nautilus_search_bar_criterion_human_from_uri (uri);

	nautilus_view_set_title (nautilus_view, human_string);

	g_free (human_string);

}

static void
fm_search_list_view_initialize_class (gpointer klass)
{
	FMListViewClass *fm_list_view_class;
	
	fm_list_view_class = FM_LIST_VIEW_CLASS (klass);
  
	fm_list_view_class->get_number_of_columns = real_get_number_of_columns;
	fm_list_view_class->get_link_column = real_get_link_column;
	fm_list_view_class->get_column_specification = real_get_column_specification;
}

static void
fm_search_list_view_initialize (gpointer object,
				gpointer klass)
{
	FMDirectoryView *directory_view;


	g_assert (GTK_BIN (object)->child == NULL);

	directory_view = FM_DIRECTORY_VIEW (object);


	gtk_signal_connect (GTK_OBJECT(directory_view),
			    "begin_loading",
			    GTK_SIGNAL_FUNC (begin_loading_callback),
			    NULL);

}

static int
real_get_number_of_columns (FMListView *view)
{
	return 7;
}

static int
real_get_link_column (FMListView *view)
{
	return 3;
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
					 "directory", _("Directory"),
					 NAUTILUS_FILE_SORT_BY_DIRECTORY,
					 30, 130, 500, FALSE);
		break;
	case 3:
		fm_list_view_column_set (specification,
					 "name", _("Name"),
					 NAUTILUS_FILE_SORT_BY_NAME,
					 30, 200, 300, FALSE);
		break;
	case 4:
		fm_list_view_column_set (specification,
					 "size", _("Size"),
					 NAUTILUS_FILE_SORT_BY_SIZE,
					 20, 55, 80, TRUE);
		break;
	case 5:
		fm_list_view_column_set (specification,
					 "type", _("Type"),
					 NAUTILUS_FILE_SORT_BY_TYPE,
					 20, 95, 200, FALSE);
		break;
	case 6:
		fm_list_view_column_set (specification,
					 "date_modified", _("Date Modified"),
					 NAUTILUS_FILE_SORT_BY_MTIME,
					 30, 95, 200, FALSE);
		break;
	default:
		g_assert_not_reached ();
	}
}
