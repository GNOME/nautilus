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
#include "nautilus-summary-view-private.h"

#include "nautilus-summary-callbacks.h"
#include "nautilus-summary-menu-items.h"
#include "nautilus-summary-dialogs.h"
#include "nautilus-summary-footer.h"

#include "eazel-services-footer.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#ifdef HAVE_RPM
#include "../inventory/eazel-inventory.h"
#endif

#include <libnautilus-extensions/nautilus-clickable-image.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-tabs.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-viewport.h>


#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>

#include <bonobo/bonobo-control.h>

#include <gnome.h>
#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#define notDEBUG_TEST	1
#define notDEBUG_PEPPER	1

#ifdef DEBUG_TEST
	#undef URL_REDIRECT_TABLE_HOME
	#define URL_REDIRECT_TABLE_HOME		"http://localhost/redirects.xml"
#endif

#define SUMMARY_TEXT_HEADER_SIZE_REL (0)
#define SUMMARY_TEXT_BODY_SIZE_REL (-2)

typedef struct ServicesButtonCallbackData ServicesButtonCallbackData;

struct ServicesButtonCallbackData {
	NautilusView    *view;
	char            *uri;
};

static void     nautilus_summary_view_initialize_class (NautilusSummaryViewClass   *klass);
static void     nautilus_summary_view_initialize       (NautilusSummaryView        *view);
static void     nautilus_summary_view_destroy          (GtkObject                  *object);
static void     summary_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusSummaryView        *view);
static void     summary_stop_loading_callback          (NautilusView               *nautilus_view,
							NautilusSummaryView        *view);
static GtkWidget * generate_eazel_news_entry_row       (NautilusSummaryView        *view,
							void                       *data);
static GtkWidget * generate_service_entry_row          (NautilusSummaryView        *view,
							void                       *data);
static GtkWidget * generate_update_news_entry_row      (NautilusSummaryView        *view,
							void                       *data);
static void     summary_view_button_callback           (GtkWidget                  *button, 
							ServicesButtonCallbackData *cbdata);
static void     cancel_load_in_progress                (NautilusSummaryView *view);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSummaryView, nautilus_summary_view, GTK_TYPE_EVENT_BOX)

static const char *footer_online_items[] =
{
	N_("Account Preferences"),
	N_("Logout"),
	N_("Terms of Use"),
	N_("Privacy Statement")
};

static const char *footer_offline_items[] =
{
	N_("Register"),
	N_("Login"),
	N_("Terms of Use"),
	N_("Privacy Statement")
};


static char **
localize_items (const char **items, 
		int nitems)
{
	int i;
	char **retval;

	retval = g_new0 (char *, nitems);

	for (i = 0; i < nitems; i++) {
		retval[i] = gettext (items[i]);
	}

	return retval;
}


static void
update_header (NautilusSummaryView *view)
{
	char *text;
	if (view->details->logged_in) {
		g_free (view->details->user_name);
		view->details->user_name = ammonite_get_default_user_username ();
		text = g_strdup_printf (_("Welcome, %s!"), view->details->user_name);
		eazel_services_header_set_left_text (EAZEL_SERVICES_HEADER (view->details->header), text);
		g_free (text);
	} else {
		eazel_services_header_set_left_text (EAZEL_SERVICES_HEADER (view->details->header),
						     _("You are not logged in"));
	}
}

static void
create_header (NautilusSummaryView *view)
{
	view->details->header = eazel_services_header_title_new (_("Connecting to Eazel Services..."));
}


static void
update_footer (NautilusSummaryView *view)
{
	char **localized_items;
	int size;

	if (view->details->logged_in) {	
		size = NAUTILUS_N_ELEMENTS (footer_online_items);
		localized_items = localize_items (footer_online_items, size);
	} else {
		size = NAUTILUS_N_ELEMENTS (footer_offline_items);
		localized_items = localize_items (footer_offline_items, size);
	}
	
	eazel_services_footer_update (EAZEL_SERVICES_FOOTER (view->details->footer),
				      (const char **) localized_items,
				      size);
	g_free (localized_items);
}

static void
create_footer (NautilusSummaryView *view)
{
	view->details->footer = eazel_services_footer_new ();

	gtk_signal_connect (GTK_OBJECT (view->details->footer), "item_clicked", 
			    GTK_SIGNAL_FUNC (footer_item_clicked_callback), view);

	update_footer (view);
}


