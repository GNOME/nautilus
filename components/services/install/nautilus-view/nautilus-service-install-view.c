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
#include "libtrilobite/libtrilobite.h"

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

/* for uname */
#include <sys/utsname.h>

/* number of rows of [message... progressbar] to scroll at the bottom */
#define STATUS_ROWS		4

#define NEXT_SERVICE_VIEW				"eazel-summary:"

/* This ensures that if the arch is detected as i[3-9]86, the
   requested archtype will be set to i386 */
#define ASSUME_ix86_IS_i386 

static void       nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass	*klass);
static void       nautilus_service_install_view_initialize       (NautilusServiceInstallView		*view);
static void       nautilus_service_install_view_destroy          (GtkObject				*object);
static void       service_install_load_location_callback         (NautilusView				*nautilus_view,
								  const char				*location,
								  NautilusServiceInstallView		*view);
static void       generate_install_form                          (NautilusServiceInstallView		*view);
#if 0
static void       fake_overall_install_progress                  (NautilusServiceInstallView		*view);
static void       generate_current_progress                      (NautilusServiceInstallView		*view,
								  char					*progress_message);
#endif
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
	int i;

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
	view->details->package_details = gtk_label_new ("Description: The GIMP is the GNU Image Manipulation Program.  It is a freely distributed piece of software suitable for such tasks as photo retouching, image composition, and image authoring."
);
	//	gtk_misc_set_alignment (GTK_MISC (view->details->package_details), 0.25, 0.6);
	gtk_label_set_justify (GTK_LABEL (view->details->package_details), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (view->details->package_details), 0.1, 0.1);
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
	gdk_font_unref (font);

	/* filler blob */
	gtk_box_pack_start (GTK_BOX (view->details->form), gtk_label_new (""), TRUE, FALSE, 0);

#if 0
	/* add a vbox at the bottom of the screen to hold current progress but don't show it yet */
        view->details->message_box = gtk_vbox_new (FALSE, 0);
        gtk_box_pack_end (GTK_BOX (view->details->form), view->details->message_box, FALSE, FALSE, 4);
	gtk_widget_show (view->details->message_box);

	/* prepopulate the vbox with spaces */
	font = nautilus_font_factory_get_font_from_preferences (14);
	for (i = 0; i < STATUS_ROWS; i++) {
		view->details->current_feedback_text = gtk_label_new ("  ");
		nautilus_gtk_widget_set_font (view->details->current_feedback_text, font);
		gtk_misc_set_alignment (GTK_MISC (view->details->current_feedback_text), 0.0, 0.0);
		gtk_widget_show (view->details->current_feedback_text);
		gtk_box_pack_start (GTK_BOX (view->details->message_box), view->details->current_feedback_text, TRUE, TRUE, 4);
		gtk_label_set_justify (GTK_LABEL (view->details->current_feedback_text), GTK_JUSTIFY_LEFT);
	}
	gdk_font_unref (font);
#else
	/* add a table at the bottom of the screen to hold current progresses */
	view->details->message_box = gtk_table_new (STATUS_ROWS, 2, FALSE);
	gtk_box_pack_end (GTK_BOX (view->details->form), view->details->message_box, FALSE, FALSE, 4);
	gtk_widget_show (view->details->message_box);

	/* prepopulate left and right columns with spaces */
	font = nautilus_font_factory_get_font_from_preferences (14);
	for (i = 0; i < STATUS_ROWS; i++) {
		GtkWidget *left, *right;

		left = gtk_label_new ("  ");
		nautilus_gtk_widget_set_font (left, font);
		gtk_misc_set_alignment (GTK_MISC (left), 0.0, 0.0);
		gtk_widget_show (left);
		view->details->message_left = g_list_append (view->details->message_left, left);
		gtk_table_attach (GTK_TABLE (view->details->message_box), left, 0, 1, i, i+1,
				  GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 4, 4);

		right = gtk_label_new ("  ");
		nautilus_gtk_widget_set_font (right, font);
		gtk_misc_set_alignment (GTK_MISC (right), 0.0, 0.0);
		gtk_widget_show (right);
		view->details->message_right = g_list_append (view->details->message_right, right);
		gtk_table_attach (GTK_TABLE (view->details->message_box), right, 1, 2, i, i+1,
				  0, 0, 4, 4);

g_message ("ADDED %d : left = %08X, right = %08X", i, (unsigned int)left, (unsigned int)right);
	}
