/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
   nautilus-trash-monitor.c: Nautilus trash state watcher.
 
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
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nautilus-trash-monitor.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-trash-directory.h"
#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>

struct NautilusTrashMonitorDetails {
	NautilusDirectory *trash_directory;
	gboolean empty;
};

enum {
	TRASH_STATE_CHANGED,
	CHECK_TRASH_DIRECTORY_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static NautilusTrashMonitor *nautilus_trash_monitor;

static void nautilus_trash_monitor_initialize_class (NautilusTrashMonitorClass *klass);
static void nautilus_trash_monitor_initialize       (gpointer                   object,
						     gpointer                   klass);
static void destroy                                 (GtkObject                 *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTrashMonitor, nautilus_trash_monitor, GTK_TYPE_OBJECT)

static void
nautilus_trash_monitor_initialize_class (NautilusTrashMonitorClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	signals[TRASH_STATE_CHANGED] = gtk_signal_new
		("trash_state_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusTrashMonitorClass, trash_state_changed),
		 gtk_marshal_NONE__BOOL,
		 GTK_TYPE_NONE, 1,
		 GTK_TYPE_BOOL);

	signals[CHECK_TRASH_DIRECTORY_ADDED] = gtk_signal_new
		("check_trash_directory_added",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusTrashMonitorClass, check_trash_directory_added),
		 gtk_marshal_NONE__POINTER,
		 GTK_TYPE_NONE, 1,
		 GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_trash_files_changed_callback (NautilusDirectory *directory, GList *files, 
				       gpointer callback_data)
{
	NautilusTrashMonitor *trash_monitor;
	gboolean old_empty_state;
	NautilusFile *file;
	
	trash_monitor = callback_data;
	g_assert (NAUTILUS_IS_TRASH_MONITOR (trash_monitor));
	g_assert (trash_monitor->details->trash_directory == directory);

	/* Something about the Trash NautilusDirectory changed, find out if 
	 * it affected the empty state.
	 */
	old_empty_state = trash_monitor->details->empty;
	trash_monitor->details->empty = !nautilus_directory_is_not_empty (directory);

	if (old_empty_state != trash_monitor->details->empty) {
		file = nautilus_file_get (EEL_TRASH_URI);
		nautilus_file_changed (file);
		nautilus_file_unref (file);

		/* trash got empty or full, notify everyone who cares */
		gtk_signal_emit (GTK_OBJECT (trash_monitor), 
				 signals[TRASH_STATE_CHANGED],
				 trash_monitor->details->empty);
	}
}

static void
nautilus_trash_monitor_initialize (gpointer object, gpointer klass)
{
	NautilusDirectory *trash_directory;
	NautilusTrashMonitor *trash_monitor;
	GList *attributes;

	trash_monitor = NAUTILUS_TRASH_MONITOR (object);

	/* set up a NautilusDirectory for the Trash directory to monitor */

	trash_directory = nautilus_directory_get (EEL_TRASH_URI);

	trash_monitor->details = g_new0 (NautilusTrashMonitorDetails, 1);
	trash_monitor->details->trash_directory = trash_directory;
	trash_monitor->details->empty = TRUE;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_METADATA);

	/* Make sure we get notified about changes */
	nautilus_directory_file_monitor_add
		(trash_directory, trash_monitor, TRUE, TRUE, attributes,
		 nautilus_trash_files_changed_callback, trash_monitor);

	g_list_free (attributes);

    	gtk_signal_connect_while_alive
		(GTK_OBJECT (trash_directory),
		 "files_added",
		 nautilus_trash_files_changed_callback,
		 trash_monitor,
		 GTK_OBJECT (trash_monitor));
    	gtk_signal_connect_while_alive
		(GTK_OBJECT (trash_directory),
		 "files_changed",
		 nautilus_trash_files_changed_callback,
		 trash_monitor,
		 GTK_OBJECT (trash_monitor));
}

static void
destroy (GtkObject *object)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = NAUTILUS_TRASH_MONITOR (object);