static void
services_button_callback_data_free (ServicesButtonCallbackData *cbdata)
{
	g_free (cbdata->uri);
	g_free (cbdata);
}


static void
summary_view_button_callback (GtkWidget                  *button, 
			      ServicesButtonCallbackData *cbdata)
{
	
	nautilus_view_open_location_in_this_window (cbdata->view, cbdata->uri);
}

static void
goto_uri_on_clicked (GtkWidget *widget,
		     NautilusView *view,
		     const char *uri)
{
	ServicesButtonCallbackData *cbdata;
	
	cbdata = g_new0 (ServicesButtonCallbackData, 1);
	cbdata->view = view;
	cbdata->uri = g_strdup (uri);
	
	gtk_signal_connect_full (GTK_OBJECT (widget), "clicked",
			         GTK_SIGNAL_FUNC (summary_view_button_callback), NULL,
				 cbdata, (GtkDestroyNotify) services_button_callback_data_free,
				 FALSE, FALSE);
}

static GtkWidget *
summary_view_button_new (char *label_text,
			 NautilusView *view,
			 const char *uri)
{
	GtkWidget *button;
	GtkWidget *label;
	
	button = gtk_button_new ();
	/* FIXME: hardcoded width! */
	gtk_widget_set_usize (button, 80, -1);
	
	label = gtk_label_new (label_text);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (button), label);
	
	goto_uri_on_clicked (button, view, uri);
	
	return button;
}

