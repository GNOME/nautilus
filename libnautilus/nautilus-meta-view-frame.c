/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
/* ntl-meta-view-frame.c: Implementation for object that represents a
   nautilus meta view implementation. */
#include "libnautilus.h"

typedef struct {
  POA_Nautilus_View servant;
  gpointer gnome_object;

  NautilusMetaViewFrame *view;
} impl_POA_Nautilus_MetaView;

extern POA_Nautilus_View__epv libnautilus_Nautilus_View_epv;

static POA_Nautilus_MetaView__epv impl_Nautilus_MetaView_epv = {
  NULL /* _private */
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_MetaView__vepv impl_Nautilus_MetaView_vepv =
{
  &base_epv,
  NULL,
  &libnautilus_Nautilus_View_epv,
  &impl_Nautilus_MetaView_epv
};

static void nautilus_meta_view_frame_init       (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_destroy    (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_class_init (NautilusMetaViewFrameClass *klass);

GtkType
nautilus_meta_view_frame_get_type (void)
{
  static GtkType view_frame_type = 0;

  if (!view_frame_type)
    {
      const GtkTypeInfo view_frame_info =
      {
	"NautilusMetaViewFrame",
	sizeof (NautilusMetaViewFrame),
	sizeof (NautilusMetaViewFrameClass),
	(GtkClassInitFunc) nautilus_meta_view_frame_class_init,
	(GtkObjectInitFunc) nautilus_meta_view_frame_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      view_frame_type = gtk_type_unique (nautilus_view_frame_get_type(), &view_frame_info);
    }
	
  return view_frame_type;
}

static void
nautilus_meta_view_frame_init       (NautilusMetaViewFrame *view)
{
}

static void
nautilus_meta_view_frame_destroy    (NautilusMetaViewFrame *view)
{  
  NautilusViewFrameClass *klass = NAUTILUS_VIEW_FRAME_CLASS(GTK_OBJECT(view)->klass);

  if(((GtkObjectClass *)klass->parent_class)->destroy)
    ((GtkObjectClass *)klass->parent_class)->destroy((GtkObject *)view);
}

static void
nautilus_meta_view_frame_class_init (NautilusMetaViewFrameClass *klass)
{
  NautilusViewFrameClass *view_class = ((NautilusViewFrameClass *)klass);

  GTK_OBJECT_CLASS(klass)->destroy = (void (*)(GtkObject *))nautilus_meta_view_frame_destroy;

  view_class->servant_init_func = POA_Nautilus_MetaView__init;
  view_class->servant_destroy_func = POA_Nautilus_MetaView__fini;
  view_class->vepv = &impl_Nautilus_MetaView_vepv;
}

void
nautilus_meta_view_frame_set_label(NautilusMetaViewFrame *mvc, const char *label)
{
  GnomeObject *ctl;
  GnomePropertyBag *bag;

  ctl = nautilus_view_frame_get_gnome_object(NAUTILUS_VIEW_FRAME(mvc));
  bag = gnome_control_get_property_bag(GNOME_CONTROL(ctl));
  if(!bag)
    {
      bag = gnome_property_bag_new();
      gnome_control_set_property_bag(GNOME_CONTROL(ctl), bag);
    }

  gnome_property_bag_add(bag, "label", "string",
			 g_strdup(label), g_strdup(label),
			 _("Label"), GNOME_PROPERTY_READ_ONLY);
}
