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

#include "fm-desktop-mounting.h"
#include "fm-icon-view.h"

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
#include <mntent.h>
#include <unistd.h>


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
									   
NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMDesktopIconView, fm_desktop_icon_view, FM_TYPE_ICON_VIEW);

static NautilusIconContainer *
get_icon_container (FMDesktopIconView *icon_view)
{
	g_return_val_if_fail (FM_IS_DESKSTOP_ICON_VIEW (icon_view), NULL);
	g_return_val_if_fail (NAUTILUS_IS_ICON_CONTAINER (GTK_BIN (icon_view)->child), NULL);

	return NAUTILUS_ICON_CONTAINER (GTK_BIN (icon_view)->child);
}

static void
fm_desktop_icon_view_destroy (GtkObject *object)
{
	FMDesktopIconView *icon_view;

	icon_view = FM_DESKTOP_ICON_VIEW (object);

	/* Remove timer function */
	gtk_timeout_remove (icon_view->details->mount_device_timer_id);

	/* Remove mount link files */
	g_list_foreach (icon_view->details->devices, (GFunc)fm_desktop_remove_mount_links, icon_view);
	
	/* Clean up other device info */
	g_list_foreach (icon_view->details->devices, (GFunc)fm_desktop_free_device_info, icon_view);
	
	/* Clean up details */	 
	g_hash_table_destroy (icon_view->details->devices_by_fsname);
	g_list_free (icon_view->details->devices);
	g_free (icon_view->details);

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
	desktop_icon_view->details->devices_by_fsname = g_hash_table_new (g_str_hash, g_str_equal);
	desktop_icon_view->details->devices = NULL;

	/* Setup home directory link */
	fm_desktop_place_home_directory (desktop_icon_view);
	
	gtk_signal_connect (GTK_OBJECT (icon_container),
			    "middle_click",
			    GTK_SIGNAL_FUNC (fm_desktop_icon_view_handle_middle_click),
			    desktop_icon_view);

	/* Check for mountable devices */
	fm_desktop_find_mount_devices (desktop_icon_view, _PATH_MNTTAB);
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
			active = fm_desktop_volume_is_mounted (element->data);
			gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (check_menu_item), TRUE);
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item), active);
			
			/* Add some data to the menu item for later mount operations */
			gtk_object_set_data_full (GTK_OBJECT (check_menu_item), "mount_point",
						  g_strdup (element->data), g_free);
			
			gtk_signal_connect (GTK_OBJECT (check_menu_item),
			    "activate",
			     GTK_SIGNAL_FUNC (fm_desktop_mount_unmount_removable),
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


