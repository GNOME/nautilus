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
#include "nautilus-icon-canvas-item.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-icon-text.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <libart_lgpl/art_svp_vpath.h>
#include "nautilus-icon-private.h"
#include <eel/eel-string.h>
#include <eel/eel-art-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include "nautilus-global-preferences.h"
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-graphic-effects.h>
#include "nautilus-file-utilities.h"
#include "nautilus-icon-factory.h"
#include "nautilus-theme.h"
#include <eel/eel-smooth-text-layout.h>
#include <eel/eel-smooth-text-layout-cache.h>

/* Comment this out if the new smooth fonts code give you problems
 * This isnt meant to be permanent.  Its just a precaution.
 */
#define EMBLEM_SPACING 2

/* gap between bottom of icon and start of text box */
#define LABEL_OFFSET 1
#define LABEL_LINE_SPACING 0

#define MAX_TEXT_WIDTH_STANDARD 135
#define MAX_TEXT_WIDTH_TIGHTER 80

/* The list of characters that cause line breaks can be localized. */
static const char untranslated_line_break_characters[] = N_(" -_,;.?/&");
#define LINE_BREAK_CHARACTERS _(untranslated_line_break_characters)

/* Private part of the NautilusIconCanvasItem structure. */
struct NautilusIconCanvasItemDetails {
	/* The image, text, font. */
	GdkPixbuf *pixbuf;
	GdkPixbuf *rendered_pixbuf;
	GList *emblem_pixbufs;
	char *editable_text;		/* Text that can be modified by a renaming function */
	char *additional_text;		/* Text that cannot be modifed, such as file size, etc. */
	GdkFont *font;
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
	
	gboolean is_renaming;

	/* Font stuff whilst in smooth mode */
	int smooth_font_size;
	EelScalableFont *smooth_font;
	
	/* Cached rectangle in canvas coordinates */
	ArtIRect canvas_rect;
	ArtIRect text_rect;
	ArtIRect emblem_rect;
};

/* Object argument IDs. */
enum {
	ARG_0,
	ARG_EDITABLE_TEXT,
	ARG_ADDITIONAL_TEXT,
	ARG_FONT,
    	ARG_HIGHLIGHTED_FOR_SELECTION,
    	ARG_HIGHLIGHTED_AS_KEYBOARD_FOCUS,
    	ARG_HIGHLIGHTED_FOR_DROP,
    	ARG_MODIFIER,
	ARG_SMOOTH_FONT_SIZE,
	ARG_SMOOTH_FONT
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

enum {
	BOUNDS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static	guint32 highlight_background_color = EEL_RGBA_COLOR_PACK (0x00, 0x00, 0x00, 0xFF);
static	guint32 highlight_text_color	   = EEL_RGBA_COLOR_PACK (0xFF, 0xFF, 0xFF, 0xFF);
static  guint32 highlight_text_info_color  = EEL_RGBA_COLOR_PACK (0xCC, 0xCC, 0xCC, 0xFF);

static int click_policy_auto_value;

/* GtkObject */
static void     nautilus_icon_canvas_item_initialize_class (NautilusIconCanvasItemClass   *class);
static void     nautilus_icon_canvas_item_initialize       (NautilusIconCanvasItem        *item);
static void     nautilus_icon_canvas_item_destroy          (GtkObject                     *object);
static int      nautilus_icon_canvas_item_event            (GnomeCanvasItem               *item,
							    GdkEvent                      *event);
static void     nautilus_icon_canvas_item_set_arg          (GtkObject                     *object,
							    GtkArg                        *arg,
							    guint                          arg_id);
static void     nautilus_icon_canvas_item_get_arg          (GtkObject                     *object,
							    GtkArg                        *arg,
							    guint                          arg_id);

/* GnomeCanvasItem */
static void     nautilus_icon_canvas_item_update           (GnomeCanvasItem               *item,
							    double                        *affine,
							    ArtSVP                        *clip_path,
							    int                            flags);
static void     nautilus_icon_canvas_item_draw             (GnomeCanvasItem               *item,
							    GdkDrawable                   *drawable,
							    int                            x,
							    int                            y,
							    int                            width,
							    int                            height);
static void     nautilus_icon_canvas_item_render           (GnomeCanvasItem               *item,
							    GnomeCanvasBuf                *buffer);
static double   nautilus_icon_canvas_item_point            (GnomeCanvasItem               *item,
							    double                         x,
							    double                         y,
							    int                            cx,
							    int                            cy,
							    GnomeCanvasItem              **actual_item);
static void     nautilus_icon_canvas_item_bounds           (GnomeCanvasItem               *item,
							    double                        *x1,
							    double                        *y1,
							    double                        *x2,
							    double                        *y2);

/* private */
static void     draw_or_measure_label_text                 (NautilusIconCanvasItem        *item,
							    GdkDrawable                   *drawable,
							    int                            icon_left,
							    int                            icon_bottom);
static void     draw_or_measure_label_text_aa              (NautilusIconCanvasItem        *item,
							    GdkPixbuf                     *destination_pixbuf,
							    int                            icon_left,
							    int                            icon_bottom);
static void     draw_label_text                            (NautilusIconCanvasItem        *item,
							    GdkDrawable                   *drawable,
							    int                            icon_left,
							    int                            icon_bottom);
static void     measure_label_text                         (NautilusIconCanvasItem        *item);
static void     get_icon_canvas_rectangle                  (NautilusIconCanvasItem        *item,
							    ArtIRect                      *rect);
static void     emblem_layout_reset                        (EmblemLayout                  *layout,
							    NautilusIconCanvasItem        *icon_item,
							    ArtIRect                       icon_rect);
static gboolean emblem_layout_next                         (EmblemLayout                  *layout,
							    GdkPixbuf                    **emblem_pixbuf,
							    ArtIRect                      *emblem_rect);
static void     draw_pixbuf                                (GdkPixbuf                     *pixbuf,
							    GdkDrawable                   *drawable,
							    int                            x,
							    int                            y);
static gboolean hit_test_stretch_handle                    (NautilusIconCanvasItem        *item,
							    ArtIRect                       canvas_rect);
static gboolean icon_canvas_item_is_smooth                 (const NautilusIconCanvasItem  *icon_item);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusIconCanvasItem, nautilus_icon_canvas_item, GNOME_TYPE_CANVAS_ITEM)

static EelSmoothTextLayoutCache *layout_cache;

static void
free_layout_cache (void)
{
	gtk_object_unref (GTK_OBJECT (layout_cache));
}

/* Class initialization function for the icon canvas item. */
static void
nautilus_icon_canvas_item_initialize_class (NautilusIconCanvasItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	if (layout_cache == NULL) {
		layout_cache = eel_smooth_text_layout_cache_new ();
		g_atexit (free_layout_cache);
	}

	object_class = GTK_OBJECT_CLASS (class);
	item_class = GNOME_CANVAS_ITEM_CLASS (class);

	gtk_object_add_arg_type	("NautilusIconCanvasItem::editable_text",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_EDITABLE_TEXT);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::additional_text",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_ADDITIONAL_TEXT);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::font",
				 GTK_TYPE_BOXED, GTK_ARG_READWRITE, ARG_FONT);	
	gtk_object_add_arg_type	("NautilusIconCanvasItem::highlighted_for_selection",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_FOR_SELECTION);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::highlighted_as_keyboard_focus",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_AS_KEYBOARD_FOCUS);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::highlighted_for_drop",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HIGHLIGHTED_FOR_DROP);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::smooth_font_size",
				 GTK_TYPE_INT, GTK_ARG_READWRITE, ARG_SMOOTH_FONT_SIZE);
	gtk_object_add_arg_type	("NautilusIconCanvasItem::smooth_font",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_SMOOTH_FONT);

	object_class->destroy = nautilus_icon_canvas_item_destroy;
	object_class->set_arg = nautilus_icon_canvas_item_set_arg;
	object_class->get_arg = nautilus_icon_canvas_item_get_arg;

	signals[BOUNDS_CHANGED]
		= gtk_signal_new ("bounds_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusIconCanvasItemClass,
						     bounds_changed),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	item_class->update = nautilus_icon_canvas_item_update;
	item_class->draw = nautilus_icon_canvas_item_draw;
	item_class->render = nautilus_icon_canvas_item_render;
	item_class->point = nautilus_icon_canvas_item_point;
	item_class->bounds = nautilus_icon_canvas_item_bounds;
	item_class->event = nautilus_icon_canvas_item_event;

	eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_CLICK_POLICY,
					  &click_policy_auto_value);
}

