/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-graphic.c - A widget to display a composited pixbuf.

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

#include "nautilus-graphic.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"

#include <math.h>

/* FIXME bugzilla.eazel.com 1612: 
 * We should use NautilusBackground for the background.  This will simplify
 * lots of things, be more effecient, and remove the need for a lot of the
 * tiling code.
 */

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_COLOR,
	ARG_BACKGROUND_PIXBUF,
	ARG_BACKGROUND_TYPE,
	ARG_GRAPHIC,
	ARG_PLACEMENT_TYPE,
};

/* Detail member struct */
struct _NautilusGraphicDetail
{
	/* Attributes */
	NautilusGraphicPlacementType	placement_type;
	NautilusGraphicBackgroundType	background_type;
	GdkPixbuf			*background_pixbuf;
	guint32				background_color;
	GdkPixbuf			*pixbuf;
	GdkPoint			background_tile_origin;
	guchar				overall_alpha;
	gboolean			background_tile_screen_relative;

	gchar				*label_text;
	GdkFont				*label_font;

	GdkGC				*copy_area_gc;
	GdkPixbuf			*buffer;

	/* Offsets */
	guint				left_offset;
	guint				right_offset;
	guint				top_offset;
	guint				bottom_offset;

	guint				extra_width;
	guint				extra_height;
};

/* GtkObjectClass methods */
static void        nautilus_graphic_initialize_class          (NautilusGraphicClass *graphic_class);
static void        nautilus_graphic_initialize                (NautilusGraphic      *graphic);
static void        nautilus_graphic_destroy                   (GtkObject            *object);
static void        nautilus_graphic_set_arg                   (GtkObject            *object,
							       GtkArg               *arg,
							       guint                 arg_id);
static void        nautilus_graphic_get_arg                   (GtkObject            *object,
							       GtkArg               *arg,
							       guint                 arg_id);


/* GtkWidgetClass methods */
static void        nautilus_graphic_map                       (GtkWidget            *widget);
static void        nautilus_graphic_unmap                     (GtkWidget            *widget);
static void        nautilus_graphic_realize                   (GtkWidget            *widget);
static void        nautilus_graphic_unrealize                 (GtkWidget            *widget);
static void        nautilus_graphic_draw                      (GtkWidget            *widget,
							       GdkRectangle         *area);
static void        nautilus_graphic_size_request              (GtkWidget            *widget,
							       GtkRequisition       *requisition);
static void        nautilus_graphic_size_allocate             (GtkWidget            *widget,
							       GtkAllocation        *allocation);

/* GtkWidgetClass event methods */
static gint        nautilus_graphic_expose                    (GtkWidget            *widget,
							       GdkEventExpose       *event);
/* Private NautilusGraphic things */
static void        ensure_buffer_size                         (NautilusGraphic      *graphic,
							       guint                 width,
							       guint                 height);
static  GdkWindow* create_child_window                        (GdkWindow            *parent,
							       GdkVisual            *visual,
							       GdkColormap          *colormap,
							       gint                  x,
							       gint                  y,
							       guint                 width,
							       guint                 height,
							       gint                  event_mask);
static GdkGC *     nautilus_gdk_create_copy_area_gc           (GdkWindow            *window);
static void        nautilus_gdk_pixbuf_render_to_drawable     (const GdkPixbuf      *pixbuf,
							       GdkDrawable          *drawable,
							       GdkGC                *gc,
							       const GdkPoint       *source_point,
							       const GdkRectangle   *destination_area,
							       GdkRgbDither          dither);
static void        nautilus_gdk_pixbuf_render_to_pixbuf       (const GdkPixbuf      *pixbuf,
							       GdkPixbuf            *destination_pixbuf,
							       const GdkPoint       *source_point,
							       const GdkRectangle   *destination_area);
static void        nautilus_gdk_pixbuf_render_to_pixbuf_alpha (const GdkPixbuf      *pixbuf,
							       GdkPixbuf            *destination_pixbuf,
							       const GdkRectangle   *destination_area,
							       GdkInterpType         interpolation_mode,
							       guchar                overall_alpha);
static void        gdk_string_dimensions                      (const GdkFont        *font,
							       const gchar          *string,
							       GtkRequisition       *size);
static void        nautilus_gdk_pixbuf_set_to_color           (GdkPixbuf            *pixbuf,
							       guint32               color);
static void        nautilus_gdk_pixbuf_tile                   (GdkPixbuf            *pixbuf,
							       const GdkPixbuf      *tile_pixbuf,
							       guint                 tile_width,
							       guint                 tile_height,
							       gint                  tile_origin_x,
							       gint                  tile_origin_y);
