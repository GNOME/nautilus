/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
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
 * Author: Maciej Stachowiak
 */

/* libmain.c - object activation infrastructure for shared library
   version of tree view. */

#include <config.h>
#include <string.h>
#include "nautilus-image-properties-view.h"
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#define VIEW_IID    "OAFIID:Nautilus_Image_Properties_View"


static CORBA_Object
image_shlib_make_object (PortableServer_POA poa,
			 const char *iid,
			 gpointer impl_ptr,
			 CORBA_Environment *ev)
{
	NautilusImagePropertiesView *view;

	if (strcmp (iid, VIEW_IID) != 0) {
		return CORBA_OBJECT_NIL;
	}

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (g_object_new (NAUTILUS_TYPE_IMAGE_PROPERTIES_VIEW, NULL));

	bonobo_activation_plugin_use (poa, impl_ptr);

	return CORBA_Object_duplicate (BONOBO_OBJREF (view), ev);
}

static const BonoboActivationPluginObject image_plugin_list[] = {
	{ VIEW_IID, image_shlib_make_object },
	{ NULL }
};

const BonoboActivationPlugin Bonobo_Plugin_info = {
	image_plugin_list,
	"Nautilus Image Properties Page"
};
