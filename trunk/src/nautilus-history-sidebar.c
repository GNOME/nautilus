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

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <libgnome/gnome-macros.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-signaller.h>

#include "nautilus-history-sidebar.h"

#define NAUTILUS_HISTORY_SIDEBAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HISTORY_SIDEBAR, NautilusHistorySidebarClass))
#define NAUTILUS_IS_HISTORY_SIDEBAR(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HISTORY_SIDEBAR))
#define NAUTILUS_IS_HISTORY_SIDEBAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HISTORY_SIDEBAR))

typedef struct {
	GtkScrolledWindowClass parent;
} NautilusHistorySidebarClass;

typedef struct {
        GObject parent;
} NautilusHistorySidebarProvider;

typedef struct {
        GObjectClass parent;
} NautilusHistorySidebarProviderClass;


enum {
	HISTORY_SIDEBAR_COLUMN_ICON,
	HISTORY_SIDEBAR_COLUMN_NAME,
	HISTORY_SIDEBAR_COLUMN_BOOKMARK,
	HISTORY_SIDEBAR_COLUMN_COUNT
};

static void  nautilus_history_sidebar_iface_init        (NautilusSidebarIface         *iface);
static void  sidebar_provider_iface_init                (NautilusSidebarProviderIface *iface);
static GType nautilus_history_sidebar_provider_get_type (void);
static void  nautilus_history_sidebar_style_set	        (GtkWidget *widget,
							 GtkStyle  *previous_style);

G_DEFINE_TYPE_WITH_CODE (NautilusHistorySidebar, nautilus_history_sidebar, GTK_TYPE_SCROLLED_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR,
						nautilus_history_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (NautilusHistorySidebarProvider, nautilus_history_sidebar_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR_PROVIDER,
						sidebar_provider_iface_init));

static void
update_history (NautilusHistorySidebar *sidebar)
{
	GtkListStore         *store;
	GtkTreeSelection     *selection;
	NautilusBookmark     *bookmark;
	GdkPixbuf            *pixbuf;
	GtkTreeIter           iter;
	char *name;
	GList *l, *history;
	
	store = GTK_LIST_STORE (gtk_tree_view_get_model (sidebar->tree_view));

	gtk_list_store_clear (store);

	history = nautilus_window_info_get_history (sidebar->window);
	for (l = history; l != NULL; l = l->next) {
		bookmark = nautilus_bookmark_copy (l->data);

		pixbuf = nautilus_bookmark_get_pixbuf (bookmark, GTK_ICON_SIZE_MENU);
		name = nautilus_bookmark_get_name (bookmark);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    HISTORY_SIDEBAR_COLUMN_ICON, pixbuf,
				    HISTORY_SIDEBAR_COLUMN_NAME, name,
				    HISTORY_SIDEBAR_COLUMN_BOOKMARK, bookmark,
				    -1);
		g_object_unref (bookmark);

		if (pixbuf != NULL) {
			g_object_unref (pixbuf);
		}
		g_free (name);
	}
	eel_g_object_list_free (history);

	selection = GTK_TREE_SELECTION (gtk_tree_view_get_selection (sidebar->tree_view));

	if (gtk_tree_model_get_iter_root (GTK_TREE_MODEL (store), &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
history_changed_callback (GObject *signaller,
			  NautilusHistorySidebar *sidebar)
{
	update_history (sidebar);
}

static void
row_activated_callback (GtkTreeView *tree_view,
			GtkTreePath *path,
			GtkTreeViewColumn *column,
			gpointer user_data)
{
	NautilusHistorySidebar *sidebar;
	GtkTreeModel *model;
	GtkTreeIter iter;
	NautilusBookmark *bookmark;
	GFile *location;
	
	sidebar = NAUTILUS_HISTORY_SIDEBAR (user_data);
	model = gtk_tree_view_get_model (tree_view);
	
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return;
	}
	
	gtk_tree_model_get 
		(model, &iter, HISTORY_SIDEBAR_COLUMN_BOOKMARK, &bookmark, -1);
	
	/* Navigate to the clicked location. */
	location = nautilus_bookmark_get_location (NAUTILUS_BOOKMARK (bookmark));
	nautilus_window_info_open_location
		(sidebar->window, 
		 location, NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE, 0, NULL);
	g_object_unref (location);
}

static void
update_click_policy (NautilusHistorySidebar *sidebar)
{
	int policy;
	
	policy = eel_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY);
	
	eel_gtk_tree_view_set_activate_on_single_click
		(sidebar->tree_view, policy == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
click_policy_changed_callback (gpointer user_data)
{
	NautilusHistorySidebar *sidebar;
	
	sidebar = NAUTILUS_HISTORY_SIDEBAR (user_data);

	update_click_policy (sidebar);
}

static void
nautilus_history_sidebar_init (NautilusHistorySidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_widget_show (GTK_WIDGET (tree_view));

	col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "pixbuf", HISTORY_SIDEBAR_COLUMN_ICON,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", HISTORY_SIDEBAR_COLUMN_NAME,
					     NULL);

	gtk_tree_view_column_set_fixed_width (col, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_tree_view_append_column (tree_view, col);
	
	store = gtk_list_store_new (HISTORY_SIDEBAR_COLUMN_COUNT,
				    GDK_TYPE_PIXBUF,
				    G_TYPE_STRING,
				    NAUTILUS_TYPE_BOOKMARK);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	g_object_unref (store);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (sidebar));
	
	sidebar->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);	

	g_signal_connect_object
		(tree_view, "row_activated", 
		 G_CALLBACK (row_activated_callback), sidebar, 0);
	
	g_signal_connect_object (nautilus_signaller_get_current (),
				 "history_list_changed",
				 G_CALLBACK (history_changed_callback), sidebar, 0);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
				      click_policy_changed_callback,
				      sidebar);
	update_click_policy (sidebar);
}

