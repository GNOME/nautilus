/* gnome-druid-page-eazel.c
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2000  Eazel, Inc.
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

#include "nautilus-druid-page-eazel.h"

#include <eel/eel-gtk-macros.h>

#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomeui/gnome-druid.h>
#include <libgnomeui/gnome-druid-page.h>

#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include "nautilus-druid.h"
#include <libgnome/gnome-i18n.h>

#include "nautilus-file-utilities.h"

struct NautilusDruidPageEazelDetails
{
	GnomeCanvasItem  *background_item;
	GnomeCanvasItem  *background_image_item;
	GnomeCanvasItem  *topbar_image_item;
	int               topbar_image_width;
	GnomeCanvasItem  *topbar_image_stretch_item;
	GnomeCanvasItem  *title_item;
	GtkWidget	 *title_label;
	guint		  title_label_signal_id;
	GnomeCanvasItem  *text_item;
	GnomeCanvasItem  *sidebar_image_item;
	GnomeCanvasItem  *title_image_item;
	GnomeCanvasItem  *widget_item;
};

static void nautilus_druid_page_eazel_initialize   	(NautilusDruidPageEazel		*druid_page_eazel);
static void nautilus_druid_page_eazel_initialize_class	(NautilusDruidPageEazelClass	*klass);
static void nautilus_druid_page_eazel_destroy       (GtkObject *object);
static void nautilus_druid_page_eazel_finalize      (GtkObject *object);
static void nautilus_druid_page_eazel_construct     (NautilusDruidPageEazel *druid_page_eazel);
static void nautilus_druid_page_eazel_configure_size(NautilusDruidPageEazel *druid_page_eazel,
						     gint width,
						     gint height);
static void nautilus_druid_page_eazel_size_allocate (GtkWidget     *widget,
						     GtkAllocation *allocation);
static void nautilus_druid_page_eazel_size_request  (GtkWidget      *widget,
						     GtkRequisition *requisition);
static void nautilus_druid_page_eazel_prepare       (GnomeDruidPage *page,
						     GtkWidget      *druid,
						     gpointer 	    *data);

#define TITLE_X 60.0
#define TITLE_Y 60.0
#define CONTENT_PADDING 15.0
#define DEFAULT_CONTENT_X 34.0
#define DRUID_PAGE_MIN_HEIGHT 322
#define DRUID_PAGE_MIN_WIDTH 516
#define DRUID_PAGE_BORDER 24

EEL_DEFINE_CLASS_BOILERPLATE (NautilusDruidPageEazel, nautilus_druid_page_eazel, GNOME_TYPE_DRUID_PAGE)

static void
nautilus_druid_page_eazel_initialize_class (NautilusDruidPageEazelClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;

	parent_class = gtk_type_class (gnome_druid_page_get_type ());

	object_class->destroy = nautilus_druid_page_eazel_destroy;
	object_class->finalize = nautilus_druid_page_eazel_finalize;

	widget_class->size_allocate = nautilus_druid_page_eazel_size_allocate;
	widget_class->size_request = nautilus_druid_page_eazel_size_request;
}

static void
nautilus_druid_page_eazel_initialize (NautilusDruidPageEazel *druid_page_eazel)
{
	druid_page_eazel->details = g_new0(NautilusDruidPageEazelDetails, 1);

	/* Set up the canvas */
	gtk_container_set_border_width (GTK_CONTAINER (druid_page_eazel), 0);
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	druid_page_eazel->canvas = gnome_canvas_new_aa ();
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
	gtk_widget_show (druid_page_eazel->canvas);
	gtk_container_add (GTK_CONTAINER (druid_page_eazel), druid_page_eazel->canvas);
	nautilus_druid_page_eazel_configure_size (druid_page_eazel,
						  DRUID_PAGE_MIN_WIDTH,
						  DRUID_PAGE_MIN_HEIGHT);
}

