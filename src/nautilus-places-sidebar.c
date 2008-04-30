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
#include <eel/eel-stock-dialogs.h>
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
#include <glib/gi18n.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <libnautilus-private/nautilus-debug-log.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-sidebar-provider.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-autorun.h>
#include <gio/gio.h>

#include "nautilus-bookmark-list.h"
#include "nautilus-places-sidebar.h"
#include "nautilus-window.h"

#define NAUTILUS_PLACES_SIDEBAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebarClass))
#define NAUTILUS_IS_PLACES_SIDEBAR(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_IS_PLACES_SIDEBAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR))

typedef struct {
	GtkScrolledWindow  parent;
	GtkTreeView        *tree_view;
	char 	           *uri;
	GtkListStore       *store;
	GtkTreeModel       *filter_model;
	NautilusWindowInfo *window;
	NautilusBookmarkList *bookmarks;
	GVolumeMonitor *volume_monitor;

	/* DnD */
	GList     *drag_list;
	gboolean  drag_data_received;
	gboolean  drop_occured;

	GtkWidget *popup_menu;
	GtkWidget *popup_menu_remove_item;
	GtkWidget *popup_menu_rename_item;
	GtkWidget *popup_menu_separator_item;
	GtkWidget *popup_menu_mount_item;
	GtkWidget *popup_menu_unmount_item;
	GtkWidget *popup_menu_eject_item;
	GtkWidget *popup_menu_rescan_item;
	GtkWidget *popup_menu_format_item;
	GtkWidget *popup_menu_empty_trash_item;
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
	PLACES_SIDEBAR_COLUMN_DRIVE,
	PLACES_SIDEBAR_COLUMN_VOLUME,
	PLACES_SIDEBAR_COLUMN_MOUNT,
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
static void  open_selected_bookmark                    (NautilusPlacesSidebar        *sidebar,
							GtkTreeModel                 *model,
							GtkTreePath                  *path,
							gboolean                      open_in_new_window);
static void  nautilus_places_sidebar_style_set         (GtkWidget                    *widget,
							GtkStyle                     *previous_style);

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry nautilus_shortcuts_source_targets[] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry nautilus_shortcuts_drop_targets [] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
	{ "text/uri-list", 0, TEXT_URI_LIST }
};

/* Drag and drop interface declarations */
typedef struct {
  GtkTreeModelFilter parent;

  NautilusPlacesSidebar *sidebar;
} NautilusShortcutsModelFilter;

typedef struct {
  GtkTreeModelFilterClass parent_class;
} NautilusShortcutsModelFilterClass;

#define NAUTILUS_SHORTCUTS_MODEL_FILTER_TYPE (_nautilus_shortcuts_model_filter_get_type ())
#define NAUTILUS_SHORTCUTS_MODEL_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_SHORTCUTS_MODEL_FILTER_TYPE, NautilusShortcutsModelFilter))

GType _nautilus_shortcuts_model_filter_get_type (void);
static void nautilus_shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusShortcutsModelFilter,
			 _nautilus_shortcuts_model_filter,
			 GTK_TYPE_TREE_MODEL_FILTER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						nautilus_shortcuts_model_filter_drag_source_iface_init));

static GtkTreeModel *nautilus_shortcuts_model_filter_new (NautilusPlacesSidebar *sidebar,
							  GtkTreeModel          *child_model,
							  GtkTreePath           *root);

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebar, nautilus_places_sidebar, GTK_TYPE_SCROLLED_WINDOW,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR,
						nautilus_places_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesSidebarProvider, nautilus_places_sidebar_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SIDEBAR_PROVIDER,
						sidebar_provider_iface_init));

static GtkTreeIter
add_place (NautilusPlacesSidebar *sidebar,
	   PlaceType place_type,
	   const char *name,
	   GIcon *icon,
	   const char *uri,
	   GDrive *drive,
	   GVolume *volume,
	   GMount *mount,
	   const int index)
{
	GdkPixbuf            *pixbuf;
	GtkTreeIter           iter, child_iter;
	NautilusIconInfo *icon_info;
	int icon_size;

	icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	icon_info = nautilus_icon_info_lookup (icon, icon_size);

	pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon_info, icon_size);
	g_object_unref (icon_info);
	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_DRIVE, drive,
			    PLACES_SIDEBAR_COLUMN_VOLUME, volume,
			    PLACES_SIDEBAR_COLUMN_MOUNT, mount,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, index,
			    -1);
	if (pixbuf != NULL) {
		g_object_unref (pixbuf);
	}
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->filter_model));
	gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (sidebar->filter_model),
							  &child_iter,
							  &iter);
	return child_iter;
}

