/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
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
 *  Author: Maciej Stachowiak <mjs@eazel.com>
 *
 */

/* nautilus-zoomable.h: Object for implementing the Zoomable CORBA interface. */

#ifndef NAUTILUS_ZOOMABLE_H
#define NAUTILUS_ZOOMABLE_H

#include <libnautilus/nautilus-view-component.h>
#include <bonobo/bonobo-control.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_ZOOMABLE		  (nautilus_zoomable_get_type ())
#define NAUTILUS_ZOOMABLE(obj)		  (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ZOOMABLE, NautilusZoomable))
#define NAUTILUS_ZOOMABLE_CLASS(klass)	  (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ZOOMABLE, NautilusZoomableClass))
#define NAUTILUS_IS_ZOOMABLE(obj)	  (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ZOOMABLE))
#define NAUTILUS_IS_ZOOMABLE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ZOOMABLE))

typedef struct NautilusZoomable NautilusZoomable;
typedef struct NautilusZoomableClass NautilusZoomableClass;

struct NautilusZoomableClass
{
	BonoboObjectClass parent_spot;

	void (*set_zoom_level)	        (NautilusZoomable *view,
					 gdouble zoom_level);
	void (*zoom_in)	                (NautilusZoomable *view);
	void (*zoom_out)	        (NautilusZoomable *view);
	void (*zoom_to_level)	        (NautilusZoomable *view,
					 gint zoom_level);
	void (*zoom_default)	        (NautilusZoomable *view);
	void (*zoom_to_fit)	        (NautilusZoomable *view);

	gpointer servant_init_func, servant_destroy_func, vepv;
};

typedef struct NautilusZoomableDetails NautilusZoomableDetails;

struct NautilusZoomable
{
	BonoboObject parent;
	NautilusZoomableDetails *details;
};

GtkType            nautilus_zoomable_get_type                  (void);
NautilusZoomable  *nautilus_zoomable_new                       (GtkWidget       *widget, 
							        double           min_zoom_level,
							        double           max_zoom_level,
							       gboolean          is_continuous,
							       double		*preferred_zoom_levels,
							       int num_preferred_zoom_levels);
NautilusZoomable  *nautilus_zoomable_new_from_bonobo_control   (BonoboControl   *bonobo_control, 
							        double           min_zoom_level,
							        double           max_zoom_level,
							        gboolean         is_continuous,
							        double		*preferred_zoom_levels,
							        int		 num_preferred_zoom_levels);
void               nautilus_zoomable_set_parameters	       (NautilusZoomable *view,
							        double            zoom_level,
							        double            min_zoom_level,
							        double            max_zoom_level);
void               nautilus_zoomable_set_zoom_level            (NautilusZoomable *view,
							        double            zoom_level);
BonoboControl     *nautilus_zoomable_get_bonobo_control        (NautilusZoomable *view);

GList *nautilus_g_list_from_ZoomLevelList (const Nautilus_ZoomLevelList *zoom_level_list);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
