/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* GNOME libraries - Icon Item class for Icon Container
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * Authors: Andy Hertzfeld <andy@eazel.com>, Darin Adler <darin@eazel.com>
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
#include "nautilus-icons-view-icon-item.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-icon-text.h>
#include "gnome-icon-container-private.h"
#include "nautilus-string.h"
#include "nautilus-glib-extensions.h"
#include "gdk-extensions.h"
#include "nautilus-gtk-macros.h"

#define STRETCH_HANDLE_THICKNESS 5
#define EMBLEM_SPACING 4

/* Private part of the NautilusIconsViewIconItem structure */
struct NautilusIconsViewIconItemDetails {
	/* The image, text, font. */
	GdkPixbuf *pixbuf;
	GList *emblem_pixbufs;
	char *text;
	char *text_source;
	GdkFont *font;
	
	/* Size of the text at current font. */
	int text_width;
	int text_height;
	
    	/* Highlight state. */
   	guint is_highlighted_for_selection : 1;
	guint is_highlighted_for_keyboard_selection: 1;
   	guint is_highlighted_for_drop : 1;
	guint show_stretch_handles : 1;
};

/* Object argument IDs. */
enum {
	ARG_0,
	ARG_PIXBUF,
	ARG_TEXT,
	ARG_FONT,
    	ARG_HIGHLIGHTED_FOR_SELECTION,
    	ARG_HIGHLIGHTED_FOR_KEYBOARD_SELECTION,
    	ARG_HIGHLIGHTED_FOR_DROP,
    	ARG_TEXT_SOURCE
};

typedef enum {
	RIGHT_SIDE,
	BOTTOM_SIDE,
	LEFT_SIDE,
	TOP_SIDE
} RectangleSide;

typedef struct {
	NautilusIconsViewIconItem *icon_item;
	ArtIRect icon_rect;
	RectangleSide side;
	int position;
	GList *emblem;
} EmblemLayout;

/* constants */

#define MAX_TEXT_WIDTH 80

/* Bitmap for stippled selection rectangles. */
static GdkBitmap *stipple;
static char stipple_bits[] = { 0x02, 0x01 };
static GdkFont *mini_text_font;

/* GtkObject */
static void     nautilus_icons_view_icon_item_initialize_class          (NautilusIconsViewIconItemClass  *class);
static void     nautilus_icons_view_icon_item_initialize                (NautilusIconsViewIconItem       *item);
static void     nautilus_icons_view_icon_item_destroy                   (GtkObject                       *object);
static void     nautilus_icons_view_icon_item_set_arg                   (GtkObject                       *object,
									 GtkArg                          *arg,
									 guint                            arg_id);
static void     nautilus_icons_view_icon_item_get_arg                   (GtkObject                       *object,
									 GtkArg                          *arg,
									 guint                            arg_id);

/* GnomeCanvasItem */
static void     nautilus_icons_view_icon_item_update                    (GnomeCanvasItem                 *item,
									 double                          *affine,
									 ArtSVP                          *clip_path,
									 int                              flags);
static void     nautilus_icons_view_icon_item_draw                      (GnomeCanvasItem                 *item,
									 GdkDrawable                     *drawable,
									 int                              x,
									 int                              y,
									 int                              width,
									 int                              height);
static double   nautilus_icons_view_icon_item_point                     (GnomeCanvasItem                 *item,
									 double                           x,
									 double                           y,
									 int                              cx,
									 int                              cy,
									 GnomeCanvasItem                **actual_item);
static void     nautilus_icons_view_icon_item_bounds                    (GnomeCanvasItem                 *item,
									 double                          *x1,
									 double                          *y1,
									 double                          *x2,
									 double                          *y2);

/* private */
static void     draw_or_measure_text_box                                (GnomeCanvasItem                 *item,
									 GdkDrawable                     *drawable,
									 int                              icon_left,
									 int                              icon_bottom);
static void     nautilus_icons_view_draw_text_box                       (GnomeCanvasItem                 *item,
									 GdkDrawable                     *drawable,
									 int                              icon_left,
									 int                              icon_bottom);
