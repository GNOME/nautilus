/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-background.c: Object for the background of a widget.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include <ctype.h>

#include "nautilus-background.h"

#include <math.h>

#include <stdio.h>
#include "nautilus-glib-extensions.h"

#include <gtk/gtksignal.h>
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-background-canvas-group.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-string.h"

#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>

#include <libart_lgpl/art_rgb.h>

static void nautilus_background_initialize_class (gpointer            klass);
static void nautilus_background_initialize       (gpointer            object,
						  gpointer            klass);
static void nautilus_background_destroy          (GtkObject          *object);
static void nautilus_background_draw_flat_box    (GtkStyle           *style,
						  GdkWindow          *window,
						  GtkStateType        state_type,
						  GtkShadowType       shadow_type,
						  GdkRectangle       *area,
						  GtkWidget          *widget,
						  char               *detail,
						  int                 x,
						  int                 y,
						  int                 width,
						  int                 height);
static void nautilus_background_real_reset       (NautilusBackground *background);
static void nautilus_background_start_loading_image (NautilusBackground *background);
static gboolean nautilus_background_is_image_load_in_progress (NautilusBackground *background);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackground, nautilus_background, GTK_TYPE_OBJECT)

enum {
	APPEARANCE_CHANGED,
	SETTINGS_CHANGED,
	IMAGE_LOADING_DONE,
	RESET,
	LAST_SIGNAL
};

#define	RESET_BACKGROUND_MAGIC_IMAGE_NAME "reset.png"

static guint signals[LAST_SIGNAL];

struct NautilusBackgroundDetails {
	char *color;
	
	int gradient_num_pixels;
	guchar *gradient_buffer;
	gboolean gradient_is_horizontal;

	gboolean is_solid_color;
	GdkColor solid_color;
	
	char *image_uri;
	GdkPixbuf *image;
	int image_width_unscaled;
	int image_height_unscaled;
	NautilusPixbufLoadHandle *load_image_handle;
	gboolean combine_mode;
	NautilusBackgroundImagePlacement image_placement;

	/* The image_rect is the area (canvas relative) the image will cover.
	 * Note: image_rect_width/height are not always the same as the image's
	 * width and height - e.g. if the image is tiled the image rect covers
	 * the whole background.
	 */
	int image_rect_x;
	int image_rect_y;
	int image_rect_width;
	int image_rect_height;
};

static void
nautilus_background_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	NautilusBackgroundClass *background_class;

	object_class = GTK_OBJECT_CLASS (klass);
	background_class = NAUTILUS_BACKGROUND_CLASS (klass);

	signals[APPEARANCE_CHANGED] =
		gtk_signal_new ("appearance_changed",
				GTK_RUN_LAST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass,
						   appearance_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	signals[SETTINGS_CHANGED] =
		gtk_signal_new ("settings_changed",
				GTK_RUN_LAST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass,
						   settings_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	signals[IMAGE_LOADING_DONE] =
		gtk_signal_new ("image_loading_done",
				GTK_RUN_LAST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass,
						   image_loading_done),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE,
				1,
				GTK_TYPE_BOOL);
	signals[RESET] =
		gtk_signal_new ("reset",
				GTK_RUN_LAST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass,
						   reset),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = nautilus_background_destroy;
	background_class->reset = nautilus_background_real_reset;
}

static void
nautilus_background_initialize (gpointer object, gpointer klass)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND(object);

	background->details = g_new0 (NautilusBackgroundDetails, 1);
}

/* The safe way to clear an image from a background is:
 * 		nautilus_background_set_image_uri (NULL);
 * This fn is a private utility - it does NOT clear
 * the details->image_uri setting.
 */
static void
nautilus_background_remove_current_image (NautilusBackground *background)
{
	if (background->details->image != NULL) {
		gdk_pixbuf_unref (background->details->image);
		background->details->image = NULL;
	}
}

static void
nautilus_background_destroy (GtkObject *object)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (object);

	nautilus_cancel_gdk_pixbuf_load (background->details->load_image_handle);
	background->details->load_image_handle = NULL;

	g_free (background->details->color);
	g_free (background->details->gradient_buffer);
	g_free (background->details->image_uri);
	nautilus_background_remove_current_image (background);
	g_free (background->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* handle the combine mode getting and setting */

gboolean
nautilus_background_get_combine_mode (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background),
			      FALSE);

	return background->details->combine_mode;
}

void
nautilus_background_set_combine_mode (NautilusBackground *background, gboolean new_combine_mode)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (new_combine_mode == FALSE || new_combine_mode == TRUE);

	if (new_combine_mode != background->details->combine_mode) {
		background->details->combine_mode = new_combine_mode;
		
		gtk_signal_emit (GTK_OBJECT (background),
			 signals[SETTINGS_CHANGED]);
		gtk_signal_emit (GTK_OBJECT (background),
			 signals[APPEARANCE_CHANGED]);
	}
}

NautilusBackgroundImagePlacement
nautilus_background_get_image_placement (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NAUTILUS_BACKGROUND_TILED);

	return background->details->image_placement;
}

void
nautilus_background_set_image_placement (NautilusBackground *background,
					 NautilusBackgroundImagePlacement new_placement)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (new_placement != background->details->image_placement) {

		if (nautilus_background_is_image_load_in_progress (background)) {
			/* We try to smart and keep using the current image for updates
			 * while a new image is loading. However, changing the placement
			 * foils these plans.
			 */
			nautilus_background_remove_current_image (background);
		}

		background->details->image_placement = new_placement;
		
		gtk_signal_emit (GTK_OBJECT (background),
			 signals[SETTINGS_CHANGED]);
		gtk_signal_emit (GTK_OBJECT (background),
			 signals[APPEARANCE_CHANGED]);
	}
}

NautilusBackground *
nautilus_background_new (void)
{
	return NAUTILUS_BACKGROUND (gtk_object_new (NAUTILUS_TYPE_BACKGROUND, NULL));
}
 
