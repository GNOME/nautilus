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
#include <bonobo/bonobo-control.h>
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
#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

#define DEFAULT_SUMMARY_BACKGROUND_COLOR	"rgb:FFFF/FFFF/FFFF"

#define	URL_REDIRECT_TABLE_HOME			"eazel-services://anonymous/services/urls"
#define	URL_REDIRECT_TABLE_HOME_2		"eazel-services:/services/urls"

#define SUMMARY_XML_KEY				"eazel_summary_xml"
#define URL_REDIRECT_TABLE			"eazel_url_table_xml"
#define REGISTER_KEY				"eazel_service_register"
#define PREFERENCES_KEY				"eazel_service_account_maintenance"

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
	BonoboUIComponent *ui_component;
	
	SummaryData	*xml_data;
	char		*feedback_text;

	/* Parent form and title */
	GtkWidget	*form;
	GtkWidget	*form_title;

	/* Login State */
	char			*user_name;
	volatile gboolean	logged_in;

	GtkWidget		*caption_table;

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
	gboolean	services_button_enabled;

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
static void	generate_summary_form			(NautilusSummaryView		*view);
static void	generate_service_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	generate_eazel_news_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	generate_update_news_entry_row		(NautilusSummaryView		*view,
							 int				row);
static void	login_button_cb				(GtkWidget			*button,
							 NautilusSummaryView		*view); 
/* static void	preferences_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view); */
/* static void	logout_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view); */
static void	goto_service_cb				(GtkWidget			*button,
							 ServicesButtonCallbackData	*cbdata);
static void	goto_update_cb				(GtkWidget			*button,
							 ServicesButtonCallbackData	*cbdata);
static void	register_button_cb			(GtkWidget			*button,
							 NautilusSummaryView		*view);
static gboolean	am_i_logged_in				(NautilusSummaryView		*view);
static char*	who_is_logged_in			(NautilusSummaryView		*view);
static gint	logged_in_callback			(gpointer			raw);
/* static gint	logged_out_callback			(gpointer			raw); */
static void	generate_error_dialog			(NautilusSummaryView		*view);
static void	generate_login_dialog			(NautilusSummaryView		*view);
static void	widget_set_nautilus_background_color	(GtkWidget			*widget,
							 const char			*color);