static void        nautilus_gdk_pixbuf_tile_alpha             (GdkPixbuf            *pixbuf,
							       const GdkPixbuf      *tile_pixbuf,
							       guint                 tile_width,
							       guint                 tile_height,
							       gint                  tile_origin_x,
							       gint                  tile_origin_y,
							       GdkInterpType         interpolation_mode,
							       guchar                overall_alpha);

#define NAUTILUS_ALPHA_NONE 255

#define NAUTILUS_MACRO_BEGIN		G_STMT_START {
#define NAUTILUS_MACRO_END		} G_STMT_END

/* GdkPixbuf refcounting macros */
#define NAUTILUS_GDK_PIXBUF_UNREF_IF(_pixbuf)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_pixbuf) != NULL) {		\
	        gdk_pixbuf_unref (_pixbuf);	\
		(_pixbuf) = NULL;		\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_PIXBUF_REF_IF(_pixbuf)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_pixbuf) != NULL) {		\
	        gdk_pixbuf_ref (_pixbuf);	\
	}					\
NAUTILUS_MACRO_END

/* GdkGC refcounting macros */			\
#define NAUTILUS_GDK_GC_UNREF_IF(_gc)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_gc) != NULL) {			\
	        gdk_gc_unref (_gc);		\
		(_gc) = NULL;			\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_GC_REF_IF(_gc)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_gc) != NULL) {			\
	        gdk_gc_ref (_gc);		\
	}					\
NAUTILUS_MACRO_END

/* GdkFont refcounting macros */
#define NAUTILUS_GDK_FONT_UNREF_IF(_font)	\
NAUTILUS_MACRO_BEGIN				\
        if ((_font) != NULL) {			\
	        gdk_font_unref (_font);		\
		(_font) = NULL;			\
	}					\
NAUTILUS_MACRO_END

#define NAUTILUS_GDK_FONT_REF_IF(_font)		\
NAUTILUS_MACRO_BEGIN				\
        if ((_font) != NULL) {			\
	        gdk_font_ref (_font);		\
	}					\
NAUTILUS_MACRO_END

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusGraphic, nautilus_graphic, GTK_TYPE_WIDGET)

