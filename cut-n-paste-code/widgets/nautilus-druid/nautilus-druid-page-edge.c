/* gnome-druid-page-edge.c
 * Copyright (C) 1999  Red Hat, Inc.
 *
 * All rights reserved.
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
/*
  @NOTATION@
*/

#include <config.h>

#include "nautilus-druid-page-edge.h"

#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/libgnomeui.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include "nautilus-druid.h"
#include <libgnome/gnome-i18n.h>


struct _NautilusDruidPageEdgePrivate
{
	GtkWidget *canvas;
	GnomeCanvasItem *background_item;
	GnomeCanvasItem *textbox_item;
	GnomeCanvasItem *text_item;
	GnomeCanvasItem *logo_item;
	GnomeCanvasItem *logoframe_item;
	GnomeCanvasItem *watermark_item;
	GnomeCanvasItem *title_item;
};

static void nautilus_druid_page_edge_init   	(NautilusDruidPageEdge		*druid_page_edge);
static void nautilus_druid_page_edge_class_init	(NautilusDruidPageEdgeClass	*klass);
static void nautilus_druid_page_edge_destroy 	(GtkObject                      *object);
static void nautilus_druid_page_edge_construct     (NautilusDruidPageEdge             *druid_page_edge);
static void nautilus_druid_page_edge_configure_size(NautilusDruidPageEdge             *druid_page_edge,
						 gint                            width,
						 gint                            height);
static void nautilus_druid_page_edge_size_allocate (GtkWidget                      *widget,
						 GtkAllocation                  *allocation);
static void nautilus_druid_page_edge_prepare	(NautilusDruidPage		        *page,
						 GtkWidget                      *druid,
						 gpointer 			*data);
static NautilusDruidPageClass *parent_class = NULL;

#define LOGO_WIDTH 50.0
#define DRUID_PAGE_HEIGHT 318
#define DRUID_PAGE_WIDTH 516
#define DRUID_PAGE_LEFT_WIDTH 100.0
#define GDK_COLOR_TO_RGBA(color) GNOME_CANVAS_COLOR (color.red/256, color.green/256, color.blue/256)


GtkType
nautilus_druid_page_edge_get_type (void)
{
	static GtkType druid_page_edge_type = 0;

	if (!druid_page_edge_type)
	{
		static const GtkTypeInfo druid_page_edge_info =
		{
			"NautilusDruidPageEdge",
			sizeof (NautilusDruidPageEdge),
			sizeof (NautilusDruidPageEdgeClass),
			(GtkClassInitFunc) nautilus_druid_page_edge_class_init,
			(GtkObjectInitFunc) nautilus_druid_page_edge_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		druid_page_edge_type = gtk_type_unique (nautilus_druid_page_get_type (), &druid_page_edge_info);
	}

	return druid_page_edge_type;
}

static void
nautilus_druid_page_edge_class_init (NautilusDruidPageEdgeClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = nautilus_druid_page_edge_destroy;
	widget_class = (GtkWidgetClass*) klass;
	widget_class->size_allocate = nautilus_druid_page_edge_size_allocate;
	parent_class = gtk_type_class (nautilus_druid_page_get_type ());
}

static void
nautilus_druid_page_edge_init (NautilusDruidPageEdge *druid_page_edge)
{
	druid_page_edge->_priv = g_new0(NautilusDruidPageEdgePrivate, 1);

	/* initialize the color values */
	druid_page_edge->background_color.red = 6400; /* midnight blue */
	druid_page_edge->background_color.green = 6400;
	druid_page_edge->background_color.blue = 28672;
	druid_page_edge->textbox_color.red = 65280; /* white */
	druid_page_edge->textbox_color.green = 65280;
	druid_page_edge->textbox_color.blue = 65280;
	druid_page_edge->logo_background_color.red = 65280; /* white */
	druid_page_edge->logo_background_color.green = 65280;
	druid_page_edge->logo_background_color.blue = 65280;
	druid_page_edge->title_color.red = 65280; /* white */
	druid_page_edge->title_color.green = 65280;
	druid_page_edge->title_color.blue = 65280;
	druid_page_edge->text_color.red = 0; /* black */
	druid_page_edge->text_color.green = 0;
	druid_page_edge->text_color.blue = 0;

	/* Set up the canvas */
	gtk_container_set_border_width (GTK_CONTAINER (druid_page_edge), 0);
	druid_page_edge->_priv->canvas = gnome_canvas_new ();
	gtk_widget_set_usize (druid_page_edge->_priv->canvas, DRUID_PAGE_WIDTH, DRUID_PAGE_HEIGHT);
	gtk_widget_show (druid_page_edge->_priv->canvas);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (druid_page_edge->_priv->canvas), 0.0, 0.0, DRUID_PAGE_WIDTH, DRUID_PAGE_HEIGHT);
	gtk_container_add (GTK_CONTAINER (druid_page_edge), druid_page_edge->_priv->canvas);

}

