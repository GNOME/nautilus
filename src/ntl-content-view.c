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
/* ntl-content-view.c: Implementation of the object representing a content view. */

#include "ntl-content-view.h"
#include <gtk/gtksignal.h>

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_ContentViewFrame__epv impl_Nautilus_ContentViewFrame_epv = {
  NULL
};

extern POA_Nautilus_ViewFrame__epv impl_Nautilus_ViewFrame_epv;

static POA_Nautilus_ContentViewFrame__vepv impl_Nautilus_ContentViewFrame_vepv =
{
   &base_epv,
   NULL,
   &impl_Nautilus_ViewFrame_epv,
   &impl_Nautilus_ContentViewFrame_epv
};

static void nautilus_content_view_class_init (NautilusContentViewClass *klass);
static void nautilus_content_view_init (NautilusContentView *view);

GtkType
nautilus_content_view_get_type(void)
{
  static guint view_type = 0;

  if (!view_type)
    {
      GtkTypeInfo view_info = {
	"NautilusContentView",
	sizeof(NautilusContentView),
	sizeof(NautilusContentViewClass),
	(GtkClassInitFunc) nautilus_content_view_class_init,
	(GtkObjectInitFunc) nautilus_content_view_init,
        NULL,
        NULL,
        NULL
      };

      view_type = gtk_type_unique (nautilus_view_get_type(), &view_info);
    }

  return view_type;
}

static void
nautilus_content_view_class_init (NautilusContentViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));

  NAUTILUS_VIEW_CLASS(klass)->servant_init_func = POA_Nautilus_ContentViewFrame__init;
  NAUTILUS_VIEW_CLASS(klass)->servant_destroy_func = POA_Nautilus_ContentViewFrame__fini;
  NAUTILUS_VIEW_CLASS(klass)->vepv = &impl_Nautilus_ContentViewFrame_vepv;
}

static void
nautilus_content_view_init (NautilusContentView *view)
{
}
