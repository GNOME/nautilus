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
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"
#include <libeazelinstall.h>
#include "../lib/eazel-install-metadata.h"
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
#include <libnautilus-extensions/nautilus-label.h>
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
#define STATUS_ROWS		2

#define PROGRESS_BAR_HEIGHT	15
#define MESSAGE_BOX_HEIGHT	110

/* This ensures that if the arch is detected as i[3-9]86, the
   requested archtype will be set to i386 */
#define ASSUME_ix86_IS_i386 

/* send the user here after a completed (success or failure) install */
#define NEXT_URL	"eazel-services:/catalog"


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

static gboolean
line_expose (GtkWidget *widget, GdkEventExpose *event)
{
	gdk_window_clear_area (widget->window, event->area.x, event->area.y, event->area.width, event->area.height);
	gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);
	gdk_draw_line (widget->window, widget->style->fg_gc[widget->state],
		       event->area.x, widget->allocation.height/2,
		       event->area.x + event->area.width, widget->allocation.height/2);
	gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], NULL);

	return TRUE;
}

static GtkWidget *
horizontal_line_new (int height)
{
	GtkWidget *line;
	NautilusBackground *background;

	line = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (line), 100, 1);
	gtk_signal_connect (GTK_OBJECT (line), "expose_event", GTK_SIGNAL_FUNC (line_expose), NULL);
	gtk_widget_set_usize (line, -2, height);

	background = nautilus_get_widget_background (line);
	nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	return line;
}

static void
install_message_destroy (InstallMessage *im)
{
	g_free (im->package_name);
	g_free (im);
}

static InstallMessage *
install_message_new (NautilusServiceInstallView *view, const char *package_name)
{
	InstallMessage *im, *im2;
	GtkWidget *bogus_label;
	GList *iter;

	im = NULL;
	
	for (iter = g_list_first (view->details->message); iter != NULL; iter = g_list_next (iter)) {
		im = (InstallMessage *)(iter->data);
		if (strcmp (im->package_name, package_name) == 0) {
			break;
		}
	}
	if (iter != NULL) {
		/* trash the old one */
		gtk_container_remove (GTK_CONTAINER (view->details->message_box), im->hbox);
		if (im->line != NULL) {
			gtk_container_remove (GTK_CONTAINER (view->details->message_box), im->line);
		} else if (iter->prev) {
			/* remove the line from the one above, if present */
			im2 = (InstallMessage *)(iter->prev->data);
			gtk_container_remove (GTK_CONTAINER (view->details->message_box), im2->line);
			im2->line = NULL;
		}
		view->details->message = g_list_remove (view->details->message, im);
		install_message_destroy (im);
	}

	im = g_new0 (InstallMessage, 1);
	im->label = eazel_services_label_new (NULL, 0, 0.0, 0.5, 0, 0,
					      EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
					      EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
					      NULL, -2, FALSE);
	nautilus_label_set_justify (NAUTILUS_LABEL (im->label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (im->label);
	im->progress_bar = gtk_progress_bar_new ();
	gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (im->progress_bar), GTK_PROGRESS_CONTINUOUS);
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (im->progress_bar), GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_progress_bar_update (GTK_PROGRESS_BAR (im->progress_bar), 0.0);
	gtk_widget_set_usize (im->progress_bar, -2, PROGRESS_BAR_HEIGHT);
	gtk_widget_show (im->progress_bar);

	im->progress_label = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 2,
						       EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
						       EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
						       NULL, -2, TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (im->progress_label), GTK_JUSTIFY_LEFT);

	gtk_widget_show (im->progress_label);

	bogus_label = gtk_label_new ("");
	gtk_widget_show (bogus_label);

	im->vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (im->vbox), im->progress_bar, FALSE, FALSE, 0);
	add_padding_to_box (im->vbox, 0, 5);
	gtk_box_pack_start (GTK_BOX (im->vbox), im->progress_label, FALSE, FALSE, 0);
	gtk_widget_show (im->vbox);

	im->hbox = gtk_hbox_new (FALSE, 0);
	add_padding_to_box (im->hbox, 20, 0);
	gtk_box_pack_start (GTK_BOX (im->hbox), im->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (im->hbox), bogus_label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (im->hbox), im->vbox, FALSE, FALSE, 0);
	add_padding_to_box (im->hbox, 20, 0);
	gtk_widget_show (im->hbox);

#if 0
	if (g_list_length (view->details->message) == STATUS_ROWS) {
		gtk_widget_set_usize (view->details->pane, -2, view->details->pane->allocation.height);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->pane),
						GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	}
