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
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_svp_vpath.h>

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
	char *image_uri;
	GdkPixbuf *image;
	NautilusPixbufLoadHandle *load_image_handle;
	gboolean combine_mode;
	NautilusBackgroundImagePlacement image_placement;
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
				GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBackgroundClass,
						   appearance_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	signals[SETTINGS_CHANGED] =
		gtk_signal_new ("settings_changed",
				GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
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

static void
nautilus_background_destroy (GtkObject *object)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (object);

	nautilus_cancel_gdk_pixbuf_load (background->details->load_image_handle);

	g_free (background->details->color);
	g_free (background->details->image_uri);
	if (background->details->image != NULL) {
		gdk_pixbuf_unref (background->details->image);
	}
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
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background),
			      NAUTILUS_BACKGROUND_TILED);

	return background->details->image_placement;
}

void
nautilus_background_set_image_placement (NautilusBackground *background,
					 NautilusBackgroundImagePlacement new_placement)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (new_placement != background->details->image_placement) {
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
	return NAUTILUS_BACKGROUND (gtk_type_new (NAUTILUS_TYPE_BACKGROUND));
}

static gboolean
nautilus_background_image_fully_obscures (NautilusBackground *background,
					  int dest_width,
					  int dest_height,
					  gboolean use_alpha)
{
	int image_width;
	int image_height;
	
	if (background->details->image == NULL) {
		return FALSE;
	} 
	if (use_alpha && gdk_pixbuf_get_has_alpha (background->details->image)) {
		return FALSE;
	}

	image_width = gdk_pixbuf_get_width (background->details->image);
	image_height = gdk_pixbuf_get_height (background->details->image);
	
	switch (background->details->image_placement) {
	case NAUTILUS_BACKGROUND_SCALED:
	case NAUTILUS_BACKGROUND_TILED:
		return TRUE;
	case NAUTILUS_BACKGROUND_SCALED_ASPECT:
		/* See if the image has the same aspect ratio as the background.
		 * Equivalent to: 
		 *   image_width/image_height = rectangle->width/rectangle->height
		 * but avoids floating point (& integer division pitfall)
		 */
		return image_width * dest_height == image_height * dest_width;
	case NAUTILUS_BACKGROUND_CENTERED:
		/* Check if it's smaller in either dimension. */
		return !(image_width < dest_width || image_height < dest_height);
	default:
		g_assert_not_reached ();
		return FALSE;
	}
}

/* This routine is for gdk style rendering, which doesn't naturally
 * support transparency, so we draw into a pixbuf offscreen if
 * necessary.
 */
