/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eazel-services-footer.c - A footer widget for services views.

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
#include "eazel-services-footer.h"
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

struct _EazelServicesFooterDetails
{
	GList *items;
};

/* GtkObjectClass methods */
static void eazel_services_footer_initialize_class (EazelServicesFooterClass *klass);
static void eazel_services_footer_initialize       (EazelServicesFooter      *footer);
static void footer_destroy                         (GtkObject                *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (EazelServicesFooter, eazel_services_footer, GTK_TYPE_HBOX)

/* EazelServicesFooterClass methods */
static void
eazel_services_footer_initialize_class (EazelServicesFooterClass *footer_class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (footer_class);

	/* GtkObjectClass */
	object_class->destroy = footer_destroy;
}

static void
eazel_services_footer_initialize (EazelServicesFooter *item)
{
	item->details = g_new0 (EazelServicesFooterDetails, 1);
}

/* GtkObjectClass methods */
static void
footer_destroy (GtkObject *object)
{
	EazelServicesFooter *footer;
	
	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (object));
	
	footer = EAZEL_SERVICES_FOOTER (object);

	g_free (footer->details);
	
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

typedef struct
{
	GtkWidget *buffered_widget;
	char *tile_name;
	char *prelight_tile_name;
	GdkPixbuf *tile_pixbuf;
	GdkPixbuf *prelight_tile_pixbuf;
} PrelightLabelData;

static gint
label_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightLabelData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightLabelData *) client_data;

	g_return_val_if_fail (data->prelight_tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	//g_print ("%s(%p)\n", __FUNCTION__, data);

	if (data->prelight_tile_pixbuf == NULL) {
		data->prelight_tile_pixbuf = pixbuf_new_from_name (data->prelight_tile_name);
	}

	if (data->prelight_tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->prelight_tile_pixbuf);
	}

	return TRUE;
}

static gint
label_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightLabelData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightLabelData *) client_data;

	g_return_val_if_fail (data->tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	if (data->tile_pixbuf == NULL) {
		data->tile_pixbuf = pixbuf_new_from_name (data->tile_name);
	}
	
	if (data->tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->tile_pixbuf);
	}

	return TRUE;
}

static gint
label_button_press_event (GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer client_data)
{
	const char *uri;
	
	g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	uri = (const char *) client_data;

	g_print ("%s(%s)\n", __FUNCTION__, uri);

	return TRUE;
}

static void
label_free_data (GtkWidget *event_box,
		 gpointer client_data)
{
	PrelightLabelData *data;

	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	data = (PrelightLabelData *) client_data;

	g_free (data->tile_name);
	g_free (data->prelight_tile_name);
	nautilus_gdk_pixbuf_unref_if_not_null (data->tile_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (data->prelight_tile_pixbuf);
	g_free (data);
}

static void
label_free_uri (GtkWidget *event_box,
		gpointer client_data)
{
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	g_free (client_data);
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

static void
buffered_widget_add_prelighting (GtkWidget *buffered_widget,
				 GtkWidget *event_box,
				 const char *tile_name,
				 const char *prelight_tile_name)
{
	PrelightLabelData *data;

	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (tile_name != NULL);
	g_return_if_fail (prelight_tile_name != NULL);

	data = g_new0 (PrelightLabelData, 1);
	
	data->buffered_widget = buffered_widget;
	data->tile_name = g_strdup (tile_name);
	data->prelight_tile_name = g_strdup (prelight_tile_name);
	
	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (label_enter_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (label_leave_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (label_free_data), data);
}

static GtkWidget *
footer_item_new (const char *text, const char *uri, gboolean has_left_bumper, gboolean has_right_bumper)
{
	GtkWidget *hbox;
	GtkWidget *event_box;
  	GtkWidget *label;
	char *uri_copy;

	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri[0] != '\0', NULL);

	event_box = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (event_box), hbox);

	if (has_left_bumper) {
		GtkWidget *left;
		left = image_new_from_name (EAZEL_SERVICES_NORMAL_LEFT_BUMPER, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
		buffered_widget_add_prelighting (left, event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
		gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
		gtk_widget_show (left);
	}

 	label = label_new (text,
			   13,
			   1,
			   2,
			   0,
			   5,
			   0,
			   EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
			   EAZEL_SERVICES_DROP_SHADOW_COLOR_RGBA,
			   EAZEL_SERVICES_TEXT_COLOR_RGBA,
			   EAZEL_SERVICES_NORMAL_FILL);
	
	gtk_widget_show (label);

	uri_copy = g_strdup (uri);

	gtk_signal_connect (GTK_OBJECT (event_box), "button_press_event", GTK_SIGNAL_FUNC (label_button_press_event), uri_copy);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (label_free_uri), uri_copy);

	buffered_widget_add_prelighting (label, event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	if (has_right_bumper) {
		GtkWidget *right;
		right = image_new_from_name (EAZEL_SERVICES_NORMAL_RIGHT_BUMPER, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
		buffered_widget_add_prelighting (right, event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
		gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);
		gtk_widget_show (right);
	}

	gtk_widget_show (hbox);

	return event_box;
}

static GtkWidget *
footer_remainder_new (void)
{
	GtkWidget *hbox;
	GtkWidget *left;
	GtkWidget *fill;
	GtkWidget *right;

	hbox = gtk_hbox_new (FALSE, 0);

	left = image_new_from_name (EAZEL_SERVICES_REMAINDER_LEFT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
	fill = image_new_from_name (NULL, EAZEL_SERVICES_REMAINDER_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
	right = image_new_from_name (EAZEL_SERVICES_REMAINDER_RIGHT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);

	gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), fill, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);

	gtk_widget_show (left);
	gtk_widget_show (fill);
	gtk_widget_show (right);

	return hbox;
}

/* EazelServicesFooter public methods */
GtkWidget *
eazel_services_footer_new (void)
{
	EazelServicesFooter *footer;
	
	footer = EAZEL_SERVICES_FOOTER (gtk_widget_new (eazel_services_footer_get_type (), NULL));

	return GTK_WIDGET (footer);
}

void
eazel_services_footer_update (EazelServicesFooter *footer,
			      const char *items[],
			      const char *uris[],
			      guint num_items)
{
	GtkWidget *remainder;
	GtkWidget *date;
	guint i;

	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (footer));
	g_return_if_fail (items != NULL);
	g_return_if_fail (uris != NULL);
	g_return_if_fail (num_items > 0);

	for (i = 0; i < num_items; i++) {
		GtkWidget *item;
		
		item = footer_item_new (items[i], uris[i], i > 0, i < (num_items - 1));
		gtk_box_pack_start (GTK_BOX (footer), item, FALSE, FALSE, 0);
		gtk_widget_show (item);
	}

	remainder = footer_remainder_new ();
	gtk_box_pack_start (GTK_BOX (footer), remainder, TRUE, TRUE, 0);

 	date = label_new ("Friday, October 13th",
			  13,
			  1,
			  2,
			  0,
			  8,
			  0,
			  EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
			  EAZEL_SERVICES_DROP_SHADOW_COLOR_RGBA,
			  EAZEL_SERVICES_TEXT_COLOR_RGBA,
			  EAZEL_SERVICES_NORMAL_FILL);

	gtk_widget_show (remainder);
	gtk_widget_show (date);

	gtk_box_pack_start (GTK_BOX (footer), date, FALSE, FALSE, 0);
}
