/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

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

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
*/

#include <config.h>
#include "fm-desktop-icon-view.h"

#include "nautilus-trash-monitor.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-directory-notify.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-file-operations.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-volume-monitor.h>
#include <limits.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TRASH_LINK_NAME _("Trash")

static void     fm_desktop_icon_view_initialize                           (FMDesktopIconView      *desktop_icon_view);
static void     fm_desktop_icon_view_initialize_class                     (FMDesktopIconViewClass *klass);
static void     fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView        *view,
									   GtkMenu                *menu);
static void	fm_desktop_icon_view_create_selection_context_menu_items  (FMDirectoryView 	  *view, 
									   GtkMenu 		  *menu, 
									   GList 		  *files);
static void     fm_desktop_icon_view_trash_state_changed_callback         (NautilusTrashMonitor   *trash,
									   gboolean                state,
									   gpointer                callback_data);
static void     volume_mounted_callback         			  (NautilusVolumeMonitor  *monitor,
									   NautilusVolume     	  *volume,
									   FMDesktopIconView      *icon_view);
static void     volume_unmounted_callback         			  (NautilusVolumeMonitor  *monitor,
									   NautilusVolume     	  *volume,
									   FMDesktopIconView      *icon_view);
static void     mount_unmount_removable                                   (GtkCheckMenuItem       *item,
									   FMDesktopIconView      *icon_view);
static void     place_home_directory                                      (FMDesktopIconView      *icon_view);
static void     remove_old_mount_links                                    (void);
static int      desktop_icons_compare_callback                            (NautilusIconContainer  *container,
									   NautilusFile           *file_a,
									   NautilusFile           *file_b,
									   FMDesktopIconView      *icon_view);
static void	create_or_rename_trash 					  (void);
								   
static gboolean real_supports_auto_layout     	 		  	  (FMIconView  	    *view);
static gboolean real_supports_zooming 	     	 		  	  (FMDirectoryView  *view);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView,
				   fm_desktop_icon_view,
				   FM_TYPE_ICON_VIEW)

static NautilusIconContainer *
get_icon_container (FMDesktopIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_DESKTOP_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
fm_desktop_icon_view_destroy (GtkObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Clean up details */	
	gtk_object_destroy (GTK_OBJECT (icon_view->details->volume_monitor));
	g_free (icon_view->details);

	/* Clean up any links that may be left over */
	remove_old_mount_links ();
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
fm_desktop_icon_view_initialize_class (FMDesktopIconViewClass *klass)
{
	GtkObjectClass		*object_class;
	FMDirectoryViewClass	*fm_directory_view_class;
	FMIconViewClass		*fm_icon_view_class;

	object_class		= GTK_OBJECT_CLASS (klass);
	fm_directory_view_class	= FM_DIRECTORY_VIEW_CLASS (klass);
	fm_icon_view_class	= FM_ICON_VIEW_CLASS (klass);

	object_class->destroy = fm_desktop_icon_view_destroy;

        fm_directory_view_class->create_background_context_menu_items = fm_desktop_icon_view_create_background_context_menu_items;
	fm_directory_view_class->create_selection_context_menu_items = fm_desktop_icon_view_create_selection_context_menu_items;
	fm_directory_view_class->supports_zooming = real_supports_zooming;

	fm_icon_view_class->supports_auto_layout = real_supports_auto_layout;
}

static void
fm_desktop_icon_view_handle_middle_click (NautilusIconContainer *icon_container,
					  GdkEventButton *event,
					  FMDesktopIconView *desktop_icon_view)
{
	XButtonEvent x_event;
	
	/* During a mouse click we have the pointer and keyboard grab.
	 * We will send a fake event to the root window which will cause it
	 * to try to get the grab so we need to let go ourselves.
	 */
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);

	/* Stop the event because we don't want anyone else dealing with it. */	
	gdk_flush ();
	gtk_signal_emit_stop_by_name (GTK_OBJECT(icon_container), "middle_click");

	/* build an X event to represent the middle click. */
	x_event.type = ButtonPress;
	x_event.send_event = True;
	x_event.display = GDK_DISPLAY ();
	x_event.window = GDK_ROOT_WINDOW ();
	x_event.root = GDK_ROOT_WINDOW ();
	x_event.subwindow = 0;
	x_event.time = event->time;
	x_event.x = event->x;
	x_event.y = event->y;
	x_event.x_root = event->x_root;
	x_event.y_root = event->y_root;
	x_event.state = event->state;
	x_event.button = event->button;
	x_event.same_screen = True;
	
	/* Send it to the root window, the window manager will handle it. */
	XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), True,
		    ButtonPressMask, (XEvent *) &x_event);
}

