/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
 
#include <config.h>

#include <gnome.h>
#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-view-component.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <liboaf/liboaf.h>

typedef struct {
	NautilusView *view;
	Nautilus_HistoryFrame history_frame;
	GtkCList *clist;

	gint notify_count;
	gint press_row;
	BonoboUIHandler *uih;
} HistoryView;

#define HISTORY_VIEW_COLUMN_ICON	0
#define HISTORY_VIEW_COLUMN_NAME	1
#define HISTORY_VIEW_COLUMN_COUNT	2

static NautilusBookmark *
get_bookmark_from_row (GtkCList *clist, int row)
{
	g_assert (NAUTILUS_IS_BOOKMARK (gtk_clist_get_row_data (clist, row)));
	return NAUTILUS_BOOKMARK (gtk_clist_get_row_data (clist, row));  
}

static char *
get_uri_from_row (GtkCList *clist, int row)
{
	return nautilus_bookmark_get_uri (get_bookmark_from_row (clist, row));
}

static Nautilus_HistoryFrame
history_view_frame_call_begin (NautilusView *view, CORBA_Environment *ev)
{
	g_return_val_if_fail (NAUTILUS_IS_VIEW (view), CORBA_OBJECT_NIL);
	
	CORBA_exception_init (ev);
	return Bonobo_Unknown_query_interface 
		(bonobo_control_get_control_frame (nautilus_view_get_bonobo_control (view)),
		 "IDL:Nautilus/HistoryFrame:1.0", ev);
}

static void
history_view_frame_call_end (Nautilus_HistoryFrame frame, CORBA_Environment *ev)
{
	Nautilus_HistoryFrame_unref (frame, ev);
	CORBA_Object_release (frame, ev);
	CORBA_exception_free (ev);
}

static void
install_icon (GtkCList *clist, gint row)
{
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	NautilusBookmark *bookmark;

	bookmark = get_bookmark_from_row (clist, row);
	
	if (!nautilus_bookmark_get_pixmap_and_mask (bookmark,
		  				    NAUTILUS_ICON_SIZE_SMALLER,
						    &pixmap,
						    &bitmap))
	{
		return;
	}

	gtk_clist_set_pixmap (clist,	
			      row,
			      HISTORY_VIEW_COLUMN_ICON,
			      pixmap,
			      bitmap);
}

static void
history_view_update_icons (GtkCList *clist)
{
	int row;
		
	for (row = 0; row < clist->rows; ++row) {
		install_icon (clist, row);
	}
}

static Nautilus_History *
get_history_list (HistoryView *hview)
{
	CORBA_Environment ev;
	Nautilus_HistoryFrame view_frame;
	Nautilus_History *list;

	view_frame = history_view_frame_call_begin (hview->view, &ev);
	list = Nautilus_HistoryFrame_get_history_list (view_frame, &ev);		
	history_view_frame_call_end (view_frame, &ev);

	return list;
}

static void
history_load_location (NautilusView *view,
                                            const char *location,
                                            HistoryView *hview)
{
	char *cols[HISTORY_VIEW_COLUMN_COUNT];
	int new_rownum;
	GtkCList *clist;
	NautilusBookmark *bookmark;
	Nautilus_History *history;
	Nautilus_HistoryItem *item;
	int i;

	hview->notify_count++;

	clist = hview->clist;
	gtk_clist_freeze (clist);

	/* Clear out list */
	gtk_clist_clear (clist);

	/* Populate with data from main history list */	
	history = get_history_list (hview);
	
	for (i = 0; i < history->list._length; i++) {
		item = &history->list._buffer[i];		
		bookmark = nautilus_bookmark_new (item->location, item->title);
		
		cols[HISTORY_VIEW_COLUMN_ICON] = NULL;
		cols[HISTORY_VIEW_COLUMN_NAME] = item->title;

		new_rownum = gtk_clist_append (clist, cols);
		
		gtk_clist_set_row_data_full (clist, new_rownum, bookmark,
					     (GtkDestroyNotify) gtk_object_unref);
		install_icon (clist, new_rownum);
		
		gtk_clist_columns_autosize (clist);
		
		if (gtk_clist_row_is_visible(clist, new_rownum) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto(clist, new_rownum, -1, 0.5, 0.0);
		}
	}
	CORBA_free (history);

	gtk_clist_select_row (clist, 0, 0);
	
	gtk_clist_thaw (clist);
	
  	hview->notify_count--;
}


