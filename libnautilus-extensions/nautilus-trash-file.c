/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-trash-file.c: Subclass of NautilusFile to help implement the
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
#include "nautilus-trash-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-trash-directory.h"
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

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

	GList *attributes;

	GList *non_ready_files;
} TrashCallback;

typedef struct {
	NautilusTrashFile *trash;

	GList *attributes;
} TrashMonitor;

static void nautilus_trash_file_initialize       (gpointer   object,
						  gpointer   klass);
static void nautilus_trash_file_initialize_class (gpointer   klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTrashFile,
				   nautilus_trash_file,
				   NAUTILUS_TYPE_FILE)

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

	nautilus_g_list_free_deep (trash_callback->attributes);
	g_list_free (trash_callback->non_ready_files);
	g_free (trash_callback);
}

static void
trash_callback_check_done (TrashCallback *trash_callback)
{
	/* Check if we are ready. */
	if (trash_callback->non_ready_files != NULL) {
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
		 monitor->attributes);
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

	nautilus_g_hash_table_safe_for_each
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
			    GList *file_attributes,
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
	trash_callback->trash = trash;
	trash_callback->callback = callback;
	trash_callback->callback_data = callback_data;
	trash_callback->attributes = nautilus_g_str_list_copy (file_attributes);
	for (node = trash->details->files; node != NULL; node = node->next) {
		trash_callback->non_ready_files = g_list_prepend
			(trash_callback->non_ready_files, node->data);
	}

	/* Put it in the hash table. */
	g_hash_table_insert (trash->details->callbacks,
			     trash_callback, trash_callback);

	/* Handle the pathological case where there are no files. */
	if (trash->details->files == NULL) {
		trash_callback_check_done (trash_callback);
	}

	/* Now connect to each file's call_when_ready. */
	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_call_when_ready
			(node->data,
			 trash_callback->attributes,
			 ready_callback, trash_callback);
	}
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
	for (node = trash_callback->non_ready_files; node != NULL; node = node->next) {
		nautilus_file_cancel_call_when_ready
			(node->data,
			 ready_callback, trash_callback);
	}
	trash_callback_destroy (trash_callback);
}

static gboolean
trash_file_check_if_ready (NautilusFile *file,
			   GList *file_attributes)
{
	NautilusTrashFile *trash;
	GList *node;

	trash = NAUTILUS_TRASH_FILE (file);

	for (node = trash->details->files; node != NULL; node = node->next) {
		if (!nautilus_file_check_if_ready (node->data,
						   file_attributes)) {
			return FALSE;
		}
	}

	return TRUE;
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
		nautilus_g_list_free_deep (monitor->attributes);
	} else {
		monitor = g_new0 (TrashMonitor, 1);
		monitor->trash = trash;
		g_hash_table_insert (trash->details->monitors,
				     (gpointer) client, monitor);
	}
	monitor->attributes = nautilus_g_str_list_copy (attributes);

	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_monitor_add (node->data, monitor, attributes);
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
	for (node = trash->details->files; node != NULL; node = node->next) {
		nautilus_file_monitor_remove (node->data, monitor);
	}

	nautilus_g_list_free_deep (monitor->attributes);
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

	trash_directory = NAUTILUS_TRASH_DIRECTORY (nautilus_directory_get (NAUTILUS_TRASH_URI));

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

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