#endif

	/* Setup the progress header */
	middle_title = create_services_header_widget ("Messages", "Progress");
        gtk_box_pack_end (GTK_BOX (view->details->form), middle_title, FALSE, FALSE, 0);
	gtk_widget_show (middle_title);

}

#if 0
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
#endif

#if 0
static void
generate_current_progress (NautilusServiceInstallView	*view, char	*progress_message) {

	GtkWidget	*temp_container;
	GtkWidget	*temp_box;
	GdkFont		*font;
	int		counter;
     
	{
		GList *list;

		list = gtk_container_children (GTK_CONTAINER (view->details->message_box));
		if (g_list_length (list) >= STATUS_ROWS) {
			/* kill off oldest one */
			gtk_container_remove (GTK_CONTAINER (view->details->message_box), list->data);
		}
	}
	temp_container = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->message_box), temp_container, 0, 0, 4);
	gtk_widget_show (temp_container);

	/* add a label for progress messages, but don't show it until there's a message */
	view->details->current_feedback_text = gtk_label_new ("");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (view->details->current_feedback_text, font);
	gtk_misc_set_alignment (GTK_MISC (view->details->current_feedback_text), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (temp_container), view->details->current_feedback_text, TRUE, TRUE, 8);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->current_feedback_text), TRUE);
	gtk_label_set_justify (GTK_LABEL (view->details->current_feedback_text), GTK_JUSTIFY_LEFT);
	gdk_font_unref (font);

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
			/* change progress bar to a label */
			gtk_container_remove (GTK_CONTAINER (temp_box), view->details->current_progress_bar);
			gtk_widget_unref (view->details->current_progress_bar);
			font = nautilus_font_factory_get_font_from_preferences (16);
			view->details->current_progress_bar = gtk_label_new ("Complete!");
			nautilus_gtk_widget_set_font (view->details->current_progress_bar, font);
			gtk_container_add (GTK_CONTAINER (temp_box), view->details->current_progress_bar);
			gtk_widget_show (view->details->current_progress_bar);
			gdk_font_unref (font);
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
#endif


/* utility routine to show an error message */
static void
show_overall_feedback (NautilusServiceInstallView	*view, char	*progress_message)
{

	gtk_label_set_text (GTK_LABEL (view->details->overall_feedback_text), progress_message);
	gtk_widget_show (view->details->overall_feedback_text);

}

static void
nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass *klass)
{

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_service_install_view_destroy;
}