static GtkWidget *
summary_view_link_image_new (NautilusSummaryView *view,
			     const char *image_uri,
			     const char *click_uri)
{
	GtkWidget *image;

	image = eazel_services_clickable_image_new_from_uri (image_uri,
							     NULL,
							     DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
							     MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
	nautilus_clickable_image_set_prelight (NAUTILUS_CLICKABLE_IMAGE (image), TRUE);

	goto_uri_on_clicked (image, view->details->nautilus_view, click_uri);
	
	return image;
}


static GtkWidget *
summary_view_item_label_new (char *label_text, 
			     int relative_font_size,
			     gboolean bold)
{
	GtkWidget *label;

	label = eazel_services_label_new (label_text,
					  0, 0.5, 0.5, 0, 0,
					  DEFAULT_SUMMARY_TEXT_COLOR_RGB,
					  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
					  NULL,
					  relative_font_size,
					  bold);
	nautilus_label_set_wrap (NAUTILUS_LABEL (label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	nautilus_label_set_adjust_wrap_on_resize (NAUTILUS_LABEL (label), TRUE);

	return label;
}


static GtkWidget *
summary_view_item_large_header_label_new (char *label_text)
{
	return summary_view_item_label_new (label_text, 
					    SUMMARY_TEXT_HEADER_SIZE_REL,
					    TRUE);
}

static GtkWidget *
summary_view_item_header_label_new (char *label_text)
{
	return summary_view_item_label_new (label_text, 
					    SUMMARY_TEXT_BODY_SIZE_REL,
					    TRUE);
}

static GtkWidget *
summary_view_item_body_label_new (char *label_text)
{
	return summary_view_item_label_new (label_text, 
					    SUMMARY_TEXT_BODY_SIZE_REL,
					    FALSE);
}

static void
append_hseparator_to_vbox (GtkWidget *vbox)
{
	GtkWidget *separator;

	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (vbox), 
			    separator, FALSE, FALSE, 8);

}




static GtkWidget *
generate_eazel_news_entry_row  (NautilusSummaryView *view,
				void                *data)
{
	GtkWidget *news_row;
	GtkWidget *item_vbox;
	GtkWidget *icon_box;
	GtkWidget *icon;
	GtkWidget *date_label;
	GtkWidget *news_item_label;
	EazelNewsData *news_node;

	news_node = data;
	news_row = gtk_hbox_new (FALSE, 0);

	/* Generate first box with icon */
	icon_box = gtk_vbox_new (FALSE, 2);
	gtk_widget_show (icon_box);
	gtk_box_pack_start (GTK_BOX (news_row), icon_box, FALSE, FALSE, 3);

	icon = eazel_services_image_new_from_uri (news_node->icon,
						  NULL,
						  DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB,
						  MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (icon_box), icon, 0, 0, 0);

	/* generate second box with bold type date and the actual contents */
	item_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (item_vbox);
	gtk_box_pack_start (GTK_BOX (news_row), item_vbox, TRUE, TRUE, 2);

	/* Date */
	date_label = summary_view_item_header_label_new (news_node->date);
	gtk_widget_show (date_label);

	gtk_box_pack_start (GTK_BOX (item_vbox), date_label, FALSE, FALSE, 2);

	/* Message */
	news_item_label = summary_view_item_body_label_new (news_node->message);
	gtk_widget_show (news_item_label);
	gtk_box_pack_start (GTK_BOX (item_vbox), news_item_label, TRUE, TRUE, 2);
	
	return news_row;
}


typedef GtkWidget * (*SummaryViewItemCreateFunction) (NautilusSummaryView *view,
						      void                *data);

static void
summary_view_update_pane (NautilusSummaryView          *view,
			  GtkWidget                    *vbox,
			  GList                        *data,
			  SummaryViewItemCreateFunction item_create)
{
	GtkWidget     *item;
	GList         *node;

	/* clear existing news. */
	gtk_container_foreach (GTK_CONTAINER (vbox), 
			       (GtkCallback) gtk_widget_destroy, NULL);

	/* build the eazel news table from the xml file */
	for (node = data; node != NULL; node = node->next) {
		item = (*item_create) (view, node->data);

		gtk_widget_show (item);
		gtk_box_pack_start (GTK_BOX (vbox), 
				    GTK_WIDGET (item), 
				    FALSE, FALSE, 0);
		
		if (node->next != NULL) {
			append_hseparator_to_vbox (vbox);
		}
	}
}

static GtkWidget *
summary_view_create_pane (NautilusSummaryView *view,
			  GtkWidget          **vbox)
{
	GtkWidget *pane;
	GtkWidget *viewport;

	pane =  gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane),
			                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
	viewport = nautilus_viewport_new (NULL, NULL);
	nautilus_viewport_set_constrain_width (NAUTILUS_VIEWPORT (viewport), TRUE);
	widget_set_nautilus_background_color (viewport, DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC);

	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_widget_show (viewport);
	gtk_container_add (GTK_CONTAINER (pane), viewport);

	/* create the parent update news box and a table to hold the labels and text entries */
	*vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (*vbox);
	gtk_container_add (GTK_CONTAINER (viewport), *vbox);

	return pane;
}


static void
update_news_pane (NautilusSummaryView *view)
{
	summary_view_update_pane (view,
				  view->details->news_item_vbox,
				  view->details->xml_data->eazel_news_list,
				  generate_eazel_news_entry_row); 
		
	/* FIXME: leak */
	g_list_free (view->details->xml_data->eazel_news_list);
}


static void
create_news_pane (NautilusSummaryView *view)
{
	view->details->news_pane = summary_view_create_pane 
		(view, &view->details->news_item_vbox);
}


static GtkWidget *
generate_service_entry_row  (NautilusSummaryView *view,
			     void                *data)
{
	GtkWidget *services_row;
	GtkWidget *icon_box;
	GtkWidget *icon;
	GtkWidget *service_name;
	GtkWidget *service_description;
	GtkWidget *description_vbox;
	GtkWidget *button_vbox;
	GtkWidget *button_hbox;
	GtkWidget *button;
	ServicesData *services_node;

	services_node = data;

	services_row = gtk_hbox_new (FALSE, 0);

	/* Generate first box with service icon */
	icon_box = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (services_row), icon_box, FALSE, FALSE, 2);

	gtk_widget_show (icon_box);

	icon =  summary_view_link_image_new (view,
					     services_node->icon,
					     services_node->uri);
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (icon_box), icon, FALSE, FALSE, 0);

	/* insert a few pixels of space here */

	/* Generate second box with service title and summary */
	description_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (description_vbox);
	gtk_box_pack_start (GTK_BOX (services_row), description_vbox, TRUE, TRUE, 0);

	/* Header */
	service_name = summary_view_item_header_label_new (services_node->description_header);
	gtk_widget_show (service_name);
	gtk_box_pack_start (GTK_BOX (description_vbox), service_name, FALSE, FALSE, 2);
	
	/* Body */
	service_description = summary_view_item_body_label_new (services_node->description);
	gtk_widget_show (service_description);
	gtk_box_pack_start (GTK_BOX (description_vbox), service_description, FALSE, FALSE, 2);

	/* Add the redirect button to the third box */
	button_vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_show (button_vbox);
	gtk_box_pack_end (GTK_BOX (services_row), button_vbox, FALSE, FALSE, 2);
	
	button_hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (button_hbox);
	gtk_box_pack_start (GTK_BOX (button_vbox), button_hbox, FALSE, FALSE, 2);	

	button = summary_view_button_new (GOTO_BUTTON_LABEL,
					  view->details->nautilus_view,
					  services_node->uri);
	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (button_hbox), button, FALSE, FALSE, 3);

	/* FIXME: respect enabled field */
	   
	return services_row;
}


