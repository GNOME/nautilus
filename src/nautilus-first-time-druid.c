/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-first-time-druid.c:
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <widgets/nautilus-druid/nautilus-druid.h>
#include <widgets/nautilus-druid/nautilus-druid-page-start.h>
#include <widgets/nautilus-druid/nautilus-druid-page-standard.h>
#include <widgets/nautilus-druid/nautilus-druid-page-finish.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-radio-button-group.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>

#include "nautilus-first-time-druid.h"

static NautilusApplication *save_application;
static gboolean save_manage_desktop;

static int last_signup_choice = 0;

static void
druid_cancel (GtkWidget *druid)
{
	gtk_widget_destroy(gtk_widget_get_toplevel(druid));
	_exit (0);
}

/* handle the final page finishing  */

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

	if (last_signup_choice < 2) {
		if (last_signup_choice == 0)
			signup_uris[0] = "http://www.eazel.com/services.html";
		else
			signup_uris[0] = "http://www.eazel.com/registration.html";
		signup_uris[1] = NULL;
		nautilus_application_startup(save_application, FALSE, save_manage_desktop, FALSE, &signup_uris[0]);
		
	} else {
		nautilus_application_startup(save_application, FALSE, save_manage_desktop, FALSE, NULL);
	}
}

/* set up an event box to serve as the background */

static GtkWidget*
set_up_background (NautilusDruidPageStandard *page, const char *background_color)
{
	GtkWidget *event_box;
	NautilusBackground *background;
	
	event_box = gtk_event_box_new();
	gtk_container_add (GTK_CONTAINER (page->vbox), event_box);
	
	background = nautilus_get_widget_background (event_box);
	nautilus_background_set_color (background, background_color);
	
	gtk_widget_show (event_box);

	return event_box;
}

/* handler for user level buttons changing */
static void
user_level_selection_changed (GtkWidget *radio_buttons, gpointer user_data)
{
	nautilus_user_level_manager_set_user_level (
		nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons)));
}

/* handler for signup buttons changing */
static void
signup_selection_changed (GtkWidget *radio_buttons, gpointer user_data)
{

	last_signup_choice = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons));
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

/* set up the user level page */
static void
set_up_user_level_page (NautilusDruidPageStandard *page)
{
	GtkWidget *radio_buttons, *frame, *label;
	GtkWidget *container, *main_box;
	GdkPixbuf *user_level_icons[3];

	container = set_up_background (page, "rgb:bbbb/bbbb/eeee-rgb:ffff/ffff/ffff:h");

	/* allocate a vbox to hold the description and the widgets */
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_container_add (GTK_CONTAINER (container), main_box);
	
	/* allocate a descriptive label */
	label = gtk_label_new (_("User levels provide a way to adjust the software to your level of technical expertise.  Pick an initial level that you feel comfortable with; you can always change it later."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 8);
	
	frame = gtk_frame_new (_("User Levels"));
	gtk_widget_show (frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);

	radio_buttons = nautilus_radio_button_group_new (FALSE);
	gtk_container_add (GTK_CONTAINER (frame),
					radio_buttons);

	user_level_icons[0] = create_named_pixbuf ("novice.png");
	g_assert (user_level_icons[0] != NULL);

	user_level_icons[1] = create_named_pixbuf ("intermediate.png");
	g_assert (user_level_icons[1] != NULL);

	user_level_icons[2] = create_named_pixbuf ("expert.png");
	g_assert (user_level_icons[2] != NULL);

	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Novice"));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Intermediate"));
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("Hacker"));

	nautilus_radio_button_group_set_entry_pixbuf (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 0, user_level_icons[0]);
	nautilus_radio_button_group_set_entry_pixbuf (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 1, user_level_icons[1]);
	nautilus_radio_button_group_set_entry_pixbuf (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 2, user_level_icons[2]);

	nautilus_radio_button_group_set_entry_description_text (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 0, _("For beginning users"));
	nautilus_radio_button_group_set_entry_description_text (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 1, _("For non-technical users"));
	nautilus_radio_button_group_set_entry_description_text (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), 2, _("For expert users"));

	nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons),
						      nautilus_user_level_manager_get_user_level ());

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (user_level_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}

/* set up the user level page */
static void
set_up_service_signup_page (NautilusDruidPageStandard *page)
{
	GtkWidget *radio_buttons, *frame, *label;
	GtkWidget *container, *main_box;

	container = set_up_background (page, "rgb:bbbb/bbbb/eeee-rgb:ffff/ffff/ffff:h");

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
	nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (radio_buttons), _("I don't want to learn about Eazel services at this time."));

	gtk_signal_connect (GTK_OBJECT (radio_buttons),
			    "changed",
			    GTK_SIGNAL_FUNC (signup_selection_changed),
			    (gpointer) NULL);

	gtk_box_pack_start (GTK_BOX (main_box), frame, FALSE, FALSE, 2);
	gtk_widget_show (radio_buttons);

}

