/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>,
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#include <config.h>
#include "nautilus-application.h"

#include "file-manager/fm-desktop-icon-view.h"
#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-list-view.h"
#include "file-manager/fm-search-list-view.h"
#include "nautilus-desktop-window.h"
#include "nautilus-first-time-druid.h"
#include "nautilus-main.h"
#include "nautilus-shell-interface.h"
#include "nautilus-shell.h"
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <dirent.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-metadata.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-metafile-factory.h>
#include <libnautilus-private/nautilus-sound.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-private/nautilus-volume-monitor.h>
#include <libnautilus-private/nautilus-authn-manager.h>
#include <liboaf/liboaf.h>

/* Needed for the is_kdesktop_present check */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#define FACTORY_IID	     "OAFIID:nautilus_factory:bd1e1862-92d7-4391-963e-37583f0daef3"
#define SEARCH_LIST_VIEW_IID "OAFIID:nautilus_file_manager_search_list_view:b186e381-198e-43cf-9c46-60b6bb35db0b"
#define SHELL_IID	     "OAFIID:nautilus_shell:cd5183b2-3913-4b74-9b8e-10528b0de08d"

/* Keeps track of the one and only desktop window. */
static NautilusDesktopWindow *nautilus_application_desktop_window;

/* Keeps track of all the nautilus windows. */
static GList *nautilus_application_window_list;

static CORBA_boolean manufactures                          (PortableServer_Servant    servant,
							    const CORBA_char         *iid,
							    CORBA_Environment        *ev);
static CORBA_Object  create_object                         (PortableServer_Servant    servant,
							    const CORBA_char         *iid,
							    const GNOME_stringlist   *params,
							    CORBA_Environment        *ev);
static void          nautilus_application_initialize       (NautilusApplication      *application);
static void          nautilus_application_initialize_class (NautilusApplicationClass *klass);
static void          nautilus_application_destroy          (GtkObject                *object);
static gboolean      confirm_ok_to_run_as_root             (void);
static gboolean      need_to_show_first_time_druid         (void);
static void          desktop_changed_callback              (gpointer                  user_data);
static void          desktop_location_changed_callback     (gpointer                  user_data);
static void          volume_mounted_callback               (NautilusVolumeMonitor    *monitor,
							    NautilusVolume           *volume,
							    NautilusApplication      *application);
static void          volume_unmounted_callback             (NautilusVolumeMonitor    *monitor,
							    NautilusVolume           *volume,
							    NautilusApplication      *application);
static void	     update_session			    (gpointer		      callback_data);
static void	     init_session 			    (void);
static gboolean      is_kdesktop_present                    (void);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusApplication, nautilus_application, BONOBO_OBJECT_TYPE)

static POA_GNOME_ObjectFactory__epv factory_epv = {
	NULL,
	&manufactures,
	&create_object
};
static PortableServer_ServantBase__epv base_epv;
static POA_GNOME_ObjectFactory__vepv vepv = {
	&base_epv,
	&factory_epv
};

static CORBA_boolean
manufactures (PortableServer_Servant servant,
	      const CORBA_char *iid,
	      CORBA_Environment *ev)
{
	return strcmp (iid, NAUTILUS_ICON_VIEW_IID) == 0
		|| strcmp (iid, NAUTILUS_DESKTOP_ICON_VIEW_IID) == 0
		|| strcmp (iid, NAUTILUS_LIST_VIEW_IID) == 0
		|| strcmp (iid, SEARCH_LIST_VIEW_IID) == 0
		|| strcmp (iid, SHELL_IID) == 0
		|| strcmp (iid, METAFILE_FACTORY_IID) == 0;
}