static void
nautilus_service_install_view_initialize (NautilusServiceInstallView *view)
{

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
nautilus_service_install_view_destroy (GtkObject *object)
{

	NautilusServiceInstallView	*view;
	
	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_service_install_view_get_nautilus_view (NautilusServiceInstallView *view)
{

	return view->details->nautilus_view;

}


static PackageData *
create_package (char *name) 
{
	struct utsname buf;
	PackageData *pack;

	uname (&buf);
	pack = packagedata_new ();
	pack->name = g_strdup (name);
	pack->archtype = g_strdup (buf.machine);
#ifdef ASSUME_ix86_IS_i386
	if (strlen (pack->archtype)==4 && pack->archtype[0]=='i' &&
	    pack->archtype[1]>='3' && pack->archtype[1]<='9' &&
	    pack->archtype[2]=='8' && pack->archtype[3]=='6') {
		g_free (pack->archtype);
		pack->archtype = g_strdup ("i386");
	}
#endif
	pack->distribution = trilobite_get_distribution ();
	pack->toplevel = TRUE;
	
	return pack;
}

/* quick & dirty: parse the url into (host, port) and a category list */
static void
nautilus_install_parse_uri (const char *uri, char **host, int *port, GList **categories)
{
	char *p, *q, *plist, *package_name;
	GList *packages = NULL;

	*host = NULL;
	*port = 80;
	*categories = NULL;

	p = strchr (uri, ':');
	if (! p) {
		/* bad mojo */
		return;
	}
	p++;
	if ((*p == '/') && (*(p+1) == '/')) {
		p += 2;
	}

	/* p = beginning of host.  now find ending "/" */
	q = strchr (p, '/');
	if (q) {
		*host = g_strndup (p, q-p);
	} else {
		*host = g_strdup (p);
	}

	/* *host contains host[:port] -- pull off the port if it's there */
	p = strchr(*host, ':');
	if (p) {
		*port = atoi (p+1);
		*p = 0;
	}

	if (! q) {
		/* no trailing slash after the host -> no package list */
		return;
	}

	plist = q+1;
	p = strchr (plist, ',');
	while (p) {
		package_name = g_strndup (plist, p - plist);
		packages = g_list_prepend (packages, create_package (package_name));
		g_free (package_name);

		plist = p+1;
		p = strchr (plist, ',');
	}
	/* last one */
	if (*plist) {
		package_name = g_strdup (plist);
		packages = g_list_prepend (packages, create_package (package_name));
		g_free (package_name);
	}

	/* add to categories */
	if (packages) {
		CategoryData *category;

		category = g_new0 (CategoryData, 1);
		category->packages = packages;
		*categories = g_list_prepend (NULL, category);
	}
	return;
}


/* add a new text+progress box to the bottom of the message box.
 * the top box is pulled off and thrown away.
 * current_feedback_text and current_progress_bar are updated to the new ones.
 */
static void
make_new_status (NautilusServiceInstallView *view)
{
	GdkFont *font;

#if 0
	GList *widget_list;
	GtkWidget *temp_container;

	/* remove top widget */
	widget_list = gtk_container_children (GTK_CONTAINER (view->details->message_box));
	gtk_container_remove (GTK_CONTAINER (view->details->message_box), widget_list->data);
	g_list_free (widget_list);

	temp_container = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->message_box), temp_container, 0, 0, 4);
	gtk_widget_show (temp_container);

	/* add a label for progress messages, but don't show it until there's a message */
	view->details->current_feedback_text = gtk_label_new ("");
	font = nautilus_font_factory_get_font_from_preferences (14);
	nautilus_gtk_widget_set_font (view->details->current_feedback_text, font);
	gtk_misc_set_alignment (GTK_MISC (view->details->current_feedback_text), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (temp_container), view->details->current_feedback_text, TRUE, TRUE, 8);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->current_feedback_text), FALSE);
	gtk_label_set_justify (GTK_LABEL (view->details->current_feedback_text), GTK_JUSTIFY_LEFT);
	gdk_font_unref (font);
	gtk_widget_show (view->details->current_feedback_text);

	/* Create a center alignment object */
	view->details->current_progress_alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_box_pack_end (GTK_BOX (temp_container), view->details->current_progress_alignment, FALSE, FALSE, 8);
	gtk_widget_show (view->details->current_progress_alignment);
	view->details->current_progress_bar = gtk_progress_bar_new ();
	gtk_container_add (GTK_CONTAINER (view->details->current_progress_alignment), view->details->current_progress_bar);
	gtk_widget_show (view->details->current_progress_bar);