static void
update_places (NautilusPlacesSidebar *sidebar)
{
	NautilusBookmark *bookmark;
	GtkTreeSelection *selection;
	GtkTreeIter iter, last_iter;
	GVolumeMonitor *volume_monitor;
	GList *mounts, *l, *ll;
	GMount *mount;
	GList *drives;
	GDrive *drive;
	GList *volumes;
	GVolume *volume;
	int bookmark_count, index;
	char *location, *mount_uri, *name, *desktop_path;
	GIcon *icon;
	GFile *root;
	
		
	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	gtk_list_store_clear (sidebar->store);
	location = nautilus_window_info_get_current_location (sidebar->window);

	/* add built in bookmarks */
	desktop_path = nautilus_get_desktop_directory ();

	if (strcmp (g_get_home_dir(), desktop_path) != 0) {
		char *display_name;

		mount_uri = nautilus_get_home_directory_uri ();
		display_name = g_filename_display_basename (g_get_home_dir ());
		icon = g_themed_icon_new (NAUTILUS_ICON_HOME);
		last_iter = add_place (sidebar, PLACES_BUILT_IN,
				       display_name, icon,
				       mount_uri, NULL, NULL, NULL, 0);
		g_object_unref (icon);
		g_free (display_name);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}	
		g_free (mount_uri);
	}

	mount_uri = g_filename_to_uri (desktop_path, NULL, NULL);
	icon = g_themed_icon_new (NAUTILUS_ICON_DESKTOP);
	last_iter = add_place (sidebar, PLACES_BUILT_IN,
			       _("Desktop"), icon,
			       mount_uri, NULL, NULL, NULL, 0);
	g_object_unref (icon);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}	
	g_free (mount_uri);
	g_free (desktop_path);
	
 	mount_uri = "file:///"; /* No need to strdup */
	icon = g_themed_icon_new (NAUTILUS_ICON_FILESYSTEM);
	last_iter = add_place (sidebar, PLACES_BUILT_IN,
			       _("File System"), icon,
			       mount_uri, NULL, NULL, NULL, 0);
	g_object_unref (icon);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}

 	mount_uri = "network:///"; /* No need to strdup */
	icon = g_themed_icon_new (NAUTILUS_ICON_NETWORK);
	last_iter = add_place (sidebar, PLACES_BUILT_IN,
			       _("Network"), icon,
			       mount_uri, NULL, NULL, NULL, 0);
	g_object_unref (icon);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}

	volume_monitor = sidebar->volume_monitor;

	/* first go through all connected drives */
	drives = g_volume_monitor_get_connected_drives (volume_monitor);
	for (l = drives; l != NULL; l = l->next) {
		drive = l->data;

		volumes = g_drive_get_volumes (drive);
		if (volumes != NULL) {
			for (ll = volumes; ll != NULL; ll = ll->next) {
				volume = ll->data;
				mount = g_volume_get_mount (volume);
				if (mount != NULL) {
					/* Show mounted volume in the sidebar */
					icon = g_mount_get_icon (mount);
					root = g_mount_get_root (mount);
					mount_uri = g_file_get_uri (root);
					g_object_unref (root);
					name = g_mount_get_name (mount);
					last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
							       name, icon, mount_uri,
							       drive, volume, mount, 0);
					if (strcmp (location, mount_uri) == 0) {
						gtk_tree_selection_select_iter (selection, &last_iter);
					}
					g_object_unref (mount);
					g_object_unref (icon);
					g_free (name);
					g_free (mount_uri);
				} else {
					/* Do show the unmounted volumes in the sidebar;
					 * this is so the user can mount it (in case automounting
					 * is off).
					 *
					 * Also, even if automounting is enabled, this gives a visual
					 * cue that the user should remember to yank out the media if
					 * he just unmounted it.
					 */
					icon = g_volume_get_icon (volume);
					name = g_volume_get_name (volume);
					last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
							       name, icon, NULL,
							       drive, volume, NULL, 0);
					g_object_unref (icon);
					g_free (name);
				}
				g_object_unref (volume);
			}
			g_list_free (volumes);
		} else {
			if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive)) {
				/* If the drive has no mountable volumes and we cannot detect media change.. we
				 * display the drive in the sidebar so the user can manually poll the drive by
				 * right clicking and selecting "Rescan..."
				 *
				 * This is mainly for drives like floppies where media detection doesn't
				 * work.. but it's also for human beings who like to turn off media detection
				 * in the OS to save battery juice.
				 */
				icon = g_drive_get_icon (drive);
				name = g_drive_get_name (drive);
				last_iter = add_place (sidebar, PLACES_BUILT_IN,
						       name, icon, NULL,
						       drive, NULL, NULL, 0);
				g_object_unref (icon);
				g_free (name);
			}
		}
		g_object_unref (drive);
	}
	g_list_free (drives);

	/* add all volumes that is not associated with a drive */
	volumes = g_volume_monitor_get_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		drive = g_volume_get_drive (volume);
		if (drive != NULL) {
		    	g_object_unref (volume);
			g_object_unref (drive);
			continue;
		}
		mount = g_volume_get_mount (volume);
		if (mount != NULL) {
			icon = g_mount_get_icon (mount);
			root = g_mount_get_root (mount);
			mount_uri = g_file_get_uri (root);
			g_object_unref (root);
			name = g_mount_get_name (mount);
			last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
					       name, icon, mount_uri,
					       NULL, volume, mount, 0);
			if (strcmp (location, mount_uri) == 0) {
				gtk_tree_selection_select_iter (selection, &last_iter);
			}
			g_object_unref (mount);
			g_object_unref (icon);
			g_free (name);
			g_free (mount_uri);
		} else {
			/* see comment above in why we add an icon for an unmounted mountable volume */
			icon = g_volume_get_icon (volume);
			name = g_volume_get_name (volume);
			last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
					       name, icon, NULL,
					       NULL, volume, NULL, 0);
			g_object_unref (icon);
			g_free (name);
		}
		g_object_unref (volume);
	}
	g_list_free (volumes);

	/* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
	mounts = g_volume_monitor_get_mounts (volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		volume = g_mount_get_volume (mount);
		if (volume != NULL) {
		    	g_object_unref (volume);
			g_object_unref (mount);
			continue;
		}
		icon = g_mount_get_icon (mount);
		root = g_mount_get_root (mount);
		mount_uri = g_file_get_uri (root);
		g_object_unref (root);
		name = g_mount_get_name (mount);
		last_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
				       name, icon, mount_uri,
				       NULL, NULL, mount, 0);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}
		g_object_unref (mount);
		g_object_unref (icon);
		g_free (name);
		g_free (mount_uri);
	}
	g_list_free (mounts);

	mount_uri = "trash:///"; /* No need to strdup */
	icon = nautilus_trash_monitor_get_icon ();
	last_iter = add_place (sidebar, PLACES_BUILT_IN,
			       _("Trash"), icon, mount_uri,
			       NULL, NULL, NULL, 0);
	if (strcmp (location, mount_uri) == 0) {
		gtk_tree_selection_select_iter (selection, &last_iter);
	}
	g_object_unref (icon);

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
		last_iter = add_place (sidebar, PLACES_BOOKMARK,
				       name, icon, mount_uri,
				       NULL, NULL, NULL, index);
		if (strcmp (location, mount_uri) == 0) {
			gtk_tree_selection_select_iter (selection, &last_iter);
		}
		g_free (name);
		g_object_unref (icon);
		g_free (mount_uri);
	}
	g_free (location);
}

