/* GNOME libraries - Icon Item class for Icon Container
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
#include <stdio.h>

#include <libgnomeui/gnome-canvas.h>
#include <libgnomeui/gnome-canvas-util.h>
#include <libgnomeui/gnome-icon-text.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>

#include "gnome-icon-container-private.h"
#include "nautilus-icons-view-icon-item.h"

/* Private part of the NautilusIconsViewIconItem structure */
typedef struct {
	/* Our main gdk-pixbuf */
	GdkPixbuf *pixbuf;

	/* text for our label */
	gchar* label;
	
	/* Width value */
	double width;

	/* Height value */
	double height;

	/* X translation */
	double x;

	/* Y translation */
	double y;

	/* width in pixels of the label */
	double text_width;
	
	/* height in pixels of the label */
	double text_height;
	
   	/* boolean to indicate selection state */
   	guint is_selected : 1;
        
	/* boolean to indicate keyboard select state */
	guint is_alt_selected: 1;

   	/* boolean to indicate hilite state (for swallow) */
   	guint is_hilited : 1;
      	
	/* Whether the pixbuf has changed */
	guint need_pixbuf_update : 1;

	/* Whether the transformation or size have changed */
	guint need_xform_update : 1;
} IconItemPrivate;


/* Object argument IDs */
enum {
	ARG_0,
	ARG_PIXBUF,
	ARG_LABEL,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_X,
	ARG_Y,
    	ARG_SELECTED,
    	ARG_ALT_SELECTED,
    	ARG_HILITED
};

/* constants */

#define MAX_LABEL_WIDTH 80

static void nautilus_icons_view_icon_item_class_init (NautilusIconsViewIconItemClass *class);
static void nautilus_icons_view_icon_item_init (NautilusIconsViewIconItem *cpb);
static void nautilus_icons_view_icon_item_destroy (GtkObject *object);
static void nautilus_icons_view_icon_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void nautilus_icons_view_icon_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void nautilus_icons_view_icon_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void nautilus_icons_view_icon_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height);
static void nautilus_icons_view_icon_item_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static double nautilus_icons_view_icon_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);
static void nautilus_icons_view_icon_item_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);

static void nautilus_icons_view_draw_text_box (GnomeCanvasItem* item, GdkDrawable *drawable, GdkFont *title_font, gchar* label, 
                                               gint icon_left, gint icon_bottom, gboolean is_selected, gboolean real_draw);
                                               
static GdkFont* get_font_for_item(GnomeCanvasItem *item);

static GnomeCanvasItemClass *parent_class;


/**
 * nautilus_icons_view_icon_item_get_type:
 * @void:
 *
 * Registers the #NautilusIconsViewIconItem class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #NautilusIconsViewIconItem class.
 **/
GtkType
nautilus_icons_view_icon_item_get_type (void)
{
	static GtkType icon_item_type = 0;

	if (!icon_item_type) {
		static const GtkTypeInfo icon_type_info = {
			"NautilusIconsViewIconItem",
			sizeof (NautilusIconsViewIconItem),
			sizeof (NautilusIconsViewIconItemClass),
			(GtkClassInitFunc) nautilus_icons_view_icon_item_class_init,
			(GtkObjectInitFunc) nautilus_icons_view_icon_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		icon_item_type = gtk_type_unique (gnome_canvas_item_get_type (),
						  &icon_type_info);
	}

	return icon_item_type;
}

/* Class initialization function for the icon canvas item */
static void
nautilus_icons_view_icon_item_class_init (NautilusIconsViewIconItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("NautilusIconsViewIconItem::pixbuf",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_PIXBUF);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::label",
				 GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::width",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::height",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::x",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_X);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::y",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_Y);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::selected",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SELECTED);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::alt_selected",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALT_SELECTED);
	gtk_object_add_arg_type ("NautilusIconsViewIconItem::hilited",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_HILITED);

	object_class->destroy = nautilus_icons_view_icon_item_destroy;
	object_class->set_arg = nautilus_icons_view_icon_item_set_arg;
	object_class->get_arg = nautilus_icons_view_icon_item_get_arg;

	item_class->update = nautilus_icons_view_icon_item_update;
	item_class->draw = nautilus_icons_view_icon_item_draw;
	item_class->render = nautilus_icons_view_icon_item_render;
	item_class->point = nautilus_icons_view_icon_item_point;
	item_class->bounds = nautilus_icons_view_icon_item_bounds;
}

