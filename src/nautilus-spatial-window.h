/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2003 Ximian, Inc.
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
 */
/* nautilus-window.h: Interface of the main window object */

#ifndef NAUTILUS_SPATIAL_WINDOW_H
#define NAUTILUS_SPATIAL_WINDOW_H

#include "nautilus-window.h"
#include "nautilus-window-private.h"

#define NAUTILUS_TYPE_SPATIAL_WINDOW              (nautilus_spatial_window_get_type())
#define NAUTILUS_SPATIAL_WINDOW(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SPATIAL_WINDOW, NautilusSpatialWindow))
#define NAUTILUS_SPATIAL_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SPATIAL_WINDOW, NautilusSpatialWindowClass))
#define NAUTILUS_IS_SPATIAL_WINDOW(obj)	          (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SPATIAL_WINDOW))
#define NAUTILUS_IS_SPATIAL_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SPATIAL_WINDOW))

#ifndef NAUTILUS_SPATIAL_WINDOW_DEFINED
#define NAUTILUS_SPATIAL_WINDOW_DEFINED
typedef struct _NautilusSpatialWindow        NautilusSpatialWindow;
#endif
typedef struct _NautilusSpatialWindowClass   NautilusSpatialWindowClass;
typedef struct _NautilusSpatialWindowDetails NautilusSpatialWindowDetails;

struct _NautilusSpatialWindow {
        NautilusWindow parent_object;

        gboolean affect_spatial_window_on_next_location_change;
        
        NautilusSpatialWindowDetails *details;
};

struct _NautilusSpatialWindowClass {
        NautilusWindowClass parent_spot;
};


GType            nautilus_spatial_window_get_type			(void);
void             nautilus_spatial_window_save_geometry			(NautilusSpatialWindow *window);
void             nautilus_spatial_window_save_scroll_position		(NautilusSpatialWindow *window);
void             nautilus_spatial_window_save_show_hidden_files_mode	(NautilusSpatialWindow *window);
void             nautilus_spatial_window_set_location_button		(NautilusSpatialWindow *window,
									 GFile                 *location);

#endif
