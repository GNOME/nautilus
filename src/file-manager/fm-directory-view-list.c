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

#include <config.h>
#include "fm-directory-view-list.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/gtkflist.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-icon-factory.h>
#include <libnautilus/nautilus-metadata.h>

struct _FMDirectoryViewListDetails
{
	gint sort_column;
	gboolean sort_reversed;
	guint zoom_level;
};

#define DEFAULT_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"

#define LIST_VIEW_COLUMN_NONE		-1

#define LIST_VIEW_COLUMN_ICON		0
#define LIST_VIEW_COLUMN_NAME		1
#define LIST_VIEW_COLUMN_SIZE		2
#define LIST_VIEW_COLUMN_MIME_TYPE	3
#define LIST_VIEW_COLUMN_DATE_MODIFIED	4
#define LIST_VIEW_COLUMN_COUNT		5

/* special values for get_data and set_data */

#define PENDING_USER_DATA_KEY		"pending user data"
#define SORT_INDICATOR_KEY		"sort indicator"
#define UP_INDICATOR_VALUE		1
#define DOWN_INDICATOR_VALUE		2

/* file attributes associated with columns */

#define LIST_VIEW_ICON_ATTRIBUTE		"icon"
#define LIST_VIEW_NAME_ATTRIBUTE		"name"
#define LIST_VIEW_SIZE_ATTRIBUTE		"size"
#define LIST_VIEW_MIME_TYPE_ATTRIBUTE		"type"
#define LIST_VIEW_DATE_MODIFIED_ATTRIBUTE	"date_modified"

#define LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE	LIST_VIEW_NAME_ATTRIBUTE


/* forward declarations */
static void add_to_flist	 		    (FMDirectoryViewList *list_view,
		   		 		     NautilusFile *file);
static void column_clicked_cb 			    (GtkCList *clist,
			       	 		     gint column,
			       	 		     gpointer user_data);
static gint compare_rows 			    (GtkCList *clist,
	      					     gconstpointer  ptr1,
	      					     gconstpointer  ptr2);
static void context_click_row_cb		    (GtkCList *clist,
						     gint row,
						     FMDirectoryViewList *list_view);
static void context_click_background_cb		    (GtkCList *clist,
						     FMDirectoryViewList *list_view);
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
static void fm_directory_view_list_begin_loading    (FMDirectoryView *view);
static void fm_directory_view_list_bump_zoom_level  (FMDirectoryView *view, 
						     gint zoom_increment);
static gboolean fm_directory_view_list_can_zoom_in  (FMDirectoryView *view);
static gboolean fm_directory_view_list_can_zoom_out (FMDirectoryView *view);
static void fm_directory_view_list_clear 	    (FMDirectoryView *view);
static guint fm_directory_view_list_get_icon_size   (FMDirectoryViewList *list_view);
static GList *fm_directory_view_list_get_selection  (FMDirectoryView *view);
static NautilusZoomLevel fm_directory_view_list_get_zoom_level 
						    (FMDirectoryViewList *list_view);
static void fm_directory_view_list_initialize 	    (gpointer object, gpointer klass);
static void fm_directory_view_list_initialize_class (gpointer klass);
static void fm_directory_view_list_destroy 	    (GtkObject *object);
static void fm_directory_view_list_done_adding_entries 
						    (FMDirectoryView *view);
static void fm_directory_view_list_select_all       (FMDirectoryView *view);

static void fm_directory_view_list_set_zoom_level   (FMDirectoryViewList *list_view,
				       		     NautilusZoomLevel new_level);
static void fm_directory_view_list_sort_items 	    (FMDirectoryViewList *list_view, 
				   		     int column, 
				   		     gboolean reversed);
const char *get_attribute_from_column 	    	    (int column);
int get_column_from_attribute	 		    (const char *value);
int get_sort_column_from_attribute 		    (const char *value);
static GtkFList *get_flist 			    (FMDirectoryViewList *list_view);
static GtkWidget *get_sort_indicator 		    (GtkFList *flist, 
						     gint column, 
						     gboolean reverse);
static void hide_sort_indicator 		    (GtkFList *flist, gint column);
static void install_icon 			    (FMDirectoryViewList *list_view, 
						     guint row);
static void show_sort_indicator 		    (GtkFList *flist, 
						     gint column, 
						     gboolean sort_reversed);
