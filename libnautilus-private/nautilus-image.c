/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.c - A widget to smoothly display images.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-image.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-art-gtk-extensions.h"
#include "nautilus-string.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-debug-drawing.h"

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_MODE,
	ARG_IS_SMOOTH,
	ARG_PIXBUF,
	ARG_PIXBUF_OPACITY,
	ARG_TILE_HEIGHT,
	ARG_TILE_MODE_HORIZONTAL,
	ARG_TILE_MODE_VERTICAL,
	ARG_TILE_OPACITY,
	ARG_TILE_PIXBUF,
	ARG_TILE_WIDTH
};

/* Signals */
typedef enum
{
	DRAW_BACKGROUND,
	SET_IS_SMOOTH,
	LAST_SIGNAL
} ImageSignal;

/* Signals */
static guint image_signals[LAST_SIGNAL] = { 0 };

/* Detail member struct */
struct _NautilusImageDetails
{
	gboolean is_smooth;

	/* Tile attributes */
	GdkPixbuf *tile_pixbuf;
	int tile_opacity;
	int tile_width;
	int tile_height;
	NautilusSmoothTileMode tile_mode_vertical;
	NautilusSmoothTileMode tile_mode_horizontal;

	/* Pixbuf */
	GdkPixbuf *pixbuf;
	int pixbuf_opacity;

	/* Background */
	NautilusSmoothBackgroundMode background_mode;
	guint32 solid_background_color;
	gboolean never_smooth;
};

/* GtkObjectClass methods */
static void     nautilus_image_initialize_class     (NautilusImageClass  *image_class);
static void     nautilus_image_initialize           (NautilusImage       *image);
static void     nautilus_image_destroy              (GtkObject           *object);
static void     nautilus_image_set_arg              (GtkObject           *object,
						     GtkArg              *arg,
						     guint                arg_id);
static void     nautilus_image_get_arg              (GtkObject           *object,
						     GtkArg              *arg,
						     guint                arg_id);
/* GtkWidgetClass methods */
static void     nautilus_image_size_request         (GtkWidget           *widget,
						     GtkRequisition      *requisition);
static int      nautilus_image_expose_event         (GtkWidget           *widget,
						     GdkEventExpose      *event);

/* NautilusImage signals */
static void     nautilus_image_set_is_smooth_signal (GtkWidget           *widget,
						     gboolean             is_smooth);

/* Private NautilusImage methods */
static ArtIRect image_get_pixbuf_frame              (const NautilusImage *image);
static ArtIRect image_get_pixbuf_bounds             (const NautilusImage *image);
static ArtIRect image_get_tile_frame                (const NautilusImage *image);
static gboolean image_is_smooth                     (const NautilusImage *image);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusImage, nautilus_image, GTK_TYPE_MISC)

