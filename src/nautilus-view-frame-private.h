/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
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

typedef void (* NautilusViewFrameFunction) (NautilusViewFrame *view_frame,
                                            gpointer callback_data);

extern POA_Nautilus_ViewFrame__vepv impl_Nautilus_ViewFrame_vepv;

void          nautilus_view_frame_queue_incoming_call                  (PortableServer_Servant     servant,
                                                                        NautilusViewFrameFunction  call,
                                                                        gpointer                   callback_data,
                                                                        GDestroyNotify             destroy_callback_data);

BonoboObject *impl_Nautilus_ViewFrame__create                          (NautilusViewFrame         *view,
                                                                        CORBA_Environment         *ev);

/* ViewFrame */
void          nautilus_view_frame_open_location_in_this_window         (NautilusViewFrame         *view,
                                                                        const char                *location);
void          nautilus_view_frame_open_location_prefer_existing_window (NautilusViewFrame         *view,
                                                                        const char                *location);
void          nautilus_view_frame_open_location_force_new_window       (NautilusViewFrame         *view,
                                                                        const char                *location,
                                                                        GList                     *selection);
void          nautilus_view_frame_report_location_change               (NautilusViewFrame         *view,
                                                                        const char                *location,
                                                                        GList                     *selection,
                                                                        const char                *title);
void          nautilus_view_frame_report_redirect                      (NautilusViewFrame         *view,
                                                                        const char                *from_location,
                                                                        const char                *to_location,
                                                                        GList                     *selection,
                                                                        const char                *title);
void          nautilus_view_frame_report_selection_change              (NautilusViewFrame         *view,
                                                                        GList                     *selection);
void          nautilus_view_frame_report_status                        (NautilusViewFrame         *view,
                                                                        const char                *status);
void          nautilus_view_frame_report_load_underway                 (NautilusViewFrame         *view);
void          nautilus_view_frame_report_load_progress                 (NautilusViewFrame         *view,
                                                                        double                     fraction_done);
void          nautilus_view_frame_report_load_complete                 (NautilusViewFrame         *view);
void          nautilus_view_frame_report_load_failed                   (NautilusViewFrame         *view);
void          nautilus_view_frame_set_title                            (NautilusViewFrame         *view,
                                                                        const char                *title);
void          nautilus_view_frame_go_back                              (NautilusViewFrame         *view);
void          nautilus_view_frame_quit_nautilus                        (NautilusViewFrame         *view);
void          nautilus_view_frame_close_desktop                        (NautilusViewFrame         *view);

/* Zoomable */
void          nautilus_view_frame_zoom_level_changed                   (NautilusViewFrame         *view,
                                                                        double                     zoom_level);
void          nautilus_view_frame_zoom_parameters_changed              (NautilusViewFrame         *view,
                                                                        double                     zoom_level,
                                                                        double                     min_zoom_level,
                                                                        double                     max_zoom_level);

#endif /* NAUTILUS_VIEW_FRAME_PRIVATE_H */
