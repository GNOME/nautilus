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

#include "eazel-services-footer.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include <gnome-xml/tree.h>
#include <bonobo/bonobo-control.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-tabs.h>

#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

#define notDEBUG_TEST	1
#define notDEBUG_PEPPER	1

#define DEFAULT_SUMMARY_BACKGROUND_COLOR	"rgb:FFFF/FFFF/FFFF"

#define	URL_REDIRECT_TABLE_HOME			"eazel-services://anonymous/services/urls"
#define	URL_REDIRECT_TABLE_HOME_2		"eazel-services:/services/urls"
#define	SUMMARY_CONFIG_XML			"eazel-services://anonymous/services"
#define	SUMMARY_CONFIG_XML_2			"eazel-services:/services"

#define SUMMARY_TERMS_OF_USE_URI		"eazel-services://anonymous/aboutus/terms_of_use"
#define SUMMARY_PRIVACY_STATEMENT_URI		"eazel-services://anonymous/aboutus/privacy"
#define SUMMARY_CHANGE_PWD_FORM			"eazel-services://anonymous/account/login/lost_pwd_form"

#define SUMMARY_XML_KEY				"eazel_summary_xml"
#define URL_REDIRECT_TABLE			"eazel_url_table_xml"
#define REGISTER_KEY				"eazel_service_register"
#define PREFERENCES_KEY				"eazel_service_account_maintenance"

#define	GOTO_BUTTON_LABEL			_("Go There!")
#define	SOFTCAT_GOTO_BUTTON_LABEL		_("More Info!")
#define	INSTALL_GOTO_BUTTON_LABEL		_("Install Me!")

#define MAX_IMAGE_WIDTH				50
#define MAX_IMAGE_HEIGHT			50

enum {
	LOGIN_DIALOG_NAME_ROW,
	LOGIN_DIALOG_PASSWORD_ROW,
	LOGIN_DIALOG_ROW_COUNT
};

enum {
	LOGIN_DIALOG_REGISTER_BUTTON_INDEX,
	LOGIN_DIALOG_OK_BUTTON_INDEX,
	LOGIN_DIALOG_CANCEL_BUTTON
};

#ifdef DEBUG_TEST
	#undef URL_REDIRECT_TABLE_HOME
	#define URL_REDIRECT_TABLE_HOME		"http://localhost/redirects.xml"
#endif

typedef struct _ServicesButtonCallbackData ServicesButtonCallbackData;

typedef enum {
	Pending_None,
	Pending_Login,
} SummaryPendingOperationType;

typedef enum {
	initial,
	retry,
	fail,
} SummaryLoginAttemptType;


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

	/* Login State */
	char			*user_name;
	volatile gboolean	logged_in;

	GtkWidget		*caption_table;
	SummaryLoginAttemptType	current_attempt;
	int			attempt_number;

	/* Services control panel */
	int		current_service_row;
	GtkWidget	*services_row;
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
	GtkWidget	*services_notebook;
	
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
	GtkWidget	*service_news_row;
	GtkWidget	*news_icon_container;
	GtkWidget	*news_icon_widget;
	char		*news_icon_name;
	GtkWidget	*news_date_widget;
	char		*news_date;
	GtkWidget	*news_description_header_widget;
	char		*news_description_header;
	GtkWidget	*news_description_body_widget;
	char		*news_description_body;

	/* Update control panel */
	int		current_update_row;
	gboolean	*updates_has_data;
	GtkWidget	*updates_row;
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
	GtkWidget	*update_softcat_goto_button;
	GtkWidget	*update_softcat_goto_label_widget;
	char		*update_softcat_goto_label;
	char		*update_softcat_redirects[500];
	GtkWidget	*updates_notebook;

	/* EazelProxy -- for logging in/logging out */
	EazelProxy_UserControl user_control;
	SummaryPendingOperationType pending_operation;
	EazelProxy_AuthnCallback authn_callback;

};

static void     nautilus_summary_view_initialize_class (NautilusSummaryViewClass   *klass);
static void     nautilus_summary_view_initialize       (NautilusSummaryView        *view);
static void     nautilus_summary_view_destroy          (GtkObject                  *object);
static void     summary_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusSummaryView        *view);
static void     generate_summary_form                  (NautilusSummaryView        *view);
static void     generate_service_entry_row             (NautilusSummaryView        *view,
							int                         row);
static void     generate_eazel_news_entry_row          (NautilusSummaryView        *view,
							int                         row);
static void     generate_update_news_entry_row         (NautilusSummaryView        *view,
							int                         row);
static void     login_button_cb                        (GtkWidget                  *button,
							NautilusSummaryView        *view);
static void     preferences_button_cb                  (GtkWidget                  *button,
							NautilusSummaryView        *view);
static void     logout_button_cb                       (GtkWidget                  *button,
							NautilusSummaryView        *view);
static void     goto_service_cb                        (GtkWidget                  *button,
							ServicesButtonCallbackData *cbdata);
static void     goto_update_cb                         (GtkWidget                  *button,
							ServicesButtonCallbackData *cbdata);
static void     register_button_cb                     (GtkWidget                  *button,
							NautilusSummaryView        *view);
