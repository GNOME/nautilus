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
  
   Authors: Darin Adler <darin@eazel.com>,
            Mike Engber <engber@eazel.com>
*/

#include <config.h>
#include "nautilus-directory-metafile.h"

#include <libnautilus-extensions/nautilus-metafile-factory.h>
#include <libnautilus-extensions/nautilus-metafile-server.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <liboaf/liboaf.h>
#include <stdio.h>

static Nautilus_MetafileFactory factory = CORBA_OBJECT_NIL;
static gboolean get_factory_from_oaf = TRUE;

void
nautilus_directory_use_self_contained_metafile_factory (void)
{
	g_return_if_fail (factory == CORBA_OBJECT_NIL);

	get_factory_from_oaf = FALSE;
}

static void
free_factory (void)
{
	bonobo_object_release_unref (factory, NULL);
}

static Nautilus_MetafileFactory
get_factory (void)
{
	NautilusMetafileFactory *instance;

	if (factory == CORBA_OBJECT_NIL) {
		if (get_factory_from_oaf) {
			factory = oaf_activate_from_id (METAFILE_FACTORY_IID, 0, NULL, NULL);
		} else {
			instance = nautilus_metafile_factory_get_instance ();
			factory = bonobo_object_dup_ref (bonobo_object_corba_objref (BONOBO_OBJECT (instance)), NULL);
			bonobo_object_unref (BONOBO_OBJECT (instance));
		}
		g_atexit (free_factory);
	}

	return factory;
}

static Nautilus_Metafile
get_metafile (NautilusDirectory *directory, CORBA_Environment *ev)
{
	char *uri;
	Nautilus_Metafile metafile;

	uri = nautilus_directory_get_uri (directory);
	metafile = Nautilus_MetafileFactory_open (get_factory (), uri, ev);
	g_free (uri);
	
	return metafile;	
}

char *
nautilus_directory_get_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	char       *result;
	const char *non_null_default;
	CORBA_char *corba_value;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (key), NULL);
	
	/* We can't pass NULL as a CORBA_string - pass "" instead. */
	non_null_default = default_metadata != NULL ? default_metadata : "";

	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	corba_value = Nautilus_Metafile_get (metafile, file_name, key, non_null_default, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	if (nautilus_str_is_empty (corba_value)) {
		/* Even though in all other respects we treat "" as NULL, we want to
		 * make sure the caller gets back the same default that was passed in.
		 */
		result = g_strdup (default_metadata);
	} else {
		result = g_strdup (corba_value);
	}

	CORBA_free (corba_value);
	
	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);

	return result;
}

GList *
nautilus_directory_get_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	GList                 *result;
	Nautilus_MetadataList *corba_value;
	CORBA_unsigned_long    buf_pos;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (list_key), NULL);
	g_return_val_if_fail (!nautilus_str_is_empty (list_subkey), NULL);
	
	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	corba_value = Nautilus_Metafile_get_list (metafile, file_name, list_key, list_subkey, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	result = NULL;
	for (buf_pos = 0; buf_pos < corba_value->_length; ++buf_pos) {
		result = g_list_prepend (result, g_strdup (corba_value->_buffer [buf_pos]));
	}
	result = g_list_reverse (result);
	CORBA_free (corba_value);
	
	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);

	return result;
}

gboolean
nautilus_directory_set_file_metadata (NautilusDirectory *directory,
				      const char *file_name,
				      const char *key,
				      const char *default_metadata,
				      const char *metadata)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	gboolean result;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (key), FALSE);
	
	/* We can't pass NULL as a CORBA_string - pass "" instead.
	 */
	if (default_metadata == NULL) {
		default_metadata = "";
	}
	if (metadata == NULL) {
		metadata = "";
	}

	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	result = Nautilus_Metafile_set (metafile, file_name, key, default_metadata, metadata, &ev);
	
	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);

	return result;
}