static int sort_criterion_from_column 		    (int column);

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
	fm_directory_view_class->begin_loading = fm_directory_view_list_begin_loading;
	fm_directory_view_class->add_entry = fm_directory_view_list_add_entry;	
	fm_directory_view_class->done_adding_entries = fm_directory_view_list_done_adding_entries;	
	fm_directory_view_class->get_selection = fm_directory_view_list_get_selection;	
	fm_directory_view_class->bump_zoom_level = fm_directory_view_list_bump_zoom_level;	
	fm_directory_view_class->can_zoom_in = fm_directory_view_list_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_directory_view_list_can_zoom_out;
	fm_directory_view_class->select_all = fm_directory_view_list_select_all;
}

static void
fm_directory_view_list_initialize (gpointer object, gpointer klass)
{
	FMDirectoryViewList *list_view;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (object));
	g_return_if_fail (GTK_BIN (object)->child == NULL);

	list_view = FM_DIRECTORY_VIEW_LIST (object);

	list_view->details = g_new0 (FMDirectoryViewListDetails, 1);

	/* These initial values are needed so the state is right when
	 * the metadata is read in later.
	 */
	list_view->details->zoom_level = NAUTILUS_ZOOM_LEVEL_SMALLER;
	list_view->details->sort_column = LIST_VIEW_COLUMN_NONE;

	create_flist (list_view);
}

static void
fm_directory_view_list_destroy (GtkObject *object)
{
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (object));

	g_free (FM_DIRECTORY_VIEW_LIST (object)->details);
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}



static void 
column_clicked_cb (GtkCList *clist, gint column, gpointer user_data)
{
	FMDirectoryViewList *list_view;
	gboolean reversed;

	g_return_if_fail (GTK_IS_FLIST (clist));
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (user_data));
	g_return_if_fail (get_flist (FM_DIRECTORY_VIEW_LIST (user_data)) == GTK_FLIST (clist));

	list_view = FM_DIRECTORY_VIEW_LIST (user_data);

	if (column == list_view->details->sort_column)
	{
		reversed = !list_view->details->sort_reversed;
	}
	else
	{
		reversed = FALSE;
	}

	fm_directory_view_list_sort_items (list_view, column, reversed);
}

static gint
compare_rows (GtkCList *clist,
	      gconstpointer  ptr1,
	      gconstpointer  ptr2)
{
	GtkCListRow *row1;
	GtkCListRow *row2;
	NautilusFile *file1;
	NautilusFile *file2;
	int sort_criterion;
  
	g_return_val_if_fail (GTK_IS_FLIST (clist), 0);
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
	if (file1 == NULL)
	{
		file1 = (NautilusFile *)gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	else if (file2 == NULL)
	{
		file2 = (NautilusFile *)gtk_object_get_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY);
	}
	g_assert (file1 != NULL && file2 != NULL);
	
	sort_criterion = sort_criterion_from_column (clist->sort_column);

	return nautilus_file_compare_for_sort (file1, file2, sort_criterion);
}

static void 
context_click_row_cb (GtkCList *clist, gint row, FMDirectoryViewList *list_view)
{
	NautilusFile * file;

	g_assert (GTK_IS_CLIST (clist));
	g_assert (FM_IS_DIRECTORY_VIEW_LIST (list_view));

	file = NAUTILUS_FILE(gtk_clist_get_row_data (clist, clist->rows - 1));

	fm_directory_view_popup_item_context_menu (FM_DIRECTORY_VIEW (list_view), file);
}


static void 
context_click_background_cb (GtkCList *clist, FMDirectoryViewList *list_view)
{
	g_assert (FM_IS_DIRECTORY_VIEW_LIST (list_view));

	fm_directory_view_popup_background_context_menu (FM_DIRECTORY_VIEW (list_view));
}