/* Object initialization function for the icon item. */
static void
nautilus_icon_canvas_item_initialize (NautilusIconCanvasItem *icon_item)
{
	NautilusIconCanvasItemDetails *details;

	details = g_new0 (NautilusIconCanvasItemDetails, 1);

	icon_item->details = details;

	icon_item->details->is_renaming = FALSE;

	/* invalidate cached text dimensions initially */
	nautilus_icon_canvas_item_invalidate_label_size (icon_item);
	
	/* set up the default font and size */
	icon_item->details->smooth_font_size = 12;
	icon_item->details->smooth_font = eel_scalable_font_get_default_font ();
}

/* Destroy handler for the icon canvas item. */
static void
nautilus_icon_canvas_item_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	NautilusIconCanvasItem *icon_item;
	NautilusIconCanvasItemDetails *details;

	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);
	icon_item = (NAUTILUS_ICON_CANVAS_ITEM (object));
	details = icon_item->details;

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	if (details->pixbuf != NULL) {
		gdk_pixbuf_unref (details->pixbuf);
	}
	eel_gdk_pixbuf_list_free (details->emblem_pixbufs);
	g_free (details->editable_text);
	g_free (details->additional_text);
	g_free (details->attach_points);
	
	if (details->font != NULL) {
		gdk_font_unref (details->font);
	}

	gtk_object_unref (GTK_OBJECT (icon_item->details->smooth_font));
	icon_item->details->smooth_font = NULL;

	if (details->rendered_pixbuf != NULL) {
		gdk_pixbuf_unref (details->rendered_pixbuf);
	}
	
	g_free (details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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

/* invalidate the text width and height cached in the item details. */
void
nautilus_icon_canvas_item_invalidate_label_size (NautilusIconCanvasItem *item)
{
	item->details->text_width = -1;
	item->details->text_height = -1;
}

/* Set_arg handler for the icon item. */
static void
nautilus_icon_canvas_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusIconCanvasItem *item;
	NautilusIconCanvasItemDetails *details;
	GdkFont *font;

	item =  NAUTILUS_ICON_CANVAS_ITEM (object);
	details = item->details;

	switch (arg_id) {

	case ARG_EDITABLE_TEXT:
		if (eel_strcmp (details->editable_text, GTK_VALUE_STRING (*arg)) == 0) {
			return;
		}

		g_free (details->editable_text);
		details->editable_text = g_strdup (GTK_VALUE_STRING (*arg));
		
		nautilus_icon_canvas_item_invalidate_label_size (item);		
		break;

	case ARG_ADDITIONAL_TEXT:
		if (eel_strcmp (details->additional_text, GTK_VALUE_STRING (*arg)) == 0) {
			return;
		}

		g_free (details->additional_text);
		details->additional_text = g_strdup (GTK_VALUE_STRING (*arg));
		
		nautilus_icon_canvas_item_invalidate_label_size (item);		
		break;

	case ARG_FONT:
		font = GTK_VALUE_BOXED (*arg);
		if (eel_gdk_font_equal (font, details->font)) {
			return;
		}

		if (font != NULL) {
			gdk_font_ref (font);
		}
		if (details->font != NULL) {
			gdk_font_unref (details->font);
		}
		details->font = font;
		
		nautilus_icon_canvas_item_invalidate_label_size (item);		
		break;

	case ARG_HIGHLIGHTED_FOR_SELECTION:
		if (!details->is_highlighted_for_selection == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_for_selection = GTK_VALUE_BOOL (*arg);
		break;
         
        case ARG_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
		if (!details->is_highlighted_as_keyboard_focus == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_as_keyboard_focus = GTK_VALUE_BOOL (*arg);
		break;
		
        case ARG_HIGHLIGHTED_FOR_DROP:
		if (!details->is_highlighted_for_drop == !GTK_VALUE_BOOL (*arg)) {
			return;
		}
		details->is_highlighted_for_drop = GTK_VALUE_BOOL (*arg);
		break;

        case ARG_SMOOTH_FONT:
		nautilus_icon_canvas_item_set_smooth_font (NAUTILUS_ICON_CANVAS_ITEM (object),
							   EEL_SCALABLE_FONT (GTK_VALUE_OBJECT (*arg)));
		nautilus_icon_canvas_item_invalidate_label_size (item);				
		break;

        case ARG_SMOOTH_FONT_SIZE:
		nautilus_icon_canvas_item_set_smooth_font_size (NAUTILUS_ICON_CANVAS_ITEM (object),
								GTK_VALUE_INT (*arg));
		break;
        
	default:
		g_warning ("nautilus_icons_view_item_item_set_arg on unknown argument");
		return;
	}
	
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (object));
}

/* Get_arg handler for the icon item */
static void
nautilus_icon_canvas_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusIconCanvasItemDetails *details;
	
	details = NAUTILUS_ICON_CANVAS_ITEM (object)->details;
	
	switch (arg_id) {
		
	case ARG_EDITABLE_TEXT:
		GTK_VALUE_STRING (*arg) = g_strdup (details->editable_text);
		break;

	case ARG_ADDITIONAL_TEXT:
		GTK_VALUE_STRING (*arg) = g_strdup (details->additional_text);
		break;
		
	case ARG_FONT:
		GTK_VALUE_BOXED (*arg) = details->font;
		break;
				
        case ARG_HIGHLIGHTED_FOR_SELECTION:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_for_selection;
                break;
		
        case ARG_HIGHLIGHTED_AS_KEYBOARD_FOCUS:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_as_keyboard_focus;
                break;
		
        case ARG_HIGHLIGHTED_FOR_DROP:
                GTK_VALUE_BOOL (*arg) = details->is_highlighted_for_drop;
                break;

        case ARG_SMOOTH_FONT:
		gtk_object_ref (GTK_OBJECT (details->smooth_font));
                GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (details->smooth_font);
		break;

        case ARG_SMOOTH_FONT_SIZE:
                GTK_VALUE_INT (*arg) = details->smooth_font_size;
		break;
        
        default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GdkPixbuf *
nautilus_icon_canvas_item_get_image (NautilusIconCanvasItem *item)
{
	NautilusIconCanvasItemDetails *details;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), NULL);

	details = item->details;

	return details->pixbuf;
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
		gdk_pixbuf_ref (image);
	}
	if (details->pixbuf != NULL) {
		gdk_pixbuf_unref (details->pixbuf);
	}
	if (details->rendered_pixbuf != NULL) {
		gdk_pixbuf_unref (details->rendered_pixbuf);
		details->rendered_pixbuf = NULL;
	}

	details->pixbuf = image;
			
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));	
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
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
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
}

