/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities..c - implementation of file manipulation routines.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-lib-self-check-functions.h"
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "nautilus-file.h"
#include "nautilus-link-set.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

#define NAUTILUS_USER_MAIN_DIRECTORY_NAME "Nautilus"

#define DEFAULT_SCHEME "file://"

#define READ_CHUNK_SIZE 8192

struct NautilusReadFileHandle {
	GnomeVFSAsyncHandle *handle;
	NautilusReadFileCallback callback;
	NautilusReadMoreCallback read_more_callback;
	gpointer callback_data;
	gboolean is_open;
	char *buffer;
	int bytes_read;
};

static void read_file_read_chunk (NautilusReadFileHandle *handle);

/**
 * nautilus_format_uri_for_display:
 *
 * Filter, modify, unescape and change URIs to make them appropriate
 * to display to users.
 *
 * @uri: a URI
 *
 * returns a g_malloc'd string
 **/
char *
nautilus_format_uri_for_display (const char *uri) 
{
	gchar *toreturn, *unescaped;

	g_assert (uri != NULL);

	unescaped = gnome_vfs_unescape_string_for_display (uri);
	
	/* Remove file:// from the beginning */
	if (nautilus_istr_has_prefix (uri, DEFAULT_SCHEME)) {
		toreturn = g_strdup (unescaped + sizeof (DEFAULT_SCHEME) - 1);
	} else {
		toreturn = g_strdup (unescaped);
	}
	
	g_free (unescaped);

	return toreturn;
}

/**
 * nautilus_make_uri_from_input:
 *
 * Takes a user input path/URI and makes a valid URI
 * out of it
 *
 * @location: a possibly mangled "uri"
 *
 * returns a newly allocated uri
 *
 **/
char *
nautilus_make_uri_from_input (const char *location)
{
	char *toreturn, *escaped;
	const char *no_method;
	int method_length;

	if (location[0] == '/') {
		escaped = gnome_vfs_escape_path_string (location);
		toreturn = g_strconcat (DEFAULT_SCHEME, escaped, NULL);
		g_free (escaped);
	} else {
		no_method = strchr (location, ':');
		if (no_method == NULL) {
			no_method = location;
		} else {
			no_method++;
			if ((no_method[0] == '/') && (no_method[1] == '/')) {
				no_method += 2;
			}
		}

		method_length = (no_method - location);
		escaped = gnome_vfs_escape_host_and_path_string (no_method);
		toreturn = g_new (char, strlen (escaped) + method_length + 1);
		toreturn[0] = '\0';
		strncat (toreturn, location, method_length);
		strcat (toreturn, escaped);
		g_free (escaped);
	}

	return toreturn;
}

/**
 * nautilus_make_path:
 * 
 * Make a path name from a base path and name. The base path
 * can end with or without a separator character.
 *
 * Return value: the combined path name.
 **/
char * 
nautilus_make_path (const char *path, const char* name)
{
    	gboolean insert_separator;
    	int path_length;
	char *result;
	
	path_length = strlen (path);
    	insert_separator = path_length > 0 && 
    			   name[0] != '\0' && 
    			   path[path_length - 1] != G_DIR_SEPARATOR;

    	if (insert_separator) {
    		result = g_strconcat (path, G_DIR_SEPARATOR_S, name, NULL);
    	} else {
    		result = g_strconcat (path, name, NULL);
    	}

	return result;
}

/**
 * nautilus_get_user_directory:
 * 
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_directory (void)
{
	char *user_directory = NULL;

	user_directory = nautilus_make_path (g_get_home_dir (),
					     NAUTILUS_USER_DIRECTORY_NAME);

	if (!g_file_exists (user_directory)) {
		mkdir (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
	}

	return user_directory;
}

/**
 * nautilus_get_desktop_directory:
 * 
 * Get the path for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_desktop_directory (void)
{
	char *desktop_directory, *user_directory;

	user_directory = nautilus_get_user_directory ();
	desktop_directory = nautilus_make_path (user_directory, DESKTOP_DIRECTORY_NAME);
	g_free (user_directory);

	if (!g_file_exists (desktop_directory)) {
		mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
	}

	return desktop_directory;
}

/**
  * nautilus_user_main_directory_exists:
  *
  * returns true if the user directory exists.  This must be called
  * before nautilus_get_user_main_directory, which creates it if necessary
  *
  **/
