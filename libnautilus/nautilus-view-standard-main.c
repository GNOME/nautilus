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

#include <X11/Xlib.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-main.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <eel/eel-gnome-extensions.h>
#include <stdlib.h>
#include <string.h>

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
view_object_destroy (GObject      *object,
		     CallbackData *callback_data)
{
	g_assert (G_IS_OBJECT (object));

	if (!g_object_get_data (object, "standard_main_destroy_accounted")) {
		g_object_set_data (object, "standard_main_destroy_accounted",
				   GUINT_TO_POINTER (TRUE));

		callback_data->object_count--;

		if (callback_data->object_count <= 0 &&
		    callback_data->delayed_quit_timeout_id == 0) {
			/*    Connect a handler that will get us out of the
			 * main loop when there are no more objects outstanding.
			 */
			callback_data->delayed_quit_timeout_id = 
				g_timeout_add (N_IDLE_SECONDS_BEFORE_QUIT * 1000,
					       delayed_quit_timeout_callback,
					       callback_data);
		}
	}
}

/*
 *   Time we're prepared to wait without a ControlFrame
 * before terminating the Control. This can happen if the
 * container activates us but crashes before the set_frame.
 *
 * NB. if we don't get a frame in 30 seconds, something
 * is badly wrong, or Gnome performance needs improving
 * markedly !
 */
#define NAUTILUS_VIEW_NEVER_GOT_FRAME_TIMEOUT (30 * 1000)
#define CALLBACK_DATA_KEY "standard_main_callback_data_key"

static void
nautilus_view_cnx_broken_callback (GObject *control)
{
	view_object_destroy (control,
			     g_object_get_data (G_OBJECT (control),
						CALLBACK_DATA_KEY));
}

static gboolean
nautilus_view_never_got_frame_timeout (gpointer user_data)
{
	g_warning ("Never got frame, container died - abnormal exit condition");

	nautilus_view_cnx_broken_callback (user_data);
	
	return FALSE;
}

static void
nautilus_view_set_frame_callback (BonoboControl *control,
				  gpointer       user_data)
{
	Bonobo_ControlFrame remote_frame;

	remote_frame = bonobo_control_get_control_frame (control, NULL);

	if (remote_frame != CORBA_OBJECT_NIL) {
		ORBitConnectionStatus status;

		g_source_remove (GPOINTER_TO_UINT (user_data));

		status = ORBit_small_get_connection_status (remote_frame);

		/* Only track out of proc controls */
		if (status != ORBIT_CONNECTION_IN_PROC) {
			g_signal_connect_closure (
				ORBit_small_get_connection (remote_frame),
				"broken",
				g_cclosure_new_object_swap (
					G_CALLBACK (nautilus_view_cnx_broken_callback),
					G_OBJECT (control)),
				FALSE);
			g_signal_connect (
				control, "destroy",
				G_CALLBACK (nautilus_view_cnx_broken_callback),
				NULL);
		}
	}
}

/*
 *   This code is somewhat duplicated in gnome-panel/libpanel-applet
 * and is ripe for abstracting in an intermediate library.
 */
static void
nautilus_view_instrument_for_failure (BonoboObject *control,
				      CallbackData *callback_data)
{
	guint no_frame_timeout_id;

	g_object_set_data (G_OBJECT (control),
			   CALLBACK_DATA_KEY, callback_data);

	no_frame_timeout_id = g_timeout_add (
		NAUTILUS_VIEW_NEVER_GOT_FRAME_TIMEOUT,
		nautilus_view_never_got_frame_timeout,
		control);
	g_signal_connect_closure (
		control, "destroy",
		g_cclosure_new_swap (
			G_CALLBACK (g_source_remove_by_user_data),
			control, NULL),
		0);
	g_signal_connect (
		control, "set_frame",
		G_CALLBACK (nautilus_view_set_frame_callback),
		GUINT_TO_POINTER (no_frame_timeout_id));
}

static BonoboObject *
make_object (BonoboGenericFactory *factory, 
	     const char           *iid, 
	     gpointer              data)
{
	BonoboObject *view;
	BonoboObject *control;
	CallbackData *callback_data;

	callback_data = (CallbackData *) data;

	g_assert (BONOBO_IS_GENERIC_FACTORY (factory));
	g_assert (iid != NULL);
	g_assert (callback_data != NULL);

	/* Check that this is one of the types of object we know how to create. */
	if (g_list_find_custom (callback_data->view_iids,
				(gpointer) iid, (GCompareFunc) strcmp) == NULL) {
		return NULL;
	}
	
	view = callback_data->create_function (iid, callback_data->user_data);

	callback_data->object_count++;
	if (callback_data->delayed_quit_timeout_id != 0) {
		g_source_remove (callback_data->delayed_quit_timeout_id);
		callback_data->delayed_quit_timeout_id = 0;
	}
	g_signal_connect (view, "destroy",
			  G_CALLBACK (view_object_destroy),
			  callback_data);

	/* We can do some more agressive tracking of controls */
	if ((control = bonobo_object_query_local_interface
	             (view, "IDL:Bonobo/Control:1.0"))) {
		nautilus_view_instrument_for_failure (control, callback_data);
		bonobo_object_unref (control);
	}

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
	if (gettext_package_name != NULL
	    && gettext_locale_directory != NULL) {
		bindtextdomain (gettext_package_name, gettext_locale_directory);
		bind_textdomain_codeset (gettext_package_name, "UTF-8");
		textdomain (gettext_package_name);
	}

	/* Initialize libraries. */
	gnome_program_init (executable_name, version,
			    LIBGNOMEUI_MODULE,
			    argc, argv,
			    /* Disable session manager connection */
			    GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,
			    NULL);
	
	bonobo_ui_init (executable_name, version, &argc, argv);

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
        registration_id = eel_bonobo_make_registration_id (factory_iid);
	factory = bonobo_generic_factory_new (registration_id, 
					      make_object,
					      &callback_data);
	g_free (registration_id);

	if (factory != NULL) {
		/* Loop until we have no more objects. */
		bonobo_activate ();
		do {
			gtk_main ();
		} while (callback_data.object_count > 0 ||
			 callback_data.delayed_quit_timeout_id != 0);
		bonobo_object_unref (factory);
	}

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

typedef GType (* TypeFunc) (void);

BonoboObject *
nautilus_view_create_from_get_type_function (const char *iid, void *user_data)
{
	return BONOBO_OBJECT (g_object_new (((TypeFunc) (user_data)) (), NULL));
}
