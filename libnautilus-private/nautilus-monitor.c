/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-monitor.c: file and directory change monitoring for nautilus
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
  
   Authors: Seth Nickell <seth@eazel.com>
            Darin Adler <darin@bentspoon.com>
	    Alex Graveley <alex@ximian.com>
*/

#include <config.h>
#include "nautilus-monitor.h"
#include "nautilus-file-changes-queue.h"
#include "nautilus-file-utilities.h"
#include "nautilus-volume-monitor.h"

#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>

struct NautilusMonitor {
	GnomeVFSMonitorHandle *handle;
};

gboolean
nautilus_monitor_active (void)
{
	static gboolean tried_monitor = FALSE;
	static gboolean monitor_success;
	char *desktop_directory, *uri;
	NautilusMonitor *monitor;

	if (tried_monitor == FALSE) {	
		desktop_directory = nautilus_get_desktop_directory ();
		uri = gnome_vfs_get_uri_from_local_path (desktop_directory);

		monitor = nautilus_monitor_directory (uri);
		monitor_success = (monitor != NULL);

		if (monitor != NULL) {
			nautilus_monitor_cancel (monitor);
		}

		g_free (desktop_directory);
		g_free (uri);

		tried_monitor = TRUE;
	}

	return monitor_success;
}

static gboolean
path_is_on_readonly_volume (const char *path)
{
	NautilusVolumeMonitor *volume_monitor;
	NautilusVolume *volume;

	volume_monitor = nautilus_volume_monitor_get ();
	volume = nautilus_volume_monitor_get_volume_for_path (volume_monitor, 
							      path);
	if (volume != NULL) {
		return nautilus_volume_is_read_only (volume);
	} else {
		return FALSE;
	}
}

static gboolean call_consume_changes_idle_id = 0;

static gboolean
call_consume_changes_idle_cb (gpointer not_used)
{
	nautilus_file_changes_consume_changes (TRUE);
	call_consume_changes_idle_id = 0;
	return FALSE;
}

static void 
monitor_notify_cb (GnomeVFSMonitorHandle    *handle,
		   const gchar              *monitor_uri,
		   const gchar              *info_uri,
		   GnomeVFSMonitorEventType  event_type,
		   gpointer                  user_data)
{
	switch (event_type) {
	case GNOME_VFS_MONITOR_EVENT_CHANGED:
		nautilus_file_changes_queue_file_changed (info_uri);
		break;
	case GNOME_VFS_MONITOR_EVENT_DELETED:
		nautilus_file_changes_queue_file_removed (info_uri);
		break;
	case GNOME_VFS_MONITOR_EVENT_CREATED:
		nautilus_file_changes_queue_file_added (info_uri);
		break;

	/* None of the following are supported yet */
	case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
	case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
	case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
		break;
	}

	if (call_consume_changes_idle_id == 0) {
		call_consume_changes_idle_id = 
			g_idle_add (call_consume_changes_idle_cb, NULL);
	}
}

static NautilusMonitor *
monitor_add_internal (const char *uri, gboolean is_directory)
{
	gchar *path;
	NautilusMonitor *ret;
	GnomeVFSResult result;

	path = gnome_vfs_get_local_path_from_uri (uri);
	if (path != NULL && 
	    path_is_on_readonly_volume (path) == FALSE) {
		g_free (path);
		return NULL;
	}
	g_free (path);

	ret = g_new0 (NautilusMonitor, 1);

	result = gnome_vfs_monitor_add (&ret->handle,
					uri,
					is_directory == TRUE ?  
						GNOME_VFS_MONITOR_DIRECTORY :
						GNOME_VFS_MONITOR_FILE,
					monitor_notify_cb,
					NULL);
	if (result != GNOME_VFS_OK) {
		g_free (ret);
		return NULL;
	}

	return ret;
}

NautilusMonitor *
nautilus_monitor_directory (const char *uri)
{
	return monitor_add_internal (uri, TRUE);
}

NautilusMonitor *
nautilus_monitor_file (const char *uri)
{
	return monitor_add_internal (uri, FALSE);
}

void 
nautilus_monitor_cancel (NautilusMonitor *monitor)
{
	if (monitor->handle != NULL) {
		gnome_vfs_monitor_cancel (monitor->handle);
	}

	g_free (monitor);
}
