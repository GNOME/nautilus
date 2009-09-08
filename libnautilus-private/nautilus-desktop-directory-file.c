/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-directory-file.c: Subclass of NautilusFile to help implement the
   virtual desktop.
 
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
#include "nautilus-desktop-directory-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>
#include "nautilus-desktop-directory.h"
#include "nautilus-metadata.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

struct NautilusDesktopDirectoryFileDetails {
	NautilusDesktopDirectory *desktop_directory;

  	NautilusFile *real_dir_file;

	GHashTable *callbacks;
	GHashTable *monitors;
};

typedef struct {
	NautilusDesktopDirectoryFile *desktop_file;
	NautilusFileCallback callback;
	gpointer callback_data;

	NautilusFileAttributes delegated_attributes;
	NautilusFileAttributes non_delegated_attributes;

	GList *non_ready_files;

	gboolean initializing;
} DesktopCallback;

typedef struct {
	NautilusDesktopDirectoryFile *desktop_file;

	NautilusFileAttributes delegated_attributes;
	NautilusFileAttributes non_delegated_attributes;
} DesktopMonitor;


static void nautilus_desktop_directory_file_init       (gpointer   object,
							gpointer   klass);
static void nautilus_desktop_directory_file_class_init (gpointer   klass);

EEL_CLASS_BOILERPLATE (NautilusDesktopDirectoryFile,
		       nautilus_desktop_directory_file,
		       NAUTILUS_TYPE_FILE)

static guint
desktop_callback_hash (gconstpointer desktop_callback_as_pointer)
{
	const DesktopCallback *desktop_callback;

	desktop_callback = desktop_callback_as_pointer;
	return GPOINTER_TO_UINT (desktop_callback->callback)
		^ GPOINTER_TO_UINT (desktop_callback->callback_data);
}

static gboolean
desktop_callback_equal (gconstpointer desktop_callback_as_pointer,
		      gconstpointer desktop_callback_as_pointer_2)
{
	const DesktopCallback *desktop_callback, *desktop_callback_2;

	desktop_callback = desktop_callback_as_pointer;
	desktop_callback_2 = desktop_callback_as_pointer_2;

	return desktop_callback->callback == desktop_callback_2->callback
		&& desktop_callback->callback_data == desktop_callback_2->callback_data;
}

     
static void
real_file_changed_callback (NautilusFile *real_file,
			    gpointer callback_data)
{
	NautilusDesktopDirectoryFile *desktop_file;
	
	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (callback_data);
	nautilus_file_changed (NAUTILUS_FILE (desktop_file));
}

static NautilusFileAttributes 
get_delegated_attributes_mask (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES;
}

static void
partition_attributes (NautilusFileAttributes attributes,
		      NautilusFileAttributes *delegated_attributes,
		      NautilusFileAttributes *non_delegated_attributes)
{
	NautilusFileAttributes mask;

	mask = get_delegated_attributes_mask ();

	*delegated_attributes = attributes & mask;
	*non_delegated_attributes = attributes & ~mask;
}

static void
desktop_directory_file_monitor_add (NautilusFile *file,
				    gconstpointer client,
				    NautilusFileAttributes attributes)
{
	NautilusDesktopDirectoryFile *desktop_file;
	DesktopMonitor *monitor;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the file by the same client.
	 */
	monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
	if (monitor != NULL) {
		g_assert (monitor->desktop_file == desktop_file);
	} else {
		monitor = g_new0 (DesktopMonitor, 1);
		monitor->desktop_file = desktop_file;
		g_hash_table_insert (desktop_file->details->monitors,
				     (gpointer) client, monitor);
	}

	partition_attributes (attributes,
			      &monitor->delegated_attributes,
			      &monitor->non_delegated_attributes);

	/* Pawn off partioned attributes to real dir file */
	nautilus_file_monitor_add (desktop_file->details->real_dir_file,
				   monitor, monitor->delegated_attributes);

	/* Do the rest ourself */
	nautilus_directory_monitor_add_internal
		(file->details->directory, file,
		 client, TRUE, TRUE,
		 monitor->non_delegated_attributes,
		 NULL, NULL);
}

