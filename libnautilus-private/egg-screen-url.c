/* egg-screen-url.c
 * Copyright (C) 1998, James Henstridge <james@daa.com.au>
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2002, Sun Microsystems Inc.
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

#include "egg-screen-url.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-url.h>

#include "egg-screen-exec.h"

/******* START COPIED + PASTED CODE TO GO AWAY ******/

#define URL_HANDLER_DIR      "/desktop/gnome/url-handlers/"
#define DEFAULT_HANDLER_PATH "/desktop/gnome/url-handlers/unknown/command"

/* This needs to be exposed from libgnome and
 * removed from here.
 */

/**
 * egg_url_show_with_env:
 * @url: The url to display. Should begin with the protocol to use (e.g.
 * "http:", "ghelp:", etc)
 * @envp: child's environment, or %NULL to inherit parent's.
 * @error: Used to store any errors that result from trying to display the @url.
 *
 * Description: Like gnome_url_show(), except that the viewer application
 * is launched with its environment set to the contents of @envp.
 *
 * Returns: %TRUE if everything went fine, %FALSE otherwise (in which case
 * @error will contain the actual error).
 **/
gboolean
egg_url_show_with_env (const char  *url,
		       char       **envp,
		       GError     **error)
{
	GConfClient *client;
	gint i;
	gchar *pos, *template;
	int argc;
	char **argv;
	gboolean ret;
	
	g_return_val_if_fail (url != NULL, FALSE);

	pos = strchr (url, ':');

	client = gconf_client_get_default ();

	if (pos != NULL) {
		gchar *protocol, *path;
		
		g_return_val_if_fail (pos >= url, FALSE);

		protocol = g_new (gchar, pos - url + 1);
		strncpy (protocol, url, pos - url);
		protocol[pos - url] = '\0';
		g_ascii_strdown (protocol, -1);

		path = g_strconcat (URL_HANDLER_DIR, protocol, "/command", NULL);
		template = gconf_client_get_string (client, path, NULL);

		if (template == NULL) {
			gchar* template_temp;
			
			template_temp = gconf_client_get_string (client,
								 DEFAULT_HANDLER_PATH,
								 NULL);
						
			/* Retry to get the right url handler */
			template = gconf_client_get_string (client, path, NULL);

			if (template == NULL) 
				template = template_temp;
			else
				g_free (template_temp);

		}
		
		g_free (path);
		g_free (protocol);

	} else {
		/* no ':' ? this shouldn't happen. Use default handler */
		template = gconf_client_get_string (client, 
						    DEFAULT_HANDLER_PATH, 
						    NULL);
	}

	g_object_unref (G_OBJECT (client));

	if (!g_shell_parse_argv (template,
				 &argc,
				 &argv,
				 error)) {
		g_free (template);
		return FALSE;
	}

	g_free (template);

	for (i = 0; i < argc; i++) {
		char *arg;

		if (strcmp (argv[i], "%s") != 0)
			continue;

		arg = argv[i];
		argv[i] = g_strdup (url);
		g_free (arg);
	}
	
	/* This can return some errors */
	ret = g_spawn_async (NULL /* working directory */,
			     argv,
			     envp,
			     G_SPAWN_SEARCH_PATH /* flags */,
			     NULL /* child_setup */,
			     NULL /* data */,
			     NULL /* child_pid */,
			     error);

	g_strfreev (argv);

	return ret;
}
/******* END COPIED + PASTED CODE TO GO AWAY ******/

/**
 * egg_screen_url_show:
 * @screen: a #GdkScreen.
 * @url: The url to display. Should begin with the protocol to use (e.g.
 * "http:", "ghelp:", etc)
 * @error: Used to store any errors that result from trying to display the @url.
 *
 * Description: Like gnome_url_show(), but ensures that the viewer application
 * appears on @screen.
 *
 * Returns: %TRUE if everything went fine, %FALSE otherwise (in which case
 * @error will contain the actual error).
 **/
gboolean
egg_screen_url_show (GdkScreen   *screen,
		     const char  *url,
		     GError     **error)
{
	char     **env;
	gboolean   retval;

	env = egg_screen_exec_environment (screen);

	retval = egg_url_show_with_env (url, env, error);

	g_strfreev (env);

	return retval;
}
