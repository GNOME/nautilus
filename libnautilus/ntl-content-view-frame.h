/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

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

/* ntl-content-view-frame.h: Interface for object that represents a
   the frame a nautilus content view plugs into. */

#ifndef NTL_CONTENT_VIEW_FRAME_H
#define NTL_CONTENT_VIEW_FRAME_H

#include <libnautilus/ntl-view-frame.h>

#define NAUTILUS_TYPE_CONTENT_VIEW_FRAME			(nautilus_content_view_frame_get_type ())
#define NAUTILUS_CONTENT_VIEW_FRAME(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_VIEW_FRAME, NautilusContentViewFrame))
#define NAUTILUS_CONTENT_VIEW_FRAME_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_VIEW_FRAME, NautilusContentViewFrameClass))
#define NAUTILUS_IS_CONTENT_VIEW_FRAME(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW_FRAME))
#define NAUTILUS_IS_CONTENT_VIEW_FRAME_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW_FRAME))

typedef struct _NautilusContentViewFrame NautilusContentViewFrame;
typedef struct _NautilusContentViewFrameClass NautilusContentViewFrameClass;

struct _NautilusContentViewFrameClass {
  NautilusViewFrameClass parent_spot;
};

struct _NautilusContentViewFrame {
  NautilusViewFrame parent;
};

GtkType                   nautilus_content_view_frame_get_type                 (void);
NautilusContentViewFrame *nautilus_content_view_frame_new                      (GtkWidget *widget);
NautilusContentViewFrame *nautilus_content_view_frame_new_from_bonobo_control  (BonoboObject *bonobo_control);

#endif