static void
reset_cached_color_info (NautilusBackground *background)
{
	background->details->gradient_num_pixels = 0;
	
	background->details->is_solid_color = !nautilus_gradient_is_gradient (background->details->color);
	
	if (background->details->is_solid_color) {
		g_free (background->details->gradient_buffer);
		background->details->gradient_buffer = NULL;
		nautilus_gdk_color_parse_with_white_default (background->details->color, &background->details->solid_color);
	} else {
		/* If color is still a gradient, don't g_free the buffer, nautilus_background_ensure_gradient_buffered
		 * uses g_realloc to try to reuse it.
		 */
		background->details->gradient_is_horizontal = nautilus_gradient_is_horizontal (background->details->color);
	}
}

static void
nautilus_background_ensure_gradient_buffered (NautilusBackground *background, int dest_width, int dest_height)
{
	int num_pixels;

	guchar *buff_ptr;
	guchar *buff_limit;

	GdkColor cur_color;

	char* color_spec;
	const char* spec_ptr;

	if (background->details->is_solid_color) {
		return;
	}

	num_pixels = background->details->gradient_is_horizontal ? dest_width : dest_height;

	if (background->details->gradient_num_pixels == num_pixels) {
		return;
	}

	background->details->gradient_num_pixels = num_pixels;
	background->details->gradient_buffer = g_realloc (background->details->gradient_buffer, num_pixels * 3);
	
	buff_ptr   = background->details->gradient_buffer;
	buff_limit = background->details->gradient_buffer + num_pixels * 3;

	spec_ptr = background->details->color;
	
	color_spec = nautilus_gradient_parse_one_color_spec (spec_ptr, NULL, &spec_ptr);
	nautilus_gdk_color_parse_with_white_default (color_spec, &cur_color);
	g_free (color_spec);

	while (spec_ptr != NULL && buff_ptr < buff_limit) {
		int percent;
		int fill_pos;
		int fill_width;
		int dr, dg, db;
		guchar *fill_limit;
		GdkColor new_color;
	
		color_spec = nautilus_gradient_parse_one_color_spec (spec_ptr, &percent, &spec_ptr);
		nautilus_gdk_color_parse_with_white_default (color_spec, &new_color);
		g_free (color_spec);

		dr = new_color.red   - cur_color.red;
		dg = new_color.green - cur_color.green;
		db = new_color.blue  - cur_color.blue;

		fill_pos   = 0;
		fill_width = (percent * num_pixels) / 100;
		fill_limit = MIN (buff_ptr + fill_width * 3, buff_limit);
		
		while (buff_ptr < fill_limit) {
			*buff_ptr++ = (cur_color.red   + (dr * fill_pos) / fill_width) >> 8;
			*buff_ptr++ = (cur_color.green + (dg * fill_pos) / fill_width) >> 8;
			*buff_ptr++ = (cur_color.blue  + (db * fill_pos) / fill_width) >> 8;
			++fill_pos;
		}

		cur_color = new_color;
	}

	/* fill in the remainder */
	art_rgb_fill_run (buff_ptr, cur_color.red, cur_color.green, cur_color.blue, (buff_limit - buff_ptr) / 3);	
}

static void
canvas_gradient_helper_v (const GnomeCanvasBuf *buf, const art_u8 *gradient_buff)
{
	int width  = buf->rect.x1 - buf->rect.x0;
	int height = buf->rect.y1 - buf->rect.y0;

	art_u8 *dst       = buf->buf;
	art_u8 *dst_limit = buf->buf + height * buf->buf_rowstride;

	gradient_buff += buf->rect.y0 * 3;
	
	while (dst < dst_limit) {
		art_u8 r = *gradient_buff++;
		art_u8 g = *gradient_buff++;
		art_u8 b = *gradient_buff++;
 		art_rgb_fill_run (dst, r, g, b, width);
		dst += buf->buf_rowstride;
	}
}

static void
canvas_gradient_helper_h (const GnomeCanvasBuf *buf, const art_u8 *gradient_buff)
{
	int width  = buf->rect.x1 - buf->rect.x0;
	int height = buf->rect.y1 - buf->rect.y0;

	art_u8 *dst       = buf->buf;
	art_u8 *dst_limit = buf->buf + height * buf->buf_rowstride;

	int copy_bytes_per_row = width * 3;

	gradient_buff += buf->rect.x0 * 3;
	
	while (dst < dst_limit) {
 		memcpy (dst, gradient_buff, copy_bytes_per_row);
		dst += buf->buf_rowstride;
	}
}

static void
fill_canvas_from_gradient_buffer (const GnomeCanvasBuf *buf, const NautilusBackground *background)
{
	g_return_if_fail (background->details->gradient_buffer != NULL);

	/* FIXME bugzilla.eazel.com 415: This hack is needed till we fix background
	 * scolling.
	 *
	 * I.e. currently you can scroll off the end of the gradient - and we
	 * handle this by pegging it the the last rgb value.
	 *
	 * It might be needed permanently after depending on how this is fixed.
	 * If we tie gradients to the boundry of icon placement (as opposed to
	 * window size) then when dragging an icon you might scroll off the
	 * end of the gradient - which will get recaluated after the drop.
	 */
	if (background->details->gradient_is_horizontal) {
		if (buf->rect.x1 > background->details->gradient_num_pixels) {
			art_u8 *rgb888 = background->details->gradient_buffer + (background->details->gradient_num_pixels - 1) * 3;
			GnomeCanvasBuf gradient = *buf;
			GnomeCanvasBuf overflow = *buf;
			gradient.rect.x1 =  gradient.rect.x0 < background->details->gradient_num_pixels ? background->details->gradient_num_pixels : gradient.rect.x0;
			overflow.buf += (gradient.rect.x1 - gradient.rect.x0) * 3;
			overflow.rect.x0 = gradient.rect.x1;
			nautilus_gnome_canvas_fill_rgb (&overflow, rgb888[0], rgb888[1], rgb888[2]);
			canvas_gradient_helper_h (&gradient, background->details->gradient_buffer);
			return;
		}
	} else {
		if (buf->rect.y1 > background->details->gradient_num_pixels) {
			art_u8 *rgb888 = background->details->gradient_buffer + (background->details->gradient_num_pixels - 1) * 3;
			GnomeCanvasBuf gradient = *buf;
			GnomeCanvasBuf overflow = *buf;
			gradient.rect.y1 = gradient.rect.y0 < background->details->gradient_num_pixels ? background->details->gradient_num_pixels : gradient.rect.y0;
			overflow.buf += (gradient.rect.y1 - gradient.rect.y0) * gradient.buf_rowstride;
			overflow.rect.y0 = gradient.rect.y1;
			nautilus_gnome_canvas_fill_rgb (&overflow, rgb888[0], rgb888[1], rgb888[2]);
			canvas_gradient_helper_v (&gradient, background->details->gradient_buffer);
			return;
		}
	}

	(background->details->gradient_is_horizontal ? canvas_gradient_helper_h : canvas_gradient_helper_v) (buf, background->details->gradient_buffer);
}

