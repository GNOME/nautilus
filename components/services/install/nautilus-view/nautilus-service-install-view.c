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

#include "nautilus-service-install-view.h"
#include "nautilus-service-install.h"
#include "shared-service-widgets.h"
#include "shared-service-utilities.h"
#include <libeazelinstall.h>

#include <rpm/rpmlib.h>
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
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define NEXT_VIEW				"http://eazel1/services/summary2.html"

static void       nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass	*klass);
static void       nautilus_service_install_view_initialize       (NautilusServiceInstallView		*view);
static void       nautilus_service_install_view_destroy          (GtkObject				*object);
static void       service_install_load_location_callback         (NautilusView				*nautilus_view,
								  const char				*location,
								  NautilusServiceInstallView		*view);
static void       generate_install_form                          (NautilusServiceInstallView		*view);
static void       fake_overall_install_progress                  (NautilusServiceInstallView		*view);
static void       generate_current_progress                      (NautilusServiceInstallView		*view,
								  char					*progress_message);
static void       nautilus_service_install_view_update_from_uri  (NautilusServiceInstallView		*view,
								  const char				*uri);
static void       show_overall_feedback                          (NautilusServiceInstallView		*view,
								  char					*progress_message);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceInstallView, nautilus_service_install_view, GTK_TYPE_EVENT_BOX)

static void
generate_install_form (NautilusServiceInstallView	*view) 
{
	GdkFont		*font;
	GtkWidget	*temp_box;
	GtkWidget	*title;
	GtkWidget	*middle_title;

	/* allocate the parent box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* Setup the title */
	title = create_services_title_widget ("Easy Install");
        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* Add package information */

	/* Package Name */
	view->details->package_name = gtk_label_new (_("Installing \"The Gimp\""));
	gtk_misc_set_alignment (GTK_MISC (view->details->package_name), 0.1, 0.1);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->package_name, FALSE, FALSE, 2);
	font = nautilus_font_factory_get_font_from_preferences (20);
	nautilus_gtk_widget_set_font (view->details->package_name, font);
	gtk_widget_show (view->details->package_name);
	gdk_font_unref (font);

	/* Package Description */
	view->details->package_details = gtk_label_new ("Description: The GIMP is the GNU Image Manipulation Program.  It is a freely distributed piece of software suitable for such tasks as photo retouching, image composition, and image authoring.");
	gtk_misc_set_alignment (GTK_MISC (view->details->package_details), 0.25, 0.6);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->package_details), TRUE);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->package_details, FALSE, FALSE, 0);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->package_details, font);
	gtk_widget_show (view->details->package_details);
	gdk_font_unref (font);

	/* Package Version */
	view->details->package_version = gtk_label_new ("Version 1.0.4-1");
	gtk_misc_set_alignment (GTK_MISC (view->details->package_version), 0.1, 0.1);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->package_version, FALSE, FALSE, 2);
	font = nautilus_font_factory_get_font_from_preferences (13);
	nautilus_gtk_widget_set_font (view->details->package_version, font);
	gtk_widget_show (view->details->package_version);
	gdk_font_unref (font);

	/* generate the overall progress bar */
	temp_box = gtk_alignment_new (0.1, 0.1, 0, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 4);
	gtk_widget_show (temp_box);
	view->details->total_progress_bar = gtk_progress_bar_new ();
	gtk_container_add (GTK_CONTAINER (temp_box), view->details->total_progress_bar);
	gtk_widget_show (view->details->total_progress_bar);

	/* add a label for progress messages, but don't show it until there's a message */
	view->details->overall_feedback_text = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (view->details->overall_feedback_text), 0.1, 0.1);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->overall_feedback_text, font);
	gtk_box_pack_start (GTK_BOX (view->details->form), view->details->overall_feedback_text, FALSE, FALSE, 8);

	/* Setup the progress header */
	middle_title = create_services_header_widget ("Messages", "Progress");
        gtk_box_pack_start (GTK_BOX (view->details->form), middle_title, FALSE, FALSE, 0);
	gtk_widget_show (middle_title);

	/* add an hbox at the bottom of the screen to hold current progress but don't show it yet */
        view->details->message_box = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_end (GTK_BOX (view->details->form), view->details->message_box, FALSE, FALSE, 4);
	gtk_widget_show (view->details->message_box);

}