static gboolean
nautilus_shortcuts_row_separator_func (GtkTreeModel *model,
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
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
		       GVolume *volume,
		       NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
			     GDrive         *drive,
			     NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
			  GDrive         *drive,
			  NautilusPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
			GDrive         *drive,
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
	open_selected_bookmark (NAUTILUS_PLACES_SIDEBAR (user_data),
				gtk_tree_view_get_model (tree_view),
				path,
				FALSE);
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
  		valid = gtk_tree_model_get_iter_first (sidebar->filter_model, &iter);

		while (valid) {
			gtk_tree_model_get (sidebar->filter_model, &iter, 
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
        	 	valid = gtk_tree_model_iter_next (sidebar->filter_model, &iter);
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
		file = nautilus_file_get_by_uri (uri);
		if (!can_accept_file_as_bookmark (file)) {
			nautilus_file_unref (file);
			return FALSE;
		}
		nautilus_file_unref (file);
	}
	
	return TRUE;
}

static void
drag_data_delete_callback (GtkWidget             *widget,
			   GdkDragContext        *context,
			   NautilusPlacesSidebar *sidebar)
{
	g_signal_stop_emission_by_name (widget, "drag-data-delete");
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
	GtkTreeIter iter, child_iter;
	char *uri;

	if (!sidebar->drag_data_received) {
		get_drag_data (tree_view, context, time);
	}

	compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

	if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    pos == GTK_TREE_VIEW_DROP_AFTER ) {
		if (can_accept_items_as_bookmarks (sidebar->drag_list)) {
			action = GDK_ACTION_COPY;
		} else {
			action = 0;
		}
	} else {
		if (sidebar->drag_list == NULL) {
			action = 0;
		} else {
			gtk_tree_model_get_iter (sidebar->filter_model,
						 &iter, path);
			gtk_tree_model_filter_convert_iter_to_child_iter (
				GTK_TREE_MODEL_FILTER (sidebar->filter_model),
				&child_iter, &iter);
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
					    &child_iter,
					    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
			nautilus_drag_default_drop_action_for_icons (context, uri,
								     sidebar->drag_list,
								     &action);
			g_free (uri);
		}
	}

	gtk_tree_view_set_drag_dest_row (tree_view, path, pos);
	gtk_tree_path_free (path);
	g_signal_stop_emission_by_name (tree_view, "drag-motion");

	if (action != 0) {
		gdk_drag_status (context, action, time);
		return TRUE;
	} else {
		return FALSE;
	}
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
		     GtkSelectionData      *selection_data,
		     int                    position)
{
	NautilusBookmark *bookmark;
	NautilusFile *file;
	char *uri, *name;
	char **uris;
	int i;
	GFile *location;
	GIcon *icon;
	
	uris = gtk_selection_data_get_uris (selection_data);
	if (!uris)
		return;
	
	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		file = nautilus_file_get_by_uri (uri);

		if (!can_accept_file_as_bookmark (file)) {
			nautilus_file_unref (file);
			continue;
		}

		uri = nautilus_file_get_drop_target_uri (file);
		location = g_file_new_for_uri (uri);
		nautilus_file_unref (file);

		name = nautilus_compute_title_for_location (location);

		icon = g_themed_icon_new (NAUTILUS_ICON_FOLDER);
		bookmark = nautilus_bookmark_new_with_icon (location, name,
							    FALSE, icon);
		g_object_unref (icon);
		
		if (!nautilus_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
			nautilus_bookmark_list_insert_item (sidebar->bookmarks, bookmark, position++);
		}

		g_object_unref (location);
		g_object_unref (bookmark);
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

static gboolean
get_selected_iter (NautilusPlacesSidebar *sidebar,
		   GtkTreeIter *iter)
{
	GtkTreeSelection *selection;
	GtkTreeIter parent_iter;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	if (!gtk_tree_selection_get_selected (selection, NULL, &parent_iter)) {
		return FALSE;
	}
	gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (sidebar->filter_model),
							  iter,
							  &parent_iter);
	return TRUE;
}

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (NautilusPlacesSidebar *sidebar,
		   int                   new_position)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	NautilusBookmark *bookmark;
	int old_position;
	int bookmarks_index;

	/* Get the selected path */

	if (!get_selected_iter (sidebar, &iter))
		g_assert_not_reached ();

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
	old_position = *gtk_tree_path_get_indices (path);
	gtk_tree_path_free (path);

	bookmarks_index = get_bookmark_index (sidebar->tree_view);
	old_position -= bookmarks_index;
	if (old_position < 0) {
		return;
	}
	g_assert (old_position < nautilus_bookmark_list_length (sidebar->bookmarks));

	/* Remove the path from the old position and insert it in the new one */

	if (old_position == new_position) {
		return;
	}
	bookmark = nautilus_bookmark_list_item_at (sidebar->bookmarks, old_position);
	nautilus_bookmark_list_insert_item (sidebar->bookmarks, bookmark, new_position);
	if (old_position > new_position) {
		old_position++;
	}
	nautilus_bookmark_list_delete_item_at (sidebar->bookmarks, old_position);
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
		if (selection_data->target != GDK_NONE &&
		    info == TEXT_URI_LIST) {
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
		case TEXT_URI_LIST:
			bookmarks_drop_uris (sidebar, selection_data, position);
			success = TRUE;
			break;
		case GTK_TREE_MODEL_ROW:
			reorder_bookmarks (sidebar, position);
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
			case TEXT_URI_LIST:
				selection_list = build_selection_list (selection_data->data);
				uris = uri_list_from_selection (selection_list);
				nautilus_file_operations_copy_move (uris, NULL, drop_uri,
								    context->action, GTK_WIDGET (tree_view),
								    NULL, NULL);
				nautilus_drag_destroy_selection_list (selection_list);
				g_list_free (uris);
				success = TRUE;
				break;
			case GTK_TREE_MODEL_ROW:
				success = FALSE;
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
	sidebar->popup_menu_separator_item = NULL;
	sidebar->popup_menu_mount_item = NULL;
	sidebar->popup_menu_unmount_item = NULL;
	sidebar->popup_menu_eject_item = NULL;
	sidebar->popup_menu_rescan_item = NULL;
	sidebar->popup_menu_format_item = NULL;
	sidebar->popup_menu_empty_trash_item = NULL;
}

