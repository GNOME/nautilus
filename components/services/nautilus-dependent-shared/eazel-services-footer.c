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

#define FOOTER_TEXT_SIZE (-3)

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

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Private stuff */
typedef struct
{
	GtkWidget *widget;
	char *name;
	char *prelight_name;
	GdkPixbuf *pixbuf;
	GdkPixbuf *prelight_pixbuf;
} PrelightData;

static gint
label_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);
	
	data = (PrelightData *) client_data;
	
	g_return_val_if_fail (NAUTILUS_IS_LABEL (data->widget), TRUE);
	g_return_val_if_fail (data->prelight_name, TRUE);

	if (data->prelight_pixbuf == NULL) {
		data->prelight_pixbuf = eazel_services_pixbuf_new (data->prelight_name);
	}
	
	if (data->prelight_pixbuf != NULL) {
		nautilus_label_set_tile_pixbuf (NAUTILUS_LABEL (data->widget),
							  data->prelight_pixbuf);
	}

	return TRUE;
}

static gint
label_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);
	
	data = (PrelightData *) client_data;
	
	g_return_val_if_fail (NAUTILUS_IS_LABEL (data->widget), TRUE);
	g_return_val_if_fail (data->name, TRUE);

	if (data->pixbuf == NULL) {
		data->pixbuf = eazel_services_pixbuf_new (data->name);
	}
	
	if (data->pixbuf != NULL) {
		nautilus_label_set_tile_pixbuf (NAUTILUS_LABEL (data->widget),
						 data->pixbuf);
	}

	return TRUE;
}

static gint
image_enter_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightData *data;
	
	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);
	
	data = (PrelightData *) client_data;
	
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (data->widget), TRUE);
	g_return_val_if_fail (data->prelight_name, TRUE);

	if (data->prelight_pixbuf == NULL) {
		data->prelight_pixbuf = eazel_services_pixbuf_new (data->prelight_name);
	}
	
	if (data->prelight_pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (data->widget), data->prelight_pixbuf);
	}

	return TRUE;
}

static gint
image_leave_event (GtkWidget *event_box,
		   GdkEventCrossing *event,
		   gpointer client_data)
{
	PrelightData *data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (event_box), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);
	
	data = (PrelightData *) client_data;
	
	g_return_val_if_fail (NAUTILUS_IS_IMAGE (data->widget), TRUE);
	g_return_val_if_fail (data->name, TRUE);

	if (data->pixbuf == NULL) {
		data->pixbuf = eazel_services_pixbuf_new (data->name);
	}
	
	if (data->pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (data->widget), data->pixbuf);
	}

	return TRUE;
}

typedef struct
{
	EazelServicesFooter *footer;
	int index;
} ButtonReleaseData;

static gint
footer_item_button_release_event (GtkWidget *widget,
				GdkEventButton *event,
				gpointer client_data)
{
	ButtonReleaseData	*data;

	g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), TRUE);
	g_return_val_if_fail (client_data != NULL, TRUE);

	data = (ButtonReleaseData *) client_data;

	g_return_val_if_fail (EAZEL_SERVICES_IS_FOOTER (data->footer), TRUE);
	
	gtk_signal_emit (GTK_OBJECT (data->footer), footer_signals[ITEM_CLICKED], data->index);

	return TRUE;
}

