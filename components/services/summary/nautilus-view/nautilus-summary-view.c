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
#include "eazel-summary-shared.h"
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
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

#define DEFAULT_BACKGROUND_COLOR		"rgb:0000/6666/6666"
#define DEFAULT_SUMMARY_BACKGROUND_COLOR	"rgb:FFFF/FFFF/FFFF"
/* #define	SUMMARY_XML_HOME			"http://localhost/summary-configuration.xml" */
#define	SUMMARY_XML_HOME			"http://services.eazel.com:8888/services"
#define	SUMMARY_XML_HOME_2			"eazel-services:/services"
#define	URL_REDIRECT_TABLE_HOME			"http://services.eazel.com:8888/services/urls"
#define	URL_REDIRECT_TABLE_HOME_2		"eazel-services:/services/urls"
#define	REGISTER_HOME				"eazel-services://anonymous/account/register/form"
#define	PREFERENCES_HOME			"eazel-services:/account/preferences/form"

typedef struct _ServicesButtonCallbackData ServicesButtonCallbackData;

typedef enum {
	Pending_None,
	Pending_Login,
} SummaryPendingOperationType;


struct _ServicesButtonCallbackData {
	NautilusView	*nautilus_view;
	char		*uri;
};

/* A NautilusContentView's private information. */
struct _NautilusSummaryViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;

	SummaryData	*xml_data;

	/* Parent form and title */
	GtkWidget	*form;
	GtkWidget	*form_title;

	/* Startup view controls */
	char		*redirect_location;
	GtkWidget	*progress_bar;
	GtkWidget	*feedback_text;

	/* Login State */
	char		*user_name;
	volatile gboolean	logged_in;

	/* Services control panel */
	int		current_service_row;
	GtkTable	*services_table;
	GtkWidget	*services_icon_container;
	GtkWidget	*services_icon_widget;
	char		*services_icon_name;
	GtkWidget	*services_description_header_widget;
	char		*services_description_header;
	GtkWidget	*services_description_body_widget;
	char		*services_description_body;
	GtkWidget	*services_button_container;
	GtkWidget	*services_goto_button;
	GtkWidget	*services_goto_label_widget;
	char		*services_goto_label;
	char		*services_redirects[500];

	/* Login Frame Widgets */
	GtkWidget	*username_label;
	GtkWidget	*password_label;
	GtkWidget	*username_entry;
	GtkWidget	*password_entry;
	/* Buttons available if user is not logged in */
	GtkWidget	*login_button;
	GtkWidget	*login_label;
	GtkWidget	*register_button;
	GtkWidget	*register_label;
	/* Buttons available if user is logged in */
	GtkWidget	*preferences_button;
	GtkWidget	*preferences_label;
	GtkWidget	*logout_button;
	GtkWidget	*logout_label;

	/* Eazel news panel */
	int		current_news_row;
	gboolean	*news_has_data;
	GtkTable	*service_news_table;
	GtkWidget	*news_icon_container;
	GtkWidget	*news_icon_widget;
	char		*news_icon_name;
	GtkWidget	*news_description_header_widget;
	char		*news_description_header;
	GtkWidget	*news_description_body_widget;
	char		*news_description_body;

	/* Update control panel */
	int		current_update_row;
	gboolean	*updates_has_data;
	GtkTable	*updates_table;
	GtkWidget	*update_icon_container;
	GtkWidget	*update_icon_widget;
	char		*update_icon_name;
	GtkWidget	*update_description_header_widget;
	char		*update_description_header;
	GtkWidget	*update_description_body_widget;
	char		*update_description_body;
	GtkWidget	*update_description_version_widget;
	char		*update_description_version;
	GtkWidget	*update_button_container;
	GtkWidget	*update_goto_button;
	GtkWidget	*update_goto_label_widget;
	char		*update_goto_label;
	char		*update_redirects[500];

	/* EazelProxy -- for logging in/logging out */
	EazelProxy_UserControl user_control;
	SummaryPendingOperationType pending_operation;
	EazelProxy_AuthnCallback authn_callback;
};

static void	nautilus_summary_view_initialize_class	(NautilusSummaryViewClass	*klass);
static void	nautilus_summary_view_initialize	(NautilusSummaryView		*view);
static void	nautilus_summary_view_destroy		(GtkObject			*object);
static void	summary_load_location_callback		(NautilusView			*nautilus_view,
							 const char			*location,
	 						 NautilusSummaryView		*view);