/* Recomputes the bounding box of a icon canvas item.
 * This is a generic implementation that could be used for any canvas item
 * class, it has no assumptions about how the item is used.
 */
static void
recompute_bounding_box (NautilusIconCanvasItem *icon_item)
{
	/* The bounds stored in the item is the same as what get_bounds
	 * returns, except it's in canvas coordinates instead of the item's
	 * parent's coordinates.
	 */

	GnomeCanvasItem *item;
	ArtPoint top_left, bottom_right;
	double i2c[6];

	item = GNOME_CANVAS_ITEM (icon_item);

	gnome_canvas_item_get_bounds (item,
				      &top_left.x, &top_left.y,
				      &bottom_right.x, &bottom_right.y);

	gnome_canvas_item_i2c_affine (item->parent, i2c);

	art_affine_point (&top_left, &top_left, i2c);
	art_affine_point (&bottom_right, &bottom_right, i2c);

	item->x1 = top_left.x;
	item->y1 = top_left.y;
	item->x2 = bottom_right.x;
	item->y2 = bottom_right.y;
}

static ArtIRect
compute_text_rectangle (NautilusIconCanvasItem *item,
			ArtIRect icon_rectangle)
{
	ArtIRect text_rectangle;

	/* Compute text rectangle. */
	text_rectangle.x0 = (icon_rectangle.x0 + icon_rectangle.x1) / 2 - item->details->text_width / 2;
	text_rectangle.y0 = icon_rectangle.y1 + LABEL_OFFSET;
	text_rectangle.x1 = text_rectangle.x0 + item->details->text_width;
	text_rectangle.y1 = text_rectangle.y0 + item->details->text_height;

	return text_rectangle;
}

void
nautilus_icon_canvas_item_update_bounds (NautilusIconCanvasItem *item)
{
	ArtIRect before, after, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf;
		
	/* Compute new bounds. */
	before = eel_gnome_canvas_item_get_current_canvas_bounds
		(GNOME_CANVAS_ITEM (item));
	recompute_bounding_box (item);
	after = eel_gnome_canvas_item_get_current_canvas_bounds
		(GNOME_CANVAS_ITEM (item));

	/* If the bounds didn't change, we are done. */
	if (eel_art_irect_equal (before, after)) {
		return;
	}
	
	/* Update canvas and text rect cache */
	get_icon_canvas_rectangle (item, &item->details->canvas_rect);
	item->details->text_rect = compute_text_rectangle (item, item->details->canvas_rect);
	
	/* Update emblem rect cache */
	item->details->emblem_rect.x0 = 0;
	item->details->emblem_rect.x1 = 0;
	item->details->emblem_rect.y0 = 0;
	item->details->emblem_rect.y1 = 0;
	emblem_layout_reset (&emblem_layout, item, item->details->canvas_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		art_irect_union (&item->details->emblem_rect, &item->details->emblem_rect, &emblem_rect);
	}

	/* Send out the bounds_changed signal and queue a redraw. */
	eel_gnome_canvas_request_redraw_rectangle
		(GNOME_CANVAS_ITEM (item)->canvas, before);
	gtk_signal_emit (GTK_OBJECT (item),
			 signals[BOUNDS_CHANGED]);
	eel_gnome_canvas_item_request_redraw
		(GNOME_CANVAS_ITEM (item));
}

/* Update handler for the icon canvas item. */
static void
nautilus_icon_canvas_item_update (GnomeCanvasItem *item,
				  double *affine,
				  ArtSVP *clip_path,
				  int flags)
{
	nautilus_icon_canvas_item_update_bounds (NAUTILUS_ICON_CANVAS_ITEM (item));
	eel_gnome_canvas_item_request_redraw (item);
	EEL_CALL_PARENT (GNOME_CANVAS_ITEM_CLASS, update,
			 (item, affine, clip_path, flags));
}

/* Rendering */

/* routine to underline the text in a gnome_icon_text structure */

static void
gnome_icon_underline_text (GnomeIconTextInfo *text_info,
			   GdkDrawable *drawable,
			   GdkGC *gc,
			   int x, int y)
{
	GList *item;
	int text_width;
	GnomeIconTextInfoRow *row;
	int xpos;

	y += text_info->font->ascent;

	for (item = text_info->rows; item; item = item->next) {
		if (item->data) {
			row = item->data;
			xpos = (text_info->width - row->width) / 2;
			text_width = gdk_text_width_wc(text_info->font, row->text_wc, row->text_length);
			gdk_draw_line(drawable, gc, x + xpos, y + 1, x + xpos + text_width, y + 1);
	
			y += text_info->baseline_skip;
		} else
			y += text_info->baseline_skip / 2;
	}
}

