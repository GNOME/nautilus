/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */
 
#include <config.h>

#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-preferences.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkscrolledwindow.h>
#include <libgnome/gnome-macros.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>

#include "nautilus-history-view.h"

#define FACTORY_IID "OAFIID:Nautilus_History_View_Factory"

#define NAUTILUS_HISTORY_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryViewClass))
#define NAUTILUS_IS_HISTORY_VIEW(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HISTORY_VIEW))
#define NAUTILUS_IS_HISTORY_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HISTORY_VIEW))

typedef struct {
	NautilusViewClass parent;
} NautilusHistoryViewClass;

enum {
	HISTORY_VIEW_COLUMN_ICON,
	HISTORY_VIEW_COLUMN_NAME,
	HISTORY_VIEW_COLUMN_BOOKMARK,
	HISTORY_VIEW_COLUMN_COUNT
};

BONOBO_CLASS_BOILERPLATE (NautilusHistoryView, nautilus_history_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
update_history (NautilusHistoryView    *view,
		const Nautilus_History *history)
{
	GtkListStore         *store;
	GtkTreeSelection     *selection;
	NautilusBookmark     *bookmark;
	Nautilus_HistoryItem *item;
	GdkPixbuf            *pixbuf;
	guint                 i;
	gboolean              stop_updating_history;
	GtkTreeIter           iter;
	
	/* Set up a local boolean so we can detect that the view has
	 * been destroyed. We can't ask the view itself because once
	 * it's destroyed it's pointer is a pointer to freed storage.
	 */
	/* FIXME: We can't just keep an extra ref to the view as we
	 * normally would because of a bug in Bonobo that means a
	 * BonoboControl must not outlast its BonoboControlFrame
	 * (NautilusHistoryView is a BonoboControl).
	 */
	if (view->stop_updating_history != NULL) {
		*view->stop_updating_history = TRUE;
	}
	stop_updating_history = FALSE;
	view->stop_updating_history = &stop_updating_history;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (view->tree_view));

	gtk_list_store_clear (store);

	for (i = 0; i < history->_length; i++) {
		item = &history->_buffer[i];
		bookmark = nautilus_bookmark_new (item->location, item->title);

		/* Through a long line of calls, nautilus_bookmark_new
		 * can end up calling through to CORBA, so a remote
		 * unref can come in at this point. In theory, other
		 * calls could result in a similar problem, so in
		 * theory we need this check after any call out, but
		 * in practice, none of the other calls used here have
		 * that problem.
		 */
		if (stop_updating_history) {
			return;
		}

		pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS, FALSE);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    HISTORY_VIEW_COLUMN_ICON, pixbuf,
				    HISTORY_VIEW_COLUMN_NAME, item->title,
				    HISTORY_VIEW_COLUMN_BOOKMARK, bookmark,
				    -1);

		if (pixbuf != NULL) {
			g_object_unref (pixbuf);
		}
	}

	selection = GTK_TREE_SELECTION (gtk_tree_view_get_selection (view->tree_view));

	if (gtk_tree_model_get_iter_root (GTK_TREE_MODEL (store), &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
	
  	view->stop_updating_history = NULL;
}

static void
history_changed_callback (NautilusHistoryView    *view,
			  const Nautilus_History *history,
			  gpointer                callback_data)
{
	g_assert (view == callback_data);

	update_history (view, history);
}

static void
row_activated_callback (GtkTreeView *tree_view,
			GtkTreePath *path,
			GtkTreeViewColumn *column,
			gpointer user_data)
{
	NautilusHistoryView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	NautilusBookmark *bookmark;
	char *uri;
	
	view = NAUTILUS_HISTORY_VIEW (user_data);
	model = gtk_tree_view_get_model (tree_view);
	
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return;
	}
	
	gtk_tree_model_get 
		(model, &iter, HISTORY_VIEW_COLUMN_BOOKMARK, &bookmark, -1);
	
	/* Navigate to the clicked location. */
	uri = nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (bookmark));
	nautilus_view_open_location
		(NAUTILUS_VIEW (view), 
		 uri, Nautilus_ViewFrame_OPEN_ACCORDING_TO_MODE, 0, NULL);
	g_free (uri);	
}

static void
update_click_policy (NautilusHistoryView *view)
{
	int policy;
	
	policy = eel_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY);
	
	eel_gtk_tree_view_set_activate_on_single_click
		(view->tree_view, policy == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
click_policy_changed_callback (gpointer user_data)
{
	NautilusHistoryView *view;
	
	view = NAUTILUS_HISTORY_VIEW (user_data);

	update_click_policy (view);
}

static void
nautilus_history_view_instance_init (NautilusHistoryView *view)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	GtkWidget         *window;
	
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_widget_show (GTK_WIDGET (tree_view));

	col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "pixbuf", HISTORY_VIEW_COLUMN_ICON,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", HISTORY_VIEW_COLUMN_NAME,
					     NULL);

	gtk_tree_view_column_set_fixed_width (col, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_tree_view_append_column (tree_view, col);
	
	store = gtk_list_store_new (HISTORY_VIEW_COLUMN_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    NAUTILUS_TYPE_BOOKMARK);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	
	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (tree_view));
	gtk_widget_show (window);
	
	nautilus_view_construct (NAUTILUS_VIEW (view), window);

	g_object_ref (tree_view);
	view->tree_view = tree_view;
		
	nautilus_view_set_listener_mask (NAUTILUS_VIEW (view),
					 NAUTILUS_VIEW_LISTEN_HISTORY);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);	

	g_signal_connect_object
		(tree_view, "row_activated", 
		 G_CALLBACK (row_activated_callback), view, 0);
	
	g_signal_connect_object (view, "history_changed",
				 G_CALLBACK (history_changed_callback), view, 0);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
				      click_policy_changed_callback,
				      view);
	update_click_policy (view);
}

static void
nautilus_history_view_finalize (GObject *object)
{
	NautilusHistoryView *view;
	
	view = NAUTILUS_HISTORY_VIEW (object);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					 click_policy_changed_callback,
					 view);

	if (view->stop_updating_history != NULL) {
		*view->stop_updating_history = TRUE;
	}

	g_object_unref (view->tree_view);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_history_view_class_init (NautilusHistoryViewClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_history_view_finalize;
}

int
main (int argc, char *argv[])
{
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}

	return nautilus_view_standard_main ("nautilus_history-view",
					    VERSION,
					    GETTEXT_PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    nautilus_view_create_from_get_type_function,
					    NULL,
					    nautilus_history_view_get_type);
}