static void
create_mount_link (const NautilusVolume *volume)
{
	gboolean result;
	char *desktop_path, *target_uri, *icon_name, *volume_name;

	/* FIXME bugzilla.eazel.com 2174: This hack was moved here
	 * from the volume monitor code.
	 */
	/* Make a link only for the root partition for now. */
	if (volume->type == NAUTILUS_VOLUME_EXT2
	    && strcmp (volume->mount_path, "/") != 0) {
		return;
	}
	
	/* Get icon type */
	if (volume->type == NAUTILUS_VOLUME_CDROM) {
		icon_name = g_strdup ("i-cdrom.png");
	} else if (volume->type == NAUTILUS_VOLUME_FLOPPY) {
		icon_name = g_strdup ("i-floppy.png");
	} else {
		icon_name = g_strdup ("i-blockdev.png");
	}
	
	desktop_path = nautilus_get_desktop_directory ();
	target_uri = gnome_vfs_get_uri_from_local_path (volume->mount_path);
	volume_name = nautilus_volume_monitor_get_volume_name (volume);
	
	/* Create link */
	result = nautilus_link_create (desktop_path, volume_name, icon_name, 
				       target_uri, NAUTILUS_LINK_MOUNT);
	/* FIXME bugzilla.eazel.com 2526: Ignoring the result here OK? */

	g_free (desktop_path);
	g_free (target_uri);
	g_free (icon_name);
	g_free (volume_name);
}

static gboolean
startup_create_mount_links (const NautilusVolume *volume, gpointer data)
{
	create_mount_link (volume);
	return TRUE;
}

static void
event_callback (GtkWidget *widget, GdkEvent *event, FMDesktopIconView *desktop_icon_view)
{
}

/* Update home link to point to new home uri */
static void
home_uri_changed (gpointer user_data)
{
	FMDesktopIconView *desktop_icon_view;

	desktop_icon_view = FM_DESKTOP_ICON_VIEW (user_data);
	place_home_directory (desktop_icon_view);
}