/* Class init methods */
static void
nautilus_image_initialize_class (NautilusImageClass *image_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (image_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (image_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_image_destroy;
	object_class->set_arg = nautilus_image_set_arg;
	object_class->get_arg = nautilus_image_get_arg;
	
	/* GtkWidgetClass */
	widget_class->size_request = nautilus_image_size_request;
	widget_class->expose_event = nautilus_image_expose_event;

	/* NautilusImageClass */
	image_class->set_is_smooth = nautilus_image_set_is_smooth_signal;
	
	/* Signals */
	image_signals[DRAW_BACKGROUND] = gtk_signal_new ("draw_background",
							 GTK_RUN_LAST,
							 object_class->type,
							 0,
							 gtk_marshal_NONE__POINTER_POINTER,
							 GTK_TYPE_NONE, 
							 2,
							 GTK_TYPE_POINTER,
							 GTK_TYPE_POINTER);

	image_signals[SET_IS_SMOOTH] = gtk_signal_new ("set_is_smooth",
						       GTK_RUN_LAST,
						       object_class->type,
						       GTK_SIGNAL_OFFSET (NautilusImageClass, set_is_smooth),
						       gtk_marshal_NONE__BOOL,
						       GTK_TYPE_NONE, 
						       1,
						       GTK_TYPE_BOOL);
	
	gtk_object_class_add_signals (object_class, image_signals, LAST_SIGNAL);

	/* Arguments */
	gtk_object_add_arg_type ("NautilusImage::is_smooth",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_IS_SMOOTH);
	gtk_object_add_arg_type ("NautilusImage::pixbuf",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PIXBUF);
	gtk_object_add_arg_type ("NautilusImage::pixbuf_opacity",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_PIXBUF_OPACITY);
	gtk_object_add_arg_type ("NautilusImage::background_mode",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_MODE);
	gtk_object_add_arg_type ("NautilusImage::tile_pixbuf",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TILE_PIXBUF);
	gtk_object_add_arg_type ("NautilusImage::tile_opacity",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_OPACITY);
	gtk_object_add_arg_type ("NautilusImage::tile_width",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_WIDTH);
	gtk_object_add_arg_type ("NautilusImage::tile_height",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_HEIGHT);
	gtk_object_add_arg_type ("NautilusImage::tile_mode_vertical",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_MODE_VERTICAL);
	gtk_object_add_arg_type ("NautilusImage::tile_mode_horizontal",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_MODE_HORIZONTAL);

	/* Make this class inherit the same kind of theme stuff as GtkPixmap */
	nautilus_gtk_class_name_make_like_existing_type ("NautilusImage", GTK_TYPE_PIXMAP);
}

void
nautilus_image_initialize (NautilusImage *image)
{
	GTK_WIDGET_UNSET_FLAGS (image, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (image, GTK_NO_WINDOW);

	image->details = g_new0 (NautilusImageDetails, 1);

	image->details->pixbuf_opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;
	image->details->tile_opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;
 	image->details->tile_width = NAUTILUS_SMOOTH_TILE_EXTENT_FULL;
 	image->details->tile_height = NAUTILUS_SMOOTH_TILE_EXTENT_FULL;
	image->details->tile_mode_vertical = NAUTILUS_SMOOTH_TILE_SELF;
	image->details->tile_mode_horizontal = NAUTILUS_SMOOTH_TILE_SELF;
	image->details->background_mode = NAUTILUS_SMOOTH_BACKGROUND_GTK;

	nautilus_smooth_widget_register (GTK_WIDGET (image));
}

/* GtkObjectClass methods */
static void
nautilus_image_destroy (GtkObject *object)
{
 	NautilusImage *image;
	
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

	image = NAUTILUS_IMAGE (object);

	nautilus_gdk_pixbuf_unref_if_not_null (image->details->tile_pixbuf);
	image->details->tile_pixbuf = NULL;

	g_free (image->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_image_set_arg (GtkObject *object,
			GtkArg *arg,
			guint arg_id)
{
	NautilusImage *image;
	
	g_return_if_fail (NAUTILUS_IS_IMAGE (object));

 	image = NAUTILUS_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_IS_SMOOTH:
		nautilus_image_set_is_smooth (image, GTK_VALUE_BOOL (*arg));
		break;

	case ARG_PIXBUF_OPACITY:
		nautilus_image_set_pixbuf_opacity (image, GTK_VALUE_INT (*arg));
		break;

	case ARG_BACKGROUND_MODE:
		nautilus_image_set_background_mode (image, GTK_VALUE_UINT (*arg));
		break;

	case ARG_PIXBUF:
		nautilus_image_set_pixbuf (image, (GdkPixbuf *) GTK_VALUE_POINTER (*arg));
		break;

	case ARG_TILE_OPACITY:
		nautilus_image_set_tile_opacity (image, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_PIXBUF:
		nautilus_image_set_tile_pixbuf (image, (GdkPixbuf *) GTK_VALUE_POINTER (*arg));
		break;
		
	case ARG_TILE_WIDTH:
		nautilus_image_set_tile_width (image, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_HEIGHT:
		nautilus_image_set_tile_height (image, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_MODE_VERTICAL:
		nautilus_image_set_tile_mode_vertical (image, GTK_VALUE_UINT (*arg));
		break;

	case ARG_TILE_MODE_HORIZONTAL:
		nautilus_image_set_tile_mode_horizontal (image, GTK_VALUE_UINT (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_image_get_arg (GtkObject *object,
			GtkArg *arg,
			guint arg_id)
{
	NautilusImage *image;

	g_return_if_fail (NAUTILUS_IS_IMAGE (object));
	
	image = NAUTILUS_IMAGE (object);

 	switch (arg_id)
	{
	case ARG_IS_SMOOTH:
		GTK_VALUE_BOOL (*arg) = nautilus_image_get_is_smooth (image);
		break;
		
	case ARG_PIXBUF_OPACITY:
		GTK_VALUE_INT (*arg) = nautilus_image_get_pixbuf_opacity (image);
		break;
		
	case ARG_BACKGROUND_MODE:
		GTK_VALUE_UINT (*arg) = nautilus_image_get_background_mode (image);
		break;
		
	case ARG_PIXBUF:
		GTK_VALUE_POINTER (*arg) = nautilus_image_get_pixbuf (image);
		break;

	case ARG_TILE_OPACITY:
		GTK_VALUE_INT (*arg) = nautilus_image_get_tile_opacity (image);
		break;
		
	case ARG_TILE_PIXBUF:
		GTK_VALUE_POINTER (*arg) = nautilus_image_get_tile_pixbuf (image);
		break;

	case ARG_TILE_WIDTH:
		GTK_VALUE_INT (*arg) = nautilus_image_get_tile_width (image);
		break;

	case ARG_TILE_HEIGHT:
		GTK_VALUE_INT (*arg) = nautilus_image_get_tile_height (image);
		break;

	case ARG_TILE_MODE_VERTICAL:
		GTK_VALUE_UINT (*arg) = nautilus_image_get_tile_mode_vertical (image);
		break;

	case ARG_TILE_MODE_HORIZONTAL:
		GTK_VALUE_UINT (*arg) = nautilus_image_get_tile_mode_horizontal (image);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_image_size_request (GtkWidget *widget,
			     GtkRequisition *requisition)
{
	NautilusImage *image;

	ArtIRect pixbuf_frame;
	ArtIRect tile_frame;
	ArtIRect preferred_frame;

	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (requisition != NULL);

 	image = NAUTILUS_IMAGE (widget);
	
	pixbuf_frame = image_get_pixbuf_frame (image);
	tile_frame = image_get_tile_frame (image);
	preferred_frame = nautilus_smooth_widget_get_preferred_frame (widget,
								      &pixbuf_frame,
								      &tile_frame,
								      image->details->tile_width,
								      image->details->tile_height);
   	requisition->width = preferred_frame.x1;
   	requisition->height = preferred_frame.y1;
}

static void
image_paint_pixbuf_callback (GtkWidget *widget,
			     GdkDrawable *destination_drawable,
			     GdkGC *gc,
			     int source_x,
			     int source_y,
			     const ArtIRect *area,
			     gpointer callback_data)
{
	NautilusImage *image;

	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));
	g_return_if_fail (destination_drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (area != NULL && !art_irect_empty (area));

	image = NAUTILUS_IMAGE (widget);

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (image->details->pixbuf));
	
	nautilus_gdk_pixbuf_draw_to_drawable (image->details->pixbuf,
					      destination_drawable,
					      gc,
					      source_x,
					      source_y,
					      area,
					      GDK_RGB_DITHER_NONE,
					      GDK_PIXBUF_ALPHA_BILEVEL,
					      NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
}

static void
image_composite_pixbuf_callback (GtkWidget *widget,
				 GdkPixbuf *destination_pixbuf,
				 int source_x,
				 int source_y,
				 const ArtIRect *area,
				 int opacity,
				 gpointer callback_data)
{
	NautilusImage *image;

	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (area != NULL && !art_irect_empty (area));

	image = NAUTILUS_IMAGE (widget);

	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (image->details->pixbuf));

	nautilus_gdk_pixbuf_draw_to_pixbuf_alpha (image->details->pixbuf,
						  destination_pixbuf,
						  source_x,
						  source_y,
						  area,
						  opacity,
						  GDK_INTERP_BILINEAR);
}	

static int
nautilus_image_expose_event (GtkWidget *widget,
			     GdkEventExpose *event)
{
 	NautilusImage *image;
	ArtIRect dirty_area;
	ArtIRect screen_dirty_area;

	ArtIRect pixbuf_bounds;
	ArtIRect tile_bounds;

	g_return_val_if_fail (NAUTILUS_IS_IMAGE (widget), TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);
	g_return_val_if_fail (event->window == widget->window, TRUE);
	
 	image = NAUTILUS_IMAGE (widget);

	pixbuf_bounds = image_get_pixbuf_bounds (image);
	tile_bounds = nautilus_smooth_widget_get_tile_bounds (widget,
							      image->details->tile_pixbuf,
							      image->details->tile_width,
							      image->details->tile_height);
	
	/* Check for the dumb case when theres nothing to do */
	if (image->details->pixbuf == NULL && image->details->tile_pixbuf == NULL) {
		return TRUE;
	}

	/* Clip the dirty area to the screen */
	dirty_area = nautilus_irect_assign_gdk_rectangle (&event->area);
	screen_dirty_area = nautilus_irect_gdk_window_clip_dirty_area_to_screen (event->window,
										 &dirty_area);
	/* Nothing to do */
	if (art_irect_empty (&screen_dirty_area)) {
		return TRUE;
	}

	/* Paint ourselves */
	nautilus_smooth_widget_paint (widget,
				      widget->style->white_gc,
				      image_is_smooth (image),
				      image->details->background_mode,
				      image->details->solid_background_color,
				      image->details->tile_pixbuf,
				      &tile_bounds,
				      image->details->tile_opacity,
				      image->details->tile_mode_vertical,
				      image->details->tile_mode_horizontal,
				      &pixbuf_bounds,
				      image->details->pixbuf_opacity,
				      &screen_dirty_area,
				      image_paint_pixbuf_callback,
				      image_composite_pixbuf_callback,
				      NULL);

	return TRUE;
}

/* NautilusImage signals */
static void
nautilus_image_set_is_smooth_signal (GtkWidget *widget,
				     gboolean is_smooth)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (widget));

	nautilus_image_set_is_smooth (NAUTILUS_IMAGE (widget), is_smooth);
}

/* Private NautilusImage methods */
static ArtIRect
image_get_pixbuf_frame (const NautilusImage *image)
{
	ArtIRect pixbuf_frame;

	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NAUTILUS_ART_IRECT_EMPTY);

	if (!image->details->pixbuf) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	pixbuf_frame.x0 = 0;
	pixbuf_frame.y0 = 0;
	pixbuf_frame.x1 = gdk_pixbuf_get_width (image->details->pixbuf);
	pixbuf_frame.y1 = gdk_pixbuf_get_height (image->details->pixbuf);

	return pixbuf_frame;
}

static ArtIRect
image_get_pixbuf_bounds (const NautilusImage *image)
{
	ArtIRect pixbuf_frame;
	ArtIRect pixbuf_bounds;
	ArtIRect bounds;

	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NAUTILUS_ART_IRECT_EMPTY);

	pixbuf_frame = image_get_pixbuf_frame (image);

	if (art_irect_empty (&pixbuf_frame)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}
	
	bounds = nautilus_irect_gtk_widget_get_bounds (GTK_WIDGET (image));
	
	pixbuf_bounds = nautilus_art_irect_align (&bounds,
						  pixbuf_frame.x1,
						  pixbuf_frame.y1,
						  GTK_MISC (image)->xalign,
						  GTK_MISC (image)->yalign);

	return pixbuf_bounds;
}

static ArtIRect
image_get_tile_frame (const NautilusImage *image)
{
	ArtIRect tile_frame;

	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NAUTILUS_ART_IRECT_EMPTY);

	if (!image->details->tile_pixbuf) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	tile_frame.x0 = 0;
	tile_frame.y0 = 0;
	tile_frame.x1 = gdk_pixbuf_get_width (image->details->tile_pixbuf);
	tile_frame.y1 = gdk_pixbuf_get_height (image->details->tile_pixbuf);

	return tile_frame;
}

gboolean
image_is_smooth (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), FALSE);

	return !image->details->never_smooth && image->details->is_smooth;
}

/* Public NautilusImage methods */
GtkWidget*
nautilus_image_new (const char *file_name)
{
	NautilusImage *image;

	image = NAUTILUS_IMAGE (gtk_widget_new (nautilus_image_get_type (), NULL));
	
	if (file_name != NULL) {
		nautilus_image_set_pixbuf_from_file_name (image, file_name);
	}

	return GTK_WIDGET (image);
}

void
nautilus_image_set_is_smooth (NautilusImage *image,
			      gboolean is_smooth)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (image->details->never_smooth) {
		return;
	}

	if (image->details->is_smooth == is_smooth) {
		return;
	}

	image->details->is_smooth = is_smooth;

	/* We call queue_resize() instead queue_draw() because
	 * we want the widget's background to be cleared of 
	 * the previous pixbuf, even though the geometry of 
	 * the image does not change.
	 */ 
	gtk_widget_queue_resize (GTK_WIDGET (image));
}

gboolean
nautilus_image_get_is_smooth (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), FALSE);

	return image_is_smooth (image);
}

/**
 * nautilus_image_set_tile_pixbuf:
 *
 * @image: A NautilusImage
 * @pixbuf:          The new tile pixbuf
 *
 * Change the tile pixbuf.  A 'pixbuf' value of NULL, means dont use a
 * tile pixbuf - this is the default behavior for the widget.
 */
void
nautilus_image_set_tile_pixbuf (NautilusImage *image,
				GdkPixbuf *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	
	if (pixbuf != image->details->tile_pixbuf) {
		nautilus_gdk_pixbuf_unref_if_not_null (image->details->tile_pixbuf);
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		
		image->details->tile_pixbuf = pixbuf;

		gtk_widget_queue_draw (GTK_WIDGET (image));
	}
}

/**
 * nautilus_image_get_tile_pixbuf:
 *
 * @image: A NautilusImage
 *
 * Return value: A reference to the tile_pixbuf.  Needs to be unreferenced with 
 * gdk_pixbuf_unref()
 */
GdkPixbuf*
nautilus_image_get_tile_pixbuf (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);

	nautilus_gdk_pixbuf_ref_if_not_null (image->details->tile_pixbuf);
	
	return image->details->tile_pixbuf;
}

