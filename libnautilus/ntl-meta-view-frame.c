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

/* ntl-meta-view-frame.c: Implementation for object that represents a
   nautilus meta view implementation. */

#include <config.h>
#include "ntl-meta-view-frame.h"

#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <libnautilus/nautilus-gtk-macros.h>


typedef struct {
  POA_Nautilus_View servant;
  gpointer bonobo_object;

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

static void nautilus_meta_view_frame_initialize       (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_destroy          (NautilusMetaViewFrame      *view);
static void nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetaViewFrame, nautilus_meta_view_frame, NAUTILUS_TYPE_VIEW_FRAME)


static void
nautilus_meta_view_frame_initialize       (NautilusMetaViewFrame *view)
{
}

NautilusMetaViewFrame *
nautilus_meta_view_frame_new (GtkWidget *widget)
{
  BonoboObject *control;

  control = BONOBO_OBJECT (bonobo_control_new (widget));

  return nautilus_meta_view_frame_new_from_bonobo_control (control);
}

NautilusMetaViewFrame *
nautilus_meta_view_frame_new_from_bonobo_control (BonoboObject *bonobo_control)
{
  NautilusMetaViewFrame *view;
  
  view = NAUTILUS_META_VIEW_FRAME (gtk_object_new (NAUTILUS_TYPE_META_VIEW_FRAME,
						   "bonobo_control", bonobo_control,
						   NULL));

  return view;
}

static void
nautilus_meta_view_frame_destroy    (NautilusMetaViewFrame *view)
{  
  NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, GTK_OBJECT (view));
}

static void
nautilus_meta_view_frame_initialize_class (NautilusMetaViewFrameClass *klass)
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
  BonoboObject *ctl;
  BonoboPropertyBag *bag;

  ctl = nautilus_view_frame_get_bonobo_control (NAUTILUS_VIEW_FRAME (mvc));

  bag = bonobo_control_get_property_bag (BONOBO_CONTROL (ctl));

  if (!bag)
    {
      bag = bonobo_property_bag_new ();
      bonobo_control_set_property_bag (BONOBO_CONTROL (ctl), bag);
    }

  bonobo_property_bag_add (bag, "label", "string",
			   g_strdup (label), g_strdup (label),
			   _("Label"), BONOBO_PROPERTY_READ_ONLY);
}
