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
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <unistd.h>

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "Desktop"
#define LEGACY_DESKTOP_DIRECTORY_NAME ".gnome-desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

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

	return eel_str_has_suffix (name_or_relative_uri, "~");
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

char *
nautilus_unique_temporary_file_name (void)
{
	const char *prefix = "/tmp/nautilus-temp-file";
	char *file_name;
	static guint count = 1;

	file_name = g_strdup_printf ("%sXXXXXX", prefix);

	if (mktemp (file_name) != file_name) {
		g_free (file_name);
		file_name = g_strdup_printf ("%s-%d-%d", prefix, count++, getpid ());
	}

	return file_name;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_utilities (void)
{
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
