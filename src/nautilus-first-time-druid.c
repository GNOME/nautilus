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

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <widgets/nautilus-druid/nautilus-druid.h>
#include <widgets/nautilus-druid/nautilus-druid-page-eazel.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>

#include "nautilus-first-time-druid.h"

#define SERVICE_UPDATE_ARCHIVE_PATH "/tmp/nautilus_update.tgz"
#define NUMBER_OF_STANDARD_PAGES 4

static void
initiate_file_download (NautilusDruid *druid);

/* globals */
static NautilusApplication *save_application;
static gboolean save_manage_desktop;

static GtkWidget *start_page;
static GtkWidget *finish_page;
static GtkWidget *pages[NUMBER_OF_STANDARD_PAGES];

static GtkWidget *download_progress;

static int last_signup_choice = 0;
static int last_update_choice = 0;

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
	
	gtk_widget_destroy(gtk_widget_get_toplevel(druid_page));

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
			signup_uris[0] = "eazel:info";
			break;
		case 1:
			signup_uris[0] = "eazel:register";
			break;
		case 2:
			signup_uris[0] = "eazel:summary";
			break;
		case 3:
		default:
			signup_uris[0]	= NULL;	
			break;
	}
		nautilus_application_startup(save_application, FALSE, FALSE, save_manage_desktop, 
					     FALSE, (signup_uris[0] != NULL) ? &signup_uris[0] : NULL);
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



static GdkPixbuf*
create_named_pixbuf (const char *name) 
{
	GdkPixbuf	*pixbuf;
	char		*path;
	
	g_return_val_if_fail (name != NULL, NULL);
	
	path = nautilus_pixmap_file (name);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_free (path);

	g_assert (pixbuf != NULL);

	return pixbuf;
}

static void
make_label_bold (GtkWidget *label)
{
	GdkFont *bold_font;

	bold_font = nautilus_gdk_font_get_bold (label->style->font);

	if (bold_font != NULL) {
		nautilus_gtk_widget_set_font (label, bold_font);
	}
}

