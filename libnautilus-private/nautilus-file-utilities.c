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
#include <ctype.h>

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

gboolean
nautilus_uri_is_trash (const char *uri)
{
	return nautilus_istr_has_prefix (uri, "trash:")
		|| nautilus_istr_has_prefix (uri, "gnome-trash:");
}

char *
nautilus_make_uri_canonical (const char *uri)
{
	size_t length;
	char *canonical_uri, *old_uri, *with_slashes, *p;

	/* Convert "gnome-trash:<anything>" and "trash:<anything>" to
	 * "trash:".
	 */
	if (nautilus_uri_is_trash (uri)) {
		return g_strdup ("trash:");
	}

	/* FIXME bugzilla.eazel.com 648: 
	 * This currently ignores the issue of two uris that are not identical but point
	 * to the same data except for the specific cases of trailing '/' characters,
	 * file:/ and file:///, and "lack of file:".
	 */

	/* Strip the trailing "/" characters. */
	canonical_uri = nautilus_str_strip_trailing_chr (uri, '/');
	if (strcmp (canonical_uri, uri) != 0) {
		/* If some trailing '/' were stripped, there's the possibility,
		 * that we stripped away all the '/' from a uri that has only
		 * '/' characters. If you change this code, check to make sure
		 * that "file:///" still works as a URI.
		 */
		length = strlen (canonical_uri);
		if (length == 0 || canonical_uri[length - 1] == ':') {
			with_slashes = g_strconcat (canonical_uri, "///", NULL);
			g_free (canonical_uri);
			canonical_uri = with_slashes;
		}
	}

	/* Add file: if there is no scheme. */
	if (strchr (canonical_uri, ':') == NULL) {
		old_uri = canonical_uri;
		canonical_uri = g_strconcat ("file:", old_uri, NULL);
		g_free (old_uri);
	}

	/* Lower-case the scheme. */
	for (p = canonical_uri; *p != ':'; p++) {
		g_assert (*p != '\0');
		if (isupper (*p)) {
			*p = tolower (*p);
		}
	}

	/* Convert file:/ to file:/// */
	if (nautilus_str_has_prefix (canonical_uri, "file:/")
	    && !nautilus_str_has_prefix (canonical_uri, "file:///")) {
		old_uri = canonical_uri;
		canonical_uri = g_strconcat ("file://", old_uri + 5, NULL);
		g_free (old_uri);
	}

	return canonical_uri;
}

gboolean
nautilus_uris_match (const char *uri_1, const char *uri_2)
{
	char *canonical_1;
	char *canonical_2;
	gboolean result;

	canonical_1 = nautilus_make_uri_canonical (uri_1);
	canonical_2 = nautilus_make_uri_canonical (uri_2);

	result = nautilus_str_is_equal (canonical_1, canonical_2);

	g_free (canonical_1);
	g_free (canonical_2);
	
	return result;
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
	char *source_directory_uri_text, *destination_directory_uri_text;
	GnomeVFSURI *source_directory_uri, *destination_directory_uri;
	GnomeVFSURI *source_uri, *destination_uri;
	
	user_main_directory = g_strdup_printf ("%s/%s",
					       g_get_home_dir(),
					       NAUTILUS_USER_MAIN_DIRECTORY_NAME);
												
	if (!g_file_exists (user_main_directory)) {			
		source_directory_uri_text = gnome_vfs_get_uri_from_local_path (NAUTILUS_DATADIR);
		source_directory_uri = gnome_vfs_uri_new (source_directory_uri_text);
		g_free (source_directory_uri_text);
		source_uri = gnome_vfs_uri_append_path (source_directory_uri, "top");
		gnome_vfs_uri_unref (source_directory_uri);

		destination_directory_uri_text = gnome_vfs_get_uri_from_local_path (g_get_home_dir());
		destination_directory_uri = gnome_vfs_uri_new (destination_directory_uri_text);
		g_free (destination_directory_uri_text);
		destination_uri = gnome_vfs_uri_append_path (destination_directory_uri, 
			NAUTILUS_USER_MAIN_DIRECTORY_NAME);
		gnome_vfs_uri_unref (destination_directory_uri);
		
		result = gnome_vfs_xfer_uri (source_uri, destination_uri,
					 GNOME_VFS_XFER_RECURSIVE, GNOME_VFS_XFER_ERROR_MODE_ABORT,
					 GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
					 NULL, NULL);
		
		gnome_vfs_uri_unref (source_uri);
		gnome_vfs_uri_unref (destination_uri);
		
		/* FIXME bugzilla.eazel.com 1286: 
		 * Is a g_warning good enough here? This seems like a big problem. 
		 */
		if (result != GNOME_VFS_OK) {
			g_warning ("could not install the novice home directory.  Make sure you typed 'make install'");
		}
					
		/* assign a custom image for the directory icon */
		file_uri = gnome_vfs_get_uri_from_local_path (user_main_directory);
		temp_str = nautilus_pixmap_file ("nautilus-logo.png");
		image_uri = gnome_vfs_get_uri_from_local_path (temp_str);
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
		file_uri = gnome_vfs_get_uri_from_local_path (temp_str);
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
		nautilus_link_set_install (user_main_directory, "apps");
		/*
		  nautilus_link_set_install (user_main_directory, "search_engines");
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

/* convenience routine to use gnome-vfs to test if a string is a remote uri */
gboolean
nautilus_is_remote_uri (const char *uri)
{
	gboolean is_local;
	GnomeVFSURI *vfs_uri;
	
	vfs_uri = gnome_vfs_uri_new (uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref(vfs_uri);
	return !is_local;
}


/* FIXME bugzilla.eazel.com 2423: 
 * Callers just use this and dereference so we core dump if
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
			gnome_vfs_close (handle);
			return result;
		}

		/* Check for overflow. */
		if (total_bytes_read + bytes_read < total_bytes_read) {
			g_free (buffer);
			gnome_vfs_close (handle);
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
	/* nautilus_make_uri_from_input */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input (""), "");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu"), "http://null.stanford.edu");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu:80"), "http://null.stanford.edu:80");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://seth@null.stanford.edu:80"), "http://seth@null.stanford.edu:80");
        NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http://null.stanford.edu/some file"), "http://null.stanford.edu/some%20file");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("file:///home/joe/some file"), "file:///home/joe/some%20file");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("file://home/joe/some file"), "file://home/joe/some%20file");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("foo://foobar.txt"), "foo://foobar.txt");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("::"), "::");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input (":://:://:::::::::::::::::"), ":://%3A%3A//%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A%3A");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("file:::::////"), "file:::::////");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_from_input ("http:::::::::"), "http:::::::::");

	/* nautilus_make_uri_canonical */
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical (""), "file:");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("file:/"), "file:///");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("file:///"), "file:///");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("TRASH:XXX"), "trash:");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("trash:xxx"), "trash:");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("GNOME-TRASH:XXX"), "trash:");
	NAUTILUS_CHECK_STRING_RESULT (nautilus_make_uri_canonical ("gnome-trash:xxx"), "trash:");
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