static void     forgot_password_button_cb              (GtkWidget                  *button,
							NautilusSummaryView        *view);
static gint     logged_in_callback                     (gpointer                    raw);
static gint     logged_out_callback                    (gpointer                    raw);
static void     generate_error_dialog                  (NautilusSummaryView        *view,
							const char                 *message);
static void     generate_login_dialog                  (NautilusSummaryView        *view);
static void     widget_set_nautilus_background_color   (GtkWidget                  *widget,
							const char                 *color);
static void     merge_bonobo_menu_items                (BonoboControl              *control,
							gboolean                    state,
							gpointer                    user_data);
static void     update_menu_items                      (NautilusSummaryView        *view,
							gboolean                    logged_in);
static void     service_tab_selected_callback          (GtkWidget                  *widget,
							int                         which_tab,
							NautilusSummaryView        *view);
static void     updates_tab_selected_callback          (GtkWidget                  *widget,
							int                         which_tab,
							NautilusSummaryView        *view);
static void     footer_item_clicked_callback           (GtkWidget                  *widget,
							int                         index,
							gpointer                    callback_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

#define FOOTER_REGISTER_OR_PREFERENCES	0
#define FOOTER_LOGIN_OR_LOGOUT		1
#define FOOTER_TERMS_OF_USER		2
#define FOOTER_PRIVACY_STATEMENT	3

static const char *footer_online_items[] = 
{
	N_("Register"),
	N_("Login"),
	N_("Terms of Use"),
	N_("Privacy Statement")
};

static const char *footer_offline_items[] = 
{
	N_("Account Preferences"),
	N_("Logout"),
	N_("Terms of Use"),
	N_("Privacy Statement")
};

static void
generate_summary_form (NautilusSummaryView	*view)
{

	GtkWidget		*frame;
	GtkWidget		*title;
	GtkWidget		*footer;
	GtkWidget		*notebook, *notebook_tabs;
	GtkWidget		*temp_box;
	ServicesData		*service_node;
	EazelNewsData		*eazel_news_node;
	UpdateNewsData		*update_news_node;
	GList			*iterator;
	GtkWidget		*temp_scrolled_window;
	GtkWidget 		*viewport;
	GtkWidget		*temp_hbox;
	GtkWidget		*temp_vbox;
	GtkWidget		*notebook_vbox;
	GtkWidget		*temp_label;
	GtkWidget		*separator;
	gboolean		has_news;
	
	view->details->current_service_row = 0;
	view->details->current_news_row = 0;
	view->details->current_update_row = 0;

	if (view->details->form != NULL) {
		gtk_container_remove (GTK_CONTAINER (view), view->details->form);
		view->details->form = NULL;
	}

#ifdef DEBUG_pepper
	g_print ("Start summary view load.\n");

#endif
	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);

	/* setup the title */
#ifdef DEBUG_pepper
	g_print ("Start title load.\n");
#endif
	title = eazel_services_header_title_new ("");

	if (view->details->logged_in) {
		char *text;
		g_free (view->details->user_name);
		view->details->user_name = ammonite_get_default_user_username (view->details->user_control);
		text = g_strdup_printf (_("Welcome Back %s!"), view->details->user_name);
		eazel_services_header_set_left_text (EAZEL_SERVICES_HEADER (title), text);
		g_free (text);
	}
	else {
		eazel_services_header_set_left_text (EAZEL_SERVICES_HEADER (title),
						     _("You are not logged in!"));
	}
	gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
	gtk_widget_show (title);
#ifdef DEBUG_pepper
	g_print ("end title load.\n");
#endif
	
	/* create the news section only if there's some news */
#ifdef DEBUG_pepper
	g_print ("start news load.\n");
#endif
	has_news = FALSE;
	if (view->details->xml_data->eazel_news_list) {
		eazel_news_node = (EazelNewsData*) view->details->xml_data->eazel_news_list->data;
		has_news = eazel_news_node->message != NULL;
	}
	
	if (has_news) {
		/* Create the Service News Frame */	
		frame = gtk_vbox_new (FALSE, 0);
		gtk_widget_show (frame);		
		gtk_box_pack_start (GTK_BOX (view->details->form), frame, TRUE, TRUE, 0);
		
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

		/* build the eazel news table from the xml file */
		for (iterator = view->details->xml_data->eazel_news_list; iterator; iterator = g_list_next (iterator)) {

			view->details->service_news_row = gtk_hbox_new (FALSE, 0);
			gtk_widget_show (view->details->service_news_row);

			view->details->current_news_row++;
			eazel_news_node = iterator->data;
			view->details->news_icon_name = eazel_news_node->icon;
			view->details->news_date = eazel_news_node->date;
			view->details->news_description_body = g_strdup(eazel_news_node->message);

			generate_eazel_news_entry_row (view, view->details->current_news_row);
			gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->service_news_row), FALSE, FALSE, 0);

			if (iterator->next != NULL || view->details->current_news_row > 1) {
				separator = gtk_hseparator_new ();
				gtk_box_pack_start (GTK_BOX (temp_box), separator, FALSE, FALSE, 8);
				gtk_widget_show (separator);
			}
			/* FIXME: is this a leak?  who frees the constituent strings? */
			g_free (eazel_news_node);
		}

		g_list_free (view->details->xml_data->eazel_news_list);

		/* draw parent vbox and connect it to the service news frame */
		gtk_container_add (GTK_CONTAINER (viewport), temp_box);
		gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
		gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);		
	}