static CORBA_Object
create_object (PortableServer_Servant servant,
	       const CORBA_char *iid,
	       const GNOME_stringlist *params,
	       CORBA_Environment *ev)
{
	BonoboObject *object;
	FMDirectoryView *directory_view;
	NautilusApplication *application;

	if (strcmp (iid, NAUTILUS_ICON_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_icon_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, NAUTILUS_DESKTOP_ICON_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_desktop_icon_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, NAUTILUS_LIST_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_list_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, SEARCH_LIST_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_search_list_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, SHELL_IID) == 0) {
		application = NAUTILUS_APPLICATION (((BonoboObjectServant *) servant)->bonobo_object);
		object = BONOBO_OBJECT (nautilus_shell_new (application));
	} else if (strcmp (iid, METAFILE_FACTORY_IID) == 0) {
		object = BONOBO_OBJECT (nautilus_metafile_factory_get_instance ());
	} else {
		return CORBA_OBJECT_NIL;
	}

	return CORBA_Object_duplicate (bonobo_object_corba_objref (object), ev);
}

static CORBA_Object
create_factory (PortableServer_POA poa,
		NautilusApplication *bonobo_object,
		CORBA_Environment *ev)
{
	BonoboObjectServant *servant;

	servant = g_new0 (BonoboObjectServant, 1);
	((POA_GNOME_ObjectFactory *) servant)->vepv = &vepv;
	POA_GNOME_ObjectFactory__init ((PortableServer_Servant) servant, ev);
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}

GList *
nautilus_application_get_window_list (void)
{
	return nautilus_application_window_list;
}

static void
nautilus_application_initialize_class (NautilusApplicationClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = nautilus_application_destroy;
}

static void
nautilus_application_initialize (NautilusApplication *application)
{
	CORBA_Environment ev;
	CORBA_Object corba_object;

	CORBA_exception_init (&ev);
	corba_object = create_factory (bonobo_poa (), application, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_error ("could not create factory");
	}
	CORBA_exception_free (&ev);
	
	bonobo_object_construct (BONOBO_OBJECT (application), corba_object);
	
	/* Create an undo manager */
	application->undo_manager = nautilus_undo_manager_new ();

	/* Watch for volume mounts so we can restore open windows */
	gtk_signal_connect (GTK_OBJECT (nautilus_volume_monitor_get ()),
			    "volume_mounted",
			    volume_mounted_callback,
			    application);

	/* Watch for volume unmounts so we can close open windows */
	gtk_signal_connect (GTK_OBJECT (nautilus_volume_monitor_get ()),
			    "volume_unmounted",
			    volume_unmounted_callback,
			    application);
}

NautilusApplication *
nautilus_application_new (void)
{
	return NAUTILUS_APPLICATION (gtk_object_new (nautilus_application_get_type (), NULL));
}