static GtkFList *
create_flist (FMDirectoryViewList *list_view)
{
	GtkFList *flist;
	GtkCList *clist;
	
	gchar *titles[] = {
		NULL,
		_("Name"),
		_("Size"),
		_("Type"),
		_("Date Modified"),
	};
	uint widths[] = {
		 fm_directory_view_list_get_icon_size (list_view),	/* Icon */
		130,	/* Name */
		 55,	/* Size */
		 95,	/* Type */
		100,	/* Modified */
	};
	int i;

	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view), NULL);

	flist = GTK_FLIST (gtk_flist_new_with_titles (LIST_VIEW_COLUMN_COUNT, titles));
	clist = GTK_CLIST (flist);

	for (i = 0; i < LIST_VIEW_COLUMN_COUNT; ++i)
	{
		GtkWidget *hbox;
		GtkWidget *label;
		GtkWidget *sort_up_indicator;
		GtkWidget *sort_down_indicator;
		gboolean right_justified;

		right_justified = (i == LIST_VIEW_COLUMN_SIZE);
	
		gtk_clist_set_column_width (clist, i, widths[i]);

		/* Column header button contains three views, a title,
		 * a "sort downward" indicator, and a "sort upward" indicator. 
		 * Only one sort indicator (for all columns) is shown at once.
		 */
		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_widget_show (GTK_WIDGET (hbox));
		label = gtk_label_new (titles[i]);
		gtk_widget_show (GTK_WIDGET (label));

		/* Sort indicators are initially hidden. They're marked with
		 * special data so they can be located later in each column. 
		 */
		sort_up_indicator = gnome_pixmap_new_from_xpm_d (up_xpm);
		gtk_object_set_data(GTK_OBJECT(sort_up_indicator), 
		    		    SORT_INDICATOR_KEY, 
		    		    GINT_TO_POINTER (UP_INDICATOR_VALUE));

		sort_down_indicator = gnome_pixmap_new_from_xpm_d (down_xpm);
		gtk_object_set_data(GTK_OBJECT(sort_down_indicator), 
		    		    SORT_INDICATOR_KEY, 
		    		    GINT_TO_POINTER (DOWN_INDICATOR_VALUE));

		if (right_justified)
		{
			gtk_box_pack_start (GTK_BOX (hbox), sort_up_indicator, FALSE, FALSE, GNOME_PAD);
			gtk_box_pack_start (GTK_BOX (hbox), sort_down_indicator, FALSE, FALSE, GNOME_PAD);
			gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);

			gtk_clist_set_column_justification (clist, i, GTK_JUSTIFY_RIGHT);
		}
		else
		{
			gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
			gtk_box_pack_end (GTK_BOX (hbox), sort_up_indicator, FALSE, FALSE, GNOME_PAD);
			gtk_box_pack_end (GTK_BOX (hbox), sort_down_indicator, FALSE, FALSE, GNOME_PAD);
		}

		gtk_clist_set_column_widget (clist, i, hbox);
	}

	gtk_clist_set_auto_sort (clist, TRUE);
	gtk_clist_set_compare_func (clist, compare_rows);

	/* Make height tall enough for icons to look good */
	gtk_clist_set_row_height (clist, fm_directory_view_list_get_icon_size (list_view));
	
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
	gtk_signal_connect (GTK_OBJECT (flist),
			    "context_click_row",
			    context_click_row_cb,
			    list_view);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "context_click_background",
			    context_click_background_cb,
			    list_view);


	gtk_signal_connect (GTK_OBJECT (nautilus_get_widget_background (GTK_WIDGET (flist))),
			    "changed",
			    GTK_SIGNAL_FUNC (fm_directory_view_list_background_changed_cb),
			    list_view);

	gtk_container_add (GTK_CONTAINER (list_view), GTK_WIDGET (flist));
	gtk_widget_show (GTK_WIDGET (flist));

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

	fm_directory_view_activate_entry (FM_DIRECTORY_VIEW (data), entry_data, FALSE);
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
add_to_flist (FMDirectoryViewList *list_view, NautilusFile *file)
{
	GtkCList *clist;
	gchar **text;
	int new_row;
	int column;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW (list_view));
	g_return_if_fail (file != NULL);

	/* One extra slot so it's NULL-terminated */
	text = g_new0 (gchar *, LIST_VIEW_COLUMN_COUNT+1);

	for (column = 0; column < LIST_VIEW_COLUMN_COUNT; ++column)
	{
		/* No text in icon column */
		if (column != LIST_VIEW_COLUMN_ICON)
		{
			text[column] = 
				nautilus_file_get_string_attribute (file, 
								    get_attribute_from_column (column));
		}
	}
	
	clist = GTK_CLIST (get_flist(list_view));

	/* Temporarily set user data value as hack for the problem
	 * that compare_rows is called before the row data can be set.
	 */
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, file);
	/* Note that since list is auto-sorted new_row isn't necessarily last row. */
	new_row = gtk_clist_append (clist, text);
	gtk_clist_set_row_data (clist, new_row, file);
	gtk_object_set_data (GTK_OBJECT (clist), PENDING_USER_DATA_KEY, NULL);

	install_icon (list_view, new_row);

	g_strfreev (text);
}

