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
#include "nautilus-first-time-druid.h"

#include <ctype.h>
#include <dirent.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-druid-page-eazel.h>
#include <libnautilus-extensions/nautilus-druid.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-preferences.h>
#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <nautilus-main.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIZE_BODY_LABEL(l)  nautilus_label_make_smaller (NAUTILUS_LABEL (l), 1)

#define SERVICE_UPDATE_ARCHIVE_PATH "/tmp/nautilus_update.tgz"
#define WELCOME_PACKAGE_URI "http://services.eazel.com/downloads/eazel/updates.tgz"

/* Wizard page number enumeration */
enum {
	USER_LEVEL_PAGE = 0,
	GMC_TRANSITION_PAGE,
	SERVICE_SIGNUP_PAGE,
	OFFER_UPDATE_PAGE,
	UPDATE_FEEDBACK_PAGE,
	PROXY_CONFIGURATION_PAGE,
	NUMBER_OF_STANDARD_PAGES	/* This must be the last item in the enumeration. */
};

/* Preference for http proxy settings */
#define DEFAULT_HTTP_PROXY_PORT 8080
#define DEFAULT_HTTP_PROXY_PORT_STRING "8080"
#define GNOME_VFS_PREFERENCES_HTTP_PROXY_HOST "/system/gnome-vfs/http-proxy-host"
#define GNOME_VFS_PREFERENCES_HTTP_PROXY_PORT "/system/gnome-vfs/http-proxy-port"
#define GNOME_VFS_PREFERENCES_USE_HTTP_PROXY "/system/gnome-vfs/use-http-proxy"

#define READ_FILE_HANDLE_TAG "Nautilus first time druid read file handle"

/* The number of seconds we'll wait for our experimental DNS resolution */
#define NETWORK_CHECK_TIMEOUT_SECONDS 15
#define DEFAULT_DNS_CHECK_HOST "services.eazel.com"

#define GALEON_PREFS_PATH "/.gnome/galeon"

#define GETLINE_INITIAL_BUFFER_SIZE 256

#define NETSCAPE_PREFS_PATH "/.netscape/preferences.js"

#define EAZEL_SERVICES_LEARN_MORE_URL "http://services.eazel.com/services"
#define EAZEL_SERVICES_SIGN_UP_URL "http://services.eazel.com/account/register/form"
#define EAZEL_SERVICES_LOG_IN_URL "eazel:"


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

/* Set by set_http_proxy; used by check_network_connectivity */

/* NULL indicates no HTTP proxy */
static char *http_proxy_host = NULL;

/* The result of the last check_network_connectivity call */
static enum {
	Untested,
	Success,
	Fail
} network_status = Untested;


/* Globals used to implement DNS timeout. */
static gboolean sigalrm_occurred;
static pid_t child_pid;

/* GMC transition tool globals */
static gboolean draw_desktop = TRUE;
static gboolean add_to_session = TRUE;
static gboolean transfer_gmc_icons = TRUE;

static void     initiate_file_download           (GnomeDruid 	*druid);
static gboolean set_http_proxy                   (const char 	*proxy_url);
static gboolean attempt_http_proxy_autoconfigure (void);
static gboolean check_network_connectivity	 (void);
static void	convert_gmc_desktop_icons 	 (void);

