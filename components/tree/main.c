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

#include "nautilus-tree-view.h"

#include <libnautilus-extensions/nautilus-debug.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs.h>


static int object_count = 0;

static void
tree_object_destroyed (GtkObject *object)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
tree_make_object (BonoboGenericFactory *factory, 
		    const char *iid, 
		    void *closure)
{
	NautilusTreeView *view;
	NautilusView *nautilus_view;

	if (strcmp (iid, "OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d")) {
		return NULL;
	}

	view = NAUTILUS_TREE_VIEW (gtk_object_new (NAUTILUS_TYPE_TREE_VIEW, NULL));

	object_count++;

	nautilus_view = nautilus_tree_view_get_nautilus_view (view);

	gtk_signal_connect (GTK_OBJECT (view), "destroy", tree_object_destroyed, NULL);

	return BONOBO_OBJECT (nautilus_view);
}

int
main (int argc, char *argv[])
{
	CORBA_ORB orb;
	BonoboGenericFactory *factory;

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	/* Initialize the services that we use. */

	g_thread_init (NULL);
	
        gnome_init_with_popt_table("nautilus-tree-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	orb = oaf_init (argc, argv);

	gnome_vfs_init ();
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	factory = bonobo_generic_factory_new_multi
		("OAFIID:nautilus_tree_view_factory:79f93d13-d404-4ef6-8de2-b8a0045a96ab",
		 tree_make_object, NULL);
		
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
