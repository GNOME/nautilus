/* gnome-druid-page-standard.c
 * Copyright (C) 1999  Red Hat, Inc.
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

#include "nautilus-druid-page-standard.h"

#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/libgnomeui.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include "nautilus-druid.h"
#include <libgnome/gnome-i18n.h>

struct _NautilusDruidPageStandardPrivate
{
	GtkWidget *canvas;
	GtkWidget *side_bar;
	GnomeCanvasItem *logoframe_item;
	GnomeCanvasItem *logo_item;
	GnomeCanvasItem *title_item;
	GnomeCanvasItem *background_item;
	GtkWidget *bottom_bar;
	GtkWidget *right_bar;
};


static void nautilus_druid_page_standard_init	    (NautilusDruidPageStandard          *druid_page_standard);
static void nautilus_druid_page_standard_class_init    (NautilusDruidPageStandardClass     *klass);
static void nautilus_druid_page_standard_destroy 	    (GtkObject                       *object);
static void nautilus_druid_page_standard_construct     (NautilusDruidPageStandard          *druid_page_standard);
static void nautilus_druid_page_standard_configure_size(NautilusDruidPageStandard          *druid_page_standard,
						     gint                             width,
						     gint                             height);
static void nautilus_druid_page_standard_size_allocate (GtkWidget                       *widget,
						     GtkAllocation                   *allocation);
static void nautilus_druid_page_standard_prepare       (NautilusDruidPage                  *page,
						     GtkWidget                       *druid,
						     gpointer                        *data);


static NautilusDruidPageClass *parent_class = NULL;

#define LOGO_WIDTH 50.0
#define DRUID_PAGE_WIDTH 516
#define GDK_COLOR_TO_RGBA(color) GNOME_CANVAS_COLOR (color.red/256, color.green/256, color.blue/256)

GtkType
nautilus_druid_page_standard_get_type (void)
{
  static GtkType druid_page_standard_type = 0;

  if (!druid_page_standard_type)
    {
      static const GtkTypeInfo druid_page_standard_info =
      {
        "NautilusDruidPageStandard",
        sizeof (NautilusDruidPageStandard),
        sizeof (NautilusDruidPageStandardClass),
        (GtkClassInitFunc) nautilus_druid_page_standard_class_init,
        (GtkObjectInitFunc) nautilus_druid_page_standard_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      druid_page_standard_type = gtk_type_unique (nautilus_druid_page_get_type (), &druid_page_standard_info);
    }

  return druid_page_standard_type;
}

static void
nautilus_druid_page_standard_class_init (NautilusDruidPageStandardClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = nautilus_druid_page_standard_destroy;
	widget_class = (GtkWidgetClass*) klass;
	widget_class->size_allocate = nautilus_druid_page_standard_size_allocate;

	parent_class = gtk_type_class (nautilus_druid_page_get_type ());

}
static void
nautilus_druid_page_standard_init (NautilusDruidPageStandard *druid_page_standard)
{
	GtkRcStyle *rc_style;
	GtkWidget *vbox;
	GtkWidget *hbox;

	druid_page_standard->_priv = g_new0(NautilusDruidPageStandardPrivate, 1);

	/* initialize the color values */
	druid_page_standard->background_color.red = 6400; /* midnight blue */
	druid_page_standard->background_color.green = 6400;
	druid_page_standard->background_color.blue = 28672;
	druid_page_standard->logo_background_color.red = 65280; /* white */
	druid_page_standard->logo_background_color.green = 65280;
	druid_page_standard->logo_background_color.blue = 65280;
	druid_page_standard->title_color.red = 65280; /* white */
	druid_page_standard->title_color.green = 65280;
	druid_page_standard->title_color.blue = 65280;

	/* Set up the widgets */
	vbox = gtk_vbox_new (FALSE, 0);
	hbox = gtk_hbox_new (FALSE, 0);
	druid_page_standard->vbox = gtk_vbox_new (FALSE, 0);
	druid_page_standard->_priv->canvas = gnome_canvas_new ();
	druid_page_standard->_priv->side_bar = gtk_drawing_area_new ();
	druid_page_standard->_priv->bottom_bar = gtk_drawing_area_new ();
	druid_page_standard->_priv->right_bar = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (druid_page_standard->_priv->side_bar),
			       15, 10);
	gtk_drawing_area_size (GTK_DRAWING_AREA (druid_page_standard->_priv->bottom_bar),
			       10, 1);
	gtk_drawing_area_size (GTK_DRAWING_AREA (druid_page_standard->_priv->right_bar),
			       1, 10);
	rc_style = gtk_rc_style_new ();
	rc_style->bg[GTK_STATE_NORMAL].red = 6400;
	rc_style->bg[GTK_STATE_NORMAL].green = 6400;
	rc_style->bg[GTK_STATE_NORMAL].blue = 28672;
	rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_BG;
	gtk_rc_style_ref (rc_style);
	gtk_widget_modify_style (druid_page_standard->_priv->side_bar, rc_style);
	gtk_rc_style_ref (rc_style);
	gtk_widget_modify_style (druid_page_standard->_priv->bottom_bar, rc_style);
	gtk_rc_style_ref (rc_style);
	gtk_widget_modify_style (druid_page_standard->_priv->right_bar, rc_style);

	/* FIXME: can I just ref the old style? */
	rc_style = gtk_rc_style_new ();
	rc_style->bg[GTK_STATE_NORMAL].red = 6400;
	rc_style->bg[GTK_STATE_NORMAL].green = 6400;
	rc_style->bg[GTK_STATE_NORMAL].blue = 28672;
	rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_BG;
	gtk_widget_modify_style (druid_page_standard->_priv->canvas, rc_style);
	gtk_box_pack_start (GTK_BOX (vbox), druid_page_standard->_priv->canvas, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), druid_page_standard->_priv->bottom_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), druid_page_standard->_priv->side_bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), druid_page_standard->vbox, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), druid_page_standard->_priv->right_bar, FALSE, FALSE, 0);
	gtk_widget_set_usize (druid_page_standard->_priv->canvas, 508, LOGO_WIDTH + GNOME_PAD * 2);
	gtk_container_set_border_width (GTK_CONTAINER (druid_page_standard), 0);
	gtk_container_add (GTK_CONTAINER (druid_page_standard), vbox);
	gtk_widget_show_all (vbox);
}

