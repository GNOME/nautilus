/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*- */

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
#include <libeazelinstall.h>
#include "libtrilobite/libtrilobite.h"
#include "libtrilobite/libammonite-gtk.h"

#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-viewport.h>
#include <libnautilus-extensions/nautilus-preferences.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

/* for uname */
#include <sys/utsname.h>

#define PASSWORD_PROMPT		_("Please enter your root password to continue installing. (This password is not saved or transmitted outside of your system.)")

static void       nautilus_service_install_view_initialize_class (NautilusServiceInstallViewClass	*klass);
static void       nautilus_service_install_view_initialize       (NautilusServiceInstallView		*view);
static void       service_install_load_location_callback         (NautilusView				*nautilus_view,
								  const char				*location,
								  NautilusServiceInstallView		*view);
static void	  service_install_stop_loading_callback		 (NautilusView				*nautilus_view,
								  NautilusServiceInstallView		*view);
    
NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusServiceInstallView, nautilus_service_install_view, GTK_TYPE_EVENT_BOX)


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
	CORBA_Environment ev;

	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);
	if (view->details->preflight_status == PREFLIGHT_UNKNOWN) {
		view->details->preflight_status = PREFLIGHT_PANIC_BUTTON;
	}

	CORBA_exception_init (&ev);
	if (view->details->installer != NULL) {
		GNOME_Trilobite_Eazel_Install_stop (eazel_install_callback_corba_objref (view->details->installer), &ev);
	}
	CORBA_exception_free (&ev);

	if (view->details->root_client != NULL) {
		trilobite_root_client_unref (GTK_OBJECT (view->details->root_client));
	}

	if (view->details->installer != NULL) {
		/* this will unref the installer too, which will indirectly cause any ongoing download to abort */
		eazel_install_callback_unref (GTK_OBJECT (view->details->installer));
	}

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_service_install_view_finalize (GtkObject *object)
{
	NautilusServiceInstallView *view;

	view = NAUTILUS_SERVICE_INSTALL_VIEW (object);

	g_free (view->details->uri);
	g_free (view->details->current_rpm);
	g_free (view->details->remembered_password);
	g_hash_table_foreach_remove (view->details->deps, (GHRFunc)deps_destroy_foreach, NULL);
	g_hash_table_destroy (view->details->deps);
	g_list_foreach (view->details->message, (GFunc)install_message_destroy, NULL);
	g_list_free (view->details->message);
	g_free (view->details->username);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, finalize, (object));
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
	object_class->finalize = nautilus_service_install_view_finalize;
}

