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
#include <libnautilus-extensions/nautilus-clickable-image.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

#define FOOTER_TEXT_SIZE (-3)

struct _EazelServicesFooterDetails
{
	GtkWidget *date;
	GdkPixbuf *item_prelight_tile;
	GdkPixbuf *item_tile;
	GdkPixbuf *left_bumper_tile_pixbuf;
	GdkPixbuf *left_bumper_tile_prelight_pixbuf;
	GdkPixbuf *right_bumper_tile_pixbuf;
	GdkPixbuf *right_bumper_prelight_tile_pixbuf;
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
eazel_services_footer_initialize (EazelServicesFooter *footer)
{
	footer->details = g_new0 (EazelServicesFooterDetails, 1);

	footer->details->item_tile = eazel_services_pixbuf_new (EAZEL_SERVICES_NORMAL_FILL);
	footer->details->item_prelight_tile = eazel_services_pixbuf_new (EAZEL_SERVICES_PRELIGHT_FILL);
	footer->details->left_bumper_tile_pixbuf = eazel_services_pixbuf_new (EAZEL_SERVICES_NORMAL_LEFT_BUMPER);
	footer->details->left_bumper_tile_prelight_pixbuf = eazel_services_pixbuf_new (EAZEL_SERVICES_PRELIGHT_LEFT_BUMPER);
	footer->details->right_bumper_tile_pixbuf = eazel_services_pixbuf_new (EAZEL_SERVICES_NORMAL_RIGHT_BUMPER);
	footer->details->right_bumper_prelight_tile_pixbuf = eazel_services_pixbuf_new (EAZEL_SERVICES_PRELIGHT_RIGHT_BUMPER);
}

/* GtkObjectClass methods */
static void
footer_destroy (GtkObject *object)
{
	EazelServicesFooter *footer;
	
	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (object));
	
	footer = EAZEL_SERVICES_FOOTER (object);
	
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->item_tile);
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->item_prelight_tile);
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->left_bumper_tile_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->left_bumper_tile_prelight_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->right_bumper_tile_pixbuf);
	nautilus_gdk_pixbuf_unref_if_not_null (footer->details->right_bumper_prelight_tile_pixbuf);

	g_free (footer->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
footer_item_clicked_callback (GtkWidget *widget,
			      gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_CLICKABLE_IMAGE (widget));
	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (callback_data));

	gtk_signal_emit (GTK_OBJECT (callback_data),
			 footer_signals[ITEM_CLICKED],
			 GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "index")));
}

static void
footer_item_enter_callback (GtkWidget *widget,
			     gpointer callback_data)
{
	EazelServicesFooter *footer;
	NautilusLabeledImage *label;
	NautilusLabeledImage *left_bumper;
	NautilusLabeledImage *right_bumper;

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (callback_data));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "left-bumper")));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "right-bumper")));

	footer = EAZEL_SERVICES_FOOTER (callback_data);

	left_bumper = NAUTILUS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "left-bumper"));
	label = NAUTILUS_LABELED_IMAGE (widget);
	right_bumper = NAUTILUS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "right-bumper"));

	nautilus_labeled_image_set_tile_pixbuf (label, footer->details->item_prelight_tile);
	nautilus_labeled_image_set_pixbuf (left_bumper, footer->details->left_bumper_tile_prelight_pixbuf);
	nautilus_labeled_image_set_pixbuf (right_bumper, footer->details->right_bumper_prelight_tile_pixbuf);
}

static void
footer_item_leave_callback (GtkWidget *widget,
			     gpointer callback_data)
{
	EazelServicesFooter *footer;
	NautilusLabeledImage *label;
	NautilusLabeledImage *left_bumper;
	NautilusLabeledImage *right_bumper;

	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	g_return_if_fail (EAZEL_SERVICES_IS_FOOTER (callback_data));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "left-bumper")));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (widget));
	g_return_if_fail (NAUTILUS_IS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "right-bumper")));

	footer = EAZEL_SERVICES_FOOTER (callback_data);

	left_bumper = NAUTILUS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "left-bumper"));
	label = NAUTILUS_LABELED_IMAGE (widget);
	right_bumper = NAUTILUS_LABELED_IMAGE (gtk_object_get_data (GTK_OBJECT (widget), "right-bumper"));

	nautilus_labeled_image_set_tile_pixbuf (label, footer->details->item_tile);
	nautilus_labeled_image_set_pixbuf (left_bumper, footer->details->left_bumper_tile_pixbuf);
	nautilus_labeled_image_set_pixbuf (right_bumper, footer->details->right_bumper_tile_pixbuf);
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

	g_return_val_if_fail (EAZEL_SERVICES_IS_FOOTER (footer), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (text[0] != '\0', NULL);
	
	event_box = gtk_event_box_new ();
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (event_box), hbox);

	if (has_left_bumper) {
		left = eazel_services_image_new_clickable (NULL, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (left), TRUE);
	}
	else {
		left = eazel_services_image_new_clickable (NULL, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (left), TRUE);
	}
	
	label = eazel_services_label_new_clickable (text,
						    1,
						    0.1,
						    0.3,
						    2,
						    0,
						    EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
						    EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
						    NULL,
						    FOOTER_TEXT_SIZE,
						    TRUE);
	
	nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (label), TRUE);
	
	if (has_right_bumper) {
		right = eazel_services_image_new_clickable (NULL, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (right), TRUE);
	}
	else {
		right = eazel_services_image_new_clickable (NULL, NULL, EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
		nautilus_labeled_image_set_fill (NAUTILUS_LABELED_IMAGE (right), TRUE);
	}
	

	gtk_object_set_data (GTK_OBJECT (left), "index", GINT_TO_POINTER (index));
	gtk_object_set_data (GTK_OBJECT (label), "index", GINT_TO_POINTER (index));
	gtk_object_set_data (GTK_OBJECT (right), "index", GINT_TO_POINTER (index));

	gtk_object_set_data (GTK_OBJECT (label), "left-bumper", left);
	gtk_object_set_data (GTK_OBJECT (label), "right-bumper", right);

	gtk_signal_connect (GTK_OBJECT (left), "clicked", GTK_SIGNAL_FUNC (footer_item_clicked_callback), footer);
	gtk_signal_connect (GTK_OBJECT (label), "clicked", GTK_SIGNAL_FUNC (footer_item_clicked_callback), footer);
	gtk_signal_connect (GTK_OBJECT (right), "clicked", GTK_SIGNAL_FUNC (footer_item_clicked_callback), footer);

	gtk_signal_connect (GTK_OBJECT (label), "enter", GTK_SIGNAL_FUNC (footer_item_enter_callback), footer);
	gtk_signal_connect (GTK_OBJECT (label), "leave", GTK_SIGNAL_FUNC (footer_item_leave_callback), footer);

	footer_item_leave_callback (label, footer);

	gtk_box_pack_start (GTK_BOX (hbox), left, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), right, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

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