#ifdef DEBUG_pepper
	g_print ("end news load.\n");
#endif
	
	/* add a set of tabs to control the notebook page switching */
#ifdef DEBUG_pepper
	g_print ("start tab load.\n");
#endif
	notebook_tabs = nautilus_tabs_new ();
	gtk_widget_show (notebook_tabs);
	gtk_box_pack_start (GTK_BOX (view->details->form), notebook_tabs, FALSE, FALSE, 0);

	/* Create the notebook container for services */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (view->details->form), notebook, TRUE, TRUE, 0);
	
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	view->details->services_notebook = notebook;
	gtk_signal_connect (GTK_OBJECT (notebook_tabs), "tab_selected", service_tab_selected_callback, view);
	
	/* add the tab */
	nautilus_tabs_add_tab (NAUTILUS_TABS (notebook_tabs), _("Your Services"), 0);
#ifdef DEBUG_pepper
	g_print ("end tab load.\n");
#endif
	
	/* Create the Services Listing Box */
#ifdef DEBUG_pepper
	g_print ("start services load.\n");
#endif
	frame = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (frame);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, NULL);

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

	/* build the services table from the xml file */
	for (iterator = view->details->xml_data->services_list; iterator; iterator = g_list_next (iterator)) {

		view->details->services_row = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (GTK_WIDGET (view->details->services_row));

		view->details->current_service_row++;
		service_node = iterator->data;
		view->details->services_icon_name = service_node->icon;
		view->details->services_description_header = service_node->description_header;
		view->details->services_description_body = service_node->description;
		view->details->services_goto_label = g_strdup (_(GOTO_BUTTON_LABEL));
		view->details->services_button_enabled = service_node->enabled;
		view->details->services_redirects[view->details->current_service_row - 1] = service_node->uri;

		generate_service_entry_row (view, view->details->current_service_row);
		gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->services_row), FALSE, FALSE, 4);

		if (iterator->next != NULL || view->details->current_service_row > 1) {
			separator = gtk_hseparator_new ();
			gtk_box_pack_start (GTK_BOX (temp_box), separator, FALSE, FALSE, 8);
			gtk_widget_show (separator);
		}

		g_free (service_node);

	}

	g_list_free (view->details->xml_data->services_list);

	/* draw parent vbox and connect it to the login frame */
	gtk_container_add (GTK_CONTAINER (viewport), temp_box);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);

	/* create the Additional Services pane */
	nautilus_tabs_add_tab (NAUTILUS_TABS (notebook_tabs), _("Additional Services"), 1);

	temp_vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_show (temp_vbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), temp_vbox, NULL);

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

	temp_label = nautilus_label_new (_("Check back here for new system management\nservices that will help make Linux easier to use."));
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

	/* add a set of tabs to control the updates page switching */
	notebook_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (notebook_vbox);
	
	notebook_tabs = nautilus_tabs_new ();
	gtk_widget_show (notebook_tabs);
	gtk_box_pack_start (GTK_BOX (notebook_vbox), notebook_tabs, FALSE, FALSE, 0);
#ifdef DEBUG_pepper
	g_print ("end services load.\n");
#endif

	/* Create the notebook container for updates */
#ifdef DEBUG_pepper
	g_print ("start updates load.\n");
#endif
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_container_add (GTK_CONTAINER (notebook_vbox), notebook);
	gtk_box_pack_start (GTK_BOX (view->details->form), notebook_vbox, TRUE, TRUE, 0);
	
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	view->details->updates_notebook = notebook;
	gtk_signal_connect (GTK_OBJECT (notebook_tabs), "tab_selected", updates_tab_selected_callback, view);
	
	/* add the tab */
	nautilus_tabs_add_tab (NAUTILUS_TABS (notebook_tabs), _("Current Updates"), 0);

	/* Create the Update News Frame */
	frame = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (frame);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, NULL);

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

	/* build the updates table from the xml file */
	for (iterator = view->details->xml_data->update_news_list; iterator; iterator = g_list_next (iterator)) {

		/* create the default update table */
		view->details->updates_row = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (GTK_WIDGET (view->details->updates_row));

		view->details->current_update_row++;
		update_news_node = iterator->data;
		view->details->update_icon_name = update_news_node->icon;
		view->details->update_description_header = update_news_node->name;
		view->details->update_description_version = g_strdup_printf (_("Version %s"), update_news_node->version);
		view->details->update_description_body = update_news_node->description;
		view->details->update_goto_label = g_strdup (_(INSTALL_GOTO_BUTTON_LABEL));
		view->details->update_softcat_goto_label = g_strdup (_(SOFTCAT_GOTO_BUTTON_LABEL));
		view->details->update_redirects[view->details->current_update_row - 1] = update_news_node->uri;
		view->details->update_softcat_redirects[view->details->current_update_row - 1] = update_news_node->softcat_uri;

		generate_update_news_entry_row (view, view->details->current_update_row);
		gtk_box_pack_start (GTK_BOX (temp_box), GTK_WIDGET (view->details->updates_row), FALSE, FALSE, 4);

		if (iterator->next != NULL || view->details->current_update_row > 1) {
			separator = gtk_hseparator_new ();
			gtk_box_pack_start (GTK_BOX (temp_box), separator, FALSE, FALSE, 8);
			gtk_widget_show (separator);
		}
		

		g_free (update_news_node);

	}

	g_list_free (view->details->xml_data->update_news_list);

	g_free (view->details->xml_data);
	view->details->xml_data = NULL;

