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
 * Author: Michael Fleming <mfleming@eazel.com> 
 * Based on nautilus-sample-content-view by Maciej Stachowiak <mjs@eazel.com>
 */

/* main.c - main function and object activation function for sample
   content view component. */

#include <config.h>

#include "trilobite-eazel-time-view.h"

#include <gnome.h>
#include <liboaf/liboaf.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <bonobo.h>
#include <glade/glade.h>

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
	TrilobiteEazelTimeView *view;
	NautilusView *nautilus_view;

	g_message ("Checking IID!");

	if (strcmp (iid, OAFIID_TRILOBITE_EAZEL_TIME_VIEW)) {
		return NULL;
	}

	g_message ("Trying to make object!");
	
	view = TRILOBITE_EAZEL_TIME_VIEW (gtk_object_new (TRILOBITE_TYPE_EAZEL_TIME_VIEW, NULL));

	object_count++;

	nautilus_view = trilobite_eazel_time_view_get_nautilus_view (view);

	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", sample_object_destroyed, NULL);

	return BONOBO_OBJECT (nautilus_view);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

        /* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
           Unfortunately, this has to be done explicitly for each domain.
        */
        if (getenv("NAUTILUS_DEBUG") != NULL)
        	nautilus_make_warnings_and_criticals_stop_in_debugger
        		(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", NULL);


        g_message ("In component.");        
	
        gnome_init_with_popt_table("trilobite-eazel-time-view", VERSION, 
				   argc, argv,
				   oaf_popt_options, 0, NULL); 

	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

#if 0
    	glade_gnome_init();
#endif /* 0 */

	factory = bonobo_generic_factory_new_multi (OAFIID_TRILOBITE_EAZEL_TIME_VIEW_FACTORY, sample_make_object, NULL);
		
	g_message ("About to do main loop.");        

	do {
		bonobo_main ();
	} while (object_count > 0);
	
	return 0;
}
