/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>,
 *
 */

#include "nautilus.h"

static int window_count = 0;

/**
 * nautilus_app_exiting:
 * 
 * Called after the main event loop has finished, just before the
 * program ends. Don't call from anywhere else.
 **/
void
nautilus_app_exiting()
{
  /* Do those things that gotta be done just once before quitting */
  nautilus_prefs_save();
  nautilus_bookmarks_menu_exiting();
}

void
nautilus_app_init(const char *initial_url)
{
  NautilusWindow *mainwin;

  nautilus_navinfo_init();
  nautilus_prefs_load();

  /* Set default configuration */
  mainwin = nautilus_app_create_window();
  bonobo_activate();
  nautilus_window_set_initial_state(mainwin, initial_url);
}

static void
nautilus_app_destroy_window(GtkObject *obj)
{
  window_count--;

  if(window_count <= 0)
    {
      gtk_main_quit();
    }
}

NautilusWindow *
nautilus_app_create_window(void)
{
  GtkWidget *win = gtk_widget_new(nautilus_window_get_type(), "app_id", "nautilus", NULL);

  window_count++;

  gtk_signal_connect(GTK_OBJECT(win), "destroy", nautilus_app_destroy_window, NULL);

  gtk_widget_show(win);

  return NAUTILUS_WINDOW(win);
}
