/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Icon canvas item class for icon container.
 *
 * Copyright (C) 2000 Eazel, Inc
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

#include <config.h>
#include <math.h>
#include "nautilus-icon-canvas-item.h"

#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-factory.h"
#include "nautilus-icon-private.h"
#include "nautilus-theme.h"
#include <eel/eel-art-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-pango-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-accessibility.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libgnome/gnome-i18n.h>
#include <eel/eel-canvas-util.h>
#include <atk/atkimage.h>
#include <atk/atkcomponent.h>
#include <atk/atknoopobject.h>
#include <stdio.h>
#include <string.h>

#define EMBLEM_SPACING 2

/* gap between bottom of icon and start of text box */
#define LABEL_OFFSET 1
#define LABEL_LINE_SPACING 0

#define MAX_TEXT_WIDTH_STANDARD 135
#define MAX_TEXT_WIDTH_TIGHTER 80
#define MAX_TEXT_WIDTH_BESIDE 90

/* Private part of the NautilusIconCanvasItem structure. */
struct NautilusIconCanvasItemDetails {
	/* The image, text, font. */
	double x, y;
	GdkPixbuf *pixbuf;
	GdkPixbuf *rendered_pixbuf;
	GList *emblem_pixbufs;
	char *editable_text;		/* Text that can be modified by a renaming function */
	char *additional_text;		/* Text that cannot be modifed, such as file size, etc. */
	NautilusEmblemAttachPoints *attach_points;
	
	/* Size of the text at current font. */
	int text_width;
	int text_height;
	
	/* preview state */
	guint is_active : 1;

    	/* Highlight state. */
   	guint is_highlighted_for_selection : 1;
	guint is_highlighted_as_keyboard_focus: 1;
   	guint is_highlighted_for_drop : 1;
	guint show_stretch_handles : 1;
	guint is_prelit : 1;

	guint rendered_is_active : 1;
	guint rendered_is_highlighted_for_selection : 1;
	guint rendered_is_highlighted_for_drop : 1;
	guint rendered_is_prelit : 1;
	guint rendered_is_focused : 1;
	
	guint is_renaming : 1;
	
	guint bounds_cached : 1;
	
	PangoLayout *editable_text_layout;
	PangoLayout *additional_text_layout;
	
	GdkRectangle embedded_text_rect;
	PangoLayout *embedded_text_layout;

	/* Cached rectangle in canvas coordinates */
	ArtIRect canvas_rect;
	ArtIRect text_rect;
	ArtIRect emblem_rect;

	ArtIRect bounds_cache;
	
	/* Accessibility bits */
	GailTextUtil *text_util;
};

/* Object argument IDs. */
enum {
	PROP_0,
	PROP_EDITABLE_TEXT,
	PROP_ADDITIONAL_TEXT,
    	PROP_HIGHLIGHTED_FOR_SELECTION,
    	PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
    	PROP_HIGHLIGHTED_FOR_DROP
};

typedef enum {
	RIGHT_SIDE,
	BOTTOM_SIDE,
	LEFT_SIDE,
	TOP_SIDE
} RectangleSide;

typedef struct {
	NautilusIconCanvasItem *icon_item;
	ArtIRect icon_rect;
	RectangleSide side;
	int position;
	int index;
	GList *emblem;
} EmblemLayout;

#define ANTIALIAS_SELECTION_RECTANGLE TRUE

static int click_policy_auto_value;

/* GtkObject */
static void     nautilus_icon_canvas_item_class_init (NautilusIconCanvasItemClass   *class);
static void     nautilus_icon_canvas_item_init       (NautilusIconCanvasItem        *item);

/* private */
static void     draw_or_measure_label_text           (NautilusIconCanvasItem        *item,
						      GdkDrawable                   *drawable,
						      ArtIRect                       icon_rect);
static void     draw_label_text                      (NautilusIconCanvasItem        *item,
						      GdkDrawable                   *drawable,
						      ArtIRect                       icon_rect);
static void     measure_label_text                   (NautilusIconCanvasItem        *item);
static void     get_icon_canvas_rectangle            (NautilusIconCanvasItem        *item,
						      ArtIRect                      *rect);
static void     emblem_layout_reset                  (EmblemLayout                  *layout,
						      NautilusIconCanvasItem        *icon_item,
						      ArtIRect                       icon_rect);
static gboolean emblem_layout_next                   (EmblemLayout                  *layout,
						      GdkPixbuf                    **emblem_pixbuf,
						      ArtIRect                      *emblem_rect);
static void     draw_pixbuf                          (GdkPixbuf                     *pixbuf,
						      GdkDrawable                   *drawable,
						      int                            x,
						      int                            y);
static PangoLayout *get_label_layout                 (PangoLayout                  **layout,
						      NautilusIconCanvasItem        *item,
						      const char                    *text);
static void     draw_label_layout                    (NautilusIconCanvasItem        *item,
						      GdkDrawable                   *drawable,
						      PangoLayout                   *layout,
						      gboolean                       highlight,
						      GdkColor                      *label_color,
						      int                            x,
						      int                            y,
						      GdkGC                         *gc);

static gboolean hit_test_stretch_handle              (NautilusIconCanvasItem        *item,
						      ArtIRect                       canvas_rect);
static void     clear_rounded_corners                (GdkPixbuf                     *destination_pixbuf,
						      GdkPixbuf                     *corner_pixbuf,
						      int                            corner_size);
static void      draw_embedded_text                  (NautilusIconCanvasItem        *icon_item,
						      GdkDrawable                   *drawable,
						      int                            x,
						      int                            y);


static NautilusIconCanvasItemClass *parent_class = NULL;
static gpointer accessible_parent_class = NULL;

/* Object initialization function for the icon item. */
static void
nautilus_icon_canvas_item_init (NautilusIconCanvasItem *icon_item)
{
	static gboolean setup_auto_enums = FALSE;
	NautilusIconCanvasItemDetails *details;

	if (!setup_auto_enums) {
		eel_preferences_add_auto_enum
			(NAUTILUS_PREFERENCES_CLICK_POLICY,
			 &click_policy_auto_value);
		setup_auto_enums = TRUE;
	}

	details = g_new0 (NautilusIconCanvasItemDetails, 1);

	icon_item->details = details;
	nautilus_icon_canvas_item_invalidate_label_size (icon_item);
}

static void
nautilus_icon_canvas_item_finalize (GObject *object)
{
	NautilusIconCanvasItemDetails *details;

	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (object));

	details = NAUTILUS_ICON_CANVAS_ITEM (object)->details;

	if (details->pixbuf != NULL) {
		g_object_unref (details->pixbuf);
	}
	
	if (details->text_util != NULL) {
		g_object_unref (details->text_util);
	}

	eel_gdk_pixbuf_list_free (details->emblem_pixbufs);
	g_free (details->editable_text);
	g_free (details->additional_text);
	g_free (details->attach_points);
	
	if (details->rendered_pixbuf != NULL) {
		g_object_unref (details->rendered_pixbuf);
	}

	if (details->editable_text_layout != NULL) {
		g_object_unref (details->editable_text_layout);
	}

	if (details->additional_text_layout != NULL) {
		g_object_unref (details->additional_text_layout);
	}

	if (details->embedded_text_layout != NULL) {
		g_object_unref (details->embedded_text_layout);
	}

	
	g_free (details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}
 
/* Currently we require pixbufs in this format (for hit testing).
 * Perhaps gdk-pixbuf will be changed so it can do the hit testing
 * and we won't have this requirement any more.
 */
static gboolean
pixbuf_is_acceptable (GdkPixbuf *pixbuf)
{
	return gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB
		&& ((!gdk_pixbuf_get_has_alpha (pixbuf)
		     && gdk_pixbuf_get_n_channels (pixbuf) == 3)
		    || (gdk_pixbuf_get_has_alpha (pixbuf)
			&& gdk_pixbuf_get_n_channels (pixbuf) == 4))
		&& gdk_pixbuf_get_bits_per_sample (pixbuf) == 8;
}

static void
nautilus_icon_canvas_item_invalidate_bounds_cache (NautilusIconCanvasItem *item)
{
	item->details->bounds_cached = FALSE;
}

/* invalidate the text width and height cached in the item details. */
void
nautilus_icon_canvas_item_invalidate_label_size (NautilusIconCanvasItem *item)
{
	nautilus_icon_canvas_item_invalidate_bounds_cache (item);
	item->details->text_width = -1;
	item->details->text_height = -1;
	if (item->details->editable_text_layout != NULL) {
		g_object_unref (item->details->editable_text_layout);
		item->details->editable_text_layout = NULL;
	}
	if (item->details->additional_text_layout != NULL) {
		g_object_unref (item->details->additional_text_layout);
		item->details->additional_text_layout = NULL;
	}
}