static void
check_visibility (GMount           *mount,
		  GVolume          *volume,
		  GDrive           *drive,
		  gboolean         *show_mount,
		  gboolean         *show_unmount,
		  gboolean         *show_eject,
		  gboolean         *show_rescan,
		  gboolean         *show_format)
{
	*show_mount = FALSE;
	*show_unmount = FALSE;
	*show_eject = FALSE;
	*show_format = FALSE;
	*show_rescan = FALSE;

	if (drive != NULL) {
		*show_eject = g_drive_can_eject (drive);

		if (g_drive_is_media_removable (drive) &&
		    !g_drive_is_media_check_automatic (drive) && 
		    g_drive_can_poll_for_media (drive))
			*show_rescan = TRUE;
	}

	if (volume != NULL) {
		*show_eject |= g_volume_can_eject (volume);
		if (mount == NULL)
			*show_mount = g_volume_can_mount (volume);
	}
	
	if (mount != NULL) {
		*show_unmount = g_mount_can_unmount (mount);
		*show_eject |= g_mount_can_eject (mount);
	}

#ifdef TODO_GIO
		if (something &&
		    g_find_program_in_path ("gfloppy")) {
			*show_format = TRUE;
		}
#endif		
}

static void
bookmarks_check_popup_sensitivity (NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type; 
	GDrive *drive = NULL;
	GVolume *volume = NULL;
	GMount *mount = NULL;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_rescan;
	gboolean show_format;
	gboolean show_empty_trash;
	char *uri = NULL;
	
	type = PLACES_BUILT_IN;

	if (sidebar->popup_menu == NULL) {
		return;
	}

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
 				    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    -1);
	}

	gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_empty_trash_item, !nautilus_trash_monitor_is_empty ());

 	check_visibility (mount, volume, drive,
 			  &show_mount, &show_unmount, &show_eject, &show_rescan, &show_format);

	/* We actually want both eject and unmount since eject will unmount all volumes. 
	 * TODO: hide unmount if the drive only has a single mountable volume 
	 */

	show_empty_trash = (uri != NULL) &&
			   (!strcmp (uri, "trash:///"));
	
	eel_gtk_widget_set_shown (sidebar->popup_menu_separator_item, 
			show_mount || show_unmount || show_eject || show_format || show_empty_trash);
	eel_gtk_widget_set_shown (sidebar->popup_menu_mount_item, show_mount);
	eel_gtk_widget_set_shown (sidebar->popup_menu_unmount_item, show_unmount);
	eel_gtk_widget_set_shown (sidebar->popup_menu_eject_item, show_eject);
	eel_gtk_widget_set_shown (sidebar->popup_menu_rescan_item, show_rescan);
	eel_gtk_widget_set_shown (sidebar->popup_menu_format_item, show_format);
	eel_gtk_widget_set_shown (sidebar->popup_menu_empty_trash_item, show_empty_trash);

	g_free (uri);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
				NautilusPlacesSidebar *sidebar)
{
	bookmarks_check_popup_sensitivity (sidebar);
}