static GtkFList *
get_flist (FMDirectoryViewList *list_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view), NULL);
	g_return_val_if_fail (GTK_IS_FLIST (GTK_BIN (list_view)->child), NULL);

	return GTK_FLIST (GTK_BIN (list_view)->child);
}

static void
fm_directory_view_list_bump_zoom_level (FMDirectoryView *view, gint zoom_increment)
{
	FMDirectoryViewList *list_view;
	NautilusZoomLevel old_level;
	NautilusZoomLevel new_level;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	list_view = FM_DIRECTORY_VIEW_LIST (view);
	old_level = fm_directory_view_list_get_zoom_level (list_view);

	if (zoom_increment < 0 && 0 - zoom_increment > old_level)
	{
		new_level = NAUTILUS_ZOOM_LEVEL_SMALLEST;
	} 
	else
	{
		new_level = MIN (old_level + zoom_increment,
				 NAUTILUS_ZOOM_LEVEL_LARGEST);
	}

	fm_directory_view_list_set_zoom_level (list_view, new_level);
}

static gboolean 
fm_directory_view_list_can_zoom_in (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view), FALSE);

	return fm_directory_view_list_get_zoom_level (FM_DIRECTORY_VIEW_LIST (view))
		< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
fm_directory_view_list_can_zoom_out (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view), FALSE);

	return fm_directory_view_list_get_zoom_level (FM_DIRECTORY_VIEW_LIST (view))
		> NAUTILUS_ZOOM_LEVEL_SMALLEST;
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
							    INDEX_PANEL_BACKGROUND_COLOR_METADATA_KEY,
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
fm_directory_view_list_begin_loading (FMDirectoryView *view)
{
	NautilusDirectory *directory;
	FMDirectoryViewList *list_view;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));

	directory = fm_directory_view_get_model (view);
	list_view = FM_DIRECTORY_VIEW_LIST (view);

	fm_directory_view_list_set_zoom_level (
		list_view,
		nautilus_directory_get_integer_metadata (
			directory, 
			LIST_VIEW_ZOOM_LEVEL_METADATA_KEY, 
			NAUTILUS_ZOOM_LEVEL_SMALLER));

	fm_directory_view_list_sort_items (
		list_view,
		get_sort_column_from_attribute (
			nautilus_directory_get_metadata (
				directory,
				LIST_VIEW_SORT_COLUMN_METADATA_KEY,
				LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE)),
		nautilus_directory_get_boolean_metadata (
			directory,
			LIST_VIEW_SORT_REVERSED_METADATA_KEY,
			FALSE));
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

static guint
fm_directory_view_list_get_icon_size (FMDirectoryViewList *list_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view), NAUTILUS_ICON_SIZE_STANDARD);

	return nautilus_icon_size_for_zoom_level (
		fm_directory_view_list_get_zoom_level (list_view));
}

static GList *
fm_directory_view_list_get_selection (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view), NULL);

	return gtk_flist_get_selection (get_flist (FM_DIRECTORY_VIEW_LIST (view)));
}

static NautilusZoomLevel
fm_directory_view_list_get_zoom_level (FMDirectoryViewList *list_view)
{
	g_return_val_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view),
			      NAUTILUS_ZOOM_LEVEL_STANDARD);

	return list_view->details->zoom_level;
}

static void
fm_directory_view_list_set_zoom_level (FMDirectoryViewList *list_view,
				       NautilusZoomLevel new_level)
{
	GtkCList *clist;
	int row;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (new_level == fm_directory_view_list_get_zoom_level (list_view))
		return;
	
	list_view->details->zoom_level = new_level;
	nautilus_directory_set_integer_metadata (
		fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view)), 
		LIST_VIEW_ZOOM_LEVEL_METADATA_KEY, 
		NAUTILUS_ZOOM_LEVEL_SMALLER,
		new_level);

	clist = GTK_CLIST (get_flist (list_view));
	
	gtk_clist_freeze (clist);
	gtk_clist_set_row_height (GTK_CLIST (get_flist (list_view)), 
				  fm_directory_view_list_get_icon_size (list_view));
	gtk_clist_set_column_width (GTK_CLIST (get_flist (list_view)),
				  LIST_VIEW_COLUMN_ICON,
				  fm_directory_view_list_get_icon_size (list_view));

	clist = GTK_CLIST (get_flist (list_view));

	for (row = 0; row < clist->rows; ++row)
	{
		install_icon (list_view, row);
	}
	gtk_clist_thaw (clist);
}

