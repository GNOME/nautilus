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
#include "fm-icon-view.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-volume-monitor.h>
#include <limits.h>
#include <mntent.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "nautilus-trash-monitor.h"


static void     fm_desktop_icon_view_initialize                           (FMDesktopIconView      *desktop_icon_view);
static void     fm_desktop_icon_view_initialize_class                     (FMDesktopIconViewClass *klass);
static void     fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView        *view,
									   GtkMenu                *menu);
static char *   fm_desktop_icon_view_get_directory_sort_by                (FMIconView             *icon_view,
									   NautilusDirectory      *directory);
static void     fm_desktop_icon_view_set_directory_sort_by                (FMIconView             *icon_view,
									   NautilusDirectory      *directory,
									   const char             *sort_by);
static gboolean fm_desktop_icon_view_get_directory_sort_reversed          (FMIconView             *icon_view,
									   NautilusDirectory      *directory);
static void     fm_desktop_icon_view_set_directory_sort_reversed          (FMIconView             *icon_view,
									   NautilusDirectory      *directory,
									   gboolean                sort_reversed);
static gboolean fm_desktop_icon_view_get_directory_auto_layout            (FMIconView             *icon_view,
									   NautilusDirectory      *directory);
static void     fm_desktop_icon_view_set_directory_auto_layout            (FMIconView             *icon_view,
									   NautilusDirectory      *directory,
									   gboolean                auto_layout);
static void	fm_desktop_icon_view_trash_state_changed_callback 	  (NautilusTrashMonitor   *trash,
									   gboolean 		   state,
									   gpointer		   callback_data);

static void	mount_unmount_removable 				  (GtkCheckMenuItem 	   *item, 
									   FMDesktopIconView 	   *icon_view);
static void	place_home_directory 					  (FMDesktopIconView 	   *icon_view);
static void	remove_old_mount_links 					  (void);

								   
NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView, fm_desktop_icon_view, FM_TYPE_ICON_VIEW);

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

        fm_icon_view_class->get_directory_sort_by       = fm_desktop_icon_view_get_directory_sort_by;
        fm_icon_view_class->set_directory_sort_by       = fm_desktop_icon_view_set_directory_sort_by;
        fm_icon_view_class->get_directory_sort_reversed = fm_desktop_icon_view_get_directory_sort_reversed;
        fm_icon_view_class->set_directory_sort_reversed = fm_desktop_icon_view_set_directory_sort_reversed;
        fm_icon_view_class->get_directory_auto_layout   = fm_desktop_icon_view_get_directory_auto_layout;
        fm_icon_view_class->set_directory_auto_layout   = fm_desktop_icon_view_set_directory_auto_layout;
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
fm_desktop_icon_view_initialize (FMDesktopIconView *desktop_icon_view)
{
	NautilusIconContainer *icon_container;

	icon_container = get_icon_container (desktop_icon_view);

	/* Set up details */
	desktop_icon_view->details = g_new0 (FMDesktopIconViewDetails, 1);	
	desktop_icon_view->details->volume_monitor = nautilus_volume_monitor_get ();

	/* Check for and clean up any old mount links that may have been left behind */		
	remove_old_mount_links ();

	/* Setup home directory link */
	place_home_directory (desktop_icon_view);
	
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "middle_click",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_handle_middle_click),
			    desktop_icon_view);

	gtk_signal_connect (GTK_OBJECT(nautilus_trash_monitor_get ()),
			    "trash_state_changed",
			    fm_desktop_icon_view_trash_state_changed_callback,
			    desktop_icon_view);

	/* Check for mountable devices */
	nautilus_volume_monitor_find_mount_devices (desktop_icon_view->details->volume_monitor);
}

static void
new_terminal_menu_item_callback (GtkMenuItem *item, FMDirectoryView *view)
{
	g_assert (FM_IS_DIRECTORY_VIEW (view));
	nautilus_gnome_open_terminal (NULL);
}


