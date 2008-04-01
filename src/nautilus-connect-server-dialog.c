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
#include <libgnomeui/gnome-help.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkliststore.h>
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

/* TODO gio port:
 * - see FIXME here
 * - see FIXME in nautilus-connect-server-dialog-main.c
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
	GtkWidget *domain_entry;
	GtkWidget *user_entry;

	GtkWidget *bookmark_check;
	GtkWidget *name_entry;
};

static void  nautilus_connect_server_dialog_class_init       (NautilusConnectServerDialogClass *class);
static void  nautilus_connect_server_dialog_init             (NautilusConnectServerDialog      *dialog);

EEL_CLASS_BOILERPLATE (NautilusConnectServerDialog,
		       nautilus_connect_server_dialog,
		       GTK_TYPE_DIALOG)
enum {
	RESPONSE_CONNECT
};	

struct MethodInfo {
	const char *scheme;
	guint flags;
};

/* A collection of flags for MethodInfo.flags */
enum {
	DEFAULT_METHOD = 0x00000001,
	
	/* Widgets to display in setup_for_type */
	SHOW_SHARE     = 0x00000010,
	SHOW_PORT      = 0x00000020,
	SHOW_USER      = 0x00000040,
	SHOW_DOMAIN    = 0x00000080,
	
	IS_ANONYMOUS   = 0x00001000
};

/* Remember to fill in descriptions below */
static struct MethodInfo methods[] = {
	/* FIXME: we need to alias ssh to sftp */
	{ "sftp",  SHOW_PORT | SHOW_USER },
	{ "ftp",  SHOW_PORT | SHOW_USER },
	{ "ftp",  DEFAULT_METHOD | IS_ANONYMOUS | SHOW_PORT},
	{ "smb",  SHOW_SHARE | SHOW_USER | SHOW_DOMAIN },
	{ "dav",  SHOW_PORT | SHOW_USER },
	/* FIXME: hrm, shouldn't it work? */
	{ "davs", SHOW_PORT | SHOW_USER },
	{ NULL,   0 }, /* Custom URI method */
};

