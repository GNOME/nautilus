/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
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

/* ntl-content-view-frame.c: Implementation for object that
   represents the frame a nautilus content view plugs into. */

#include <config.h>
#include "ntl-content-view-frame.h"
#include <bonobo/bonobo-control.h>

typedef struct {
  POA_Nautilus_View servant;
  gpointer bonobo_object;

  NautilusContentViewFrame *view;
} impl_POA_Nautilus_ContentView;

extern POA_Nautilus_View__epv libnautilus_Nautilus_View_epv;

static POA_Nautilus_ContentView__epv impl_Nautilus_ContentView_epv = {
  NULL /* _private */
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_ContentView__vepv impl_Nautilus_ContentView_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv,
  &impl_Nautilus_ContentView_epv
};

static void nautilus_content_view_frame_init       (NautilusContentViewFrame      *view);
static void nautilus_content_view_frame_destroy    (NautilusContentViewFrame      *view);
static void nautilus_content_view_frame_class_init (NautilusContentViewFrameClass *klass);

GtkType
nautilus_content_view_frame_get_type (void)
{
  static GtkType view_frame_type = 0;

  if (!view_frame_type)
    {
      const GtkTypeInfo view_frame_info =
      {
	"NautilusContentViewFrame",
	sizeof (NautilusContentViewFrame),
	sizeof (NautilusContentViewFrameClass),
	(GtkClassInitFunc) nautilus_content_view_frame_class_init,
	(GtkObjectInitFunc) nautilus_content_view_frame_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      view_frame_type = gtk_type_unique (nautilus_view_frame_get_type(), &view_frame_info);
    }
	
  return view_frame_type;
}

static void
nautilus_content_view_frame_init       (NautilusContentViewFrame *view)
{
}

NautilusContentViewFrame *
nautilus_content_view_frame_new (GtkWidget *widget)
{
  BonoboObject *control;
  
  control = BONOBO_OBJECT (bonobo_control_new (widget));

  return nautilus_content_view_frame_new_from_bonobo_control (control);
}

NautilusContentViewFrame *
nautilus_content_view_frame_new_from_bonobo_control (BonoboObject *bonobo_control)
{
  NautilusContentViewFrame *view;
  
  view = NAUTILUS_CONTENT_VIEW_FRAME (gtk_object_new (NAUTILUS_TYPE_CONTENT_VIEW_FRAME,
						      "bonobo_control", bonobo_control,
						      NULL));

  return view;
}

static void
nautilus_content_view_frame_destroy    (NautilusContentViewFrame *view)
{  
  NautilusViewFrameClass *klass = NAUTILUS_VIEW_FRAME_CLASS(GTK_OBJECT(view)->klass);

  if(((GtkObjectClass *)klass->parent_class)->destroy)
    ((GtkObjectClass *)klass->parent_class)->destroy((GtkObject *)view);
}

static void
nautilus_content_view_frame_class_init (NautilusContentViewFrameClass *klass)
{
  NautilusViewFrameClass *view_class = ((NautilusViewFrameClass *)klass);

  GTK_OBJECT_CLASS(klass)->destroy = (void (*)(GtkObject *))nautilus_content_view_frame_destroy;

  view_class->servant_init_func = POA_Nautilus_ContentView__init;
  view_class->servant_destroy_func = POA_Nautilus_ContentView__fini;
  view_class->vepv = &impl_Nautilus_ContentView_vepv;
}