/* select all of the items in the view */
static void
fm_directory_view_list_select_all (FMDirectoryView *view)
{
	GtkCList *clist;
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (view));
	
        clist = GTK_CLIST (get_flist (FM_DIRECTORY_VIEW_LIST(view)));
        gtk_clist_select_all(clist);
}

static void
fm_directory_view_list_sort_items (FMDirectoryViewList *list_view, 
				   int column, 
				   gboolean reversed)
{
	GtkFList *flist;
	GtkCList *clist;
	NautilusDirectory *directory;
	
	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view));
	g_return_if_fail (column >= 0);
	g_return_if_fail (column < GTK_CLIST (get_flist (list_view))->columns);

	if (column == list_view->details->sort_column &&
	    reversed == list_view->details->sort_reversed)
	{
		return;
	}

	directory = fm_directory_view_get_model (FM_DIRECTORY_VIEW (list_view));
	nautilus_directory_set_metadata (
		directory,
		LIST_VIEW_SORT_COLUMN_METADATA_KEY,
		LIST_VIEW_DEFAULT_SORTING_ATTRIBUTE,
		get_attribute_from_column (column));
	nautilus_directory_set_boolean_metadata (
		directory,
		LIST_VIEW_SORT_REVERSED_METADATA_KEY,
		FALSE,
		reversed);

	flist = get_flist (list_view);
	clist = GTK_CLIST (flist);
	hide_sort_indicator (flist, list_view->details->sort_column);
	show_sort_indicator (flist, column, reversed);
	
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
					 INDEX_PANEL_BACKGROUND_COLOR_METADATA_KEY,
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);
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
	switch (column)
	{
		case LIST_VIEW_COLUMN_ICON:
			return LIST_VIEW_ICON_ATTRIBUTE;
		case LIST_VIEW_COLUMN_NAME:
			return LIST_VIEW_NAME_ATTRIBUTE;
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


static GtkWidget *
get_sort_indicator (GtkFList *flist, gint column, gboolean reverse)
{
	GtkWidget *column_widget;
	GtkWidget *result;
	GList *children;
	GList *iter;

	g_return_val_if_fail (GTK_IS_FLIST (flist), NULL);
	g_return_val_if_fail (column >= 0, NULL);

	column_widget = gtk_clist_get_column_widget (GTK_CLIST (flist), column);
	g_assert (GTK_IS_HBOX (column_widget));

	children = gtk_container_children (GTK_CONTAINER (column_widget));
	
	iter = children;
	result = NULL;
	while (iter != NULL)
	{
		int indicator_int = GPOINTER_TO_INT (
			gtk_object_get_data (GTK_OBJECT (iter->data), SORT_INDICATOR_KEY));

		if ((reverse && indicator_int == DOWN_INDICATOR_VALUE) ||
		    (!reverse && indicator_int == UP_INDICATOR_VALUE))
		{
			result = GTK_WIDGET (iter->data);
			break;
		}

		iter = g_list_next(iter);
	}

	g_assert (result != NULL);
	
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
 * @row: row index of target cell
 * 
 **/
static void
install_icon (FMDirectoryViewList *list_view, guint row)
{
	NautilusFile *file;
	GtkCList *clist;
	NautilusScalableIcon *scalable_icon;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_LIST (list_view));
	g_return_if_fail (file != NULL);

	clist = GTK_CLIST (get_flist (list_view));
	file = gtk_clist_get_row_data (clist, row);

	g_assert (file != NULL);
	
	scalable_icon = nautilus_icon_factory_get_icon_for_file (file);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon,
		 fm_directory_view_list_get_icon_size (list_view));
	nautilus_scalable_icon_unref (scalable_icon);

	/* GtkCList requires a pixmap & mask rather than a pixbuf */
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
	gtk_clist_set_pixmap (clist, row, LIST_VIEW_COLUMN_ICON, pixmap, bitmap);

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

static int
sort_criterion_from_column (int column)
{
	switch (column)
	{
		case LIST_VIEW_COLUMN_ICON:	
			return NAUTILUS_FILE_SORT_BY_TYPE;
		case LIST_VIEW_COLUMN_NAME:
			return NAUTILUS_FILE_SORT_BY_NAME;
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
