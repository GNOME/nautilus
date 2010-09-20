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
#include <eel/eel-stock-dialogs.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include "nautilus-location-entry.h"
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-names.h>

/* TODO:
 * - dns-sd fill out servers
 * - pre-fill user?
 * - name entry + pre-fill
 * - folder browse function
 */

/* TODO gio port:
 * - see FIXME here
 * - see FIXME in nautilus-connect-server-dialog-main.c
 */

struct _NautilusConnectServerDialogDetails {
	NautilusApplication *application;

	GtkWidget *user_details;
	GtkWidget *port_spinbutton;

	GtkWidget *table;
	
	GtkWidget *type_combo;
	GtkWidget *server_entry;
	GtkWidget *share_entry;
	GtkWidget *folder_entry;
	GtkWidget *domain_entry;
	GtkWidget *user_entry;

	GtkWidget *bookmark_check;
	GtkWidget *name_entry;
};

G_DEFINE_TYPE (NautilusConnectServerDialog, nautilus_connect_server_dialog,
	       GTK_TYPE_DIALOG)

enum {
	RESPONSE_CONNECT
};	

struct MethodInfo {
	const char *scheme;
	guint flags;
	guint default_port;
};

/* A collection of flags for MethodInfo.flags */
enum {
	DEFAULT_METHOD = (1 << 0),
	
	/* Widgets to display in setup_for_type */
	SHOW_SHARE     = (1 << 1),
	SHOW_PORT      = (1 << 2),
	SHOW_USER      = (1 << 3),
	SHOW_DOMAIN    = (1 << 4),
	
	IS_ANONYMOUS   = (1 << 5)
};

/* Remember to fill in descriptions below */
static struct MethodInfo methods[] = {
	/* FIXME: we need to alias ssh to sftp */
	{ "sftp",  SHOW_PORT | SHOW_USER, 22 },
	{ "ftp",  SHOW_PORT | SHOW_USER, 21 },
	{ "ftp",  DEFAULT_METHOD | IS_ANONYMOUS | SHOW_PORT, 21 },
	{ "smb",  SHOW_SHARE | SHOW_USER | SHOW_DOMAIN, 0 },
	{ "dav",  SHOW_PORT | SHOW_USER, 80 },
	/* FIXME: hrm, shouldn't it work? */
	{ "davs", SHOW_PORT | SHOW_USER, 443 },
};

/* To get around non constant gettext strings */
static const char*
get_method_description (struct MethodInfo *meth)
{
	if (strcmp (meth->scheme, "sftp") == 0) {
		return _("SSH");
	} else if (strcmp (meth->scheme, "ftp") == 0) {
		if (meth->flags & IS_ANONYMOUS) {
			return _("Public FTP");
		} else {
			return _("FTP (with login)");
		}
	} else if (strcmp (meth->scheme, "smb") == 0) {
		return _("Windows share");
	} else if (strcmp (meth->scheme, "dav") == 0) {
		return _("WebDAV (HTTP)");
	} else if (strcmp (meth->scheme, "davs") == 0) {
		return _("Secure WebDAV (HTTPS)");

	/* No descriptive text */
	} else {
		return meth->scheme;
	}
}

static void
connect_to_server (NautilusConnectServerDialog *dialog)
{
	struct MethodInfo *meth;
	GFile *location;
	int index;
	GtkTreeIter iter;
	char *user, *initial_path, *server, *folder, *domain, *port_str;
	char *t, *join, *uri;
	double port;

	/* Get our method info */
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo), &iter);
	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

	server = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->server_entry), 0, -1);

	user = NULL;
	initial_path = NULL;
	domain = NULL;
	folder = NULL;

	/* FTP special case */
	if (meth->flags & IS_ANONYMOUS) {
		user = g_strdup ("anonymous");
		
		/* SMB special case */
	} else if (strcmp (meth->scheme, "smb") == 0) {
		t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->share_entry), 0, -1);
		initial_path = g_strconcat ("/", t, NULL);

		g_free (t);
	}

	/* port */
	port = gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->details->port_spinbutton));

	/* username */
	if (!user) {
		t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->user_entry), 0, -1);
		user = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);
		g_free (t);
	}

	/* domain */
	domain = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->domain_entry), 0, -1);
			
	if (strlen (domain) != 0) {
		t = user;

		user = g_strconcat (domain , ";" , t, NULL);
		g_free (t);
	}

	/* folder */
	folder = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->folder_entry), 0, -1);

	if (folder[0] != 0 &&
	    folder[0] != '/') {
		join = "/";
	} else {
		join = "";
	}

	if (initial_path != NULL) {
		t = folder;
		folder = g_strconcat (initial_path, join, t, NULL);
		g_free (t);
	}

	t = folder;
	folder = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
	g_free (t);

	/* port */
	if (port != 0 && port != meth->default_port) {
		port_str = g_strdup_printf ("%d", (int) port);
	} else {
		port_str = NULL;
	}

	/* final uri */
	uri = g_strdup_printf ("%s://%s%s%s%s%s%s",
			       meth->scheme,
			       (user != NULL) ? user : "",
			       (user[0] != 0) ? "@" : "",
			       server,
			       (port_str != NULL) ? ":" : "",
			       (port_str != NULL) ? port_str : "",
			       (folder != NULL) ? folder : "");

	g_free (initial_path);
	g_free (server);
	g_free (folder);
	g_free (user);
	g_free (domain);
	g_free (port_str);

	gtk_widget_hide (GTK_WIDGET (dialog));

	location = g_file_new_for_uri (uri);
	g_free (uri);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->details->bookmark_check))) {
		char *name;
		NautilusBookmark *bookmark;
		NautilusBookmarkList *list;
		GIcon *icon;

		name = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->name_entry), 0, -1);
		icon = g_themed_icon_new (NAUTILUS_ICON_FOLDER_REMOTE);
		bookmark = nautilus_bookmark_new (location, strlen (name) ? name : NULL,
		                                  TRUE, icon);
		list = nautilus_bookmark_list_new ();
		if (!nautilus_bookmark_list_contains (list, bookmark)) {
			nautilus_bookmark_list_append (list, bookmark);
		}

		g_object_unref (bookmark);
		g_object_unref (list);
		g_object_unref (icon);
		g_free (name);
	}

	nautilus_connect_server_dialog_present_uri (dialog->details->application,
						    location,
						    GTK_WIDGET (dialog));
}