static void
nautilus_application_destroy (GtkObject *object)
{
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (object);

	nautilus_bookmarks_exiting ();
	
	bonobo_object_unref (BONOBO_OBJECT (application->undo_manager));

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static gboolean
check_required_directories (NautilusApplication *application)
{
	char *user_directory;
	char *desktop_directory;
	EelStringList *directories;
	char *directories_as_string;
	char *error_string;
	char *dialog_title;
	GnomeDialog *dialog;
	int failed_count;
	
	g_assert (NAUTILUS_IS_APPLICATION (application));

	user_directory = nautilus_get_user_directory ();
	desktop_directory = nautilus_get_desktop_directory ();

	directories = eel_string_list_new (TRUE);
	
	if (!g_file_test (user_directory, G_FILE_TEST_ISDIR)) {
		eel_string_list_insert (directories, user_directory);
	}
	g_free (user_directory);	    
	    
	if (!g_file_test (desktop_directory, G_FILE_TEST_ISDIR)) {
		eel_string_list_insert (directories, desktop_directory);
	}
	g_free (desktop_directory);

	failed_count = eel_string_list_get_length (directories);

	if (failed_count != 0) {
		directories_as_string = eel_string_list_as_string (directories, "\n", EEL_STRING_LIST_ALL_STRINGS);

		if (failed_count == 1) {
			dialog_title = g_strdup (_("Couldn't Create Required Folder"));
			error_string = g_strdup_printf (_("Nautilus could not create the required folder \"%s\". "
							  "Before running Nautilus, please create this folder, or "
							  "set permissions such that Nautilus can create it."),
							directories_as_string);
		} else {
			dialog_title = g_strdup (_("Couldn't Create Required Folders"));
			error_string = g_strdup_printf (_("Nautilus could not create the following required folders:\n\n"
							  "%s\n\n"
							  "Before running Nautilus, please create these folders, or "
							  "set permissions such that Nautilus can create them."),
							directories_as_string);
		}
		
		dialog = eel_show_error_dialog (error_string, dialog_title, NULL);
		/* We need the main event loop so the user has a chance to see the dialog. */
		nautilus_main_event_loop_register (GTK_OBJECT (dialog));

		g_free (directories_as_string);
		g_free (error_string);
		g_free (dialog_title);
	}

	eel_string_list_free (directories);

	return failed_count == 0;
}

static int
nautilus_strv_length (const char * const *strv)
{
	const char * const *p;

	for (p = strv; *p != NULL; p++) { }
	return p - strv;
}

static Nautilus_URIList *
nautilus_make_uri_list_from_shell_strv (const char * const *strv)
{
	int length, i;
	Nautilus_URIList *uri_list;
	char *translated_uri;

	length = nautilus_strv_length (strv);

	uri_list = Nautilus_URIList__alloc ();
	uri_list->_maximum = length;
	uri_list->_length = length;
	uri_list->_buffer = CORBA_sequence_Nautilus_URI_allocbuf (length);
	for (i = 0; i < length; i++) {
		translated_uri = eel_make_uri_from_shell_arg (strv[i]);
		uri_list->_buffer[i] = CORBA_string_dup (translated_uri);
		g_free (translated_uri);
		translated_uri = NULL;
	}
	CORBA_sequence_set_release (uri_list, CORBA_TRUE);

	return uri_list;
}

/* Find ~/.gnome-desktop/Trash and rename it to ~/.gnome-desktop/Trash.gmc
 * Only if it is a directory
 */
static void
migrate_gmc_trash (void)
{
	char *dp, *trash_dir, *dest;
	struct stat buf;

	dp = nautilus_get_desktop_directory ();
	trash_dir = g_strconcat (dp, "/", "Trash", NULL);
	dest = g_strconcat (dp, "/", "Trash.gmc", NULL);
	
	if (stat (trash_dir, &buf) == 0 && S_ISDIR (buf.st_mode)) {
		rename (trash_dir, dest);
		gnome_metadata_rename (trash_dir, dest);
	}
	
	g_free (dp);
	g_free (trash_dir);
	g_free (dest);
}

static void
migrate_old_nautilus_files (void)
{
	char *new_desktop_dir, *np;
	char *old_desktop_dir, *op;
	char *old_desktop_dir_new_name;
	struct stat buf;
	DIR *dir;
	struct dirent *de;
	
	old_desktop_dir = g_strconcat (g_get_home_dir (), "/.nautilus/desktop", NULL);
	if (stat (old_desktop_dir, &buf) == -1) {
		g_free (old_desktop_dir);
		return;
	}
	if (!S_ISLNK (buf.st_mode)){
		dir = opendir (old_desktop_dir);
		if (dir == NULL) {
			g_free (old_desktop_dir);
			return;
		}
	
		new_desktop_dir = nautilus_get_desktop_directory ();
		
		while ((de = readdir (dir)) != NULL){
			if (de->d_name [0] == '.'){
				if (de->d_name [0] == 0)
					continue;
				
				if (de->d_name [1] == '.' && de->d_name [2] == 0)
					continue;
			}
	
			op = g_strconcat (old_desktop_dir, "/", de->d_name, NULL);
			np = g_strconcat (new_desktop_dir, "/", de->d_name, NULL);
	
			rename (op, np);
	
			g_free (op);
			g_free (np);
		}

		closedir (dir);

		g_free (new_desktop_dir);
	}

	/* In case we miss something */
	old_desktop_dir_new_name = g_strconcat (old_desktop_dir, "-old", NULL);
	rename (old_desktop_dir, old_desktop_dir_new_name);
	g_free (old_desktop_dir_new_name);

	g_free (old_desktop_dir);
}

static gint
create_starthere_link_callback (gpointer data)
{
	char *desktop_path;
	char *desktop_link_file;
	char *cmd;
	
	/* Create default services icon on the desktop */
	desktop_path = nautilus_get_desktop_directory ();
	desktop_link_file = nautilus_make_path (desktop_path,
						"starthere.desktop");

	cmd = g_strconcat ("/bin/cp ",
			   NAUTILUS_DATADIR,
			   "/starthere-link.desktop ",
			   desktop_link_file,
			   NULL);

	if (system (cmd) != 0) {
		g_warning ("Failed to execute command '%s'\n", cmd);
	}
	
	g_free (desktop_path);
	g_free (desktop_link_file);
	g_free (cmd);
	
	return FALSE;
}

static void
finish_startup (NautilusApplication *application)
{
	/* initialize the sound machinery */
	nautilus_sound_initialize ();

	/* initialize URI authentication manager */
	nautilus_authentication_manager_initialize ();

	/* Make the desktop work with gmc and old Nautilus. */
	migrate_gmc_trash ();
	migrate_old_nautilus_files ();
}

void
nautilus_application_startup (NautilusApplication *application,
			      gboolean kill_shell,
			      gboolean restart_shell,
			      gboolean no_default_window,
			      gboolean no_desktop,
			      gboolean do_first_time_druid_check,
			      const char *geometry,
			      const char *urls[])
{
	CORBA_Environment ev;
	Nautilus_Shell shell;
	OAF_RegistrationResult result;
	const char *message, *detailed_message;
	GnomeDialog *dialog;
	Nautilus_URIList *url_list;
	const CORBA_char *corba_geometry;
	int num_failures;

	num_failures = 0;

	/* Perform check for nautilus being run as super user */
	if (!(kill_shell || restart_shell)) {
		if (!confirm_ok_to_run_as_root ()) {
			return;
		}
	}

	/* Check the user's ~/.nautilus directories and post warnings
	 * if there are problems.
	 */
	if (!kill_shell && !check_required_directories (application)) {
		return;
	}

	/* Run the first time startup druid if needed. */
	if (do_first_time_druid_check && need_to_show_first_time_druid ()) {
		/* Do this at idle time, once nautilus has initialized
		 * itself. Otherwise we may spawn a second nautilus
		 * process when looking for a metadata factory..
		 */
		g_idle_add (create_starthere_link_callback, NULL);
		nautilus_set_first_time_file_flag ();
	}

	CORBA_exception_init (&ev);

	/* Start up the factory. */
	while (TRUE) {
		/* Try to register the file manager view factory with OAF. */
		result = oaf_active_server_register
			(FACTORY_IID,
			 bonobo_object_corba_objref (BONOBO_OBJECT (application)));
		switch (result) {
		case OAF_REG_SUCCESS:
			/* We are registered with OAF and all is right with the world. */
			finish_startup (application);
		case OAF_REG_ALREADY_ACTIVE:
			/* Another copy of nautilus already is running and registered. */
			message = NULL;
			detailed_message = NULL;
			break;
		case OAF_REG_NOT_LISTED:
			/* Can't register myself due to trouble locating the
			 * Nautilus_Shell.oaf file. This has happened when you
			 * launch Nautilus with an LD_LIBRARY_PATH that
			 * doesn't include the directory containg the oaf
			 * library. It could also happen if the
			 * Nautilus_Shell.oaf file was not present for some
			 * reason. Sometimes killing oafd and gconfd fixes
			 * this problem but we don't exactly understand why,
			 * since neither of the above causes explain it.
			 */
			message = _("Nautilus can't be used now. "
				    "Running the command \"nautilus-clean.sh -x\""
				    " from the console may fix the problem. If not,"
				    " you can try rebooting the computer or"
				    " installing Nautilus again.");
			/* FIXME bugzilla.gnome.org 42536: The guesses and stuff here are lame. */
			detailed_message = _("Nautilus can't be used now. "
					     "Running the command \"nautilus-clean.sh -x\""
					     " from the console may fix the problem. If not,"
					     " you can try rebooting the computer or"
					     " installing Nautilus again.\n\n"
					     "OAF couldn't locate the Nautilus_shell.oaf file. "
					     "One cause of this seems to be an LD_LIBRARY_PATH "
					     "that does not include the oaf library's directory. "
					     "Another possible cause would be bad install "
					     "with a missing Nautilus_Shell.oaf file.\n\n"
					     "Running \"nautilus-clean.sh -x\" will kill all "
					     "OAF and GConf processes, which may be needed by "
					     "other applications.\n\n"
					     "Sometimes killing oafd and gconfd fixes "
					     "the problem, but we don't know why.\n\n"
					     "We have also seen this error when a faulty "
					     "version of oaf was installed.");
			break;
		default:
			/* This should never happen. */
			g_warning ("bad error code from oaf_active_server_register");
		case OAF_REG_ERROR:
			/* Some misc. error (can never happen with current
			 * version of OAF). Show dialog and terminate the
			 * program.
			 */
			/* FIXME bugzilla.gnome.org 42537: Looks like this does happen with the
			 * current OAF. I guess I read the code
			 * wrong. Need to figure out when and make a
			 * good message.
			 */
			message = _("Nautilus can't be used now, due to an unexpected error.");
			detailed_message = _("Nautilus can't be used now, due to an unexpected error "
					     "from OAF when attempting to register the file manager view server.");
			break;
		}

		/* Get the shell object. */
		if (message == NULL) {
			shell = oaf_activate_from_id (SHELL_IID, 0, NULL, NULL);
			if (!CORBA_Object_is_nil (shell, &ev)) {
				break;
			}

			/* If we couldn't find ourselves it's a bad problem so
			 * we better stop looping.
			 */
			if (result == OAF_REG_SUCCESS) {
				/* FIXME bugzilla.gnome.org 42538: When can this happen? */
				message = _("Nautilus can't be used now, due to an unexpected error.");
				detailed_message = _("Nautilus can't be used now, due to an unexpected error "
						     "from OAF when attempting to locate the factory."
						     "Killing oafd and restarting Nautilus may help fix the problem.");
			} else {
				num_failures++;
				if (num_failures > 20) {
					message = _("Nautilus can't be used now, due to an unexpected error.");
					detailed_message = _("Nautilus can't be used now, due to an unexpected error "
							     "from OAF when attempting to locate the shell object. "
							     "Killing oafd and restarting Nautilus may help fix the problem.");
					
				}
			}
		}

		if (message != NULL) {
			dialog = eel_show_error_dialog_with_details (message, NULL, detailed_message, NULL);
			/* We need the main event loop so the user has a chance to see the dialog. */
			nautilus_main_event_loop_register (GTK_OBJECT (dialog));
			goto out;
		}
	}

	if (kill_shell) {
		Nautilus_Shell_quit (shell, &ev);
	} else if (restart_shell) {
		Nautilus_Shell_restart (shell, &ev);
	} else {
		/* If KDE desktop is running, then force no_desktop */
		if (is_kdesktop_present ())
			no_desktop = TRUE;
		
		if (!no_desktop && eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
			Nautilus_Shell_start_desktop (shell, &ev);
		}
		
		/* Monitor the preference to show or hide the desktop */
		eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_SHOW_DESKTOP,
							       desktop_changed_callback,
							       application,
							       GTK_OBJECT (application));

		/* Monitor the preference to have the desktop */
		/* point to the Unix home folder */
		eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
							       desktop_location_changed_callback,
							       NULL,
							       GTK_OBJECT (application));

		/* CORBA C mapping doesn't allow NULL to be passed
		   for string parameters */
		corba_geometry = (geometry != NULL) ? geometry : "";

	  	/* Create the other windows. */
		if (urls != NULL) {
			url_list = nautilus_make_uri_list_from_shell_strv (urls);
			Nautilus_Shell_open_windows (shell, url_list, corba_geometry, &ev);
			CORBA_free (url_list);
		} else if (!no_default_window) {
			Nautilus_Shell_open_default_window (shell, corba_geometry, &ev);
		}
		
		/* Add ourselves to the session */
		init_session ();
	}

	/* We're done with the shell now, so let it go. */
	/* HACK: Don't bother releasing the shell in the case where we
	 * just told it to quit -- that just leads to hangs and does
	 * no good. We could probably fix this in some fancier way if
	 * we could figure out a better lifetime rule.
	 */
	if (!(kill_shell || restart_shell)) {
		bonobo_object_release_unref (shell, NULL);
	}

 out:
	CORBA_exception_free (&ev);
}