static void
drawable_gradient_helper_v (GdkDrawable *drawable, GdkGC *gc, const GdkRectangle *rect, art_u8 *gradient_buff)
{
	int y;

	gradient_buff += rect->y * 3;

	for (y = 0; y < rect->height; ++y) {
		art_u8 r = *gradient_buff++;
		art_u8 g = *gradient_buff++;
		art_u8 b = *gradient_buff++;
		GdkColor color = {0, r << 8, g << 8, b << 8};
		gdk_colormap_alloc_color (gdk_colormap_get_system (), &color, FALSE, TRUE);
		gdk_gc_set_foreground (gc, &color);
		gdk_draw_line (drawable, gc, 0, y, rect->width, y);
	}	
}

static void
drawable_gradient_helper_h (GdkDrawable *drawable, GdkGC *gc, const GdkRectangle *rect, art_u8 *gradient_buff)
{
	gradient_buff += rect->x * 3;

	/* We can do the fill with a single call to gdk_draw_rgb_image_dithalign
	 * because we pass a rowstride of zero. Zero rowstride make gradient_buff
	 * act like a pixbuf of infinite height - with all identical rows.
	 */
	gdk_draw_rgb_image_dithalign (drawable, gc, 0, 0, rect->width, rect->height, GDK_RGB_DITHER_NONE, gradient_buff, 0, 0, 0);
}

static void
fill_drawable_from_gradient_buffer (GdkDrawable *drawable, GdkGC *gc,
			  	    int drawable_x, int drawable_y,
			  	    int drawable_width, int drawable_height,
			 	    NautilusBackground *background)

{
	GdkRectangle rect = {drawable_x, drawable_y, drawable_width, drawable_height};
	
	g_return_if_fail (background->details->gradient_buffer != NULL);

	/* FIXME bugzilla.eazel.com 415: This hack is needed till we fix background
	 * scolling.
	 *
	 * I.e. currently you can scroll off the end of the gradient - and we
	 * handle this by pegging it the the last rgb value.
	 *
	 * It might be needed permanently after depending on how this is fixed.
	 * If we tie gradients to the boundry of icon placement (as opposed to
	 * window size) then when dragging an icon you might scroll off the
	 * end of the gradient - which will get recaluated after the drop.
	 */
	if (background->details->gradient_is_horizontal) {
		int drawable_end = rect.x + rect.width;
		if (drawable_end > background->details->gradient_num_pixels) {
			art_u8 *rgb888 = background->details->gradient_buffer + (background->details->gradient_num_pixels - 1) * 3;
			GdkRectangle overflow;
			rect.width =  rect.x < background->details->gradient_num_pixels ? background->details->gradient_num_pixels - rect.x : 0;
			/* overflow is in drawable relative coords (as opposed to canvas relative) */
			overflow.x = rect.width;
			overflow.y = 0;
			overflow.width = drawable_width - rect.width;
			overflow.height = drawable_height;
			nautilus_fill_rectangle_with_color (drawable, gc, &overflow, nautilus_rgb8_to_rgb (rgb888[0], rgb888[1], rgb888[2]));
		}
	} else {
		int drawable_end = rect.y + rect.height;
		if (drawable_end > background->details->gradient_num_pixels) {
			art_u8 *rgb888 = background->details->gradient_buffer + (background->details->gradient_num_pixels - 1) * 3;
			GdkRectangle overflow;
			rect.height = rect.y < background->details->gradient_num_pixels ? background->details->gradient_num_pixels - rect.y : 0;
			/* overflow is in drawable relative coords (as opposed to canvas relative) */
			overflow.x = 0;
			overflow.y = rect.height;
			overflow.width = drawable_width;
			overflow.height = drawable_height - rect.height;
			nautilus_fill_rectangle_with_color (drawable, gc, &overflow, nautilus_rgb8_to_rgb (rgb888[0], rgb888[1], rgb888[2]));
		}
	}

	(background->details->gradient_is_horizontal ? drawable_gradient_helper_h : drawable_gradient_helper_v)
		(drawable, gc, &rect, background->details->gradient_buffer);
}

/* Fills in an update rect/drawable from a pixbuf by calling gdk_pixbuf_render_to_drawable
 * with the minimum width/height in order to minimize overhead. I.e. this wrapper handles
 * the clipping rather than sending all the data to the x-server and letting it do it.
 * 
 * x0,y0,x1,y1 are all in canvas coords.
 */