static void
druid_cancel (GtkWidget *druid)
{
	gtk_widget_destroy (gtk_widget_get_toplevel (druid));

	/* FIXME bugzilla.eazel.com 5050: Why _exit instead of a plain exit? It might be OK
	 * to do nothing here now that Nautilus knows to quit when
	 * windows go away.
	 */
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
	char *user_main_directory, *desktop_path;
	const char *signup_uris[2];
	
	
	/* Hide druid so we don't have a blocked dialog visible while we process the startup tasks. */
	gtk_widget_hide_all (gtk_widget_get_toplevel (druid_page));
	
	user_main_directory = nautilus_get_user_main_directory ();

	g_free (user_main_directory);

	/* write out the first time file to indicate that we've successfully traversed the druid */
	druid_set_first_time_file_flag ();

	/* Here we check to see if we can resolve hostnames in a timely
	 * fashion. If we can't then we silently tell nautilus to start up
	 * pointing to the home directory and not any of the HTTP addresses--
	 * we don't want Nautilus to hang indefinitely trying to resolve
	 * an HTTP address.
	 */
	/* FIXME bugzilla.eazel.com 5051: Perhaps we can fix the underlying problem instead of
	 * having this hack here to guess whether the network is broken.
	 */
	if (Untested == network_status
	    && (last_signup_choice == 0 || last_signup_choice == 1)) {
		check_network_connectivity ();
	}
	if (Fail == network_status) {
		last_signup_choice = 3;
	}
	
	/* Choose the URL based on the signup choice. */
	switch (last_signup_choice) {
	case 0:
		signup_uris[0] = EAZEL_SERVICES_LEARN_MORE_URL;
		break;
	case 1:
		signup_uris[0] = EAZEL_SERVICES_SIGN_UP_URL;
		break;
	case 2:
		signup_uris[0] = EAZEL_SERVICES_LOG_IN_URL;
		break;
	case 3:
	default:
		signup_uris[0]	= NULL;	
		break;
	}
	signup_uris[1] = NULL;
	
	
	/* Do the GMC to Nautilus Transition */
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP, draw_desktop);	
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_ADD_TO_SESSION, add_to_session);	
	if (transfer_gmc_icons) {
		convert_gmc_desktop_icons ();
	}
	
	/* Create default services icon on the desktop */
	desktop_path = nautilus_get_desktop_directory ();
	nautilus_link_local_create (desktop_path, _("Eazel Services"), "hand.png", 
				    "eazel:", NULL, NAUTILUS_LINK_GENERIC);
	g_free (desktop_path);
	
	/* Time to start. Hooray! */
	nautilus_application_startup (save_application, FALSE, FALSE, save_manage_desktop, 
				      FALSE, FALSE, NULL, (signup_uris[0] != NULL) ? signup_uris : NULL);
	
	/* Destroy druid last because it may be the only thing keeping the main event loop alive. */
	gtk_widget_destroy (gtk_widget_get_toplevel (druid_page));
}

/* set up an event box to serve as the background */

