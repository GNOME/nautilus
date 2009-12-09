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

#ifndef NAUTILUS_VIEW_AS_ACTION_H
#define NAUTILUS_VIEW_AS_ACTION_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_VIEW_AS_ACTION            (nautilus_view_as_action_get_type ())
#define NAUTILUS_VIEW_AS_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_VIEW_AS_ACTION, NautilusViewAsAction))
#define NAUTILUS_VIEW_AS_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_AS_ACTION, NautilusViewAsActionClass))
#define NAUTILUS_IS_VIEW_AS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_VIEW_AS_ACTION))
#define NAUTILUS_IS_VIEW_AS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW_AS_ACTION))
#define NAUTILUS_VIEW_AS_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NAUTILUS_TYPE_VIEW_AS_ACTION, NautilusViewAsActionClass))

typedef struct _NautilusViewAsAction       NautilusViewAsAction;
typedef struct _NautilusViewAsActionClass  NautilusViewAsActionClass;
typedef struct NautilusViewAsActionPrivate NautilusViewAsActionPrivate;

struct _NautilusViewAsAction
{
	GtkAction parent;

	/*< private >*/
	NautilusViewAsActionPrivate *priv;
};

struct _NautilusViewAsActionClass
{
	GtkActionClass parent_class;
};

GType    nautilus_view_as_action_get_type   (void);

#endif