static void
nautilus_druid_page_eazel_destroy(GtkObject *object)
{
	NautilusDruidPageEazel *druid_page_eazel =
		NAUTILUS_DRUID_PAGE_EAZEL(object);

	druid_page_eazel->canvas = NULL;
	druid_page_eazel->widget = NULL;

	g_free (druid_page_eazel->title);
	druid_page_eazel->title = NULL;
	g_free (druid_page_eazel->text);
	druid_page_eazel->text = NULL;

	if (druid_page_eazel->title_image != NULL)
		gdk_pixbuf_unref (druid_page_eazel->title_image);
	druid_page_eazel->title_image = NULL;
	if (druid_page_eazel->sidebar_image != NULL)
		gdk_pixbuf_unref (druid_page_eazel->sidebar_image);
	druid_page_eazel->sidebar_image = NULL;

	if (druid_page_eazel->widget != NULL)
		gtk_widget_unref (druid_page_eazel->widget);
	druid_page_eazel->widget = NULL;

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_druid_page_eazel_finalize (GtkObject *object)
{
	NautilusDruidPageEazel *druid_page_eazel =
		NAUTILUS_DRUID_PAGE_EAZEL(object);

	g_free(druid_page_eazel->details);
	druid_page_eazel->details = NULL;

	/* Chain finalize */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, finalize, (object));
}

static void
get_content_xy (NautilusDruidPageEazel *druid_page_eazel,
		double *content_x, double *content_y)
{
	double title_height;

	if (druid_page_eazel->sidebar_image) {
		*content_x = gdk_pixbuf_get_width (druid_page_eazel->sidebar_image);
	} else {
		*content_x = DEFAULT_CONTENT_X;
	}

	if (druid_page_eazel->title_image) {
		*content_y = gdk_pixbuf_get_height (druid_page_eazel->title_image) + TITLE_Y + CONTENT_PADDING;
	} else {
		*content_y = TITLE_Y;
	}

	title_height = 0.0;
	if (druid_page_eazel->title && druid_page_eazel->title[0] != '\0') {
		gtk_object_get (GTK_OBJECT (druid_page_eazel->details->title_item),
				"height", &title_height,
				NULL);
		title_height += CONTENT_PADDING;
	}

	if (*content_y < title_height + TITLE_Y) {
		*content_y = title_height + TITLE_Y;
	}
}


static void
nautilus_druid_page_eazel_configure_size (NautilusDruidPageEazel *druid_page_eazel, gint width, gint height)
{
	double content_x;
	double content_y;

	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	get_content_xy (druid_page_eazel, &content_x, &content_y);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (druid_page_eazel->canvas),
					0.0, 0.0, width, height);

	if (druid_page_eazel->details->background_item != NULL) {
		gnome_canvas_item_set (druid_page_eazel->details->background_item,
				       "x1", 0.0,
				       "y1", 0.0,
				       "x2", (gfloat) width,
				       "y2", (gfloat) height,
				       NULL);
	}

	if (druid_page_eazel->details->topbar_image_stretch_item != NULL) {
		gnome_canvas_item_set (druid_page_eazel->details->topbar_image_stretch_item,
				       "width", (double) (width - druid_page_eazel->details->topbar_image_width),
				       "width_set", TRUE,
				       NULL);
	}

	if (druid_page_eazel->details->widget_item != NULL) {
		gnome_canvas_item_set (druid_page_eazel->details->widget_item,
				       "x", content_x,
				       "y", content_y,
				       NULL);

		/* Event boxes can handle not having the size set, and
		 * not doing so allows them to scale with their child
		 * widget. On the other hand, some other widgets require
		 * the size to be set, otherwise they won't appear on the
		 * canvas!
		 */
		if (druid_page_eazel->widget != NULL && !GTK_IS_EVENT_BOX (druid_page_eazel->widget)) {
			gnome_canvas_item_set (druid_page_eazel->details->widget_item,
					       "width", (gfloat) width - content_x,
					       "height", (gfloat) height - content_y,
					       NULL);
		}
	}

	if (druid_page_eazel->details->text_item != NULL) {
		gnome_canvas_item_set (druid_page_eazel->details->text_item,
				       "x", content_x,
				       "y", content_y,
				       NULL);
	}
}

static void
set_image (GnomeCanvasItem *item, const char *file,
	   int *width, int *height)
{
	char *fullname;

	if (width != NULL)
		*width = 0;
	if (height != NULL)
		*height = 0;

	fullname = nautilus_pixmap_file (file);
	if (fullname != NULL) {
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (fullname);
		if (pixbuf != NULL) {
			if (width != NULL)
				*width = gdk_pixbuf_get_width (pixbuf);
			if (height != NULL)
				*height = gdk_pixbuf_get_height (pixbuf);
			gnome_canvas_item_set (item,
					       "pixbuf", pixbuf,
					       NULL);
			gdk_pixbuf_unref (pixbuf);
		}
		g_free (fullname);
	}
}

