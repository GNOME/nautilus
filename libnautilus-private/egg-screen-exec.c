/* egg-screen-exec.c
 *
 * Copyright (C) 2002  Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "egg-screen-exec.h"

#include <string.h>
#include <libgnome/gnome-exec.h>

#ifndef HAVE_GTK_MULTIHEAD
#include <gdk/gdkx.h>
#endif

extern char **environ;

/**
 * egg_screen_exec_display_string:
 * @screen: A #GdkScreen
 *
 * Description: Returns a string that when passed to XOpenDisplay()
 * would cause @screen to be the default screen on the newly opened
 * X display. This string is suitable for setting $DISPLAY when
 * launching an application which should appear on @screen.
 *
 * Returns: a newly allocated string or %NULL on error.
 **/
char *
egg_screen_exec_display_string (GdkScreen *screen)
{
#ifdef HAVE_GTK_MULTIHEAD
	GString    *str;
	const char *old_display;
	char       *retval;
	char       *p;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

	if (gdk_screen_get_default () == screen)
		return g_strdup_printf ("DISPLAY=%s",
				gdk_display_get_name (
					gdk_screen_get_display (screen)));

	old_display = gdk_display_get_name (gdk_screen_get_display (screen));

	str = g_string_new ("DISPLAY=");
	g_string_append (str, old_display);

	p = strrchr (str->str, '.');
	if (p && p >  strchr (str->str, ':'))
		g_string_truncate (str, p - str->str);

	g_string_append_printf (str, ".%d", gdk_screen_get_number (screen));

	retval = str->str;

	g_string_free (str, FALSE);

	return retval;
#else
	return g_strdup (DisplayString (GDK_DISPLAY ()));
#endif
}

/**
 * egg_screen_exec_environment:
 * @screen: A #GdkScreen
 *
 * Description: Modifies the current program environment to
 * ensure that $DISPLAY is set such that a launched application
 * inheriting this environment would appear on @screen.
 *
 * Returns: a newly-allocated %NULL-terminated array of strings or
 * %NULL on error. Use g_strfreev() to free it.
 **/
char **
egg_screen_exec_environment (GdkScreen *screen)
{
	char **retval = NULL;
	int    i;
#ifdef HAVE_GTK_MULTIHEAD
	int    display_index = -1;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

	for (i = 0; environ [i]; i++)
		if (!strncmp (environ [i], "DISPLAY", 7))
			display_index = i;

	if (display_index == -1)
		display_index = i++;
#else
	for (i = 0; environ [i]; i++);
#endif

	retval = g_new (char *, i + 1);

	for (i = 0; environ [i]; i++)
#ifdef HAVE_GTK_MULTIHEAD
		if (i == display_index)
			retval [i] = egg_screen_exec_display_string (screen);
		else
#endif
			retval [i] = g_strdup (environ [i]);

	retval [i] = NULL;

	return retval;
}

/**
 * egg_screen_execute_async:
 * @screen: A #GdkScreen
 * @dir: Directory in which child should be executed, or %NULL for current
 *       directory
 * @argc: Number of arguments
 * @argv: Argument vector to exec child
 *
 * Description: Like gnome_execute_async(), but ensures that the child
 * is launched in an environment such that if it calls XOpenDisplay()
 * the resulting display would have @screen as the default screen.
 *
 * Returns: process id of child, or %-1 on error.
 **/
int
egg_screen_execute_async (GdkScreen    *screen,
                          const char   *dir,
                          int           argc,
                          char * const  argv [])
{
#ifdef HAVE_GTK_MULTIHEAD
	char **envp = NULL;
	int    envc = 0;
	int    retval;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

	if (gdk_screen_get_default () != screen) {
		envc = 1;
		envp = g_new0 (char *, 2);
		envp [0] = egg_screen_exec_display_string (screen);
	}

	retval = gnome_execute_async_with_env (dir, argc, argv, envc, envp);

	g_strfreev (envp);

	return retval;
#else
	return gnome_execute_async (dir, argc, argv);
#endif
}

/**
 * egg_screen_execute_shell:
 * @screen: A #GdkScreen.
 * @dir: Directory in which child should be executed, or %NULL for current
 *       directory
 * @commandline: Shell command to execute
 *
 * Description: Like gnome_execute_shell(), but ensures that the child
 * is launched in an environment such that if it calls XOpenDisplay()
 * the resulting display would have @screen as the default screen.
 *
 * Returns: process id of shell, or %-1 on error.
 **/
int
egg_screen_execute_shell (GdkScreen    *screen,
                          const char   *dir,
                          const char   *command)
{
#ifdef HAVE_GTK_MULTIHEAD
	int retval = -1;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

	if (gdk_screen_get_default () == screen)
		retval = gnome_execute_shell (dir, command);

	else {
		char *exec;
		char *display;

		display = egg_screen_exec_display_string (screen);
		exec = g_strconcat (display, " ", command, NULL);

		retval = gnome_execute_shell (dir, exec);

		g_free (display);
		g_free (exec);
	}

	return retval;
#else
	return gnome_execute_shell (dir, command);
#endif
}

/**
 * egg_screen_execute_command_line_async:
 * @screen: A #GdkScreen.
 * @command_line: a command line
 * @error: return location for errors
 *
 * Description: Like g_spawn_command_line_async(), but ensures that
 * the child is launched in an environment such that if it calls
 * XOpenDisplay() the resulting display would have @screen as the
 * default screen.
 *
 * Returns: %TRUE on success, %FALSE if error is set.
 **/
gboolean
egg_screen_execute_command_line_async (GdkScreen    *screen,
                                       const char   *command,
                                       GError      **error)
{
#ifdef HAVE_GTK_MULTIHEAD
	gboolean   retval;
	char     **argv = NULL;
	char     **envp = NULL;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
	g_return_val_if_fail (command != NULL, FALSE);

	if (!g_shell_parse_argv (command, NULL, &argv, error))
		return FALSE;

	if (gdk_screen_get_default () != screen)
		envp = egg_screen_exec_environment (screen);

	retval = g_spawn_async (g_get_home_dir (),
				argv, envp, G_SPAWN_SEARCH_PATH,
				NULL, NULL, NULL, error);
	g_strfreev (argv);
	g_strfreev (envp);

	return retval;
#else
	return g_spawn_command_line_async (command, error);
#endif
}
