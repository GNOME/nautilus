 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>

#include <nautilus-main.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-druid.h>
#include <libnautilus-extensions/nautilus-druid-page-eazel.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>
#include <libnautilus-extensions/nautilus-preferences.h>

#include "nautilus-first-time-druid.h"

#define SERVICE_UPDATE_ARCHIVE_PATH "/tmp/nautilus_update.tgz"
#define NUMBER_OF_STANDARD_PAGES 5

#define USER_LEVEL_PAGE 0
#define SERVICE_SIGNUP_PAGE 1
#define OFFER_UPDATE_PAGE 2
#define UPDATE_FEEDBACK_PAGE 3
#define PROXY_CONFIGURATION_PAGE 4

/* Preference for http proxy settings */
#define GNOME_VFS_PREFERENCES_HTTP_PROXY "/system/gnome-vfs/http-proxy"

static void     initiate_file_download           (GnomeDruid *druid);
static gboolean set_http_proxy                   (const char *proxy_url);
static gboolean attempt_http_proxy_autoconfigure (void);

/* globals */
static NautilusApplication *save_application;
static gboolean save_manage_desktop;

static GtkWidget *start_page;
static GtkWidget *finish_page;
static GtkWidget *pages[NUMBER_OF_STANDARD_PAGES];

static GtkWidget *download_label;

static int last_signup_choice = 0;
static int last_update_choice = 0;
static int last_proxy_choice = 1;

static GtkWidget *port_number_entry;
static GtkWidget *proxy_address_entry;

static void
druid_cancel (GtkWidget *druid)
{
	gtk_widget_destroy(gtk_widget_get_toplevel(druid));
	_exit (0);
}

/* handle the final page finishing  */

static void
druid_set_first_time_file_flag (void)
{
	FILE *stream;
	char *user_directory, *druid_flag_file_name;
	
	user_directory = nautilus_get_user_directory ();
	druid_flag_file_name = g_strdup_printf ("%s/%s",
						user_directory,
						"first-time-wizard-flag");
	g_free (user_directory);

		
	stream = fopen (druid_flag_file_name, "w");
	if (stream) {
		const char *blurb =
			_("Existence of this file indicates that the Nautilus configuration wizard\n"
				"has been presented.\n\n"
				"You can manually erase this file to present the wizard again.\n\n");
			
			fwrite (blurb, sizeof (char), strlen (blurb), stream);
			fclose (stream);
	}
	
	g_free (druid_flag_file_name);
}

static void
druid_finished (GtkWidget *druid_page)
{
	char *user_main_directory;
	const char *signup_uris[2];
	

	user_main_directory = nautilus_get_user_main_directory();

	/* FIXME bugzilla.eazel.com 1555:
	 * Need to post a error dialog if the user's main directory was not created
	 */
	if (!g_file_exists (user_main_directory)) {
		g_print ("FIXME bugzilla.eazel.com 1555\n");
		g_print ("Need to post a error dialog if the user's main directory was not created\n");
	}
	g_free (user_main_directory);

	/* write out the first time file to indicate that we've successfully traversed the druid */
	druid_set_first_time_file_flag ();
	signup_uris[1] = NULL;
	
	switch (last_signup_choice) {
		case 0:
			signup_uris[0] = "http://www.eazel.com/services.html";
			break;
		case 1:
			signup_uris[0] = "http://services.eazel.com:8888/account/register/form";
			break;
		case 2:
			signup_uris[0] = "eazel:";
			break;
		case 3:
		default:
			signup_uris[0]	= NULL;	
			break;
	}
	
	nautilus_application_startup(save_application, FALSE, FALSE, save_manage_desktop, 
					     FALSE, (signup_uris[0] != NULL) ? &signup_uris[0] : NULL);

	/* Destroy druid last because it may be the only thing keeping the main event loop alive. */
	gtk_widget_destroy(gtk_widget_get_toplevel(druid_page));
}

/* set up an event box to serve as the background */

static GtkWidget*
set_up_background (NautilusDruidPageEazel *page, const char *background_color)
{
	GtkWidget *event_box;
	NautilusBackground *background;
	
	event_box = gtk_event_box_new();
	
	background = nautilus_get_widget_background (event_box);
	nautilus_background_set_color (background, background_color);
	
	gtk_widget_show (event_box);

	nautilus_druid_page_eazel_put_widget (page, event_box);

	return event_box;
}

