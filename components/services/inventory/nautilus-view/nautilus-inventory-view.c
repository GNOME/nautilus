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

#include <ghttp.h>
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

#include "nautilus-inventory-view.h"
#include "eazel-inventory-shared.h"



/* A NautilusContentView's private information. */
struct _NautilusInventoryViewDetails {
	char 		*uri;
	char 		*auth_token;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*feedback_text;
};

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR	"rgb:BBBB/DDDD/FFFF"
#define SERVICE_DOMAIN_NAME			"eazel24.eazel.com"

static void		nautilus_inventory_view_initialize_class	(NautilusInventoryViewClass	*klass);
static void		nautilus_inventory_view_initialize		(NautilusInventoryView		*view);
static void		nautilus_inventory_view_destroy			(GtkObject			*object);
static void		inventory_load_location_callback		(NautilusView			*nautilus_view,
									 const char			*location,
	 								 NautilusInventoryView		*view);
static void		show_feedback					(NautilusInventoryView		*view,
									 char				*error_message);
static void		generate_inventory_form				(NautilusInventoryView		*view);
static void		gather_config_button_cb				(GtkWidget			*button,
									 NautilusInventoryView		*view);
static void		register_later_cb				(GtkWidget			*button,
									 NautilusInventoryView		*view);
static void		go_to_uri					(NautilusInventoryView		*view,
									 char				*uri);
static char		*get_home_uri					(void);
static void		update_now					(void);
static ghttp_request	*make_http_post_request 			(char				*uri,
									 char				*post_body,
									 char				*auth_token);
static void		generate_form_title				(NautilusInventoryView		*view,
									 const char			*title_text);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryView, nautilus_inventory_view, GTK_TYPE_EVENT_BOX)

static void
generate_inventory_form (NautilusInventoryView	*view) {

	char		*message;
	char		*file_name;
	GtkWidget	*temp_widget;
	GtkWidget	*temp_box;
	GtkWidget	*config_button;
	GtkWidget	*config_label;

	/* allocate a vbox as a container */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_form_title (view, "Eazel Services Inventory");

	/* if we came from signup, add a congrats message */
	/* FIXME bugzilla.eazel.com 727: get the text from a file or from the service */

	if (nautilus_str_has_suffix (view->details->uri, "signup")) {
		message = "Congratulations, you have successfully signed up with the Eazel service!";
		temp_widget = gtk_label_new (message);

		gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 12);
		gtk_widget_show (temp_widget);
	}

	/* make label containing text about uploading the configuration data */
	/* FIXME bugzilla.eazel.com 727: It should get this text from a file or from the service */

	temp_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	file_name = nautilus_pixmap_file ("config-gather.png");
	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 8);
	gtk_widget_show (temp_widget);
	g_free (file_name);

	message = "With your permission, the Eazel service will gather data about the hardware and software configuration of your system so it can provide you with a customized software catalog with one-click installation.  Your configuration data will be kept strictly confidential and will not be used for any other purpose.  Click the button below to begin gathering the data.";
	temp_widget = gtk_label_new (message);
	gtk_label_set_line_wrap (GTK_LABEL(temp_widget), TRUE);
	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 12);
	gtk_widget_show (temp_widget);

	/* add buttons for accepting and declining */

	config_button = gtk_button_new ();
	config_label = gtk_label_new (" Gather Configuration Data ");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label);
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);

	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
	GTK_SIGNAL_FUNC (gather_config_button_cb), view);
	gtk_widget_show (config_button);

	/* now allocate the decline button */

	config_button = gtk_button_new ();
	config_label = gtk_label_new (" No, Don't Gather Configuration Data ");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label);
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);
	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
	GTK_SIGNAL_FUNC (register_later_cb), view);
	gtk_widget_show (config_button);

	/* show the buttons */
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 16);

	/* add a label for feedback, but don't show it until there's an error */
	view->details->feedback_text = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);

}

/* callback to handle the configuration button.  First, make the configuration xml file,
 * then feed it to the service over HTTP.  Display an error message, or move on to the summary
 * screen if successful
 */

/* FIXME bugzilla.eazel.com 727: text strings should be gotten from a file */

static void
gather_config_button_cb (GtkWidget	*button, NautilusInventoryView	*view) {

	FILE		*config_file;
	char		buffer[256];
	char		host_name[512];
	char		*config_file_name;
	char		*config_string;
	char		*uri;
	char		*response_str;
	char		*cookie_str;
	char		*encoded_token;
	char		*encoded_host_name;
	GString		*config_data;
	xmlDocPtr	config_doc;
	ghttp_request	*request;

	show_feedback (view, "gathering configuration data...");
	update_now ();

	config_doc = create_configuration_metafile ();
	if (config_doc == NULL) {
		show_feedback (view, "Sorry, there was an error during the gathering");
		return;
	}

	/* save the configuration file */

	config_file_name = g_strdup_printf ("%s/.nautilus/configuration.xml", g_get_home_dir ());
	xmlSaveFile (config_file_name, config_doc);
	xmlFreeDoc (config_doc);

	/* load the config file text into memory */

	config_data = g_string_new ("");
	config_file = fopen (config_file_name, "r");

	while (fgets (buffer, 255, config_file) != NULL) {
		g_string_append (config_data, buffer);
	}
	fclose (config_file);
	config_string = strdup (config_data->str);
	g_string_free(config_data, TRUE);
	g_free (config_file_name);

	/* move into transmission phase by changing the feedback message */

	show_feedback (view, "transmitting configuration data...");
	update_now ();

	/* send the config file to the server via HTTP */
	uri = g_strdup_printf ("http://%s/profile/set.pl", SERVICE_DOMAIN_NAME);

	gethostname (&host_name[0], 511);

	encoded_token = gnome_vfs_escape_string (view->details->auth_token);
	encoded_host_name = gnome_vfs_escape_string (host_name);

	cookie_str = g_strdup_printf ("token=%s; computer=%s", encoded_token, encoded_host_name);
	request = make_http_post_request (uri, config_string, cookie_str);

	g_free (encoded_token);
	g_free (encoded_host_name);
	g_free (cookie_str);

	response_str = ghttp_get_body (request);
	g_message ("config response was %s", response_str);

	g_free (uri);
	ghttp_request_destroy (request);

	/* handle the error response */
	if (strstr (response_str, "<ERROR field=") == response_str) {
		show_feedback (view, "Sorry, but there was an error.  Please try again some other time.");
	}
	else {

		/* give error feedback or go to the summary form if successful */
		go_to_uri (view, "eazel:overview?config");
	}

	g_free (config_string);

}