static void
title_label_size_allocated (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	NautilusDruidPageEazel *druid_page_eazel = NAUTILUS_DRUID_PAGE_EAZEL (data);
	gnome_canvas_item_set (druid_page_eazel->details->title_item,
			       "size_pixels", TRUE,
			       "height", (double) allocation->height,
			       "width",  (double) allocation->width,
			       NULL);
}


static void
nautilus_druid_page_eazel_construct (NautilusDruidPageEazel *druid_page_eazel)
{
	druid_page_eazel->details->background_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_rect_get_type (),
				       "x1", 0.0,
				       "y1", 0.0,
				       "fill_color", "white",
				       NULL);

	druid_page_eazel->details->background_image_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       "x_in_pixels", TRUE,
				       "y_in_pixels", TRUE,
				       NULL);
	if (druid_page_eazel->background_image)
		gnome_canvas_item_set (druid_page_eazel->details->background_image_item,
				       "pixbuf", druid_page_eazel->background_image,
				       NULL);

	druid_page_eazel->details->sidebar_image_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       "x_in_pixels", TRUE,
				       "y_in_pixels", TRUE,
				       NULL);
	if (druid_page_eazel->sidebar_image)
		gnome_canvas_item_set (druid_page_eazel->details->sidebar_image_item,
				       "pixbuf", druid_page_eazel->sidebar_image,
				       NULL);

	druid_page_eazel->details->topbar_image_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       "x_in_pixels", TRUE,
				       "y_in_pixels", TRUE,
				       NULL);
	set_image (druid_page_eazel->details->topbar_image_item,
		   "druid_header.png",
		   &druid_page_eazel->details->topbar_image_width,
		   NULL);

	druid_page_eazel->details->topbar_image_stretch_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x", (double)druid_page_eazel->details->topbar_image_width,
				       "y", 0.0,
				       "x_in_pixels", TRUE,
				       "y_in_pixels", TRUE,
				       NULL);
	set_image (druid_page_eazel->details->topbar_image_stretch_item,
		   "druid_header_stretch.png", NULL, NULL);

	druid_page_eazel->details->title_image_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_pixbuf_get_type (),
				       "x", TITLE_X,
				       "y", TITLE_Y,
				       "x_in_pixels", TRUE,
				       "y_in_pixels", TRUE,
				       NULL);
	if (druid_page_eazel->title_image)
		gnome_canvas_item_set (druid_page_eazel->details->title_image_item,
				       "pixbuf", druid_page_eazel->title_image,
				       NULL);

	druid_page_eazel->details->title_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_widget_get_type (),
				       "x", TITLE_X,
				       "y", TITLE_Y,
				       NULL);
						   
	druid_page_eazel->details->text_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_text_get_type (),
				       "text", druid_page_eazel->text,
				       "fill_color", "black",
				       /* Note to localizers: this font is used for text items in Druid pages */
				       "fontset", _("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-*-*,*-r-*"),
				       "anchor", GTK_ANCHOR_NW,
				       NULL);

	nautilus_druid_page_eazel_configure_size (druid_page_eazel, DRUID_PAGE_MIN_WIDTH, DRUID_PAGE_MIN_HEIGHT);
	gtk_signal_connect (GTK_OBJECT (druid_page_eazel),
			    "prepare",
			    nautilus_druid_page_eazel_prepare,
			    NULL);
}

static void
nautilus_druid_page_eazel_prepare (GnomeDruidPage *page,
				   GtkWidget *druid,
				   gpointer *data)
{
	switch (NAUTILUS_DRUID_PAGE_EAZEL (page)->position) {
	case NAUTILUS_DRUID_PAGE_EAZEL_START:
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), FALSE, TRUE, TRUE);
		gnome_druid_set_show_finish (GNOME_DRUID (druid), FALSE);
		gtk_widget_grab_default (GNOME_DRUID (druid)->next);
		break;
	case NAUTILUS_DRUID_PAGE_EAZEL_FINISH:
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), TRUE, FALSE, TRUE);
		gnome_druid_set_show_finish (GNOME_DRUID (druid), TRUE);
		gtk_widget_grab_default (GNOME_DRUID (druid)->finish);
		break;
	case NAUTILUS_DRUID_PAGE_EAZEL_OTHER:
		gnome_druid_set_buttons_sensitive (GNOME_DRUID (druid), TRUE, TRUE, TRUE);
		gnome_druid_set_show_finish (GNOME_DRUID (druid), FALSE);
	default:
		break;
	}
}


