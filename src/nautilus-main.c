/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include "config.h"
#include "nautilus.h"
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#include <file-manager/fm-public-api.h>

static int window_count = 0;
static void
check_for_quit(void)
{
  if(--window_count <= 0)
    gtk_main_quit();
}

static GnomeObject *
nautilus_make_object(GnomeGenericFactory *gfact, const char *goad_id, gpointer closure)
{
  GtkObject *theobj = NULL;

  if(!strcmp(goad_id, "ntl_file_manager"))
    theobj = gtk_object_new(fm_directory_view_get_type(), NULL);

  if(!theobj)
    return NULL;

  if(GNOME_IS_OBJECT(theobj))
    return GNOME_OBJECT(theobj);

  if(NAUTILUS_IS_VIEW_CLIENT(theobj))
    {
      gtk_widget_show(GTK_WIDGET(theobj));
      return nautilus_view_client_get_gnome_object(NAUTILUS_VIEW_CLIENT(theobj));
    }

  gtk_object_destroy(theobj);

  return NULL;
}

int main(int argc, char *argv[])
{
  poptContext ctx;
  CORBA_Environment ev;
  CORBA_ORB orb;
  struct poptOption options[] = {
    {NULL}
  };
  GtkWidget *mainwin;
  GnomeGenericFactory *gfact;

  orb = gnome_CORBA_init_with_popt_table("nautilus", VERSION, &argc, argv, options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  g_thread_init(NULL);
  gnome_vfs_init();

  gfact = gnome_generic_factory_new_multi("nautilus_factory", nautilus_make_object, NULL);

  mainwin = gtk_widget_new(nautilus_window_get_type(), "app_id", "nautilus", NULL);
  bonobo_activate();
  nautilus_window_set_initial_state(NAUTILUS_WINDOW(mainwin));
  gtk_widget_show(mainwin);
  window_count++;

  gtk_signal_connect(GTK_OBJECT(mainwin), "destroy", check_for_quit, NULL);

  bonobo_main();
  return 0;
}
