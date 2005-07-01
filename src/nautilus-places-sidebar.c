/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
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
 *  Author : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *
 */
 
#include <config.h>

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>

#include "nautilus-bookmark-list.h"
#include "nautilus-places-sidebar.h"

#define NAUTILUS_PLACES_SIDEBAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebarClass))
#define NAUTILUS_IS_PLACES_SIDEBAR(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_IS_PLACES_SIDEBAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR))

typedef struct {
	GtkScrolledWindow  parent;
	GtkTreeView        *tree_view;
	char 	           *uri;
	GtkListStore       *store;
	NautilusWindowInfo *window;
	NautilusBookmarkList *bookmarks;
} NautilusPlacesSidebar;

typedef struct {
	GtkScrolledWindowClass parent;
} NautilusPlacesSidebarClass;

typedef struct {
        GObject parent;
} NautilusPlacesSidebarProvider;

typedef struct {
        GObjectClass parent;
} NautilusPlacesSidebarProviderClass;

enum {
	PLACES_SIDEBAR_COLUMN_ROW_TYPE,
	PLACES_SIDEBAR_COLUMN_URI,
	PLACES_SIDEBAR_COLUMN_NAME,
	PLACES_SIDEBAR_COLUMN_ICON,
	
	PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum {
	PLACES_BUILT_IN,
	PLACES_MOUNTED_VOLUME,
	PLACES_BOOKMARK,
	PLACES_SEPARATOR
} PlaceType;

static void  nautilus_places_sidebar_iface_init        (NautilusSidebarIface         *iface);
static void  sidebar_provider_iface_init               (NautilusSidebarProviderIface *iface);
static GType nautilus_places_sidebar_provider_get_type (void);

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebar, nautilus_places_sidebar, GTK_TYPE_SCROLLED_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR,
						nautilus_places_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebarProvider, nautilus_places_sidebar_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR_PROVIDER,
						sidebar_provider_iface_init));


static GtkTreeIter
add_place (GtkListStore *store, PlaceType place_type,
	   const char *name, const char *icon, const char *uri)
{
	GdkPixbuf            *pixbuf;
	GtkTreeIter           iter;

	pixbuf = nautilus_icon_factory_get_pixbuf_from_name (icon, NULL, NAUTILUS_ICON_SIZE_FOR_MENUS, TRUE, NULL);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    -1);
	if (pixbuf != NULL) {
		g_object_unref (pixbuf);
	}
	return iter;
}

static void
update_places (NautilusPlacesSidebar *sidebar)
{
	NautilusBookmark      *bookmark;
	GtkTreeSelection      *selection;
	GtkTreeIter           iter, last_iter;
	GnomeVFSVolumeMonitor *volume_monitor;
	GList 		      *volumes, *l;
	GnomeVFSVolume 	      *volume;
	int 		      bookmark_count, index;
	char 		      *location, *icon, *mount_uri, *name, *desktop_path;
		
	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	gtk_list_store_clear (sidebar->store);
	location = nautilus_window_info_get_current_location (sidebar->window);

	/* add built in bookmarks */

	desktop_path = nautilus_get_desktop_directory ();

	if (strcmp (g_get_home_dir(), desktop_path) != 0) {
		mount_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
		last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
				       _("Home"), "gnome-fs-home", mount_uri);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}	
		g_free (mount_uri);
	}

	mount_uri = gnome_vfs_get_uri_from_local_path (desktop_path);
	last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
			       _("Desktop"), "gnome-fs-desktop", mount_uri);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}	
	g_free (mount_uri);
	g_free (desktop_path);
	
 	mount_uri = "file:///"; // No need to strdup
	last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
			       _("Filesystem"), "gnome-fs-blockdev", mount_uri);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}	


	/* add mounted volumes */

	volume_monitor = gnome_vfs_get_volume_monitor ();
	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		if (!gnome_vfs_volume_is_user_visible (volume)) {
			gnome_vfs_volume_unref (volume);
			continue;
		}
		icon = gnome_vfs_volume_get_icon (volume);
		mount_uri = gnome_vfs_volume_get_activation_uri (volume);
		name = gnome_vfs_volume_get_display_name (volume);
		last_iter = add_place (sidebar->store, PLACES_MOUNTED_VOLUME,
				       name, icon, mount_uri);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}
		gnome_vfs_volume_unref (volume);
		g_free (icon);
		g_free (name);
		g_free (mount_uri);

	}
	g_list_free (volumes);
	

	/* add separator */

	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_SEPARATOR,
			    -1);

	/* add bookmarks */

	bookmark_count = nautilus_bookmark_list_length (sidebar->bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		bookmark = nautilus_bookmark_list_item_at (sidebar->bookmarks, index);

		name = nautilus_bookmark_get_name (bookmark);
		icon = nautilus_bookmark_get_icon (bookmark);
		mount_uri = nautilus_bookmark_get_uri (bookmark);
		last_iter = add_place (sidebar->store, PLACES_BOOKMARK,
				       name, icon, mount_uri);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}
		g_free (name);
		g_free (icon);
		g_free (mount_uri);
	}

	g_free (location);
	
}