static void	generate_startup_form			(NautilusSummaryView		*view);
static void	generate_startup_form_logo		(NautilusSummaryView		*view);
static void	generate_summary_form			(NautilusSummaryView		*view);
static void	generate_service_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	generate_eazel_news_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	generate_update_news_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	login_button_cb				(GtkWidget			*button,
							 NautilusSummaryView		*view); 
static void	preferences_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view); 
static void	logout_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view);
static void	entry_changed_cb			(GtkWidget			*entry,
							 NautilusSummaryView		*view);
static void	goto_service_cb				(GtkWidget			*button,
							 ServicesButtonCallbackData	*cbdata);
static void	goto_update_cb				(GtkWidget			*button,
							 ServicesButtonCallbackData	*cbdata);
static void	register_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view);
static gboolean	am_i_logged_in				(NautilusSummaryView		*view);
static gint	logged_in_callback			(gpointer			raw);
static gint	logged_out_callback			(gpointer			raw);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

static void
generate_startup_form (NautilusSummaryView       *view)
{
	GtkWidget		*temp_widget;
	GtkWidget		*temp_box;
	GtkWidget		*align;
	int			counter;
	gboolean		got_url_table;

	view->details->logged_in = am_i_logged_in (view);

	if (view->details->logged_in == TRUE) {
		/* fetch urls */
		got_url_table = trilobite_redirect_fetch_table (URL_REDIRECT_TABLE_HOME_2);
		if (got_url_table != TRUE) {
			g_assert ("Could not get url table !\n");
		}
		/* fetch and parse the xml file */
		view->details->xml_data = parse_summary_xml_file (SUMMARY_XML_HOME_2);
		/* dispose of startup form that was shown */
		if (view->details->form != NULL) {
			gtk_widget_destroy (view->details->form);
			view->details->form = NULL;
		}

		generate_summary_form (view);
	}
	else {

	/* fetch urls */
	got_url_table = trilobite_redirect_fetch_table (URL_REDIRECT_TABLE_HOME);
	if (got_url_table != TRUE) {
		g_error (_("Could not get url table !\n"));
		return;
	}

	/* fetch and parse the xml file */
	view->details->xml_data = parse_summary_xml_file (SUMMARY_XML_HOME);
	if (view->details->xml_data == NULL) {
		g_error (_("Could not get summary configuration file !\n"));
		return;
	}

	/* set to default not logged in for now */
	view->details->logged_in = FALSE;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	nautilus_gtk_widget_set_background_color (view->details->form, DEFAULT_BACKGROUND_COLOR);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_startup_form_logo (view);

	/* create a fill space box */
	temp_box = gtk_hbox_new (TRUE, 30);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	/* Add the watch icon */
	temp_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 40);
	gtk_widget_show (temp_box);

	temp_widget = create_image_widget ("service-watch.png",
					   DEFAULT_BACKGROUND_COLOR);
	g_assert (temp_widget != NULL);

	gtk_box_pack_start (GTK_BOX (temp_box), temp_widget, 0, 0, 8);
	gtk_widget_show (temp_widget);

	/* Add a label for error status messages */
	view->details->feedback_text = nautilus_label_new ("");
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->feedback_text), 12);
	nautilus_label_set_text_color (NAUTILUS_LABEL (view->details->feedback_text), NAUTILUS_RGB_COLOR_WHITE);
	gtk_box_pack_end (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 15);

	/* Create a center alignment object */
	align = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_box_pack_end (GTK_BOX (view->details->form), align, FALSE, FALSE, 5);
	gtk_widget_show (align);

	/* Add the progress meter */
	view->details->progress_bar = gtk_progress_bar_new ();
	gtk_container_add (GTK_CONTAINER (align), view->details->progress_bar);
	gtk_widget_show (view->details->progress_bar);

	/* bogus progress loop */
	for (counter = 1; counter <= 10000; counter++) {
		float value;

		value = (float) counter / 10000;

		if (counter == 1) {
			show_feedback (view->details->feedback_text, "Testing eazel-proxy configuration ...");
		}
		if (counter == 5000) {
			show_feedback (view->details->feedback_text, "Authenticating user anonymous ...");
		}
		if (counter == 8000) {
			show_feedback (view->details->feedback_text, "Retrieving services list ...");
		}
		if (counter == 10000) {
			/* dispose of startup form that was shown */
			if (view->details->form != NULL) {
				gtk_widget_destroy (view->details->form);
				view->details->form = NULL;
			}

			generate_summary_form (view);
		}
		else {
			gtk_progress_bar_update (GTK_PROGRESS_BAR (view->details->progress_bar), value);
			while (gtk_events_pending ()) {
				gtk_main_iteration ();
			}
		}
	}
	}

}

