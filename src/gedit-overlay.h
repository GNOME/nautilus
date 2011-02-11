/*
 * gedit-overlay.h
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

#ifndef __GEDIT_OVERLAY_H__
#define __GEDIT_OVERLAY_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "gedit-overlay-child.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_OVERLAY		(gedit_overlay_get_type ())
#define GEDIT_OVERLAY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OVERLAY, GeditOverlay))
#define GEDIT_OVERLAY_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OVERLAY, GeditOverlay const))
#define GEDIT_OVERLAY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_OVERLAY, GeditOverlayClass))
#define GEDIT_IS_OVERLAY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_OVERLAY))
#define GEDIT_IS_OVERLAY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_OVERLAY))
#define GEDIT_OVERLAY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_OVERLAY, GeditOverlayClass))

typedef struct _GeditOverlay		GeditOverlay;
typedef struct _GeditOverlayClass	GeditOverlayClass;
typedef struct _GeditOverlayPrivate	GeditOverlayPrivate;

struct _GeditOverlay
{
	GtkContainer parent;

	GeditOverlayPrivate *priv;
};

struct _GeditOverlayClass
{
	GtkContainerClass parent_class;

	void (* set_scroll_adjustments)	  (GeditOverlay	 *overlay,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);
};

GType		 gedit_overlay_get_type			(void) G_GNUC_CONST;

GtkWidget	*gedit_overlay_new			(GtkWidget *main_widget);

void		 gedit_overlay_add			(GeditOverlay             *overlay,
							 GtkWidget                *widget,
							 GeditOverlayChildPosition position,
							 guint                     offset);

G_END_DECLS

#endif /* __GEDIT_OVERLAY_H__ */
