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
#include "shared-service-utilities.h"

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

	GtkWidget	*username_label;
	GtkWidget	*password_label;
	GtkWidget	*username_entry;
	GtkWidget	*password_entry;
	GtkWidget	*login_button;
	GtkWidget	*login_label;
	GtkWidget	*maintenance_button;
	GtkWidget	*maintenance_label;

	GtkWidget	*feedback_text;
};

static void	nautilus_summary_view_initialize_class	(NautilusSummaryViewClass	*klass);
static void	nautilus_summary_view_initialize	(NautilusSummaryView		*view);
static void	nautilus_summary_view_destroy		(GtkObject			*object);
static void	summary_load_location_callback		(NautilusView			*nautilus_view,
							 const char			*location,
	 						 NautilusSummaryView		*view);
static void	generate_summary_form			(NautilusSummaryView		*view);
static void	login_button_cb				(GtkWidget			*button,
							 NautilusSummaryView		*view); 
static void	maintenance_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view); 
static void	entry_changed_cb			(GtkWidget			*entry,
							 NautilusSummaryView		*view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

static void
generate_summary_form (NautilusSummaryView	*view)
{

	GtkWidget	*frame;
	GtkTable	*parent;
	GtkWidget	*title;
	GtkWidget	*temp_box;
	GtkTable	*login_table;
	GtkWidget	*button_box;
	GdkFont		*font;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	title = create_services_title_widget ("Eazel Services Summary");
	gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* Create the Parent Table to hold the 4 frames */
	parent = GTK_TABLE (gtk_table_new (3, 2, FALSE));

	/* Create the Services Listing Frame */
	frame = gtk_frame_new ("Services Placeholder");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_frame_set_label_align (GTK_FRAME (frame), 0.5, 0);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 1,
			  0, 1,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* Create the Login Frame */
	frame = gtk_frame_new ("Login Placeholder");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_frame_set_label_align (GTK_FRAME (frame), 0.5, 0);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  1, 2,
			  0, 1,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	temp_box = gtk_vbox_new (FALSE, 0);
	login_table = GTK_TABLE (gtk_table_new (4, 2, TRUE));

	view->details->username_label = gtk_label_new ("User Name:");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (view->details->username_label, font);
	gdk_font_unref (font);
	gtk_table_attach (login_table, view->details->username_label, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->username_label);

	view->details->username_entry = gtk_entry_new_with_max_length (36);
	gtk_table_attach (login_table, view->details->username_entry, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->username_entry);

	view->details->password_label = gtk_label_new ("Password:");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (view->details->password_label, font);
	gdk_font_unref (font);
	gtk_table_attach (login_table, view->details->password_label, 0, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->password_label);

	view->details->password_entry = gtk_entry_new_with_max_length (36);
	gtk_table_attach (login_table, view->details->password_entry, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->password_entry), FALSE);
	gtk_widget_show (view->details->password_entry);

	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (login_table), 0, 0, 0);
	gtk_widget_show (GTK_WIDGET (login_table));

	gtk_signal_connect (GTK_OBJECT (view->details->username_entry), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);
	gtk_signal_connect (GTK_OBJECT (view->details->password_entry), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);

	view->details->login_button = gtk_button_new ();
	view->details->login_label = gtk_label_new (" I'm ready to login! ");
	gtk_widget_show (view->details->login_label);
	gtk_container_add (GTK_CONTAINER (view->details->login_button), view->details->login_label);
	button_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), view->details->login_button, FALSE, FALSE, 21);
	gtk_signal_connect (GTK_OBJECT (view->details->login_button), "clicked", GTK_SIGNAL_FUNC (login_button_cb), view);
	gtk_widget_set_sensitive (view->details->login_button, FALSE);
	gtk_widget_show (view->details->login_button);
	gtk_widget_show (button_box);
	gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);

	view->details->maintenance_button = gtk_button_new ();
	view->details->maintenance_label = gtk_label_new (" I need some help! ");
	gtk_widget_show (view->details->maintenance_label);
	gtk_container_add (GTK_CONTAINER (view->details->maintenance_button), view->details->maintenance_label);
	button_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), view->details->maintenance_button, FALSE, FALSE, 21);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->maintenance_button, FALSE, FALSE, 1);
	gtk_signal_connect (GTK_OBJECT (view->details->maintenance_button), "clicked", GTK_SIGNAL_FUNC (maintenance_button_cb), view);
	gtk_widget_show (view->details->maintenance_button);
	gtk_widget_show (button_box);
	gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);


	gtk_widget_show (temp_box);
	gtk_container_add (GTK_CONTAINER (frame), temp_box);

	/* Create the Service News Frame */
	frame = gtk_frame_new ("Service News Placeholder");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_frame_set_label_align (GTK_FRAME (frame), 0.5, 0);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 2,
			  1, 2,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* Create the Update News Frame */
	frame = gtk_frame_new ("Update News Placeholder");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_frame_set_label_align (GTK_FRAME (frame), 0.5, 0);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 2,
			  2, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET (parent), 0, 0, 4);
	gtk_widget_show (GTK_WIDGET (parent));
}

/* callback to enable / disable the login button when something is typed into the field */
static void
entry_changed_cb (GtkWidget	*entry, NautilusSummaryView	*view)
{

	char		*user_name;
	char		*password;
	gboolean	button_enabled;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->username_entry));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->password_entry));

	button_enabled = user_name && strlen (user_name) && password && strlen (password);
	gtk_widget_set_sensitive (view->details->login_button, button_enabled);

}

/* callback to handle the login button.  Right now only does a simple redirect. */
static void
login_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	go_to_uri (view->details->nautilus_view, "eazel-inventory:");

}

/* callback to handle the maintenance button.  Right now only does a simple redirect. */
static void
maintenance_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	go_to_uri (view->details->nautilus_view, "http://www.eazel.com/services.html");

}

static void
nautilus_summary_view_initialize_class (NautilusSummaryViewClass *klass)
{

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_summary_view_destroy;

}

static void
nautilus_summary_view_initialize (NautilusSummaryView *view)
{

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
nautilus_summary_view_destroy (GtkObject *object)
{

	NautilusSummaryView	*view;
	
	view = NAUTILUS_SUMMARY_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_summary_view_get_nautilus_view (NautilusSummaryView *view)
{

	return view->details->nautilus_view;

}

void
nautilus_summary_view_load_uri (NautilusSummaryView	*view,
			        const char		*uri)
{

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
			        NautilusSummaryView	*view)
{

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_summary_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);

}