#ifdef DEBUG_pepper
	g_print ("start updates load.\n");

	g_print ("start footer load.\n");
#endif
	footer = eazel_services_footer_new ();
	gtk_signal_connect (GTK_OBJECT (footer), "item_clicked", GTK_SIGNAL_FUNC (footer_item_clicked_callback), view);

	if (view->details->logged_in) {
		eazel_services_footer_update (EAZEL_SERVICES_FOOTER (footer),
					      footer_offline_items,
					      NAUTILUS_N_ELEMENTS (footer_online_items));
	}
	else {
		eazel_services_footer_update (EAZEL_SERVICES_FOOTER (footer),
					      footer_online_items,
					      NAUTILUS_N_ELEMENTS (footer_online_items));
	}

	gtk_box_pack_start (GTK_BOX (view->details->form), footer, FALSE, FALSE, 0);
	gtk_widget_show (footer);
#ifdef DEBUG_pepper
	g_print ("end footer load.\n");
#endif

	/* draw parent vbox and connect it to the update news frame */
	gtk_container_add (GTK_CONTAINER (viewport), temp_box);
	gtk_container_add (GTK_CONTAINER (temp_scrolled_window), viewport);
	gtk_box_pack_start (GTK_BOX (frame), temp_scrolled_window, TRUE, TRUE, 0);

	/* Finally, show the form that hold everything */
	gtk_widget_show (view->details->form);
#ifdef DEBUG_pepper
	g_print ("Load summary view end.\n");
#endif
}

static void
generate_service_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget			*temp_vbox;
	GtkWidget			*temp_hbox;
	GdkFont				*font;
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first box with service icon */
	view->details->services_icon_container = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (view->details->services_icon_container);
	view->details->services_icon_widget = create_image_widget_from_uri (view->details->services_icon_name, 
									    DEFAULT_SUMMARY_BACKGROUND_COLOR,
									    MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
	
	g_assert (view->details->services_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->services_icon_container), view->details->services_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->services_icon_widget);
	gtk_box_pack_start (GTK_BOX (view->details->services_row), view->details->services_icon_container, FALSE, FALSE, 2);

	/* Generate second box with service title and summary */
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
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->services_description_header_widget, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 2);
	
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
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->services_description_body_widget, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (view->details->services_row), temp_vbox, FALSE, FALSE, 0);

	/* Add the redirect button to the third box */

	temp_vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_show (temp_vbox);

	view->details->services_button_container = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (view->details->services_button_container);
	view->details->services_goto_button = gtk_button_new ();
	gtk_widget_show (view->details->services_goto_button);
	gtk_widget_set_usize (view->details->services_goto_button, 80, -1);
	view->details->services_goto_label_widget = gtk_label_new (view->details->services_goto_label);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->services_goto_label_widget, font);
	gdk_font_unref (font);
	gtk_widget_show (view->details->services_goto_label_widget);
	g_free (view->details->services_goto_label);
	view->details->services_goto_label = NULL;
	
	gtk_container_add (GTK_CONTAINER (view->details->services_goto_button), view->details->services_goto_label_widget);
	gtk_box_pack_end (GTK_BOX (view->details->services_button_container), view->details->services_goto_button, FALSE, FALSE, 3);
	
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->services_redirects[view->details->current_service_row - 1];
	gtk_signal_connect_full (GTK_OBJECT (view->details->services_goto_button), "clicked",
			         GTK_SIGNAL_FUNC (goto_service_cb), NULL,
				 cbdata, g_free, FALSE, FALSE);
	
	gtk_box_pack_start (GTK_BOX (temp_vbox), view->details->services_button_container, FALSE, FALSE, 2);	
	gtk_box_pack_end (GTK_BOX (view->details->services_row), temp_vbox, FALSE, FALSE, 2);

}

