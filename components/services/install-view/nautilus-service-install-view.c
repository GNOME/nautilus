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
#include "shared-service-widgets.h"
#include "shared-service-utilities.h"
#include "eazel-services-header.h"
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
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
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

#define NEXT_SERVICE_VIEW				"eazel:"

/* This ensures that if the arch is detected as i[3-9]86, the
   requested archtype will be set to i386 */
#define ASSUME_ix86_IS_i386 


static void       nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass	*klass);
static void       nautilus_service_install_view_initialize       (NautilusServiceInstallView		*view);
static void       nautilus_service_install_view_destroy          (GtkObject				*object);
static void       service_install_load_location_callback         (NautilusView				*nautilus_view,
								  const char				*location,
								  NautilusServiceInstallView		*view);
static void	  service_install_stop_loading_callback		 (NautilusView				*nautilus_view,
								  NautilusServiceInstallView		*view);
static void       generate_install_form                          (NautilusServiceInstallView		*view);
static void       nautilus_service_install_view_update_from_uri  (NautilusServiceInstallView		*view,
								  const char				*uri);
static void       show_overall_feedback                          (NautilusServiceInstallView		*view,
								  char					*progress_message);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceInstallView, nautilus_service_install_view, GTK_TYPE_EVENT_BOX)


/* gtk rulez */
static void
add_padding_to_box (GtkWidget *box, int pad_x, int pad_y)
{
	GtkWidget *filler;

	filler = gtk_label_new ("");
	gtk_widget_set_usize (filler, pad_x ? pad_x : 1, pad_y ? pad_y : 1);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (box), filler, FALSE, FALSE, 0);
}

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
	title = eazel_services_header_title_new (_("Easy Install"));
        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* Add package information */

	add_padding_to_box (view->details->form, 0, 6);

	/* Package Name */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 0);
	gtk_widget_show (temp_box);
	view->details->package_name = nautilus_label_new (" ");
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->package_name), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (view->details->package_name), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_name, FALSE, FALSE, 15);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->package_name), "helvetica", "bold",
						 NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->package_name), 18);
	gtk_widget_show (view->details->package_name);

	/* Package Version */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_version = nautilus_label_new (" ");
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->package_version), GTK_JUSTIFY_LEFT);
	nautilus_label_set_font_from_components (NAUTILUS_LABEL (view->details->package_version), "helvetica", "bold",
						 NULL, NULL);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->package_version), 12);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_version, FALSE, FALSE, 15);
	gtk_widget_show (view->details->package_version);

	add_padding_to_box (view->details->form, 0, 4);

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
	view->details->overall_feedback_text = nautilus_label_new (" ");
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->overall_feedback_text), GTK_JUSTIFY_LEFT);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->overall_feedback_text), 11);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->overall_feedback_text, FALSE, FALSE, 30);

	add_padding_to_box (view->details->form, 0, 10);

	/* Package Description */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_details = nautilus_label_new (" ");
	nautilus_label_set_text_justification (NAUTILUS_LABEL (view->details->package_details), GTK_JUSTIFY_LEFT);
	nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->package_details), TRUE);
	nautilus_label_set_font_size (NAUTILUS_LABEL (view->details->package_details), 12);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_details, FALSE, FALSE, 15);
	gtk_widget_show (view->details->package_details);

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
	middle_title = eazel_services_header_middle_new (_("Messages"), _("Progress"));
        gtk_box_pack_end (GTK_BOX (view->details->form), middle_title, FALSE, FALSE, 0);
	gtk_widget_show (middle_title);

	gtk_widget_show (view->details->form);
}



