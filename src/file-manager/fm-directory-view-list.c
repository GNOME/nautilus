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
#include <gtk/gtkhbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/gtkflist.h>
#include <libnautilus/nautilus-background.h>

struct _FMDirectoryViewListDetails
{
	gint sort_column;
	gboolean sort_reversed;
	guint icon_size;
};

#define DEFAULT_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"

#define LIST_VIEW_COLUMN_NONE		-1

#define LIST_VIEW_COLUMN_ICON		0
#define LIST_VIEW_COLUMN_NAME		1
#define LIST_VIEW_COLUMN_SIZE		2
#define LIST_VIEW_COLUMN_MIME_TYPE	3
#define LIST_VIEW_COLUMN_DATE_MODIFIED	4
#define LIST_VIEW_COLUMN_COUNT		5


/* forward declarations */
static void add_to_flist 			    (FMDirectoryViewList *list_view,
		   		 		     NautilusFile *file);
static void column_clicked_cb 			    (GtkCList *clist,
			       	 		     gint column,
			       	 		     gpointer user_data);
static GtkFList *create_flist 			    (FMDirectoryViewList *list_view);
static void flist_activate_cb 			    (GtkFList *flist,
			       	 		     gpointer entry_data,
			       	 		     gpointer data);
static void flist_selection_changed_cb 	  	    (GtkFList *flist, gpointer data);
static void fm_directory_view_list_add_entry 	    (FMDirectoryView *view, 
				 		     NautilusFile *file);
static void fm_directory_view_list_background_changed_cb
                                                    (NautilusBackground *background,
						     FMDirectoryViewList *list_view);
static void fm_directory_view_list_begin_adding_entries
						    (FMDirectoryView *view);
static void fm_directory_view_list_clear 	    (FMDirectoryView *view);
static GList *fm_directory_view_list_get_selection  (FMDirectoryView *view);
static void fm_directory_view_list_initialize 	    (gpointer object, gpointer klass);
static void fm_directory_view_list_initialize_class (gpointer klass);
static void fm_directory_view_list_destroy 	    (GtkObject *object);
static void fm_directory_view_list_done_adding_entries 
						    (FMDirectoryView *view);
static GtkFList *get_flist 			    (FMDirectoryViewList *list_view);
static GtkWidget *get_sort_indicator 		    (GtkFList *flist, 
						     gint column, 
						     gboolean reverse);
static void hide_sort_indicator 		    (GtkFList *flist, gint column);
static void install_icon 			    (FMDirectoryViewList *list_view, 
						     NautilusFile *file,
						     guint row,
						     guint column);
static void show_sort_indicator 		    (GtkFList *flist, 
						     gint column, 
						     gboolean sort_reversed);

static char * down_xpm[] = {
"6 5 2 1",
" 	c None",
".	c #000000",
"......",
"      ",
" .... ",
"      ",
"  ..  "};

static char * up_xpm[] = {
"6 5 2 1",
" 	c None",
".	c #000000",
"  ..  ",
"      ",
" .... ",
"      ",
"......"};


NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDirectoryViewList, fm_directory_view_list, FM_TYPE_DIRECTORY_VIEW);



/* GtkObject methods.  */

static void
fm_directory_view_list_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	FMDirectoryViewClass *fm_directory_view_class;

	object_class = GTK_OBJECT_CLASS (klass);
	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (klass);

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
	list_view->details->sort_column = LIST_VIEW_COLUMN_NONE;
	list_view->details->sort_reversed = FALSE;
	list_view->details->icon_size = NAUTILUS_ICON_SIZE_SMALLER;
	
	create_flist (list_view);
}

static void
fm_directory_view_list_destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



