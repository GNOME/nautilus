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

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <liboaf/liboaf.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <stdlib.h>

#define N_IDLE_SECONDS_BEFORE_QUIT  5

typedef struct {
	int                          object_count;
	GList                       *view_iids;
	NautilusViewCreateFunction   create_function;
	void                        *user_data;
	guint                        delayed_quit_timeout_id;
} CallbackData;

static gboolean
delayed_quit_timeout_callback (gpointer data)
{
	CallbackData *callback_data;

	callback_data = (CallbackData *) data;
	callback_data->delayed_quit_timeout_id = 0;
	gtk_main_quit ();
	return FALSE;
}

static void
object_destroyed (GtkObject     *object,
		  CallbackData  *callback_data)
{
	g_assert (GTK_IS_OBJECT (object));

	callback_data->object_count--;
	if (callback_data->object_count <= 0 && callback_data->delayed_quit_timeout_id == 0) {
		callback_data->delayed_quit_timeout_id = g_timeout_add (N_IDLE_SECONDS_BEFORE_QUIT * 1000,
		                                                        delayed_quit_timeout_callback,
		                                                        callback_data);
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
	if (callback_data->delayed_quit_timeout_id != 0) {
		g_source_remove (callback_data->delayed_quit_timeout_id);
		callback_data->delayed_quit_timeout_id = 0;
	}
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    object_destroyed, callback_data);

	return BONOBO_OBJECT (view);
}

/**
 * nautilus_view_standard_main_multi
 *
 * A version of nautilus_view_standard_main that accepts multiple view
 * IIDs.
 *
 * @executable_name: The name of the executable binary.
 * @version: Component version.  Usually VERSION.
 * @gettext_package_name: Package name for gettext support.  Usually PACKAGE.
 *                        Can be NULL, in which case the component will not
 *                        have gettext support and translations might not
 *                        work
 * @gettext_locale_directory: Locale directory for gettext support.  Usually
 *                            GNOMELOCALEDIR.  Must not be NULL if
 *                            @gettext_package_name is not NULL.
 * @argc: Command line argument count.
 * @argv: Command line argument vector.
 * @factory_iid: The components's factory IID.
 * @view_iids: A GList of NautilusView IIDs.
 * @create_function: Function called to create the NautilusView instance.
 * @post_initialize_callback: An optional callback which is invoked after
 *                            all modules have been initialized (gtk, bonobo,
 *                            gnome-vfs, etc.) but before the execution of 
 *                            the main event loop or the creation of the 
 *                            component's factory.
 * @user_data:                User data for @create_function.
 **/
int
nautilus_view_standard_main_multi (const char *executable_name,
				   const char *version,
				   const char *gettext_package_name,
				   const char *gettext_locale_directory,
				   int argc,
				   char **argv,
				   const char *factory_iid,
				   GList *view_iids,
				   NautilusViewCreateFunction create_function,
				   GVoidFunc post_initialize_callback,
				   void *user_data)
{
	CORBA_ORB orb;
	BonoboGenericFactory *factory;
	CallbackData callback_data;
	char *registration_id;

	g_return_val_if_fail (executable_name != NULL, EXIT_FAILURE);
	g_return_val_if_fail (version != NULL, EXIT_FAILURE);
	g_return_val_if_fail (argc > 0, EXIT_FAILURE);
	g_return_val_if_fail (argv != NULL, EXIT_FAILURE);
	g_return_val_if_fail (argv[0] != NULL, EXIT_FAILURE);
	g_return_val_if_fail (factory_iid != NULL, EXIT_FAILURE);
	g_return_val_if_fail (g_list_length (view_iids) > 0, EXIT_FAILURE);
	g_return_val_if_fail (create_function != NULL, EXIT_FAILURE);

	if (gettext_package_name != NULL) {
		g_return_val_if_fail (gettext_locale_directory != NULL, EXIT_FAILURE);
	}
	if (gettext_locale_directory != NULL) {
		g_return_val_if_fail (gettext_package_name != NULL, EXIT_FAILURE);
	}

	/* Initialize gettext support if needed  */
#ifdef ENABLE_NLS
	if (gettext_package_name != NULL
	    && gettext_locale_directory != NULL) {
		bindtextdomain (gettext_package_name, gettext_locale_directory);
		textdomain (gettext_package_name);
	}
#endif

	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

	/* Initialize libraries. */
        gnome_init (executable_name, version, argc, argv); 
	gdk_rgb_init ();
	g_thread_init (NULL);
	gnome_vfs_init ();
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	if (post_initialize_callback != NULL) {
		(* post_initialize_callback) ();
	}
	
	/* Fill in the callback data */
	callback_data.object_count = 0;
	callback_data.view_iids = view_iids;
	callback_data.create_function = create_function;
	callback_data.user_data = user_data;
	callback_data.delayed_quit_timeout_id = 0;

	/* Create the factory. */
        registration_id = oaf_make_registration_id (factory_iid, 
						    DisplayString (GDK_DISPLAY ()));
	factory = bonobo_generic_factory_new_multi (registration_id, 
						    make_object,
						    &callback_data);
	g_free (registration_id);

	/* Loop until we have no more objects. */
	do {
		bonobo_main ();
	} while (callback_data.object_count > 0 || callback_data.delayed_quit_timeout_id != 0);

	/* Let the factory go. */
	bonobo_object_unref (BONOBO_OBJECT (factory));

	gnome_vfs_shutdown ();

	return EXIT_SUCCESS;
}

