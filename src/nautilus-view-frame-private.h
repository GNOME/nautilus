/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

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

/* nautilus-view-frame-private.h: Internals of the view proxy that are shared between different implementation files */

#ifndef NAUTILUS_VIEW_FRAME_PRIVATE_H
#define NAUTILUS_VIEW_FRAME_PRIVATE_H

#include "nautilus-view-frame.h"

typedef struct {
        POA_Nautilus_ViewFrame servant;
        gpointer bonobo_object;
        
        NautilusViewFrame *view;
} impl_POA_Nautilus_ViewFrame;

extern POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv;
extern POA_Nautilus_ZoomableFrame__vepv impl_Nautilus_ZoomableFrame_vepv;

BonoboObject *impl_Nautilus_ViewFrame__create              (NautilusViewFrame                    *view,
                                                            CORBA_Environment                    *ev);
BonoboObject *impl_Nautilus_ZoomableFrame__create          (NautilusViewFrame                    *view,
                                                            CORBA_Environment                    *ev);
void          nautilus_view_frame_request_location_change  (NautilusViewFrame                    *view,
                                                            const Nautilus_NavigationRequestInfo *loc);
void          nautilus_view_frame_request_selection_change (NautilusViewFrame                    *view,
                                                            const Nautilus_SelectionRequestInfo  *loc);
void          nautilus_view_frame_request_status_change    (NautilusViewFrame                    *view,
                                                            const Nautilus_StatusRequestInfo     *loc);
void          nautilus_view_frame_request_progress_change  (NautilusViewFrame                    *view,
                                                            const Nautilus_ProgressRequestInfo   *loc);
void          nautilus_view_frame_request_title_change     (NautilusViewFrame                    *view,
                                                            const char                           *title);
void          nautilus_view_frame_notify_zoom_level        (NautilusViewFrame                    *view,
                                                            double                                level);

struct NautilusViewComponentType {
        const char *primary_repoid;
        gboolean (* try_load)(NautilusViewFrame *view, CORBA_Object obj, CORBA_Environment *ev);
        void (* destroy) (NautilusViewFrame *view, CORBA_Environment *ev);
        void (* save_state)(NautilusViewFrame *view, const char *config_path, CORBA_Environment *ev);
        void (* load_state)(NautilusViewFrame *view, const char *config_path, CORBA_Environment *ev);
        void (* notify_location_change)(NautilusViewFrame *view, Nautilus_NavigationInfo *nav_ctx, CORBA_Environment *ev);
        void (* stop_location_change)(NautilusViewFrame *view, CORBA_Environment *ev);
        void (* notify_selection_change)(NautilusViewFrame *view, Nautilus_SelectionInfo *nav_ctx, CORBA_Environment *ev);
        void (* show_properties)(NautilusViewFrame *view, CORBA_Environment *ev);
};

#endif /* NTL_VIEW_PRIVATE_H */

