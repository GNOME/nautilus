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

#include <libgnome/gnome-url.h>

#include "egg-screen-url.h"
#include "egg-screen-exec.h"

/**
 * egg_url_show_on_screen:
 * @url: The url to display. Should begin with the protocol to use (e.g.
 * "http:", "ghelp:", etc)
 * @screen: a #GdkScreen.
 * @error: Used to store any errors that result from trying to display the @url.
 *
 * Description: Like gnome_url_show(), but ensures that the viewer application
 * appears on @screen.
 *
 * Returns: %TRUE if everything went fine, %FALSE otherwise (in which case
 * @error will contain the actual error).
 **/
gboolean
egg_url_show_on_screen (const char  *url,
			GdkScreen   *screen,
			GError     **error)
{
	char     **env;
	gboolean   retval;

	env = egg_screen_exec_environment (screen);

	retval = gnome_url_show_with_env (url, env, error);

	g_strfreev (env);

	return retval;
}
