/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-canvas-note-item.c: annotation canvas item for nautilus implementation
   
   Copyright (C) 2001 Eazel, Inc.
  
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

   based on gnome_canvas_rect_item by Federico Mena Quintero
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include <math.h>

#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-icon-text.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_rgb_svp.h>

#include "nautilus-annotation.h"
#include "nautilus-canvas-note-item.h"
#include "nautilus-font-factory.h"
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-smooth-text-layout.h>
#include <eel/eel-string.h>

enum {
	ARG_0,
	ARG_X1,
	ARG_Y1,
	ARG_X2,
	ARG_Y2,
	ARG_FILL_COLOR,
	ARG_FILL_COLOR_GDK,
	ARG_FILL_COLOR_RGBA,
	ARG_NOTE_TEXT,
	ARG_OUTLINE_COLOR,
	ARG_OUTLINE_COLOR_GDK,
	ARG_OUTLINE_COLOR_RGBA,
	ARG_FILL_STIPPLE,
	ARG_OUTLINE_STIPPLE,
	ARG_WIDTH_PIXELS,
	ARG_WIDTH_UNITS
};

#define ANNOTATION_WIDTH 240
#define DEFAULT_FONT_SIZE 12
#define LINE_BREAK_CHARACTERS " -_,;.?/&"

#define ARROW_HEIGHT 16
#define MIN_ARROW_HALF_WIDTH 4
#define MAX_ARROW_HALF_WIDTH 12

static void nautilus_canvas_note_item_class_init (NautilusCanvasNoteItemClass *class);
static void nautilus_canvas_note_item_init       (NautilusCanvasNoteItem      *note_item);
static void nautilus_canvas_note_item_destroy    (GtkObject          *object);
static void nautilus_canvas_note_item_set_arg    (GtkObject          *object,
						   GtkArg	      *arg,
						    guint             arg_id);
static void nautilus_canvas_note_item_get_arg    (GtkObject          *object,
						   GtkArg             *arg,
						   guint               arg_id);

static void nautilus_canvas_note_item_realize     (GnomeCanvasItem *item);
static void nautilus_canvas_note_item_unrealize   (GnomeCanvasItem *item);
static void nautilus_canvas_note_item_translate   (GnomeCanvasItem *item, double dx, double dy);
static void nautilus_canvas_note_item_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);

static void nautilus_canvas_note_item_draw	  (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height);
static void nautilus_canvas_note_item_render      (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static void nautilus_canvas_note_item_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static double nautilus_canvas_note_item_point	  (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);

static GnomeCanvasItemClass *note_item_parent_class;


GtkType
nautilus_canvas_note_item_get_type (void)
{
	static GtkType note_item_type = 0;

	if (!note_item_type) {
		GtkTypeInfo note_item_info = {
			"NautilusCanvasNoteItem",
			sizeof (NautilusCanvasNoteItem),
			sizeof (NautilusCanvasNoteItemClass),
			(GtkClassInitFunc) nautilus_canvas_note_item_class_init,
			(GtkObjectInitFunc) nautilus_canvas_note_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		note_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &note_item_info);
	}

	return note_item_type;
}

static void
nautilus_canvas_note_item_class_init (NautilusCanvasNoteItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	note_item_parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("NautilusCanvasNoteItem::x1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::y1", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::x2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::y2", GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y2);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::fill_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_FILL_COLOR);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::fill_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_FILL_COLOR_GDK);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::fill_color_rgba", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_FILL_COLOR_RGBA);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::note_text", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_NOTE_TEXT);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::outline_color", GTK_TYPE_STRING, GTK_ARG_WRITABLE, ARG_OUTLINE_COLOR);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::outline_color_gdk", GTK_TYPE_GDK_COLOR, GTK_ARG_READWRITE, ARG_OUTLINE_COLOR_GDK);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::outline_color_rgba", GTK_TYPE_UINT, GTK_ARG_READWRITE, ARG_OUTLINE_COLOR_RGBA);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::fill_stipple", GTK_TYPE_GDK_WINDOW, GTK_ARG_READWRITE, ARG_FILL_STIPPLE);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::outline_stipple", GTK_TYPE_GDK_WINDOW, GTK_ARG_READWRITE, ARG_OUTLINE_STIPPLE);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::width_pixels", GTK_TYPE_UINT, GTK_ARG_WRITABLE, ARG_WIDTH_PIXELS);
	gtk_object_add_arg_type ("NautilusCanvasNoteItem::width_units", GTK_TYPE_DOUBLE, GTK_ARG_WRITABLE, ARG_WIDTH_UNITS);

	object_class->destroy = nautilus_canvas_note_item_destroy;
	object_class->set_arg = nautilus_canvas_note_item_set_arg;
	object_class->get_arg = nautilus_canvas_note_item_get_arg;

	item_class->realize = nautilus_canvas_note_item_realize;
	item_class->unrealize = nautilus_canvas_note_item_unrealize;
	item_class->translate = nautilus_canvas_note_item_translate;
	item_class->bounds = nautilus_canvas_note_item_bounds;

	item_class->draw = nautilus_canvas_note_item_draw;
	item_class->point = nautilus_canvas_note_item_point;
	item_class->update = nautilus_canvas_note_item_update;
	item_class->render = nautilus_canvas_note_item_render;
}