static void
desktop_directory_file_monitor_remove (NautilusFile *file,
				       gconstpointer client)
{
	NautilusDesktopDirectoryFile *desktop_file;
	DesktopMonitor *monitor;
	
	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);
	
	/* Map the client to the value used by the earlier add call. */
        monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
	if (monitor == NULL) {
		return;
	}

	/* Call through to the real file remove calls. */
	g_hash_table_remove (desktop_file->details->monitors, client);

	/* Remove the locally handled parts */
	nautilus_directory_monitor_remove_internal
		(file->details->directory, file, client);
}

static void
desktop_callback_destroy (DesktopCallback *desktop_callback)
{
	g_assert (desktop_callback != NULL);
	g_assert (NAUTILUS_IS_DESKTOP_DIRECTORY_FILE (desktop_callback->desktop_file));

	nautilus_file_unref (NAUTILUS_FILE (desktop_callback->desktop_file));
	g_list_free (desktop_callback->non_ready_files);
	g_free (desktop_callback);
}

static void
desktop_callback_check_done (DesktopCallback *desktop_callback)
{
	/* Check if we are ready. */
	if (desktop_callback->initializing ||
	    desktop_callback->non_ready_files != NULL) {
		return;
	}

	/* Remove from the hash table before sending it. */
	g_hash_table_remove (desktop_callback->desktop_file->details->callbacks,
			     desktop_callback);

	/* We are ready, so do the real callback. */
	(* desktop_callback->callback) (NAUTILUS_FILE (desktop_callback->desktop_file),
					desktop_callback->callback_data);

	/* And we are done. */
	desktop_callback_destroy (desktop_callback);
}

static void
desktop_callback_remove_file (DesktopCallback *desktop_callback,
			      NautilusFile *file)
{
	desktop_callback->non_ready_files = g_list_remove
		(desktop_callback->non_ready_files, file);
	desktop_callback_check_done (desktop_callback);
}

static void
ready_callback (NautilusFile *file,
		gpointer callback_data)
{
	DesktopCallback *desktop_callback;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (callback_data != NULL);

	desktop_callback = callback_data;
	g_assert (g_list_find (desktop_callback->non_ready_files, file) != NULL);

	desktop_callback_remove_file (desktop_callback, file);
}

static void
desktop_directory_file_call_when_ready (NautilusFile *file,
					NautilusFileAttributes attributes,
					NautilusFileCallback callback,
					gpointer callback_data)