static void
generate_startup_form_logo (NautilusSummaryView  *view)
{

	GtkWidget       *logo_container;
	GtkWidget       *logo_widget;

	logo_container = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), logo_container, 0, 0, 4);

	logo_widget = create_image_widget ("startup-logo.png",
					   DEFAULT_BACKGROUND_COLOR);

	g_assert (logo_widget != NULL);

	gtk_box_pack_start (GTK_BOX(logo_container), logo_widget, 0, 0, 4);
	gtk_widget_show_all (logo_container);

}


static void
generate_summary_form (NautilusSummaryView	*view)
{

/*	NautilusBackground	*background; */
	GtkWidget		*frame;
	GtkTable		*parent;
	GtkWidget		*title;
	GtkWidget		*small_title;
	GtkWidget		*temp_box;
	GtkWidget		*temp_hbox;
	GtkWidget		*button_box;
	ServicesData		*service_node;
	EazelNewsData		*eazel_news_node;
	UpdateNewsData		*update_news_node;
	GList			*iterator;

	view->details->current_service_row = 0;
	view->details->current_news_row = 0;
	view->details->current_update_row = 0;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	nautilus_gtk_widget_set_background_color (view->details->form, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* reset the default background color */
/*	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, DEFAULT_SUMMARY_BACKGROUND_COLOR); */

	/* setup the title */
	if (view->details->logged_in == FALSE) {
		title = create_services_title_widget ("You are not logged in!.");
	}
	else {
		char	*title_string;

		title_string = g_strdup_printf ("Welcome Back %s!", view->details->user_name);
		title = create_services_title_widget (title_string);
		g_free (title_string);
	}
	gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* Create the Parent Table to hold the 4 frames */
	parent = GTK_TABLE (gtk_table_new (3, 2, FALSE));
	g_print ("main\n");
	/* Create the Services Listing Frame */
	frame = gtk_frame_new ("");
	gtk_widget_show (frame);
	g_print ("services start\n");
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 1,
			  0, 1,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* create the parent services list box */
	temp_box = gtk_vbox_new (FALSE, 0);

	/* setup the title */
	title = create_summary_service_large_grey_header_widget ("Services");
	gtk_box_pack_start (GTK_BOX (temp_box), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* Create the parent table to hold 5 rows */
	view->details->services_table = GTK_TABLE (gtk_table_new (5, 3, FALSE));

	/* build the services table from the xml file */
	for (iterator = view->details->xml_data->services_list; iterator; iterator = g_list_next (iterator)) {

		view->details->current_service_row++;
		service_node = iterator->data;
		view->details->services_icon_name = service_node->icon;
		view->details->services_description_header = service_node->description_header;
		view->details->services_description_body = service_node->description;
		view->details->services_goto_label = service_node->button_label;
		view->details->services_redirects[view->details->current_service_row - 1] = service_node->uri;
		generate_service_entry_row (view, view->details->current_service_row);

	}

	g_list_free (iterator);

	/* draw parent vbox and connect it to the login frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->services_table), 0, 0, 0);
	gtk_widget_show (GTK_WIDGET (view->details->services_table));
	gtk_widget_show (temp_box);
	gtk_container_add (GTK_CONTAINER (frame), temp_box);
	g_print ("services end\n");

	/* Create the Login Frame */
	g_print ("login start\n");
	frame = gtk_frame_new ("");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  1, 2,
			  0, 1,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* create the parent login box  */
	temp_box = gtk_vbox_new (TRUE, 0);

	/* setup the title */
	title = create_summary_service_large_grey_header_widget ("Options");
	gtk_box_pack_start (GTK_BOX (temp_box), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	if (view->details->logged_in == FALSE) {
		/* username label */
		g_print ("label unl start\n");
		temp_hbox = gtk_hbox_new (FALSE, 0);
		view->details->username_label = nautilus_label_new ("User Name:");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->username_label), 16);
		gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->username_label, FALSE, FALSE, 0);
		gtk_widget_show (view->details->username_label);
		gtk_widget_show (temp_hbox);
		gtk_box_pack_start (GTK_BOX (temp_box), temp_hbox, FALSE, FALSE, 4);
		g_print ("label unl end\n");

		/* username text entry */
		g_print ("label unt start\n");
		temp_hbox = gtk_hbox_new (FALSE, 4);
		view->details->username_entry = gtk_entry_new_with_max_length (36);
		gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->username_entry, FALSE, FALSE, 0);
		gtk_widget_show (view->details->username_entry);
		gtk_widget_show (temp_hbox);
		gtk_box_pack_start (GTK_BOX (temp_box), temp_hbox, FALSE, FALSE, 4);
		g_print ("label unt end\n");

		/* password label */
		g_print ("label pwl start\n");
		temp_hbox = gtk_hbox_new (FALSE, 0);
		view->details->password_label = nautilus_label_new ("Password:");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->password_label), 16);
		gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->password_label, FALSE, FALSE, 0);
		gtk_widget_show (view->details->password_label);
		gtk_widget_show (temp_hbox);
		gtk_box_pack_start (GTK_BOX (temp_box), temp_hbox, FALSE, FALSE, 4);
		g_print ("label pwl end\n");

		/* password text entry */
		g_print ("label pwt start\n");
		temp_hbox = gtk_hbox_new (FALSE, 4);
		view->details->password_entry = gtk_entry_new_with_max_length (36);
		gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->password_entry, FALSE, FALSE, 0);
		gtk_entry_set_visibility (GTK_ENTRY (view->details->password_entry), FALSE);
		gtk_widget_show (view->details->password_entry);
		gtk_widget_show (temp_hbox);
		gtk_box_pack_start (GTK_BOX (temp_box), temp_hbox, FALSE, FALSE, 4);
		g_print ("label pwt end\n");

		gtk_signal_connect (GTK_OBJECT (view->details->username_entry), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);
		gtk_signal_connect (GTK_OBJECT (view->details->password_entry), "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);

		/* login button */
		g_print ("label lib start\n");
		view->details->login_button = gtk_button_new ();
		view->details->login_label = nautilus_label_new (" I'm ready to login! ");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->login_label), 12);
		gtk_widget_show (view->details->login_label);
		gtk_container_add (GTK_CONTAINER (view->details->login_button), view->details->login_label);
		button_box = gtk_hbox_new (TRUE, 0);
		gtk_box_pack_start (GTK_BOX (button_box), view->details->login_button, FALSE, FALSE, 21);
		gtk_signal_connect (GTK_OBJECT (view->details->login_button), "clicked", GTK_SIGNAL_FUNC (login_button_cb), view);
		gtk_widget_set_sensitive (view->details->login_button, FALSE);
		gtk_widget_show (view->details->login_button);
		gtk_widget_show (button_box);
		gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);
		g_print ("label lib end\n");

		/* register button */
		g_print ("label rb start\n");
		view->details->register_button = gtk_button_new ();
		view->details->register_label = nautilus_label_new ("    Register Now!    ");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->register_label), 12);
		gtk_widget_show (view->details->register_label);
		gtk_container_add (GTK_CONTAINER (view->details->register_button), view->details->register_label);
		button_box = gtk_hbox_new (TRUE, 0);
		gtk_box_pack_start (GTK_BOX (button_box), view->details->register_button, FALSE, FALSE, 21);
		gtk_signal_connect (GTK_OBJECT (view->details->register_button), "clicked", GTK_SIGNAL_FUNC (register_button_cb), view);
		gtk_widget_show (view->details->register_button);
		gtk_widget_show (button_box);
		gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);
		g_print ("label rb end\n");

		/* Add a label for error status messages */
		g_print ("label es start\n");
		view->details->feedback_text = nautilus_label_new ("");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->feedback_text), 12);
		nautilus_label_set_text_color (NAUTILUS_LABEL (view->details->feedback_text), NAUTILUS_RGB_COLOR_RED);
		gtk_box_pack_end (GTK_BOX (temp_box), view->details->feedback_text, 0, 0, 15);
		g_print ("label es end\n");
	}
	else {
		/* preferences button */
		view->details->preferences_button = gtk_button_new ();
		view->details->preferences_label = nautilus_label_new (" Account Preferences ");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->preferences_label), 12);
		gtk_widget_show (view->details->preferences_label);
		gtk_container_add (GTK_CONTAINER (view->details->preferences_button), view->details->preferences_label);
		button_box = gtk_hbox_new (TRUE, 0);
		gtk_box_pack_start (GTK_BOX (button_box), view->details->preferences_button, FALSE, FALSE, 21);
		gtk_signal_connect (GTK_OBJECT (view->details->preferences_button), "clicked", GTK_SIGNAL_FUNC (preferences_button_cb), view);
		gtk_widget_show (view->details->preferences_button);
		gtk_widget_show (button_box);
		gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);

		/* logout button */
		view->details->logout_button = gtk_button_new ();
		view->details->logout_label = nautilus_label_new ("     Log me out!     ");
		nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->logout_label), 12);
		gtk_widget_show (view->details->logout_label);
		gtk_container_add (GTK_CONTAINER (view->details->logout_button), view->details->logout_label);
		button_box = gtk_hbox_new (TRUE, 0);
		gtk_box_pack_start (GTK_BOX (button_box), view->details->logout_button, FALSE, FALSE, 21);
		gtk_signal_connect (GTK_OBJECT (view->details->logout_button), "clicked", GTK_SIGNAL_FUNC (logout_button_cb), view);
		gtk_widget_show (view->details->logout_button);
		gtk_widget_show (button_box);
		gtk_box_pack_start (GTK_BOX (temp_box), button_box, FALSE, FALSE, 4);
	}

	/* draw parent vbox and connect it to the login frame */
	gtk_widget_show (temp_box);
	gtk_container_add (GTK_CONTAINER (frame), temp_box);
	g_print ("login end\n");

	/* Create the Service News Frame */
	g_print ("service news start\n");
	frame = gtk_frame_new ("");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 2,
			  1, 2,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* create the parent service news box and a table to hold the data */
	temp_box = gtk_vbox_new (FALSE, 0);
	/* setup the large grey header */
	title = create_summary_service_large_grey_header_widget ("Eazel News and Updates");
	gtk_box_pack_start (GTK_BOX (temp_box), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* setup the small grey header */
	small_title = create_summary_service_small_grey_header_widget ("Service News");
	gtk_box_pack_start (GTK_BOX (temp_box), small_title, FALSE, FALSE, 0);
	gtk_widget_show (small_title);

	view->details->service_news_table = GTK_TABLE (gtk_table_new (4, 2, FALSE));

	/* build the eazel news table from the xml file */
	for (iterator = view->details->xml_data->eazel_news_list; iterator; iterator = g_list_next (iterator)) {

		view->details->current_news_row++;
		eazel_news_node = iterator->data;
		view->details->news_icon_name = eazel_news_node->icon;
		view->details->news_description_body = eazel_news_node->message;
		generate_eazel_news_entry_row (view, view->details->current_news_row);

	}

	g_list_free (iterator);

	/* draw parent vbox and connect it to the login frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->service_news_table), 0, 0, 0);
	gtk_widget_show (GTK_WIDGET (view->details->service_news_table));
	gtk_widget_show (temp_box);
	gtk_container_add (GTK_CONTAINER (frame), temp_box);
	g_print ("service news end\n");

	/* Create the Update News Frame */
	g_print ("update news start\n");
	frame = gtk_frame_new ("");
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_table_attach (GTK_TABLE (parent), frame,
			  0, 2,
			  2, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);

	/* create the parent login box and a table to hold the labels and text entries */
	temp_box = gtk_vbox_new (FALSE, 0);

	/* setup the small grey header */
	small_title = create_summary_service_small_grey_header_widget ("Software Updates");
	gtk_box_pack_start (GTK_BOX (temp_box), small_title, FALSE, FALSE, 0);
	gtk_widget_show (small_title);

	/* create the default update table with 4 rows */
	view->details->updates_table = GTK_TABLE (gtk_table_new (4, 3, FALSE));

	/* build the updates table from the xml file */
	for (iterator = view->details->xml_data->update_news_list; iterator; iterator = g_list_next (iterator)) {

		view->details->current_update_row++;
		update_news_node = iterator->data;
		view->details->update_icon_name = update_news_node->icon;
		view->details->update_description_header = update_news_node->name;
		view->details->update_description_version = update_news_node->version;
		view->details->update_description_body = update_news_node->description;
		view->details->update_goto_label = update_news_node->button_label;
		view->details->update_redirects[view->details->current_update_row - 1] = update_news_node->uri;
		generate_update_news_entry_row (view, view->details->current_update_row);

	}

	g_list_free (iterator);

	/* draw parent vbox and connect it to the login frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->updates_table), 0, 0, 0);
	gtk_widget_show (GTK_WIDGET (view->details->updates_table));
	gtk_widget_show (temp_box);
	gtk_container_add (GTK_CONTAINER (frame), temp_box);

	/* draw the parent frame box */
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET (parent), 0, 0, 4);
	gtk_widget_show (GTK_WIDGET (parent));
	g_print ("update news end\n");

}