static gboolean
in_single_click_mode (void)
{
	return click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE;
}

/* Keep these for a bit while we work on performance of draw_or_measure_label_text. */

/*
  #define PERFORMANCE_TEST_DRAW_DISABLE
  #define PERFORMANCE_TEST_MEASURE_DISABLE
*/

/* Draw the text in a box, using gnomelib routines. */
static void
draw_or_measure_label_text (NautilusIconCanvasItem *item,
			    GdkDrawable *drawable,
			    int icon_left,
			    int icon_bottom)
{
	NautilusIconCanvasItemDetails *details;
	guint width_so_far, height_so_far;
	GdkGC* gc;
	GdkGCValues save_gc;
	guint32 label_color;
	GnomeCanvasItem *canvas_item;
	int max_text_width;
	int icon_width, text_left, box_left;
	GnomeIconTextInfo *icon_text_info;
	char **pieces;
	const char *text_piece;
	int i;
	char *combined_text;
	gboolean have_editable, have_additional, needs_highlight;

	gc = NULL;
	icon_width = 0;
	
	details = item->details;
	needs_highlight = details->is_highlighted_for_selection || details->is_highlighted_for_drop;

	have_editable = details->editable_text != NULL
		&& details->editable_text[0] != '\0';
	have_additional = details->additional_text != NULL
		&& details->additional_text[0] != '\0';


	/* No font or no text, then do no work. */
	if (details->font == NULL || (!have_editable && !have_additional)) {
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

	/* Combine editable and additional text for processing */
	combined_text = g_strconcat
		(have_editable ? details->editable_text : "",
		 (have_editable && have_additional) ? "\n" : "",
		 have_additional ? details->additional_text : "",
		 NULL);

	width_so_far = 0;
	height_so_far = 0;

	canvas_item = GNOME_CANVAS_ITEM (item);
	if (drawable != NULL) {
		icon_width = details->pixbuf == NULL ? 0 : gdk_pixbuf_get_width (details->pixbuf);
		gc = gdk_gc_new (canvas_item->canvas->layout.bin_window);
		gdk_gc_get_values (gc, &save_gc);
	}
	
	max_text_width = floor (nautilus_icon_canvas_item_get_max_text_width (item));
				
	/* if the icon is highlighted, do some set-up */
	if (needs_highlight && drawable != NULL && !details->is_renaming) {
		gdk_rgb_gc_set_foreground (gc, highlight_background_color);
		
		gdk_draw_rectangle
			(drawable, gc, TRUE,
			 icon_left + (icon_width - details->text_width) / 2,
			 icon_bottom,
			 details->text_width, details->text_height);

		gdk_rgb_gc_set_foreground (gc, highlight_text_color);
	}
	
	if (!needs_highlight && drawable != NULL) {
		label_color = nautilus_icon_container_get_label_color (NAUTILUS_ICON_CONTAINER (canvas_item->canvas), TRUE);
		gdk_rgb_gc_set_foreground (gc, label_color);
	}
	
	pieces = g_strsplit (combined_text, "\n", 0);
	
	for (i = 0; (text_piece = pieces[i]) != NULL; i++) {
		/* Replace empty string with space for measurement and drawing.
		 * This makes empty lines appear, instead of being collapsed out.
		 */
		if (text_piece[0] == '\0') {
			text_piece = " ";
		}
		
		icon_text_info = gnome_icon_layout_text
			(details->font, text_piece,
			 LINE_BREAK_CHARACTERS,
			 max_text_width, TRUE);
		
		/* Draw text if we are not in user rename mode */
		if (drawable != NULL && !details->is_renaming) {
			text_left = icon_left + (icon_width - icon_text_info->width) / 2;
						
			gnome_icon_paint_text
				(icon_text_info, drawable, gc,
				 text_left, icon_bottom + height_so_far,
				 GTK_JUSTIFY_CENTER);
			
			/* if it's highlighted, embolden by drawing twice */
			if (needs_highlight) {
				gnome_icon_paint_text
					(icon_text_info, drawable, gc,
					 text_left + 1, icon_bottom + height_so_far,
					 GTK_JUSTIFY_CENTER);
			}
			
			/* if it's prelit, and we're in click-to-activate mode, underline the text */
			if (details->is_prelit && in_single_click_mode ()) {
				gnome_icon_underline_text
					(icon_text_info, drawable, gc,
					 text_left + 1, icon_bottom + height_so_far);
			}
		}

		if (drawable != NULL && i == 0) {
			if (needs_highlight) {
				gdk_rgb_gc_set_foreground (gc, highlight_text_info_color);
			} else {
				label_color = nautilus_icon_container_get_label_color (NAUTILUS_ICON_CONTAINER (canvas_item->canvas), FALSE);
				gdk_rgb_gc_set_foreground (gc, label_color);
			}
		}
		
		width_so_far = MAX (width_so_far, (guint) icon_text_info->width);
		height_so_far += icon_text_info->height;
		
		gnome_icon_text_info_free (icon_text_info);
	}
	g_strfreev (pieces);
	
	/* add slop used for highlighting, even if we're not highlighting now */
	width_so_far += 4;
	
	if (drawable != NULL) {
		/* Current calculations should match what we measured before drawing.
		 * This assumes that we will always make a separate call to measure
		 * before the call to draw. We might later decide to use this function
		 * differently and change these asserts.
		 */
#if (defined PERFORMANCE_TEST_MEASURE_DISABLE || defined PERFORMANCE_TEST_DRAW_DISABLE)
		g_assert ((int) height_so_far == details->text_height);
		g_assert ((int) width_so_far == details->text_width);
#endif

		gdk_gc_set_foreground (gc, &save_gc.foreground);
	
		box_left = icon_left + (icon_width - width_so_far) / 2;
		
		/* indicate keyboard selection by framing the text with a gray-stippled rectangle */
		if (details->is_highlighted_as_keyboard_focus) {
			gdk_gc_set_stipple (gc, eel_stipple_bitmap ());
			gdk_gc_set_fill (gc, GDK_STIPPLED);
			gdk_draw_rectangle
				(drawable, gc, FALSE,
				 box_left, icon_bottom - 2,
				 width_so_far, 2 + height_so_far);
		}
		
		gdk_gc_unref (gc);
	} else {
		/* If measuring, remember the width & height. */
		details->text_width = width_so_far;
		details->text_height = height_so_far;
	}

	g_free (combined_text);
}

static void
measure_label_text (NautilusIconCanvasItem *item)
{
	/* check to see if the cached values are still valid; if so, there's
	 * no work necessary
	 */
	
	if (item->details->text_width >= 0 && item->details->text_height >= 0) {
		return;
	}
	
	if (icon_canvas_item_is_smooth (item)) {
		draw_or_measure_label_text_aa (item, NULL, 0, 0);
	}
	else {
		draw_or_measure_label_text (item, NULL, 0, 0);
	}
}

static void
draw_label_text (NautilusIconCanvasItem *item, GdkDrawable *drawable,
		 int icon_left, int icon_bottom)
{
	draw_or_measure_label_text (item, drawable, icon_left, icon_bottom + LABEL_OFFSET);
}

static void
draw_stretch_handles (NautilusIconCanvasItem *item, GdkDrawable *drawable,
		      const ArtIRect *rect)
{
	GdkGC *gc;
	char *knob_filename;
	GdkPixbuf *knob_pixbuf;
	int knob_width, knob_height;
	
	if (!item->details->show_stretch_handles) {
		return;
	}

	gc = gdk_gc_new (drawable);

	knob_filename = nautilus_theme_get_image_path ("knob.png");
	knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename);
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);
	
	/* first draw the box */		
	gdk_gc_set_stipple (gc, eel_stipple_bitmap ());
	gdk_gc_set_fill (gc, GDK_STIPPLED);
	gdk_draw_rectangle
		(drawable, gc, FALSE,
		 rect->x0,
		 rect->y0,
		 rect->x1 - rect->x0 - 1,
		 rect->y1 - rect->y0 - 1);
	
	/* draw the stretch handles themselves */
	
	draw_pixbuf (knob_pixbuf, drawable, rect->x0, rect->y0);
	draw_pixbuf (knob_pixbuf, drawable, rect->x0,  rect->y1 - knob_height);
	draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y0);
	draw_pixbuf (knob_pixbuf, drawable, rect->x1 - knob_width, rect->y1 - knob_height);
	
	g_free(knob_filename);
	gdk_pixbuf_unref(knob_pixbuf);	

	gdk_gc_unref (gc);
}

