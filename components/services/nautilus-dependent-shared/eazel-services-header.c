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
#include "eazel-services-extensions.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-theme.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

struct _EazelServicesHeaderDetails
{
	GtkWidget *left_text;
	GtkWidget *right_text;
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
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* EazelServicesHeader public methods */
GtkWidget *
eazel_services_header_title_new (const char *left_text)
{
	EazelServicesHeader *header;
 	GtkWidget *fill;
 	GtkWidget *logo;
	
	header = EAZEL_SERVICES_HEADER (gtk_widget_new (eazel_services_header_get_type (), NULL));

	header->details->left_text = eazel_services_label_new (left_text,
							       1,
							       0.1,
							       0.1,
							       10,
							       0,
							       EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
							       EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
							       EAZEL_SERVICES_HEADER_TITLE_FILL_ICON,
							       EAZEL_SERVICES_HEADER_TEXT_SIZE_REL,
							       TRUE);

	gtk_box_pack_start (GTK_BOX (header), header->details->left_text, FALSE, FALSE, 0);
	gtk_widget_show (header->details->left_text);
	
	fill = eazel_services_image_new (NULL,
					 EAZEL_SERVICES_HEADER_TITLE_FILL_ICON,
					 EAZEL_SERVICES_BACKGROUND_COLOR_RGB);

	gtk_box_pack_start (GTK_BOX (header), fill, TRUE, TRUE, 0);
	gtk_widget_show (fill);

	logo = eazel_services_image_new (EAZEL_SERVICES_HEADER_TITLE_LOGO_ICON,
					 NULL,
					 EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
	gtk_box_pack_end (GTK_BOX (header), logo, FALSE, FALSE, 0);
	gtk_widget_show (logo);

	return GTK_WIDGET (header);
}

GtkWidget *
eazel_services_header_middle_new (const char *left_text,
				  const char *right_text)
{
	EazelServicesHeader *header;
 	GtkWidget *fill;

	header = EAZEL_SERVICES_HEADER (gtk_widget_new (eazel_services_header_get_type (), NULL));

	header->details->left_text =
		eazel_services_label_new (left_text,
					  0,
					  0.1,
					  0.3,
					  10,
					  0,
					  EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
					  EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
					  EAZEL_SERVICES_HEADER_MIDDLE_FILL_ICON,
					  0,
					  TRUE);

	nautilus_label_set_tile_height (NAUTILUS_LABEL (header->details->left_text),
					NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);

	gtk_box_pack_start (GTK_BOX (header), header->details->left_text, FALSE, FALSE, 0);
	gtk_widget_show (header->details->left_text);
	
	fill = eazel_services_image_new (NULL,
					 EAZEL_SERVICES_HEADER_MIDDLE_FILL_ICON,
					 EAZEL_SERVICES_BACKGROUND_COLOR_RGB);
	nautilus_image_set_tile_height (NAUTILUS_IMAGE (fill),
					NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);

	gtk_box_pack_start (GTK_BOX (header), fill, TRUE, TRUE, 0);
	gtk_widget_show (fill);

	header->details->right_text = 
		eazel_services_label_new (right_text,
					  0,
					  0.1,
					  0.3,
					  76,
					  0,
					  EAZEL_SERVICES_TITLE_TEXT_COLOR_RGB,
					  EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
					  EAZEL_SERVICES_HEADER_MIDDLE_FILL_ICON,
					  -2,
					  TRUE);

	nautilus_label_set_tile_height (NAUTILUS_LABEL (header->details->right_text),
					NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);

	gtk_box_pack_start (GTK_BOX (header), header->details->right_text, FALSE, FALSE, 0);
	gtk_widget_show (header->details->right_text);

	return GTK_WIDGET (header);
}

void
eazel_services_header_set_left_text (EazelServicesHeader *header,
				     const char *text)
{
	g_return_if_fail (EAZEL_SERVICES_IS_HEADER (header));
	g_return_if_fail (text != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (header->details->left_text));
	
	nautilus_label_set_text (NAUTILUS_LABEL (header->details->left_text), text);
}

void
eazel_services_header_set_right_text (EazelServicesHeader *header,
				      const char *text)
{
	g_return_if_fail (EAZEL_SERVICES_IS_HEADER (header));
	g_return_if_fail (text != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (header->details->right_text));
	
	nautilus_label_set_text (NAUTILUS_LABEL (header->details->right_text), text);
}