static void
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
	NautilusIconContainer *icon_container;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	
	icon_container = get_icon_container (desktop_icon_view);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	
	desktop_icon_view->details->volume_monitor = nautilus_volume_monitor_get ();
	
	nautilus_icon_container_set_is_fixed_size (icon_container, TRUE);

	/* Set allocation to be at 0, 0 */
	allocation = &GTK_WIDGET (icon_container)->allocation;
	allocation->x = 0;
	allocation->y = 0;
	gtk_widget_queue_resize (GTK_WIDGET (icon_container));

	hadj = GTK_LAYOUT (icon_container)->hadjustment;
	vadj = GTK_LAYOUT (icon_container)->vadjustment;

	nautilus_gtk_adjustment_set_value (hadj, 0);
	nautilus_gtk_adjustment_set_value (vadj, 0);

	/* Set our default layout mode */
	nautilus_icon_container_set_layout_mode (icon_container,
						 NAUTILUS_ICON_LAYOUT_T_B_L_R);

	/* Check for and clean up any old mount links that may have been left behind */		
	remove_old_mount_links ();

	/* Setup home directory link */
	place_home_directory (desktop_icon_view);

	/* Create trash link */
	create_or_rename_trash ();

	/* Create initial mount links */
	nautilus_volume_monitor_each_mounted_volume (desktop_icon_view->details->volume_monitor, 
					     	     startup_create_mount_links, desktop_icon_view);
	
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "middle_click",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_handle_middle_click),
			    desktop_icon_view);
			    
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "compare_icons",
			    GTK_SIGNAL_FUNC (desktop_icons_compare_callback),
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (desktop_icon_view),
			    "event",
			    GTK_SIGNAL_FUNC (event_callback),
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (nautilus_trash_monitor_get ()),
			    "trash_state_changed",
			    fm_desktop_icon_view_trash_state_changed_callback,
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (desktop_icon_view->details->volume_monitor),
			    "volume_mounted",
			    volume_mounted_callback,
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT (desktop_icon_view->details->volume_monitor),
			    "volume_unmounted",
			    volume_unmounted_callback,
			    desktop_icon_view);

	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HOME_URI, home_uri_changed,
				  	   desktop_icon_view);
			    
}

static void
new_terminal_menu_item_callback (GtkMenuItem *item, FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	nautilus_gnome_open_terminal (NULL);
}

static void
reset_desktop_background_menu_item_callback (GtkMenuItem *item, FMDirectoryView *view)
{
	nautilus_background_reset (fm_directory_view_get_background (view));
}

static void
change_desktop_background_menu_item_callback (GtkMenuItem *item, FMDirectoryView *view)
{
	nautilus_launch_application_from_command ("background-properties-capplet", NULL);
}

static void 
empty_trash_callback (gpointer ignored, gpointer view)
{
        g_assert (FM_IS_DIRECTORY_VIEW (view));

	nautilus_file_operations_empty_trash (GTK_WIDGET (FM_DIRECTORY_VIEW (view)));
}

static gboolean
trash_link_is_selection (FMDirectoryView *view)
{
	GList *selection;
	gboolean result;
	char *uri;

	result = FALSE;
	
	selection = fm_directory_view_get_selection (view);

	if (!nautilus_g_list_exactly_one_item (selection)) {
		nautilus_file_list_free (selection);
		return result;
	}

	if (nautilus_file_is_nautilus_link (NAUTILUS_FILE (selection->data))) {
		uri = nautilus_file_get_uri (NAUTILUS_FILE (selection->data));
		if (nautilus_link_is_trash_link (uri)) {				
			result = TRUE;
		}
		g_free (uri);
	}
	
	nautilus_file_list_free (selection);

	return result;
}

static void
fm_desktop_icon_view_create_selection_context_menu_items (FMDirectoryView *view, GtkMenu *menu, GList *files)
{
	GtkWidget *menu_item;
	
	g_assert (FM_IS_ICON_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_selection_context_menu_items,
		 (view, menu, files));
		 
	/* Add Empty Trash item if only the trash link is the selection */
	if (trash_link_is_selection (view))  {
		/* add a separator for more clarity */
		menu_item = gtk_menu_item_new ();
		gtk_widget_show (menu_item);
		gtk_menu_append (menu, menu_item);

		menu_item = gtk_menu_item_new_with_label (_("Empty Trash"));
		gtk_widget_show (menu_item);
		gtk_menu_append (menu, menu_item);
                gtk_signal_connect (GTK_OBJECT (menu_item),
                                    "activate",
                                    empty_trash_callback,
                                    view);
		gtk_widget_set_sensitive (menu_item, !nautilus_trash_monitor_is_empty ());
	}
}