static GtkWidget *
make_hbox_user_level_radio_button (int index, GtkWidget *radio_buttons[],
				   const char *icon_name, const char *text,
				   const char *comment, const char *background)
{
	GtkWidget *hbox, *label_box, *icon, *label, *alignment;
	GtkWidget *comment_vbox, *comment_hbox;
	GdkPixbuf *icon_pixbuf;

	hbox = gtk_hbox_new (FALSE, 0);

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
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (icon), icon_pixbuf);
	gtk_box_pack_start (GTK_BOX (label_box), icon, FALSE, FALSE, 0);
	label = gtk_label_new (text);
	make_label_bold (label);
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
	label = gtk_label_new (comment);
	gtk_box_pack_start (GTK_BOX (comment_hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label),
			       GTK_JUSTIFY_LEFT);

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

	label = gtk_label_new (NULL);
	gtk_label_set (GTK_LABEL (label),
			    _("User levels provide a way to adjust the software to\nyour level of technical expertise.  Pick an initial level that you\nfeel comfortable with; you can always change it later."));
	gtk_label_set_justify (GTK_LABEL (label),
			       GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	/* Make the user level radio buttons and fill the radio_buttons
	 * array */
	hbox = make_hbox_user_level_radio_button
		(0, radio_buttons, "novice.png",
		 _("Beginner"),
		 _("For beginner users that are not yet\n"
		   "familiar with the working of "
		   "GNOME and Linux."),
		 NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 0);
	hbox = make_hbox_user_level_radio_button
		(1, radio_buttons, "intermediate.png",
		 _("Intermediate"),
		 _("For non-techincal users that are comfortable with\n"
		   "their GNOME and Linux environment."),
				       NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 0);
	hbox = make_hbox_user_level_radio_button
		(2, radio_buttons, "expert.png",
		 _("Advanced"),
		 _("For techincal users that have the need to be exposed\n"
		   "to every detail of their operating system"),
		 NULL);
	gtk_box_pack_start (GTK_BOX (main_box), hbox, FALSE, FALSE, 0);

	user_level = nautilus_user_level_manager_get_user_level ();
	g_assert (user_level >= 0 && user_level < 3);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_buttons[user_level]), TRUE);


	for (index = 0; index < 3; index ++) {
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
	label = gtk_label_new (_("Eazel offers a growing number of services to help you install and maintain new software and manage your files across the network.  If you want to find out more about Eazel services, just press the 'Next' button. "));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

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
	label = gtk_label_new (_("Nautilus will now contact Eazel services to verify your web connection and download the latest updates.  Click the Next button to continue."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("Updating Nautilus"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Yes, verify my web connection and update Nautilus now."));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("No, I do not wish to verify my web connect and update Nautilus now."));	

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (update_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}

/* set up the update feedback page */
static void
set_up_update_feedback_page (NautilusDruidPageEazel *page)
{
	GtkWidget *frame, *label;
	GtkWidget *container, *main_box;
	GtkWidget *progress_box, *temp_box;
	
	container = set_up_background (page, "rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = gtk_label_new (_("We are now contacting the Eazel service to verify your web connection and update Nautilus.  We will advance to the next page automatically when updating is complete."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
		
	frame = gtk_frame_new (_("Nautilus Update Progress"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	progress_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (progress_box);
	
	download_progress = gtk_progress_bar_new ();
	gtk_progress_set_show_text (GTK_PROGRESS (download_progress), TRUE);		  
	
	/* put it in an hbox to restrict it's size */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX(temp_box), download_progress, FALSE, FALSE, 168);
		
	gtk_widget_show (download_progress);
	gtk_box_pack_start (GTK_BOX (progress_box), temp_box, FALSE, FALSE, 2);
	
	label = gtk_label_new (_("Downloading Nautilus updates..."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (progress_box), label, FALSE, FALSE, 2);

	gtk_container_add (GTK_CONTAINER (frame), progress_box);
	
	/* allocate update progess widgets */
	
	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
}

/* handle the "next" signal for the update page based on the user's choice */
static gboolean
next_update_page_callback (GtkWidget *button, NautilusDruid *druid)
{
	if (last_update_choice == 0) {
		/* initiate the file transfer and launch a timer task to track feedback */
		initiate_file_download (druid);
		
		/* return FALSE to display the feedback page */
		return FALSE;
	}

	/* the user declined to update, so skip the feedback page and go directly to finish */
	nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (finish_page));
	return TRUE;
}

/* handle the "back" signal from the finish page to skip the feedback page */
static gboolean
finish_page_back_callback (GtkWidget *button, NautilusDruid *druid)
{
	nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (pages[2]));
	return TRUE;
}

/* create the initial preferences druid */

static void
set_page_sidebar (NautilusDruidPageEazel *page)
{
	GdkPixbuf *pixbuf;
	char *file;

	file = nautilus_pixmap_file ("druid_sidebar.png");
	if (file != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (file);
	} else {
		pixbuf = NULL;
	}
	g_free (file);

	if (pixbuf != NULL) {
		nautilus_druid_page_eazel_set_sidebar_image (page, pixbuf);
		gdk_pixbuf_unref (pixbuf);
	}
}

GtkWidget *nautilus_first_time_druid_show (NautilusApplication *application, gboolean manage_desktop, const char *urls[])
{	
	GtkWidget *dialog;
	GtkWidget *druid;
	char *file;
	int i;

	GdkPixbuf *pixbuf;
	
	/* remember parameters for later window invocation */
	save_application = application;
	save_manage_desktop = manage_desktop;
	
	dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (dialog),
			      _("Nautilus: Initial Preferences"));
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), 0);
  	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	druid = nautilus_druid_new ();
  	gtk_container_set_border_width (GTK_CONTAINER (druid), 0);

	start_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_START);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (start_page));
	finish_page = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_FINISH);
	set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (finish_page));

	for (i = 0; i < NUMBER_OF_STANDARD_PAGES; i++) {
		pages[i] = nautilus_druid_page_eazel_new (NAUTILUS_DRUID_OTHER);
		set_page_sidebar (NAUTILUS_DRUID_PAGE_EAZEL (pages[i]));
	}
		
	/* set up the initial page */
	file = nautilus_pixmap_file ("druid_welcome.png");
	if (file != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (file);
	} else {
		pixbuf = NULL;
	}
	g_free (file);

	if (pixbuf != NULL) {
		nautilus_druid_page_eazel_set_title_image (NAUTILUS_DRUID_PAGE_EAZEL (start_page), pixbuf);
		gdk_pixbuf_unref (pixbuf);
	} else {
		nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (start_page), _("Welcome to Nautilus!"));
	}

	nautilus_druid_page_eazel_set_text (NAUTILUS_DRUID_PAGE_EAZEL(start_page), _("Welcome to Nautilus!\n\nSince this is the first time that you've launched\nNautilus, we'd like to ask you a few questions\nto help personalize it for your use.\n\nPress the next button to continue."));
	
	/* set up the final page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (finish_page), _("Finished"));
	nautilus_druid_page_eazel_set_text (NAUTILUS_DRUID_PAGE_EAZEL(finish_page), _("Click to finish button to launch Nautilus.\n\nWe hope that you enjoying using it!"));

	/* set up the user level page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[0]), _("Select A User Level"));
	set_up_user_level_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[0]));
				
	/* set up the service sign-up page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[1]), _("Sign Up for Eazel Services"));
	set_up_service_signup_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[1]));

	/* set up the update page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[2]), _("Nautilus Update"));
	set_up_update_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[2]));

	gtk_signal_connect (GTK_OBJECT (pages[2]), "next",
			    GTK_SIGNAL_FUNC (next_update_page_callback),
			    druid);

	/* set up the update feedback page */
	nautilus_druid_page_eazel_set_title (NAUTILUS_DRUID_PAGE_EAZEL (pages[3]), _("Updating Nautilus..."));
	set_up_update_feedback_page (NAUTILUS_DRUID_PAGE_EAZEL (pages[3]));

	/* capture the "back" signal from the finish page to skip the feedback page */
	gtk_signal_connect (GTK_OBJECT (finish_page), "back",
			    GTK_SIGNAL_FUNC (finish_page_back_callback),
			    druid);
	
	/* append all of the pages to the druid */
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (start_page));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[0]));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[1]));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[2]));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[3]));
	
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (finish_page));

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
	NautilusDruid *druid;
	
	druid = NAUTILUS_DRUID (callback_data);
	
	/* check for errors */
	if (result == GNOME_VFS_OK) {
		/* there was no error, so write out the file into the /tmp directory */
		size = file_size;
		outfile = fopen (SERVICE_UPDATE_ARCHIVE_PATH, "wb");	 	
		write_result = fwrite (file_contents, size, 1, outfile);
		fclose (outfile);
		g_free (file_contents);
		
		/* expand the directory into the proper place */
		/* first, formulate the command string */
		user_directory_path = nautilus_get_user_directory ();
		command_str = g_strdup_printf ("cd %s; tar xvfz %s >/dev/null", user_directory_path, SERVICE_UPDATE_ARCHIVE_PATH);
		
		/* execute the command to make the updates folder */
		expand_result = system (command_str);
		
		g_free (user_directory_path);
		g_free (command_str);	
	
		/* advance to the next page */
		nautilus_druid_set_page (druid, NAUTILUS_DRUID_PAGE (finish_page));	
	}
}

/* initiate downloading of the welcome package from the service */

static void
initiate_file_download (NautilusDruid *druid)
{
	NautilusReadFileHandle *file_handle;
	/* for testing */
	const char *file_uri = "http://dellbert.differnet.com/eazel-services/updates.tgz";
	
	 /* initiate the file transfer */
	file_handle = nautilus_read_entire_file_async (file_uri, download_callback, druid);
	
	/* launch a timer task to provide feedback */

}