static void
nautilus_service_install_view_initialize (NautilusServiceInstallView *view)
{

	NautilusBackground *background;

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
	} else if (strncmp (name, "product_id%3D", 13) == 0) {
		pack->suite_id = g_strdup_printf ("P:%s", name+13);
	} else if (strncmp (name, "product_id=", 11) == 0) {
		pack->suite_id = g_strdup_printf ("P:%s", name+11);
	} else if (strncmp (name, "suite_id%3D", 11) == 0) {
		pack->suite_id = g_strdup_printf ("S:%s", name+11);
	} else if (strncmp (name, "suite_id=", 9) == 0) {
		pack->suite_id = g_strdup_printf ("S:%s", name+9);
	} else if (strncmp (name, "product_name%3D", 15) == 0) {
		pack->suite_id = g_strdup_printf ("N:%s", name+15);
	} else if (strncmp (name, "product_name=", 13) == 0) {
		pack->suite_id = g_strdup_printf ("N:%s", name+13);
	} else if (strncmp (name, "suite_name%3D", 13) == 0) {
		pack->suite_id = g_strdup_printf ("X:%s", name+13);
	} else if (strncmp (name, "suite_name=", 11) == 0) {
		pack->suite_id = g_strdup_printf ("X:%s", name+11);
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
	char *p, *q, *pnext, *package_name, *host_spec, *rest, *ptr;
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
			g_free (*username);
			*username = host_spec;
			if (*(p+1)) {
				g_free (*host);
				*host = g_strdup (p+1);
				result = TRUE;
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
		rest = g_strdup (p);
		ptr = rest;
		do {
			pnext = strchr (ptr, ';');
			if ((pnext != NULL) && (*(pnext+1) != '\0')) {
				another_package = TRUE;
				*pnext++ = '\0';
			} else {
				another_package = FALSE;
			}

			trilobite_debug ("package '%s'", ptr);
			/* version name specified? */
			q = strchr (ptr, '?');
			if (q) {
				*q++ = 0;
				if (strncmp (q, "version=", 8) == 0) {
					q += 8;
				}
			}

			package_name = gnome_vfs_unescape_string_for_display (ptr);
			pack = create_package (package_name, view->details->using_local_file);
			if (q) {
				pack->version = g_strdup (q);
			}
			packages = g_list_prepend (packages, pack);
			g_free (package_name);

			if (pnext != NULL) {
				ptr = pnext;
			}
		} while (another_package);
		g_free (rest);
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

void
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
					(_(" \xB7 %s is in the GNOME footprint menu under %s"),
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
char *
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
		message = g_strdup_printf ("%s\n \n%s", PASSWORD_PROMPT, _("Incorrect password."));
	} else {
		message = g_strdup (PASSWORD_PROMPT);
	}

	dialog = nautilus_password_dialog_new (_("Authenticate as root"), message, prompt, "", TRUE);
	okay = nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (dialog));

	if (! okay) {
		/* cancel */
		view->details->password_attempts = 0;
		view->details->cancelled = TRUE;
		out = g_strdup ("");
	} else {
		out = nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (dialog));
		if (nautilus_password_dialog_get_remember (NAUTILUS_PASSWORD_DIALOG (dialog))) {
			view->details->remembered_password = g_strdup (out);
		}
	}

	gtk_widget_destroy (dialog);

	if (okay) {
		view->details->password_attempts++;
	}

	g_free (message);

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
nautilus_service_install_view_update_from_uri_finish (NautilusServiceInstallView *view, const char *uri)
{
	PackageData		*pack;
	CategoryData		*category_data;
	char			*host;
	int			port;
	GNOME_Trilobite_Eazel_Install	service;
	CORBA_Environment	ev;
	char 			*out, *p;
	gboolean                set_auth;

	/* get default host/port */
	host = g_strdup (trilobite_get_services_address ());
	p = strchr (host, ':');
	if (p != NULL) {
		*p = 0;
		port = atoi (p+1);
	} else {
		port = 80;
	}

	set_auth = !(nautilus_install_parse_uri (uri, view, &host, &port, &view->details->username));

	if (! view->details->categories) {
		return;
	}

	/* NOTE: This adds a libeazelinstall packagedata object to the view */
	pack = (PackageData*) gtk_object_get_data (GTK_OBJECT (view), "packagedata");
	if (pack != NULL) {
		/* Destroy the old */
		gtk_object_unref (GTK_OBJECT (pack));
	}

	/* find the package data for the package we're about to install */
	category_data = (CategoryData *) view->details->categories->data;
	pack = (PackageData *) category_data->packages->data;

	gtk_object_set_data (GTK_OBJECT (view), "packagedata", pack);

	if (g_list_length (category_data->packages) > 1) {
		out = g_strdup_printf (_("Installing packages"));
	} else if ((pack->eazel_id != NULL) || (pack->suite_id != NULL)) {
		out = g_strdup_printf (_("Installing remote package"));
	} else if (pack->name != NULL) {
		out = g_strdup_printf (_("Installing \"%s\""), pack->name);
	} else {
		out = g_strdup_printf (_("Installing some package"));
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

	if (view->details->username != NULL) {
		GNOME_Trilobite_Eazel_Install__set_username (service, view->details->username, &ev);
	}
	GNOME_Trilobite_Eazel_Install__set_test_mode (service, FALSE, &ev);

	gtk_signal_connect (GTK_OBJECT (view->details->installer), "dependency_check",
			    nautilus_service_install_dependency_check, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "file_conflict_check",
			    GTK_SIGNAL_FUNC (nautilus_service_install_conflict_check), view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "preflight_check",
			    GTK_SIGNAL_FUNC (nautilus_service_install_preflight_check), view);
        gtk_signal_connect (GTK_OBJECT (view->details->installer), "save_transaction", 
                            GTK_SIGNAL_FUNC (nautilus_service_install_save_transaction), view);        
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_progress",
			    GTK_SIGNAL_FUNC (nautilus_service_install_download_progress), view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "download_failed",
			    GTK_SIGNAL_FUNC (nautilus_service_install_download_failed), view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_progress",
			    nautilus_service_install_progress, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "install_failed",
			    nautilus_service_install_failed, view);
	gtk_signal_connect (GTK_OBJECT (view->details->installer), "done",
			    nautilus_service_install_done, view);
	eazel_install_callback_install_packages (view->details->installer, 
						 view->details->categories, 
						 NULL, &ev);

	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Contacting the Eazel Software Catalog ..."));

	/* might take a while (leave the throbber on) */
}

