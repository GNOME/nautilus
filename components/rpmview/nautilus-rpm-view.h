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

/* header file for the rpm view component */

#ifndef NAUTILUS_RPM_VIEW_H
#define NAUTILUS_RPM_VIEW_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libnautilus/nautilus-view.h>
#include <gnome.h>

typedef struct NautilusRPMView      NautilusRPMView;
typedef struct NautilusRPMViewClass NautilusRPMViewClass;

#define NAUTILUS_TYPE_RPM_VIEW	          (nautilus_rpm_view_get_type ())
#define NAUTILUS_RPM_VIEW(obj)	          (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_RPM_VIEW, NautilusRPMView))
#define NAUTILUS_RPM_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_RPM_VIEW, NautilusRPMViewClass))
#define NAUTILUS_IS_RPM_VIEW(obj)	  (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_RPM_VIEW))
#define NAUTILUS_IS_RPM_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_RPM_VIEW))

typedef struct NautilusRPMViewDetails NautilusRPMViewDetails;

struct NautilusRPMView {
	GtkEventBox parent;
	NautilusRPMViewDetails *details;
};

struct NautilusRPMViewClass {
	GtkEventBoxClass parent_class;
};

/* GtkObject support */
GtkType       nautilus_rpm_view_get_type          (void);

/* Component embedding support */
NautilusView *nautilus_rpm_view_get_nautilus_view (NautilusRPMView *view);

/* URI handling */
char*	      nautilus_rpm_view_get_uri		  (NautilusRPMView *view);
gboolean      nautilus_rpm_view_load_uri          (NautilusRPMView *view,
						   const char      *uri);
gboolean      nautilus_rpm_view_get_installed     (NautilusRPMView *view);
NautilusView* nautilus_rpm_view_get_view          (NautilusRPMView *view);

#endif /* NAUTILUS_RPM_VIEW_H */