void
nautilus_image_set_pixbuf (NautilusImage *image,
			   GdkPixbuf *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	if (pixbuf != image->details->pixbuf) {
		nautilus_gdk_pixbuf_unref_if_not_null (image->details->pixbuf);
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		image->details->pixbuf = pixbuf;
		gtk_widget_queue_resize (GTK_WIDGET (image));
	}
}

void
nautilus_image_set_pixbuf_from_file_name (NautilusImage *image,
					  const char *file_name)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (file_name != NULL);

	pixbuf = gdk_pixbuf_new_from_file (file_name);			
	
	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (image, pixbuf);	
		gdk_pixbuf_unref (pixbuf);
	}
}

GdkPixbuf*
nautilus_image_get_pixbuf (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NULL);
	
	nautilus_gdk_pixbuf_ref_if_not_null (image->details->pixbuf);
	
	return image->details->pixbuf;
}

void
nautilus_image_set_pixbuf_opacity (NautilusImage *image,
				   int pixbuf_opacity)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (pixbuf_opacity >= NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (pixbuf_opacity <= NAUTILUS_OPACITY_FULLY_OPAQUE);

	if (image->details->pixbuf_opacity == pixbuf_opacity) {
		return;
	}

	image->details->pixbuf_opacity = pixbuf_opacity;

	gtk_widget_queue_draw (GTK_WIDGET (image));
}