static void     nautilus_icons_view_measure_text_box                    (GnomeCanvasItem                 *item);
static void     nautilus_icons_view_icon_item_get_icon_canvas_rectangle (NautilusIconsViewIconItem       *item,
									 ArtIRect                        *rect);
static void     emblem_layout_reset                                     (EmblemLayout                    *layout,
									 NautilusIconsViewIconItem       *icon_item,
									 const ArtIRect                  *icon_rect);
static gboolean emblem_layout_next                                      (EmblemLayout                    *layout,
									 GdkPixbuf                      **emblem_pixbuf,
									 ArtIRect                        *emblem_rect);
static void     draw_pixbuf                                             (GdkPixbuf                       *pixbuf,
									 GdkDrawable                     *drawable,
									 int                              x,
									 int                              y);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconsViewIconItem, nautilus_icons_view_icon_item, GNOME_TYPE_CANVAS_ITEM)

/* Class initialization function for the icon canvas item. */
static void
nautilus_icons_view_icon_item_initialize_class (NautilusIconsViewIconItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = GTK_OBJECT_CLASS (class);
	item_class = GNOME_CANVAS_ITEM_CLASS (class);

	gtk_object_add_arg_type ("NautilusIconsViewIconItem::pixbuf",
				 GTK_TYPE_BOXED, GTK_ARG_READWRITE, ARG_PIXBUF);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::text",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TEXT);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::font",
				 GTK_TYPE_BOXED, GTK_ARG_READWRITE, ARG_FONT);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::highlighted_for_selection",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_FOR_SELECTION);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::highlighted_for_keyboard_selection",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_FOR_KEYBOARD_SELECTION);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::highlighted_for_drop",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_FOR_DROP);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::text_source",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TEXT_SOURCE);

	object_class->destroy = nautilus_icons_view_icon_item_destroy;
	object_class->set_arg = nautilus_icons_view_icon_item_set_arg;
	object_class->get_arg = nautilus_icons_view_icon_item_get_arg;

	item_class->update = nautilus_icons_view_icon_item_update;
	item_class->draw = nautilus_icons_view_icon_item_draw;
	item_class->point = nautilus_icons_view_icon_item_point;
	item_class->bounds = nautilus_icons_view_icon_item_bounds;

	stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);
        /* FIXME: the font shouldn't be hard-wired like this */
        mini_text_font = gdk_font_load("-bitstream-charter-medium-r-normal-*-9-*-*-*-*-*-*-*");
}

/* Object initialization function for the icon item. */
static void
nautilus_icons_view_icon_item_initialize (NautilusIconsViewIconItem *icon_item)
{
	NautilusIconsViewIconItemDetails *details;

	details = g_new0 (NautilusIconsViewIconItemDetails, 1);

	icon_item->details = details;
}

/* Destroy handler for the icon canvas item. */
static void
nautilus_icons_view_icon_item_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	NautilusIconsViewIconItem *icon_item;
	NautilusIconsViewIconItemDetails *details;

	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);
	icon_item = (NAUTILUS_ICONS_VIEW_ICON_ITEM (object));
	details = icon_item->details;

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	if (details->pixbuf != NULL) {
		gdk_pixbuf_unref (details->pixbuf);
	}
	nautilus_gdk_pixbuf_list_free (details->emblem_pixbufs);
	g_free (details->text);
	if (details->font != NULL) {
		gdk_font_unref (details->font);
	}
	
	g_free (details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}
 
/* Currently we require pixbufs in this format (for hit testing).
 * Perhaps gdk-pixbuf will be changed so it can do the hit testing
 * and we won't have this requirement any more.
 */
static gboolean
pixbuf_is_acceptable (GdkPixbuf *pixbuf)
{
	return gdk_pixbuf_get_format (pixbuf) == ART_PIX_RGB
		&& (gdk_pixbuf_get_n_channels (pixbuf) == 3
		    || gdk_pixbuf_get_n_channels (pixbuf) == 4)
		&& gdk_pixbuf_get_bits_per_sample (pixbuf) == 8;
}

