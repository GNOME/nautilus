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

#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <string.h>

struct NautilusDesktopLinkMonitorDetails {
	NautilusDirectory *desktop_dir;
	
	NautilusDesktopLink *home_link;
	NautilusDesktopLink *trash_link;

	GList *volume_links;

	GList *mount_black_list;
};


static void nautilus_desktop_link_monitor_init       (gpointer              object,
						      gpointer              klass);
static void nautilus_desktop_link_monitor_class_init (gpointer              klass);

EEL_CLASS_BOILERPLATE (NautilusDesktopLinkMonitor,
		       nautilus_desktop_link_monitor,
		       G_TYPE_OBJECT)

static NautilusDesktopLinkMonitor *link_monitor = NULL;
     
NautilusDesktopLinkMonitor *
nautilus_desktop_link_monitor_get (void)
{
	if (link_monitor == NULL) {
		link_monitor = NAUTILUS_DESKTOP_LINK_MONITOR (g_object_new (NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NULL));
	}
	return link_monitor;
}

void
nautilus_desktop_link_monitor_delete_link (NautilusDesktopLinkMonitor *monitor,
					   NautilusDesktopLink *link,
					   GtkWidget *parent_view)
{
	/* FIXME: Is this right? How to get them back?
	 * Do we disallow this, or add a prefs ui to get them back? */
	
	switch (nautilus_desktop_link_get_link_type (link)) {
	case NAUTILUS_DESKTOP_LINK_HOME:
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE, FALSE);
		break;
	case NAUTILUS_DESKTOP_LINK_TRASH:
		eel_preferences_set_boolean (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE, FALSE);
		break;
	default:
		eel_run_simple_dialog
			(parent_view, 
			 FALSE,
			 _("You cannot delete a volume icon. If you want to eject "
			   "the volume, please use Eject in the right-click menu of "
			   "the volume."), 
			 _("Can't delete volume"),
			 GTK_STOCK_OK, NULL);
		break;
	}
}


static gboolean
volume_in_black_list (NautilusDesktopLinkMonitor *monitor,
		      const NautilusVolume *volume)
{
	GList *p;
	
	g_return_val_if_fail (NAUTILUS_IS_DESKTOP_LINK_MONITOR (monitor), TRUE);

	for (p = monitor->details->mount_black_list; p != NULL; p = p->next) {
		if (strcmp ((char *) p->data, nautilus_volume_get_mount_path (volume)) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
volume_name_exists (NautilusDesktopLinkMonitor *monitor,
		    const char *name)
{
	GList *l;
	char *other_name;
	
	for (l = monitor->details->volume_links; l != NULL; l = l->next) {
		other_name = nautilus_desktop_link_get_display_name (l->data);
		if (strcmp (name, other_name) == 0) {
			g_free (other_name);
			return TRUE;
		}
		g_free (other_name);
	}
	return FALSE;
}

static void
create_volume_link (NautilusDesktopLinkMonitor *monitor,
		    const NautilusVolume *volume)
{
	NautilusDesktopLink *link;
	char *volume_name;
	char *unique_name;
	int index;
	
	if (volume_in_black_list (monitor, volume)) {
		return;
	}

	/* FIXME bugzilla.gnome.org 45412: Design a comprehensive desktop mounting strategy */
	if (!nautilus_volume_is_removable (volume)) {
		return;
	}

	volume_name = nautilus_volume_get_name (volume);
	index = 1;

	unique_name = g_strdup (volume_name);
	while (volume_name_exists (monitor, volume_name)) {
		g_free (unique_name);
		unique_name = g_strdup_printf ("%s (%d)", volume_name, index);
	}

	if (index != 1) {
		nautilus_volume_monitor_set_volume_name (nautilus_volume_monitor_get (),
							 volume, unique_name);
	}
	g_free (volume_name);
	g_free (unique_name);
	
	link = nautilus_desktop_link_new_from_volume (volume);
	monitor->details->volume_links = g_list_prepend (monitor->details->volume_links, link);
}

static gboolean
create_one_volume_link (const NautilusVolume *volume, gpointer callback_data)
{
	create_volume_link (NAUTILUS_DESKTOP_LINK_MONITOR (callback_data),
			    volume);
	return TRUE;
}


static void
volume_mounted_callback (NautilusVolumeMonitor *volume_monitor,
			 NautilusVolume *volume, 
			 NautilusDesktopLinkMonitor *monitor)
{
	create_volume_link (monitor, volume);
}


static void
volume_unmounted_callback (NautilusVolumeMonitor *volume_monitor,
			   NautilusVolume *volume, 
			   NautilusDesktopLinkMonitor *monitor)
{
	GList *l;
	NautilusDesktopLink *link;
	char *mount_path;

	link = NULL;
	for (l = monitor->details->volume_links; l != NULL; l = l->next) {
		mount_path = nautilus_desktop_link_get_mount_path (l->data);
		
		if (strcmp (mount_path, nautilus_volume_get_mount_path (volume)) == 0) {
			link = l->data;
			g_free (mount_path);
			break;
		}
		g_free (mount_path);
	}

	if (link) {
		g_object_unref (link);
		monitor->details->volume_links = g_list_remove (monitor->details->volume_links, link);
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
	GList *list;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (object);

	monitor->details = g_new0 (NautilusDesktopLinkMonitorDetails, 1);

	/* Set up default mount black list */
	list = g_list_prepend (NULL, g_strdup ("/proc"));
	list = g_list_prepend (list, g_strdup ("/boot"));
	monitor->details->mount_black_list = list;

	/* We keep around a ref to the desktop dir */
	monitor->details->desktop_dir = nautilus_directory_get (EEL_DESKTOP_URI);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE)) {
		monitor->details->home_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_HOME);
	}
	
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE)) {
		monitor->details->trash_link = nautilus_desktop_link_new (NAUTILUS_DESKTOP_LINK_TRASH);
	}

	nautilus_volume_monitor_each_mounted_volume (nautilus_volume_monitor_get (),
					     	     create_one_volume_link,
						     monitor);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
				      desktop_home_visible_changed,
				      monitor);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
				      desktop_trash_visible_changed,
				      monitor);

	
	g_signal_connect_object (nautilus_volume_monitor_get (), "volume_mounted",
				 G_CALLBACK (volume_mounted_callback), monitor, 0);
	g_signal_connect_object (nautilus_volume_monitor_get (), "volume_unmounted",
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

	if (monitor->details->trash_link != NULL) {
		g_object_unref (monitor->details->home_link);
		monitor->details->trash_link = NULL;
	}

	g_list_foreach (monitor->details->volume_links, (GFunc)g_object_unref, NULL);
	g_list_free (monitor->details->volume_links);
	monitor->details->volume_links = NULL;
		
	nautilus_directory_unref (monitor->details->desktop_dir);
	monitor->details->desktop_dir = NULL;

	eel_g_list_free_deep (monitor->details->mount_black_list);
	monitor->details->mount_black_list = NULL;

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
					 desktop_home_visible_changed,
					 monitor);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
					 desktop_trash_visible_changed,
					 monitor);
	
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
