/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

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

/* nautilus-view-frame.h: Interface of the object representing the frame a
   data view plugs into. */

#ifndef NAUTILUS_VIEW_FRAME_H
#define NAUTILUS_VIEW_FRAME_H

#include <libnautilus/nautilus-view-component.h>
#include <bonobo/bonobo-object.h>
#include <gtk/gtkwidget.h>

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
	BonoboObjectClass parent_spot;

	void (*save_state)              (NautilusViewFrame *view, const char *config_path);
	void (*load_state)              (NautilusViewFrame *view, const char *config_path);
	void (*notify_location_change)	(NautilusViewFrame *view,
					 Nautilus_NavigationInfo *nav_context);
	void (*stop_location_change)    (NautilusViewFrame *view);
	void (*notify_selection_change)	(NautilusViewFrame *view,
					 Nautilus_SelectionInfo *nav_context);
	void (*show_properties)         (NautilusViewFrame *view);

	BonoboObjectClass *parent_class;

	gpointer servant_init_func, servant_destroy_func, vepv;
};

typedef struct _NautilusViewFramePrivate NautilusViewFramePrivate;

struct _NautilusViewFrame
{
	BonoboObject parent;
	NautilusViewFramePrivate *private;
};

GtkType            nautilus_view_frame_get_type                 (void);
NautilusViewFrame *nautilus_view_frame_new                      (GtkWidget *widget);
NautilusViewFrame *nautilus_view_frame_new_from_bonobo_control  (BonoboObject *bonobo_control);
void               nautilus_view_frame_request_location_change  (NautilusViewFrame              *view,
								 Nautilus_NavigationRequestInfo *loc);
void               nautilus_view_frame_request_selection_change (NautilusViewFrame              *view,
								 Nautilus_SelectionRequestInfo  *loc);
void               nautilus_view_frame_request_status_change    (NautilusViewFrame              *view,
								 Nautilus_StatusRequestInfo     *loc);
void               nautilus_view_frame_request_progress_change  (NautilusViewFrame              *view,
								 Nautilus_ProgressRequestInfo   *loc);
BonoboObject      *nautilus_view_frame_get_bonobo_control       (NautilusViewFrame              *view);
CORBA_Object	  nautilus_view_frame_get_main_window 		(NautilusViewFrame 		*view);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
