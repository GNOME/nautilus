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
#include <gtk/gtkcombobox.h>
#include "nautilus-location-entry.h"
#include <libnautilus-private/nautilus-global-preferences.h>

/* TODO:
 * - dns-sd fill out servers
 * - pre-fill user?
 * - name entry + pre-fill
 * - folder browse function
 */

struct _NautilusConnectServerDialogDetails {
	NautilusApplication *application;
	
	GtkWidget *table;
	
	GtkWidget *type_combo;
	GtkWidget *uri_entry;
	GtkWidget *server_entry;
	GtkWidget *share_entry;
	GtkWidget *port_entry;
	GtkWidget *folder_entry;
	GtkWidget *user_entry;

	GtkWidget *name_entry;
};

static void  nautilus_connect_server_dialog_class_init       (NautilusConnectServerDialogClass *class);
static void  nautilus_connect_server_dialog_init             (NautilusConnectServerDialog      *dialog);

EEL_CLASS_BOILERPLATE (NautilusConnectServerDialog,
		       nautilus_connect_server_dialog,
		       GTK_TYPE_DIALOG)
enum {
	RESPONSE_BROWSE,
	RESPONSE_CONNECT
};	

/* Keep this order in sync with strings below */
enum {
	TYPE_SSH,
	TYPE_ANON_FTP,
	TYPE_FTP,
	TYPE_SMB,
	TYPE_DAV,
	TYPE_DAVS,
	TYPE_URI
};

static void
nautilus_connect_server_dialog_finalize (GObject *object)
{
	NautilusConnectServerDialog *dialog;

	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (object);

	g_object_unref (dialog->details->uri_entry);
	g_object_unref (dialog->details->server_entry);
	g_object_unref (dialog->details->share_entry);
	g_object_unref (dialog->details->port_entry);
	g_object_unref (dialog->details->folder_entry);
	g_object_unref (dialog->details->user_entry);
	g_object_unref (dialog->details->name_entry);
	
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
	int type;
	
	type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->details->type_combo));

	if (type == TYPE_URI) {
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
			return;
		} else {
			gnome_vfs_uri_unref (vfs_uri);
		}
	} else {
		char *method, *user, *port, *initial_path, *server, *folder;
		char *t;
		gboolean free_initial_path, free_user, free_port;

		server = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->server_entry), 0, -1);
		if (strlen (server) == 0) {
			eel_show_error_dialog (_("You must enter a name for the server."), 
					       _("Please enter a name and try again."), 
					       _("Can't Connect to Server"), GTK_WINDOW (dialog));
			g_free (server);
			return;
		}
		
		method = "";
		user = "";
		port = "";
		initial_path = "";
		free_initial_path = FALSE;
		free_user = FALSE;
		free_port = FALSE;
		switch (type) {
		case TYPE_SSH:
			method = "sftp";
			break;
		case TYPE_ANON_FTP:
			method = "ftp";
			user = "anonymous";
			break;
		case TYPE_FTP:
			method = "ftp";
			break;
		case TYPE_SMB:
			method = "smb";
			t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->share_entry), 0, -1);
			initial_path = g_strconcat ("/", t, NULL);
			free_initial_path = TRUE;
			g_free (t);
			break;
		case TYPE_DAV:
			method = "dav";
			break;
		case TYPE_DAVS:
			method = "davs";
			break;
		}

		if (dialog->details->port_entry->parent != NULL) {
			free_port = TRUE;
			port = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->port_entry), 0, -1);
		}
		folder = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->folder_entry), 0, -1);
		if (dialog->details->user_entry->parent != NULL) {
			free_user = TRUE;
			user = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->user_entry), 0, -1);
		}

		if (folder[0] != 0 &&
		    folder[0] != '/') {
			t = folder;
			folder = g_strconcat ("/", t, NULL);
			g_free (t);
		}
		
		uri = g_strdup_printf ("%s://%s%s%s%s%s%s%s",
				       method,
				       user, (user[0] != 0) ? "@" : "",
				       server,
				       (port[0] != 0) ? ":" : "", port,
				       initial_path,
				       folder);
		if (free_initial_path) {
			g_free (initial_path);
		}
		g_free (server);
		if (free_port) {
			g_free (port);
		}
		g_free (folder);
		if (free_user) {
			g_free (user);
		}
	}
	
	name = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->name_entry), 0, -1);
	if (strlen (name) == 0) {
		const char *host, *path;
		char *path_utf8, *basename;
		
		g_free (name);
		
		vfs_uri = gnome_vfs_uri_new (uri);
	
		if (vfs_uri == NULL) {
			g_warning ("Illegal uri in connect to server!\n");
			g_free (uri);
			g_free (name);
			return;
		} 

		host = gnome_vfs_uri_get_host_name (vfs_uri);
		path = gnome_vfs_uri_get_path (vfs_uri);
		if (path != NULL &&
		    strlen (path) > 0 &&
		    strcmp (path, "/") != 0) {
			path_utf8 = eel_format_uri_for_display (uri);
			basename = g_path_get_basename (path_utf8);
			name = g_strdup_printf (_("%s on %s"), basename, host);
			g_free (path_utf8);
			g_free (basename);
		} else {
			name = g_strdup (host);
		}
		gnome_vfs_uri_unref (vfs_uri);
	}
		
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


	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
		nautilus_application_present_spatial_window (dialog->details->application,
							     NULL,
							     uri,
							     gtk_widget_get_screen (GTK_WIDGET (dialog)));
	}

	g_free (uri);
	g_free (name);
}

