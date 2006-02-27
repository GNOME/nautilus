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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkimagemenuitem.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-operations.h>
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

	/* DnD */
	GList     *drag_list;
	gboolean  drag_data_received;
	gboolean  drop_occured;

	GtkWidget *popup_menu;
	GtkWidget *popup_menu_remove_item;
	GtkWidget *popup_menu_rename_item;
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
	PLACES_SIDEBAR_COLUMN_INDEX,
	
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

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry drop_targets [] = {
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST }
};

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebar, nautilus_places_sidebar, GTK_TYPE_SCROLLED_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR,
						nautilus_places_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebarProvider, nautilus_places_sidebar_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR_PROVIDER,
						sidebar_provider_iface_init));


static GtkTreeIter
add_place (GtkListStore *store, PlaceType place_type,
	   const char *name, const char *icon, const char *uri, const int index)
{
	GdkPixbuf            *pixbuf;
	GtkTreeIter           iter;

	pixbuf = nautilus_icon_factory_get_pixbuf_from_name_with_stock_size (icon, NULL, GTK_ICON_SIZE_MENU, NULL);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, index,
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
		char *display_name;

		mount_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
		display_name = g_filename_display_basename (g_get_home_dir ());
		last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
				       display_name, "gnome-fs-home", mount_uri, 0);
		g_free (display_name);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}	
		g_free (mount_uri);
	}

	mount_uri = gnome_vfs_get_uri_from_local_path (desktop_path);
	last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
			       _("Desktop"), "gnome-fs-desktop", mount_uri, 0);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}	
	g_free (mount_uri);
	g_free (desktop_path);
	
 	mount_uri = "file:///"; /* No need to strdup */
	last_iter = add_place (sidebar->store, PLACES_BUILT_IN,
			       _("File System"), "gnome-dev-harddisk", mount_uri, 0);
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
				       name, icon, mount_uri, 0);
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

		if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}

		name = nautilus_bookmark_get_name (bookmark);
		icon = nautilus_bookmark_get_icon (bookmark);
		mount_uri = nautilus_bookmark_get_uri (bookmark);
		last_iter = add_place (sidebar->store, PLACES_BOOKMARK,
				       name, icon, mount_uri, index);
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


static unsigned int
get_bookmark_index (GtkTreeView *tree_view)
{
	GtkTreeModel *model;
	GtkTreePath *p;
	GtkTreeIter iter;
	PlaceType place_type;
	int bookmark_index;

	model = gtk_tree_view_get_model (tree_view);

	bookmark_index = -1;

	/* find separator */
	p = gtk_tree_path_new_first ();
	while (p != NULL) {
		gtk_tree_model_get_iter (model, &iter, p);
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    -1);

		if (place_type == PLACES_SEPARATOR) {
			bookmark_index = *gtk_tree_path_get_indices (p) + 1;
			break;
		}

		gtk_tree_path_next (p);
	}
	gtk_tree_path_free (p);

	g_assert (bookmark_index >= 0);

	return bookmark_index;
}

/* Computes the appropriate row and position for dropping */
static void
compute_drop_position (GtkTreeView *tree_view,
				 int                      x,
				 int                      y,
				 GtkTreePath            **path,
				 GtkTreeViewDropPosition *pos,
				 NautilusPlacesSidebar *sidebar)
{
	int bookmarks_index;
	int num_bookmarks;
	int row;
	
	bookmarks_index = get_bookmark_index (tree_view);
	
	num_bookmarks = nautilus_bookmark_list_length (sidebar->bookmarks);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
					   x,
					   y,
					   path,
					   pos)) {
		row = bookmarks_index + num_bookmarks - 1;
		*path = gtk_tree_path_new_from_indices (row, -1);
		*pos = GTK_TREE_VIEW_DROP_AFTER;
		return;
	}
	
	row = *gtk_tree_path_get_indices (*path);
	gtk_tree_path_free (*path);
	
	if (row < bookmarks_index) {
		/* Hardcoded shortcuts can only be dragged into */
		*pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	}
	else if (row > bookmarks_index + num_bookmarks - 1) {
		row = bookmarks_index + num_bookmarks - 1;
		*pos = GTK_TREE_VIEW_DROP_AFTER;
	}

	*path = gtk_tree_path_new_from_indices (row, -1);
}


static void
get_drag_data (GtkTreeView *tree_view,
	       GdkDragContext *context, 
	       unsigned int time)
{
	GdkAtom target;
	
	target = gtk_drag_dest_find_target (GTK_WIDGET (tree_view), 
					    context, 
					    NULL);

	gtk_drag_get_data (GTK_WIDGET (tree_view),
			   context, target, time);
}

