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
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <stdio.h>

/* Ramiro's very cool generic image widget */
GtkWidget*
create_image_widget (const char			*icon_name,
		     const char			*background_color_spec,
		     NautilusImagePlacementType	placement) {

	char		*path;
	GtkWidget	*image;
	GdkPixbuf	*pixbuf;
	guint32		background_rgb;

	g_return_val_if_fail (icon_name != NULL, NULL);
	g_return_val_if_fail (background_color_spec != NULL, NULL);

	image = nautilus_image_new ();

	path = nautilus_pixmap_file (icon_name);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
		gdk_pixbuf_unref (pixbuf);
	}
	else {
		g_warning ("Could not find the requested icon.");
	}

	nautilus_image_set_background_type (NAUTILUS_IMAGE (image),
					    NAUTILUS_IMAGE_BACKGROUND_SOLID);

	background_rgb = nautilus_parse_rgb_with_white_default (background_color_spec);

	nautilus_image_set_background_color (NAUTILUS_IMAGE (image),
					     background_rgb);

	nautilus_image_set_placement_type (NAUTILUS_IMAGE (image), placement);

	return image;

}

/* utility routine to create the standard services title bar */

GtkWidget*
create_services_title_widget (const char	*title_text) {

	GtkWidget       *title_hbox;
	GtkWidget       *logo_image;
	GtkWidget       *filler_image;
	GtkWidget       *text_image;
	GdkFont         *font;

	g_assert (title_text != NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	logo_image = create_image_widget ("eazel-services-logo.png",
					  SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					  NAUTILUS_IMAGE_PLACEMENT_CENTER);

	filler_image = create_image_widget ("eazel-services-logo-tile.png",
					    SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					    NAUTILUS_IMAGE_PLACEMENT_TILE);

	text_image = create_image_widget ("eazel-services-logo-tile.png",
					  SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					  NAUTILUS_IMAGE_PLACEMENT_TILE);

	font = nautilus_font_factory_get_font_by_family ("helvetica", 20);

	nautilus_image_set_label_text (NAUTILUS_IMAGE (text_image), title_text);
	nautilus_image_set_label_font (NAUTILUS_IMAGE (text_image), font);
	nautilus_image_set_extra_width (NAUTILUS_IMAGE (text_image), 8);
	nautilus_image_set_right_offset (NAUTILUS_IMAGE (text_image), 8);
	nautilus_image_set_top_offset (NAUTILUS_IMAGE (text_image), 3);

	gdk_font_unref (font);

	gtk_widget_show (logo_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (text_image);

	gtk_box_pack_start (GTK_BOX (title_hbox), logo_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), text_image, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to create a section header */

GtkWidget*
create_services_header_widget	(const char	*left_text,
				 const char	*right_text) {

	GtkWidget	*title_hbox;
	GtkWidget	*left_image;
	GtkWidget	*right_image;
	GtkWidget	*filler_image;
	GdkFont		*font;

	g_assert (left_text != NULL);
	g_assert (right_text != NULL);

	title_hbox = gtk_hbox_new (FALSE, 0);

	left_image = create_image_widget ("eazel-services-logo-tile.png",
					  SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					  NAUTILUS_IMAGE_PLACEMENT_TILE);

	filler_image = create_image_widget ("eazel-services-logo-tile.png",
					    SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					    NAUTILUS_IMAGE_PLACEMENT_TILE);

	right_image = create_image_widget ("eazel-services-logo-tile.png",
					   SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					   NAUTILUS_IMAGE_PLACEMENT_TILE);

	font = nautilus_font_factory_get_font_by_family ("helvetica", 18);

	nautilus_image_set_label_text (NAUTILUS_IMAGE (left_image), left_text);
	nautilus_image_set_label_font (NAUTILUS_IMAGE (left_image), font);

	nautilus_image_set_extra_width (NAUTILUS_IMAGE (left_image), 8);
	nautilus_image_set_left_offset (NAUTILUS_IMAGE (left_image), 8);
	nautilus_image_set_top_offset (NAUTILUS_IMAGE (left_image), 1);

	nautilus_image_set_label_text (NAUTILUS_IMAGE (right_image), right_text);
	nautilus_image_set_label_font (NAUTILUS_IMAGE (right_image), font);

	nautilus_image_set_extra_width (NAUTILUS_IMAGE (right_image), 8);
	nautilus_image_set_right_offset (NAUTILUS_IMAGE (right_image), 8);
	nautilus_image_set_top_offset (NAUTILUS_IMAGE (right_image), 1);

	gdk_font_unref (font);

	gtk_widget_show (left_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (right_image);

	gtk_box_pack_start (GTK_BOX (title_hbox), left_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (title_hbox), right_image, FALSE, FALSE, 0);

	return title_hbox;

}

/* utility routine to set the text color */

void
set_widget_foreground_color (GtkWidget	*widget, const char	*color_spec) {

	GtkStyle	*style;
	GdkColor	color;

	style = gtk_widget_get_style (widget);

	/* Make a copy of the style. */
	style = gtk_style_copy (style);

	nautilus_gdk_color_parse_with_white_default (color_spec, &color);
	style->fg[GTK_STATE_NORMAL] = color;
	style->base[GTK_STATE_NORMAL] = color;
	style->fg[GTK_STATE_ACTIVE] = color;
	style->base[GTK_STATE_ACTIVE] = color;

	/* Put the style in the widget. */
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);

}

/* utility routine to show an error message */

void
show_feedback (GtkWidget	*widget, char	*error_message) {

	gtk_label_set_text (GTK_LABEL (widget), error_message);
	gtk_widget_show (widget);

}

