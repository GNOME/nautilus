/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* ntl-meta-view.h: Interface of the object representing a meta/navigation view. Derived from NautilusView. */

#ifndef NAUTILUS_META_VIEW_H
#define NAUTILUS_META_VIEW_H

#include "ntl-view.h"

#define NAUTILUS_TYPE_META_VIEW_FRAME            (nautilus_meta_view_frame_get_type())
#define NAUTILUS_META_VIEW_FRAME(obj)	         (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW_FRAME, NautilusMetaViewFrame))
#define NAUTILUS_META_VIEW_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW_FRAME, NautilusMetaViewFrameClass))
#define NAUTILUS_IS_META_VIEW_FRAME(obj)	 (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW_FRAME))
#define NAUTILUS_IS_META_VIEW_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_META_VIEW_FRAME))

typedef struct NautilusMetaViewFrame NautilusMetaViewFrame;

typedef struct {
	NautilusViewFrameClass parent_spot;
	
	NautilusViewFrameClass *parent_class;
} NautilusMetaViewFrameClass;

struct NautilusMetaViewFrame {
	NautilusViewFrame parent_object;
	char *label;
};

GtkType                nautilus_meta_view_frame_get_type  (void);
NautilusMetaViewFrame *nautilus_meta_view_frame_new       (void);
const char *           nautilus_meta_view_frame_get_label (NautilusMetaViewFrame *view);
void                   nautilus_meta_view_frame_set_label (NautilusMetaViewFrame *view,
							   const char            *label);

#endif