/* utility routine to show an error message */
static void
show_overall_feedback (NautilusServiceInstallView	*view, char	*progress_message)
{
	nautilus_label_set_text (NAUTILUS_LABEL (view->details->overall_feedback_text), progress_message);
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
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view),
			    "stop_loading",
			    GTK_SIGNAL_FUNC (service_install_stop_loading_callback),
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	/* Crude fix for 
	   FIXME bugzilla.eazel.com 3431
	*/
	view->details->core_package = FALSE;

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_service_install_view_destroy (GtkObject *object)
{
	NautilusServiceInstallView *view;
	Trilobite_Eazel_Install	service;
	CORBA_Environment ev;

	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	CORBA_exception_init (&ev);
	service = eazel_install_callback_corba_objref (view->details->installer);
	Trilobite_Eazel_Install_stop (service, &ev);
	CORBA_exception_free (&ev);

	g_free (view->details->uri);
	g_list_free (view->details->message_left);
	g_list_free (view->details->message_right);
	g_free (view->details->current_rpm);
	g_free (view->details->remembered_password);

	if (view->details->root_client) {
		trilobite_root_client_unref (GTK_OBJECT (view->details->root_client));
	}

	if (view->details->installer != NULL) {
		/* this will unref the installer too, which will indirectly cause any ongoing download to abort */
		eazel_install_callback_unref (GTK_OBJECT (view->details->installer));
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
create_package (char *name, int local_file) 
{
	struct utsname buf;
	PackageData *pack;

	g_assert (name);

	uname (&buf);
	pack = packagedata_new ();
	if (local_file) {
		pack->filename = g_strdup (name);
	} else if (strncmp (name, "rpm_id%3D", 9) == 0) {
		pack->eazel_id = g_strdup (name+9);
	} else if (strncmp (name, "rpm_id=", 7) == 0) {
		pack->eazel_id = g_strdup (name+7);
	} else {
		pack->name = g_strdup (name);
	}
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
/* format:
 * "eazel-install:" [ "//" [ username "@" ] [ "hostname" [ ":" port ] ] "/" ] package-name [ "?version=" version ]
 */
static void
nautilus_install_parse_uri (const char *uri, NautilusServiceInstallView *view, GList **categories,
			    char **host, int *port, char **username)
{
	char *p, *q, *package_name, *host_spec;
	GList *packages = NULL;
	PackageData *pack;

	*categories = NULL;

	p = strchr (uri, ':');
	if (! p) {
		/* bad mojo */
		return;
	}
	p++;

	/* "//[user@]host[:port]" spec? */
	if ((*p == '/') && (*(p+1) == '/')) {
		p += 2;

		q = strchr (p, '/');
		if (! q) {
			q = p + strlen(p);
		}
		host_spec = g_strndup (p, q - p);

		/* optional "user@" */
		p = strchr (host_spec, '@');
		if (p) {
			*p = 0;
			*username = host_spec;
			if (*(p+1)) {
				g_free (*host);
				*host = g_strdup (p+1);
			}
		} else {
			g_free (*host);
			*host = host_spec;
		}

		if (*host) {
			/* optional ":port" */
			p = strchr (*host, ':');
			if (p) {
				*p = 0;
				*port = atoi (p+1);
			}
		}

		/* push p to past the trailing '/' */
		p = (*q) ? q+1 : q;
	}

	/* full path specified?  local file instead of server */
	if (*p == '/') {
		view->details->using_local_file = 1;
	}

	if (*p) {
		/* version name specified? */
		q = strchr (p, '?');
		if (q) {
			*q++ = 0;
			if (strncmp (q, "version=", 8) == 0) {
				q += 8;
			}
		}

		package_name = g_strdup (p);
		pack = create_package (package_name, view->details->using_local_file);
		if (q) {
			pack->version = g_strdup (q);
		}
		packages = g_list_prepend (packages, pack);
		g_free (package_name);
	}

	g_message ("host '%s:%d' username '%s'", *host ? *host : "(default)", *host ? *port : 0,
		   *username ? *username : "(default)");

	/* add to categories */
	if (packages) {
		CategoryData *category;

		category = categorydata_new ();
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
	left = nautilus_label_new ("");
	nautilus_label_set_font_size (NAUTILUS_LABEL (left), 12);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (left), GTK_JUSTIFY_LEFT);
	gtk_table_attach (GTK_TABLE (view->details->message_box), left, 0, 1, STATUS_ROWS-1, STATUS_ROWS,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 12, 4);
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

static void
turn_cylon_off (NautilusServiceInstallView *view, float progress)
{
	gtk_progress_set_activity_mode (GTK_PROGRESS (view->details->total_progress_bar), FALSE);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), progress);
	if (view->details->cylon_timer) {
		gtk_timeout_remove (view->details->cylon_timer);
		view->details->cylon_timer = 0;
	}
}

/* replace the current progress bar (in the message box) with a centered label saying "Complete!" */
static void
current_progress_bar_complete (NautilusServiceInstallView *view, const char *text)
{
	GtkWidget *right;
	int width, height;

	right = (GtkWidget *) g_list_nth_data (view->details->message_right, STATUS_ROWS-1);
	view->details->message_right = g_list_remove (view->details->message_right, right);
	/* apparently there is no better way to do this :( */
	width = right->allocation.width;
	height = right->allocation.height;
	gtk_container_remove (GTK_CONTAINER (view->details->message_box), right);
	view->details->current_progress_bar = NULL;

	right = nautilus_label_new (text);
	view->details->message_right = g_list_append (view->details->message_right, right);
	nautilus_label_set_font_size (NAUTILUS_LABEL (right), 12);
	nautilus_label_set_text_justification (NAUTILUS_LABEL (right), GTK_JUSTIFY_CENTER);
	gtk_table_attach (GTK_TABLE (view->details->message_box), right, 1, 2, STATUS_ROWS-1, STATUS_ROWS,
			  GTK_EXPAND, 0, 12, 4);
	gtk_widget_set_usize (right, width, height);
	gtk_widget_show (right);
}


static void
nautilus_service_install_downloading (EazelInstallCallback *cb, const char *name, int amount, int total,
				      NautilusServiceInstallView *view)
{
	char *out;
	const char *root_name, *tmp;

	if (view->details->installer == NULL) {
		g_warning ("Got download notice after unref!");
		return;
	}

	/* sometimes the "name" is annoyingly the entire path */
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
		out = g_strdup_printf (_("Downloading package %s"), root_name);
		show_overall_feedback (view, out);
		g_free (out);

		/* new progress message and bar */
		make_new_status (view);
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), 0.0);
		out = g_strdup_printf (_("Downloading package %s ..."), root_name);
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_feedback_text), out);
		g_free (out);
	} else if (amount == total) {
		/* done!  turn progress bar into a label */
		current_progress_bar_complete (view, _("Complete!"));
		g_free (view->details->current_rpm);
		view->details->current_rpm = NULL;
	} else {
		/* could be a leftover event, after user hit STOP (in which case, current_progress_bar = NULL) */
		if (view->details->current_progress_bar != NULL) {
			gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar),
						     (float) amount / (float) total);
			/* spin the cylon some more, whee! */
			spin_cylon (view);
		}
	}
}



