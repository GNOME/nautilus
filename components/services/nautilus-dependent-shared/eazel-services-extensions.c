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

#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-theme.h>

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
eazel_services_image_new (const char *icon_name, const char *tile_name, guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	g_return_val_if_fail (icon_name || tile_name, NULL);

	if (icon_name) {
		pixbuf = eazel_services_pixbuf_new (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = eazel_services_pixbuf_new (tile_name);
	}

	g_return_val_if_fail (pixbuf || tile_pixbuf, NULL);

	image = nautilus_image_new_loaded (pixbuf, 0, 0, 0, 0, background_color, tile_pixbuf);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

GtkWidget *
eazel_services_label_new (const char *text,
			  guint font_size,
			  const char *weight,
			  gint xpadding,
			  gint ypadding,
			  guint vertical_offset,
			  guint horizontal_offset,
			  guint32 background_color,
			  const char *tile_name)
{
 	GtkWidget *label;
	GdkPixbuf *text_tile;

	text_tile = eazel_services_pixbuf_new (tile_name);
	
	label = nautilus_label_new_loaded (text,
					   EAZEL_SERVICES_FONT_FAMILY,
					   weight,
					   font_size,
					   1, /* drop_shadow_offset */
					   EAZEL_SERVICES_DROP_SHADOW_COLOR_RGBA,
					   EAZEL_SERVICES_TEXT_COLOR_RGBA,
					   xpadding,
					   ypadding,
					   vertical_offset,
					   horizontal_offset,
					   EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
					   text_tile);

	nautilus_gdk_pixbuf_unref_if_not_null (text_tile);

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
