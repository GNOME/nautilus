/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  Authors: Maciej Stachowiak <mjs@eazel.com>
 *
 */

/* nautilus-view-standard-main.c: Standard main functions for Nautilus
   views, to reduce boilerplate code. */

#include <config.h>
#include "nautilus-view-standard-main.h"

#include <libgnomeui/gnome-init.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <liboaf/liboaf.h>
#include <stdlib.h>

typedef struct {
	int                          object_count;
	GList                       *view_iids;
	NautilusViewCreateFunction   create_function;
	void                        *user_data;
} CallbackData;


static void
object_destroyed (GtkObject     *object,
		  CallbackData  *callback_data)
{
	g_assert (GTK_IS_OBJECT (object));

	callback_data->object_count--;
	if (callback_data->object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
make_object (BonoboGenericFactory *factory, 
	     const char           *iid, 
	     gpointer              data)
{
	CallbackData *callback_data;
	NautilusView *view;

	callback_data = (CallbackData *) data;

	g_assert (BONOBO_IS_GENERIC_FACTORY (factory));
	g_assert (iid != NULL);
	g_assert (callback_data != NULL);

	/* Check that this is one of the types of object we know how to
	 * create.
	 */

	if (g_list_find_custom (callback_data->view_iids,
				(gpointer) iid, (GCompareFunc) strcmp) == NULL) {
		return NULL;
	}
	
	view = callback_data->create_function (iid, callback_data->user_data);

	/* Connect a handler that will get us out of the main loop
         * when there are no more objects outstanding.
	 */
	callback_data->object_count++;
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    object_destroyed, callback_data);

	return BONOBO_OBJECT (view);
}


int
nautilus_view_standard_main_multi (const char                 *executable_name,
				   const char                 *version,
				   int                         argc,
				   char                      **argv,
				   const char                 *factory_iid,
				   GList                      *view_iids,
				   NautilusViewCreateFunction  create_function,
				   void                       *user_data)
{
	CORBA_ORB orb;
	BonoboGenericFactory *factory;
	CallbackData callback_data;
	char *registration_id;

	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

	/* Initialize libraries. */
        gnome_init (executable_name, version, 
		    argc, argv); 
	gdk_rgb_init ();
	g_thread_init (NULL);
	gnome_vfs_init ();
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* Fill in the callback data */
	callback_data.object_count = 0;
	callback_data.view_iids = view_iids;
	callback_data.create_function = create_function;
	callback_data.user_data = user_data;

	/* Create the factory. */
        registration_id = oaf_make_registration_id (factory_iid, g_getenv ("DISPLAY"));
	factory = bonobo_generic_factory_new_multi (registration_id, 
						    make_object,
						    &callback_data);
	g_free (registration_id);

	
	/* Loop until we have no more objects. */
	do {
		bonobo_main ();
	} while (callback_data.object_count > 0);

	/* Let the factory go. */
	bonobo_object_unref (BONOBO_OBJECT (factory));

	gnome_vfs_shutdown ();

	return EXIT_SUCCESS;
}



int
nautilus_view_standard_main (const char                 *executable_name,
			     const char                 *version,
			     int                         argc,
			     char                      **argv,
			     const char                 *factory_iid,
			     const char                 *view_iid,
			     NautilusViewCreateFunction  create_function,
			     void                       *user_data)
{
	GList       node;

	node.data = (gpointer) view_iid;

	return nautilus_view_standard_main_multi (executable_name, version, 
						  argc, argv,
						  factory_iid, &node, 
						  create_function, user_data);
}

typedef GtkType (* TypeFunc) (void);



NautilusView *
nautilus_view_create_from_get_type_function (const char *iid, void *user_data)
{
	return NAUTILUS_VIEW (gtk_object_new (((TypeFunc) (user_data)) (), NULL));
}

