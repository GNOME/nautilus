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
#include <liboaf/liboaf.h>
#include <bonobo.h>

static int object_count = 0;

static void
sample_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
sample_make_object (BonoboGenericFactory *factory, 
		    const char *iid, 
		    void *closure)
{
	NautilusSampleContentView *view;
	NautilusView *nautilus_view;

	if (strcmp (iid, "OAFIID:nautilus_sample_content_view:45c746bc-7d64-4346-90d5-6410463b43ae")) {
		return NULL;
	}

	view = NAUTILUS_SAMPLE_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_SAMPLE_CONTENT_VIEW, NULL));

	object_count++;

	nautilus_view = nautilus_sample_content_view_get_nautilus_view (view);

	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", sample_object_destroyed, NULL);

	return BONOBO_OBJECT (nautilus_view);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	CORBA_Environment ev;

	CORBA_exception_init(&ev);
	
        gnome_init_with_popt_table("nautilus-sample-content-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	factory = bonobo_generic_factory_new_multi ("OAFIID:nautilus_sample_content_view_factory:3df6b028-be44-4a18-95c3-7720f50ca0c5", sample_make_object, NULL);
		
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
