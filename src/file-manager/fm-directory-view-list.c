/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-directory-view-list.c - implementation of list view of directory.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fm-directory-view-list.h"
#include "fm-icon-cache.h"
#include <libgnome/gnome-i18n.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/gtkflist.h>

static FMDirectoryViewClass *parent_class = NULL;

struct _FMDirectoryViewListDetails
{
	FMDirectoryViewSortType sort_type;
	gboolean sort_reversed;
};


/* forward declarations */
static void add_to_flist 			    (FMIconCache *icon_manager,
		   		 		     GtkFList *flist,
		   		 		     GnomeVFSFileInfo *info);
static void column_clicked_cb 			    (GtkCList *ignored,
			       	 		     gint column,
			       	 		     gpointer user_data);
static GtkFList *create_flist 			    (FMDirectoryViewList *list_view);
static void flist_activate_cb 			    (GtkFList *ignored,
			       	 		     gpointer entry_data,
			       	 		     gpointer data);
static void flist_selection_changed_cb 	  	    (GtkFList *flist, gpointer data);
static void fm_directory_view_list_initialize_class (gpointer klass);
static void fm_directory_view_list_initialize 	    (gpointer object, gpointer klass);
static void fm_directory_view_list_destroy 	    (GtkObject *object);
static void fm_directory_view_list_add_entry 	    (FMDirectoryView *view, 
				 		     GnomeVFSFileInfo *info);
static void fm_directory_view_list_begin_adding_entries 
						    (FMDirectoryView *view);
static void fm_directory_view_list_clear 	    (FMDirectoryView *view);
static void fm_directory_view_list_done_adding_entries 
						    (FMDirectoryView *view);
static GList * fm_directory_view_list_get_selection (FMDirectoryView *view);
static GtkFList *get_flist 			    (FMDirectoryViewList *list_view);




/* GtkObject methods.  */

NAUTILUS_DEFINE_GET_TYPE_FUNCTION (FMDirectoryViewList, fm_directory_view_list, FM_TYPE_DIRECTORY_VIEW);

static void
fm_directory_view_list_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

	parent_class = gtk_type_class (gtk_type_parent(object_class->type));
	
	object_class->destroy = fm_directory_view_list_destroy;
	
	fm_directory_view_class->clear = fm_directory_view_list_clear;	
	fm_directory_view_class->begin_adding_entries = fm_directory_view_list_begin_adding_entries;	
	fm_directory_view_class->add_entry = fm_directory_view_list_add_entry;	
	fm_directory_view_class->done_adding_entries = fm_directory_view_list_done_adding_entries;	
	fm_directory_view_class->get_selection = fm_directory_view_list_get_selection;	
}

static void
fm_directory_view_list_initialize (gpointer object, gpointer klass)
{
	FMDirectoryViewList *list_view;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (object));
	g_return_if_fail (GTK_BIN (object)->child == NULL);

	list_view = FM_DIRECTORY_VIEW_LIST (object);

	list_view->details = g_new0 (FMDirectoryViewListDetails, 1);
	list_view->details->sort_type = FM_DIRECTORY_VIEW_SORT_NONE;
	list_view->details->sort_reversed = FALSE;
	
	create_flist (list_view);
}

static void
fm_directory_view_list_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



#define LIST_VIEW_COLUMN_ICON		0
#define LIST_VIEW_COLUMN_NAME		1
#define LIST_VIEW_COLUMN_SIZE		2
#define LIST_VIEW_COLUMN_MIME_TYPE	3
#define LIST_VIEW_COLUMN_DATE_MODIFIED	4
#define LIST_VIEW_COLUMN_COUNT		5


static void 
column_clicked_cb (GtkCList *ignored, gint column, gpointer user_data)
{
	FMDirectoryViewList *list_view;
	FMDirectoryViewSortType sort_type;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (user_data));

	list_view = FM_DIRECTORY_VIEW_LIST (user_data);
	
	switch (column)
	{
		case LIST_VIEW_COLUMN_ICON:	
			sort_type = FM_DIRECTORY_VIEW_SORT_BYTYPE;
			break;
		case LIST_VIEW_COLUMN_NAME:
			sort_type = FM_DIRECTORY_VIEW_SORT_BYNAME;
			break;
		case LIST_VIEW_COLUMN_SIZE:
			sort_type = FM_DIRECTORY_VIEW_SORT_BYSIZE;
			break;
		case LIST_VIEW_COLUMN_DATE_MODIFIED:
			sort_type = FM_DIRECTORY_VIEW_SORT_BYMTIME;
			break;
		case LIST_VIEW_COLUMN_MIME_TYPE:
			sort_type = FM_DIRECTORY_VIEW_SORT_BYTYPE;
			break;
		default: g_assert_not_reached();
	}

	if (sort_type == list_view->details->sort_type)
		list_view->details->sort_reversed = !list_view->details->sort_reversed;
	else
		list_view->details->sort_reversed = FALSE;

	list_view->details->sort_type = sort_type;
	
	fm_directory_view_sort (FM_DIRECTORY_VIEW (list_view), 
				list_view->details->sort_type,
				list_view->details->sort_reversed
				);
}

