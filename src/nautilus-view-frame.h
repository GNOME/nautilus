/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
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
#include <libnautilus-extensions/nautilus-generous-bin.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus/nautilus-view-component.h>

#define NAUTILUS_TYPE_VIEW_FRAME            (nautilus_view_frame_get_type ())
#define NAUTILUS_VIEW_FRAME(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrame))
#define NAUTILUS_VIEW_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrameClass))
#define NAUTILUS_IS_VIEW_FRAME(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))
#define NAUTILUS_IS_VIEW_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VIEW_FRAME))

typedef struct NautilusViewFrameDetails NautilusViewFrameDetails;

typedef struct {
        NautilusGenerousBin parent;
        NautilusViewFrameDetails *details;

        NautilusUndoManager *undo_manager;
     
        char *iid;

        /* The frame itself (from various interface points of view). */
        BonoboObject *view_frame;
        BonoboObject *zoomable_frame;
        BonoboObject *history_frame;
        
        /* The view inside the (various interfaces). */
        BonoboObjectClient *client_object;
        Nautilus_Zoomable zoomable;
        GtkWidget *client_widget;
} NautilusViewFrame;

typedef struct {
        NautilusGenerousBinClass parent_spot;
        
        /* Some of these calls correspond to CORBA calls, others are
         * higher level operations.
         */
        void (* open_location)	               (NautilusViewFrame *view,
                                                const char        *location);
        void (* open_location_in_new_window)   (NautilusViewFrame *view,
                                                const char        *location,
                                                GList             *selection); /* list of char * */
        void (* report_selection_change)       (NautilusViewFrame *view,
                                                GList             *selection); /* list of char * */
        void (* report_status)                 (NautilusViewFrame *view,
                                                const char        *status);
        void (* report_load_underway)          (NautilusViewFrame *view);
        void (* report_load_progress)          (NautilusViewFrame *view,
                                                double             fraction_done);
        void (* report_load_complete)          (NautilusViewFrame *view);
        void (* report_load_failed)            (NautilusViewFrame *view);
        void (* title_changed)                 (NautilusViewFrame *view);
        void (* zoom_level_changed)            (NautilusViewFrame *view,
                                                double             zoom_level);

        void (* report_activation_complete)    (NautilusViewFrame *view,
                                                BonoboObjectClient *object);

        /* Error handling for when client goes away. */
        void (* client_gone)                   (NautilusViewFrame *view);

	/* Get a CORBA copy of the history list */
	Nautilus_HistoryList *
             (* get_history_list)	       (NautilusViewFrame *view);
} NautilusViewFrameClass;

typedef void (*NautilusActivationCallback) (NautilusViewFrame *view_frame, gpointer data);

void                  nautilus_view_frame_load_client_async (NautilusViewFrame *view, 
                                                             const char *iid);

void                  nautilus_view_frame_stop_activation           (NautilusViewFrame *view);



GtkType               nautilus_view_frame_get_type                  (void);
NautilusViewFrame *   nautilus_view_frame_new                       (BonoboUIContainer   *ui_container,
                                                                     NautilusUndoManager *undo_manager);
gboolean              nautilus_view_frame_load_client               (NautilusViewFrame   *view,
                                                                     const char          *iid);
const char *          nautilus_view_frame_get_iid                   (NautilusViewFrame   *view);

/* Nautilus:View */
void                  nautilus_view_frame_load_location             (NautilusViewFrame   *view,
                                                                     const char          *location);
void                  nautilus_view_frame_stop_loading              (NautilusViewFrame   *view);
void                  nautilus_view_frame_selection_changed         (NautilusViewFrame   *view,
                                                                     GList               *selection);

/* Nautilus:Zoomable */
gboolean              nautilus_view_frame_is_zoomable               (NautilusViewFrame   *view);
gdouble               nautilus_view_frame_get_zoom_level            (NautilusViewFrame   *view);
void                  nautilus_view_frame_set_zoom_level            (NautilusViewFrame   *view,
                                                                     double               zoom_level);
gdouble               nautilus_view_frame_get_min_zoom_level        (NautilusViewFrame   *view);
gdouble               nautilus_view_frame_get_max_zoom_level        (NautilusViewFrame   *view);
gboolean              nautilus_view_frame_get_is_continuous         (NautilusViewFrame   *view);
GList *               nautilus_view_frame_get_preferred_zoom_levels (NautilusViewFrame   *view);
void                  nautilus_view_frame_zoom_in                   (NautilusViewFrame   *view);
void                  nautilus_view_frame_zoom_out                  (NautilusViewFrame   *view);
void                  nautilus_view_frame_zoom_to_fit               (NautilusViewFrame   *view);

/* Other. */
char *                nautilus_view_frame_get_label                 (NautilusViewFrame   *view);
void                  nautilus_view_frame_set_label                 (NautilusViewFrame   *view,
                                                                     const char          *label);
void                  nautilus_view_frame_activate                  (NautilusViewFrame   *view);
Nautilus_HistoryList *nautilus_view_frame_get_history_list          (NautilusViewFrame   *view);
void                  nautilus_view_frame_title_changed             (NautilusViewFrame   *view,
                                                                     const char          *title);
/* view state */
char *                nautilus_view_frame_get_title                 (NautilusViewFrame   *view);
gboolean              nautilus_view_frame_get_is_underway           (NautilusViewFrame   *view);

#endif /* NAUTILUS_VIEW_FRAME_H */
