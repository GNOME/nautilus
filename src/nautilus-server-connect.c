/* Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <glib.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/gnome-icon-theme.h>

#undef DEBUG
#ifdef DEBUG
#define D(x) g_message x
#else
#define D(x) 
#endif

#define NETWORK_USER_DIR "/.gnome2/vfolders/network/"
#define ICON_SIZE_STANDARD 48

static GladeXML *xml = NULL;
static GtkWidget *toplevel = NULL;
static GtkWidget *entry = NULL;
static GtkWidget *image = NULL;
static const char *naut_icon = NULL;

static void
error (char *msg)
{
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (NULL,
			0,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			msg,
			NULL);
	gtk_dialog_run (GTK_DIALOG (dialog));
}

static GnomeVFSResult
gnome_vfs_make_directory_with_parents_for_uri (GnomeVFSURI * uri,
					       guint perm)
{
	GnomeVFSResult result;
	GnomeVFSURI *parent, *work_uri;
	GList *list = NULL;

	result = gnome_vfs_make_directory_for_uri (uri, perm);
	if (result == GNOME_VFS_OK || result != GNOME_VFS_ERROR_NOT_FOUND)
		return result;

	work_uri = uri;

	while (result == GNOME_VFS_ERROR_NOT_FOUND) {
		parent = gnome_vfs_uri_get_parent (work_uri);
		D(("trying to create: %s", gnome_vfs_uri_to_string (parent, 0)));
		result = gnome_vfs_make_directory_for_uri (parent, perm);

		if (result == GNOME_VFS_ERROR_NOT_FOUND)
			list = g_list_prepend (list, parent);
		work_uri = parent;
	}

	if (result != GNOME_VFS_OK) {
		/* Clean up */
		while (list != NULL) {
			gnome_vfs_uri_unref ((GnomeVFSURI *) list->data);
			list = g_list_remove (list, list->data);
		}
	}

	while (result == GNOME_VFS_OK && list != NULL) {
		D(("creating: %s", gnome_vfs_uri_to_string (list->data, 0)));
		result = gnome_vfs_make_directory_for_uri
		    ((GnomeVFSURI *) list->data, perm);

		gnome_vfs_uri_unref ((GnomeVFSURI *) list->data);
		list = g_list_remove (list, list->data);
	}

	result = gnome_vfs_make_directory_for_uri (uri, perm);
	return result;
}

static GnomeVFSResult
gnome_vfs_make_directory_with_parents (const gchar * text_uri, guint perm)
{
	GnomeVFSURI *uri;
	GnomeVFSResult result;

	D(("gnome_vfs_make_directory_with_parents (%s)", text_uri));
	uri = gnome_vfs_uri_new (text_uri);
	result = gnome_vfs_make_directory_with_parents_for_uri (uri, perm);
	D(("gnome_vfs_make_directory_with_parents: %s\n",
			gnome_vfs_result_to_string (result)));
	gnome_vfs_uri_unref (uri);

	return result;
}

static void
browse (char *uri)
{
	char *argv[3] = {"nautilus", NULL, NULL};

	argv[1] = uri;

	D (("browse (%s)", uri));
	if (gnome_execute_async (g_get_home_dir (), 2, argv) < 0)
	{
		error (_("Couldn't execute nautilus\nMake sure nautilus is in your path and correctly installed"));
	}
}

static gboolean
already_linked (char *uri)
{
	GDir *dir;
	char *path;
	const char *files;
	gboolean found = FALSE;

	path = g_strconcat (g_get_home_dir (), NETWORK_USER_DIR, NULL);
	dir = g_dir_open (path, 0, NULL);

	D (("already_linked: opened %s", path));

	if (dir == NULL)
	{
		g_free (path);
		return FALSE;
	}

	files = g_dir_read_name (dir);
	/* No files in the directory ? */
	if (files == NULL)
	{
		g_dir_close (dir);
		g_free (path);
		return FALSE;
	}

	for (files = g_dir_read_name (dir); files != NULL && found != TRUE;
			files = g_dir_read_name (dir))
	{
		GnomeDesktopItem *di;
		char *long_path;
		const char *target;

		long_path = g_strconcat (path, files, NULL);
		D (("already_linked: opening desktop %s", long_path));

		di = gnome_desktop_item_new_from_file (long_path,
				GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
				NULL);
		if (gnome_desktop_item_get_entry_type (di) !=
				GNOME_DESKTOP_ITEM_TYPE_LINK)
		{
			D (("already_linked: %s not a link", long_path));
			g_free (long_path);
			gnome_desktop_item_unref (di);
			continue;
		}

		target = gnome_desktop_item_get_string (di,
				GNOME_DESKTOP_ITEM_URL);
		if (strncmp (target, uri, MIN (strlen (target), strlen (uri))) == 0)
		{
			D (("%s (%d) and %s (%d) matched (on %d chars)",
					target, strlen (target),
					uri, strlen (uri),
					MIN (strlen (target), strlen (uri))));
			found = TRUE;
		}

		g_free (long_path);
		gnome_desktop_item_unref (di);
	}

	g_dir_close (dir);
	g_free (path);
	D (("already_linked: returning %s", found ? "TRUE" : "FALSE"));
	return found;
}