gboolean
nautilus_user_main_directory_exists(void)
{
	gboolean directory_exists;
	char *main_directory;
	
	main_directory = g_strdup_printf ("%s/%s",
					g_get_home_dir(),
					NAUTILUS_USER_MAIN_DIRECTORY_NAME);
	directory_exists = g_file_exists(main_directory);
	g_free(main_directory);
	return directory_exists;
}

/**
 * nautilus_get_user_main_directory:
 * 
 * Get the path for the user's main Nautilus directory.  
 * Usually ~/Nautilus
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_main_directory (void)
{
	char *user_main_directory = NULL;
	GnomeVFSResult result;
	NautilusFile *file;
	char *file_uri, *image_uri, *temp_str;
	char *source_directory_uri, *destination_directory_uri;
	GList *source_name_list, *destination_name_list;
	
	user_main_directory = g_strdup_printf ("%s/%s",
					       g_get_home_dir(),
					       NAUTILUS_USER_MAIN_DIRECTORY_NAME);
												
	if (!g_file_exists (user_main_directory)) {			
		source_directory_uri = nautilus_get_uri_from_local_path (NAUTILUS_DATADIR);
		destination_directory_uri = nautilus_get_uri_from_local_path (g_get_home_dir());
		source_name_list = g_list_prepend (NULL, "top");
		destination_name_list = g_list_prepend (NULL, NAUTILUS_USER_MAIN_DIRECTORY_NAME);
		
		result = gnome_vfs_xfer (source_directory_uri, source_name_list,
					 destination_directory_uri, destination_name_list,
					 GNOME_VFS_XFER_RECURSIVE, GNOME_VFS_XFER_ERROR_MODE_ABORT,
					 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
					 NULL, NULL);
		
		g_free (source_directory_uri);
		g_free (destination_directory_uri);
		g_list_free (source_name_list);
		g_list_free (destination_name_list);
		
		/* FIXME bugzilla.eazel.com 1286: 
		 * Is a g_warning good enough here? This seems like a big problem. 
		 */
		if (result != GNOME_VFS_OK) {
			g_warning ("could not install the novice home directory.  Make sure you typed 'make install'");
		}
					
		/* assign a custom image for the directory icon */
		file_uri = nautilus_get_uri_from_local_path (user_main_directory);
		temp_str = nautilus_pixmap_file ("nautilus-logo.png");
		image_uri = nautilus_get_uri_from_local_path (temp_str);
		g_free (temp_str);
		
		file = nautilus_file_get (file_uri);
		g_free (file_uri);
		if (file != NULL) {
			nautilus_file_set_metadata (file,
						    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
						    NULL,
						    image_uri);
			nautilus_file_unref (file);
		}
		
		/* now do the same for the about file */
		temp_str = g_strdup_printf ("%s/About.html", user_main_directory);
		file_uri = nautilus_get_uri_from_local_path (temp_str);
		g_free (temp_str);
		
		file = nautilus_file_get (file_uri);
		if (file != NULL) {
			nautilus_file_set_metadata (file,
						    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
						    NULL,
						    image_uri);
			nautilus_file_unref (file);
		}
		g_free (file_uri);
		
		g_free (image_uri);
		
		/* install the default link set */
		nautilus_link_set_install(user_main_directory, "apps");
		/*
		  nautilus_link_set_install(user_main_directory, "search_engines");
		*/
	}

	return user_main_directory;
}

/**
 * nautilus_get_pixmap_directory
 * 
 * Get the path for the directory containing Nautilus pixmaps.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_pixmap_directory (void)
{
	char *pixmap_directory;

	pixmap_directory = g_strdup_printf ("%s/%s", DATADIR, "pixmaps/nautilus");

	return pixmap_directory;
}

/**
 * nautilus_get_local_path_from_uri:
 * 
 * Return a local path for a file:// URI.
 *
 * Return value: the local path or NULL on error.
 **/
char *
nautilus_get_local_path_from_uri (const char *uri)
{
	char *result, *unescaped_uri;

	if (uri == NULL) {
		return NULL;
	}

	unescaped_uri = gnome_vfs_unescape_string (uri, "/");

	if (unescaped_uri == NULL) {
		return NULL;
	}

	if (nautilus_istr_has_prefix (unescaped_uri, "file://")) {
		result = g_strdup (unescaped_uri + 7);
	} else if (unescaped_uri[0] == '/') {
		result = g_strdup (unescaped_uri);
	} else {
		result = NULL;
	}

	g_free (unescaped_uri);

	return result;
}