/* Object initialization function for the icon item */
static void
nautilus_icons_view_icon_item_init (NautilusIconsViewIconItem *icon_view_item)
{
	IconItemPrivate *priv;

	priv = g_new0 (IconItemPrivate, 1);
	icon_view_item->priv = priv;

	priv->width = 0.0;
	priv->height = 0.0;
	priv->x = 0.0;
	priv->y = 0.0;

	priv->text_width = 0.0;
	priv->text_height = 0.0;
	
	priv->is_selected = FALSE;
	priv->is_alt_selected = FALSE;
	priv->is_hilited = FALSE;
	
	priv->need_pixbuf_update = FALSE;	
	priv->need_xform_update = FALSE;	
}

/* Destroy handler for the icon canvas item */
static void
nautilus_icons_view_icon_item_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_ICONS_VIEW_ICON_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);
	icon_view_item = (NAUTILUS_ICONS_VIEW_ICON_ITEM (object));
	priv = icon_view_item->priv;

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	if (priv->pixbuf)
		gdk_pixbuf_unref (priv->pixbuf);
        if (priv->label)
                g_free(priv->label);
                
	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
 
/* Set_arg handler for the icon item */

static void
nautilus_icons_view_icon_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;
	GdkPixbuf *pixbuf;
	gchar* new_label;
	double val;

	item = GNOME_CANVAS_ITEM (object);
	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (object);
	priv = icon_view_item->priv;

	switch (arg_id) {

	case ARG_PIXBUF:
		pixbuf = GTK_VALUE_POINTER (*arg);
		if (pixbuf != priv->pixbuf) {
			if (pixbuf) {
				g_return_if_fail (pixbuf->art_pixbuf->format == ART_PIX_RGB);
				g_return_if_fail (pixbuf->art_pixbuf->n_channels == 3
						  || pixbuf->art_pixbuf->n_channels == 4);
				g_return_if_fail (pixbuf->art_pixbuf->bits_per_sample == 8);

				gdk_pixbuf_ref (pixbuf);
			}

			if (priv->pixbuf)
				gdk_pixbuf_unref (priv->pixbuf);

			priv->pixbuf = pixbuf;
		}

		priv->need_pixbuf_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_LABEL:
		new_label = GTK_VALUE_STRING (*arg);
		if (priv->label)
			g_free(priv->label);
		priv->label = g_strdup(new_label);
		
		priv->need_pixbuf_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->width = val;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->height = val;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_X:
		priv->x = GTK_VALUE_DOUBLE (*arg);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_Y:
		priv->y = GTK_VALUE_DOUBLE (*arg);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;
	
        case ARG_SELECTED:
		priv->is_selected = GTK_VALUE_BOOL (*arg);
		gnome_canvas_item_request_update (item);
		break;
         
         case ARG_ALT_SELECTED:
		priv->is_alt_selected = GTK_VALUE_BOOL (*arg);
		gnome_canvas_item_request_update (item);
		break;
       
        case ARG_HILITED:
		priv->is_hilited = GTK_VALUE_BOOL (*arg);
		gnome_canvas_item_request_update (item);
		break;


	default:
		break;
	}
}