/* handler for user level buttons changing */
static void
user_level_selection_changed (GtkWidget *radio_button, gpointer user_data)
{
	int user_level = GPOINTER_TO_INT (user_data);

	if (GTK_TOGGLE_BUTTON (radio_button)->active) {
		nautilus_user_level_manager_set_user_level (user_level);
	}
}

/* handler for signup buttons changing */
static void
signup_selection_changed (GtkWidget *radio_buttons, gpointer user_data)
{

	last_signup_choice = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons));
}

static void
update_selection_changed (GtkWidget *radio_buttons, gpointer user_data)
{

	last_update_choice = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons));
}

static void
proxy_selection_changed (GtkWidget *radio_buttons, gpointer user_data)
{

	last_proxy_choice = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons));
}


/* utility to allocate an anti-aliased description label */

static GtkWidget*
make_anti_aliased_label (const char *text)
{
	GtkWidget *label;
	
	label = nautilus_label_new (text);

	nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "medium", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 14);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (label),
			       GTK_JUSTIFY_LEFT);

	return label;
}

static GdkPixbuf*
create_named_pixbuf (const char *name) 
{
	GdkPixbuf	*pixbuf;
	char		*path;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	path = nautilus_pixmap_file (name);

	if (path == NULL) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	return pixbuf;
}

static GtkWidget *
make_hbox_user_level_radio_button (int index, GtkWidget *radio_buttons[],
				   const char *icon_name,
				   const char *comment, const char *background)
{
	GtkWidget *hbox, *label_box, *icon, *label, *alignment;
	GtkWidget *comment_vbox, *comment_hbox;
	GdkPixbuf *icon_pixbuf;
	char *user_level_name;

	hbox = gtk_hbox_new (FALSE, 0);

	user_level_name = nautilus_user_level_manager_get_user_level_name_for_display (index);

	/* Add an "indent" */
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_widget_set_usize (alignment, 50, -1);
	gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);

	/* make new box for radiobutton/comment */
	comment_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), comment_vbox, FALSE, FALSE, 0);

	/* Make new radio button */
	if (index > 0) {
		radio_buttons[index] = gtk_radio_button_new_from_widget
			(GTK_RADIO_BUTTON (radio_buttons[0]));
	} else {
		radio_buttons[0] = gtk_radio_button_new (NULL);
	}
	gtk_box_pack_start (GTK_BOX (comment_vbox), radio_buttons[index], FALSE, FALSE, 0);

	/* Make label */
	label_box = gtk_hbox_new (FALSE, 5);

	icon_pixbuf = create_named_pixbuf (icon_name);
	icon = nautilus_image_new ();
	if (icon_pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (icon), icon_pixbuf);
	}
	gtk_box_pack_start (GTK_BOX (label_box), icon, FALSE, FALSE, 0);
	label = make_anti_aliased_label (user_level_name);
	g_free (user_level_name);

	nautilus_label_set_font_from_components (NAUTILUS_LABEL (label), "helvetica", "bold", NULL, NULL);

	gtk_box_pack_start (GTK_BOX (label_box), label, FALSE, FALSE, 0);

	/* add label to radio button */
	gtk_container_add (GTK_CONTAINER (radio_buttons[index]), label_box);

	/* make new hbox for comment */
	comment_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (comment_vbox), comment_hbox, FALSE, FALSE, 0);

	/* Add an "indent" */
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_widget_set_usize (alignment, 50, -1);
	gtk_box_pack_start (GTK_BOX (comment_hbox), alignment, FALSE, FALSE, 0);

	/* Make comment label */
	label = make_anti_aliased_label (comment);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 12);

	gtk_box_pack_start (GTK_BOX (comment_hbox), label, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);
	return hbox;
}