static void
generate_eazel_news_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget *item_box;
	
	/* Generate first box with icon */
	view->details->news_icon_container = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (view->details->news_icon_container);
	view->details->news_icon_widget = create_image_widget_from_uri (view->details->news_icon_name,
									DEFAULT_SUMMARY_BACKGROUND_COLOR,
									MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
	g_assert (view->details->news_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->news_icon_container), view->details->news_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->news_icon_widget);
	gtk_box_pack_start (GTK_BOX (view->details->service_news_row), view->details->news_icon_container, FALSE, FALSE, 3);
	
	/* generate second box with bold type date and the actual contents */
	item_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (item_box);
	
	view->details->news_date_widget = nautilus_label_new (view->details->news_date);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->news_date_widget), 12);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->news_date_widget),
						 "helvetica",
						 "bold",
						 NULL,
						 NULL);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->news_date_widget), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (item_box), view->details->news_date_widget, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (view->details->service_news_row), item_box, TRUE, TRUE, 2);
	gtk_widget_show (view->details->news_date_widget);
	
	g_free (view->details->news_date);
	view->details->news_date = NULL;
	
	/* Generate the actual content */
	view->details->news_description_body_widget = nautilus_label_new (view->details->news_description_body);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->news_description_body_widget), 12);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->news_description_body_widget), TRUE);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->news_description_body_widget), GTK_JUSTIFY_LEFT);
	/*
	nautilus_label_set_line_wrap_width (NAUTILUS_LABEL (view->details->news_description_body_widget), -1);
	*/
	gtk_box_pack_start (GTK_BOX (item_box), view->details->news_description_body_widget, TRUE, TRUE, 2);
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
	ServicesButtonCallbackData	*cbdata_2;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);
	cbdata_2 = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first box with icon */
	view->details->update_icon_container = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (view->details->update_icon_container);
	view->details->update_icon_widget = create_image_widget_from_uri (view->details->update_icon_name,
									  DEFAULT_SUMMARY_BACKGROUND_COLOR,
									  MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
	g_assert (view->details->update_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->update_icon_container), view->details->update_icon_widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->updates_row), view->details->update_icon_container, FALSE, FALSE, 0);
	gtk_widget_show (view->details->update_icon_widget);

	/* Generate second box with update title, summary, and version */
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
	
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_version_widget, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (view->details->updates_row), temp_vbox, FALSE, FALSE, 0);

	/* Add the redirect button and softcat button to the third box */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_vbox);

	view->details->update_button_container = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (view->details->update_button_container);
	view->details->update_softcat_goto_button = gtk_button_new ();
	gtk_widget_show (view->details->update_softcat_goto_button);
	gtk_widget_set_usize (view->details->update_softcat_goto_button, 80, -1);
	
	view->details->update_softcat_goto_label_widget = gtk_label_new (view->details->update_softcat_goto_label);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->update_softcat_goto_label_widget, font);
	gdk_font_unref (font);
	
	gtk_widget_show (view->details->update_softcat_goto_label_widget);
	g_free (view->details->update_softcat_goto_label);
	view->details->update_softcat_goto_label = NULL;

	gtk_container_add (GTK_CONTAINER (view->details->update_softcat_goto_button), view->details->update_softcat_goto_label_widget);
	gtk_box_pack_start (GTK_BOX (view->details->update_button_container), view->details->update_softcat_goto_button, FALSE, FALSE, 4);
	cbdata->nautilus_view = view->details->nautilus_view;
	cbdata->uri = view->details->update_softcat_redirects[view->details->current_update_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->update_softcat_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_update_cb), cbdata);
	gtk_box_pack_start (GTK_BOX (temp_vbox), view->details->update_button_container, FALSE, FALSE, 4);

	view->details->update_button_container = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (view->details->update_button_container);
	view->details->update_goto_button = gtk_button_new ();
	gtk_widget_show (view->details->update_goto_button);
	gtk_widget_set_usize (view->details->update_goto_button, 80, -1);
	view->details->update_goto_label_widget = gtk_label_new (view->details->update_goto_label);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->update_goto_label_widget, font);
	gdk_font_unref (font);
	gtk_widget_show (view->details->update_goto_label_widget);
	g_free (view->details->update_goto_label);
	view->details->update_goto_label = NULL;
	
	gtk_container_add (GTK_CONTAINER (view->details->update_goto_button), view->details->update_goto_label_widget);
	gtk_box_pack_start (GTK_BOX (view->details->update_button_container), view->details->update_goto_button, FALSE, FALSE, 	4);
	cbdata_2->nautilus_view = view->details->nautilus_view;
	cbdata_2->uri = view->details->update_redirects[view->details->current_update_row - 1];
	gtk_signal_connect (GTK_OBJECT (view->details->update_goto_button), "clicked", GTK_SIGNAL_FUNC (goto_update_cb), cbdata_2);
	gtk_box_pack_start (GTK_BOX (temp_vbox), view->details->update_button_container, FALSE, FALSE, 4);

	gtk_box_pack_end (GTK_BOX (view->details->updates_row), temp_vbox, FALSE, FALSE, 0);

}

static void
authn_cb_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusSummaryView    *view;
	gint			timeout;

	view = NAUTILUS_SUMMARY_VIEW (state);

	g_assert (Pending_Login == view->details->pending_operation);

	view->details->pending_operation = Pending_None;
	
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

	view->details->logged_in = FALSE;
	
	update_menu_items (view, FALSE);
	
	generate_login_dialog (view);
	
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

	view->details->attempt_number++;
	if (view->details->attempt_number > 0 && view->details->attempt_number < 5) {
		view->details->current_attempt = retry;
	}	
	else {
		view->details->current_attempt = fail;
	}

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

	timeout = gtk_timeout_add (0, logged_out_callback, view);

	CORBA_exception_free (&ev);
}

