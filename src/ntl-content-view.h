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
/* ntl-content-view.h: Interface of the object representing a content view. */

#ifndef NTL_CONTENT_VIEW_H
#define NTL_CONTENT_VIEW_H

#include "ntl-view.h"

#define NAUTILUS_TYPE_CONTENT_VIEW_FRAME            (nautilus_content_view_frame_get_type())
#define NAUTILUS_CONTENT_VIEW_FRAME(obj)	    (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_VIEW_FRAME, NautilusContentViewFrame))
#define NAUTILUS_CONTENT_VIEW_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_VIEW_FRAME, NautilusContentViewFrameClass))
#define NAUTILUS_IS_CONTENT_VIEW_FRAME(obj)	    (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW_FRAME))
#define NAUTILUS_IS_CONTENT_VIEW_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CONTENT_VIEW_FRAME))

typedef struct NautilusContentViewFrame NautilusContentViewFrame;
typedef struct NautilusContentViewFrameClass NautilusContentViewFrameClass;

struct NautilusContentViewFrameClass
{
	NautilusViewFrameClass parent_spot;
	
	/* These signals correspond to the Nautilus:ContentViewFrame CORBA interface.
	 * They are requests that the underlying view may make of the framework.
	 */
	void (*request_title_change)		(NautilusContentViewFrame *view,
						 const char *new_title);
	
	NautilusViewFrameClass *parent_class;
};

struct NautilusContentViewFrame {
	NautilusViewFrame parent_object;
};

GtkType nautilus_content_view_frame_get_type   (void);
void    nautilus_content_view_frame_set_active (NautilusContentViewFrame *view); 

#endif /* NTL_CONTENT_VIEW_H */
