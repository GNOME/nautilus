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

/* main.c - main function and object activation function for the rpm view component. */

#include <config.h>

#include "nautilus-rpm-view.h"

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

static int object_count = 0;

static void
rpm_view_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
rpm_view_make_object (BonoboGenericFactory *factory, 
		    const char *goad_id, 
		    void *closure)
{
	NautilusRPMView *rpm_view;
	NautilusView *nautilus_view;

	if (strcmp (goad_id, "OAFIID:nautilus_rpm_view:22ea002c-11e6-44fd-b13c-9445175a5e70")) {
		return NULL;
	}
	
	rpm_view = NAUTILUS_RPM_VIEW (gtk_object_new (NAUTILUS_TYPE_RPM_VIEW, NULL));

	object_count++;

	gtk_signal_connect (GTK_OBJECT (rpm_view), "destroy", rpm_view_object_destroyed, NULL);

	nautilus_view = nautilus_rpm_view_get_nautilus_view (rpm_view);
	return BONOBO_OBJECT (nautilus_view);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
        gnome_init_with_popt_table("nautilus-rpm-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* initialize gnome-vfs, etc */
	g_thread_init (NULL);
	gnome_vfs_init ();
	
	factory = bonobo_generic_factory_new_multi ("OAFIID:nautilus_rpm_view_factory:5986d6a5-8840-44ea-84a1-e7f052bd85cf", 
						    rpm_view_make_object, NULL);
	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
