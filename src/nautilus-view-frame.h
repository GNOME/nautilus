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

/* nautilus-view-frame.h: Interface of the object representing a data
 * view. This is actually the widget for the view frame rather than
 * the view frame itself.
 */

#ifndef NAUTILUS_VIEW_FRAME_H
#define NAUTILUS_VIEW_FRAME_H

#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-zoomable-frame.h>
#include <eel/eel-generous-bin.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus/nautilus-view-component.h>

#define NAUTILUS_TYPE_VIEW_FRAME            (nautilus_view_frame_get_type ())
#define NAUTILUS_VIEW_FRAME(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrame))
#define NAUTILUS_VIEW_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrameClass))
#define NAUTILUS_IS_VIEW_FRAME(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))
#define NAUTILUS_IS_VIEW_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VIEW_FRAME))

typedef struct NautilusViewFrameDetails NautilusViewFrameDetails;

typedef struct {
        EelGenerousBin parent;
        NautilusViewFrameDetails *details;
} NautilusViewFrame;

typedef struct {
        EelGenerousBinClass parent_spot;
        
        /* These roughly correspond to CORBA calls, but in some cases they are higher level. */

        /* This happens only just after load_view. */
        void               (* view_loaded)                          (NautilusViewFrame *view);

        /* These can happen pretty much any time. */
        void               (* load_underway)                        (NautilusViewFrame *view);
        void               (* failed)                               (NautilusViewFrame *view);

        /* These will only happen after load_underway (guaranteed). */
        void               (* open_location_in_this_window)         (NautilusViewFrame *view,
                                                                     const char        *location);
        void               (* open_location_prefer_existing_window) (NautilusViewFrame *view,
                                                                     const char        *location);
        void               (* open_location_force_new_window)       (NautilusViewFrame *view,
                                                                     const char        *location,
                                                                     GList             *selection); /* list of char * */
        void               (* report_location_change)               (NautilusViewFrame *view,
                                                                     const char        *location,
                                                                     GList             *selection, /* list of char * */
                                                                     const char        *title);
        void               (* report_redirect)                      (NautilusViewFrame *view,
                                                                     const char        *from_location,
                                                                     const char        *to_location,
                                                                     GList             *selection, /* list of char * */
                                                                     const char        *title);
        void               (* change_selection)                     (NautilusViewFrame *view,
                                                                     GList             *selection); /* list of char * */
        void               (* change_status)                        (NautilusViewFrame *view,
                                                                     const char        *status);
        void               (* load_progress_changed)                (NautilusViewFrame *view);
        void               (* load_complete)                        (NautilusViewFrame *view);
        void               (* title_changed)                        (NautilusViewFrame *view);
        void               (* zoom_level_changed)                   (NautilusViewFrame *view);
        void               (* zoom_parameters_changed)              (NautilusViewFrame *view);
	Nautilus_History * (* get_history_list)                     (NautilusViewFrame *view);
        void               (* go_back)                              (NautilusViewFrame *view);
} NautilusViewFrameClass;

/* basic view management */
GtkType            nautilus_view_frame_get_type                  (void);
NautilusViewFrame *nautilus_view_frame_new                       (BonoboUIContainer   *ui_container,
                                                                  NautilusUndoManager *undo_manager);
Bonobo_Control	   nautilus_view_frame_get_control		 (NautilusViewFrame   *view);

/* connecting to a Nautilus:View */
void               nautilus_view_frame_load_view                 (NautilusViewFrame   *view,
                                                                  const char          *view_iid);
void               nautilus_view_frame_stop                      (NautilusViewFrame   *view);

/* calls to Nautilus:View functions */
void               nautilus_view_frame_load_location             (NautilusViewFrame   *view,
                                                                  const char          *location);
void               nautilus_view_frame_selection_changed         (NautilusViewFrame   *view,
                                                                  GList               *selection);
void               nautilus_view_frame_title_changed             (NautilusViewFrame   *view,
                                                                  const char          *title);

/* calls to Bonobo:Zoomable functions */
double             nautilus_view_frame_get_zoom_level            (NautilusViewFrame   *view);
void               nautilus_view_frame_set_zoom_level            (NautilusViewFrame   *view,
                                                                  double               zoom_level);
double             nautilus_view_frame_get_min_zoom_level        (NautilusViewFrame   *view);
double             nautilus_view_frame_get_max_zoom_level        (NautilusViewFrame   *view);
gboolean           nautilus_view_frame_get_has_min_zoom_level    (NautilusViewFrame   *view);
gboolean           nautilus_view_frame_get_has_max_zoom_level    (NautilusViewFrame   *view);
gboolean           nautilus_view_frame_get_is_continuous         (NautilusViewFrame   *view);
GList *            nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame   *view);
void               nautilus_view_frame_zoom_in                   (NautilusViewFrame   *view);
void               nautilus_view_frame_zoom_out                  (NautilusViewFrame   *view);
void               nautilus_view_frame_zoom_to_fit               (NautilusViewFrame   *view);

/* Other. */
gboolean           nautilus_view_frame_get_is_view_loaded        (NautilusViewFrame   *view);
const char *       nautilus_view_frame_get_view_iid              (NautilusViewFrame   *view);
gboolean           nautilus_view_frame_get_is_zoomable           (NautilusViewFrame   *view);
char *             nautilus_view_frame_get_title                 (NautilusViewFrame   *view);
char *             nautilus_view_frame_get_label                 (NautilusViewFrame   *view);
void               nautilus_view_frame_set_label                 (NautilusViewFrame   *view,
                                                                  const char          *label);

#endif /* NAUTILUS_VIEW_FRAME_H */