/* Set_arg handler for the icon item. */
static void
nautilus_icons_view_icon_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusIconsViewIconItemDetails *details;
	GdkPixbuf *pixbuf;
	GdkFont *font;

	details = NAUTILUS_ICONS_VIEW_ICON_ITEM (object)->details;

	switch (arg_id) {

	case ARG_PIXBUF:
		pixbuf = GTK_VALUE_BOXED (*arg);
		if (pixbuf == details->pixbuf) {
			return;
		}
		
		if (pixbuf != NULL) {
			g_return_if_fail (pixbuf_is_acceptable (pixbuf));
			gdk_pixbuf_ref (pixbuf);
		}
		
		if (details->pixbuf != NULL) {
			gdk_pixbuf_unref (details->pixbuf);
		}
		
		details->pixbuf = pixbuf;
		break;

	case ARG_TEXT:
		if (nautilus_strcmp (details->text, GTK_VALUE_STRING (*arg)) == 0) {
			return;
		}

		g_free (details->text);
		details->text = g_strdup (GTK_VALUE_STRING (*arg));
		break;

	case ARG_FONT:
		font = GTK_VALUE_BOXED (*arg);
		if (nautilus_gdk_font_equal (font, details->font)) {
			return;
		}

		if (font != NULL) {
			gdk_font_ref (font);
		}
		if (details->font != NULL) {
			gdk_font_unref (details->font);
		}
		details->font = font;
		break;

	case ARG_HIGHLIGHTED_FOR_SELECTION:
		if (!details->is_highlighted_for_selection == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_for_selection = GTK_VALUE_BOOL (*arg);
		break;
         
        case ARG_HIGHLIGHTED_FOR_KEYBOARD_SELECTION:
		if (!details->is_highlighted_for_keyboard_selection == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_for_keyboard_selection = GTK_VALUE_BOOL (*arg);
		break;
		
        case ARG_HIGHLIGHTED_FOR_DROP:
		if (!details->is_highlighted_for_drop == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_for_drop = GTK_VALUE_BOOL (*arg);
		break;
        
        case ARG_TEXT_SOURCE:
		if (nautilus_strcmp (details->text_source, GTK_VALUE_STRING (*arg)) == 0) {
			return;
		}
		
		g_free (details->text_source);
		details->text_source = g_strdup (GTK_VALUE_STRING (*arg));
		break;
		
	default:
		g_warning ("nautilus_icons_view_item_item_set_arg on unknown argument");
		return;
	}
	
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (object));
}

/* Get_arg handler for the icon item */
static void
nautilus_icons_view_icon_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusIconsViewIconItemDetails *details;
	
	details = NAUTILUS_ICONS_VIEW_ICON_ITEM (object)->details;
	
	switch (arg_id) {
		
	case ARG_PIXBUF:
		GTK_VALUE_BOXED (*arg) = details->pixbuf;
		break;
		
	case ARG_TEXT:
		GTK_VALUE_STRING (*arg) = g_strdup (details->text);
		break;
		
	case ARG_FONT:
		GTK_VALUE_BOXED (*arg) = details->font;
		break;
		
        case ARG_HIGHLIGHTED_FOR_SELECTION:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_for_selection;
                break;
		
        case ARG_HIGHLIGHTED_FOR_KEYBOARD_SELECTION:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_for_keyboard_selection;
                break;
		
        case ARG_HIGHLIGHTED_FOR_DROP:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_for_drop;
                break;
        
        case ARG_TEXT_SOURCE:
		GTK_VALUE_STRING (*arg) = g_strdup (details->text_source);
                break;
		
        default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

void
nautilus_icons_view_icon_item_set_emblems (NautilusIconsViewIconItem *item,
					   GList *emblem_pixbufs)
{
	GList *p;

	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item));

	g_assert (item->details->emblem_pixbufs != emblem_pixbufs || emblem_pixbufs == NULL);

	/* The case where the emblems are identical is fairly common,
	 * so lets take the time to check for it.
	 */
	if (nautilus_g_list_equal (item->details->emblem_pixbufs, emblem_pixbufs)) {
		return;
	}

	/* Check if they are acceptable. */
	for (p = emblem_pixbufs; p != NULL; p = p->next) {
		g_return_if_fail (pixbuf_is_acceptable (p->data));
	}
	
	/* Take in the new list of emblems. */
	nautilus_gdk_pixbuf_list_ref (emblem_pixbufs);
	nautilus_gdk_pixbuf_list_free (item->details->emblem_pixbufs);
	item->details->emblem_pixbufs = g_list_copy (emblem_pixbufs);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
}