/* is this really necessary?  in most cases it will probably just flash past at the speed of light. */
static void
nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
					   const PackageData *needs, NautilusServiceInstallView *view)
{
	char *out;
	char *required = packagedata_get_readable_name (needs);

	out = g_strdup_printf (_("Dependency check: Package %s needs %s"), package->name, required);
	show_overall_feedback (view, out);
	g_free (out);
	g_free (required);
}


static void
nautilus_service_install_check_for_desktop_files (NautilusServiceInstallView *view,
						  EazelInstallCallback *cb,
						  PackageData *package)
{
	GList *iterator;

	for (iterator = package->provides; iterator; iterator = g_list_next (iterator)) {
		char *fname = (char*)(iterator->data);
		char *ptr;

		ptr = strrchr (fname, '.');
		if (ptr && ((strcmp (ptr, ".desktop") == 0) ||
			    (strcmp (ptr, ".kdelink") == 0))) {
			view->details->desktop_files = g_list_prepend (view->details->desktop_files,
								       g_strdup (fname));
		}
	}
}

/* do what gnome ought to do automatically */
static void
reply_callback (int reply, gboolean *answer)
{
	*answer = (reply == 0);
}

static gboolean
nautilus_service_install_preflight_check (EazelInstallCallback *cb, const GList *packages,
					  int total_bytes, int total_packages,
					  NautilusServiceInstallView *view)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GString *message;
	gboolean answer;
	PackageData *package;
	GList *iter;
	GList *package_list;
	char *out;

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);
	/* turn off the cylon and show "real" progress */
	turn_cylon_off (view, 0.0);

	toplevel = gtk_widget_get_toplevel (view->details->message_box);

	/* assemble initial list of packages to browse */
	package_list = NULL;
	for (iter = g_list_first ((GList *)packages); iter; iter = g_list_next (iter)) {
		package_list = g_list_append (package_list, iter->data);
	}

	message = g_string_new ("");
	message = g_string_append (message, _("I'm about to install the following packages:\n\n"));

	/* treat package_list as a stack -- remove one package, print it, and then prepend any dependent
	 * packages back to the stack for processing.
	 */
	while (package_list) {
		package = (PackageData *) (package_list->data);
		package_list = g_list_remove (package_list, package_list->data);
	        g_string_sprintfa (message, " \xB7 %s v%s\n", package->name, package->version);

		for (iter = g_list_first (package->soft_depends); iter; iter = g_list_next (iter)) {
			package_list = g_list_prepend (package_list, iter->data);
		}
		for (iter = g_list_first (package->hard_depends); iter; iter = g_list_next (iter)) {
			package_list = g_list_prepend (package_list, iter->data);
		}
		if (package->toplevel && package->modify_status == PACKAGE_MOD_INSTALLED) {
			nautilus_service_install_check_for_desktop_files (view,
									  cb,
									  package);
		}
	}	

	message = g_string_append (message, _("\nIs this okay?"));

	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_ok_cancel_dialog_parented (message->str, (GnomeReplyCallback)reply_callback,
							  &answer, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_ok_cancel_dialog (message->str, (GnomeReplyCallback)reply_callback,
						 &answer);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	g_string_free (message, TRUE);

	if (!answer) {
		view->details->cancelled = TRUE;
		return answer;
	}

	if (total_packages == 1) {
		out = g_strdup (_("Preparing to install 1 package"));
	} else {
		out = g_strdup_printf (_("Preparing to install %d packages"), total_packages);
	}
	show_overall_feedback (view, out);
	g_free (out);

	view->details->current_package = 0;
	return answer;
}


