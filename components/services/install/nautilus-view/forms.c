/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001  Eazel, Inc
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
#include "forms.h"
#include "callbacks.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"
#include <stdio.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-viewport.h>


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

void
install_message_destroy (InstallMessage *im)
{
	g_free (im->package_name);
	g_free (im);
}

InstallMessage *
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

	im->progress_label = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
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

	/* show the middle header in case this is the first install message */
	gtk_widget_show (view->details->middle_title);
	gtk_widget_show (view->details->total_progress_bar);

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

void
generate_install_form (NautilusServiceInstallView *view)
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
	/* show the progress bar only when ready */
	gtk_widget_hide (view->details->total_progress_bar);

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

	/* query box: for asking the user questions during the preflight signal */
	view->details->query_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->query_box, FALSE, FALSE, 0);
	gtk_widget_hide (view->details->query_box);

	/* filler blob to separate the top from the bottom */
	gtk_box_pack_start (GTK_BOX (view->details->form), gtk_label_new (""), TRUE, FALSE, 0);

	/* add a table at the bottom of the screen to hold current progresses */
	view->details->message_box = gtk_vbox_new (FALSE, 0);
	filler = gtk_label_new ("");
	gtk_widget_show (filler);
	gtk_box_pack_end (GTK_BOX (view->details->message_box), filler, TRUE, TRUE, 0);

	view->details->pane = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	viewport = nautilus_viewport_new (NULL, NULL);
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
	/* don't show the progress header until there's still to go there */

	gtk_widget_show (view->details->form);
}

/* utility routine to show an error message */
void
show_overall_feedback (NautilusServiceInstallView *view, char *progress_message)
{
	nautilus_label_set_text (NAUTILUS_LABEL (view->details->overall_feedback_text), progress_message);
	gtk_widget_show (view->details->overall_feedback_text);
}

void
update_package_info_display (NautilusServiceInstallView *view, const PackageData *pack, const char *format)
{
	char *out;

       out = g_strdup_printf (format, pack->name);
       nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_name), out);
       g_free (out);

       if ((pack->description != NULL) && 
           (strchr (pack->description, '\n') != NULL)) {
               nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->package_details), FALSE);
       } else {
               nautilus_label_set_wrap (NAUTILUS_LABEL (view->details->package_details), TRUE);
       }
       nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_details), pack->description);
       out = g_strdup_printf (_("Version: %s"), pack->version);
       nautilus_label_set_text (NAUTILUS_LABEL (view->details->package_version), out);
       g_free (out);
}

/* replace the current progress bar (in the message box) with a centered label saying "Complete!" */
void
current_progress_bar_complete (NautilusServiceInstallView *view, const char *text)
{
	/* can't figure out a decent way to do this yet... :( */
	if (view->details->current_im != NULL) {
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_im->progress_label), text);
		gtk_widget_queue_resize (view->details->message_box);
	}
}

static void
button_ok_clicked (GtkWidget *button, NautilusServiceInstallView *view)
{
        view->details->preflight_status = PREFLIGHT_OK;
}

static void
button_cancel_clicked (GtkWidget *button, NautilusServiceInstallView *view)
{
        view->details->preflight_status = PREFLIGHT_CANCEL;
}