static gint
logged_in_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = TRUE;

	update_menu_items (view, TRUE);
	nautilus_view_open_location (view->details->nautilus_view, "eazel:");

	return (FALSE);
}


static gint
logged_out_callback (gpointer	raw)
{
	NautilusSummaryView	*view;

	view = NAUTILUS_SUMMARY_VIEW (raw);
	view->details->logged_in = FALSE;
	
	update_menu_items (view, FALSE);
	nautilus_view_open_location (view->details->nautilus_view, "eazel:");

	return (FALSE);
}

/* callback to handle the maintenance button.  Right now only does a simple redirect. */
static void
preferences_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{
	char	*url;
	url = NULL;

	url = trilobite_redirect_lookup (PREFERENCES_KEY);
	if (!url) {
		g_assert ("Failed to load Registration url!\n");
	}

	nautilus_view_open_location (view->details->nautilus_view, url);
	g_free (url);

}

/* callback to handle the forgotten password button. */
static void
forgot_password_button_cb (GtkWidget      *button, NautilusSummaryView    *view)
{

	nautilus_view_open_location (view->details->nautilus_view, SUMMARY_CHANGE_PWD_FORM);

}

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

	nautilus_view_open_location (view->details->nautilus_view, url);
	g_free (url);

}

/* callback to handle the goto a service button. */
static void
goto_service_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	nautilus_view_open_location (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle update buttons. */
static void
goto_update_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	nautilus_view_open_location (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle cancel error_dialog button. */
static void
error_dialog_cancel_cb (GtkWidget      *button, NautilusSummaryView	*view)
{
	char	*user_home;
	user_home = nautilus_get_user_main_directory ();	
	nautilus_view_open_location (view->details->nautilus_view, user_home);
	g_free (user_home);

}

static void
set_dialog_parent (NautilusSummaryView *view, GnomeDialog *dialog)
{
	GtkWidget *parent_window;

	g_assert (NAUTILUS_IS_SUMMARY_VIEW (view));
	g_assert (GNOME_IS_DIALOG (dialog));

	parent_window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	/* this can sometimes be null */
	if (parent_window != NULL) {
		gnome_dialog_set_parent (dialog, GTK_WINDOW (parent_window));
	}
}

static void
generate_error_dialog (NautilusSummaryView *view, const char *message)
{
	GnomeDialog	*dialog;
	GtkWidget	*icon_container;
	GtkWidget	*icon_widget;
	GtkWidget	*hbox;
	GtkWidget	*error_text;

	dialog = GNOME_DIALOG (gnome_dialog_new (_("Service Error"), GNOME_STOCK_BUTTON_OK, NULL));

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (dialog->vbox), hbox, FALSE, FALSE, 0);

	icon_container = gtk_hbox_new (TRUE, 4);
	icon_widget = create_image_widget ("gnome-warning.png", DEFAULT_SUMMARY_BACKGROUND_COLOR);
	g_assert (icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (icon_container), icon_widget, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), icon_container, FALSE, FALSE, 0);
	gtk_widget_show (icon_widget);
	gtk_widget_show (icon_container);

	error_text = nautilus_label_new (message);
	nautilus_label_set_font_size (NAUTILUS_LABEL (error_text), 12);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (error_text), GTK_JUSTIFY_LEFT);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (error_text), TRUE);
	/* FIXME: setting a fixed size here seems so hackish, but the results are so ugly otherwise. */
	nautilus_label_set_line_wrap_width (NAUTILUS_LABEL (error_text), 300);
	gtk_widget_show (error_text);
	gtk_box_pack_start (GTK_BOX (hbox), error_text, FALSE, FALSE, 0);


	gnome_dialog_set_close (dialog, TRUE);
	set_dialog_parent (view, dialog);

	gtk_widget_show (hbox);
	gtk_widget_show (GTK_WIDGET (dialog));
	gnome_dialog_button_connect (dialog, 0, GTK_SIGNAL_FUNC (error_dialog_cancel_cb), view);
	gnome_dialog_run (dialog);
}

static void
name_or_password_field_activated (GtkWidget *caption_table, int active_entry, gpointer user_data)
{
	g_assert (NAUTILUS_IS_CAPTION_TABLE (caption_table));
	g_assert (GTK_IS_BUTTON (user_data));

	/* auto-click "OK" button when password activated (via Enter key) */
	if (active_entry == LOGIN_DIALOG_OK_BUTTON_INDEX) {
		nautilus_gtk_button_auto_click (GTK_BUTTON (user_data));
	}
}