void
nautilus_background_draw (NautilusBackground *background,
			  GdkDrawable *drawable,			  
			  GdkGC *gc,
			  const GdkRectangle *rectangle,
			  int origin_x,
			  int origin_y)
{
	GdkPixbuf *pixbuf;
	GnomeCanvasBuf buffer;
	int image_width;
	int image_height;
	char *start_color_spec, *end_color_spec;
	guint32 start_rgb, end_rgb;
	gboolean horizontal_gradient;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (background->details->combine_mode) {
		/* allocate a pixbuf the size of the rectangle */
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, rectangle->width, rectangle->height);
		
		/* contrive a CanvasBuf structure to point to it */
		buffer.buf =  gdk_pixbuf_get_pixels (pixbuf);
		buffer.buf_rowstride =  gdk_pixbuf_get_rowstride (pixbuf);
		buffer.rect.x0 = rectangle->x;
		buffer.rect.y0 = rectangle->y;
		buffer.rect.x1 = rectangle->x + rectangle->width;
		buffer.rect.y1 = rectangle->y + rectangle->height;
		buffer.bg_color = 0xFFFFFFFF;
		buffer.is_buf = FALSE;
		buffer.is_bg = FALSE;
		
		/* invoke the anti-aliased code to do the work */
		nautilus_background_draw_aa (background, &buffer, rectangle->width, rectangle->height);
		
		/* blit the pixbuf to the drawable */
		gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
					       0, 0,
					       rectangle->x, rectangle->y, rectangle->width, rectangle->height,
					       GDK_RGB_DITHER_NORMAL, origin_x, origin_y);
		
		/* free things up and we're done */
		gdk_pixbuf_unref (pixbuf);
		
		return;
	}

	if (!nautilus_background_image_fully_obscures (background, rectangle->width, rectangle->height, FALSE)) {
		start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
		end_color_spec = nautilus_gradient_get_end_color_spec (background->details->color);
		horizontal_gradient = nautilus_gradient_is_horizontal (background->details->color);

		start_rgb = nautilus_parse_rgb_with_white_default (start_color_spec);
		end_rgb = nautilus_parse_rgb_with_white_default (end_color_spec);
		
		g_free (start_color_spec);
		g_free (end_color_spec);
		
		nautilus_fill_rectangle_with_gradient (drawable,
						       gc,
						       rectangle,
						       start_rgb,
						       end_rgb,
						       horizontal_gradient);
	}

	if (background->details->image != NULL) {
		switch (background->details->image_placement) {
		case NAUTILUS_BACKGROUND_TILED:
			nautilus_gdk_pixbuf_render_to_drawable_tiled (background->details->image,
								      drawable,
								      gc,
								      rectangle,
								      GDK_RGB_DITHER_NORMAL,
								      origin_x, origin_y);
			break;
		default:
			g_assert_not_reached ();
			/* fall through */
		case NAUTILUS_BACKGROUND_CENTERED:
			image_width = gdk_pixbuf_get_width (background->details->image);
			image_height = gdk_pixbuf_get_height (background->details->image);
			gdk_pixbuf_render_to_drawable (background->details->image, drawable, gc,
						       0, 0,
						       rectangle->x + (rectangle->width - image_width)/2,
						       rectangle->y + (rectangle->height - image_height)/2,
						       image_width, image_height,
						       GDK_RGB_DITHER_NORMAL, 0, 0);
			break;
		case NAUTILUS_BACKGROUND_SCALED:
			pixbuf = gdk_pixbuf_scale_simple (background->details->image,
							  rectangle->width, rectangle->height,
							  GDK_INTERP_BILINEAR);
			gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
						       0, 0,
						       rectangle->x, rectangle->y,
						       rectangle->width, rectangle->height,
						       GDK_RGB_DITHER_NORMAL, 0, 0);
			gdk_pixbuf_unref (pixbuf);
			break;
		case NAUTILUS_BACKGROUND_SCALED_ASPECT:
			pixbuf = nautilus_gdk_pixbuf_scale_to_fit (background->details->image,
								   rectangle->width, rectangle->height);
			/* Use new width & height to draw centered. */
			image_width = gdk_pixbuf_get_width (pixbuf);
			image_height = gdk_pixbuf_get_height (pixbuf);
			gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
						       0, 0,
						       rectangle->x + (rectangle->width - image_width)/2,
						       rectangle->y + (rectangle->height - image_height)/2,
						       image_width, image_height,
						       GDK_RGB_DITHER_NORMAL, 0, 0);
			gdk_pixbuf_unref (pixbuf);
			break;
		}
	}
}

static void
draw_pixbuf_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buf,
		int x_offset, int y_offset, double h_scale, double v_scale)
{
	void (* affine_function)
		(art_u8 *dst, int x0, int y0, int x1, int y1, int dst_rowstride,
		 const art_u8 *src, int src_width, int src_height, int src_rowstride,
		 const double affine[6],
		 ArtFilterLevel level,
		 ArtAlphaGamma *alpha_gamma);

	double affine[6];
	
	art_affine_identity (affine);
	art_affine_scale (affine, h_scale, v_scale);

	affine[4] += x_offset;
	affine[5] += y_offset;

	affine_function = gdk_pixbuf_get_has_alpha (pixbuf)
		? art_rgb_rgba_affine
		: art_rgb_affine;
	
	(* affine_function)
		(buf->buf,
		 buf->rect.x0, buf->rect.y0,
		 buf->rect.x1, buf->rect.y1,
		 buf->buf_rowstride,
		 gdk_pixbuf_get_pixels (pixbuf),
		 gdk_pixbuf_get_width (pixbuf),
		 gdk_pixbuf_get_height (pixbuf),
		 gdk_pixbuf_get_rowstride (pixbuf),
		 affine,
		 ART_FILTER_NEAREST,
		 NULL);
}

static void
draw_pixbuf_scaled_aspect_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer,
			      int entire_width, int entire_height)
{
	int image_width;
	int image_height;
	int image_left;
	int image_top;

	double scale;

	image_width = gdk_pixbuf_get_width (pixbuf);
	image_height = gdk_pixbuf_get_height (pixbuf);

	scale = MIN ((double) entire_width / image_width, (double) entire_height / image_height);

	image_left = (entire_width - floor (image_width * scale + 0.5)) / 2;
	image_top = (entire_height - floor (image_height * scale + 0.5)) / 2;
	
	draw_pixbuf_aa (pixbuf, buffer, image_left, image_top, scale, scale);
}

static void
draw_pixbuf_scaled_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer,
		       int entire_width, int entire_height)
{
	double h_scale;
	double v_scale;
	
	h_scale = (double) entire_width / gdk_pixbuf_get_width (pixbuf);
	v_scale = (double) entire_height / gdk_pixbuf_get_height (pixbuf);
			  
	draw_pixbuf_aa (pixbuf, buffer, 0, 0, h_scale, v_scale);	
}