static gboolean
shortcuts_row_separator_func (GtkTreeModel *model,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
	PlaceType	 	type; 

  	gtk_tree_model_get (model, iter, PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type, -1);
  
  	if (type == PLACES_SEPARATOR) {
   		return TRUE;
	}

  	return FALSE;
}


static void
volume_mounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			 GnomeVFSVolume *volume,
			 NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_unmounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			   GnomeVFSVolume *volume,
			   NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
row_activated_callback (GtkTreeView *tree_view,
			GtkTreePath *path,
			GtkTreeViewColumn *column,
			gpointer user_data)
{
	NautilusPlacesSidebar *sidebar;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	
	sidebar = NAUTILUS_PLACES_SIDEBAR (user_data);
	model = gtk_tree_view_get_model (tree_view);
	
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return;
	}

	gtk_tree_model_get 
		(model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);
	
	if (uri != NULL) {
		/* Navigate to the clicked location. */
		nautilus_window_info_open_location
			(sidebar->window, 
			 uri, NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE, 0, NULL);
		g_free (uri);
	}
}

static void
update_click_policy (NautilusPlacesSidebar *sidebar)
{
	int policy;
	
	policy = eel_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY);
	
	eel_gtk_tree_view_set_activate_on_single_click
		(sidebar->tree_view, policy == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
click_policy_changed_callback (gpointer user_data)
{
	NautilusPlacesSidebar *sidebar;
	
	sidebar = NAUTILUS_PLACES_SIDEBAR (user_data);

	update_click_policy (sidebar);
}

static void
desktop_location_changed_callback (gpointer user_data)
{
	NautilusPlacesSidebar *sidebar;
	
	sidebar = NAUTILUS_PLACES_SIDEBAR (user_data);

	update_places (sidebar);
}

static void
loading_uri_callback (NautilusWindowInfo *window,
		      char *location,
		      NautilusPlacesSidebar *sidebar)
{
	GtkTreeSelection *selection;
	GtkTreeIter 	 iter;
	gboolean 	 valid;
	char  		 *uri;

        if (strcmp (sidebar->uri, location) != 0) {
		g_free (sidebar->uri);
                sidebar->uri = g_strdup (location);
  
		/* set selection if any place matches location */
		selection = gtk_tree_view_get_selection (sidebar->tree_view);
		gtk_tree_selection_unselect_all (selection);
  		valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);

		while (valid) {
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter, 
		 		       	    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
			if (uri != NULL) {
				if (strcmp (uri, location) == 0) {
					g_free (uri);
					gtk_tree_selection_select_iter (selection, &iter);
					break;
				}
				g_free (uri);
			}
        	 	valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sidebar->store), &iter);
		}
    	}
}


static void
nautilus_places_sidebar_init (NautilusPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeSelection  *selection;
	GtkWidget 	  *swin;
   	GtkWidget 	  *vbox;
  	
	vbox = gtk_vbox_new (FALSE, 6);
  	gtk_widget_show (vbox);

 	/* Scrolled window */
	swin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
		  		        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				             GTK_SHADOW_IN);
	gtk_widget_show (swin);
  	gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);


  	/* tree view */
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());
	
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "pixbuf", PLACES_SIDEBAR_COLUMN_ICON,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_NAME,
					     NULL);

	gtk_tree_view_set_row_separator_func (tree_view,
					      shortcuts_row_separator_func,
					      NULL,
					      NULL);

	gtk_tree_view_column_set_fixed_width (col, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_tree_view_append_column (tree_view, col);
	
	sidebar->store = gtk_list_store_new (PLACES_SIDEBAR_COLUMN_COUNT,
					     G_TYPE_INT, 
					     G_TYPE_STRING,
					     G_TYPE_STRING,
					     GDK_TYPE_PIXBUF
					     );

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store));
	gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sidebar), GTK_WIDGET (vbox));
	gtk_widget_show (GTK_WIDGET (sidebar));
	sidebar->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	g_signal_connect_object
		(tree_view, "row_activated", 
		 G_CALLBACK (row_activated_callback), sidebar, 0);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
				      click_policy_changed_callback,
				      sidebar);
	update_click_policy (sidebar);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
						  desktop_location_changed_callback,
						  sidebar,
						  G_OBJECT (sidebar));
}