static void
generate_login_dialog (NautilusSummaryView	*view)
{
	GnomeDialog	*dialog;
	GtkWidget	*hbox;
	GtkWidget	*pixmap;
	GtkWidget	*message;
	GtkWidget	*caption_hbox;
	char		*message_text;
	char		*pixmap_name;
	char		*button_text;

	dialog = NULL;
	pixmap = NULL;

	if (view->details->attempt_number == 0) {
		button_text = g_strdup (_("Register Now"));
	} else {
		button_text = g_strdup (_("Help"));
	}

	dialog = GNOME_DIALOG (gnome_dialog_new (_("Services Login"), button_text, 
			       GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL));

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	view->details->caption_table = nautilus_caption_table_new (LOGIN_DIALOG_ROW_COUNT);
	gtk_widget_show (view->details->caption_table);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (view->details->caption_table),
					     LOGIN_DIALOG_NAME_ROW,
					     _("Username:"),
					     "",
					     TRUE,
					     FALSE);

	nautilus_caption_table_set_row_info (NAUTILUS_CAPTION_TABLE (view->details->caption_table),
					     LOGIN_DIALOG_PASSWORD_ROW,
					     _("Password:"),
					     "",
					     FALSE,
					     FALSE);

	switch (view->details->current_attempt) {
		case initial:
			pixmap_name = "big_services_icon.png";
			message_text = _("Please log in to Eazel services");
			break;
		case retry:
			pixmap_name = "serv_dialog_alert.png";
			message_text = _("Your user name or password were not correct, or you have not activated your account.  Please try again, or check your email for an account activation notice.");
			break;
		case fail:
			pixmap_name = "serv_dialog_alert.png";
			message_text = _("We're sorry, but your name and password are still not recognized.");
			break;
		default:
			pixmap_name = "serv_dialog_alert.png";
			message_text = _("Please log in to Eazel services");
	}

	pixmap = create_image_widget (pixmap_name, DEFAULT_SUMMARY_BACKGROUND_COLOR);
	hbox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (hbox);

	if (pixmap) {
		gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
		gtk_widget_show (pixmap);
	}

	gtk_box_set_spacing (GTK_BOX (dialog->vbox), 4);

	message = gtk_label_new (message_text);
	gtk_label_set_justify (GTK_LABEL (message), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (message), TRUE);
	nautilus_gtk_label_make_bold (GTK_LABEL (message));
	gtk_widget_show (message);

	/* right justify the caption table box */
	caption_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (caption_hbox);
	gtk_widget_set_usize (view->details->caption_table, 260, -1);
	gtk_box_pack_end (GTK_BOX (caption_hbox), view->details->caption_table, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (hbox), message, FALSE, FALSE, 0);	
	gtk_box_pack_start (GTK_BOX (dialog->vbox), hbox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (dialog->vbox), caption_hbox, FALSE, FALSE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (view->details->caption_table), 4);

	gtk_widget_show (dialog->vbox);

	gnome_dialog_set_close (dialog, TRUE);
	set_dialog_parent (view, dialog);

	gnome_dialog_set_default (dialog, LOGIN_DIALOG_OK_BUTTON_INDEX);
	gtk_signal_connect (GTK_OBJECT (view->details->caption_table), "activate",
			    name_or_password_field_activated, 
			    nautilus_gnome_dialog_get_button_by_index (dialog, LOGIN_DIALOG_OK_BUTTON_INDEX));
	nautilus_caption_table_entry_grab_focus	(NAUTILUS_CAPTION_TABLE (view->details->caption_table), LOGIN_DIALOG_NAME_ROW);

	if (view->details->attempt_number == 0) {
		gnome_dialog_button_connect (dialog, LOGIN_DIALOG_REGISTER_BUTTON_INDEX, GTK_SIGNAL_FUNC (register_button_cb), view);
	} else {
		gnome_dialog_button_connect (dialog, LOGIN_DIALOG_REGISTER_BUTTON_INDEX, GTK_SIGNAL_FUNC (forgot_password_button_cb), view);
	}

	gnome_dialog_button_connect (dialog, LOGIN_DIALOG_OK_BUTTON_INDEX, GTK_SIGNAL_FUNC (login_button_cb), view);

  	gnome_dialog_set_close (dialog, TRUE);
	gtk_widget_show (GTK_WIDGET (dialog));
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
	char		*user_name;
	gboolean	got_url_table;

	url = NULL;

	/* set up some sanity values for error control */
	view->details->attempt_number = 0;
	view->details->current_attempt = initial;

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* get xml data and verify network connections */
#ifdef DEBUG_pepper
	g_print ("start load\n");
#endif

	user_name = ammonite_get_default_user_username (view->details->user_control);
	view->details->logged_in = (NULL != user_name);
	g_free (user_name);
	user_name = NULL;

#ifdef DEBUG_pepper
	g_print ("start xml table fetch\n");
#endif
	got_url_table = trilobite_redirect_fetch_table 
		(view->details->logged_in
		 ? URL_REDIRECT_TABLE_HOME_2
		 : URL_REDIRECT_TABLE_HOME);

	if (!got_url_table) {
		/* FIXME bugzilla.eazel.com 3743:
		 * We should do more to figure out why this failed so we can
		 * present a much more helpful message. There are several different
		 * reasons why it might have failed.
		 */
		generate_error_dialog 
			(view, _("Unable to connect to Eazel's server. "
				 "The server might be unavailable right now, "
				 "or your computer might be configured incorrectly."
				 "You could try again later."));
	} else {
#ifdef DEBUG_pepper
		g_print ("end xml table fetch\n");
		/* fetch and parse the xml file */
		g_print ("start xml config fetch\n");
#endif
		url = trilobite_redirect_lookup (SUMMARY_XML_KEY);
		if (!url) {
			g_assert ("Failed to get summary xml home !\n");
		}
		view->details->xml_data = parse_summary_xml_file (url);
		g_free (url);
		if (view->details->xml_data == NULL) {
			generate_error_dialog 
				(view, _("Found problem with data on Eazel servers. "
					 "Please contact support@eazel.com."));
		} else {
#ifdef DEBUG_pepper
			g_print ("end xml config fetch\n");
			g_print ("start summary draw\n");
#endif
			generate_summary_form (view);		
#ifdef DEBUG_pepper
			g_print ("end summary draw\n");
#endif
			if (!view->details->logged_in) {
				generate_login_dialog (view);
			}
		}
	}
}