/* Set property handler for the icon item. */
static void
nautilus_icon_canvas_item_set_property (GObject        *object,
					guint           property_id,
					const GValue   *value,
					GParamSpec     *pspec)
{
	NautilusIconCanvasItem *item;
	NautilusIconCanvasItemDetails *details;

	item = NAUTILUS_ICON_CANVAS_ITEM (object);
	details = item->details;

	switch (property_id) {

	case PROP_EDITABLE_TEXT:
		if (eel_strcmp (details->editable_text,
				g_value_get_string (value)) == 0) {
			return;
		}

		g_free (details->editable_text);
		details->editable_text = g_strdup (g_value_get_string (value));
		if (details->text_util) {
			gail_text_util_text_setup (details->text_util,
						   details->editable_text);
		}
		
		nautilus_icon_canvas_item_invalidate_label_size (item);
		break;

	case PROP_ADDITIONAL_TEXT:
		if (eel_strcmp (details->additional_text,
				g_value_get_string (value)) == 0) {
			return;
		}

		g_free (details->additional_text);
		details->additional_text = g_strdup (g_value_get_string (value));
		
		nautilus_icon_canvas_item_invalidate_label_size (item);		
		break;

	case PROP_HIGHLIGHTED_FOR_SELECTION:
		if (!details->is_highlighted_for_selection == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_for_selection = g_value_get_boolean (value);
		break;
         
        case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
		if (!details->is_highlighted_as_keyboard_focus == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_as_keyboard_focus = g_value_get_boolean (value);

		if (details->is_highlighted_as_keyboard_focus) {
			AtkObject *atk_object = eel_accessibility_for_object (object);
			atk_focus_tracker_notify (atk_object);
		}
		break;
		
        case PROP_HIGHLIGHTED_FOR_DROP:
		if (!details->is_highlighted_for_drop == !g_value_get_boolean (value)) {
			return;
		}
		details->is_highlighted_for_drop = g_value_get_boolean (value);
		break;
		
	default:
		g_warning ("nautilus_icons_view_item_item_set_arg on unknown argument");
		return;
	}
	
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (object));
}

/* Get property handler for the icon item */
static void
nautilus_icon_canvas_item_get_property (GObject        *object,
					guint           property_id,
					GValue         *value,
					GParamSpec     *pspec)
{
	NautilusIconCanvasItemDetails *details;
	
	details = NAUTILUS_ICON_CANVAS_ITEM (object)->details;
	
	switch (property_id) {
		
	case PROP_EDITABLE_TEXT:
		g_value_set_string (value, details->editable_text);
		break;

	case PROP_ADDITIONAL_TEXT:
		g_value_set_string (value, details->additional_text);
		break;
		
        case PROP_HIGHLIGHTED_FOR_SELECTION:
		g_value_set_boolean (value, details->is_highlighted_for_selection);
                break;
		
        case PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
		g_value_set_boolean (value, details->is_highlighted_as_keyboard_focus);
                break;
		
        case PROP_HIGHLIGHTED_FOR_DROP:
		g_value_set_boolean (value, details->is_highlighted_for_drop);
                break;

        default:
		g_warning ("invalid property %d", property_id);
		break;
	}
}

GdkPixmap *
nautilus_icon_canvas_item_get_image (NautilusIconCanvasItem *item,
				     GdkBitmap **mask)
{
	GdkPixmap *pixmap;
	EelCanvas *canvas;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), NULL);

	canvas = EEL_CANVAS_ITEM (item)->canvas;
	gdk_pixbuf_render_pixmap_and_mask_for_colormap (item->details->pixbuf,
							gtk_widget_get_colormap (GTK_WIDGET (canvas)),
							&pixmap,
							mask,
							128);
	
	draw_embedded_text (item, GDK_DRAWABLE (pixmap), 0,0);

	return pixmap;
}

void
nautilus_icon_canvas_item_set_image (NautilusIconCanvasItem *item,
				     GdkPixbuf *image)
{
	NautilusIconCanvasItemDetails *details;	
	
	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (image == NULL || pixbuf_is_acceptable (image));

	details = item->details;	
	if (details->pixbuf == image) {
		return;
	}

	if (image != NULL) {
		g_object_ref (image);
	}
	if (details->pixbuf != NULL) {
		g_object_unref (details->pixbuf);
	}
	if (details->rendered_pixbuf != NULL) {
		g_object_unref (details->rendered_pixbuf);
		details->rendered_pixbuf = NULL;
	}

	details->pixbuf = image;
			
	nautilus_icon_canvas_item_invalidate_bounds_cache (item);
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));	
}

void
nautilus_icon_canvas_item_set_emblems (NautilusIconCanvasItem *item,
				       GList *emblem_pixbufs)
{
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));

	g_assert (item->details->emblem_pixbufs != emblem_pixbufs || emblem_pixbufs == NULL);

	/* The case where the emblems are identical is fairly common,
	 * so lets take the time to check for it.
	 */
	if (eel_g_list_equal (item->details->emblem_pixbufs, emblem_pixbufs)) {
		return;
	}

	/* Check if they are acceptable. */
	for (p = emblem_pixbufs; p != NULL; p = p->next) {
		g_return_if_fail (pixbuf_is_acceptable (p->data));
	}
	
	/* Take in the new list of emblems. */
	eel_gdk_pixbuf_list_ref (emblem_pixbufs);
	eel_gdk_pixbuf_list_free (item->details->emblem_pixbufs);
	item->details->emblem_pixbufs = g_list_copy (emblem_pixbufs);

	nautilus_icon_canvas_item_invalidate_bounds_cache (item);
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

void 
nautilus_icon_canvas_item_set_attach_points (NautilusIconCanvasItem *item,
					     NautilusEmblemAttachPoints *attach_points)
{
	g_free (item->details->attach_points);
	item->details->attach_points = NULL;

	if (attach_points != NULL && attach_points->num_points != 0) {
		item->details->attach_points = g_new (NautilusEmblemAttachPoints, 1);
		*item->details->attach_points = *attach_points;
	}
	
	nautilus_icon_canvas_item_invalidate_bounds_cache (item);
}

void
nautilus_icon_canvas_item_set_embedded_text_rect (NautilusIconCanvasItem       *item,
						  const GdkRectangle           *text_rect)
{
	item->details->embedded_text_rect = *text_rect;

	nautilus_icon_canvas_item_invalidate_bounds_cache (item);
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

void
nautilus_icon_canvas_item_set_embedded_text (NautilusIconCanvasItem       *item,
					     const char                   *text)
{
	PangoContext *context;
	PangoFontDescription *desc;
	
	if (item->details->embedded_text_layout == NULL && text != NULL) {
		context = gtk_widget_get_pango_context (GTK_WIDGET (EEL_CANVAS_ITEM(item)->canvas));
		item->details->embedded_text_layout = pango_layout_new (context);
		
		desc = pango_font_description_from_string ("monospace 6");
		pango_layout_set_font_description (item->details->embedded_text_layout, desc);
		pango_font_description_free (desc);
	}
	
	if (item->details->embedded_text_layout != NULL) {
		if (text != NULL) {
			pango_layout_set_text (item->details->embedded_text_layout, text, -1);
		} else {
			pango_layout_set_text (item->details->embedded_text_layout, "", -1);
		}
			
		eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
	}
}


/* Recomputes the bounding box of a icon canvas item.
 * This is a generic implementation that could be used for any canvas item
 * class, it has no assumptions about how the item is used.
 */
static void
recompute_bounding_box (NautilusIconCanvasItem *icon_item,
			double i2w_dx, double i2w_dy)
{
	/* The bounds stored in the item is the same as what get_bounds
	 * returns, except it's in canvas coordinates instead of the item's
	 * parent's coordinates.
	 */

	EelCanvasItem *item;
	ArtPoint top_left, bottom_right;

	item = EEL_CANVAS_ITEM (icon_item);

	eel_canvas_item_get_bounds (item,
				    &top_left.x, &top_left.y,
				    &bottom_right.x, &bottom_right.y);

	top_left.x += i2w_dx;
	top_left.y += i2w_dy;
	bottom_right.x += i2w_dx;
	bottom_right.y += i2w_dy;
	eel_canvas_w2c_d (item->canvas,
			  top_left.x, top_left.y,
			  &item->x1, &item->y1);
	eel_canvas_w2c_d (item->canvas,
			  bottom_right.x, bottom_right.y,
			  &item->x2, &item->y2);
}

static ArtIRect
compute_text_rectangle (const NautilusIconCanvasItem *item,
			ArtIRect icon_rectangle,
			gboolean canvas_coords)
{
	ArtIRect text_rectangle;
	double pixels_per_unit;
	double text_width, text_height;

	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	if (canvas_coords) {
		text_width = item->details->text_width;
		text_height = item->details->text_height;
	} else {
		text_width = item->details->text_width / pixels_per_unit;
		text_height = item->details->text_height / pixels_per_unit;
	}
	
	if (NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
                text_rectangle.x0 = icon_rectangle.x1;
                text_rectangle.x1 = text_rectangle.x0 + text_width;
                text_rectangle.y0 = icon_rectangle.y0;
                text_rectangle.y1 = text_rectangle.y0 + text_height + LABEL_OFFSET / pixels_per_unit;
	} else {
                text_rectangle.x0 = (icon_rectangle.x0 + icon_rectangle.x1) / 2
			- text_width / 2;
                text_rectangle.y0 = icon_rectangle.y1;
                text_rectangle.x1 = text_rectangle.x0 + text_width;
                text_rectangle.y1 = text_rectangle.y0 + text_height + LABEL_OFFSET / pixels_per_unit;
        }

	return text_rectangle;
}

static ArtIRect
get_current_canvas_bounds (EelCanvasItem *item)
{
	ArtIRect bounds;

	g_return_val_if_fail (EEL_IS_CANVAS_ITEM (item), eel_art_irect_empty);

	bounds.x0 = item->x1;
	bounds.y0 = item->y1;
	bounds.x1 = item->x2;
	bounds.y1 = item->y2;

	return bounds;
}

void
nautilus_icon_canvas_item_update_bounds (NautilusIconCanvasItem *item,
					 double i2w_dx, double i2w_dy)
{
	ArtIRect before, after, emblem_rect;
	EmblemLayout emblem_layout;
	EelCanvasItem *canvas_item;
	GdkPixbuf *emblem_pixbuf;

	canvas_item = EEL_CANVAS_ITEM (item);
	
	/* Compute new bounds. */
	before = get_current_canvas_bounds (canvas_item);
	recompute_bounding_box (item, i2w_dx, i2w_dy);
	after = get_current_canvas_bounds (canvas_item);

	/* If the bounds didn't change, we are done. */
	if (eel_art_irect_equal (before, after)) {
		return;
	}
	
	/* Update canvas and text rect cache */
	get_icon_canvas_rectangle (item, &item->details->canvas_rect);
	item->details->text_rect = compute_text_rectangle (item, item->details->canvas_rect, FALSE);
	
	/* Update emblem rect cache */
	item->details->emblem_rect.x0 = 0;
	item->details->emblem_rect.x1 = 0;
	item->details->emblem_rect.y0 = 0;
	item->details->emblem_rect.y1 = 0;
	emblem_layout_reset (&emblem_layout, item, item->details->canvas_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		art_irect_union (&item->details->emblem_rect, &item->details->emblem_rect, &emblem_rect);
	}

	/* queue a redraw. */
	eel_canvas_request_redraw (canvas_item->canvas,
				   before.x0, before.y0,
				   before.x1 + 1, before.y1 + 1);
}

/* Update handler for the icon canvas item. */
static void
nautilus_icon_canvas_item_update (EelCanvasItem *item,
				  double i2w_dx, double i2w_dy,
				  gint flags)
{
	nautilus_icon_canvas_item_update_bounds (NAUTILUS_ICON_CANVAS_ITEM (item), i2w_dx, i2w_dy);

	eel_canvas_item_request_redraw (EEL_CANVAS_ITEM (item));

	EEL_CALL_PARENT (EEL_CANVAS_ITEM_CLASS, update,
			 (item, i2w_dx, i2w_dy, flags));
}

/* Rendering */

static gboolean
in_single_click_mode (void)
{
	return click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE;
}

/* Multiplies each pixel in a pixbuf by the specified color */
static void
multiply_pixbuf_rgba (GdkPixbuf *pixbuf, guint rgba)
{
	guchar *pixels;
	int r, g, b, a;
	int width, height, rowstride;
	gboolean has_alpha;
	int x, y;
	guchar *p;

	g_return_if_fail (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
	g_return_if_fail (gdk_pixbuf_get_n_channels (pixbuf) == 3
			  || gdk_pixbuf_get_n_channels (pixbuf) == 4);

	r = EEL_RGBA_COLOR_GET_R (rgba);
	g = EEL_RGBA_COLOR_GET_G (rgba);
	b = EEL_RGBA_COLOR_GET_B (rgba);
	a = EEL_RGBA_COLOR_GET_A (rgba);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	pixels = gdk_pixbuf_get_pixels (pixbuf);

	for (y = 0; y < height; y++) {
		p = pixels;

		for (x = 0; x < width; x++) {
			p[0] = p[0] * r / 255;
			p[1] = p[1] * g / 255;
			p[2] = p[2] * b / 255;

			if (has_alpha) {
				p[3] = p[3] * a / 255;
				p += 4;
			} else
				p += 3;
		}

		pixels += rowstride;
	}
}

static void
draw_frame (NautilusIconCanvasItem *item,
	    GdkDrawable *drawable,
	    guint color,
	    int x, 
	    int y,
	    int width,
	    int height)
{
	GdkPixbuf *selection_pixbuf;
	NautilusIconContainer *container;

	container = NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);

	if (ANTIALIAS_SELECTION_RECTANGLE) {
		selection_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
						   TRUE,
						   8,
						   width,
						   height);
		eel_gdk_pixbuf_fill_rectangle_with_color (selection_pixbuf,
							  eel_gdk_pixbuf_whole_pixbuf,
							  0xffffffff);
		clear_rounded_corners (selection_pixbuf,
				       container->details->highlight_frame,
				       5);
		multiply_pixbuf_rgba (selection_pixbuf,
				      color);
		
		draw_pixbuf (selection_pixbuf, drawable, x, y);
		g_object_unref (selection_pixbuf);
	} else {
		gdk_draw_rectangle
			(drawable, 
			 GTK_WIDGET (container)->style->black_gc, TRUE,
			 x, y, width, height);
	}
}