static void
nautilus_canvas_note_item_init (NautilusCanvasNoteItem *note_item)
{
	note_item->width = 0.0;
	note_item->fill_svp = NULL;
	note_item->outline_svp = NULL;
}

static void
nautilus_canvas_note_item_destroy (GtkObject *object)
{
	NautilusCanvasNoteItem *note_item;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_CANVAS_NOTE_ITEM (object));

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (object);

	if (note_item->fill_stipple)
		gdk_bitmap_unref (note_item->fill_stipple);

	if (note_item->outline_stipple)
		gdk_bitmap_unref (note_item->outline_stipple);

	if (note_item->fill_svp)
		art_svp_free (note_item->fill_svp);

	if (note_item->outline_svp)
		art_svp_free (note_item->outline_svp);

	if (note_item->note_text)
		g_free (note_item->note_text);
		
	if (GTK_OBJECT_CLASS (note_item_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (note_item_parent_class)->destroy) (object);
}

static void get_bounds (NautilusCanvasNoteItem *note_item, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double x1, y1, x2, y2;
	int cx1, cy1, cx2, cy2;
	double hwidth;

	item = GNOME_CANVAS_ITEM (note_item);

	if (note_item->width_pixels)
		hwidth = (note_item->width / item->canvas->pixels_per_unit) / 2.0;
	else
		hwidth = note_item->width / 2.0;

	x1 = note_item->x1;
	y1 = note_item->y1;
	x2 = note_item->x2;
	y2 = note_item->y2;

	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &y2);
	gnome_canvas_w2c (item->canvas, x1 - hwidth, y1 - hwidth, &cx1, &cy1);
	gnome_canvas_w2c (item->canvas, x2 + hwidth, y2 + hwidth, &cx2, &cy2);
	*px1 = cx1;
	*py1 = cy1;
	*px2 = cx2;
	*py2 = cy2;

	/* Some safety fudging */

	*px1 -= 2;
	*py1 -= 2;
	*px2 += 2;
	*py2 += 2;
}

/* Convenience function to set a GC's foreground color to the specified pixel value */
static void
set_gc_foreground (GdkGC *gc, gulong pixel)
{
	GdkColor c;

	if (!gc)
		return;

	c.pixel = pixel;
	gdk_gc_set_foreground (gc, &c);
}

/* Sets the stipple pattern for the specified gc */
static void
set_stipple (GdkGC *gc, GdkBitmap **internal_stipple, GdkBitmap *stipple, int reconfigure)
{
	if (*internal_stipple && !reconfigure)
		gdk_bitmap_unref (*internal_stipple);

	*internal_stipple = stipple;
	if (stipple && !reconfigure)
		gdk_bitmap_ref (stipple);

	if (gc) {
		if (stipple) {
			gdk_gc_set_stipple (gc, stipple);
			gdk_gc_set_fill (gc, GDK_STIPPLED);
		} else
			gdk_gc_set_fill (gc, GDK_SOLID);
	}
}

/* Recalculate the outline width of the rectangle/ellipse and set it in its GC */
static void
set_outline_gc_width (NautilusCanvasNoteItem *note_item)
{
	int width;

	if (!note_item->outline_gc)
		return;

	if (note_item->width_pixels)
		width = (int) note_item->width;
	else
		width = (int) (note_item->width * note_item->item.canvas->pixels_per_unit + 0.5);

	gdk_gc_set_line_attributes (note_item->outline_gc, width,
				    GDK_LINE_SOLID, GDK_CAP_PROJECTING, GDK_JOIN_MITER);
}