static void
update_drawable_with_pixbuf (GdkDrawable *dst, GdkGC *gc,
			     int dst_x0, int dst_y0, int dst_x1, int dst_y1,
			     GdkPixbuf *src,
			     int src_x0, int src_y0, int src_x1, int src_y1)
{
	if (src_x1 > dst_x0 && src_y1 > dst_y0 && src_x0 < dst_x1  && src_y0 < dst_y1) {
		int src_offset_x;
		int src_offset_y;
		int dst_offset_x;
		int dst_offset_y;
		int copy_x0;
		int copy_y0;
		int copy_x1;
		int copy_y1;
		
		if (src_x0 > dst_x0) {
			src_offset_x = 0;
			dst_offset_x = src_x0 - dst_x0;
			copy_x0 = src_x0;
		} else {
			src_offset_x = dst_x0 - src_x0;
			dst_offset_x = 0;
			copy_x0 = dst_x0;
		}

		copy_x1 = MIN (src_x1, dst_x1);
		
		if (src_y0 > dst_y0) {
			src_offset_y = 0;
			dst_offset_y = src_y0 - dst_y0;
			copy_y0 = src_y0;
		} else {
			src_offset_y = dst_y0 - src_y0;
			dst_offset_y = 0;
			copy_y0 = dst_y0;
		}

		copy_y1 = MIN (src_y1, dst_y1);
		
		gdk_pixbuf_render_to_drawable (src, dst, gc,
									   src_offset_x, src_offset_y,
									   dst_offset_x, dst_offset_y,
									   copy_x1 - copy_x0, copy_y1 - copy_y0,
									   GDK_RGB_DITHER_NONE, 0, 0);
	}
}

/* fill the canvas buffer with a tiled pixbuf */
static void
draw_pixbuf_tiled (GdkPixbuf *pixbuf, GdkDrawable *drawable, GdkGC *gc, int drawable_x, int drawable_y, int drawable_right, int drawable_bottom)
{
	int x, y;
	int start_x, start_y;
	int tile_width, tile_height;
	
	tile_width = gdk_pixbuf_get_width (pixbuf);
	tile_height = gdk_pixbuf_get_height (pixbuf);
	
	start_x = drawable_x - (drawable_x % tile_width);
	start_y = drawable_y - (drawable_y % tile_height);

	for (y = start_y; y < drawable_bottom; y += tile_height) {
		for (x = start_x; x < drawable_right; x += tile_width) {
			update_drawable_with_pixbuf (drawable, gc,
										 drawable_x, drawable_y, drawable_right, drawable_bottom,
										 pixbuf,
										 x, y, x + tile_width, y + tile_height);
		}
	}
}

/* Initializes a pseudo-canvas buf so canvas drawing routines can be used to draw into a pixbuf.
 */
static void
canvas_buf_from_pixbuf (GnomeCanvasBuf* buf, GdkPixbuf *pixbuf, int x, int y, int width, int height)
{
	buf->buf =  gdk_pixbuf_get_pixels (pixbuf);
	buf->buf_rowstride =  gdk_pixbuf_get_rowstride (pixbuf);
	buf->rect.x0 = x;
	buf->rect.y0 = y;
	buf->rect.x1 = x + width;
	buf->rect.y1 = y + height;
	buf->bg_color = 0xFFFFFFFF;
	buf->is_bg = TRUE;
	buf->is_buf = FALSE;
}

static gboolean
nautilus_background_image_totally_obscures (NautilusBackground *background)
{
	if (background->details->image == NULL || gdk_pixbuf_get_has_alpha (background->details->image)) {
		return FALSE;
	}

	switch (background->details->image_placement) {
	case NAUTILUS_BACKGROUND_TILED:
	case NAUTILUS_BACKGROUND_SCALED:
		return TRUE;
	default:
		g_assert_not_reached ();
		/* fall through */
	case NAUTILUS_BACKGROUND_CENTERED:
	case NAUTILUS_BACKGROUND_SCALED_ASPECT:
		/* It's possible that the image totally obscures in this case, but we don't
		 * have enough info to know. So we guess conservatively.
		 */
		return FALSE;
	}
}

static void
nautilus_background_ensure_image_scaled (NautilusBackground *background, int dest_width, int dest_height)
{
	if (background->details->image == NULL) {
		background->details->image_rect_x = 0;
		background->details->image_rect_y = 0;
		background->details->image_rect_width = 0;
		background->details->image_rect_height = 0;
	} else if (!nautilus_background_is_image_load_in_progress (background)){
		int image_width;
		int image_height;
		int fit_width;
		int fit_height;
		gboolean cur_scaled;
		gboolean reload_image;
		GdkPixbuf *scaled_pixbuf;

		image_width = gdk_pixbuf_get_width (background->details->image);
		image_height = gdk_pixbuf_get_height (background->details->image);
		cur_scaled = image_width != background->details->image_width_unscaled ||
			     image_height != background->details->image_height_unscaled;
		reload_image = FALSE;
		scaled_pixbuf = NULL;
	
		switch (background->details->image_placement) {
		case NAUTILUS_BACKGROUND_TILED:
		case NAUTILUS_BACKGROUND_CENTERED:
			reload_image = cur_scaled;
			break;
		case NAUTILUS_BACKGROUND_SCALED:
			if (image_width != dest_width || image_height != dest_height) {
				if (cur_scaled) {
					reload_image = TRUE;
				} else {
					scaled_pixbuf = gdk_pixbuf_scale_simple (background->details->image, dest_width, dest_height, GDK_INTERP_BILINEAR);
					gdk_pixbuf_unref (background->details->image);
					background->details->image = scaled_pixbuf;
					image_width = gdk_pixbuf_get_width (scaled_pixbuf);
					image_height = gdk_pixbuf_get_height (scaled_pixbuf);
				}
			}
			break;
		case NAUTILUS_BACKGROUND_SCALED_ASPECT:
			nautilus_gdk_scale_to_fit_factor (background->details->image_width_unscaled,
							  background->details->image_height_unscaled,
							  dest_width, dest_height,
							  &fit_width, &fit_height);
			if (image_width != fit_width || image_height != fit_height) {
				if (cur_scaled) {
					reload_image = TRUE;
				} else {
					scaled_pixbuf = nautilus_gdk_pixbuf_scale_to_fit (background->details->image, dest_width, dest_height);
					gdk_pixbuf_unref (background->details->image);
					background->details->image = scaled_pixbuf;
					image_width = gdk_pixbuf_get_width (scaled_pixbuf);
					image_height = gdk_pixbuf_get_height (scaled_pixbuf);
				}
			}
			break;
		}

		if (reload_image) {
			gdk_pixbuf_unref (background->details->image);
			background->details->image = NULL;
			nautilus_background_start_loading_image (background);
			background->details->image_rect_x = 0;
			background->details->image_rect_y = 0;
			background->details->image_rect_width = 0;
			background->details->image_rect_height = 0;
		} else if (background->details->image_placement == NAUTILUS_BACKGROUND_TILED) {
			background->details->image_rect_x = 0;
			background->details->image_rect_y = 0;
			background->details->image_rect_width = dest_width;
			background->details->image_rect_height = dest_height;
		} else {
			background->details->image_rect_x = (dest_width - image_width) / 2;
			background->details->image_rect_y = (dest_height - image_height) / 2;
			background->details->image_rect_width = image_width;
			background->details->image_rect_height = image_height;
		}
	}
}

