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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Robey Pointer <robey@eazel.com>
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
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

/* for uname */
#include <sys/utsname.h>

/* number of rows of (label, progressbar) to scroll at the bottom */
#define STATUS_ROWS		4

#define NEXT_SERVICE_VIEW				"eazel-summary:"

/* this stuff will need to be configurable, once we have a config pane */
#define INSTALL_HOST	      	"ham.eazel.com"
#define INSTALL_PORT		8888

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

	/* Setup the title */
	title = create_services_title_widget ("Easy Install");
        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* Add package information */

	/* Package Name */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_name = gtk_label_new (" ");
	gtk_label_set_justify (GTK_LABEL (view->details->package_name), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (view->details->package_name), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_name, FALSE, FALSE, 15);
	font = nautilus_font_factory_get_font_from_preferences (20);
	nautilus_gtk_widget_set_font (view->details->package_name, font);
	gtk_widget_show (view->details->package_name);
	gdk_font_unref (font);

	/* Package Description */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_details = gtk_label_new (" ");
	gtk_misc_set_alignment (GTK_MISC (view->details->package_details), 0.0, 0.0);
	gtk_label_set_justify (GTK_LABEL (view->details->package_details), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->package_details), TRUE);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_details, FALSE, FALSE, 15);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->package_details, font);
	gtk_widget_show (view->details->package_details);
	gdk_font_unref (font);

	/* Package Version */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_version = gtk_label_new (" ");
	gtk_misc_set_alignment (GTK_MISC (view->details->package_version), 0.0, 0.0);
	gtk_label_set_justify (GTK_LABEL (view->details->package_version), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_version, FALSE, FALSE, 15);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (view->details->package_version, font);
	gtk_widget_show (view->details->package_version);
	gdk_font_unref (font);

	/* generate the overall progress bar */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->total_progress_bar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->total_progress_bar, FALSE, FALSE, 30);
	gtk_widget_show (view->details->total_progress_bar);

	/* add a label for progress messages, but don't show it until there's a message */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->overall_feedback_text = gtk_label_new (" ");
	font = nautilus_font_factory_get_font_from_preferences (10);
	nautilus_gtk_widget_set_font (view->details->overall_feedback_text, font);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->overall_feedback_text, FALSE, FALSE, 30);
	gdk_font_unref (font);

	/* filler blob to separate the top from the bottom */
	gtk_box_pack_start (GTK_BOX (view->details->form), gtk_label_new (""), TRUE, FALSE, 0);

	/* add a table at the bottom of the screen to hold current progresses */
	view->details->message_box = gtk_table_new (STATUS_ROWS, 2, FALSE);
	gtk_box_pack_end (GTK_BOX (view->details->form), view->details->message_box, FALSE, FALSE, 4);
	gtk_widget_show (view->details->message_box);

	/* prepopulate left and right columns with spaces */
	font = nautilus_font_factory_get_font_from_preferences (12);
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
	}

	/* Setup the progress header */
	middle_title = create_services_header_widget ("Messages", "Progress");
        gtk_box_pack_end (GTK_BOX (view->details->form), middle_title, FALSE, FALSE, 0);
	gtk_widget_show (middle_title);

	gtk_widget_show (view->details->form);
}



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

	g_free (view->details->uri);
	g_list_free (view->details->message_left);
	g_list_free (view->details->message_right);
	g_free (view->details->current_rpm);
	g_free (view->details->remembered_password);

	if (view->details->root_client) {
		trilobite_root_client_unref (GTK_OBJECT (view->details->root_client));
	}

	eazel_install_callback_unref (GTK_OBJECT (view->details->installer));

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
nautilus_install_parse_uri (const char *uri, GList **categories)
{
	char *p, *plist, *package_name;
	GList *packages = NULL;

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

	plist = p;

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
	GtkWidget *left, *right;
	int i;

	gtk_widget_hide (view->details->message_box);

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
	font = nautilus_font_factory_get_font_from_preferences (12);
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

	gtk_widget_show (view->details->message_box);
}


/* when the overall progress bar is in cylon mode, this will tap it to keep it spinning. */
static gint
spin_cylon (NautilusServiceInstallView *view)
{
	gfloat val;

	val = gtk_progress_get_value (GTK_PROGRESS (view->details->total_progress_bar));
	val += 1.0;
	if (val > 100.0) {
		val = 0.0;
	}
	gtk_progress_set_value (GTK_PROGRESS (view->details->total_progress_bar), val);

	/* in case we're being called as a timer */
	return TRUE;
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
	font = nautilus_font_factory_get_font_from_preferences (12);
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
	/* FIXME: this will change. */
	root_name = name;
	while ((tmp = strchr (root_name, '/')) != NULL) {
		root_name = tmp+1;
	}

	if (amount == 0) {
		/* could be a redundant zero-trigger for the same rpm... */
		if (view->details->current_rpm && (strcmp (view->details->current_rpm, root_name) == 0)) {
			spin_cylon (view);
			return;
		}

		if (view->details->cylon_timer) {
			gtk_timeout_remove (view->details->cylon_timer);
			view->details->cylon_timer = 0;
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
		current_progress_bar_complete (view);
		g_free (view->details->current_rpm);
		view->details->current_rpm = NULL;
	} else {
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar),
					     (float) amount / (float) total);

		/* spin the cylon some more, whee! */
		spin_cylon (view);
	}
}


/* is this really necessary?  in most cases it will probably just flash past at the speed of light. */
static void
nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
					   const PackageData *needs, NautilusServiceInstallView *view)
{
	char *out;

	out = g_strdup_printf ("Dependency check: Package %s needs %s", package->name, needs->name);
	show_overall_feedback (view, out);
	g_free (out);
}


