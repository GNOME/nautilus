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

/* main.c - main function and object activation function for the hardware view component. */

#include <config.h>

#include "nautilus-hardware-view.h"

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

static int object_count = 0;

static void
hardware_view_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
hardware_view_make_object (BonoboGenericFactory *factory, 
		    const char *goad_id, 
		    void *closure)
{
	NautilusHardwareView *hardware_view;

	if (strcmp (goad_id, "OAFIID:nautilus_hardware_view:20000422-2250")) {
		return NULL;
	}
	
	hardware_view = NAUTILUS_HARDWARE_VIEW (gtk_object_new (NAUTILUS_TYPE_HARDWARE_VIEW, NULL));

	object_count++;

	gtk_signal_connect (GTK_OBJECT (hardware_view), "destroy", hardware_view_object_destroyed, NULL);

	return BONOBO_OBJECT (nautilus_hardware_view_get_nautilus_view (hardware_view));
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	CORBA_Environment ev;
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	CORBA_exception_init(&ev);
	
        gnome_init_with_popt_table("nautilus-hardware-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* initialize gnome-vfs, etc */
	g_thread_init (NULL);
	gnome_vfs_init ();
	
	factory = bonobo_generic_factory_new_multi ("OAFIID:nautilus_hardware_view_factory:20000422-2250", 
						    hardware_view_make_object, NULL);
	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