/* Class init methods */
	static void
	nautilus_graphic_initialize_class (NautilusGraphicClass *graphic_class)
{
	GtkObjectClass		*object_class = GTK_OBJECT_CLASS (graphic_class);
	GtkWidgetClass		*widget_class = GTK_WIDGET_CLASS (graphic_class);

	/* Arguments */
	gtk_object_add_arg_type ("NautilusGraphic::placement_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PLACEMENT_TYPE);

	gtk_object_add_arg_type ("NautilusGraphic::background_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_TYPE);

	gtk_object_add_arg_type ("NautilusGraphic::background_pixbuf",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_PIXBUF);

	gtk_object_add_arg_type ("NautilusGraphic::background_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_COLOR);

	gtk_object_add_arg_type ("NautilusGraphic::graphic",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_GRAPHIC);

	/* GtkObjectClass */
	object_class->destroy = nautilus_graphic_destroy;
	object_class->set_arg = nautilus_graphic_set_arg;
	object_class->get_arg = nautilus_graphic_get_arg;

	/* GtkWidgetClass */
	widget_class->realize = nautilus_graphic_realize;
	widget_class->unrealize = nautilus_graphic_unrealize;
	widget_class->draw = nautilus_graphic_draw;
	widget_class->map = nautilus_graphic_map;
	widget_class->unmap = nautilus_graphic_unmap;
	widget_class->expose_event = nautilus_graphic_expose;
	widget_class->size_request = nautilus_graphic_size_request;
	widget_class->size_allocate = nautilus_graphic_size_allocate;
}

void
nautilus_graphic_initialize (NautilusGraphic *graphic)
{
	GTK_WIDGET_SET_FLAGS (graphic, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (graphic, GTK_NO_WINDOW);

	graphic->detail = g_new (NautilusGraphicDetail, 1);

	graphic->detail->placement_type = NAUTILUS_GRAPHIC_PLACEMENT_CENTER;

	graphic->detail->background_color = NAUTILUS_RGB_COLOR_WHITE;
	graphic->detail->background_type = NAUTILUS_GRAPHIC_BACKGROUND_SOLID;
	graphic->detail->background_pixbuf = NULL;

	graphic->detail->pixbuf = NULL;

	graphic->detail->buffer = NULL;
	graphic->detail->copy_area_gc = NULL;
	graphic->detail->label_text = NULL;
	graphic->detail->label_font = gdk_font_load ("fixed");

	graphic->detail->background_tile_origin.x = 0;
	graphic->detail->background_tile_origin.y = 0;
	graphic->detail->overall_alpha = NAUTILUS_ALPHA_NONE;
	graphic->detail->background_tile_screen_relative = TRUE;

	graphic->detail->left_offset = 0;
	graphic->detail->right_offset = 0;
	graphic->detail->top_offset = 0;
	graphic->detail->bottom_offset = 0;

	graphic->detail->extra_width = 0;
	graphic->detail->extra_height = 0;
}

/* GtkObjectClass methods */
static void
nautilus_graphic_destroy (GtkObject *object)
{
 	NautilusGraphic *graphic;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (object));

	graphic = NAUTILUS_GRAPHIC (object);

	NAUTILUS_GDK_GC_UNREF_IF (graphic->detail->copy_area_gc);
	NAUTILUS_GDK_FONT_UNREF_IF (graphic->detail->label_font);

	NAUTILUS_GDK_PIXBUF_UNREF_IF (graphic->detail->buffer);
	NAUTILUS_GDK_PIXBUF_UNREF_IF (graphic->detail->background_pixbuf);
	NAUTILUS_GDK_PIXBUF_UNREF_IF (graphic->detail->pixbuf);

	g_free (graphic->detail->label_text);

	g_free (graphic->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_graphic_set_arg (GtkObject	*object,
			  GtkArg		*arg,
			  guint		arg_id)
{
	NautilusGraphic		*graphic;

 	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (object));

 	graphic = NAUTILUS_GRAPHIC (object);

 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		graphic->detail->placement_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_TYPE:
		graphic->detail->background_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_PIXBUF:
		nautilus_graphic_set_background_pixbuf (graphic, (GdkPixbuf*) GTK_VALUE_OBJECT (*arg));
		break;

	case ARG_BACKGROUND_COLOR:
		graphic->detail->background_color = GTK_VALUE_UINT (*arg);
		break;

	case ARG_GRAPHIC:
		nautilus_graphic_set_pixbuf (graphic, (GdkPixbuf*) GTK_VALUE_OBJECT (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_graphic_get_arg (GtkObject	*object,
			  GtkArg		*arg,
			  guint		arg_id)
{
	NautilusGraphic	*graphic;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (object));
	
	graphic = NAUTILUS_GRAPHIC (object);

 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		GTK_VALUE_ENUM (*arg) = graphic->detail->placement_type;
		break;
		
	case ARG_BACKGROUND_TYPE:
		GTK_VALUE_ENUM (*arg) = graphic->detail->background_type;
		break;

	case ARG_BACKGROUND_PIXBUF:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_graphic_get_background_pixbuf (graphic);
		break;

	case ARG_BACKGROUND_COLOR:
		GTK_VALUE_UINT (*arg) = graphic->detail->background_color;
		break;

	case ARG_GRAPHIC:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_graphic_get_pixbuf (graphic);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_graphic_realize (GtkWidget *widget)
{
	NautilusGraphic	*graphic;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	
	graphic = NAUTILUS_GRAPHIC (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	
	widget->window = create_child_window (gtk_widget_get_parent_window (widget),
					      gtk_widget_get_visual (widget),
					      gtk_widget_get_colormap (widget),
					      widget->allocation.x,
					      widget->allocation.y,
					      widget->allocation.width,
					      widget->allocation.height,
					      gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK);
	
	gdk_window_set_user_data (widget->window, widget);
	
	widget->style = gtk_style_attach (widget->style, widget->window);

 	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

	gdk_window_set_static_gravities (widget->window, TRUE);
	
	/* Create GCs */
	graphic->detail->copy_area_gc = nautilus_gdk_create_copy_area_gc (widget->window);
}

static void
nautilus_graphic_unrealize (GtkWidget *widget)
{
	NautilusGraphic	*graphic;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	
	graphic = NAUTILUS_GRAPHIC (widget);

	/* Chain unrealize */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, unrealize, (widget));

}

static void
nautilus_graphic_draw (GtkWidget *widget, GdkRectangle *area)
{
	NautilusGraphic	*graphic;
	GdkPoint	source_point;
 	GdkRectangle	destination_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	g_return_if_fail (area != NULL);

	graphic = NAUTILUS_GRAPHIC (widget);

	source_point.x = area->x;
	source_point.y = area->y;
	
 	destination_area.x = area->x;
 	destination_area.y = area->y;
 	destination_area.width = widget->allocation.width - area->x;
 	destination_area.height = widget->allocation.height - area->y;

	nautilus_gdk_pixbuf_render_to_drawable (graphic->detail->buffer,
						widget->window,
						graphic->detail->copy_area_gc,
						&source_point,
						&destination_area,
						GDK_INTERP_NEAREST);
}

static void
nautilus_graphic_size_allocate (GtkWidget *widget, GtkAllocation* allocation)
{
	NautilusGraphic *graphic;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	g_return_if_fail (allocation != NULL);

	graphic = NAUTILUS_GRAPHIC (widget);

	/* Assign the new allocation */
	widget->allocation.x = allocation->x;
	widget->allocation.y = allocation->y;
	widget->allocation.width = MAX (1, allocation->width);
	widget->allocation.height = MAX (1, allocation->height);

	/* If the widget is realized, move and resize its window */
	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
					allocation->x, 
					allocation->y,
					allocation->width, 
					allocation->height);

		if (graphic->detail->background_tile_screen_relative)
		{
			GdkWindow	*top_level;
			gint	top_level_x;
			gint	top_level_y;
			gint	graphic_x;
			gint	graphic_y;
			

			top_level = gdk_window_get_toplevel (widget->window);
			g_assert (top_level != NULL);

			gdk_window_get_origin (top_level, &top_level_x, &top_level_y);
			gdk_window_get_origin (widget->window, &graphic_x, &graphic_y);

#if 0
			g_print ("2 %p:top_level = (%d,%d), %p:graphic = (%d,%d), diff = (%d,%d)\n",
				 top_level,
				 top_level_x,
				 top_level_y,
				 widget->window,
				 graphic_x,
				 graphic_y,
				 top_level_x - graphic_x,
				 top_level_y - graphic_y);
#endif

 			graphic->detail->background_tile_origin.x = ABS (top_level_x - graphic_x);
 			graphic->detail->background_tile_origin.y = ABS (top_level_y - graphic_y);
		}
	}

	ensure_buffer_size (graphic, allocation->width, allocation->height);

	switch (graphic->detail->background_type)
	{
	case NAUTILUS_GRAPHIC_BACKGROUND_PIXBUF:
		if (graphic->detail->background_pixbuf != NULL)
		{
			nautilus_gdk_pixbuf_tile (graphic->detail->buffer,
						  graphic->detail->background_pixbuf,
						  gdk_pixbuf_get_width (graphic->detail->background_pixbuf),
						  gdk_pixbuf_get_height (graphic->detail->background_pixbuf),
						  graphic->detail->background_tile_origin.x, 
						  graphic->detail->background_tile_origin.y);
		}
		break;

	case NAUTILUS_GRAPHIC_BACKGROUND_SOLID:
		nautilus_gdk_pixbuf_set_to_color (graphic->detail->buffer, graphic->detail->background_color);
		break;
	}

	if (graphic ->detail->pixbuf != NULL)
	{
		switch (graphic->detail->placement_type)
		{
		case NAUTILUS_GRAPHIC_PLACEMENT_CENTER:
		{
			gint x;
			gint y;
			
			x = (widget->allocation.width - gdk_pixbuf_get_width (graphic->detail->pixbuf)) / 2;
			y = (widget->allocation.height - gdk_pixbuf_get_height (graphic->detail->pixbuf)) / 2;
			
			gdk_pixbuf_composite (graphic->detail->pixbuf,
					      graphic->detail->buffer, 
					      x,
					      y,
					      gdk_pixbuf_get_width (graphic->detail->pixbuf),
					      gdk_pixbuf_get_height (graphic->detail->pixbuf),
					      (double) x,
					      (double) y,
					      1.0,
					      1.0,
					      GDK_INTERP_BILINEAR,
					      graphic->detail->overall_alpha);
		}
		break;
		
		case NAUTILUS_GRAPHIC_PLACEMENT_TILE:
		{
			nautilus_gdk_pixbuf_tile_alpha (graphic->detail->buffer,
							graphic->detail->pixbuf,
							gdk_pixbuf_get_width (graphic->detail->pixbuf),
							gdk_pixbuf_get_height (graphic->detail->pixbuf),
							0,
							0,
							GDK_INTERP_BILINEAR,
							graphic->detail->overall_alpha);
		}
		break;
		}
	}

	if (graphic ->detail->label_text != NULL)
	{
		GtkRequisition text_size;
		gint x;
		gint y;
		ArtIRect text_rect;
			
		g_assert (graphic->detail->label_font != NULL);
		
		gdk_string_dimensions (graphic->detail->label_font,
				       graphic ->detail->label_text,
				       &text_size);

		x = widget->allocation.width - text_size.width - graphic->detail->right_offset;
		y = graphic->detail->top_offset;

// 		x = (widget->allocation.width - text_size.width) / 2;
//  		y = (widget->allocation.height - text_size.height) / 2;

		text_rect.x0 = x;
		text_rect.y0 = y;
		text_rect.x1 = x + text_size.width;
		text_rect.y1 = y + text_size.height;

/* FIXME bugzilla.eazel.com xxxx: 
 * Need to be able to pass in a rgb colot into the draw_text function.
 */
		nautilus_gdk_pixbuf_draw_text (graphic->detail->buffer,
					       graphic->detail->label_font,
					       &text_rect,
					       graphic->detail->label_text,
					       graphic->detail->overall_alpha);
	}
}

static void
nautilus_graphic_size_request (GtkWidget	*widget,
			       GtkRequisition	*requisition)
{
	GtkRequisition	pixbuf_size;
	GtkRequisition	text_size;
	NautilusGraphic	*graphic;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	g_return_if_fail (requisition != NULL);

	graphic = NAUTILUS_GRAPHIC (widget);
	
   	requisition->width = 10;
   	requisition->height = 10;

	pixbuf_size.width = 0;
	pixbuf_size.height = 0;
	text_size.width = 0;
	text_size.height = 0;
	
	if (graphic ->detail->pixbuf != NULL)
	{
		pixbuf_size.width = gdk_pixbuf_get_width (graphic->detail->pixbuf);
		pixbuf_size.height = gdk_pixbuf_get_height (graphic->detail->pixbuf);
	}

	if (graphic ->detail->label_text != NULL)
	{
		g_assert (graphic->detail->label_font != NULL);

		gdk_string_dimensions (graphic->detail->label_font,
				       graphic ->detail->label_text,
				       &text_size);
	}

   	requisition->width = 
		MAX (pixbuf_size.width, text_size.width) +
		graphic->detail->extra_width;

   	requisition->height = 
		MAX (pixbuf_size.height, text_size.height) +
		graphic->detail->extra_height;
}

static void
nautilus_graphic_map (GtkWidget *widget)
{
	NautilusGraphic *graphic;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	graphic = NAUTILUS_GRAPHIC (widget);
	
	gdk_window_show (widget->window);
}

static void
nautilus_graphic_unmap (GtkWidget *widget)
{
	NautilusGraphic *graphic;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (widget));
	
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);

	graphic = NAUTILUS_GRAPHIC (widget);
	
	gdk_window_hide (widget->window);
}

static gint
nautilus_graphic_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusGraphic	*graphic;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (widget), FALSE);
	
	graphic = NAUTILUS_GRAPHIC (widget);
	
	nautilus_graphic_draw (widget, &event->area);
	
	return TRUE;
}

/* Private NautilusGraphic things */
static void
ensure_buffer_size (NautilusGraphic	*graphic,
		    guint		new_width,
		    guint		new_height)
{
	guint old_width = 0;
	guint old_height = 0;

	g_assert (graphic != NULL);
	g_assert (NAUTILUS_IS_GRAPHIC (graphic));

	if (new_width == 0 || new_height == 0) {
		return;
	}

	if (graphic->detail->buffer != NULL) {
		old_width = gdk_pixbuf_get_width (graphic->detail->buffer);
		old_height = gdk_pixbuf_get_height (graphic->detail->buffer);
	}
	
	if (old_width < new_width || old_height < new_height) {
		GdkPixbuf *new_pixbuf = NULL;

		new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, new_width, new_height);
		
		if (graphic->detail->buffer) {
			gdk_pixbuf_unref (graphic->detail->buffer);
		}
		
		graphic->detail->buffer = new_pixbuf;
	}
}

