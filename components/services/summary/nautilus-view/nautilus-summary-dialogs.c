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

#include <libnautilus-extensions/nautilus-caption-table.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-image.h>

#include <libgnomeui/gnome-stock.h>
#include <stdio.h>
#include <unistd.h>

#include <orb/orbit.h>
#include <liboaf/liboaf.h>
#include <libtrilobite/trilobite-redirect.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>

#include "nautilus-summary-view.h"
#include "eazel-services-extensions.h"
#include "eazel-summary-shared.h"
#include "eazel-services-extensions.h"
#include "nautilus-summary-callbacks.h"
#include "nautilus-summary-dialogs.h"
#include "nautilus-summary-view-private.h"

#define notDEBUG_PEPPER	1


static void		error_dialog_cancel_cb			(GtkWidget			*button,
								 NautilusSummaryView		*view);
static GtkWindow	*get_window_from_summary_view		(NautilusSummaryView		*view);
static void		set_dialog_parent			(NautilusSummaryView		*view,
								 GnomeDialog			*dialog);
static void		name_or_password_field_activated	(GtkWidget			*caption_table,
								 int				active_entry,
								 gpointer			user_data);

void
nautilus_summary_show_login_failure_dialog (NautilusSummaryView *view, 
					    const char *message)
{
	nautilus_show_error_dialog (message, 
			            _("Eazel Service Login Error"), 
				    get_window_from_summary_view (view));
}

void
nautilus_summary_show_error_dialog (NautilusSummaryView *view, 
				     const char *message)
{
	GnomeDialog	*dialog;

	dialog = nautilus_show_error_dialog (message, 
				             _("Service Error"), 
				             get_window_from_summary_view (view));
	gtk_signal_connect (GTK_OBJECT (dialog),
			    "destroy",
			    error_dialog_cancel_cb,
			    view);
}

void
nautilus_summary_show_login_dialog (NautilusSummaryView *view)
{
	GnomeDialog	*dialog;
	GtkWidget	*hbox;
	GtkWidget	*image;
	GtkWidget	*message;
	GtkWidget	*caption_hbox;
	char		*message_text;
	char		*image_name;
	char		*button_text;

	dialog = NULL;
	image = NULL;

	if (view->details->attempt_number == 0) {
		button_text = g_strdup (_("Register Now"));
	} else {
		button_text = g_strdup (_("Help"));
	}

	/* if the dialog is still open, then close it and open a new one */
	if (view->details->login_dialog != NULL) {
		gnome_dialog_close (GNOME_DIALOG (view->details->login_dialog));
		view->details->login_dialog = NULL;
	}

	dialog = GNOME_DIALOG (gnome_dialog_new (_("Services Login"), button_text, 
			       GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL));

	/* TODO: replace all reference to dialog in this code with view->details->login_dialog */
	view->details->login_dialog = dialog;

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), 
		&view->details->login_dialog);
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
			image_name = "big_services_icon.png";
			message_text = _("Please log in to Eazel services");
			break;
		case retry:
			image_name = "serv_dialog_alert.png";
			message_text = _("Your user name or password were not correct.  Please try again.");
			break;
		default:
			g_assert_not_reached();
			image_name = "big_services_icon.png";
			message_text = _("Please log in to Eazel services");
			break;
	}

	image = eazel_services_image_new (image_name, NULL, 0);
	nautilus_image_set_background_mode (NAUTILUS_IMAGE (image), NAUTILUS_SMOOTH_BACKGROUND_GTK);

	hbox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (hbox);

	if (image) {
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		gtk_widget_show (image);
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

void
widget_set_nautilus_background_color (GtkWidget *widget, const char *color)
{
	NautilusBackground      *background;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (color != NULL);

	background = nautilus_get_widget_background (widget);

	nautilus_background_reset (background);
	nautilus_background_set_color (background, color);

}

/* callback to handle cancel error_dialog button. */
static void
error_dialog_cancel_cb (GtkWidget *button, 
			NautilusSummaryView *view)
{
	nautilus_view_go_back (view->details->nautilus_view);
}

static GtkWindow *
get_window_from_summary_view (NautilusSummaryView *view)
{
	GtkWidget *parent_window;

	g_assert (NAUTILUS_IS_SUMMARY_VIEW (view));

	parent_window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	if (parent_window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (parent_window);
}

static void
set_dialog_parent (NautilusSummaryView *view, GnomeDialog *dialog)
{
	GtkWindow *parent;

	g_assert (NAUTILUS_IS_SUMMARY_VIEW (view));
	g_assert (GNOME_IS_DIALOG (dialog));

	parent = get_window_from_summary_view (view);
	if (parent != NULL && gnome_preferences_get_dialog_centered ()) {
		/* User wants us to center over parent */
		
		/* FIXME: this is cut and pasted from gnome-dialog.h,
		 * because calling gnome_dialog_set_parent would make
		 * the dialog transient for the summary view's plug
		 * widget, not the top level window, thus making it
		 * not get focus.
		 */
		gint x, y, w, h, dialog_x, dialog_y;
		
		if (!GTK_WIDGET_VISIBLE (parent)) return; /* Can't get its
							     size/pos */
		
		/* Throw out other positioning */
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_NONE);
		
		gdk_window_get_origin (GTK_WIDGET (parent)->window, &x, &y);
		gdk_window_get_size (GTK_WIDGET (parent)->window, &w, &h);
		
		/* The problem here is we don't know how big the dialog is.
		   So "centered" isn't really true. We'll go with 
		   "kind of more or less on top" */
		
		dialog_x = x + w/4;
		dialog_y = y + h/4;
		
		gtk_widget_set_uposition (GTK_WIDGET (dialog), dialog_x, dialog_y); 
	}
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