#endif

	/* add this, and possibly a separating line, to the message box */
	if (g_list_length (view->details->message) > 0) {
		/* draw line */
		im->line = horizontal_line_new (6);
		gtk_widget_show (im->line);
		gtk_box_pack_end (GTK_BOX (view->details->message_box), im->line, FALSE, FALSE, 0);
	} else {
		im->line = NULL;
	}
	gtk_box_pack_end (GTK_BOX (view->details->message_box), im->hbox, FALSE, FALSE, 5);

	gtk_adjustment_changed (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (view->details->pane)));

	im->package_name = g_strdup (package_name);
	view->details->message = g_list_prepend (view->details->message, im);

	/* unbelievable!  gtk won't redraw correctly unless we poke it in the ass */
	gtk_widget_queue_resize (view->details->form);
	gtk_widget_queue_draw (view->details->form);

	return im;
}

static void
generate_install_form (NautilusServiceInstallView	*view) 
{
	GtkWidget	*temp_box;
	GtkWidget	*title;
	GtkWidget	*viewport;
	GtkWidget	*filler;
	NautilusBackground *background;

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
	view->details->package_name = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
								EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
								EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
								NULL, 4, TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->package_name), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_name, FALSE, FALSE, 15);
	gtk_widget_show (view->details->package_name);

	/* Package Version */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->package_version = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
								   EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
								   EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
								   NULL, -2, TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->package_version), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_version, FALSE, FALSE, 15);
	gtk_widget_show (view->details->package_version);

	add_padding_to_box (view->details->form, 0, 4);

	/* generate the overall progress bar */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->total_progress_bar = gtk_progress_bar_new ();
	gtk_widget_set_usize (view->details->total_progress_bar, -2, PROGRESS_BAR_HEIGHT);
	add_padding_to_box (temp_box, 30, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->total_progress_bar, FALSE, FALSE, 0);
	gtk_widget_show (view->details->total_progress_bar);

	/* add a label for progress messages, but don't show it until there's a message */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);
	view->details->overall_feedback_text = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
									 EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
									 EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
									 NULL, -2, FALSE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->overall_feedback_text), GTK_JUSTIFY_LEFT);
	nautilus_label_set_text (NAUTILUS_LABEL (view->details->overall_feedback_text), " ");
	gtk_widget_show (view->details->overall_feedback_text);
	add_padding_to_box (temp_box, 30, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->overall_feedback_text, TRUE, TRUE, 0);

	add_padding_to_box (view->details->form, 0, 10);

	/* Package Description */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 2);
	gtk_widget_show (temp_box);

	view->details->package_details = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
								   EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
								   EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
								   NULL, -2, FALSE);
	nautilus_label_set_justify (NAUTILUS_LABEL (view->details->package_details), GTK_JUSTIFY_LEFT);
	nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->package_details), TRUE);

	gtk_box_pack_start (GTK_BOX (temp_box), view->details->package_details, FALSE, FALSE, 15);
	gtk_widget_show (view->details->package_details);

	/* filler blob to separate the top from the bottom */
	gtk_box_pack_start (GTK_BOX (view->details->form), gtk_label_new (""), TRUE, FALSE, 0);

	/* add a table at the bottom of the screen to hold current progresses */
	view->details->message_box = gtk_vbox_new (FALSE, 0);
	filler = gtk_label_new ("");
	gtk_widget_show (filler);
	gtk_box_pack_end (GTK_BOX (view->details->message_box), filler, TRUE, TRUE, 0);

	view->details->pane = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (view->details->pane), viewport);
	gtk_widget_show (viewport);
	gtk_container_add (GTK_CONTAINER (viewport), view->details->message_box);
	gtk_widget_show (view->details->message_box);
	background = nautilus_get_widget_background (viewport);
	nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);
	gtk_widget_set_usize (view->details->pane, -2, MESSAGE_BOX_HEIGHT);
	gtk_widget_show (view->details->pane);
	gtk_box_pack_end (GTK_BOX (view->details->form), view->details->pane, FALSE, FALSE, 2);

	/* Setup the progress header */
	/* FIXME: the middle header is all fubar now, so we just say "messages", but the spec
	 * shows "progress" over the 2nd column of the message box.  it's just too hard to get
	 * it aligned currently.
	 */
	view->details->middle_title = eazel_services_header_middle_new (_("Messages"), "");
        gtk_box_pack_end (GTK_BOX (view->details->form), view->details->middle_title, FALSE, FALSE, 0);
	gtk_widget_show (view->details->middle_title);

	gtk_widget_show (view->details->form);
}



/* utility routine to show an error message */
static void
show_overall_feedback (NautilusServiceInstallView *view, char *progress_message)
{
	nautilus_label_set_text (NAUTILUS_LABEL (view->details->overall_feedback_text), progress_message);
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
	nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	view->details->core_package = FALSE;
	view->details->deps = g_hash_table_new (g_str_hash, g_str_equal);

	gtk_widget_show (GTK_WIDGET (view));
}