static GtkFList *
create_flist (FMDirectoryViewList *list_view)
{
	GtkFList *flist;
	gchar *titles[] = {
		NULL,
		_("Name"),
		_("Size"),
		_("Type"),
		_("Date Modified"),
	};
	uint widths[] = {
		 20,	/* Icon */
		130,	/* Name */
		 55,	/* Size */
		 95,	/* Type */
		100,	/* Modified */
	};
	int i;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view), NULL);

	flist = GTK_FLIST (gtk_flist_new_with_titles (LIST_VIEW_COLUMN_COUNT, titles));

	for (i = 0; i < LIST_VIEW_COLUMN_COUNT; ++i)
	{
		gtk_clist_set_column_width (GTK_CLIST (flist), i, widths[i]);
	}

	gtk_clist_set_column_justification (GTK_CLIST (flist), 
					    LIST_VIEW_COLUMN_SIZE, 
					    GTK_JUSTIFY_RIGHT);
	
	GTK_WIDGET_SET_FLAGS (flist, GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (flist),
			    "activate",
			    GTK_SIGNAL_FUNC (flist_activate_cb),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (flist_selection_changed_cb),
			    list_view);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "click_column",
			    column_clicked_cb,
			    list_view);
	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (flist));

	gtk_widget_show (GTK_WIDGET (flist));

	fm_directory_view_populate (FM_DIRECTORY_VIEW (list_view));

	return flist;
}

static void
flist_activate_cb (GtkFList *ignored,
		   gpointer entry_data,
		   gpointer data)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (data));
	g_return_if_fail (entry_data != NULL);

	fm_directory_view_activate_entry (FM_DIRECTORY_VIEW (data), 
					  (GnomeVFSFileInfo *) entry_data);
}

static void
flist_selection_changed_cb (GtkFList *flist,
			    gpointer data)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (data));
	g_return_if_fail (flist == get_flist (FM_DIRECTORY_VIEW_LIST (data)));

	fm_directory_view_notify_selection_changed (FM_DIRECTORY_VIEW (data));
}

static void
add_to_flist (FMIconCache *icon_manager,
	      GtkFList *flist,
	      GnomeVFSFileInfo *info)
{
	GtkCList *clist;
	gchar *text[LIST_VIEW_COLUMN_COUNT];
	gchar *size_string;
	gchar *modified_string;
	gchar *type_string;

	/* FIXME: Icon column needs a pixmap */
	text[LIST_VIEW_COLUMN_ICON] = NULL;
	
	text[LIST_VIEW_COLUMN_NAME] = info->name;

	size_string = nautilus_file_size_as_string (info);
	text[LIST_VIEW_COLUMN_SIZE] = size_string;

	modified_string = nautilus_file_date_as_string (info);
	text[LIST_VIEW_COLUMN_DATE_MODIFIED] = modified_string;

	type_string = nautilus_file_type_as_string (info);
	text[LIST_VIEW_COLUMN_MIME_TYPE] = type_string;
	
	clist = GTK_CLIST (flist);
	gtk_clist_append (clist, text);
	gtk_clist_set_row_data (clist, clist->rows - 1, info);

	g_free (size_string);
	g_free (modified_string);
	g_free (type_string);
}

static GtkFList *
get_flist (FMDirectoryViewList *list_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view), NULL);
	g_return_val_if_fail (GTK_IS_FLIST (GTK_BIN (list_view)->child), NULL);

	return GTK_FLIST (GTK_BIN (list_view)->child);
}

static void
fm_directory_view_list_clear (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	gtk_clist_clear (GTK_CLIST (get_flist (FM_DIRECTORY_VIEW_LIST (view))));
}

static void
fm_directory_view_list_begin_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	gtk_clist_freeze (GTK_CLIST (get_flist (FM_DIRECTORY_VIEW_LIST (view))));
}

static void
fm_directory_view_list_add_entry (FMDirectoryView *view, GnomeVFSFileInfo *info)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	add_to_flist (fm_get_current_icon_cache(), get_flist (FM_DIRECTORY_VIEW_LIST (view)), info);
}

static void
fm_directory_view_list_done_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	gtk_clist_thaw (GTK_CLIST (get_flist (FM_DIRECTORY_VIEW_LIST (view))));
}

static GList *
fm_directory_view_list_get_selection (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view), NULL);

	return gtk_flist_get_selection (get_flist (FM_DIRECTORY_VIEW_LIST (view)));
}


