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
#include "nautilus-gtk-macros.h"

struct NautilusTrashDirectoryDetails {
	GList *directories;
	GHashTable *callbacks;
	GHashTable *monitor_clients;
};

typedef struct {
	NautilusDirectory *directory;
} RealTrashDirectory;

typedef struct {
	NautilusTrashDirectory *trash;
	NautilusDirectoryCallback callback;
	gpointer callback_data;
	GList *non_ready_directories;
	GList *merged_file_list;
} CallWhenReadyState;

static void     nautilus_trash_directory_initialize       (gpointer      object,
							   gpointer      klass);
static void     nautilus_trash_directory_initialize_class (gpointer      klass);
static guint    callback_hash                             (gconstpointer call_when_ready_state);
static gboolean callback_equal                            (gconstpointer call_when_ready_state,
							   gconstpointer call_when_ready_state_2);

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
		(callback_hash, callback_equal);
	trash->details->monitor_clients = g_hash_table_new
		(g_direct_hash, g_direct_equal);
}

static void
trash_destroy (GtkObject *object)
{
	NautilusTrashDirectory *trash;

	trash = NAUTILUS_TRASH_DIRECTORY (object);

	if (g_hash_table_size (trash->details->callbacks) != 0) {
		g_warning ("call_when_ready still pending when trash virtual directory is destroyed");
	}
	if (g_hash_table_size (trash->details->monitor_clients) != 0) {
		g_warning ("file monitor still active when trash virtual directory is destroyed");
	}

	g_hash_table_destroy (trash->details->callbacks);
	g_hash_table_destroy (trash->details->monitor_clients);
	g_free (trash->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static guint
callback_hash (gconstpointer call_when_ready_state)
{
	const CallWhenReadyState *state;

	state = call_when_ready_state;
	return GPOINTER_TO_UINT (state->callback)
		^ GPOINTER_TO_UINT (state->callback_data);
}

static gboolean
callback_equal (gconstpointer call_when_ready_state,
		gconstpointer call_when_ready_state_2)
{
	const CallWhenReadyState *state, *state_2;

	state = call_when_ready_state;
	state_2 = call_when_ready_state_2;

	return state->callback == state_2->callback
		&& state->callback_data == state_2->callback_data;
}

/* Return true if any directory in the list does. */
static gboolean
trash_contains_file (NautilusDirectory *directory,
		     NautilusFile *file)
{
	NautilusTrashDirectory *trash;
	GList *p;
	RealTrashDirectory *real_trash;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;

		if (nautilus_directory_contains_file (real_trash->directory, file)) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
call_when_ready_state_destroy (CallWhenReadyState *state)
{
	g_assert (state != NULL);
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (state->trash));

	g_hash_table_remove (state->trash->details->callbacks, state);
	g_list_free (state->non_ready_directories);
	nautilus_file_list_free (state->merged_file_list);
	g_free (state);
}

static void
call_when_ready_state_check_done (CallWhenReadyState *state)
{
	/* Check if we are ready. */
	if (state->non_ready_directories != NULL) {
		return;
	}

	/* We are ready, so do the real callback. */
	(* state->callback) (NAUTILUS_DIRECTORY (state->trash),
			     state->merged_file_list,
			     state->callback_data);

	/* And we are done. */
	call_when_ready_state_destroy (state);
}

static void
directory_ready_callback (NautilusDirectory *directory,
			  GList *files,
			  gpointer callback_data)
{
	CallWhenReadyState *state;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback_data != NULL);

	state = callback_data;
	g_assert (g_list_find (state->non_ready_directories, directory) != NULL);

	/* Update based on this call. */
	state->merged_file_list = g_list_concat
		(state->merged_file_list,
		 nautilus_file_list_copy (files));
	state->non_ready_directories = g_list_remove
		(state->non_ready_directories,
		 directory);

	/* Check if we are ready. */
	call_when_ready_state_check_done (state);
}

static void
trash_call_when_ready (NautilusDirectory *directory,
		       GList *file_attributes,
		       gboolean wait_for_metadata,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	NautilusTrashDirectory *trash;
	CallWhenReadyState search_key, *state;
	GList *p;
	RealTrashDirectory *real_trash;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (trash->details->callbacks,
				 &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a state record. */
	state = g_new0 (CallWhenReadyState, 1);
	state->trash = trash;
	state->callback = callback;
	state->callback_data = callback_data;
	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;
		
		state->non_ready_directories = g_list_prepend
			(state->non_ready_directories, real_trash->directory);
	}

	/* Put it in the hash table. */
	g_hash_table_insert (trash->details->callbacks, state, state);

	/* Now tell all the directories about it. */
	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;
		
		nautilus_directory_call_when_ready
			(real_trash->directory,
			 file_attributes, wait_for_metadata,
			 directory_ready_callback, state);
	}

	/* Check just in case we are already done. */
	call_when_ready_state_check_done (state);
}