/* Keep these for a bit while we work on performance of draw_or_measure_label_text. */
/*
  #define PERFORMANCE_TEST_DRAW_DISABLE
  #define PERFORMANCE_TEST_MEASURE_DISABLE
*/

static void
draw_or_measure_label_text (NautilusIconCanvasItem *item,
			    GdkDrawable *drawable,
			    ArtIRect icon_rect)
{
	NautilusIconCanvasItemDetails *details;
	NautilusIconContainer *container;
	guint editable_height, editable_width;
	guint additional_height, additional_width;
	EelCanvasItem *canvas_item;
	PangoLayout *editable_layout;
	PangoLayout *additional_layout;
	GdkColor *label_color;
	int icon_width;
	gboolean have_editable, have_additional, needs_highlight, needs_frame;
	int max_text_width;
	int x;
	GdkGC *gc;
	ArtIRect text_rect;
	
	icon_width = 0;
	gc = NULL;

	details = item->details;
	needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;

	have_editable = details->editable_text != NULL && details->editable_text[0] != '\0';
	have_additional = details->additional_text != NULL && details->additional_text[0] != '\0';

	/* No font or no text, then do no work. */
	if (!have_editable && !have_additional) {
		details->text_height = 0;
		details->text_width = 0;			
		return;
	}

#if (defined PERFORMANCE_TEST_MEASURE_DISABLE && defined PERFORMANCE_TEST_DRAW_DISABLE)
	/* don't do any drawing and fake out the width */
	details->text_width = 80;
	details->text_height = 20;
	return;
#endif

#ifdef PERFORMANCE_TEST_MEASURE_DISABLE
	if (drawable == NULL) {
		/* fake out the width */
		details->text_width = 80;
		details->text_height = 20;
		return;
	}
#endif

#ifdef PERFORMANCE_TEST_DRAW_DISABLE
	if (drawable != NULL) {
		return;
	}
#endif

	canvas_item = EEL_CANVAS_ITEM (item);
	if (drawable != NULL) {
		icon_width = details->pixbuf == NULL ? 0 : gdk_pixbuf_get_width (details->pixbuf);
	}
	
	editable_width = 0;
	editable_height = 0;
	additional_width = 0;
	additional_height = 0;

	max_text_width = floor (nautilus_icon_canvas_item_get_max_text_width (item));

	container = NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);	
	editable_layout = NULL;
	additional_layout = NULL;

	if (have_editable) {
		editable_layout = get_label_layout (&details->editable_text_layout, item, details->editable_text);
		
		pango_layout_get_pixel_size (editable_layout, 
					     &editable_width,
					     &editable_height);
	}

	if (have_additional) {
		additional_layout = get_label_layout (&details->additional_text_layout, item, details->additional_text);

		pango_layout_get_pixel_size (additional_layout, 
					     &additional_width,
					     &additional_height);
	}

	details->text_width = MAX (editable_width, additional_width);

	if (have_additional) {
		details->text_height = editable_height + LABEL_LINE_SPACING + additional_height;
	} else {
		details->text_height = editable_height;
	}

	if (ANTIALIAS_SELECTION_RECTANGLE) {
		/* add some extra space for highlighting even when we don't highlight so things won't move */
		details->text_height += 2; /* extra slop for nicer highlighting */	
		details->text_width += 8;  /* extra to make it look nicer */
	} else {
		/* add slop used for highlighting, even if we're not highlighting now */
		details->text_width += 4;
	}

	/* if measuring, we are done */
	if (!drawable) {
		if (editable_layout) {
			g_object_unref (editable_layout);
		}

		if (additional_layout) {
			g_object_unref (additional_layout);
		}

		return;
	}

	text_rect = compute_text_rectangle (item, icon_rect, TRUE);
	
	/* if the icon is highlighted, do some set-up */
	if (needs_highlight && !details->is_renaming &&
	    details->text_width > 0 && details->text_height > 0) {
		draw_frame (item,
			    drawable,
			    GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (container)) ? container->details->highlight_color_rgba : container->details->active_color_rgba,
			    text_rect.x0,
			    text_rect.y0,
			    text_rect.x1 - text_rect.x0,
			    text_rect.y1 - text_rect.y0);
	}

		
	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		x = text_rect.x0 + 2;
	} else {
		x = text_rect.x0 + ((text_rect.x1 - text_rect.x0) - max_text_width) / 2;
	}
	
	if (have_editable) {
		gtk_widget_style_get (GTK_WIDGET (container),
				      "frame_text", &needs_frame,
				      NULL);
		if (needs_frame && !needs_highlight && details->text_width > 0 && details->text_height > 0) {
			draw_frame (item, 
				    drawable,
				    eel_gdk_color_to_rgb (&GTK_WIDGET (container)->style->base[GTK_STATE_NORMAL]),
				    text_rect.x0,
				    text_rect.y0,
				    text_rect.x1 - text_rect.x0,
				    text_rect.y1 - text_rect.y0);
		}
		
		gc = nautilus_icon_container_get_label_color_and_gc
			(NAUTILUS_ICON_CONTAINER (canvas_item->canvas),
			 &label_color, TRUE, needs_highlight);

		draw_label_layout (item, drawable,
				   editable_layout, needs_highlight,
				   label_color,
				   x,
				   text_rect.y0, gc);
	}

	if (have_additional) {
		gc = nautilus_icon_container_get_label_color_and_gc
			(NAUTILUS_ICON_CONTAINER (canvas_item->canvas),
			 &label_color, FALSE, needs_highlight);
		
		draw_label_layout (item, drawable,
				   additional_layout, needs_highlight,
				   label_color,
				   x,
				   text_rect.y0 + editable_height + LABEL_LINE_SPACING, gc);
	}

	if (item->details->is_highlighted_as_keyboard_focus) {
		gtk_paint_focus (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas)->style,
				 drawable,
				 needs_highlight ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,
				 NULL,
				 GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas),
				 "icon-container",
				 text_rect.x0,
				 text_rect.y0,
				 text_rect.x1 - text_rect.x0,
				 text_rect.y1 - text_rect.y0);
	}

	if (editable_layout) {
		g_object_unref (editable_layout);
	}
	
	if (additional_layout) {
		g_object_unref (additional_layout);
	}
}

static void
measure_label_text (NautilusIconCanvasItem *item)
{
	ArtIRect rect = {0, };
	
	/* check to see if the cached values are still valid; if so, there's
	 * no work necessary
	 */
	
	if (item->details->text_width >= 0 && item->details->text_height >= 0) {
		return;
	}
	
	draw_or_measure_label_text (item, NULL, rect);
}