/* Get_arg handler for the icon item */
static void
nautilus_icons_view_icon_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;

	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (object);
	priv = icon_view_item->priv;

	switch (arg_id) {
	
	case ARG_PIXBUF:
		GTK_VALUE_POINTER (*arg) = priv->pixbuf;
		break;

	case ARG_LABEL:
		GTK_VALUE_STRING (*arg) = priv->label;
		break;

	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = priv->width;
		break;

	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = priv->height;
		break;

	case ARG_X:
		GTK_VALUE_DOUBLE (*arg) = priv->x;
		break;

	case ARG_Y:
		GTK_VALUE_DOUBLE (*arg) = priv->y;
		break;
	
        case ARG_SELECTED:
                GTK_VALUE_BOOL(*arg) = priv->is_selected;
                break;
         
        case ARG_ALT_SELECTED:
                GTK_VALUE_BOOL(*arg) = priv->is_alt_selected;
                break;
         
        case ARG_HILITED:
                GTK_VALUE_BOOL(*arg) = priv->is_hilited;
                break;
        default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}


/* Bounds and utilities */

/* Computes the amount by which the unit horizontal and vertical vectors will be
 * scaled by an affine transformation.
 */
static void
compute_xform_scaling (double *affine, ArtPoint *i_c, ArtPoint *j_c)
{
	ArtPoint orig, orig_c;
	ArtPoint i, j;

	/* Origin */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, affine);

	/* Horizontal and vertical vectors */

	i.x = 1.0;
	i.y = 0.0;
	art_affine_point (i_c, &i, affine);
	i_c->x -= orig_c.x;
	i_c->y -= orig_c.y;

	j.x = 0.0;
	j.y = 1.0;
	art_affine_point (j_c, &j, affine);
	j_c->x -= orig_c.x;
	j_c->y -= orig_c.y;
}

/* computes the addtional resolution dependent affine needed to
 * fit the image within its viewport defined by x,y,width and height
 * args
 */
static void
compute_viewport_affine (NautilusIconsViewIconItem *icon_view_item, double *viewport_affine, double *i2c)
{
	IconItemPrivate *priv;
	ArtPoint i_c, j_c;
	double i_len, j_len;
	double si_len, sj_len;
	double ti_len, tj_len;
	double scale[6], translate[6];
	double w, h;
	double x, y;

	priv = icon_view_item->priv;

	/* Compute scaling vectors and required width/height */

	compute_xform_scaling (i2c, &i_c, &j_c);

	i_len = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);
	j_len = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

	w = priv->width;
        if (!w)
	    w = priv->pixbuf->art_pixbuf->width;

	h = priv->height;
	if (!h)
            h = priv->pixbuf->art_pixbuf->height;

	x = priv->x;
	y = priv->y;

	si_len = w / priv->pixbuf->art_pixbuf->width;
	sj_len = h / priv->pixbuf->art_pixbuf->height;

	ti_len = x;
	tj_len = y;

	/* Compute the final affine */

	art_affine_scale (scale, si_len, sj_len);
	art_affine_translate (translate, ti_len, tj_len);
	art_affine_multiply (viewport_affine, scale, translate);
}

/* Computes the affine transformation with which the pixbuf needs to be
 * transformed to render it on the canvas.  This is not the same as the
 * item_to_canvas transformation because we may need to scale the pixbuf
 * by some other amount.
 */
static void
compute_render_affine (NautilusIconsViewIconItem *icon_view_item, double *render_affine, double *i2c)
{
	double viewport_affine[6];

	compute_viewport_affine (icon_view_item, viewport_affine, i2c);
	art_affine_multiply (render_affine, viewport_affine, i2c);
}

/* utility to return the proper font for a given item, factoring in the current zoom level */
static GdkFont*
get_font_for_item(GnomeCanvasItem *item)
{
  GnomeIconContainer* container = GNOME_ICON_CONTAINER(item->canvas);
  return container->details->label_font[container->details->zoom_level];
}

