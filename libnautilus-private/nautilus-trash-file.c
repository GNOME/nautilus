/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-file.c: Subclass of NautilusFile to help implement the
   virtual trash directory.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
#include "nautilus-trash-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include "nautilus-trash-directory.h"
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

struct NautilusTrashFileDetails {
	NautilusTrashDirectory *trash_directory;

	guint add_directory_connection_id;
	guint remove_directory_connection_id;

	GList *files;

	GHashTable *callbacks;
	GHashTable *monitors;
};

typedef struct {
	NautilusTrashFile *trash;
	NautilusFileCallback callback;
	gpointer callback_data;

	GList *delegated_attributes;
	GList *non_delegated_attributes;

	GList *non_ready_files;

	gboolean initializing;
} TrashCallback;

typedef struct {
	NautilusTrashFile *trash;

	GList *delegated_attributes;
	GList *non_delegated_attributes;
} TrashMonitor;

static const char * const delegated_attributes[] = {
	NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS,
	NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
	NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES
};

static void nautilus_trash_file_initialize       (gpointer   object,
						  gpointer   klass);
static void nautilus_trash_file_initialize_class (gpointer   klass);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTrashFile,
				   nautilus_trash_file,
				   NAUTILUS_TYPE_FILE)

static gboolean
is_delegated_attribute (const char *attribute)
{
	guint i;

	g_return_val_if_fail (attribute != NULL, FALSE);

	for (i = 0; i < EEL_N_ELEMENTS (delegated_attributes); i++) {
		if (strcmp (attribute, delegated_attributes[i]) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
is_delegated_attribute_predicate (gpointer data,
				  gpointer callback_data)
{
	return is_delegated_attribute (data);
}

static void
partition_attributes (GList *attributes,
		      GList **delegated_attributes,
		      GList **non_delegated_attributes)
{
	*delegated_attributes = eel_g_list_partition
		(attributes,
		 is_delegated_attribute_predicate,
		 NULL,
		 non_delegated_attributes);
}

static void             
real_monitor_add (NautilusFile *file,
		  gconstpointer client,
		  GList *attributes)
{
	nautilus_directory_monitor_add_internal
		(file->details->directory, file,
		 client, TRUE, TRUE, attributes, NULL, NULL);
}   
			   
static void
real_monitor_remove (NautilusFile *file,
		     gconstpointer client)
{
	nautilus_directory_monitor_remove_internal
		(file->details->directory, file, client);
}			      

static void
real_call_when_ready (NautilusFile *file,
		      GList *attributes,
		      NautilusFileCallback callback,
		      gpointer callback_data)

{
	nautilus_directory_call_when_ready_internal
		(file->details->directory, file,
		 attributes, FALSE, NULL, callback, callback_data);
}

static void
real_cancel_call_when_ready (NautilusFile *file,
			     NautilusFileCallback callback,
			     gpointer callback_data)
{
	nautilus_directory_cancel_callback_internal
		(file->details->directory, file,
		 NULL, callback, callback_data);
}

static gboolean
real_check_if_ready (NautilusFile *file,
		     GList *attributes)
{
	return nautilus_directory_check_if_ready_internal
		(file->details->directory, file,
		 attributes);
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

static void
trash_callback_destroy (TrashCallback *trash_callback)
{
	g_assert (trash_callback != NULL);
	g_assert (NAUTILUS_IS_TRASH_FILE (trash_callback->trash));

	nautilus_file_unref (NAUTILUS_FILE (trash_callback->trash));
	eel_g_list_free_deep (trash_callback->delegated_attributes);
	eel_g_list_free_deep (trash_callback->non_delegated_attributes);
	g_list_free (trash_callback->non_ready_files);
	g_free (trash_callback);
}

static void
trash_callback_check_done (TrashCallback *trash_callback)
{
	/* Check if we are ready. */
	if (trash_callback->initializing || trash_callback->non_ready_files != NULL) {
		return;
	}

	/* Remove from the hash table before sending it. */
	g_hash_table_remove (trash_callback->trash->details->callbacks,
			     trash_callback);

	/* We are ready, so do the real callback. */
	(* trash_callback->callback) (NAUTILUS_FILE (trash_callback->trash),
				      trash_callback->callback_data);

	/* And we are done. */
	trash_callback_destroy (trash_callback);
}

static void
trash_callback_remove_file (TrashCallback *trash_callback,
			    NautilusFile *file)
{
	trash_callback->non_ready_files = g_list_remove
		(trash_callback->non_ready_files, file);
	trash_callback_check_done (trash_callback);
}

static void
ready_callback (NautilusFile *file,
		gpointer callback_data)
{
	TrashCallback *trash_callback;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (callback_data != NULL);

	trash_callback = callback_data;
	g_assert (g_list_find (trash_callback->non_ready_files, file) != NULL);

	trash_callback_remove_file (trash_callback, file);
}

static void
real_file_changed_callback (NautilusFile *real_file,
			    gpointer callback_data)
{
	NautilusTrashFile *trash_file;
	
	trash_file = NAUTILUS_TRASH_FILE (callback_data);
	nautilus_file_changed (NAUTILUS_FILE (trash_file));
}

static void
monitor_add_file (gpointer key,
		  gpointer value,
		  gpointer callback_data)
{
	TrashMonitor *monitor;
	
	monitor = value;
	nautilus_file_monitor_add
		(NAUTILUS_FILE (callback_data),
		 monitor,
		 monitor->delegated_attributes);
}

static void
add_real_file (NautilusTrashFile *trash,
	       NautilusFile *real_file)
{
	g_return_if_fail (NAUTILUS_IS_TRASH_FILE (trash));
	g_return_if_fail (NAUTILUS_IS_FILE (real_file));
	g_return_if_fail (!NAUTILUS_IS_TRASH_FILE (real_file));
	g_return_if_fail (g_list_find (trash->details->files, real_file) == NULL);

	nautilus_file_ref (real_file);
	trash->details->files = g_list_prepend
		(trash->details->files, real_file);
	
	gtk_signal_connect (GTK_OBJECT (real_file),
			    "changed",
			    real_file_changed_callback,
			    trash);

	/* Add the file to any extant monitors. */
	g_hash_table_foreach (trash->details->monitors,
			      monitor_add_file,
			      real_file);
}

static void
trash_callback_remove_file_cover (gpointer key,
				       gpointer value,
				       gpointer callback_data)
{
	trash_callback_remove_file
		(value, NAUTILUS_FILE (callback_data));
}

static void
monitor_remove_file (gpointer key,
		     gpointer value,
		     gpointer callback_data)
{
	nautilus_file_monitor_remove
		(NAUTILUS_FILE (callback_data), value);
}

static void
remove_real_file (NautilusTrashFile *trash,
		  NautilusFile *real_file)
{
	g_return_if_fail (NAUTILUS_IS_TRASH_FILE (trash));
	g_return_if_fail (NAUTILUS_IS_FILE (real_file));
	g_return_if_fail (g_list_find (trash->details->files, real_file) != NULL);

	eel_g_hash_table_safe_for_each
		(trash->details->callbacks,
		 trash_callback_remove_file_cover,
		 real_file);
	g_hash_table_foreach
		(trash->details->monitors,
		 monitor_remove_file,
		 real_file);

	gtk_signal_disconnect_by_func (GTK_OBJECT (real_file),
				       real_file_changed_callback,
				       trash);

	trash->details->files = g_list_remove
		(trash->details->files, real_file);
	nautilus_file_unref (real_file);
}

static void
add_real_file_given_directory (NautilusTrashFile *trash_file,
			       NautilusDirectory *real_directory)
{
	NautilusFile *real_file;

	real_file = nautilus_directory_get_corresponding_file (real_directory);
	add_real_file (trash_file, real_file);
	nautilus_file_unref (real_file);
}

static void
add_directory_callback (NautilusTrashDirectory *trash_directory,
			NautilusDirectory *real_directory,
			NautilusTrashFile *trash_file)
{
	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_directory));
	g_assert (NAUTILUS_IS_DIRECTORY (real_directory));
	g_assert (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));
	g_assert (NAUTILUS_IS_TRASH_FILE (trash_file));
	g_assert (trash_file->details->trash_directory == trash_directory);

	add_real_file_given_directory (trash_file, real_directory);

	nautilus_file_changed (NAUTILUS_FILE (trash_file));
}

static void
remove_directory_callback (NautilusTrashDirectory *trash_directory,
			   NautilusDirectory *real_directory,
			   NautilusTrashFile *trash_file)
{
	NautilusFile *real_file;

	g_assert (NAUTILUS_IS_TRASH_DIRECTORY (trash_directory));
	g_assert (NAUTILUS_IS_DIRECTORY (real_directory));
	g_assert (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));
	g_assert (NAUTILUS_IS_TRASH_FILE (trash_file));
	g_assert (trash_file->details->trash_directory == trash_directory);

	real_file = nautilus_directory_get_corresponding_file (real_directory);
	remove_real_file (trash_file, real_file);
	nautilus_file_unref (real_file);

	nautilus_file_changed (NAUTILUS_FILE (trash_file));
}

