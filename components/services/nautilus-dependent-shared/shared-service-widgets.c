/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ramiro Estrugo	<ramiro@eazel.com>
 *          J Shane Culpepper	<pepper@eazel.com>
 *
 */

#include <config.h>

#include "shared-service-widgets.h"

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-theme.h>

#include <stdio.h>

/* private shared helper routine to create an image widget from a pixbuf */
static GtkWidget*
create_image_widget_from_pixbuf (GdkPixbuf *icon_pixbuf, const char *tile_icon_name)
{
	GtkWidget *image_widget;

	g_return_val_if_fail (icon_pixbuf || tile_icon_name, NULL);

	image_widget = nautilus_image_new ();

	if (icon_pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image_widget), icon_pixbuf);
	}

	if (tile_icon_name != NULL) {
		char *tile_icon_path;

		tile_icon_path = nautilus_pixmap_file (tile_icon_name);
		
		if (tile_icon_path != NULL) {
			GdkPixbuf *tile_icon_pixbuf;
			tile_icon_pixbuf = gdk_pixbuf_new_from_file (tile_icon_path);
			g_free (tile_icon_path);
			
			if (tile_icon_pixbuf != NULL) {
				nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (image_widget), tile_icon_pixbuf);
				gdk_pixbuf_unref (tile_icon_pixbuf);
			}
			else {
				g_warning ("Could not find the requested tile_icon: %s", tile_icon_path);
			}
		}
	}

	return image_widget;
}

/* create and return an image widget using a themed nautilus icon name and a tiled background */
GtkWidget*
create_image_widget (const char *icon_name, const char *tile_icon_name)
{
	GtkWidget *image_widget;
	GdkPixbuf *pixbuf;
	
	g_return_val_if_fail (icon_name || tile_icon_name, NULL);

	pixbuf = NULL;
	if (icon_name != NULL) {
		char *icon_path;
		
		icon_path = nautilus_theme_get_image_path (icon_name);
		
		if (icon_path != NULL) {
			pixbuf = gdk_pixbuf_new_from_file (icon_path);
			g_free (icon_path);
			
			if (pixbuf == NULL) {
				g_warning ("Could not find the requested icon: %s", icon_path);
			}
		}
	}
	
	/* create the image widget then release the pixbuf*/
	image_widget = create_image_widget_from_pixbuf (pixbuf, tile_icon_name);
	if (pixbuf != NULL) {
		gdk_pixbuf_unref (pixbuf);
	}
	return image_widget;
}

/* create and return an image widget from a uri and a tiled background.
   It also pins the image to the specified dimensions */
GtkWidget*
create_image_widget_from_uri (const char *uri, const char *tile_icon_name,
			      int max_width, int max_height)
{
	GtkWidget *image_widget;
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	
	g_return_val_if_fail (uri || tile_icon_name, NULL);

	/* as an optimization, it can be a local file.  If it doesn't start with http://,
	   just pass it on to create_image_widget */
	if (!nautilus_istr_has_prefix (uri, "http://")) {
		create_image_widget (uri, tile_icon_name);
	}
	
	/* load the image - synchronously, at least at first */
	pixbuf = nautilus_gdk_pixbuf_load (uri);
	
	/* pin the image to the specified dimensions if necessary */
	if (pixbuf && max_width > 0 && max_height > 0) {
		scaled_pixbuf = nautilus_gdk_pixbuf_scale_down_to_fit (pixbuf, max_width, max_height);
		gdk_pixbuf_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}
		
	/* create the image widget then release the pixbuf*/
	image_widget = create_image_widget_from_pixbuf (pixbuf, tile_icon_name);
	if (pixbuf != NULL) {
		gdk_pixbuf_unref (pixbuf);
	}
	
	return image_widget;
}

/* create a label widget with anti-aliased text and a tiled image background */
GtkWidget*
create_label_widget (const char		*text,
		     guint		font_size,
		     const char		*tile_icon_name,
		     guint		xpad,
		     guint		ypad,
		     gint		horizontal_offset,
		     gint		vertical_offset)
{
	GtkWidget		*label;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (font_size > 0, NULL);

	label = nautilus_label_new (text);

        nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "bold", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), font_size);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);

	if (tile_icon_name != NULL) {
		char *tile_icon_path;

		tile_icon_path = nautilus_pixmap_file (tile_icon_name);
		
		if (tile_icon_path != NULL) {
			GdkPixbuf *tile_icon_pixbuf;
			tile_icon_pixbuf = gdk_pixbuf_new_from_file (tile_icon_path);
			g_free (tile_icon_path);
			
			if (tile_icon_pixbuf != NULL) {
				nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_icon_pixbuf);
				gdk_pixbuf_unref (tile_icon_pixbuf);
			}
			else {
				g_warning ("Could not find the requested tile_icon: %s", tile_icon_path);
			}
		}
	}

	gtk_misc_set_padding (GTK_MISC (label), xpad, ypad);

	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (label), vertical_offset);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (label), horizontal_offset);

	return label;
}