static  GdkWindow*
create_child_window (GdkWindow   *parent,
		     GdkVisual   *visual,
		     GdkColormap *colormap,
		     gint	x,
		     gint	y,
		     guint	width,
		     guint	height,
		     gint	event_mask)
{
	GdkWindow	*window = NULL;
	GdkWindowAttr	attrib;
	gint		attrib_mask;
	
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (visual != NULL, NULL);
	g_return_val_if_fail (colormap != NULL, NULL);
	
	attrib.window_type = GDK_WINDOW_CHILD;
	attrib.x	   = x;
	attrib.y	   = y;
	attrib.width	   = MAX (1, width);
	attrib.height	   = MAX (1, height);
	attrib.wclass	   = GDK_INPUT_OUTPUT;
	attrib.visual	   = visual;
	attrib.colormap	   = colormap;
	attrib.event_mask  = event_mask;

	attrib_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	
	window = gdk_window_new (parent, &attrib, attrib_mask);

	return window;
}

static GdkGC *
nautilus_gdk_create_copy_area_gc (GdkWindow	*window)
{
	GdkGC *copy_area_gc;

	g_return_val_if_fail (window != NULL, NULL);

	copy_area_gc = gdk_gc_new (window);

	gdk_gc_set_function (copy_area_gc, GDK_COPY);

	return copy_area_gc;
}

