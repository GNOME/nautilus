/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

/* nautilus-content-view-frame.h: Interface for object that represents a
   the frame a nautilus content view plugs into. */

#ifndef NAUTILUS_CONTENT_VIEW_H
#define NAUTILUS_CONTENT_VIEW_H

#include <libnautilus/nautilus-view-frame.h>

#define NAUTILUS_TYPE_CONTENT_VIEW	    (nautilus_content_view_get_type ())
#define NAUTILUS_CONTENT_VIEW(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_VIEW, NautilusContentView))
#define NAUTILUS_CONTENT_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_VIEW, NautilusContentViewClass))
#define NAUTILUS_IS_CONTENT_VIEW(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW))
#define NAUTILUS_IS_CONTENT_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW))

typedef struct NautilusContentView NautilusContentView;
typedef struct NautilusContentViewClass NautilusContentViewClass;

struct NautilusContentViewClass {
	NautilusViewClass parent_spot;
};

struct NautilusContentView {
	NautilusView parent;
};

GtkType              nautilus_content_view_get_type                (void);
NautilusContentView *nautilus_content_view_new                     (GtkWidget           *widget);
NautilusContentView *nautilus_content_view_new_from_bonobo_control (BonoboControl       *bonobo_control);
void                 nautilus_content_view_request_title_change    (NautilusContentView *view,
								    const char          *new_title);

#endif