static void
nautilus_druid_page_eazel_size_allocate(GtkWidget               *widget,
					GtkAllocation           *allocation)
{
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate,
			      (widget, allocation));

	gnome_canvas_set_scroll_region (GNOME_CANVAS (NAUTILUS_DRUID_PAGE_EAZEL (widget)->canvas),
					0.0, 0.0,
					allocation->width,
					allocation->height);

	nautilus_druid_page_eazel_configure_size (NAUTILUS_DRUID_PAGE_EAZEL (widget),
						  allocation->width,
						  allocation->height);
}

static void
nautilus_druid_page_eazel_size_request(GtkWidget           *widget,
				       GtkRequisition      *requisition)
{
	NautilusDruidPageEazel *druid_page_eazel;

	druid_page_eazel = NAUTILUS_DRUID_PAGE_EAZEL (widget);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request,
			      (widget, requisition));

	if (druid_page_eazel->widget) {
		GtkRequisition child_requisition;
		double x, y;

		g_assert (druid_page_eazel->details->widget_item != NULL);

		get_content_xy (druid_page_eazel, &x, &y);

		gtk_widget_get_child_requisition (druid_page_eazel->widget,
						  &child_requisition);

		if (child_requisition.width + x > requisition->width) {
			requisition->width = child_requisition.width + x + DRUID_PAGE_BORDER;
		}
		if (child_requisition.height + y > requisition->height) {
			requisition->height = child_requisition.height + y + DRUID_PAGE_BORDER;
		}
	}
}


/**
 * nautilus_druid_page_eazel_new:
 *
 * Creates a new NautilusDruidPageEazel widget.
 *
 * Return value: Pointer to new NautilusDruidPageEazel
 **/
/* Public functions */
GtkWidget *
nautilus_druid_page_eazel_new (NautilusDruidPageEazelPosition position)
{
	NautilusDruidPageEazel *page;

	page = NAUTILUS_DRUID_PAGE_EAZEL (gtk_widget_new (nautilus_druid_page_eazel_get_type (), NULL));

	page->position = position;
	page->title = g_strdup ("");
	page->text = g_strdup ("");
	page->title_image = NULL;
	page->sidebar_image = NULL;
	page->background_image = NULL;
	nautilus_druid_page_eazel_construct (page);

	return GTK_WIDGET (page);
}
/**
 * nautilus_druid_page_eazel_new_with_vals:
 * @title: The title.
 * @text: The introduction text.
 * @logo: The logo in the upper right corner.
 * @watermark: The watermark on the left.
 *
 * This will create a new GNOME Druid Eazel page, with the values
 * given.  It is acceptable for any of them to be %NULL.
 *
 * Return value: GtkWidget pointer to new NautilusDruidPageEazel.
 **/
GtkWidget *
nautilus_druid_page_eazel_new_with_vals (NautilusDruidPageEazelPosition position,
					 const gchar *title,
					 const gchar* text,
					 GdkPixbuf *title_image,
					 GdkPixbuf *sidebar_image,
					 GdkPixbuf *background_image)
{
	NautilusDruidPageEazel *page;

	page = NAUTILUS_DRUID_PAGE_EAZEL (gtk_widget_new (nautilus_druid_page_eazel_get_type (), NULL));

	page->position = position;
	page->title = g_strdup (title ? title : "");
	page->text = g_strdup (text ? text : "");

	if (title_image)
		gdk_pixbuf_ref (title_image);
	page->title_image = title_image;

	if (sidebar_image)
		gdk_pixbuf_ref (sidebar_image);
	page->sidebar_image = sidebar_image;

	if (background_image)
		gdk_pixbuf_ref (background_image);
	page->background_image = background_image;

	nautilus_druid_page_eazel_construct (page);

	return GTK_WIDGET (page);
}

void
nautilus_druid_page_eazel_set_text (NautilusDruidPageEazel *druid_page_eazel,
				    const gchar *text)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	g_free (druid_page_eazel->text);
	druid_page_eazel->text = g_strdup (text ? text : "");
	gnome_canvas_item_set (druid_page_eazel->details->text_item,
			       "text", druid_page_eazel->text,
			       NULL);
}

static GtkWidget *
make_title_label (const char *text)
{
	GtkWidget *label;
        GtkStyle *new_style;
	GdkFont *font;

	label = gtk_label_new (text);

	/* Note to localizers: this font is used for page titles in Druid pages */
	font = gdk_fontset_load (_("-adobe-helvetica-bold-r-normal-*-*-180-*-*-p-*-*-*,*-r-*"));
	if (font != NULL) {
		new_style = gtk_style_copy (gtk_widget_get_style (label));
		gdk_font_unref (new_style->font);
		new_style->font = font;
		gtk_widget_set_style (label, new_style);
		gtk_style_unref (new_style);
	}

	return label;
}