static void	merge_bonobo_menu_items			(BonoboControl *control,
							 gboolean state,
							 gpointer user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

static void
generate_summary_form (NautilusSummaryView	*view)
{

	GtkWidget		*frame;
	GtkWidget		*title;
	GtkWidget		*notebook;
	GtkWidget		*tab_label_widget;
	GtkWidget		*temp_box;
	ServicesData		*service_node;
	EazelNewsData		*eazel_news_node;
	UpdateNewsData		*update_news_node;
	GList			*iterator;
	GtkWidget		*temp_scrolled_window;
	GtkWidget 		*viewport;
	GtkWidget		*temp_hbox;
	GtkWidget		*temp_vbox;
	GtkWidget		*temp_label;

	view->details->current_service_row = 0;
	view->details->current_news_row = 0;
	view->details->current_update_row = 0;

	if (view->details->form != NULL) {
		gtk_container_remove (GTK_CONTAINER (view), view->details->form);
		view->details->form = NULL;
	}

	g_print ("Start summary view load.\n");

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);

	/* setup the title */
	if (!view->details->logged_in) {
		title = create_services_title_widget ("You are not logged in!.");
		widget_set_nautilus_background_color (title, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	}
	else {
		char	*title_string;

		g_free (view->details->user_name);
		view->details->user_name = who_is_logged_in (view);
		title_string = g_strdup_printf ("Welcome Back %s!", view->details->user_name);
		g_print ("%s\n", title_string);
		title = create_services_title_widget (title_string);
		widget_set_nautilus_background_color (title, DEFAULT_SUMMARY_BACKGROUND_COLOR);
		g_free (title_string);
	}
	gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
	gtk_widget_show (title);

	/* Create the Service News Frame */
	frame = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (view->details->form), frame, FALSE, FALSE, 0);

	/* create the service news scroll widget */
	temp_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (temp_scrolled_window);
	viewport = gtk_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_scrolled_window),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* create the parent service news box and a table to hold the data */
	temp_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_box);

	view->details->service_news_table = GTK_TABLE (gtk_table_new (4, 2, FALSE));
	gtk_widget_show (GTK_WIDGET (view->details->service_news_table));

	/* build the eazel news table from the xml file */
	for (iterator = view->details->xml_data->eazel_news_list; iterator; iterator = g_list_next (iterator)) {

		view->details->current_news_row++;
		eazel_news_node = iterator->data;
		view->details->news_icon_name = eazel_news_node->icon;
		view->details->news_description_body = eazel_news_node->message;
		generate_eazel_news_entry_row (view, view->details->current_news_row);

		g_free (eazel_news_node);

	}

	g_list_free (view->details->xml_data->eazel_news_list);

	/* draw parent vbox and connect it to the service news frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->service_news_table), TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (viewport), temp_box);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);


	/* Create the notebook container for services */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_container_add (GTK_CONTAINER (view->details->form), notebook);

	/* Create the tab_label_widget */
	tab_label_widget = nautilus_label_new ("Your Services");
	nautilus_label_set_font_size (NAUTILUS_LABEL (tab_label_widget), 16);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (tab_label_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (tab_label_widget);

	/* Create the Services Listing Box */
	frame = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (frame);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, tab_label_widget);

	/* create the services scroll widget */
	temp_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (temp_scrolled_window);
	viewport = gtk_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_scrolled_window),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* create the parent services list box */
	temp_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_box);

	/* Create the parent table to hold 5 rows */
	view->details->services_table = GTK_TABLE (gtk_table_new (5, 3, FALSE));
	gtk_widget_show (GTK_WIDGET (view->details->services_table));

	/* build the services table from the xml file */
	for (iterator = view->details->xml_data->services_list; iterator; iterator = g_list_next (iterator)) {

		view->details->current_service_row++;
		service_node = iterator->data;
		view->details->services_icon_name = service_node->icon;
		view->details->services_description_header = service_node->description_header;
		view->details->services_description_body = service_node->description;
		view->details->services_goto_label = service_node->button_label;
		view->details->services_button_enabled = service_node->enabled;
		view->details->services_redirects[view->details->current_service_row - 1] = service_node->uri;
		generate_service_entry_row (view, view->details->current_service_row);

		g_free (service_node);

	}

	g_list_free (view->details->xml_data->services_list);

	/* draw parent vbox and connect it to the login frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->services_table), 0, 0, 0);
	gtk_container_add (GTK_CONTAINER (viewport), temp_box);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);

	/* create the Additional Services pane */

	tab_label_widget = nautilus_label_new ("Additional Services");
	nautilus_label_set_font_size (NAUTILUS_LABEL (tab_label_widget), 16);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (tab_label_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (tab_label_widget);

	temp_vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_show (temp_vbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), temp_vbox, tab_label_widget);

	temp_hbox = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (temp_hbox);

	/* create the services scroll widget */
	temp_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (temp_scrolled_window);
	viewport = gtk_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_scrolled_window),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	temp_label = nautilus_label_new ("There are no additional services available at this time");
	nautilus_label_set_font_size (NAUTILUS_LABEL (temp_label), 18);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (temp_label),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (temp_label);
	gtk_box_pack_start (GTK_BOX (temp_hbox), temp_label, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (viewport), temp_hbox);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_scrolled_window, TRUE, TRUE, 0);

	/* Create the notebook container for services */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_container_add (GTK_CONTAINER (view->details->form), notebook);

	/* Create the tab_label_widget */
	tab_label_widget = nautilus_label_new ("Current Updates");
	nautilus_label_set_font_size (NAUTILUS_LABEL (tab_label_widget), 16);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (tab_label_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (tab_label_widget);
	/* Create the Update News Frame */
	frame = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (frame);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, tab_label_widget);

	/* create the update news scroll widget */
	temp_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (temp_scrolled_window);
	viewport = gtk_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_scrolled_window),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* create the parent update news box and a table to hold the labels and text entries */
	temp_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_box);

	/* create the default update table with 4 rows */
	view->details->updates_table = GTK_TABLE (gtk_table_new (4, 3, FALSE));
	gtk_widget_show (GTK_WIDGET (view->details->updates_table));

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

		g_free (update_news_node);

	}

	g_list_free (view->details->xml_data->update_news_list);

	g_free (view->details->xml_data);
	view->details->xml_data = NULL;

	/* draw parent vbox and connect it to the update news frame */
	gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->updates_table), TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (viewport), temp_box);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);

	/* Finally, show the form that hold everything */
	gtk_widget_show (view->details->form);
	g_print ("Load summary view end.\n");
}

