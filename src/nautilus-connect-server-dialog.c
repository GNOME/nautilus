/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2003 Red Hat, Inc.
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
#include "nautilus-connect-server-dialog.h"

#include <string.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include "nautilus-location-entry.h"

struct _NautilusConnectServerDialogDetails {
	GtkWidget *name_entry;
	GtkWidget *uri_entry;
};

static void  nautilus_connect_server_dialog_class_init       (NautilusConnectServerDialogClass *class);
static void  nautilus_connect_server_dialog_init             (NautilusConnectServerDialog      *dialog);

EEL_CLASS_BOILERPLATE (NautilusConnectServerDialog,
		       nautilus_connect_server_dialog,
		       GTK_TYPE_DIALOG)
enum {
	RESPONSE_CONNECT
};	

static void
nautilus_connect_server_dialog_finalize (GObject *object)
{
	NautilusConnectServerDialog *dialog;

	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (object);

	g_free (dialog->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_connect_server_dialog_destroy (GtkObject *object)
{
	NautilusConnectServerDialog *dialog;

	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (object);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
connect_to_server (NautilusConnectServerDialog *dialog)
{
	char *uri;
	char *user_uri;
	GnomeVFSURI *vfs_uri;
	char *error_message;
	char *name;
	char *icon;

	name = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->name_entry), 0, -1);
	if (strlen (name) == 0) {
                eel_show_error_dialog (_("You must enter a name for the server."), 
		                       _("Please enter a name and try again."), 
		                       _("Can't Connect to Server"), GTK_WINDOW (dialog));
		g_free (name);
		return;
	}
	
	user_uri = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->uri_entry), 0, -1);
	uri = eel_make_uri_from_input (user_uri);
	g_free (user_uri);
	
	vfs_uri = gnome_vfs_uri_new (uri);
	
	if (vfs_uri == NULL) {
		error_message = g_strdup_printf
			(_("\"%s\" is not a valid location."),
                         uri);
                eel_show_error_dialog (error_message, _("Please check the spelling and try again."), 
		                       _("Can't Connect to Server"), GTK_WINDOW (dialog));
		g_free (error_message);
	} else {
		gnome_vfs_uri_unref (vfs_uri);
		if (g_str_has_prefix (uri, "smb:")) {
			icon = "gnome-fs-smb";
		} else if (g_str_has_prefix (uri, "ssh:") ||
			   g_str_has_prefix (uri, "sftp:")) {
			icon = "gnome-fs-ssh";
		} else if (g_str_has_prefix (uri, "ftp:")) {
			icon = "gnome-fs-ftp";
		} else {
			icon = "gnome-fs-share";
		}

		gnome_vfs_connect_to_server (uri, name, icon);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
		g_free (name);
	g_free (uri);
}

static void
response_callback (NautilusConnectServerDialog *dialog,
		   int response_id,
		   gpointer data)
{
	switch (response_id) {
	case RESPONSE_CONNECT:
		connect_to_server (dialog);
		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
entry_activate_callback (GtkEntry *entry,
			 gpointer user_data)
{
	NautilusConnectServerDialog *dialog;
	
	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (user_data);
	gtk_dialog_response (GTK_DIALOG (dialog), RESPONSE_CONNECT);
}

static void
nautilus_connect_server_dialog_class_init (NautilusConnectServerDialogClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = nautilus_connect_server_dialog_finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = nautilus_connect_server_dialog_destroy;
}

static void
nautilus_connect_server_dialog_init (NautilusConnectServerDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *table;
	char *text;
	
	dialog->details = g_new0 (NautilusConnectServerDialogDetails, 1);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Connect to Server"));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	table = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);
	
	label = gtk_label_new_with_mnemonic (_("Link _name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  0, 1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	dialog->details->name_entry = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->name_entry);
	gtk_widget_show (dialog->details->name_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->name_entry,
			  1, 2,
			  0, 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);

	
	label = gtk_label_new_with_mnemonic (_("_Location (URL):"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  1, 2,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	dialog->details->uri_entry = nautilus_location_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->uri_entry);
	g_signal_connect (dialog->details->uri_entry,
			  "activate", 
			  G_CALLBACK (entry_activate_callback),
			  dialog);
	gtk_widget_show (dialog->details->uri_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->uri_entry,
			  1, 2,
			  1, 2,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);
	text = g_strdup_printf ("<small><i><b>%s </b>ftp://ftp.gnome.org</i></small>", _("Example:"));
	label = gtk_label_new (text);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	g_free (text);
	gtk_table_attach (GTK_TABLE (table), label,
			  1, 2,
			  2, 3,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);
	/*	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
		label, TRUE, TRUE, 0);*/

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("C_onnect"),
			       RESPONSE_CONNECT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 RESPONSE_CONNECT);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback),
			  dialog);
}

GtkWidget *
nautilus_connect_server_dialog_new (NautilusWindow *window)
{
	GtkWidget *dialog;

	dialog = gtk_widget_new (NAUTILUS_TYPE_CONNECT_SERVER_DIALOG, NULL);

	if (window) {
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_window_get_screen (GTK_WINDOW (window)));
	}

	return dialog;
}
