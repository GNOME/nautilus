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

#include <gnome-xml/tree.h>
#include <bonobo/bonobo-control.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-tabs.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-viewport.h>

#include <gnome.h>
#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <bonobo/bonobo-main.h>

#include "nautilus-summary-view.h"

#include "eazel-summary-shared.h"
#include "eazel-services-footer.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include "nautilus-summary-callbacks.h"
#include "nautilus-summary-menu-items.h"
#include "nautilus-summary-dialogs.h"
#include "nautilus-summary-footer.h"
#include "nautilus-summary-view-private.h"

#define notDEBUG_TEST	1
#define notDEBUG_PEPPER	1

#define SUMMARY_TEXT_HEADER_SIZE_REL (+2)
#define SUMMARY_TEXT_BODY_SIZE_REL (-2)

#ifdef DEBUG_TEST
	#undef URL_REDIRECT_TABLE_HOME
	#define URL_REDIRECT_TABLE_HOME		"http://localhost/redirects.xml"
#endif

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
static void     goto_service_cb                        (GtkWidget                  *button,
							ServicesButtonCallbackData *cbdata);
static void     goto_update_cb                         (GtkWidget                  *button,
							ServicesButtonCallbackData *cbdata);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

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
		viewport = nautilus_viewport_new (NULL, NULL);
		widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC);
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
	viewport = nautilus_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC);
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
	viewport = nautilus_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_scrolled_window),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
	temp_label = eazel_services_label_new (_("\nCheck back here for new system management\nservices that will help make Linux easier to use."),
					       0,
					       0.5,
					       0.5,
					       0,
					       0,
					       DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					       DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					       NULL,
					       SUMMARY_TEXT_HEADER_SIZE_REL,
					       TRUE);

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
	
	viewport = nautilus_viewport_new (NULL, NULL);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC);
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
	ServicesButtonCallbackData	*cbdata;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first box with service icon */
	view->details->services_icon_container = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (view->details->services_icon_container);
	view->details->services_icon_widget = 
		eazel_services_image_new_from_uri (view->details->services_icon_name,
						   NULL,
						   DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
						   MAX_IMAGE_WIDTH,
						   MAX_IMAGE_HEIGHT);

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
	view->details->services_description_header_widget = 
		eazel_services_label_new (view->details->services_description_header,
					  0,
					  0.5,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  TRUE);
	gtk_widget_show (view->details->services_description_header_widget);
	g_free (view->details->services_description_header);
	view->details->services_description_header = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->services_description_header_widget, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 2);
	
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (temp_hbox);
	view->details->services_description_body_widget = 
		eazel_services_label_new (view->details->services_description_body,
					  0,
					  0.5,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  FALSE);

	nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->services_description_body_widget), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->services_description_body_widget), GTK_JUSTIFY_LEFT);

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

#if 0
/* FIXME well, we want to resize the text, but I don't know how */
static void
text_resize_callback      (GtkWidget *widget,
				GtkAllocation *allocation,
				gpointer user_data)
{
	NautilusLabel *label;

	g_return_if_fail (NAUTILUS_IS_LABEL (user_data));

	g_print ("text_resize New size %u\n", (unsigned) allocation->width);

	label = NAUTILUS_LABEL (user_data);

	nautilus_label_set_smooth_line_wrap_width (label, allocation->width);
}
#endif