static void
fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu)
{	
	GtkWidget *menu_item;
	int position;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_background_context_menu_items, 
		 (view, menu));

	position = fm_directory_view_get_context_menu_index
			(menu, FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER) + 1;
	fm_directory_view_insert_context_menu_item 
		(view, menu, 
		 _("New Terminal"), 
		 NULL, 
		 position,
		 new_terminal_menu_item_callback,
		 TRUE);

	/* Add Disks item to show state of removable volumes */
	nautilus_gtk_menu_append_separator (menu);

	menu_item = gtk_menu_item_new_with_label (_("Disks"));
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	{
		GtkMenu *sub_menu;
		GList *disk_list;
		GList *element;
		GtkWidget *check_menu_item;
		gboolean active;
		char *name;
		NautilusVolume *volume;
		
		/* Get a list containing the all removable volumes in the volume monitor */
		disk_list = nautilus_volume_monitor_get_removable_volumes (nautilus_volume_monitor_get ());

		/* Create submenu to place them in */
		sub_menu = GTK_MENU (gtk_menu_new ());
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), GTK_WIDGET (sub_menu));

		/* Iterate list and populate menu with removable volumes */
		for (element = disk_list; element != NULL; element = element->next) {
			volume = element->data;

			 /* Create item with human readable name */
			name = strrchr (volume->mount_path, '/');
			if (name != NULL) {
				check_menu_item = gtk_check_menu_item_new_with_label (name + 1);
			} else {
				check_menu_item = gtk_check_menu_item_new_with_label (volume->mount_path);
			}

			/* Add check mark if volume is mounted */
			active = nautilus_volume_monitor_volume_is_mounted (volume);
			gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (check_menu_item), TRUE);
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item), active);
			
			/* Add some data to the menu item for later mount operations */
			gtk_object_set_data_full (GTK_OBJECT (check_menu_item), "mount_point",
						  g_strdup (volume->mount_path), g_free);
			
			gtk_signal_connect (GTK_OBJECT (check_menu_item),
					    "activate",
					    GTK_SIGNAL_FUNC (mount_unmount_removable),
					    FM_DESKTOP_ICON_VIEW (view));
			
			gtk_menu_append (sub_menu, check_menu_item);
			gtk_widget_show (check_menu_item);
		}
		g_list_free (disk_list);
	}
	
	position = fm_directory_view_get_context_menu_index
			(menu, FM_DIRECTORY_VIEW_MENU_PATH_RESET_BACKGROUND);
	/* Hide the old Reset Background item so we can replace it with one of our own */
	nautilus_gtk_menu_set_item_visibility (menu, position, FALSE);
	
	fm_directory_view_insert_context_menu_item 
		(view, menu, 
		 _("Reset Desktop Background"), 
		 NULL, 
		 position++,
		 reset_desktop_background_menu_item_callback,
		 nautilus_directory_background_is_set (fm_directory_view_get_background (view)));

	fm_directory_view_insert_context_menu_item 
		(view, menu, 
		 _("Change Desktop Background"), 
		 NULL, 
		 position,
		 change_desktop_background_menu_item_callback,
		 TRUE);
}

static void
fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
						   gboolean state, gpointer callback_data)
{
	char *desktop_directory_path, *path;

	desktop_directory_path = nautilus_get_desktop_directory ();
	path = nautilus_make_path (desktop_directory_path, TRASH_LINK_NAME);

	/* Change the XML file to have a new icon. */
	nautilus_link_set_icon (path, state ? "trash-empty.png" : "trash-full.png");

	g_free (path);
	g_free (desktop_directory_path);
}

static void
volume_mounted_callback (NautilusVolumeMonitor *monitor,
			 NautilusVolume *volume, 
			 FMDesktopIconView *icon_view)
{
	create_mount_link (volume);
}