static void
draw_pixbuf_centered_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer,
			 int entire_width, int entire_height)
{
	int image_width;
	int image_height;
	int image_left;
	int image_top;
		
	image_width = gdk_pixbuf_get_width (pixbuf);
	image_height = gdk_pixbuf_get_height (pixbuf);
	image_left = (entire_width - image_width) / 2;
	image_top = (entire_height - image_height) / 2;

	draw_pixbuf_aa (pixbuf, buffer, image_left, image_top, 1, 1);
}

/* fill the canvas buffer with a tiled pixmap */
static void
draw_pixbuf_tiled_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer)
{
	int x, y;
	int start_x, start_y;
	int end_x, end_y;
	int tile_x, tile_y;
	int blit_x, blit_y;
	int tile_width, tile_height;
	int blit_width, blit_height;
	int tile_offset_x, tile_offset_y;
	
	tile_width = gdk_pixbuf_get_width (pixbuf);
	tile_height = gdk_pixbuf_get_height (pixbuf);
	
	tile_offset_x = buffer->rect.x0  % tile_width;
	tile_offset_y = buffer->rect.y0  % tile_height;

	start_x = buffer->rect.x0 - tile_offset_x;
	start_y = buffer->rect.y0 - tile_offset_y;
	end_x = buffer->rect.x1;
	end_y = buffer->rect.y1;

	for (x = start_x; x < end_x; x += tile_width) {
		blit_x = MAX (x, buffer->rect.x0);
		tile_x = blit_x - x;
		blit_width = MIN (tile_width, end_x - x) - tile_x;
		
		for (y = start_y; y < end_y; y += tile_height) {
			blit_y = MAX (y, buffer->rect.y0);
			tile_y = blit_y - y;
			blit_height = MIN (tile_height, end_y - y) - tile_y;
			
			draw_pixbuf_aa (pixbuf, buffer, x, y, 1, 1);
		}
	}
}

/* draw the background on the anti-aliased canvas */
/* we support n-point gradients by looping on the color string */

void
nautilus_background_draw_aa (NautilusBackground *background,
			     GnomeCanvasBuf *buffer,
			     int entire_width,
			     int entire_height)
{
	char *start_color_spec, *current_color;
	char *temp_str, *percentage_str; 
	GnomeCanvasBuf save_buf;
	guint32 start_rgb, end_rgb;
	gboolean horizontal_gradient, more_to_do;
	int current_width, current_height;
	int remaining_width, remaining_height;
	int accumulator, temp_value;

	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	remaining_width = 0;
	remaining_height = 0;
	
	if (!buffer->is_buf) {
		if (!nautilus_background_image_fully_obscures (background, entire_width, entire_height, TRUE)) {
			/* get the initial color */
			start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
			start_rgb = nautilus_parse_rgb_with_white_default (start_color_spec);
			g_free (start_color_spec);

			/* set up constants for the loop */
			horizontal_gradient = nautilus_gradient_is_horizontal (background->details->color);
			current_color = nautilus_strchr (background->details->color, '-');
			more_to_do = TRUE;
			current_width = entire_width;
			current_height = entire_height;			
			save_buf = *buffer;
			
			while (more_to_do) {
				/* extract the next color and flag the continuation state */
				if (current_color == NULL) {
					end_rgb = start_rgb;
					more_to_do = FALSE;
				} else {
					start_color_spec = nautilus_gradient_get_start_color_spec (current_color + 1);
					
					/* remove percentage specifier, if necessary */
					percentage_str = nautilus_strchr (start_color_spec, '|');
					if (percentage_str) {
						*percentage_str = '\0';
					}
					end_rgb = nautilus_parse_rgb_with_white_default (start_color_spec);
					g_free (start_color_spec);

					temp_str = nautilus_strchr (current_color + 1, '-');
					if (temp_str == NULL) {
						more_to_do = FALSE;
					} else {
						/* extract the percentage and scale done the width or height */
						percentage_str = nautilus_strchr (current_color, '|');
						if (percentage_str) {
							percentage_str += 1;
							accumulator = 0;
							while (isdigit (*percentage_str))  {
								accumulator = (10 * accumulator) + (*percentage_str - '0');
								percentage_str += 1;
							}
							
							if (horizontal_gradient) {
								temp_value = current_width * accumulator / 100;
								remaining_width = current_width - temp_value;
								current_width = temp_value;
							} else {
								temp_value = current_height * accumulator / 100;
								remaining_height = current_height - temp_value;
								current_height = temp_value;
							}
							
							current_color = temp_str;
							more_to_do = TRUE;
						} else {
							more_to_do = FALSE;
						}
						
					}
				}
				
				/* draw the gradient or solid color */		
				nautilus_gnome_canvas_fill_with_gradient
					(buffer, current_width, current_height,
					 start_rgb, end_rgb,
					 horizontal_gradient);
			
				/* set things up for the next time through, if necessary */
				start_rgb = end_rgb;
				/* bump the buffer pointer by the amount done */
				if (more_to_do) {
					if (horizontal_gradient) {
						buffer->buf += (3 * current_width);
						current_width = remaining_width;
						buffer->rect.x1 = buffer->rect.x0 + remaining_width;
					} else {
						buffer->buf += (current_height * buffer->buf_rowstride);
						current_height = remaining_height;
						buffer->rect.y1 = buffer->rect.y0 + remaining_height;
					}
				}
			}
			*buffer = save_buf;
		}

		if (background->details->image != NULL) {
			switch (background->details->image_placement) {
			case NAUTILUS_BACKGROUND_TILED:
				draw_pixbuf_tiled_aa (background->details->image, buffer);
				break;
			case NAUTILUS_BACKGROUND_SCALED:
				draw_pixbuf_scaled_aa (background->details->image, buffer, entire_width, entire_height);
				break;
			case NAUTILUS_BACKGROUND_SCALED_ASPECT:
				draw_pixbuf_scaled_aspect_aa (background->details->image, buffer, entire_width, entire_height);
				break;
			default:
				g_assert_not_reached ();
				/* fall through */
			case NAUTILUS_BACKGROUND_CENTERED:
				draw_pixbuf_centered_aa (background->details->image, buffer, entire_width, entire_height);
				break;
			}
		}
						
		buffer->is_buf = TRUE;
	}
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
		
	return TRUE;
}