static void
nautilus_application_create_desktop_window (NautilusApplication *application)
{
	g_return_if_fail (nautilus_application_desktop_window == NULL);
	g_return_if_fail (NAUTILUS_IS_APPLICATION (application));

	nautilus_application_desktop_window = nautilus_desktop_window_new (application);
	gtk_widget_show (GTK_WIDGET (nautilus_application_desktop_window));
}

void
nautilus_application_open_desktop (NautilusApplication *application)
{
	if (nautilus_application_desktop_window == NULL) {
		nautilus_application_create_desktop_window (application);
	}
}

void
nautilus_application_close_desktop (void)
{
	if (nautilus_application_desktop_window != NULL) {
		gtk_widget_destroy (GTK_WIDGET (nautilus_application_desktop_window));
		nautilus_application_desktop_window = NULL;
	}
}

void
nautilus_application_close_all_windows (void)
{
	while (nautilus_application_window_list != NULL) {
		nautilus_window_close (NAUTILUS_WINDOW (nautilus_application_window_list->data));
	}
}

static void
nautilus_application_destroyed_window (GtkObject *object, NautilusApplication *application)
{
	nautilus_application_window_list = g_list_remove (nautilus_application_window_list, object);
}

static gboolean
nautilus_window_delete_event_callback (GtkWidget *widget,
				       GdkEvent *event,
				       gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (widget);
	nautilus_window_close (window);

	return TRUE;
}				       