static gboolean
deps_destroy_foreach (char *key, char *value)
{
	g_free (key);
	g_free (value);
	return TRUE;
}

static void
nautilus_service_install_view_destroy (GtkObject *object)
{
	NautilusServiceInstallView *view;
	GNOME_Trilobite_Eazel_Install	service;
	CORBA_Environment ev;

	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	CORBA_exception_init (&ev);
	service = eazel_install_callback_corba_objref (view->details->installer);
	GNOME_Trilobite_Eazel_Install_stop (service, &ev);
	CORBA_exception_free (&ev);

	g_free (view->details->uri);
	g_free (view->details->current_rpm);
	g_free (view->details->remembered_password);
	g_hash_table_foreach_remove (view->details->deps, (GHRFunc)deps_destroy_foreach, NULL);
	g_hash_table_destroy (view->details->deps);
	g_list_foreach (view->details->message, (GFunc)install_message_destroy, NULL);
	g_list_free (view->details->message);

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
 * "eazel-install:" [ "//" [ username "@" ] [ "hostname" [ ":" port ] ] "/" ] 
 * 	package-name [ "?version=" version ] ( ";" package-name [ "?version=" version ] )*
 *
 * eazel-install:xfig
 * eazel-install://anonymous@/freeamp
 * eazel-install://example.com:8888/nautilus?version=1.0;xpdf;sephiroth?version=0.4
 */
/* returns TRUE if a hostname was parsed from the uri */
static gboolean
nautilus_install_parse_uri (const char *uri, NautilusServiceInstallView *view,
			    char **host, int *port, char **username)
{
	char *p, *q, *pnext, *package_name, *host_spec;
	GList *packages = NULL;
	PackageData *pack;
	gboolean result = FALSE;
	gboolean another_package;

	view->details->categories = NULL;

	p = strchr (uri, ':');
	if (! p) {
		/* bad mojo */
		return result;
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
			result = TRUE;
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
		do {
			pnext = strchr (p, ';');
			if ((pnext != NULL) && (*(pnext+1) != '\0')) {
				another_package = TRUE;
				*pnext++ = '\0';
			} else {
				another_package = FALSE;
			}

			trilobite_debug ("package '%s'", p);
			/* version name specified? */
			q = strchr (p, '?');
			if (q) {
				*q++ = 0;
				if (strncmp (q, "version=", 8) == 0) {
					q += 8;
				}
			}

			package_name = gnome_vfs_unescape_string_for_display (p);
			pack = create_package (package_name, view->details->using_local_file);
			if (q) {
				pack->version = g_strdup (q);
			}
			packages = g_list_prepend (packages, pack);
			g_free (package_name);

			if (pnext != NULL) {
				p = pnext;
			}
		} while (another_package);
	}

	trilobite_debug ("host '%s:%d' username '%s'", *host ? *host : "(default)", *host ? *port : 0,
			 *username ? *username : "(default)");

	/* add to categories */
	if (packages) {
		CategoryData *category;

		category = categorydata_new ();
		category->packages = packages;
		view->details->categories = g_list_prepend (view->details->categories, category);
	}
	return result;
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
	/* can't figure out a decent way to do this yet... :( */
	if (view->details->current_im != NULL) {
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_im->progress_label), text);
		gtk_widget_queue_resize (view->details->message_box);
	}
}