static void
nautilus_druid_page_edge_destroy(GtkObject *object)
{
	NautilusDruidPageEdge *druid_page_edge = NAUTILUS_DRUID_PAGE_EDGE(object);

	g_free(druid_page_edge->_priv);
	druid_page_edge->_priv = NULL;

	if(GTK_OBJECT_CLASS(parent_class)->destroy)
		(* GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}


static void
nautilus_druid_page_edge_configure_size (NautilusDruidPageEdge *druid_page_edge, gint width, gint height)
{
	gfloat watermark_width = DRUID_PAGE_LEFT_WIDTH;
	gfloat watermark_height = (gfloat) height - LOGO_WIDTH + GNOME_PAD * 2.0;
	gfloat watermark_ypos = LOGO_WIDTH + GNOME_PAD * 2.0;

	if (druid_page_edge->watermark_image) {
		watermark_width = gdk_pixbuf_get_width (druid_page_edge->watermark_image);
		watermark_height = gdk_pixbuf_get_height (druid_page_edge->watermark_image);
		watermark_ypos = (gfloat) height - watermark_height;
		if (watermark_width < 1)
			watermark_width = 1.0;
		if (watermark_height < 1)
			watermark_height = 1.0;
	}

	gnome_canvas_item_set (druid_page_edge->_priv->background_item,
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", (gfloat) width,
			       "y2", (gfloat) height,
			       "width_units", 1.0, NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->textbox_item,
			       "x1", watermark_width,
			       "y1", LOGO_WIDTH + GNOME_PAD * 2.0,
			       "x2", (gfloat) width,
			       "y2", (gfloat) height,
			       "width_units", 1.0, NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->logoframe_item,
			       "x1", (gfloat) width - LOGO_WIDTH -GNOME_PAD,
			       "y1", (gfloat) GNOME_PAD,
			       "x2", (gfloat) width - GNOME_PAD,
			       "y2", (gfloat) GNOME_PAD + LOGO_WIDTH,
			       "width_units", 1.0, NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->logo_item,
			       "x", (gfloat) width - GNOME_PAD - LOGO_WIDTH,
			       "y", (gfloat) GNOME_PAD,
			       "width", (gfloat) LOGO_WIDTH,
			       "height", (gfloat) LOGO_WIDTH, NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->watermark_item,
			       "x", 0.0,
			       "y", watermark_ypos,
			       "width", watermark_width,
			       "height", watermark_height,
			       NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->title_item,
			       "x", 15.0,
			       "y", (gfloat) GNOME_PAD + LOGO_WIDTH / 2.0,
			       "anchor", GTK_ANCHOR_WEST,
			       NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->text_item,
			       "x", ((width - watermark_width) * 0.5) + watermark_width,
			       "y", LOGO_WIDTH + GNOME_PAD * 2.0 + (height - (LOGO_WIDTH + GNOME_PAD * 2.0))/ 2.0,
			       "anchor", GTK_ANCHOR_CENTER,
			       NULL);
}

static void
nautilus_druid_page_edge_construct (NautilusDruidPageEdge *druid_page_edge)
{
	guint32 fill_color, outline_color;

	/* set up the rest of the page */
	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->background_color);
	druid_page_edge->_priv->background_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_rect_get_type (),
				       "fill_color_rgba", fill_color,
				       NULL);

	outline_color = fill_color;
	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->textbox_color);
	druid_page_edge->_priv->textbox_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_rect_get_type (),
				       "fill_color_rgba", fill_color,
				       "outline_color_rgba", outline_color,
				       NULL);

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->logo_background_color);
	druid_page_edge->_priv->logoframe_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_rect_get_type (),
				       "fill_color_rgba", fill_color,
				       NULL);

	druid_page_edge->_priv->logo_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x_in_pixels", TRUE, "y_in_pixels", TRUE,
				       NULL);

	if (druid_page_edge->logo_image != NULL)
		gnome_canvas_item_set (druid_page_edge->_priv->logo_item,
				       "pixbuf", druid_page_edge->logo_image, NULL);

	druid_page_edge->_priv->watermark_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x_in_pixels", TRUE, "y_in_pixels", TRUE,
				       NULL);

	if (druid_page_edge->watermark_image != NULL)
		gnome_canvas_item_set (druid_page_edge->_priv->watermark_item,
				       "pixbuf", druid_page_edge->watermark_image, NULL);

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->title_color);
	druid_page_edge->_priv->title_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_text_get_type (),
				       "text", druid_page_edge->title,
				       "fill_color_rgba", fill_color,
				       "font", _("-adobe-helvetica-bold-r-normal-*-*-180-*-*-p-*-*-*"),
				       NULL);

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->text_color);
	druid_page_edge->_priv->text_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_edge->_priv->canvas)),
				       gnome_canvas_text_get_type (),
				       "text", druid_page_edge->text,
				       "justification", GTK_JUSTIFY_LEFT,
				       "font", _("-adobe-helvetica-medium-r-normal-*-*-120-*-*-p-*-*-*"),
				       "fill_color_rgba", fill_color,
				       NULL);

	nautilus_druid_page_edge_configure_size (druid_page_edge, DRUID_PAGE_WIDTH, DRUID_PAGE_HEIGHT);
	gtk_signal_connect (GTK_OBJECT (druid_page_edge),
			    "prepare",
			    nautilus_druid_page_edge_prepare,
			    NULL);
}
static void
nautilus_druid_page_edge_prepare (NautilusDruidPage *page,
			       GtkWidget *druid,
			       gpointer *data)
{
	switch (NAUTILUS_DRUID_PAGE_EDGE (page)->position) {
	case GNOME_EDGE_START:
		nautilus_druid_set_buttons_sensitive (NAUTILUS_DRUID (druid), FALSE, TRUE, TRUE);
		nautilus_druid_set_show_finish (NAUTILUS_DRUID (druid), FALSE);
		gtk_widget_grab_default (NAUTILUS_DRUID (druid)->next);
		break;
	case GNOME_EDGE_FINISH:
		nautilus_druid_set_buttons_sensitive (NAUTILUS_DRUID (druid), TRUE, FALSE, TRUE);
		nautilus_druid_set_show_finish (NAUTILUS_DRUID (druid), TRUE);
		gtk_widget_grab_default (NAUTILUS_DRUID (druid)->finish);
		break;
	case GNOME_EDGE_OTHER:
	default:
		break;
	}
}


