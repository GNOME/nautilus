/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-header.c - A header widget for services views.

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
#include "eazel-services-header.h"
#include "eazel-services-constants.h"

#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-theme.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

struct _EazelServicesHeaderDetails
{
	GtkWidget *text;
};

/* GtkObjectClass methods */
static void eazel_services_header_initialize_class (EazelServicesHeaderClass *klass);
static void eazel_services_header_initialize       (EazelServicesHeader      *header);
static void header_destroy                         (GtkObject                *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (EazelServicesHeader, eazel_services_header, GTK_TYPE_HBOX)

/* EazelServicesHeaderClass methods */
static void
eazel_services_header_initialize_class (EazelServicesHeaderClass *header_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (header_class);

	/* GtkObjectClass */
	object_class->destroy = header_destroy;
}

static void
eazel_services_header_initialize (EazelServicesHeader *item)
{
	item->details = g_new0 (EazelServicesHeaderDetails, 1);
}

/* GtkObjectClass methods */
static void
header_destroy (GtkObject *object)
{
	EazelServicesHeader *header;
	
	g_return_if_fail (EAZEL_SERVICES_IS_HEADER (object));
	
	header = EAZEL_SERVICES_HEADER (object);

	g_free (header->details);
	
	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Private stuff */
static GdkPixbuf *
pixbuf_new_from_name (const char *name)
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

static GtkWidget *
label_new (const char *text,
	   guint font_size,
	   guint drop_shadow_offset,
	   guint vertical_offset,
	   guint horizontal_offset,
	   gint xpadding,
	   gint ypadding,
	   guint32 background_color,
	   guint32 drop_shadow_color,
	   guint32 text_color,
	   const char *tile_name)
{
	GtkWidget *label;

	g_return_val_if_fail (text != NULL, NULL);
	
 	label = nautilus_label_new (text);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), font_size);
	nautilus_label_set_drop_shadow_offset (NAUTILUS_LABEL (label), drop_shadow_offset);
	nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET (label), NAUTILUS_BACKGROUND_SOLID);
	nautilus_buffered_widget_set_background_color (NAUTILUS_BUFFERED_WIDGET (label), background_color);
	nautilus_label_set_drop_shadow_color (NAUTILUS_LABEL (label), drop_shadow_color);
	nautilus_label_set_text_color (NAUTILUS_LABEL (label), background_color);

	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (label), vertical_offset);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (label), horizontal_offset);

	gtk_misc_set_padding (GTK_MISC (label), xpadding, ypadding);

	if (tile_name != NULL) {
		GdkPixbuf *tile_pixbuf;

		tile_pixbuf = pixbuf_new_from_name (tile_name);

		if (tile_pixbuf != NULL) {
			nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_pixbuf);
		}

		nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);
	}
	
	return label;
}

static GtkWidget *
image_new (GdkPixbuf *pixbuf, GdkPixbuf *tile_pixbuf, guint32 background_color)
{
	GtkWidget *image;

	g_return_val_if_fail (pixbuf || tile_pixbuf, NULL);

	image = nautilus_image_new ();

	nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET (image), NAUTILUS_BACKGROUND_SOLID);
	nautilus_buffered_widget_set_background_color (NAUTILUS_BUFFERED_WIDGET (image), background_color);

	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
	}
	
	if (tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (image), tile_pixbuf);
	}

	return image;
}

static GtkWidget *
image_new_from_name (const char *icon_name, const char *tile_name, guint32 background_color)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *tile_pixbuf = NULL;

	g_return_val_if_fail (icon_name || tile_name, NULL);

	if (icon_name) {
		pixbuf = pixbuf_new_from_name (icon_name);
	}

	if (tile_name) {
		tile_pixbuf = pixbuf_new_from_name (tile_name);
	}

	g_return_val_if_fail (pixbuf || tile_pixbuf, NULL);

	image = image_new (pixbuf, tile_pixbuf, background_color);

	nautilus_gdk_pixbuf_unref_if_not_null (pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (tile_pixbuf);

	return image;
}

/* EazelServicesHeader public methods */
GtkWidget *
eazel_services_header_new (const char *text)
{
	EazelServicesHeader *header;
 	GtkWidget *middle;
 	GtkWidget *logo;

	header = EAZEL_SERVICES_HEADER (gtk_widget_new (eazel_services_header_get_type (), NULL));
	
 	header->details->text = label_new (text ? text : "foo",
					   18,
					   1, 
					   4,
					   0,
					   10,
					   0,
					   EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
					   EAZEL_SERVICES_DROP_SHADOW_COLOR_RGBA,
					   EAZEL_SERVICES_TEXT_COLOR_RGBA,
					   EAZEL_SERVICES_LOGO_LEFT_SIDE_REPEAT_ICON);

	middle = image_new_from_name (NULL, EAZEL_SERVICES_LOGO_LEFT_SIDE_REPEAT_ICON, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);

	logo = image_new_from_name (EAZEL_SERVICES_LOGO_RIGHT_SIDE_ICON, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);

	gtk_box_pack_start (GTK_BOX (header), header->details->text, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (header), middle, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (header), logo, FALSE, FALSE, 0);

	gtk_widget_show (header->details->text);
	gtk_widget_show (middle);
	gtk_widget_show (logo);

	return GTK_WIDGET (header);
}

void
eazel_services_header_set_text (EazelServicesHeader *header,
				const char *text)
{
	g_return_if_fail (EAZEL_SERVICES_IS_HEADER (header));
	g_return_if_fail (text != NULL);

	nautilus_label_set_text (NAUTILUS_LABEL (header->details->text), text);
}