static void
nautilus_service_install_downloading (EazelInstallCallback *cb, const PackageData *pack, int amount, int total,
				      NautilusServiceInstallView *view)
{
	char *out;
	const char *needed_by;
	GList *iter;
	InstallMessage *im = view->details->current_im;
	float fake_amount;

	if (view->details->installer == NULL) {
		g_warning ("Got download notice after unref!");
		return;
	}

	/* install lib better damn well know the name of the package by the time we download it! */
	g_assert (pack->name != NULL);

	if (amount == 0) {
		/* could be a redundant zero-trigger for the same rpm... */
		if (view->details->current_rpm && (strcmp (view->details->current_rpm, pack->name) == 0)) {
			/* spin_cylon (view); */
			return;
		}

		if (view->details->cylon_timer) {
			gtk_timeout_remove (view->details->cylon_timer);
			view->details->cylon_timer = 0;
		}

		g_free (view->details->current_rpm);
		view->details->current_rpm = g_strdup (pack->name);

		/* figure out if this is a toplevel package, and if so, update the header */
		for (iter = g_list_first (((CategoryData *)(view->details->categories->data))->packages);
		     iter != NULL; iter = g_list_next (iter)) {
			PackageData *pack2 = (PackageData *)(iter->data);
			if ((pack2->name != NULL) && (strcmp (pack2->name, pack->name) == 0)) {
				out = g_strdup_printf (_("Downloading \"%s\""), pack->name);
				nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_name), out);
				g_free (out);
			}
		}

		/* new progress message and bar */
		im = view->details->current_im = install_message_new (view, pack->name);
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 0.0);
		out = g_strdup_printf (_("0K of %dK"), total/1024);
		nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
		g_free (out);
		view->details->last_k = 0;

		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" needs \"%s\" to run.\nI'm now attempting to download it."),
					       needed_by, pack->name);
		} else {
			out = g_strdup_printf (_("I'm attempting to download package \"%s\"."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
	} else if (amount == total) {
		/* done! */
		current_progress_bar_complete (view, _("Complete"));
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 1.0);
		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" needs \"%s\" to run.\nI've downloaded it successfully."),
					       needed_by, pack->name);
		} else {
			out = g_strdup_printf (_("I've successfully downloaded package \"%s\"."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
		g_free (view->details->current_rpm);
		view->details->current_rpm = NULL;
		view->details->current_im = NULL;
		/* update downloaded bytes */
		view->details->download_bytes_sofar += pack->bytesize;
		/* not until we get an rpm size */
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar),
					     (float) view->details->download_bytes_sofar /
					     (float) view->details->download_bytes_total);
	} else {
		/* could be a leftover event, after user hit STOP (in which case, current_im = NULL) */
		if ((im != NULL) && (im->progress_bar != NULL)) {
			gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar),
						     (float) amount / (float) total);
			if ((amount/1024) >= view->details->last_k + 10) {
				out = g_strdup_printf (_("%dK of %dK"), amount/1024, total/1024);
				nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
				g_free (out);
				view->details->last_k = (amount/1024);
			}
		}

		/* so, for PR3, we are given a "size" field in the softcat XML which is actually 
		 * the size of the decompressed files.  so this little hocus-pocus scales the
		 * actual size (which we know once we start downloading the file) to match the   
		 * previously-assumed size
		 */
		fake_amount = (float)amount * (float)pack->bytesize / (float)total;
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar),
					     ((float) view->details->download_bytes_sofar + fake_amount) /
					     (float) view->details->download_bytes_total);
	}
}

/* keep this info for secret later use */
static void
nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
					   const PackageData *needs, NautilusServiceInstallView *view)
{
	char *key, *value;

	/* add to deps hash for later */
	if (g_hash_table_lookup_extended (view->details->deps, needs->name, (void **)&key, (void **)&value)) {
		g_hash_table_remove (view->details->deps, key);
		g_free (key);
		g_free (value);
	}
	g_hash_table_insert (view->details->deps, g_strdup (needs->name), g_strdup (package->name));

	value = g_strdup_printf (_("Getting information about package \"%s\" ..."), package->name);
	show_overall_feedback (view, value);
	g_free (value);
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

static void
flatten_package_tree_foreach (PackageData *package, GList **flattened_list)
{
	GList *iter;
	gboolean found = FALSE;
	PackageData *pack;

	for (iter = g_list_first (*flattened_list); iter != NULL; iter = g_list_next (iter)) {
		pack = (PackageData *)(iter->data);
		if ((strcmp (pack->name, package->name) == 0) &&
		    (strcmp (pack->version, package->version) == 0)) {
			found = TRUE;
			break;
		}
	}

	if (! found) {
		/* add it to the flattened list */
		*flattened_list = g_list_prepend (*flattened_list, package);
	}

	g_list_foreach (package->soft_depends, (GFunc)flatten_package_tree_foreach, flattened_list);
	g_list_foreach (package->hard_depends, (GFunc)flatten_package_tree_foreach, flattened_list);
}

/* given a package tree containing possibly redundant packages, assemble a new list
 * which contains only those packages with unique <name, version>
 */
static void
flatten_package_tree (GList *package_list_in, GList **flattened_list)
{
	g_list_foreach (package_list_in, (GFunc)flatten_package_tree_foreach, flattened_list);
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
	GList *package_list;
	GList *iter;
	char *out;
	unsigned long total_k;

	/* turn off the cylon and show "real" progress */
	turn_cylon_off (view, 0.0);

	/* assemble initial list of packages to browse */
	package_list = NULL;
	flatten_package_tree ((GList *)packages, &package_list);
	package_list = g_list_reverse (package_list);

	show_overall_feedback (view, _("Preparing to download packages..."));

	message = g_string_new ("");
	message = g_string_append (message, _("I'm about to download and install the following packages:\n\n"));

	view->details->download_bytes_total = view->details->download_bytes_sofar = 0;
	for (iter = g_list_first (package_list); iter != NULL; iter = g_list_next (iter)) {
		package = (PackageData *)(iter->data);
		out = packagedata_get_readable_name (package);
	        g_string_sprintfa (message, " \xB7 %s\n", out);
		g_free (out);
		view->details->download_bytes_total += package->bytesize;

		if (package->toplevel) {
			nautilus_service_install_check_for_desktop_files (view,
									  cb,
									  package);
		}
	}
	total_k = (view->details->download_bytes_total+512)/1024;
	/* arbitrary dividing line */
	if (total_k > 4096) {
		out = g_strdup_printf (_("for a total of %ld MB."), (total_k+512)/1024);
	} else {
		out = g_strdup_printf (_("for a total of %ld kB."), total_k);
	}
	g_string_sprintfa (message, "\n%s", out);
	g_free (out);

	message = g_string_append (message, _("\nIs this okay?"));
	toplevel = gtk_widget_get_toplevel (view->details->message_box);

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
		g_list_free (package_list);
		view->details->cancelled = TRUE;
		view->details->cancelled_before_downloads = TRUE;
		/* EVIL EVIL hack that causes the next dialog to show up instead of being hidden */
		sleep (1);
		while (gtk_events_pending ())
			gtk_main_iteration ();
		return answer;
	}

	if (g_list_length (package_list) == 1) {
		out = g_strdup (_("Downloading 1 package"));
	} else {
		out = g_strdup_printf (_("Downloading %d packages"), g_list_length (package_list));
	}
	show_overall_feedback (view, out);
	g_free (out);

	g_list_free (package_list);
	view->details->current_package = 0;
	return answer;
}