static void
summary_load_location_callback (NautilusView		*nautilus_view, 
			        const char		*location,
			        NautilusSummaryView	*view)
{
	
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);

	nautilus_view_set_title (nautilus_view, "Eazel Services");
	
	nautilus_summary_view_load_uri (view, location);

	nautilus_view_report_load_complete (nautilus_view);

}

/* here is the callback to handle service tab selection */
static void
service_tab_selected_callback (GtkWidget *widget,
		   int which_tab,
		   NautilusSummaryView	*view)
{
	gtk_notebook_set_page (GTK_NOTEBOOK (view->details->services_notebook), which_tab);
}

/* here is the callback to handle updates tab selection */
static void
updates_tab_selected_callback (GtkWidget *widget,
		   int which_tab,
		   NautilusSummaryView	*view)
{
	gtk_notebook_set_page (GTK_NOTEBOOK (view->details->updates_notebook), which_tab);
}

/* here are the callbacks to handle bonobo menu items */
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
bonobo_logout_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	logout_button_cb (NULL, view);
}

static void
bonobo_preferences_callback (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	NautilusSummaryView *view;
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);
	preferences_button_cb (NULL, view);
}

/* update the visibility of the menu items according to the login state */
static void
update_menu_items (NautilusSummaryView *view, gboolean logged_in)
{
	BonoboUIComponent *ui;

	ui = bonobo_control_get_ui_component 
		(nautilus_view_get_bonobo_control 
			(view->details->nautilus_view)); 

	nautilus_bonobo_set_hidden (ui,
				    "/commands/Register",
				    logged_in);
	
	nautilus_bonobo_set_hidden (ui,
				     "/commands/Login",
				    logged_in);

	nautilus_bonobo_set_hidden (ui,
				    "/commands/Preferences",
				    !logged_in);
	
	nautilus_bonobo_set_hidden (ui,
				    "/commands/Logout",
				    !logged_in);				    				    
}

/* this routine is invoked when the view is activated to merge in our menu items */
static void
merge_bonobo_menu_items (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusSummaryView *view;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Register", bonobo_register_callback),
		BONOBO_UI_VERB ("Login", bonobo_login_callback),
		BONOBO_UI_VERB ("Logout", bonobo_logout_callback),
		BONOBO_UI_VERB ("Preferences", bonobo_preferences_callback),		
		BONOBO_UI_VERB_END
	};

	g_assert (BONOBO_IS_CONTROL (control));
	
	view = NAUTILUS_SUMMARY_VIEW (user_data);

	if (state) {
		gboolean logged_in;
		char * user_name;
	
		nautilus_view_set_up_ui (view->details->nautilus_view,
				         DATADIR,
					 "nautilus-summary-view-ui.xml",
					 "nautilus-summary-view");
									
		bonobo_ui_component_add_verb_list_with_data 
			(bonobo_control_get_ui_component (control), verbs, view);

		user_name = ammonite_get_default_user_username (view->details->user_control);
		logged_in = (NULL != user_name);
		update_menu_items (view, logged_in);
		g_free (user_name);
	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are never explicitly deactivated
	 */
}

static void
footer_item_clicked_callback (GtkWidget *widget, int index, gpointer callback_data)
{
	NautilusSummaryView *view;

	g_return_if_fail (NAUTILUS_IS_SUMMARY_VIEW (callback_data));
	g_return_if_fail (index >= FOOTER_REGISTER_OR_PREFERENCES);
	g_return_if_fail (index <= FOOTER_PRIVACY_STATEMENT);

	view = NAUTILUS_SUMMARY_VIEW (callback_data);

	switch (index) {
	case FOOTER_REGISTER_OR_PREFERENCES:
		if (!view->details->logged_in) {
			register_button_cb (NULL, view);
		} else {
			preferences_button_cb (NULL, view);
		}
		break;

	case FOOTER_LOGIN_OR_LOGOUT:
		if (!view->details->logged_in) {
			generate_login_dialog (view);
		} else {
			logout_button_cb (NULL, view);
		}
		break;

	case FOOTER_TERMS_OF_USER:
		nautilus_view_open_location (view->details->nautilus_view, SUMMARY_TERMS_OF_USE_URI);
		break;

	case FOOTER_PRIVACY_STATEMENT:
		nautilus_view_open_location (view->details->nautilus_view, SUMMARY_PRIVACY_STATEMENT_URI);
		break;

	default:
		g_assert_not_reached ();
		break;
	}
}