static void
free_drag_data (NautilusPlacesSidebar *sidebar)
{
	sidebar->drag_data_received = FALSE;

	if (sidebar->drag_list) {
		nautilus_drag_destroy_selection_list (sidebar->drag_list);
		sidebar->drag_list = NULL;
	}
}

static gboolean
can_accept_file_as_bookmark (NautilusFile *file)
{
	return nautilus_file_is_directory (file);
}

static gboolean
can_accept_items_as_bookmarks (const GList *items)
{
	int max;
	char *uri;
	NautilusFile *file;

	/* Iterate through selection checking if item will get accepted as a bookmark.
	 * If more than 100 items selected, return an over-optimistic result.
	 */
	for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
		uri = ((NautilusDragSelectionItem *)items->data)->uri;
		file = nautilus_file_get (uri);
		if (!can_accept_file_as_bookmark (file)) {
			nautilus_file_unref (file);
			return FALSE;
		}
		nautilus_file_unref (file);
	}
	
	return TRUE;
}

static gboolean
drag_motion_callback (GtkTreeView *tree_view,
		      GdkDragContext *context,
		      int x,
		      int y,
		      unsigned int time,
		      NautilusPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
	int action;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;

	if (!sidebar->drag_data_received) {
		get_drag_data (tree_view, context, time);
	}

	compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

	if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    pos == GTK_TREE_VIEW_DROP_AFTER ||
	    sidebar->drag_list == NULL) {
		if (can_accept_items_as_bookmarks (sidebar->drag_list)) {
			action = GDK_ACTION_COPY;
		} else {
			action = 0;
		}
	} else {
		model = gtk_tree_view_get_model (tree_view);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    -1);
		nautilus_drag_default_drop_action_for_icons (context, uri,
							     sidebar->drag_list,
							     &action);
		g_free (uri);
	}

	gtk_tree_view_set_drag_dest_row (tree_view, path, pos);
	gtk_tree_path_free (path);
	g_signal_stop_emission_by_name (tree_view, "drag-motion");

	gdk_drag_status (context, action, time);

	return TRUE;
}

static void
drag_leave_callback (GtkTreeView *tree_view,
		     GdkDragContext *context,
		     unsigned int time,
		     NautilusPlacesSidebar *sidebar)
{
	free_drag_data (sidebar);
	gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
	g_signal_stop_emission_by_name (tree_view, "drag-leave");
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
bookmarks_drop_uris (NautilusPlacesSidebar *sidebar,
		     const char            *data,
		     int                    position)
{
	NautilusBookmark *bookmark;
	NautilusFile *file;
	char *uri, *name, *name_truncated;
	char **uris;
	int i;
	
	uris = g_uri_list_extract_uris (data);
	
	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		file = nautilus_file_get (uri);

		if (!can_accept_file_as_bookmark (file)) {
			nautilus_file_unref (file);
			continue;
		}

		uri = nautilus_file_get_drop_target_uri (file);
		nautilus_file_unref (file);

		name = nautilus_compute_title_for_uri (uri);
		name_truncated = eel_truncate_text_for_menu_item (name);	

		bookmark = nautilus_bookmark_new_with_icon (uri, name_truncated,
							    FALSE, "gnome-fs-directory");
		
		if (!nautilus_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
			nautilus_bookmark_list_insert_item (sidebar->bookmarks, bookmark, position++);
		}

		g_object_unref (bookmark);
		g_free (name_truncated);
		g_free (name);
		g_free (uri);
	}

	g_strfreev (uris);
}

static GList *
uri_list_from_selection (GList *selection)
{
	NautilusDragSelectionItem *item;
	GList *ret;
	GList *l;
	
	ret = NULL;
	for (l = selection; l != NULL; l = l->next) {
		item = l->data;
		ret = g_list_prepend (ret, item->uri);
	}
	
	return g_list_reverse (ret);
}

static GList*
build_selection_list (const char *data)
{
	NautilusDragSelectionItem *item;
	GList *result;
	char **uris;
	char *uri;
	int i;

	uris = g_uri_list_extract_uris (data);

	result = NULL;
	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		item = nautilus_drag_selection_item_new ();
		item->uri = g_strdup (uri);
		item->got_icon_position = FALSE;
		result = g_list_prepend (result, item);
	}

	g_strfreev (uris);

	return g_list_reverse (result);
}

