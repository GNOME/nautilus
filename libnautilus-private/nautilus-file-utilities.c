/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities.c - implementation of file manipulation routines.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

#include "nautilus-global-preferences.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-metadata.h"
#include "nautilus-metafile.h"
#include "nautilus-file.h"
#include "nautilus-search-directory.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <unistd.h>
#include <stdlib.h>

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "Desktop"
#define LEGACY_DESKTOP_DIRECTORY_NAME ".gnome-desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)


char *
nautilus_compute_title_for_uri (const char *text_uri)
{
	NautilusFile *file;
	GnomeVFSURI *uri;
	char *title, *displayname;
	const char *hostname;
	NautilusDirectory *directory;
	NautilusQuery *query;
	hostname = NULL;

	if (text_uri) {
		if (eel_uri_is_search (text_uri)) {
			directory = nautilus_directory_get (text_uri);
			
			query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
			nautilus_directory_unref (directory);
			
			if (query != NULL) {
				title = nautilus_query_to_readable_string (query);
				g_object_unref (query);
			} else {
				title = g_strdup (_("Search"));
			}

			return title;
		}
		file = nautilus_file_get (text_uri);
		uri = gnome_vfs_uri_new (text_uri);
		if (uri && !gnome_vfs_uri_is_local (uri)) {
			hostname = gnome_vfs_uri_get_host_name (uri);
		}
		displayname = nautilus_file_get_display_name (file);
		if (hostname) {
			title = g_strdup_printf (_("%s on %s"), displayname, hostname);
			g_free (displayname);
		} else {
			title = displayname;
		}
		if (uri) {
			gnome_vfs_uri_unref (uri);
		}
		nautilus_file_unref (file);
	} else {
		title = g_strdup ("");
	}
	
	return title;
}


gboolean
nautilus_file_name_matches_hidden_pattern (const char *name_or_relative_uri)
{
	g_return_val_if_fail (name_or_relative_uri != NULL, FALSE);

	return name_or_relative_uri[0] == '.';
}