/* create the initial preferences druid */

GtkWidget *nautilus_first_time_druid_show (NautilusApplication *application, gboolean manage_desktop, const char *urls[])
{
	
	char *logo_path;
	GtkWidget *dialog;
	GtkWidget *druid;

	GtkWidget *start_page;
	GtkWidget *finish_page;

	GtkWidget *pages[2];

	GdkPixbuf *logo;

	/* remember parameters for later window invocation */
	save_application = application;
	save_manage_desktop = manage_desktop;
	
	dialog = gnome_dialog_new (_("Nautilus: Initial Preferences"),
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	druid = nautilus_druid_new ();

	start_page = nautilus_druid_page_start_new ();
	finish_page = nautilus_druid_page_finish_new ();

	pages[0] = nautilus_druid_page_standard_new ();
	pages[1] = nautilus_druid_page_standard_new ();

	/* set up the initial page */
	nautilus_druid_page_start_set_title (NAUTILUS_DRUID_PAGE_START (start_page), _("Welcome to Nautilus!"));
	nautilus_druid_page_start_set_text (NAUTILUS_DRUID_PAGE_START(start_page), _("Welcome to Nautilus!\n\nSince this is the first time that you've launched\nNautilus, we'd like to ask you a few questions\nto help personalize it for your use.\n\nPress the next button to continue."));
	
	/* set up the final page */
	nautilus_druid_page_finish_set_title (NAUTILUS_DRUID_PAGE_FINISH (finish_page), _("Finished"));
	nautilus_druid_page_finish_set_text (NAUTILUS_DRUID_PAGE_FINISH(finish_page), _("Click to finish button to launch Nautilus.\n\nWe hope that you enjoying using it!"));

	/* set up the user level page */
	nautilus_druid_page_standard_set_title (NAUTILUS_DRUID_PAGE_STANDARD (pages[0]), "Select A User Level");
	set_up_user_level_page(NAUTILUS_DRUID_PAGE_STANDARD (pages[0]));
				
	/* set up the service sign-up page */
	nautilus_druid_page_standard_set_title (NAUTILUS_DRUID_PAGE_STANDARD (pages[1]), "Sign Up for Eazel Services");
	set_up_service_signup_page(NAUTILUS_DRUID_PAGE_STANDARD (pages[1]));

	/* append all of the pages to the druid */
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (start_page));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[0]));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (pages[1]));
	nautilus_druid_append_page (NAUTILUS_DRUID (druid), NAUTILUS_DRUID_PAGE (finish_page));

	/* set up the logo images */	

	logo_path = nautilus_pixmap_file ("nautilus-logo.png");
	logo = gdk_pixbuf_new_from_file (logo_path);
	g_assert (logo != NULL);
	g_free (logo_path);
 	
	
	/*
	nautilus_druid_page_start_set_logo (NAUTILUS_DRUID_PAGE_START (start_page), logo);
	nautilus_druid_page_finish_set_logo (NAUTILUS_DRUID_PAGE_FINISH (finish_page), logo);
	nautilus_druid_page_standard_set_logo (NAUTILUS_DRUID_PAGE_STANDARD (pages[0]), logo);
	nautilus_druid_page_standard_set_logo (NAUTILUS_DRUID_PAGE_STANDARD (pages[1]), logo);

	*/
	gdk_pixbuf_unref (logo);
	
  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), 
  			    druid,
  			    TRUE,
			    TRUE,
			    0);

	/* set up the signals */
  	gtk_signal_connect (GTK_OBJECT (druid), "cancel",
                      GTK_SIGNAL_FUNC (druid_cancel),
                      NULL);
	gtk_signal_connect (GTK_OBJECT (finish_page), "finish",
                      GTK_SIGNAL_FUNC (druid_finished),
                      NULL);

	gtk_widget_show_all (druid);

	/* Im commenting out this call cause it makes the druid unusable for me and 
	 * thus any new users of nautilus that use large fonts as i do.
	 * Perhaps we need a better way to control the druid geometry.  We can
	 * file specific bugs for that --ramiro
	 */

	/* gtk_widget_set_usize (druid, 400, 320); */

	gtk_widget_show (GTK_WIDGET (dialog));
	return druid;
}

