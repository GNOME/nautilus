/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the zoom control on the location bar
 *
 */

#ifndef NAUTILUS_ZOOM_CONTROL_H
#define NAUTILUS_ZOOM_CONTROL_H

#include <gtk/gtkhbox.h>
#include <libnautilus-private/nautilus-icon-info.h> /* For NautilusZoomLevel */

#define NAUTILUS_TYPE_ZOOM_CONTROL	      (nautilus_zoom_control_get_type ())
#define NAUTILUS_ZOOM_CONTROL(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ZOOM_CONTROL, NautilusZoomControl))
#define NAUTILUS_ZOOM_CONTROL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ZOOM_CONTROL, NautilusZoomControlClass))
#define NAUTILUS_IS_ZOOM_CONTROL(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ZOOM_CONTROL))
#define NAUTILUS_IS_ZOOM_CONTROL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ZOOM_CONTROL))

typedef struct NautilusZoomControl NautilusZoomControl;
typedef struct NautilusZoomControlClass NautilusZoomControlClass;
typedef struct NautilusZoomControlDetails NautilusZoomControlDetails;

struct NautilusZoomControl {
	GtkHBox parent;
	NautilusZoomControlDetails *details;
};

struct NautilusZoomControlClass {
	GtkHBoxClass parent_class;
	
	void (*zoom_in)		(NautilusZoomControl *control);
	void (*zoom_out) 	(NautilusZoomControl *control);
	void (*zoom_to_level) 	(NautilusZoomControl *control,
				 NautilusZoomLevel zoom_level);
	void (*zoom_to_default)	(NautilusZoomControl *control);

	/* Action signal for keybindings, do not connect to this */
	void (*change_value)    (NautilusZoomControl *control,
				 GtkScrollType scroll);
};

GType             nautilus_zoom_control_get_type           (void);
GtkWidget *       nautilus_zoom_control_new                (void);
void              nautilus_zoom_control_set_zoom_level     (NautilusZoomControl *zoom_control,
							    NautilusZoomLevel    zoom_level);
void              nautilus_zoom_control_set_parameters     (NautilusZoomControl *zoom_control,
							    NautilusZoomLevel    min_zoom_level,
							    NautilusZoomLevel    max_zoom_level,
							    gboolean             has_min_zoom_level,
							    gboolean             has_max_zoom_level,
							    GList               *zoom_levels);
NautilusZoomLevel nautilus_zoom_control_get_zoom_level     (NautilusZoomControl *zoom_control);
NautilusZoomLevel nautilus_zoom_control_get_min_zoom_level (NautilusZoomControl *zoom_control);
NautilusZoomLevel nautilus_zoom_control_get_max_zoom_level (NautilusZoomControl *zoom_control);
gboolean          nautilus_zoom_control_has_min_zoom_level (NautilusZoomControl *zoom_control);
gboolean          nautilus_zoom_control_has_max_zoom_level (NautilusZoomControl *zoom_control);
gboolean          nautilus_zoom_control_can_zoom_in        (NautilusZoomControl *zoom_control);
gboolean          nautilus_zoom_control_can_zoom_out       (NautilusZoomControl *zoom_control);

#endif /* NAUTILUS_ZOOM_CONTROL_H */