static void
draw_label_text (NautilusIconCanvasItem *item, GdkDrawable *drawable,
		 ArtIRect icon_rect)
{
	draw_or_measure_label_text (item, drawable, icon_rect);
}

static void
draw_stretch_handles (NautilusIconCanvasItem *item, GdkDrawable *drawable,
		      const ArtIRect *rect)
{
	GdkGC *gc;
	char *knob_filename;
	GdkPixbuf *knob_pixbuf;
	GdkBitmap *stipple;
	int knob_width, knob_height;
	
	if (!item->details->show_stretch_handles) {
		return;
	}

	gc = gdk_gc_new (drawable);

	knob_filename = nautilus_theme_get_image_path ("knob.png");
	knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename, NULL);
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);

	stipple = eel_stipple_bitmap_for_screen (
			gdk_drawable_get_screen (GDK_DRAWABLE (drawable)));
	
	/* first draw the box */		
	gdk_gc_set_stipple (gc, stipple);
	gdk_gc_set_fill (gc, GDK_STIPPLED);
	gdk_draw_rectangle
		(drawable, gc, FALSE,
		 rect->x0,
		 rect->y0,
		 rect->x1 - rect->x0 - 1,
		 rect->y1 - rect->y0 - 1);
	
	/* draw the stretch handles themselves */
	
	draw_pixbuf (knob_pixbuf, drawable, rect->x0, rect->y0);
	draw_pixbuf (knob_pixbuf, drawable, rect->x0, rect->y1 - knob_height);
	draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y0);
	draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y1 - knob_height);
	
	g_free (knob_filename);
	g_object_unref (knob_pixbuf);	

	g_object_unref (gc);
}

static void
emblem_layout_reset (EmblemLayout *layout, NautilusIconCanvasItem *icon_item, ArtIRect icon_rect)
{
	layout->icon_item = icon_item;
	layout->icon_rect = icon_rect;
	layout->side = RIGHT_SIDE;
	layout->position = 0;
	layout->index = 0;
	layout->emblem = icon_item->details->emblem_pixbufs;
}

static gboolean
emblem_layout_next (EmblemLayout *layout,
		    GdkPixbuf **emblem_pixbuf,
		    ArtIRect *emblem_rect)
{
	GdkPixbuf *pixbuf;
	int width, height, x, y;
	NautilusEmblemAttachPoints *attach_points;
	
	/* Check if we have layed out all of the pixbufs. */
	if (layout->emblem == NULL) {
		return FALSE;
	}

	/* Get the pixbuf. */
	pixbuf = layout->emblem->data;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);


	/* Advance to the next emblem. */
	layout->emblem = layout->emblem->next;

	attach_points = layout->icon_item->details->attach_points;
	if (attach_points != NULL) {
		if (layout->index >= attach_points->num_points) {
			return FALSE;
		}
		
		x = layout->icon_rect.x0 + attach_points->points[layout->index].x;
		y = layout->icon_rect.y0 + attach_points->points[layout->index].y;

		layout->index += 1;
		
		/* Return the rectangle and pixbuf. */
		*emblem_pixbuf = pixbuf;
		emblem_rect->x0 = x - width / 2;
		emblem_rect->y0 = y - height / 2;
		emblem_rect->x1 = emblem_rect->x0 + width;
		emblem_rect->y1 = emblem_rect->y0 + height;

		return TRUE;

	}
	
	for (;;) {

		/* Find the side to lay out along. */
		switch (layout->side) {
		case RIGHT_SIDE:
			x = layout->icon_rect.x1;
			y = layout->icon_rect.y0;
			break;
		case BOTTOM_SIDE:
			x = layout->icon_rect.x1;
			y = layout->icon_rect.y1;
			break;
		case LEFT_SIDE:
			x = layout->icon_rect.x0;
			y = layout->icon_rect.y1;
			break;
		case TOP_SIDE:
			x = layout->icon_rect.x0;
			y = layout->icon_rect.y0;
			break;
		default:
			g_assert_not_reached ();
			x = 0;
			y = 0;
			break;
		}
		if (layout->position != 0) {
			switch (layout->side) {
			case RIGHT_SIDE:
				y += layout->position + height / 2;
				break;
			case BOTTOM_SIDE:
				x -= layout->position + width / 2;
				break;
			case LEFT_SIDE:
				y -= layout->position + height / 2;
				break;
			case TOP_SIDE:
				x += layout->position + width / 2;
				break;
			}
		}
		
		/* Check to see if emblem fits in current side. */
		if (x >= layout->icon_rect.x0 && x <= layout->icon_rect.x1
		    && y >= layout->icon_rect.y0 && y <= layout->icon_rect.y1) {

			/* It fits. */

			/* Advance along the side. */
			switch (layout->side) {
			case RIGHT_SIDE:
			case LEFT_SIDE:
				layout->position += height + EMBLEM_SPACING;
				break;
			case BOTTOM_SIDE:
			case TOP_SIDE:
				layout->position += width + EMBLEM_SPACING;
				break;
			}

			/* Return the rectangle and pixbuf. */
			*emblem_pixbuf = pixbuf;
			emblem_rect->x0 = x - width / 2;
			emblem_rect->y0 = y - height / 2;
			emblem_rect->x1 = emblem_rect->x0 + width;
			emblem_rect->y1 = emblem_rect->y0 + height;

			return TRUE;
		}
	
		/* It doesn't fit, so move to the next side. */
		switch (layout->side) {
		case RIGHT_SIDE:
			layout->side = BOTTOM_SIDE;
			break;
		case BOTTOM_SIDE:
			layout->side = LEFT_SIDE;
			break;
		case LEFT_SIDE:
			layout->side = TOP_SIDE;
			break;
		case TOP_SIDE:
		default:
			return FALSE;
		}
		layout->position = 0;
	}
}

static void
draw_pixbuf (GdkPixbuf *pixbuf, GdkDrawable *drawable, int x, int y)
{
	/* FIXME bugzilla.gnome.org 40703: 
	 * Dither would be better if we passed dither values. 
	 */
	gdk_draw_pixbuf (drawable, NULL, pixbuf, 0, 0, x, y,
			 gdk_pixbuf_get_width (pixbuf),
			 gdk_pixbuf_get_height (pixbuf),
			 GDK_RGB_DITHER_NORMAL, 0, 0);
}

/* shared code to highlight or dim the passed-in pixbuf */
static GdkPixbuf *
real_map_pixbuf (NautilusIconCanvasItem *icon_item)
{
	EelCanvas *canvas;
	char *audio_filename;
	GdkPixbuf *temp_pixbuf, *old_pixbuf, *audio_pixbuf;
	
	temp_pixbuf = icon_item->details->pixbuf;
	canvas = EEL_CANVAS_ITEM(icon_item)->canvas;

	g_object_ref (temp_pixbuf);

	if (icon_item->details->is_prelit) {
		old_pixbuf = temp_pixbuf;
		temp_pixbuf = eel_create_spotlight_pixbuf (temp_pixbuf);
		g_object_unref (old_pixbuf);

		/* FIXME bugzilla.gnome.org 42471: This hard-wired image is inappropriate to
		 * this level of code, which shouldn't know that the
		 * preview is audio, nor should it have an icon
		 * hard-wired in.
		 */

		/* if the icon is currently being previewed, superimpose an image to indicate that */
		/* audio is the only kind of previewing right now, so this code isn't as general as it could be */
		if (icon_item->details->is_active) {
			/* Load the audio symbol. */
			audio_filename = nautilus_pixmap_file ("audio.png");
			if (audio_filename != NULL) {
				audio_pixbuf = gdk_pixbuf_new_from_file (audio_filename, NULL);
			} else {
				audio_pixbuf = NULL;
			}
			
			/* Composite it onto the icon. */
			if (audio_pixbuf != NULL) {
				gdk_pixbuf_composite
					(audio_pixbuf,
					 temp_pixbuf,
					 0, 0,
					 gdk_pixbuf_get_width (temp_pixbuf),
					 gdk_pixbuf_get_height(temp_pixbuf),
					 0, 0,
					 canvas->pixels_per_unit,
					 canvas->pixels_per_unit,
					 GDK_INTERP_BILINEAR, 0xFF);
				
				g_object_unref (audio_pixbuf);
			}
			
			g_free (audio_filename);
		}
	}

	if (icon_item->details->is_highlighted_for_selection
	    || icon_item->details->is_highlighted_for_drop) {
		guint color;

		old_pixbuf = temp_pixbuf;

		color =  GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (canvas)) ? NAUTILUS_ICON_CONTAINER (canvas)->details->highlight_color_rgba : NAUTILUS_ICON_CONTAINER (canvas)->details->active_color_rgba;

		temp_pixbuf = eel_create_colorized_pixbuf (temp_pixbuf,
							   EEL_RGBA_COLOR_GET_R (color),
							   EEL_RGBA_COLOR_GET_G(color),
							   EEL_RGBA_COLOR_GET_B (color));
							   
		g_object_unref (old_pixbuf);
	} 

	return temp_pixbuf;
}

static GdkPixbuf *
map_pixbuf (NautilusIconCanvasItem *icon_item)
{
	if (!(icon_item->details->rendered_pixbuf != NULL
	      && icon_item->details->rendered_is_active == icon_item->details->is_active
	      && icon_item->details->rendered_is_prelit == icon_item->details->is_prelit
	      && icon_item->details->rendered_is_highlighted_for_selection == icon_item->details->is_highlighted_for_selection
	      && icon_item->details->rendered_is_highlighted_for_drop == icon_item->details->is_highlighted_for_drop
	      && (icon_item->details->is_highlighted_for_selection && icon_item->details->rendered_is_focused == GTK_WIDGET_HAS_FOCUS (EEL_CANVAS_ITEM (icon_item)->canvas)))) {
		if (icon_item->details->rendered_pixbuf != NULL) {
			g_object_unref (icon_item->details->rendered_pixbuf);
		}
		icon_item->details->rendered_pixbuf = real_map_pixbuf (icon_item);
		icon_item->details->rendered_is_active = icon_item->details->is_active;
		icon_item->details->rendered_is_prelit = icon_item->details->is_prelit;
		icon_item->details->rendered_is_highlighted_for_selection = icon_item->details->is_highlighted_for_selection;
		icon_item->details->rendered_is_highlighted_for_drop = icon_item->details->is_highlighted_for_drop;
		icon_item->details->rendered_is_focused = GTK_WIDGET_HAS_FOCUS (EEL_CANVAS_ITEM (icon_item)->canvas);
	}

	g_object_ref (icon_item->details->rendered_pixbuf);

	return icon_item->details->rendered_pixbuf;
}