static gboolean
create_desktop (char *uri)
{
	char *path, *prefix;
	int i;
	gboolean created = FALSE;

	if (already_linked (uri) == TRUE)
		return created;

	prefix = g_strdup (g_path_get_basename (uri));
	D (("create_desktop: basename prefix %s", prefix));
	if (prefix == NULL)
	{
		D (("create_desktop: dirname prefix %s", prefix));
		prefix = g_path_get_dirname (uri);
	}

	if (prefix == NULL)
		return created;

	i = -1;
	while (created == FALSE)
	{
		GnomeDesktopItem *di;

		/* i == 0 on the first time it's called */
		i++;

		path = g_strdup_printf ("network:///%s.%d.desktop", prefix, i);
		D (("create_desktop: trying %s", path));
		if (g_file_test (path, G_FILE_TEST_EXISTS) == TRUE)
		{
			D (("create_desktop: %s exists", path));
			g_free (path);
			continue;
		}

		di = gnome_desktop_item_new ();
		if (di == NULL)
		{
			D (("create_desktop: couldn't create an di"));
			g_free (path);
			continue;
		}

		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_VERSION,
				"1.0");
		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_ENCODING,
				"UTF-8");
		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_NAME,
				prefix);
		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_URL, uri);
		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_TYPE,
				"Link");
		gnome_desktop_item_set_string (di, GNOME_DESKTOP_ITEM_ICON,
				naut_icon);

		created = gnome_desktop_item_save (di, path, TRUE, NULL);
#ifdef DEBUG
		if (created == TRUE)
			g_message ("create_desktop: created %s", path);
		else
			g_message ("create_desktop: couldn't create %s", path);
#endif
		g_free (path);
	}

	g_free (prefix);

	return created;
}

static gboolean
can_connect (const char *uri)
{
	GnomeVFSDirectoryHandle *handle;
	GnomeVFSResult result;

	D (("can_connect (%s)", uri));

	result = gnome_vfs_directory_open (&handle, uri,
			GNOME_VFS_FILE_INFO_DEFAULT);
	D (("can_connect: %s (%d)", gnome_vfs_result_to_string (result),
	    result));

	if (result == GNOME_VFS_OK)
	{
		gnome_vfs_directory_close (handle);
		return TRUE;
	}

	return FALSE;
}

static void
update_icon (GtkEntry *entry, gpointer user_data)
{
	GnomeIconTheme *theme;
	GtkWidget *button;
	char *uri_utf8, *uri, *filename;
	const GnomeIconData *icon_data;
	int base_size;

	uri_utf8 = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (uri_utf8 == NULL)
		uri = NULL;
	else
		uri = g_filename_from_utf8 (uri_utf8, -1, NULL, NULL, NULL);
	g_free (uri_utf8);

	button = glade_xml_get_widget (xml, "button3");

	if (uri == NULL || strcmp (uri, "") == 0)
	{
		gtk_widget_set_sensitive (button, FALSE);
		naut_icon = "gnome-fs-share";
	} else {
		gtk_widget_set_sensitive (button, TRUE);
		if (g_str_has_prefix (uri, "smb:")
				|| (g_strrstr (uri, ":") == NULL)) {
			naut_icon = "gnome-fs-smb";
		} else if (g_str_has_prefix (uri, "ftp:")) {
			naut_icon = "gnome-fs-ftp";
		} else if (g_str_has_prefix (uri, "http:")) {
			naut_icon = "gnome-fs-web";
		} else if (g_str_has_prefix (uri, "ssh:")) {
			naut_icon = "gnome-fs-ssh";
		} else {
			naut_icon = "gnome-fs-share";
		}
	}

	theme = gnome_icon_theme_new ();
	filename = gnome_icon_theme_lookup_icon (theme, naut_icon,
			ICON_SIZE_STANDARD,
			&icon_data,
			&base_size);

	gtk_image_set_from_file (GTK_IMAGE (image), filename);
	g_free (filename);
	g_free (uri);
}

