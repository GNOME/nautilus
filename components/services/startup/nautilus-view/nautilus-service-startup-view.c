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
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
            Andy Hertzfeld <andy@eazel.com>
 */

/* nautilus-services-content-view.c - services content view
   component. This component supplies some UI for service registration
   invokes other functions like configuration upload */
   
#include <config.h>
#include <ghttp.h>
#include <unistd.h>
#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <stdio.h>

#include "nautilus-service-startup-view.h"
#include "eazel-register.h"

struct _NautilusServicesContentViewDetails {
	char* uri;
	char* auth_token;
	NautilusView* nautilus_view;
	GtkWidget* form;
	GtkWidget* form_title;
	GtkWidget* account_name;
	GtkWidget* account_password;
	GtkWidget* confirm_password;
	GtkWidget* register_button;
	GtkWidget* feedback_text;
};


#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:BBBB/DDDD/FFFF"
/* FIXME bugzilla.eazel.com 728: the service domain name should be settable and kept with the other preferences. */
#define SERVICE_DOMAIN_NAME		       "eazel24.eazel.com"

static void nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass* klass);
static void nautilus_service_startup_view_initialize (NautilusServicesContentView* view);
static void nautilus_service_startup_view_destroy (GtkObject* object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServicesContentView, nautilus_service_startup_view, GTK_TYPE_EVENT_BOX)

static void service_main_notify_location_change_cb (NautilusView* view, 
                                                    Nautilus_NavigationInfo* navinfo, 
                                                    NautilusServicesContentView* services);
static gboolean is_location (char* document_str, const char* place_str);

/* utility routine to go to another uri */

static void
go_to_uri(NautilusServicesContentView* view, char* uri)
{

	Nautilus_NavigationRequestInfo nri;	
	memset (&nri, 0, sizeof(nri));
 	nri.requested_uri = uri;
  	nautilus_view_request_location_change (view->details->nautilus_view, &nri);
}

/* temporary callback to handle the configuration button */    
static void
config_button_cb (GtkWidget* button, char* data) {

	xmlDocPtr packages_file;
	char* temp_str;

	packages_file = create_configuration_metafile ();
	if (packages_file != NULL) {
  		temp_str = g_strdup_printf ("%s/configuration.xml", g_get_home_dir());
		xmlSaveFile (temp_str, packages_file);
  		xmlFreeDoc (packages_file);
  		g_free (temp_str);
	}
}

/* callback invoked to enable/disable the register button when something is typed into a field */
static void
entry_changed_cb (GtkWidget* entry, NautilusServicesContentView* view) {

	char* email;
	char* password;
	char* confirm;
	gboolean button_enabled;

	email = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));
	confirm  = gtk_entry_get_text (GTK_ENTRY (view->details->confirm_password));

	button_enabled = email && strlen (email) && password && strlen (password) && confirm && strlen (confirm);
	gtk_widget_set_sensitive (view->details->register_button, button_enabled);
}

/* utility routine to make an HTTP request.  For now, it works synchronously but soon 
   it will optionally work asynchronously.  Return NULL if we get an error */