static void
nautilus_service_install_download_failed (EazelInstallCallback *cb, const PackageData *pack,
					  NautilusServiceInstallView *view)
{
	char *out, *tmp;

	turn_cylon_off (view, 0.0);

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	tmp = packagedata_get_readable_name (pack);
	out = g_strdup_printf (_("Download of package \"%s\" failed!"), tmp);
	g_free (tmp);
	if (view->details->current_im != NULL) {
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_im->label), out);
	}
	g_free (out);
}

static void
previous_install_finished (NautilusServiceInstallView *view)
{
	InstallMessage *im;
	char *needed_by;
	char *out;

	im = view->details->current_im;
	if (im != NULL) {
		current_progress_bar_complete (view, _("Complete"));
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 1.0);

		needed_by = g_hash_table_lookup (view->details->deps, view->details->current_rpm);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" needed \"%s\" to run.\nI've downloaded and installed it."),
					       needed_by, view->details->current_rpm);
		} else {
			out = g_strdup_printf (_("I've downloaded and installed package \"%s\"."), view->details->current_rpm);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
	}
	g_free (view->details->current_rpm);
	view->details->current_rpm = NULL;
	view->details->current_im = NULL;
}

static void
nautilus_service_install_installing (EazelInstallCallback *cb, const PackageData *pack,
				     int current_package, int total_packages,
				     int package_progress, int package_total,
				     int total_progress, int total_total,
				     NautilusServiceInstallView *view)
{
	InstallMessage *im;
	gfloat overall_complete, complete;
	char *out;
	char *needed_by;

	im = view->details->current_im;
	if (current_package != view->details->current_package) {
		/* no longer "loading" anything */
		nautilus_view_report_load_complete (view->details->nautilus_view);

		/* starting a new package -- create new progress indicator */
		out = g_strdup_printf (_("Installing package %d of %d"), current_package, total_packages);
		show_overall_feedback (view, out);
		g_free (out);

		/* new behavior: sometimes the previous package wasn't quite closed out -- do it now */
		if (im != NULL) {
			previous_install_finished (view);
		}

		/* if you're looking for the place where we notice that one of nautilus's core
		 * packages is being upgraded, this is it.  this is an evil, evil way to do it,
		 * but nobody's come up with anything better yet.
		 */
		if (pack->name) {		
			if ((g_strncasecmp (pack->name, "nautilus", 8) == 0) ||
			    (g_strncasecmp (pack->name, "gnome-vfs", 9) == 0) ||
			    (g_strncasecmp (pack->name, "oaf", 3) == 0)) {
				view->details->core_package = TRUE;
			} 
		}

		g_free (view->details->current_rpm);
		view->details->current_rpm = g_strdup (pack->name);
		view->details->current_im = im = install_message_new (view, pack->name);
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 0.0);
		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" needs \"%s\" to run.\nI'm now installing it."),
					       needed_by, pack->name);
		} else {
			out = g_strdup_printf (_("I'm installing package \"%s\"."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);

		view->details->current_package = current_package;

		if (pack->toplevel) {
			/* this package is a main one.  update package info display, now that we know it */
			out = g_strdup_printf (_("Installing \"%s\""), pack->name);
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_name), out);
			gtk_widget_queue_draw (view->details->package_name);
			g_free (out);
			if (strchr (pack->description, '\n') != NULL) {
				nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->package_details), FALSE);
			} else {
				nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->package_details), TRUE);
			}
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_details), pack->description);
			out = g_strdup_printf (_("Version: %s"), pack->version);
			nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_version), out);
			g_free (out);
		}
	}

	complete = (gfloat) package_progress / package_total;
	overall_complete = (gfloat) total_progress / total_total;
	gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), complete);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), overall_complete);
	out = g_strdup_printf (_("%d%%"), (int)(complete*100.0));
	nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
	g_free (out);

	if ((package_progress == package_total) && (package_total > 0)) {
		/* done with another package! */
		previous_install_finished (view);
	}
}