static void
fake_overall_install_progress (NautilusServiceInstallView	*view) {

	int 	counter;

	for (counter = 1; counter <= 20000; counter++) {
		float value;

		value = (float) counter / 20000;

		if (counter == 1) {
			show_overall_feedback (view, "Installing package 1 of 4 ...");
		}
		if (counter == 5000) {
			show_overall_feedback (view, "Installing package 2 of 4 ...");
		}
		if (counter == 10000) {
			show_overall_feedback (view, "Installing package 3 of 4 ...");
		}
		if (counter == 15000) {
			show_overall_feedback (view, "Installing package 4 of 4 ...");
		}
		if (counter == 20000) {
			break;
		}
		else {
			gtk_progress_bar_update (GTK_PROGRESS_BAR (view->details->total_progress_bar), value);
			while (gtk_events_pending ()) {
				gtk_main_iteration ();
			}
		}
	}

}

static void
generate_current_progress (NautilusServiceInstallView	*view, char	*progress_message) {

	GtkWidget	*temp_container;
	GtkWidget	*temp_box;
	GdkFont		*font;
	int		counter;
     
	temp_container = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->message_box), temp_container, 0, 0, 4);
	gtk_widget_show (temp_container);

	/* add a label for progress messages, but don't show it until there's a message */
	view->details->current_feedback_text = gtk_label_new ("");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (view->details->current_feedback_text, font);
	gtk_box_pack_start (GTK_BOX (temp_container), view->details->current_feedback_text, 0, 0, 8);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->current_feedback_text), TRUE);

	/* Create a center alignment object */
	temp_box = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_box_pack_end (GTK_BOX (temp_container), temp_box, FALSE, FALSE, 8);
			    gtk_widget_show (temp_box);
	gtk_widget_show (temp_box);
	view->details->current_progress_bar = gtk_progress_bar_new ();
	gtk_container_add (GTK_CONTAINER (temp_box), view->details->current_progress_bar);
	gtk_widget_show (view->details->current_progress_bar);

	/* bogus current progress loop */
	for (counter = 1; counter <= 5000; counter++) {
		float value;

		value = (float) counter / 5000;

		if (counter == 1) {
			gtk_label_set_text (GTK_LABEL (view->details->current_feedback_text), progress_message);
			gtk_widget_show (view->details->current_feedback_text);
		}
		if (counter == 5000) {
			gtk_label_set_text (GTK_LABEL (view->details->current_feedback_text), "Complete!");
			gtk_widget_show (view->details->current_feedback_text);
			break;
		}
		else {
			gtk_progress_bar_update (GTK_PROGRESS_BAR (view->details->current_progress_bar), value);
			while (gtk_events_pending ()) {
				gtk_main_iteration ();
			}
		}
	}

}

/* utility routine to show an error message */
static void
show_overall_feedback (NautilusServiceInstallView	*view, char	*progress_message) {

	gtk_label_set_text (GTK_LABEL (view->details->overall_feedback_text), progress_message);
	gtk_widget_show (view->details->overall_feedback_text);

}

static void
nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_service_install_view_destroy;
}

