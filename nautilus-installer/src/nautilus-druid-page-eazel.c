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

#include "nautilus-gtk-macros.h"

#include "gnome-druid.h"
#include "gnome-druid-page.h"

#include "nautilus-druid.h"

#include <libtrilobite/trilobite-i18n.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

struct NautilusDruidPageEazelDetails
{
	GtkWidget *fixed;

        GdkPixbuf *background;
        GtkWidget *widget;
  
        int widget_x, widget_y;
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

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusDruidPageEazel, nautilus_druid_page_eazel, GNOME_TYPE_DRUID_PAGE)

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

	druid_page_eazel->details->fixed = gtk_fixed_new ();
	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
	gtk_widget_show (druid_page_eazel->details->fixed);
	gtk_container_add (GTK_CONTAINER (druid_page_eazel), druid_page_eazel->details->fixed);
	nautilus_druid_page_eazel_configure_size (druid_page_eazel,
						  DRUID_PAGE_MIN_WIDTH,
						  DRUID_PAGE_MIN_HEIGHT);
}

static void
nautilus_druid_page_eazel_destroy(GtkObject *object)
{
	NautilusDruidPageEazel *druid_page_eazel =
		NAUTILUS_DRUID_PAGE_EAZEL(object);


	if (druid_page_eazel->details->background != NULL) {
	        gdk_pixbuf_unref (druid_page_eazel->details->background);
	}

	druid_page_eazel->details->background = NULL;

	if (druid_page_eazel->details->widget != NULL) {
		gtk_widget_unref (druid_page_eazel->details->widget);
	}
	druid_page_eazel->details->widget = NULL;

	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_druid_page_eazel_finalize (GtkObject *object)
{
	NautilusDruidPageEazel *druid_page_eazel =
		NAUTILUS_DRUID_PAGE_EAZEL(object);

	g_free(druid_page_eazel->details);
	druid_page_eazel->details = NULL;

	/* Chain finalize */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, finalize, (object));
}

static void
get_content_xy (NautilusDruidPageEazel *druid_page_eazel,
		double *content_x, double *content_y)
{
	*content_x = DEFAULT_CONTENT_X;
	*content_y = TITLE_Y;
}


static void
nautilus_druid_page_eazel_configure_size (NautilusDruidPageEazel *druid_page_eazel, 
					  gint width, 
					  gint height)
{
	double content_x;
	double content_y;

	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	get_content_xy (druid_page_eazel, &content_x, &content_y);

	druid_page_eazel->details->widget_x = content_x;
	druid_page_eazel->details->widget_y = content_y;
	
	if (druid_page_eazel->details->widget != NULL) {
	         gtk_fixed_move (GTK_FIXED (druid_page_eazel->details->fixed),
				 druid_page_eazel->details->widget,
				 druid_page_eazel->details->widget_x,
				 druid_page_eazel->details->widget_y);
	}
}

#include <stdio.h>

static void
realize_handler (GtkWidget *fixed, 
		 NautilusDruidPageEazel *druid_page_eazel)
{
        GdkPixmap *pixmap;
        GdkBitmap *mask;
	
	gdk_pixbuf_render_pixmap_and_mask (druid_page_eazel->details->background,
					   &pixmap, &mask,
					   127);
	
	gdk_window_set_back_pixmap (druid_page_eazel->details->fixed->window,
				    pixmap,
				    FALSE);
	
	if (mask != NULL) {
	  gdk_bitmap_unref (mask);
	}
	
	gdk_pixmap_unref (pixmap);
	
	gdk_window_clear (druid_page_eazel->details->fixed->window);
}

static void
nautilus_druid_page_eazel_construct (NautilusDruidPageEazel *druid_page_eazel)
{
        GdkPixmap *pixmap;
        GdkBitmap *mask;

	if (druid_page_eazel->details->background != NULL) {
	  if (GTK_WIDGET_REALIZED (druid_page_eazel->details->fixed)) {

	    gdk_pixbuf_render_pixmap_and_mask (druid_page_eazel->details->background,
					       &pixmap, &mask,
					       127);
	    
	    gdk_window_set_back_pixmap (druid_page_eazel->details->fixed->window,
					pixmap,
					FALSE);
	    if (mask != NULL) {
	      gdk_bitmap_unref (mask);
	    }
	    gdk_pixmap_unref (pixmap);
	    
	    gdk_window_clear (druid_page_eazel->details->fixed->window);
	  } else {
	    gtk_signal_connect (GTK_OBJECT (druid_page_eazel->details->fixed),
				"realize",
				(GtkSignalFunc) realize_handler,
				druid_page_eazel);
	  }
	}


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
nautilus_druid_page_eazel_size_allocate (GtkWidget               *widget,
					 GtkAllocation           *allocation)
{
	NAUTILUS_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate,
			      (widget, allocation));
}

static void
nautilus_druid_page_eazel_size_request(GtkWidget           *widget,
				       GtkRequisition      *requisition)
{
	NautilusDruidPageEazel *druid_page_eazel;

	druid_page_eazel = NAUTILUS_DRUID_PAGE_EAZEL (widget);

	NAUTILUS_CALL_PARENT (GTK_WIDGET_CLASS, size_request,
			      (widget, requisition));

	if (druid_page_eazel->details->widget) {
		GtkRequisition child_requisition;
		double x, y;

		get_content_xy (druid_page_eazel, &x, &y);

		gtk_widget_get_child_requisition (druid_page_eazel->details->widget,
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

	if (background_image)
		gdk_pixbuf_ref (background_image);
	page->details->background = background_image;

	nautilus_druid_page_eazel_construct (page);

	return GTK_WIDGET (page);
}



void
nautilus_druid_page_eazel_put_widget (NautilusDruidPageEazel *druid_page_eazel,
				      GtkWidget *widget)
{
	g_return_if_fail (druid_page_eazel != NULL);
	g_return_if_fail (NAUTILUS_IS_DRUID_PAGE_EAZEL (druid_page_eazel));

	if (druid_page_eazel->details->widget != NULL) {
                gtk_container_remove (GTK_CONTAINER (druid_page_eazel->details->fixed),
				      druid_page_eazel->details->widget);
		gtk_widget_unref (druid_page_eazel->details->widget);
	}

	druid_page_eazel->details->widget = widget;
	if (widget != NULL) {
		gtk_widget_ref (widget);
		gtk_fixed_put (GTK_FIXED (druid_page_eazel->details->fixed),
			       druid_page_eazel->details->widget,
			       druid_page_eazel->details->widget_x,
			       druid_page_eazel->details->widget_y);
	}

	gtk_widget_queue_resize (GTK_WIDGET (druid_page_eazel));
}