static void
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *selection_data,
			     unsigned int info,
			     unsigned int time,
			     NautilusPlacesSidebar *sidebar)
{
	GtkTreeView *tree_view;
	GtkTreePath *tree_path;
	GtkTreeViewDropPosition tree_pos;
	GtkTreeIter iter;
	int position, bookmarks_index;
	GtkTreeModel *model;
	char *drop_uri;
	GList *selection_list, *uris;
	gboolean success;

	tree_view = GTK_TREE_VIEW (widget);


	if (!sidebar->drag_data_received) {
		if (selection_data->target != GDK_NONE) {
			sidebar->drag_list = build_selection_list (selection_data->data);
		} else {
			sidebar->drag_list = NULL;
		}
		sidebar->drag_data_received = TRUE;
	}

	g_signal_stop_emission_by_name (widget, "drag-data-received");

	if (!sidebar->drop_occured) {
		return;
	}

	/* Compute position */
	compute_drop_position (tree_view, x, y, &tree_path, &tree_pos, sidebar);

	success = FALSE;

	if (tree_pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
		/* bookmark addition requested */
		bookmarks_index = get_bookmark_index (tree_view);
		position = *gtk_tree_path_get_indices (tree_path);
	
		if (tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
			position++;
		}
	
		g_assert (position >= bookmarks_index);
		position -= bookmarks_index;
	
		switch (info) {
		case NAUTILUS_ICON_DND_URI_LIST:
			bookmarks_drop_uris (sidebar, selection_data->data, position);
			success = TRUE;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		/* file transfer requested */
		if (context->action == GDK_ACTION_ASK) {
			context->action =
				nautilus_drag_drop_action_ask (GTK_WIDGET (tree_view),
							       context->actions);
		}

		if (context->action > 0) {
			model = gtk_tree_view_get_model (tree_view);

			gtk_tree_model_get_iter (model, &iter, tree_path);
			gtk_tree_model_get (model, &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &drop_uri,
					    -1);

			switch (info) {
			case NAUTILUS_ICON_DND_URI_LIST:
				selection_list = build_selection_list (selection_data->data);
				uris = uri_list_from_selection (selection_list);
				nautilus_file_operations_copy_move (uris, NULL, drop_uri,
								    context->action, GTK_WIDGET (tree_view),
								    NULL, NULL);
				nautilus_drag_destroy_selection_list (selection_list);
				g_list_free (uris);
				success = TRUE;
				break;
			default:
				g_assert_not_reached ();
				break;
			}

			g_free (drop_uri);
		}
	}

	sidebar->drop_occured = FALSE;
	free_drag_data (sidebar);
	gtk_drag_finish (context, success, FALSE, time);

	gtk_tree_path_free (tree_path);

}

static gboolean
drag_drop_callback (GtkTreeView *tree_view,
		    GdkDragContext *context,
		    int x,
		    int y,
		    unsigned int time,
		    NautilusPlacesSidebar *sidebar)
{
	sidebar->drop_occured = TRUE;
	get_drag_data (tree_view, context, time);
	g_signal_stop_emission_by_name (tree_view, "drag-drop");
	return TRUE;
}

/* Callback used when the file list's popup menu is detached */
static void
bookmarks_popup_menu_detach_cb (GtkWidget *attach_widget,
				GtkMenu   *menu)
{
	NautilusPlacesSidebar *sidebar;
	
	sidebar = NAUTILUS_PLACES_SIDEBAR (attach_widget);
	g_assert (NAUTILUS_IS_PLACES_SIDEBAR (sidebar));
	
	sidebar->popup_menu = NULL;
	sidebar->popup_menu_remove_item = NULL;
	sidebar->popup_menu_rename_item = NULL;
}

static void
bookmarks_check_popup_sensitivity (NautilusPlacesSidebar *sidebar)
{
  GtkTreeIter iter;
  PlaceType type; 
  GtkTreeSelection *selection;

  type = PLACES_BUILT_IN;

  if (sidebar->popup_menu == NULL)
    return;

  selection = gtk_tree_view_get_selection (sidebar->tree_view);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
 	  gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			      PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			      -1);
  }

  gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
  gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
				NautilusPlacesSidebar *sidebar)
{
	bookmarks_check_popup_sensitivity (sidebar);
}


/* Rename the selected bookmark */
static void
rename_selected_bookmark (NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GList *renderers;
	GtkTreeSelection *selection;
	
	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
		renderers = gtk_tree_view_column_get_cell_renderers (column);
		cell = g_list_nth_data (renderers, 1);
		g_list_free (renderers);
		g_object_set (cell, "editable", TRUE, NULL);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
						path, column, cell, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
		    NautilusPlacesSidebar *sidebar)
{
	rename_selected_bookmark (sidebar);
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	int index;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}
	
	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_INDEX, &index,
			    -1);

	nautilus_bookmark_list_delete_item_at (sidebar->bookmarks, index);
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
		    NautilusPlacesSidebar *sidebar)
{
	remove_selected_bookmarks (sidebar);
}


/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
bookmarks_key_press_event_cb (GtkWidget             *widget,
			      GdkEventKey           *event,
			      NautilusPlacesSidebar *sidebar)
{
  guint modifiers;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  if ((event->keyval == GDK_BackSpace
      || event->keyval == GDK_Delete
      || event->keyval == GDK_KP_Delete)
      && (event->state & modifiers) == 0) {
      remove_selected_bookmarks (sidebar);
      return TRUE;
  }

  if ((event->keyval == GDK_F2)
      && (event->state & modifiers) == 0) {
      rename_selected_bookmark (sidebar);
      return TRUE;
  }

  return FALSE;
}