NautilusWindow *
nautilus_application_create_window (NautilusApplication *application)
{
	NautilusWindow *window;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);
	
	window = NAUTILUS_WINDOW (gtk_widget_new (nautilus_window_get_type (),
						  "app", GTK_OBJECT (application),
						  "app_id", "nautilus", NULL));
	
	gtk_signal_connect (GTK_OBJECT (window), 
			    "delete_event", GTK_SIGNAL_FUNC (nautilus_window_delete_event_callback),
                    	    NULL);

	gtk_signal_connect (GTK_OBJECT (window),
			    "destroy", nautilus_application_destroyed_window,
			    application);

	nautilus_application_window_list = g_list_prepend (nautilus_application_window_list, window);

	/* Do not yet show the window. It will be shown later on if it can
	 * successfully display its initial URI. Otherwise it will be destroyed
	 * without ever having seen the light of day.
	 */

	return window;
}

/*
 * confirm_ok_to_run_as_root:
 *
 * Puts out a warning if the user is running nautilus as root.
 */
static gboolean
confirm_ok_to_run_as_root (void)
{
	GtkWidget *dialog;
	int result;

	if (geteuid () != 0) {
		return TRUE;
	}

	if (g_getenv ("NAUTILUS_OK_TO_RUN_AS_ROOT") != NULL) {
		return TRUE;
	}

	dialog = gnome_message_box_new
		(_("You are about to run Nautilus as root.\n\n"
		   "As root, you can damage your system if you are not careful, and\n"
		   "Nautilus will not stop you from doing it."),
		 GNOME_MESSAGE_BOX_WARNING,
		 GNOME_STOCK_BUTTON_OK, _("Quit"), NULL);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	
	return result == 0;
}