static void
draw_embedded_text (NautilusIconCanvasItem *icon_item,
		    GdkDrawable *drawable,
		    int x, int y)
{
	GdkGC *gc;
	GdkRectangle clip_rect;
	
	if (icon_item->details->embedded_text_layout == NULL ||
	    icon_item->details->embedded_text_rect.width == 0 ||
	    icon_item->details->embedded_text_rect.height == 0) {
		return;
	}

	gc = gdk_gc_new (drawable);

	clip_rect.x = x + icon_item->details->embedded_text_rect.x;
	clip_rect.y = y + icon_item->details->embedded_text_rect.y;
	clip_rect.width = icon_item->details->embedded_text_rect.width;
	clip_rect.height = icon_item->details->embedded_text_rect.height;
	
	gdk_gc_set_clip_rectangle  (gc, &clip_rect);

	gdk_draw_layout (drawable, gc,
			 x + icon_item->details->embedded_text_rect.x,
			 y + icon_item->details->embedded_text_rect.y,
			 icon_item->details->embedded_text_layout);
	
	g_object_unref (gc);
}

/* Draw the icon item for non-anti-aliased mode. */
static void
nautilus_icon_canvas_item_draw (EelCanvasItem *item, GdkDrawable *drawable,
				GdkEventExpose *expose)
{
	NautilusIconCanvasItem *icon_item;
	NautilusIconCanvasItemDetails *details;
	ArtIRect icon_rect, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf, *temp_pixbuf;
	GdkRectangle draw_rect, pixbuf_rect;
			
	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

        /* Draw the pixbuf. */
     	if (details->pixbuf == NULL) {
		return;
	}

	icon_rect = icon_item->details->canvas_rect;
	
	/* if the pre-lit or selection flag is set, make a pre-lit or darkened pixbuf and draw that instead */
	temp_pixbuf = map_pixbuf (icon_item);
	pixbuf_rect.x = icon_rect.x0;
	pixbuf_rect.y = icon_rect.y0;
	pixbuf_rect.width = gdk_pixbuf_get_width (temp_pixbuf);
	pixbuf_rect.height = gdk_pixbuf_get_height (temp_pixbuf);
	if (gdk_rectangle_intersect (&(expose->area), &pixbuf_rect, &draw_rect)) {
		gdk_draw_pixbuf (drawable,
				 NULL,
				 temp_pixbuf,
				 draw_rect.x - pixbuf_rect.x,
				 draw_rect.y - pixbuf_rect.y,
				 draw_rect.x,
				 draw_rect.y,
				 draw_rect.width,
				 draw_rect.height,
				 GDK_RGB_DITHER_NORMAL,
				 0,0);
	}
	g_object_unref (temp_pixbuf);

	draw_embedded_text (icon_item, drawable,  icon_rect.x0, icon_rect.y0);
	
	/* Draw the emblem pixbufs. */
	emblem_layout_reset (&emblem_layout, icon_item, icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		draw_pixbuf (emblem_pixbuf, drawable, emblem_rect.x0, emblem_rect.y0);
	}
	
	/* Draw stretching handles (if necessary). */
	draw_stretch_handles (icon_item, drawable, &icon_rect);
	
	/* Draw the label text. */
	draw_label_text (icon_item, drawable, icon_rect);
}

static PangoLayout *
create_label_layout (NautilusIconCanvasItem *item,
		     const char *text)
{
	PangoLayout *layout;
	PangoContext *context;
	PangoFontDescription *desc;
	NautilusIconContainer *container;
	EelCanvasItem *canvas_item;

	canvas_item = EEL_CANVAS_ITEM (item);

	container = NAUTILUS_ICON_CONTAINER (canvas_item->canvas);
	context = gtk_widget_get_pango_context (GTK_WIDGET (canvas_item->canvas));
	layout = pango_layout_new (context);

	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, floor (nautilus_icon_canvas_item_get_max_text_width (item)) * PANGO_SCALE);
			
	if (container->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
		pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	} else {
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	}

	pango_layout_set_spacing (layout, LABEL_LINE_SPACING);
	pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

	/* Create a font description */
	if (container->details->font) {
		desc = pango_font_description_from_string (container->details->font);
	} else {
		desc = pango_font_description_copy (pango_context_get_font_description (context));
		pango_font_description_set_size (desc,
						 pango_font_description_get_size (desc) +
						 container->details->font_size_table [container->details->zoom_level]);
	}
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	
	return layout;
}

static PangoLayout *
get_label_layout (PangoLayout **layout,
		  NautilusIconCanvasItem *item,
		  const char *text)
{
	if (*layout == NULL) {
		*layout = create_label_layout (item, text);
	}
	
	g_object_ref (*layout);
	return *layout;
}

static void
draw_label_layout (NautilusIconCanvasItem *item,
		   GdkDrawable *drawable,
		   PangoLayout *layout,
		   gboolean highlight,
		   GdkColor *label_color,
		   int x,
		   int y,
		   GdkGC *gc)
{
	if (drawable == NULL) {
		return;
	}

	if (item->details->is_renaming) {
		return;
	}

	if (!highlight && (NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas)->details->use_drop_shadows)) {
		/* draw a drop shadow */
		eel_gdk_draw_layout_with_drop_shadow (drawable, gc,
						      label_color,
						      &GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas)->style->black,
						      x, y,
						      layout);
	} else {
		gdk_draw_layout (drawable, gc,
				 x, y,
				 layout);
	}
}

/* clear the corners of the selection pixbuf by copying the corners of the passed-in pixbuf */
static void
clear_rounded_corners (GdkPixbuf *destination_pixbuf, GdkPixbuf *corner_pixbuf, int corner_size)
{
	int dest_width, dest_height, src_width, src_height;
		
	dest_width = gdk_pixbuf_get_width (destination_pixbuf);
	dest_height = gdk_pixbuf_get_height (destination_pixbuf);
	
	src_width = gdk_pixbuf_get_width (corner_pixbuf);
	src_height = gdk_pixbuf_get_height (corner_pixbuf);
	
	/* draw top left corner */
	gdk_pixbuf_copy_area (corner_pixbuf,
			      0, 0,
			      corner_size, corner_size,
			      destination_pixbuf,
			      0, 0);
	
	/* draw top right corner */
	gdk_pixbuf_copy_area (corner_pixbuf,
			      src_width - corner_size, 0,
			      corner_size, corner_size,
			      destination_pixbuf,
			      dest_width - corner_size, 0);

	/* draw bottom left corner */
	gdk_pixbuf_copy_area (corner_pixbuf,
			      0, src_height - corner_size,
			      corner_size, corner_size,
			      destination_pixbuf,
			      0, dest_height - corner_size);
	
	/* draw bottom right corner */
	gdk_pixbuf_copy_area (corner_pixbuf,
			      src_width - corner_size, src_height - corner_size,
			      corner_size, corner_size,
			      destination_pixbuf,
			      dest_width - corner_size, dest_height - corner_size);
}


/* handle events */

static int
nautilus_icon_canvas_item_event (EelCanvasItem *item, GdkEvent *event)
{
	NautilusIconCanvasItem *icon_item;
	GdkCursor *cursor;

	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		if (!icon_item->details->is_prelit) {
			icon_item->details->is_prelit = TRUE;
			eel_canvas_item_request_update (item);
			/* show a hand cursor */
			if (in_single_click_mode ()) {
				cursor = gdk_cursor_new_for_display (gdk_display_get_default(),
								     GDK_HAND2);
				gdk_window_set_cursor (((GdkEventAny *)event)->window, cursor);
				gdk_cursor_unref (cursor);
			}

			/* FIXME bugzilla.gnome.org 42473: 
			 * We should emit our own signal here,
			 * not one from the container; it could hook
			 * up to that signal and emit one of its
			 * own. Doing it this way hard-codes what
			 * "user_data" is. Also, the two signals
			 * should be separate. The "unpreview" signal
			 * does not have a return value.
			 */
			icon_item->details->is_active = nautilus_icon_container_emit_preview_signal
				(NAUTILUS_ICON_CONTAINER (item->canvas),
				 NAUTILUS_ICON_CANVAS_ITEM (item)->user_data,
				 TRUE);
		}
		return TRUE;
		
	case GDK_LEAVE_NOTIFY:
		if (icon_item->details->is_prelit 
		    || icon_item->details->is_highlighted_for_drop) {
			/* When leaving, turn of the prelight state and the
			 * higlighted for drop. The latter gets turned on
			 * by the drag&drop motion callback.
			 */
			/* FIXME bugzilla.gnome.org 42473: 
			 * We should emit our own signal here,
			 * not one from the containe; it could hook up
			 * to that signal and emit one of its
			 * ownr. Doing it this way hard-codes what
			 * "user_data" is. Also, the two signals
			 * should be separate. The "unpreview" signal
			 * does not have a return value.
			 */
			nautilus_icon_container_emit_preview_signal
				(NAUTILUS_ICON_CONTAINER (item->canvas),
				 NAUTILUS_ICON_CANVAS_ITEM (item)->user_data,
				 FALSE);			
			icon_item->details->is_prelit = FALSE;
			icon_item->details->is_active = 0;			
			icon_item->details->is_highlighted_for_drop = FALSE;
			eel_canvas_item_request_update (item);

			/* show default cursor */
			gdk_window_set_cursor (((GdkEventAny *)event)->window, NULL);
		}
		return TRUE;
		
	default:
		/* Don't eat up other events; icon container might use them. */
		return FALSE;
	}
}

