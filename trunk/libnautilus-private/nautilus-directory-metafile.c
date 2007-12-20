/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-metafile.c: Nautilus directory model.
 
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
  
   Authors: Darin Adler <darin@bentspoon.com>,
            Mike Engber <engber@eazel.com>
*/

#include <config.h>
#include "nautilus-directory-metafile.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"
#include "nautilus-metafile.h"
#include <eel/eel-debug.h>
#include <eel/eel-string.h>
#include <stdio.h>


static NautilusMetafile *
get_metafile (NautilusDirectory *directory)
{
	char *uri;

	if (directory->details->metafile == NULL) {
		uri = nautilus_directory_get_uri (directory);
		directory->details->metafile = nautilus_metafile_get_for_uri (uri);
		g_free (uri);
	}

	return directory->details->metafile;	
}

gboolean
nautilus_directory_is_metadata_read (NautilusDirectory *directory)
{
	NautilusMetafile *metafile;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	
	metafile = get_metafile (directory);

	if (metafile == NULL) {
		return TRUE;
	}
		
	return nautilus_metafile_is_read (metafile);
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata)
{
	NautilusMetafile *metafile;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), g_strdup (default_metadata));
	g_return_val_if_fail (!eel_str_is_empty (file_name), g_strdup (default_metadata));
	g_return_val_if_fail (!eel_str_is_empty (key), g_strdup (default_metadata));
	
	metafile = get_metafile (directory);

	return nautilus_metafile_get (metafile, file_name, key, default_metadata);
}

GList *
nautilus_directory_get_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey)
{
	NautilusMetafile *     metafile;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (!eel_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!eel_str_is_empty (list_key), NULL);
	g_return_val_if_fail (!eel_str_is_empty (list_subkey), NULL);
	
	metafile = get_metafile (directory);
	return nautilus_metafile_get_list (metafile, file_name, list_key, list_subkey);
}

void
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata,
				      const char *metadata)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (!eel_str_is_empty (file_name));
	g_return_if_fail (!eel_str_is_empty (key));
	
	metafile = get_metafile (directory);
	nautilus_metafile_set (metafile, file_name, key, default_metadata, metadata);
}

void
nautilus_directory_set_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey,
					   GList *list)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (!eel_str_is_empty (file_name));
	g_return_if_fail (!eel_str_is_empty (list_key));
	g_return_if_fail (!eel_str_is_empty (list_subkey));

	metafile = get_metafile (directory);
	nautilus_metafile_set_list (metafile,
				    file_name,
				    list_key,
				    list_subkey,
				    list);
}

gboolean 
nautilus_directory_get_boolean_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      gboolean default_metadata)
{
	char *result_as_string;
	gboolean result;

	result_as_string = nautilus_directory_get_file_metadata
		(directory, file_name, key,
		 default_metadata ? "true" : "false");
	g_assert (result_as_string != NULL);
	
	if (g_ascii_strcasecmp (result_as_string, "true") == 0) {
		result = TRUE;
	} else if (g_ascii_strcasecmp (result_as_string, "false") == 0) {
		result = FALSE;
	} else {
		g_error ("boolean metadata with value other than true or false");
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;
}

void
nautilus_directory_set_boolean_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      gboolean default_metadata,
					      gboolean metadata)
{
	nautilus_directory_set_file_metadata
		(directory, file_name, key,
		 default_metadata ? "true" : "false",
		 metadata ? "true" : "false");
}

int 
nautilus_directory_get_integer_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      int default_metadata)
{
	char *result_as_string;
	char default_as_string[32];
	int result;
	char c;

	g_snprintf (default_as_string, sizeof (default_as_string), "%d", default_metadata);
	result_as_string = nautilus_directory_get_file_metadata
		(directory, file_name, key, default_as_string);

	/* Normally we can't get a a NULL, but we check for it here to
	 * handle the oddball case of a non-existent directory.
	 */
	if (result_as_string == NULL) {
		result = default_metadata;
	} else {
		if (sscanf (result_as_string, " %d %c", &result, &c) != 1) {
			result = default_metadata;
		}
		g_free (result_as_string);
	}

	return result;
}