/**
 * nautilus_view_standard_main
 *
 * An implementation of most of a typical main.c file for Nautilus views.
 * Just call the function from main and pass it the right arguments. This
 * should make writing Nautilus views simpler.
 *
 * @executable_name: The name of the executable binary.
 * @version: Component version.  Usually VERSION.
 * @gettext_package_name: Package name for gettext support.  Usually PACKAGE.
 *                        Can be NULL, in which case the component will not
 *                        have gettext support and translations might not
 *                        work
 * @gettext_locale_directory: Locale directory for gettext support.  Usually
 *                            GNOMELOCALEDIR.  Must not be NULL if
 *                            @gettext_package_name is not NULL.
 * @argc: Command line argument count.
 * @argv: Command line argument vector.
 * @factory_iid: The components's factory IID.
 * @view_iid: The component's NautilusView IID.
 * @create_function: Function called to create the NautilusView instance.
 * @post_initialize_callback: An optional callback which is invoked after
 *                            all modules have been initialized (gtk, bonobo,
 *                            gnome-vfs, etc.) but before the execution of 
 *                            the main event loop or the creation of the 
 *                            component's factory.
 * @user_data:                User data for @create_function.
 **/
int
nautilus_view_standard_main (const char *executable_name,
			     const char *version,
			     const char *gettext_package_name,
			     const char *gettext_locale_directory,
			     int argc,
			     char **argv,
			     const char *factory_iid,
			     const char *view_iid,
			     NautilusViewCreateFunction create_function,
			     GVoidFunc post_initialize_callback,
			     void *user_data)
{
	GList node;

	g_return_val_if_fail (executable_name != NULL, EXIT_FAILURE);
	g_return_val_if_fail (version != NULL, EXIT_FAILURE);
	g_return_val_if_fail (argc > 0, EXIT_FAILURE);
	g_return_val_if_fail (argv != NULL, EXIT_FAILURE);
	g_return_val_if_fail (argv[0] != NULL, EXIT_FAILURE);
	g_return_val_if_fail (factory_iid != NULL, EXIT_FAILURE);
	g_return_val_if_fail (view_iid != NULL, EXIT_FAILURE);
	g_return_val_if_fail (create_function != NULL, EXIT_FAILURE);

	if (gettext_package_name != NULL) {
		g_return_val_if_fail (gettext_locale_directory != NULL, EXIT_FAILURE);
	}
	if (gettext_locale_directory != NULL) {
		g_return_val_if_fail (gettext_package_name != NULL, EXIT_FAILURE);
	}

	node.data = (gpointer) view_iid;
	node.next = NULL;
	node.prev = NULL;

	return nautilus_view_standard_main_multi (executable_name,
						  version,
						  gettext_package_name,
						  gettext_locale_directory,
						  argc,
						  argv,
						  factory_iid,
						  &node, 
						  create_function,
						  post_initialize_callback,
						  user_data);
}

typedef GtkType (* TypeFunc) (void);

NautilusView *
nautilus_view_create_from_get_type_function (const char *iid, void *user_data)
{
	return NAUTILUS_VIEW (gtk_object_new (((TypeFunc) (user_data)) (), NULL));
}