static void 
column_clicked_cb (GtkCList *clist, gint column, gpointer user_data)
{
	FMDirectoryViewList *list_view;
	FMDirectoryViewSortType sort_type;
	GtkFList *flist;

	g_return_if_fail (GTK_IS_FLIST (clist));
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (user_data));
	g_return_if_fail (get_flist (FM_DIRECTORY_VIEW_LIST (user_data)) == GTK_FLIST (clist));

	list_view = FM_DIRECTORY_VIEW_LIST (user_data);
	sort_type = FM_DIRECTORY_VIEW_SORT_NONE;
	flist = GTK_FLIST (clist);
	
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

	hide_sort_indicator (flist, list_view->details->sort_column);

	if (column == list_view->details->sort_column)
	{
		list_view->details->sort_reversed = !list_view->details->sort_reversed;
	}
	else
	{
		list_view->details->sort_reversed = FALSE;
		list_view->details->sort_column = column;
	}

	show_sort_indicator (flist, column, list_view->details->sort_reversed);

	
	fm_directory_view_sort (FM_DIRECTORY_VIEW (list_view), 
				sort_type,
				list_view->details->sort_reversed);
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
		 list_view->details->icon_size,	/* Icon */
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
		GtkWidget *hbox;
		GtkWidget *label;
		GtkWidget *sort_up_indicator;
		GtkWidget *sort_down_indicator;
		gboolean right_justified;

		right_justified = (i == LIST_VIEW_COLUMN_SIZE);
	
		gtk_clist_set_column_width (GTK_CLIST (flist), i, widths[i]);

		/* Column header button contains three views, a title,
		 * a "sort downward" indicator, and a "sort upward" indicator. 
		 * Only one sort indicator (for all columns) is shown at once.
		 */
		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_widget_show (GTK_WIDGET (hbox));
		label = gtk_label_new (titles[i]);
		gtk_widget_show (GTK_WIDGET (label));

		/* sort indicators are initially hidden */
		sort_up_indicator = gnome_pixmap_new_from_xpm_d (up_xpm);
		sort_down_indicator = gnome_pixmap_new_from_xpm_d (down_xpm);

		if (!right_justified)
		{
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		}

		gtk_box_pack_end (GTK_BOX (hbox), sort_up_indicator, FALSE, FALSE, GNOME_PAD);
		gtk_box_pack_end (GTK_BOX (hbox), sort_down_indicator, FALSE, FALSE, GNOME_PAD);

		if (right_justified)
		{
			gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);
			gtk_clist_set_column_justification (GTK_CLIST (flist), i, GTK_JUSTIFY_RIGHT);
		}
		
		gtk_clist_set_column_widget (GTK_CLIST (flist), i, hbox);
	}

	/* Make height tall enough for icons to look good */
	gtk_clist_set_row_height (GTK_CLIST (flist), list_view->details->icon_size);
	
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

	gtk_signal_connect (GTK_OBJECT (nautilus_get_widget_background (GTK_WIDGET (flist))),
			    "changed",
			    GTK_SIGNAL_FUNC (fm_directory_view_list_background_changed_cb),
			    list_view);

	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (flist));

	gtk_widget_show (GTK_WIDGET (flist));

	fm_directory_view_populate (FM_DIRECTORY_VIEW (list_view));

	return flist;
}

static void
flist_activate_cb (GtkFList *flist,
		   gpointer entry_data,
		   gpointer data)
{
	g_return_if_fail (GTK_IS_FLIST (flist));
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (data));
	g_return_if_fail (entry_data != NULL);

	fm_directory_view_activate_entry (FM_DIRECTORY_VIEW (data), entry_data);
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
add_to_flist (FMDirectoryViewList *list_view,
	      NautilusFile *file)
{
	GtkCList *clist;
	gchar *text[LIST_VIEW_COLUMN_COUNT];
	gchar *name;
	gchar *size_string;
	gchar *modified_string;
	gchar *type_string;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (list_view));

	text[LIST_VIEW_COLUMN_ICON] = NULL;
	
	name = nautilus_file_get_name (file);
	text[LIST_VIEW_COLUMN_NAME] = name;

	size_string = nautilus_file_get_size_as_string (file);
	text[LIST_VIEW_COLUMN_SIZE] = size_string;

	modified_string = nautilus_file_get_date_as_string (file);
	text[LIST_VIEW_COLUMN_DATE_MODIFIED] = modified_string;

	type_string = nautilus_file_get_type_as_string (file);
	text[LIST_VIEW_COLUMN_MIME_TYPE] = type_string;
	
	clist = GTK_CLIST (get_flist(list_view));
	gtk_clist_append (clist, text);
	gtk_clist_set_row_data (clist, clist->rows - 1, file);

	install_icon (list_view, 
		      file, 
		      clist->rows - 1, 
		      LIST_VIEW_COLUMN_ICON);

	g_free (name);
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
	GtkFList *flist;
	char *background_color;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	flist = get_flist (FM_DIRECTORY_VIEW_LIST (view));

	/* Clear away the existing list items. */
	gtk_clist_clear (GTK_CLIST (flist));

	/* Set up the background color from the metadata. */
	background_color = nautilus_directory_get_metadata (fm_directory_view_get_model (view),
							    "LIST_VIEW_BACKGROUND_COLOR",
							    DEFAULT_BACKGROUND_COLOR);
	nautilus_background_set_color (nautilus_get_widget_background (GTK_WIDGET (flist)),
				       background_color);
	g_free (background_color);
}