/* utility routine that uses libart to draw an outlined rectangle */
static void
draw_outline_rectangle_aa (GnomeCanvasBuf *buf, int x0, int y0, int x1, int y1, guint outline_color)
{
	ArtVpath vpath[12];
	ArtSVP *path;
	int halfwidth;
	
	halfwidth = 1.0;

	vpath[0].code = ART_MOVETO;
	vpath[0].x = x0 - halfwidth;
	vpath[0].y = y0 - halfwidth;

	vpath[1].code = ART_LINETO;
	vpath[1].x = x0 - halfwidth;
	vpath[1].y = y1 + halfwidth;

	vpath[2].code = ART_LINETO;
	vpath[2].x = x1 + halfwidth;
	vpath[2].y = y1 + halfwidth;

	vpath[3].code = ART_LINETO;
	vpath[3].x = x1 + halfwidth;
	vpath[3].y = y0 - halfwidth;

	vpath[4].code = ART_LINETO;
	vpath[4].x = x0 - halfwidth;
	vpath[4].y = y0 - halfwidth;

	vpath[5].code = ART_MOVETO;
	vpath[5].x = x0 + halfwidth;
	vpath[5].y = y0 + halfwidth;

	vpath[6].code = ART_LINETO;
	vpath[6].x = x1 - halfwidth;
	vpath[6].y = y0 + halfwidth;

	vpath[7].code = ART_LINETO;
	vpath[7].x = x1 - halfwidth;
	vpath[7].y = y1 - halfwidth;

	vpath[8].code = ART_LINETO;
	vpath[8].x = x0 + halfwidth;
	vpath[8].y = y1 - halfwidth;

	vpath[9].code = ART_LINETO;
	vpath[9].x = x0 + halfwidth;
	vpath[9].y = y0 + halfwidth;

	vpath[10].code = ART_END;
	vpath[10].x = 0;
	vpath[10].y = 0;
	
	path = art_svp_from_vpath(vpath);
	gnome_canvas_render_svp(buf, path, outline_color);
	art_svp_free(path);	
}

/* draw the stretch handles in the anti-aliased canvas */
static void
draw_stretch_handles_aa (NautilusIconCanvasItem *item, GnomeCanvasBuf *buf,
			 const ArtIRect *rect)
{
	int knob_width, knob_height;
	GnomeCanvasItem *canvas_item;
	char *knob_filename;
	GdkPixbuf *knob_pixbuf;
	
	if (!item->details->show_stretch_handles) {
		return;
	}

	canvas_item = GNOME_CANVAS_ITEM (item);
	
	knob_filename = nautilus_theme_get_image_path ("knob.png");
	knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename);
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);
		
	/* draw a box to connect the dots */
	draw_outline_rectangle_aa (buf, rect->x0 + 1, rect->y0 + 1,
				   rect->x1 - 1, rect->y1 - 1,
				   EEL_RGBA_COLOR_PACK (153, 153, 153, 127));	

	/* now draw the stretch handles themselves  */
	eel_gnome_canvas_draw_pixbuf (buf, knob_pixbuf, rect->x0, rect->y0);
	eel_gnome_canvas_draw_pixbuf (buf, knob_pixbuf, rect->x0, rect->y1 - knob_height);
	eel_gnome_canvas_draw_pixbuf (buf, knob_pixbuf, rect->x1 - knob_width, rect->y0);
	eel_gnome_canvas_draw_pixbuf (buf, knob_pixbuf, rect->x1 - knob_width, rect->y1 - knob_height);
			
	g_free(knob_filename);
	gdk_pixbuf_unref(knob_pixbuf);	
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
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable, 0, 0, x, y,
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf),
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);
}