int
nautilus_image_get_pixbuf_opacity (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NAUTILUS_OPACITY_FULLY_OPAQUE);

	return image->details->pixbuf_opacity;
}

void
nautilus_image_set_tile_opacity (NautilusImage *image,
				 int tile_opacity)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_opacity >= NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (tile_opacity <= NAUTILUS_OPACITY_FULLY_OPAQUE);

	if (image->details->tile_opacity == tile_opacity) {
		return;
	}

	image->details->tile_opacity = tile_opacity;

	gtk_widget_queue_draw (GTK_WIDGET (image));
}

int
nautilus_image_get_tile_opacity (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), NAUTILUS_OPACITY_FULLY_OPAQUE);

	return image->details->tile_opacity;
}

void
nautilus_image_set_tile_width (NautilusImage *image,
			       int tile_width)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_width >= NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);
	
	if (image->details->tile_width == tile_width) {
		return;
	}

	image->details->tile_width = tile_width;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

/**
 * nautilus_image_get_tile_width:
 *
 * @image: A NautilusImage
 *
 * Return value: The tile width.
 */
int
nautilus_image_get_tile_width (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);

	return image->details->tile_width;
}

void
nautilus_image_set_tile_height (NautilusImage *image,
				int tile_height)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_height >= NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);
	
	if (image->details->tile_height == tile_height) {
		return;
	}

	image->details->tile_height = tile_height;

	gtk_widget_queue_resize (GTK_WIDGET (image));
}