void
nautilus_background_pre_draw (NautilusBackground *background, int entire_width, int entire_height)
{
	nautilus_background_ensure_image_scaled (background, entire_width, entire_height);
	nautilus_background_ensure_gradient_buffered (background, entire_width, entire_height);
}
 
/* This routine is for gdk style rendering, which doesn't naturally
 * support transparency, so we draw into a pixbuf offscreen if
 * necessary.
 */
void
nautilus_background_draw (NautilusBackground *background,
			  GdkDrawable *drawable, GdkGC *gc,
			  int drawable_x, int drawable_y,
			  int drawable_width, int drawable_height)
{
	int drawable_right;
	int drawable_bottom;
	int image_rect_right;
	int image_rect_bottom;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (background->details->combine_mode) {
		GdkPixbuf *pixbuf;
		GnomeCanvasBuf buffer;
		
		/* allocate a pixbuf the size of the rectangle */
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, drawable_width, drawable_height);
		
		/* contrive a CanvasBuf structure to point to it */
		canvas_buf_from_pixbuf (&buffer, pixbuf, drawable_x, drawable_y, drawable_width, drawable_height);
		
		/* invoke the anti-aliased code to do the work */
		nautilus_background_draw_aa (background, &buffer);
		
		/* blit the pixbuf to the drawable */
		gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
					       0, 0,
					       0, 0,
					       drawable_width, drawable_height,
					       GDK_RGB_DITHER_NONE, 0, 0);
		
		/* free things up and we're done */
		gdk_pixbuf_unref (pixbuf);
		
		return;
	}

	drawable_right  = drawable_x + drawable_width;
	drawable_bottom = drawable_y + drawable_height;
	
	image_rect_right  = background->details->image_rect_x + background->details->image_rect_width;
	image_rect_bottom = background->details->image_rect_y + background->details->image_rect_height;
	
	if (!background->details->image ||
	    drawable_x  < background->details->image_rect_x ||
	    drawable_y  < background->details->image_rect_y ||
	    drawable_right > image_rect_right ||
	    drawable_bottom > image_rect_bottom) {
		if (background->details->is_solid_color) {
			GdkRectangle rect = {0 , 0, drawable_width, drawable_height};
			nautilus_fill_rectangle_with_color (drawable, gc, &rect, nautilus_gdk_color_to_rgb (&background->details->solid_color));
		} else {
			fill_drawable_from_gradient_buffer (drawable, gc, drawable_x, drawable_y, drawable_width, drawable_height, background);
		}
	}

	if (background->details->image != NULL) {
		switch (background->details->image_placement) {
		case NAUTILUS_BACKGROUND_TILED:
			draw_pixbuf_tiled (background->details->image, drawable, gc, drawable_x, drawable_y, drawable_right, drawable_bottom);
			break;
		case NAUTILUS_BACKGROUND_CENTERED:
		case NAUTILUS_BACKGROUND_SCALED:
		case NAUTILUS_BACKGROUND_SCALED_ASPECT:
			/* Since the image has already been scaled, all these cases can be treated identically.
			 */
			update_drawable_with_pixbuf (drawable, gc,
						     drawable_x, drawable_y,
						     drawable_right, drawable_bottom,
						     background->details->image,
						     background->details->image_rect_x, background->details->image_rect_y,
						     image_rect_right, image_rect_bottom);
			break;
		}
	}
}

void
nautilus_background_draw_to_drawable (NautilusBackground *background,
				      GdkDrawable *drawable, GdkGC *gc,
				      int drawable_x, int drawable_y,
				      int drawable_width, int drawable_height,
				      int entire_width, int entire_height)
{
	nautilus_background_pre_draw (background, entire_width, entire_height);
	nautilus_background_draw (background, drawable, gc, drawable_x, drawable_y, drawable_width, drawable_height);
}


/* fill the canvas buffer with a tiled pixbuf */
static void
draw_pixbuf_tiled_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer)
{
	int x, y;
	int start_x, start_y;
	int tile_width, tile_height;
	
	tile_width = gdk_pixbuf_get_width (pixbuf);
	tile_height = gdk_pixbuf_get_height (pixbuf);
	
	start_x = buffer->rect.x0 - (buffer->rect.x0 % tile_width);
	start_y = buffer->rect.y0 - (buffer->rect.y0 % tile_height);

	for (y = start_y; y < buffer->rect.y1; y += tile_height) {
		for (x = start_x; x < buffer->rect.x1; x += tile_width) {
			nautilus_gnome_canvas_draw_pixbuf (buffer, pixbuf, x, y);
		}
	}
}

