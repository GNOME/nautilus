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
} BonoboControlInfo;

static void
destroy_bonobo_control_view(NautilusView *view, CORBA_Environment *ev)
{
  BonoboControlInfo *bci = view->component_data;
  g_free(bci);
}

static void
nautilus_view_activate_uri(BonoboControlFrame *frame, const char *uri, gboolean relative, NautilusView *view)
{
  Nautilus_NavigationRequestInfo nri;
  g_assert(!relative);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = (char *)uri;
  nautilus_view_request_location_change(view, &nri);
}

static gboolean
bonobo_control_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboControlInfo *bci;
  Bonobo_UIHandler uih = bonobo_object_corba_objref(BONOBO_OBJECT(nautilus_window_get_uih(NAUTILUS_WINDOW(view->main_window))));

  view->component_data = bci = g_new0(BonoboControlInfo, 1);

  bci->control_frame = BONOBO_OBJECT(bonobo_control_frame_new(uih));
  bonobo_object_add_interface(BONOBO_OBJECT(bci->control_frame), view->view_frame);

  bonobo_control_frame_bind_to_control(BONOBO_CONTROL_FRAME(bci->control_frame), obj);

  view->client_widget = bonobo_control_frame_get_widget(BONOBO_CONTROL_FRAME(bci->control_frame));
  
  gtk_signal_connect(GTK_OBJECT(bci->control_frame),
                     "activate_uri", GTK_SIGNAL_FUNC(nautilus_view_activate_uri), view);

  return TRUE;
}

static char *
bonobo_control_get_label(NautilusView *view, CORBA_Environment *ev)
{
  return g_strdup_printf(_("Control %p"), view);
}

static void
bonobo_control_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *real_nav_ctx, const char *initial_title, CORBA_Environment *ev)
{
  Nautilus_ProgressRequestInfo pri;
  pri.amount = 0;
  pri.type = Nautilus_PROGRESS_UNDERWAY;
  nautilus_view_request_progress_change(view, &pri);
  pri.type = Nautilus_PROGRESS_DONE_OK;
  nautilus_view_request_progress_change(view, &pri);
}

NautilusViewComponentType bonobo_control_component_type = {
  "IDL:Bonobo/Control:1.0",
  &bonobo_control_try_load_client, /* try_load */
  &destroy_bonobo_control_view, /* destroy */
  NULL, /* save_state */
  NULL, /* load_state */
  &bonobo_control_notify_location_change, /* notify_location_change */
  NULL, /* stop_location_change */
  NULL, /* notify_selection_change */
  NULL, /* notify_title_change */
  NULL, /* show_properties */
  &bonobo_control_get_label /* get_label */
};
