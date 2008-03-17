/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gio/gio.h>
#include "nautilus-connect-server-dialog.h"
#include <libnautilus-private/nautilus-global-preferences.h>

/* This file contains the glue for the calls from the connect to server dialog
 * to the main nautilus binary. A different version of this glue is in
 * nautilus-connect-server-dialog-main.c for the standalone version.
 */

void
nautilus_connect_server_dialog_present_uri (NautilusApplication *application,
					    GFile *location,
					    GtkWidget *widget)
{
	NautilusWindow *window;

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
		window = nautilus_application_create_navigation_window (application,
									NULL,
									gtk_widget_get_screen (widget));
		nautilus_window_go_to (window, location);
	} else {
		nautilus_application_present_spatial_window (application,
							     NULL,
							     NULL,
							     location,
							     gtk_widget_get_screen (widget));
	}

	gtk_widget_destroy (widget);
	g_object_unref (location);
}
