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
/* ntl-meta-view-frame.h: Interface for object that represents the
   frame a nautilus meta view plugs into. */

#ifndef NTL_META_VIEW_FRAME_H
#define NTL_META_VIEW_FRAME_H

#include <libnautilus/ntl-view-frame.h>
#include <bonobo/bonobo-control.h>

#define NAUTILUS_TYPE_META_VIEW_FRAME			(nautilus_meta_view_frame_get_type ())
#define NAUTILUS_META_VIEW_FRAME(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW_FRAME, NautilusMetaViewFrame))
#define NAUTILUS_META_VIEW_FRAME_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW_FRAME, NautilusMetaViewFrameClass))
#define NAUTILUS_IS_META_VIEW_FRAME(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW_FRAME))
#define NAUTILUS_IS_META_VIEW_FRAME_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_META_VIEW_FRAME))

typedef struct NautilusMetaViewFrame NautilusMetaViewFrame;
typedef struct NautilusMetaViewFrameClass NautilusMetaViewFrameClass;
typedef struct NautilusMetaViewFrameDetails NautilusMetaViewFrameDetails;

struct NautilusMetaViewFrame {
	NautilusViewFrame base;
	NautilusMetaViewFrameDetails *details;
};

struct NautilusMetaViewFrameClass {
	NautilusViewFrameClass base;
};

GtkType                nautilus_meta_view_frame_get_type                (void);
NautilusMetaViewFrame *nautilus_meta_view_frame_new                     (GtkWidget             *widget);
NautilusMetaViewFrame *nautilus_meta_view_frame_new_from_bonobo_control (BonoboControl         *control);
void                   nautilus_meta_view_frame_set_label               (NautilusMetaViewFrame *view,
									 const char            *label);

#endif