void
make_query_box (NautilusServiceInstallView *view, EazelInstallCallbackOperation op, GList *package_list)
{
        GtkWidget *top_label;
        GtkWidget *bottom_label;
        GtkWidget *hbox_list;
        GtkWidget *vbox_list;
        GtkWidget *list_label;
        GtkWidget *hbox_buttons;
        GtkWidget *button_ok;
        GtkWidget *button_cancel;
        char *text;
        GList *iter;
        unsigned long total_k;
        PackageData *package;
        int max_width;
        GtkRequisition requisition;

	switch (op) {
	case EazelInstallCallbackOperation_INSTALL:
                text = _("These packages are about to be downloaded and installed:");
		break;
	case EazelInstallCallbackOperation_UNINSTALL:
                text = _("These packages are about to be uninstalled:");	
		break;
	case EazelInstallCallbackOperation_REVERT:
                text = _("These packages are about to be reverted:");	
		break;
        default:
                text = "???";
	}
        top_label = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
					      EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
					      EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
					      NULL, -1, FALSE);
        nautilus_label_set_text (NAUTILUS_LABEL (top_label), text);
	nautilus_label_set_justify (NAUTILUS_LABEL (top_label), GTK_JUSTIFY_LEFT);
        gtk_widget_show (top_label);

        /* build up vbox list of packages */
        vbox_list = gtk_vbox_new (FALSE, 0);
	for (iter = g_list_first (package_list); iter != NULL; iter = g_list_next (iter)) {
		package = PACKAGEDATA (iter->data);
		text = packagedata_get_readable_name (package);
                list_label = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
                                                       EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
                                                       EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
                                                       NULL, -1, TRUE);
                nautilus_label_set_text (NAUTILUS_LABEL (list_label), text);
                nautilus_label_set_justify (NAUTILUS_LABEL (list_label), GTK_JUSTIFY_LEFT);
                gtk_widget_show (list_label);
                gtk_box_pack_start (GTK_BOX (vbox_list), list_label, FALSE, FALSE, 0);
                g_free (text);
        }
        gtk_widget_show (vbox_list);

        /* hbox around the list to offset it */
        hbox_list = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox_list), vbox_list, FALSE, FALSE, 20);
        gtk_widget_show (hbox_list);

        /* bottom label -- confirmation question */
	total_k = (view->details->download_bytes_total+512)/1024;
	/* arbitrary dividing line */
	if (total_k > 4096) {
		text = g_strdup_printf (_("for a total of %ld MB."), (total_k+512)/1024);
	} else {
		text = g_strdup_printf (_("for a total of %ld KB."), total_k);
	}
        bottom_label = eazel_services_label_new (NULL, 0, 0.0, 0.0, 0, 0,
                                                 EAZEL_SERVICES_BODY_TEXT_COLOR_RGB,
                                                 EAZEL_SERVICES_BACKGROUND_COLOR_RGB,
                                                 NULL, -1, FALSE);
        nautilus_label_set_text (NAUTILUS_LABEL (bottom_label), text);
	nautilus_label_set_justify (NAUTILUS_LABEL (bottom_label), GTK_JUSTIFY_LEFT);
        gtk_widget_show (bottom_label);
        g_free (text);

        /* buttons */
        button_ok = gtk_button_new_with_label (_("Continue"));
        gtk_signal_connect (GTK_OBJECT (button_ok), "clicked", GTK_SIGNAL_FUNC (button_ok_clicked), view);
        gtk_widget_show (button_ok);

        button_cancel = gtk_button_new_with_label (_("Cancel"));
        gtk_signal_connect (GTK_OBJECT (button_cancel), "clicked", GTK_SIGNAL_FUNC (button_cancel_clicked), view);
        gtk_widget_show (button_cancel);

        gtk_widget_size_request (button_ok, &requisition);
        max_width = button_ok->requisition.width;
        gtk_widget_size_request (button_cancel, &requisition);
        if (requisition.width > max_width) {
                max_width = requisition.width;
        }
        gtk_widget_set_usize (button_ok, max_width+10, -2);
        gtk_widget_set_usize (button_cancel, max_width+10, -2);

        hbox_buttons = gtk_hbox_new (FALSE, 0);
        list_label = gtk_label_new ("");
        gtk_widget_show (list_label);
        gtk_box_pack_start (GTK_BOX (hbox_buttons), list_label, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox_buttons), button_cancel, FALSE, FALSE, 10);
        gtk_box_pack_start (GTK_BOX (hbox_buttons), button_ok, FALSE, FALSE, 0);
        gtk_widget_show (hbox_buttons);

        gtk_box_pack_start (GTK_BOX (view->details->query_box), top_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (view->details->query_box), hbox_list, FALSE, FALSE, 10);
        gtk_box_pack_start (GTK_BOX (view->details->query_box), bottom_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (view->details->query_box), hbox_buttons, FALSE, FALSE, 10);
        gtk_widget_show (view->details->query_box);

        gtk_container_set_focus_child (GTK_CONTAINER (view->details->form), button_ok);
}