static void
nautilus_service_install_download_failed (EazelInstallCallback *cb, const char *name,
					  NautilusServiceInstallView *view)
{
	char *out;

	turn_cylon_off (view, 0.0);

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	out = g_strdup_printf (_("Download of package %s failed!"), name);
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
		out = g_strdup_printf (_("Installing package %d of %d"), current_package, total_packages);
		show_overall_feedback (view, out);
		g_free (out);

		/* Crude fix for 
		   FIXME bugzilla.eazel.com 3431
		*/	
		if (pack->name) {		
			if (strncasecmp (pack->name, "nautilus", 8)==0) {
				view->details->core_package = TRUE;
			} 
		}

		make_new_status (view);
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), 0.0);
		out = g_strdup_printf (_("Installing package %s ..."), pack->name);
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_feedback_text), out);
		g_free (out);

		view->details->current_package = current_package;

		if (current_package == 1) {
			/* first package is the main one.  update top info, now that we know it */
			out = g_strdup_printf (_("Installing \"%s\""), pack->name);
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_name), out);
			g_free (out);
			if (strchr (pack->description, '\n') != NULL) {
				nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->package_details), FALSE);
			} else {
				nautilus_label_set_line_wrap (NAUTILUS_LABEL (view->details->package_details), TRUE);
			}
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_details), pack->description);
			out = g_strdup_printf (_("Version: %s"), pack->version);
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_version), out);
			g_free (out);
		}
	}

	complete = (gfloat) package_progress / package_total;
	overall_complete = (gfloat) total_progress / total_total;
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->current_progress_bar), complete);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), overall_complete);

	if ((package_progress == package_total) && (package_total > 0)) {
		current_progress_bar_complete (view, _("Complete!"));
	}
}

