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
 * Author: Ramiro Estrugo
 */

/* nautilus-services-startup-view.h - services bootstrap startup view.
 */
#ifndef NAUTILUS_SERVICE_STARTUP_VIEW_H
#define NAUTILUS_SERVICE_STARTUP_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtk.h>

typedef struct _NautilusServiceStartupView NautilusServiceStartupView;
typedef struct _NautilusServiceStartupViewClass NautilusServiceStartupViewClass;

#define NAUTILUS_TYPE_SERVICE_STARTUP_VIEW		(nautilus_service_startup_view_get_type ())
#define NAUTILUS_SERVICE_STARTUP_VIEW(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW, NautilusServiceStartupView))
#define NAUTILUS_SERVICE_STARTUP_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW, NautilusServiceStartupViewClass))
#define NAUTILUS_IS_SERVICE_STARTUP_VIEW(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW))
#define NAUTILUS_IS_SERVICE_STARTUP_VIEW_CLASS (klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW))

typedef struct _NautilusServiceStartupViewDetails NautilusServiceStartupViewDetails;

struct _NautilusServiceStartupView {
	GtkEventBox				parent;
	NautilusServiceStartupViewDetails	*details;
};

struct _NautilusServiceStartupViewClass {
	GtkVBoxClass	parent_class;
};

/* GtkObject support */
GtkType       nautilus_service_startup_view_get_type          (void);

/* Component embedding support */
NautilusView *nautilus_service_startup_view_get_nautilus_view (NautilusServiceStartupView *view);

/* URI handling */
void          nautilus_service_startup_view_load_uri          (NautilusServiceStartupView	*view,
							       const char			*uri);

#endif /* NAUTILUS_SERVICE_STARTUP_VIEW_H */
