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

/* main.c - main function and object activation function for the hardware view component. */

#define FACTORY_IID	"OAFIID:nautilus_hardware_view_factory:8c80e55a-5c03-4403-9e51-3a5711b8a5ce" 
#define VIEW_IID	"OAFIID:nautilus_hardware_view:4a3f3793-bab4-4640-9f56-e7871fe8e150"

#include <config.h>

#include "nautilus-hardware-view.h"
#include <libnautilus/nautilus-view-standard-main.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-debug.h>

static NautilusView *
cb_create_hardware_view (const char *ignore0, void *ignore1)
{
	NautilusHardwareView *hardware_view =
		g_object_new (NAUTILUS_TYPE_HARDWARE_VIEW, NULL);
	return nautilus_hardware_view_get_nautilus_view (hardware_view);

}
int
main (int argc, char *argv[])
{
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}

	return nautilus_view_standard_main ("nautilus-hardware-view",
					    VERSION,
					    GETTEXT_PACKAGE,
					    GNOMELOCALEDIR,
					    argc,
					    argv,
					    FACTORY_IID,
					    VIEW_IID,
					    cb_create_hardware_view,
					    nautilus_global_preferences_init,
					    NULL);
}