/* set up the user level page */
static void
set_up_user_level_page (NautilusDruidPageEazel *page)
{
	GtkWidget *radio_buttons[3], *label;
	GtkWidget *container, *main_box, *hbox;
	int user_level, index;

	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 0);

	label = make_anti_aliased_label (_("User levels provide a way to adjust the software to\nyour level of technical expertise.  Pick an initial level that you\nfeel comfortable with; you can always change it later."));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 12);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	/* Make the user level radio buttons and fill the radio_buttons
	 * array */
	hbox = make_hbox_user_level_radio_button
		(NAUTILUS_USER_LEVEL_NOVICE, radio_buttons, "novice.png",
		 _("For beginner users that are not yet\n"
		   "familiar with the working of "
		   "GNOME and Linux."),
		 NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 4);
	hbox = make_hbox_user_level_radio_button
		(NAUTILUS_USER_LEVEL_INTERMEDIATE, radio_buttons, "intermediate.png",
		 _("For non-technical users that are comfortable with\n"
		   "their GNOME and Linux environment."),
				       NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 4);
	hbox = make_hbox_user_level_radio_button
		(NAUTILUS_USER_LEVEL_HACKER, radio_buttons, "expert.png",
		 _("For users that have the need to be exposed\n"
		   "to every detail of their operating system."),
		 NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 4);

	user_level = nautilus_user_level_manager_get_user_level ();
	g_assert (user_level >= NAUTILUS_USER_LEVEL_NOVICE && user_level <= NAUTILUS_USER_LEVEL_HACKER);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_buttons[user_level]), TRUE);


	for (index = NAUTILUS_USER_LEVEL_NOVICE; index <= NAUTILUS_USER_LEVEL_HACKER; index ++) {
		gtk_signal_connect (GTK_OBJECT (radio_buttons[index]),
				    "toggled",
				    GTK_SIGNAL_FUNC (user_level_selection_changed),
				    GINT_TO_POINTER (index));
	}

	gtk_widget_show_all (main_box);
}

/* set up the user level page */
static void
set_up_service_signup_page (NautilusDruidPageEazel *page)
{
	GtkWidget *radio_buttons, *frame, *label;
	GtkWidget *container, *main_box;

	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = make_anti_aliased_label (_("Eazel offers a growing number of services to \nhelp you install and maintain new software and manage \nyour files across the network.  If you want to find out more \nabout Eazel services, just press the 'Next' button. "));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("Eazel Services"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("I want to learn more about Eazel services."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("I want to sign up for Eazel services now."));	
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("I've already signed up and want to login now."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("I don't want to learn about Eazel services at this time."));

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (signup_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}

/* set up the "Nautilus Update" page */
static void
set_up_update_page (NautilusDruidPageEazel *page)
{
	GtkWidget *radio_buttons, *frame, *label;
	GtkWidget *container, *main_box;

	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = make_anti_aliased_label (_("Nautilus will now contact Eazel services to verify your \nweb connection and download the latest updates.  \nClick the Next button to continue."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("Updating Nautilus"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Yes, verify my web connection and update Nautilus now."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("No, I don't wish to connect and update Nautilus now."));	

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (update_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}

/* set up the proxy configuration page */
static void
set_up_proxy_config_page (NautilusDruidPageEazel *page)
{
	GtkWidget *radio_buttons;
	GtkWidget *frame, *label;
	GtkWidget *container, *main_box;
	GtkWidget *vbox, *hbox;
	GtkWidget *alignment;
	int proxy_label_width;
	
	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = make_anti_aliased_label (_("We are having troubles making an external web connection.  \nSometimes, firewalls require you to specify a web proxy server.  \n Fill in the name of port of your proxy server, if any, below."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("HTTP Proxy Configuration"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 12);

	/* allocate a pair of radio buttons */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame),
					vbox);

	radio_buttons = nautilus_radio_button_group_new (FALSE);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("No proxy server required."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Use this proxy server:"));	
	
	nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 1);

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (proxy_selection_changed),
			    (gpointer) NULL);

	gtk_widget_show (radio_buttons);	

	gtk_box_pack_start (GTK_BOX (vbox), radio_buttons, FALSE, FALSE, 2);

	/* allocate the proxy name entry */
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);

	/* allocate an alignment width to indent */
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_widget_set_usize (alignment, 24, -1);
	gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);
	
	/* allocate the proxy label, followed by the entry */
	label = nautilus_label_new (_("Proxy address:"));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 12);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
	proxy_label_width = label->allocation.width;
	proxy_address_entry = gtk_entry_new_with_max_length (24);
	gtk_widget_set_usize (proxy_address_entry, 180, -1);
	gtk_widget_show (proxy_address_entry);
	gtk_box_pack_start (GTK_BOX (hbox), proxy_address_entry, FALSE, FALSE, 2);
	
	/* allocate the port number entry */
	
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);
	gtk_widget_set_usize (alignment, 8, -1);
	
	/* allocate the proxy label, followed by the entry */
	label = nautilus_label_new (_("Port:"));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 12);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

	port_number_entry = gtk_entry_new_with_max_length (5);
	gtk_widget_set_usize (port_number_entry, 48, -1);
	gtk_widget_show (port_number_entry);
	gtk_box_pack_start (GTK_BOX (hbox), port_number_entry, FALSE, FALSE, 2);
	
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);


}