static gboolean
hit_test_pixbuf (GdkPixbuf *pixbuf, ArtIRect pixbuf_location, ArtIRect probe_rect)
{
	ArtIRect relative_rect, pixbuf_rect;
	int x, y;
	guint8 *pixel;
	
	/* You can get here without a pixbuf in some strange cases. */
	if (pixbuf == NULL) {
		return FALSE;
	}
	
	/* Check to see if it's within the rectangle at all. */
	relative_rect.x0 = probe_rect.x0 - pixbuf_location.x0;
	relative_rect.y0 = probe_rect.y0 - pixbuf_location.y0;
	relative_rect.x1 = probe_rect.x1 - pixbuf_location.x0;
	relative_rect.y1 = probe_rect.y1 - pixbuf_location.y0;
	pixbuf_rect.x0 = 0;
	pixbuf_rect.y0 = 0;
	pixbuf_rect.x1 = gdk_pixbuf_get_width (pixbuf);
	pixbuf_rect.y1 = gdk_pixbuf_get_height (pixbuf);
	art_irect_intersect (&relative_rect, &relative_rect, &pixbuf_rect);
	if (art_irect_empty (&relative_rect)) {
		return FALSE;
	}

	/* If there's no alpha channel, it's opaque and we have a hit. */
	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		return TRUE;
	}
	g_assert (gdk_pixbuf_get_n_channels (pixbuf) == 4);
	
	/* Check the alpha channel of the pixel to see if we have a hit. */
	for (x = relative_rect.x0; x < relative_rect.x1; x++) {
		for (y = relative_rect.y0; y < relative_rect.y1; y++) {
			pixel = gdk_pixbuf_get_pixels (pixbuf)
				+ y * gdk_pixbuf_get_rowstride (pixbuf)
				+ x * 4;
			if (pixel[3] > 1) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

static gboolean
hit_test (NautilusIconCanvasItem *icon_item, ArtIRect canvas_rect)
{
	NautilusIconCanvasItemDetails *details;
	ArtIRect emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;
	
	details = icon_item->details;
	
	/* Quick check to see if the rect hits the icon, text or emblems at all. */
	if (!eel_art_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)
	    && (!eel_art_irect_hits_irect (details->text_rect, canvas_rect))
	    && (!eel_art_irect_hits_irect (details->emblem_rect, canvas_rect))) {
		return FALSE;
	}

	/* Check for hits in the stretch handles. */
	if (hit_test_stretch_handle (icon_item, canvas_rect)) {
		return TRUE;
	}
	
	/* Check for hit in the icon. */
	if (eel_art_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)) {
		return TRUE;
	}

	/* Check for hit in the text. */
	if (eel_art_irect_hits_irect (details->text_rect, canvas_rect)
	    && !icon_item->details->is_renaming) {
		return TRUE;
	}

	/* Check for hit in the emblem pixbufs. */
	emblem_layout_reset (&emblem_layout, icon_item, icon_item->details->canvas_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		if (hit_test_pixbuf (emblem_pixbuf, emblem_rect, canvas_rect)) {
			return TRUE;
		}	
	}
	
	return FALSE;
}

/* Point handler for the icon canvas item. */
static double
nautilus_icon_canvas_item_point (EelCanvasItem *item, double x, double y, int cx, int cy,
				 EelCanvasItem **actual_item)
{
	ArtIRect canvas_rect;

	*actual_item = item;
	canvas_rect.x0 = cx;
	canvas_rect.y0 = cy;
	canvas_rect.x1 = cx + 1;
	canvas_rect.y1 = cy + 1;
	if (hit_test (NAUTILUS_ICON_CANVAS_ITEM (item), canvas_rect)) {
		return 0.0;
	} else {
		/* This value means not hit.
		 * It's kind of arbitrary. Can we do better?
		 */
		return item->canvas->pixels_per_unit * 2 + 10;
	}
}

static void
nautilus_icon_canvas_item_translate (EelCanvasItem *item, double dx, double dy)
{
	NautilusIconCanvasItem *icon_item;
	NautilusIconCanvasItemDetails *details;
	
	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

	details->x += dx;
	details->y += dy;
}
	
/* Bounds handler for the icon canvas item. */
static void
nautilus_icon_canvas_item_bounds (EelCanvasItem *item,
				  double *x1, double *y1, double *x2, double *y2)
{
	NautilusIconCanvasItem *icon_item;
	NautilusIconCanvasItemDetails *details;
	ArtIRect icon_rect, text_rect, total_rect, emblem_rect;
	double pixels_per_unit;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;

	g_assert (x1 != NULL);
	g_assert (y1 != NULL);
	g_assert (x2 != NULL);
	g_assert (y2 != NULL);
	
	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

	if (details->bounds_cached) {
		total_rect = details->bounds_cache;
	} else {	
		measure_label_text (icon_item);
		
		/* Compute icon rectangle. */
		icon_rect.x0 = 0;
		icon_rect.y0 = 0;
		if (details->pixbuf == NULL) {
			icon_rect.x1 = icon_rect.x0;
			icon_rect.y1 = icon_rect.y0;
		} else {
			pixels_per_unit = item->canvas->pixels_per_unit;
			icon_rect.x1 = icon_rect.x0 + gdk_pixbuf_get_width (details->pixbuf) / pixels_per_unit;
			icon_rect.y1 = icon_rect.y0 + gdk_pixbuf_get_height (details->pixbuf) / pixels_per_unit;
		}
		
		/* Compute text rectangle. */
		text_rect = compute_text_rectangle (icon_item, icon_rect, FALSE);
		
		/* Compute total rectangle, adding in emblem rectangles. */
		art_irect_union (&total_rect, &icon_rect, &text_rect);
		emblem_layout_reset (&emblem_layout, icon_item, icon_rect);
		while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
			art_irect_union (&total_rect, &total_rect, &emblem_rect);
		}

		details->bounds_cache = total_rect;
		details->bounds_cached = TRUE;
	}
        
	/* Return the result. */
	*x1 = (int)details->x + total_rect.x0;
	*y1 = (int)details->y + total_rect.y0;
	*x2 = (int)details->x + total_rect.x1 + 1;
	*y2 = (int)details->y + total_rect.y1 + 1;
}

/* Get the rectangle of the icon only, in world coordinates. */
ArtDRect
nautilus_icon_canvas_item_get_icon_rectangle (const NautilusIconCanvasItem *item)
{
	ArtDRect rectangle;
	double pixels_per_unit;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), eel_art_drect_empty);

	rectangle.x0 = item->details->x;
	rectangle.y0 = item->details->y;
	
	pixbuf = item->details->pixbuf;
	
	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	rectangle.x1 = rectangle.x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
	rectangle.y1 = rectangle.y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;

	eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
			     &rectangle.x0,
			     &rectangle.y0);
	eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
			     &rectangle.x1,
			     &rectangle.y1);

	return rectangle;
}

ArtDRect
nautilus_icon_canvas_item_get_text_rectangle (const NautilusIconCanvasItem *item)
{
	/* FIXME */
	ArtIRect icon_rectangle;
	ArtIRect text_rectangle;
	ArtDRect ret;
	double pixels_per_unit;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), eel_art_drect_empty);

	icon_rectangle.x0 = item->details->x;
	icon_rectangle.y0 = item->details->y;
	
	pixbuf = item->details->pixbuf;
	
	pixels_per_unit = EEL_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	icon_rectangle.x1 = icon_rectangle.x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
	icon_rectangle.y1 = icon_rectangle.y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;

	text_rectangle = compute_text_rectangle (item, icon_rectangle, FALSE);
 
	ret.x0 = text_rectangle.x0;
	ret.y0 = text_rectangle.y0;
	ret.x1 = text_rectangle.x1;
	ret.y1 = text_rectangle.y1;

        eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                             &ret.x0,
                             &ret.y0);
        eel_canvas_item_i2w (EEL_CANVAS_ITEM (item),
                             &ret.x1,
                             &ret.y1);
 
        return ret;
}


/* Get the rectangle of the icon only, in canvas coordinates. */
static void
get_icon_canvas_rectangle (NautilusIconCanvasItem *item,
			   ArtIRect *rect)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (rect != NULL);

	eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
			item->details->x,
			item->details->y,
			&rect->x0,
			&rect->y0);
	
	pixbuf = item->details->pixbuf;
	
	rect->x1 = rect->x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf));
	rect->y1 = rect->y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf));
}

void
nautilus_icon_canvas_item_set_show_stretch_handles (NautilusIconCanvasItem *item,
						    gboolean show_stretch_handles)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (show_stretch_handles == FALSE || show_stretch_handles == TRUE);
	
	if (!item->details->show_stretch_handles == !show_stretch_handles) {
		return;
	}

	item->details->show_stretch_handles = show_stretch_handles;
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

/* Check if one of the stretch handles was hit. */
static gboolean
hit_test_stretch_handle (NautilusIconCanvasItem *item,
			 ArtIRect probe_canvas_rect)
{
	ArtIRect icon_rect;
	char *knob_filename;
	GdkPixbuf *knob_pixbuf;
	int knob_width, knob_height;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), FALSE);

	/* Make sure there are handles to hit. */
	if (!item->details->show_stretch_handles) {
		return FALSE;
	}

	/* Quick check to see if the rect hits the icon at all. */
	icon_rect = item->details->canvas_rect;
	if (!eel_art_irect_hits_irect (probe_canvas_rect, icon_rect)) {
		return FALSE;
	}

	knob_filename = nautilus_theme_get_image_path ("knob.png");
	knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename, NULL);
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);

	g_free (knob_filename);
	g_object_unref (knob_pixbuf);	
	
	/* Check for hits in the stretch handles. */
	return (probe_canvas_rect.x0 < icon_rect.x0 + knob_width
     		|| probe_canvas_rect.x1 >= icon_rect.x1 - knob_width)
		&& (probe_canvas_rect.y0 < icon_rect.y0 + knob_height
		    || probe_canvas_rect.y1 >= icon_rect.y1 - knob_height);
}