/* callback for changing the directory the desktop points to */
static void
desktop_location_changed_callback (gpointer user_data)
{
	if (nautilus_application_desktop_window != NULL) {
		nautilus_desktop_window_update_directory
			(nautilus_application_desktop_window);
	}
}

/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
	NautilusApplication *application;
	
	application = NAUTILUS_APPLICATION (user_data);
	if ( eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
		nautilus_application_open_desktop (application);
	} else {
		nautilus_application_close_desktop ();
	}

	/* Can't make this function just watch the preference
	 * itself changing since ordering is important
	 */
	update_session (gnome_master_client ());
}

/*
 * need_to_show_first_time_druid
 *
 * Determine whether Nautilus needs to show the first time druid.
 * 
 * Note that the flag file indicating whether the druid has been
 * presented is: ~/.nautilus/first-time-flag.
 *
 * Another alternative could be to use preferences to store this flag
 * However, there because of bug 1229 this is not yet possible.
 *
 * Also, for debugging purposes, it is convenient to have just one file
 * to kill in order to test the startup druid:
 *
 * rm -f ~/.nautilus/first-time-flag
 *
 * In order to accomplish the same thing with preferences, you would have
 * to either kill ALL your preferences or spend time digging in ~/.gconf
 * xml files finding the right one.
 */