static void
button_clicked (GtkWidget *widget, int response, gpointer data)
{
	char *uri_utf8, *uri;

	gtk_widget_hide (toplevel);
	while (gtk_events_pending())
		gtk_main_iteration();

	D (("button_clicked: %d", response));

	if (response == GTK_RESPONSE_CANCEL
			|| response == GTK_RESPONSE_DELETE_EVENT)
		exit (0);

	uri_utf8 = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	uri = g_filename_from_utf8 (uri_utf8, -1, NULL, NULL, NULL);
	g_free (uri_utf8);

	D (("uri: %s", uri));

	if (uri == NULL || strcmp (uri, "") == 0)
		exit (0);

	/* non-uri-like addresses are considered either workgroups
	 * or computers on an smb network */
	if (g_strrstr (uri, ":") == NULL)
	{
		char *new_uri;

		new_uri = g_strdup_printf ("smb://%s", uri);
		g_free (uri);
		uri = new_uri;
	}

	/* We create the .desktop file even if we can't connect to the URI */
	create_desktop (uri);

	if (can_connect(uri) == FALSE)
	{
		char *msg;

		D (("couldn't connect to %s", uri));

		msg = g_strdup_printf (_("Couldn't connect to URI %s\n"
					"Please make sure that the address is "
					"correct and alternatively, type in "
					"this address in the file manager "
					"directly"), uri);
		error (msg);
		g_free (msg);
	} else {
		browse(uri);
	}

	exit (0);
}

static void
create_user_network_dir (void)
{
	char *path;

	gnome_vfs_init ();
	path = g_strconcat ("file://", g_get_home_dir (),
			NETWORK_USER_DIR, NULL);
	gnome_vfs_make_directory_with_parents (path,
			GNOME_VFS_PERM_USER_ALL
			| GNOME_VFS_PERM_GROUP_ALL
			| GNOME_VFS_PERM_OTHER_READ);
	g_free (path);
}

int
main (int argc, char *argv[])
{
	GnomeClient *client;
	gchar *window_icon;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("nautilus-server-connect", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PROGRAM_STANDARD_PROPERTIES,
			NULL);

	glade_gnome_init();
	client = gnome_master_client ();
	gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);

	if (g_file_test ("nautilus-server-connect.glade",
				G_FILE_TEST_EXISTS) == TRUE) {
		xml = glade_xml_new ("nautilus-server-connect.glade", NULL, NULL);
	}
	if (xml == NULL) {
		xml = glade_xml_new (GLADEDIR "/nautilus-server-connect.glade",
				     NULL, NULL);
	}
	if (xml == NULL) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new
			(NULL /* parent */,
			 0 /* flags */,
			 GTK_MESSAGE_ERROR,
			 GTK_BUTTONS_OK,
			 _("Glade file for the connect to server program is"
			   " missing.\nPlease check your installation of "
			   "nautilus"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		exit (1);
	}

	create_user_network_dir ();

	toplevel = glade_xml_get_widget (xml, "dialog1");
	image = glade_xml_get_widget (xml, "image1");
	entry = glade_xml_get_widget (xml, "entry");
	update_icon (GTK_ENTRY (entry), NULL);

	window_icon = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_PIXMAP, 
			"nautilus/nautilus-server-connect.png", FALSE, NULL);

	if (window_icon) {
		gnome_window_icon_set_from_file (GTK_WINDOW (toplevel),
				window_icon);
		g_free (window_icon);
	}

	gtk_widget_grab_focus (entry);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (button_clicked),
			  NULL);
	g_signal_connect (G_OBJECT (toplevel), "response",
			  G_CALLBACK (button_clicked),
			  NULL);
	g_signal_connect (G_OBJECT (toplevel), "close",
			  G_CALLBACK (exit),
			  0);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (update_icon),
			  NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (toplevel),
			GTK_RESPONSE_OK);

	gtk_widget_show_now (toplevel);

	gtk_main ();

	return 0;
}
