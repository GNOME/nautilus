/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus clients.
 *
 *  Copyright (C) 1999 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-content-view-client.c: Implementation for object that represents a nautilus content view implementation. */
#include "libnautilus.h"

typedef struct {
  POA_Nautilus_View servant;
  gpointer gnome_object;

  NautilusContentViewClient *view;
} impl_POA_Nautilus_ContentView;

extern POA_Nautilus_View__epv libnautilus_Nautilus_View_epv;

static POA_Nautilus_ContentView__epv impl_Nautilus_ContentView_epv = {
  NULL /* _private */
};

static PortableServer_ServantBase__epv base_epv = { NULL};

static POA_Nautilus_ContentView__vepv impl_Nautilus_ContentView_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv,
  &impl_Nautilus_ContentView_epv
};

static void nautilus_content_view_client_init       (NautilusContentViewClient      *view);
static void nautilus_content_view_client_destroy    (NautilusContentViewClient      *view);
static void nautilus_content_view_client_class_init (NautilusContentViewClientClass *klass);

GtkType
nautilus_content_view_client_get_type (void)
{
  static GtkType view_client_type = 0;

  if (!view_client_type)
    {
      const GtkTypeInfo view_client_info =
      {
	"NautilusContentViewClient",
	sizeof (NautilusContentViewClient),
	sizeof (NautilusContentViewClientClass),
	(GtkClassInitFunc) nautilus_content_view_client_class_init,
	(GtkObjectInitFunc) nautilus_content_view_client_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      view_client_type = gtk_type_unique (nautilus_view_client_get_type(), &view_client_info);
    }
	
  return view_client_type;
}

static void
nautilus_content_view_client_init       (NautilusContentViewClient *view)
{
}

static void
nautilus_content_view_client_destroy    (NautilusContentViewClient *view)
{  
  NautilusViewClientClass *klass = NAUTILUS_VIEW_CLIENT_CLASS(GTK_OBJECT(view)->klass);

  if(((GtkObjectClass *)klass->parent_class)->destroy)
    ((GtkObjectClass *)klass->parent_class)->destroy((GtkObject *)view);
}

static void
nautilus_content_view_client_class_init (NautilusContentViewClientClass *klass)
{
  NautilusViewClientClass *view_class = ((NautilusViewClientClass *)klass);

  GTK_OBJECT_CLASS(klass)->destroy = (void (*)(GtkObject *))nautilus_content_view_client_destroy;

  view_class->servant_init_func = POA_Nautilus_ContentView__init;
  view_class->servant_destroy_func = POA_Nautilus_ContentView__fini;
  view_class->vepv = &impl_Nautilus_ContentView_vepv;
}
