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
/* ntl-view-private.h: Internals of the view proxy that are shared between different implementation files */
#ifndef NTL_VIEW_PRIVATE_H
#define NTL_VIEW_PRIVATE_H 1

#include "nautilus.h"

extern POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv;
GnomeObject *impl_Nautilus_ViewFrame__create(NautilusView *view, CORBA_Environment * ev);

void nautilus_view_request_location_change(NautilusView *view,
					   Nautilus_NavigationRequestInfo *loc);
void nautilus_view_request_selection_change(NautilusView *view,
					    Nautilus_SelectionRequestInfo *loc);
void nautilus_view_request_status_change(NautilusView *view,
					 Nautilus_StatusRequestInfo *loc);
void nautilus_view_request_progress_change(NautilusView              *view,
					   Nautilus_ProgressRequestInfo *loc);

struct _NautilusViewComponentType {
  const char *primary_repoid;
  gboolean (* try_load)(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev);
  void (* destroy) (NautilusView *view, CORBA_Environment *ev);
  void (* show_properties)(NautilusView *view, CORBA_Environment *ev);
  void (* save_state)(NautilusView *view, const char *config_path, CORBA_Environment *ev);
  void (* load_state)(NautilusView *view, const char *config_path, CORBA_Environment *ev);
  void (* notify_location_change)(NautilusView *view, Nautilus_NavigationInfo *nav_ctx, CORBA_Environment *ev);
  void (* notify_selection_change)(NautilusView *view, Nautilus_SelectionInfo *nav_ctx, CORBA_Environment *ev);
  void (* stop_location_change)(NautilusView *view, CORBA_Environment *ev);

  char * (* get_label)(NautilusView *view, CORBA_Environment *ev);
};

#endif