/**
 * nautilus_image_get_tile_height:
 *
 * @image: A NautilusImage
 *
 * Return value: The tile height.
 */
int
nautilus_image_get_tile_height (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);

	return image->details->tile_height;
}

void
nautilus_image_set_tile_mode_vertical (NautilusImage *image,
				       NautilusSmoothTileMode tile_mode_vertical)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_mode_vertical >= NAUTILUS_SMOOTH_TILE_SELF);
	g_return_if_fail (tile_mode_vertical <= NAUTILUS_SMOOTH_TILE_ANCESTOR);

	if (image->details->tile_mode_vertical == tile_mode_vertical) {
		return;
	}

	image->details->tile_mode_vertical = tile_mode_vertical;

	gtk_widget_queue_draw (GTK_WIDGET (image));
}

NautilusSmoothTileMode
nautilus_image_get_tile_mode_vertical (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->details->tile_mode_vertical;
}

void
nautilus_image_set_tile_mode_horizontal (NautilusImage *image,
					 NautilusSmoothTileMode tile_mode_horizontal)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_mode_horizontal >= NAUTILUS_SMOOTH_TILE_SELF);
	g_return_if_fail (tile_mode_horizontal <= NAUTILUS_SMOOTH_TILE_ANCESTOR);

	if (image->details->tile_mode_horizontal == tile_mode_horizontal) {
		return;
	}

	image->details->tile_mode_horizontal = tile_mode_horizontal;

	gtk_widget_queue_draw (GTK_WIDGET (image));
}

