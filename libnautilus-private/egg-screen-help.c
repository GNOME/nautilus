/* egg-screen-help.c
 * Copyright (C) 2001 Sid Vicious
 * Copyright (C) 2001 Jonathan Blandford <jrb@alum.mit.edu>
 * Copyright (C) 2002 Sun Microsystems Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc.,  59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "egg-screen-help.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-i18n.h>

#include "egg-screen-url.h"
#include "egg-screen-exec.h"

/******* START COPIED + PASTED CODE TO GO AWAY ******/

/* The _with_env methods need to go into and
 * be exposed from libgnome. They can then be
 * removed from here.
 */

/**
 * egg_help_display_uri_with_env:
 * @help_uri: The URI to display.
 * @envp: child's environment, or %NULL to inherit parent's.
 * @error: return location for errors.
 *
 * Description: Like gnome_help_display_uri, except that the help viewer
 * application is launched with its environment set to the contents of
 * @envp.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_help_display_uri_with_env (const char    *help_uri,
			       char         **envp,
			       GError       **error)
{
	GError *real_error;
	gboolean retval;

	real_error = NULL;
	retval = egg_url_show_with_env (help_uri, envp, &real_error);

	if (real_error != NULL)
		g_propagate_error (error, real_error);

	return retval;
}

static char *
locate_help_file (const char *path, const char *doc_name)
{
	int i;
	char *exts[] = { ".xml", ".docbook", ".sgml", ".html", "", NULL };
	const GList *lang_list = gnome_i18n_get_language_list ("LC_MESSAGES");

	for (;lang_list != NULL; lang_list = lang_list->next) {
		const char *lang = lang_list->data;

		/* This has to be a valid language AND a language with
		 * no encoding postfix.  The language will come up without
		 * encoding next */
		if (lang == NULL ||
		    strchr (lang, '.') != NULL)
			continue;

		for (i = 0; exts[i] != NULL; i++) {
			char *name;
			char *full;

			name = g_strconcat (doc_name, exts[i], NULL);
			full = g_build_filename (path, lang, name, NULL);
			g_free (name);

			if (g_file_test (full, G_FILE_TEST_EXISTS))
				return full;

			g_free (full);
		}
	}

	return NULL;
}

/**
 * egg_help_display_with_doc_id_with_env:
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The document identifier, or %NULL to default to the application ID
 * (app_id) of the specified @program.
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @envp: child's environment, or %NULL to inherit parent's.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display_with_doc_id(), except that the help
 * viewer application is launched with its environment set to the contents
 * of @envp.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_help_display_with_doc_id_with_env (GnomeProgram  *program,
				       const char    *doc_id,
				       const char    *file_name,
				       const char    *link_id,
				       char         **envp,
				       GError       **error)
{
	gchar *local_help_path;
	gchar *global_help_path;
	gchar *file;
	struct stat local_help_st;
	struct stat global_help_st;
	gchar *uri;
	gboolean retval;

	g_return_val_if_fail (file_name != NULL, FALSE);

	retval = FALSE;

	local_help_path = NULL;
	global_help_path = NULL;
	file = NULL;
	uri = NULL;

	if (program == NULL)
		program = gnome_program_get ();

	if (doc_id == NULL)
		doc_id = gnome_program_get_app_id (program);

	/* Compute the local and global help paths */

	local_help_path = gnome_program_locate_file (program,
						     GNOME_FILE_DOMAIN_APP_HELP,
						     "",
						     FALSE /* only_if_exists */,
						     NULL /* ret_locations */);

	if (local_help_path == NULL) {
		g_set_error (error,
			     GNOME_HELP_ERROR,
			     GNOME_HELP_ERROR_INTERNAL,
			     _("Unable to find the GNOME_FILE_DOMAIN_APP_HELP domain"));
		goto out;
	}

	global_help_path = gnome_program_locate_file (program,
						      GNOME_FILE_DOMAIN_HELP,
						      "",
						      FALSE /* only_if_exists */,
						      NULL /* ret_locations */);
	if (global_help_path == NULL) {
		g_set_error (error,
			     GNOME_HELP_ERROR,
			     GNOME_HELP_ERROR_INTERNAL,
			     _("Unable to find the GNOME_FILE_DOMAIN_HELP domain."));
		goto out;
	}

	/* Try to access the help paths, first the app-specific help path
	 * and then falling back to the global help path if the first one fails.
	 */

	if (stat (local_help_path, &local_help_st) == 0) {
		if (!S_ISDIR (local_help_st.st_mode)) {
			g_set_error (error,
				     GNOME_HELP_ERROR,
				     GNOME_HELP_ERROR_NOT_FOUND,
				     _("Unable to show help as %s is not a directory.  "
				       "Please check your installation."),
				     local_help_path);
			goto out;
		}

		file = locate_help_file (local_help_path, file_name);
	}

	if (file == NULL) {
		if (stat (global_help_path, &global_help_st) == 0) {
			if (!S_ISDIR (global_help_st.st_mode)) {
				g_set_error (error,
					     GNOME_HELP_ERROR,
					     GNOME_HELP_ERROR_NOT_FOUND,
					     _("Unable to show help as %s is not a directory.  "
					       "Please check your installation."),
					     global_help_path);
				goto out;
			}
		} else {
			g_set_error (error,
				     GNOME_HELP_ERROR,
				     GNOME_HELP_ERROR_NOT_FOUND,
				     _("Unable to find the help files in either %s "
				       "or %s.  Please check your installation"),
				     local_help_path,
				     global_help_path);
			goto out;
		}

		if (!(local_help_st.st_dev == global_help_st.st_dev
		      && local_help_st.st_ino == global_help_st.st_ino))
			file = locate_help_file (global_help_path, file_name);
	}

	if (file == NULL) {
		g_set_error (error,
			     GNOME_HELP_ERROR,
			     GNOME_HELP_ERROR_NOT_FOUND,
			     _("Unable to find the help files in either %s "
			       "or %s.  Please check your installation"),
			     local_help_path,
			     global_help_path);
		goto out;
	}

	/* Now that we have a file name, try to display it in the help browser */

	if (link_id)
		uri = g_strconcat ("ghelp://", file, "?", link_id, NULL);
	else
		uri = g_strconcat ("ghelp://", file, NULL);

	retval = egg_help_display_uri_with_env (uri, envp, error);

 out:

	g_free (local_help_path);
	g_free (global_help_path);
	g_free (file);
	g_free (uri);

	return retval;
}