/* To get around non constant gettext strings */
static const char*
get_method_description (struct MethodInfo *meth)
{
	if (!meth->scheme) {
		return _("Custom Location");
	} else if (strcmp (meth->scheme, "sftp") == 0) {
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
nautilus_connect_server_dialog_finalize (GObject *object)
{
	NautilusConnectServerDialog *dialog;

	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (object);

	g_object_unref (dialog->details->uri_entry);
	g_object_unref (dialog->details->server_entry);
	g_object_unref (dialog->details->share_entry);
	g_object_unref (dialog->details->port_entry);
	g_object_unref (dialog->details->folder_entry);
	g_object_unref (dialog->details->domain_entry);
	g_object_unref (dialog->details->user_entry);
	g_object_unref (dialog->details->bookmark_check);
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
	struct MethodInfo *meth;
	char *uri;
	GFile *location;
	int index;
	GtkTreeIter iter;
	
	/* Get our method info */
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo), &iter);
	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

	if (meth->scheme == NULL) {
		uri = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->uri_entry), 0, -1);
		/* FIXME: we should validate it in some way? */
	} else {
		char *user, *port, *initial_path, *server, *folder ,*domain ;
		char *t, *join;
		gboolean free_initial_path, free_user, free_domain, free_port;

		server = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->server_entry), 0, -1);
		if (strlen (server) == 0) {
			eel_show_error_dialog (_("Cannot Connect to Server. You must enter a name for the server."), 
					       _("Please enter a name and try again."), 
					       GTK_WINDOW (dialog));
			g_free (server);
			return;
		}
		
		user = "";
		port = "";
		initial_path = "";
		domain = "";
		free_initial_path = FALSE;
		free_user = FALSE;
		free_domain = FALSE;
		free_port = FALSE;
		
		/* FTP special case */
		if (meth->flags & IS_ANONYMOUS) {
			user = "anonymous";
		
		/* SMB special case */
		} else if (strcmp (meth->scheme, "smb") == 0) {
			t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->share_entry), 0, -1);
			initial_path = g_strconcat ("/", t, NULL);
			free_initial_path = TRUE;
			g_free (t);
		}

		if (dialog->details->port_entry->parent != NULL) {
			free_port = TRUE;
			port = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->port_entry), 0, -1);
		}
		folder = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->folder_entry), 0, -1);
		if (dialog->details->user_entry->parent != NULL) {
			free_user = TRUE;
			
			t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->user_entry), 0, -1);

			user = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);

			g_free (t);
		}
		if (dialog->details->domain_entry->parent != NULL) {
			free_domain = TRUE;

			domain = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->domain_entry), 0, -1);
			
			if (strlen (domain) != 0) {
				t = user;

				user = g_strconcat (domain , ";" , t, NULL);

				if (free_user) {
					g_free (t);
				}

				free_user = TRUE;
			}
		}

		if (folder[0] != 0 &&
		    folder[0] != '/') {
			join = "/";
		} else {
			join = "";
		}

		t = folder;
		folder = g_strconcat (initial_path, join, t, NULL);
		g_free (t);

		t = folder;
		folder = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
		g_free (t);

		uri = g_strdup_printf ("%s://%s%s%s%s%s%s",
				       meth->scheme,
				       user, (user[0] != 0) ? "@" : "",
				       server,
				       (port[0] != 0) ? ":" : "", port,
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
		if (free_domain) {
			g_free (domain);
		}
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
	
	location = g_file_new_for_uri (uri);
	g_free (uri);

	/* FIXME: sensitivity */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->details->bookmark_check))) {
		char *name;
		NautilusBookmark *bookmark;
		NautilusBookmarkList *list;

		name = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->name_entry), 0, -1);
		bookmark = nautilus_bookmark_new (location, strlen (name) ? name : NULL);
		list = nautilus_bookmark_list_new ();
		if (!nautilus_bookmark_list_contains (list, bookmark)) {
			nautilus_bookmark_list_append (list, bookmark);
		}

		g_object_unref (bookmark);
		g_object_unref (list);
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
		gnome_help_display_desktop_on_screen (NULL, "user-guide", "user-guide.xml",
						      "nautilus-server-connect",
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
	struct MethodInfo *meth;
	int index, i;
	GtkWidget *label, *table;
	GtkTreeIter iter;
	
	/* Get our method info */
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo), &iter);
	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
			    &iter, 0, &index, -1);
	g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
	meth = &(methods[index]);

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
	if (dialog->details->domain_entry->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->domain_entry);
	}
	if (dialog->details->bookmark_check->parent != NULL) {
		gtk_container_remove (GTK_CONTAINER (dialog->details->table),
				      dialog->details->bookmark_check);
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
	
	if (meth->scheme == NULL) {
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
	
	if (meth->flags & SHOW_SHARE) {
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

	if (meth->flags & SHOW_PORT) {
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

	if (meth->flags & SHOW_USER) {
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

	if (meth->flags & SHOW_DOMAIN) {
		label = gtk_label_new_with_mnemonic (_("_Domain Name:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1,
				  i, i+1,
				  GTK_FILL, GTK_FILL,
				  0, 0);

                gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->domain_entry);
                gtk_widget_show (dialog->details->domain_entry);
                gtk_table_attach (GTK_TABLE (table), dialog->details->domain_entry,
                                  1, 2,
                                  i, i+1,
                                  GTK_FILL | GTK_EXPAND, GTK_FILL,
                                  0, 0);

                i++;
        }



 connection_name:
	
	gtk_widget_show (dialog->details->bookmark_check);
	gtk_table_attach (GTK_TABLE (table), dialog->details->bookmark_check,
			  0, 1,
			  i, i+1,
			  GTK_FILL, GTK_FILL,
			  0, 0);
	i++;

	label = gtk_label_new_with_mnemonic (_("Bookmark _name:"));
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
display_server_location (NautilusConnectServerDialog *dialog, GFile *location)
{
#if 0 /*FIXME */
	struct MethodInfo *meth = NULL;
	char *scheme;
	int i, index = 0;
	char *folder;
	const char *t;

	/* Find an appropriate method */
	scheme = g_file_get_uri_scheme (location);
	g_return_if_fail (scheme != NULL);
	
	for (i = 0; i < G_N_ELEMENTS (methods); i++) {
		
		/* The default is 'Custom URI' */
		if (methods[i].scheme == NULL) {
			meth = &(methods[i]);
			index = i;
			
		} else if (strcmp (methods[i].scheme, scheme) == 0) {
			
			/* FTP Special case: If no user keep searching for public ftp */
			if (strcmp (scheme, "ftp") == 0) {
				t = gnome_vfs_uri_get_user_name (uri);
				if ((!t || !t[0] || strcmp (t, "anonymous") == 0) && 
				    (!(methods[i].flags & IS_ANONYMOUS))) {
					continue;
				}
			}
			
			meth = &(methods[i]);
			index = i;
			break;
		}
	}

	g_free (scheme);
	g_assert (meth);
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->details->type_combo), index);
	setup_for_type (dialog);
	
	/* Custom URI */
	if (meth->scheme == NULL) {
		gchar *uri;

		/* FIXME: with gnome-vfs, we had GNOME_VFS_URI_HIDE_PASSWORD |
		 * GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER */
		uri = g_file_get_uri (location)
		gtk_entry_set_text (GTK_ENTRY (dialog->details->uri_entry), uri);
		g_free (uri);
	
	} else {
		
		folder = g_file_get_path (location);
		if (!folder) {
			folder = "";
		} else if (folder[0] == '/') {
			folder++;
		}
		
		/* Server */
		t = gnome_vfs_uri_get_host_name (uri);
		gtk_entry_set_text (GTK_ENTRY (dialog->details->server_entry), 
				    t ? t : "");
		
		/* Share */
		if (meth->flags & SHOW_SHARE) {
			t = strchr (folder, '/');
			if (t) {
				char *share = g_strndup (folder, t - folder);
				gtk_entry_set_text (GTK_ENTRY (dialog->details->share_entry), share);
				g_free (share);
				folder = t + 1;
			}
			
		}

		/* Port */
		if (meth->flags & SHOW_PORT) {
			guint port = gnome_vfs_uri_get_host_port (uri);
			if (port != 0) {
				char sport[32];
				g_snprintf (sport, sizeof (sport), "%d", port);
				gtk_entry_set_text (GTK_ENTRY (dialog->details->port_entry), sport);
			}
		}

		/* Folder */
		gtk_entry_set_text (GTK_ENTRY (dialog->details->folder_entry), folder);
		g_free (folder);

		/* User */
		if (meth->flags & SHOW_USER) {
			const char *user = gnome_vfs_uri_get_user_name (uri);
			if (user) {
				t = strchr (user, ';');
				if (t) {
					user = t + 1;
				}
				gtk_entry_set_text (GTK_ENTRY (dialog->details->user_entry), user);
			}
		}

		/* Domain */
		if (meth->flags & SHOW_DOMAIN) {
			const char *user = gnome_vfs_uri_get_user_name (uri);
			if (user) {
				t = strchr (user, ';');
				if (t) {
					char *domain = g_strndup (user, t - user);
					gtk_entry_set_text (GTK_ENTRY (dialog->details->domain_entry), domain);
					g_free (domain);
				}
			}
		}
	}
#endif
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
	int pos;

	if (new_text_length < 0) {
		new_text_length = strlen (new_text);
	}

	/* Only allow digits to be inserted as port number */
	for (pos = 0; pos < new_text_length; pos++) {
		if (!g_ascii_isdigit (new_text[pos])) {
			GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editable));
			if (toplevel != NULL) {
				gdk_window_beep (toplevel->window);
			}
		    g_signal_stop_emission_by_name (editable, "insert_text");
		    return;
		}
	}
}

static void
bookmark_checkmark_toggled (GtkToggleButton *toggle, NautilusConnectServerDialog *dialog)
{
	gtk_widget_set_sensitive (GTK_WIDGET(dialog->details->name_entry),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (toggle)));
}

