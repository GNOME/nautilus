/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* ntl-view-frame.h: Interface of the object representing the frame a
   data view plugs into. */

#ifndef NTL_VIEW_FRAME_H
#define NTL_VIEW_FRAME_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW_FRAME			(nautilus_view_frame_get_type ())
#define NAUTILUS_VIEW_FRAME(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrame))
#define NAUTILUS_VIEW_FRAME_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_FRAME, NautilusViewFrameClass))
#define NAUTILUS_IS_VIEW_FRAME(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))
#define NAUTILUS_IS_VIEW_FRAME_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW_FRAME))

typedef struct _NautilusViewFrame       NautilusViewFrame;
typedef struct _NautilusViewFrameClass  NautilusViewFrameClass;

struct _NautilusViewFrameClass
{
  GtkBinClass parent_spot;

  void (*notify_location_change)	(NautilusViewFrame *view,
					 Nautilus_NavigationInfo *nav_context);
  void (*notify_selection_change)	(NautilusViewFrame *view,
					 Nautilus_SelectionInfo *nav_context);
  void (*load_state)                    (NautilusViewFrame *view, const char *config_path);
  void (*save_state)                    (NautilusViewFrame *view, const char *config_path);
  void (*show_properties)               (NautilusViewFrame *view);
  void (*stop_location_change)          (NautilusViewFrame *view);

  GtkBinClass *parent_class;

  gpointer servant_init_func, servant_destroy_func, vepv;

  guint view_frame_signals[6];
};

struct _NautilusViewFrame
{
  GtkBin parent;

  GtkWidget *main_window;
 
  GnomeObject *control, *view_server;
  Nautilus_ViewFrame view_frame;
};

GtkType nautilus_view_frame_get_type                (void);
void    nautilus_view_frame_request_location_change (NautilusViewFrame         *view,
						      Nautilus_NavigationRequestInfo *loc);
void    nautilus_view_frame_request_selection_change (NautilusViewFrame        *view,
						       Nautilus_SelectionRequestInfo *loc);
void    nautilus_view_frame_request_status_change    (NautilusViewFrame        *view,
						       Nautilus_StatusRequestInfo *loc);
void    nautilus_view_frame_request_progress_change  (NautilusViewFrame        *view,
						       Nautilus_ProgressRequestInfo *loc);
GnomeObject *nautilus_view_frame_get_gnome_object    (NautilusViewFrame        *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