gboolean
nautilus_directory_set_file_metadata_list (NautilusDirectory *directory,
					   const char *file_name,
					   const char *list_key,
					   const char *list_subkey,
					   GList *list)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	gboolean result;
	
	Nautilus_MetadataList *corba_list;
	int	len;
	int	buf_pos;
	GList   *list_ptr;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (file_name), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (list_key), FALSE);
	g_return_val_if_fail (!nautilus_str_is_empty (list_subkey), FALSE);
	
	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	len = g_list_length (list);
	
	corba_list = Nautilus_MetadataList__alloc ();
	corba_list->_maximum = len;
	corba_list->_length  = len;
	corba_list->_buffer  = CORBA_sequence_CORBA_string_allocbuf (len);

	/* We allocate our buffer with CORBA calls, so CORBA_free will clean it
	 * all up if we set release to TRUE.
	 */
	CORBA_sequence_set_release (corba_list, CORBA_TRUE);

	buf_pos  = 0;
	list_ptr = list;
	while (list_ptr != NULL) {
		corba_list->_buffer [buf_pos] = CORBA_string_dup (list_ptr->data);
		list_ptr = g_list_next (list_ptr);
		++buf_pos;
	}

	result = Nautilus_Metafile_set_list (metafile, file_name, list_key, list_subkey, corba_list, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	CORBA_free (corba_list);

	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);

	return result;
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
	
	g_strdown (result_as_string);
	if (strcmp (result_as_string, "true") == 0) {
		result = TRUE;
	} else if (strcmp (result_as_string, "false") == 0) {
		result = FALSE;
	} else {
		if (result_as_string != NULL) {
			g_warning ("boolean metadata with value other than true or false");
		}
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;
}

gboolean
nautilus_directory_set_boolean_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      gboolean default_metadata,
					      gboolean metadata)
{
	return nautilus_directory_set_file_metadata
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
	char *default_as_string;
	int result;

	default_as_string = g_strdup_printf ("%d", default_metadata);
	result_as_string = nautilus_directory_get_file_metadata
		(directory, file_name, key, default_as_string);

	/* Normally we can't get a a NULL, but we check for it here to
	 * handle the oddball case of a non-existent directory.
	 */
	if (result_as_string == NULL) {
		result = default_metadata;
	} else {
		if (sscanf (result_as_string, " %d %*s", &result) != 1) {
			result = default_metadata;
		}
		g_free (result_as_string);
	}

	g_free (default_as_string);
	return result;
}

gboolean
nautilus_directory_set_integer_file_metadata (NautilusDirectory *directory,
					      const char *file_name,
					      const char *key,
					      int default_metadata,
					      int metadata)
{
	char *value_as_string;
	char *default_as_string;

	value_as_string = g_strdup_printf ("%d", metadata);
	default_as_string = g_strdup_printf ("%d", default_metadata);

	return nautilus_directory_set_file_metadata
		(directory, file_name, key,
		 default_as_string, value_as_string);

	g_free (value_as_string);
	g_free (default_as_string);
}

void
nautilus_directory_copy_file_metadata (NautilusDirectory *source_directory,
				       const char *source_file_name,
				       NautilusDirectory *destination_directory,
				       const char *destination_file_name)
{
	CORBA_Environment ev;
	Nautilus_Metafile source_metafile;
	char* destination_uri;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (source_directory));
	g_return_if_fail (source_file_name != NULL);
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (destination_directory));
	g_return_if_fail (destination_file_name != NULL);
	
	CORBA_exception_init (&ev);
	source_metafile = get_metafile (source_directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	destination_uri = nautilus_directory_get_uri (destination_directory);

	Nautilus_Metafile_copy (source_metafile, source_file_name, destination_uri, destination_file_name, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	g_free (destination_uri);
	bonobo_object_release_unref (source_metafile, NULL);
	CORBA_exception_free (&ev);
}

void
nautilus_directory_remove_file_metadata (NautilusDirectory *directory,
					 const char *file_name)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (file_name != NULL);
	
	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	Nautilus_Metafile_remove (metafile, file_name, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */
	
	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);
}

void
nautilus_directory_rename_file_metadata (NautilusDirectory *directory,
					 const char *old_file_name,
					 const char *new_file_name)
{
	CORBA_Environment ev;
	Nautilus_Metafile metafile;

	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
	g_return_if_fail (old_file_name != NULL);
	g_return_if_fail (new_file_name != NULL);
	
	CORBA_exception_init (&ev);
	metafile = get_metafile (directory, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */

	Nautilus_Metafile_rename (metafile, old_file_name, new_file_name, &ev);

	/* FIXME bugzilla.eazel.com 6664: examine ev for errors */
	
	bonobo_object_release_unref (metafile, NULL);
	CORBA_exception_free (&ev);
}