static void
fm_desktop_icon_view_create_background_context_menu_items (FMDirectoryView *view, GtkMenu *menu)
{
	GtkWidget *menu_item;

	g_assert (FM_IS_DIRECTORY_VIEW (view));
	g_assert (GTK_IS_MENU (menu));

	fm_directory_view_add_menu_item (view, menu, _("New Terminal"), new_terminal_menu_item_callback,
		       TRUE);
	NAUTILUS_CALL_PARENT_CLASS
		(FM_DIRECTORY_VIEW_CLASS, 
		 create_background_context_menu_items, 
		 (view, menu));

	/* Add Disks item to show state of removable volumes */
	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

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
		
		/* Get a list containing the mount point of all removable volumes in fstab */
		disk_list = fm_desktop_get_removable_volume_list ();

		/* Create submenu to place them in */
		sub_menu = GTK_MENU (gtk_menu_new ());
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), GTK_WIDGET (sub_menu));

		/* Iterate list and populate menu with removable volumes */
		for (element = disk_list; element != NULL; element = element->next) {
			/* Create item with human readable name */
			name = strrchr (element->data, '/');
			if (name != NULL) {
				check_menu_item = gtk_check_menu_item_new_with_label (name + 1);
			} else {
				check_menu_item = gtk_check_menu_item_new_with_label (element->data);
			}
			
			/* Add check mark if volume is mounted */
			active = nautilus_volume_monitor_volume_is_mounted (element->data);
			gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (check_menu_item), TRUE);
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item), active);
			
			/* Add some data to the menu item for later mount operations */
			gtk_object_set_data_full (GTK_OBJECT (check_menu_item), "mount_point",
						  g_strdup (element->data), g_free);
			
			gtk_signal_connect (GTK_OBJECT (check_menu_item),
			    "activate",
			     GTK_SIGNAL_FUNC (mount_unmount_removable),
			     FM_DESKTOP_ICON_VIEW (view));
			     			
			gtk_menu_append (sub_menu, check_menu_item);
			gtk_widget_show (check_menu_item);
			g_free (element->data);
		}
		g_list_free (disk_list);
	}
}

static char *
fm_desktop_icon_view_get_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory)
{
	return g_strdup("name");
}

static void
fm_desktop_icon_view_set_directory_sort_by (FMIconView *icon_view, NautilusDirectory *directory, const char* sort_by)
{
	/* do nothing - the desktop always uses the same sort_by */
}

static gboolean
fm_desktop_icon_view_get_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_sort_reversed (FMIconView *icon_view, NautilusDirectory *directory, gboolean sort_reversed)
{
	/* do nothing - the desktop always uses sort_reversed == FALSE */
}

static gboolean
fm_desktop_icon_view_get_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory)
{
	return FALSE;
}

static void
fm_desktop_icon_view_set_directory_auto_layout (FMIconView *icon_view, NautilusDirectory *directory, gboolean auto_layout)
{
	/* do nothing - the desktop always uses auto_layout == FALSE */
}


static void
fm_desktop_icon_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
	gboolean state, gpointer callback_data)
{
	char *desktop_directory_path, *path;

	desktop_directory_path = nautilus_get_desktop_directory ();
	path = nautilus_make_path (desktop_directory_path, "Trash");

	/* Change the XML file to have a new icon. */
	nautilus_link_set_icon (path, state ? "trash-empty.png" : "trash-full.png");

	g_free (path);
	g_free (desktop_directory_path);
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

/* fm_desktop_place_home_directory
 * 
 * Add an icon representing the user's home directory on the desktop.
 * Create if necessary
 */
static void
place_home_directory (FMDesktopIconView *icon_view)
{
	char *desktop_path, *home_link_name, *home_link_path, *home_link_uri, *home_dir_uri;
	GnomeVFSResult result;
	GnomeVFSFileInfo info;
	
	desktop_path = nautilus_get_desktop_directory ();
	home_link_name = g_strdup_printf ("%s's Home", g_get_user_name ());
	home_link_path = nautilus_make_path (desktop_path, home_link_name);
	home_link_uri = nautilus_get_uri_from_local_path (home_link_path);
	
	result = gnome_vfs_get_file_info (home_link_uri, &info, 0);
	if (result != GNOME_VFS_OK) {
		/* FIXME: Maybe we should only create if the error was "not found". */
		/* There was no link file.  Create it and add it to the desktop view */		
		home_dir_uri = nautilus_get_uri_from_local_path (g_get_home_dir ());
		result = nautilus_link_create (desktop_path, home_link_name, "temp-home.png", home_dir_uri);
		g_free (home_dir_uri);
		if (result != GNOME_VFS_OK) {
			/* FIXME: Is a message to the console acceptable here? */
			g_message ("Unable to create home link: %s", gnome_vfs_result_to_string (result));
		}
	}
	
	g_free (home_link_uri);
	g_free (home_link_path);
	g_free (home_link_name);
	g_free (desktop_path);
}

static void
remove_old_mount_links (void)
{
	DIR *current_dir;
	char *desktop_path;		
	struct dirent *this_entry;
	struct stat status;
	char cwd[PATH_MAX + 1];
	char *link_path;

	desktop_path = nautilus_get_desktop_directory ();

	/* Open directory for reading */
	current_dir = opendir (desktop_path);
	if (current_dir == NULL) {
		return;
	}

	/* Save working directory and connect to desktop directory */
	getcwd (cwd, PATH_MAX + 1);
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
				if (nautilus_volume_monitor_is_volume_link (this_entry->d_name)) {
					link_path = nautilus_make_path (desktop_path, this_entry->d_name);
					unlink (this_entry->d_name);
					g_free (link_path);
				}
			}
		}
	}
	
	closedir (current_dir);
}


