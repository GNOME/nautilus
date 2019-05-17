
/* Nautilus - Canvas item class for canvas container.
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <eel/eel-canvas.h>
#include <eel/eel-art-extensions.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_CANVAS_ITEM nautilus_canvas_item_get_type()
#define NAUTILUS_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CANVAS_ITEM, NautilusCanvasItem))
#define NAUTILUS_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CANVAS_ITEM, NautilusCanvasItemClass))
#define NAUTILUS_IS_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CANVAS_ITEM))
#define NAUTILUS_IS_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CANVAS_ITEM))
#define NAUTILUS_CANVAS_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CANVAS_ITEM, NautilusCanvasItemClass))

typedef struct NautilusCanvasItem NautilusCanvasItem;
typedef struct NautilusCanvasItemClass NautilusCanvasItemClass;
typedef struct NautilusCanvasItemPrivate NautilusCanvasItemPrivate;

struct NautilusCanvasItem {
	EelCanvasItem item;
	NautilusCanvasItemPrivate *priv;
	gpointer user_data;
};

struct NautilusCanvasItemClass {
	EelCanvasItemClass parent_class;
};

/* not namespaced due to their length */
typedef enum {
	BOUNDS_USAGE_FOR_LAYOUT,
	BOUNDS_USAGE_FOR_ENTIRE_ITEM,
	BOUNDS_USAGE_FOR_DISPLAY
} NautilusCanvasItemBoundsUsage;

/* GObject */
GType       nautilus_canvas_item_get_type                 (void);

/* attributes */
void        nautilus_canvas_item_set_image                (NautilusCanvasItem       *item,
							   GdkPixbuf                *image);
cairo_surface_t* nautilus_canvas_item_get_drag_surface    (NautilusCanvasItem       *item);
void        nautilus_canvas_item_set_emblems              (NautilusCanvasItem       *item,
							   GList                    *emblem_pixbufs);

/* geometry and hit testing */
gboolean    nautilus_canvas_item_hit_test_rectangle       (NautilusCanvasItem       *item,
							   EelIRect                  canvas_rect);
void        nautilus_canvas_item_invalidate_label         (NautilusCanvasItem       *item);
void        nautilus_canvas_item_invalidate_label_size    (NautilusCanvasItem       *item);
EelDRect    nautilus_canvas_item_get_icon_rectangle     (const NautilusCanvasItem *item);
void        nautilus_canvas_item_get_bounds_for_layout    (NautilusCanvasItem       *item,
							   double *x1, double *y1, double *x2, double *y2);
void        nautilus_canvas_item_get_bounds_for_entire_item (NautilusCanvasItem       *item,
							     double *x1, double *y1, double *x2, double *y2);
void        nautilus_canvas_item_update_bounds            (NautilusCanvasItem       *item,
							   double i2w_dx, double i2w_dy);
void        nautilus_canvas_item_set_is_visible           (NautilusCanvasItem       *item,
							   gboolean                  visible);
/* whether the entire label text must be visible at all times */
void        nautilus_canvas_item_set_entire_text          (NautilusCanvasItem       *canvas_item,
							   gboolean                  entire_text);

G_END_DECLS
