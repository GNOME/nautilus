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
/* ntl-meta-view.c: Implementation of the object representing a meta/navigation view. */

#include "ntl-meta-view.h"
#include <gtk/gtksignal.h>

static PortableServer_ServantBase__epv base_epv = { NULL};

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

static void nautilus_meta_view_class_init (NautilusMetaViewClass *klass);
static void nautilus_meta_view_init (NautilusMetaView *view);

GtkType
nautilus_meta_view_get_type(void)
{
  static guint view_type = 0;

  if (!view_type)
    {
      GtkTypeInfo view_info = {
	"NautilusMetaView",
	sizeof(NautilusMetaView),
	sizeof(NautilusMetaViewClass),
	(GtkClassInitFunc) nautilus_meta_view_class_init,
	(GtkObjectInitFunc) nautilus_meta_view_init
      };

      view_type = gtk_type_unique (nautilus_view_get_type(), &view_info);
    }

  return view_type;
}

static void
nautilus_meta_view_class_init (NautilusMetaViewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  klass->parent_class = gtk_type_class (gtk_type_parent (object_class->type));
  NAUTILUS_VIEW_CLASS(klass)->servant_init_func = POA_Nautilus_MetaViewFrame__init;
  NAUTILUS_VIEW_CLASS(klass)->servant_destroy_func = POA_Nautilus_MetaViewFrame__fini;
  NAUTILUS_VIEW_CLASS(klass)->vepv = &impl_Nautilus_MetaViewFrame_vepv;
}

static void
nautilus_meta_view_init (NautilusMetaView *view)
{
}

NautilusMetaView *
nautilus_meta_view_new(void)
{
  return NAUTILUS_META_VIEW (gtk_type_new (nautilus_meta_view_get_type()));
}

const char *
nautilus_meta_view_get_label(NautilusMetaView *nview)
{
  NautilusView *view = NAUTILUS_VIEW(nview);
  GnomePropertyBagClient *bc;
  GNOME_Property prop;
  CORBA_Environment ev;
  char *retval = NULL;
  CORBA_any *anyval;

  g_return_val_if_fail(view->client, NULL);

  bc = gnome_control_frame_get_control_property_bag(gnome_bonobo_widget_get_control_frame(GNOME_BONOBO_WIDGET(view->client)));
  g_return_val_if_fail(bc, NULL);

  prop = gnome_property_bag_client_get_property(bc, "label");
  CORBA_exception_init(&ev);

  if(CORBA_Object_is_nil(prop, &ev))
    goto out;

  anyval = GNOME_Property_get_value(prop, &ev);
  if(ev._major != CORBA_NO_EXCEPTION)
    goto out;

  if(!CORBA_TypeCode_equal(anyval->_type, TC_string, &ev))
    goto out;

  retval = g_strdup(*(CORBA_char **)anyval->_value);

  CORBA_free(anyval);

 out:
  CORBA_exception_free(&ev);
  return retval;
}