/* utility to update the canvas item bounding box from the note item's private bounding box */
static void
update_item_bounding_box (NautilusCanvasNoteItem *note_item)
{
	GnomeCanvasItem *item;
	item  = GNOME_CANVAS_ITEM (note_item);
	
	item->x1 = note_item->x1;
	item->y1 = note_item->y1;
	item->x2 = note_item->x2 + 1;
	item->y2 = note_item->y2 + 1;
}

static void
nautilus_canvas_note_item_set_note_text (NautilusCanvasNoteItem *note_item, const char *new_text)
{
	char *display_text;
	int total_width, height, width, font_height;
	GnomeCanvasItem *item;
	EelScalableFont *scalable_font;
	GdkFont *font;
	EelDimensions dimensions;
	
	item = GNOME_CANVAS_ITEM (note_item);
	
	if (note_item->note_text) {
		g_free (note_item->note_text);
	}

	height = 0; width = 0; /* to avoid compiler complaint */
	note_item->note_text = g_strdup (new_text);

	/* set the width and height based on the display text */
	/* this will get more sophisticated as we get fancier */
	display_text = nautilus_annotation_get_display_text (new_text);	
	
	if (item->canvas->aa) {
		scalable_font = eel_scalable_font_get_default_font ();
		dimensions = eel_scalable_font_measure_text (scalable_font,
								  DEFAULT_FONT_SIZE, 
								  display_text, 
								  strlen (display_text));
		total_width = dimensions.width + 8;
		height = dimensions.height * (1 + (total_width / ANNOTATION_WIDTH));
		height += ARROW_HEIGHT;
		gtk_object_unref (GTK_OBJECT (scalable_font));
	} else {
		font = nautilus_font_factory_get_font_from_preferences (DEFAULT_FONT_SIZE);				
		total_width = 8 + gdk_text_measure (font, display_text, strlen (display_text));
		font_height = gdk_text_height (font, display_text, strlen (display_text));
		height =  font_height * (1 + (total_width / ANNOTATION_WIDTH));
		gdk_font_unref (font);
	}
	
	width = (total_width < ANNOTATION_WIDTH) ? total_width : ANNOTATION_WIDTH;
	
	/* add some vertical slop for descenders and incorporate scale factor */
	note_item->x2 = note_item->x1 + (width / item->canvas->pixels_per_unit);
	note_item->y2 = note_item->y1 + 4.0 + (height / item->canvas->pixels_per_unit);
	
	
	update_item_bounding_box (note_item);
	
	g_free (display_text);
}