static void
nautilus_places_sidebar_finalize (GObject *object)
{
	NautilusPlacesSidebar *sidebar;
	
	sidebar = NAUTILUS_PLACES_SIDEBAR (object);

	g_free (sidebar->uri);
	sidebar->uri = NULL;

	if (sidebar->store != NULL) {
		g_object_unref (sidebar->store);
		sidebar->store = NULL;
	}

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					 click_policy_changed_callback,
					 sidebar);

	G_OBJECT_CLASS (nautilus_places_sidebar_parent_class)->finalize (object);
}

static void
nautilus_places_sidebar_class_init (NautilusPlacesSidebarClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_places_sidebar_finalize;
}

static const char *
nautilus_places_sidebar_get_sidebar_id (NautilusSidebar *sidebar)
{
	return NAUTILUS_PLACES_SIDEBAR_ID;
}

static char *
nautilus_places_sidebar_get_tab_label (NautilusSidebar *sidebar)
{
	return g_strdup (_("Places"));
}

static char *
nautilus_places_sidebar_get_tab_tooltip (NautilusSidebar *sidebar)
{
	return g_strdup (_("Show Places"));
}

static GdkPixbuf *
nautilus_places_sidebar_get_tab_icon (NautilusSidebar *sidebar)
{
	return NULL;
}

static void
nautilus_places_sidebar_is_visible_changed (NautilusSidebar *sidebar,
					     gboolean         is_visible)
{
	/* Do nothing */
}

static void
nautilus_places_sidebar_iface_init (NautilusSidebarIface *iface)
{
	iface->get_sidebar_id = nautilus_places_sidebar_get_sidebar_id;
	iface->get_tab_label = nautilus_places_sidebar_get_tab_label;
	iface->get_tab_tooltip = nautilus_places_sidebar_get_tab_tooltip;
	iface->get_tab_icon = nautilus_places_sidebar_get_tab_icon;
	iface->is_visible_changed = nautilus_places_sidebar_is_visible_changed;
}

static void
nautilus_places_sidebar_set_parent_window (NautilusPlacesSidebar *sidebar,
					    NautilusWindowInfo *window)
{	
	GnomeVFSVolumeMonitor *volume_monitor;

	sidebar->window = window;
	
	sidebar->bookmarks = nautilus_window_info_get_bookmark_list (window);
	sidebar->uri = nautilus_window_info_get_current_location (window);

	g_signal_connect_object (sidebar->bookmarks, "contents_changed",
				 G_CALLBACK (update_places),
				 sidebar, G_CONNECT_SWAPPED);

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback),
				 sidebar, 0);
			 
	volume_monitor = gnome_vfs_get_volume_monitor ();
	
	g_signal_connect_object (volume_monitor, "volume_mounted",
				 G_CALLBACK (volume_mounted_callback), sidebar, 0);
	g_signal_connect_object (volume_monitor, "volume_unmounted",
				 G_CALLBACK (volume_unmounted_callback), sidebar, 0);

	update_places (sidebar);
}

static NautilusSidebar *
nautilus_places_sidebar_create (NautilusSidebarProvider *provider,
				 NautilusWindowInfo *window)
{
	NautilusPlacesSidebar *sidebar;
	
	sidebar = g_object_new (nautilus_places_sidebar_get_type (), NULL);
	nautilus_places_sidebar_set_parent_window (sidebar, window);
	g_object_ref (sidebar);
	gtk_object_sink (GTK_OBJECT (sidebar));

	return NAUTILUS_SIDEBAR (sidebar);
}

static void 
sidebar_provider_iface_init (NautilusSidebarProviderIface *iface)
{
	iface->create = nautilus_places_sidebar_create;
}

static void
nautilus_places_sidebar_provider_init (NautilusPlacesSidebarProvider *sidebar)
{
}

static void
nautilus_places_sidebar_provider_class_init (NautilusPlacesSidebarProviderClass *class)
{
}

void
nautilus_places_sidebar_register (void)
{
        nautilus_module_add_type (nautilus_places_sidebar_provider_get_type ());
}

