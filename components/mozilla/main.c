/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/* main.c - main function and object activation function for mozilla
   content view component. */

#include <config.h>

#include "nautilus-mozilla-content-view.h"

#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>

static int object_count = 0;

static void
mozilla_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
mozilla_make_object (BonoboGenericFactory *factory, 
		    const char *goad_id, 
		    void *closure)
{
	NautilusMozillaContentView *view;
	NautilusViewFrame *view_frame;

	if (strcmp (goad_id, "nautilus_mozilla_content_view")) {
		return NULL;
	}
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW, NULL));

	object_count++;

	view_frame = NAUTILUS_VIEW_FRAME (nautilus_mozilla_content_view_get_view_frame (view));

	gtk_signal_connect (GTK_OBJECT (view_frame), "destroy", mozilla_object_destroyed, NULL);

	return BONOBO_OBJECT (view_frame);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);
	
	orb = gnome_CORBA_init_with_popt_table ("nautilus-mozilla-content-view", VERSION, &argc, argv, NULL, 0, NULL,
						GNORBA_INIT_SERVER_FUNC, &ev);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	factory = bonobo_generic_factory_new_multi ("nautilus_mozilla_content_view_factory", mozilla_make_object, NULL);
	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