{
	NautilusDesktopDirectoryFile *desktop_file;
	DesktopCallback search_key, *desktop_callback;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (desktop_file->details->callbacks, &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a desktop_callback record. */
	desktop_callback = g_new0 (DesktopCallback, 1);
	nautilus_file_ref (file);
	desktop_callback->desktop_file = desktop_file;
	desktop_callback->callback = callback;
	desktop_callback->callback_data = callback_data;
	desktop_callback->initializing = TRUE;

	partition_attributes (attributes,
			      &desktop_callback->delegated_attributes,
			      &desktop_callback->non_delegated_attributes);

	desktop_callback->non_ready_files = g_list_prepend
		(desktop_callback->non_ready_files, file);
	desktop_callback->non_ready_files = g_list_prepend
		(desktop_callback->non_ready_files, desktop_file->details->real_dir_file);
	
	/* Put it in the hash table. */
	g_hash_table_insert (desktop_file->details->callbacks,
			     desktop_callback, desktop_callback);

	/* Now connect to each file's call_when_ready. */
	nautilus_directory_call_when_ready_internal
		(file->details->directory, file,
		 desktop_callback->non_delegated_attributes,
		 FALSE, NULL, ready_callback, desktop_callback);
	nautilus_file_call_when_ready
			(desktop_file->details->real_dir_file,
			 desktop_callback->delegated_attributes,
			 ready_callback, desktop_callback);

	desktop_callback->initializing = FALSE;

	/* Check if any files became read while we were connecting up
	 * the call_when_ready callbacks (also handles the pathological
	 * case where there are no files at all).
	 */
	desktop_callback_check_done (desktop_callback);

}

static void
desktop_directory_file_cancel_call_when_ready (NautilusFile *file,
					       NautilusFileCallback callback,
					       gpointer callback_data)
{
	NautilusDesktopDirectoryFile *desktop_file;
	DesktopCallback search_key, *desktop_callback;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	desktop_callback = g_hash_table_lookup (desktop_file->details->callbacks, &search_key);
	if (desktop_callback == NULL) {
		return;
	}

	/* Remove from the hash table before working with it. */
	g_hash_table_remove (desktop_callback->desktop_file->details->callbacks, desktop_callback);

	/* Tell the real directory to cancel the call. */
	nautilus_directory_cancel_callback_internal
		(file->details->directory, file,
		 NULL, ready_callback, desktop_callback);
	
	nautilus_file_cancel_call_when_ready
		(desktop_file->details->real_dir_file,
		 ready_callback, desktop_callback);
	
	desktop_callback_destroy (desktop_callback);
}

static gboolean
real_check_if_ready (NautilusFile *file,
		     NautilusFileAttributes attributes)
{
	return nautilus_directory_check_if_ready_internal
		(file->details->directory, file,
		 attributes);
}

static gboolean
desktop_directory_file_check_if_ready (NautilusFile *file,
				       NautilusFileAttributes attributes)
{
	NautilusFileAttributes delegated_attributes, non_delegated_attributes;
	NautilusDesktopDirectoryFile *desktop_file;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);

	partition_attributes (attributes,
			      &delegated_attributes,
			      &non_delegated_attributes);

	return real_check_if_ready (file, non_delegated_attributes) &&
		nautilus_file_check_if_ready (desktop_file->details->real_dir_file,
					      delegated_attributes);
}

static gboolean
desktop_directory_file_get_item_count (NautilusFile *file, 
				       guint *count,
				       gboolean *count_unreadable)
{
	NautilusDesktopDirectoryFile *desktop_file;
	gboolean got_count;
	
	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);
	
	got_count = nautilus_file_get_directory_item_count (desktop_file->details->real_dir_file,
							    count,
							    count_unreadable);

	if (count) {
		*count += g_list_length (file->details->directory->details->file_list);
	}
	
	return got_count;
}

static NautilusRequestStatus
desktop_directory_file_get_deep_counts (NautilusFile *file,
					guint *directory_count,
					guint *file_count,
					guint *unreadable_directory_count,
					goffset *total_size)
{
	NautilusDesktopDirectoryFile *desktop_file;
	NautilusRequestStatus status;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);
	
	status = nautilus_file_get_deep_counts (desktop_file->details->real_dir_file,
						directory_count,
						file_count,
						unreadable_directory_count,
						total_size,
						TRUE);

	if (file_count) {
		*file_count += g_list_length (file->details->directory->details->file_list);
	}
	
	return status;
}

static gboolean
desktop_directory_file_get_date (NautilusFile *file,
				 NautilusDateType date_type,
				 time_t *date)
{
	NautilusDesktopDirectoryFile *desktop_file;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (file);

	return nautilus_file_get_date (desktop_file->details->real_dir_file,
				       date_type,
				       date);
}

static char *
desktop_directory_file_get_where_string (NautilusFile *file)
{
	return g_strdup (_("on the desktop"));
}


static void
monitor_destroy (gpointer data)
{
	DesktopMonitor *monitor = data;
	
	nautilus_file_monitor_remove
		(NAUTILUS_FILE (monitor->desktop_file->details->real_dir_file), monitor);
	g_free (monitor);
}