static ghttp_request*
make_http_post_request (char* uri, char* post_body, char* auth_token) {

    ghttp_request* request;
    char* proxy;

    request = NULL;
    proxy = g_getenv ("http_proxy");
        
    request = ghttp_request_new();
   
    if (!request)
    	return NULL;
     	
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

/* utility to force updating to happen */
static void
update_now () {

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

/* utility routine to show an error message */
static void
show_feedback (NautilusServicesContentView* view, char* error_message) {

	gtk_label_set_text (GTK_LABEL (view->details->feedback_text), error_message);
	gtk_widget_show (view->details->feedback_text);
}


/* callback to handle the configuration button.  First, make the configuration xml file,
   then feed it to the service over HTTP.  Display an error message, or move on to the summary
   screen if successful */
/* FIXME bugzilla.eazel.com 727: text strings should be gotten from a file */      

static void
gather_config_button_cb (GtkWidget* button, NautilusServicesContentView* view) {

	FILE* config_file;
	char buffer[256];
	char host_name[512];
	char* config_file_name;
	char* config_string;
	char* uri;
	char* response_str;
	char* cookie_str;
	char* encoded_token;
	char* encoded_host_name;
	GString* config_data;
	xmlDocPtr config_doc;
	ghttp_request* request;	
	
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
		
	encoded_token = gnome_vfs_escape_string (view->details->auth_token, GNOME_VFS_URI_ENCODING_XALPHAS);
	encoded_host_name = gnome_vfs_escape_string (host_name, GNOME_VFS_URI_ENCODING_XALPHAS);
	
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

/* handle the registration command */
/* FIXME bugzilla.eazel.com 727: get error messages from a file somewhere */
   
static void
register_button_cb (GtkWidget* button, NautilusServicesContentView* view) {

	ghttp_request* request;	
	char* response_str;
	char* body;
	char* uri;
	char* encoded_email;
	char* encoded_password;
	char* email;
	char* password;
	char* confirm; 
	gboolean registered_ok;

	email = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));
	confirm  = gtk_entry_get_text (GTK_ENTRY (view->details->confirm_password));
	registered_ok = FALSE;
		
	/* see if the email address is valid; if not, display an error */
	
	if (!strlen (email) || !strchr (email,'@')) {
		show_feedback (view, "You have not typed a valid email address!  Please correct it and try again.");
		return;
	}
	
	/* see if the passwords are valid and matching; if not, display an error */
	
	if (!strlen (password) || !strlen (confirm)) {
		show_feedback (view, "You have not typed a valid password!  Please correct it and try again.");
		return;
	}

	if (strcmp (password, confirm)) {
		show_feedback (view, "The passwords don't match!  Please correct them and try again.");
		return;
	}
		
	/* hide the error text and give feedback in the status area during the request */
	gtk_widget_hide (view->details->feedback_text);
		
	encoded_email = gnome_vfs_escape_string (email, GNOME_VFS_URI_ENCODING_XALPHAS);
	encoded_password = gnome_vfs_escape_string (password, GNOME_VFS_URI_ENCODING_XALPHAS);
	
	body = g_strdup_printf ("email=%s&pwd=%s", encoded_email, encoded_password);
	uri = g_strdup_printf ("http://%s/member/new.pl", SERVICE_DOMAIN_NAME);
	g_free (encoded_email);
	g_free (encoded_password);
	
	request = make_http_post_request (uri, body, NULL);
	response_str = ghttp_get_body (request);
	
	/* handle the error response */
	if (request == NULL) {
		show_feedback (view, "Sorry, but the service did not respond.  Please try again later.");
	}
	else if (response_str && (strstr (response_str, "<ERROR field=") == response_str)) {
		if (strstr (response_str, "email")) {
			if (strstr (response_str, "taken"))
				show_feedback (view, "That email address is already registered!  Please change it and try again.");
			else
				show_feedback (view, "That email address is invalid!  Please change it and try again.");
		}
	}
	else {
	/* check for a cookie that indicates success */
		const char* cookie;
		char* token_start;

		cookie = ghttp_get_header (request, "Set-Cookie");
		
 	 	/* we found the cookie, so save it in the services file */
		token_start = strstr (cookie, "token=");
		if (token_start) {

			char* temp_str;
			char* token_end;

			temp_str = strdup(token_start + strlen("token="));
			token_end = strchr(temp_str, ';');

			if (token_end) {
				xmlDocPtr service_doc;
				xmlNodePtr service_node;
				char* temp_filename;
			
				*token_end = '\0';
	
				service_doc = xmlNewDoc ("1.0");
				service_node = xmlNewDocNode (service_doc, NULL, "SERVICE", NULL);
				service_doc->root = service_node;
	                        xmlSetProp (service_node, "domain", SERVICE_DOMAIN_NAME);
  		
  				/* save the token in the view object for subsequent accesses */
  				if (view->details->auth_token)
  					g_free (view->details->auth_token);
  				
				view->details->auth_token = g_strdup (gnome_vfs_unescape_string (temp_str));	
	                        xmlSetProp (service_node, "token", view->details->auth_token);
				
				temp_filename = g_strdup_printf ("%s/.nautilus/service.xml", g_get_home_dir ());
				xmlSaveFile (temp_filename, service_doc);
  				xmlFreeDoc (service_doc);
  				g_free (temp_filename);
			}
			g_free (temp_str);
		}
		
		registered_ok = TRUE;
	}
	
	g_free (uri);
	g_free (body);
	ghttp_request_destroy (request);
	
	/* we succeeded in registering, so advance to the configuration form */	
	if (registered_ok) {
  		go_to_uri (view, "eazel:config?signup");
	}
}

/* FIXME bugzilla.eazel.com 729: 
 * this routine should be somewhere else, and should take user preferences into account 
 */
static char*
get_home_uri () {

	return g_strdup_printf ("file://%s", g_get_home_dir ());
}

/* callback to handle the register later button */    
static void
register_later_cb (GtkWidget* button, NautilusServicesContentView* view) {

	char* home_path;

	home_path = get_home_uri (); 
	go_to_uri (view, home_path);
	g_free (home_path);
}

/* this creates the simple test form (just temporary) */

static void
setup_test_form (NautilusServicesContentView* view) {

	GtkWidget* temp_widget;
	GtkWidget* config_button;
	GtkWidget* config_label;
	GtkWidget* temp_box;

	/* allocate a box to hold everything */	
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);
	
	/* put a label here as a placeholder just to show something */
	temp_widget = gtk_label_new ("Service View");
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 0);	
	gtk_widget_show (temp_widget);

	/* add a button to create the configuration file */

	config_button = gtk_button_new ();		    
	config_label = gtk_label_new ("Create Configuration Inventory");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label); 	
	gtk_button_set_relief (GTK_BUTTON (config_button), GTK_RELIEF_NORMAL);
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 16);
	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
			    GTK_SIGNAL_FUNC (config_button_cb), NULL);	
	gtk_widget_show (config_button);
}