static void
show_dialog_and_run_away (NautilusServiceInstallView *view, const char *message)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

#if 0
	toplevel = gtk_widget_get_ancestor (view->details->message_box, GTK_TYPE_WINDOW);
#else
	toplevel = gtk_widget_get_toplevel (view->details->message_box);
#endif
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_ok_dialog_parented (message, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_ok_dialog (message);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	/* just in case we were in "loading" mode */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	nautilus_view_open_location (view->details->nautilus_view, NEXT_SERVICE_VIEW);
}

/* Get the toplevel menu name for the desktop file installed */
static char*
nautilus_install_service_locate_menu_entries (NautilusServiceInstallView *view) 
{
	GList *iterator;
	char *result;
	
	result = g_strdup ("");

	for (iterator = view->details->desktop_files; iterator; iterator = g_list_next (iterator)) {
		char *fname = (char*)(iterator->data);
		char *addition;
		char *tmp;
		GnomeDesktopEntry *dentry = gnome_desktop_entry_load (fname);

		g_message ("DEntry for %s (%s app), location %s", 
			   dentry->name, dentry->is_kde ? "KDE" : "Gnome",
			   dentry->location);

		if (dentry->is_kde) {
			addition = g_strdup_printf (_(" \xB7 %s is in the KDE menu.\n"), dentry->name);
		} else {
			char *gnomeapp = "gnome/apps/";
			char *apps_ptr = strstr (fname, gnomeapp);
			if (apps_ptr) {
				char *slash;
				char *menu;
				apps_ptr += strlen (gnomeapp);
				slash = strrchr (apps_ptr, G_DIR_SEPARATOR);
				if (slash) {
					menu = g_strndup (apps_ptr, strlen (apps_ptr) - strlen (slash));
					addition = g_strdup_printf (_(" \xB7 %s is in the Gnome menu under %s.\n"), 
								    dentry->name, menu);
					g_free (menu);
				} else {
					addition = g_strdup_printf (_(" \xB7 %s is in the Gnome menu.\n"), 
								    dentry->name);
				}
			} else {			       
				addition = g_strdup_printf (_(" \xB7 %s in somewhere...\n"), 
							    dentry->name);
			}
		}
		tmp = g_strdup_printf ("%s%s", result, addition);
		g_free (result);
		result = tmp;
		g_free (addition);
	}
	return result;
}

