/*
 * gedit-overlay-child.h
 * This file is part of gedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GEDIT_OVERLAY_CHILD_H__
#define __GEDIT_OVERLAY_CHILD_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_OVERLAY_CHILD		(gedit_overlay_child_get_type ())
#define GEDIT_OVERLAY_CHILD(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OVERLAY_CHILD, GeditOverlayChild))
#define GEDIT_OVERLAY_CHILD_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OVERLAY_CHILD, GeditOverlayChild const))
#define GEDIT_OVERLAY_CHILD_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_OVERLAY_CHILD, GeditOverlayChildClass))
#define GEDIT_IS_OVERLAY_CHILD(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_OVERLAY_CHILD))
#define GEDIT_IS_OVERLAY_CHILD_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_OVERLAY_CHILD))
#define GEDIT_OVERLAY_CHILD_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_OVERLAY_CHILD, GeditOverlayChildClass))

typedef struct _GeditOverlayChild		GeditOverlayChild;
typedef struct _GeditOverlayChildClass		GeditOverlayChildClass;
typedef struct _GeditOverlayChildPrivate	GeditOverlayChildPrivate;

struct _GeditOverlayChild
{
	GtkBin parent;

	GeditOverlayChildPrivate *priv;
};

struct _GeditOverlayChildClass
{
	GtkBinClass parent_class;
};

typedef enum
{
	GEDIT_OVERLAY_CHILD_POSITION_NORTH_WEST = 1,
	GEDIT_OVERLAY_CHILD_POSITION_NORTH,
	GEDIT_OVERLAY_CHILD_POSITION_NORTH_EAST,
	GEDIT_OVERLAY_CHILD_POSITION_WEST,
	GEDIT_OVERLAY_CHILD_POSITION_CENTER,
	GEDIT_OVERLAY_CHILD_POSITION_EAST,
	GEDIT_OVERLAY_CHILD_POSITION_SOUTH_WEST,
	GEDIT_OVERLAY_CHILD_POSITION_SOUTH,
	GEDIT_OVERLAY_CHILD_POSITION_SOUTH_EAST,
	GEDIT_OVERLAY_CHILD_POSITION_STATIC
} GeditOverlayChildPosition;

GType                     gedit_overlay_child_get_type     (void) G_GNUC_CONST;

GeditOverlayChild        *gedit_overlay_child_new          (GtkWidget *widget);

GeditOverlayChildPosition gedit_overlay_child_get_position (GeditOverlayChild *child);

void                      gedit_overlay_child_set_position (GeditOverlayChild        *child,
                                                            GeditOverlayChildPosition position);

guint                     gedit_overlay_child_get_offset   (GeditOverlayChild *child);

void                      gedit_overlay_child_set_offset   (GeditOverlayChild *child,
                                                            guint              offset);

gboolean                  gedit_overlay_child_get_fixed    (GeditOverlayChild *child);

void                      gedit_overlay_child_set_fixed    (GeditOverlayChild *child,
                                                            gboolean           fixed);

G_END_DECLS

#endif /* __GEDIT_OVERLAY_CHILD_H__ */
