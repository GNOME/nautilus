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

#include "nautilus-file.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include <gtk/gtksignal.h>

struct NautilusTrashDirectoryDetails {
	GList *directories;
	GHashTable *callbacks;
	GHashTable *monitors;
};

typedef struct {
	/* Basic configuration. */
	NautilusTrashDirectory *trash;
	NautilusDirectoryCallback callback;
	gpointer callback_data;

	GList *wait_for_attributes;
	gboolean wait_for_metadata;

	GList *non_ready_directories;
	GList *merged_file_list;
} TrashCallback;

typedef struct {
	NautilusTrashDirectory *trash;

	GList *monitor_attributes;
	gboolean monitor_metadata;
	gboolean force_reload;
} TrashMonitor;

static void     nautilus_trash_directory_initialize       (gpointer                object,
							   gpointer                klass);
static void     nautilus_trash_directory_initialize_class (gpointer                klass);
static void     remove_all_real_directories               (NautilusTrashDirectory *trash);
static guint    trash_callback_hash                       (gconstpointer           trash_callback);
static gboolean trash_callback_equal                      (gconstpointer           trash_callback,
							   gconstpointer           trash_callback_2);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashDirectory,
				   nautilus_trash_directory,
				   NAUTILUS_TYPE_DIRECTORY)

static void
nautilus_trash_directory_initialize (gpointer object, gpointer klass)
{
	NautilusTrashDirectory *trash;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	trash->details = g_new0 (NautilusTrashDirectoryDetails, 1);
	trash->details->callbacks = g_hash_table_new
		(trash_callback_hash, trash_callback_equal);
	trash->details->monitors = g_hash_table_new
		(g_direct_hash, g_direct_equal);
}

static void
trash_destroy (GtkObject *object)
{
	NautilusTrashDirectory *trash;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	remove_all_real_directories (trash);

	if (g_hash_table_size (trash->details->callbacks) != 0) {
		g_warning ("call_when_ready still pending when trash virtual directory is destroyed");
	}
	if (g_hash_table_size (trash->details->monitors) != 0) {
		g_warning ("file monitor still active when trash virtual directory is destroyed");
	}

	g_hash_table_destroy (trash->details->callbacks);
	g_hash_table_destroy (trash->details->monitors);
	g_free (trash->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static guint
trash_callback_hash (gconstpointer trash_callback_as_pointer)
{
	const TrashCallback *trash_callback;

	trash_callback = trash_callback_as_pointer;
	return GPOINTER_TO_UINT (trash_callback->callback)
		^ GPOINTER_TO_UINT (trash_callback->callback_data);
}

static gboolean
trash_callback_equal (gconstpointer trash_callback_as_pointer,
		      gconstpointer trash_callback_as_pointer_2)
{
	const TrashCallback *trash_callback, *trash_callback_2;

	trash_callback = trash_callback_as_pointer;
	trash_callback_2 = trash_callback_as_pointer_2;

	return trash_callback->callback == trash_callback_2->callback
		&& trash_callback->callback_data == trash_callback_2->callback_data;
}

/* Return true if any directory in the list does. */
static gboolean
trash_contains_file (NautilusDirectory *directory,
		     NautilusFile *file)
{
	NautilusTrashDirectory *trash;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		if (nautilus_directory_contains_file (p->data, file)) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
trash_callback_destroy (TrashCallback *trash_callback)
{
	g_assert (trash_callback != NULL);
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_callback->trash));

	g_hash_table_remove (trash_callback->trash->details->callbacks, trash_callback);
	nautilus_g_list_free_deep (trash_callback->wait_for_attributes);
	g_list_free (trash_callback->non_ready_directories);
	nautilus_file_list_free (trash_callback->merged_file_list);
	g_free (trash_callback);
}

static void
trash_callback_check_done (TrashCallback *trash_callback)
{
	/* Check if we are ready. */
	if (trash_callback->non_ready_directories != NULL) {
		return;
	}

	/* We are ready, so do the real callback. */
	(* trash_callback->callback) (NAUTILUS_DIRECTORY (trash_callback->trash),
				      trash_callback->merged_file_list,
				      trash_callback->callback_data);

	/* And we are done. */
	trash_callback_destroy (trash_callback);
}

static void
directory_ready_callback (NautilusDirectory *directory,
			  GList *files,
			  gpointer callback_data)
{
	TrashCallback *trash_callback;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback_data != NULL);

	trash_callback = callback_data;
	g_assert (g_list_find (trash_callback->non_ready_directories, directory) != NULL);

	/* Update based on this call. */
	trash_callback->merged_file_list = g_list_concat
		(trash_callback->merged_file_list,
		 nautilus_file_list_copy (files));
	trash_callback->non_ready_directories = g_list_remove
		(trash_callback->non_ready_directories,
		 directory);

	/* Check if we are ready. */
	trash_callback_check_done (trash_callback);
}

static void
trash_callback_connect_directory (TrashCallback *trash_callback,
				  NautilusDirectory *real_trash)
{
	nautilus_directory_call_when_ready
		(real_trash,
		 trash_callback->wait_for_attributes,
		 trash_callback->wait_for_metadata,
		 directory_ready_callback, trash_callback);
}

