/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
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
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/* nautilus-mozilla-content-view.h - Mozilla content view component. */

#ifndef NAUTILUS_MOZILLA_CONTENT_VIEW_H
#define NAUTILUS_MOZILLA_CONTENT_VIEW_H

#include <libnautilus/nautilus-view.h>
#include <gtk/gtkvbox.h>

typedef struct NautilusMozillaContentView      NautilusMozillaContentView;
typedef struct NautilusMozillaContentViewClass NautilusMozillaContentViewClass;

#define NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW	      (nautilus_mozilla_content_view_get_type ())
#define NAUTILUS_MOZILLA_CONTENT_VIEW(obj)	      (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW, NautilusMozillaContentView))
#define NAUTILUS_MOZILLA_CONTENT_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW, NautilusMozillaContentViewClass))
#define NAUTILUS_IS_MOZILLA_CONTENT_VIEW(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW))
#define NAUTILUS_IS_MOZILLA_CONTENT_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW))

typedef struct NautilusMozillaContentViewDetails NautilusMozillaContentViewDetails;

struct NautilusMozillaContentView {
	GtkVBox					parent;
	NautilusMozillaContentViewDetails	*details;
};

struct NautilusMozillaContentViewClass {
	GtkVBoxClass parent_class;
};

/* GtkObject support */
GtkType       nautilus_mozilla_content_view_get_type          (void);


/* Component embedding support */
BonoboObject *nautilus_mozilla_content_view_new (void);

#endif /* NAUTILUS_MOZILLA_CONTENT_VIEW_H */
