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
 * Author: Maciej Stachowiak
 */

/* main.c - main function and object activation function for sample
   content view component. */

#include <config.h>

#include "nautilus-sample-content-view.h"

#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo.h>

static int object_count = 0;

static void
sample_object_destroyed(GtkObject *obj)
{
	puts ("destroying object.");
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
sample_make_object (BonoboGenericFactory *factory, 
		    const char *goad_id, 
		    void *closure)
{
	NautilusSampleContentView *view;
	NautilusViewFrame *view_frame;

	puts ("Trying to create object.");

	if (strcmp (goad_id, "nautilus_sample_content_view")) {
		return NULL;
	}
	

	view = NAUTILUS_SAMPLE_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_SAMPLE_CONTENT_VIEW, NULL));

	object_count++;

	gtk_signal_connect (GTK_OBJECT (view), "destroy", sample_object_destroyed, NULL);

	view_frame = NAUTILUS_VIEW_FRAME (nautilus_sample_content_view_get_view_frame (view));
	
	printf ("Returning new object %x\n", (unsigned) view_frame);

	return BONOBO_OBJECT (view_frame);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);
	
	orb = gnome_CORBA_init_with_popt_table ("nautilus-sample-content-view", VERSION, &argc, argv, NULL, 0, NULL,
						GNORBA_INIT_SERVER_FUNC, &ev);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	factory = bonobo_generic_factory_new_multi ("nautilus_sample_content_view_factory", sample_make_object, NULL);
	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