/**
 * nautilus_get_uri_from_local_path:
 * 
 * Return a file:// URI for a local path.
 *
 * Return value: the URI (NULL for some bad errors).
 **/
char *
nautilus_get_uri_from_local_path (const char *local_path)
{
	char *escaped_path, *result;
	
	if (local_path == NULL) {
		return NULL;
	}

	g_return_val_if_fail (local_path[0] == '/', NULL);

	escaped_path = gnome_vfs_escape_path_string (local_path);
	result = g_strconcat ("file://", escaped_path, NULL);
	g_free (escaped_path);
	return result;
}

/* convenience routine to use gnome-vfs to test if a string is a remote uri */
gboolean
nautilus_is_remote_uri (const char *uri)
{
	gboolean is_local;
	GnomeVFSURI *vfs_uri;
	
	vfs_uri = gnome_vfs_uri_new(uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref(vfs_uri);
	return !is_local;
}


/* FIXME: Callers just use this and dereference so we core dump if
 * pixmaps are missing. That is lame.
 */
char *
nautilus_pixmap_file (const char *partial_path)
{
	char *path;

	path = nautilus_make_path (DATADIR "/pixmaps/nautilus", partial_path);
	if (g_file_exists (path)) {
		return path;
	} else {
		g_free (path);
		return NULL;
	}
}

GnomeVFSResult
nautilus_read_entire_file (const char *uri,
			   int *file_size,
			   char **file_contents)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *buffer;
	int total_bytes_read;
	GnomeVFSFileSize bytes_read;

	*file_size = 0;
	*file_contents = NULL;

	/* Open the file. */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	/* Read the whole thing. */
	buffer = NULL;
	total_bytes_read = 0;
	do {
		buffer = g_realloc (buffer, total_bytes_read + READ_CHUNK_SIZE);
		result = gnome_vfs_read (handle,
					 buffer + total_bytes_read,
					 READ_CHUNK_SIZE,
					 &bytes_read);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			g_free (buffer);
			return result;
		}

		/* Check for overflow. */
		if (total_bytes_read + bytes_read < total_bytes_read) {
			g_free (buffer);
			return GNOME_VFS_ERROR_TOO_BIG;
		}

		total_bytes_read += bytes_read;
	} while (result == GNOME_VFS_OK);

	/* Close the file. */
	result = gnome_vfs_close (handle);
	if (result != GNOME_VFS_OK) {
		g_free (buffer);
		return result;
	}

	/* Return the file. */
	*file_size = total_bytes_read;
	*file_contents = g_realloc (buffer, total_bytes_read);
	return GNOME_VFS_OK;
}

/* When close is complete, there's no more work to do. */
static void
read_file_close_callback (GnomeVFSAsyncHandle *handle,
				 GnomeVFSResult result,
				 gpointer callback_data)
{
	if (result != GNOME_VFS_OK) {
		g_warning ("close failed -- this should never happen");
	}
}

/* Do a close if it's needed.
 * Be sure to get this right, or we have extra threads hanging around.
 */
static void
read_file_close (NautilusReadFileHandle *read_handle)
{
	if (read_handle->is_open) {
		gnome_vfs_async_close (read_handle->handle,
				       read_file_close_callback,
				       NULL);
		read_handle->is_open = FALSE;
	}
}

/* Close the file and then tell the caller we succeeded, handing off
 * the buffer to the caller.
 */
static void
read_file_succeeded (NautilusReadFileHandle *read_handle)
{
	read_file_close (read_handle);
	
	/* Reallocate the buffer to the exact size since it might be
	 * around for a while.
	 */
	(* read_handle->callback) (GNOME_VFS_OK,
				   read_handle->bytes_read,
				   g_realloc (read_handle->buffer,
					      read_handle->bytes_read),
				   read_handle->callback_data);

	g_free (read_handle);
}

/* Tell the caller we failed. */
static void
read_file_failed (NautilusReadFileHandle *read_handle, GnomeVFSResult result)
{
	read_file_close (read_handle);
	g_free (read_handle->buffer);
	
	(* read_handle->callback) (result, 0, NULL, read_handle->callback_data);
	g_free (read_handle);
}

