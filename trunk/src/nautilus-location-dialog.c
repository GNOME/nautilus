/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "nautilus-location-dialog.h"

#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <libgnomeui/gnome-help.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include "nautilus-location-entry.h"
#include "nautilus-desktop-window.h"

struct _NautilusLocationDialogDetails {
	GtkWidget *entry;
	NautilusWindow *window;
};

static void  nautilus_location_dialog_class_init       (NautilusLocationDialogClass *class);
static void  nautilus_location_dialog_init             (NautilusLocationDialog      *dialog);

EEL_CLASS_BOILERPLATE (NautilusLocationDialog,
		       nautilus_location_dialog,
		       GTK_TYPE_DIALOG)
enum {
	RESPONSE_OPEN
};	

static void
nautilus_location_dialog_finalize (GObject *object)
{
	NautilusLocationDialog *dialog;

	dialog = NAUTILUS_LOCATION_DIALOG (object);

	g_free (dialog->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_location_dialog_destroy (GtkObject *object)
{
	NautilusLocationDialog *dialog;

	dialog = NAUTILUS_LOCATION_DIALOG (object);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
open_current_location (NautilusLocationDialog *dialog)
{
	GFile *location;
	char *user_location;
	
	user_location = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->entry), 0, -1);
	location = g_file_parse_name (user_location);
	nautilus_window_go_to (dialog->details->window, location);
	g_object_unref (location);
	g_free (user_location);
}

static void
response_callback (NautilusLocationDialog *dialog,
		   int response_id,
		   gpointer data)
{
	GError *error;

	switch (response_id) {
	case RESPONSE_OPEN :
		open_current_location (dialog);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_NONE :
	case GTK_RESPONSE_DELETE_EVENT :
	case GTK_RESPONSE_CANCEL :
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_HELP :
		error = NULL;
		gnome_help_display_desktop_on_screen (NULL, "user-guide", "user-guide.xml",
						      "nautilus-open-location",
						      gtk_window_get_screen (GTK_WINDOW (dialog)),
						      &error);
		if (error) {
			eel_show_error_dialog (_("There was an error displaying help."), error->message,
					       GTK_WINDOW (dialog));
			g_error_free (error);
		}
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
entry_activate_callback (GtkEntry *entry,
			 gpointer user_data)
{
	NautilusLocationDialog *dialog;
	
	dialog = NAUTILUS_LOCATION_DIALOG (user_data);
	gtk_dialog_response (GTK_DIALOG (dialog), RESPONSE_OPEN);
}

static void
nautilus_location_dialog_class_init (NautilusLocationDialogClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = nautilus_location_dialog_finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = nautilus_location_dialog_destroy;
}

static void
nautilus_location_dialog_init (NautilusLocationDialog *dialog)
{
	GtkWidget *box;
	GtkWidget *label;
	
	dialog->details = g_new0 (NautilusLocationDialogDetails, 1);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Open Location"));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	
	box = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (box), 5);
	gtk_widget_show (box);

	label = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	
	dialog->details->entry = nautilus_location_entry_new ();
        gtk_entry_set_width_chars (GTK_ENTRY (dialog->details->entry), 30);
	g_signal_connect (dialog->details->entry,
			  "activate", 
			  G_CALLBACK (entry_activate_callback),
			  dialog);
	
	gtk_widget_show (dialog->details->entry);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->entry);

	gtk_box_pack_start (GTK_BOX (box), dialog->details->entry, 
			    TRUE, TRUE, 0);
	
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    box, FALSE, TRUE, 0);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_HELP,
			       GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_OPEN,
			       RESPONSE_OPEN);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 RESPONSE_OPEN);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback),
			  dialog);
}

GtkWidget *
nautilus_location_dialog_new (NautilusWindow *window)
{
	GtkWidget *dialog;
	GFile *location;
	char *formatted_location;
	
	dialog = gtk_widget_new (NAUTILUS_TYPE_LOCATION_DIALOG, NULL);

	if (window) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_window_get_screen (GTK_WINDOW (window)));
		NAUTILUS_LOCATION_DIALOG (dialog)->details->window = window;
	}
	

	location = nautilus_window_get_location (window);
	if (location != NULL) {
		if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
			formatted_location = g_strdup_printf ("%s/", g_get_home_dir ());
		} else {
			formatted_location = g_file_get_parse_name (location);
		}
		nautilus_entry_set_text (NAUTILUS_ENTRY (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), formatted_location);
		gtk_editable_select_region (GTK_EDITABLE (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), 0, -1);
		gtk_editable_set_position (GTK_EDITABLE (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), -1);
		g_free (formatted_location);
		g_object_unref (location);
	}
	
	gtk_widget_grab_focus (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry);

	return dialog;
}

void
nautilus_location_dialog_set_location (NautilusLocationDialog *dialog,
				       const char *location)
{
	nautilus_entry_set_text (NAUTILUS_ENTRY (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), location);
	gtk_editable_select_region (GTK_EDITABLE (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), 0, 0);
	gtk_editable_set_position (GTK_EDITABLE (NAUTILUS_LOCATION_DIALOG (dialog)->details->entry), -1);
}