/* Recomputes the bounding box of a icon canvas item. */
static void
recompute_bounding_box (NautilusIconsViewIconItem *icon_item)
{
	/* The bounds stored in the item is the same as what get_bounds
	 * returns, except it's in canvas coordinates instead of world
	 * coordinates.
	 */

	GnomeCanvasItem *item;
	double x1, y1, x2, y2;

	item = GNOME_CANVAS_ITEM (icon_item);
	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);
	gnome_canvas_w2c_d (item->canvas, x1, y1, &item->x1, &item->y1);
	gnome_canvas_w2c_d (item->canvas, x2, y2, &item->x2, &item->y2);
}

/* Update handler for the icon canvas item. */
static void
nautilus_icons_view_icon_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	NautilusIconsViewIconItem *icon_item;
	NautilusIconsViewIconItemDetails *details;

	icon_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	details = icon_item->details;

	/* Make sure the text box measurements are set up
	 * before recalculating the bounding box.
	 */
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
	nautilus_icons_view_measure_text_box (item);
	recompute_bounding_box (icon_item);
	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
	
	NAUTILUS_CALL_PARENT_CLASS (GNOME_CANVAS_ITEM_CLASS, update, (item, affine, clip_path, flags));
}



/* Rendering */

/* Draw the text in a box, using gnomelib routines. */
static void
draw_or_measure_text_box (GnomeCanvasItem* item, 
			  GdkDrawable *drawable,
                          int icon_left,
                          int icon_bottom)
{
	NautilusIconsViewIconItemDetails *details;
        int width_so_far, height_so_far;
        GdkGC* gc;
	int max_text_width;
	int icon_width, text_left, box_left;
	GnomeIconTextInfo *icon_text_info;
	char **pieces;
	const char *text_piece;
	int i;

	details = NAUTILUS_ICONS_VIEW_ICON_ITEM (item)->details;

	if (details->font == NULL || details->text == NULL || details->text[0] == '\0') {
		details->text_height = 0;
		details->text_width = 0;
		return;
	}
	
	width_so_far = 0;
	height_so_far = 0;

	if (drawable != NULL) {
		icon_width = details->pixbuf == NULL ? 0 : gdk_pixbuf_get_width (details->pixbuf);
		gc = gdk_gc_new (item->canvas->layout.bin_window);
	}
	
	max_text_width = floor (MAX_TEXT_WIDTH * item->canvas->pixels_per_unit);
	
	pieces = g_strsplit (details->text, "\n", 0);
	for (i = 0; (text_piece = pieces[i]) != NULL; i++) {
		/* Replace empty string with space for measurement and drawing.
		 * This makes empty lines appear, instead of being collapsed out.
		 */
		if (text_piece[0] == '\0') {
			text_piece = " ";
		}
		
		icon_text_info = gnome_icon_layout_text
			(details->font, text_piece, " -_,;.?/&", max_text_width, TRUE);
		
		if (drawable != NULL) {
			text_left = icon_left + (icon_width - icon_text_info->width) / 2;
			gnome_icon_paint_text (icon_text_info, drawable, gc,
					       text_left, icon_bottom + height_so_far, GTK_JUSTIFY_CENTER);
		}
		
		width_so_far = MAX (width_so_far, icon_text_info->width);
		height_so_far += icon_text_info->height;
		
		gnome_icon_text_info_free (icon_text_info);
	}
	g_strfreev (pieces);
	
	height_so_far += 2; /* extra slop for nicer highlighting */
	
	if (drawable != NULL) {

		/* Current calculations should match what we measured before drawing.
		 * This assumes that we will always make a separate call to measure
		 * before the call to draw. We might later decide to use this function
		 * differently and change these asserts.
		 */
		g_assert (height_so_far == details->text_height);
		g_assert (width_so_far == details->text_width);
	
		box_left = icon_left + (icon_width - width_so_far) / 2;

		/* invert to indicate selection if necessary */
		if (details->is_highlighted_for_selection) {
			gdk_gc_set_function (gc, GDK_INVERT);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    box_left, icon_bottom - 2,
					    width_so_far, 2 + height_so_far);
			gdk_gc_set_function (gc, GDK_COPY);
		}
		
		/* indicate keyboard selection by framing the text with a gray-stippled rectangle */
		if (details->is_highlighted_for_keyboard_selection) {
			gdk_gc_set_stipple (gc, stipple);
			gdk_gc_set_fill (gc, GDK_STIPPLED);
			gdk_draw_rectangle (drawable, gc, FALSE,
					    box_left, icon_bottom - 2,
					    width_so_far, 2 + height_so_far);
		}
		
		gdk_gc_unref (gc);
	}
	else
	{
		/* If measuring, remember the width & height. */
		details->text_width = width_so_far;
		details->text_height = height_so_far;
	}
}