static void
open_selected_bookmark (NautilusPlacesSidebar *sidebar,
			GtkTreeModel	      *model,
			GtkTreePath	      *path,
			gboolean	      open_in_new_window)
{
	GtkTreeIter iter;
	GFile *location;
	char *uri;

	if (!path) {
		return;
	}

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return;
	}

	gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

	if (uri != NULL) {
		nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
				    "activate from places sidebar window=%p: %s",
				    sidebar->window, uri);
		location = g_file_new_for_uri (uri);
		/* Navigate to the clicked location */
		if (!open_in_new_window) {
			nautilus_window_info_open_location (sidebar->window, location,
							    NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
							    0, NULL);
		} else {
			NautilusWindow *cur, *new;
			
			cur = NAUTILUS_WINDOW (sidebar->window);
			new = nautilus_application_create_navigation_window (cur->application,
									     NULL,
									     gtk_window_get_screen (GTK_WINDOW (cur)));
			nautilus_window_go_to (new, location);
		}
		g_object_unref (location);
		g_free (uri);

	} else {
		GVolume *volume;
		gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_VOLUME, &volume, -1);
		if (volume != NULL) {
			nautilus_file_operations_mount_volume (NULL, volume, FALSE);
			g_object_unref (volume);
		}
	}
}

static void
open_shortcut_from_menu (NautilusPlacesSidebar *sidebar,
			 gboolean	       open_in_new_window)
{
	GtkTreeModel *model;
	GtkTreePath *path;

	model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

	open_selected_bookmark (sidebar, model, path, open_in_new_window);

	gtk_tree_path_free (path);
}