gboolean
nautilus_file_name_matches_backup_pattern (const char *name_or_relative_uri)
{
	g_return_val_if_fail (name_or_relative_uri != NULL, FALSE);

	return g_str_has_suffix (name_or_relative_uri, "~") &&
	       !g_str_equal (name_or_relative_uri, "~");
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

	user_directory = g_build_filename (g_get_home_dir (),
					   NAUTILUS_USER_DIRECTORY_NAME,
					   NULL);
	
	if (!g_file_test (user_directory, G_FILE_TEST_EXISTS)) {
		mkdir (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
		/* FIXME bugzilla.gnome.org 41286: 
		 * How should we handle the case where this mkdir fails? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return user_directory;
}

static char *
get_desktop_path (void)
{
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
		return g_strdup (g_get_home_dir());
	} else {
		return g_build_filename (g_get_home_dir (), DESKTOP_DIRECTORY_NAME, NULL);
	}
	
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
	char *desktop_directory;
	
	desktop_directory = get_desktop_path ();

	/* Don't try to create a home directory */
	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
		if (!g_file_test (desktop_directory, G_FILE_TEST_EXISTS)) {
			mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
			/* FIXME bugzilla.gnome.org 41286: 
			 * How should we handle the case where this mkdir fails? 
			 * Note that nautilus_application_startup will refuse to launch if this 
			 * directory doesn't get created, so that case is OK. But the directory 
			 * could be deleted after Nautilus was launched, and perhaps
			 * there is some bad side-effect of not handling that case.
			 */
		}
	}

	return desktop_directory;
}


/**
 * nautilus_get_desktop_directory_uri:
 * 
 * Get the uri for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_desktop_directory_uri (void)
{
	char *desktop_path;
	char *desktop_uri;
	
	desktop_path = nautilus_get_desktop_directory ();
	desktop_uri = gnome_vfs_get_uri_from_local_path (desktop_path);
	g_free (desktop_path);

	return desktop_uri;
}

char *
nautilus_get_desktop_directory_uri_no_create (void)
{
	char *desktop_path;
	char *desktop_uri;
	
	desktop_path = get_desktop_path ();
	desktop_uri = gnome_vfs_get_uri_from_local_path (desktop_path);
	g_free (desktop_path);

	return desktop_uri;
}

char *
nautilus_get_templates_directory (void)
{
	return  g_build_filename (g_get_home_dir(),
				  "Templates", NULL);
}

void
nautilus_create_templates_directory (void)
{
	char *dir;

	dir = nautilus_get_templates_directory ();
	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		mkdir (dir, DEFAULT_NAUTILUS_DIRECTORY_MODE);
	}
	g_free (dir);
}

char *
nautilus_get_templates_directory_uri (void)
{
	char *directory, *uri;

	directory = nautilus_get_templates_directory ();
	uri = gnome_vfs_get_uri_from_local_path (directory);
	g_free (directory);
	return uri;
}

char *
nautilus_get_searches_directory (void)
{
	char *user_dir;
	char *searches_dir;

	user_dir = nautilus_get_user_directory ();
	searches_dir = g_build_filename (user_dir, "searches", NULL);
	g_free (user_dir);
	
	if (!g_file_test (searches_dir, G_FILE_TEST_EXISTS))
		mkdir (searches_dir, DEFAULT_NAUTILUS_DIRECTORY_MODE);

	return searches_dir;
}

/* These need to be reset to NULL when desktop_is_home_dir changes */
static char *escaped_desktop_dir = NULL;
static char *escaped_desktop_dir_dirname = NULL;
static char *escaped_desktop_dir_filename = NULL;
static gboolean desktop_dir_changed_callback_installed = FALSE;

static void
desktop_dir_changed_callback (gpointer callback_data)
{
	g_free (escaped_desktop_dir);
	g_free (escaped_desktop_dir_filename);
	g_free (escaped_desktop_dir_dirname);
	escaped_desktop_dir = NULL;
	escaped_desktop_dir_dirname = NULL;
	escaped_desktop_dir_filename = NULL;
}

static void
update_desktop_dir (void)
{
	char *uri, *path;
	GnomeVFSURI *vfs_uri;

	path = get_desktop_path ();
	uri = gnome_vfs_get_uri_from_local_path (path);
	vfs_uri = gnome_vfs_uri_new (uri);
	g_free (path);
	g_free (uri);
	
	escaped_desktop_dir = g_strdup (vfs_uri->text);
	escaped_desktop_dir_filename = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	escaped_desktop_dir_dirname = gnome_vfs_uri_extract_dirname (vfs_uri);
	
	gnome_vfs_uri_unref (vfs_uri);
}

gboolean
nautilus_is_home_directory_file_escaped (char *escaped_dirname,
					 char *escaped_file)
{
	static char *escaped_home_dir_dirname = NULL;
	static char *escaped_home_dir_filename = NULL;
	char *uri;
	GnomeVFSURI *vfs_uri;
	
	if (escaped_home_dir_dirname == NULL) {
		uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
		vfs_uri = gnome_vfs_uri_new (uri);
		g_free (uri);

		escaped_home_dir_filename = gnome_vfs_uri_extract_short_path_name (vfs_uri);
		escaped_home_dir_dirname = gnome_vfs_uri_extract_dirname (vfs_uri);

		gnome_vfs_uri_unref (vfs_uri);
	}

	return (strcmp (escaped_dirname, escaped_home_dir_dirname) == 0 &&
		strcmp (escaped_file, escaped_home_dir_filename) == 0);
}
					 
gboolean
nautilus_is_desktop_directory_file_escaped (char *escaped_dirname,
					    char *escaped_file)
{

	if (!desktop_dir_changed_callback_installed) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
					      desktop_dir_changed_callback,
					      NULL);
		desktop_dir_changed_callback_installed = TRUE;
	}
		
	if (escaped_desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return (strcmp (escaped_dirname, escaped_desktop_dir_dirname) == 0 &&
		strcmp (escaped_file, escaped_desktop_dir_filename) == 0);
}

gboolean
nautilus_is_desktop_directory_escaped (char *escaped_dir)
{

	if (!desktop_dir_changed_callback_installed) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
					      desktop_dir_changed_callback,
					      NULL);
		desktop_dir_changed_callback_installed = TRUE;
	}
		
	if (escaped_desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return strcmp (escaped_dir, escaped_desktop_dir) == 0;
}


/**
 * nautilus_get_gmc_desktop_directory:
 * 
 * Get the path for the directory containing the legacy gmc desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_gmc_desktop_directory (void)
{
	return g_build_filename (g_get_home_dir (), LEGACY_DESKTOP_DIRECTORY_NAME, NULL);
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
	return g_strdup (DATADIR "/pixmaps/nautilus");
}

/* FIXME bugzilla.gnome.org 42423: 
 * Callers just use this and dereference so we core dump if
 * pixmaps are missing. That is lame.
 */