static void
volume_unmounted_callback (NautilusVolumeMonitor *monitor,
			   NautilusVolume *volume, 
			   FMDesktopIconView *icon_view)
{
	GnomeVFSResult result;
	char *link_uri, *desktop_path, *volume_name;
	GList dummy_list;
	
	desktop_path = nautilus_get_desktop_directory ();
	volume_name = nautilus_volume_monitor_get_volume_name (volume);
	link_uri = nautilus_make_path (desktop_path, volume_name);
	
	if (link_uri != NULL) {
		/* Remove mounted device icon from desktop */
		dummy_list.data = link_uri;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
		nautilus_directory_notify_files_removed (&dummy_list);
		
		result = gnome_vfs_unlink (link_uri);
		if (result != GNOME_VFS_OK) {
			/* FIXME bugzilla.eazel.com 2526: OK to ignore error? */
		}
		g_free (link_uri);
	}

	g_free (desktop_path);
	g_free (volume_name);
}

static void
mount_unmount_removable (GtkCheckMenuItem *item, FMDesktopIconView *icon_view)
{
	gboolean is_mounted;
	char *mount_point;
	
	is_mounted = FALSE;
	
	/* Locate our mount point data */
	mount_point = gtk_object_get_data (GTK_OBJECT (item), "mount_point");
	if (mount_point != NULL) {
		is_mounted = nautilus_volume_monitor_mount_unmount_removable 
				(icon_view->details->volume_monitor, mount_point); 
	}
	
	/* Set the check state of menu item even thought the user may not see it */
	gtk_check_menu_item_set_active (item, is_mounted);
}

static gboolean
find_and_update_home_link (void)
{
	DIR *current_dir;
	struct dirent *this_entry;
	struct stat status;
	char *desktop_path, *link_path;
	char *home_link_name, *home_link_path, *home_dir_uri, *home_uri;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		g_free (desktop_path);
		return FALSE;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  The home link is at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a home link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);				
				if (nautilus_link_is_home_link (link_path)) {

					/* Create the home link */					
					home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
					home_link_path = nautilus_make_path (desktop_path, home_link_name);
					
					home_dir_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
					home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, home_dir_uri);

					/* Make sure URI points to user specified home location */
					nautilus_link_set_link_uri (home_link_path, home_uri);

					g_free (home_link_path);
					g_free (home_link_name);
					g_free (desktop_path);
					g_free (home_dir_uri);
					g_free (home_uri);
					g_free (link_path);
										  
					return TRUE;
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);

	g_free (desktop_path);

	return FALSE;
}

/* place_home_directory
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
static void
place_home_directory (FMDesktopIconView *icon_view)
{
	char *desktop_path, *home_link_name, *home_link_path, *home_dir_uri, *home_uri;
	gboolean made_link;

	/* Check and see if there is a home link already.  If so, make */
	if (find_and_update_home_link ()) {
		return;
	}

	/* Create the home link */
	desktop_path = nautilus_get_desktop_directory ();
	home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
	home_link_path = nautilus_make_path (desktop_path, home_link_name);
	
	home_dir_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, home_dir_uri);
	made_link = nautilus_link_create (desktop_path, home_link_name, "temp-home.png", 
					  home_uri, NAUTILUS_LINK_HOME);	
	if (!made_link) {
		/* FIXME bugzilla.eazel.com 2526: Is a message to the console acceptable here? */
		g_message ("Unable to create home link");
	}
	
	g_free (home_link_path);
	g_free (home_link_name);
	g_free (desktop_path);
	g_free (home_dir_uri);
	g_free (home_uri);
}

/* Find a trash link and reset the name to Trash */	
static gboolean
find_and_rename_trash_link (void)
{
	DIR *current_dir;
	char *desktop_path;		
	struct dirent *this_entry;
	struct stat status;
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		return FALSE;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  The home link is at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a home link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);				
				if (nautilus_link_is_trash_link (link_path)) {
					/* Reset name */
					rename (this_entry->d_name, TRASH_LINK_NAME);
					/* Make sure LINK is set to "trash:" */
					nautilus_link_set_link_uri (link_path, "trash:");
					return TRUE;
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);

	return FALSE;
}