static void
generate_service_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget			*temp_vbox;
	GtkWidget			*temp_hbox;
	GdkFont				*font;
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first column with service icon */
	view->details->services_icon_container = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (view->details->services_icon_container);
	view->details->services_icon_widget = create_image_widget (view->details->services_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->services_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->services_icon_container), view->details->services_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->services_icon_widget);
	gtk_table_attach (view->details->services_table, view->details->services_icon_container, 0, 1, row - 1 , row, GTK_FILL, GTK_FILL, 0, 0);

	/* Generate second Column with service title and summary */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_vbox);
	/* Header */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (temp_hbox);
	view->details->services_description_header_widget = nautilus_label_new (view->details->services_description_header);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->services_description_header_widget), 12);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->services_description_header_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->services_description_header_widget);
	g_free (view->details->services_description_header);
	view->details->services_description_header = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->services_description_header_widget, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (temp_hbox);
	view->details->services_description_body_widget = nautilus_label_new (view->details->services_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->services_description_body_widget), 12);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->services_description_body_widget), TRUE);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->services_description_body_widget), GTK_JUSTIFY_LEFT);
	gtk_widget_show (view->details->services_description_body_widget);
	g_free (view->details->services_description_body);
	view->details->services_description_body = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->services_description_body_widget, FALSE, FALSE, 6);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	gtk_table_attach (view->details->services_table, temp_vbox, 1, 2, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);

	/* Add the redirect button to the third column */
	view->details->services_button_container = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (view->details->services_button_container);
	view->details->services_goto_button = gtk_button_new ();
	gtk_widget_show (view->details->services_goto_button);
	gtk_widget_set_usize (view->details->services_goto_button, 140, -1);
	view->details->services_goto_label_widget = gtk_label_new (view->details->services_goto_label);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->services_goto_label_widget, font);
	gdk_font_unref (font);
	gtk_widget_show (view->details->services_goto_label_widget);
	g_free (view->details->services_goto_label);
	view->details->services_goto_label = NULL;
	
	gtk_container_add (GTK_CONTAINER (view->details->services_goto_button), view->details->services_goto_label_widget);
	gtk_box_pack_end (GTK_BOX (view->details->services_button_container), view->details->services_goto_button, FALSE, FALSE, 0);
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->services_redirects[view->details->current_service_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->services_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_service_cb), cbdata);

	gtk_table_attach (view->details->services_table, view->details->services_button_container, 2, 3, row - 1, row, 0, 0, 0, 0);

}

static void
generate_eazel_news_entry_row  (NautilusSummaryView	*view, int	row)
{

	/* Generate first column with icon */
	view->details->news_icon_container = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (view->details->news_icon_container);
	view->details->news_icon_widget = create_image_widget (view->details->news_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->news_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->news_icon_container), view->details->news_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->news_icon_widget);
	gtk_table_attach (view->details->service_news_table, view->details->news_icon_container, 0, 1, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	/* Generate second Column with news content */
	view->details->news_description_body_widget = nautilus_label_new (view->details->news_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->news_description_body_widget), 12);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->news_description_body_widget), TRUE);
	gtk_table_attach (view->details->service_news_table, view->details->news_description_body_widget, 1, 2, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->news_description_body_widget);
	g_free (view->details->news_description_body);
	view->details->news_description_body = NULL;

}