static GtkWidget *
set_up_background (NautilusDruidPageEazel *page, const char *background_color)
{
	GtkWidget *event_box;
	NautilusBackground *background;
	
	event_box = gtk_event_box_new ();
	
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
		nautilus_preferences_set_user_level (user_level);
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
label_new_left_justified (const char *text)
{
	GtkWidget *label;
	
	label = nautilus_label_new (text);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);

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
	GtkWidget *hbox, *vbox;
	GtkWidget *label_box, *icon, *label, *alignment;
	GtkWidget *comment_vbox, *comment_hbox;
	GdkPixbuf *icon_pixbuf;
	char *user_level_name;

	hbox = gtk_hbox_new (FALSE, 0);

	user_level_name = nautilus_preferences_get_user_level_name_for_display (index);

	/* Add an "indent" */
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
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
	icon = nautilus_image_new (NULL);
	if (icon_pixbuf != NULL) {
		nautilus_image_set_pixbuf (NAUTILUS_IMAGE (icon), icon_pixbuf);
	}
	gtk_box_pack_start (GTK_BOX (label_box), icon, FALSE, FALSE, 0);
	label = label_new_left_justified (user_level_name);
	g_free (user_level_name);

	nautilus_label_make_bold (NAUTILUS_LABEL (label));
	SIZE_BODY_LABEL (label);

	/* extra vbox to help with alignment */
	vbox = gtk_vbox_new (FALSE, 0);
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_widget_set_usize (alignment, -1, 6);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (label_box), vbox, FALSE, FALSE, 0);

	/* add label to radio button */
	gtk_container_add (GTK_CONTAINER (radio_buttons[index]), label_box);

	/* make new hbox for comment */
	comment_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (comment_vbox), comment_hbox, FALSE, FALSE, 0);

	/* Add an "indent" */
	alignment = gtk_alignment_new (1.0, 1.0, 1.0, 1.0);
	gtk_widget_set_usize (alignment, 50, -1);
	gtk_box_pack_start (GTK_BOX (comment_hbox), alignment, FALSE, FALSE, 0);

	/* Make comment label */
	label = label_new_left_justified (comment);
	SIZE_BODY_LABEL (label);

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

	label = label_new_left_justified (_("User levels provide a way to adjust the software to your\n"
					   "level of technical expertise. Pick an initial level that you\n"
					   "feel comfortable with; you can always change it later."));

	SIZE_BODY_LABEL (label);

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
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 2);
	hbox = make_hbox_user_level_radio_button
		(NAUTILUS_USER_LEVEL_INTERMEDIATE, radio_buttons, "intermediate.png",
		 _("For non-technical users that are comfortable with\n"
		   "their GNOME and Linux environment."),
				       NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 2);
	hbox = make_hbox_user_level_radio_button
		(NAUTILUS_USER_LEVEL_ADVANCED, radio_buttons, "expert.png",
		 _("For users that have the need to be exposed\n"
		   "to every detail of their operating system."),
		 NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 2);

	user_level = nautilus_preferences_get_user_level ();
	g_assert (user_level >= NAUTILUS_USER_LEVEL_NOVICE && user_level <= NAUTILUS_USER_LEVEL_ADVANCED);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_buttons[user_level]), TRUE);


	for (index = NAUTILUS_USER_LEVEL_NOVICE; index <= NAUTILUS_USER_LEVEL_ADVANCED; index ++) {
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
	label = label_new_left_justified (_("Eazel offers a growing number of services to help you\n"
					   "install and maintain new software and manage your files\n"
					   "across the network.  Choose an option below, and the\n"
					   "information will be presented in Nautilus after you've\n"
					   "finished setting up."));

	SIZE_BODY_LABEL (label);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("Eazel Services"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons),
					    _("I want to learn more about Eazel services."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons),
					    _("I want to sign up for Eazel services now."));	
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons),
					    _("I've already signed up and want to login now."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons),
					    _("I don't want to learn about Eazel services at this time."));

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
	label = label_new_left_justified (_("Nautilus will now contact Eazel services to quickly verify \nyour web connection and download the latest updates. \n\nIf you know your computer uses a proxy connection, \nclick Yes and Nautilus will use it. \n\nClick the Next button to continue."));
	SIZE_BODY_LABEL (label);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("Updating Nautilus"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Yes, verify my connection and update Nautilus now."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("No, don't contact Eazel services at this time."));	

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (update_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}


static gint
proxy_address_entry_key_press (GtkWidget	*widget,
                 	       GdkEventKey	*event,
                 	       gpointer		callback_data)
{
	char *keysym_name;
	GtkWidget *focus_target;

	focus_target = GTK_WIDGET (callback_data);

	g_return_val_if_fail (NULL != focus_target, FALSE);
	
	keysym_name = gdk_keyval_name (event->keyval);

	if (strcmp (keysym_name, "Tab") == 0) {
		gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");

		gtk_widget_grab_focus (focus_target);

		if (GTK_IS_EDITABLE (focus_target)) {
			gtk_editable_select_region (GTK_EDITABLE (focus_target), 0, -1);
		}

		return TRUE;
	}

	return FALSE;
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
	label = label_new_left_justified (_("We are having troubles making an external web connection.  \nSometimes, firewalls require you to specify a web proxy server.  \n Fill in the name of port of your proxy server, if any, below."));
	SIZE_BODY_LABEL (label);
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
	gtk_widget_show (label);
	SIZE_BODY_LABEL (label);
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
	SIZE_BODY_LABEL (label);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);

	port_number_entry = gtk_entry_new_with_max_length (5);
	gtk_entry_set_text (GTK_ENTRY (port_number_entry), DEFAULT_HTTP_PROXY_PORT_STRING);
	
	gtk_widget_set_usize (port_number_entry, 48, -1);
	gtk_widget_show (port_number_entry);
	gtk_box_pack_start (GTK_BOX (hbox), port_number_entry, FALSE, FALSE, 2);
	
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);


	/* Slam it so that the <tab> in "Proxy Address" goes to "Port"
	 * I have no idea why this doesn't just work
	 */

	gtk_signal_connect (GTK_OBJECT (proxy_address_entry),
			    "key_press_event",
			    GTK_SIGNAL_FUNC (proxy_address_entry_key_press),
			    port_number_entry);

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
	label = label_new_left_justified (_("We are now contacting the Eazel service to test your \nweb connection and update Nautilus."));
	SIZE_BODY_LABEL (label);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
		
		
	download_label = nautilus_label_new (_("Downloading Nautilus updates..."));
	nautilus_label_make_larger (NAUTILUS_LABEL (download_label), 4);
	
	gtk_widget_show (download_label);
	
	gtk_box_pack_start (GTK_BOX (main_box), download_label, FALSE, FALSE, 2);
}

