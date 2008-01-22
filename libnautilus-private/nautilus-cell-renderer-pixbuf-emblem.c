/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-cell-renderer-pixbuf-emblem.c: cell renderer which can render
   an emblem on top of a pixbuf (for use in FMListView and FMTreeView)
 
   Copyright (C) 2003 Juerg Billeter
  
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
  
   This is based on GtkCellRendererPixbuf written by
   Jonathan Blandford <jrb@redhat.com>
  
   Author: Juerg Billeter <j@bitron.ch>
*/

#include "nautilus-cell-renderer-pixbuf-emblem.h"

static void nautilus_cell_renderer_pixbuf_emblem_get_property (GObject *object,
							guint		param_id,
							GValue		*value,
							GParamSpec	*pspec);
static void nautilus_cell_renderer_pixbuf_emblem_set_property (GObject *object,
							guint		param_id,
							const GValue	*value,
							GParamSpec	*pspec);
static void nautilus_cell_renderer_pixbuf_emblem_init       (NautilusCellRendererPixbufEmblem *cellpixbuf);
static void nautilus_cell_renderer_pixbuf_emblem_class_init  (NautilusCellRendererPixbufEmblemClass *klass);
static void nautilus_cell_renderer_pixbuf_emblem_finalize (GObject *object);
static void nautilus_cell_renderer_pixbuf_emblem_create_stock_pixbuf (NautilusCellRendererPixbufEmblem *cellpixbuf,
                                                          GtkWidget             *widget);
static void nautilus_cell_renderer_pixbuf_emblem_get_size   (GtkCellRenderer            *cell,
                                                 GtkWidget                  *widget,
                                                 GdkRectangle               *rectangle,
                                                 gint                       *x_offset,
                                                 gint                       *y_offset,
                                                 gint                       *width,
                                                 gint                       *height);
static void     nautilus_cell_renderer_pixbuf_emblem_render     (GtkCellRenderer            *cell,
                                                          GdkWindow                  *window,
                                                          GtkWidget                  *widget,
                                                          GdkRectangle               *background_area,
                                                          GdkRectangle               *cell_area,
                                                          GdkRectangle               *expose_area,
                                                          GtkCellRendererState                       flags);

enum {
	PROP_ZERO,
	PROP_PIXBUF,
	PROP_PIXBUF_EXPANDER_OPEN,
	PROP_PIXBUF_EXPANDER_CLOSED,
	PROP_STOCK_ID,
	PROP_STOCK_SIZE,
	PROP_STOCK_DETAIL,
	PROP_PIXBUF_EMBLEM
};

static gpointer parent_class;

#define CELLINFO_KEY "nautilus-cell-renderer-pixbuf-emblem-info"

typedef struct _NautilusCellRendererPixbufEmblemInfo NautilusCellRendererPixbufEmblemInfo;
struct _NautilusCellRendererPixbufEmblemInfo
{
	gchar *stock_id;
	GtkIconSize stock_size;
	gchar *stock_detail;
};

GType
nautilus_cell_renderer_pixbuf_emblem_get_type (void)
{
	static GType cell_pixbuf_type = 0;

	if (!cell_pixbuf_type) {
		const GTypeInfo cell_pixbuf_info =
		{
			sizeof (NautilusCellRendererPixbufEmblemClass),
			NULL,                                                     /* base_init */
			NULL,                                                     /* base_finalize */
			(GClassInitFunc) nautilus_cell_renderer_pixbuf_emblem_class_init,
			NULL,                                                     /* class_finalize */
			NULL,                                                     /* class_data */
			sizeof (NautilusCellRendererPixbufEmblem),
			0,                                                        /* n_preallocs */
			(GInstanceInitFunc) nautilus_cell_renderer_pixbuf_emblem_init,
		};

		cell_pixbuf_type = g_type_register_static (GTK_TYPE_CELL_RENDERER,
							   "NautilusCellRendererPixbufEmblem",
							   &cell_pixbuf_info, 0);
	}

	return cell_pixbuf_type;
}

static void
nautilus_cell_renderer_pixbuf_emblem_init (NautilusCellRendererPixbufEmblem *cellpixbuf)
{
	NautilusCellRendererPixbufEmblemInfo *cellinfo;
	
	cellinfo = g_new0 (NautilusCellRendererPixbufEmblemInfo, 1);
	cellinfo->stock_size = GTK_ICON_SIZE_MENU;
	g_object_set_data (G_OBJECT (cellpixbuf), CELLINFO_KEY, cellinfo);
}

