/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-extensions.c - Extensions to Nautilus and gtk widget.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eazel-services-extensions.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-clickable-image.h>

#include <time.h>

GdkPixbuf *
eazel_services_pixbuf_new (const char *name)
{
	char *path;
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (name != NULL, NULL);

	path = nautilus_theme_get_image_path (name);
	
	g_return_val_if_fail (path != NULL, NULL);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	return pixbuf;
}

GtkWidget *
eazel_services_image_new (const char *icon_name,
			  const char *tile_name,
			  guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	if (icon_name) {
		pixbuf = eazel_services_pixbuf_new (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	image = nautilus_image_new_solid (pixbuf, 0.5, 0.5, 0, 0, background_color, tile_pixbuf);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

GtkWidget *
eazel_services_image_new_clickable (const char *icon_name,
				    const char *tile_name,
				    guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	if (icon_name) {
		pixbuf = eazel_services_pixbuf_new (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	image = nautilus_clickable_image_new_solid (NULL,
						    pixbuf,
						    0,
						    0,
						    0,
						    0.5,
						    0.5,
						    0,
						    0,
						    background_color,
						    tile_pixbuf);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

GtkWidget *
eazel_services_image_new_from_uri (const char *uri,
				   const char *tile_name,
				   guint32 background_color,
				   int max_width,
				   int max_height)
{
	GtkWidget *image = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;

	g_return_val_if_fail (uri != NULL, NULL);

	/* as an optimization, it can be a local file.  If it doesn't start with http://,
	   just pass it on to create_image_widget */
	if (!nautilus_istr_has_prefix (uri, "http://")) {
		return eazel_services_image_new (uri, tile_name, background_color);
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
	image = eazel_services_image_new (NULL, tile_name, background_color);

	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
	}

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);

	return image;
}

GtkWidget *
eazel_services_label_new (const char *text,
			  guint drop_shadow_offset,
			  float xalign,
			  float yalign,
			  gint xpadding,
			  gint ypadding,
			  guint32 text_color,
			  guint32 background_color,
			  const char *tile_name,
			  gint num_larger_sizes,
			  gboolean bold)
{
 	GtkWidget *label;
	GdkPixbuf *tile_pixbuf = NULL;

	if (tile_name != NULL) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	label = nautilus_label_new_solid (text,
					  drop_shadow_offset,
					  EAZEL_SERVICES_DROP_SHADOW_COLOR_RGB,
					  text_color,
					  xalign,
					  yalign,
					  xpadding,
					  ypadding,
					  background_color,
					  tile_pixbuf);


	if (num_larger_sizes < 0) {
		nautilus_label_make_smaller (NAUTILUS_LABEL (label), ABS (num_larger_sizes));
	} else if (num_larger_sizes > 0) {
		nautilus_label_make_larger (NAUTILUS_LABEL (label), num_larger_sizes);
	}

	if (bold) {
		nautilus_label_make_bold (NAUTILUS_LABEL (label));
	}
	
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return label;
}

GtkWidget *
eazel_services_label_new_clickable (const char *text,
				    guint drop_shadow_offset,
				    float xalign,
				    float yalign,
				    gint xpadding,
				    gint ypadding,
				    guint32 text_color,
				    guint32 background_color,
				    const char *tile_name,
				    gint num_larger_sizes,
				    gboolean bold)
{
 	GtkWidget *label;
	GdkPixbuf *tile_pixbuf = NULL;

	if (tile_name != NULL) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	label = nautilus_clickable_image_new_solid (text,
						    NULL,
						    drop_shadow_offset,
						    EAZEL_SERVICES_DROP_SHADOW_COLOR_RGB,
						    text_color,
						    xalign,
						    yalign,
						    xpadding,
						    ypadding,
						    background_color,
						    tile_pixbuf);

	if (num_larger_sizes < 0) {
		nautilus_labeled_image_make_smaller (NAUTILUS_LABELED_IMAGE (label), ABS (num_larger_sizes));
	} else if (num_larger_sizes > 0) {
		nautilus_labeled_image_make_larger (NAUTILUS_LABELED_IMAGE (label), num_larger_sizes);
	}

	if (bold) {
		nautilus_labeled_image_make_bold (NAUTILUS_LABELED_IMAGE (label));
	}
	
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return label;
}

char *
eazel_services_get_current_date_string (void)
{
	time_t my_time;
	struct tm *my_localtime;

	my_time = time (NULL);

	if (my_time == -1) {
		return g_strdup (_("Unknown Date"));
	}

	my_localtime = localtime (&my_time);

        return nautilus_strdup_strftime (_("%A, %B %d"), my_localtime);
}