static void
open_shortcut_cb (GtkMenuItem		*item,
		  NautilusPlacesSidebar	*sidebar)
{
	open_shortcut_from_menu (sidebar, FALSE);
}

static void
open_shortcut_in_new_window_cb (GtkMenuItem	      *item,
				NautilusPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, TRUE);
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
	
	if (get_selected_iter (sidebar, &iter)) {
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
	PlaceType type; 
	int index;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}
	
	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type != PLACES_BOOKMARK) {
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

static void
mount_shortcut_cb (GtkMenuItem           *item,
		   NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GVolume *volume;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    -1);

	if (volume != NULL) {
		nautilus_file_operations_mount_volume (NULL, volume, FALSE);
		g_object_unref (volume);
	}
}

static void
unmount_shortcut_cb (GtkMenuItem           *item,
		     NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;
	GVolume *volume;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    -1);

	if (mount != NULL) {
		GtkWidget *toplevel;
		
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (sidebar->tree_view));
		nautilus_file_operations_unmount_mount (GTK_WINDOW (toplevel),
							mount, FALSE, TRUE);
	}
	if (mount != NULL) {
		g_object_unref (mount);
	}
	if (volume != NULL) {
		g_object_unref (volume);
	}
}

static void
drive_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GError *error;
	char *primary;
	char *name;
	error = NULL;
	if (!g_drive_eject_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
				       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
volume_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GError *error;
	char *primary;
	char *name;
	error = NULL;
	if (!g_volume_eject_finish (G_VOLUME (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_volume_get_name (G_VOLUME (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
					       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
mount_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GError *error;
	char *primary;
	char *name;
	error = NULL;
	if (!g_mount_eject_finish (G_MOUNT (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_mount_get_name (G_MOUNT (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
					       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
eject_shortcut_cb (GtkMenuItem           *item,
		   NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (mount != NULL) {
		g_mount_eject (mount, 0, NULL, mount_eject_cb, NULL);
	} else if (volume != NULL) {
		g_volume_eject (volume, 0, NULL, volume_eject_cb, NULL);
	} else if (drive != NULL) {
		g_drive_eject (drive, 0, NULL, drive_eject_cb, NULL);
	}

	if (mount != NULL)
		g_object_unref (mount);
	if (volume != NULL)
		g_object_unref (volume);
	if (drive != NULL)
		g_object_unref (drive);
}

static void
drive_poll_for_media_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GError *error;
	char *primary;
	char *name;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
					       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
rescan_shortcut_cb (GtkMenuItem           *item,
		    NautilusPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, NULL);
	}
	g_object_unref (drive);
}

static void
format_shortcut_cb (GtkMenuItem           *item,
		    NautilusPlacesSidebar *sidebar)
{
	g_spawn_command_line_async ("gfloppy", NULL);
}

static void
empty_trash_cb (GtkMenuItem           *item,
		NautilusPlacesSidebar *sidebar)
{
	nautilus_file_operations_empty_trash (GTK_WIDGET (sidebar->window));
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
	
	item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				       gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_in_new_window_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

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
	
	/* Mount/Unmount/Eject menu items */

	sidebar->popup_menu_separator_item =
		GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

	item = gtk_menu_item_new_with_mnemonic (_("_Mount"));
	sidebar->popup_menu_mount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (mount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Unmount"));
	sidebar->popup_menu_unmount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (unmount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Eject"));
	sidebar->popup_menu_eject_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (eject_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Rescan"));
	sidebar->popup_menu_rescan_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rescan_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Format"));
	sidebar->popup_menu_format_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (format_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Empty Trash menu item */

	item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
	sidebar->popup_menu_empty_trash_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (empty_trash_cb), sidebar);
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
trash_state_changed_cb (NautilusTrashMonitor *trash_monitor,
			gboolean             state,
			gpointer             data)
{
	NautilusPlacesSidebar *sidebar;

	sidebar = NAUTILUS_PLACES_SIDEBAR (data);

	/* The trash icon changed, update the sidebar */
	update_places (sidebar);

	bookmarks_check_popup_sensitivity (sidebar);
}

static void
nautilus_places_sidebar_init (NautilusPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeSelection  *selection;

	sidebar->volume_monitor = g_volume_monitor_get ();
	
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
					      nautilus_shortcuts_row_separator_func,
					      NULL,
					      NULL);

	gtk_tree_view_column_set_fixed_width (col, NAUTILUS_ICON_SIZE_SMALLER);
	gtk_tree_view_append_column (tree_view, col);
	
	sidebar->store = gtk_list_store_new (PLACES_SIDEBAR_COLUMN_COUNT,
					     G_TYPE_INT, 
					     G_TYPE_STRING,
					     G_TYPE_DRIVE,
					     G_TYPE_VOLUME,
					     G_TYPE_MOUNT,
					     G_TYPE_STRING,
					     GDK_TYPE_PIXBUF,
					     G_TYPE_INT
					     );
	sidebar->filter_model = nautilus_shortcuts_model_filter_new (sidebar,
								     GTK_TREE_MODEL (sidebar->store),
								     NULL);

	gtk_tree_view_set_model (tree_view, sidebar->filter_model);
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));

	gtk_widget_show (GTK_WIDGET (sidebar));
	sidebar->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	g_signal_connect_object
		(tree_view, "row_activated", 
		 G_CALLBACK (row_activated_callback), sidebar, 0);


	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK,
						nautilus_shortcuts_source_targets,
						G_N_ELEMENTS (nautilus_shortcuts_source_targets),
						GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (tree_view),
			   0,
			   nautilus_shortcuts_drop_targets, G_N_ELEMENTS (nautilus_shortcuts_drop_targets),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

	g_signal_connect (tree_view, "key-press-event",
			  G_CALLBACK (bookmarks_key_press_event_cb), sidebar);

	g_signal_connect (tree_view, "drag-data-delete",
			  G_CALLBACK (drag_data_delete_callback), sidebar);
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

	eel_gtk_tree_view_set_activate_on_single_click (sidebar->tree_view,
							TRUE);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
						  desktop_location_changed_callback,
						  sidebar,
						  G_OBJECT (sidebar));

	g_signal_connect_object (nautilus_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (trash_state_changed_cb),
				 sidebar, 0);
}

static void
nautilus_places_sidebar_dispose (GObject *object)
{
	NautilusPlacesSidebar *sidebar;

	sidebar = NAUTILUS_PLACES_SIDEBAR (object);

	sidebar->window = NULL;
	sidebar->tree_view = NULL;

	g_free (sidebar->uri);
	sidebar->uri = NULL;

	free_drag_data (sidebar);

	if (sidebar->store != NULL) {
		g_object_unref (sidebar->store);
		sidebar->store = NULL;
	}

	if (sidebar->volume_monitor != NULL) {
		g_object_unref (sidebar->volume_monitor);
		sidebar->volume_monitor = NULL;
	}

	G_OBJECT_CLASS (nautilus_places_sidebar_parent_class)->dispose (object);
}

static void
nautilus_places_sidebar_class_init (NautilusPlacesSidebarClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_places_sidebar_dispose;

	GTK_WIDGET_CLASS (class)->style_set = nautilus_places_sidebar_style_set;
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
	sidebar->window = window;
	
	sidebar->bookmarks = nautilus_window_info_get_bookmark_list (window);
	sidebar->uri = nautilus_window_info_get_current_location (window);

	g_signal_connect_object (sidebar->bookmarks, "contents_changed",
				 G_CALLBACK (update_places),
				 sidebar, G_CONNECT_SWAPPED);

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback),
				 sidebar, 0);
			 
	g_signal_connect_object (sidebar->volume_monitor, "volume_added",
				 G_CALLBACK (volume_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
				 G_CALLBACK (volume_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
				 G_CALLBACK (volume_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
				 G_CALLBACK (mount_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
				 G_CALLBACK (drive_disconnected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
				 G_CALLBACK (drive_connected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
				 G_CALLBACK (drive_changed_callback), sidebar, 0);

	update_places (sidebar);
}

static void
nautilus_places_sidebar_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	NautilusPlacesSidebar *sidebar;

	sidebar = NAUTILUS_PLACES_SIDEBAR (widget);

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

/* Drag and drop interfaces */

static void
_nautilus_shortcuts_model_filter_class_init (NautilusShortcutsModelFilterClass *class)
{
}

static void
_nautilus_shortcuts_model_filter_init (NautilusShortcutsModelFilter *model)
{
	model->sidebar = NULL;
}

/* GtkTreeDragSource::row_draggable implementation for the shortcuts filter model */
static gboolean
nautilus_shortcuts_model_filter_row_draggable (GtkTreeDragSource *drag_source,
					       GtkTreePath       *path)
{
	NautilusShortcutsModelFilter *model;
	int pos;
	int bookmarks_pos;
	int num_bookmarks;

	model = NAUTILUS_SHORTCUTS_MODEL_FILTER (drag_source);

	pos = *gtk_tree_path_get_indices (path);
	bookmarks_pos = get_bookmark_index (model->sidebar->tree_view);
	num_bookmarks = nautilus_bookmark_list_length (model->sidebar->bookmarks);

	return (pos >= bookmarks_pos && pos < bookmarks_pos + num_bookmarks);
}

/* GtkTreeDragSource::drag_data_get implementation for the shortcuts filter model */
static gboolean
nautilus_shortcuts_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
					       GtkTreePath       *path,
					       GtkSelectionData  *selection_data)
{
	NautilusShortcutsModelFilter *model;

	model = NAUTILUS_SHORTCUTS_MODEL_FILTER (drag_source);

	/* FIXME */

	return FALSE;
}

/* Fill the GtkTreeDragSourceIface vtable */
static void
nautilus_shortcuts_model_filter_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = nautilus_shortcuts_model_filter_row_draggable;
	iface->drag_data_get = nautilus_shortcuts_model_filter_drag_data_get;
}

static GtkTreeModel *
nautilus_shortcuts_model_filter_new (NautilusPlacesSidebar *sidebar,
				     GtkTreeModel          *child_model,
				     GtkTreePath           *root)
{
	NautilusShortcutsModelFilter *model;

	model = g_object_new (NAUTILUS_SHORTCUTS_MODEL_FILTER_TYPE,
			      "child-model", child_model,
			      "virtual-root", root,
			      NULL);

	model->sidebar = sidebar;

	return GTK_TREE_MODEL (model);
}
