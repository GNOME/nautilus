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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* main.c - Main function and object activation function for sample
 * view component.
 */

#include <config.h>

#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-defs.h> /* must come before gnome-init.h */
#include <libgnomeui/gnome-init.h> /* must come before liboaf.h */
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include "nautilus-sample-content-view.h"

#define FACTORY_IID "OAFIID:nautilus_sample_content_view_factory:3df6b028-be44-4a18-95c3-7720f50ca0c5"
#define VIEW_IID    "OAFIID:nautilus_sample_content_view:45c746bc-7d64-4346-90d5-6410463b43ae"

static int object_count = 0;

static void
sample_object_destroyed (GtkObject *object)
{
	g_assert (GTK_IS_OBJECT (object));

	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
sample_make_object (BonoboGenericFactory *factory, 
		    const char *iid, 
		    gpointer callback_data)
{
	NautilusSampleContentView *widget;
	NautilusView *view;

	g_assert (BONOBO_IS_GENERIC_FACTORY (factory));
	g_assert (iid != NULL);
	g_assert (callback_data == NULL);

	/* Check that this is the one type of object we know how to
	 * create.
	 */
	if (strcmp (iid, VIEW_IID) != 0) {
		return NULL;
	}

	/* Create the view. The way this sample is set up, we create a
	 * widget which makes the NautilusView object as part of it's
	 * initialization. This is a bit backwards as it's the view
	 * that owns the widget.
	 */
	widget = NAUTILUS_SAMPLE_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_SAMPLE_CONTENT_VIEW, NULL));
	view = nautilus_sample_content_view_get_nautilus_view (widget);

	/* Connect a handler that will get us out of the main loop
         * when there are no more objects outstanding.
	 */
	object_count++;
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    sample_object_destroyed, NULL);

	return BONOBO_OBJECT (view);
}

int
main (int argc, char *argv[])
{
	CORBA_ORB orb;
	BonoboGenericFactory *factory;

	/* Initialize libraries. */
        gnome_init_with_popt_table ("nautilus-sample-content-view", VERSION, 
				    argc, argv,
				    oaf_popt_options, 0, NULL); 
	orb = oaf_init (argc, argv);
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* Create the factory. */
	factory = bonobo_generic_factory_new_multi (FACTORY_IID, sample_make_object, NULL);
	
	/* Loop until we have no more objects. */
	do {
		bonobo_main ();
	} while (object_count > 0);

	/* Let the factory go. */
	bonobo_object_unref (BONOBO_OBJECT (factory));

	return EXIT_SUCCESS;
}