static void
generate_eazel_news_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget *item_box;
	
	/* Generate first box with icon */
	view->details->news_icon_container = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (view->details->news_icon_container);
	view->details->news_icon_widget = 
		eazel_services_image_new_from_uri (view->details->news_icon_name,
						   NULL,
						   DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
						   MAX_IMAGE_WIDTH,
						   MAX_IMAGE_HEIGHT);
	
	g_assert (view->details->news_icon_widget != NULL);
	gtk_box_pack_start (GTK_BOX (view->details->news_icon_container), view->details->news_icon_widget, 0, 0, 0);
	gtk_widget_show (view->details->news_icon_widget);
	gtk_box_pack_start (GTK_BOX (view->details->service_news_row), view->details->news_icon_container, FALSE, FALSE, 3);
	
	/* generate second box with bold type date and the actual contents */
	item_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (item_box);
	
	view->details->news_date_widget = 
		eazel_services_label_new (view->details->news_date,
					  0,
					  0,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  TRUE);

	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->news_date_widget), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (item_box), view->details->news_date_widget, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (view->details->service_news_row), item_box, TRUE, TRUE, 2);
	gtk_widget_show (view->details->news_date_widget);
	
	g_free (view->details->news_date);
	view->details->news_date = NULL;
	
	/* Generate the actual content */
	view->details->news_description_body_widget = 
		eazel_services_label_new (view->details->news_description_body,
					  0,
					  0,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  FALSE);

	nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->news_description_body_widget), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->news_description_body_widget), GTK_JUSTIFY_LEFT);
	/*
	nautilus_label_set_wrap_width (NAUTILUS_LABEL (view->details->news_description_body_widget), -1);
	*/
	
	gtk_box_pack_start (GTK_BOX (item_box), view->details->news_description_body_widget, TRUE, TRUE, 2);

#if 0
	/* FIXME see above */
	gtk_signal_connect (GTK_OBJECT (view->details->service_news_row), "size-allocate", text_resize_callback, view->details->news_description_body_widget);
#endif

	gtk_widget_show (view->details->news_description_body_widget);
	
	g_free (view->details->news_description_body);
	view->details->news_description_body = NULL;

}

static void
generate_update_news_entry_row  (NautilusSummaryView	*view, int	row)
{
	GtkWidget			*temp_vbox;
	GtkWidget			*temp_hbox;
	ServicesButtonCallbackData	*cbdata;
	ServicesButtonCallbackData	*cbdata_2;

	cbdata = g_new0 (ServicesButtonCallbackData, 1);
	cbdata_2 = g_new0 (ServicesButtonCallbackData, 1);

	/* Generate first box with icon */
	view->details->update_icon_container = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (view->details->update_icon_container);
	view->details->update_icon_widget = 
		eazel_services_image_new_from_uri (view->details->update_icon_name,
						   NULL,
						   DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
						   MAX_IMAGE_WIDTH,
						   MAX_IMAGE_HEIGHT);
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
	view->details->update_description_header_widget = 
		eazel_services_label_new (view->details->update_description_header,
					  0,
					  0.5,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  TRUE);

	gtk_widget_show (view->details->update_description_header_widget);
	g_free (view->details->update_description_header);
	view->details->update_description_header = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_header_widget, FALSE, FALSE, 4);	
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	
	/* Body */
	temp_hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (temp_hbox);
	view->details->update_description_body_widget = 
		eazel_services_label_new (view->details->update_description_body,
					  0,
					  0.5,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  FALSE);
	
	nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->update_description_body_widget), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->update_description_body_widget), GTK_JUSTIFY_LEFT);
	gtk_widget_show (view->details->update_description_body_widget);

	g_free (view->details->update_description_body);
	view->details->update_description_body = NULL;
	gtk_box_pack_start (GTK_BOX (temp_hbox), view->details->update_description_body_widget, FALSE, FALSE, 6);
	gtk_box_pack_start (GTK_BOX (temp_vbox), temp_hbox, FALSE, FALSE, 4);
	
	/* Version */
	temp_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (temp_hbox);
	view->details->update_description_version_widget = 
		eazel_services_label_new (view->details->update_description_version,
					  0,
					  0.5,
					  0.5,
					  0,
					  0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  SUMMARY_TEXT_BODY_SIZE_REL,
					  TRUE);

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

/* callback to handle the goto a service button. */
static void
goto_service_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	nautilus_view_open_location_in_this_window (cbdata->nautilus_view, cbdata->uri);

}

/* callback to handle update buttons. */
static void
goto_update_cb (GtkWidget      *button, ServicesButtonCallbackData	*cbdata)
{
	
	nautilus_view_open_location_in_this_window (cbdata->nautilus_view, cbdata->uri);

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

	view->details->user_control = ammonite_get_user_control();

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

