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

#include "nautilus-login-view.h"



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

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR	"rgb:FFFF/FFFF/FFFF"
#define SERVICE_DOMAIN_NAME			"testmachine.eazel.com"
#define SERVICE_SUMMARY_LOCATION                "http://eazel1.eazel.com/services/control1.html"
#define SERVICE_HELP_LOCATION                   "http://www.eazel.com"

static void       nautilus_login_view_initialize_class (NautilusLoginViewClass     *klass);
static void       nautilus_login_view_initialize       (NautilusLoginView          *view);
static void       nautilus_login_view_destroy          (GtkObject                  *object);
static void       login_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusLoginView          *view);
static void       show_feedback                        (NautilusLoginView          *view,
							char                       *error_message);
static void       generate_login_form                  (NautilusLoginView          *view);
static void       entry_changed_cb                     (GtkWidget                  *entry,
							NautilusLoginView          *view);
static void       login_button_cb                      (GtkWidget                  *button,
							NautilusLoginView          *view);
static void       maintenance_button_cb                (GtkWidget                  *button,
							NautilusLoginView          *view);
static void       go_to_uri                            (NautilusLoginView          *view,
							char                       *uri);
static GtkWidget* create_title_widget                  (const char                 *title_text);
static GtkWidget* create_image_widget                  (const char                 *icon_name,
							const char                 *background_color_spec,
							NautilusImagePlacementType  placement);

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
	title = create_title_widget ("Please Sign in!");

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
		show_feedback (view, "Login Failed! Please try again.");
	}

	if (registered_ok) {
		go_to_uri (view, SERVICE_SUMMARY_LOCATION);
	}
}

/* callback to point account maintenance button to webpage */
static void
maintenance_button_cb (GtkWidget	*button, NautilusLoginView	*view) {

	go_to_uri (view, SERVICE_HELP_LOCATION);
}

/* utility routine to show an error message */
static void
show_feedback (NautilusLoginView	*view, char	*error_message) {

        gtk_label_set_text (GTK_LABEL (view->details->feedback_text), error_message);
        gtk_widget_show (view->details->feedback_text);
}

/* utility routine to go to another uri */

static void
go_to_uri (NautilusLoginView	*view, char	*uri) {

	nautilus_view_open_location (view->details->nautilus_view, uri);

}

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

/* FIXME bugzilla.eazel.com xxxx: 
 * create_image_widget() and create_title_widget() are cut-n-pasted from 
 * components/services/install/nautilus-view/nautilus-service-install-view.c
 * These should be put in a common place.
 */
static GtkWidget*
create_image_widget (const char			*icon_name,
		     const char			*background_color_spec,
		     NautilusImagePlacementType	placement)
{
	char		*path;
	GtkWidget	*image;
	GdkPixbuf	*pixbuf;
	guint32		background_rgb;

	g_return_val_if_fail (icon_name != NULL, NULL);
	g_return_val_if_fail (background_color_spec != NULL, NULL);

	image = nautilus_image_new();
	
	path = nautilus_pixmap_file (icon_name);
	
	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	if (pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);
		gdk_pixbuf_unref (pixbuf);
	}
	else {
		g_warning ("Could not find the requested icon.");
	}
	
	nautilus_image_set_background_type (NAUTILUS_IMAGE (image),
					    NAUTILUS_IMAGE_BACKGROUND_SOLID);
	
	background_rgb = nautilus_parse_rgb_with_white_default (background_color_spec);
	
	nautilus_image_set_background_color (NAUTILUS_IMAGE (image),
					     background_rgb);

	nautilus_image_set_placement_type (NAUTILUS_IMAGE (image), placement);

	return image;
}

static GtkWidget*
create_title_widget (const char *title_text) 
{
        GtkWidget	*title_hbox;
        GtkWidget	*logo_image;
        GtkWidget	*filler_image;
        GtkWidget	*text_image;
	GdkFont		*font;

	g_assert (title_text != NULL);

        title_hbox = gtk_hbox_new (FALSE, 0);

	logo_image = create_image_widget ("eazel-services-logo.png",
					  SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					  NAUTILUS_IMAGE_PLACEMENT_CENTER);

	filler_image = create_image_widget ("eazel-services-logo-tile.png",
					    SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					    NAUTILUS_IMAGE_PLACEMENT_TILE);

	text_image = create_image_widget ("eazel-services-logo-tile.png",
					  SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR,
					  NAUTILUS_IMAGE_PLACEMENT_TILE);
	
	font = nautilus_font_factory_get_font_by_family ("helvetica", 20);

	nautilus_image_set_label_text (NAUTILUS_IMAGE (text_image), title_text);
	nautilus_image_set_label_font (NAUTILUS_IMAGE (text_image), font);
	nautilus_image_set_extra_width (NAUTILUS_IMAGE (text_image), 8);
	nautilus_image_set_right_offset (NAUTILUS_IMAGE (text_image), 8);
	nautilus_image_set_top_offset (NAUTILUS_IMAGE (text_image), 3);

	gdk_font_unref (font);

	gtk_widget_show (logo_image);
	gtk_widget_show (filler_image);
	gtk_widget_show (text_image);

        gtk_box_pack_start (GTK_BOX (title_hbox), logo_image, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (title_hbox), filler_image, TRUE, TRUE, 0);
        gtk_box_pack_end (GTK_BOX (title_hbox), text_image, FALSE, FALSE, 0);

	return title_hbox;
}