/* draw the background on the anti-aliased canvas */
void
nautilus_background_draw_aa (NautilusBackground *background, GnomeCanvasBuf *buffer)
{	
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	/* If the image has alpha - we always draw the gradient behind it.
	 * In principle, we could do better by having already drawn gradient behind
	 * the scaled image. However, this would add a significant amount of
	 * complexity to our image scaling/caching logic. I.e. it would tie the
	 * scaled image to a location on the screen because it holds a gradient.
	 * This is especially problematic for tiled images with alpha.
	 */
	if (!background->details->image ||
	     gdk_pixbuf_get_has_alpha (background->details->image) ||
	     buffer->rect.x0  < background->details->image_rect_x ||
	     buffer->rect.y0  < background->details->image_rect_y ||
	     buffer->rect.x1  > (background->details->image_rect_x + background->details->image_rect_width) ||
	     buffer->rect.y1  > (background->details->image_rect_y + background->details->image_rect_height)) {
		if (background->details->is_solid_color) {
			nautilus_gnome_canvas_fill_rgb (buffer,
							background->details->solid_color.red >> 8,
							background->details->solid_color.green >> 8,
							background->details->solid_color.blue >> 8);
		} else {
			fill_canvas_from_gradient_buffer (buffer, background);
		}
	}

	if (background->details->image != NULL) {
		switch (background->details->image_placement) {
		case NAUTILUS_BACKGROUND_TILED:
			draw_pixbuf_tiled_aa (background->details->image, buffer);
			break;
		default:
			g_assert_not_reached ();
			/* fall through */
		case NAUTILUS_BACKGROUND_CENTERED:
		case NAUTILUS_BACKGROUND_SCALED:
		case NAUTILUS_BACKGROUND_SCALED_ASPECT:
			/* Since the image has already been scaled, all these cases
			 * can be treated identically.
			 */
			nautilus_gnome_canvas_draw_pixbuf (buffer,
							   background->details->image,
							   background->details->image_rect_x,
							   background->details->image_rect_y);
			break;
		}
	}
					
	buffer->is_bg  = FALSE;
	buffer->is_buf = TRUE;
}

void
nautilus_background_draw_to_canvas (NautilusBackground *background,
				    GnomeCanvasBuf *buffer,
				    int entire_width,
				    int entire_height)
{
	nautilus_background_pre_draw (background, entire_width, entire_height);
	nautilus_background_draw_aa (background, buffer);
}

char *
nautilus_background_get_color (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NULL);

	return g_strdup (background->details->color);
}

char *
nautilus_background_get_image_uri (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NULL);

	return g_strdup (background->details->image_uri);
}

static gboolean
nautilus_background_set_color_no_emit (NautilusBackground *background,
			       const char *color)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);

	if (nautilus_strcmp (background->details->color, color) == 0) {
		return FALSE;
	}
	g_free (background->details->color);
	background->details->color = g_strdup (color);
	reset_cached_color_info (background);

	return TRUE;
}

void
nautilus_background_set_color (NautilusBackground *background,
			       const char *color)
{
	if (nautilus_background_set_color_no_emit (background, color)) {
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
		if (!nautilus_background_image_totally_obscures (background)) {
			gtk_signal_emit (GTK_OBJECT (background), signals[APPEARANCE_CHANGED]);
		}
	}
}

static void
nautilus_background_load_image_callback (GnomeVFSResult error,
					 GdkPixbuf *pixbuf,
					 gpointer callback_data)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (callback_data);

	background->details->load_image_handle = NULL;

	nautilus_background_remove_current_image (background);

	/* Just ignore errors. */
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
		background->details->image = pixbuf;
		background->details->image_width_unscaled = gdk_pixbuf_get_width (pixbuf);
		background->details->image_height_unscaled = gdk_pixbuf_get_height (pixbuf);
	}

	gtk_signal_emit (GTK_OBJECT (background), signals[APPEARANCE_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (background), signals[IMAGE_LOADING_DONE], pixbuf != NULL || background->details->image_uri == NULL);
}

static gboolean
nautilus_background_is_image_load_in_progress (NautilusBackground *background)
{
	return background->details->load_image_handle != NULL;
}

static void
nautilus_background_cancel_loading_image (NautilusBackground *background)
{
	if (nautilus_background_is_image_load_in_progress (background)) {
		nautilus_cancel_gdk_pixbuf_load (background->details->load_image_handle);
		background->details->load_image_handle = NULL;
		gtk_signal_emit (GTK_OBJECT (background), signals[IMAGE_LOADING_DONE], FALSE);
	}
}

static void
nautilus_background_start_loading_image (NautilusBackground *background)
{
	if (background->details->image_uri != NULL) {
		background->details->load_image_handle = nautilus_gdk_pixbuf_load_async (background->details->image_uri,
											 nautilus_background_load_image_callback,
											 background);
	} else {
		nautilus_background_load_image_callback (0, NULL, background);
	}
}

static gboolean
nautilus_background_set_image_uri_no_emit (NautilusBackground *background,
					const char *image_uri)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);

	if (nautilus_strcmp (background->details->image_uri, image_uri) == 0) {
		return FALSE;
	}

	nautilus_background_cancel_loading_image (background);
	
	g_free (background->details->image_uri);
	background->details->image_uri = g_strdup (image_uri);

	/* We do not get rid of the current image here. This gets done after the new
	 * image loads - in nautilus_background_load_image_callback. This way the
	 * current image can be used if an update is needed before the load completes.
	 */
	
	nautilus_background_start_loading_image (background);


	return TRUE;
}

void
nautilus_background_set_image_uri (NautilusBackground *background, const char *image_uri)
{
	if (nautilus_background_set_image_uri_no_emit (background, image_uri)) {
		/* The APPEARANCE_CHANGED is emitted when the image is done loading,
		 * in nautilus_background_load_image_callback. 
		 */
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
	}
}


static void
set_image_and_color_image_loading_done_callback (NautilusBackground *background, gboolean successful_load, const char *color)
{
	gtk_signal_disconnect_by_func (GTK_OBJECT (background), GTK_SIGNAL_FUNC (set_image_and_color_image_loading_done_callback), (gpointer) color);
	/* Note that the image loading done will have already emitted APPEARANCE_CHANGED
	 * and that this set color may cause another APPEARANCE_CHANGED (as well as a
	 * SETTINGS_CHANGED)
	 */
	nautilus_background_set_color (background, color);
}