/* shared code to highlight or dim the passed-in pixbuf */
static GdkPixbuf *
real_map_pixbuf (NautilusIconCanvasItem *icon_item)
{
	GnomeCanvas *canvas;
	char *audio_filename;
	GdkPixbuf *temp_pixbuf, *old_pixbuf, *audio_pixbuf;
	
	temp_pixbuf = icon_item->details->pixbuf;
	canvas = GNOME_CANVAS_ITEM(icon_item)->canvas;

	gdk_pixbuf_ref (temp_pixbuf);

	if (icon_item->details->is_prelit) {
		old_pixbuf = temp_pixbuf;
		temp_pixbuf = eel_create_spotlight_pixbuf (temp_pixbuf);
		gdk_pixbuf_unref (old_pixbuf);

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
				audio_pixbuf = gdk_pixbuf_new_from_file (audio_filename);
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
				
				gdk_pixbuf_unref (audio_pixbuf);
			}
			
			g_free (audio_filename);
		}
	}
	
	if (icon_item->details->is_highlighted_for_selection
	    || icon_item->details->is_highlighted_for_drop) {
		old_pixbuf = temp_pixbuf;
		temp_pixbuf = eel_create_darkened_pixbuf (temp_pixbuf,
							  0.8 * 255,
							  0.8 * 255);
		gdk_pixbuf_unref (old_pixbuf);
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
	      && icon_item->details->rendered_is_highlighted_for_drop == icon_item->details->is_highlighted_for_drop)) {
		if (icon_item->details->rendered_pixbuf != NULL) {
			gdk_pixbuf_unref (icon_item->details->rendered_pixbuf);
		}
		icon_item->details->rendered_pixbuf = real_map_pixbuf (icon_item);
		icon_item->details->rendered_is_active = icon_item->details->is_active;
		icon_item->details->rendered_is_prelit = icon_item->details->is_prelit;
		icon_item->details->rendered_is_highlighted_for_selection = icon_item->details->is_highlighted_for_selection;
		icon_item->details->rendered_is_highlighted_for_drop = icon_item->details->is_highlighted_for_drop;
	}

	gdk_pixbuf_ref (icon_item->details->rendered_pixbuf);

	return icon_item->details->rendered_pixbuf;
}

/* Draw the icon item for non-anti-aliased mode. */
static void
nautilus_icon_canvas_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				int x, int y, int width, int height)
{
	NautilusIconCanvasItem *icon_item;
	NautilusIconCanvasItemDetails *details;
	ArtIRect icon_rect, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf, *temp_pixbuf;
			
	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);
	details = icon_item->details;

        /* Draw the pixbuf. */
     	if (details->pixbuf == NULL) {
		return;
	}

	/* Compute icon rectangle in drawable coordinates. */
	icon_rect = icon_item->details->canvas_rect;
	icon_rect.x0 -= x;
	icon_rect.y0 -= y;
	icon_rect.x1 -= x;
	icon_rect.y1 -= y;

	/* if the pre-lit or selection flag is set, make a pre-lit or darkened pixbuf and draw that instead */
	temp_pixbuf = map_pixbuf (icon_item);
	draw_pixbuf (temp_pixbuf, drawable, icon_rect.x0, icon_rect.y0);
	gdk_pixbuf_unref (temp_pixbuf);

	/* Draw the emblem pixbufs. */
	emblem_layout_reset (&emblem_layout, icon_item, icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		draw_pixbuf (emblem_pixbuf, drawable, emblem_rect.x0, emblem_rect.y0);
	}
	
	/* Draw stretching handles (if necessary). */
	draw_stretch_handles (icon_item, drawable, &icon_rect);
	
	/* Draw the label text. */
	draw_label_text (icon_item, drawable, icon_rect.x0, icon_rect.y1);
}