/* utility routine to create the standard services title bar */

GtkWidget*
create_services_title_widget (const char *title_text)
{
	GtkWidget		*title_hbox;
	GtkWidget		*logo_image;
	GtkWidget		*filler_image;
	GtkWidget		*label;

	g_return_val_if_fail (title_text != NULL, NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	logo_image = create_image_widget ("eazel-services-logo.png", NULL);

	filler_image = create_image_widget (NULL, "eazel-services-logo-tile.png");

	label = create_label_widget (title_text, 20, "eazel-services-logo-tile.png", 10, 0, 0, -4);

        nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "bold", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);

	gtk_widget_show (logo_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (title_hbox), logo_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), label, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to create the top half of the summary title */

GtkWidget*
create_summary_service_title_top_widget (const char *login_status_text)
{
	GtkWidget		*title_hbox;
	GtkWidget		*logo_image;
	GtkWidget		*filler_image;
	GtkWidget		*label;

	g_return_val_if_fail (login_status_text != NULL, NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	logo_image = create_image_widget ("service-summary-logo-top.png", NULL);

	filler_image = create_image_widget (NULL, "service-summary-large-teal-section.png");

	label = create_label_widget (login_status_text, 20, "service-summary-large-teal-section.png", 10, 0, 0, -1);

        nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "bold", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);

	gtk_widget_show (logo_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (title_hbox), logo_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), label, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to create the bottom half of the summary title */
GtkWidget*
create_summary_service_title_bottom_widget (const char *section_title)
{
	GtkWidget		*title_hbox;
	GtkWidget		*logo_image;
	GtkWidget		*filler_image;
	GtkWidget		*label;

	g_return_val_if_fail (section_title != NULL, NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	logo_image = create_image_widget ("service-summary-logo-bottom.png", NULL);

	filler_image = create_image_widget (NULL, "service-summary-large-grey-section.png");

	label = create_label_widget (section_title, 20, "service-summary-large-grey-section.png", 10, 0, 0, -1);

        nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "bold", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), NAUTILUS_RGB_COLOR_WHITE);

	gtk_widget_show (logo_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (title_hbox), logo_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), label, FALSE, FALSE, 0);

	return title_hbox;

}


/* utility routine to create a section header */

GtkWidget*
create_services_header_widget (const char	*left_text,
			       const char	*right_text) 
{
	GtkWidget	*title_hbox;
	GtkWidget	*left_label;
	GtkWidget	*right_label;
	GtkWidget	*filler_image;

	g_assert (left_text != NULL);
	g_assert (right_text != NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	left_label = create_label_widget (left_text, 12, "eazel-services-logo-tile.png", 10, 0, 0, -6);

	filler_image = create_image_widget (NULL, "eazel-services-logo-tile.png");

	right_label = create_label_widget (right_text, 12, "eazel-services-logo-tile.png", 10, 0, 0, -6);

	gtk_widget_show (left_label);
	gtk_widget_show (filler_image);
	gtk_widget_show (right_label);

	gtk_box_pack_start (GTK_BOX (title_hbox), left_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), right_label, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to create a large grey section header */

GtkWidget*
create_summary_service_large_grey_header_widget (const char	*right_text)
{
	GtkWidget	*title_hbox;
	GtkWidget	*right_label;
	GtkWidget	*filler_image;

	g_assert (right_text != NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	filler_image = create_image_widget (NULL, "service-summary-large-grey-section.png");

	right_label = create_label_widget (right_text, 18, "service-summary-large-grey-section.png", 10, 0, 0, -1);

	gtk_widget_show (filler_image);
	gtk_widget_show (right_label);

	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), right_label, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to create a small grey section header */

GtkWidget*
create_summary_service_small_grey_header_widget (const char	*left_text)
{
	GtkWidget	*title_hbox;
	GtkWidget	*left_label;
	GtkWidget	*filler_image;

	g_assert (left_text != NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	left_label = create_label_widget (left_text, 12, "service-summary-short-grey-section.png", 10, 0, 0, -1);

	filler_image = create_image_widget (NULL, "service-summary-short-grey-section.png");

	gtk_widget_show (left_label);
	gtk_widget_show (filler_image);

	gtk_box_pack_start (GTK_BOX (title_hbox), left_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);

	return title_hbox;

}

/* utility routine to set the text color */

void
set_widget_foreground_color (GtkWidget	*widget, 
			     const char *color_spec) 
{
	nautilus_gtk_widget_set_foreground_color (widget, color_spec);
}

/* utility routine to show an error message */

void
show_feedback (GtkWidget	*widget, 
	       char		*error_message) 
{
	nautilus_label_set_text (NAUTILUS_LABEL (widget), error_message);
	gtk_widget_show (widget);
}

