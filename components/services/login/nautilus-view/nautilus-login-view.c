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
 *         Andy Hertzfeld <andy@eazel.com>
 *         Ramiro Estrugo <ramiro@eazel.com>
 */

#include <config.h>

#include "nautilus-login-view.h"
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
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <stdio.h>
#include <unistd.h>

/* A NautilusContentView's private information. */
struct _NautilusLoginViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*account_name;
	GtkWidget	*account_password;
	GtkWidget	*login_button;
	GtkWidget	*maintenance_button;
	GtkWidget	*feedback_text;
};

#define SERVICE_SUMMARY_LOCATION                "http://eazel1/services/summary1.html"
#define SERVICE_HELP_LOCATION                   "http://www.eazel.com"

static void       nautilus_login_view_initialize_class (NautilusLoginViewClass     *klass);
static void       nautilus_login_view_initialize       (NautilusLoginView          *view);
static void       nautilus_login_view_destroy          (GtkObject                  *object);
static void       login_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusLoginView          *view);
static void       generate_login_form                  (NautilusLoginView          *view);
static void       entry_changed_cb                     (GtkWidget                  *entry,
							NautilusLoginView          *view);
static void       login_button_cb                      (GtkWidget                  *button,
							NautilusLoginView          *view);
static void       maintenance_button_cb                (GtkWidget                  *button,
							NautilusLoginView          *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLoginView, nautilus_login_view, GTK_TYPE_EVENT_BOX)

static void
generate_login_form (NautilusLoginView	*view) 
{
	GtkTable	*table;
	GtkWidget	*temp_widget;
	GtkWidget	*temp_box;
	GtkWidget	*login_label;
	GtkWidget	*maintenance_button;
	GtkWidget	*maintenance_label;
	GdkFont		*font;
	GtkWidget	*title;

	/* allocate a box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* Setup the title */
	title = create_services_title_widget ("Please Sign in!");

        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* initialize the parent form */
	temp_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	/* allocate a table to hold the login form */
	table = GTK_TABLE (gtk_table_new (4, 3, TRUE));

	/* username */
	temp_widget = gtk_label_new ("User Name: ");

	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);

	gtk_table_attach (table, temp_widget, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);

	view->details->account_name = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_name, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 4);
	gtk_widget_show (view->details->account_name);

	/* password */
	temp_widget = gtk_label_new ("Password: ");

	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);

	gtk_table_attach (table, temp_widget, 0, 2, 2, 3, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);
	view->details->account_password = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_password, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 4, 4);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_password), FALSE);
	gtk_widget_show (view->details->account_password);

	/* insert the table */
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET(table), 0, 0, 4);
	gtk_widget_show (GTK_WIDGET(table));

	/* attach a changed signal to the 2 entry fields, so we can enable the button when something is typed into both fields */
	gtk_signal_connect (GTK_OBJECT (view->details->account_name),		"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_password),	"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);

	/* allocate the command buttons - first the login button */

	view->details->login_button = gtk_button_new ();
	login_label = gtk_label_new (" I'm ready to login! ");
	gtk_widget_show (login_label);
	gtk_container_add (GTK_CONTAINER (view->details->login_button), login_label);

	temp_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->login_button, FALSE, FALSE, 21);

	gtk_signal_connect (GTK_OBJECT (view->details->login_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (login_button_cb), view);
	gtk_widget_set_sensitive (view->details->login_button, FALSE);
	gtk_widget_show (view->details->login_button);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 4);

        /* now allocate the account maintenance button */

        maintenance_button = gtk_button_new ();
        maintenance_label = gtk_label_new ("  I need some help!  ");
        gtk_widget_show (maintenance_label);
        gtk_container_add (GTK_CONTAINER (maintenance_button), maintenance_label);
	temp_box = gtk_hbox_new (TRUE, 0);
        gtk_box_pack_start (GTK_BOX (temp_box), maintenance_button, FALSE, FALSE, 21);
        gtk_signal_connect (GTK_OBJECT (maintenance_button), "clicked",
			    GTK_SIGNAL_FUNC (maintenance_button_cb), view);
        gtk_widget_show (maintenance_button);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 4);

        /* add a label for error messages, but don't show it until there's an error */
        view->details->feedback_text = gtk_label_new ("");
        gtk_box_pack_end (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);
}

/* callback to enable/disable the login button when something is typed in the field */
static void
entry_changed_cb (GtkWidget	*entry, NautilusLoginView	*view) {

	char		*user_name;
	char		*password;
	gboolean	button_enabled;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));

	button_enabled = user_name && strlen (user_name) && password && strlen (password);
	gtk_widget_set_sensitive (view->details->login_button, button_enabled);
}

/* callback to handle the login button.  Right now only dumps a simple feedback message. */
static void
login_button_cb (GtkWidget	*button, NautilusLoginView	*view) {

	char		*user_name;
	char		*password;
	gboolean	registered_ok;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));
	registered_ok = FALSE;

	gtk_widget_hide (view->details->feedback_text);

	/* Do Nothing right now just return an ok response */
	registered_ok = TRUE;

	if (registered_ok == FALSE) {
		show_feedback (view->details->feedback_text, "Login Failed! Please try again.");
	}

	if (registered_ok) {
		go_to_uri (view->details->nautilus_view, SERVICE_SUMMARY_LOCATION);
	}
}

/* callback to point account maintenance button to webpage */
static void
maintenance_button_cb (GtkWidget	*button, NautilusLoginView	*view) {

	go_to_uri (view->details->nautilus_view, SERVICE_HELP_LOCATION);
}

/* utility routine to go to another uri */

static void
nautilus_login_view_initialize_class (NautilusLoginViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_login_view_destroy;
}

static void
nautilus_login_view_initialize (NautilusLoginView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusLoginViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (login_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_login_view_destroy (GtkObject *object) {

	NautilusLoginView	*view;
	
	view = NAUTILUS_LOGIN_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_login_view_get_nautilus_view (NautilusLoginView *view) {

	return view->details->nautilus_view;
}

void
nautilus_login_view_load_uri (NautilusLoginView	*view,
			      const char	*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_login_form (view);
}

static void
login_load_location_callback (NautilusView	*nautilus_view, 
			      const char	*location,
			      NautilusLoginView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_login_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}