static void /* AmmonitePromptLoginCb */
user_login_callback (
	gpointer user_data, 
	const EazelProxy_User *user, 
	const EazelProxy_AuthnFailInfo *fail_info,
	AmmoniteDialogButton button_pressed)
{
	NautilusServiceInstallView *view;

	view = NAUTILUS_SERVICE_INSTALL_VIEW (user_data);

	/* if the view has been destroyed while the callback was gone, just drop everything */
	if (!GTK_OBJECT_DESTROYED (GTK_OBJECT(view))) {
		if (fail_info == NULL) {
			/* login succeeded */
			nautilus_service_install_view_update_from_uri_finish (view, view->details->uri);
		} else {
			if (button_pressed == AMMONITE_BUTTON_REGISTER) {
				nautilus_view_open_location_in_this_window (
					view->details->nautilus_view, 
					EAZEL_ACCOUNT_REGISTER_URI);
			} else if (button_pressed == AMMONITE_BUTTON_FORGOT) {
				nautilus_view_open_location_in_this_window (
					view->details->nautilus_view, 
					EAZEL_ACCOUNT_FORGOTPW_URI);
			} else {
				nautilus_view_open_location_in_this_window (
					view->details->nautilus_view, 
					NEXT_URL_ANONYMOUS);
			}
		}
	}

	gtk_object_unref (GTK_OBJECT(view));
}

static void
nautilus_service_install_view_update_from_uri (NautilusServiceInstallView *view, const char *uri)
{
	char *host;
	int port;

	host = NULL;

	nautilus_install_parse_uri (uri, view, &host, &port, &view->details->username);

	if (host == NULL) {
		/* Ensure that the user is logged in.  Note that the "anonymous" user
		 * will not be prompted
		 */

		gtk_object_ref (GTK_OBJECT(view));

		show_overall_feedback (view, _("Checking for authorization..."));

		/* Cancel a pending login request, if there was one...*/
		ammonite_prompt_login_async_cancel (user_login_callback);
		ammonite_do_prompt_login_async (view->details->username, NULL, NULL, view->details->username == NULL ? TRUE: FALSE, view, user_login_callback);
	} else {
		nautilus_service_install_view_update_from_uri_finish (view, uri);
	}

	g_free (host);
	host = NULL;
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
	view->details->failures = 0;
	view->details->downloaded_anything = FALSE;

	generate_install_form (view);

	nautilus_view_report_load_underway (NAUTILUS_VIEW (view->details->nautilus_view));

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
	GNOME_Trilobite_Eazel_Install service;
	CORBA_Environment ev;
	int i;

	view->details->cancelled = TRUE;
	view->details->preflight_status = PREFLIGHT_CANCEL;
	show_overall_feedback (view, _("Aborting package downloads (please wait) ..."));
	/* on a fast download, the GUI could get stuck here, constantly updating the download progress */
	for (i = 0; i < 10; i++) {
		if (gtk_events_pending ()) {
			gtk_main_iteration ();
		} else {
			i = 11;
		}
	}
	/* have to set these up here, because if they hit STOP before any downloads have started, the
	 * call to _stop below will freeze until we get the preflight signal later.
	 */

	g_assert (nautilus_view == view->details->nautilus_view);

	CORBA_exception_init (&ev);
	service = eazel_install_callback_corba_objref (view->details->installer);
	GNOME_Trilobite_Eazel_Install_stop (service, &ev);
	CORBA_exception_free (&ev);

	show_overall_feedback (view, _("Package download aborted."));
	current_progress_bar_complete (view, _("Aborted"));
}