static void
nautilus_druid_page_edge_size_allocate   (GtkWidget               *widget,
				       GtkAllocation           *allocation)
{
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (NAUTILUS_DRUID_PAGE_EDGE (widget)->_priv->canvas),
					0.0, 0.0,
					allocation->width,
					allocation->height);
	nautilus_druid_page_edge_configure_size (NAUTILUS_DRUID_PAGE_EDGE (widget),
					      allocation->width,
					      allocation->height);
}

#if 0
{
/* Fragment left behind by CVS.  Don't get it. */
gnome_canvas_item_set (druid_page_edge->_priv->background_item,
		       "fill_color_gdk", &(druid_page_edge->background_color),
		       NULL);
gnome_canvas_item_set (druid_page_edge->_priv->textbox_item,
		       "fill_color_gdk", &(druid_page_edge->textbox_color),
		       "outline_color_gdk", &(druid_page_edge->background_color),
		       NULL);
gnome_canvas_item_set (druid_page_edge->_priv->logoframe_item,
		       "fill_color_gdk", &druid_page_edge->logo_background_color,
		       NULL);
gnome_canvas_item_set (druid_page_edge->_priv->title_item,
		       "fill_color_gdk", &druid_page_edge->title_color,
		       NULL);
gnome_canvas_item_set (druid_page_edge->_priv->text_item,
		       "fill_color_gdk", &druid_page_edge->text_color,
		       NULL);
GTK_WIDGET_CLASS (parent_class)->realize (widget);
}
#endif