static void
generate_service_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget			*temp_vbox;
	GtkWidget			*temp_hbox;
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first column with service icon */
	view->details->services_icon_container = gtk_hbox_new (TRUE, 4);
	view->details->services_icon_widget = create_image_widget (view->details->services_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->services_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->services_icon_container), view->details->services_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->services_icon_widget);
	gtk_table_attach (view->details->services_table, view->details->services_icon_container, 0, 1, row - 1 , row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->services_icon_container);

	/* Generate second Column with service title and summary */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	/* Header */
	temp_hbox = gtk_hbox_new (TRUE, 0);
	view->details->services_description_header_widget = nautilus_label_new (view->details->services_description_header);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->services_description_header_widget), 12);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->services_description_header_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->services_description_header_widget);
	gtk_container_add (GTK_CONTAINER (temp_hbox), view->details->services_description_header_widget);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);
	gtk_widget_show (temp_hbox);
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	view->details->services_description_body_widget = nautilus_label_new (view->details->services_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->services_description_body_widget), 12);
	gtk_widget_show (view->details->services_description_body_widget);
	gtk_container_add (GTK_CONTAINER (temp_hbox), view->details->services_description_body_widget);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);
	gtk_widget_show (temp_hbox);

	gtk_table_attach (view->details->services_table, temp_vbox, 1, 2, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (temp_vbox);

	/* Add the redirect button to the third column */
	view->details->services_button_container = gtk_hbox_new (TRUE, 0);
	view->details->services_goto_button = gtk_button_new ();
	view->details->services_goto_label_widget = nautilus_label_new (view->details->services_goto_label);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->services_goto_label_widget), 12);
	gtk_widget_show (view->details->services_goto_label_widget);
	gtk_container_add (GTK_CONTAINER (view->details->services_goto_button), view->details->services_goto_label_widget);
	gtk_box_pack_start (GTK_BOX (view->details->services_button_container), view->details->services_goto_button, FALSE, FALSE, 13);
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->services_redirects[view->details->current_service_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->services_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_service_cb), cbdata);
	gtk_widget_show (view->details->services_goto_button);
	gtk_table_attach (view->details->services_table, view->details->services_button_container, 2, 3, row - 1, row, 0, 0, 0, 0);
	gtk_widget_show (view->details->services_button_container);

}

