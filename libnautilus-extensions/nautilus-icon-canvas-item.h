/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Icon canvas item class for icon container.
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

#ifndef NAUTILUS_ICON_CANVAS_ITEM_H
#define NAUTILUS_ICON_CANVAS_ITEM_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "nautilus-icon-factory.h"
#include <eel/eel-scalable-font.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_ICON_CANVAS_ITEM \
	(nautilus_icon_canvas_item_get_type ())
#define NAUTILUS_ICON_CANVAS_ITEM(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ICON_CANVAS_ITEM, NautilusIconCanvasItem))
#define NAUTILUS_ICON_CANVAS_ITEM_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_CANVAS_ITEM, NautilusIconCanvasItemClass))
#define NAUTILUS_IS_ICON_CANVAS_ITEM(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ICON_CANVAS_ITEM))
#define NAUTILUS_IS_ICON_CANVAS_ITEM_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_ICON_CANVAS_ITEM))

typedef struct NautilusIconCanvasItem NautilusIconCanvasItem;
typedef struct NautilusIconCanvasItemClass NautilusIconCanvasItemClass;
typedef struct NautilusIconCanvasItemDetails NautilusIconCanvasItemDetails;

struct NautilusIconCanvasItem {
	GnomeCanvasItem item;
	NautilusIconCanvasItemDetails *details;
	gpointer user_data;
};

struct NautilusIconCanvasItemClass {
	GnomeCanvasItemClass parent_class;

	void (* bounds_changed) (NautilusIconCanvasItem *item,
				 const ArtDRect         *old_world_bounds);
};

/* GtkObject */
GtkType     nautilus_icon_canvas_item_get_type                 (void);

/* attributes */
void        nautilus_icon_canvas_item_set_image                (NautilusIconCanvasItem       *item,
								GdkPixbuf                    *image);
GdkPixbuf * nautilus_icon_canvas_item_get_image                (NautilusIconCanvasItem       *item);
void        nautilus_icon_canvas_item_set_emblems              (NautilusIconCanvasItem       *item,
								GList                        *emblem_pixbufs);
void        nautilus_icon_canvas_item_set_show_stretch_handles (NautilusIconCanvasItem       *item,
								gboolean                      show_stretch_handles);
void        nautilus_icon_canvas_item_set_attach_points        (NautilusIconCanvasItem       *item,
								NautilusEmblemAttachPoints   *attach_points);
double      nautilus_icon_canvas_item_get_max_text_width       (NautilusIconCanvasItem       *item);
const char *nautilus_icon_canvas_item_get_editable_text        (NautilusIconCanvasItem       *icon_item);
void        nautilus_icon_canvas_item_set_renaming             (NautilusIconCanvasItem       *icon_item,
								gboolean                      state);


/* geometry and hit testing */
gboolean    nautilus_icon_canvas_item_hit_test_rectangle       (NautilusIconCanvasItem       *item,
								ArtIRect                      canvas_rect);
gboolean    nautilus_icon_canvas_item_hit_test_stretch_handles (NautilusIconCanvasItem       *item,
								ArtPoint                      world_point);
void        nautilus_icon_canvas_item_invalidate_label_size    (NautilusIconCanvasItem       *item);
ArtDRect    nautilus_icon_canvas_item_get_icon_rectangle       (const NautilusIconCanvasItem *item);
void        nautilus_icon_canvas_item_update_bounds            (NautilusIconCanvasItem       *item);
void        nautilus_icon_canvas_item_set_smooth_font          (NautilusIconCanvasItem       *item,
								EelScalableFont              *font);
void        nautilus_icon_canvas_item_set_smooth_font_size     (NautilusIconCanvasItem       *item,
								int                           font_size);

END_GNOME_DECLS

#endif /* NAUTILUS_ICON_CANVAS_ITEM_H */