void
nautilus_directory_set_integer_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      int default_metadata,
					      int metadata)
{
	char value_as_string[32];
	char default_as_string[32];

	g_snprintf (value_as_string, sizeof (value_as_string), "%d", metadata);
	g_snprintf (default_as_string, sizeof (default_as_string), "%d", default_metadata);

	nautilus_directory_set_file_metadata
		(directory, file_name, key,
		 default_as_string, value_as_string);
}

void
nautilus_directory_copy_file_metadata (NautilusDirectory *source_directory,
				       const char *source_file_name,
				       NautilusDirectory *destination_directory,
				       const char *destination_file_name)
{
	char *destination_uri;
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (source_directory));
	g_return_if_fail (source_file_name != NULL);
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (destination_directory));
	g_return_if_fail (destination_file_name != NULL);

	metafile = get_metafile (source_directory);
	destination_uri = nautilus_directory_get_uri (destination_directory);

	nautilus_metafile_copy (metafile, source_file_name,
				destination_uri, destination_file_name);

	g_free (destination_uri);
}

void
nautilus_directory_remove_file_metadata (NautilusDirectory *directory,
					 const char *file_name)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (file_name != NULL);

	metafile = get_metafile (directory);
	nautilus_metafile_remove (metafile, file_name);
}

void
nautilus_directory_rename_file_metadata (NautilusDirectory *directory,
					 const char *old_file_name,
					 const char *new_file_name)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (old_file_name != NULL);
	g_return_if_fail (new_file_name != NULL);

	metafile = get_metafile (directory);
	nautilus_metafile_rename (metafile, old_file_name,new_file_name);
}

void
nautilus_directory_rename_directory_metadata (NautilusDirectory *directory,
					      const char *new_directory_uri)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (new_directory_uri != NULL);

	metafile = get_metafile (directory);
	nautilus_metafile_rename_directory (metafile, new_directory_uri);
}

static void
metafile_changed_cb (NautilusMetafile *metafile,
		     GList *file_names,
		     NautilusDirectory *directory)
{
	GList                   *file_list;
	NautilusFile		*file;
	
	file_list = NULL;
	while (file_names != NULL) {
		file = nautilus_directory_find_file_by_internal_filename
			(directory, file_names->data);

		if (file != NULL) {
			if (nautilus_file_is_self_owned (file)) {
				nautilus_file_emit_changed (file);
			} else {
				file_list = g_list_prepend (file_list, file);
			}
		}
		file_names = file_names->next;
	}
	
	if (file_list != NULL) {
		file_list = g_list_reverse (file_list);
		nautilus_directory_emit_change_signals (directory, file_list);
		g_list_free (file_list);
	}
}

static void
metafile_ready_cb (NautilusMetafile *metafile,
		   NautilusDirectory *directory)
{
	emit_change_signals_for_all_files (directory);
	nautilus_directory_async_state_changed (directory);
}


void
nautilus_directory_register_metadata_monitor (NautilusDirectory *directory)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	if (directory->details->metafile_monitored) {
		return;
	}

	metafile = get_metafile (directory);
	directory->details->metafile_monitored = TRUE;

	g_signal_connect (metafile, "changed", (GCallback)metafile_changed_cb, directory);
	g_signal_connect (metafile, "ready", (GCallback)metafile_ready_cb, directory);
	
	nautilus_metafile_load (metafile);
}

void
nautilus_directory_unregister_metadata_monitor (NautilusDirectory *directory)
{
	NautilusMetafile *metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));	

	directory->details->metafile_monitored = FALSE;

	metafile = get_metafile (directory);
	g_signal_handlers_disconnect_by_func (metafile, metafile_changed_cb, directory);
	g_signal_handlers_disconnect_by_func (metafile, metafile_ready_cb, directory);
}