static void
generate_eazel_news_entry_row  (NautilusSummaryView	*view, int	row)
{

	/* Generate first column with icon */
	view->details->news_icon_container = gtk_hbox_new (TRUE, 4);
	view->details->news_icon_widget = create_image_widget (view->details->news_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->news_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->news_icon_container), view->details->news_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->news_icon_widget);
	gtk_table_attach (view->details->service_news_table, view->details->news_icon_container, 0, 1, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->news_icon_container);

	/* Generate second Column with news content */
	view->details->news_description_body_widget = nautilus_label_new (view->details->news_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->news_description_body_widget), 12);
	gtk_table_attach (view->details->service_news_table, view->details->news_description_body_widget, 1, 2, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->news_description_body_widget);

}

static void
generate_update_news_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget	*temp_vbox;
	GtkWidget	*temp_hbox;
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first column with icon */
	view->details->update_icon_container = gtk_hbox_new (TRUE, 4);
	view->details->update_icon_widget = create_image_widget (view->details->update_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->update_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->update_icon_container), view->details->update_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->update_icon_widget);
	gtk_table_attach (view->details->updates_table, view->details->update_icon_container, 0, 1, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->update_icon_container);

	/* Generate second Column with update title, summary, and version */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	/* Header */
	temp_hbox = gtk_hbox_new (TRUE, 0);
	view->details->update_description_header_widget = nautilus_label_new (view->details->update_description_header);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_header_widget), 14);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->update_description_header_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->update_description_header_widget);
	gtk_container_add (GTK_CONTAINER (temp_hbox), view->details->update_description_header_widget);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);
	gtk_widget_show (temp_hbox);
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	view->details->update_description_body_widget = nautilus_label_new (view->details->update_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_body_widget), 12);
	gtk_widget_show (view->details->update_description_body_widget);
	gtk_container_add (GTK_CONTAINER (temp_hbox), view->details->update_description_body_widget);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);
	gtk_widget_show (temp_hbox);
	/* Version */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	view->details->update_description_version_widget = nautilus_label_new (view->details->update_description_version);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_version_widget), 12);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->update_description_version_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->update_description_version_widget);
	gtk_container_add (GTK_CONTAINER (temp_hbox), view->details->update_description_version_widget);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);
	gtk_widget_show (temp_hbox);

	gtk_table_attach (view->details->updates_table, temp_vbox, 1, 2, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (temp_vbox);

	/* Add the redirect button to the third column */
	view->details->update_button_container = gtk_hbox_new (TRUE, 0);
	view->details->update_goto_button = gtk_button_new ();
	view->details->update_goto_label_widget = nautilus_label_new (view->details->update_goto_label);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_goto_label_widget), 12);
	gtk_widget_show (view->details->update_goto_label_widget);
	gtk_container_add (GTK_CONTAINER (view->details->update_goto_button), view->details->update_goto_label_widget);
	gtk_box_pack_start (GTK_BOX (view->details->update_button_container), view->details->update_goto_button, FALSE, FALSE, 13);
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->update_redirects[view->details->current_update_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->update_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_update_cb), cbdata);
	gtk_widget_show (view->details->update_goto_button);
	gtk_table_attach (view->details->updates_table, view->details->update_button_container, 2, 3, row - 1, row, 0, 0, 0, 0);
	gtk_widget_show (view->details->update_button_container);

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

	button_enabled = 	user_name && strlen (user_name) && password && strlen (password)
				&& (Pending_None == view->details->pending_operation);

	gtk_widget_set_sensitive (view->details->login_button, button_enabled);

}