static void
generate_update_news_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget			*temp_vbox;
	GtkWidget			*temp_hbox;
	GdkFont				*font;
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first column with icon */
	view->details->update_icon_container = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (view->details->update_icon_container);
	view->details->update_icon_widget = create_image_widget (view->details->update_icon_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (view->details->update_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->update_icon_container), view->details->update_icon_widget, 0, 0, 0);
	gtk_table_attach (view->details->updates_table, view->details->update_icon_container, 0, 1, row - 1, row, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show (view->details->update_icon_widget);

	/* Generate second Column with update title, summary, and version */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_vbox);
	/* Header */
	temp_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (temp_hbox);
	view->details->update_description_header_widget = nautilus_label_new (view->details->update_description_header);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_header_widget), 14);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->update_description_header_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->update_description_header_widget);
	g_free (view->details->update_description_header);
	view->details->update_description_header = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_header_widget, FALSE, FALSE, 4);	
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (temp_hbox);
	view->details->update_description_body_widget = nautilus_label_new (view->details->update_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_body_widget), 12);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->update_description_body_widget), TRUE);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->update_description_body_widget), GTK_JUSTIFY_LEFT);
	gtk_widget_show (view->details->update_description_body_widget);
	g_free (view->details->update_description_body);
	view->details->update_description_body = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_body_widget, FALSE, FALSE, 6);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	/* Version */
	temp_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (temp_hbox);
	view->details->update_description_version_widget = nautilus_label_new (view->details->update_description_version);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->update_description_version_widget), 12);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->update_description_version_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	gtk_widget_show (view->details->update_description_version_widget);
	g_free (view->details->update_description_version);
	view->details->update_description_version = NULL;
	
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_version_widget, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);

	gtk_table_attach (view->details->updates_table, temp_vbox, 1, 2, row - 1, row, FALSE, FALSE, 0, 0);

	/* Add the redirect button to the third column */
	view->details->update_button_container = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (view->details->update_button_container);
	view->details->update_goto_button = gtk_button_new ();
	gtk_widget_show (view->details->update_goto_button);
	gtk_widget_set_usize (view->details->update_goto_button, 140, -1);
	view->details->update_goto_label_widget = gtk_label_new (view->details->update_goto_label);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->update_goto_label_widget, font);
	gdk_font_unref (font);
	gtk_widget_show (view->details->update_goto_label_widget);
	g_free (view->details->update_goto_label);
	view->details->update_goto_label = NULL;
	
	gtk_container_add (GTK_CONTAINER (view->details->update_goto_button), view->details->update_goto_label_widget);
	gtk_box_pack_start (GTK_BOX (view->details->update_button_container), view->details->update_goto_button, FALSE, FALSE, 13);
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->update_redirects[view->details->current_update_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->update_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_update_cb), cbdata);
	gtk_table_attach (view->details->updates_table, view->details->update_button_container, 2, 3, row - 1, row, 0, 0, 0, 0);

}

static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;
	gint			timeout;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;
	
	g_message ("Login succeeded");
	timeout = gtk_timeout_add (0, logged_in_callback, view);

	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
}

static void
authn_cb_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;

	g_message ("Login FAILED");
	view->details->logged_in = FALSE;
	view->details->feedback_text = g_strdup_printf ("Login Failed! Please try again.");
	generate_error_dialog (view);
	
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

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		view->details->authn_callback = ammonite_auth_callback_wrapper_new (bonobo_poa(), &cb_funcs, view);

		user_name = nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (view->details->caption_table), 0);
		password = nautilus_caption_table_get_entry_text (NAUTILUS_CAPTION_TABLE (view->details->caption_table), 1);

		authinfo = EazelProxy_AuthnInfo__alloc ();
		authinfo->username = CORBA_string_dup (user_name);
		authinfo->password = CORBA_string_dup (password);
		user_name = NULL;
		password = NULL;
				
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


	}
	
	CORBA_exception_free (&ev);

}

/*
callback to handle the logout button.  Right now only does a simple redirect.
static void
logout_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	CORBA_Environment	ev;
	EazelProxy_UserList	*users;
	CORBA_unsigned_long	i;
	gint			timeout;
	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
Get list of currently active users

		users = EazelProxy_UserControl_get_active_users (
			view->details->user_control, &ev
		);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("Exception while logging out user");
			return;
		}

Log out the current default user
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

	timeout = gtk_timeout_add (0, logged_out_callback, view);

	CORBA_exception_free (&ev);
}
*/