NautilusSmoothTileMode
nautilus_image_get_tile_mode_horizontal (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->details->tile_mode_horizontal;
}

void
nautilus_image_set_tile_pixbuf_from_file_name (NautilusImage *image,
					       const char *tile_file_name)
{
	GdkPixbuf *tile_pixbuf;

	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (tile_file_name != NULL);

	tile_pixbuf = gdk_pixbuf_new_from_file (tile_file_name);
	
	if (tile_pixbuf != NULL) {
		nautilus_image_set_tile_pixbuf (image, tile_pixbuf);
		gdk_pixbuf_unref (tile_pixbuf);
	}
}

void
nautilus_image_set_background_mode (NautilusImage *image,
				    NautilusSmoothBackgroundMode background_mode)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (background_mode >= NAUTILUS_SMOOTH_BACKGROUND_GTK);
	g_return_if_fail (background_mode <= NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);

	if (image->details->background_mode == background_mode) {
		return;
	}

	image->details->background_mode = background_mode;

	gtk_widget_queue_draw (GTK_WIDGET (image));
}

NautilusSmoothBackgroundMode
nautilus_image_get_background_mode (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->details->background_mode;
}

void
nautilus_image_set_solid_background_color (NautilusImage *image,
					   guint32 solid_background_color)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	
	if (image->details->solid_background_color == solid_background_color) {
		return;
	}

	image->details->solid_background_color = solid_background_color;
	
	gtk_widget_queue_draw (GTK_WIDGET (image));
}

guint32
nautilus_image_get_solid_background_color (const NautilusImage *image)
{
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (image), 0);
	
	return image->details->solid_background_color;
}

/**
 * nautilus_image_new_solid:
 *
 * @pixbuf: A GdkPixbuf or NULL.
 * @x_alignment: Horizontal alignment.
 * @y_alignment: Vertical alignment.
 * @x_padding: Horizontal padding.
 * @y_padding: Vertical padding.
 * @background_color: Background color.
 * @tile_pixbuf: A GdkPixbuf or NULL.
 *
 * Create an image with a solid background.
 *
 * Return value: The newly allocated NautilusImage with the
 * given attributes.
 */
GtkWidget *
nautilus_image_new_solid (GdkPixbuf *pixbuf,
			  float x_alignment,
			  float y_alignment,
			  int x_padding,
			  int y_padding,
			  guint32 background_color,
			  GdkPixbuf *tile_pixbuf)
{
	NautilusImage *image;

 	image = NAUTILUS_IMAGE (nautilus_image_new (NULL));
	
	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (image, pixbuf);
	}

	nautilus_image_set_background_mode (image, NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_image_set_solid_background_color (image, background_color);

	gtk_misc_set_padding (GTK_MISC (image), x_padding, y_padding);
	gtk_misc_set_alignment (GTK_MISC (image), x_alignment, y_alignment);
	
	if (tile_pixbuf != NULL) {
		nautilus_image_set_tile_pixbuf (image, tile_pixbuf);
	}
	
	return GTK_WIDGET (image);
}


/**
 * nautilus_image_set_never_smooth
 *
 * @image: A NautilusImage.
 * @never_smooth: A boolean value indicating whether the image can NEVER be smooth.
 *
 * Force an image to never be smooth.  Calls to nautilus_image_set_is_smooth () will
 * thus be ignored.  This is useful if you want to use a NautilusImage in a situation.
 */
void
nautilus_image_set_never_smooth (NautilusImage *image,
				 gboolean never_smooth)
{
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));

	image->details->never_smooth = never_smooth;
	gtk_widget_queue_resize (GTK_WIDGET (image));
}