static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;
	gint			timeout;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;
	gtk_widget_set_sensitive (view->details->login_button, TRUE);
	
	g_message ("Login succeeded");
	timeout = gtk_timeout_add (100, logged_in_callback, view);

	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
	g_print ("finish success callback\n");
}

static void
authn_cb_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;

	gtk_widget_set_sensitive (view->details->login_button, TRUE);
	gtk_entry_set_text (GTK_ENTRY (view->details->username_entry), "");
	gtk_entry_set_text (GTK_ENTRY (view->details->password_entry), "");
	g_message ("Login FAILED");
	view->details->logged_in = FALSE;
	gtk_widget_set_sensitive (view->details->login_button, FALSE);
	show_feedback (view->details->feedback_text, "Login Failed Please try again.");
	
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
}

/* callback to handle the login button.  Right now only does a simple redirect. */
static void
login_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char		*user_name;
	char		*password;
	EazelProxy_AuthnInfo *authinfo;
	CORBA_Environment ev;

	AmmoniteAuthCallbackWrapperFuncs cb_funcs = {
		authn_cb_succeeded, authn_cb_failed
	};

	CORBA_exception_init (&ev);

	g_assert (Pending_None == view->details->pending_operation);

	gtk_widget_set_sensitive (view->details->login_button, FALSE);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		view->details->authn_callback = ammonite_auth_callback_wrapper_new (bonobo_poa(), &cb_funcs, view);

		user_name = gtk_entry_get_text (GTK_ENTRY (view->details->username_entry));
		view->details->user_name = user_name;
		password = gtk_entry_get_text (GTK_ENTRY (view->details->password_entry));

		authinfo = EazelProxy_AuthnInfo__alloc ();
		authinfo->username = CORBA_string_dup (user_name);
		authinfo->password = CORBA_string_dup (password);
		authinfo->services_redirect_uri = CORBA_string_dup ("");
		authinfo->services_login_path = CORBA_string_dup ("");

		/* Ref myself until the callback returns */
		bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));

		view->details->pending_operation = Pending_Login;
		
		EazelProxy_UserControl_authenticate_user (
			view->details->user_control,
			authinfo, TRUE, 
			view->details->authn_callback, &ev
		);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_warning ("Exception during EazelProxy login");
			/* FIXME bugzilla.eazel.com 2745: cleanup after fail here */ 
		}

		g_free (user_name);
		g_free (password);

	}
	
	CORBA_exception_free (&ev);

}

