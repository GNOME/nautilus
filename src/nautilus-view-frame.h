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

/* nautilus-view-frame.h: Interface of the object representing a data
   view. */

#ifndef NAUTILUS_VIEW_FRAME_H
#define NAUTILUS_VIEW_FRAME_H

#include <gtk/gtkwidget.h>
#include <gtk/gtkbin.h>
#include <bonobo.h>
#include <libnautilus/nautilus-view-component.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW_FRAME            (nautilus_view_frame_get_type ())
#define NAUTILUS_VIEW_FRAME(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrame))
#define NAUTILUS_VIEW_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrameClass))
#define NAUTILUS_IS_VIEW_FRAME(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))
#define NAUTILUS_IS_VIEW_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))
        
typedef struct NautilusViewFrame NautilusViewFrame;
typedef struct NautilusViewFrameClass NautilusViewFrameClass;

struct NautilusViewFrameClass {
        GtkBinClass parent_spot;
        
        /* These signals correspond to the Nautilus::ViewFrame CORBA interface. They
         * are requests that the underlying view may make of the shell via the frame.
         */
        void (*request_location_change)	 (NautilusViewFrame *view,
					  Nautilus_NavigationRequestInfo *navinfo);
        void (*request_selection_change) (NautilusViewFrame *view,
                                          Nautilus_SelectionRequestInfo *selinfo);
        void (*request_status_change)    (NautilusViewFrame *view,
                                          Nautilus_StatusRequestInfo *loc);
        void (*request_progress_change)  (NautilusViewFrame *view,
                                          Nautilus_ProgressRequestInfo *loc);
        void (*request_title_change)     (NautilusViewFrame *view,
                                          const char *title);

        /* Part of Nautilus::ZoomableFrame CORBA interface. */
        void (*notify_zoom_level)        (NautilusViewFrame *view,
                                          gdouble       zoom_level);
        
        /* Not a signal. Work-around for Gtk+'s lack of a 'constructed' operation */
        void (*view_constructed)         (NautilusViewFrame *view);
        
        GtkBinClass *parent_class;
        guint num_construct_args;
        
        gpointer servant_init_func, servant_destroy_func, vepv;
        gpointer zoomable_servant_init_func, zoomable_servant_destroy_func, zoomable_vepv;
};

typedef struct NautilusViewComponentType NautilusViewComponentType;

struct NautilusViewFrame {
        GtkBin parent;
        
        GtkWidget *main_window;
        
        char *iid;
        
        BonoboObjectClient *client_object;
        GtkWidget *client_widget;
        
        BonoboObject *view_frame;
        BonoboObject *zoomable_frame;
        
        Nautilus_Zoomable zoomable;
        NautilusViewComponentType *component_class;
        gpointer component_data;
        
        guint construct_arg_count;
        
        guint timer_id;
        guint checking;

        char *label;
};

GtkType       nautilus_view_frame_get_type                (void);
gboolean      nautilus_view_frame_load_client             (NautilusViewFrame       *view,
                                                           const char              *iid);
const char *  nautilus_view_frame_get_iid                 (NautilusViewFrame       *view);
CORBA_Object  nautilus_view_frame_get_client_objref       (NautilusViewFrame       *view);
BonoboObject *nautilus_view_frame_get_control_frame       (NautilusViewFrame       *view);
CORBA_Object  nautilus_view_frame_get_objref              (NautilusViewFrame       *view);

/* These functions correspond to methods of the Nautilus:View CORBAinterface. */
void          nautilus_view_frame_notify_location_change  (NautilusViewFrame       *view,
                                                           Nautilus_NavigationInfo *nav_context);
void          nautilus_view_frame_notify_selection_change (NautilusViewFrame       *view,
                                                           Nautilus_SelectionInfo  *sel_context);
void          nautilus_view_frame_load_state              (NautilusViewFrame       *view,
                                                           const char              *config_path);
void          nautilus_view_frame_save_state              (NautilusViewFrame       *view,
                                                           const char              *config_path);
void          nautilus_view_frame_show_properties         (NautilusViewFrame       *view);
void          nautilus_view_frame_stop_location_change    (NautilusViewFrame       *view);
void          nautilus_view_frame_set_active_errors       (NautilusViewFrame       *view,
                                                           gboolean                 enabled);
gboolean      nautilus_view_frame_is_zoomable             (NautilusViewFrame       *view);
gdouble       nautilus_view_frame_get_zoom_level          (NautilusViewFrame       *view);
void          nautilus_view_frame_set_zoom_level          (NautilusViewFrame       *view,
                                                           gdouble                  zoom_level);
gdouble       nautilus_view_frame_get_min_zoom_level      (NautilusViewFrame       *view);
gdouble       nautilus_view_frame_get_max_zoom_level      (NautilusViewFrame       *view);
gboolean      nautilus_view_frame_get_is_continuous       (NautilusViewFrame       *view);
void          nautilus_view_frame_zoom_in                 (NautilusViewFrame       *view);
void          nautilus_view_frame_zoom_out                (NautilusViewFrame       *view);
void          nautilus_view_frame_zoom_to_fit             (NautilusViewFrame       *view);
char *        nautilus_view_frame_get_label               (NautilusViewFrame       *view);
void          nautilus_view_frame_set_label               (NautilusViewFrame       *view,
                                                           const char              *label);
void          nautilus_view_frame_activate                (NautilusViewFrame       *view);

/* This is a "protected" operation */
void          nautilus_view_frame_construct_arg_set       (NautilusViewFrame       *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_VIEW_FRAME_H */
