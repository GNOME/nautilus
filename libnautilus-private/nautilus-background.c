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
#include "nautilus-background.h"

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

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBackground, nautilus_background, GTK_TYPE_OBJECT)

enum {
	APPEARANCE_CHANGED,
	SETTINGS_CHANGED,
	LAST_SIGNAL
};

#define	RESET_BACKGROUND_IMAGE	"reset.png"

static guint signals[LAST_SIGNAL];

struct NautilusBackgroundDetails {
	char *color;
	char *tile_image_uri;
	GdkPixmap *tile_pixmap;
	GdkPixbuf *tile_image;
	NautilusPixbufLoadHandle *load_tile_image_handle;
};

static void
nautilus_background_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

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

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = nautilus_background_destroy;
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

	nautilus_cancel_gdk_pixbuf_load (background->details->load_tile_image_handle);

	g_free (background->details->color);
	g_free (background->details->tile_image_uri);
	if (background->details->tile_image != NULL) {
		gdk_pixbuf_unref (background->details->tile_image);
	}
	g_free (background->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusBackground *
nautilus_background_new (void)
{
	return NAUTILUS_BACKGROUND (gtk_type_new (NAUTILUS_TYPE_BACKGROUND));
}

void
nautilus_background_draw (NautilusBackground *background,
			  GdkDrawable *drawable,
			  GdkGC *gc,
			  const GdkRectangle *rectangle,
			  int origin_x,
			  int origin_y)
{
	char *start_color_spec, *end_color_spec;
	guint32 start_rgb, end_rgb;
	gboolean horizontal_gradient;

	if (background->details->tile_image != NULL) {
		nautilus_gdk_pixbuf_render_to_drawable_tiled (background->details->tile_image,
							      drawable,
							      gc,
							      rectangle,
							      GDK_RGB_DITHER_NORMAL,
							      origin_x, origin_y);
	} else {
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
}

static void
draw_pixbuf_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buf, double affine[6], int x_offset, int y_offset)
{
	void (* affine_function)
		(art_u8 *dst, int x0, int y0, int x1, int y1, int dst_rowstride,
		 const art_u8 *src, int src_width, int src_height, int src_rowstride,
		 const double affine[6],
		 ArtFilterLevel level,
		 ArtAlphaGamma *alpha_gamma);

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

	affine[4] -= x_offset;
	affine[5] -= y_offset;
}

/* fill the canvas buffer with a tiled pixmap */
static void
draw_pixbuf_tiled_aa(GdkPixbuf *pixbuf, GnomeCanvasBuf *buffer)
{
	int x, y;
	int start_x, start_y;
	int end_x, end_y;
	int tile_x, tile_y;
	int blit_x, blit_y;
	int tile_width, tile_height;
	int blit_width, blit_height;
	int tile_offset_x, tile_offset_y;
	
	double affine[6];
	
	art_affine_identity(affine);

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
			
			draw_pixbuf_aa(pixbuf, buffer, affine, x, y);
		}
	}
}