/* callback to handle the logout button.  Right now only does a simple redirect. */
static void
logout_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	CORBA_Environment	ev;
	EazelProxy_UserList	*users;
	CORBA_unsigned_long	i;
	gint			timeout;
	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		/* Get list of currently active users */

		users = EazelProxy_UserControl_get_active_users (
			view->details->user_control, &ev
		);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("Exception while logging out user");
			return;
		}

		/* Log out the current default user */
		for (i = 0; i < users->_length ; i++) {
			EazelProxy_User *cur;

			cur = users->_buffer + i;

			if (cur->is_default) {
				g_message ("Logging out user '%s'", cur->user_name);
				EazelProxy_UserControl_logout_user (
					view->details->user_control,
					cur->proxy_port, &ev
				);
				break;
			}
		}

		CORBA_free (users);
	}

	timeout = gtk_timeout_add (100, logged_out_callback, view);

	CORBA_exception_free (&ev);
}

static gboolean
am_i_logged_in (NautilusSummaryView	*view)
{
	CORBA_Environment	ev;
	EazelProxy_User		*user;
	gboolean		rv;

	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		/* Get list of currently active users */

		user = EazelProxy_UserControl_get_default_user (
			view->details->user_control, &ev);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("No default user!");
			rv =  FALSE;
		}
		else {
			g_message ("Default user found!");
			CORBA_free (user);
			rv = TRUE;
		}
	}

	CORBA_exception_free (&ev);
	return rv;
} /* end am_i_logged_in */