static void
response_callback (NautilusConnectServerDialog *dialog,
		   int response_id,
		   gpointer data)
{
	GError *error;

	switch (response_id) {
	case RESPONSE_CONNECT:
		connect_to_server (dialog);
		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_HELP :
		error = NULL;
		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (dialog)),
			      "ghelp:user-guide#nautilus-server-connect",
			      gtk_get_current_event_time (), &error);
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
nautilus_connect_server_dialog_class_init (NautilusConnectServerDialogClass *class)
{
	g_type_class_add_private (class, sizeof (NautilusConnectServerDialogDetails));
}

static void
setup_for_type (NautilusConnectServerDialog *dialog)
{
	struct MethodInfo *meth;
	int index;;
	GtkTreeIter iter;
	
	/* Get our method info */
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo), &iter);
	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

	g_object_set (dialog->details->share_entry,
		      "visible",
		      (meth->flags & SHOW_SHARE) != 0,
		      NULL);

	g_object_set (dialog->details->port_spinbutton,
		      "sensitive",
		      (meth->flags & SHOW_PORT) != 0,
		      "value", (gdouble) meth->default_port,
		      NULL);

	g_object_set (dialog->details->user_details,
		      "visible",
		      (meth->flags & SHOW_USER) != 0 ||
		      (meth->flags & SHOW_DOMAIN) != 0,
		      NULL);

	g_object_set (dialog->details->user_entry,
		      "visible",
		      (meth->flags & SHOW_USER) != 0,
		      NULL);

	g_object_set (dialog->details->domain_entry,
		      "visible",
		      (meth->flags & SHOW_DOMAIN) != 0,
		      NULL);
}

static void
entry_changed_callback (GtkEditable *editable,
			GtkWidget *connect_button)
{
	guint length;

	length = gtk_entry_get_text_length (GTK_ENTRY (editable));

	gtk_widget_set_sensitive (connect_button,
				  length > 0);
}

static void
bind_visibility (NautilusConnectServerDialog *dialog,
		 GtkWidget *source,
		 GtkWidget *dest)
{
	g_object_bind_property (source,
				"visible",
				dest,
				"visible",
				G_BINDING_DEFAULT);
}