static gboolean
am_i_logged_in (NautilusSummaryView	*view)
{
	CORBA_Environment	ev;
	EazelProxy_User		*user;
	gboolean		rv = FALSE;

	rv = FALSE;
	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {

		user = EazelProxy_UserControl_get_default_user (
			view->details->user_control, &ev);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("No default user!");
			rv = FALSE;
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

static char*
who_is_logged_in (NautilusSummaryView	*view)
{
	CORBA_Environment	ev;
	EazelProxy_UserList	*user;
	char			*rv;
	int			i;

	rv = NULL;

	CORBA_exception_init (&ev);

	if (CORBA_OBJECT_NIL != view->details->user_control) {
		/* Get list of currently active users */

		user = EazelProxy_UserControl_get_active_users (
			view->details->user_control, &ev);

		if (CORBA_NO_EXCEPTION != ev._major) {
			g_message ("No default user!");
		} else {
			g_message ("Default user found!");
			for (i = 0; i < user->_length; i++) {
				EazelProxy_User *current;
	
				current = user->_buffer + i;
				if (current->is_default) {
					rv = g_strdup (current->user_name);
				}
			}
			CORBA_free (user);
		}
	}

	CORBA_exception_free (&ev);
	return rv;
} /* end who_is_logged_in */

static gint
logged_in_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = TRUE;

	go_to_uri (view->details->nautilus_view, "eazel:");

	return (FALSE);
}

/*
static gint
logged_out_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = FALSE;

	go_to_uri (view->details->nautilus_view, "eazel:");

	return (FALSE);
} */

/*
callback to handle the maintenance button.  Right now only does a simple redirect.
static void
preferences_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char	*url;
	url = NULL;

	url = trilobite_redirect_lookup (PREFERENCES_KEY);
	if (!url) {
		g_assert ("Failed to load Registration url!\n");
	}

	go_to_uri (view->details->nautilus_view, url);
	g_free (url);

}

*/

/* callback to handle the register button.  Right now only does a simple redirect. */
static void
register_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char	*url;
	url = NULL;

	url = trilobite_redirect_lookup (REGISTER_KEY);
	if (!url) {
		g_assert ("Failed to load Registration url!\n");
	}

	go_to_uri (view->details->nautilus_view, url);
	g_free (url);

}

/* callback to handle the goto a service button. */
static void
goto_service_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	go_to_uri (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle update buttons. */
static void
goto_update_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	go_to_uri (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle retry error_dialog button. */
static void
error_dialog_retry_cb (GtkWidget      *button, NautilusSummaryView	*view)
{
	
	go_to_uri (view->details->nautilus_view, "eazel:");

}

/* callback to handle cancel error_dialog button. */
static void
error_dialog_cancel_cb (GtkWidget      *button, NautilusSummaryView	*view)
{
	char	*user_home;
	user_home = nautilus_get_user_main_directory ();	
	go_to_uri (view->details->nautilus_view, user_home);
	g_free (user_home);

}

static void
generate_error_dialog (NautilusSummaryView *view)
{
	GtkWidget	*dialog;
	GtkWidget	*icon_container;
	GtkWidget	*icon_widget;
	GtkWidget	*hbox;
	GtkWidget	*error_text;
	int		reply;

	dialog = NULL;

	dialog = gnome_dialog_new (_("Service Error"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);

	icon_container = gtk_hbox_new (TRUE, 4);
	icon_widget = create_image_widget ("gnome-warning.png", DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (icon_container), icon_widget, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), icon_container, FALSE, FALSE, 0);
	gtk_widget_show (icon_widget);
	gtk_widget_show (icon_container);

	error_text = nautilus_label_new (view->details->feedback_text);
	nautilus_label_set_font_size (NAUTILUS_LABEL (error_text), 12);
	gtk_widget_show (error_text);
	gtk_box_pack_start (GTK_BOX (hbox), error_text, FALSE, FALSE, 0);


	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
/*	gnome_dialog_set_parent (GNOME_DIALOG (dialog), view->details->nautilus_view); */

	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (dialog));
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, GTK_SIGNAL_FUNC (error_dialog_retry_cb), view);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, GTK_SIGNAL_FUNC (error_dialog_cancel_cb), view);
	reply = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