#else
	GtkWidget *left, *right;
	int i;

	/* remove top row of the table */
	left = (GtkWidget *) g_list_nth_data (view->details->message_left, 0);
	right = (GtkWidget *) g_list_nth_data (view->details->message_right, 0);
	view->details->message_left = g_list_remove (view->details->message_left, left);
	view->details->message_right = g_list_remove (view->details->message_right, right);
	gtk_container_remove (GTK_CONTAINER (view->details->message_box), left);
	gtk_container_remove (GTK_CONTAINER (view->details->message_box), right);

	/* shift all contents of the table up one row (painnnnn) */
	for (i = 1; i < STATUS_ROWS; i++) {
		left = (GtkWidget *) g_list_nth_data (view->details->message_left, i-1);
		right = (GtkWidget *) g_list_nth_data (view->details->message_right, i-1);
		/* need to ref the widgets before removing them, or they'll wisp away into the wind... */
		gtk_widget_ref (left);
		gtk_widget_ref (right);
  
		gtk_container_remove (GTK_CONTAINER (view->details->message_box), left);
		gtk_container_remove (GTK_CONTAINER (view->details->message_box), right);
		gtk_table_attach (GTK_TABLE (view->details->message_box), left, 0, 1, i-1, i,
				  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 12, 4);
		gtk_table_attach (GTK_TABLE (view->details->message_box), right, 1, 2, i-1, i,
				  GTK_EXPAND, 0, 12, 4);
	}

	/* new entries */
	left = gtk_label_new ("");
	font = nautilus_font_factory_get_font_from_preferences (14);
	nautilus_gtk_widget_set_font (left, font);
	gtk_misc_set_alignment (GTK_MISC (left), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (view->details->message_box), left, 0, 1, STATUS_ROWS-1, STATUS_ROWS,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 12, 4);
	gtk_label_set_line_wrap (GTK_LABEL (left), FALSE);
	gtk_label_set_justify (GTK_LABEL (left), GTK_JUSTIFY_LEFT);
	gdk_font_unref (font);
	gtk_widget_show (left);
	view->details->message_left = g_list_append (view->details->message_left, left);

	right = gtk_progress_bar_new ();
	gtk_table_attach (GTK_TABLE (view->details->message_box), right, 1, 2, STATUS_ROWS-1, STATUS_ROWS,
			  GTK_EXPAND, 0, 12, 4);
	gtk_widget_show (right);
	view->details->message_right = g_list_append (view->details->message_right, right);

	view->details->current_feedback_text = left;
	view->details->current_progress_bar = right;
#endif
}

static void
spin_cylon (NautilusServiceInstallView *view)
{
	gfloat val;

	val = gtk_progress_get_value (GTK_PROGRESS (view->details->total_progress_bar));
	val += 1.0;
	if (val > 100.0) {
		val = 0.0;
	}
	gtk_progress_set_value (GTK_PROGRESS (view->details->total_progress_bar), val);
}

/* replace the current progress bar (in the message box) with a centered label saying "Complete!" */
static void
current_progress_bar_complete (NautilusServiceInstallView *view)
{
	GtkWidget *right;
	GdkFont *font;
	int width, height;

	right = (GtkWidget *) g_list_nth_data (view->details->message_right, STATUS_ROWS-1);
	view->details->message_right = g_list_remove (view->details->message_right, right);
	/* FIXME -- this is an awful way to do it!  isn't there a better way? */
	width = right->allocation.width;
	height = right->allocation.height;
	gtk_container_remove (GTK_CONTAINER (view->details->message_box), right);

	right = gtk_label_new ("Complete!");
	view->details->message_right = g_list_append (view->details->message_right, right);
	font = nautilus_font_factory_get_font_from_preferences (14);
	nautilus_gtk_widget_set_font (right, font);
	gtk_label_set_justify (GTK_LABEL (right), GTK_JUSTIFY_CENTER);
	gtk_table_attach (GTK_TABLE (view->details->message_box), right, 1, 2, STATUS_ROWS-1, STATUS_ROWS,
			  GTK_EXPAND | GTK_FILL, 0, 12, 4);
	gtk_widget_set_usize (right, width, height);
	gtk_widget_show (right);
	gdk_font_unref (font);
}