/* handle the "next" signal for the update page based on the user's choice */
static gboolean
next_update_page_callback (GtkWidget *button, GnomeDruid *druid)
{
	if (last_update_choice == 0) {
		/* initiate the file transfer and launch a timer task to track feedback */
		/* FIXME bugzilla.eazel.com 5053: There's no timer task! */
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
	const char *proxy_text;
	const char *port_text;
	char *proxy_url;

	proxy_text = gtk_entry_get_text (GTK_ENTRY (proxy_address_entry));
	port_text = gtk_entry_get_text (GTK_ENTRY (port_number_entry));

	/* Update the http proxy only if there is some user input */
	if (nautilus_strlen (proxy_text) > 0 && nautilus_strlen (port_text) > 0) {
		proxy_url = g_strdup_printf ("http://%s:%s", proxy_text, port_text);
		set_http_proxy (proxy_url);
		g_free (proxy_url);
	}
	
	/* now, go back to the offer update page or finish, depending on the user's selection */
	if (last_proxy_choice == 1) {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (pages[OFFER_UPDATE_PAGE]));
	} else {
		gnome_druid_set_page (druid, GNOME_DRUID_PAGE (finish_page));
	}
	
	return TRUE;
}


static void
convert_gmc_desktop_icons (void)
{
	const char *home_dir;
	char *gmc_desktop_dir,*nautilus_desktop_dir, *link_path;
	struct stat st;
	DIR *dir;
	struct dirent *dirent;
	GnomeDesktopEntry *gmc_link;
	
	home_dir = g_get_home_dir ();
	if (home_dir == NULL) {
		return;
	}
		
	gmc_desktop_dir = g_strdup_printf ("%s/.gnome-desktop", home_dir);
	
	if (stat (gmc_desktop_dir, &st) != 0) {
		g_free (gmc_desktop_dir);
		return;
	}
	
	if (!S_ISDIR (st.st_mode)) {
		g_free (gmc_desktop_dir);
		g_message ("Not a dir");
		return;
	}
	
	dir = opendir (gmc_desktop_dir);
	if (dir == NULL) {
		g_free (gmc_desktop_dir);
		return;
	}

	nautilus_desktop_dir = nautilus_get_desktop_directory ();

	/* Iterate all the files here and indentify the GMC links. */
	for (dirent = readdir (dir); dirent != NULL; dirent = readdir (dir)) {
		if (strcmp (dirent->d_name, ".") == 0 || strcmp (dirent->d_name, "..") == 0) {
			continue;
		}
		
		link_path = g_strdup_printf ("%s/%s", gmc_desktop_dir, dirent->d_name);

		gmc_link = gnome_desktop_entry_load (link_path);
		gmc_link = gnome_desktop_entry_load_unconditional (link_path);
		g_free (link_path);
		
		if (gmc_link != NULL) {
			nautilus_link_local_create_from_gnome_entry (gmc_link, nautilus_desktop_dir, NULL);
			gnome_desktop_entry_free (gmc_link);
		}
	}
				
	g_free (gmc_desktop_dir);
	g_free (nautilus_desktop_dir);	

}

