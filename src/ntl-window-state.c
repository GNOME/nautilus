/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

#include <config.h>
#include "nautilus.h"
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>

void
nautilus_window_set_initial_state (NautilusWindow *window, const char *initial_url)
{
	if (initial_url)
	{
		nautilus_window_goto_uri (window, initial_url);
	}
	else
	{
		char *home_uri;
		char *default_home_uri = g_strdup_printf ("file://%s", g_get_home_dir());

		home_uri = nautilus_preferences_get (NAUTILUS_PREFERENCES_HOME_URI, default_home_uri);

		g_assert (home_uri != NULL);

		nautilus_window_goto_uri (window, home_uri);

		g_free (home_uri);
		g_free (default_home_uri);
	}
}