static void
nautilus_druid_page_standard_destroy(GtkObject *object)
{
	NautilusDruidPageStandard *druid_page_standard = NAUTILUS_DRUID_PAGE_STANDARD(object);

	g_free(druid_page_standard->_priv);
	druid_page_standard->_priv = NULL;

	if(GTK_OBJECT_CLASS(parent_class)->destroy)
		(* GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

static void
nautilus_druid_page_standard_configure_size (NautilusDruidPageStandard *druid_page_standard, gint width, gint height)
{
	gnome_canvas_item_set (druid_page_standard->_priv->background_item,
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", (gfloat) width,
			       "y2", (gfloat) LOGO_WIDTH + GNOME_PAD * 2,
			       "width_units", 1.0, NULL);
	gnome_canvas_item_set (druid_page_standard->_priv->logoframe_item,
			       "x1", (gfloat) width - LOGO_WIDTH - GNOME_PAD,
			       "y1", (gfloat) GNOME_PAD,
			       "x2", (gfloat) width - GNOME_PAD,
			       "y2", (gfloat) GNOME_PAD + LOGO_WIDTH,
			       "width_units", 1.0, NULL);
	gnome_canvas_item_set (druid_page_standard->_priv->logo_item,
			       "x", (gfloat) width - GNOME_PAD - LOGO_WIDTH,
			       "y", (gfloat) GNOME_PAD,
			       "width", (gfloat) LOGO_WIDTH,
			       "height", (gfloat) LOGO_WIDTH, NULL);
}

static void
nautilus_druid_page_standard_construct (NautilusDruidPageStandard *druid_page_standard)
{
	static guint32 fill_color = 0;

	/* set up the rest of the page */
	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->background_color);
	druid_page_standard->_priv->background_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_standard->_priv->canvas)),
				       gnome_canvas_rect_get_type (),
				       "fill_color_rgba", fill_color,
				       NULL);

	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->logo_background_color);
	druid_page_standard->_priv->logoframe_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_standard->_priv->canvas)),
				       gnome_canvas_rect_get_type (),
				       "fill_color_rgba", fill_color,
				       NULL);

	druid_page_standard->_priv->logo_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_standard->_priv->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x_in_pixels", TRUE, "y_in_pixels", TRUE,
				       NULL);

	if (druid_page_standard->logo_image != NULL) {
		gnome_canvas_item_set (druid_page_standard->_priv->logo_item,
				       "pixbuf", druid_page_standard->logo_image, NULL);
	}

	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->title_color);
	druid_page_standard->_priv->title_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_standard->_priv->canvas)),
				       gnome_canvas_text_get_type (),
				       "text", druid_page_standard->title,
				       "font", _("-adobe-helvetica-bold-r-normal-*-*-180-*-*-p-*-*-*"),
				       "fill_color_rgba", fill_color,
				       NULL);

	gnome_canvas_item_set (druid_page_standard->_priv->title_item,
			       "x", 15.0,
			       "y", (gfloat) GNOME_PAD + LOGO_WIDTH / 2.0,
			       "anchor", GTK_ANCHOR_WEST,
			       NULL);

	nautilus_druid_page_standard_configure_size (druid_page_standard, DRUID_PAGE_WIDTH, GNOME_PAD * 2 + LOGO_WIDTH);
	gtk_signal_connect (GTK_OBJECT (druid_page_standard),
			    "prepare",
			    nautilus_druid_page_standard_prepare,
			    NULL);

}
static void
nautilus_druid_page_standard_prepare (NautilusDruidPage *page,
				   GtkWidget *druid,
				   gpointer *data)
{
	nautilus_druid_set_buttons_sensitive (NAUTILUS_DRUID (druid), TRUE, TRUE, TRUE);
	nautilus_druid_set_show_finish (NAUTILUS_DRUID (druid), FALSE);
	gtk_widget_grab_default (NAUTILUS_DRUID (druid)->next);
}

