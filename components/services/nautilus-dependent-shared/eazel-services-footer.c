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
#include "eazel-services-extensions.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-theme.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

struct _EazelServicesFooterDetails
{
	GtkWidget *date;
};

/* Signals */
typedef enum
{
	ITEM_CLICKED,
	LAST_SIGNAL
} FooterSignal;

/* Signals */
static guint footer_signals[LAST_SIGNAL] = { 0 };

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
	
 	/* Signals */
	footer_signals[ITEM_CLICKED] = gtk_signal_new ("item_clicked",
						       GTK_RUN_LAST,
						       object_class->type,
						       0,
						       gtk_marshal_NONE__INT,
						       GTK_TYPE_NONE, 
						       1,
						       GTK_TYPE_INT);
	
 	gtk_object_class_add_signals (object_class, footer_signals, LAST_SIGNAL);
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

	g_print ("%s()\n", __FUNCTION__);
	
	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Private stuff */

typedef struct
{
	GtkWidget *buffered_widget;
	char *tile_name;
	char *prelight_tile_name;
	GdkPixbuf *tile_pixbuf;
	GdkPixbuf *prelight_tile_pixbuf;
} PrelightData;

static gint
footer_item_enter_event (GtkWidget *event_box,
			 GdkEventCrossing *event,
			 gpointer client_data)
{
	PrelightData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightData *) client_data;

	g_return_val_if_fail (data->prelight_tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	if (data->prelight_tile_pixbuf == NULL) {
		data->prelight_tile_pixbuf = eazel_services_pixbuf_new (data->prelight_tile_name);
	}

	if (data->prelight_tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->prelight_tile_pixbuf);
	}

	return TRUE;
}

static gint
footer_item_leave_event (GtkWidget *event_box,
			 GdkEventCrossing *event,
			 gpointer client_data)
{
	PrelightData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (PrelightData *) client_data;

	g_return_val_if_fail (data->tile_name, TRUE);
	g_return_val_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (data->buffered_widget), TRUE);

	if (data->tile_pixbuf == NULL) {
		data->tile_pixbuf = eazel_services_pixbuf_new (data->tile_name);
	}
	
	if (data->tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (data->buffered_widget),
							  data->tile_pixbuf);
	}

	return TRUE;
}

typedef struct
{
	EazelServicesFooter *footer;
	int index;
} ButtonPressData;

static gint
footer_item_button_press_event (GtkWidget *widget,
				GdkEventButton *event,
				gpointer client_data)
{
	ButtonPressData	*data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (ButtonPressData *) client_data;

	g_return_val_if_fail (EAZEL_SERVICES_IS_FOOTER (data->footer), TRUE);
	
	gtk_signal_emit (GTK_OBJECT (data->footer), footer_signals[ITEM_CLICKED], data->index);

	return TRUE;
}

static void
footer_item_prelight_data_free_callback (GtkWidget *event_box,
					 gpointer client_data)
{
	PrelightData *data;

	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	data = (PrelightData *) client_data;

	g_free (data->tile_name);
	g_free (data->prelight_tile_name);
	nautilus_gdk_pixbuf_unref_if_not_null (data->tile_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (data->prelight_tile_pixbuf);
	g_free (data);
}

static void
free_data_callback (GtkWidget *event_box,
		    gpointer client_data)
{
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	g_free (client_data);
}

static void
buffered_widget_add_prelighting (GtkWidget *buffered_widget,
				 GtkWidget *event_box,
				 const char *tile_name,
				 const char *prelight_tile_name)
{
	PrelightData *data;

	g_return_if_fail (NAUTILUS_IS_BUFFERED_WIDGET (buffered_widget));
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (tile_name != NULL);
	g_return_if_fail (prelight_tile_name != NULL);

	data = g_new0 (PrelightData, 1);
	
	data->buffered_widget = buffered_widget;
	data->tile_name = g_strdup (tile_name);
	data->prelight_tile_name = g_strdup (prelight_tile_name);
	
	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (footer_item_enter_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (footer_item_leave_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (footer_item_prelight_data_free_callback), data);
}

static GtkWidget *
footer_item_new (EazelServicesFooter *footer,
		 const char *text,
		 int index,
		 gboolean has_left_bumper,
		 gboolean has_right_bumper)
{
	GtkWidget *hbox;
	GtkWidget *event_box;
  	GtkWidget *label;
	ButtonPressData *data;

	g_return_val_if_fail (EAZEL_SERVICES_IS_FOOTER (footer), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);

	event_box = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (event_box), hbox);

	if (has_left_bumper) {
		GtkWidget *left;
		left = eazel_services_image_new (EAZEL_SERVICES_NORMAL_LEFT_BUMPER, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
		buffered_widget_add_prelighting (left, event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
		gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
		gtk_widget_show (left);
	}

	label = eazel_services_label_new (text,
					  EAZEL_SERVICES_FOOTER_FONT_WEIGHT,
					  EAZEL_SERVICES_FOOTER_FONT_SIZE,
					  5, 0,
					  2, 0,
					  EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
					  EAZEL_SERVICES_NORMAL_FILL);
	
	gtk_widget_show (label);

	data = g_new (ButtonPressData, 1);
	data->index = index;
	data->footer = footer;

	gtk_signal_connect (GTK_OBJECT (event_box), "button_press_event", GTK_SIGNAL_FUNC (footer_item_button_press_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (free_data_callback), data);

	buffered_widget_add_prelighting (label, event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	if (has_right_bumper) {
		GtkWidget *right;
		right = eazel_services_image_new (EAZEL_SERVICES_NORMAL_RIGHT_BUMPER, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
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

	left = eazel_services_image_new (EAZEL_SERVICES_REMAINDER_LEFT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
	fill = eazel_services_image_new (NULL, EAZEL_SERVICES_REMAINDER_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);
	right = eazel_services_image_new (EAZEL_SERVICES_REMAINDER_RIGHT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGBA);

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
			      guint num_items)
{
	GtkWidget *remainder;
	guint i;
	char *date_string;

	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (footer));
	g_return_if_fail (items != NULL);
	g_return_if_fail (num_items > 0);

	for (i = 0; i < num_items; i++) {
		GtkWidget *item;
		
		item = footer_item_new (footer, items[i], i, i > 0, i < (num_items - 1));

		gtk_box_pack_start (GTK_BOX (footer), item, FALSE, FALSE, 0);
		gtk_widget_show (item);
	}

	remainder = footer_remainder_new ();
	gtk_box_pack_start (GTK_BOX (footer), remainder, TRUE, TRUE, 0);

	date_string = eazel_services_get_current_date_string ();

	footer->details->date = eazel_services_label_new (date_string,
							  EAZEL_SERVICES_FOOTER_FONT_WEIGHT,
							  EAZEL_SERVICES_FOOTER_FONT_SIZE,
							  8, 0,
							  2, 0,
							  EAZEL_SERVICES_BACKGROUND_COLOR_RGBA,
							  EAZEL_SERVICES_NORMAL_FILL);
	g_free (date_string);
	
	gtk_widget_show (remainder);
	gtk_widget_show (footer->details->date);

	gtk_box_pack_start (GTK_BOX (footer), footer->details->date, FALSE, FALSE, 0);
}

void
eazel_services_footer_set_date (EazelServicesFooter *footer,
				const char *date)
{

	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (footer));
	g_return_if_fail (date != NULL);
	g_return_if_fail (date[0] != '\0');

	g_return_if_fail (NAUTILUS_IS_LABEL (footer->details->date));

	nautilus_label_set_text (NAUTILUS_LABEL (footer->details->date), date);
}