static void
nautilus_cell_renderer_pixbuf_emblem_class_init (NautilusCellRendererPixbufEmblemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = nautilus_cell_renderer_pixbuf_emblem_finalize;

	object_class->get_property = nautilus_cell_renderer_pixbuf_emblem_get_property;
	object_class->set_property = nautilus_cell_renderer_pixbuf_emblem_set_property;

	cell_class->get_size = nautilus_cell_renderer_pixbuf_emblem_get_size;
	cell_class->render = nautilus_cell_renderer_pixbuf_emblem_render;

	g_object_class_install_property (object_class,
				   PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							"Pixbuf Object",
							"The pixbuf to render",
							GDK_TYPE_PIXBUF,
							G_PARAM_READABLE |
							G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_OPEN,
				   g_param_spec_object ("pixbuf_expander_open",
							"Pixbuf Expander Open",
							"Pixbuf for open expander",
							GDK_TYPE_PIXBUF,
							G_PARAM_READABLE |
							G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
				   PROP_PIXBUF_EXPANDER_CLOSED,
				   g_param_spec_object ("pixbuf_expander_closed",
							"Pixbuf Expander Closed",
							"Pixbuf for closed expander",
							GDK_TYPE_PIXBUF,
							G_PARAM_READABLE |
							G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock_id",
							"Stock ID",
							"The stock ID of the stock icon to render",
							NULL,
							G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
				   PROP_STOCK_SIZE,
				   g_param_spec_enum ("stock_size",
						      "Size",
						      "The size of the rendered icon",
						      GTK_TYPE_ICON_SIZE,
						      GTK_ICON_SIZE_MENU,
						      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
				   PROP_STOCK_DETAIL,
				   g_param_spec_string ("stock_detail",
							"Detail",
							"Render detail to pass to the theme engine",
							NULL,
							G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_PIXBUF_EMBLEM,
					 g_param_spec_object ("pixbuf_emblem",
							      "Pixbuf Emblem Object",
							      "The emblem to overlay",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READABLE |
							      G_PARAM_WRITABLE));
}

static void
nautilus_cell_renderer_pixbuf_emblem_finalize (GObject *object)
{
	NautilusCellRendererPixbufEmblem *cellpixbuf = NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM (object);
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (object, CELLINFO_KEY);
	
	if (cellpixbuf->pixbuf && cellinfo->stock_id) {
		g_object_unref (cellpixbuf->pixbuf);
	}
	
	if (cellinfo->stock_id) {
		g_free (cellinfo->stock_id);
	}
	
	if (cellinfo->stock_detail) {
		g_free (cellinfo->stock_detail);
	}
	
	g_free (cellinfo);
	g_object_set_data (object, CELLINFO_KEY, NULL);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
nautilus_cell_renderer_pixbuf_emblem_get_property (GObject        *object,
                                       guint           param_id,
                                       GValue         *value,
                                       GParamSpec     *pspec)
{
	NautilusCellRendererPixbufEmblem *cellpixbuf = NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM (object);
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (object, CELLINFO_KEY);
                                                                            
	switch (param_id)
	{
		case PROP_PIXBUF:
			g_value_set_object (value,
					    cellpixbuf->pixbuf ? G_OBJECT (cellpixbuf->pixbuf) : NULL);
			break;
		case PROP_PIXBUF_EXPANDER_OPEN:
			g_value_set_object (value,
					    cellpixbuf->pixbuf_expander_open ? G_OBJECT (cellpixbuf->pixbuf_expander_open) : NULL);
			break;
		case PROP_PIXBUF_EXPANDER_CLOSED:
			g_value_set_object (value,
					    cellpixbuf->pixbuf_expander_closed ? G_OBJECT (cellpixbuf->pixbuf_expander_closed) : NULL);
			break;
		case PROP_STOCK_ID:
			g_value_set_string (value, cellinfo->stock_id);
			break;
		case PROP_STOCK_SIZE:
			g_value_set_enum (value, cellinfo->stock_size);
			break;
		case PROP_STOCK_DETAIL:
			g_value_set_string (value, cellinfo->stock_detail);
			break;
		case PROP_PIXBUF_EMBLEM:
			g_value_set_object (value,
					    cellpixbuf->pixbuf_emblem ? G_OBJECT (cellpixbuf->pixbuf_emblem) : NULL);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}
                                                                                                                                                                       
static void
nautilus_cell_renderer_pixbuf_emblem_set_property (GObject      *object,
						   guint         param_id,
						   const GValue *value,
						   GParamSpec   *pspec)
{
	GdkPixbuf *pixbuf;
	NautilusCellRendererPixbufEmblem *cellpixbuf = NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM (object);
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (object, CELLINFO_KEY);
                                                                            
	switch (param_id)
	{
		case PROP_PIXBUF:
			pixbuf = (GdkPixbuf*) g_value_get_object (value);
			if (pixbuf) {
				g_object_ref (pixbuf);
			}
			if (cellpixbuf->pixbuf) {
				g_object_unref (cellpixbuf->pixbuf);
			}
			cellpixbuf->pixbuf = pixbuf;
			break;
		case PROP_PIXBUF_EXPANDER_OPEN:
			pixbuf = (GdkPixbuf*) g_value_get_object (value);
			if (pixbuf) {
				g_object_ref (pixbuf);
			}
			if (cellpixbuf->pixbuf_expander_open) {
				g_object_unref (cellpixbuf->pixbuf_expander_open);
			}
			cellpixbuf->pixbuf_expander_open = pixbuf;
			break;
		case PROP_PIXBUF_EXPANDER_CLOSED:
			pixbuf = (GdkPixbuf*) g_value_get_object (value);
			if (pixbuf) {
				g_object_ref (pixbuf);
			}
			if (cellpixbuf->pixbuf_expander_closed) {
				g_object_unref (cellpixbuf->pixbuf_expander_closed);
			}
			cellpixbuf->pixbuf_expander_closed = pixbuf;
			break;
		case PROP_STOCK_ID:
			if (cellinfo->stock_id) {
				g_free (cellinfo->stock_id);
			}
			cellinfo->stock_id = g_strdup (g_value_get_string (value));
			break;
		case PROP_STOCK_SIZE:
			cellinfo->stock_size = g_value_get_enum (value);
			break;
		case PROP_STOCK_DETAIL:
			if (cellinfo->stock_detail) {
				g_free (cellinfo->stock_detail);
			}
			cellinfo->stock_detail = g_strdup (g_value_get_string (value));
			break;
		case PROP_PIXBUF_EMBLEM:
			pixbuf = (GdkPixbuf *) g_value_get_object (value);
			if (pixbuf) {
				g_object_ref (pixbuf);
			}
			if (cellpixbuf->pixbuf_emblem) {
				g_object_unref (cellpixbuf->pixbuf_emblem);
			}
			cellpixbuf->pixbuf_emblem = pixbuf;
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

GtkCellRenderer *
nautilus_cell_renderer_pixbuf_emblem_new (void)
{
	return g_object_new (NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM, NULL);
}

static void
nautilus_cell_renderer_pixbuf_emblem_create_stock_pixbuf (NautilusCellRendererPixbufEmblem *cellpixbuf,
							  GtkWidget             *widget)
{
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (G_OBJECT (cellpixbuf), CELLINFO_KEY);

	if (cellpixbuf->pixbuf) {
		g_object_unref (cellpixbuf->pixbuf);
	}

	cellpixbuf->pixbuf = gtk_widget_render_icon (widget,
					       cellinfo->stock_id,
					       cellinfo->stock_size,
					       cellinfo->stock_detail);
}

static void
nautilus_cell_renderer_pixbuf_emblem_get_size (GtkCellRenderer *cell,
				   GtkWidget       *widget,
				   GdkRectangle    *cell_area,
				   gint            *x_offset,
				   gint            *y_offset,
				   gint            *width,
				   gint            *height)
{
	NautilusCellRendererPixbufEmblem *cellpixbuf = (NautilusCellRendererPixbufEmblem *) cell;
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (G_OBJECT (cell), CELLINFO_KEY);
	gint pixbuf_width  = 0;
	gint pixbuf_height = 0;
	gint calc_width;
	gint calc_height;

	if (!cellpixbuf->pixbuf && cellinfo->stock_id)
		nautilus_cell_renderer_pixbuf_emblem_create_stock_pixbuf (cellpixbuf, widget);

	if (cellpixbuf->pixbuf) {
		pixbuf_width  = gdk_pixbuf_get_width (cellpixbuf->pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (cellpixbuf->pixbuf);
	}
	if (cellpixbuf->pixbuf_expander_open) {
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_open));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_open));
	}
	if (cellpixbuf->pixbuf_expander_closed) {
		pixbuf_width  = MAX (pixbuf_width, gdk_pixbuf_get_width (cellpixbuf->pixbuf_expander_closed));
		pixbuf_height = MAX (pixbuf_height, gdk_pixbuf_get_height (cellpixbuf->pixbuf_expander_closed));
	}
  
	calc_width  = (gint) cell->xpad * 2 + pixbuf_width;
	calc_height = (gint) cell->ypad * 2 + pixbuf_height;
  
	if (x_offset) *x_offset = 0;
	if (y_offset) *y_offset = 0;

	if (cell_area && pixbuf_width > 0 && pixbuf_height > 0) {
		if (x_offset) {
			*x_offset = (((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
				1.0 - cell->xalign : cell->xalign) * 
				(cell_area->width - calc_width - 2 * cell->xpad));
			*x_offset = MAX (*x_offset, 0) + cell->xpad;
		}
		if (y_offset) {
			*y_offset = (cell->yalign *
				(cell_area->height - calc_height - 2 * cell->ypad));
			*y_offset = MAX (*y_offset, 0) + cell->ypad;
		}
	}

	if (width)
		*width = calc_width;
  
	if (height)
		*height = calc_height;
}

static void
nautilus_cell_renderer_pixbuf_emblem_render (GtkCellRenderer      *cell,
                                 GdkWindow            *window,
                                 GtkWidget            *widget,
                                 GdkRectangle         *background_area,
                                 GdkRectangle         *cell_area,
                                 GdkRectangle         *expose_area,
                                 GtkCellRendererState  flags)
 
{
	NautilusCellRendererPixbufEmblem *cellpixbuf = (NautilusCellRendererPixbufEmblem *) cell;
	NautilusCellRendererPixbufEmblemInfo *cellinfo = g_object_get_data (G_OBJECT (cell), CELLINFO_KEY);
	GdkPixbuf *pixbuf;
	GdkRectangle pix_rect;
	GdkRectangle pix_emblem_rect;
	GdkRectangle draw_rect;
	gboolean stock_pixbuf = FALSE;
	
	pixbuf = cellpixbuf->pixbuf;
	if (cell->is_expander) {
		if (cell->is_expanded &&
		    cellpixbuf->pixbuf_expander_open != NULL) {
			pixbuf = cellpixbuf->pixbuf_expander_open;
		} else if (!cell->is_expanded &&
			   cellpixbuf->pixbuf_expander_closed != NULL) {
			pixbuf = cellpixbuf->pixbuf_expander_closed;
		}
	}

	if (!pixbuf && !cellinfo->stock_id) {
		return;
	} else if (!pixbuf && cellinfo->stock_id) {
		stock_pixbuf = TRUE;
	}

	nautilus_cell_renderer_pixbuf_emblem_get_size (cell, widget, cell_area,
				     &pix_rect.x,
				     &pix_rect.y,
				     &pix_rect.width,
				     &pix_rect.height);

	if (stock_pixbuf)
		pixbuf = cellpixbuf->pixbuf;
  
	pix_rect.x += cell_area->x;
	pix_rect.y += cell_area->y;
	pix_rect.width  -= cell->xpad * 2;
	pix_rect.height -= cell->ypad * 2;

	if (gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) &&
	    gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect)) {
		gdk_draw_pixbuf (window,
			widget->style->black_gc,
			pixbuf,
			/* pixbuf 0, 0 is at pix_rect.x, pix_rect.y */
			draw_rect.x - pix_rect.x,
			draw_rect.y - pix_rect.y,
			draw_rect.x,
			draw_rect.y,
			draw_rect.width,
			draw_rect.height,
			GDK_RGB_DITHER_NORMAL,
			0, 0);
	}

	if (cellpixbuf->pixbuf_emblem) {
		pix_emblem_rect.width = gdk_pixbuf_get_width (cellpixbuf->pixbuf_emblem);
		pix_emblem_rect.height = gdk_pixbuf_get_height (cellpixbuf->pixbuf_emblem);
		pix_emblem_rect.x = pix_rect.x;
		pix_emblem_rect.y = pix_rect.y + pix_rect.height - pix_emblem_rect.height;
		if (gdk_rectangle_intersect (cell_area, &pix_emblem_rect, &draw_rect) &&
		    gdk_rectangle_intersect (expose_area, &draw_rect, &draw_rect)) {
			gdk_draw_pixbuf (window,
				widget->style->black_gc,
				cellpixbuf->pixbuf_emblem,
				/* pixbuf 0, 0 is at pix_emblem_rect.x, pix_emblem_rect.y */
				draw_rect.x - pix_emblem_rect.x,
				draw_rect.y - pix_emblem_rect.y,
				draw_rect.x,
				draw_rect.y,
				draw_rect.width,
				draw_rect.height,
				GDK_RGB_DITHER_NORMAL,
				0, 0);
		}
	}
}