static void
update_services_list_pane (NautilusSummaryView *view)
{
	summary_view_update_pane (view,
				   view->details->services_list_vbox,
				   view->details->xml_data->services_list,
				   generate_service_entry_row); 
		
	/* FIXME: leak */
	g_list_free (view->details->xml_data->services_list);
}

static void
create_services_list_pane (NautilusSummaryView *view)
{
	view->details->services_list_pane = summary_view_create_pane 
		(view, &view->details->services_list_vbox);
}


static GtkWidget *
generate_update_news_entry_row  (NautilusSummaryView *view,
				 void                *data)
{
	GtkWidget *update_row;
	GtkWidget *icon_box;
	GtkWidget *icon;
	GtkWidget *description_vbox;
	GtkWidget *name_label;
	GtkWidget *description_label;
	GtkWidget *version_label;
	GtkWidget *button_vbox;
	GtkWidget *more_info_button_hbox;
	GtkWidget *more_info_button;
	GtkWidget *install_button_hbox;
	GtkWidget *install_button;
	char      *version_text;
	UpdateNewsData *update_node;

	update_node = data;
	update_row = gtk_hbox_new (FALSE, 0);

	/* Generate first box with icon */
	icon_box = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (icon_box);
	gtk_box_pack_start (GTK_BOX (update_row), icon_box, FALSE, FALSE, 0);

	icon =  summary_view_link_image_new (view,
					     update_node->icon,
					     update_node->softcat_uri);
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (icon_box), icon, FALSE, FALSE, 0);

	/* Generate second box with update title, summary, and version */
	description_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (description_vbox);
	gtk_box_pack_start (GTK_BOX (update_row), description_vbox, TRUE, TRUE, 0);
	
	/* Header */

	name_label = summary_view_item_large_header_label_new (update_node->name);
	gtk_widget_show (name_label);
	gtk_box_pack_start (GTK_BOX (description_vbox), name_label, FALSE, FALSE, 4);
	
	/* Body */

	description_label = summary_view_item_body_label_new (update_node->description);
	gtk_widget_show (description_label);
	gtk_box_pack_start (GTK_BOX (description_vbox), description_label, FALSE, FALSE, 4);
	
	/* Version */
	
	if (update_node->version != NULL) {
		version_text = g_strdup_printf (_("Version: %s"), update_node->version);
	} else {
		version_text = g_strdup ("");
	}
	version_label = summary_view_item_header_label_new (version_text);
	gtk_widget_show (version_label);
	gtk_box_pack_start (GTK_BOX (description_vbox), version_label, FALSE, FALSE, 0);
	g_free (version_text);

	/* Add the redirect button and softcat button to the third box */
	button_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (button_vbox);
	gtk_box_pack_end (GTK_BOX (update_row), button_vbox, FALSE, FALSE, 0);

	more_info_button_hbox = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (more_info_button_hbox);
	gtk_box_pack_start (GTK_BOX (button_vbox), more_info_button_hbox, FALSE, FALSE, 4);


	more_info_button = summary_view_button_new (SOFTCAT_GOTO_BUTTON_LABEL,
						    view->details->nautilus_view,
						    update_node->softcat_uri);
	gtk_widget_show (more_info_button);
	gtk_box_pack_start (GTK_BOX (more_info_button_hbox), more_info_button, FALSE, FALSE, 4);


	install_button_hbox = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (install_button_hbox);
	gtk_box_pack_start (GTK_BOX (button_vbox), install_button_hbox, FALSE, FALSE, 4);
	
	install_button = summary_view_button_new (INSTALL_GOTO_BUTTON_LABEL,
						  view->details->nautilus_view,
						  update_node->uri);
	gtk_widget_show (install_button);
	gtk_box_pack_start (GTK_BOX (install_button_hbox), install_button, FALSE, FALSE, 4);

	return update_row;
}