/* Get a description of the application pointed to by a given dentry and path fragment */
static char*
nautilus_install_service_describe_menu_entry (GnomeDesktopEntry *dentry,
                                              const char        *path_prefix,
                                              const char        *path_fragment)
{
	char *slash;
	char *addition = NULL, *addition_tmp;
	char *fragment_tmp;

	char **pieces;
	char *so_far;
	int i;
	char *dir, *file, *menu;
	GnomeDesktopEntry *dir_dentry;

	fragment_tmp = g_strdup (path_fragment);
	slash = strrchr (fragment_tmp, G_DIR_SEPARATOR);
	if (slash != NULL) {
		*slash = '\0';
	}
	pieces = g_strsplit (fragment_tmp, "/", 128); /* FIXME "/" -> G_DIR_SEPARATOR */
	g_free (fragment_tmp);
	so_far = g_strdup (path_prefix);

	for (i=0; pieces[i] != NULL; i++) {

		dir = g_strconcat (so_far, pieces[i], "/", NULL);
		file = g_strconcat (dir, ".directory", NULL);

		g_free (so_far);
		so_far = dir;

		dir_dentry = gnome_desktop_entry_load (file);
		g_free (file);

		menu = NULL;
		if (dir_dentry != NULL) {
			menu = dir_dentry->name;
		} else {
			menu = pieces[i];
		}

		if (addition == NULL) {
			addition = g_strdup_printf 
					(_(" \xB7 %s is in the Gnome menu under %s"),
					dentry->name, menu);
		} else {
			addition_tmp = g_strconcat (addition, " / ", dir_dentry->name, NULL);
			g_free (addition);
			addition = addition_tmp;
		}

		/* menu doesn't need to be freed, because it points into another structure */

		if (dir_dentry != NULL) {
			gnome_desktop_entry_free (dir_dentry);
		}
	}	
	g_free (so_far);
	g_strfreev (pieces);

	if (addition == NULL) {
		addition = g_strdup_printf (_(" \xB7 %s is in the Gnome menu.\n"), dentry->name);
	} else {
		addition_tmp = g_strconcat (addition, ".\n", NULL);
		g_free (addition);
		addition = addition_tmp;
	}

	return addition;
	
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
		char *addition = NULL;
		char *tmp;
		GnomeDesktopEntry *dentry = gnome_desktop_entry_load (fname);

		if (dentry->is_kde) {
			addition = g_strdup_printf (_(" \xB7 %s is in the KDE menu.\n"), dentry->name);
		} else {
			/* match desktop files against a set of paths that the panel is known to
			 * put in the menu. */
			char *desktop_prefixes[] = {
				"/gnome/apps/",
				"/applnk/"
			};
			int num_prefixes = 2;
			int i;

			for (i=0; i<num_prefixes; i++) {
				char *gnomeapp = desktop_prefixes[i];
				char *apps_ptr = strstr (fname, gnomeapp);
				if (apps_ptr) {
					char *full_prefix = g_strndup (fname, (apps_ptr)-fname + 
							strlen (gnomeapp));
					addition = nautilus_install_service_describe_menu_entry
							(dentry, full_prefix, apps_ptr+strlen (gnomeapp));
					g_free (full_prefix);
					if (addition != NULL) {
						break;
					}
				}
			}
		}
		if (addition) {
			tmp = g_strdup_printf ("%s%s", result, addition);
			g_free (result);
			result = tmp;
			g_free (addition);
		}
		gnome_desktop_entry_free (dentry);
	}
	return result;
}