/**
 * egg_help_display_desktop_with_env:
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The name of the help file relative to the system's help domain
 * (#GNOME_FILE_DOMAIN_HELP).
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @envp: child's environment, or %NULL to inherit parent's.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display_desktop(), except that the help
 * viewer application is launched with its environment set to the contents
 * of @envp.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_help_display_desktop_with_env (GnomeProgram  *program,
				   const char    *doc_id,
				   const char    *file_name,
				   const char    *link_id,
				   char         **envp,
				   GError       **error)
{
	GSList *ret_locations, *li;
	char *file;
	gboolean retval;
	char *url;

	g_return_val_if_fail (doc_id != NULL, FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);

	if (program == NULL)
		program = gnome_program_get ();

	ret_locations = NULL;
	gnome_program_locate_file (program,
				   GNOME_FILE_DOMAIN_HELP,
				   doc_id,
				   FALSE /* only_if_exists */,
				   &ret_locations);

	if (ret_locations == NULL) {
		g_set_error (error,
			     GNOME_HELP_ERROR,
			     GNOME_HELP_ERROR_NOT_FOUND,
			     _("Unable to find doc_id %s in the help path"),
			     doc_id);
		return FALSE;
	}

	file = NULL;
	for (li = ret_locations; li != NULL; li = li->next) {
		char *path = li->data;

		file = locate_help_file (path, file_name);
		if (file != NULL)
			break;
	}

	g_slist_foreach (ret_locations, (GFunc)g_free, NULL);
	g_slist_free (ret_locations);

	if (file == NULL) {
		g_set_error (error,
			     GNOME_HELP_ERROR,
			     GNOME_HELP_ERROR_NOT_FOUND,
			     _("Help document %s/%s not found"),
			     doc_id,
			     file_name);
		return FALSE;
	}

	if (link_id != NULL) {
		url = g_strconcat ("ghelp://", file, "?", link_id, NULL);
	} else {
		url = g_strconcat ("ghelp://", file, NULL);
	}

	g_free (file);

	retval = egg_help_display_uri_with_env (url, envp, error);

	return retval;
}
/******* END COPIED + PASTED CODE TO GO AWAY ******/

/**
 * egg_screen_help_display:
 * @screen: a #GdkScreen.
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display(), but ensures that the help viewer
 * application appears on @screen.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_screen_help_display (GdkScreen   *screen,
			 const char  *file_name,
			 const char  *link_id,
			 GError     **error)
{
	return egg_screen_help_display_with_doc_id (
			screen, NULL, NULL, file_name, link_id, error);
}

/**
 * egg_screen_help_display_with_doc_id
 * @screen: a #GdkScreen.
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The document identifier, or %NULL to default to the application ID
 * (app_id) of the specified @program.
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display_with_doc_id(), but ensures that the help
 * viewer application appears on @screen.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_screen_help_display_with_doc_id (GdkScreen     *screen,
				     GnomeProgram  *program,
				     const char    *doc_id,
				     const char    *file_name,
				     const char    *link_id,
				     GError       **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = egg_help_display_with_doc_id_with_env (
			program, doc_id, file_name, link_id, env, error);

	g_strfreev (env);

	return retval;
}

/**
 * egg_screen_help_display_desktop
 * @screen: a #GdkScreen.
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The name of the help file relative to the system's help domain
 * (#GNOME_FILE_DOMAIN_HELP).
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display_desktop(), but ensures that the help
 * viewer application appears on @screen.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_screen_help_display_desktop (GdkScreen     *screen,
				 GnomeProgram  *program,
				 const char    *doc_id,
				 const char    *file_name,
				 const char    *link_id,
				 GError       **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = egg_help_display_desktop_with_env (
			program, doc_id, file_name, link_id, env, error);

	g_strfreev (env);

	return retval;
}

/**
 * egg_screen_help_display_uri
 * @screen: a #GdkScreen.
 * @help_uri: The URI to display.
 * @error: A #GError instance that will hold the specifics of any error which
 * occurs during processing, or %NULL
 *
 * Description: Like gnome_help_display_uri(), but ensures that the help viewer
 * application appears on @screen.
 *
 * Returns: %TRUE on success, %FALSE otherwise (in which case @error will
 * contain the actual error).
 **/
gboolean
egg_screen_help_display_uri (GdkScreen   *screen,
			     const char  *help_uri,
			     GError     **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = egg_help_display_uri_with_env (help_uri, env, error);

	g_strfreev (env);

	return retval;
}
