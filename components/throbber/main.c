/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Andy Hertzfeld
 */

/* main.c - main function and object activation function for the throbber component. */

#include <config.h>
#include "nautilus-throbber.h"

#include <eel/eel-debug.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus/nautilus-view-standard-main.h>

#define FACTORY_IID "OAFIID:nautilus_throbber_factory"
#define VIEW_IID    "OAFIID:nautilus_throbber"

static BonoboObject *
cb_create_throbber (const char *ignore0, void *ignore1)
{
	NautilusThrobber *throbber =
		g_object_new (NAUTILUS_TYPE_THROBBER, NULL);
	return nautilus_throbber_get_control (throbber);
}

int
main (int argc, char *argv[])
{
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}

	return nautilus_view_standard_main ("nautilus-throbber",
					    VERSION,
					    GETTEXT_PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    cb_create_throbber,
                                            nautilus_global_preferences_init,
					    NULL);
}
