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
/* nautilus-meta-view-frame.h: Interface for object that represents the
   frame a nautilus meta view plugs into. */

#ifndef NAUTILUS_META_VIEW_FRAME_H
#define NAUTILUS_META_VIEW_FRAME_H

#include <libnautilus/nautilus-view-frame.h>
#include <bonobo/bonobo-control.h>

#define NAUTILUS_TYPE_META_VIEW			(nautilus_meta_view_get_type ())
#define NAUTILUS_META_VIEW(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW, NautilusMetaView))
#define NAUTILUS_META_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW, NautilusMetaViewClass))
#define NAUTILUS_IS_META_VIEW(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW))
#define NAUTILUS_IS_META_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_META_VIEW))

typedef struct NautilusMetaView NautilusMetaView;
typedef struct NautilusMetaViewClass NautilusMetaViewClass;

struct NautilusMetaView {
	NautilusView base;
};

struct NautilusMetaViewClass {
	NautilusViewClass base;
};

GtkType           nautilus_meta_view_get_type                (void);
NautilusMetaView *nautilus_meta_view_new                     (GtkWidget     *widget);
NautilusMetaView *nautilus_meta_view_new_from_bonobo_control (BonoboControl *control);

#endif
