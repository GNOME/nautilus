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

#ifndef NAUTILUS_VIEW_H
#define NAUTILUS_VIEW_H

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
#define NAUTILUS_IS_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_VIEW))

typedef struct NautilusViewDetails NautilusViewDetails;

typedef struct {
	BonoboObject parent_spot;

	NautilusViewDetails *details;
} NautilusView;

typedef struct {
	BonoboObjectClass parent_spot;
	
	void (*load_location)     (NautilusView *view,
		                   const char   *location_uri,
		                   const char   *referring_location_uri);
	void (*stop_loading)      (NautilusView *view);
	void (*selection_changed) (NautilusView *view,
				   GList        *selection); /* list of URI char *s */
} NautilusViewClass;

GtkType           nautilus_view_get_type                    (void);
NautilusView *    nautilus_view_new                         (GtkWidget              *widget);
NautilusView *    nautilus_view_new_from_bonobo_control     (BonoboControl          *bonobo_control);
BonoboControl *   nautilus_view_get_bonobo_control          (NautilusView           *view);

/* Calls to the Nautilus shell via the view frame. See the IDL for detailed comments. */
void              nautilus_view_report_location_change      (NautilusView           *view,
							     const char             *location_uri);
void              nautilus_view_open_location               (NautilusView           *view,
							     const char             *location_uri);
void              nautilus_view_open_location_in_new_window (NautilusView           *view,
							     const char             *location_uri);
void              nautilus_view_report_selection_change     (NautilusView           *view,
							     GList                  *selection); /* list of URI char *s */
void              nautilus_view_report_status               (NautilusView           *view,
							     const char             *status);
void              nautilus_view_report_load_underway        (NautilusView           *view);
void              nautilus_view_report_load_progress        (NautilusView           *view,
							     double                  fraction_done);
void              nautilus_view_report_load_complete        (NautilusView           *view);
void              nautilus_view_report_load_failed          (NautilusView           *view);
void              nautilus_view_set_title                   (NautilusView           *view,
							     const char             *title);

/* Some utility functions useful for doing the CORBA work directly.
 * Not needed by most components, but shared with the view frame code,
 * which is why they are public.
 */
Nautilus_URIList *nautilus_uri_list_from_g_list             (GList                  *list);
GList *           nautilus_shallow_g_list_from_uri_list     (const Nautilus_URIList *uri_list);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_VIEW_H */