static void
nautilus_canvas_note_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	NautilusCanvasNoteItem *note_item;
	GdkColor color = { 0, 0, 0, 0, };
	GdkColor *pcolor;
	int have_pixel;

	item = GNOME_CANVAS_ITEM (object);
	note_item = NAUTILUS_CANVAS_NOTE_ITEM (object);
	have_pixel = FALSE;

	switch (arg_id) {
	case ARG_X1:
		note_item->x1 = GTK_VALUE_DOUBLE (*arg);
		update_item_bounding_box (note_item);
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y1:
		note_item->y1 = GTK_VALUE_DOUBLE (*arg);
		update_item_bounding_box (note_item);
		gnome_canvas_item_request_update (item);
		break;

	case ARG_X2:
		note_item->x2 = GTK_VALUE_DOUBLE (*arg);
		update_item_bounding_box (note_item);
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y2:
		note_item->y2 = GTK_VALUE_DOUBLE (*arg);
		update_item_bounding_box (note_item);
		gnome_canvas_item_request_update (item);
		break;

	case ARG_FILL_COLOR:
	case ARG_FILL_COLOR_GDK:
	case ARG_FILL_COLOR_RGBA:
		switch (arg_id) {
		case ARG_FILL_COLOR:
			gdk_color_parse (GTK_VALUE_STRING (*arg), &color);

			note_item->fill_color = ((color.red & 0xff00) << 16 |
					  (color.green & 0xff00) << 8 |
					  (color.blue & 0xff00) |
					  0xff);
			break;

		case ARG_FILL_COLOR_GDK:
			pcolor = GTK_VALUE_BOXED (*arg);
			if (pcolor) {
				color = *pcolor;
				gdk_color_context_query_color (item->canvas->cc, &color);
				have_pixel = TRUE;
			}

			note_item->fill_color = ((color.red & 0xff00) << 16 |
					  (color.green & 0xff00) << 8 |
					  (color.blue & 0xff00) |
					  0xff);
			break;

		case ARG_FILL_COLOR_RGBA:
			note_item->fill_color = GTK_VALUE_UINT (*arg);
			break;
		}

		if (have_pixel)
			note_item->fill_pixel = color.pixel;
		else
			note_item->fill_pixel = gnome_canvas_get_color_pixel (item->canvas, note_item->fill_color);

		if (!item->canvas->aa)
			set_gc_foreground (note_item->fill_gc, note_item->fill_pixel);

		gnome_canvas_item_request_redraw_svp (item, note_item->fill_svp);
		break;

	case ARG_OUTLINE_COLOR:
	case ARG_OUTLINE_COLOR_GDK:
	case ARG_OUTLINE_COLOR_RGBA:
		switch (arg_id) {
		case ARG_OUTLINE_COLOR:
			gdk_color_parse (GTK_VALUE_STRING (*arg), &color);

			note_item->outline_color = ((color.red & 0xff00) << 16 |
					     (color.green & 0xff00) << 8 |
					     (color.blue & 0xff00) |
					     0xff);
			break;

		case ARG_OUTLINE_COLOR_GDK:
			pcolor = GTK_VALUE_BOXED (*arg);
			if (pcolor) {
				color = *pcolor;
				gdk_color_context_query_color (item->canvas->cc, &color);
				have_pixel = TRUE;
			}

			note_item->outline_color = ((color.red & 0xff00) << 16 |
					     (color.green & 0xff00) << 8 |
					     (color.blue & 0xff00) |
					     0xff);
			break;

		case ARG_OUTLINE_COLOR_RGBA:
			note_item->outline_color = GTK_VALUE_UINT (*arg);
			break;
		}

		if (have_pixel)
			note_item->outline_pixel = color.pixel;
		else
			note_item->outline_pixel = gnome_canvas_get_color_pixel (item->canvas,
									  note_item->outline_color);

		if (!item->canvas->aa)
			set_gc_foreground (note_item->outline_gc, note_item->outline_pixel);

		gnome_canvas_item_request_redraw_svp (item, note_item->outline_svp);
		break;

	case ARG_NOTE_TEXT:
		nautilus_canvas_note_item_set_note_text (note_item, GTK_VALUE_STRING (*arg));
		break;
		
	case ARG_FILL_STIPPLE:
		if (!item->canvas->aa)
			set_stipple (note_item->fill_gc, &note_item->fill_stipple, GTK_VALUE_BOXED (*arg), FALSE);

		break;

	case ARG_OUTLINE_STIPPLE:
		if (!item->canvas->aa)
			set_stipple (note_item->outline_gc, &note_item->outline_stipple, GTK_VALUE_BOXED (*arg), FALSE);
		break;

	case ARG_WIDTH_PIXELS:
		note_item->width = GTK_VALUE_UINT (*arg);
		note_item->width_pixels = TRUE;
		if (!item->canvas->aa)
			set_outline_gc_width (note_item);

		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH_UNITS:
		note_item->width = fabs (GTK_VALUE_DOUBLE (*arg));
		note_item->width_pixels = FALSE;
		if (!item->canvas->aa)
			set_outline_gc_width (note_item);

		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

/* Allocates a GdkColor structure filled with the specified pixel, and puts it into the specified
 * arg for returning it in the get_arg method.
 */
static void
get_color_arg (NautilusCanvasNoteItem *note_item, gulong pixel, GtkArg *arg)
{
	GdkColor *color;

	color = g_new (GdkColor, 1);
	color->pixel = pixel;
	gdk_color_context_query_color (GNOME_CANVAS_ITEM (note_item)->canvas->cc, color);
	GTK_VALUE_BOXED (*arg) = color;
}

static void
nautilus_canvas_note_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusCanvasNoteItem *note_item;
	GnomeCanvasItem *item;
	
	item = GNOME_CANVAS_ITEM (object);
	note_item = NAUTILUS_CANVAS_NOTE_ITEM (object);

	switch (arg_id) {
	case ARG_X1:
		GTK_VALUE_DOUBLE (*arg) = note_item->x1;
		break;

	case ARG_Y1:
		GTK_VALUE_DOUBLE (*arg) = note_item->y1;
		break;

	case ARG_X2:
		GTK_VALUE_DOUBLE (*arg) = note_item->x2;
		break;

	case ARG_Y2:
		GTK_VALUE_DOUBLE (*arg) = note_item->y2;
		break;

	case ARG_FILL_COLOR_GDK:
		get_color_arg (note_item, note_item->fill_pixel, arg);
		break;

	case ARG_OUTLINE_COLOR_GDK:
		get_color_arg (note_item, note_item->outline_pixel, arg);
		break;

	case ARG_FILL_COLOR_RGBA:
		GTK_VALUE_UINT (*arg) = note_item->fill_color;
		break;

	case ARG_OUTLINE_COLOR_RGBA:
		GTK_VALUE_UINT (*arg) = note_item->outline_color;
		break;

	case ARG_FILL_STIPPLE:
		GTK_VALUE_BOXED (*arg) = note_item->fill_stipple;
		break;

	case ARG_OUTLINE_STIPPLE:
		GTK_VALUE_BOXED (*arg) = note_item->outline_stipple;
		break;

	case ARG_NOTE_TEXT:
		GTK_VALUE_STRING (*arg) = note_item->note_text;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
nautilus_canvas_note_item_realize (GnomeCanvasItem *item)
{
	NautilusCanvasNoteItem *note_item;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	if (note_item_parent_class->realize)
		(* note_item_parent_class->realize) (item);

	if (!item->canvas->aa) {
		note_item->fill_gc = gdk_gc_new (item->canvas->layout.bin_window);
		note_item->outline_gc = gdk_gc_new (item->canvas->layout.bin_window);
	}
}

static void
nautilus_canvas_note_item_unrealize (GnomeCanvasItem *item)
{
	NautilusCanvasNoteItem *note_item;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	if (!item->canvas->aa) {
		gdk_gc_unref (note_item->fill_gc);
		note_item->fill_gc = NULL;
		gdk_gc_unref (note_item->outline_gc);
		note_item->outline_gc = NULL;
	}

	if (note_item_parent_class->unrealize)
		(* note_item_parent_class->unrealize) (item);
}

static void
nautilus_canvas_note_item_translate (GnomeCanvasItem *item, double dx, double dy)
{
	NautilusCanvasNoteItem *note_item;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);
	
	note_item->x1 += dx;
	note_item->y1 += dy;
	note_item->x2 += dx;
	note_item->y2 += dy;

	update_item_bounding_box (note_item);

	if (item->canvas->aa) {
		gnome_canvas_item_request_update (item);
	}
}

static void
nautilus_canvas_note_item_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	NautilusCanvasNoteItem *note_item;
	double hwidth;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	if (note_item->width_pixels)
		hwidth = (note_item->width / item->canvas->pixels_per_unit) / 2.0;
	else
		hwidth = note_item->width / 2.0;

	*x1 = note_item->x1 - hwidth;
	*y1 = note_item->y1 - hwidth;
	*x2 = note_item->x2 + hwidth;
	*y2 = note_item->y2 + hwidth;
}