char *
nautilus_pixmap_file (const char *partial_path)
{
	char *path;

	path = g_build_filename (DATADIR "/pixmaps/nautilus", partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);
	return NULL;
}

char *
nautilus_get_data_file_path (const char *partial_path)
{
	char *path;
	char *user_directory;

	/* first try the user's home directory */
	user_directory = nautilus_get_user_directory ();
	path = g_build_filename (user_directory, partial_path, NULL);
	g_free (user_directory);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);
	
	/* next try the shared directory */
	path = g_build_filename (NAUTILUS_DATADIR, partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);

	return NULL;
}

static gboolean
test_uri_exists (const char *path)
{
	GnomeVFSURI *uri;
	gboolean exists;

	uri = gnome_vfs_uri_new (path);
	exists = gnome_vfs_uri_exists (uri);
	gnome_vfs_uri_unref (uri);

	return exists;
}

char *
nautilus_ensure_unique_file_name (const char *directory_uri,
				  const char *base_name,
				  const char *extension)
{
	char *path, *escaped_name;
	gboolean exists;
	int copy;

	escaped_name = gnome_vfs_escape_string (base_name);

	path = g_strdup_printf ("%s/%s%s",
				directory_uri,
				escaped_name,
				extension);
	exists = test_uri_exists (path);

	copy = 1;
	while (exists) {
		g_free (path);
		path = g_strdup_printf ("%s/%s-%d%s",
					directory_uri,
					escaped_name,
					copy,
					extension);

		exists = test_uri_exists (path);

		copy++;
	}

	g_free (escaped_name);

	return path;
}

char *
nautilus_unique_temporary_file_name (void)
{
	const char *prefix = "/tmp/nautilus-temp-file";
	char *file_name;
	int fd;

	file_name = g_strdup_printf ("%sXXXXXX", prefix);

	fd = mkstemp (file_name); 
	if (fd == -1) {
		g_free (file_name);
		file_name = NULL;
	} else {
		close (fd);
	}
	
	return file_name;
}

const char *
nautilus_get_vfs_method_display_name (char *method)
{
	if (g_ascii_strcasecmp (method, "computer") == 0 ) {
		return _("Computer");
	} else if (g_ascii_strcasecmp (method, "network") == 0 ) {
		return _("Network");
	} else if (g_ascii_strcasecmp (method, "fonts") == 0 ) {
		return _("Fonts");
	} else if (g_ascii_strcasecmp (method, "themes") == 0 ) {
		return _("Themes");
	} else if (g_ascii_strcasecmp (method, "burn") == 0 ) {
		return _("CD/DVD Creator");
	} else if (g_ascii_strcasecmp (method, "smb") == 0 ) {
		return _("Windows Network");
	} else if (g_ascii_strcasecmp (method, "dns-sd") == 0 ) {
		/* translators: this is the title of the "dns-sd:///" location */
		return _("Services in");
	}
	return NULL;
}

char *
nautilus_get_uri_shortname_for_display (GnomeVFSURI *uri)
{
	char *utf8_name, *name, *tmp;
	char *text_uri, *local_file;
	gboolean validated;
	const char *method;

	
	validated = FALSE;
	name = gnome_vfs_uri_extract_short_name (uri);
	if (name == NULL) {
		name = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
	} else if (g_ascii_strcasecmp (uri->method_string, "file") == 0) {
		text_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
		local_file = gnome_vfs_get_local_path_from_uri (text_uri);
		g_free (name);
		name = g_filename_display_basename (local_file);
		g_free (local_file);
		g_free (text_uri);
		validated = TRUE;
	} else if (!gnome_vfs_uri_has_parent (uri)) {
		/* Special-case the display name for roots that are not local files */
		method = nautilus_get_vfs_method_display_name (uri->method_string);
		if (method == NULL) {
			method = uri->method_string;
		}
		
		if (name == NULL ||
		    strcmp (name, GNOME_VFS_URI_PATH_STR) == 0) {
			g_free (name);
			name = g_strdup (method);
		} else {
			tmp = name;
			name = g_strdup_printf ("%s: %s", method, name);
			g_free (tmp);
		}
	}

	if (!validated && !g_utf8_validate (name, -1, NULL)) {
		utf8_name = eel_make_valid_utf8 (name);
		g_free (name);
		name = utf8_name;
	}

	return name;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_utilities (void)
{
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