static void
nautilus_connect_server_dialog_init (NautilusConnectServerDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *combo;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	int i;
	
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

	dialog->details->type_combo = combo = gtk_combo_box_new ();

	/* each row contains: method index, textual description */
	store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

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
	dialog->details->domain_entry = gtk_entry_new ();
	dialog->details->user_entry = gtk_entry_new ();
	dialog->details->bookmark_check = gtk_check_button_new_with_mnemonic (_("Add _bookmark"));
	dialog->details->name_entry = gtk_entry_new ();

	g_signal_connect (dialog->details->bookmark_check, "toggled", 
			  G_CALLBACK (bookmark_checkmark_toggled), dialog);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->details->bookmark_check), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(dialog->details->name_entry), FALSE);

	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->uri_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->server_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->share_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->port_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->folder_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->domain_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->user_entry), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->name_entry), TRUE);

	/* We need an extra ref so we can remove them from the table */
	g_object_ref (dialog->details->uri_entry);
	g_object_ref (dialog->details->server_entry);
	g_object_ref (dialog->details->share_entry);
	g_object_ref (dialog->details->port_entry);
	g_object_ref (dialog->details->folder_entry);
	g_object_ref (dialog->details->domain_entry);
	g_object_ref (dialog->details->user_entry);
	g_object_ref (dialog->details->bookmark_check);
	g_object_ref (dialog->details->name_entry);
	
	setup_for_type (dialog);
	
        gtk_dialog_add_button (GTK_DIALOG (dialog),
                               GTK_STOCK_HELP,
                               GTK_RESPONSE_HELP);
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
nautilus_connect_server_dialog_new (NautilusWindow *window, GFile *location)
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

	if (location) {
		/* If it's a remote URI, then load as the default */
		if (!g_file_is_native (location)) {
			display_server_location (conndlg, location);
		}
	}

	return dialog;
}