static void
nautilus_druid_page_standard_size_allocate (GtkWidget *widget,
					 GtkAllocation *allocation)
{
	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (NAUTILUS_DRUID_PAGE_STANDARD (widget)->_priv->canvas),
					0.0, 0.0,
					allocation->width,
					allocation->height);
	nautilus_druid_page_standard_configure_size (NAUTILUS_DRUID_PAGE_STANDARD (widget),
						  allocation->width,
						  allocation->height);
}

GtkWidget *
nautilus_druid_page_standard_new (void)
{
	GtkWidget *retval = GTK_WIDGET (gtk_type_new (nautilus_druid_page_standard_get_type ()));
	NAUTILUS_DRUID_PAGE_STANDARD (retval)->title = g_strdup ("");
	NAUTILUS_DRUID_PAGE_STANDARD (retval)->logo_image = NULL;
	nautilus_druid_page_standard_construct (NAUTILUS_DRUID_PAGE_STANDARD (retval));
	return retval;
}
GtkWidget *
nautilus_druid_page_standard_new_with_vals (const gchar *title, GdkPixbuf *logo)
{
	GtkWidget *retval = GTK_WIDGET (gtk_type_new (nautilus_druid_page_standard_get_type ()));
	NAUTILUS_DRUID_PAGE_STANDARD (retval)->title = g_strdup (title);
	NAUTILUS_DRUID_PAGE_STANDARD (retval)->logo_image = logo;
	nautilus_druid_page_standard_construct (NAUTILUS_DRUID_PAGE_STANDARD (retval));
	return retval;
}
void
nautilus_druid_page_standard_set_bg_color      (NautilusDruidPageStandard *druid_page_standard,
					     GdkColor *color)
{
	guint32 fill_color;

	g_return_if_fail (druid_page_standard != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_STANDARD (druid_page_standard));

	druid_page_standard->background_color.red = color->red;
	druid_page_standard->background_color.green = color->green;
	druid_page_standard->background_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->background_color);

	gnome_canvas_item_set (druid_page_standard->_priv->background_item,
			       "fill_color_rgba", fill_color,
			       NULL);

	if (GTK_WIDGET_REALIZED (druid_page_standard)) {

		GtkStyle *style;

		style = gtk_style_copy (gtk_widget_get_style (druid_page_standard->_priv->side_bar));
		style->bg[GTK_STATE_NORMAL].red = color->red;
		style->bg[GTK_STATE_NORMAL].green = color->green;
		style->bg[GTK_STATE_NORMAL].blue = color->blue;
		gtk_widget_set_style (druid_page_standard->_priv->side_bar, style);
		gtk_widget_set_style (druid_page_standard->_priv->bottom_bar, style);
		gtk_widget_set_style (druid_page_standard->_priv->right_bar, style);
	} else {
		GtkRcStyle *rc_style;

		rc_style = gtk_rc_style_new ();
		rc_style->bg[GTK_STATE_NORMAL].red = color->red;
		rc_style->bg[GTK_STATE_NORMAL].green = color->green;
		rc_style->bg[GTK_STATE_NORMAL].blue = color->blue;
		rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_BG;
		gtk_rc_style_ref (rc_style);
		gtk_widget_modify_style (druid_page_standard->_priv->side_bar, rc_style);
		gtk_rc_style_ref (rc_style);
		gtk_widget_modify_style (druid_page_standard->_priv->bottom_bar, rc_style);
		gtk_rc_style_ref (rc_style);
		gtk_widget_modify_style (druid_page_standard->_priv->right_bar, rc_style);
	}
}