/* utility routine to map raw annotation text into text to be displayed */
/* for now this is pretty naive and only handles free-form text, just returning
 * the first suitable annotation it can find
 */

/* utility routine to draw a text string into the passed-in item */
static void
draw_item_aa_text (GnomeCanvasBuf *buf, GnomeCanvasItem *item, const char *note_text)
{
	EelScalableFont *font;
	GdkPixbuf *text_pixbuf;	
	ArtIRect item_bounds, dest_bounds;
	int width, height;
	EelSmoothTextLayout *smooth_text_layout;
	
	font = eel_scalable_font_get_default_font ();

	eel_gnome_canvas_item_get_canvas_bounds (item, &item_bounds);
	width = item_bounds.x1 - item_bounds.x0;
	height = item_bounds.y1 - item_bounds.y0;
	
 	text_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
 				      TRUE,
 				      8,
 				      width,
 				      height);
	eel_gdk_pixbuf_fill_rectangle_with_color (text_pixbuf, NULL,
						       EEL_RGBA_COLOR_PACK (0, 0, 0, 0));
	
	smooth_text_layout = eel_smooth_text_layout_new (
				note_text, strlen(note_text),
				font, DEFAULT_FONT_SIZE, TRUE);
	
	eel_smooth_text_layout_set_line_wrap_width (smooth_text_layout, width - 4);
	
	dest_bounds.x0 = 0;
	dest_bounds.y0 = ARROW_HEIGHT;
	dest_bounds.x1 = width;
	dest_bounds.y1 = height;
	 
	eel_smooth_text_layout_draw_to_pixbuf
		(smooth_text_layout, text_pixbuf,
		 0, 0, &dest_bounds, GTK_JUSTIFY_LEFT,
		 FALSE, EEL_RGBA_COLOR_OPAQUE_BLACK,
		 EEL_OPACITY_FULLY_OPAQUE);
	
	gtk_object_destroy (GTK_OBJECT (smooth_text_layout));
		
	eel_gnome_canvas_draw_pixbuf (buf, text_pixbuf, item_bounds.x0 + 4, item_bounds.y0 + 2);
	
	gdk_pixbuf_unref (text_pixbuf);	
	gtk_object_unref (GTK_OBJECT (font));
}