static gboolean
need_to_show_first_time_druid (void)
{
	gboolean result;
	char *user_directory;
	char *druid_flag_file_name;
	
	user_directory = nautilus_get_user_directory ();

	druid_flag_file_name = g_strconcat (user_directory, "/first-time-flag", NULL);
	result = !g_file_exists (druid_flag_file_name);	
	g_free (druid_flag_file_name);

	/* we changed the name of the flag for version 1.0, so we should
	 * check for and delete the old one, if the new one didn't exist 
	 */
	if (result) {
		druid_flag_file_name = g_strconcat (user_directory, "/first-time-wizard-flag", NULL);
		unlink (druid_flag_file_name);
		g_free (druid_flag_file_name);
	}
	g_free (user_directory); 
	return result;
}

static void
volume_mounted_callback (NautilusVolumeMonitor *monitor, NautilusVolume *volume,
			 NautilusApplication *application)
{
	NautilusWindow *window;
	char *uri;
	
	if (volume == NULL || application == NULL) {
		return;
	}
	
	/* Open a window to the CD if the user has set that preference. */
	if (nautilus_volume_get_device_type (volume) == NAUTILUS_DEVICE_CDROM_DRIVE
		&& gnome_config_get_bool ("/magicdev/Options/do_fileman_window=true")) {		
		window = nautilus_application_create_window (application);
		uri = gnome_vfs_get_uri_from_local_path (nautilus_volume_get_mount_path (volume));
		nautilus_window_go_to (window, uri);
		g_free (uri);
	}
}

static gboolean
window_can_be_closed (NautilusWindow *window)
{
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
is_last_closable_window (NautilusWindow *window)
{
	GList *node, *window_list;
	
	window_list = nautilus_application_get_window_list ();
	
	for (node = window_list; node != NULL; node = node->next) {
		if (window != NAUTILUS_WINDOW (node->data) && window_can_be_closed (NAUTILUS_WINDOW (node->data))) {
			return FALSE;
		}
	}
	
	return TRUE;
}


/* Called whenever a volume is unmounted. Check and see if there are any windows open
 * displaying contents on the volume. If there are, close them.
 * It would also be cool to save open window and position info.
 */
static void
volume_unmounted_callback (NautilusVolumeMonitor *monitor, NautilusVolume *volume,
			   NautilusApplication *application)
{
	GList *window_list, *node, *close_list;
	NautilusWindow *window;
	char *uri;
	char *path;
		
	close_list = NULL;
	
	/* Check and see if any of the open windows are displaying contents from the unmounted volume */
	window_list = nautilus_application_get_window_list ();
	
	/* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) {
		window = NAUTILUS_WINDOW (node->data);
		if (window != NULL && window_can_be_closed (window)) {
			uri = nautilus_window_get_location (window);
			path = gnome_vfs_get_local_path_from_uri (uri);
			if (eel_str_has_prefix (path, nautilus_volume_get_mount_path (volume))) {
				close_list = g_list_prepend (close_list, window);
			}
			g_free (path);
			g_free (uri);
		}
	}
		
	/* Handle the windows in the close list. */
	for (node = close_list; node != NULL; node = node->next) {
		window = NAUTILUS_WINDOW (node->data);
		if (is_last_closable_window (window)) {
			/* Don't close the last or only window. Try to redirect to the default home directory. */		 	
			nautilus_window_go_home (window);
		} else {
			nautilus_window_close (window);
		}
	}
		
	g_list_free (close_list);
}


static void
removed_from_session (GnomeClient *client, gpointer data)
{
	nautilus_main_event_loop_quit ();
}

static gint
save_session (GnomeClient *client, gint phase, GnomeSaveStyle save_style, gint shutdown,
	      GnomeInteractStyle interact_style, gint fast, gpointer data)
{
	return TRUE;
}

static void
set_session_restart (GnomeClient *client, gboolean restart)
{
	static char *restart_argv[] = { "nautilus", "--no-default-window", 0 };

	gnome_client_set_restart_command (client, 2, restart_argv);
	gnome_client_set_priority (client, 40);

	if (restart && g_getenv ("NAUTILUS_DEBUG") == NULL) {
		/* Don't respawn in debug mode */
		gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
	} else {
		gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);
	}
}