static char *
get_metadata_gconf_path (const char *name,
			 const char *key)
{
	char *res, *escaped_name;

	escaped_name = gconf_escape_key (name, -1);
	res = g_build_filename (NAUTILUS_DESKTOP_METADATA_GCONF_PATH, escaped_name, key, NULL);
	g_free (escaped_name);

	return res;
}

void
nautilus_desktop_set_metadata_string (NautilusFile *file,
				      const char *name,
				      const char *key,
				      const char *string)
{
	GConfClient *client;
	char *gconf_key;

	client = gconf_client_get_default ();
	gconf_key = get_metadata_gconf_path (name, key);

	if (string) {
		gconf_client_set_string (client, gconf_key, string, NULL);
	} else {
		gconf_client_unset (client, gconf_key, NULL);
	}

	g_free (gconf_key);
	g_object_unref (client);

	if (nautilus_desktop_update_metadata_from_gconf (file, name)) {
		nautilus_file_changed (file);
	}
}

void
nautilus_desktop_set_metadata_stringv (NautilusFile *file,
				       const char *name,
				       const char *key,
				       char **stringv)
{
	GConfClient *client;
	char *gconf_key;
	GSList *list;
	int i;

	client = gconf_client_get_default ();
	gconf_key = get_metadata_gconf_path (name, key);

	list = NULL;
	for (i = 0; stringv[i] != NULL; i++) {
		list = g_slist_prepend (list, stringv[i]);
	}
	list = g_slist_reverse (list);

	gconf_client_set_list (client, gconf_key,
			       GCONF_VALUE_STRING,
			       list, NULL);

	g_slist_free (list);
	g_free (gconf_key);
	g_object_unref (client);

	if (nautilus_desktop_update_metadata_from_gconf (file, name)) {
		nautilus_file_changed (file);
	}
}

gboolean
nautilus_desktop_update_metadata_from_gconf (NautilusFile *file,
					     const char *name)
{
	GConfClient *client;
	GSList *entries, *l;
	char *dir;
	const char *key;
	GConfEntry *entry;
	GConfValue *value;
	GFileInfo *info;
	gboolean changed;
	char *gio_key;
	GSList *value_list;
	char **strv;
	int i;

	client = gconf_client_get_default ();

	dir = get_metadata_gconf_path (name, NULL);
	entries = gconf_client_all_entries (client, dir, NULL);
	g_free (dir);

	info = g_file_info_new ();

	for (l = entries; l != NULL; l = l->next) {
		entry = l->data;

		key = gconf_entry_get_key (entry);
		value = gconf_entry_get_value (entry);

                if (value == NULL) {
			continue;
		}
		key = strrchr (key, '/') + 1;

		gio_key = g_strconcat ("metadata::", key, NULL);
		if (value->type == GCONF_VALUE_STRING) {
			g_file_info_set_attribute_string (info, gio_key,
							  gconf_value_get_string (value));
		} else if (value->type == GCONF_VALUE_LIST &&
			   gconf_value_get_list_type (value) == GCONF_VALUE_STRING) {
			value_list = gconf_value_get_list (value);
			strv = g_new (char *, g_slist_length (value_list) + 1);
			for (i = 0; value_list != NULL; i++, value_list = value_list->next) {
				strv[i] = l->data;
			}
			strv[i] = NULL;
			g_file_info_set_attribute_stringv (info, gio_key, strv);
			g_free (strv);
		}

		g_free (gio_key);

		gconf_entry_unref (entry);
	}
	g_slist_free (entries);

	changed = nautilus_file_update_metadata_from_info (file, info);

	g_object_unref (info);
	g_object_unref (client);

	return changed;
}

static void
nautilus_desktop_directory_file_set_metadata (NautilusFile           *file,
					      const char             *key,
					      const char             *value)
{
	nautilus_desktop_set_metadata_string (file, "directory", key, value);
}

