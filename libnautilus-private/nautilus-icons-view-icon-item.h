/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* GNOME libraries - Icon Item class for Icon View
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_ICONS_VIEW_ICON_ITEM_H
#define NAUTILUS_ICONS_VIEW_ICON_ITEM_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_ICONS_VIEW_ICON_ITEM \
	(nautilus_icons_view_icon_item_get_type ())
#define NAUTILUS_ICONS_VIEW_ICON_ITEM(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ICONS_VIEW_ICON_ITEM, NautilusIconsViewIconItem))
#define NAUTILUS_ICONS_VIEW_ICON_ITEM_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICONS_VIEW_ICON_ITEM, NautilusIconsViewIconItemClass))
#define NAUTILUS_IS_ICONS_VIEW_ICON_ITEM(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ICONS_VIEW_ICON_ITEM))
#define NAUTILUS_IS_ICONS_VIEW_ICON_ITEM_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_ICONS_VIEW_ICON_ITEM))


typedef struct NautilusIconsViewIconItem NautilusIconsViewIconItem;
typedef struct NautilusIconsViewIconItemClass NautilusIconsViewIconItemClass;
typedef struct NautilusIconsViewIconItemDetails NautilusIconsViewIconItemDetails;

struct NautilusIconsViewIconItem {
	GnomeCanvasItem item;
	NautilusIconsViewIconItemDetails *details;
};

struct NautilusIconsViewIconItemClass {
	GnomeCanvasItemClass parent_class;
};

GtkType  nautilus_icons_view_icon_item_get_type                  (void);

void     nautilus_icons_view_icon_item_set_emblems               (NautilusIconsViewIconItem *item,
								  GList                     *emblem_pixbufs);

void     nautilus_icons_view_icon_item_get_icon_world_rectangle  (NautilusIconsViewIconItem *item,
								  ArtDRect                  *world_rectangle);
void     nautilus_icons_view_icon_item_get_icon_window_rectangle (NautilusIconsViewIconItem *item,
								  ArtIRect                  *window_rectangle);

void     nautilus_icons_view_icon_item_set_show_stretch_handles  (NautilusIconsViewIconItem *item,
								  gboolean                   show_stretch_handles);

gboolean nautilus_icons_view_icon_item_get_hit_stretch_handle    (NautilusIconsViewIconItem *item,
								  int                        canvas_x,
								  int                        canvas_y);

END_GNOME_DECLS

#endif