static void
nautilus_gdk_pixbuf_render_to_drawable (const GdkPixbuf		*pixbuf,
					GdkDrawable		*drawable,
					GdkGC			*gc,
					const GdkPoint		*source_point,
					const GdkRectangle	*destination_area,
					GdkRgbDither		dither)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (destination_area != NULL);
 	g_return_if_fail (destination_area->width > 0);
 	g_return_if_fail (destination_area->height > 0);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);

	g_assert (gdk_pixbuf_get_width (pixbuf) > 0);
	g_assert (gdk_pixbuf_get_height (pixbuf) > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));

		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	gdk_pixbuf_render_to_drawable ((GdkPixbuf *) pixbuf,
				       drawable,
				       gc,
				       src.x,
				       src.y,
				       dst.x,
				       dst.y,
				       dst.width,
				       dst.height,
				       dither,
				       0,
				       0);
}

static void
nautilus_gdk_pixbuf_render_to_pixbuf (const GdkPixbuf		*pixbuf,
				      GdkPixbuf			*destination_pixbuf,
				      const GdkPoint		*source_point,
				      const GdkRectangle	*destination_area)
{
	GdkPoint	src;
	GdkRectangle	dst;
	GdkPoint	end;

	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (source_point != NULL);
	g_return_if_fail (source_point->x >= 0);
	g_return_if_fail (source_point->y >= 0);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	src = *source_point;
	dst = *destination_area;

	/* Clip to the left edge of the drawable */
	if (dst.x < 0)
	{
		src.x += ABS (dst.x);
		dst.x = 0;
	}

	/* Clip to the top edge of the drawable */
	if (dst.y < 0)
	{
		src.y += ABS (dst.y);
		dst.y = 0;
	}
	
	end.x = src.x + dst.width;
	end.y = src.y + dst.height;
	
	if (end.x >= gdk_pixbuf_get_width (pixbuf))
	{
		g_assert (dst.width >= (end.x - gdk_pixbuf_get_width (pixbuf)));

		dst.width -= (end.x - gdk_pixbuf_get_width (pixbuf));
	}

	if (end.y >= gdk_pixbuf_get_height (pixbuf))
	{
		g_assert (dst.height >= (end.y - gdk_pixbuf_get_height (pixbuf)));

		dst.height -= (end.y - gdk_pixbuf_get_height (pixbuf));
	}

	gdk_pixbuf_copy_area ((GdkPixbuf *) pixbuf,
			      src.x,
			      src.y,
			      dst.width,
			      dst.height,
			      destination_pixbuf,
			      dst.x,
			      dst.y);
}