static void
nautilus_desktop_directory_file_set_metadata_as_list (NautilusFile           *file,
						      const char             *key,
						      char                  **value)
{
	nautilus_desktop_set_metadata_stringv (file, "directory", key, value);
}

static void
nautilus_desktop_directory_file_init (gpointer object, gpointer klass)
{
	NautilusDesktopDirectoryFile *desktop_file;
	NautilusDesktopDirectory *desktop_directory;
	NautilusDirectory *real_dir;
	NautilusFile *real_dir_file;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (object);

	desktop_directory = NAUTILUS_DESKTOP_DIRECTORY (nautilus_directory_get_by_uri (EEL_DESKTOP_URI));

	desktop_file->details = g_new0 (NautilusDesktopDirectoryFileDetails, 1);
	desktop_file->details->desktop_directory = desktop_directory;

	desktop_file->details->callbacks = g_hash_table_new
		(desktop_callback_hash, desktop_callback_equal);
	desktop_file->details->monitors = g_hash_table_new_full (NULL, NULL,
								 NULL, monitor_destroy);

	real_dir = nautilus_desktop_directory_get_real_directory (desktop_directory);
	real_dir_file = nautilus_directory_get_corresponding_file (real_dir);
	nautilus_directory_unref (real_dir);
	
	desktop_file->details->real_dir_file = real_dir_file;

	nautilus_desktop_update_metadata_from_gconf (NAUTILUS_FILE (desktop_file), "directory");

	g_signal_connect_object (real_dir_file, "changed",
				 G_CALLBACK (real_file_changed_callback), desktop_file, 0);
}


static void
desktop_callback_remove_file_cover (gpointer key,
				    gpointer value,
				    gpointer callback_data)
{
	desktop_callback_remove_file
		(value, NAUTILUS_FILE (callback_data));
}


static void
desktop_finalize (GObject *object)
{
	NautilusDesktopDirectoryFile *desktop_file;
	NautilusDesktopDirectory *desktop_directory;

	desktop_file = NAUTILUS_DESKTOP_DIRECTORY_FILE (object);
	desktop_directory = desktop_file->details->desktop_directory;

	/* Todo: ghash now safe? */
	eel_g_hash_table_safe_for_each
		(desktop_file->details->callbacks,
		 desktop_callback_remove_file_cover,
		 desktop_file->details->real_dir_file);
	
	if (g_hash_table_size (desktop_file->details->callbacks) != 0) {
		g_warning ("call_when_ready still pending when desktop virtual file is destroyed");
	}

	g_hash_table_destroy (desktop_file->details->callbacks);
	g_hash_table_destroy (desktop_file->details->monitors);

	nautilus_file_unref (desktop_file->details->real_dir_file);

	g_free (desktop_file->details);

	nautilus_directory_unref (NAUTILUS_DIRECTORY (desktop_directory));

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_desktop_directory_file_class_init (gpointer klass)
{
	GObjectClass *object_class;
	NautilusFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NAUTILUS_FILE_CLASS (klass);
	
	object_class->finalize = desktop_finalize;

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;
	
	file_class->monitor_add = desktop_directory_file_monitor_add;
	file_class->monitor_remove = desktop_directory_file_monitor_remove;
	file_class->call_when_ready = desktop_directory_file_call_when_ready;
	file_class->cancel_call_when_ready = desktop_directory_file_cancel_call_when_ready;
	file_class->check_if_ready = desktop_directory_file_check_if_ready;
	file_class->get_item_count = desktop_directory_file_get_item_count;
	file_class->get_deep_counts = desktop_directory_file_get_deep_counts;
	file_class->get_date = desktop_directory_file_get_date;
	file_class->get_where_string = desktop_directory_file_get_where_string;
	file_class->set_metadata = nautilus_desktop_directory_file_set_metadata;
	file_class->set_metadata_as_list = nautilus_desktop_directory_file_set_metadata_as_list;
}