/* set up the update feedback page */
static void
set_up_update_feedback_page (NautilusDruidPageEazel *page)
{
	GtkWidget *label;
	GtkWidget *container, *main_box;

	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = make_anti_aliased_label (_("We are now contacting the Eazel service to test your \nweb connection and update Nautilus."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
		
		
	download_label = nautilus_label_new (_("Downloading Nautilus updates..."));
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (download_label), "helvetica", "medium", NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (download_label), 18);
	
	gtk_widget_show (download_label);
	
	gtk_box_pack_start (GTK_BOX (main_box), download_label, FALSE, FALSE, 2);
}

/* handle the "next" signal for the update page based on the user's choice */
static gboolean
next_update_page_callback (GtkWidget *button, GnomeDruid *druid)
{
	if (last_update_choice == 0) {
		/* initiate the file transfer and launch a timer task to track feedback */
		initiate_file_download (druid);
		
		/* return FALSE to display the feedback page */
		return FALSE;
	}

	/* the user declined to update, so skip the feedback page and go directly to finish */
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (finish_page));
	return TRUE;
}

/* handle the "next" signal for the update feedback page to skip the error page */
static gboolean
next_update_feedback_page_callback (GtkWidget *button, GnomeDruid *druid)
{
	/* skip the error page by going write to the finish line */
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (finish_page));
	return TRUE;
}

/* handle the "next" signal for the update feedback page to skip the error page */
static gboolean
next_proxy_configuration_page_callback (GtkWidget *button, GnomeDruid *druid)
{
	char *proxy_string;

	proxy_string = g_strdup_printf ("http://%s:%s", gtk_entry_get_text (GTK_ENTRY(proxy_address_entry)), gtk_entry_get_text (GTK_ENTRY(port_number_entry)));
	set_http_proxy (proxy_string);
	g_free (proxy_string);
	
	/* now, go back to the offer update page or finish, depending on the user's selection */
	if (last_proxy_choice == 1) {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (pages[OFFER_UPDATE_PAGE]));
	} else {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (finish_page));
	}
	
	return TRUE;
}

/* handle the "back" signal from the finish page to skip the feedback page */
static gboolean
finish_page_back_callback (GtkWidget *button, GnomeDruid *druid)
{
	gnome_druid_set_page (druid, GNOME_DRUID_PAGE (pages[OFFER_UPDATE_PAGE]));
	return TRUE;
}

/* create the initial preferences druid */

static void
set_page_sidebar (NautilusDruidPageEazel *page)
{
	GdkPixbuf *pixbuf;

	pixbuf = create_named_pixbuf ("druid_sidebar.png");

	if (pixbuf != NULL) {
		nautilus_druid_page_eazel_set_sidebar_image (page, pixbuf);
		gdk_pixbuf_unref (pixbuf);
	}
}