static void
nautilus_gdk_pixbuf_render_to_pixbuf_alpha (const GdkPixbuf        *pixbuf,
					    GdkPixbuf              *destination_pixbuf,
					    const GdkRectangle      *destination_area,
					    GdkInterpType     interpolation_mode,
					    guchar                    overall_alpha)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (destination_area->width > 0);
	g_return_if_fail (destination_area->height > 0);

	gdk_pixbuf_composite (pixbuf,
			      destination_pixbuf,
			      destination_area->x,
			      destination_area->y,
			      destination_area->width,
			      destination_area->height,
			      (double) destination_area->x,
			      (double) destination_area->y,
			      1.0,
			      1.0,
			      interpolation_mode,
			      overall_alpha);
}

static void
gdk_string_dimensions (const GdkFont	*font,
		       const gchar	*string,
		       GtkRequisition	*size)
{
	g_return_if_fail (font != NULL);
	g_return_if_fail (size != NULL);

	size->width = 0;
	size->height = 0;

	if (string && (strlen (string) > 0))
	{
		gint	width;
		gint	lbearing;
		gint	rbearing;
		gint	ascent;
		gint	descent;
		
		gdk_text_extents ((GdkFont *) font,
				  string,
				  strlen (string),
				  &lbearing,
				  &rbearing,
				  &width,
				  &ascent,
				  &descent);

		size->width = width;
		size->height = ascent + descent;
	}
}