static void
create_or_rename_trash (void)
{
	char *desktop_directory_path;

	/* Check for trash link */
	if (find_and_rename_trash_link ()) {
		return;
	}

	desktop_directory_path = nautilus_get_desktop_directory ();
	nautilus_link_create (desktop_directory_path,
			      TRASH_LINK_NAME,
			      "trash-empty.png", 
			      "trash:",
			      NAUTILUS_LINK_TRASH);
	g_free (desktop_directory_path);
}

static void
remove_old_mount_links (void)
{
	DIR *current_dir;
	char *desktop_path;		
	struct dirent *this_entry;
	struct stat status;
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		return;
	}

	/* Connect to desktop directory */
	chdir (desktop_path);

	/* Look at all the entries */
	while ((this_entry = readdir (current_dir)) != NULL) {
		/* Ignore "." and ".." */
		if ((strcmp (this_entry->d_name, ".") != 0) &&
		    (strcmp (this_entry->d_name, "..") != 0)) {
			stat (this_entry->d_name, &status);

			/* Ignore directories.  Mount links are at the top level */
			if (!S_ISDIR (status.st_mode)) {
				/* Check and see if this is a link */
				link_path = nautilus_make_path (desktop_path, this_entry->d_name);
				if (nautilus_link_is_volume_link (link_path)) {
					unlink (this_entry->d_name);					
				}
				g_free (link_path);
			}
		}
	}
	
	closedir (current_dir);
}

static char *
get_local_path (NautilusFile *file)
{
	char *uri, *local_path;

	uri = nautilus_file_get_uri (file);
	local_path = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);
	return local_path;
}

/* Sort as follows:
 *   1) home link
 *   2) mount links
 *   3) other
 *   4) trash link
 */

typedef enum {
	SORT_HOME_LINK,
	SORT_MOUNT_LINK,
	SORT_OTHER,
	SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NautilusFile *file)
{
	char *path, *link_type;
	SortCategory category;

	if (!nautilus_file_is_nautilus_link (file)) {
		category = SORT_OTHER;
	} else {
		path = get_local_path (file);
		g_return_val_if_fail (path != NULL, SORT_OTHER);
		
		link_type = nautilus_link_get_link_type (path);
		if (link_type == NULL) {
			category = SORT_OTHER;
		} else if (strcmp (link_type, NAUTILUS_LINK_HOME_TAG) == 0) {
			category = SORT_HOME_LINK;
		} else if (strcmp (link_type, NAUTILUS_LINK_MOUNT_TAG) == 0) {
			category = SORT_MOUNT_LINK;
		} else if (strcmp (link_type, NAUTILUS_LINK_TRASH_TAG) == 0) {
			category = SORT_TRASH_LINK;
		} else {
			category = SORT_OTHER;
		}
		
		g_free (path);
		g_free (link_type);
	}
	
	return category;
}

static int
desktop_icons_compare_callback (NautilusIconContainer *container,
				NautilusFile *file_a,
				NautilusFile *file_b,
				FMDesktopIconView *icon_view)
{
	SortCategory category_a, category_b;

	category_a = get_sort_category (file_a);
	category_b = get_sort_category (file_b);

	/* Let the previous handler do the compare. */
	if (category_a == category_b) {
		return 0;
	}

	/* We know the answer, so prevent the other handlers
	 * from overwriting our result.
	 */
	gtk_signal_emit_stop_by_name (GTK_OBJECT (container),
				      "compare_icons");
	if (category_a < category_b) {
		return -1;
	} else {
		return +1;
	}
}

static gboolean
real_supports_auto_layout (FMIconView *view)
{
	/* Can't use auto-layout on the desktop, because doing so would cause all
	 * sorts of complications involving the fixed-size window.
	 */
	return FALSE;
}

static gboolean
real_supports_zooming (FMDirectoryView *view)
{
	/* Can't zoom on the desktop, because doing so would cause all
	 * sorts of complications involving the fixed-size window.
	 */
	return FALSE;
}