static void
nautilus_service_install_downloading (EazelInstallCallback *cb, const char *name, int amount, int total,
				      NautilusServiceInstallView *view)
{
	char *out;
	const char *root_name, *tmp;

	/* the "name" is annoyingly the entire path */
	root_name = name;
	while ((tmp = strchr (root_name, '/')) != NULL) {
		root_name = tmp+1;
	}

	usleep (25000);

	if (total < 0) {
		/* weird bug */
		return;
	}

	if (amount == 0) {
		/* could be a redundant zero-trigger for the same rpm... */
		if (view->details->current_rpm && (strcmp (view->details->current_rpm, root_name) == 0)) {
			spin_cylon (view);
			return;
		}

		g_free (view->details->current_rpm);
		view->details->current_rpm = g_strdup (root_name);

		/* just spin in a cylon until we finish downloading. */
		/* this will be annoying to the user since downloading is 99% of the time spent over a modem,
		 * but we don't know how much work there is to do yet...
		 */
		gtk_progress_set_activity_mode (GTK_PROGRESS (view->details->total_progress_bar), TRUE);
		out = g_strdup_printf ("Downloading package %s", root_name);
		show_overall_feedback (view, out);
		g_free (out);

		/* new progress message and bar */
		make_new_status (view);
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), 0.0);
		out = g_strdup_printf ("Downloading package %s ...", root_name);
		gtk_label_set_text (GTK_LABEL (view->details->current_feedback_text), out);
		g_free (out);
	} else if (amount == total) {
		/* done!  turn progress bar into a label */
#if 0
		GdkFont *font;

		gtk_container_remove (GTK_CONTAINER (view->details->current_progress_alignment),
				      view->details->current_progress_bar);
		font = nautilus_font_factory_get_font_from_preferences (14);
		view->details->current_progress_bar = gtk_label_new ("Complete!");
		nautilus_gtk_widget_set_font (view->details->current_progress_bar, font);
		gtk_label_set_justify (GTK_LABEL (view->details->current_progress_bar), GTK_JUSTIFY_LEFT);
		gtk_container_add (GTK_CONTAINER (view->details->current_progress_alignment),
				   view->details->current_progress_bar);
		gtk_widget_show (view->details->current_progress_bar);
		gdk_font_unref (font);
#else
		current_progress_bar_complete (view);
#endif

		g_free (view->details->current_rpm);
		view->details->current_rpm = NULL;
	} else {
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar),
					     (float) amount / (float) total);

		/* spin the cylon some more, whee! */
		spin_cylon (view);
	}
}

static void
nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
					   const PackageData *needs, NautilusServiceInstallView *view)
{
	char *out;

	out = g_strdup_printf ("Dependency check: Package %s needs %s", package->name, needs->name);
	show_overall_feedback (view, out);
	g_free (out);
}


static void 
nautilus_service_install_preflight_check (EazelInstallCallback *cb, int total_bytes, int total_packages,
					  NautilusServiceInstallView *view)
{
	char *out;

	out = g_strdup_printf ("Preparing to install %d package%s", total_packages,
			       total_packages == 1 ? "" : "s");
	show_overall_feedback (view, out);
	g_free (out);

	view->details->current_package = 0;

	/* turn off the cylon and show "real" progress */
	gtk_progress_set_activity_mode (GTK_PROGRESS (view->details->total_progress_bar), FALSE);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), 0.0);
}


static void
nautilus_service_install_download_failed (EazelInstallCallback *cb, const char *name,
					  NautilusServiceInstallView *view)
{
	char *out;

	out = g_strdup_printf ("Download of package %s failed!", name);
	show_overall_feedback (view, out);
	g_free (out);
}