	nautilus_directory_file_monitor_remove
		(trash_monitor->details->trash_directory, 
		 trash_monitor);
	nautilus_directory_unref (trash_monitor->details->trash_directory);
	g_free (trash_monitor->details);
}

static void
unref_trash_monitor (void)
{
	gtk_object_unref (GTK_OBJECT (nautilus_trash_monitor));
}

NautilusTrashMonitor *
nautilus_trash_monitor_get (void)
{
	NautilusDirectory *trash_directory;

	if (nautilus_trash_monitor == NULL) {
		/* not running yet, start it up */

		/* the trash directory object will get created by this */
		trash_directory = nautilus_directory_get (EEL_TRASH_URI);
		
		nautilus_trash_monitor = NAUTILUS_TRASH_MONITOR
			(gtk_object_new (NAUTILUS_TYPE_TRASH_MONITOR, NULL));
		gtk_object_ref (GTK_OBJECT (nautilus_trash_monitor));
		gtk_object_sink (GTK_OBJECT (nautilus_trash_monitor));
		g_atexit (unref_trash_monitor);
		
		/* make sure we get signalled when trash directories get added */
		nautilus_trash_directory_finish_initializing
			(NAUTILUS_TRASH_DIRECTORY (trash_directory));

		nautilus_directory_unref (trash_directory);
	}

	return nautilus_trash_monitor;
}

gboolean
nautilus_trash_monitor_is_empty (void)
{
	return nautilus_trash_monitor_get ()->details->empty;
}

static gboolean
add_one_volume_trash (const NautilusVolume *volume,
		      gpointer callback_data)
{
	char *uri_str;
	GnomeVFSURI *volume_mount_point_uri;
	GnomeVFSURI *trash_uri;
	GList **result;
	
	result = (GList **) callback_data;

	if (nautilus_volume_should_integrate_trash (volume)) {
	
		/* Get the uri of the volume mount point as the place
		 * "near" which to look for trash on the given volume.
		 */
		uri_str = gnome_vfs_get_uri_from_local_path (nautilus_volume_get_mount_path (volume));
		volume_mount_point_uri = gnome_vfs_uri_new (uri_str);
		g_free (uri_str);

		g_assert (volume_mount_point_uri != NULL);
	
		/* Look for trash. It is OK to use a sync call here because
		 * the options we use (don't create, don't look for it if we
		 * already don't know where it is) do not cause any IO.
		 */
		if (gnome_vfs_find_directory (volume_mount_point_uri,
					      GNOME_VFS_DIRECTORY_KIND_TRASH, &trash_uri,
					      FALSE, FALSE, 0777) == GNOME_VFS_OK) {
			
			/* found trash, put it on the list */
			*result = g_list_prepend (*result, trash_uri);
		}
		
		gnome_vfs_uri_unref (volume_mount_point_uri);
	}

	/* don't stop iterating */
	return FALSE; 
}

GList *
nautilus_trash_monitor_get_trash_directories (void)
{
	GList *result;

	result = NULL;

	/* Collect the trash directories on all the mounted volumes. */
	nautilus_volume_monitor_each_mounted_volume
		(nautilus_volume_monitor_get (), add_one_volume_trash, &result);
	
	return result;
}

static gboolean
add_one_trash_directory_if_needed (const NautilusVolume *volume,
				   gpointer callback_data)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = NAUTILUS_TRASH_MONITOR (callback_data);
	gtk_signal_emit (GTK_OBJECT (trash_monitor),
			 signals[CHECK_TRASH_DIRECTORY_ADDED],
			 volume);
	
	return FALSE;
}

void 
nautilus_trash_monitor_add_new_trash_directories (void)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = nautilus_trash_monitor_get ();
	nautilus_volume_monitor_each_mounted_volume
		(nautilus_volume_monitor_get (), add_one_trash_directory_if_needed,
		trash_monitor);
}