static void
trash_file_call_when_ready (NautilusFile *file,
			    GList *attributes,
			    NautilusFileCallback callback,
			    gpointer callback_data)

{
	NautilusTrashFile *trash;
	TrashCallback search_key, *trash_callback;
	GList *node;

	trash = NAUTILUS_TRASH_FILE (file);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (trash->details->callbacks, &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a trash_callback record. */
	trash_callback = g_new0 (TrashCallback, 1);
	nautilus_file_ref (file);
	trash_callback->trash = trash;
	trash_callback->callback = callback;
	trash_callback->callback_data = callback_data;
	trash_callback->initializing = TRUE;

	partition_attributes (eel_g_str_list_copy (attributes),
			      &trash_callback->delegated_attributes,
			      &trash_callback->non_delegated_attributes);

	trash_callback->non_ready_files = g_list_prepend
		(trash_callback->non_ready_files, file);
	for (node = trash->details->files; node != NULL; node = node->next) {
		trash_callback->non_ready_files = g_list_prepend
			(trash_callback->non_ready_files, node->data);
	}

	/* Put it in the hash table. */
	g_hash_table_insert (trash->details->callbacks,
			     trash_callback, trash_callback);

	/* Now connect to each file's call_when_ready. */
	real_call_when_ready
		(file, trash_callback->non_delegated_attributes,
		 ready_callback, trash_callback);
	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_call_when_ready
			(node->data, trash_callback->delegated_attributes,
			 ready_callback, trash_callback);
	}

	trash_callback->initializing = FALSE;

	/* Check if any files became read while we were connecting up
	 * the call_when_ready callbacks (also handles the pathological
	 * case where there are no files at all).
	 */
	trash_callback_check_done (trash_callback);
}

