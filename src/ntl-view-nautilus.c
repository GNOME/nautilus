/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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

#include <config.h>

#include "nautilus.h"
#include "ntl-view-private.h"

typedef struct {
  BonoboObject *control_frame;
  CORBA_Object view_client;
} NautilusViewInfo;

static gboolean
nautilus_view_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  Bonobo_Control control;
  NautilusViewInfo *nvi;
  Bonobo_UIHandler uih = bonobo_object_corba_objref(BONOBO_OBJECT(nautilus_window_get_uih(NAUTILUS_WINDOW(view->main_window))));
  nvi = view->component_data = g_new0(NautilusViewInfo, 1);

  control = Bonobo_Unknown_query_interface(obj, "IDL:Bonobo/Control:1.0", ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    control = CORBA_OBJECT_NIL;

  if(CORBA_Object_is_nil(control, ev))
    goto out;

  nvi->view_client = CORBA_Object_duplicate(obj, ev);

  nvi->control_frame = BONOBO_OBJECT(bonobo_control_frame_new(uih));
  bonobo_object_add_interface(BONOBO_OBJECT(nvi->control_frame), view->view_frame);

  bonobo_control_frame_bind_to_control(BONOBO_CONTROL_FRAME(nvi->control_frame), control);
  view->client_widget = bonobo_control_frame_get_widget(BONOBO_CONTROL_FRAME(nvi->control_frame));

  Bonobo_Unknown_unref(control, ev);
  CORBA_Object_release(control, ev);

  return TRUE;

 out:
  g_free(nvi);

  return FALSE;
}

static void
destroy_nautilus_view(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  CORBA_Object_release(nvi->view_client, ev);

  g_free(nvi);
}

static void
nv_show_properties(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_show_properties(nvi->view_client, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
nv_save_state(NautilusView *view, const char *config_path, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_save_state(nvi->view_client, config_path, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
nv_load_state(NautilusView *view, const char *config_path, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_load_state(nvi->view_client, config_path, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
nv_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *nav_ctx, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_notify_location_change(nvi->view_client, nav_ctx, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
nv_notify_selection_change(NautilusView *view, Nautilus_SelectionInfo *nav_ctx, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_notify_selection_change(nvi->view_client, nav_ctx, ev);

  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static void
nv_stop_location_change(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_stop_location_change(nvi->view_client, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    gtk_object_destroy(GTK_OBJECT(view));
}

static char *
nv_get_label(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;
  BonoboPropertyBagClient *bc;
  Bonobo_Property prop;
  char *retval = NULL;
  CORBA_any *anyval;
  BonoboControlFrame *control_frame;

  control_frame = BONOBO_CONTROL_FRAME(nvi->control_frame);
  bc = bonobo_control_frame_get_control_property_bag(control_frame);
  g_return_val_if_fail(bc, NULL);

  prop = bonobo_property_bag_client_get_property(bc, "label");

  if(CORBA_Object_is_nil(prop, ev))
    return NULL;

  anyval = Bonobo_Property_get_value(prop, ev);
  if(ev->_major == CORBA_NO_EXCEPTION && CORBA_TypeCode_equal(anyval->_type, TC_string, ev))
    {
      retval = g_strdup(*(CORBA_char **)anyval->_value);

      CORBA_free(anyval);
    }

  return retval;
}

NautilusViewComponentType nautilus_view_component_type = {
  "IDL:Nautilus/View:1.0",
  &nautilus_view_try_load_client, /* try_load */
  &destroy_nautilus_view, /* destroy */
  &nv_save_state, /* save_state */
  &nv_load_state, /* load_state */
  &nv_notify_location_change, /* notify_location_change */
  &nv_stop_location_change, /*stop_location_change */
  &nv_notify_selection_change, /* notify_selection_change */
  &nv_show_properties, /* show_properties */
  &nv_get_label /* get_label */
};