static void
prelight_data_free_callback (GtkWidget *event_box,
			     gpointer client_data)
{
	PrelightData *data;

	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (client_data != NULL);

	data = (PrelightData *) client_data;

	g_free (data->name);
	g_free (data->prelight_name);
	nautilus_gdk_pixbuf_unref_if_not_null (data->pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (data->prelight_pixbuf);
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
image_add_prelighting (NautilusImage *image,
		       GtkWidget *event_box,
		       const char *name,
		       const char *prelight_name)
{
	PrelightData *data;
	
	g_return_if_fail (NAUTILUS_IS_IMAGE (image));
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (name != NULL);
	g_return_if_fail (prelight_name != NULL);

	data = g_new0 (PrelightData, 1);

	data->widget = GTK_WIDGET (image);
	data->name = g_strdup (name);
	data->prelight_name = g_strdup (prelight_name);
	
	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (image_enter_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (image_leave_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (prelight_data_free_callback), data);
}

static void
label_add_prelighting (NautilusLabel *label,
		       GtkWidget *event_box,
		       const char *name,
		       const char *prelight_name)
{
	PrelightData *data;
	
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (GTK_IS_EVENT_BOX (event_box));
	g_return_if_fail (name != NULL);
	g_return_if_fail (prelight_name != NULL);

	data = g_new0 (PrelightData, 1);

	data->widget = GTK_WIDGET (label);
	data->name = g_strdup (name);
	data->prelight_name = g_strdup (prelight_name);
	
	gtk_signal_connect (GTK_OBJECT (event_box), "enter_notify_event", GTK_SIGNAL_FUNC (label_enter_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "leave_notify_event", GTK_SIGNAL_FUNC (label_leave_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (prelight_data_free_callback), data);
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
	GtkWidget *left;
	GtkWidget *label;
 	GtkWidget *right;
	ButtonReleaseData *data;

	g_return_val_if_fail (EAZEL_SERVICES_IS_FOOTER (footer), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);
	
	event_box = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (event_box), hbox);

	if (has_left_bumper) {
		left = eazel_services_image_new (EAZEL_SERVICES_NORMAL_LEFT_BUMPER,
						 EAZEL_SERVICES_NORMAL_FILL,
						 EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		image_add_prelighting (NAUTILUS_IMAGE (left), event_box, EAZEL_SERVICES_NORMAL_LEFT_BUMPER, EAZEL_SERVICES_PRELIGHT_LEFT_BUMPER);
	}
	else {
		left = eazel_services_image_new (EAZEL_SERVICES_NORMAL_FILL,
						 NULL,
						 EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		image_add_prelighting (NAUTILUS_IMAGE (left), event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
	}
	
	gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
	gtk_widget_show (left);

	label = eazel_services_label_new (text,
					  1,
					  0.1,
					  0.3,
					  2,
					  0,
					  EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
					  EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
					  EAZEL_SERVICES_NORMAL_FILL,
					  FOOTER_TEXT_SIZE,
					  TRUE);
	
	gtk_widget_show (label);

	data = g_new (ButtonReleaseData, 1);
	data->index = index;
	data->footer = footer;

	gtk_signal_connect (GTK_OBJECT (event_box), "button_release_event", GTK_SIGNAL_FUNC (footer_item_button_release_event), data);
	gtk_signal_connect (GTK_OBJECT (event_box), "destroy", GTK_SIGNAL_FUNC (free_data_callback), data);

	label_add_prelighting (NAUTILUS_LABEL (label), event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	if (has_right_bumper) {
		right = eazel_services_image_new (EAZEL_SERVICES_NORMAL_RIGHT_BUMPER, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		image_add_prelighting (NAUTILUS_IMAGE (right), event_box, EAZEL_SERVICES_NORMAL_RIGHT_BUMPER, EAZEL_SERVICES_PRELIGHT_RIGHT_BUMPER);
	}
	else {
		right = eazel_services_image_new (EAZEL_SERVICES_NORMAL_FILL, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		image_add_prelighting (NAUTILUS_IMAGE (right), event_box, EAZEL_SERVICES_NORMAL_FILL, EAZEL_SERVICES_PRELIGHT_FILL);
	}

	gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);
	gtk_widget_show (right);

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

	left = eazel_services_image_new (EAZEL_SERVICES_REMAINDER_LEFT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
	fill = eazel_services_image_new (NULL, EAZEL_SERVICES_REMAINDER_FILL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
	right = eazel_services_image_new (EAZEL_SERVICES_REMAINDER_RIGHT_BUMPER, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);

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
							  1,
							  0.1,
							  0.3,
							  5,
							  0,
							  EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
							  EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
							  EAZEL_SERVICES_NORMAL_FILL,
							  FOOTER_TEXT_SIZE,
							  TRUE);
	
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