void
nautilus_background_set_color (NautilusBackground *background,
			       const char *color)
{
	if (nautilus_background_set_color_no_emit (background, color)) {
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
		gtk_signal_emit (GTK_OBJECT (background), signals[APPEARANCE_CHANGED]);
	}
}


static void
load_image_callback (GnomeVFSResult error,
		     GdkPixbuf *pixbuf,
		     gpointer callback_data)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (callback_data);

	g_assert (background->details->image == NULL);
	g_assert (background->details->load_image_handle != NULL);

	background->details->load_image_handle = NULL;

	/* Just ignore errors. */
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
		background->details->image = pixbuf;

		gtk_signal_emit (GTK_OBJECT (background),
				 signals[APPEARANCE_CHANGED]);
	}

	/* always emit IMAGE_LOADING_DONE
	 */
	gtk_signal_emit (GTK_OBJECT (background), signals[IMAGE_LOADING_DONE], pixbuf != NULL);
}

static void
start_loading_image (NautilusBackground *background)
{
	if (background->details->image_uri == NULL) {
		return;
	}

	background->details->load_image_handle =
		nautilus_gdk_pixbuf_load_async (background->details->image_uri,
						load_image_callback,
						background);
}

static gboolean
nautilus_background_set_image_uri_no_emit (NautilusBackground *background,
					const char *image_uri)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);

	if (nautilus_strcmp (background->details->image_uri, image_uri) == 0) {
		return FALSE;
	}

	nautilus_cancel_gdk_pixbuf_load (background->details->load_image_handle);
	background->details->load_image_handle = NULL;
	
	g_free (background->details->image_uri);
	
	if (background->details->image != NULL) {
		gdk_pixbuf_unref (background->details->image);
		background->details->image = NULL;
	}

	background->details->image_uri = g_strdup (image_uri);
	start_loading_image (background);

	return TRUE;
}

void
nautilus_background_set_image_uri (NautilusBackground *background,
				   const char *image_uri)
{
	if (nautilus_background_set_image_uri_no_emit (background, image_uri)) {
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
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
		nautilus_background_set_image_uri_no_emit (background, image_uri);
		nautilus_background_set_color_no_emit (background, NULL);
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
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
	GdkRectangle rectangle;

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
	rectangle.x = x;
	rectangle.y = y;
	rectangle.width = width;
	rectangle.height = height;
	
	nautilus_background_draw (background, window, gc,
				  &rectangle, 0, 0);	
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
	
	return background->details->image_uri == NULL
		|| background->details->image != NULL;
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
	if (nautilus_background_set_color_no_emit (background, NULL)
	    || nautilus_background_set_image_uri_no_emit (background, NULL)) {
		gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
		gtk_signal_emit (GTK_OBJECT (background), signals[APPEARANCE_CHANGED]);
	}
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
	gtk_object_set_data_full (GTK_OBJECT (widget), "nautilus_background",
				  background, (GtkDestroyNotify) gtk_object_unref);
	gtk_object_ref (GTK_OBJECT (background));
	gtk_object_sink (GTK_OBJECT (background));

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
	
	if (background->details->image != NULL) {
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

	nautilus_background_set_color_no_emit (background, new_gradient_spec);
	nautilus_background_set_image_uri_no_emit (background, NULL);
	gtk_signal_emit (GTK_OBJECT (background), signals[SETTINGS_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (background), signals[APPEARANCE_CHANGED]);

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