/* handle the "next" signal for the update feedback page to skip the error page */
static gboolean
transition_value_changed (GtkWidget *checkbox, gboolean *value)
{	
	*value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
	
	return TRUE;
}

/* set up the "GMC to Nautilus Transition" page */
static void
set_up_gmc_transition_page (NautilusDruidPageEazel *page)
{
	GtkWidget *checkbox;
	GtkWidget *container, *main_box;

	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	checkbox = gtk_check_button_new_with_label (_("Use Nautilus to draw desktop."));
	gtk_box_pack_start (GTK_BOX (main_box), checkbox, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);	
	gtk_signal_connect (GTK_OBJECT (checkbox), "toggled", GTK_SIGNAL_FUNC (transition_value_changed), &draw_desktop);

	checkbox = gtk_check_button_new_with_label (_("Transfer GMC desktop icons to Nautilus desktop."));
	gtk_box_pack_start (GTK_BOX (main_box), checkbox, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);	
	gtk_signal_connect (GTK_OBJECT (checkbox), "toggled", GTK_SIGNAL_FUNC (transition_value_changed), &transfer_gmc_icons);

	checkbox = gtk_check_button_new_with_label (_("Launch Nautilus at Gnome session startup."));
	gtk_box_pack_start (GTK_BOX (main_box), checkbox, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
	gtk_signal_connect (GTK_OBJECT (checkbox), "toggled", GTK_SIGNAL_FUNC (transition_value_changed), &add_to_session);
	
	draw_desktop = TRUE;
	add_to_session = TRUE;
	transfer_gmc_icons = TRUE;

	gtk_widget_show_all (main_box);
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

GtkWidget *
nautilus_first_time_druid_show (NautilusApplication *application, gboolean manage_desktop, const char *urls[])
{	
	GtkWidget *dialog;
	GtkWidget *druid;
	int index;
	GtkWidget *container, *main_box, *label;
	
	/* remember parameters for later window invocation */
	save_application = application;
	save_manage_desktop = manage_desktop;
	
	dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (dialog),
			      _("Nautilus First Time Setup"));
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "initial_preferences", "Nautilus");
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), 0);
  	//gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);
  	/* Ensure there's a main event loop while the druid is running. */
  	nautilus_main_event_loop_register (GTK_OBJECT (dialog));


	druid = nautilus_druid_new ();
  	gtk_container_set_border_width (GTK_CONTAINER (druid), 0);

	start_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_START);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (start_page));
	finish_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_FINISH);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (finish_page));

	for (index = 0; index < NUMBER_OF_STANDARD_PAGES; index++) {
		pages[index] = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_PAGE_EAZEL_OTHER);
		set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (pages[index]));
	}
		
	/* set up the initial page */

	/* allocate the description using a nautilus_label to get anti-aliased text */
	container = set_up_background (NAUTILUS_DRUID_PAGE_EAZEL (start_page), "rgb:ffff/ffff/ffff:h");
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* make the title label */
	label = label_new_left_justified ( _("Welcome to Nautilus!"));
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 4);
	nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (label), 2);
	nautilus_label_set_smooth_drop_shadow_color (NAUTILUS_LABEL (label), NAUTILUS_RGBA_COLOR_PACK (191, 191, 191, 255));
	
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, TRUE, 16);
	
	label = label_new_left_justified ( _("Since this is the first time that you've launched\nNautilus, we'd like to ask you a few questions\nto help personalize it for your use."));
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 4);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	label = label_new_left_justified ( _("Press the next button to continue."));
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 4);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 16);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	
	/* set up the final page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), _("Finished"));

	container = set_up_background (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), "rgb:ffff/ffff/ffff:h");
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	label = label_new_left_justified ( _("Click the finish button to launch Nautilus.\nWe hope that you enjoy using it!"));
	nautilus_label_make_larger (NAUTILUS_LABEL (label), 4);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
		
	/* set up the user level page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[USER_LEVEL_PAGE]), _("Select A User Level"));
	set_up_user_level_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[USER_LEVEL_PAGE]));
				
	/* set up the GMC transition page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[GMC_TRANSITION_PAGE]), _("GMC to Nautilus Transition"));
	set_up_gmc_transition_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[GMC_TRANSITION_PAGE]));

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
	for (index = 0; index < NUMBER_OF_STANDARD_PAGES; index++) {
		gnome_druid_append_page (GNOME_DRUID (druid), GNOME_DRUID_PAGE (pages[index]));
	}
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
	char *untar_command, *user_directory_path;
	int size, write_result, expand_result;
	FILE* outfile;
	GnomeDruid *druid;
	char *temporary_file;
	char *remove_command;
	
	druid = GNOME_DRUID (callback_data);
	
	/* Let the caller know we're done. */
	gtk_object_remove_no_notify (GTK_OBJECT (druid), READ_FILE_HANDLE_TAG);

	/* check for errors */
	if (result == GNOME_VFS_OK) {
		/* there was no error, so write out the file into the /tmp directory */
		temporary_file = nautilus_unique_temporary_file_name ();
		g_assert (temporary_file != NULL);
		size = file_size;
		outfile = fopen (temporary_file, "wb");	 	
		write_result = fwrite (file_contents, size, 1, outfile);
		fclose (outfile);
		g_free (file_contents);

		/* change the message to expanding file */
		nautilus_label_set_text (NAUTILUS_LABEL (download_label), _("Decoding Update..."));
		
		/* expand the directory into the proper place */
		/* first, formulate the command string */
		user_directory_path = nautilus_get_user_directory ();
		untar_command = g_strdup_printf ("cd %s; tar xvfz %s >/dev/null",
						 user_directory_path, temporary_file);
		remove_command = g_strdup_printf ("rm -f %s >/dev/null", temporary_file);

		g_free (temporary_file);
		
		/* execute the command to make the updates folder */
		expand_result = system (untar_command);

		/* Remove the temporary file */
		expand_result = system (remove_command);

		nautilus_label_set_text (NAUTILUS_LABEL (download_label), _("Update Completed... Press Next to Continue."));
			
		g_free (user_directory_path);
		g_free (untar_command);
		g_free (remove_command);

		/* now that we're done, reenable the buttons */
		gtk_widget_set_sensitive (druid->next, TRUE);
		gtk_widget_set_sensitive (druid->back, TRUE);
	} else {
		/* there was an error; see if we can't find some HTTP proxy config info */
		/* note that attempt_http_proxy_autoconfigure returns FALSE if it's already been tried */ 
		if (attempt_http_proxy_autoconfigure ()) {
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
read_file_handle_cancel_cover (gpointer data)
{
	NautilusReadFileHandle *handle;

	handle = data;
	nautilus_read_file_cancel (handle);
}

static void
initiate_file_download (GnomeDruid *druid)
{
	NautilusReadFileHandle *handle;
	static gboolean prevent_re_entry = FALSE;

	/* We exercise the event loop below, so we need to make sure that 
	 * we don't get re-entered
	 */

	if (prevent_re_entry) {
		return;
	}

	prevent_re_entry = TRUE;

	/* disable the next and previous buttons during the file loading process */
	gtk_widget_set_sensitive (druid->next, FALSE);
	gtk_widget_set_sensitive (druid->back, FALSE);

	/* Cancel any download already in progress. */
	gtk_object_remove_data (GTK_OBJECT (druid), READ_FILE_HANDLE_TAG);
			
	/* FIXME bugzilla.eazel.com 5054: We might hang here for a while; if we do, we don't want
	 * the user to get forced through the druid again
	 */
	druid_set_first_time_file_flag ();

	/* We need to exercise the main loop so that gnome-vfs can get its
	 * gconf callback for the HTTP proxy autoconfiguration case.
	 * FIXME: note that "druid" can be freed in this event loop
	 */

	while (gtk_events_pending()) {
		gtk_main_iteration();
	}

	prevent_re_entry = FALSE;

	if (check_network_connectivity ()) {
		/* initiate the file transfer */
		handle = nautilus_read_entire_file_async
			(WELCOME_PACKAGE_URI, download_callback, druid);
		if (handle != NULL) {
			/* cancel the transfer if the druid goes away */
			gtk_object_set_data_full (GTK_OBJECT (druid),
						  READ_FILE_HANDLE_TAG,
						  handle,
						  read_file_handle_cancel_cover);
		}
	} else {
		download_callback (GNOME_VFS_ERROR_GENERIC, 0, NULL, druid);
	}
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
	char *colon;
	int port;

	/* DEBUG */
	g_print ("set_http_proxy: %s\n", proxy_url);

	/* We reset this later */
	if (http_proxy_host) {
		g_free (http_proxy_host);
		http_proxy_host = NULL;
	}

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

	http_proxy_host = g_strdup (proxy_url_port_part);

	/* chew off trailing / */
	if ('/' == http_proxy_host[proxy_len - 1]) {
		http_proxy_host[proxy_len - 1] = 0;
	}

	/* Scan for port */
	if ( NULL != ( colon = strchr (http_proxy_host, (unsigned char)':'))) {
		char *endptr;
		
		*colon = '\0';
		port = strtoul (colon+1, &endptr, 10);

		/*no integer here*/
		if ('\0' == *(colon+1) || endptr == colon+1) {
			port = DEFAULT_HTTP_PROXY_PORT;
		}
	} else {
		port = DEFAULT_HTTP_PROXY_PORT;
	}

	nautilus_preferences_set (GNOME_VFS_PREFERENCES_HTTP_PROXY_HOST, http_proxy_host);
	nautilus_preferences_set_integer (GNOME_VFS_PREFERENCES_HTTP_PROXY_PORT, port);
	nautilus_preferences_set_boolean (GNOME_VFS_PREFERENCES_USE_HTTP_PROXY, TRUE);

	return TRUE;
}

/**
 * getline_dup
 *
 * reads newline (or CR) or EOF terminated line from stream, allocating the return
 * buffer as appropriate
 **/
/* FIXME bugzilla.eazel.com 5055: Belongs in a library, not here. */
static char * 
getline_dup (FILE* stream)
{
	char *ret;
	size_t ret_size;
	size_t ret_used;
	int char_read;
	gboolean done;

	ret = g_malloc (GETLINE_INITIAL_BUFFER_SIZE);
	ret_size = GETLINE_INITIAL_BUFFER_SIZE;

	for ( ret_used = 0, done = FALSE ;
	      !done && (EOF != (char_read = fgetc (stream))) ; 
	      ret_used++) {
		
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

	if (ret_used == 0) {
		g_free (ret);
		ret = NULL;
	} else {
		ret [ret_used] = '\0';
	}

	return ret;
}

/* user_pref("network.proxy.http", "localhost");
 * user_pref("network.proxy.http_port", 8080);
 * user_pref("network.proxy.type", 1);
 */
static char *
load_netscape_proxy_settings (void)
{
	char *prefs_path;
	char *ret ;
	char *proxy_host;
	guint32 proxy_port;
	gboolean has_proxy_type;

	char *line;
	char *current, *end;
	FILE *prefs_file;

	ret = NULL;
	proxy_host = NULL;
	proxy_port = 8080;
	has_proxy_type = FALSE;

	prefs_path = g_strconcat (g_get_home_dir (), NETSCAPE_PREFS_PATH, NULL);

	prefs_file = fopen (prefs_path, "r");
	if (prefs_file == NULL) {
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

	return ret;
}


/* http_proxy=localhost
 * http_proxy_port=4128
 */
static char *
load_galeon_proxy_settings (void)
{
	char * prefs_path = NULL;
	char * line;
	FILE * prefs_file;
	char * proxy_host = NULL;
	guint32 proxy_port = 8080;
	char * ret = NULL;

	prefs_path = g_strdup_printf ("%s%s", g_get_home_dir (), GALEON_PREFS_PATH);
	prefs_file = fopen (prefs_path, "r");
	if ( NULL == prefs_file ) {
		goto error;
	}

	/* i have no qualms about doing it "this way" ;) */
	for ( ; NULL != (line = getline_dup (prefs_file)) ; g_free (line) ) {
		if ((g_strncasecmp (line, "http_proxy=", 11) == 0) && (strlen (line+11))) {
			proxy_host = g_strdup (line+11);
		}
		if ((g_strncasecmp (line, "http_proxy_port=", 16) == 0) && (strlen (line+16))) {
			proxy_port = strtoul (line+16, NULL, 10);
		}
	}

	if (proxy_host != NULL) {
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
		goto done;
	}

	/* Check Netscape 4.x settings */
	proxy_url = load_netscape_proxy_settings ();
	if (NULL != proxy_url) {
		success = TRUE;
		set_http_proxy (proxy_url);
		g_free (proxy_url);
		proxy_url = NULL;
		goto done;
	}

	/* Check for Galeon */
	proxy_url = load_galeon_proxy_settings ();
	if (NULL != proxy_url) {
		success = TRUE;
		set_http_proxy (proxy_url);
		g_free (proxy_url);
		proxy_url = NULL;
		goto done;
	}

done:
	autoconfigure_attempted = TRUE;
	return success;
}

static void
sigalrm_handler (int sig)
{
	sigalrm_occurred = TRUE;
	kill (child_pid, SIGKILL);
}

/* Do a simple DNS lookup wrapped in sigalrm to check the presense of the network
 * The purpose of this check is to ensure that the DNS resolution doesn't hang
 * indefinitely, not that the host was actually resolved.  If the sigalrm handler
 * gets called, then it's looking like we were hanging indefinitely, so elsewhere
 * first-time-druid we're going to avoid DNS calls
 */
 
static gboolean
check_dns_resolution (const char *host)
{
	struct hostent *my_hostent;
	int child_status;
	void *sigalrm_old;

	sigalrm_occurred = FALSE;

	child_pid = fork ();
	if (child_pid == 0) {
		my_hostent = gethostbyname (host);
		_exit (0);
	}

	if (child_pid > 0) {
		sigalrm_old = signal (SIGALRM, sigalrm_handler);
		alarm (NETWORK_CHECK_TIMEOUT_SECONDS);
		waitpid (child_pid, &child_status, 0);
		alarm (0);
		signal (SIGALRM, sigalrm_old);	
	}
	
	return !sigalrm_occurred;
}

static gboolean
check_network_connectivity (void) 
{
	gboolean ret;
	
	/* If there's an HTTP proxy, then we want to try to resolve the HTTP proxy
	 * because we may not have DNS to the outside world
	 */

	if (NULL != http_proxy_host) {
		ret = check_dns_resolution (http_proxy_host);
	} else {
		ret = check_dns_resolution (DEFAULT_DNS_CHECK_HOST);
	}

	network_status = ret ? Success : Fail;

	return ret;
}
