/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Maciej Stachowiak <mjs@eazel.com>
 *
 */

#include <config.h>

#include "fm-directory-view-icons.h"
#include "fm-directory-view-list.h"
#include <libnautilus/nautilus-debug.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <libgnomevfs/gnome-vfs-init.h>

static int object_count = 0;

static void
do_destroy (GtkObject *obj)
{
        object_count--;
        if (object_count <= 0)
                gtk_main_quit();
}

static BonoboObject *
make_obj (BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
        FMDirectoryView *dir_view;
        NautilusContentViewFrame *view_frame;
        
        g_return_val_if_fail (strcmp (goad_id, "ntl_file_manager_icon_view") == 0 ||
			      strcmp (goad_id, "ntl_file_manager_list_view") == 0, NULL);
        
        if (strcmp (goad_id, "ntl_file_manager_icon_view") == 0)
                 dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_icons_get_type (), NULL));
        else
                 dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_list_get_type (), NULL));
        
        g_return_val_if_fail(dir_view, NULL);
        
        view_frame = fm_directory_view_get_view_frame (dir_view);
        
        if (BONOBO_IS_OBJECT (view_frame))
                return BONOBO_OBJECT (view_frame);
        
        gtk_signal_connect (GTK_OBJECT (view_frame), "destroy", do_destroy, NULL);
        gtk_widget_show (GTK_WIDGET (view_frame));
        
        object_count++;
        
        return nautilus_view_frame_get_bonobo_object (NAUTILUS_VIEW_FRAME (view_frame));
}

int main(int argc, char *argv[])
{
        CORBA_Environment ev;
        CORBA_ORB orb;
        BonoboGenericFactory *factory;
        
        CORBA_exception_init(&ev);
        
	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	   Unfortunately, this has to be done explicitly for each domain.
	*/
	if (getenv("NAUTILUS_DEBUG") != NULL)
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", NULL);
        
        orb = gnome_CORBA_init_with_popt_table("ntl-file-manager", VERSION, &argc, argv, NULL, 0, NULL,
                                               GNORBA_INIT_SERVER_FUNC, &ev);
        bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
        g_thread_init (NULL);
        gnome_vfs_init ();
        
        factory = bonobo_generic_factory_new_multi("ntl_file_manager_factory", make_obj, NULL);
        
        do
                bonobo_main();
        while (object_count > 0);
        
        return EXIT_SUCCESS;
}