/**
 * nautilus_druid_page_edge_new:
 *
 * Creates a new NautilusDruidPageEdge widget.
 *
 * Return value: Pointer to new NautilusDruidPageEdge
 **/
/* Public functions */
GtkWidget *
nautilus_druid_page_edge_new (GnomeEdgePosition position)
{
	GtkWidget *retval =  GTK_WIDGET (gtk_type_new (nautilus_druid_page_edge_get_type ()));

	NAUTILUS_DRUID_PAGE_EDGE (retval)->position = position;
	NAUTILUS_DRUID_PAGE_EDGE (retval)->title = g_strdup ("");
	NAUTILUS_DRUID_PAGE_EDGE (retval)->text = g_strdup ("");
	NAUTILUS_DRUID_PAGE_EDGE (retval)->logo_image = NULL;
	NAUTILUS_DRUID_PAGE_EDGE (retval)->watermark_image = NULL;
	nautilus_druid_page_edge_construct (NAUTILUS_DRUID_PAGE_EDGE (retval));
	return retval;
}
/**
 * nautilus_druid_page_edge_new_with_vals:
 * @title: The title.
 * @text: The introduction text.
 * @logo: The logo in the upper right corner.
 * @watermark: The watermark on the left.
 *
 * This will create a new GNOME Druid Edge page, with the values
 * given.  It is acceptable for any of them to be %NULL.
 *
 * Return value: GtkWidget pointer to new NautilusDruidPageEdge.
 **/
GtkWidget *
nautilus_druid_page_edge_new_with_vals (GnomeEdgePosition position,
				     const gchar *title,
				     const gchar* text,
				     GdkPixbuf *logo,
				     GdkPixbuf *watermark)
{
	GtkWidget *retval =  GTK_WIDGET (gtk_type_new (nautilus_druid_page_edge_get_type ()));

	NAUTILUS_DRUID_PAGE_EDGE (retval)->position = position;
	NAUTILUS_DRUID_PAGE_EDGE (retval)->title = g_strdup (title);
	NAUTILUS_DRUID_PAGE_EDGE (retval)->text = g_strdup (text);

	if (logo)
		gdk_pixbuf_ref (logo);
	NAUTILUS_DRUID_PAGE_EDGE (retval)->logo_image = logo;

	if (watermark)
		gdk_pixbuf_ref (watermark);
	NAUTILUS_DRUID_PAGE_EDGE (retval)->watermark_image = watermark;
	nautilus_druid_page_edge_construct (NAUTILUS_DRUID_PAGE_EDGE (retval));
	return retval;
}

/**
 * nautilus_druid_page_edge_set_bg_color:
 * @druid_page_edge: A DruidPageEdge.
 * @color: The new background color.
 *
 * This will set the background color to be the @color.  You do not
 * need to allocate the color, as the @druid_page_edge will do it for
 * you.
 **/