static void
fm_directory_view_list_begin_adding_entries (FMDirectoryView *view)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	gtk_clist_freeze (GTK_CLIST (get_flist (FM_DIRECTORY_VIEW_LIST (view))));
}

static void
fm_directory_view_list_add_entry (FMDirectoryView *view, NautilusFile *file)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	add_to_flist (FM_DIRECTORY_VIEW_LIST (view), file);
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

static void
fm_directory_view_list_background_changed_cb (NautilusBackground *background,
					      FMDirectoryViewList *list_view)
{
	NautilusDirectory *directory;
	char *color_spec;

	g_assert (FM_IS_DIRECTORY_VIEW_LIST (list_view));
	g_assert (background == nautilus_get_widget_background
		  (GTK_WIDGET (get_flist (list_view))));

	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view));
	if (directory == NULL)
		return;
	
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (directory,
					 "LIST_VIEW_BACKGROUND_COLOR",
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);
}

static GtkWidget *
get_sort_indicator (GtkFList *flist, gint column, gboolean reverse)
{
	GtkWidget *column_widget;
	GtkWidget *result;
	GList *children;

	g_return_val_if_fail (GTK_IS_FLIST (flist), NULL);
	g_return_val_if_fail (column >= 0, NULL);

	column_widget = gtk_clist_get_column_widget (GTK_CLIST (flist), column);
	g_assert (GTK_IS_HBOX (column_widget));

	children = gtk_container_children (GTK_CONTAINER (column_widget));
	result = GTK_WIDGET (g_list_nth_data (children, reverse ? 1 : 2));
	g_list_free (children);

	return result;
}

static void
hide_sort_indicator (GtkFList *flist, gint column)
{
	g_return_if_fail (GTK_IS_FLIST (flist));

	if (column == LIST_VIEW_COLUMN_NONE)
		return;

	gtk_widget_hide (get_sort_indicator (flist, column, FALSE));
	gtk_widget_hide (get_sort_indicator (flist, column, TRUE));
}


/**
 * install_icon:
 *
 * Put an icon for a file into the specified cell.
 * @list_view: FMDirectoryView in which to install icon.
 * @file: NautilusFile representing file whose icon should be installed.
 * @row: row index of target cell
 * @column: column index of target cell
 * 
 **/
static void
install_icon (FMDirectoryViewList *list_view, 
	      NautilusFile *file, 
	      guint row, 
	      guint column)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view));
	g_return_if_fail (file != NULL);
	
	pixbuf = fm_icon_cache_get_icon_for_file (fm_get_current_icon_cache(), 
					 	  file,
					 	  list_view->details->icon_size);

	/* GtkCList requires a pixmap & mask rather than a pixbuf */
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
	gtk_clist_set_pixmap (GTK_CLIST (get_flist (list_view)), row, column, pixmap, bitmap);

	gdk_pixbuf_unref (pixbuf);
}

static void
show_sort_indicator (GtkFList *flist, gint column, gboolean sort_reversed)
{
	g_return_if_fail (GTK_IS_FLIST (flist));

	if (column == LIST_VIEW_COLUMN_NONE)
		return;

	gtk_widget_show (get_sort_indicator (flist, column, sort_reversed));
}

