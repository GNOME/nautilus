/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-directory.c: Subclass of NautilusDirectory to implement the
   virtual trash directory.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-trash-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-trash-monitor.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>

struct NautilusTrashDirectoryDetails {
	GHashTable *volumes;
};

typedef struct {
	NautilusTrashDirectory *trash;
	GnomeVFSVolume *volume;

	GnomeVFSAsyncHandle *handle;
	NautilusDirectory *real_directory;
} TrashVolume;

static void add_volume (NautilusTrashDirectory *trash,
			GnomeVFSVolume         *volume);

GNOME_CLASS_BOILERPLATE (NautilusTrashDirectory, nautilus_trash_directory,
			 NautilusMergedDirectory, NAUTILUS_TYPE_MERGED_DIRECTORY)

static void
find_directory_callback (GnomeVFSAsyncHandle *handle,
			 GList *results,
			 gpointer callback_data)
{
	TrashVolume *trash_volume;
	GnomeVFSFindDirectoryResult *result;
	char *uri;
	NautilusDirectory *directory;

	trash_volume = callback_data;

	g_assert (eel_g_list_exactly_one_item (results));
	g_assert (trash_volume != NULL);
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_volume->trash));
	g_assert (trash_volume->real_directory == NULL);
	g_assert (trash_volume->handle == handle);

	/* We are done with the async. I/O. */
	trash_volume->handle = NULL;

	/* If we can't find the trash, ignore it silently. */
	result = results->data;
	if (result->result != GNOME_VFS_OK) {
		return;
	}

	/* If we can't make a directory object, ignore it silently. */
	uri = gnome_vfs_uri_to_string (result->uri, 
				       GNOME_VFS_URI_HIDE_NONE);
	directory = nautilus_directory_get (uri);
	g_free (uri);
	if (directory == NULL) {
		return;
	}

	/* Add the directory object. */
	trash_volume->real_directory = directory;
	nautilus_merged_directory_add_real_directory
		(NAUTILUS_MERGED_DIRECTORY (trash_volume->trash),
		 directory);
}

static gboolean
get_trash_volume (NautilusTrashDirectory *trash,
		  GnomeVFSVolume *volume,
		  TrashVolume **trash_volume,
		  GnomeVFSURI **volume_mount_uri)
{
	char *uri_str;

	/* Quick out if we already know about this volume. */
	*trash_volume = g_hash_table_lookup (trash->details->volumes,
					     volume);
					    
	if (*trash_volume != NULL && (*trash_volume)->real_directory != NULL) {
		return FALSE;
	}

	if (!gnome_vfs_volume_handles_trash (volume)) {
		return FALSE;
	}
	
	uri_str = gnome_vfs_volume_get_activation_uri (volume);
	*volume_mount_uri = gnome_vfs_uri_new (uri_str);
	g_free (uri_str);

	if (*trash_volume == NULL) {
		/* Make the structure used to track the trash for this volume. */
		*trash_volume = g_new0 (TrashVolume, 1);
		(*trash_volume)->trash = trash;
		(*trash_volume)->volume = gnome_vfs_volume_ref (volume);
		g_hash_table_insert (trash->details->volumes, volume, *trash_volume);
	}
	
	return TRUE;
}

static void
add_volume (NautilusTrashDirectory *trash,
	    GnomeVFSVolume *volume)
{
	TrashVolume *trash_volume;
	GnomeVFSURI *volume_mount_uri;
	GList vfs_uri_as_list;

	if (!get_trash_volume (trash, volume, &trash_volume, &volume_mount_uri)) {
		return;
	}

	if (trash_volume->handle) {
		/* Already searching for trash */
		gnome_vfs_uri_unref (volume_mount_uri);
		return;
	}

	/* Find the real trash directory for this one. */
	vfs_uri_as_list.data = volume_mount_uri;
	vfs_uri_as_list.next = NULL;
	vfs_uri_as_list.prev = NULL;

	/* Search for Trash directories but don't create new ones. */
	gnome_vfs_async_find_directory
		(&trash_volume->handle, &vfs_uri_as_list, 
		 GNOME_VFS_DIRECTORY_KIND_TRASH, FALSE, TRUE, 0777,
		 GNOME_VFS_PRIORITY_DEFAULT,
		 find_directory_callback, trash_volume);

	gnome_vfs_uri_unref (volume_mount_uri);
}

static void
check_trash_created (NautilusTrashDirectory *trash,
		     GnomeVFSVolume *volume)
{
	GnomeVFSResult result;
	TrashVolume *trash_volume;
	GnomeVFSURI *volume_mount_uri;
	GnomeVFSURI *trash_uri;
	char *uri;

	if (!get_trash_volume (trash, volume, &trash_volume, &volume_mount_uri)) {
		return;
	}

	/* Do a synch trash lookup -- if the trash directory was just created, it's location will
	 * be known and returned immediately without any blocking.
	 */
	result = gnome_vfs_find_directory (volume_mount_uri, GNOME_VFS_DIRECTORY_KIND_TRASH,
		&trash_uri, FALSE, FALSE, 077);
	
	gnome_vfs_uri_unref (volume_mount_uri);
	if (result != GNOME_VFS_OK) {
		return;
	}

	uri = gnome_vfs_uri_to_string (trash_uri, 
				       GNOME_VFS_URI_HIDE_NONE);
	trash_volume->real_directory = nautilus_directory_get (uri);
	g_free (uri);
	gnome_vfs_uri_unref (trash_uri);
	if (trash_volume->real_directory == NULL) {
		return;
	}
	
	/* Add the directory object. */
	nautilus_merged_directory_add_real_directory
		(NAUTILUS_MERGED_DIRECTORY (trash_volume->trash),
		 trash_volume->real_directory);
}