/* Use this fn to set both the image and color and avoid flash. The color isn't
 * changed till after the image is done loading, that way if an update occurs
 * before then, it will use the old color and image.
 */
static void
nautilus_background_set_image_uri_and_color (NautilusBackground *background, const char *image_uri, const char *color)
{
	gtk_signal_connect (GTK_OBJECT (background),
			    "image_loading_done",
			    GTK_SIGNAL_FUNC (set_image_and_color_image_loading_done_callback),
			    (gpointer) color);
	if (!nautilus_background_set_image_uri_no_emit (background, image_uri)) {
		/* If the image loading was a no-op, then we have to manually call
		 * set_image_and_color_image_loading_done_callback. 
		 */
		set_image_and_color_image_loading_done_callback (background, TRUE, color);
	}
}

void
nautilus_background_receive_dropped_background_image (NautilusBackground *background,
						      const char *image_uri)
{
	/* Special case the reset-background image by file name. */
	if (nautilus_str_has_suffix (image_uri, "/" RESET_BACKGROUND_MAGIC_IMAGE_NAME)) {
		nautilus_background_reset (background);
	} else {
		nautilus_background_set_image_uri_and_color (background, image_uri, NULL);
	}
}

static GtkStyleClass *
nautilus_gtk_style_get_default_class (void)
{
	static GtkStyleClass *default_class;
	GtkStyle *style;
	
	if (default_class == NULL) {
		style = gtk_style_new ();
		default_class = style->klass;
		gtk_style_unref (style);
	}

	return default_class;
}

static void
nautilus_gdk_window_update_sizes (GdkWindow *window, int *width, int *height)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (width != NULL);
	g_return_if_fail (height != NULL);

	if (*width == -1 && *height == -1) {
		gdk_window_get_size (window, width, height);
	} else if (*width == -1) {
		gdk_window_get_size (window, width, NULL);
	} else if (*height == -1) {
		gdk_window_get_size (window, NULL, height);
	}
}

static void
nautilus_background_draw_flat_box (GtkStyle *style,
				   GdkWindow *window,
				   GtkStateType state_type,
				   GtkShadowType shadow_type,
				   GdkRectangle *area,
				   GtkWidget *widget,
				   char *detail,
				   int x,
				   int y,
				   int width,
				   int height)
{
	gboolean call_parent;
	NautilusBackground *background;
	GdkGC *gc;

	call_parent = TRUE;

	background = NULL;
	if (state_type == GTK_STATE_NORMAL) {
		background = nautilus_get_widget_background (widget);
		if (background != NULL) {
			call_parent = FALSE;
		}
	}

	if (call_parent) {
		(* nautilus_gtk_style_get_default_class()->draw_flat_box)
			(style, window, state_type, shadow_type, area, widget,
			 detail, x, y, width, height);
		return;
	}

    	gc = style->bg_gc[state_type];
	if (area)
		gdk_gc_set_clip_rectangle (gc, area);

	nautilus_gdk_window_update_sizes (window, &width, &height);	

	nautilus_background_draw_to_drawable (background, window, gc, 0, 0, width, height, widget->allocation.width, widget->allocation.height);
}

static GtkStyleClass *
nautilus_background_get_gtk_style_class (void)
{
	static GtkStyleClass *klass;

	if (klass == NULL) {
		static GtkStyleClass klass_storage;

		klass = &klass_storage;
		*klass = *nautilus_gtk_style_get_default_class ();

		klass->draw_flat_box = nautilus_background_draw_flat_box;
	}

	return klass;
}

static void
nautilus_background_set_widget_style (NautilusBackground *background,
				      GtkWidget *widget)
{
	GtkStyle *style;
	char *start_color_spec;
	GdkColor color;
	
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	style = gtk_widget_get_style (widget);
	
	/* Make a copy of the style. */
	style = gtk_style_copy (style);
	
	/* Give it the special class that allows it to draw gradients. */
	style->klass = nautilus_background_get_gtk_style_class ();

	/* Set up the colors in the style. */
	start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
	nautilus_gdk_color_parse_with_white_default (start_color_spec, &color);
	g_free (start_color_spec);
	
	style->bg[GTK_STATE_NORMAL] = color;
	style->base[GTK_STATE_NORMAL] = color;
	style->bg[GTK_STATE_ACTIVE] = color;
	style->base[GTK_STATE_ACTIVE] = color;
	
	/* Put the style in the widget. */
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);
}

/**
 * nautilus_background_is_set:
 * 
 * Check whether the background's color or image has been set.
 */
gboolean
nautilus_background_is_set (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);

	return background->details->color != NULL
		|| background->details->image_uri != NULL;
}

/* Returns false if the image is still loading, true
 * if it's done loading or there is no image.
 */
gboolean
nautilus_background_is_loaded (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);
	
	return background->details->image_uri == NULL ||
		   (!nautilus_background_is_image_load_in_progress (background) && background->details->image != NULL);
}

/**
 * nautilus_background_reset:
 *
 * Emit the reset signal to forget any color or image that has been
 * set previously.
 */
void
nautilus_background_reset (NautilusBackground *background)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	gtk_signal_emit (GTK_OBJECT (background),
			 signals[RESET]);
}

static void
nautilus_background_real_reset (NautilusBackground *background)
{
	nautilus_background_set_image_uri_and_color (background, NULL, NULL);
}

static void
nautilus_background_set_up_canvas (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	/* Attach ourselves to a canvas in a way that will work.
	   Changing the style is not sufficient.

	   Since there's no signal to override in GnomeCanvas to control
	   drawing the background, we change the class of the canvas root.
	   This gives us a chance to draw the background before any of the
	   objects draw themselves, and has no effect on the bounds or
	   anything related to scrolling.

	   We settled on this after less-than-thrilling results using a
	   canvas item as the background. The canvas item contributed to
	   the bounds of the canvas and had to constantly be resized.
	*/
	if (GNOME_IS_CANVAS (widget)) {
		g_assert (GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass
			  == gtk_type_class (GNOME_TYPE_CANVAS_GROUP)
			  || GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass
			  == gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP));

		GTK_OBJECT (GNOME_CANVAS (widget)->root)->klass =
			gtk_type_class (NAUTILUS_TYPE_BACKGROUND_CANVAS_GROUP);
	}
}

