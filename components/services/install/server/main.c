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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#undef STOP_IN_DEBUG

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <signal.h>

#include <libtrilobite/libtrilobite-service.h>
#include <libtrilobite/libtrilobite.h>

#ifdef STOP_IN_DEBUG
/* Also add 
	$(top_builddir)/libnautilus-extensions/nautilus-debug.o				 	\
to the ldadd line in Makefile.am
*/
#include <libnautilus-extensions/nautilus-debug.h>
#endif

#include <trilobite-eazel-install.h>
#include <eazel-install-public.h>

#include <libgnomevfs/gnome-vfs.h>

#define OAF_ID_FACTORY "OAFIID:trilobite_eazel_install_service_factory:b423ff3f-1941-4b0d-bd45-6542f64abbfc"
#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
CORBA_Environment         ev;

static BonoboGenericFactory *factory;
static int trilobites_active = 0;

static void trilobite_service_factory_destroy (GtkObject *object);
static void sig_segv_handler (int);

static void 
sig_segv_handler (int roedgroed_med_floede)
{
	g_error ("Crash: Install server hit a sigsegv");
}

static void
trilobite_service_factory_destroy (GtkObject *object) 
{
	trilobites_active--;

	trilobite_debug ("destroy, eazel_install trilobites active = %d", trilobites_active);
	if (trilobites_active != 0) {
		return;
	}

	trilobite_debug ("Destroying factory object");

	bonobo_object_unref (BONOBO_OBJECT (factory)); 
	trilobite_main_quit ();
}

static BonoboObject*
eazel_install_service_factory (BonoboGenericFactory *this_factory, 
			       const gchar *oaf_id,
			       gpointer data) 
{
	TrilobiteService *trilobite;
	TrilobitePasswordQuery *trilobite_password;

	EazelInstall *service;
	if (strcmp (oaf_id, OAF_ID)) {
		g_warning ("Unhandled OAF id %s", oaf_id);
		return NULL;
	}

	trilobite = TRILOBITE_SERVICE (gtk_object_new (TRILOBITE_TYPE_SERVICE,
						       "name", "Install",
						       "version", "0.1",
						       "vendor_name", "Eazel, inc.",
						       "vendor_url", "http://www.eazel.com",
						       "url", "http://www.eazel.com/",
						       "icon", "file:///gnome/share/pixmaps/gnome-default-dlg.png",
						       NULL));

	trilobite_password = TRILOBITE_PASSWORDQUERY (gtk_object_new (TRILOBITE_TYPE_PASSWORDQUERY, 
								      "prompt", "root", 
								      NULL));

	service = eazel_install_new_with_config ();

	g_assert (trilobite != NULL);
	g_assert (service != NULL);

	trilobites_active++;

	trilobite_service_add_interface (trilobite, BONOBO_OBJECT (service));
	trilobite_passwordquery_add_interface (trilobite_password, BONOBO_OBJECT (service));
	
	gtk_signal_connect (GTK_OBJECT (trilobite),
			    "destroy",
			    trilobite_service_factory_destroy, NULL);

	return BONOBO_OBJECT (trilobite);
}

int main(int argc, char *argv[]) {

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	signal (SIGSEGV, &sig_segv_handler);

	if (!trilobite_init ("trilobite-eazel-install-service", "0.1", "~/.nautilus/trilobite-install.log",
			     NULL, argc, argv)) {
		g_error ("Could not initialize trilobite. :(");
		exit (1);
	}
	trilobite_set_debug_mode (TRUE);

	gnome_vfs_init ();

	factory = bonobo_generic_factory_new_multi (OAF_ID_FACTORY, 
						    eazel_install_service_factory,
						    NULL);
	
	if (factory == NULL) {
		g_error ("Could not register factory");
	}

#ifdef STOP_IN_DEBUG
	nautilus_make_warnings_and_criticals_stop_in_debugger ("GLib", NULL);
#endif

	do {
		bonobo_activate ();
		trilobite_main ();
	} while (trilobites_active > 0);

	g_message ("EazelInstall service terminating");

	CORBA_exception_free (&ev);

	return 0;
};