static void
nautilus_canvas_note_item_render (GnomeCanvasItem *item,
			GnomeCanvasBuf *buf)
{
	NautilusCanvasNoteItem *note_item;
	char *display_text;
	
	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	if (note_item->fill_svp != NULL) {
		gnome_canvas_render_svp (buf, note_item->fill_svp, note_item->fill_color);
	}

	/* draw the annotation text, if necessary */
	if (note_item->note_text) {
		display_text = nautilus_annotation_get_display_text (note_item->note_text);
		if (display_text && strlen (display_text)) {
			draw_item_aa_text (buf, item, display_text);
		}
		g_free (display_text);
	}

	if (note_item->outline_svp != NULL) {
		gnome_canvas_render_svp (buf, note_item->outline_svp, note_item->outline_color);
	}
}

static void
nautilus_canvas_note_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	NautilusCanvasNoteItem *note_item;
	GdkFont *font;
	char* display_text;
	double i2w[6], w2c[6], i2c[6];
	int x1, y1, x2, y2;
	ArtPoint i1, i2;
	ArtPoint c1, c2;
	GnomeIconTextInfo *text_info;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	/* Get canvas pixel coordinates */
	gnome_canvas_item_i2w_affine (item, i2w);
	gnome_canvas_w2c_affine (item->canvas, w2c);
	art_affine_multiply (i2c, i2w, w2c);

	i1.x = note_item->x1;
	i1.y = note_item->y1;
	i2.x = note_item->x2;
	i2.y = note_item->y2;
	art_affine_point (&c1, &i1, i2c);
	art_affine_point (&c2, &i2, i2c);
	x1 = c1.x;
	y1 = c1.y;
	x2 = c2.x;
	y2 = c2.y;

	if (note_item->fill_stipple)
		gnome_canvas_set_stipple_origin (item->canvas, note_item->fill_gc);

	gdk_draw_rectangle (drawable,
				note_item->fill_gc,
				TRUE,
				x1 - x,
				y1 - y,
				x2 - x1 + 1,
				y2 - y1 + 1);

	/* draw the annotation text */
	if (note_item->note_text) {
		font = nautilus_font_factory_get_font_from_preferences (DEFAULT_FONT_SIZE);		
		display_text = nautilus_annotation_get_display_text (note_item->note_text);

		text_info = gnome_icon_layout_text
			(font, display_text,
			 LINE_BREAK_CHARACTERS,
			 x2 - x1 - 2, TRUE);

		gnome_icon_paint_text (text_info, drawable, note_item->outline_gc,
			 	       x1  - x + 4, y1 - y + 4, GTK_JUSTIFY_LEFT);
		gnome_icon_text_info_free (text_info);
		
		gdk_font_unref (font);
		g_free (display_text);
	}
		
	if (note_item->outline_stipple)
		gnome_canvas_set_stipple_origin (item->canvas, note_item->outline_gc);

	gdk_draw_rectangle (drawable,
				note_item->outline_gc,
				FALSE,
				x1 - x,
				y1 - y,
				x2 - x1,
				y2 - y1);
}

static double
nautilus_canvas_note_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	NautilusCanvasNoteItem *note_item;
	double x1, y1, x2, y2;
	double hwidth;
	double dx, dy;

	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	x1 = note_item->x1;
	y1 = note_item->y1;
	x2 = note_item->x2;
	y2 = note_item->y2;

	if (note_item->width_pixels)
		hwidth = (note_item->width / item->canvas->pixels_per_unit) / 2.0;
	else
		hwidth = note_item->width / 2.0;

	x1 -= hwidth;
	y1 -= hwidth;
	x2 += hwidth;
	y2 += hwidth;

	/* Is point inside rectangle (which can be hollow if it has no fill set)? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}

	/* Point is outside rectangle */

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}