static void
trash_file_cancel_call_when_ready (NautilusFile *file,
				   NautilusFileCallback callback,
				   gpointer callback_data)
{
	NautilusTrashFile *trash;
	TrashCallback search_key, *trash_callback;
	GList *node;

	trash = NAUTILUS_TRASH_FILE (file);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	trash_callback = g_hash_table_lookup (trash->details->callbacks, &search_key);
	if (trash_callback == NULL) {
		return;
	}

	/* Remove from the hash table before working with it. */
	g_hash_table_remove (trash_callback->trash->details->callbacks, trash_callback);

	/* Tell all the directories to cancel the call. */
	real_cancel_call_when_ready (file, ready_callback, trash_callback);
	for (node = trash_callback->non_ready_files; node != NULL; node = node->next) {
		nautilus_file_cancel_call_when_ready
			(node->data, ready_callback, trash_callback);
	}
	trash_callback_destroy (trash_callback);
}

static gboolean
trash_file_check_if_ready (NautilusFile *file,
			   GList *attributes)
{
	GList *delegated_attributes, *non_delegated_attributes;
	NautilusTrashFile *trash;
	GList *node;
	gboolean ready;

	trash = NAUTILUS_TRASH_FILE (file);

	partition_attributes (g_list_copy (attributes),
			      &delegated_attributes,
			      &non_delegated_attributes);

	ready = real_check_if_ready (file, non_delegated_attributes);

	if (ready) {
		for (node = trash->details->files; node != NULL; node = node->next) {
			if (!nautilus_file_check_if_ready (node->data,
							   delegated_attributes)) {
				ready = FALSE;
				break;
			}
		}
	}

	g_list_free (delegated_attributes);
	g_list_free (non_delegated_attributes);

	return ready;
}

