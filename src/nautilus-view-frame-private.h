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

BonoboObject *impl_Nautilus_ViewFrame__create                 (NautilusViewFrame *view,
                                                               CORBA_Environment *ev);
BonoboObject *impl_Nautilus_ZoomableFrame__create             (NautilusViewFrame *view,
                                                               CORBA_Environment *ev);

/* ViewFrame */
void          nautilus_view_frame_open_location               (NautilusViewFrame *view,
                                                               const char        *location);
void          nautilus_view_frame_open_location_in_new_window (NautilusViewFrame *view,
                                                               const char        *location);
void          nautilus_view_frame_report_location_change      (NautilusViewFrame *view,
                                                               const char        *location);
void          nautilus_view_frame_report_selection_change     (NautilusViewFrame *view,
                                                               GList             *selection);
void          nautilus_view_frame_report_status               (NautilusViewFrame *view,
                                                               const char        *status);
void          nautilus_view_frame_report_load_underway        (NautilusViewFrame *view);
void          nautilus_view_frame_report_load_progress        (NautilusViewFrame *view,
                                                               double             fraction_done);
void          nautilus_view_frame_report_load_complete        (NautilusViewFrame *view);
void          nautilus_view_frame_report_load_failed          (NautilusViewFrame *view);
void          nautilus_view_frame_set_title                   (NautilusViewFrame *view,
                                                               const char        *title);
void          nautilus_view_frame_quit_nautilus		      (NautilusViewFrame *view);
void          nautilus_view_frame_close_desktop		      (NautilusViewFrame *view);

/* Zoomable */
void          nautilus_view_frame_zoom_level_changed          (NautilusViewFrame *view,
                                                               double             zoom_level);
void          nautilus_view_frame_zoom_parameters_changed     (NautilusViewFrame *view,
                                                               double             zoom_level,
                                                               double             min_zoom_level,
                                                               double             max_zoom_level);

struct NautilusViewComponentType {
        const char *primary_repoid;
        gboolean (* try_load) (NautilusViewFrame *view, CORBA_Object obj, CORBA_Environment *ev);
        void (* destroy) (NautilusViewFrame *view, CORBA_Environment *ev);
        void (* load_location) (NautilusViewFrame *view, Nautilus_URI location, CORBA_Environment *ev);
        void (* stop_loading) (NautilusViewFrame *view, CORBA_Environment *ev);
        void (* selection_changed) (NautilusViewFrame *view, const Nautilus_URIList *selection, CORBA_Environment *ev);
};

#endif /* NAUTILUS_VIEW_FRAME_PRIVATE_H */