gboolean
nautilus_icon_canvas_item_hit_test_stretch_handles (NautilusIconCanvasItem *item,
						    ArtPoint world_point)
{
	ArtIRect canvas_rect;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), FALSE);
	
	eel_canvas_w2c (EEL_CANVAS_ITEM (item)->canvas,
			  world_point.x,
			  world_point.y,
			  &canvas_rect.x0,
			  &canvas_rect.y0);
	canvas_rect.x1 = canvas_rect.x0 + 1;
	canvas_rect.y1 = canvas_rect.y0 + 1;
	return hit_test_stretch_handle (item, canvas_rect);
}

/* nautilus_icon_canvas_item_hit_test_rectangle
 *
 * Check and see if there is an intersection between the item and the
 * canvas rect.
 */
gboolean
nautilus_icon_canvas_item_hit_test_rectangle (NautilusIconCanvasItem *item, ArtIRect canvas_rect)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), FALSE);

	return hit_test (item, canvas_rect);
}

const char *
nautilus_icon_canvas_item_get_editable_text (NautilusIconCanvasItem *icon_item)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (icon_item), NULL);

	return icon_item->details->editable_text;
}

void
nautilus_icon_canvas_item_set_renaming (NautilusIconCanvasItem *item, gboolean state)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
	g_return_if_fail (state == FALSE || state == TRUE);

	if (!item->details->is_renaming == !state) {
		return;
	}

	item->details->is_renaming = state;
	eel_canvas_item_request_update (EEL_CANVAS_ITEM (item));
}

double
nautilus_icon_canvas_item_get_max_text_width (NautilusIconCanvasItem *item)
{
	EelCanvasItem *canvas_item;
	
	canvas_item = EEL_CANVAS_ITEM (item);
	if (nautilus_icon_container_is_tighter_layout (NAUTILUS_ICON_CONTAINER (canvas_item->canvas))) {
		return MAX_TEXT_WIDTH_TIGHTER * canvas_item->canvas->pixels_per_unit;
	} else {
				
                if (NAUTILUS_ICON_CONTAINER (canvas_item->canvas)->details->label_position == NAUTILUS_ICON_LABEL_POSITION_BESIDE) {
			return MAX_TEXT_WIDTH_BESIDE * canvas_item->canvas->pixels_per_unit;
                } else {
			return MAX_TEXT_WIDTH_STANDARD * canvas_item->canvas->pixels_per_unit;
                }


	}

}

static G_CONST_RETURN gchar *
nautilus_icon_canvas_item_accessible_get_name (AtkObject *accessible)
{
	NautilusIconCanvasItem *item;

	item = eel_accessibility_get_gobject (accessible);
	if (!item) {
		return NULL;
	}

	return item->details->editable_text;
}

static G_CONST_RETURN gchar*
nautilus_icon_canvas_item_accessible_get_description (AtkObject *accessible)
{
	NautilusIconCanvasItem *item;

	item = eel_accessibility_get_gobject (accessible);
	if (!item) {
		return NULL;
	}

	return item->details->additional_text;
}

static AtkObject *
nautilus_icon_canvas_item_accessible_get_parent (AtkObject *accessible)
{
	NautilusIconCanvasItem *item;
	
	item = eel_accessibility_get_gobject (accessible);
	if (!item) {
		return NULL;
	}

	return gtk_widget_get_accessible (GTK_WIDGET (EEL_CANVAS_ITEM (item)->canvas));
}

static int
nautilus_icon_canvas_item_accessible_get_index_in_parent (AtkObject *accessible)
{
	NautilusIconCanvasItem *item;
	NautilusIconContainer *container;
	GList *l;
	NautilusIcon *icon;
	int i;

	item = eel_accessibility_get_gobject (accessible);
	if (!item) {
		return -1;
	}
	
	container = NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	
	l = container->details->icons;
	i = 0;
	while (l) {
		icon = l->data;
		
		if (icon->item == item) {
			return i;
		}
		
		i++;
		l = l->next;
	}

	return -1;
}

static AtkStateSet*
nautilus_icon_canvas_item_accessible_ref_state_set (AtkObject *accessible)
{
	AtkStateSet *state_set;
	NautilusIconCanvasItem *item;
	NautilusIconContainer *container;
	NautilusIcon *icon;
	GList *l;
	gboolean one_item_selected;

	state_set = ATK_OBJECT_CLASS (accessible_parent_class)->ref_state_set (accessible);

	item = eel_accessibility_get_gobject (accessible);
	container = NAUTILUS_ICON_CONTAINER (EEL_CANVAS_ITEM (item)->canvas);
	if (!item) {
		atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
	} else if (item->details->is_highlighted_as_keyboard_focus) {
		atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
	} else if (!container->details->keyboard_focus) {

		one_item_selected = FALSE;
		l = container->details->icons;
		while (l) {
			icon = l->data;
		
			if (icon->item == item) {
				if (icon->is_selected) {
					one_item_selected = TRUE;
				} else {
					break;
				}
			} else if (icon->is_selected) {
				one_item_selected = FALSE;
				break;
			}

			l = l->next;
		}

		if (one_item_selected) {
			atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);
		}
	}

	return state_set;
}

static void
nautilus_icon_canvas_item_accessible_class_init (AtkObjectClass *klass)
{
	accessible_parent_class = g_type_class_peek_parent (klass);

	klass->get_name = nautilus_icon_canvas_item_accessible_get_name;
	klass->get_description = nautilus_icon_canvas_item_accessible_get_description;
	klass->get_parent = nautilus_icon_canvas_item_accessible_get_parent;
	klass->get_index_in_parent = nautilus_icon_canvas_item_accessible_get_index_in_parent;
	klass->ref_state_set = nautilus_icon_canvas_item_accessible_ref_state_set;
}


static G_CONST_RETURN gchar * 
nautilus_icon_canvas_item_accessible_get_image_description
	(AtkImage *image)
{
	return _("file icon");
}


static void
nautilus_icon_canvas_item_accessible_get_image_size
	(AtkImage *image, 
	 gint     *width,
	 gint     *height)
{
	NautilusIconCanvasItem *item;

	item = eel_accessibility_get_gobject (ATK_OBJECT (image));

	if (!item || !item->details->pixbuf) {
		*width = *height = 0;
	} else {
		*width = gdk_pixbuf_get_width (item->details->pixbuf);
		*height = gdk_pixbuf_get_height (item->details->pixbuf);
	}
}

static void
nautilus_icon_canvas_item_accessible_get_image_position
	(AtkImage		 *image,
	 gint                    *x,
	 gint	                 *y,
	 AtkCoordType	         coord_type)
{
	NautilusIconCanvasItem *item;
	gint x_offset, y_offset, itmp;

	item = eel_accessibility_get_gobject (ATK_OBJECT (image));
	if (!item) {
		return;
	}
	if (!item->details->canvas_rect.x0 && !item->details->canvas_rect.x1) {
		return;
	} else {
		x_offset = 0;
		y_offset = 0;
		if (item->details->text_width) {
			itmp = item->details->canvas_rect.x0 -
			       item->details->text_rect.x0;
			if (itmp > x_offset) {
				x_offset = itmp;
			}
			itmp = item->details->canvas_rect.y0 -
			       item->details->text_rect.y0;
			if (itmp > y_offset) {
				y_offset = itmp;
			}
		}
		if (item->details->emblem_pixbufs) {
			itmp = item->details->canvas_rect.x0 -
			       item->details->emblem_rect.x0;
			if (itmp > x_offset) {
				x_offset = itmp;
			}
			itmp = item->details->canvas_rect.y0 -
			       item->details->emblem_rect.y0;
			if (itmp > y_offset) {
				y_offset = itmp;
			}
		}
	}
	atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
	*x += x_offset;
	*y += y_offset;
}

static gboolean
nautilus_icon_canvas_item_accessible_set_image_description
	(AtkImage    *image,
	 const gchar *description)
{
	g_warning (G_STRLOC "this api seems broken");
	return FALSE;
}

static void
nautilus_icon_canvas_item_accessible_image_interface_init (AtkImageIface *iface)
{
	iface->get_image_description = nautilus_icon_canvas_item_accessible_get_image_description;
	iface->set_image_description = nautilus_icon_canvas_item_accessible_set_image_description;
	iface->get_image_size        = nautilus_icon_canvas_item_accessible_get_image_size;
	iface->get_image_position    = nautilus_icon_canvas_item_accessible_get_image_position;
}

static gint
nautilus_icon_canvas_item_accessible_get_offset_at_point (AtkText	 *text,
                                                          gint           x,
                                                          gint           y,
                                                          AtkCoordType coords)
{
	gint real_x, real_y, real_width, real_height;
	NautilusIconCanvasItem *item;
	gint editable_height;
	gint offset = 0;
	gint index;
	PangoLayout *layout;
	PangoRectangle rect0;
	char *icon_text;
	gboolean have_editable;
	gboolean have_additional;
	gint text_offset;

	atk_component_get_extents (ATK_COMPONENT (text), &real_x, &real_y,
                                   &real_width, &real_height, coords);

	x -= real_x;
	y -= real_y; 

	item = eel_accessibility_get_gobject (ATK_OBJECT (text));

	if (item->details->pixbuf) {
		y -= gdk_pixbuf_get_height (item->details->pixbuf);
	}
	have_editable = item->details->editable_text != NULL &&
			item->details->editable_text[0] != '\0';
	have_additional = item->details->additional_text != NULL &&item->details->additional_text[0] != '\0';
	if (have_editable) {
		pango_layout_get_pixel_size (item->details->editable_text_layout, NULL, &editable_height);
		if (y >= editable_height &&
                    have_additional) {
			layout = item->details->additional_text_layout;
			icon_text = item->details->additional_text;
			y -= editable_height + LABEL_LINE_SPACING;
		} else {
			layout = item->details->editable_text_layout;
			icon_text = item->details->editable_text;
		}
	} else if (have_additional) {
		layout = item->details->additional_text_layout;
		icon_text = item->details->additional_text;
	} else {
		return 0;
	}

	text_offset = 0;
	if (have_editable) {
		pango_layout_index_to_pos (item->details->editable_text_layout, 0, &rect0);
		text_offset = PANGO_PIXELS (rect0.x);
	}
	if (have_additional) {
		gint itmp;

		pango_layout_index_to_pos (item->details->additional_text_layout, 0, &rect0);
		itmp = PANGO_PIXELS (rect0.x);
		if (itmp < text_offset) {
			text_offset = itmp;
		}
	}
	pango_layout_index_to_pos (layout, 0, &rect0);
	x += text_offset;
	if (!pango_layout_xy_to_index (layout, 
                                       x * PANGO_SCALE, 
                                       y * PANGO_SCALE, 
                                       &index, NULL)) {
		if (x < 0 || y < 0) {
			index = 0;
		} else {
			index = -1;
		}
	}
	if (index == -1) {
		offset = g_utf8_strlen (icon_text, -1);
	} else {
		offset = g_utf8_pointer_to_offset (icon_text, icon_text + index);
	}
	if (layout == item->details->additional_text_layout) {
		offset += g_utf8_strlen (item->details->editable_text, -1);	
	}
	return offset;
}

