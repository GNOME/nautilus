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

#include <libgnome/gnome-help.h>

#include "egg-screen-exec.h"

/**
 * egg_help_display_on_screen:
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @screen: a #GdkScreen.
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
egg_help_display_on_screen (const char  *file_name,
			    const char  *link_id,
			    GdkScreen   *screen,
			    GError     **error)
{
	return egg_help_display_with_doc_id_on_screen (
			NULL, NULL, file_name, link_id, screen, error);
}

/**
 * egg_help_display_with_doc_id_on_screen:
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The document identifier, or %NULL to default to the application ID
 * (app_id) of the specified @program.
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @screen: a #GdkScreen.
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
egg_help_display_with_doc_id_on_screen (GnomeProgram  *program,
					const char    *doc_id,
					const char    *file_name,
					const char    *link_id,
					GdkScreen     *screen,
					GError       **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = gnome_help_display_with_doc_id_and_env (
			program, doc_id, file_name, link_id, env, error);

	g_strfreev (env);

	return retval;
}

/**
 * egg_help_display_desktop_on_screen:
 * @program: The current application object, or %NULL for the default one.
 * @doc_id: The name of the help file relative to the system's help domain
 * (#GNOME_FILE_DOMAIN_HELP).
 * @file_name: The name of the help document to display.
 * @link_id: Can be %NULL. If set, refers to an anchor or section id within the
 * requested document.
 * @screen: a #GdkScreen.
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
egg_help_display_desktop_on_screen (GnomeProgram  *program,
				    const char    *doc_id,
				    const char    *file_name,
				    const char    *link_id,
				    GdkScreen     *screen,
				    GError       **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = gnome_help_display_desktop_with_env (
			program, doc_id, file_name, link_id, env, error);

	g_strfreev (env);

	return retval;
}

/**
 * egg_help_display_uri_on_screen:
 * @help_uri: The URI to display.
 * @screen: a #GdkScreen.
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
egg_help_display_uri_on_screen (const char  *help_uri,
				GdkScreen   *screen,
				GError     **error)
{
	gboolean   retval;
	char     **env;

	env = egg_screen_exec_environment (screen);

	retval = gnome_help_display_uri_with_env (help_uri, env, error);

	g_strfreev (env);

	return retval;
}