static void
nautilus_service_install_done (EazelInstallCallback *cb, gboolean success, NautilusServiceInstallView *view)
{
	CORBA_Environment ev;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	char *message;
	char *real_message;
	gboolean answer;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	if (view->details->failure) {
		success = FALSE;
		view->details->failure = FALSE;
		/* we have already indicated failure elsewhere.  good day to you, sir. */
		return;
	}

	turn_cylon_off (view, success ? 1.0 : 0.0);

	if (success) {
		/* Crude fix for 
		   FIXME bugzilla.eazel.com 3431
		   should go into nautilus_service_install_done, and only by called 
		   if result==TRUE
		*/
		if (view->details->core_package) {
			message = _("Installation complete!\n"
				    "A core package of Nautilus has been\n"
				    "updated, you should restart Nautilus");
		} else {
			message = _("Installation complete!");
		}
	} else if (view->details->cancelled) {
		message = _("Installation aborted.");
	} else {
		message = _("Installation failed!");
	}

	show_overall_feedback (view, message);

	if (success && view->details->desktop_files) {
		real_message = g_strdup_printf (_("%s\n%s\nErase the leftover RPM files?"), 
						message,
						nautilus_install_service_locate_menu_entries (view));
	} else {
		real_message = g_strdup_printf (_("%s\nErase the leftover RPM files?"), message);
	}
	toplevel = gtk_widget_get_toplevel (view->details->message_box);
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_question_dialog_parented (real_message, (GnomeReplyCallback)reply_callback,
							 &answer, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_question_dialog (real_message, (GnomeReplyCallback)reply_callback, &answer);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	g_free (real_message);

	if (answer) {
		CORBA_exception_init (&ev);
		eazel_install_callback_delete_files (cb, &ev);
		CORBA_exception_free (&ev);
	}

	nautilus_view_open_location (view->details->nautilus_view, NEXT_SERVICE_VIEW);
}


/* recursive descent of a package and its dependencies, building up a GString of errors */
static void
dig_up_errors (NautilusServiceInstallView *view,
	       const PackageData *package, 
	       GString *messages)
{
	GList *iter;
	GList *strings;

	strings = eazel_install_problem_tree_to_string (view->details->problem,
							package);
	
	for (iter = strings; iter; iter = g_list_next (iter)) {
		g_string_sprintfa (messages, " \xB7 %s\n", (char*)(iter->data));
	}
	g_list_foreach (strings, (GFunc)g_free, NULL);
	g_list_free (strings);
}

static void
nautilus_service_install_failed (EazelInstallCallback *cb, const PackageData *pack, NautilusServiceInstallView *view)
{
	CORBA_Environment ev;
	GString *message;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	turn_cylon_off (view, 0.0);
	message = g_string_new ("");

	/* override the "success" result for install_done signal */
	view->details->failure = TRUE;

	if (pack->status == PACKAGE_ALREADY_INSTALLED) {
		message = g_string_append (message, _("This package has already been installed."));
		show_overall_feedback (view, message->str);
		show_dialog_and_run_away (view, message->str);
		g_string_free (message, TRUE);

		/* always delete the RPM files in this case */
		CORBA_exception_init (&ev);
		eazel_install_callback_delete_files (cb, &ev);
		CORBA_exception_free (&ev);

		return;
	}

	g_string_sprintfa (message, _("Installation Failed!\n\n"));
	show_overall_feedback (view, message->str);

	dig_up_errors (view, pack, message);
	show_dialog_and_run_away (view, message->str);
	g_string_free (message, TRUE);

	/* always delete the RPM files in this case */
	CORBA_exception_init (&ev);
	eazel_install_callback_delete_files (cb, &ev);
	CORBA_exception_free (&ev);
}


#if 0
static gboolean
nautilus_service_install_delete_files (EazelInstallCallback *cb, NautilusServiceInstallView *view)
{
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gboolean answer;
	const char *message;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	message = _("Should I delete the leftover RPM files?");

	if (view->details->cancelled ||
	    view->details->failure) {
		/* don't bother to ask on failure -- just clean up */
		return TRUE;
	}

	toplevel = gtk_widget_get_toplevel (view->details->message_box);
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_question_dialog_parented (message, (GnomeReplyCallback)reply_callback,
							 &answer, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_question_dialog (message, (GnomeReplyCallback)reply_callback, &answer);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	return answer;
}
#endif


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
		message = _("Incorrect password.");
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
/*	gtk_main_iteration (); */

	if (okay) {
		view->details->password_attempts++;
	}

	return out;
}

/* bad password -- let em try again? */
static gboolean
nautilus_service_try_again (GtkObject *object, NautilusServiceInstallView *view)
{
	if (view->details->password_attempts == 0) {
		/* user hit "cancel" */
		return FALSE;
	}

	/* a wrong password shouldn't be remembered :) */
	g_free (view->details->remembered_password);
	view->details->remembered_password = NULL;

	if (view->details->password_attempts >= 3) {
		/* give up. */
		view->details->password_attempts = 0;
		return FALSE;
	}
	return TRUE;
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
		gtk_signal_connect (GTK_OBJECT (root_client), "try_again",
				    GTK_SIGNAL_FUNC (nautilus_service_try_again),
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
	char			*username;
	Trilobite_Eazel_Install	service;
	CORBA_Environment	ev;
	char 			*out, *p;

	/* get default host/port */
	host = g_strdup (trilobite_get_services_address ());
	if ((p = strchr (host, ':')) != NULL) {
		*p = 0;
		port = atoi (p+1);
	} else {
		port = 443;
	}
	username = NULL;
	nautilus_install_parse_uri (uri, view, &categories, &host, &port, &username);

	if (! categories) {
		return;
	}

	/* NOTE: This adds a libeazelinstall packagedata object to the view */
	pack = (PackageData*) gtk_object_get_data (GTK_OBJECT (view), "packagedata");
	if (pack != NULL) {
		/* Destroy the old */
		packagedata_destroy (pack, TRUE);
	}

	/* find the package data for the package we're about to install */
	category_data = (CategoryData *) categories->data;
	pack = (PackageData *) category_data->packages->data;

	gtk_object_set_data (GTK_OBJECT (view), "packagedata", pack);

	if (pack->eazel_id != NULL) {
		out = g_strdup_printf (_("Downloading remote package"));
	} else if (pack->name != NULL) {
		out = g_strdup_printf (_("Downloading \"%s\""), pack->name);
	} else {
		out = g_strdup_printf (_("Downloading some package"));
	}
	nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_name), out);
	g_free (out);

	CORBA_exception_init (&ev);
	view->details->installer = eazel_install_callback_new ();
	view->details->problem = eazel_install_problem_new ();
	view->details->root_client = set_root_client (eazel_install_callback_bonobo (view->details->installer), view);
	service = eazel_install_callback_corba_objref (view->details->installer);
	Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	Trilobite_Eazel_Install__set_server (service, host, &ev);
	Trilobite_Eazel_Install__set_server_port (service, port, &ev);
	if (username != NULL) {
		Trilobite_Eazel_Install__set_username (service, username, &ev);
	}
	Trilobite_Eazel_Install__set_test_mode (service, FALSE, &ev);

	/* attempt to create a directory we can use */

	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_progress",
			    GTK_SIGNAL_FUNC (nautilus_service_install_downloading), view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_failed",
			    nautilus_service_install_download_failed, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "dependency_check",
			    nautilus_service_install_dependency_check, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "preflight_check",
			    GTK_SIGNAL_FUNC (nautilus_service_install_preflight_check), view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_progress",
			    nautilus_service_install_installing, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_failed",
			    nautilus_service_install_failed, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "done",
			    nautilus_service_install_done, view);
#if 0
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "delete_files",
			    GTK_SIGNAL_FUNC (nautilus_service_install_delete_files), view);
#endif
	eazel_install_callback_install_packages (view->details->installer, categories, NULL, &ev);

	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Contacting install server ..."));

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
}

static void
service_install_stop_loading_callback (NautilusView *nautilus_view, NautilusServiceInstallView *view)
{
	Trilobite_Eazel_Install	service;
	CORBA_Environment ev;

	g_assert (nautilus_view == view->details->nautilus_view);

	CORBA_exception_init (&ev);
	service = eazel_install_callback_corba_objref (view->details->installer);
	Trilobite_Eazel_Install_stop (service, &ev);
	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Package download aborted."));
	turn_cylon_off (view, 0.0);
	current_progress_bar_complete (view, _("Aborted."));

	view->details->cancelled = TRUE;
}