GtkWidget *nautilus_first_time_druid_show (NautilusApplication *application, gboolean manage_desktop, const char *urls[])
{	
	GtkWidget *dialog;
	GtkWidget *druid;
	int i;
	GtkWidget *container, *main_box, *label;
	
	/* remember parameters for later window invocation */
	save_application = application;
	save_manage_desktop = manage_desktop;
	
	dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (dialog),
			      _("Initial Preferences"));
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), 0);
  	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);
  	/* Ensure there's a main event loop while the druid is running. */
  	nautilus_main_event_loop_register (GTK_OBJECT (dialog));


	druid = nautilus_druid_new ();
  	gtk_container_set_border_width (GTK_CONTAINER (druid), 0);

	start_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_START);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (start_page));
	finish_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_FINISH);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (finish_page));

	for (i = 0; i < NUMBER_OF_STANDARD_PAGES; i++) {
		pages[i] = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_OTHER);
		set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (pages[i]));
	}
		
	/* set up the initial page */

	/* allocate the description using a nautilus_label to get anti-aliased text */
	container = set_up_background (NAUTILUS_DRUID_PAGE_EAZEL (start_page), "rgb:ffff/ffff/ffff:h");
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* make the title label */
	label = make_anti_aliased_label ( _("Welcome to Nautilus!"));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 28);
	nautilus_label_set_drop_shadow_offset (NAUTILUS_LABEL (label), 2);
	nautilus_label_set_drop_shadow_color (NAUTILUS_LABEL (label), NAUTILUS_RGBA_COLOR_PACK (191, 191, 191, 255));
	
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 16);
	
	label = make_anti_aliased_label ( _("Since this is the first time that you've launched\nNautilus, we'd like to ask you a few questions\nto help personalize it for your use."));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	label = make_anti_aliased_label ( _("Press the next button to continue."));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 16);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	
	/* set up the final page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), _("Finished"));

	container = set_up_background (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), "rgb:ffff/ffff/ffff:h");
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	label = make_anti_aliased_label ( _("Click the finish button to launch Nautilus.\nWe hope that you enjoy using it!"));
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 18);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
		
	/* set up the user level page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[USER_LEVEL_PAGE]), _("Select A User Level"));
	set_up_user_level_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[USER_LEVEL_PAGE]));
				
	/* set up the service sign-up page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[SERVICE_SIGNUP_PAGE]), _("Sign Up for Eazel Services"));
	set_up_service_signup_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[SERVICE_SIGNUP_PAGE]));

	/* set up the update page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[OFFER_UPDATE_PAGE]), _("Nautilus Update"));
	set_up_update_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[OFFER_UPDATE_PAGE]));

	gtk_signal_connect (GTK_OBJECT (pages[OFFER_UPDATE_PAGE]), "next",
			    GTK_SIGNAL_FUNC (next_update_page_callback),
			    druid);

	/* set up the update feedback page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[UPDATE_FEEDBACK_PAGE]), _("Updating Nautilus..."));
	set_up_update_feedback_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[UPDATE_FEEDBACK_PAGE]));

	gtk_signal_connect (GTK_OBJECT (pages[UPDATE_FEEDBACK_PAGE]), "next",
			    GTK_SIGNAL_FUNC (next_update_feedback_page_callback),
			    druid);

	/* set up the (optional) proxy configuration page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[PROXY_CONFIGURATION_PAGE]), _("Web Proxy Configuration"));
	set_up_proxy_config_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[PROXY_CONFIGURATION_PAGE]));

	gtk_signal_connect (GTK_OBJECT (pages[PROXY_CONFIGURATION_PAGE]), "next",
			    GTK_SIGNAL_FUNC (next_proxy_configuration_page_callback),
			    druid);
	gtk_signal_connect (GTK_OBJECT (pages[PROXY_CONFIGURATION_PAGE]), "back",
			    GTK_SIGNAL_FUNC (finish_page_back_callback),
			    druid);


	/* capture the "back" signal from the finish page to skip the feedback page */
	gtk_signal_connect (GTK_OBJECT (finish_page), "back",
			    GTK_SIGNAL_FUNC (finish_page_back_callback),
			    druid);
		
	/* append all of the pages to the druid */
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (start_page));
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[0]));
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[1]));
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[2]));
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[3]));
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[4]));
	
	gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (finish_page));

	gtk_container_add (GTK_CONTAINER (dialog), druid);

	/* set up the signals */
  	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
                      GTK_SIGNAL_FUNC (druid_cancel),
                      NULL);
	gtk_signal_connect (GTK_OBJECT (finish_page), "finish",
                      GTK_SIGNAL_FUNC (druid_finished),
                      NULL);

	gtk_widget_show_all (druid);

	gtk_widget_show (GTK_WIDGET (dialog));
	return druid;
}

/* download_callback is invokes when the file is completely downloaded, to write it out and expand it */