static void
trash_file_monitor_add (NautilusFile *file,
			gconstpointer client,
			GList *attributes)
{
	NautilusTrashFile *trash;
	TrashMonitor *monitor;
	GList *node;

	trash = NAUTILUS_TRASH_FILE (file);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the file by the same client.
	 */
	monitor = g_hash_table_lookup (trash->details->monitors, client);
	if (monitor != NULL) {
		g_assert (monitor->trash == trash);
		eel_g_list_free_deep (monitor->delegated_attributes);
		eel_g_list_free_deep (monitor->non_delegated_attributes);
	} else {
		monitor = g_new0 (TrashMonitor, 1);
		monitor->trash = trash;
		g_hash_table_insert (trash->details->monitors,
				     (gpointer) client, monitor);
	}

	partition_attributes (eel_g_str_list_copy (attributes),
			      &monitor->delegated_attributes,
			      &monitor->non_delegated_attributes);

	real_monitor_add (file, monitor,
			  monitor->non_delegated_attributes);
	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_monitor_add (node->data, monitor,
					   monitor->delegated_attributes);
	}
}   
			   
static void
trash_file_monitor_remove (NautilusFile *file,
			   gconstpointer client)
{
	NautilusTrashFile *trash;
	TrashMonitor *monitor;
	GList *node;
	
	trash = NAUTILUS_TRASH_FILE (file);
	
	/* Map the client to the value used by the earlier add call. */
        monitor = g_hash_table_lookup (trash->details->monitors, client);
	if (monitor == NULL) {
		return;
	}
	g_hash_table_remove (trash->details->monitors, client);

	/* Call through to the real file remove calls. */
	real_monitor_remove (file, monitor);
	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_monitor_remove (node->data, monitor);
	}

	eel_g_list_free_deep (monitor->delegated_attributes);
	eel_g_list_free_deep (monitor->non_delegated_attributes);
	g_free (monitor);
}

static GnomeVFSFileType
trash_file_get_file_type (NautilusFile *file)
{
	return GNOME_VFS_FILE_TYPE_DIRECTORY;
}			      

static gboolean
trash_file_get_item_count (NautilusFile *file, 
			   guint *count,
			   gboolean *count_unreadable)
{
	NautilusTrashFile *trash;
	GList *node;
	guint one_count, one_unreadable;
	gboolean got_count;
	
	trash = NAUTILUS_TRASH_FILE (file);
	
	got_count = TRUE;
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}

	for (node = trash->details->files; node != NULL; node = node->next) {
		if (!nautilus_file_get_directory_item_count (node->data,
							     &one_count,
							     &one_unreadable)) {
			got_count = FALSE;
		}

		if (count != NULL) {
			*count += one_count;
		}
		if (count_unreadable != NULL && one_unreadable) {
			*count_unreadable = TRUE;
		}
	}

	return got_count;
}

static NautilusRequestStatus
trash_file_get_deep_counts (NautilusFile *file,
			    guint *directory_count,
			    guint *file_count,
			    guint *unreadable_directory_count,
			    GnomeVFSFileSize *total_size)
{
	NautilusTrashFile *trash;
	GList *node;
	NautilusRequestStatus status, one_status;
	guint one_directory_count, one_file_count, one_unreadable_directory_count;
	GnomeVFSFileSize one_total_size;

	trash = NAUTILUS_TRASH_FILE (file);
	
	status = NAUTILUS_REQUEST_DONE;
	if (directory_count != NULL) {
		*directory_count = 0;
	}
	if (file_count != NULL) {
		*file_count = 0;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		*total_size = 0;
	}

	for (node = trash->details->files; node != NULL; node = node->next) {
		one_status = nautilus_file_get_deep_counts
			(node->data,
			 &one_directory_count,
			 &one_file_count,
			 &one_unreadable_directory_count,
			 &one_total_size);
		
		if (one_status < status) {
			status = one_status;
		}
		if (directory_count != NULL) {
			*directory_count += one_directory_count;
		}
		if (file_count != NULL) {
			*file_count += one_file_count;
		}
		if (unreadable_directory_count != NULL) {
			*unreadable_directory_count += one_unreadable_directory_count;
		}
		if (total_size != NULL) {
			*total_size += one_total_size;
		}
	}

	return status;
}