static void
update_session (gpointer callback_data)
{
	set_session_restart (callback_data,
			     eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ADD_TO_SESSION)
			     /* Only ever add ourselves to the session
			      * if we have a desktop window. Prevents the
			      * session thrashing that's seen otherwise
			      */
			     && nautilus_application_desktop_window != NULL);
}

static void
init_session (void)
{
	GnomeClient *client;

	client = gnome_master_client ();

	gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
			    (GtkSignalFunc) save_session,
			    NULL);
	
	gtk_signal_connect (GTK_OBJECT (client), "die",
			    (GtkSignalFunc) removed_from_session,
			    NULL);
	
	eel_preferences_add_callback
		(NAUTILUS_PREFERENCES_ADD_TO_SESSION,
		 update_session, client);

	update_session (client);
}

static gboolean
get_self_typed_prop (Window      xwindow,
                     Atom        atom,
                     gulong     *val)
{  
	Atom type;
	int format;
	gulong nitems;
	gulong bytes_after;
	gulong *num;
	int err;
  
	gdk_error_trap_push ();
	type = None;
	XGetWindowProperty (gdk_display,
			    xwindow,
			    atom,
			    0, G_MAXLONG,
			    False, atom, &type, &format, &nitems,
			    &bytes_after, (guchar **)&num);  

	err = gdk_error_trap_pop ();
	if (err != Success) {
		return FALSE;
	}
  
	if (type != atom) {
		return FALSE;
	}

	if (val)
		*val = *num;
  
	XFree (num);

	return TRUE;
}

static gboolean
has_wm_state (Window xwindow)
{
	return get_self_typed_prop (xwindow,
				    XInternAtom (gdk_display, "WM_STATE", False),
				    NULL);
}

static gboolean
look_for_kdesktop_recursive (Window xwindow)
{
  
	Window ignored1, ignored2;
	Window *children;
	unsigned int n_children;
	unsigned int i;
	gboolean retval;
  
	/* If WM_STATE is set, this is a managed client, so look
	 * for the class hint and end recursion. Otherwise,
	 * this is probably just a WM frame, so keep recursing.
	 */
	if (has_wm_state (xwindow)) {      
		XClassHint ch;
      
		gdk_error_trap_push ();
		ch.res_name = NULL;
		ch.res_class = NULL;
      
		XGetClassHint (gdk_display, xwindow, &ch);
      
		gdk_error_trap_pop ();
      
		if (ch.res_name)
			XFree (ch.res_name);
      
		if (ch.res_class) {
			if (strcmp (ch.res_class, "kdesktop") == 0) {
				XFree (ch.res_class);
				return TRUE;
			}
			else
				XFree (ch.res_class);
		}

		return FALSE;
	}
  
	retval = FALSE;
  
	gdk_error_trap_push ();
  
	XQueryTree (gdk_display,
		    xwindow,
		    &ignored1, &ignored2, &children, &n_children);

	if (gdk_error_trap_pop ()) {
		return FALSE;
	}

	i = 0;
	while (i < n_children) {
		if (look_for_kdesktop_recursive (children[i])) {
			retval = TRUE;
			break;
		}
      
		++i;
	}
  
	if (children)
		XFree (children);

	return retval;
}

static gboolean
is_kdesktop_present (void)
{
	/* FIXME this is a pretty lame hack, should be replaced
	 * eventually with e.g. a requirement that desktop managers
	 * support a manager selection, ICCCM sec 2.8
	 */

	return look_for_kdesktop_recursive (GDK_ROOT_WINDOW ());
}
