/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Andy Hertzfeld
 */

/* header file for the hardware view component */

#ifndef NAUTILUS_HARDWARE_VIEW_H
#define NAUTILUS_HARDWARE_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtkeventbox.h>


typedef struct _NautilusHardwareView      NautilusHardwareView;
typedef struct _NautilusHardwareViewClass NautilusHardwareViewClass;

#define NAUTILUS_TYPE_HARDWARE_VIEW		(nautilus_hardware_view_get_type ())
#define NAUTILUS_HARDWARE_VIEW(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_HARDWARE_VIEW, NautilusHardwareView))
#define NAUTILUS_HARDWARE_VIEW_CLASS(klass) 	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_HARDWARE_VIEW, NautilusHardwareViewClass))
#define NAUTILUS_IS_HARDWARE_VIEW(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_HARDWARE_VIEW))
#define NAUTILUS_IS_HARDWARE_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_HARDWARE_VIEW))

typedef struct _NautilusHardwareViewDetails NautilusHardwareViewDetails;

struct _NautilusHardwareView {
	GtkEventBox parent;
	NautilusHardwareViewDetails *details;
};

struct _NautilusHardwareViewClass {
	GtkEventBoxClass parent_class;
};

/* GtkObject support */
GtkType       nautilus_hardware_view_get_type          (void);

/* Component embedding support */
NautilusView *nautilus_hardware_view_get_nautilus_view (NautilusHardwareView *view);

/* URI handling */
void          nautilus_hardware_view_load_uri          (NautilusHardwareView *view,
							const char           *uri);

#endif /* NAUTILUS_HARDWARE_VIEW_H */