static void
nautilus_gdk_pixbuf_set_to_color (GdkPixbuf	*pixbuf,
				  guint32     color)
{
	guchar			*pixels;
	guint			width;
	guint			height;
	guint			rowstride;
	guint			x;
	guint			y;
	guchar			*offset;

	g_return_if_fail (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	if (gdk_pixbuf_get_has_alpha (pixbuf))
	{
		for (y = 0; y < height; y++) 
		{
			offset = pixels + y * rowstride;
			
			for (x = 0; x < width; x++) 
			{
				*offset++ = NAUTILUS_RGBA_COLOR_GET_R (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_G (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_B (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_A (color);
			}
		}
	}
	else
	{
		for (y = 0; y < height; y++) 
		{
			offset = pixels + y * rowstride;
			
			for (x = 0; x < width; x++) 
			{
				*offset++ = NAUTILUS_RGBA_COLOR_GET_R (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_G (color);
				*offset++ = NAUTILUS_RGBA_COLOR_GET_B (color);
			}
		}
	}
}

/* FIXME bugzilla.eazel.com 1612: 
 * Tile origin should be respected.  Should be fixed when I use NautilusBackground.
 */
static void
nautilus_gdk_pixbuf_tile (GdkPixbuf		*pixbuf,
			  const GdkPixbuf	*tile_pixbuf,
			  guint			tile_width,
			  guint			tile_height,
			  gint			tile_origin_x,
			  gint			tile_origin_y)
{
	gint	x;
	gint	y;
	guchar	*pixels;
	guchar	*tile_pixels;
	guint	num_ver_iterations;
	guint	num_hor_iterations;
	guint	i;
	guint	j;
	guint	width;
	guint	height;
	
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (tile_pixbuf != NULL);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (tile_pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (tile_pixbuf));
	g_return_if_fail (gdk_pixbuf_get_pixels (pixbuf) != NULL);
	g_return_if_fail (gdk_pixbuf_get_pixels (tile_pixbuf) != NULL);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	tile_pixels = gdk_pixbuf_get_pixels (tile_pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	num_ver_iterations = ceil (height / tile_height) + 1;
	num_hor_iterations = ceil (width / tile_width) + 1;

	y = 0;

	for (j = 0; j < num_ver_iterations; j++)
	{
		x = 0;

		for (i = 0; i < num_hor_iterations; i++)
		{
			guint   copy_width;
			guint   copy_height;
			gint    dst_x;
			gint    dst_y;
			gint    dst_x2;
			gint    dst_y2;

			dst_x = x;
			dst_y = y;

			copy_width = tile_width;
			copy_height = tile_height;

			dst_x2 = dst_x + copy_width;
			dst_y2 = dst_y + copy_height;

			if (dst_x2 > width)
			{
				copy_width -= (dst_x2 - width);
			}

			if (dst_y2 > height)
			{
				copy_height -= (dst_y2 - height);
			}

			if (copy_width > 0 && copy_height > 0)
			{
				GdkPoint	source_point;
				GdkRectangle	destination_area;

				source_point.x = 0;
				source_point.y = 0;

				destination_area.x = dst_x;
				destination_area.y = dst_y;
				destination_area.width = copy_width;
				destination_area.height = copy_height;
				
				nautilus_gdk_pixbuf_render_to_pixbuf (tile_pixbuf,
								      pixbuf,
								      &source_point,
								      &destination_area);
			}

			x += tile_width;
		}

		y += tile_height;
	}
}

static void
nautilus_gdk_pixbuf_tile_alpha (GdkPixbuf		*pixbuf,
				const GdkPixbuf		*tile_pixbuf,
				guint			tile_width,
				guint			tile_height,
				gint			tile_origin_x,
				gint			tile_origin_y,
				GdkInterpType		interpolation_mode,
				guchar			overall_alpha)
{
	gint	x;
	gint	y;
	guchar	*pixels;
	guchar	*tile_pixels;
	guint	num_ver_iterations;
	guint	num_hor_iterations;
	guint	i;
	guint	j;
	guint	width;
	guint	height;
	
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (tile_pixbuf != NULL);
	g_return_if_fail (tile_width <= gdk_pixbuf_get_width (tile_pixbuf));
	g_return_if_fail (tile_height <= gdk_pixbuf_get_height (tile_pixbuf));
	g_return_if_fail (gdk_pixbuf_get_pixels (pixbuf) != NULL);
	g_return_if_fail (gdk_pixbuf_get_pixels (tile_pixbuf) != NULL);

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	tile_pixels = gdk_pixbuf_get_pixels (tile_pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	num_ver_iterations = ceil (height / tile_height) + 1;
	num_hor_iterations = ceil (width / tile_width) + 1;

	y = 0;

	for (j = 0; j < num_ver_iterations; j++)
	{
		x = 0;

		for (i = 0; i < num_hor_iterations; i++)
		{
			guint   copy_width;
			guint   copy_height;
			gint    dst_x;
			gint    dst_y;
			gint    dst_x2;
			gint    dst_y2;

			dst_x = x;
			dst_y = y;

			copy_width = tile_width;
			copy_height = tile_height;

			dst_x2 = dst_x + copy_width;
			dst_y2 = dst_y + copy_height;

			if (dst_x2 > width)
			{
				copy_width -= (dst_x2 - width);
			}

			if (dst_y2 > height)
			{
				copy_height -= (dst_y2 - height);
			}

			if (copy_width > 0 && copy_height > 0)
			{
				GdkRectangle destination_area;

				destination_area.x = dst_x;
				destination_area.y = dst_y;
				destination_area.width = copy_width;
				destination_area.height = copy_height;
				
				nautilus_gdk_pixbuf_render_to_pixbuf_alpha (tile_pixbuf,
									    pixbuf,
									    &destination_area,
									    interpolation_mode,
									    overall_alpha);
			}

			x += tile_width;
		}

		y += tile_height;
	}
}

/* Public NautilusGraphic */
GtkWidget*
nautilus_graphic_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_graphic_get_type ()));
}

void
nautilus_graphic_set_background_pixbuf (NautilusGraphic *graphic, GdkPixbuf *background_pixbuf)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	if (background_pixbuf != graphic->detail->background_pixbuf)
	{
		NAUTILUS_GDK_PIXBUF_UNREF_IF (graphic->detail->background_pixbuf);
		
		NAUTILUS_GDK_PIXBUF_REF_IF (background_pixbuf);
		
		graphic->detail->background_pixbuf = background_pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

GdkPixbuf*
nautilus_graphic_get_background_pixbuf (const NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), NULL);

	NAUTILUS_GDK_PIXBUF_REF_IF (graphic->detail->background_pixbuf);
	
	return graphic->detail->background_pixbuf;
}

void
nautilus_graphic_set_background_type (NautilusGraphic *graphic, NautilusGraphicBackgroundType background_type)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	if (background_type == graphic->detail->background_type)
	{
		return;
	}

	graphic->detail->background_type = background_type;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

NautilusGraphicBackgroundType
nautilus_graphic_get_background_type (const NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), 0);
	
	return graphic->detail->background_type;
}

void
nautilus_graphic_set_background_color (NautilusGraphic *graphic, guint32 background_color)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	if (background_color == graphic->detail->background_color)
	{
		return;
	}

	graphic->detail->background_color = background_color;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

guint32
nautilus_graphic_get_background_color (const NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), 0);
	
	return graphic->detail->background_color;
}