/* A read is complete, so we might or might not be done. */
static void
read_file_read_callback (GnomeVFSAsyncHandle *handle,
				GnomeVFSResult result,
				gpointer buffer,
				GnomeVFSFileSize bytes_requested,
				GnomeVFSFileSize bytes_read,
				gpointer callback_data)
{
	NautilusReadFileHandle *read_handle;
	gboolean read_more;

	/* Do a few reality checks. */
	g_assert (bytes_requested == READ_CHUNK_SIZE);
	read_handle = callback_data;
	g_assert (read_handle->handle == handle);
	g_assert (read_handle->buffer + read_handle->bytes_read == buffer);
	g_assert (bytes_read <= bytes_requested);

	/* Check for a failure. */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		read_file_failed (read_handle, result);
		return;
	}

	/* Check for the extremely unlikely case where the file size overflows. */
	if (read_handle->bytes_read + bytes_read < read_handle->bytes_read) {
		read_file_failed (read_handle, GNOME_VFS_ERROR_TOO_BIG);
		return;
	}

	/* Bump the size. */
	read_handle->bytes_read += bytes_read;

	/* Read more unless we are at the end of the file. */
	if (bytes_read == 0 || result != GNOME_VFS_OK) {
		read_more = FALSE;
	} else {
		if (read_handle->read_more_callback == NULL) {
			read_more = TRUE;
		} else {
			read_more = (* read_handle->read_more_callback)
				(read_handle->bytes_read,
				 read_handle->buffer,
				 read_handle->callback_data);
		}
	}
	if (read_more) {
		read_file_read_chunk (read_handle);
		return;
	}

	/* If at the end of the file, we win! */
	read_file_succeeded (read_handle);
}

/* Start reading a chunk. */
static void
read_file_read_chunk (NautilusReadFileHandle *handle)
{
	handle->buffer = g_realloc (handle->buffer, handle->bytes_read + READ_CHUNK_SIZE);
	gnome_vfs_async_read (handle->handle,
			      handle->buffer + handle->bytes_read,
			      READ_CHUNK_SIZE,
			      read_file_read_callback,
			      handle);
}

/* Once the open is finished, read a first chunk. */
static void
read_file_open_callback (GnomeVFSAsyncHandle *handle,
			 GnomeVFSResult result,
			 gpointer callback_data)
{
	NautilusReadFileHandle *read_handle;
	
	read_handle = callback_data;
	g_assert (read_handle->handle == handle);

	/* Handle the failure case. */
	if (result != GNOME_VFS_OK) {
		read_file_failed (read_handle, result);
		return;
	}

	/* Handle success by reading the first chunk. */
	read_handle->is_open = TRUE;
	read_file_read_chunk (read_handle);
}

/* Set up the read handle and start reading. */
NautilusReadFileHandle *
nautilus_read_file_async (const char *uri,
			  NautilusReadFileCallback callback,
			  NautilusReadMoreCallback read_more_callback,
			  gpointer callback_data)
{
	NautilusReadFileHandle *handle;

	handle = g_new0 (NautilusReadFileHandle, 1);

	handle->callback = callback;
	handle->read_more_callback = read_more_callback;
	handle->callback_data = callback_data;

	gnome_vfs_async_open (&handle->handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      read_file_open_callback,
			      handle);

	return handle;
}

/* Set up the read handle and start reading. */
NautilusReadFileHandle *
nautilus_read_entire_file_async (const char *uri,
				 NautilusReadFileCallback callback,
				 gpointer callback_data)
{
	return nautilus_read_file_async (uri, callback, NULL, callback_data);
}

/* Stop the presses! */
void
nautilus_read_file_cancel (NautilusReadFileHandle *handle)
{
	gnome_vfs_async_cancel (handle->handle);
	read_file_close (handle);
	g_free (handle->buffer);
	g_free (handle);
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_utilities (void)
{
	char *tmp;

	/* check to make sure the current implementation doesn't cause problems     */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu"), "http://null.stanford.edu");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu:80"), "http://null.stanford.edu:80");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://seth@null.stanford.edu:80"), "http://seth@null.stanford.edu:80");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu/some file"), "http://null.stanford.edu/some%20file");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("file:///home/joe/some file"), "file:///home/joe/some%20file");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("file://home/joe/some file"), "file://home/joe/some%20file");

	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("foo://foobar.txt"), "foo://foobar.txt");

	/* now we test input we just want to make sure doesn't crash the function */
	tmp = nautilus_make_uri_from_input (":://:://:::::::::::::::::");
	g_free (tmp);		
	tmp = nautilus_make_uri_from_input ("file:::::////");
	g_free (tmp);
	tmp = nautilus_make_uri_from_input ("http:::::::::");
	g_free (tmp);
	tmp = nautilus_make_uri_from_input ("::");
	g_free (tmp);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
