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
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-trash-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-file.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-volume-monitor.h"
#include <gtk/gtksignal.h>

struct NautilusTrashDirectoryDetails {
	GHashTable *volumes;
};

typedef struct {
	NautilusTrashDirectory *trash;
	NautilusVolume *volume;

	GnomeVFSAsyncHandle *handle;
	NautilusDirectory *real_directory;
} TrashVolume;

static void     nautilus_trash_directory_initialize       (gpointer                object,
							   gpointer                klass);
static void     nautilus_trash_directory_initialize_class (gpointer                klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashDirectory,
				   nautilus_trash_directory,
				   NAUTILUS_TYPE_MERGED_DIRECTORY)

static GnomeVFSURI *
get_volume_vfs_uri_if_writable (NautilusVolume *volume)
{
	char *uri;
	GnomeVFSURI *vfs_uri;

	/* FIXME bugzilla.eazel.com 2443: 
	 * Why is trash only found on EXT2 volumes? This seems
	 * like an incorrect check.
	 */

	/* FIXME bugzilla.eazel.com 2444:
	 * Why can't we view the trash on a read-only volume 
	 * if it happens to be there?
	 */
	if (volume->type != NAUTILUS_VOLUME_EXT2 || volume->is_read_only) {
		return NULL;
	}

	uri = gnome_vfs_get_uri_from_local_path (volume->mount_path);
	vfs_uri = gnome_vfs_uri_new (uri);
	g_free (uri);

	return vfs_uri;
}

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

	g_assert (nautilus_g_list_exactly_one_item (results));
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

static void
add_volume (NautilusTrashDirectory *trash,
	    NautilusVolume *volume)
{
	TrashVolume *trash_volume;
	GnomeVFSURI *vfs_uri;
	GList vfs_uri_as_list;

	/* Quick out if we already know about this volume. */
	trash_volume = g_hash_table_lookup (trash->details->volumes,
					    volume);
	if (trash_volume != NULL) {
		return;
	}

	/* If we can't get the URI, we jus don't do trash on this volume. */
	vfs_uri = get_volume_vfs_uri_if_writable (volume);
	if (vfs_uri == NULL) {
		return;
	}

	/* Make the structure used to track the trash for this volume. */
	trash_volume = g_new0 (TrashVolume, 1);
	trash_volume->trash = trash;
	trash_volume->volume = volume;
	g_hash_table_insert (trash->details->volumes, volume, trash_volume);

	/* Find the real trash directory for this one. */
	vfs_uri_as_list.data = vfs_uri;
	vfs_uri_as_list.next = NULL;
	vfs_uri_as_list.prev = NULL;
	gnome_vfs_async_find_directory
		(&trash_volume->handle, &vfs_uri_as_list, 
		 GNOME_VFS_DIRECTORY_KIND_TRASH, TRUE, TRUE, 0777,
		 find_directory_callback, trash_volume);
}

static void
remove_trash_volume (TrashVolume *trash_volume)
{
	g_hash_table_remove (trash_volume->trash->details->volumes,
			     trash_volume->volume);
	
	if (trash_volume->handle != NULL) {
		gnome_vfs_async_cancel (trash_volume->handle);
	}
	if (trash_volume->real_directory != NULL) {
		nautilus_merged_directory_remove_real_directory
			(NAUTILUS_MERGED_DIRECTORY (trash_volume->trash),
			 trash_volume->real_directory);
		nautilus_directory_unref (trash_volume->real_directory);
	}
	g_free (trash_volume);
}


static void
remove_volume (NautilusTrashDirectory *trash,
	       NautilusVolume *volume)
{
	TrashVolume *trash_volume;

	/* Quick out if don't already know about this volume. */
	trash_volume = g_hash_table_lookup (trash->details->volumes,
					    volume);
	if (trash_volume != NULL) {
		remove_trash_volume (trash_volume);
	}
}

static gboolean
add_one_volume (const NautilusVolume *volume,
		gpointer callback_data)
{
	/* The const is a kinda silly idea which we must cast away. */
	add_volume (NAUTILUS_TRASH_DIRECTORY (callback_data),
		    (NautilusVolume *) volume);
	return FALSE; /* don't stop iterating */
}

static void
volume_mounted_callback (NautilusVolumeMonitor *monitor,
			 NautilusVolume *volume,
			 NautilusTrashDirectory *trash)
{
	add_volume (trash, volume);
}

static void
volume_unmounted_callback (NautilusVolumeMonitor *monitor,
			   NautilusVolume *volume,
			   NautilusTrashDirectory *trash)
{
	remove_volume (trash, volume);
}

static void
nautilus_trash_directory_initialize (gpointer object, gpointer klass)
{
	NautilusTrashDirectory *trash;
	NautilusVolumeMonitor *volume_monitor;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	trash->details = g_new0 (NautilusTrashDirectoryDetails, 1);
	trash->details->volumes = g_hash_table_new
		(g_direct_hash, g_direct_equal);

	volume_monitor = nautilus_volume_monitor_get ();

	gtk_signal_connect
		(GTK_OBJECT (volume_monitor), "volume_mounted",
		 volume_mounted_callback, trash);
	gtk_signal_connect
		(GTK_OBJECT (volume_monitor), "volume_unmounted",
		 volume_unmounted_callback, trash);
	nautilus_volume_monitor_each_mounted_volume
		(volume_monitor, add_one_volume, trash);
}

static void
remove_trash_volume_cover (gpointer key, gpointer value, gpointer callback_data)
{
	TrashVolume *trash_volume;

	g_assert (key != NULL);
	g_assert (value != NULL);
	g_assert (callback_data == NULL);

	trash_volume = value;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_volume->trash));
	g_assert (trash_volume->volume == key);

	remove_trash_volume (trash_volume);
}

static void
trash_destroy (GtkObject *object)
{
	NautilusTrashDirectory *trash;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	nautilus_g_hash_table_safe_for_each
		(trash->details->volumes,
		 remove_trash_volume_cover,
		 NULL);
	g_hash_table_destroy (trash->details->volumes);
	g_free (trash->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_trash_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = trash_destroy;
}