generate_login_dialog (NautilusSummaryView	*view)
{
	GtkWidget	*dialog;
	GtkWidget	*hbox;
	GtkWidget	*pixmap;
	GtkWidget	*message;
	int		reply;

	dialog = NULL;
	pixmap = NULL;

	dialog = gnome_dialog_new (_("Services Login"), _("Register Now!"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	view->details->caption_table = nautilus_caption_table_new (2);
	gtk_widget_show (view->details->caption_table);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (view->details->caption_table),
					     0,
					     "Username:",
					     "",
					     TRUE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (view->details->caption_table),
					     1,
					     "Password:",
					     "",
					     FALSE,
					     FALSE);

	pixmap = create_image_widget ("nautilus-logo.png", DEFAULT_SUMMARY_BACKGROUND_COLOR);
	hbox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (hbox);

	if (pixmap) {
		gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
		gtk_widget_show (pixmap);
	}
	gtk_box_pack_end (GTK_BOX (hbox), view->details->caption_table, TRUE, TRUE, 0);

	gtk_box_set_spacing (GTK_BOX (GNOME_DIALOG (dialog)->vbox), 10);

	message = nautilus_label_new ("Please Login to services");
	gtk_widget_show (message);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    GTK_WIDGET (message),
			    TRUE,
			    TRUE,
			    0);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    hbox,
			    TRUE,
			    TRUE,
			    0);

	gtk_container_set_border_width (GTK_CONTAINER (view->details->caption_table), 4);

	gtk_widget_show (GNOME_DIALOG (dialog)->vbox);

	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, GTK_SIGNAL_FUNC (register_button_cb), view);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, GTK_SIGNAL_FUNC (login_button_cb), view);

	reply = gnome_dialog_run (GNOME_DIALOG (dialog));

}

static void
widget_set_nautilus_background_color (GtkWidget *widget, const char *color)
{
	NautilusBackground      *background;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (color != NULL);

	background = nautilus_get_widget_background (widget);

	nautilus_background_reset (background);
	nautilus_background_set_color (background, color);

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

	/* get notified when we are activated so we can merge in our menu items */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(view->details->nautilus_view)),
                            "activate",
                            merge_bonobo_menu_items,
                            view);

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

	if (view->details->ui_component) {
		bonobo_ui_component_unset_container (view->details->ui_component);
		bonobo_object_unref (BONOBO_OBJECT (view->details->ui_component));
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

	char		*url;
	gboolean	got_url_table;

	got_url_table = FALSE;
	url = NULL;

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* get xml data and verify network connections */
	view->details->logged_in = am_i_logged_in (view);

	if (view->details->logged_in) {
		/* fetch urls */
		got_url_table = trilobite_redirect_fetch_table (URL_REDIRECT_TABLE_HOME_2);
		if (!got_url_table) {
			view->details->feedback_text = g_strdup_printf ("Unable to connect to Eazel servers!");
			generate_error_dialog (view);
		}
		/* fetch and parse the xml file */
		url = trilobite_redirect_lookup (SUMMARY_XML_KEY);
		if (!url) {
			g_assert ("Failed to get summary xml home !\n");
		}
		view->details->xml_data = parse_summary_xml_file (url);
		g_free (url);
		if (view->details->xml_data == NULL) {
			view->details->feedback_text = g_strdup_printf ("Unable to connect to Eazel servers!");
			generate_error_dialog (view);
		}

	}
	else {

		/* fetch urls */
		got_url_table = trilobite_redirect_fetch_table (URL_REDIRECT_TABLE_HOME);
		if (!got_url_table) {
			view->details->feedback_text = g_strdup_printf ("Unable to connect to Eazel servers!");
			generate_error_dialog (view);
		}

		/* fetch and parse the xml file */
		url = trilobite_redirect_lookup (SUMMARY_XML_KEY);
		if (!url) {
			g_assert ("Failed to get summary xml home !\n");
		}
		view->details->xml_data = parse_summary_xml_file (url);
		g_free (url);
		if (view->details->xml_data == NULL) {
			view->details->feedback_text = g_strdup_printf ("Unable to connect to Eazel servers!");
			generate_error_dialog (view);
		}
	}

	generate_summary_form (view);

	if (!view->details->logged_in) {
		generate_login_dialog (view);
	}

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

static void
bonobo_register_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	register_button_cb (NULL, view);
}

static void
bonobo_login_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	generate_login_dialog (view);
}

static void
merge_bonobo_menu_items (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusSummaryView *view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Register", bonobo_register_callback),
		BONOBO_UI_VERB ("Login", bonobo_login_callback),
		BONOBO_UI_VERB_END
	};

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);

	if (state) {
		view->details->ui_component = nautilus_view_set_up_ui (view->details->nautilus_view,
							DATADIR,
							"nautilus-summary-view-ui.xml",
							"nautilus-summary-view");
									
		bonobo_ui_component_add_verb_list_with_data (view->details->ui_component, verbs, view);
	
	
	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are never explicitly deactivated
	 */
}