/* shared utility to allocate a title for a form */

static void 
setup_form_title (NautilusServicesContentView* view,
                  const char* title_text) {

	GtkWidget* temp_widget;
	char* file_name;	
	GtkWidget* temp_container;

	temp_container = gtk_hbox_new (FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_container, 0, 0, 4);	
	gtk_widget_show (temp_container);
	
 	file_name = gnome_pixmap_file ("nautilus/eazel-logo.gif");
  	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start (GTK_BOX(temp_container), temp_widget, 0, 0, 8);		
  	gtk_widget_show (temp_widget);
  	g_free (file_name);

 	view->details->form_title = gtk_label_new (title_text);
	/* FIXME bugzilla.eazel.com 667: don't use hardwired font like this */
	nautilus_gtk_widget_set_font_by_name (view->details->form_title,
					      "-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*"); ;
	gtk_box_pack_start (GTK_BOX (temp_container), view->details->form_title, 0, 0, 8);			 	
	gtk_widget_show (view->details->form_title);
}

/* create the signup form */

static void
setup_signup_form(NautilusServicesContentView* view) {

	char* message;
	char* file_name;
	GtkTable* table;
	GtkWidget* temp_widget;
	GtkWidget* temp_box;
	GtkWidget* config_button;
	GtkWidget* config_label;
	
	/* allocate a vbox as the container for the form */	
	view->details->form = gtk_vbox_new (FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);

	/* set up the title */	
	setup_form_title (view, "Eazel Service Registration Form");
	
	/* display an image and a descriptive message */
	/* FIXME bugzilla.eazel.com 727: get the text from a file or from the service */
	temp_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
 	gtk_widget_show (temp_box);
 	file_name = gnome_pixmap_file ("nautilus/register.png");
  	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 8);		
  	gtk_widget_show (temp_widget);
  	g_free (file_name);

	message = "Use this form to register for the Eazel Service, free of charge.  It will give you a storage space on the web that is easily accessed from Nautilus, and access to a customized software catalog that will allow you to install new applications with a single click.";
	temp_widget = gtk_label_new (message);
 	gtk_label_set_line_wrap (GTK_LABEL (temp_widget), TRUE);
	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 12);
 	gtk_widget_show (temp_widget);

	/* allocate a table to hold the signup form */
	
	table = GTK_TABLE (gtk_table_new (2, 3, FALSE));

	/* email address */
  	temp_widget = gtk_label_new ("Your E-mail Address: ");
  	gtk_misc_set_alignment (GTK_MISC (temp_widget), 1.0, 0.5);
  	gtk_table_attach (table, temp_widget, 0,1, 0,1, GTK_FILL, GTK_FILL, 2,2);
  	gtk_widget_show (temp_widget);
  	view->details->account_name = gtk_entry_new_with_max_length (36);
  	gtk_table_attach (table, view->details->account_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4,4);
  	gtk_widget_show (view->details->account_name);

	/* password */
  	temp_widget = gtk_label_new ("Your Password: ");
  	gtk_misc_set_alignment (GTK_MISC (temp_widget), 1.0, 0.5);
  	gtk_table_attach (table, temp_widget, 0,1, 1,2, GTK_FILL, GTK_FILL, 2,2);
  	gtk_widget_show (temp_widget);
  	view->details->account_password = gtk_entry_new_with_max_length (36);
  	gtk_table_attach (table, view->details->account_password, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4,4);
  	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_password), FALSE);
	gtk_widget_show (view->details->account_password);

	/* confirm password */
  	temp_widget = gtk_label_new ("Confirm Password: ");
  	gtk_misc_set_alignment (GTK_MISC (temp_widget), 1.0, 0.5);
  	gtk_table_attach (table, temp_widget, 0,1, 2,3, GTK_FILL, GTK_FILL, 2,2);
  	gtk_widget_show (temp_widget);
  	view->details->confirm_password = gtk_entry_new_with_max_length (36);
  	gtk_table_attach (table, view->details->confirm_password, 1, 2, 2,3, GTK_FILL, GTK_FILL, 4,4);
   	gtk_entry_set_visibility (GTK_ENTRY(view->details->confirm_password), FALSE);
 	gtk_widget_show (view->details->confirm_password);
	
	/* insert the table */
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET(table), 0, 0, 4);	
	gtk_widget_show (GTK_WIDGET(table));	

	/* attach a changed signal to the 3 entry fields, so we can enable the button when something is typed into all 3 fields */
	gtk_signal_connect (GTK_OBJECT (view->details->account_name), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);	
	gtk_signal_connect (GTK_OBJECT (view->details->account_password), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);	
	gtk_signal_connect (GTK_OBJECT (view->details->confirm_password), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);	
	
	/* allocate the command buttons - first the register button */
	
	view->details->register_button = gtk_button_new ();		    
	config_label = gtk_label_new (" Register Now ");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (view->details->register_button), config_label); 	
	
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->register_button, FALSE, FALSE, 16);

	gtk_signal_connect (GTK_OBJECT (view->details->register_button),
                            "clicked",
                            GTK_SIGNAL_FUNC (register_button_cb), view);
	gtk_widget_set_sensitive (view->details->register_button, FALSE);
	gtk_widget_show (view->details->register_button);

	/* now allocate the decline button */
	
	config_button = gtk_button_new ();		    
	config_label = gtk_label_new (" Register Later ");
	gtk_widget_show (config_label);
	gtk_container_add (GTK_CONTAINER (config_button), config_label); 	
	gtk_box_pack_start (GTK_BOX (temp_box), config_button, FALSE, FALSE, 16);
	gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
			    GTK_SIGNAL_FUNC (register_later_cb), view);	
	gtk_widget_show (config_button);

	/* show the buttons */
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 16);

	/* add a label for error messages, but don't show it until there's an error */
	view->details->feedback_text = gtk_label_new ("");	
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);			 	
}