static void
nautilus_canvas_note_item_update (GnomeCanvasItem *item, double affine[6], ArtSVP *clip_path, gint flags)
{
	NautilusCanvasNoteItem *note_item;
	ArtVpath vpath[9];
	ArtVpath *vpath2;
	ArtSVP *stroke_svp;
	double x0, y0, x1, y1;
	double round_off_amount;
	double midpoint;
	double arrow_half_width;
	note_item = NAUTILUS_CANVAS_NOTE_ITEM (item);

	if (note_item_parent_class->update)
		(* note_item_parent_class->update) (item, affine, clip_path, flags);

	if (item->canvas->aa) {
		x0 = note_item->x1;
		y0 = note_item->y1 + ARROW_HEIGHT;
		x1 = note_item->x2;
		y1 = note_item->y2;

		round_off_amount = item->canvas->pixels_per_unit;
		
		gnome_canvas_item_reset_bounds (item);
		midpoint = (x1 + x0) / 2;
		arrow_half_width = (x1 - x0) / 16;
		arrow_half_width = CLAMP (arrow_half_width, MIN_ARROW_HALF_WIDTH, MAX_ARROW_HALF_WIDTH);
				
		vpath[0].code = ART_MOVETO;
		vpath[0].x = x0 - round_off_amount;
		vpath[0].y = y0 - round_off_amount;
		
		vpath[1].code = ART_LINETO;
		vpath[1].x = x0 - round_off_amount;
		vpath[1].y = y1 + round_off_amount;
		
		vpath[2].code = ART_LINETO;
		vpath[2].x = x1 + round_off_amount;
		vpath[2].y = y1 + round_off_amount;
		
		vpath[3].code = ART_LINETO;
		vpath[3].x = x1 + round_off_amount;
		vpath[3].y = y0 - round_off_amount;

		vpath[4].code = ART_LINETO;
		vpath[4].x = midpoint + arrow_half_width + round_off_amount;
		vpath[4].y = y0 - round_off_amount;
		
		vpath[5].code = ART_LINETO;
		vpath[5].x = midpoint + round_off_amount;
		vpath[5].y = note_item->y1 - round_off_amount;
				
		vpath[6].code = ART_LINETO;
		vpath[6].x = midpoint - arrow_half_width - round_off_amount;
		vpath[6].y = y0 - round_off_amount;
				
		vpath[7].code = ART_LINETO;
		vpath[7].x = x0 - round_off_amount;
		vpath[7].y = y0 - round_off_amount;
				
		vpath[8].code = ART_END;
		vpath[8].x = 0;
		vpath[8].y = 0;

		vpath2 = art_vpath_affine_transform (vpath, affine);

		gnome_canvas_item_update_svp_clip (item, &note_item->fill_svp, art_svp_from_vpath (vpath2), clip_path);
			
		stroke_svp = art_svp_vpath_stroke (vpath2,
							ART_PATH_STROKE_JOIN_MITER,
							ART_PATH_STROKE_CAP_BUTT,
							(note_item->width_pixels) ? note_item->width : (note_item->width * item->canvas->pixels_per_unit),
							4,
							25);

		gnome_canvas_item_update_svp_clip (item, &note_item->outline_svp, stroke_svp, clip_path);
		art_free (vpath2);

		eel_gnome_canvas_item_request_redraw
			(GNOME_CANVAS_ITEM (item));
	
	} else {
		/* xlib rendering - just update the bbox */

		set_gc_foreground (note_item->fill_gc, note_item->fill_pixel);
		set_gc_foreground (note_item->outline_gc, note_item->outline_pixel);
		set_stipple (note_item->fill_gc, &note_item->fill_stipple, note_item->fill_stipple, TRUE);
		set_stipple (note_item->outline_gc, &note_item->outline_stipple, note_item->outline_stipple, TRUE);
		set_outline_gc_width (note_item);
		
		get_bounds (note_item, &x0, &y0, &x1, &y1);
		gnome_canvas_update_bbox (item, x0, y0, x1, y1);
	}
}