static void
update_featured_downloads_pane (NautilusSummaryView *view)
{
	summary_view_update_pane (view,
				   view->details->featured_downloads_vbox,
				   view->details->xml_data->update_news_list,
				   generate_update_news_entry_row); 

	/* FIXME: leak */
	g_list_free (view->details->xml_data->update_news_list);
}


static void
create_featured_downloads_pane (NautilusSummaryView *view)
{
	view->details->featured_downloads_pane = summary_view_create_pane 
		(view, &view->details->featured_downloads_vbox);
}


static void
update_summary_form (NautilusSummaryView *view,
		     SummaryData *xml_data)
{
	view->details->xml_data = xml_data;

	update_header (view);
	update_news_pane (view);
	update_services_list_pane (view);
	update_featured_downloads_pane (view);
	update_footer (view);
}


static void
create_summary_form (NautilusSummaryView *view)
{
	GtkWidget               *notebook_tabs;
	
	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);

	create_header (view);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->header, FALSE, FALSE, 0);
	gtk_widget_show (view->details->header);

	/* Create the News pane */
	create_news_pane (view);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->news_pane, FALSE, FALSE, 0);
	gtk_widget_show (view->details->news_pane);		

	/* Header for Services pane */
	notebook_tabs = nautilus_tabs_new ();
	nautilus_tabs_add_tab (NAUTILUS_TABS (notebook_tabs), _("Services"), 0);
	gtk_widget_show (notebook_tabs);
	gtk_box_pack_start (GTK_BOX (view->details->form), notebook_tabs, FALSE, FALSE, 0);

	/* Create the Services pane */
	create_services_list_pane (view);
	gtk_widget_show (view->details->services_list_pane);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->services_list_pane, TRUE, TRUE, 0);

	/* Header for Featured Downloads pane */
	notebook_tabs = nautilus_tabs_new ();
	nautilus_tabs_add_tab (NAUTILUS_TABS (notebook_tabs), _("Featured Downloads"), 0);
	gtk_widget_show (notebook_tabs);
	gtk_box_pack_start (GTK_BOX (view->details->form), notebook_tabs, FALSE, FALSE, 0);

	/* Create the Featured Downloads pane */
	create_featured_downloads_pane (view);
	gtk_widget_show (view->details->featured_downloads_pane);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->featured_downloads_pane, TRUE, TRUE, 0);

	create_footer (view);
	gtk_widget_show (view->details->footer);
	gtk_box_pack_start (GTK_BOX (view->details->form), 
			    view->details->footer, 
			    FALSE, FALSE, 0);


	/* Finally, show the form that hold everything */
	gtk_widget_show (view->details->form);
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

	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "stop_loading",
			    GTK_SIGNAL_FUNC (summary_stop_loading_callback), 
			    view);



	view->details->user_control = ammonite_get_user_control ();

	if (CORBA_NO_EXCEPTION != ev._major) {
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

	create_summary_form (view);

	gtk_widget_show (GTK_WIDGET (view));

	CORBA_exception_free (&ev);

}

static void
nautilus_summary_view_destroy (GtkObject *object)
{

	NautilusSummaryView *view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	view = NAUTILUS_SUMMARY_VIEW (object);

	cancel_load_in_progress (view);

	if (view->details->uri) {
		g_free (view->details->uri);
	}

	/* FIXME: what the hell, we can't assert this here */
	g_assert (Pending_None == view->details->pending_operation);

	g_free (view->details);
	
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));

	CORBA_exception_free (&ev);

}

NautilusView *
nautilus_summary_view_get_nautilus_view (NautilusSummaryView *view)
{

	return view->details->nautilus_view;

}

static void
summary_fetch_callback (GnomeVFSResult result,
			SummaryData *xml_data,
			gpointer callback_data)
{
	NautilusSummaryView *view;

	view = NAUTILUS_SUMMARY_VIEW (callback_data);
	view->details->summary_fetch_handle = NULL;

	if (result != GNOME_VFS_OK) {
		nautilus_summary_show_error_dialog 
			(view, _("Unable to get services data from Eazel's server. "
				 "The server might be unavailable right now, "
				 "or your computer might be configured incorrectly. "
				 "Please contact support@eazel.com."));
		return;
	} 

	
	if (xml_data == NULL) {
		nautilus_summary_show_error_dialog 
			(view, _("Found a problem with services data on Eazel servers. "
				 "Please contact support@eazel.com."));
		return;
	} 

	update_summary_form (view, xml_data);		
		
	if (!view->details->logged_in) {
		nautilus_summary_show_login_dialog (view);
	}

	nautilus_view_report_load_complete (view->details->nautilus_view);
}