/* create the config form */

static void
setup_config_form (NautilusServicesContentView* view) {

	char* message;
	char* file_name;
	GtkWidget* temp_widget;
	GtkWidget* temp_box;
	GtkWidget* config_button;
	GtkWidget* config_label;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new (FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);

	/* set up the title */	
	setup_form_title (view, "Eazel Service Configuration Gathering");

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

 	file_name = gnome_pixmap_file ("nautilus/config-gather.png");
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

/* create the overview form */

static void
setup_overview_form (NautilusServicesContentView* view) {

	char* message;
	GtkWidget* temp_widget;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new (FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);

	/* set up the title */	
	setup_form_title (view, "Eazel Service Overview");

	/* if we came from signup, add a congrats message */
	/* FIXME bugzilla.eazel.com 727: get the text from a file or from the service */
	
	if (nautilus_str_has_suffix (view->details->uri, "config")) {
		message = "Congratulations, you have successfully transmitted your configuration!";
		temp_widget = gtk_label_new (message);

		gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 12);
 		gtk_widget_show (temp_widget);
	}

	message = "This is a placeholder for the myeazel service summary page.";
	temp_widget = gtk_label_new (message);
 	gtk_label_set_line_wrap (GTK_LABEL (temp_widget), TRUE);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_widget, 0, 0, 12);
 	gtk_widget_show (temp_widget);

	/* add a label for feedback, but don't show it until there's an error */
	view->details->feedback_text = gtk_label_new ("");	
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);			 	

}