void
nautilus_druid_page_standard_set_logo_bg_color (NautilusDruidPageStandard *druid_page_standard,
					  GdkColor *color)
{
	guint32 fill_color;

	g_return_if_fail (druid_page_standard != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_STANDARD (druid_page_standard));

	druid_page_standard->logo_background_color.red = color->red;
	druid_page_standard->logo_background_color.green = color->green;
	druid_page_standard->logo_background_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->logo_background_color);
	gnome_canvas_item_set (druid_page_standard->_priv->logoframe_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}
void
nautilus_druid_page_standard_set_title_color   (NautilusDruidPageStandard *druid_page_standard,
					  GdkColor *color)
{
	guint32 fill_color;

	g_return_if_fail (druid_page_standard != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_STANDARD (druid_page_standard));

	druid_page_standard->title_color.red = color->red;
	druid_page_standard->title_color.green = color->green;
	druid_page_standard->title_color.blue = color->blue;

	fill_color = GDK_COLOR_TO_RGBA (druid_page_standard->title_color);
	gnome_canvas_item_set (druid_page_standard->_priv->title_item,
			       "fill_color_rgba", fill_color,
			       NULL);
}

void
nautilus_druid_page_standard_set_title         (NautilusDruidPageStandard *druid_page_standard,
					  const gchar *title)
{
	g_return_if_fail (druid_page_standard != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_STANDARD (druid_page_standard));

	g_free (druid_page_standard->title);
	druid_page_standard->title = g_strdup (title);
	gnome_canvas_item_set (druid_page_standard->_priv->title_item,
			       "text", druid_page_standard->title,
			       NULL);
}
void
nautilus_druid_page_standard_set_logo          (NautilusDruidPageStandard *druid_page_standard,
					     GdkPixbuf*logo_image)
{
	g_return_if_fail (druid_page_standard != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_STANDARD (druid_page_standard));

	if (druid_page_standard->logo_image)
		gdk_pixbuf_unref (druid_page_standard->logo_image);

	druid_page_standard->logo_image = logo_image;
	gdk_pixbuf_ref (logo_image);
	gnome_canvas_item_set (druid_page_standard->_priv->logo_item,
			       "pixbuf", druid_page_standard->logo_image, NULL);
}

