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

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkscrolledwindow.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-view-component.h>
#include <libnautilus/nautilus-view-standard-main.h>

#define NAUTILUS_TYPE_HISTORY_VIEW            (nautilus_history_view_get_type ())
#define NAUTILUS_HISTORY_VIEW(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryView))
#define NAUTILUS_HISTORY_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryViewClass))
#define NAUTILUS_IS_HISTORY_VIEW(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HISTORY_VIEW))
#define NAUTILUS_IS_HISTORY_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HISTORY_VIEW))

typedef struct {
	NautilusView parent;
	GtkCList *list;
	gboolean updating_history;
	int press_row;
} NautilusHistoryView;

typedef struct {
	NautilusViewClass parent;
} NautilusHistoryViewClass;

#define HISTORY_VIEW_COLUMN_ICON	0
#define HISTORY_VIEW_COLUMN_NAME	1
#define HISTORY_VIEW_COLUMN_COUNT	2

static GtkType nautilus_history_view_get_type         (void);
static void    nautilus_history_view_initialize_class (NautilusHistoryViewClass *klass);
static void    nautilus_history_view_initialize       (NautilusHistoryView      *view);
static void    nautilus_history_view_destroy          (GtkObject                *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusHistoryView,
				   nautilus_history_view,
				   NAUTILUS_TYPE_VIEW)

static NautilusBookmark *
get_bookmark_from_row (GtkCList *list, int row)
{
	return NAUTILUS_BOOKMARK (gtk_clist_get_row_data (list, row));
}

static char *
get_uri_from_row (GtkCList *list, int row)
{
	return nautilus_bookmark_get_uri (get_bookmark_from_row (list, row));
}

static void
install_icon (GtkCList *list, gint row, GdkPixbuf *pixbuf)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	NautilusBookmark *bookmark;

	if (pixbuf != NULL) {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 
						   NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
	} else {
		bookmark = get_bookmark_from_row (list, row);
		if (!nautilus_bookmark_get_pixmap_and_mask (bookmark, NAUTILUS_ICON_SIZE_SMALLER, 
							    &pixmap, &mask)) {
			return;
		}
	}
	
	gtk_clist_set_pixmap (list, row, HISTORY_VIEW_COLUMN_ICON, pixmap, mask);

	gdk_pixmap_unref (pixmap);
	if (mask != NULL) {
		gdk_pixmap_unref (mask);
	}
}

static void
update_history (NautilusHistoryView *view,
		Nautilus_History *history)
{
	char *cols[HISTORY_VIEW_COLUMN_COUNT];
	int new_row;
	GtkCList *list;
	NautilusBookmark *bookmark;
	Nautilus_HistoryItem *item;
	GdkPixbuf *pixbuf;
	guint i;

	/* FIXME: We'll end up with old history if this happens. */
	if (view->updating_history) {
		return;
	}

	view->updating_history = TRUE;

	list = view->list;
	gtk_clist_freeze (list);

	gtk_clist_clear (list);

	for (i = 0; i < history->_length; i++) {
		item = &history->_buffer[i];
		bookmark = nautilus_bookmark_new (item->location, item->title);
		
		cols[HISTORY_VIEW_COLUMN_ICON] = NULL;
		cols[HISTORY_VIEW_COLUMN_NAME] = item->title;

		new_row = gtk_clist_append (list, cols);
		
		gtk_clist_set_row_data_full (list, new_row, bookmark,
					     (GtkDestroyNotify) gtk_object_unref);

		pixbuf = bonobo_ui_util_xml_to_pixbuf (item->icon);
		if (pixbuf != NULL) {
			install_icon (list, new_row, pixbuf);
			gdk_pixbuf_unref (pixbuf);
		} else {
			install_icon (list, new_row, NULL);
		}
		
		gtk_clist_columns_autosize (list);
		
		if (gtk_clist_row_is_visible (list, new_row) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto (list, new_row, -1, 0.5, 0.0);
		}
	}

	gtk_clist_select_row (list, 0, 0);
	
	gtk_clist_thaw (list);
	
  	view->updating_history = FALSE;
}

static void
button_press_callback (GtkCList *list,
		       GdkEventButton *event,
		       NautilusHistoryView *view)
{
	int row, column;

	if (event->button != 1) {
		return;
	}

	gtk_clist_get_selection_info (list, event->x, event->y, &row, &column);

	view->press_row = row;
}

static void
button_release_callback (GtkCList *list,
			 GdkEventButton *event,
			 NautilusHistoryView *view)
{
	char *uri;
	int row, column;
	
	/* FIXME: Is it really a good idea to just ignore button presses when we are updating? */
	if (view->updating_history) {
		return;
	}
	
	if (event->button != 1) {
		return;
	}
	
	gtk_clist_get_selection_info (list, event->x, event->y, &row, &column);
	
	/* Do nothing if row is zero. A click either in the top list
	 * item or in the history content view is ignored.
	 */
	if (row <= 0) {
		return;
	}
	
 	/* Do nothing if the row does not match the row we stashed on
	 * the mouse down. This means that dragging will not cause
	 * navigation.
	 */
	if (row != view->press_row) {
		return;
	}
	
	/* Navigate to the clicked location. */
	uri = get_uri_from_row (list, row);
	nautilus_view_open_location_in_this_window
		(NAUTILUS_VIEW (view), uri);
	g_free (uri);
}

static void
history_changed_callback (NautilusHistoryView *view,
			  Nautilus_History *list,
			  gpointer callback_data)
{
	g_assert (view == callback_data);
	update_history (view, list);
}

static void
nautilus_history_view_initialize_class (NautilusHistoryViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_history_view_destroy;
}

static void
nautilus_history_view_initialize (NautilusHistoryView *view)
{
	GtkCList *list;
	GtkWidget *window;

  	list = GTK_CLIST (gtk_clist_new (HISTORY_VIEW_COLUMN_COUNT));
  	gtk_clist_column_titles_hide (list);
	gtk_clist_set_row_height (list, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_clist_set_selection_mode (list, GTK_SELECTION_BROWSE);
	gtk_clist_columns_autosize (list);
	gtk_widget_show (GTK_WIDGET (list));
	
	window = gtk_scrolled_window_new (gtk_clist_get_hadjustment (list),
					  gtk_clist_get_vadjustment (list));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (list));
	gtk_widget_show (window);
	
	nautilus_view_construct (NAUTILUS_VIEW (view), window);

	view->list = list;

	gtk_signal_connect (GTK_OBJECT (list),
			    "button-press-event",
			    button_press_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (list),
			    "button-release-event",
			    button_release_callback,
			    view);

	gtk_signal_connect (GTK_OBJECT (view),
			    "history_changed", 
			    history_changed_callback,
			    view);
}

static void
nautilus_history_view_destroy (GtkObject *object)
{
	NautilusHistoryView *view;
	
	view = NAUTILUS_HISTORY_VIEW (object);

	gtk_clist_clear (view->list);
	
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

int
main (int argc, char *argv[])
{
	return nautilus_view_standard_main ("nautilus_history-view", VERSION,
					    argc, argv,
					    "OAFIID:nautilus_history_view_factory:912d6634-d18f-40b6-bb83-bdfe16f1d15e",
					    "OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb",
					    nautilus_view_create_from_get_type_function,
					    nautilus_history_view_get_type);
}