static void
nautilus_service_install_view_initialize (NautilusServiceInstallView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusServiceInstallViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (service_install_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_service_install_view_destroy (GtkObject *object) {

	NautilusServiceInstallView	*view;
	
	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_service_install_view_get_nautilus_view (NautilusServiceInstallView *view) {

	return view->details->nautilus_view;

}

/* here's where we do most of the real work of populating the view with info from the package */
/* open the package and copy the information, and then set up the appropriate views with it */
/* FIXME bugzilla.eazel.com 725: use gnome-vfs to open the package */

static void
nautilus_service_install_view_update_from_uri (NautilusServiceInstallView	*view, const char	*uri) {

	/* open the package */
	HeaderIterator		iterator;
	Header			header_info;
	Header			signature;
	int			iterator_tag;
	int			type;
	int			data_size;
	int			result;
	char			*data_ptr;
	char			*temp_str;
	FD_t			file_descriptor;
	int			*integer_ptr;
	PackageData		*pack;

	char			*temp_version = NULL;
	char			*temp_release = NULL;
	char			*package_name = NULL;

	const char		*path_name = uri + 7;

	file_descriptor = fdOpen (path_name, O_RDONLY, 0644);

	if (file_descriptor != NULL) {

		/* read out the appropriate fields, and set them up in the view */
		result = rpmReadPackageInfo (file_descriptor, &signature, &header_info);
		if (result) {
			g_message ("couldn't read package!");
			return;
		}

		iterator = headerInitIterator (header_info);
		while (headerNextIterator (iterator, &iterator_tag, &type, (void**) &data_ptr, &data_size)) {
			integer_ptr = (int*) data_ptr;
			switch (iterator_tag) {
				case RPMTAG_NAME:
					package_name = g_strdup (data_ptr);
					temp_str = g_strdup_printf (_("Installing \"%s\" "), data_ptr);
					gtk_label_set (GTK_LABEL (view->details->package_name), temp_str);
					g_free (temp_str);
					break;
				case RPMTAG_VERSION:
					temp_version = g_strdup (data_ptr);
					break;
				case RPMTAG_RELEASE:
					temp_release = g_strdup (data_ptr);
					break;
				case RPMTAG_SUMMARY:
					gtk_label_set (GTK_LABEL (view->details->package_summary), data_ptr+4);
					break;
				case RPMTAG_DESCRIPTION:
					gtk_label_set (GTK_LABEL (view->details->package_details), data_ptr+4);
					break;
				case RPMTAG_SIZE:
					break;
				case RPMTAG_DISTRIBUTION:
					break;
				case RPMTAG_GROUP:
					break;
				case RPMTAG_ICON:
					break;
				case RPMTAG_LICENSE:
					break;
				case RPMTAG_BUILDTIME:
					break;
				case RPMTAG_INSTALLTIME:
					break;
				case RPMTAG_VENDOR:
					break;
				case RPMTAG_GIF:
					break;
				case RPMTAG_XPM:
					break;
			}

		}

		if (temp_version) {
			temp_str = g_strdup_printf (_("Version %s-%s"), temp_version, temp_release);
			gtk_label_set (GTK_LABEL (view->details->package_version), temp_str);
			g_free (temp_str);
		}

		headerFreeIterator (iterator);

		/* close the package */
		fdClose (file_descriptor);
	}

	/* NOTE: This adds a libeazelinstall packagedata object to the view */
	pack = (PackageData*) gtk_object_get_data (GTK_OBJECT (view), "packagedata");
	if (pack != NULL) {
		/* Destroy the old */
			packagedata_destroy (pack);
	}
	pack = packagedata_new ();
	pack->name = g_strdup (package_name);
	pack->version = g_strdup (temp_version);
	pack->minor = g_strdup (temp_release);
	gtk_object_set_data (GTK_OBJECT (view), "packagedata", pack);

	if (package_name) {
		g_free (package_name);
	}
	if (temp_version) {
		g_free (temp_version);
	}
	if (temp_release) {
		g_free (temp_release);
	}

	/* Ad hock crap to fake a full install */
	show_overall_feedback (view, "Checking Dependancies");
	show_overall_feedback (view, "Waiting for downloads");
	generate_current_progress (view, "Downloading png image libraries ...");
	show_overall_feedback (view, "Checking Dependancies");
	show_overall_feedback (view, "Waiting for downloads");
	generate_current_progress (view, "Downloading gnome core libraries ...");		
	show_overall_feedback (view, "Checking Dependancies");
	show_overall_feedback (view, "Waiting for downloads");
	generate_current_progress (view, "Downloading gtk+ libraries ...");
	show_overall_feedback (view, "Checking Dependancies");
	show_overall_feedback (view, "Waiting for downloads");
	generate_current_progress (view, "Downloading glib libraries ...");		
	fake_overall_install_progress (view);

}

void
nautilus_service_install_view_load_uri (NautilusServiceInstallView	*view,
			     	        const char			*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_install_form (view);
	nautilus_service_install_view_update_from_uri (view, uri);
}

static void
service_install_load_location_callback (NautilusView			*nautilus_view, 
			  	        const char			*location,
			       		NautilusServiceInstallView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_service_install_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);

	go_to_uri (view->details->nautilus_view, NEXT_VIEW);

}