/* Recomputes the bounding box of a icon canvas item. */
static void
recompute_bounding_box (NautilusIconsViewIconItem *icon_view_item)
{
	GnomeCanvasItem *item;
	IconItemPrivate *priv;
	double i2c[6], render_affine[6];
	ArtDRect rect;

	item = GNOME_CANVAS_ITEM (icon_view_item);
	priv = icon_view_item->priv;

	if (!priv->pixbuf) {
		item->x1 = item->y1 = item->x2 = item->y2 = 0.0;
		return;
	}

        /* add 2 pixels slop to each side for hilite margin */
	rect.x0 = 0.0;
	rect.x1 = priv->pixbuf->art_pixbuf->width + 4;
	if ((priv->text_width + 4)> rect.x1)
		rect.x1 = priv->text_width + 4;
		
	rect.y0 = 0.0;
	rect.y1 = priv->pixbuf->art_pixbuf->height;
        rect.y1 += floor(priv->text_height);
	
        gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (icon_view_item, render_affine, i2c);
	art_drect_affine_transform (&rect, &rect, render_affine);

	item->x1 = floor (rect.x0);
	item->y1 = floor (rect.y0);
	item->x2 = ceil (rect.x1);
	item->y2 = ceil (rect.y1);
}

/* Update sequence */

