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
 * Author: Andy Hertzfeld
 */

/* main.c - main function and object activation function for the music view component. */

#include <config.h>
#include "nautilus-music-view.h"

#include <bonobo.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <liboaf/liboaf.h>

static int object_count = 0;

static void
music_view_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
music_view_make_object (BonoboGenericFactory *factory, 
			const char *iid, 
			void *closure)
{
	NautilusMusicView *music_view;
	NautilusView *nautilus_view;

	if (strcmp (iid, "OAFIID:nautilus_music_view:9456b5d2-60a8-407f-a56e-d561e1821391")) {
		return NULL;
	}
	
	music_view = NAUTILUS_MUSIC_VIEW (gtk_object_new (NAUTILUS_TYPE_MUSIC_VIEW, NULL));

	object_count++;

	nautilus_view = nautilus_music_view_get_nautilus_view (music_view);

	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", music_view_object_destroyed, NULL);

	return BONOBO_OBJECT (nautilus_view);
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	char *registration_id;

	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	/* Make criticals and warnings stop in the debugger if
	 * NAUTILUS_DEBUG is set. Unfortunately, this has to be done
	 * explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib,
			 "Bonobo",
			 "Gdk",
			 "GnomeUI",
			 "GnomeVFS",
			 "GnomeVFS-CORBA",
			 "GnomeVFS-pthread",
			 "Gtk",
			 "Nautilus",
			 "Nautilus-Authenticate",
			 "Nautilus-Tree",
			 "ORBit",
			 NULL);
	}
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

        gnome_init_with_popt_table("nautilus-music-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	gdk_rgb_init ();
	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* initialize gnome-vfs, etc */
	g_thread_init (NULL);
	gnome_vfs_init ();
	
        registration_id = oaf_make_registration_id ("OAFIID:nautilus_music_view_factory:1be0c129-87cd-4daa-9d3a-94397de9bce2", getenv ("DISPLAY"));
	factory = bonobo_generic_factory_new_multi (registration_id, 
						    music_view_make_object,
						    NULL);
	g_free (registration_id);

	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
        gnome_vfs_shutdown ();

	return 0;
}