/* callback to handle the asynchronous reading of icons */
static void
download_callback (GnomeVFSResult result,
		   GnomeVFSFileSize file_size,
		   char *file_contents,
		   gpointer callback_data)
{
	char *command_str, *user_directory_path;
	int size, write_result, expand_result;
	FILE* outfile;
	GnomeDruid *druid;
	
	druid = GNOME_DRUID (callback_data);
	
	/* check for errors */
	if (result == GNOME_VFS_OK) {
		/* there was no error, so write out the file into the /tmp directory */
		size = file_size;
		outfile = fopen (SERVICE_UPDATE_ARCHIVE_PATH, "wb");	 	
		write_result = fwrite (file_contents, size, 1, outfile);
		fclose (outfile);
		g_free (file_contents);
		
		/* change the message to expanding file */
		nautilus_label_set_text (NAUTILUS_LABEL (download_label), _("Decoding Update..."));
		
		/* expand the directory into the proper place */
		/* first, formulate the command string */
		user_directory_path = nautilus_get_user_directory ();
		command_str = g_strdup_printf ("cd %s; tar xvfz %s >/dev/null", user_directory_path, SERVICE_UPDATE_ARCHIVE_PATH);
		
		/* execute the command to make the updates folder */
		expand_result = system (command_str);
		nautilus_label_set_text (NAUTILUS_LABEL (download_label), _("Update Completed... Press Next to Continue."));
		
		g_free (user_directory_path);
		g_free (command_str);	
	
		/* now that we're done, reenable the buttons */
		gtk_widget_set_sensitive (druid->next, TRUE);
		gtk_widget_set_sensitive (druid->back, TRUE);
	} else {
		/* there was an error; see if we can't find some HTTP proxy config info */
		/* note that attempt_http_proxy_autoconfigure() returns FALSE if its already been tried */ 
		if (attempt_http_proxy_autoconfigure()) {
			initiate_file_download (druid);
		} else {
			/* Autoconfiguration didn't work; prompt the user */
			gnome_druid_set_page (druid, GNOME_DRUID_PAGE (pages[PROXY_CONFIGURATION_PAGE]));	
		}
	}
}

/* initiate downloading of the welcome package from the service */
/* Note that this may be invoked more than once, if the first time fails */

static void
initiate_file_download (GnomeDruid *druid)
{
	NautilusReadFileHandle *file_handle;

	/* FIXME bugzilla.eazel.com 1826: for testing; this needs to move to the real service */
	const char *file_uri = "http://www.eazel.com/software/nautilus/updates.tgz";

	/* disable the next and previous buttons during the  file loading process */
	gtk_widget_set_sensitive (druid->next, FALSE);
	gtk_widget_set_sensitive (druid->back, FALSE);
			
	/* initiate the file transfer */
	file_handle = nautilus_read_entire_file_async (file_uri, download_callback, druid);
}

/**
 * set_http_proxy
 * 
 * sets the HTTP proxy preference to the "http://host:port" style string proxy_url
 * also sets "http_proxy" environment variable
 */
static gboolean
set_http_proxy (const char *proxy_url)
{
	const char *proxy_url_port_part;
	size_t proxy_len;
	char *proxy_host_port;

	/* set the "http_proxy" environment variable */

	nautilus_setenv ("http_proxy", proxy_url, TRUE);

	/* The variable is expected to be in the form host:port */
	if (0 != strncmp (proxy_url, "http://", strlen ("http://"))) {
		return FALSE;
	}

	/* Chew off a leading http:// */
	proxy_url_port_part = strlen ("http://") + proxy_url;
	proxy_len = strlen (proxy_url_port_part);
	if (0 == proxy_len) {
		return FALSE;
	}

	/* chew off trailing / */
	proxy_host_port = g_strdup (proxy_url_port_part);
	if ('/' == proxy_host_port[proxy_len - 1]) {
		proxy_host_port[proxy_len - 1] = 0;
	}

	nautilus_preferences_set (GNOME_VFS_PREFERENCES_HTTP_PROXY, proxy_host_port);

	g_free (proxy_host_port);

	return TRUE;
}

/**
 * getline_dup
 *
 * reads newline or EOF terminated line from stream, allocating the return
 * buffer as appropriate
 */ 
