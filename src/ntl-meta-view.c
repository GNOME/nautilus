/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-meta-view.c: Implementation of the object representing a meta/navigation view. */

#include "nautilus.h"
#include "ntl-view-private.h"
#include <gtk/gtksignal.h>
#include <libnautilus/nautilus-gtk-macros.h>

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_MetaViewFrame__epv impl_Nautilus_MetaViewFrame_epv = {
  NULL
};

extern POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv;

static POA_Nautilus_MetaViewFrame__vepv impl_Nautilus_MetaViewFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewFrame_epv,
   &impl_Nautilus_MetaViewFrame_epv
};

static void nautilus_meta_view_initialize_class (NautilusMetaViewClass *klass);
static void nautilus_meta_view_initialize (NautilusMetaView *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMetaView, nautilus_meta_view, NAUTILUS_TYPE_VIEW)

static void
nautilus_meta_view_initialize_class (NautilusMetaViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  NautilusViewClass *view_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  view_class = (NautilusViewClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  view_class->servant_init_func = POA_Nautilus_MetaViewFrame__init;
  view_class->servant_destroy_func = POA_Nautilus_MetaViewFrame__fini;
  view_class->vepv = &impl_Nautilus_MetaViewFrame_vepv;
}

static void
nautilus_meta_view_initialize (NautilusMetaView *view)
{
}


const char *
nautilus_meta_view_get_label(NautilusMetaView *nview)
{
  NautilusView *view = NAUTILUS_VIEW(nview);
  char *retval = NULL;

  g_return_val_if_fail(view, NULL);
  g_return_val_if_fail(view->component_class, NULL);

  if(view->component_class->get_label)
    {
      CORBA_Environment ev;

      CORBA_exception_init(&ev);

      retval = view->component_class->get_label(view, &ev);

      CORBA_exception_free(&ev);
    }

  return retval;
}