void
nautilus_druid_page_eazel_set_title (NautilusDruidPageEazel *druid_page_eazel,
				     const gchar *title)
{
	GtkWidget *label;

	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	g_free (druid_page_eazel->title);
	druid_page_eazel->title = g_strdup (title ? title : "");

	if (druid_page_eazel->details->title_label == NULL) {
		label = make_title_label (druid_page_eazel->title);
		nautilus_druid_page_eazel_set_title_label (druid_page_eazel, GTK_LABEL (label));
	} else {
		gtk_label_set_text (GTK_LABEL (druid_page_eazel->details->title_label), druid_page_eazel->title);
	}
}

void
nautilus_druid_page_eazel_set_title_label (NautilusDruidPageEazel *druid_page_eazel,
					   GtkLabel *label)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));
	g_return_if_fail (GTK_IS_LABEL (label));

	if (druid_page_eazel->details->title_label != NULL) {
		gtk_signal_disconnect (GTK_OBJECT (druid_page_eazel->details->title_label),
				       druid_page_eazel->details->title_label_signal_id);
	}

	gtk_widget_show (GTK_WIDGET (label));
	gnome_canvas_item_set (druid_page_eazel->details->title_item,
			       "widget", label,
			       NULL);
	druid_page_eazel->details->title_label = GTK_WIDGET (label);
	druid_page_eazel->details->title_label_signal_id =
		gtk_signal_connect (GTK_OBJECT (label), "size_allocate",
				    title_label_size_allocated,
				    druid_page_eazel);

	if (druid_page_eazel->title != NULL) {
		g_free (druid_page_eazel->title);
	}
	druid_page_eazel->title = g_strdup (label->label);
}

void
nautilus_druid_page_eazel_set_title_image (NautilusDruidPageEazel *druid_page_eazel,
					   GdkPixbuf *title_image)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	if (druid_page_eazel->title_image)
		gdk_pixbuf_unref (druid_page_eazel->title_image);

	druid_page_eazel->title_image = title_image;
	if (title_image != NULL)
		gdk_pixbuf_ref (title_image);
	gnome_canvas_item_set (druid_page_eazel->details->title_image_item,
			       "pixbuf", druid_page_eazel->title_image, NULL);
}

void
nautilus_druid_page_eazel_set_sidebar_image (NautilusDruidPageEazel *druid_page_eazel,
					     GdkPixbuf *sidebar_image)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	if (druid_page_eazel->sidebar_image)
		gdk_pixbuf_unref (druid_page_eazel->sidebar_image);

	druid_page_eazel->sidebar_image = sidebar_image;
	if (sidebar_image != NULL)
		gdk_pixbuf_ref (sidebar_image);
	gnome_canvas_item_set (druid_page_eazel->details->sidebar_image_item,
			       "pixbuf", druid_page_eazel->sidebar_image, NULL);
}

void
nautilus_druid_page_eazel_set_background_image (NautilusDruidPageEazel *druid_page_eazel,
						GdkPixbuf *background_image)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	if (druid_page_eazel->background_image)
		gdk_pixbuf_unref (druid_page_eazel->background_image);

	druid_page_eazel->background_image = background_image;
	if (background_image != NULL)
		gdk_pixbuf_ref (background_image);
	gnome_canvas_item_set (druid_page_eazel->details->background_image_item,
			       "pixbuf", druid_page_eazel->background_image, NULL);
}

void
nautilus_druid_page_eazel_put_widget (NautilusDruidPageEazel *druid_page_eazel,
				      GtkWidget *widget)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	if (druid_page_eazel->details->widget_item != NULL) {
		gtk_object_destroy (GTK_OBJECT (druid_page_eazel->details->widget_item));
	}
	if (druid_page_eazel->widget != NULL) {
		gtk_widget_unref (druid_page_eazel->widget);
	}

	druid_page_eazel->widget = widget;
	if (widget != NULL)
		gtk_widget_ref (widget);

	druid_page_eazel->details->widget_item =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (druid_page_eazel->canvas)),
				       gnome_canvas_widget_get_type (),
				       "widget", widget,
				       NULL);

	gtk_widget_queue_resize (GTK_WIDGET (druid_page_eazel));
}