static void fetch_summary_data (NautilusSummaryView *view) {
	char *uri;

	/* Read and parse summary view XML */

	uri = trilobite_redirect_lookup (SUMMARY_XML_KEY);

	if (uri == NULL) {
		nautilus_summary_show_error_dialog 
			(view, _("Information is missing from the redirect data on Eazel servers. "
				 "Please contact support@eazel.com."));
		return;
	}
	view->details->summary_fetch_handle = eazel_summary_fetch_data_async 
		(uri, summary_fetch_callback, view);

	g_free (uri);
}

#ifdef HAVE_RPM
static void
inventory_load_callback (EazelInventory *inventory,
	  		 gboolean succeeded,
	  		 gpointer callback_data)
{
	NautilusSummaryView *view;

	view = NAUTILUS_SUMMARY_VIEW (callback_data);

	gtk_object_unref (GTK_OBJECT (inventory));

	fetch_summary_data (view);
}
#endif

static void
redirect_fetch_callback (GnomeVFSResult result,
			 gboolean parsed_xml,
			 gpointer callback_data)
{
	NautilusSummaryView *view;
#ifdef HAVE_RPM
	EazelInventory *inventory_service;
#endif

	view = NAUTILUS_SUMMARY_VIEW (callback_data);

	view->details->redirect_fetch_handle = NULL;

	if (result != GNOME_VFS_OK) {
		nautilus_summary_show_error_dialog 
			(view, _("Unable to connect to Eazel's server. "
				 "The server might be unavailable right now, "
				 "or your computer might be configured incorrectly. "
				 "You could try again later."));
		return;
	} 


	if (!parsed_xml) {
		nautilus_summary_show_error_dialog 
			(view, _("Found a problem with redirect data on Eazel servers. "
				 "Please contact support@eazel.com."));
		return;
	} 


#ifdef HAVE_RPM
	if (view->details->logged_in) {
		inventory_service = eazel_inventory_get ();

		if (inventory_service) {
			eazel_inventory_upload (inventory_service, 
						inventory_load_callback,
						view);

		} else {
			nautilus_summary_show_error_dialog 
				(view, _("Failed to upload system inventory."));
			fetch_summary_data (view);
		}
	} else {
		fetch_summary_data (view);
	}
#else
	fetch_summary_data (view);
#endif
}




static void
cancel_load_in_progress (NautilusSummaryView *view)
{
	if (view->details->redirect_fetch_handle != NULL) {
		trilobite_redirect_fetch_table_cancel (view->details->redirect_fetch_handle);
		view->details->redirect_fetch_handle = NULL;
	}

	if (view->details->summary_fetch_handle != NULL) {
		eazel_summary_fetch_data_cancel (view->details->summary_fetch_handle);
		view->details->summary_fetch_handle = NULL;
	}
}

void
nautilus_summary_view_load_uri (NautilusSummaryView	*view,
			        const char		*uri)
{
	char		*user_name;

	/* set up some sanity values for error control */
	view->details->attempt_number = 0;
	view->details->current_attempt = initial;

	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	user_name = ammonite_get_default_user_username ();
	view->details->logged_in = (NULL != user_name);
	g_free (user_name);
	user_name = NULL;

	cancel_load_in_progress (view);

	view->details->redirect_fetch_handle = trilobite_redirect_fetch_table_async 
		(view->details->logged_in ? URL_REDIRECT_TABLE_HOME_2 : URL_REDIRECT_TABLE_HOME,
		 redirect_fetch_callback,
		 view);
}

static void
summary_load_location_callback (NautilusView		*nautilus_view, 
			        const char		*location,
			        NautilusSummaryView	*view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);

	nautilus_view_set_title (nautilus_view, _("Eazel Services"));
	
	nautilus_summary_view_load_uri (view, location);
}



static void
summary_stop_loading_callback (NautilusView               *nautilus_view,
			       NautilusSummaryView        *view)

{
	cancel_load_in_progress (view);
}
