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

/* nautilus-services-content-view.h - services content view
   component. This component just displays a simple label of the URI
   and does nothing else. It should be a good basis for writing
   out-of-proc content views.*/

#ifndef NAUTILUS_SERVICE_STARTUP_VIEW_H
#define NAUTILUS_SERVICE_STARTUP_VIEW_H

#include <libnautilus/ntl-content-view-frame.h>
#include <gtk/gtk.h>


typedef struct _NautilusServicesContentView      NautilusServicesContentView;
typedef struct _NautilusServicesContentViewClass NautilusServicesContentViewClass;

#define NAUTILUS_TYPE_SERVICE_STARTUP_VIEW	      (nautilus_service_startup_view_get_type ())
#define NAUTILUS_SERVICE_STARTUP_VIEW(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW, NautilusServicesContentView))
#define NAUTILUS_SERVICE_STARTUP_VIEW_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW, NautilusServicesContentViewClass))
#define NAUTILUS_IS_SERVICE_STARTUP_VIEW(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW))
#define NAUTILUS_IS_SERVICE_STARTUP_VIEW_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_SERVICE_STARTUP_VIEW))

typedef struct _NautilusServicesContentViewDetails NautilusServicesContentViewDetails;

struct _NautilusServicesContentView {
	GtkVBox parent;
	NautilusServicesContentViewDetails *details;
};

struct _NautilusServicesContentViewClass {
	GtkVBoxClass parent_class;
};



/* GtkObject support */
GtkType                   nautilus_service_startup_view_get_type                      (void);

/* Component embedding support */
NautilusContentViewFrame *nautilus_service_startup_view_get_view_frame                (NautilusServicesContentView *view);

/* URI handling */
void                      nautilus_service_startup_view_load_uri                      (NautilusServicesContentView *view,
										      const char                *uri);


#endif /* NAUTILUS_SERVICE_STARTUP_VIEW_H */