static void
nautilus_connect_server_dialog_init (NautilusConnectServerDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *alignment;
	GtkWidget *content_area;
	GtkWidget *combo ,* table;
	GtkWidget *hbox, *connect_button;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	gchar *str;
	int i;
	
	dialog->details = G_TYPE_INSTANCE_GET_PRIVATE (dialog, NAUTILUS_TYPE_CONNECT_SERVER_DIALOG,
						       NautilusConnectServerDialogDetails);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	/* set dialog properties */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Connect to Server"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_box_set_spacing (GTK_BOX (content_area), 2);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	/* server settings label */
	label = gtk_label_new (NULL);
	str = g_strdup_printf ("<b>%s</b>", _("Server Details"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 6);
	gtk_widget_show (label);

	/* server settings alignment */
	alignment = gtk_alignment_new (0, 0, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (content_area), alignment, TRUE, TRUE, 0);
	gtk_widget_show (alignment);

	table = gtk_table_new (4, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_widget_show (table);

	/* first row: server entry + port spinbutton */
	label = gtk_label_new_with_mnemonic (_("_Server:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
	gtk_widget_show (label);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_table_attach (GTK_TABLE (table), hbox,
			  1, 2,
			  0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	dialog->details->server_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->server_entry), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->server_entry, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->server_entry);
	gtk_widget_show (dialog->details->server_entry);

	/* port */
	label = gtk_label_new_with_mnemonic (_("_Port:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	dialog->details->port_spinbutton =
		gtk_spin_button_new_with_range (0.0, 65535.0, 1.0);
	g_object_set (dialog->details->port_spinbutton,
		      "digits", 0,
		      "numeric", TRUE,
		      "update-policy", GTK_UPDATE_IF_VALID,
		      NULL);
	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->port_spinbutton,
			    FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->port_spinbutton);
	gtk_widget_show (dialog->details->port_spinbutton);

	/* second row: type combobox */
	label = gtk_label_new (_("Type:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
	gtk_widget_show (label);

	dialog->details->type_combo = combo = gtk_combo_box_new ();

	/* each row contains: method index, textual description */
	store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 1);

	for (i = 0; i < G_N_ELEMENTS (methods); i++) {
		GtkTreeIter iter;
		const gchar * const *supported;
		int j;

		/* skip methods that don't have corresponding GnomeVFSMethods */
		supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

		if (methods[i].scheme != NULL) {
			gboolean found;

			found = FALSE;
			for (j = 0; supported[j] != NULL; j++) {
				if (strcmp (methods[i].scheme, supported[j]) == 0) {
					found = TRUE;
					break;
				}
			}

			if (!found) {
				continue;
			}
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, i,
				    1, get_method_description (&(methods[i])),
				    -1);


		if (methods[i].flags & DEFAULT_METHOD) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
		}
	}

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) < 0) {
		/* default method not available, use any other */
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	}

	gtk_widget_show (combo);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
	gtk_table_attach (GTK_TABLE (table), combo,
			  1, 2,
			  1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, 6, 3);
	g_signal_connect_swapped (combo, "changed",
				  G_CALLBACK (setup_for_type),
				  dialog);

	/* third row: share entry */
	label = gtk_label_new (_("Share:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  2, 3,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	dialog->details->share_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->share_entry), TRUE);
	gtk_table_attach (GTK_TABLE (table), dialog->details->share_entry,
			  1, 2,
			  2, 3,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	bind_visibility (dialog, dialog->details->share_entry, label);

	/* fourth row: folder entry */
	label = gtk_label_new (_("Folder:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  3, 4,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
	gtk_widget_show (label);

	dialog->details->folder_entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (dialog->details->folder_entry), "/");
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->folder_entry), TRUE);
	gtk_table_attach (GTK_TABLE (table), dialog->details->folder_entry,
			  1, 2,
			  3, 4,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
	gtk_widget_show (dialog->details->folder_entry);

	/* user details label */
	label = gtk_label_new (NULL);
	str = g_strdup_printf ("<b>%s</b>", _("User Details"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 6);

	/* user details alignment */
	alignment = gtk_alignment_new (0, 0, 0, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (content_area), alignment, TRUE, TRUE, 0);

	bind_visibility (dialog, alignment, label);
	dialog->details->user_details = alignment;

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);
	gtk_widget_show (table);

	/* first row: domain entry */
	label = gtk_label_new (_("Domain Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	dialog->details->domain_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->domain_entry), TRUE);
	gtk_table_attach (GTK_TABLE (table), dialog->details->domain_entry,
			  1, 2,
			  0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	bind_visibility (dialog, dialog->details->domain_entry, label);

	/* second row: username entry */
	label = gtk_label_new (_("User Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	dialog->details->user_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->user_entry), TRUE);
	gtk_table_attach (GTK_TABLE (table), dialog->details->user_entry,
			  1, 2,
			  1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	bind_visibility (dialog, dialog->details->user_entry, label);

	/* add as bookmark */
	dialog->details->bookmark_check = gtk_check_button_new_with_mnemonic (_("Add _bookmark"));
	gtk_box_pack_start (GTK_BOX (content_area), dialog->details->bookmark_check, TRUE, TRUE, 0);
	gtk_widget_show (dialog->details->bookmark_check);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);

	label = gtk_label_new (_("Bookmark Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	dialog->details->name_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->name_entry, TRUE, TRUE, 0);

	gtk_widget_show_all (hbox);

	g_object_bind_property (dialog->details->bookmark_check, "active",
				dialog->details->name_entry, "sensitive",
				G_BINDING_DEFAULT |
				G_BINDING_SYNC_CREATE);
	setup_for_type (dialog);

        gtk_dialog_add_button (GTK_DIALOG (dialog),
                               GTK_STOCK_HELP,
                               GTK_RESPONSE_HELP);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	connect_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
						_("C_onnect"),
						RESPONSE_CONNECT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 RESPONSE_CONNECT);

	g_signal_connect (dialog->details->server_entry, "changed",
			  G_CALLBACK (entry_changed_callback),
			  connect_button);
	entry_changed_callback (GTK_EDITABLE (dialog->details->server_entry),
				connect_button);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (response_callback),
			  dialog);
}

GtkWidget *
nautilus_connect_server_dialog_new (NautilusWindow *window)
{
	NautilusConnectServerDialog *conndlg;
	GtkWidget *dialog;

	dialog = gtk_widget_new (NAUTILUS_TYPE_CONNECT_SERVER_DIALOG, NULL);
	conndlg = NAUTILUS_CONNECT_SERVER_DIALOG (dialog);

	if (window) {
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_window_get_screen (GTK_WINDOW (window)));
		conndlg->details->application = window->application;
	}

	return dialog;
}
