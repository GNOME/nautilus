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
#include <bonobo/bonobo-control.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW	      (nautilus_view_get_type ())
#define NAUTILUS_VIEW(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW, NautilusView))
#define NAUTILUS_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW, NautilusViewClass))
#define NAUTILUS_IS_VIEW(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW))
#define NAUTILUS_IS_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW))

typedef struct NautilusView NautilusView;
typedef struct NautilusViewClass NautilusViewClass;

struct NautilusViewClass
{
	BonoboObjectClass parent_spot;
	
	void (*save_state)              (NautilusView *view, const char *config_path);
	void (*load_state)              (NautilusView *view, const char *config_path);
	void (*notify_location_change)	(NautilusView *view,
					 Nautilus_NavigationInfo *nav_context);
	void (*stop_location_change)    (NautilusView *view);
	void (*notify_selection_change)	(NautilusView *view,
					 Nautilus_SelectionInfo *nav_context);
	void (*show_properties)         (NautilusView *view);
	
	BonoboObjectClass *parent_class;
	
	gpointer servant_init_func, servant_destroy_func, vepv;
};

typedef struct NautilusViewDetails NautilusViewDetails;

struct NautilusView {
	BonoboObject parent;
	NautilusViewDetails *details;
};

GtkType        nautilus_view_get_type                 (void);
NautilusView * nautilus_view_new                      (GtkWidget                      *widget);
NautilusView * nautilus_view_new_from_bonobo_control  (BonoboControl                  *bonobo_control);
void           nautilus_view_request_location_change  (NautilusView                   *view,
						       Nautilus_NavigationRequestInfo *loc);
void           nautilus_view_request_selection_change (NautilusView                   *view,
						       Nautilus_SelectionRequestInfo  *loc);
void           nautilus_view_request_status_change    (NautilusView                   *view,
						       Nautilus_StatusRequestInfo     *loc);
void           nautilus_view_request_progress_change  (NautilusView                   *view,
						       Nautilus_ProgressRequestInfo   *loc);
void           nautilus_view_request_title_change     (NautilusView                   *view,
						       const char                     *title);
BonoboControl *nautilus_view_get_bonobo_control       (NautilusView                   *view);
CORBA_Object   nautilus_view_get_main_window          (NautilusView                   *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