static void
trash_cancel_callback (NautilusDirectory *directory,
		       NautilusDirectoryCallback callback,
		       gpointer callback_data)
{
	NautilusTrashDirectory *trash;
	CallWhenReadyState search_key, *state;
	GList *p;
	NautilusDirectory *real_directory;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	state = g_hash_table_lookup (trash->details->callbacks,
				     &search_key);
	if (state == NULL) {
		return;
	}

	/* Tell all the directories to cancel the call. */
	for (p = state->non_ready_directories; p != NULL; p = p->next) {
		real_directory = NAUTILUS_DIRECTORY (p->data);
		
		nautilus_directory_cancel_callback
			(real_directory,
			 directory_ready_callback, state);
	}
	call_when_ready_state_destroy (state);
}

/* Add the files that are passed to make one large list. */
static void
trash_files_callback (NautilusDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	GList **merged_file_list;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback_data != NULL);

	merged_file_list = callback_data;
	*merged_file_list = g_list_concat
		(*merged_file_list, nautilus_file_list_copy (files));
}

/* Create a monitor on each of the directories in the list. */
static void
trash_file_monitor_add (NautilusDirectory *directory,
			gconstpointer client,
			GList *file_attributes,
			gboolean monitor_metadata,
			gboolean force_reload,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	NautilusTrashDirectory *trash;
	gpointer unique_client;
	GList *p, *merged_file_list;
	RealTrashDirectory *real_trash;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the directory by the same client.
	 */
	unique_client = g_hash_table_lookup
		(trash->details->monitor_clients, client);
	if (unique_client == NULL) {
		unique_client = g_new (char, 1);
		g_hash_table_insert (trash->details->monitor_clients,
				     (gpointer) client, unique_client);
	}
	
	/* Call through to the real directory add calls. */
	merged_file_list = NULL;
	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;
		
		nautilus_directory_file_monitor_add
			(real_trash->directory, unique_client,
			 file_attributes, monitor_metadata, force_reload,
			 callback == NULL ? NULL : trash_files_callback,
			 &merged_file_list);
	}

	/* Now do the callback, with the total list. */
	if (callback != NULL) {
		(* callback) (directory, merged_file_list, callback_data);
	}
	nautilus_file_list_free (merged_file_list);
}

/* Remove the monitor from each of the directories in the list. */
static void
trash_file_monitor_remove (NautilusDirectory *directory,
			   gconstpointer client)
{
	NautilusTrashDirectory *trash;
	gpointer unique_client;
	GList *p;
	RealTrashDirectory *real_trash;
	
	trash = NAUTILUS_TRASH_DIRECTORY (directory);
	
	/* Map the client to the value used by the earlier add call. */
	unique_client = g_hash_table_lookup
		(trash->details->monitor_clients, client);
	if (unique_client == NULL) {
		return;
	}
	g_hash_table_remove (trash->details->monitor_clients, client);

	/* Call through to the real directory remove calls. */
	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;
		
		nautilus_directory_file_monitor_remove
			(real_trash->directory, unique_client);
	}

	g_free (unique_client);
}

/* Return true only if all directories in the list do. */
static gboolean
trash_are_all_files_seen (NautilusDirectory *directory)
{
	NautilusTrashDirectory *trash;
	GList *p;
	RealTrashDirectory *real_trash;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;

		if (!nautilus_directory_are_all_files_seen (real_trash->directory)) {
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
	RealTrashDirectory *real_trash;

	trash = NAUTILUS_TRASH_DIRECTORY (directory);

	for (p = trash->details->directories; p != NULL; p = p->next) {
		real_trash = p->data;

		if (nautilus_directory_is_not_empty (real_trash->directory)) {
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