static void
response_callback (NautilusConnectServerDialog *dialog,
		   int response_id,
		   gpointer data)
{
	NautilusWindow *window;
	
	switch (response_id) {
	case RESPONSE_BROWSE:
		if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
			window = nautilus_application_create_navigation_window (dialog->details->application,
										gtk_widget_get_screen (GTK_WIDGET (dialog)));
			nautilus_window_go_to (window, "network:///");
		} else {
			nautilus_application_present_spatial_window (dialog->details->application,
								     NULL,
								     "network:///",
								     gtk_widget_get_screen (GTK_WIDGET (dialog)));
		}
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
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
setup_for_type (NautilusConnectServerDialog *dialog)
{
	int type, i;
	gboolean show_share, show_port, show_user;
	GtkWidget *label, *table;

	type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->details->type_combo));

	if (dialog->details->uri_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->uri_entry);
	}
	if (dialog->details->server_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->server_entry);
	}
	if (dialog->details->share_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->share_entry);
	}
	if (dialog->details->port_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->port_entry);
	}
	if (dialog->details->folder_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->folder_entry);
	}
	if (dialog->details->user_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->user_entry);
	}
	if (dialog->details->name_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->name_entry);
	}
	/* Destroy all labels */
	gtk_container_foreach (GTK_CONTAINER (dialog->details->table),
			       (GtkCallback) gtk_widget_destroy, NULL);

	
	i = 1;
	table = dialog->details->table;
	
	if (type == TYPE_URI) {
		label = gtk_label_new_with_mnemonic (_("_Location (URI):"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->uri_entry);
		gtk_widget_show (dialog->details->uri_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->uri_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
		
		goto connection_name;
	}
	
	switch (type) {
	default:
	case TYPE_SSH:
	case TYPE_FTP:
	case TYPE_DAV:
	case TYPE_DAVS:
		show_share = FALSE;
		show_port = TRUE;
		show_user = TRUE;
		break;
	case TYPE_ANON_FTP:
		show_share = FALSE;
		show_port = TRUE;
		show_user = FALSE;
		break;
	case TYPE_SMB:
		show_share = TRUE;
		show_port = FALSE;
		show_user = TRUE;
		break;
	}

	label = gtk_label_new_with_mnemonic (_("_Server:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->server_entry);
	gtk_widget_show (dialog->details->server_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->server_entry,
			  1, 2,
			  i, i+1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);

	i++;

	label = gtk_label_new (_("Optional information:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 2,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);

	i++;
	
	if (show_share) {
		label = gtk_label_new_with_mnemonic (_("_Share:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->share_entry);
		gtk_widget_show (dialog->details->share_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->share_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}

	if (show_port) {
		label = gtk_label_new_with_mnemonic (_("_Port:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->port_entry);
		gtk_widget_show (dialog->details->port_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->port_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}

	label = gtk_label_new_with_mnemonic (_("_Folder:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->folder_entry);
	gtk_widget_show (dialog->details->folder_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->folder_entry,
			  1, 2,
			  i, i+1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);

	i++;

	if (show_user) {
		label = gtk_label_new_with_mnemonic (_("_User Name:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->user_entry);
		gtk_widget_show (dialog->details->user_entry);
		gtk_table_attach (GTK_TABLE (table), dialog->details->user_entry,
				  1, 2,
				  i, i+1,
				  GTK_FILL | GTK_EXPAND, GTK_FILL,
				  0, 0);

		i++;
	}


 connection_name:
	
	label = gtk_label_new_with_mnemonic (_("_Name to use for connection:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->name_entry);
	gtk_widget_show (dialog->details->name_entry);
	gtk_table_attach (GTK_TABLE (table), dialog->details->name_entry,
			  1, 2,
			  i, i+1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL,
			  0, 0);
	
	i++;

}

static void
combo_changed_callback (GtkComboBox *combo_box,
			NautilusConnectServerDialog *dialog)
{
	setup_for_type (dialog);
}


static void
port_insert_text (GtkEditable *editable,
		  const gchar *new_text,
		  gint         new_text_length,
		  gint        *position)
{
	if (new_text_length < 0) {
		new_text_length = strlen (new_text);
	}

	if (new_text_length != 1 ||
	    !g_ascii_isdigit (new_text[0])) {
		gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (editable)));
		g_signal_stop_emission_by_name (editable, "insert_text");
	}
}


static void
nautilus_connect_server_dialog_init (NautilusConnectServerDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *combo;
	GtkWidget *hbox;
	GtkWidget *vbox;
	
	dialog->details = g_new0 (NautilusConnectServerDialogDetails, 1);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Connect to Server"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    vbox, FALSE, TRUE, 0);
	gtk_widget_show (vbox);
			    
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);
	
	label = gtk_label_new_with_mnemonic (_("Service _type:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox),
			    label, FALSE, FALSE, 0);

	dialog->details->type_combo = combo = gtk_combo_box_new_text ();
	/* Keep this in sync with enum */
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("SSH"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Public FTP"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("FTP (with login)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Windows share"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("WebDAV (HTTP)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Secure WebDAV (HTTPS)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
				   _("Custom Location"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), TYPE_ANON_FTP);
	gtk_widget_show (combo);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
	gtk_box_pack_start (GTK_BOX (hbox),
			    combo, TRUE, TRUE, 0);
	g_signal_connect (combo, "changed",
			  G_CALLBACK (combo_changed_callback),
			  dialog);
	

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox),
			    hbox, FALSE, TRUE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new_with_mnemonic ("    ");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox),
			    label, FALSE, FALSE, 0);
	
	
	dialog->details->table = table = gtk_table_new (5, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (hbox),
			    table, TRUE, TRUE, 0);

	dialog->details->uri_entry = nautilus_location_entry_new ();
	dialog->details->server_entry = gtk_entry_new ();
	dialog->details->share_entry = gtk_entry_new ();
	dialog->details->port_entry = gtk_entry_new ();
	g_signal_connect (dialog->details->port_entry, "insert_text", G_CALLBACK (port_insert_text),
			  NULL);
	dialog->details->folder_entry = gtk_entry_new ();
	dialog->details->user_entry = gtk_entry_new ();
	dialog->details->name_entry = gtk_entry_new ();
	/* We need an extra ref so we can remove them from the table */
	g_object_ref (dialog->details->uri_entry);
	g_object_ref (dialog->details->server_entry);
	g_object_ref (dialog->details->share_entry);
	g_object_ref (dialog->details->port_entry);
	g_object_ref (dialog->details->folder_entry);
	g_object_ref (dialog->details->user_entry);
	g_object_ref (dialog->details->name_entry);
	
	setup_for_type (dialog);
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Browse _Network"),
			       RESPONSE_BROWSE);
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

	NAUTILUS_CONNECT_SERVER_DIALOG (dialog)->details->application = window->application;

	return dialog;
}