static void
draw_or_measure_label_text_aa (NautilusIconCanvasItem *item,
			       GdkPixbuf *destination_pixbuf,
			       int icon_left,
			       int icon_bottom)
{
	NautilusIconCanvasItemDetails *details;
	guint width_so_far, height_so_far;
	guint32 label_name_color;
	guint32 label_info_color;
	GnomeCanvasItem *canvas_item;
	int max_text_width;
	int icon_width, text_left, box_left;
	const EelSmoothTextLayout *smooth_text_layout;
	char **pieces;
	const char *text_piece;
	int i;
	char *combined_text;
	gboolean have_editable, have_additional, needs_highlight;

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
	if (destination_pixbuf == NULL) {
		/* fake out the width */
		details->text_width = 80;
		details->text_height = 20;
		return;
	}
#endif

#ifdef PERFORMANCE_TEST_DRAW_DISABLE
	if (destination_pixbuf != NULL) {
		return;
	}
#endif

	/* Combine editable and additional text for processing */
	combined_text = g_strconcat
		(have_editable ? details->editable_text : "",
		 (have_editable && have_additional) ? "\n" : "",
		 have_additional ? details->additional_text : "",
		 NULL);

	width_so_far = 0;
	height_so_far = 0;

	canvas_item = GNOME_CANVAS_ITEM (item);
	if (destination_pixbuf == NULL ) {
		icon_width = 0;
	} else {
		icon_width = details->pixbuf == NULL ? 0 : gdk_pixbuf_get_width (details->pixbuf);
	}
	
	max_text_width = floor (nautilus_icon_canvas_item_get_max_text_width (item));

	label_name_color = nautilus_icon_container_get_label_color (NAUTILUS_ICON_CONTAINER (canvas_item->canvas), TRUE);
	label_info_color = nautilus_icon_container_get_label_color (NAUTILUS_ICON_CONTAINER (canvas_item->canvas), FALSE);
	
	pieces = g_strsplit (combined_text, "\n", 0);
	
	for (i = 0; (text_piece = pieces[i]) != NULL; i++) {
		guint32 label_color;

		if (needs_highlight) {
			if (i == 0) {
				label_color = highlight_text_color;
			}
			else {
				label_color = highlight_text_info_color;
			}
		} else {
			if (i == 0) {
				label_color = label_name_color;
			}
			else {
				label_color = label_info_color;
			}
		}
		
		/* Replace empty string with space for measurement and drawing.
		 * This makes empty lines appear, instead of being collapsed out.
		 */
		if (text_piece[0] == '\0') {
			text_piece = " ";
		}

		smooth_text_layout = eel_smooth_text_layout_cache_render (layout_cache,
									  text_piece,
									  strlen (text_piece),
									  details->smooth_font,
									  details->smooth_font_size,
									  TRUE, LABEL_LINE_SPACING,
									  max_text_width);
		
		/* Draw text if we are not in user rename mode */
		if (destination_pixbuf != NULL && !details->is_renaming) {
			gboolean underlined;
			ArtIRect destination_area;

			text_left = icon_left + (icon_width - eel_smooth_text_layout_get_width (smooth_text_layout)) / 2;
			
			/* if it's prelit, and we're in click-to-activate mode, underline the text */
			underlined = (details->is_prelit && in_single_click_mode ());
			
			/* draw the shadow in black */
			if (needs_highlight) {
				icon_bottom += 1; /* leave some space for selection frame */
				text_left -= 1;
				
				destination_area.x0 = text_left + 2;
				destination_area.y0 = icon_bottom + height_so_far + 1;
				destination_area.x1 = destination_area.x0 + eel_smooth_text_layout_get_width (smooth_text_layout);
				destination_area.y1 = destination_area.y0 + eel_smooth_text_layout_get_height (smooth_text_layout);
				eel_smooth_text_layout_draw_to_pixbuf (smooth_text_layout,
								       destination_pixbuf,
								       0,
								       0,
								       destination_area,
								       GTK_JUSTIFY_CENTER,
								       underlined,
								       EEL_RGB_COLOR_BLACK,
								       0xff);
			}
			
			destination_area.x0 = text_left;
			destination_area.y0 = icon_bottom + height_so_far;
			destination_area.x1 = destination_area.x0 + eel_smooth_text_layout_get_width (smooth_text_layout);
			destination_area.y1 = destination_area.y0 + eel_smooth_text_layout_get_height (smooth_text_layout);
			eel_smooth_text_layout_draw_to_pixbuf (smooth_text_layout,
							       destination_pixbuf,
							       0,
							       0,
							       destination_area,
							       GTK_JUSTIFY_CENTER,
							       underlined,
							       label_color,
							       0xff);
			
			/* if it's highlighted, embolden by drawing twice */
			if (needs_highlight) {
				destination_area.x0 = text_left + 1;
				destination_area.y0 = icon_bottom + height_so_far;
				destination_area.x1 = destination_area.x0 + eel_smooth_text_layout_get_width (smooth_text_layout);
				destination_area.y1 = destination_area.y0 + eel_smooth_text_layout_get_height (smooth_text_layout);
				eel_smooth_text_layout_draw_to_pixbuf (smooth_text_layout,
								       destination_pixbuf,
								       0,
								       0,
								       destination_area,
								       GTK_JUSTIFY_CENTER,
								       underlined,
								       label_color,
								       0xff);
			}
			
		}
		
		width_so_far = MAX (width_so_far, (guint) eel_smooth_text_layout_get_width (smooth_text_layout));
		height_so_far += eel_smooth_text_layout_get_height (smooth_text_layout) + LABEL_LINE_SPACING;
		
		gtk_object_unref (GTK_OBJECT (smooth_text_layout));
	}
	g_strfreev (pieces);
	
	/* add some extra space for highlighting, even when we don't highlight so things wont move */
	height_so_far += 2; /* extra slop for nicer highlighting */	
	width_so_far += 8;  /* account for emboldening, plus extra to make it look nicer */
	
	if (destination_pixbuf != NULL) {
		/* Current calculations should match what we measured before drawing.
		 * This assumes that we will always make a separate call to measure
		 * before the call to draw. We might later decide to use this function
		 * differently and change these asserts.
		 */
#if (defined PERFORMANCE_TEST_MEASURE_DISABLE || defined PERFORMANCE_TEST_DRAW_DISABLE)
		g_assert ((int) height_so_far == details->text_height);
		g_assert ((int) width_so_far == details->text_width);
#endif
	
		box_left = icon_left + (icon_width - width_so_far) / 2;

	} else {
		/* If measuring, remember the width & height. */
		details->text_width = width_so_far;
		details->text_height = height_so_far;
	}

	g_free (combined_text);
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

static void
draw_label_text_aa (NautilusIconCanvasItem *icon_item, GnomeCanvasBuf *buf, int x, int y, int x_delta)
{
	GdkPixbuf *text_pixbuf;
	NautilusIconContainer *container;
	
	gboolean needs_highlight;
	gboolean have_editable;
	gboolean have_additional;
	
	/* make sure this is really necessary */	
	have_editable = icon_item->details->editable_text != NULL
		&& icon_item->details->editable_text[0] != '\0';
	have_additional = icon_item->details->additional_text != NULL
		&& icon_item->details->additional_text[0] != '\0';

	/* No font or no text, then do no work. */
	if (icon_item->details->smooth_font == NULL
	    || (!have_editable && !have_additional)) {
		icon_item->details->text_height = 0;
		icon_item->details->text_width = 0;			
		return;
	}
	
	if (icon_item->details->is_renaming) {
		/* Exit if we are renaming. We don't need to set the text
		 * width and height to 0 because there is text, it just is not
		 * drawn to the canvas while the renaming widget is dispalyed.
		 */
		return;
	}
		
	/* Set up the background. */
	needs_highlight = icon_item->details->is_highlighted_for_selection
		|| icon_item->details->is_highlighted_for_drop;

	/* Optimizing out the allocation of this pixbuf on every call is a
	 * measureable speed improvement, but only by around 5%.
	 * draw_or_measure_label_text_aa accounts for about 90% of the time.
	 */
 	text_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
 				      TRUE,
 				      8,
 				      icon_item->details->text_width,
 				      icon_item->details->text_height);
	if (needs_highlight) {
		container = NAUTILUS_ICON_CONTAINER (GNOME_CANVAS_ITEM (icon_item)->canvas);	
		eel_gdk_pixbuf_fill_rectangle_with_color (text_pixbuf,
							  eel_gdk_pixbuf_whole_pixbuf,
							  container->details->highlight_color);
		clear_rounded_corners (text_pixbuf, container->details->highlight_frame, 5);
		
	} else {
		eel_gdk_pixbuf_fill_rectangle_with_color (text_pixbuf,
							  eel_gdk_pixbuf_whole_pixbuf,
							  EEL_RGBA_COLOR_PACK (0, 0, 0, 0));
	}

	draw_or_measure_label_text_aa (icon_item, text_pixbuf, x_delta, 0);
	
	/* Draw the pixbuf containing the label. */

	eel_gnome_canvas_draw_pixbuf (buf, text_pixbuf, x - x_delta, y + LABEL_OFFSET);

	gdk_pixbuf_unref (text_pixbuf);	

	/* draw the keyboard selection focus indicator if necessary */
	if (icon_item->details->is_highlighted_as_keyboard_focus) {
		draw_outline_rectangle_aa (buf, x - x_delta + 1, y + 1,
					   x - x_delta + icon_item->details->text_width,
					   y + icon_item->details->text_height,
					   EEL_RGBA_COLOR_PACK (153, 153, 153, 127));
	}
}

/* draw the item for anti-aliased mode */
static void
nautilus_icon_canvas_item_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	ArtIRect icon_rect, emblem_rect;
	EmblemLayout emblem_layout;
	GdkPixbuf *emblem_pixbuf, *temp_pixbuf;
	NautilusIconCanvasItem *icon_item;
	int x_delta;

	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);
	
	/* map the pixbuf for selection or other effects */
	temp_pixbuf = map_pixbuf (icon_item);

	icon_rect = icon_item->details->canvas_rect;

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	/* draw the icon */
	eel_gnome_canvas_draw_pixbuf (buf, temp_pixbuf, icon_rect.x0, icon_rect.y0);

	gdk_pixbuf_unref (temp_pixbuf);

	/* draw the emblems */	
	emblem_layout_reset (&emblem_layout, icon_item, icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		eel_gnome_canvas_draw_pixbuf (buf, emblem_pixbuf, emblem_rect.x0, emblem_rect.y0);
	}

	/* draw the stretch handles */
	draw_stretch_handles_aa (icon_item, buf, &icon_rect);
		
	/* draw the text */
	x_delta = (icon_item->details->text_width - (icon_rect.x1 - icon_rect.x0)) / 2;
	draw_label_text_aa (icon_item, buf, icon_rect.x0, icon_rect.y1, x_delta);
}