void
nautilus_graphic_set_placement_type (NautilusGraphic *graphic, NautilusGraphicPlacementType placement_type)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	if (placement_type == graphic->detail->placement_type)
	{
		return;
	}

	graphic->detail->placement_type = placement_type;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

NautilusGraphicPlacementType
nautilus_graphic_get_placement_type (const NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), 0);
	
	return graphic->detail->placement_type;
}

void
nautilus_graphic_set_pixbuf (NautilusGraphic *graphic, GdkPixbuf *pixbuf)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	if (pixbuf != graphic->detail->pixbuf)
	{
		NAUTILUS_GDK_PIXBUF_UNREF_IF (graphic->detail->pixbuf);
		
		NAUTILUS_GDK_PIXBUF_REF_IF (pixbuf);
		
		graphic->detail->pixbuf = pixbuf;
	}

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

GdkPixbuf*
nautilus_graphic_get_pixbuf (const NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), NULL);

	NAUTILUS_GDK_PIXBUF_REF_IF (graphic->detail->pixbuf);
	
	return graphic->detail->pixbuf;
}

void
nautilus_graphic_set_overall_alpha (NautilusGraphic	*graphic,
				   guchar		pixbuf_alpha)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->overall_alpha = pixbuf_alpha;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_label_text (NautilusGraphic	*graphic,
				 const gchar		*text)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	g_free (graphic->detail->label_text);
	
	graphic->detail->label_text = text ? g_strdup (text) : NULL;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

gchar*
nautilus_graphic_get_label_text (NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), NULL);

	return graphic->detail->label_text ? g_strdup (graphic->detail->label_text) : NULL;
}

void
nautilus_graphic_set_label_font (NautilusGraphic	*graphic,
				 GdkFont		*font)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	NAUTILUS_GDK_FONT_UNREF_IF (graphic->detail->label_font);

	graphic->detail->label_font = font;

	NAUTILUS_GDK_FONT_REF_IF (graphic->detail->label_font);

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

GdkFont *
nautilus_graphic_get_label_font (NautilusGraphic *graphic)
{
	g_return_val_if_fail (graphic != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_GRAPHIC (graphic), NULL);

	NAUTILUS_GDK_FONT_REF_IF (graphic->detail->label_font);

	return graphic->detail->label_font;
}

void
nautilus_graphic_set_left_offset (NautilusGraphic	*graphic,
				  guint			left_offset)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->left_offset = left_offset;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_right_offset (NautilusGraphic	*graphic,
				   guint		right_offset)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->right_offset = right_offset;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_top_offset (NautilusGraphic	*graphic,
				 guint			top_offset)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->top_offset = top_offset;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_bottom_offset (NautilusGraphic	*graphic,
				    guint		bottom_offset)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->bottom_offset = bottom_offset;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_extra_width (NautilusGraphic	*graphic,
				    guint		extra_width)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->extra_width = extra_width;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

void
nautilus_graphic_set_extra_height (NautilusGraphic	*graphic,
				    guint		extra_height)
{
	g_return_if_fail (graphic != NULL);
	g_return_if_fail (NAUTILUS_IS_GRAPHIC (graphic));

	graphic->detail->extra_height = extra_height;

	gtk_widget_queue_resize (GTK_WIDGET (graphic));
}