/* Constructs the popup menu for the file list if needed */
static void
bookmarks_build_popup_menu (NautilusPlacesSidebar *sidebar)
{
	GtkWidget *item;
	
	if (sidebar->popup_menu) {
		return;
	}
	
	sidebar->popup_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (sidebar->popup_menu),
			           GTK_WIDGET (sidebar),
			           bookmarks_popup_menu_detach_cb);
	
	item = gtk_image_menu_item_new_with_label (_("Remove"));
	sidebar->popup_menu_remove_item = item;
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		    G_CALLBACK (remove_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);
	
	item = gtk_menu_item_new_with_label (_("Rename..."));
	sidebar->popup_menu_rename_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rename_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);
	
	bookmarks_check_popup_sensitivity (sidebar);
}

static void
bookmarks_update_popup_menu (NautilusPlacesSidebar *sidebar)
{
	bookmarks_build_popup_menu (sidebar);  
}

static void
bookmarks_popup_menu (NautilusPlacesSidebar *sidebar,
		      GdkEventButton        *event)
{
	bookmarks_update_popup_menu (sidebar);
	eel_pop_up_context_menu (GTK_MENU(sidebar->popup_menu),
			      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
			      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
			      event);
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
			 NautilusPlacesSidebar *sidebar)
{
	bookmarks_popup_menu (sidebar, NULL);
	return TRUE;
}

/* Callback used when a button is pressed on the shortcuts list.  
 * We trap button 3 to bring up a popup menu.
 */
static gboolean
bookmarks_button_press_event_cb (GtkWidget             *widget,
				 GdkEventButton        *event,
				 NautilusPlacesSidebar *sidebar)
{
	if (event->button == 3) {
		bookmarks_popup_menu (sidebar, event);
	}
	return FALSE;
}


static void
bookmarks_edited (GtkCellRenderer       *cell,
		  gchar                 *path_string,
		  gchar                 *new_text,
		  NautilusPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	NautilusBookmark *bookmark;
	int index;

	g_object_set (cell, "editable", FALSE, NULL);
	
	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
		            PLACES_SIDEBAR_COLUMN_INDEX, &index,
		            -1);
	gtk_tree_path_free (path);
	bookmark = nautilus_bookmark_list_item_at (sidebar->bookmarks, index);

	if (bookmark != NULL) {
		nautilus_bookmark_set_has_custom_name (bookmark, TRUE);
		nautilus_bookmark_set_name (bookmark, new_text);
	}
}

static void
bookmarks_editing_canceled (GtkCellRenderer       *cell,
			    NautilusPlacesSidebar *sidebar)
{
	g_object_set (cell, "editable", FALSE, NULL);
}

static void
nautilus_places_sidebar_init (NautilusPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeSelection  *selection;

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	
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

	g_signal_connect (cell, "edited", 
			  G_CALLBACK (bookmarks_edited), sidebar);
	g_signal_connect (cell, "editing-canceled", 
			  G_CALLBACK (bookmarks_editing_canceled), sidebar);

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
					     GDK_TYPE_PIXBUF,
					     G_TYPE_INT
					     );

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store));
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));

	gtk_widget_show (GTK_WIDGET (sidebar));
	sidebar->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	g_signal_connect_object
		(tree_view, "row_activated", 
		 G_CALLBACK (row_activated_callback), sidebar, 0);

	gtk_drag_dest_set (GTK_WIDGET (tree_view), 0,
			   drop_targets, G_N_ELEMENTS (drop_targets),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

	g_signal_connect (tree_view, "key-press-event",
			  G_CALLBACK (bookmarks_key_press_event_cb), sidebar);
	g_signal_connect (tree_view, "drag-motion",
			  G_CALLBACK (drag_motion_callback), sidebar);
	g_signal_connect (tree_view, "drag-leave",
			  G_CALLBACK (drag_leave_callback), sidebar);
	g_signal_connect (tree_view, "drag-data-received",
			  G_CALLBACK (drag_data_received_callback), sidebar);
	g_signal_connect (tree_view, "drag-drop",
			  G_CALLBACK (drag_drop_callback), sidebar);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (bookmarks_selection_changed_cb), sidebar);
	g_signal_connect (tree_view, "popup-menu",
			  G_CALLBACK (bookmarks_popup_menu_cb), sidebar);
	g_signal_connect (tree_view, "button-press-event",
			  G_CALLBACK (bookmarks_button_press_event_cb), sidebar);

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

	free_drag_data (sidebar);

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

