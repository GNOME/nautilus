/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-link-monitor.c: singleton thatn manages the links
    
   Copyright (C) 2003 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-desktop-link-monitor.h"
#include "nautilus-desktop-link.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-directory.h"
#include "nautilus-desktop-directory.h"
#include "nautilus-global-preferences.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <string.h>

struct NautilusDesktopLinkMonitorDetails {
	NautilusDirectory *desktop_dir;
	
	NautilusDesktopLink *home_link;
	NautilusDesktopLink *computer_link;
	NautilusDesktopLink *trash_link;

	gulong mount_id;
	gulong unmount_id;
	
	GList *volume_links;
};


static void nautilus_desktop_link_monitor_init       (gpointer              object,
						      gpointer              klass);
static void nautilus_desktop_link_monitor_class_init (gpointer              klass);

EEL_CLASS_BOILERPLATE (NautilusDesktopLinkMonitor,
		       nautilus_desktop_link_monitor,
		       G_TYPE_OBJECT)

static NautilusDesktopLinkMonitor *link_monitor = NULL;

static void
destroy_desktop_link_monitor (void)
{
	if (link_monitor != NULL) {
		g_object_unref (link_monitor);
	}
}

NautilusDesktopLinkMonitor *
nautilus_desktop_link_monitor_get (void)
{
	if (link_monitor == NULL) {
		link_monitor = NAUTILUS_DESKTOP_LINK_MONITOR (g_object_new (NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NULL));
		eel_debug_call_at_shutdown (destroy_desktop_link_monitor);
	}
	return link_monitor;
}

void
nautilus_desktop_link_monitor_delete_link (NautilusDesktopLinkMonitor *monitor,
					   NautilusDesktopLink *link,
					   GtkWidget *parent_view)
{
	switch (nautilus_desktop_link_get_link_type (link)) {
	case NAUTILUS_DESKTOP_LINK_HOME:
	case NAUTILUS_DESKTOP_LINK_COMPUTER:
	case NAUTILUS_DESKTOP_LINK_TRASH:
		/* just ignore. We don't allow you to delete these */
		break;
	default:
		eel_run_simple_dialog
			(parent_view, 
			 FALSE,
			 GTK_MESSAGE_ERROR,
			 _("You cannot delete a volume icon."),
			 _("If you want to eject the volume, please use Eject in the "
			   "right-click menu of the volume."),
			 _("Can't Delete Volume"),
			 GTK_STOCK_OK, NULL);
		break;
	}
}

static void
create_volume_link (NautilusDesktopLinkMonitor *monitor,
		    GnomeVFSVolume *volume)
{
	NautilusDesktopLink *link;
	
	if (!gnome_vfs_volume_is_user_visible (volume)) {
		return;
	}

	link = nautilus_desktop_link_new_from_volume (volume);
	monitor->details->volume_links = g_list_prepend (monitor->details->volume_links, link);
}



static void
volume_mounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			 GnomeVFSVolume *volume, 
			 NautilusDesktopLinkMonitor *monitor)
{
	create_volume_link (monitor, volume);
}


static void
volume_unmounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			   GnomeVFSVolume *volume, 
			   NautilusDesktopLinkMonitor *monitor)
{
	GList *l;
	NautilusDesktopLink *link;
	GnomeVFSVolume *other_volume;

	link = NULL;
	for (l = monitor->details->volume_links; l != NULL; l = l->next) {
		other_volume = nautilus_desktop_link_get_volume (l->data);
		if (volume == other_volume) {
			gnome_vfs_volume_unref (other_volume);
			link = l->data;
			break;
		}
		gnome_vfs_volume_unref (other_volume);
	}

	if (link) {
		monitor->details->volume_links = g_list_remove (monitor->details->volume_links, link);
		g_object_unref (link);
	}
}


static void
desktop_home_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE)) {
		if (monitor->details->home_link == NULL) {
			monitor->details->home_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_HOME);
		}
	} else {
		if (monitor->details->home_link != NULL) {
			g_object_unref (monitor->details->home_link);
			monitor->details->home_link = NULL;
		}
	}
}

static void
desktop_computer_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE)) {
		if (monitor->details->computer_link == NULL) {
			monitor->details->computer_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_COMPUTER);
		}
	} else {
		if (monitor->details->computer_link != NULL) {
			g_object_unref (monitor->details->computer_link);
			monitor->details->computer_link = NULL;
		}
	}
}

static void
desktop_trash_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE)) {
		if (monitor->details->trash_link == NULL) {
			monitor->details->trash_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_TRASH);
		}
	} else {
		if (monitor->details->trash_link != NULL) {
			g_object_unref (monitor->details->trash_link);
			monitor->details->trash_link = NULL;
		}
	}
}

static void
nautilus_desktop_link_monitor_init (gpointer object, gpointer klass)
{
	NautilusDesktopLinkMonitor *monitor;
	GList *l, *volumes;
	GnomeVFSVolume *volume;
	GnomeVFSVolumeMonitor *volume_monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (object);

	monitor->details = g_new0 (NautilusDesktopLinkMonitorDetails, 1);

	/* We keep around a ref to the desktop dir */
	monitor->details->desktop_dir = nautilus_directory_get (EEL_DESKTOP_URI);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE)) {
		monitor->details->home_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_HOME);
	}
	
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE)) {
		monitor->details->computer_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_COMPUTER);
	}
	
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE)) {
		monitor->details->trash_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_TRASH);
	}

	volume_monitor = gnome_vfs_get_volume_monitor ();
	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		create_volume_link (monitor, volume);
		gnome_vfs_volume_unref (volume);
	}
	g_list_free (volumes);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
				      desktop_home_visible_changed,
				      monitor);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
				      desktop_computer_visible_changed,
				      monitor);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
				      desktop_trash_visible_changed,
				      monitor);

	
	monitor->details->mount_id = g_signal_connect_object (volume_monitor, "volume_mounted",
							      G_CALLBACK (volume_mounted_callback), monitor, 0);
	monitor->details->unmount_id = g_signal_connect_object (volume_monitor, "volume_unmounted",
								G_CALLBACK (volume_unmounted_callback), monitor, 0);

}	

static void
desktop_link_monitor_finalize (GObject *object)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (object);

	if (monitor->details->home_link != NULL) {
		g_object_unref (monitor->details->home_link);
		monitor->details->home_link = NULL;
	}

	if (monitor->details->computer_link != NULL) {
		g_object_unref (monitor->details->computer_link);
		monitor->details->computer_link = NULL;
	}

	if (monitor->details->trash_link != NULL) {
		g_object_unref (monitor->details->trash_link);
		monitor->details->trash_link = NULL;
	}

	g_list_foreach (monitor->details->volume_links, (GFunc)g_object_unref, NULL);
	g_list_free (monitor->details->volume_links);
	monitor->details->volume_links = NULL;
		
	nautilus_directory_unref (monitor->details->desktop_dir);
	monitor->details->desktop_dir = NULL;

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
					 desktop_home_visible_changed,
					 monitor);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
					 desktop_computer_visible_changed,
					 monitor);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
					 desktop_trash_visible_changed,
					 monitor);

	if (monitor->details->mount_id != 0) {
		g_source_remove (monitor->details->mount_id);
	}
	if (monitor->details->unmount_id != 0) {
		g_source_remove (monitor->details->unmount_id);
	}
	
	g_free (monitor->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_desktop_link_monitor_class_init (gpointer klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = desktop_link_monitor_finalize;

}