/* callback to handle the register later button */
static void
register_later_cb (GtkWidget	*button, NautilusInventoryView	*view) {

	char	*home_path;

	home_path = get_home_uri ();
	go_to_uri (view, home_path);
	g_free (home_path);

}

/* utility routine to show an error message */
static void
show_feedback (NautilusInventoryView	*view, char	*error_message) {

        gtk_label_set_text (GTK_LABEL (view->details->feedback_text), error_message);
        gtk_widget_show (view->details->feedback_text);
}

/* utility routine to make an HTTP request.  For now, it works synchronously but soon 
 *    it will optionally work asynchronously.  Return NULL if we get an error */

static ghttp_request*
make_http_post_request (char	*uri, char	*post_body, char	*auth_token) {

	ghttp_request* request;
	char* proxy;

	request = NULL;
	proxy = g_getenv ("http_proxy");

	request = ghttp_request_new();

	if (!request) {
		return NULL;
	}

	if (proxy && (ghttp_set_proxy (request, proxy) != 0)) {
		ghttp_request_destroy (request);
		return NULL;
	}

	if (ghttp_set_uri (request, uri) != 0) {
		ghttp_request_destroy (request);
		return NULL;
	}

	ghttp_set_type (request, ghttp_type_post);
	ghttp_set_header (request, http_hdr_Accept, "text/xml");
	ghttp_set_header (request, http_hdr_Content_Type, "text/xml");
	ghttp_set_header (request, http_hdr_Host, SERVICE_DOMAIN_NAME);
	/* FIXME bugzilla.eazel.com 726: user agent version and OS should be substituted on the fly */
	ghttp_set_header (request, http_hdr_User_Agent, "Nautilus/0.1 (Linux)");

	if (auth_token) {
		ghttp_set_header (request, "Cookie", auth_token);
	}

	ghttp_set_header (request, http_hdr_Connection, "close");

	ghttp_set_body (request, post_body, strlen (post_body));

	if (ghttp_prepare (request) != 0) {
		ghttp_request_destroy (request);
		return NULL;
	}

	/* here's where it spends all the time doing the actual request  */
	if (ghttp_process (request) != ghttp_done) {
		ghttp_request_destroy (request);
		return NULL;
	}

	return request;

}

/* utility routine to go to another uri */

static void
go_to_uri (NautilusInventoryView	*view, char	*uri) {

	nautilus_view_open_location (view->details->nautilus_view, uri);

}

/* FIXME bugzilla.eazel.com 729: 
 * this routine should be somewhere else, and should take user preferences into account 
 */

static char*
get_home_uri () {

	return g_strdup_printf ("file://%s", g_get_home_dir ());

}

/* utility to force updating to happen */
static void
update_now () {

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

}

/* generate the standard eazel services header */
static void
generate_form_title (NautilusInventoryView	*view,
		     const char			*title_text) {

        GtkWidget	*temp_widget;
        char		*file_name;
        GtkWidget	*temp_container;
        GdkFont		*font;

        temp_container = gtk_hbox_new (FALSE, 0);

        gtk_box_pack_start (GTK_BOX (view->details->form), temp_container, 0, 0, 4);
        gtk_widget_show (temp_container);

        file_name = nautilus_pixmap_file ("eazel-logo.gif");
        temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
        gtk_box_pack_start (GTK_BOX(temp_container), temp_widget, 0, 0, 8);
        gtk_widget_show (temp_widget);
        g_free (file_name);

        view->details->form_title = gtk_label_new (title_text);

        font = nautilus_font_factory_get_font_from_preferences (18);
        nautilus_gtk_widget_set_font (view->details->form_title, font);
        gdk_font_unref (font);

        gtk_box_pack_start (GTK_BOX (temp_container), view->details->form_title, 0, 0, 8);
        gtk_widget_show (view->details->form_title);
}

static void
nautilus_inventory_view_initialize_class (NautilusInventoryViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_inventory_view_destroy;
}

static void
nautilus_inventory_view_initialize (NautilusInventoryView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusInventoryViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (inventory_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_inventory_view_destroy (GtkObject *object) {

	NautilusInventoryView	*view;
	
	view = NAUTILUS_INVENTORY_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_inventory_view_get_nautilus_view (NautilusInventoryView *view) {

	return view->details->nautilus_view;
}

void
nautilus_inventory_view_load_uri (NautilusInventoryView	*view,
			     	  const char		*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_inventory_form (view);
}

static void
inventory_load_location_callback (NautilusView		*nautilus_view, 
			      const char		*location,
			      NautilusInventoryView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_inventory_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}