/* most likely OBSOLETE */
static gboolean
nautilus_service_install_solve_cases (NautilusServiceInstallView *view)
{
	gboolean answer = FALSE;
	GtkWidget *toplevel;
	GString *messages;
	GList *strings;
	GtkWidget *dialog;

	messages = g_string_new ("");

	if (view->details->problem_cases) {
		GList *iterator;
		/* Create string versions to show the user */
		g_string_sprintfa (messages, "%s\n%s\n\n", 
				   _("I ran into problems while installing."), 
				   _("I'd like to try the following :"));
		strings = eazel_install_problem_cases_to_string (view->details->problem,
								 view->details->problem_cases);
		for (iterator = strings; iterator; iterator = g_list_next (iterator)) {
			g_string_sprintfa (messages, " \xB7 %s\n", (char*)(iterator->data));
		}
		g_list_foreach (strings, (GFunc)g_free, NULL);
		g_list_free (strings);
		g_string_sprintfa (messages, "\n%s",  
				   _("Is this ok ?"));
		
		toplevel = gtk_widget_get_toplevel (view->details->message_box);
		if (GTK_IS_WINDOW (toplevel)) {
			dialog = gnome_question_dialog_parented (messages->str, (GnomeReplyCallback)reply_callback,
								 &answer, GTK_WINDOW (toplevel));
		} else {
			dialog = gnome_question_dialog (messages->str, (GnomeReplyCallback)reply_callback, &answer);
		}
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		g_string_free (messages, TRUE);
	}

	return answer;
}

static void
nautilus_service_install_done (EazelInstallCallback *cb, gboolean success, NautilusServiceInstallView *view)
{
	CORBA_Environment ev;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	char *message;
	char *real_message;
	gboolean answer = FALSE;
	gboolean question_dialog;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	if (view->details->failure) {
		success = FALSE;
		view->details->failure = FALSE;
		/* we have already indicated failure elsewhere.  good day to you, sir. */
	}

	turn_cylon_off (view, success ? 1.0 : 0.0);
	g_free (view->details->current_rpm);
	view->details->current_rpm = NULL;

	if (view->details->cancelled) {
		message = _("Installation aborted.");
	} else if (view->details->already_installed) {
		message = _("This package has already been installed.");
	} else if (success) {
		message = _("Installation complete!");
	} else {
		message = _("Installation failed!");
		answer = nautilus_service_install_solve_cases (view);
	}

	show_overall_feedback (view, message);

	if (answer) {
		eazel_install_problem_handle_cases (view->details->problem, 
						    view->details->installer, 
						    &(view->details->problem_cases), 
						    &(view->details->categories),
						    NULL,
						    NULL);
	} else {
		question_dialog = TRUE;

		if (success && view->details->desktop_files &&
				!view->details->cancelled &&
				!view->details->already_installed) {
			real_message = g_strdup_printf (_("%s\n%s\nErase the leftover RPM files?"), 
							message,
							nautilus_install_service_locate_menu_entries (view));
		} else if (view->details->cancelled_before_downloads || view->details->already_installed) {
			real_message = g_strdup (message);
			question_dialog = FALSE;
		} else {
			if (view->details->cancelled || view->details->failure) {
				real_message = g_strdup_printf (_("%s\nErase the RPM files?"), message);
			} else {
				real_message = g_strdup_printf (_("%s\nErase the leftover RPM files?"), message);
			}
		}
		toplevel = gtk_widget_get_toplevel (view->details->message_box);
		if (GTK_IS_WINDOW (toplevel)) {
			if (question_dialog) {
				dialog = gnome_question_dialog_parented (real_message, (GnomeReplyCallback)reply_callback,
									 &answer, GTK_WINDOW (toplevel));
			} else {
				dialog = gnome_ok_dialog_parented (real_message, GTK_WINDOW (toplevel));
				answer = FALSE;
			}
		} else {
			if (question_dialog) {
				dialog = gnome_question_dialog (real_message, (GnomeReplyCallback)reply_callback, &answer);
			} else {
				dialog = gnome_ok_dialog (real_message);
				answer = FALSE;
			}
		}
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		g_free (real_message);

		if (answer) {
			CORBA_exception_init (&ev);
			eazel_install_callback_delete_files (cb, &ev);
			CORBA_exception_free (&ev);
		}
		
		if (success && view->details->core_package) {
			message = _("A core package of Nautilus has been\n"
				    "updated.  You should restart Nautilus.\n\n"
				    "Do you wish to do that now?");
			if (GTK_IS_WINDOW (toplevel)) {
				dialog = gnome_question_dialog_parented (message,
									 (GnomeReplyCallback)reply_callback,
									 &answer, GTK_WINDOW (toplevel));
			} else {
				dialog = gnome_question_dialog (message, (GnomeReplyCallback)reply_callback,
								&answer);
			}
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
			
			if (answer) {
				if (execlp ("nautilus", "nautilus", "--restart", NULL) < 0) {
					g_message ("Exec error %s", strerror (errno));
				}
			}
		}

		/* send them to the predetermined "next" url
		 * -- but only if they haven't set jump-after-install off
		 */
		message = g_strdup (NEXT_URL);
		if (eazel_install_configure_check_jump_after_install (&message)) {
			nautilus_view_open_location_in_this_window (view->details->nautilus_view, message);
		}
		g_free (message);
	}
}

