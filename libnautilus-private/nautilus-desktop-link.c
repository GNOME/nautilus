/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-link.c: Class that handles the links on the desktop
    
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
#include "nautilus-desktop-link.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-directory-private.h"
#include "nautilus-desktop-directory.h"

#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <string.h>

#define TRASH_EMPTY_ICON "gnome-fs-trash-empty"
#define TRASH_FULL_ICON "gnome-fs-trash-full"

struct NautilusDesktopLinkDetails {
	NautilusDesktopLinkType type;
        char *filename;
	char *display_name;
	char *activation_uri;
	char *icon;

	NautilusDesktopIconFile *icon_file;
	
	/* Just for trash icons: */
	gulong trash_state_handler;

	/* Just for volume icons: */
	char *mount_path;
};

static void nautilus_desktop_link_init       (gpointer              object,
					      gpointer              klass);
static void nautilus_desktop_link_class_init (gpointer              klass);
static void trash_state_changed_callback     (NautilusTrashMonitor *trash_monitor,
					      gboolean              state,
					      gpointer              callback_data);
static void nautilus_desktop_link_changed    (NautilusDesktopLink  *link);
static void home_uri_changed                 (gpointer              callback_data);

EEL_CLASS_BOILERPLATE (NautilusDesktopLink,
		       nautilus_desktop_link,
		       G_TYPE_OBJECT)

static void
create_icon_file (NautilusDesktopLink *link)
{
	link->details->icon_file = nautilus_desktop_icon_file_new (link);
}

static void
home_name_changed (gpointer callback_data)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (callback_data);
	g_assert (link->details->type == NAUTILUS_DESKTOP_LINK_HOME);

	g_free (link->details->display_name);
	link->details->display_name = eel_preferences_get (NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME);
	
	nautilus_desktop_link_changed (link);
}

static void
trash_name_changed (gpointer callback_data)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (callback_data);
	g_assert (link->details->type == NAUTILUS_DESKTOP_LINK_TRASH);

	
	g_free (link->details->display_name);
	link->details->display_name = eel_preferences_get (NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME);
	nautilus_desktop_link_changed (link);
}



NautilusDesktopLink *
nautilus_desktop_link_new (NautilusDesktopLinkType type)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (g_object_new (NAUTILUS_TYPE_DESKTOP_LINK, NULL));

	link->details->type = type;
	switch (type) {
	case NAUTILUS_DESKTOP_LINK_HOME:
		link->details->filename = g_strdup ("home");

		link->details->display_name = eel_preferences_get (NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME);
		
#ifdef WEB_NAVIGATION_ENABLED
		link->details->activation_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
#else
		link->details->activation_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
#endif
		link->details->icon = g_strdup ("gnome-fs-home");

		eel_preferences_add_callback (NAUTILUS_PREFERENCES_HOME_URI,
					      home_uri_changed,
					      link);
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME,
					      home_name_changed,
					      link);
		
		break;
	case NAUTILUS_DESKTOP_LINK_TRASH:
		link->details->filename = g_strdup ("trash");
		link->details->display_name = g_strdup (_("Trash"));
		link->details->activation_uri = g_strdup (EEL_TRASH_URI);
		if (nautilus_trash_monitor_is_empty ()) {
			link->details->icon = g_strdup (TRASH_EMPTY_ICON);
		} else {
			link->details->icon = g_strdup (TRASH_FULL_ICON);
		}
		
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME,
					      trash_name_changed,
					      link);
		link->details->trash_state_handler =
			g_signal_connect_object (nautilus_trash_monitor_get (), "trash_state_changed",
						 G_CALLBACK (trash_state_changed_callback), link, 0);	
		break;
	default:
	case NAUTILUS_DESKTOP_LINK_VOLUME:
		g_assert_not_reached();
	}

	create_icon_file (link);

	return link;
}

NautilusDesktopLink *
nautilus_desktop_link_new_from_volume (const NautilusVolume *volume)
{
	NautilusDesktopLink *link;
	const char *mount_path;
	char *underscore_mount_path, *p;

	link = NAUTILUS_DESKTOP_LINK (g_object_new (NAUTILUS_TYPE_DESKTOP_LINK, NULL));
	
	link->details->type = NAUTILUS_DESKTOP_LINK_VOLUME;

	mount_path = nautilus_volume_get_mount_path (volume);
	link->details->mount_path = g_strdup (mount_path);
	
	/* Convert slashes in the mount path to underscores and skip
	   first slash */
	while (*mount_path == '/') {
		mount_path ++;
	}
	underscore_mount_path = g_strdup (mount_path);
	for (p = underscore_mount_path; *p != 0; p++) {
		if (*p == '/') {
			*p = '_';
		}
	}

	link->details->filename = g_strconcat ("mount_", underscore_mount_path, NULL);
	g_free (underscore_mount_path);
	
	link->details->display_name = nautilus_volume_get_name (volume);
	
	link->details->activation_uri = nautilus_volume_get_target_uri (volume);
	link->details->icon = nautilus_volume_get_icon (volume);
	
	create_icon_file (link);

	return link;
}

