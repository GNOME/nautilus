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
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkscrolledwindow.h>
#include <libgnome/gnome-macros.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus/nautilus-view-standard-main.h>

#define FACTORY_IID	"OAFIID:nautilus_history_view_factory:912d6634-d18f-40b6-bb83-bdfe16f1d15e"
#define VIEW_IID	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb"

#define NAUTILUS_TYPE_HISTORY_VIEW            (nautilus_history_view_get_type ())
#define NAUTILUS_HISTORY_VIEW(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryView))
#define NAUTILUS_HISTORY_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryViewClass))
#define NAUTILUS_IS_HISTORY_VIEW(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HISTORY_VIEW))
#define NAUTILUS_IS_HISTORY_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HISTORY_VIEW))

typedef struct {
	NautilusView      parent;
	GtkTreeView      *treeview;
	GtkListStore     *store;
	GtkTreeSelection *selection;
	gboolean          updating_history;
	int               press_row;
	gboolean         *external_destroyed_flag;
	int               selection_changed_id;
} NautilusHistoryView;

typedef struct {
	NautilusViewClass parent;
} NautilusHistoryViewClass;

enum {
	HISTORY_VIEW_COLUMN_ICON,
	HISTORY_VIEW_COLUMN_NAME,
	HISTORY_VIEW_COLUMN_BOOKMARK,
	HISTORY_VIEW_COLUMN_COUNT,
} HistoryViewColumnType;

static GtkType nautilus_history_view_get_type   (void);

BONOBO_CLASS_BOILERPLATE (NautilusHistoryView, nautilus_history_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
update_history (NautilusHistoryView    *view,
		const Nautilus_History *history)
{
	GtkTreeView          *treeview;
	GtkListStore         *store;
	GtkTreeSelection     *selection;
	NautilusBookmark     *bookmark;
	Nautilus_HistoryItem *item;
	GdkPixbuf            *pixbuf;
	guint                 i;
	gboolean              destroyed_flag;
	GtkTreeIter           iter;
	
	/* FIXME: We'll end up with old history if this happens. */
	if (view->updating_history) {
		return;
	}

	treeview = view->treeview;
	store = view->store;
	selection = view->selection;
	
	/* Set up a local boolean so we can detect that the view has
	 * been destroyed. We can't ask the view itself because once
	 * it's destroyed it's pointer is a pointer to freed storage.
	 */
	/* FIXME: We can't just keep an extra ref to the view as we
	 * normally would because of a bug in Bonobo that means a
	 * BonoboControl must not outlast its BonoboFrame
	 * (NautilusHistoryView is a BonoboControl).
	 */
	destroyed_flag = FALSE;
	view->external_destroyed_flag = &destroyed_flag;

	view->updating_history = TRUE;

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
		if (destroyed_flag) {
			return;
		}
		pixbuf = bonobo_ui_util_xml_to_pixbuf (item->icon);
		
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

	g_signal_handler_block (selection, view->selection_changed_id);
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
	g_signal_handler_unblock (selection, view->selection_changed_id);	
	
  	view->updating_history = FALSE;

	view->external_destroyed_flag = NULL;
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
on_selection_changed (GtkTreeSelection *selection,
		      gpointer user_data)
{
	NautilusHistoryView *view;
	GtkListStore        *store;
	GtkTreeIter          iter;
	NautilusBookmark    *bookmark;
	char                *uri;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (NAUTILUS_IS_HISTORY_VIEW (user_data));

	view = NAUTILUS_HISTORY_VIEW (user_data);
	store = view->store;

	/* If this function returns FALSE, we don't have any rows selected */
	if (!gtk_tree_selection_get_selected (selection,
					      NULL,
					      &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (store),
			    &iter,
			    HISTORY_VIEW_COLUMN_BOOKMARK, &bookmark,
			    -1);
	
	/* Navigate to the clicked location. */
	uri = nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (bookmark));
	nautilus_view_open_location_in_this_window
		(NAUTILUS_VIEW (view), uri);
	g_free (uri);

}

static void
nautilus_history_view_instance_init (NautilusHistoryView *view)
{
	GtkTreeView       *treeview;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	GtkWidget         *window;
	
	treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (treeview, FALSE);
	gtk_widget_show (GTK_WIDGET (treeview));

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
	gtk_tree_view_append_column (treeview, col);
	
	store = gtk_list_store_new (HISTORY_VIEW_COLUMN_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    NAUTILUS_TYPE_BOOKMARK);

	gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));
	
	selection = GTK_TREE_SELECTION (gtk_tree_view_get_selection (treeview));
	
	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (treeview));
	gtk_widget_show (window);
	
	nautilus_view_construct (NAUTILUS_VIEW (view), window);

	g_object_ref (treeview);
	view->treeview = treeview;
	view->store = store;
	view->selection = selection;
		
	nautilus_view_set_listener_mask (NAUTILUS_VIEW (view),
					 NAUTILUS_VIEW_LISTEN_HISTORY);

	view->selection_changed_id = g_signal_connect (selection,
						       "changed",
						       G_CALLBACK (on_selection_changed),
						       view);
	
	g_signal_connect (view,
			  "history_changed",
			  G_CALLBACK (history_changed_callback),
			  view);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);	
}

static void
nautilus_history_view_finalize (GObject *object)
{
	NautilusHistoryView *view;
	
	view = NAUTILUS_HISTORY_VIEW (object);

	if (view->external_destroyed_flag != NULL) {
		*view->external_destroyed_flag = TRUE;
	}

	g_object_unref (view->treeview);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_history_view_class_init (NautilusHistoryViewClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = nautilus_history_view_finalize;
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
					    nautilus_global_preferences_init,
					    nautilus_history_view_get_type);
}