/* draw the background on the anti-aliased canvas */
void nautilus_background_draw_aa (NautilusBackground *background,
				  GnomeCanvasBuf *buffer,
				  int entire_width,
				  int entire_height)
{
	char *start_color_spec, *end_color_spec;
	guint32 start_rgb, end_rgb;
	gboolean horizontal_gradient;
	
	if (!buffer->is_buf) {
		if (background->details->tile_image) {
			draw_pixbuf_tiled_aa (background->details->tile_image, buffer);
		} else {
			start_color_spec = nautilus_gradient_get_start_color_spec (background->details->color);
			end_color_spec = nautilus_gradient_get_end_color_spec (background->details->color);
			horizontal_gradient = nautilus_gradient_is_horizontal (background->details->color);

			start_rgb = nautilus_parse_rgb_with_white_default (start_color_spec);
			end_rgb = nautilus_parse_rgb_with_white_default (end_color_spec);
		
			g_free (start_color_spec);
			g_free (end_color_spec);
		
			if (start_rgb != end_rgb) {
				nautilus_gnome_canvas_fill_with_gradient(buffer, entire_width, entire_height, start_rgb, end_rgb,
								  horizontal_gradient);
			} else
				gnome_canvas_buf_ensure_buf(buffer);
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
nautilus_background_get_tile_image_uri (NautilusBackground *background)
{
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NULL);

	return g_strdup (background->details->tile_image_uri);
}

void
nautilus_background_set_color (NautilusBackground *background,
			       const char *color)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (nautilus_strcmp (background->details->color, color) == 0) {
		return;
	}

	g_free (background->details->color);
	background->details->color = g_strdup (color);

	gtk_signal_emit (GTK_OBJECT (background),
			 signals[SETTINGS_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (background),
			 signals[APPEARANCE_CHANGED]);
}

static void
load_image_callback (GnomeVFSResult error,
		     GdkPixbuf *pixbuf,
		     gpointer callback_data)
{
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (callback_data);

	g_assert (background->details->tile_image == NULL);
	g_assert (background->details->load_tile_image_handle != NULL);

	background->details->load_tile_image_handle = NULL;

	/* Just ignore errors. */
	if (pixbuf == NULL) {
		return;
	}

	gdk_pixbuf_ref (pixbuf);
	background->details->tile_image = pixbuf;

	gtk_signal_emit (GTK_OBJECT (background),
			 signals[APPEARANCE_CHANGED]);
}

static void
start_loading_tile_image (NautilusBackground *background)
{
	if (background->details->tile_image_uri == NULL) {
		return;
	}

	background->details->load_tile_image_handle =
		nautilus_gdk_pixbuf_load_async (background->details->tile_image_uri,
						load_image_callback,
						background);
}

void
nautilus_background_set_tile_image_uri (NautilusBackground *background,
					const char *image_uri)
{
	g_return_if_fail (NAUTILUS_IS_BACKGROUND (background));

	if (nautilus_strcmp (background->details->tile_image_uri, image_uri) == 0) {
		return;
	}

	nautilus_cancel_gdk_pixbuf_load (background->details->load_tile_image_handle);
	background->details->load_tile_image_handle = NULL;

	g_free (background->details->tile_image_uri);
	
	/* special case the reset background image */
	if (nautilus_str_has_suffix(image_uri, RESET_BACKGROUND_IMAGE)) {
		nautilus_background_set_color (background, NULL);
		image_uri = NULL;
	}
	
	if (background->details->tile_image != NULL) {
		gdk_pixbuf_unref (background->details->tile_image);
		background->details->tile_image = NULL;
	}
	background->details->tile_image_uri = g_strdup (image_uri);
	start_loading_tile_image (background);

	gtk_signal_emit (GTK_OBJECT (background),
			 signals[SETTINGS_CHANGED]);
	gtk_signal_emit (GTK_OBJECT (background),
			 signals[APPEARANCE_CHANGED]);
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
			if (nautilus_gradient_is_gradient (background->details->color) ||
			   (background->details->tile_image != NULL)) {
				call_parent = FALSE;
			}
		  
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
	return background->details->color != NULL ||
	       background->details->tile_image_uri != NULL;
}

/**
 * nautilus_background_reset:
 * 
 * Forget any color or image that has been set previously.
 */
void
nautilus_background_reset (NautilusBackground *background)
{
	nautilus_background_set_color (background, NULL);
	nautilus_background_set_tile_image_uri (background, NULL);
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
nautilus_background_is_too_complex_for_gtk_style (NautilusBackground *background)
{
	if (background == NULL) {
		return FALSE;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), FALSE);

	if (background->details->tile_image != NULL) {
		return TRUE;
	}
	if (nautilus_gradient_is_gradient (background->details->color)) {
		return TRUE;
	}
	
	return FALSE;
}

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

	nautilus_background_set_color (background, new_gradient_spec);
	nautilus_background_set_tile_image_uri (background, NULL);
	
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