static void
nautilus_service_install_installing (EazelInstallCallback *cb, const PackageData *pack,
				     int current_package, int total_packages,
				     int package_progress, int package_total,
				     int total_progress, int total_total,
				     NautilusServiceInstallView *view)
{
	gfloat overall_complete, complete;
	char *out;

	usleep (25000);
	if (current_package != view->details->current_package) {
		/* starting a new package -- create new progress indicator */
		out = g_strdup_printf ("Installing package %d of %d", current_package, total_packages);
		show_overall_feedback (view, out);
		g_free (out);

		make_new_status (view);
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), 0.0);
		out = g_strdup_printf ("Installing package %s ...", pack->name);
		gtk_label_set_text (GTK_LABEL (view->details->current_feedback_text), out);
		g_free (out);

		view->details->current_package = current_package;
	}

	complete = (gfloat) package_progress / package_total;
	overall_complete = (gfloat) total_progress / total_total;
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), complete);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), overall_complete);

	if ((package_progress == package_total) && (package_total > 0)) {
		current_progress_bar_complete (view);
	}
}

static void
nautilus_service_install_done (EazelInstallCallback *cb, NautilusServiceInstallView *view)
{
	show_overall_feedback (view, "Installation complete!");
}




static char *
get_password_dude (TrilobiteRootClient *root_client, const char *prompt, void *user_data)
{
	return g_strdup ("ezinepo");
}


static TrilobiteRootClient *
set_root_client (BonoboObjectClient *service)
{
	TrilobiteRootClient *root_client = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/PasswordQuery:1.0", &ev)) {
		root_client = trilobite_root_client_new ();
		gtk_signal_connect (GTK_OBJECT (root_client), "need_password", GTK_SIGNAL_FUNC (get_password_dude),
				    NULL);

		if (! trilobite_root_client_attach (root_client, service)) {
			g_warning ("unable to attach root client to Trilobite/PasswordQuery!");
		}
	} else {
		g_warning ("Object does not support IDL:Trilobite/PasswordQuery:1.0");
	}

	CORBA_exception_free (&ev);
	return root_client;
}


static void
nautilus_service_install_view_update_from_uri (NautilusServiceInstallView	*view, const char	*uri)
{

	/* open the package */
	PackageData		*pack;

	char			*temp_version = NULL;
	char			*temp_release = NULL;
	char			*package_name = NULL;

	GList			*categories;
	char			*host;
	int			port;
	EazelInstallCallback	*cb;
	Trilobite_Eazel_Install	service;
	CORBA_Environment	ev;

	nautilus_install_parse_uri (uri, &host, &port, &categories);

	if (categories) {
		CORBA_exception_init (&ev);
		cb = eazel_install_callback_new ();
		set_root_client (eazel_install_callback_bonobo (cb));
		service = eazel_install_callback_corba_objref (cb);
		Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_HTTP, &ev);
		Trilobite_Eazel_Install__set_tmp_dir (service, "/tmp/eazel-install", &ev);
		Trilobite_Eazel_Install__set_server (service, host, &ev);
		Trilobite_Eazel_Install__set_server_port (service, port, &ev);

		gtk_signal_connect (GTK_OBJECT (cb), "download_progress", nautilus_service_install_downloading, view);
		gtk_signal_connect (GTK_OBJECT (cb), "download_failed", nautilus_service_install_download_failed, view);
		gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", nautilus_service_install_dependency_check, view);
		gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", nautilus_service_install_preflight_check, view);
		gtk_signal_connect (GTK_OBJECT (cb), "install_progress", nautilus_service_install_installing, view);
		gtk_signal_connect (GTK_OBJECT (cb), "done", nautilus_service_install_done, view);
		eazel_install_callback_install_packages (cb, categories, &ev);

		CORBA_exception_free (&ev);
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

	gtk_object_set_data (GTK_OBJECT (view), "categories", categories);
	gtk_object_set_data (GTK_OBJECT (view), "host", host);
	gtk_object_set_data (GTK_OBJECT (view), "port", (void *)port);

#if 0
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
#endif

}

void
nautilus_service_install_view_load_uri (NautilusServiceInstallView	*view,
			     	        const char			*uri)
{

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
			       		NautilusServiceInstallView	*view)
{

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_service_install_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);

#if 0
	go_to_uri (nautilus_view, NEXT_SERVICE_VIEW);
#endif

}