/* handle events */

static int
nautilus_icon_canvas_item_event (GnomeCanvasItem *item, GdkEvent *event)
{
	NautilusIconCanvasItem *icon_item;

	icon_item = NAUTILUS_ICON_CANVAS_ITEM (item);

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		if (!icon_item->details->is_prelit) {
			icon_item->details->is_prelit = TRUE;
			gnome_canvas_item_request_update (item);
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
			gnome_canvas_item_request_update (item);
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
	
	/* Check for hit in the icon. If we're highlighted for dropping, anywhere in the rect is OK */
	if (icon_item->details->is_highlighted_for_drop) {
		if (eel_art_irect_hits_irect (icon_item->details->canvas_rect, canvas_rect)) {
			return TRUE;
		}
	} else {
		if (hit_test_pixbuf (details->pixbuf, icon_item->details->canvas_rect, canvas_rect)) {
			return TRUE;
		}
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
nautilus_icon_canvas_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
				 GnomeCanvasItem **actual_item)
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

/* Bounds handler for the icon canvas item. */
static void
nautilus_icon_canvas_item_bounds (GnomeCanvasItem *item,
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

	measure_label_text (icon_item);

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
	text_rect = compute_text_rectangle (icon_item, icon_rect);

	/* Compute total rectangle, adding in emblem rectangles. */
	art_irect_union (&total_rect, &icon_rect, &text_rect);
	emblem_layout_reset (&emblem_layout, icon_item, icon_rect);
	while (emblem_layout_next (&emblem_layout, &emblem_pixbuf, &emblem_rect)) {
		art_irect_union (&total_rect, &total_rect, &emblem_rect);
	}
        
	/* Return the result. */
	pixels_per_unit = item->canvas->pixels_per_unit;
	*x1 = total_rect.x0 / pixels_per_unit;
	*y1 = total_rect.y0 / pixels_per_unit;
	*x2 = total_rect.x1 / pixels_per_unit;
	*y2 = total_rect.y1 / pixels_per_unit;
}

/* Get the rectangle of the icon only, in world coordinates. */
ArtDRect
nautilus_icon_canvas_item_get_icon_rectangle (const NautilusIconCanvasItem *item)
{
	ArtDRect rectangle;
	double i2w[6];
	ArtPoint art_point;
	double pixels_per_unit;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item), eel_art_drect_empty);

	gnome_canvas_item_i2w_affine (GNOME_CANVAS_ITEM (item), i2w);

	art_point.x = 0;
	art_point.y = 0;
	art_affine_point (&art_point, &art_point, i2w);
	
	rectangle.x0 = art_point.x;
	rectangle.y0 = art_point.y;

	pixbuf = item->details->pixbuf;
	pixels_per_unit = GNOME_CANVAS_ITEM (item)->canvas->pixels_per_unit;
	
	rectangle.x1 = rectangle.x0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_width (pixbuf)) / pixels_per_unit;
	rectangle.y1 = rectangle.y0 + (pixbuf == NULL ? 0 : gdk_pixbuf_get_height (pixbuf)) / pixels_per_unit;

	return rectangle;
}

/* Get the rectangle of the icon only, in canvas coordinates. */
static void
get_icon_canvas_rectangle (NautilusIconCanvasItem *item,
			   ArtIRect *rect)
{
	double i2c[6];
	ArtPoint art_point;
	GdkPixbuf *pixbuf;

	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (item));
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
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
}

/* Check whether the item is in smooth mode */
static gboolean
icon_canvas_item_is_smooth (const NautilusIconCanvasItem *icon_item)
{
	GnomeCanvas *parent_canvas;

	g_return_val_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (icon_item), FALSE);

	parent_canvas = GNOME_CANVAS (GNOME_CANVAS_ITEM (icon_item)->canvas);

	return parent_canvas->aa;
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
	knob_pixbuf = gdk_pixbuf_new_from_file (knob_filename);
	knob_width = gdk_pixbuf_get_width (knob_pixbuf);
	knob_height = gdk_pixbuf_get_height (knob_pixbuf);

	g_free(knob_filename);
	gdk_pixbuf_unref(knob_pixbuf);	
	
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
	
	gnome_canvas_w2c (GNOME_CANVAS_ITEM (item)->canvas,
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

	if (item->details->is_renaming == state) {
		return;
	}

	item->details->is_renaming = state;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
}

double
nautilus_icon_canvas_item_get_max_text_width (NautilusIconCanvasItem *item)
{
	GnomeCanvasItem *canvas_item;
	
	canvas_item = GNOME_CANVAS_ITEM (item);
	if (nautilus_icon_container_is_tighter_layout (NAUTILUS_ICON_CONTAINER (canvas_item->canvas))) {
		return MAX_TEXT_WIDTH_TIGHTER * canvas_item->canvas->pixels_per_unit;
	} else {
		return MAX_TEXT_WIDTH_STANDARD * canvas_item->canvas->pixels_per_unit;
	}

}

void
nautilus_icon_canvas_item_set_smooth_font (NautilusIconCanvasItem	*icon_item,
					   EelScalableFont		*font)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (icon_item));
	g_return_if_fail (EEL_IS_SCALABLE_FONT (font));

	gtk_object_unref (GTK_OBJECT (icon_item->details->smooth_font));

	gtk_object_ref (GTK_OBJECT (font));

	icon_item->details->smooth_font = font;

	/* Only need to update if in smooth mode */
	if (icon_canvas_item_is_smooth (icon_item)) {
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (icon_item));
	}
}

void
nautilus_icon_canvas_item_set_smooth_font_size (NautilusIconCanvasItem *icon_item,
						int font_size)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CANVAS_ITEM (icon_item));
	g_return_if_fail (font_size > 0);

	if (icon_item->details->smooth_font_size == font_size) {
		return;
	}

	icon_item->details->smooth_font_size = font_size;

	/* Only need to update if in smooth mode */
	if (icon_canvas_item_is_smooth (icon_item)) {
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (icon_item));
	}
}