static void
nautilus_history_sidebar_finalize (GObject *object)
{
	NautilusHistorySidebar *sidebar;
	
	sidebar = NAUTILUS_HISTORY_SIDEBAR (object);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					 click_policy_changed_callback,
					 sidebar);

	G_OBJECT_CLASS (nautilus_history_sidebar_parent_class)->finalize (object);
}

static void
nautilus_history_sidebar_class_init (NautilusHistorySidebarClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_history_sidebar_finalize;

	GTK_WIDGET_CLASS (class)->style_set = nautilus_history_sidebar_style_set;
}

static const char *
nautilus_history_sidebar_get_sidebar_id (NautilusSidebar *sidebar)
{
	return NAUTILUS_HISTORY_SIDEBAR_ID;
}

static char *
nautilus_history_sidebar_get_tab_label (NautilusSidebar *sidebar)
{
	return g_strdup (_("History"));
}

static char *
nautilus_history_sidebar_get_tab_tooltip (NautilusSidebar *sidebar)
{
	return g_strdup (_("Show History"));
}

static GdkPixbuf *
nautilus_history_sidebar_get_tab_icon (NautilusSidebar *sidebar)
{
	return NULL;
}

static void
nautilus_history_sidebar_is_visible_changed (NautilusSidebar *sidebar,
					     gboolean         is_visible)
{
	/* Do nothing */
}

static void
nautilus_history_sidebar_iface_init (NautilusSidebarIface *iface)
{
	iface->get_sidebar_id = nautilus_history_sidebar_get_sidebar_id;
	iface->get_tab_label = nautilus_history_sidebar_get_tab_label;
	iface->get_tab_tooltip = nautilus_history_sidebar_get_tab_tooltip;
	iface->get_tab_icon = nautilus_history_sidebar_get_tab_icon;
	iface->is_visible_changed = nautilus_history_sidebar_is_visible_changed;
}

static void
nautilus_history_sidebar_set_parent_window (NautilusHistorySidebar *sidebar,
					    NautilusWindowInfo *window)
{
	sidebar->window = window;
	update_history (sidebar);
}

static void
nautilus_history_sidebar_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	NautilusHistorySidebar *sidebar;

	sidebar = NAUTILUS_HISTORY_SIDEBAR (widget);

	update_history (sidebar);
}

static NautilusSidebar *
nautilus_history_sidebar_create (NautilusSidebarProvider *provider,
				 NautilusWindowInfo *window)
{
	NautilusHistorySidebar *sidebar;
	
	sidebar = g_object_new (nautilus_history_sidebar_get_type (), NULL);
	nautilus_history_sidebar_set_parent_window (sidebar, window);
	g_object_ref (sidebar);
	gtk_object_sink (GTK_OBJECT (sidebar));

	return NAUTILUS_SIDEBAR (sidebar);
}

static void 
sidebar_provider_iface_init (NautilusSidebarProviderIface *iface)
{
	iface->create = nautilus_history_sidebar_create;
}

static void
nautilus_history_sidebar_provider_init (NautilusHistorySidebarProvider *sidebar)
{
}

static void
nautilus_history_sidebar_provider_class_init (NautilusHistorySidebarProviderClass *class)
{
}

void
nautilus_history_sidebar_register (void)
{
        nautilus_module_add_type (nautilus_history_sidebar_provider_get_type ());
}