static void
trash_call_when_ready (NautilusDirectory *directory,
		       GList *file_attributes,
		       gboolean wait_for_metadata,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	NautilusTrashDirectory *trash;
	TrashCallback search_key, *trash_callback;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (trash->details->callbacks,
				 &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a trash_callback record. */
	trash_callback = g_new0 (TrashCallback, 1);
	trash_callback->trash = trash;
	trash_callback->callback = callback;
	trash_callback->callback_data = callback_data;
	trash_callback->wait_for_attributes = nautilus_g_str_list_copy (file_attributes);
	trash_callback->wait_for_metadata = wait_for_metadata;
	for (p = trash->details->directories; p != NULL; p = p->next) {
		trash_callback->non_ready_directories = g_list_prepend
			(trash_callback->non_ready_directories, p->data);
	}

	/* Put it in the hash table. */
	g_hash_table_insert (trash->details->callbacks, trash_callback, trash_callback);

	/* Now tell all the directories about it. */
	for (p = trash->details->directories; p != NULL; p = p->next) {
		trash_callback_connect_directory (trash_callback, p->data);
	}

	/* Check just in case we are already done. */
	trash_callback_check_done (trash_callback);
}

static void
trash_cancel_callback (NautilusDirectory *directory,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	NautilusTrashDirectory *trash;
	TrashCallback search_key, *trash_callback;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	trash_callback = g_hash_table_lookup
		(trash->details->callbacks, &search_key);
	if (trash_callback == NULL) {
		return;
	}

	/* Tell all the directories to cancel the call. */
	for (p = trash_callback->non_ready_directories; p != NULL; p = p->next) {
		nautilus_directory_cancel_callback
			(p->data,
			 directory_ready_callback, trash_callback);
	}
	trash_callback_destroy (trash_callback);
}

/* Create a monitor on each of the directories in the list. */
static void
trash_file_monitor_add (NautilusDirectory *directory,
			gconstpointer client,
			GList *file_attributes,
			gboolean monitor_metadata,
			gboolean force_reload)
{
	NautilusTrashDirectory *trash;
	gpointer unique_client;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the directory by the same client.
	 */
	unique_client = g_hash_table_lookup
		(trash->details->monitors, client);
	if (unique_client == NULL) {
		unique_client = g_new (char, 1);
		g_hash_table_insert (trash->details->monitors,
				     (gpointer) client, unique_client);
	}
	
	/* Call through to the real directory add calls. */
	for (p = trash->details->directories; p != NULL; p = p->next) {
		nautilus_directory_file_monitor_add
			(p->data, unique_client,
			 file_attributes, monitor_metadata, force_reload);
	}
}

/* Remove the monitor from each of the directories in the list. */
static void
trash_file_monitor_remove (NautilusDirectory *directory,
			   gconstpointer client)
{
	NautilusTrashDirectory *trash;
	gpointer unique_client;
	GList *p;
	
	trash = NAUTILUS_TRASH_DIRECTORY (directory);
	
	/* Map the client to the value used by the earlier add call. */
	unique_client = g_hash_table_lookup
		(trash->details->monitors, client);
	if (unique_client == NULL) {
		return;
	}
	g_hash_table_remove (trash->details->monitors, client);

	/* Call through to the real directory remove calls. */
	for (p = trash->details->directories; p != NULL; p = p->next) {
		nautilus_directory_file_monitor_remove
			(p->data, unique_client);
	}

	g_free (unique_client);
}

/* Return true only if all directories in the list do. */
static gboolean
trash_are_all_files_seen (NautilusDirectory *directory)
{
	NautilusTrashDirectory *trash;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		if (!nautilus_directory_are_all_files_seen (p->data)) {
			return FALSE;
		}
	}
	return TRUE;
}

/* Return true if any directory in the list does. */
static gboolean
trash_is_not_empty (NautilusDirectory *directory)
{
	NautilusTrashDirectory *trash;
	GList *p;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		if (nautilus_directory_is_not_empty (p->data)) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
nautilus_trash_directory_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	NautilusDirectoryClass *directory_class;

	object_class = GTK_OBJECT_CLASS (klass);
	directory_class = NAUTILUS_DIRECTORY_CLASS (klass);
	
	object_class->destroy = trash_destroy;

	directory_class->contains_file = trash_contains_file;
	directory_class->call_when_ready = trash_call_when_ready;
	directory_class->cancel_callback = trash_cancel_callback;
	directory_class->file_monitor_add = trash_file_monitor_add;
	directory_class->file_monitor_remove = trash_file_monitor_remove;
 	directory_class->are_all_files_seen = trash_are_all_files_seen;
	directory_class->is_not_empty = trash_is_not_empty;
}

void
nautilus_trash_directory_add_real_trash_directory (NautilusTrashDirectory *trash,
						   NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_TRASH_DIRECTORY (trash));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (real_directory));
	g_return_if_fail (!NAUTILUS_IS_TRASH_DIRECTORY (real_directory));
	g_return_if_fail (g_list_find (trash->details->directories, real_directory) != NULL);

	/* Add to our list of directories. */
	nautilus_directory_ref (real_directory);
	trash->details->directories = g_list_prepend
		(trash->details->directories, real_directory);

	/* FIXME: Connect signals. */
	/* FIXME: Add to pending I/O. */
}

void
nautilus_trash_directory_remove_real_trash_directory (NautilusTrashDirectory *trash,
						      NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_TRASH_DIRECTORY (trash));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (real_directory));

	if (g_list_find (trash->details->directories, real_directory) == NULL) {
		return;
	}

	/* FIXME: Remove from pending I/O. */

	/* Disconnect all the signals. */
	gtk_signal_disconnect_by_data (GTK_OBJECT (real_directory), trash);

	/* Remove from our list of directories. */
	trash->details->directories = g_list_remove
		(trash->details->directories, real_directory);
	nautilus_directory_unref (real_directory);
}

static void
remove_all_real_directories (NautilusTrashDirectory *trash)
{
	while (trash->details->directories != NULL) {
		nautilus_trash_directory_remove_real_trash_directory
			(trash, trash->details->directories->data);
	}
}