char *
nautilus_desktop_link_get_mount_path (NautilusDesktopLink *link)
{
	g_assert (link->details->type == NAUTILUS_DESKTOP_LINK_VOLUME);
	return g_strdup (link->details->mount_path);
}


NautilusDesktopLinkType
nautilus_desktop_link_get_link_type  (NautilusDesktopLink *link)
{
  return link->details->type;
}

char *
nautilus_desktop_link_get_file_name (NautilusDesktopLink *link)
{
	return g_strdup (link->details->filename);
}

char *
nautilus_desktop_link_get_display_name (NautilusDesktopLink *link)
{
	return g_strdup (link->details->display_name);
}

char *
nautilus_desktop_link_get_icon (NautilusDesktopLink *link)
{
	return g_strdup (link->details->icon);
}

char *
nautilus_desktop_link_get_activation_uri (NautilusDesktopLink *link)
{
	return g_strdup (link->details->activation_uri);
}

gboolean
nautilus_desktop_link_get_date (NautilusDesktopLink *link,
				NautilusDateType     date_type,
				time_t               *date)
{
	return FALSE;
}

static void
nautilus_desktop_link_changed (NautilusDesktopLink *link)
{
	if (link->details->icon_file != NULL) {
		nautilus_desktop_icon_file_update (link->details->icon_file);
	}
}

static void
trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
			      gboolean state,
			      gpointer callback_data)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (callback_data);
	g_assert (link->details->type == NAUTILUS_DESKTOP_LINK_TRASH);

	g_free (link->details->icon);
	
	if (state) {
		link->details->icon = g_strdup (TRASH_EMPTY_ICON);
	} else {
		link->details->icon = g_strdup (TRASH_FULL_ICON);
	}

	nautilus_desktop_link_changed (link);
}

static void
home_uri_changed (gpointer callback_data)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (callback_data);

	g_free (link->details->activation_uri);
#ifdef WEB_NAVIGATION_ENABLED
	link->details->activation_uri = eel_preferences_get (NAUTILUS_PREFERENCES_HOME_URI);
#else
	link->details->activation_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
#endif
	
	nautilus_desktop_link_changed (link);
}


gboolean
nautilus_desktop_link_can_rename (NautilusDesktopLink     *link)
{
	return (link->details->type == NAUTILUS_DESKTOP_LINK_HOME ||
		link->details->type == NAUTILUS_DESKTOP_LINK_TRASH);
}

gboolean
nautilus_desktop_link_rename (NautilusDesktopLink     *link,
			      const char              *name)
{
	switch (link->details->type) {
	case NAUTILUS_DESKTOP_LINK_HOME:
		eel_preferences_set (NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME,
				     name);
		break;
	case NAUTILUS_DESKTOP_LINK_TRASH:
		eel_preferences_set (NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME,
				     name);
		break;
	default:
		g_assert_not_reached ();
		/* FIXME: Do we want volume renaming?
		 * We didn't support that before. */
		break;
	}
	
	return TRUE;
}


static void
nautilus_desktop_link_init (gpointer object, gpointer klass)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (object);

	link->details = g_new0 (NautilusDesktopLinkDetails, 1);
}	

static void
desktop_link_finalize (GObject *object)
{
	NautilusDesktopLink *link;

	link = NAUTILUS_DESKTOP_LINK (object);

	if (link->details->trash_state_handler != 0) {
		g_signal_handler_disconnect (nautilus_trash_monitor_get (),
					     link->details->trash_state_handler);
	}

	if (link->details->icon_file != NULL) {
		nautilus_desktop_icon_file_remove (link->details->icon_file);
		nautilus_file_unref (NAUTILUS_FILE (link->details->icon_file));
		link->details->icon_file = NULL;
	}

	if (link->details->type == NAUTILUS_DESKTOP_LINK_HOME) {
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_HOME_URI,
						 home_uri_changed,
						 link);
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME,
						 home_name_changed,
						 link);
	}
	
	if (link->details->type == NAUTILUS_DESKTOP_LINK_TRASH) {
		eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME,
						 trash_name_changed,
						 link);
	}
	
	g_free (link->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_desktop_link_class_init (gpointer klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = desktop_link_finalize;

}