static void
nautilus_icons_view_measure_text_box (GnomeCanvasItem* item)
{
	draw_or_measure_text_box (item, NULL, 0, 0);
}

static void
nautilus_icons_view_draw_text_box (GnomeCanvasItem* item, GdkDrawable *drawable,
                                   int icon_left, int icon_bottom)
{
	draw_or_measure_text_box (item, drawable, icon_left, icon_bottom);
}

/* utility routine to draw the mini-text inside text files */
/* FIXME: We should cache the text in the object instead
 * of reading each time we draw, so we can work well over the network.
 */

static void
draw_mini_text (GnomeCanvasItem* item, GdkDrawable *drawable,
		int x_pos, int y_pos, int icon_width, int icon_height, char *text_source)
{
	FILE *text_file;
	char *file_name;
	GdkRectangle clip_rect;
	NautilusIconsViewIconItem *icon_item;
	NautilusIconsViewIconItemDetails *details;
	char text_buffer[256];
	int cur_x, cur_y, y_limit;
	GdkGC *gc;
	
	icon_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	details = icon_item->details;

	/* Draw the first few lines of the text file until we fill up the icon */
	/* FIXME: need to use gnome_vfs to read the file  */
	
	file_name = details->text_source;
	if (nautilus_has_prefix (details->text_source, "file://")) {
		file_name += 7;
	}
        text_file = fopen(file_name, "r");
	if (text_file == NULL) {
		return;
	}

	gc = gdk_gc_new (drawable);
	
	/* clip to the icon bounds */         
	clip_rect.x = x_pos;
	clip_rect.y = y_pos;
	clip_rect.width = icon_width - 4;
	clip_rect.height = icon_height;
	gdk_gc_set_clip_rectangle (gc, &clip_rect);
	
	cur_x = x_pos + 6;
	cur_y = y_pos + 13;
	y_limit = y_pos + icon_height - 8;
	while (fgets (text_buffer, 256, text_file)) {
		gdk_draw_string (drawable, mini_text_font, gc, cur_x, cur_y, text_buffer);
		cur_y += 9;
		if (cur_y > y_limit) {
				break;
		}
	}
	
	fclose (text_file);
	
	gdk_gc_unref(gc);
}

