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
 * Author: J Shane Culpepper <pepper@eazel.com>
 */

#include <config.h>

#include "nautilus-summary-view.h"
#include "shared-service-widgets.h"

#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <stdio.h>
#include <unistd.h>

/* A NautilusContentView's private information. */
struct _NautilusSummaryViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*feedback_text;
};

#define SERVICE_DOMAIN_NAME			"testmachine.eazel.com"

static void	nautilus_summary_view_initialize_class	(NautilusSummaryViewClass	*klass);
static void	nautilus_summary_view_initialize	(NautilusSummaryView		*view);
static void	nautilus_summary_view_destroy		(GtkObject			*object);
static void	summary_load_location_callback		(NautilusView		*nautilus_view,
							 const char		*location,
	 						 NautilusSummaryView	*view);
static void	generate_summary_form			(NautilusSummaryView	*view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

static void
generate_summary_form (NautilusSummaryView	*view) {

	GtkWidget	*temp_widget;
	GtkWidget	*title;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	title = create_services_title_widget ("Eazel Services Summary");
	gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* put a mystery label here as a placeholder. */
	temp_widget = gtk_label_new ("I am just a view.  One day I will be gone.");
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 0);
	gtk_widget_show (temp_widget);

}

static void
nautilus_summary_view_initialize_class (NautilusSummaryViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_summary_view_destroy;
}

static void
nautilus_summary_view_initialize (NautilusSummaryView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusSummaryViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (summary_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_summary_view_destroy (GtkObject *object) {

	NautilusSummaryView	*view;
	
	view = NAUTILUS_SUMMARY_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_summary_view_get_nautilus_view (NautilusSummaryView *view) {

	return view->details->nautilus_view;
}

void
nautilus_summary_view_load_uri (NautilusSummaryView	*view,
			        const char		*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_summary_form (view);
}

static void
summary_load_location_callback (NautilusView		*nautilus_view, 
			        const char		*location,
			        NautilusSummaryView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_summary_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}

