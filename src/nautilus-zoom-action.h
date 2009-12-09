/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2009 Red Hat, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NAUTILUS_ZOOM_ACTION_H
#define NAUTILUS_ZOOM_ACTION_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_ZOOM_ACTION            (nautilus_zoom_action_get_type ())
#define NAUTILUS_ZOOM_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ZOOM_ACTION, NautilusZoomAction))
#define NAUTILUS_ZOOM_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ZOOM_ACTION, NautilusZoomActionClass))
#define NAUTILUS_IS_ZOOM_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ZOOM_ACTION))
#define NAUTILUS_IS_ZOOM_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_ZOOM_ACTION))
#define NAUTILUS_ZOOM_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NAUTILUS_TYPE_ZOOM_ACTION, NautilusZoomActionClass))

typedef struct _NautilusZoomAction       NautilusZoomAction;
typedef struct _NautilusZoomActionClass  NautilusZoomActionClass;
typedef struct NautilusZoomActionPrivate NautilusZoomActionPrivate;

struct _NautilusZoomAction
{
	GtkAction parent;

	/*< private >*/
	NautilusZoomActionPrivate *priv;
};

struct _NautilusZoomActionClass
{
	GtkActionClass parent_class;
};

GType    nautilus_zoom_action_get_type   (void);

#endif
