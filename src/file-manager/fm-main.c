/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

#include "config.h"

#include <libnautilus/libnautilus.h>

#include "fm-directory-view.h"
#include "fm-directory-view-icons.h"
#include "fm-directory-view-list.h"

static int object_count = 0;

static void
do_destroy(GtkObject *obj)
{
  object_count--;
  if(object_count <= 0)
    gtk_main_quit();
}

static GnomeObject *make_obj(GnomeGenericFactory *Factory, const char *goad_id, gpointer closure)
{
  FMDirectoryView *dir_view;
  GnomeObject *ctl;
  
  g_return_val_if_fail(strcmp(goad_id, "ntl_file_manager_icon_view") == 0 ||
  		       strcmp(goad_id, "ntl_file_manager_list_view") == 0, NULL);

  if (strcmp (goad_id, "ntl_file_manager_icon_view") == 0)
  {
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_icons_get_type(), NULL));
  }
  else
  {
    dir_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_directory_view_list_get_type(), NULL));
  }

  g_return_val_if_fail(dir_view, NULL);

  if(GNOME_IS_OBJECT(dir_view->view_frame))
    return GNOME_OBJECT(dir_view->view_frame);

  gtk_signal_connect(GTK_OBJECT(dir_view->view_frame), "destroy", do_destroy, NULL);

  gtk_widget_show(GTK_WIDGET(dir_view->view_frame));

  ctl = nautilus_view_frame_get_gnome_object(NAUTILUS_VIEW_FRAME(dir_view->view_frame));

  object_count++;

  printf("%x\n", (unsigned) ctl);
  
  return ctl;
}


int main(int argc, char *argv[])
{
  GnomeGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);

  if (getenv("NAUTILUS_DEBUG"))
    g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  orb = gnome_CORBA_init_with_popt_table("ntl-file-manager", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  g_thread_init (NULL);
  gnome_vfs_init ();

  factory = gnome_generic_factory_new_multi("ntl_file_manager_factory", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}
