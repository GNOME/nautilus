/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- c-set-style: linux */

/* nautilus-gconf.c - GConf-related functions

   Copyright (C) 2000 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Havoc Pennington <hp@redhat.com>
*/

#include "nautilus-gconf.h"

static GConfClient *global_client = NULL;

GConfClient *
nautilus_gconf_client_get (void)
{
        if (global_client == NULL) {
                global_client = gconf_client_new();
                /* Make sure we own the client */
                gtk_object_ref(GTK_OBJECT(global_client));
                gtk_object_sink(GTK_OBJECT(global_client));
        }

        return global_client;
}

void
nautilus_gconf_shutdown (void)
{
        if (global_client != NULL) {
                gtk_object_unref(GTK_OBJECT(global_client));
        }
}