#define GETLINE_INITIAL 256
static char * 
getline_dup (FILE* stream)
{
	char *ret;
	size_t ret_size;
	size_t ret_used;
	int char_read;
	gboolean done;

	ret = g_malloc( GETLINE_INITIAL * sizeof(char) );
	ret_size = GETLINE_INITIAL;

	for ( ret_used = 0, done = FALSE ;
	      !done && (EOF != (char_read = fgetc (stream))) ; 
	      ret_used++
	) {
		if (ret_size == (ret_used + 1)) {
			ret_size *= 2;
			ret = g_realloc (ret, ret_size); 
		}
		if ('\n' == char_read || '\r' == char_read ) {
			done = TRUE;
			ret [ret_used] = '\0';
		} else {
			ret [ret_used] = char_read;
		}
	}

	if ( 0 == ret_used ) {
		g_free (ret);
		ret = NULL;
	} else {
		ret [ret_used] = '\0';
	}

	return ret;
}

#define NETSCAPE_PREFS_PATH "/.netscape/preferences.js"

/* user_pref("network.proxy.http", "localhost");
 * user_pref("network.proxy.http_port", 8080);
 * user_pref("network.proxy.type", 1);
 */
static char *
load_nscp_proxy_settings ()
{
	char * user_directory = NULL;
	char * prefs_path = NULL;
	char * ret = NULL;
	char * proxy_host = NULL;
	guint32 proxy_port = 8080;
	gboolean has_proxy_type = FALSE;

	char * line;
	char * current, *end;
	FILE * prefs_file;

	user_directory = g_get_home_dir ();

	prefs_path = g_strconcat (user_directory, NETSCAPE_PREFS_PATH, NULL);
	g_free (user_directory);
	user_directory = NULL;

	prefs_file = fopen (prefs_path, "r");

	if ( NULL == prefs_file ) {
		goto error;
	}

	/* Normally I wouldn't be caught dead doing it this way...*/
	for ( ; NULL != (line = getline_dup (prefs_file)) ; g_free (line) ) {
		if ( NULL != (current = strstr (line, "\"network.proxy.http\"")) ) {
			current += strlen ("\"network.proxy.http\"");

			current = strchr (current, '"');

			if (NULL == current) {
				continue;
			}
			current++;

			end = strchr (current, '"');
			if (NULL == end) {
				continue;
			}

			proxy_host = g_strndup (current, end-current);
		} else if ( NULL != (current = strstr (line, "\"network.proxy.http_port\""))) {
			current += strlen ("\"network.proxy.http_port\"");

			while ( *current && !isdigit(*current)) {
				current++;
			}

			if ( '\0' == *current ) {
				continue;
			}

			proxy_port = strtoul (current, &end, 10);

		} else if ( NULL != (current = strstr (line, "\"network.proxy.type\""))) {
			/* Proxy type must equal '1' */
			current += strlen ("\"network.proxy.type\"");

			while ( *current && !isdigit(*current)) {
				current++;
			}

			has_proxy_type = ('1' == *current);
		}
	}

	if (has_proxy_type && NULL != proxy_host) {
		ret = g_strdup_printf ("http://%s:%u", proxy_host, proxy_port);
	}
	
error:
	g_free (proxy_host);
	g_free (prefs_path);
	prefs_path = NULL;

	return ret;
}

/**
 * attempt_http_proxy_autoconfigure
 *
 * Attempt to discover HTTP proxy settings from environment variables
 * and Netscape 4.x configuation files
 */
static gboolean
attempt_http_proxy_autoconfigure (void)
{
	static gboolean autoconfigure_attempted = FALSE;
	gboolean success = FALSE;
	char * proxy_url;

	/* If we've already failed once, we're not going to try again */
	if (autoconfigure_attempted) {
		return FALSE;
	}
	
	/* The "http_proxy" environment variable is used by libwww */

	/* Note that g_getenv returns a pointer to a static buffer */
	proxy_url = g_getenv ("http_proxy");

	if (NULL != proxy_url) {
		success = TRUE;
		set_http_proxy (proxy_url);
		g_free (proxy_url);
		proxy_url = NULL;
	}

	/* Check Netscape 4.x settings */

	proxy_url = load_nscp_proxy_settings ();

	if (NULL != proxy_url) {
		success = TRUE;
		set_http_proxy (proxy_url);
		g_free (proxy_url);
		proxy_url = NULL;
	}

	autoconfigure_attempted = TRUE;
	return success;
}