void
nautilus_druid_page_edge_set_bg_color      (NautilusDruidPageEdge *druid_page_edge,
					 GdkColor *color)
{
	guint fill_color;

	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	druid_page_edge->background_color.red = color->red;
	druid_page_edge->background_color.green = color->green;
	druid_page_edge->background_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->background_color);

	gnome_canvas_item_set (druid_page_edge->_priv->textbox_item,
			       "outline_color_rgba", fill_color,
			       NULL);
	gnome_canvas_item_set (druid_page_edge->_priv->background_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}

void
nautilus_druid_page_edge_set_textbox_color (NautilusDruidPageEdge *druid_page_edge,
					 GdkColor *color)
{
	guint fill_color;

	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	druid_page_edge->textbox_color.red = color->red;
	druid_page_edge->textbox_color.green = color->green;
	druid_page_edge->textbox_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->textbox_color);
	gnome_canvas_item_set (druid_page_edge->_priv->textbox_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}

void
nautilus_druid_page_edge_set_logo_bg_color (NautilusDruidPageEdge *druid_page_edge,
					 GdkColor *color)
{
	guint fill_color;

	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	druid_page_edge->logo_background_color.red = color->red;
	druid_page_edge->logo_background_color.green = color->green;
	druid_page_edge->logo_background_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->logo_background_color);
	gnome_canvas_item_set (druid_page_edge->_priv->logoframe_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}
void
nautilus_druid_page_edge_set_title_color   (NautilusDruidPageEdge *druid_page_edge,
					 GdkColor *color)
{
	guint32 fill_color;

	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	druid_page_edge->title_color.red = color->red;
	druid_page_edge->title_color.green = color->green;
	druid_page_edge->title_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->title_color);
	gnome_canvas_item_set (druid_page_edge->_priv->title_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}
void
nautilus_druid_page_edge_set_text_color    (NautilusDruidPageEdge *druid_page_edge,
					 GdkColor *color)
{
	guint32 fill_color;

	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	druid_page_edge->text_color.red = color->red;
	druid_page_edge->text_color.green = color->green;
	druid_page_edge->text_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_edge->text_color);
	gnome_canvas_item_set (druid_page_edge->_priv->text_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}
void
nautilus_druid_page_edge_set_text          (NautilusDruidPageEdge *druid_page_edge,
					 const gchar *text)
{
	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	g_free (druid_page_edge->text);
	druid_page_edge->text = g_strdup (text);
	gnome_canvas_item_set (druid_page_edge->_priv->text_item,
			       "text", druid_page_edge->text,
			       NULL);
}
void
nautilus_druid_page_edge_set_title         (NautilusDruidPageEdge *druid_page_edge,
					 const gchar *title)
{
	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	g_free (druid_page_edge->title);
	druid_page_edge->title = g_strdup (title);
	gnome_canvas_item_set (druid_page_edge->_priv->title_item,
			       "text", druid_page_edge->title,
			       NULL);
}
void
nautilus_druid_page_edge_set_logo          (NautilusDruidPageEdge *druid_page_edge,
					 GdkPixbuf *logo_image)
{
	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	if (druid_page_edge->logo_image)
		gdk_pixbuf_unref (druid_page_edge->logo_image);

	druid_page_edge->logo_image = logo_image;
	gdk_pixbuf_ref (logo_image);
	gnome_canvas_item_set (druid_page_edge->_priv->logo_item,
			       "pixbuf", druid_page_edge->logo_image, NULL);
}
void
nautilus_druid_page_edge_set_watermark     (NautilusDruidPageEdge *druid_page_edge,
					 GdkPixbuf *watermark)
{
	g_return_if_fail (druid_page_edge != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EDGE (druid_page_edge));

	if (druid_page_edge->watermark_image)
		gdk_pixbuf_unref (druid_page_edge->watermark_image);

	druid_page_edge->watermark_image = watermark;
	gdk_pixbuf_ref (watermark);
	gnome_canvas_item_set (druid_page_edge->_priv->watermark_item,
			       "pixbuf", druid_page_edge->watermark_image, NULL);
}
