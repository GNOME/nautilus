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

#include "nautilus-view-frame-private.h"
#include "nautilus-window.h"

#include <bonobo/bonobo-control-frame.h>

typedef struct {
  BonoboControlFrame *control_frame;
} BonoboControlInfo;

static void
destroy_bonobo_control_view(NautilusViewFrame *view, CORBA_Environment *ev)
{
  BonoboControlInfo *bci = view->component_data;
  g_free(bci);
}

static void
nautilus_view_frame_activate_uri(BonoboControlFrame *frame, const char *uri, gboolean relative, NautilusViewFrame *view)
{
  /* FIXME: Can we ship with this assert?
   * Why is this not going to happen?
   */
  g_assert(!relative);
  nautilus_view_frame_open_location(view, uri);
}

static gboolean
bonobo_control_try_load_client(NautilusViewFrame *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboControlInfo *bci;
  Bonobo_UIHandler uih;

  view->component_data = bci = g_new0(BonoboControlInfo, 1);

  uih = bonobo_object_corba_objref(BONOBO_OBJECT(view->ui_handler));
  bci->control_frame = bonobo_control_frame_new(uih);
  bonobo_object_add_interface(BONOBO_OBJECT(bci->control_frame), view->view_frame);

  bonobo_control_frame_bind_to_control(bci->control_frame, obj);

  view->client_widget = bonobo_control_frame_get_widget(bci->control_frame);
  
  gtk_signal_connect(GTK_OBJECT(bci->control_frame),
                     "activate_uri", GTK_SIGNAL_FUNC(nautilus_view_frame_activate_uri), view);

  return TRUE;
}

static void
bonobo_control_load_location(NautilusViewFrame *view, Nautilus_URI location, CORBA_Environment *ev)
{
  nautilus_view_frame_report_load_underway(view);
  nautilus_view_frame_report_load_complete(view);
}

NautilusViewComponentType bonobo_control_component_type = {
  "IDL:Bonobo/Control:1.0",
  &bonobo_control_try_load_client, /* try_load */
  &destroy_bonobo_control_view, /* destroy */
  &bonobo_control_load_location, /* load_location */
  NULL, /* stop_loading */
  NULL /* selection_changed */
};