static gint
logged_in_callback (gpointer	raw)
{

	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = TRUE;
	/* dispose of startup form that was shown */
	gtk_widget_destroy (view->details->form);
	view->details->form = NULL;

	generate_startup_form (view);

	return (FALSE);
}

static gint
logged_out_callback (gpointer	raw)
{

	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = FALSE;
	/* dispose of startup form that was shown */
	gtk_widget_destroy (view->details->form);
	view->details->form = NULL;

	generate_startup_form (view);

	return (FALSE);
}

/* callback to handle the maintenance button.  Right now only does a simple redirect. */
static void
preferences_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	go_to_uri (view->details->nautilus_view, PREFERENCES_HOME);

}

/* callback to handle the register button.  Right now only does a simple redirect. */
static void
register_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	go_to_uri (view->details->nautilus_view, REGISTER_HOME);

}

/* callback to handle the goto a service button. */
static void
goto_service_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	go_to_uri (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle install netscape  button.  Right now only does a simple redirect. */
static void
goto_update_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	go_to_uri (cbdata->nautilus_view, cbdata->uri);

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
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	view->details = g_new0 (NautilusSummaryViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (summary_load_location_callback), 
			    view);

	view->details->user_control = (EazelProxy_UserControl) oaf_activate_from_id (IID_EAZELPROXY, 0, NULL, &ev);

	if ( CORBA_NO_EXCEPTION != ev._major ) {
		/* FIXME bugzilla.eazel.com 2740: user should be warned that Ammonite may not be installed */
		g_warning ("Couldn't instantiate eazel-proxy\n");
		view->details->user_control = CORBA_OBJECT_NIL;
	}

	gtk_widget_show (GTK_WIDGET (view));

	CORBA_exception_free (&ev);

}

static void
nautilus_summary_view_destroy (GtkObject *object)
{

	NautilusSummaryView	*view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	view = NAUTILUS_SUMMARY_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}

	g_assert (Pending_None == view->details->pending_operation);
	CORBA_Object_release (view->details->user_control, &ev);

	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

	CORBA_exception_free (&ev);

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

	generate_startup_form (view);

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