static void
history_button_press (GtkCList *clist, GdkEventButton *event, HistoryView *hview)
{
	int row, column;
		
	/* Get row and column */
	gtk_clist_get_selection_info (clist, event->x, event->y, &row, &column);

	hview->press_row = row;
}

static void
history_button_release (GtkCList *clist, GdkEventButton *event, HistoryView *hview)
{
	char *uri;
	int row, column;
	
	if(hview->notify_count > 0) {
		return;
	}
	
	/* Get row and column */
	gtk_clist_get_selection_info (clist, event->x, event->y, &row, &column);

	/* Do nothing if row is zero.  A click either in the top list item
	 * or in the history content view is ignored. 
	 */
	if (row <= 0) {
		return;
	}
	
 	/* Return if the row does not match the rwo we stashed on the mouse down. */
	if (row != hview->press_row) {
		return;
	}
	
	/* Navigate to uri */
	uri = get_uri_from_row (clist, row);
	nautilus_view_open_location (hview->view, uri);
	g_free (uri);
}

static int object_count = 0;

static void
do_destroy(GtkObject *obj, HistoryView *hview)
{
	object_count--;
	if(object_count <= 0) {
    		gtk_main_quit();
    	}
}


static BonoboObject *
make_obj (BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
	GtkWidget *wtmp;
	GtkCList *clist;
	HistoryView *hview;

  	g_return_val_if_fail(!strcmp(goad_id, "OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb"), NULL);

  	hview = g_new0(HistoryView, 1);

	/* create interface */
  	clist = GTK_CLIST (gtk_clist_new (HISTORY_VIEW_COLUMN_COUNT));
  	gtk_clist_column_titles_hide (clist);
	gtk_clist_set_row_height (clist, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_clist_set_selection_mode (clist, GTK_SELECTION_BROWSE);
	gtk_clist_columns_autosize (clist);
	
	wtmp = gtk_scrolled_window_new (gtk_clist_get_hadjustment (clist),
					gtk_clist_get_vadjustment (clist));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (wtmp), GTK_WIDGET (clist));

	gtk_widget_show_all (wtmp);

	/* create object */
	hview->view = nautilus_view_new (wtmp);
	gtk_signal_connect (GTK_OBJECT (hview->view), "destroy", do_destroy, hview);
	object_count++;

	hview->clist = (GtkCList *)clist;

	/* handle events */
	gtk_signal_connect(GTK_OBJECT(hview->view), "load_location", 
			   history_load_location, hview);

	gtk_signal_connect(GTK_OBJECT(clist), "button-press-event", history_button_press, hview);
	gtk_signal_connect(GTK_OBJECT(clist), "button-release-event", history_button_release, hview);

	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					 "icons_changed",
					 history_view_update_icons,
					 GTK_OBJECT (hview->clist));
					 
	return BONOBO_OBJECT (hview->view);
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;

	gnome_init_with_popt_table ("nautilus-history-view", VERSION, 
				    argc, argv,
				    oaf_popt_options, 0, NULL); 
	orb = oaf_init (argc, argv);
	bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	gnome_vfs_init ();

	factory = bonobo_generic_factory_new_multi ("OAFIID:nautilus_history_view_factory:912d6634-d18f-40b6-bb83-bdfe16f1d15e", make_obj, NULL);

	do {
		bonobo_main();
  	} while(object_count > 0);

	return 0;
}
