/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Sean Atkinson
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
 * Author: Sean Atkinson
 */

/* libmain.c - object activation infrastructure for shared library
   version of history view. */

#include <config.h>
#include <string.h>
#include "nautilus-history-view.h"
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>

static gboolean shortcut_registered = FALSE;

static CORBA_Object
create_object (const char *iid,
	       gpointer callback_data)
{
	NautilusHistoryView *view;

	if (strcmp (iid, VIEW_IID) != 0) {
		return CORBA_OBJECT_NIL;
	}

	view = NAUTILUS_HISTORY_VIEW (g_object_new (NAUTILUS_TYPE_HISTORY_VIEW, NULL));

	return CORBA_Object_duplicate (BONOBO_OBJREF (view), NULL);
}

static CORBA_Object
history_shlib_make_object (PortableServer_POA poa,
			const char *iid,
			gpointer impl_ptr,
			CORBA_Environment *ev)
{
	NautilusHistoryView *view;

	if (!shortcut_registered) {
		nautilus_bonobo_register_activation_shortcut (VIEW_IID,
							      create_object, NULL);
		shortcut_registered = TRUE;
	}

	if (strcmp (iid, VIEW_IID) != 0) {
		return CORBA_OBJECT_NIL;
	}

	view = NAUTILUS_HISTORY_VIEW (g_object_new (NAUTILUS_TYPE_HISTORY_VIEW, NULL));

	bonobo_activation_plugin_use (poa, impl_ptr);

	return CORBA_Object_duplicate (BONOBO_OBJREF (view), ev);
}

static const BonoboActivationPluginObject history_plugin_list[] = {
	{ VIEW_IID, history_shlib_make_object },
	{ NULL }
};

const BonoboActivationPlugin Bonobo_Plugin_info = {
	history_plugin_list,
	"Nautilus History Sidebar Panel"
};