static void
nautilus_service_install_failed (EazelInstallCallback *cb, const PackageData *package, NautilusServiceInstallView *view)
{
	char *tmp, *message;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	turn_cylon_off (view, 0.0);

	/* override the "success" result for install_done signal */
	view->details->failure = TRUE;

	if (package->status == PACKAGE_ALREADY_INSTALLED) {
		view->details->already_installed = TRUE;
		return;
	}

	tmp = packagedata_get_readable_name (package);
	message = g_strdup_printf (_("Installation failed on %s"), tmp);
	show_overall_feedback (view, message);
	g_free (tmp);
	g_free (message);

	/* Get the new set of problem cases */
	eazel_install_problem_tree_to_case (view->details->problem,
					    package,
					    FALSE,
					    &(view->details->problem_cases));

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
	char			*host;
	int			port;
	char			*username;
	GNOME_Trilobite_Eazel_Install	service;
	CORBA_Environment	ev;
	char 			*out, *p;
	gboolean                set_auth;

	/* get default host/port */
	host = g_strdup (trilobite_get_services_address ());
	if ((p = strchr (host, ':')) != NULL) {
		*p = 0;
		port = atoi (p+1);
	} else {
		port = 80;
	}
	username = NULL;
	set_auth = !(nautilus_install_parse_uri (uri, view, &host, &port, &username));

	if (! view->details->categories) {
		return;
	}

	/* NOTE: This adds a libeazelinstall packagedata object to the view */
	pack = (PackageData*) gtk_object_get_data (GTK_OBJECT (view), "packagedata");
	if (pack != NULL) {
		/* Destroy the old */
		packagedata_destroy (pack, TRUE);
	}

	/* find the package data for the package we're about to install */
	category_data = (CategoryData *) view->details->categories->data;
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
	if (view->details->installer) {
		eazel_install_callback_unref (GTK_OBJECT (view->details->installer));
	}
	view->details->installer = eazel_install_callback_new ();
	if (view->details->installer == NULL) {
		GtkWidget *toplevel, *dialog;
		char *message;

		nautilus_view_report_load_complete (view->details->nautilus_view);
		gtk_widget_hide (view->details->form);

		message = g_strdup (_("The Eazel install service is missing:\nInstalls will not work."));
		toplevel = gtk_widget_get_toplevel (view->details->message_box);
		if (GTK_IS_WINDOW (toplevel)) {
			dialog = gnome_error_dialog_parented (message, GTK_WINDOW (toplevel));
		} else {
			dialog = gnome_error_dialog (message);
		}
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		return;
	}

	view->details->problem = eazel_install_problem_new ();
	view->details->root_client = set_root_client (eazel_install_callback_bonobo (view->details->installer), view);
	service = eazel_install_callback_corba_objref (view->details->installer);
	GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	GNOME_Trilobite_Eazel_Install__set_server (service, host, &ev);
	GNOME_Trilobite_Eazel_Install__set_server_port (service, port, &ev);
	GNOME_Trilobite_Eazel_Install__set_auth (service, set_auth, &ev);

	if (username != NULL) {
		GNOME_Trilobite_Eazel_Install__set_username (service, username, &ev);
	}
	GNOME_Trilobite_Eazel_Install__set_test_mode (service, FALSE, &ev);

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
	eazel_install_callback_install_packages (view->details->installer, 
						 view->details->categories, 
						 NULL, &ev);

	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Contacting the software catalog ..."));

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
	if (view->details->message) {
		g_list_foreach (view->details->message, (GFunc)install_message_destroy, NULL);
		g_list_free (view->details->message);
		view->details->message = NULL;
	}
	if (view->details->desktop_files) {
		g_list_foreach (view->details->desktop_files, (GFunc)g_free, NULL);
		g_list_free (view->details->desktop_files);
		view->details->desktop_files = NULL;
	}

	/* clear some variables */
	view->details->already_installed = FALSE;
	view->details->cancelled = FALSE;
	view->details->failure = FALSE;
	

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
	GNOME_Trilobite_Eazel_Install	service;
	CORBA_Environment ev;

	g_assert (nautilus_view == view->details->nautilus_view);

	CORBA_exception_init (&ev);
	service = eazel_install_callback_corba_objref (view->details->installer);
	GNOME_Trilobite_Eazel_Install_stop (service, &ev);
	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Package download aborted."));
	turn_cylon_off (view, 0.0);
	current_progress_bar_complete (view, _("Aborted"));

	view->details->cancelled = TRUE;
}