/* Update handler for the icon canvas item */
static void
nautilus_icons_view_icon_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	NautilusIconsViewIconItem *icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	IconItemPrivate *priv = icon_view_item->priv;
 	GdkFont *title_font = get_font_for_item(item);

	/* make sure the text box measurements are set up before recalculating the bounding box */
	nautilus_icons_view_draw_text_box(item, NULL, title_font, priv->label, 0, 0, priv->is_selected, FALSE);       	
	recompute_bounding_box(icon_view_item);
	
        if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	if (((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	     && !(GTK_OBJECT_FLAGS (item) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_pixbuf_update
	    || priv->need_xform_update)
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* If we need a pixbuf update, or if the item changed visibility to
	 * shown, recompute the bounding box.
	 */
	if (priv->need_pixbuf_update
	    || priv->need_xform_update
	    || ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
		&& (GTK_OBJECT_FLAGS (icon_view_item) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		recompute_bounding_box (icon_view_item);
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		priv->need_pixbuf_update = FALSE;
		priv->need_xform_update = FALSE;
	}
}



/* Rendering */

/* This is private to libart, but we need it.  Sigh. */
extern void art_rgb_affine_run (int *p_x0, int *p_x1, int y, int src_width, int src_height,
				const double affine[6]);

/* Fills the specified buffer with the transformed version of a pixbuf */
static void
transform_pixbuf (guchar *dest, int x, int y, int width, int height, int rowstride,
		  GdkPixbuf *pixbuf, double *affine)
{
	ArtPixBuf *apb;
	int xx, yy;
	double inv[6];
	guchar *src, *d;
	ArtPoint src_p, dest_p;
	int run_x1, run_x2;
	int src_x, src_y;
	int i;

	apb = pixbuf->art_pixbuf;

	art_affine_invert (inv, affine);

	for (yy = 0; yy < height; yy++) {
		dest_p.y = y + yy + 0.5;

		run_x1 = x;
		run_x2 = x + width;
		art_rgb_affine_run (&run_x1, &run_x2, yy + y,
				    apb->width, apb->height,
				    inv);

		d = dest + yy * rowstride + (run_x1 - x) * 4;

		for (xx = run_x1; xx < run_x2; xx++) {
			dest_p.x = xx + 0.5;
			art_affine_point (&src_p, &dest_p, inv);
			src_x = floor (src_p.x);
			src_y = floor (src_p.y);

			src = apb->pixels + src_y * apb->rowstride + src_x * apb->n_channels;

			for (i = 0; i < apb->n_channels; i++)
				*d++ = *src++;

			if (!apb->has_alpha)
				*d++ = 255; /* opaque */
		}
	}
}

/* utility routine to draw the label in a box, using gnomelib routines */
static void
nautilus_icons_view_draw_text_box (GnomeCanvasItem* item, GdkDrawable *drawable, GdkFont *title_font, gchar* label, 
                                   gint icon_left, gint icon_bottom, gboolean is_selected, gboolean real_draw)
{
	GnomeIconTextInfo *icon_text_info;
 	gint box_left;       
        GdkGC* temp_gc = gdk_gc_new(item->canvas->layout.bin_window);
	NautilusIconsViewIconItem *icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	IconItemPrivate *priv = icon_view_item->priv;
        gint line_width = gdk_string_width(title_font, label);
	gint line_height = gdk_string_height(title_font, label);
	
	if (line_width < floor(MAX_LABEL_WIDTH * item->canvas->pixels_per_unit))
	  {
	    gint item_width = floor(item->x2 - item->x1);
            box_left = icon_left + ((item_width - line_width) >> 1);
	    if (real_draw)
                gdk_draw_string (drawable, title_font, temp_gc, box_left, icon_bottom + line_height, label);
	    line_height += 4; /* extra slop for nicer hilite */
           }
	else
	  {	
	    box_left = icon_left;
            icon_text_info = gnome_icon_layout_text(title_font, label, " -_,;.:?/&",
                                                    floor(MAX_LABEL_WIDTH * item->canvas->pixels_per_unit),
                                                    TRUE);
	    if (real_draw)
                gnome_icon_paint_text(icon_text_info, drawable, temp_gc, box_left, icon_bottom, GTK_JUSTIFY_CENTER);
	    line_width = icon_text_info->width;
            line_height = icon_text_info->height;
 	    gnome_icon_text_info_free(icon_text_info);
         }
			                 
        /* invert to indicate selection if necessary */
        if (is_selected && real_draw)
	  {
	    gdk_gc_set_function(temp_gc, GDK_INVERT); 
	    gdk_draw_rectangle(drawable, temp_gc, TRUE, box_left, icon_bottom - 2, line_width, line_height);	    
	    gdk_gc_set_function(temp_gc, GDK_COPY); 
	  }

	gdk_gc_unref(temp_gc);   

	priv->text_width  =  (double) line_width;
	priv->text_height =  (double) line_height;
}

/* draw the icon item */

static void
nautilus_icons_view_icon_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
                                    int x, int y, int width, int height)
{
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;
	double i2c[6], render_affine[6];
	guchar *buf;
	GdkPixbuf *pixbuf;
	ArtIRect p_rect, a_rect, d_rect;
	gint w, h, icon_height;
        gint center_offset = 0;
        GnomeIconContainer *container = GNOME_ICON_CONTAINER(item->canvas);
 	GdkFont *title_font = get_font_for_item(item); 	
       
	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	priv = icon_view_item->priv;

        /* handle drawing the pixbuf */
        
	if (priv->pixbuf)
          {
            center_offset = nautilus_icons_view_icon_item_center_offset(item);
            
            gnome_canvas_item_i2c_affine (item, i2c);
	    compute_render_affine (icon_view_item, render_affine, i2c);

	    /* Compute the area we need to repaint */

	    p_rect.x0 = item->x1;
	    p_rect.y0 = item->y1;
	    p_rect.x1 = item->x2;
	    p_rect.y1 = item->y2;

	    a_rect.x0 = x - center_offset;
	    a_rect.y0 = y;
	    a_rect.x1 = x + width - center_offset;
	    a_rect.y1 = y + height;

	    art_irect_intersect (&d_rect, &p_rect, &a_rect);
	    if (art_irect_empty (&d_rect))
		    return;

	    /* Create a temporary buffer and transform the pixbuf there */
	    /* FIXME: only do this if really necessary */
	    
	    w = d_rect.x1 - d_rect.x0;
	    h = d_rect.y1 - d_rect.y0;

	    buf = g_new0 (guchar, w * h * 4);
	    transform_pixbuf (buf,
			  d_rect.x0, d_rect.y0,
			  w, h,
			  w * 4,
			  priv->pixbuf, render_affine);

	    pixbuf = gdk_pixbuf_new_from_data (buf, ART_PIX_RGB, TRUE, w, h, w * 4, NULL, NULL);

            gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable,
					        0, 0,
					        d_rect.x0 - x + center_offset, d_rect.y0 - y,
					        w, h,
					        GDK_PIXBUF_ALPHA_BILEVEL,
					        128,
					        GDK_RGB_DITHER_MAX,
					        d_rect.x0, d_rect.y0);
	    gdk_pixbuf_unref (pixbuf);
	    g_free (buf);
          }
          
	  /* now compute the position of the label and draw it */
          if (container->details->zoom_level != NAUTILUS_ZOOM_LEVEL_SMALLEST)
            {
              icon_height = priv->pixbuf->art_pixbuf->height * item->canvas->pixels_per_unit;
              nautilus_icons_view_draw_text_box(item, drawable, title_font, priv->label, item->x1 - x, 
                                                item->y1 - y  + icon_height, priv->is_selected, TRUE);       	
            }
        }

/* return the center offset for this icon */
gint
nautilus_icons_view_icon_item_center_offset(GnomeCanvasItem *item)
{
  NautilusIconsViewIconItem *icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
  IconItemPrivate *priv = icon_view_item->priv;
  gint box_width = floor(item->x2 - item->x1);      
  gint center_offset = (box_width - floor(priv->pixbuf->art_pixbuf->width * item->canvas->pixels_per_unit)) / 2;
  return center_offset;
}

/* Render handler for the icon canvas item */
static void
nautilus_icons_view_icon_item_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;
	double i2c[6], render_affine[6];

	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	priv = icon_view_item->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (icon_view_item, render_affine, i2c);
        gnome_canvas_buf_ensure_buf (buf);

	art_rgb_pixbuf_affine (buf->buf,
			       buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
			       buf->buf_rowstride,
			       priv->pixbuf->art_pixbuf,
			       render_affine,
			       ART_FILTER_NEAREST, NULL);
	buf->is_bg = 0;
}