static void
nautilus_icon_canvas_item_accessible_get_character_extents (AtkText	   *text,
                                                            gint	   offset,
                                                            gint	   *x,
                                                            gint	   *y,
                                                            gint	   *width,
                                                            gint	   *height,
                                                            AtkCoordType coords)
{
	gint pos_x, pos_y;
	gint len, byte_offset;
	gint editable_height;
	gchar *icon_text;
	NautilusIconCanvasItem *item;
	PangoLayout *layout;
	PangoRectangle rect;
	PangoRectangle rect0;
	gboolean have_editable;
	gint text_offset;

	atk_component_get_position (ATK_COMPONENT (text), &pos_x, &pos_y, coords);
	item = eel_accessibility_get_gobject (ATK_OBJECT (text));

	if (item->details->pixbuf) {
		pos_y += gdk_pixbuf_get_height (item->details->pixbuf);
	}

	have_editable = item->details->editable_text != NULL &&
			item->details->editable_text[0] != '\0';
	if (have_editable) {
		len = g_utf8_strlen (item->details->editable_text, -1);
	} else {
		len = 0;
	}

	if (offset < len) {
		icon_text = item->details->editable_text;
		layout = item->details->editable_text_layout;
	} else {
		offset -= len;
		icon_text = item->details->additional_text;
		layout = item->details->additional_text_layout;
		pos_y += LABEL_LINE_SPACING;
		if (have_editable) {
			pango_layout_get_pixel_size (item->details->editable_text_layout, NULL, &editable_height);
			pos_y += editable_height;
		}
	}
	byte_offset = g_utf8_offset_to_pointer (icon_text, offset) - icon_text;
	pango_layout_index_to_pos (layout, byte_offset, &rect);
	text_offset = 0;
	if (have_editable) {
		pango_layout_index_to_pos (item->details->editable_text_layout, 0, &rect0);
		text_offset = PANGO_PIXELS (rect0.x);
	}
	if (item->details->additional_text != NULL &&
	    item->details->additional_text[0] != '\0') {
		gint itmp;

		pango_layout_index_to_pos (item->details->additional_text_layout, 0, &rect0);
		itmp = PANGO_PIXELS (rect0.x);
		if (itmp < text_offset) {
			text_offset = itmp;
		}
	}

	*x = pos_x + PANGO_PIXELS (rect.x) - text_offset;
	*y = pos_y + PANGO_PIXELS (rect.y);
	*width = PANGO_PIXELS (rect.width);
	*height = PANGO_PIXELS (rect.height);
}

static void
nautilus_icon_canvas_item_accessible_text_interface_init (AtkTextIface *iface)
{
 	iface->get_text                = eel_accessibility_text_get_text;
	iface->get_character_at_offset = eel_accessibility_text_get_character_at_offset;
        iface->get_text_before_offset  = eel_accessibility_text_get_text_before_offset;
        iface->get_text_at_offset      = eel_accessibility_text_get_text_at_offset;
   	iface->get_text_after_offset   = eel_accessibility_text_get_text_after_offset;
      	iface->get_character_count     = eel_accessibility_text_get_character_count;
	iface->get_character_extents   = nautilus_icon_canvas_item_accessible_get_character_extents;
	iface->get_offset_at_point     = nautilus_icon_canvas_item_accessible_get_offset_at_point;
}

static GType
nautilus_icon_canvas_item_accessible_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GInterfaceInfo atk_image_info = {
			(GInterfaceInitFunc)
			nautilus_icon_canvas_item_accessible_image_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc)
			nautilus_icon_canvas_item_accessible_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = eel_accessibility_create_derived_type (
			"NautilusIconCanvasItemAccessibility",
			EEL_TYPE_CANVAS_ITEM,
			nautilus_icon_canvas_item_accessible_class_init);

		if (type != G_TYPE_INVALID) {
			g_type_add_interface_static (
				type, ATK_TYPE_IMAGE, &atk_image_info);

			g_type_add_interface_static (
				type, ATK_TYPE_TEXT, &atk_text_info);

		}
	}

	return type;
}

static AtkObject *
nautilus_icon_canvas_item_accessible_create (GObject *for_object)
{
	GType type;
	AtkObject *accessible;
	NautilusIconCanvasItem *item;
	GString *item_text;

	item = NAUTILUS_ICON_CANVAS_ITEM (for_object);
	g_return_val_if_fail (item != NULL, NULL);

	type = nautilus_icon_canvas_item_accessible_get_type ();

	if (type == G_TYPE_INVALID) {
		return atk_no_op_object_new (for_object);
	}

	item_text = g_string_new (NULL);
	if (item->details->editable_text) {
        	g_string_append (item_text, item->details->editable_text);
	}
	if (item->details->additional_text) {
        	g_string_append (item_text, item->details->additional_text);
	}
	item->details->text_util = gail_text_util_new ();
	gail_text_util_text_setup (item->details->text_util,
				   item_text->str);
	g_string_free (item_text, TRUE);

	accessible = g_object_new (type, NULL);
	atk_object_set_role (accessible, ATK_ROLE_LABEL);

	return eel_accessibility_set_atk_object_return
		(for_object, accessible);
}

EEL_ACCESSIBLE_FACTORY (nautilus_icon_canvas_item_accessible_get_type (),
			"NautilusIconCanvasItemAccessibilityFactory",
			nautilus_icon_canvas_item_accessible,
			nautilus_icon_canvas_item_accessible_create);


static GailTextUtil *
nautilus_icon_canvas_item_get_text (GObject *text)
{
	return NAUTILUS_ICON_CANVAS_ITEM (text)->details->text_util;
}

static void
nautilus_icon_canvas_item_text_interface_init (EelAccessibleTextIface *iface)
{
	iface->get_text = nautilus_icon_canvas_item_get_text;
}

/* Class initialization function for the icon canvas item. */
static void
nautilus_icon_canvas_item_class_init (NautilusIconCanvasItemClass *class)
{
	GObjectClass *object_class;
	EelCanvasItemClass *item_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	item_class = EEL_CANVAS_ITEM_CLASS (class);

	object_class->finalize = nautilus_icon_canvas_item_finalize;
	object_class->set_property = nautilus_icon_canvas_item_set_property;
	object_class->get_property = nautilus_icon_canvas_item_get_property;

        g_object_class_install_property (
		object_class,
		PROP_EDITABLE_TEXT,
		g_param_spec_string ("editable_text",
				     _("editable text"),
				     _("the editable label"),
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_ADDITIONAL_TEXT,
		g_param_spec_string ("additional_text",
				     _("additional text"),
				     _("some more text"),
				     "", G_PARAM_READWRITE));

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_SELECTION,
		g_param_spec_boolean ("highlighted_for_selection",
				      _("highlighted for selection"),
				      _("whether we are highlighted for a selection"),
				      FALSE, G_PARAM_READWRITE)); 

        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
		g_param_spec_boolean ("highlighted_as_keyboard_focus",
				      _("highlighted as keyboard focus"),
				      _("whether we are highlighted to render keyboard focus"),
				      FALSE, G_PARAM_READWRITE)); 


        g_object_class_install_property (
		object_class,
		PROP_HIGHLIGHTED_FOR_DROP,
		g_param_spec_boolean ("highlighted_for_drop",
				      _("highlighted for drop"),
				      _("whether we are highlighted for a D&D drop"),
				      FALSE, G_PARAM_READWRITE)); 

	item_class->update = nautilus_icon_canvas_item_update;
	item_class->draw = nautilus_icon_canvas_item_draw;
	item_class->point = nautilus_icon_canvas_item_point;
	item_class->translate = nautilus_icon_canvas_item_translate;
	item_class->bounds = nautilus_icon_canvas_item_bounds;
	item_class->event = nautilus_icon_canvas_item_event;	

	EEL_OBJECT_SET_FACTORY (NAUTILUS_TYPE_ICON_CANVAS_ITEM,
				nautilus_icon_canvas_item_accessible);
}

GType
nautilus_icon_canvas_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (NautilusIconCanvasItemClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) nautilus_icon_canvas_item_class_init,
			NULL,		/* class_finalize */
			NULL,               /* class_data */
			sizeof (NautilusIconCanvasItem),
			0,                  /* n_preallocs */
			(GInstanceInitFunc) nautilus_icon_canvas_item_init,
		};
		static const GInterfaceInfo eel_text_info = {
			(GInterfaceInitFunc)
			nautilus_icon_canvas_item_text_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static
			(EEL_TYPE_CANVAS_ITEM, "NautilusIconCanvasItem", &info, 0);

		g_type_add_interface_static
			(type, EEL_TYPE_ACCESSIBLE_TEXT, &eel_text_info);
	}

	return type;
}