static void
nautilus_widget_background_changed (GtkWidget *widget, NautilusBackground *background)
{
	nautilus_background_set_widget_style (background, widget);
	nautilus_background_set_up_canvas (widget);

	gtk_widget_queue_clear (widget);
}

/* Gets the background attached to a widget.

   If the widget doesn't already have a NautilusBackground object,
   this will create one. To change the widget's background, you can
   just call nautilus_background methods on the widget.

   Later, we might want a call to find out if we already have a background,
   or a way to share the same background among multiple widgets; both would
   be straightforward.
*/
NautilusBackground *
nautilus_get_widget_background (GtkWidget *widget)
{
	gpointer data;
	NautilusBackground *background;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	/* Check for an existing background. */
	data = gtk_object_get_data (GTK_OBJECT (widget), "nautilus_background");
	if (data != NULL) {
		g_assert (NAUTILUS_IS_BACKGROUND (data));
		return data;
	}

	/* Store the background in the widget's data. */
	background = nautilus_background_new ();
	gtk_object_ref (GTK_OBJECT (background));
	gtk_object_sink (GTK_OBJECT (background));
	gtk_object_set_data_full (GTK_OBJECT (widget), "nautilus_background",
				  background, (GtkDestroyNotify) gtk_object_unref);

	/* Arrange to get the signal whenever the background changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (background),
					       "appearance_changed",
					       nautilus_widget_background_changed,
					       GTK_OBJECT (widget));
	nautilus_widget_background_changed (widget, background);

	return background;
}

gboolean
nautilus_widget_has_attached_background (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	return gtk_object_get_data (GTK_OBJECT (widget), "nautilus_background") != NULL;
}

gboolean
nautilus_background_is_too_complex_for_gtk_style (NautilusBackground *background)
{
	if (background == NULL) {
		return FALSE;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);
	
	if (background->details->image_uri != NULL) {
		return TRUE;
	}
	if (nautilus_gradient_is_gradient (background->details->color)) {
		return TRUE;
	}
	
	return FALSE;
}

/* determine if a background is darker or lighter than average, to help clients know what
   colors to draw on top with */
gboolean
nautilus_background_is_dark (NautilusBackground *background)
{
	GdkColor color;
	int intensity, intensity2;
	char *start_color_spec, *end_color_spec;

	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);
	
	if (background->details->image != NULL) {
		nautilus_gdk_pixbuf_average_value (background->details->image, &color);
			
	} else if (nautilus_gradient_is_gradient (background->details->color)) {
		start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
		end_color_spec = nautilus_gradient_get_end_color_spec (background->details->color);
		
		nautilus_gdk_color_parse_with_white_default (start_color_spec, &color);	
		intensity = (((color.red >> 8) * 77) + ((color.green >> 8) * 150) + ((color.blue >> 8) * 28)) >> 8;		 
		nautilus_gdk_color_parse_with_white_default (end_color_spec, &color);	
		intensity2 = (((color.red >> 8) * 77) + ((color.green >> 8) * 150) + ((color.blue >> 8) * 28)) >> 8;		 
		
		g_free (start_color_spec);
		g_free (end_color_spec);
		
		return (intensity + intensity2) < 320;
	} else {
		nautilus_gdk_color_parse_with_white_default (background->details->color, &color);	
	}
	
	intensity = (((color.red >> 8) * 77) + ((color.green >> 8) * 150) + ((color.blue >> 8) * 28)) >> 8;		 
	return intensity < 160; /* biased slightly to be dark */
}

   
/* handle dropped colors */
void
nautilus_background_receive_dropped_color (NautilusBackground *background,
					   GtkWidget *widget,
					   int drop_location_x,
					   int drop_location_y,
					   const GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;
	char *new_gradient_spec;
	int left_border, right_border, top_border, bottom_border;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (selection_data != NULL);

	/* Convert the selection data into a color spec. */
	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	channels = (guint16 *) selection_data->data;
	color_spec = g_strdup_printf ("rgb:%04hX/%04hX/%04hX", channels[0], channels[1], channels[2]);

	/* Figure out if the color was dropped close enough to an edge to create a gradient.
	   For the moment, this is hard-wired, but later the widget will have to have some
	   say in where the borders are.
	*/
	left_border = 32;
	right_border = widget->allocation.width - 32;
	top_border = 32;
	bottom_border = widget->allocation.height - 32;
	if (drop_location_x < left_border && drop_location_x <= right_border) {
		new_gradient_spec = nautilus_gradient_set_left_color_spec (background->details->color, color_spec);
	} else if (drop_location_x >= left_border && drop_location_x > right_border) {
		new_gradient_spec = nautilus_gradient_set_right_color_spec (background->details->color, color_spec);
	} else if (drop_location_y < top_border && drop_location_y <= bottom_border) {
		new_gradient_spec = nautilus_gradient_set_top_color_spec (background->details->color, color_spec);
	} else if (drop_location_y >= top_border && drop_location_y > bottom_border) {
		new_gradient_spec = nautilus_gradient_set_bottom_color_spec (background->details->color, color_spec);
	} else {
		new_gradient_spec = g_strdup (color_spec);
	}
	
	g_free (color_spec);

	nautilus_background_set_image_uri_and_color (background, NULL, new_gradient_spec);

	g_free (new_gradient_spec);
}

/* self check code */

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_background (void)
{
	NautilusBackground *background;

	background = nautilus_background_new ();

	nautilus_background_set_color (background, NULL);
	nautilus_background_set_color (background, "");
	nautilus_background_set_color (background, "red");
	nautilus_background_set_color (background, "red-blue");
	nautilus_background_set_color (background, "red-blue:h");

	gtk_object_unref (GTK_OBJECT (background));
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