/* Point handler for the icon canvas item */
static double
nautilus_icons_view_icon_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			   GnomeCanvasItem **actual_item)
{
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;
	double i2c[6], render_affine[6], inv[6];
	ArtPoint c, p;
	gint px, py;
	gint center_offset;
        double no_hit;
	ArtPixBuf *apb;
	guchar *src;

	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	priv = icon_view_item->priv;
        center_offset = nautilus_icons_view_icon_item_center_offset(item);

	*actual_item = item;

	no_hit = item->canvas->pixels_per_unit * 2 + 10;

	if (!priv->pixbuf)
		return no_hit;

	apb = priv->pixbuf->art_pixbuf;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (icon_view_item, render_affine, i2c);
	art_affine_invert (inv, render_affine);

	c.x = cx - center_offset;
	c.y = cy;
	art_affine_point (&p, &c, inv);
	px = p.x;
	py = p.y;

	if (px < 0 || px >= apb->width || py < 0 || py >= apb->height)
		return no_hit;

	if (!apb->has_alpha)
		return 0.0;

	src = apb->pixels + py * apb->rowstride + px * apb->n_channels;

	if (src[3] < 128)
		return no_hit;
	else
		return 0.0;
}


/* Bounds handler for the icon canvas item */
static void
nautilus_icons_view_icon_item_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	NautilusIconsViewIconItem *icon_view_item;
	IconItemPrivate *priv;
	double i2c[6], viewport_affine[6];
	ArtDRect rect;

	icon_view_item = NAUTILUS_ICONS_VIEW_ICON_ITEM (item);
	priv = icon_view_item->priv;

	if (!priv->pixbuf) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

        /* add 2 pixels slop to each side for hilite margin */
	rect.x0 = 0.0;
	rect.x1 = priv->pixbuf->art_pixbuf->width + 4;
	if ((priv->text_width + 4) > rect.x1)
		rect.x1 = priv->text_width + 4;
		
	rect.y0 = 0.0;
	rect.y1 = priv->pixbuf->art_pixbuf->height;
        rect.y1 += floor(priv->text_height);
        
	gnome_canvas_item_i2c_affine (item, i2c);
	compute_viewport_affine (icon_view_item, viewport_affine, i2c);
	art_drect_affine_transform (&rect, &rect, viewport_affine);

	*x1 = rect.x0;
	*y1 = rect.y0;
	*x2 = rect.x1;
	*y2 = rect.y1;
}