/* comment from above applies here too. */
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

	if (view->details->cylon_timer) {
		gtk_timeout_remove (view->details->cylon_timer);
		view->details->cylon_timer = 0;
	}

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

		if (current_package == 1) {
			/* first package is the main one.  update top info, now that we know it */
			out = g_strdup_printf ("Installing \"%s\"", pack->name);
			gtk_label_set_text (GTK_LABEL (view->details->package_name), out);
			g_free (out);
			if (strchr (pack->description, '\n') != NULL) {
				gtk_label_set_line_wrap (GTK_LABEL (view->details->package_details), FALSE);
			}
			gtk_label_set_text (GTK_LABEL (view->details->package_details), pack->description);
			out = g_strdup_printf ("Version: %s", pack->version);
			gtk_label_set_text (GTK_LABEL (view->details->package_version), out);
			g_free (out);
		}
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
	if (view->details->cylon_timer) {
		gtk_timeout_remove (view->details->cylon_timer);
		view->details->cylon_timer = 0;
	}

	show_overall_feedback (view, "Installation complete!");
}

/* FIXME -- need to show whole dep tree here */
static void
nautilus_service_install_failed (EazelInstallCallback *cb, const PackageData *pack, NautilusServiceInstallView *view)
{
	if (view->details->cylon_timer) {
		gtk_timeout_remove (view->details->cylon_timer);
		view->details->cylon_timer = 0;
	}

	show_overall_feedback (view, "Installation failed!! :( :( :(");
	g_message ("I am sad.");
}


/* signal callback -- ask the user for the root password (for installs) */
static char *
nautilus_service_need_password (GtkObject *object, const char *prompt, NautilusServiceInstallView *view)
{
	char *message = NULL;
	GtkWidget *dialog;
	gboolean okay;
	char *out;

	if (view->details->remembered_password) {
		return g_strdup (view->details->remembered_password);
	}

	if (view->details->password_attempts > 0) {
		message = "Incorrect password.";
	}

	dialog = nautilus_password_dialog_new ("Authenticate Me", message, prompt, "", TRUE);
	okay = nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (dialog));

	if (! okay) {
		/* cancel */
		view->details->password_attempts = 0;
		out = g_strdup ("");
	} else {
		out = nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (dialog));
		if (nautilus_password_dialog_get_remember (NAUTILUS_PASSWORD_DIALOG (dialog))) {
			view->details->remembered_password = g_strdup (out);
		}
	}

	gtk_widget_destroy (dialog);
	gtk_main_iteration ();

	if (okay) {
		view->details->password_attempts++;
	}

	return out;
}

static TrilobiteRootClient *
set_root_client (BonoboObjectClient *service, NautilusServiceInstallView *view)
{
	TrilobiteRootClient *root_client = NULL;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/PasswordQuery:1.0", &ev)) {
		root_client = trilobite_root_client_new ();
		if (! trilobite_root_client_attach (root_client, service)) {
			g_warning ("unable to attach root client to Trilobite/PasswordQuery!");
		}

		gtk_signal_connect (GTK_OBJECT (root_client), "need_password",
				    GTK_SIGNAL_FUNC (nautilus_service_need_password),
				    view);
	} else {
		g_warning ("Object does not support IDL:Trilobite/PasswordQuery:1.0");
	}

	CORBA_exception_free (&ev);
	return root_client;
}


static void
nautilus_service_install_view_update_from_uri (NautilusServiceInstallView *view, const char *uri)
{
	PackageData		*pack;
	CategoryData		*category_data;
	GList			*categories;
	char			*host;
	int			port;
	Trilobite_Eazel_Install	service;
	CORBA_Environment	ev;
	char 			*out;

	nautilus_install_parse_uri (uri, &categories);
	host = INSTALL_HOST;
	port = INSTALL_PORT;

	if (! categories) {
		return;
	}

	/* NOTE: This adds a libeazelinstall packagedata object to the view */
	pack = (PackageData*) gtk_object_get_data (GTK_OBJECT (view), "packagedata");
	if (pack != NULL) {
		/* Destroy the old */
		packagedata_destroy (pack);
	}

	/* find the package data for the package we're about to install */
	category_data = (CategoryData *) categories->data;
	pack = (PackageData *) category_data->packages->data;

	gtk_object_set_data (GTK_OBJECT (view), "packagedata", pack);

	out = g_strdup_printf ("Downloading \"%s\"", pack->name);
	gtk_label_set_text (GTK_LABEL (view->details->package_name), out);
	g_free (out);

	CORBA_exception_init (&ev);
	view->details->installer = eazel_install_callback_new ();
	view->details->root_client = set_root_client (eazel_install_callback_bonobo (view->details->installer), view);
	service = eazel_install_callback_corba_objref (view->details->installer);
	Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	Trilobite_Eazel_Install__set_tmp_dir (service, "/tmp/eazel-install", &ev);
	Trilobite_Eazel_Install__set_server (service, host, &ev);
	Trilobite_Eazel_Install__set_server_port (service, port, &ev);

	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_progress",
			    nautilus_service_install_downloading, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_failed",
			    nautilus_service_install_download_failed, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "dependency_check",
			    nautilus_service_install_dependency_check, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "preflight_check",
			    nautilus_service_install_preflight_check, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_progress",
			    nautilus_service_install_installing, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_failed",
			    nautilus_service_install_failed, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "done",
			    nautilus_service_install_done, view);
	eazel_install_callback_install_packages (view->details->installer, categories, &ev);

	CORBA_exception_free (&ev);

	show_overall_feedback (view, "Contacting install server ...");

	/* might take a while... cylon a bit */
	gtk_progress_set_activity_mode (GTK_PROGRESS (view->details->total_progress_bar), TRUE);
	view->details->cylon_timer = gtk_timeout_add (100, (GtkFunction)spin_cylon, view);
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