static void
draw_stretch_handles (NautilusIconsViewIconItem *item, GdkDrawable *drawable,
		      const ArtIRect *rect)
{
	GdkGC *gc;

	if (!item->details->show_stretch_handles) {
		return;
	}

	gc = gdk_gc_new (drawable);
	
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rect->x0,
			    rect->y0,
			    STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rect->x1 - STRETCH_HANDLE_THICKNESS,
			    rect->y0,
			    STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rect->x0,
			    rect->y1 - STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    rect->x1 - STRETCH_HANDLE_THICKNESS,
			    rect->y1 - STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS,
			    STRETCH_HANDLE_THICKNESS);
	
	gdk_gc_set_stipple (gc, stipple);
	gdk_gc_set_fill (gc, GDK_STIPPLED);
	gdk_draw_rectangle (drawable, gc, FALSE,
			    rect->x0 + (STRETCH_HANDLE_THICKNESS - 1) / 2,
			    rect->y0 + (STRETCH_HANDLE_THICKNESS - 1) / 2,
			    rect->x1 - rect->x0 - (STRETCH_HANDLE_THICKNESS - 1) - 1,
			    rect->y1 - rect->y0 - (STRETCH_HANDLE_THICKNESS - 1) - 1);
	
	gdk_gc_unref (gc);
}

static void
emblem_layout_reset (EmblemLayout *layout, NautilusIconsViewIconItem *icon_item, const ArtIRect *icon_rect)
{
	layout->icon_item = icon_item;
	layout->icon_rect = *icon_rect;
	layout->side = RIGHT_SIDE;
	layout->position = 0;
	layout->emblem = icon_item->details->emblem_pixbufs;
}

static gboolean
emblem_layout_next (EmblemLayout *layout,
		    GdkPixbuf **emblem_pixbuf,
		    ArtIRect *emblem_rect)
{
	GdkPixbuf *pixbuf;
	int width, height, x, y;

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
	/* FIXME: Dither would be better if we passed dither values. */
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable, 0, 0, x, y,
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf),
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);

}

/* Draw the icon item. */
static void
nautilus_icons_view_icon_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
                                    int x, int y, int width, int height)
{
	NautilusIconsViewIconItem *icon_item;
	NautilusIconsViewIconItemDetails *details;
	ArtIRect icon_rect, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;
	
	icon_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	details = icon_item->details;

        /* Draw the pixbuf. */
     	if (details->pixbuf == NULL) {
		return;
	}

	/* Compute icon rectangle in drawable coordinates. */
	nautilus_icons_view_icon_item_get_icon_canvas_rectangle
		(icon_item, &icon_rect);
	icon_rect.x0 -= x;
	icon_rect.y0 -= y;
	icon_rect.x1 -= x;
	icon_rect.y1 -= y;
	
	/* Draw the icon's pixbuf. */
	draw_pixbuf (details->pixbuf, drawable, icon_rect.x0, icon_rect.y0);

	/* Draw the emblem pixbufs. */
	emblem_layout_reset (&emblem_layout, icon_item, &icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		draw_pixbuf (emblem_pixbuf, drawable, emblem_rect.x0, emblem_rect.y0);
	}
	
	/* Draw stretching handles (if necessary). */
	draw_stretch_handles (icon_item, drawable, &icon_rect);
	
	/* FIXME: The text reading does not belong here at all, but rather in the caller. */
	if (details->text_source) {
		draw_mini_text (item, drawable, icon_rect.x0, icon_rect.y0,
				icon_rect.x1 - icon_rect.x0, icon_rect.y1 - icon_rect.y0,
				details->text_source);
	}
	
	/* Draw the label text. */
	nautilus_icons_view_draw_text_box (item, drawable, icon_rect.x0, icon_rect.y1);
}



static gboolean
hit_test_pixbuf (GdkPixbuf *pixbuf, int pixbuf_x, int pixbuf_y, int probe_x, int probe_y)
{
	int relative_x, relative_y;
	guint8 *pixel;
	
	/* You can get here without a pixbuf in some strage cases. */
	if (pixbuf == NULL) {
		return FALSE;
	}
	
	/* Check to see if it's within the rectangle at all. */
	relative_x = probe_x - pixbuf_x;
	relative_y = probe_y - pixbuf_y;
	if (relative_x < 0 || relative_y < 0
	    || relative_x > gdk_pixbuf_get_width (pixbuf)
	    || relative_y > gdk_pixbuf_get_height (pixbuf)) {
		return FALSE;
	}

	/* If there's no alpha channel, it's opaque and we have a hit. */
	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		return TRUE;
	}
	g_assert (gdk_pixbuf_get_n_channels (pixbuf) == 4);
	
	/* Check the alpha channel of the pixel to see if we have a hit. */
	pixel = gdk_pixbuf_get_pixels (pixbuf)
		+ relative_y * gdk_pixbuf_get_rowstride (pixbuf)
		+ relative_x * 4;
	return pixel[3] >= 128;
}