static void
remove_trash_volume (TrashVolume *trash_volume, gboolean finalizing)
{
	g_hash_table_remove (trash_volume->trash->details->volumes,
			     trash_volume->volume);
	
	if (trash_volume->handle != NULL) {
		gnome_vfs_async_cancel (trash_volume->handle);
	}
	if (trash_volume->real_directory != NULL) {
		if (! finalizing) {
			nautilus_merged_directory_remove_real_directory
				(NAUTILUS_MERGED_DIRECTORY (trash_volume->trash),
				 trash_volume->real_directory);
		}
		nautilus_directory_unref (trash_volume->real_directory);
	}
	gnome_vfs_volume_unref (trash_volume->volume);
	g_free (trash_volume);
}

static void
remove_volume (NautilusTrashDirectory *trash,
	       GnomeVFSVolume *volume)
{
	TrashVolume *trash_volume;

	/* Quick out if don't already know about this volume. */
	trash_volume = g_hash_table_lookup (trash->details->volumes, volume);
	if (trash_volume != NULL) {
		remove_trash_volume (trash_volume, FALSE);
	}
}

static void
check_trash_directory_added_callback (GnomeVFSVolumeMonitor *monitor,
				      GnomeVFSVolume *volume,
				      NautilusTrashDirectory *trash)
{
	check_trash_created (trash, volume);
}

static void
volume_unmount_started_callback (GnomeVFSVolumeMonitor *monitor,
				 GnomeVFSVolume *volume,
				 NautilusTrashDirectory *trash)
{
	remove_volume (trash, volume);
}

static void
volume_mounted_callback (GnomeVFSVolumeMonitor *monitor,
			 GnomeVFSVolume *volume,
			 NautilusTrashDirectory *trash)
{
	add_volume (trash, volume);
}

static void
nautilus_trash_directory_instance_init (NautilusTrashDirectory *trash)
{
	GnomeVFSVolumeMonitor *volume_monitor;

	trash->details = g_new0 (NautilusTrashDirectoryDetails, 1);
	trash->details->volumes = g_hash_table_new (NULL, NULL);

	volume_monitor = gnome_vfs_get_volume_monitor ();

	g_signal_connect_object (volume_monitor, "volume_mounted",
				 G_CALLBACK (volume_mounted_callback), trash, 0);
	g_signal_connect_object (volume_monitor, "volume_pre_unmount",
				 G_CALLBACK (volume_unmount_started_callback), trash, 0);
}

/* Finish initializing a new NautilusTrashDirectory. We have to do the
 * remaining initialization here rather than in nautilus_trash_directory_init
 * because of a cyclic dependency between the NautilusTrashDirectory and
 * NautilusTrashMonitor instances.
 */
void
nautilus_trash_directory_finish_initializing (NautilusTrashDirectory *trash)
{
	GnomeVFSVolumeMonitor *volume_monitor;
	GList *volumes, *l;
	
	volume_monitor = gnome_vfs_get_volume_monitor ();

	g_signal_connect_object (nautilus_trash_monitor_get (), "check_trash_directory_added",
				 G_CALLBACK (check_trash_directory_added_callback), trash, 0);

	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		add_volume (trash, l->data);
		gnome_vfs_volume_unref (l->data);
	}
	g_list_free (volumes);
}

static void
remove_trash_volume_finalizing_cover (gpointer key, gpointer value, gpointer callback_data)
{
	TrashVolume *trash_volume;

	g_assert (key != NULL);
	g_assert (value != NULL);
	g_assert (callback_data == NULL);

	trash_volume = value;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_volume->trash));
	g_assert (trash_volume->volume == key);

	remove_trash_volume (trash_volume, TRUE);
}

static void
trash_finalize (GObject *object)
{
	NautilusTrashDirectory *trash;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	eel_g_hash_table_safe_for_each (trash->details->volumes,
					remove_trash_volume_finalizing_cover, NULL);
	g_hash_table_destroy (trash->details->volumes);
	g_free (trash->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static char *
trash_get_name_for_self_as_new_file (NautilusDirectory *directory)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (directory));
	return g_strdup (_("Trash"));
}

static void
nautilus_trash_directory_class_init (NautilusTrashDirectoryClass *class)
{
	G_OBJECT_CLASS (class)->finalize = trash_finalize;
	NAUTILUS_DIRECTORY_CLASS (class)->get_name_for_self_as_new_file = trash_get_name_for_self_as_new_file;
}