static gboolean
trash_file_get_date (NautilusFile *file,
		     NautilusDateType date_type,
		     time_t *date)
{
	NautilusTrashFile *trash;
	GList *node;
	gboolean got_at_least_one;
	gboolean got_all;
	time_t one_date;

	trash = NAUTILUS_TRASH_FILE (file);

	got_at_least_one = FALSE;
	got_all = TRUE;
	
	for (node = trash->details->files; node != NULL; node = node->next) {
		if (nautilus_file_get_date (node->data,
					    date_type,
					    &one_date)) {
			if (!got_at_least_one) {
				got_at_least_one = TRUE;
				if (date != NULL) {
					*date = one_date;
				}
			} else {
				if (date != NULL && one_date > *date) {
					*date = one_date;
				}
			}
		} else {
			got_all = FALSE;
		}
	}

	return got_at_least_one && got_all;
}

static char *
trash_file_get_where_string (NautilusFile *file)
{
	return g_strdup (_("on the desktop"));
}


static void
remove_all_real_files (NautilusTrashFile *trash)
{
	while (trash->details->files != NULL) {
		remove_real_file (trash, trash->details->files->data);
	}
}

static void
nautilus_trash_file_initialize (gpointer object, gpointer klass)
{
	NautilusTrashFile *trash_file;
	NautilusTrashDirectory *trash_directory;
	GList *real_directories, *node;

	trash_file = NAUTILUS_TRASH_FILE (object);

	trash_directory = NAUTILUS_TRASH_DIRECTORY (nautilus_directory_get (EEL_TRASH_URI));

	trash_file->details = g_new0 (NautilusTrashFileDetails, 1);
	trash_file->details->trash_directory = trash_directory;

	trash_file->details->callbacks = g_hash_table_new
		(trash_callback_hash, trash_callback_equal);
	trash_file->details->monitors = g_hash_table_new (NULL, NULL);

	trash_file->details->add_directory_connection_id = gtk_signal_connect
		(GTK_OBJECT (trash_directory),
		 "add_real_directory",
		 add_directory_callback,
		 trash_file);
	trash_file->details->remove_directory_connection_id = gtk_signal_connect
		(GTK_OBJECT (trash_directory),
		 "remove_real_directory",
		 remove_directory_callback,
		 trash_file);

	real_directories = nautilus_merged_directory_get_real_directories
		(NAUTILUS_MERGED_DIRECTORY (trash_directory));
	for (node = real_directories; node != NULL; node = node->next) {
		add_real_file_given_directory (trash_file, node->data);
	}
	g_list_free (real_directories);
}

static void
trash_destroy (GtkObject *object)
{
	NautilusTrashFile *trash_file;
	NautilusTrashDirectory *trash_directory;

	trash_file = NAUTILUS_TRASH_FILE (object);
	trash_directory = trash_file->details->trash_directory;

	remove_all_real_files (trash_file);

	if (g_hash_table_size (trash_file->details->callbacks) != 0) {
		g_warning ("call_when_ready still pending when trash virtual file is destroyed");
	}
	if (g_hash_table_size (trash_file->details->monitors) != 0) {
		g_warning ("file monitor still active when trash virtual file is destroyed");
	}

	gtk_signal_disconnect (GTK_OBJECT (trash_directory),
			       trash_file->details->add_directory_connection_id);
	gtk_signal_disconnect (GTK_OBJECT (trash_directory),
			       trash_file->details->remove_directory_connection_id);

	g_hash_table_destroy (trash_file->details->callbacks);
	g_hash_table_destroy (trash_file->details->monitors);

	g_free (trash_file->details);

	nautilus_directory_unref (NAUTILUS_DIRECTORY (trash_directory));

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_trash_file_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	NautilusFileClass *file_class;

	object_class = GTK_OBJECT_CLASS (klass);
	file_class = NAUTILUS_FILE_CLASS (klass);
	
	object_class->destroy = trash_destroy;

	file_class->monitor_add = trash_file_monitor_add;
	file_class->monitor_remove = trash_file_monitor_remove;
	file_class->call_when_ready = trash_file_call_when_ready;
	file_class->cancel_call_when_ready = trash_file_cancel_call_when_ready;
	file_class->check_if_ready = trash_file_check_if_ready;
	file_class->get_file_type = trash_file_get_file_type;
	file_class->get_item_count = trash_file_get_item_count;
	file_class->get_deep_counts = trash_file_get_deep_counts;
	file_class->get_date = trash_file_get_date;
	file_class->get_where_string = trash_file_get_where_string;
}