static gboolean
hit_test (NautilusIconsViewIconItem *icon_item, int cx, int cy)
{
	NautilusIconsViewIconItemDetails *details;
	ArtIRect icon_rect, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;
	
	details = icon_item->details;

	/* Check for hits in the stretch handles. */
	if (nautilus_icons_view_icon_item_get_hit_stretch_handle (icon_item, cx, cy)) {
		return TRUE;
	}
	
	/* Check for hit in the icon. */
	nautilus_icons_view_icon_item_get_icon_canvas_rectangle (icon_item, &icon_rect);
	if (hit_test_pixbuf (details->pixbuf, icon_rect.x0, icon_rect.y0, cx, cy)) {
		return TRUE;
	}

	/* Check for hit in the emblem pixbufs. */
	emblem_layout_reset (&emblem_layout, icon_item, &icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		if (hit_test_pixbuf (emblem_pixbuf, emblem_rect.x0, emblem_rect.y0, cx, cy)) {
			return TRUE;
		}
	}

	return FALSE;
}

/* Point handler for the icon canvas item. */
/* FIXME: This currently only reports a hit if the pixbuf or stretch handles are hit,
 * and ignores hits on the text.
 */
static double
nautilus_icons_view_icon_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
				     GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	if (hit_test (NAUTILUS_ICONS_VIEW_ICON_ITEM (item), cx, cy)) {
		return 0.0;
	} else {
		return item->canvas->pixels_per_unit * 2 + 10;
	}
}

/* Bounds handler for the icon canvas item. */
static void
nautilus_icons_view_icon_item_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	NautilusIconsViewIconItem *icon_item;
	NautilusIconsViewIconItemDetails *details;
	ArtIRect icon_rect, text_rect, total_rect, emblem_rect;
	double pixels_per_unit;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;

	g_assert (x1 != NULL);
	g_assert (y1 != NULL);
	g_assert (x2 != NULL);
	g_assert (y2 != NULL);
	
	icon_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	details = icon_item->details;

	/* Compute icon rectangle. */
	icon_rect.x0 = 0;
	icon_rect.y0 = 0;
	if (details->pixbuf == NULL) {
		icon_rect.x1 = 0;
		icon_rect.y1 = 0;
	} else {
		icon_rect.x1 = gdk_pixbuf_get_width (details->pixbuf);
		icon_rect.y1 = gdk_pixbuf_get_height (details->pixbuf);
	}
	
	/* Compute text rectangle. */
	text_rect.x0 = icon_rect.x1 / 2 - details->text_width / 2;
	text_rect.y0 = icon_rect.y1;
	text_rect.x1 = text_rect.x0 + details->text_width;
	text_rect.y1 = text_rect.y0 + details->text_height;

	/* Compute total rectangle, adding in emblem rectangles. */
	art_irect_union (&total_rect, &icon_rect, &text_rect);
	emblem_layout_reset (&emblem_layout, icon_item, &icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		art_irect_union (&total_rect, &total_rect, &emblem_rect);
	}
	
        
        /* Add 2 pixels slop to each side. */
	total_rect.x0 -= 2;
	total_rect.x1 += 2;
	total_rect.y0 -= 2;
	total_rect.y1 += 2;
	
	/* Return the result. */
	pixels_per_unit = item->canvas->pixels_per_unit;
	*x1 = total_rect.x0 / pixels_per_unit;
	*y1 = total_rect.y0 / pixels_per_unit;
	*x2 = total_rect.x1 / pixels_per_unit;
	*y2 = total_rect.y1 / pixels_per_unit;
}