/* create the services summary form */


static void
setup_summary_form (NautilusServicesContentView* view) {

	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new (FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show (view->details->form);

	/* set up the title */	
	setup_form_title (view, "Services");
}

static void
nautilus_service_startup_view_initialize_class (NautilusServicesContentViewClass* klass) {

	GtkObjectClass* object_class;
	GtkWidgetClass* widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
 	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_service_startup_view_destroy;
}

static void
nautilus_service_startup_view_initialize (NautilusServicesContentView* view) {

  	NautilusBackground* background;

	view->details = g_new0 (NautilusServicesContentViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (service_main_notify_location_change_cb),
			    view);

  	background = nautilus_get_widget_background (GTK_WIDGET (view));
  	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show_all (GTK_WIDGET (view));
}

static void
nautilus_service_startup_view_destroy (GtkObject* object) {

	NautilusServicesContentView* view;

	view = NAUTILUS_SERVICE_STARTUP_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}

	if (view->details->auth_token) {
		g_free (view->details->auth_token);
	}
	g_free (view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusView *
nautilus_service_startup_view_get_nautilus_view (NautilusServicesContentView* view) {
	return view->details->nautilus_view;
}

/* utility for checking uri */
static gboolean
is_location (char* document_str, const char* place_str) {

	return document_str && !strncmp (document_str + 1, place_str, strlen (place_str));
}

/* URI handling */
void
nautilus_service_startup_view_load_uri (NautilusServicesContentView* view,
				        const char* uri) {

	char* document_name;
	
	/* dispose of the old uri and copy the new one */
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	/* extract the document part of the uri */
	document_name = strchr (uri, ':');

	/* load the appropriate form, based on the uri and the registration state */
	if (is_location (document_name, "signup")) {
		setup_signup_form (view);
	}
	else if (is_location (document_name, "config")) {
		setup_config_form(view);
	}
	else if (is_location(document_name, "overview")) {
		setup_overview_form(view);
	}
	else if (is_location(document_name, "summary")) {
		setup_summary_form(view);
	}
	else {
		setup_test_form(view); /* eventually, this should be setup_bad_location_form */
	}
}

static void
service_main_notify_location_change_cb (NautilusView* view, 
					Nautilus_NavigationInfo* navinfo, 
					NautilusServicesContentView* services) {

	Nautilus_ProgressRequestInfo pri;
	
	memset (&pri, 0, sizeof (pri));
	
	/* we must send a PROGRESS_UNDERWAY message */
	pri.type = Nautilus_PROGRESS_UNDERWAY;
	pri.amount = 0.0;
	nautilus_view_request_progress_change (services->details->nautilus_view, &pri);

	nautilus_service_startup_view_load_uri (services, navinfo->actual_uri);
	
	/* likewise, we must send a PROGRESS_DONE message */
	pri.type = Nautilus_PROGRESS_DONE_OK;
	pri.amount = 100.0;
	nautilus_view_request_progress_change (services->details->nautilus_view, &pri);
}