/* Get the rectangle of the icon only, in world coordinates. */
void
nautilus_icons_view_icon_item_get_icon_world_rectangle (NautilusIconsViewIconItem *item,
							ArtDRect *rect)
{
	double i2w[6];
	ArtPoint art_point;
	double pixels_per_unit;
	GdkPixbuf *pixbuf;
	
	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item));
	g_return_if_fail (rect != NULL);

	gnome_canvas_item_i2w_affine (GNOME_CANVAS_ITEM (item), i2w);

	art_point.x = 0;
	art_point.y = 0;
	art_affine_point (&art_point, &art_point, i2w);
	
	rect->x0 = art_point.x;
	rect->y0 = art_point.y;

	pixbuf = item->details->pixbuf;
	pixels_per_unit = GNOME_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	
	rect->x1 = rect->x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
	rect->y1 = rect->y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;
}

/* Get the rectangle of the icon only, in canvas coordinates. */
void
nautilus_icons_view_icon_item_get_icon_canvas_rectangle (NautilusIconsViewIconItem *item,
							 ArtIRect *rect)
{
	double i2c[6];
	ArtPoint art_point;
	GdkPixbuf *pixbuf;

	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item));
	g_return_if_fail (rect != NULL);

	gnome_canvas_item_i2c_affine (GNOME_CANVAS_ITEM (item), i2c);

	art_point.x = 0;
	art_point.y = 0;
	art_affine_point (&art_point, &art_point, i2c);
	
	rect->x0 = floor (art_point.x);
	rect->y0 = floor (art_point.y);

	pixbuf = item->details->pixbuf;
	
	rect->x1 = rect->x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf));
	rect->y1 = rect->y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf));
}

/* Get the rectangle of the icon only, in window coordinates. */
void
nautilus_icons_view_icon_item_get_icon_window_rectangle (NautilusIconsViewIconItem *item,
							 ArtIRect *rect)
{
	double i2w[6];
	ArtPoint art_point;
	GdkPixbuf *pixbuf;
	
	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item));
	g_return_if_fail (rect != NULL);

	gnome_canvas_item_i2w_affine (GNOME_CANVAS_ITEM (item), i2w);

	art_point.x = 0;
	art_point.y = 0;
	art_affine_point (&art_point, &art_point, i2w);
	gnome_canvas_world_to_window (GNOME_CANVAS_ITEM (item)->canvas,
				      art_point.x, art_point.y,
				      &art_point.x, &art_point.y);
	
	rect->x0 = floor (art_point.x);
	rect->y0 = floor (art_point.y);

	pixbuf = item->details->pixbuf;
	
	rect->x1 = rect->x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf));
	rect->y1 = rect->y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf));
}

void
nautilus_icons_view_icon_item_set_show_stretch_handles (NautilusIconsViewIconItem *item,
							gboolean show_stretch_handles)
{
	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item));
	g_return_if_fail (show_stretch_handles == FALSE || show_stretch_handles == TRUE);
	
	if (!item->details->show_stretch_handles == !show_stretch_handles) {
		return;
	}

	item->details->show_stretch_handles = show_stretch_handles;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
}

/* Check if one of the stretch handles was hit. */
gboolean
nautilus_icons_view_icon_item_get_hit_stretch_handle (NautilusIconsViewIconItem *item,
						      int canvas_x, int canvas_y)
{
	ArtIRect rect;

	g_return_val_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (item), FALSE);

	/* Make sure there are handles to hit. */
	if (!item->details->show_stretch_handles) {
		return FALSE;
	}

	/* Get both the point and the icon rectangle in canvas coordinates. */
	nautilus_icons_view_icon_item_get_icon_canvas_rectangle (item, &rect);

	/* Handle the case where it's not in the rectangle at all. */
	if (canvas_x < rect.x0 || canvas_x >= rect.x1 || canvas_y < rect.y0 || canvas_y >= rect.y1) {
		return FALSE;
	}

	/* Check for hits in the stretch handles. */
	return (canvas_x < rect.x0 + STRETCH_HANDLE_THICKNESS
     		|| canvas_x >= rect.x1 - STRETCH_HANDLE_THICKNESS)
		&& (canvas_y < rect.y0 + STRETCH_HANDLE_THICKNESS
		    || canvas_y >= rect.y1 - STRETCH_HANDLE_THICKNESS);
}
