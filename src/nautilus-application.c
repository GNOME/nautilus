/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>,
 *
 */

#include <config.h>
#include "nautilus-application.h"

/* FIXME: This is a workaround for ORBit bug where including idl files
 * in other idl files causes trouble.
 */
#include "nautilus-shell-interface.h"
#define nautilus_view_component_H

#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-desktop-icon-view.h"
#include "file-manager/fm-list-view.h"
#include "file-manager/fm-search-list-view.h"
#include "nautilus-desktop-window.h"
#include "nautilus-first-time-druid.h"
#include "nautilus-shell.h"
#include <bonobo.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <liboaf/liboaf.h>

#define FACTORY_IID	"OAFIID:nautilus_factory:bd1e1862-92d7-4391-963e-37583f0daef3"
#define ICON_VIEW_IID	"OAFIID:nautilus_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058"
#define LIST_VIEW_IID	"OAFIID:nautilus_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c"
#define SEARCH_LIST_VIEW_IID "OAFIID:nautilus_file_manager_search_list_view:b186e381-198e-43cf-9c46-60b6bb35db0b"
#define SHELL_IID	"OAFIID:nautilus_shell:cd5183b2-3913-4b74-9b8e-10528b0de08d"

static CORBA_boolean manufactures                                (PortableServer_Servant    servant,
								  const CORBA_char         *iid,
								  CORBA_Environment        *ev);
static CORBA_Object  create_object                               (PortableServer_Servant    servant,
								  const CORBA_char         *iid,
								  const GNOME_stringlist  *params,
								  CORBA_Environment        *ev);
static void          nautilus_application_initialize             (NautilusApplication      *application);
static void          nautilus_application_initialize_class       (NautilusApplicationClass *klass);
static void          nautilus_application_destroy                (GtkObject                *object);
static void          nautilus_application_check_user_directories (NautilusApplication      *application);
static gboolean	     check_for_and_run_as_super_user 		 (void);
static gboolean	     need_to_show_first_time_druid		 (void);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusApplication, nautilus_application, BONOBO_OBJECT_TYPE)

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
	return strcmp (iid, ICON_VIEW_IID) == 0
		|| strcmp (iid, NAUTILUS_DESKTOP_ICON_VIEW_IID) == 0
		|| strcmp (iid, LIST_VIEW_IID) == 0
		|| strcmp (iid, SEARCH_LIST_VIEW_IID) == 0
		|| strcmp (iid, SHELL_IID) == 0;
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

	if (strcmp (iid, ICON_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_icon_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, NAUTILUS_DESKTOP_ICON_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_desktop_icon_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, LIST_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_list_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, SEARCH_LIST_VIEW_IID) == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_search_list_view_get_type (), NULL));
		object = BONOBO_OBJECT (fm_directory_view_get_nautilus_view (directory_view));
	} else if (strcmp (iid, SHELL_IID) == 0) {
		application = NAUTILUS_APPLICATION (((BonoboObjectServant *) servant)->bonobo_object);
		object = BONOBO_OBJECT (nautilus_shell_new (application));
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

/* Keeps track of the one and only desktop window. */
static NautilusDesktopWindow *nautilus_application_desktop;

/* Keeps track of all the nautilus windows. */
static GSList *nautilus_application_window_list;

GSList *nautilus_application_windows (void)
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

	/* Initialize preferences. This is needed so that proper 
	 * defaults are available before any preference peeking 
	 * happens.
	 */
	nautilus_global_preferences_initialize ();

	/* Create an undo manager */
	application->undo_manager = nautilus_undo_manager_new ();
}

NautilusApplication *
nautilus_application_new (void)
{
	return NAUTILUS_APPLICATION (gtk_object_new (nautilus_application_get_type (), NULL));
}

static void
nautilus_application_destroy (GtkObject *object)
{
	/* Shut down preferences. This is needed so that the global
	 * preferences object and all its allocations are freed. Not
	 * calling this function would NOT cause the user to lose
	 * preferences.  The only effect would be to leak those
	 * objects - which would be collected at exit() time anyway,
	 * but it adds noise to memory profile tool runs.
	 */
	nautilus_global_preferences_shutdown ();

	nautilus_bookmarks_exiting ();

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_application_check_user_directories (NautilusApplication *application)
{
	char			*user_directory;
	char			*user_main_directory;
	char			*desktop_directory;
	NautilusStringList	*dir_list;
	
	g_assert (NAUTILUS_IS_APPLICATION (application));

	user_directory = nautilus_get_user_directory ();
	user_main_directory = nautilus_get_user_main_directory ();
	desktop_directory = nautilus_get_desktop_directory ();

	dir_list = nautilus_string_list_new ();
	
	/* FIXME bugzilla.eazel.com 1115: Need better name for "User Directory"
	 * and "User Data Directory".
	 */

	if (!g_file_test (user_directory, G_FILE_TEST_ISDIR)) {
		nautilus_string_list_insert (dir_list, "User Directory");
	}
	g_free (user_directory);
	    
	if (!g_file_test (user_main_directory, G_FILE_TEST_ISDIR)) {
		nautilus_string_list_insert (dir_list, "User Main Directory");
	}
	g_free (user_main_directory);
	    
	if (!g_file_test (desktop_directory, G_FILE_TEST_ISDIR)) {
		nautilus_string_list_insert (dir_list, "Desktop Directory");
	}
	g_free (desktop_directory);

	if (nautilus_string_list_get_length (dir_list) > 0) {
		char *dir_list_concatenated;
		char *error_string;

		dir_list_concatenated = nautilus_string_list_as_concatenated_string (dir_list, "\n");
		
		error_string = g_strdup_printf ("%s\n\n%s\n\n%s",
						"The following directories are missing:",
						dir_list_concatenated,
						"Please restart Nautilus to fix this problem.");

		nautilus_error_dialog (error_string, NULL);

		g_free (dir_list_concatenated);
		g_free (error_string);
	}

	nautilus_string_list_free (dir_list);
}

static int
nautilus_strv_length (const char * const *strv)
{
	const char * const *p;

	for (p = strv; *p != NULL; p++) { }
	return p - strv;
}

static Nautilus_URIList *
nautilus_make_uri_list_from_strv (const char * const *strv)
{
	int length, i;
	Nautilus_URIList *uri_list;

	length = nautilus_strv_length (strv);

	uri_list = Nautilus_URIList__alloc ();
	uri_list->_maximum = length;
	uri_list->_length = length;
	uri_list->_buffer = CORBA_sequence_Nautilus_URI_allocbuf (length);
	for (i = 0; i < length; i++) {
		uri_list->_buffer[i] = CORBA_string_dup (strv[i]);
	}
	CORBA_sequence_set_release (uri_list, CORBA_TRUE);

	return uri_list;
}

gboolean
nautilus_application_startup (NautilusApplication *application,
			      gboolean kill_shell,
			      gboolean stop_desktop,
			      gboolean start_desktop,
			      const char *urls[])
{
	CORBA_Environment ev;
	Nautilus_Shell shell;
	OAF_RegistrationResult result;
	const char *message, *detailed_message;
	GnomeDialog *dialog;
	Nautilus_URIList *url_list;
	gboolean need_main_loop;

	/* Perform check for nautilus being run as super user */
	if (!check_for_and_run_as_super_user ()) {
		return FALSE;
	}

	/* Run the first time startup druid if needed. */
	if (need_to_show_first_time_druid ()) {
		nautilus_first_time_druid_show (application, start_desktop, urls);
		return TRUE;
	}
	
	/* Check the user's ~/.nautilus directories and post warnings
	 * if there are problems.
	 */
	nautilus_application_check_user_directories (application);

	CORBA_exception_init (&ev);

	/* Start up the factory. */
	for (;;) {
		/* Try to register the file manager view factory with OAF. */
		result = oaf_active_server_register
			(FACTORY_IID,
			 bonobo_object_corba_objref (BONOBO_OBJECT (application)));
		switch (result) {
		case OAF_REG_SUCCESS:
			/* We are registered with OAF and all is right with the world. */
		case OAF_REG_ALREADY_ACTIVE:
			/* Another copy of . */
			message = NULL;
			break;
		case OAF_REG_NOT_LISTED:
			/* Can't register myself due to trouble locating the
			 * nautilus.oafinfo file. This has happened when you
			 * launch Nautilus with an LD_LIBRARY_PATH that
			 * doesn't include the directory containg the oaf
			 * library. It could also happen if the
			 * nautilus.oafinfo file was not present for some
			 * reason. Sometimes killing oafd and gconfd fixes
			 * this problem but we don't exactly understand why,
			 * since neither of the above causes explain it.
			 */
			message = _("Nautilus can't be used now. "
				    "Rebooting the computer or installing "
				    "Nautilus again may fix the problem.");
			/* FIXME: The guesses and stuff here are lame. */
			detailed_message = _("Nautilus can't be used now. "
					     "Rebooting the computer or installing "
					     "Nautilus again may fix the problem.\n\n"
					     "OAF couldn't locate the nautilus.oafinfo file. "
					     "One cause of this seems to be an LD_LIBRARY_PATH "
					     "that does not include the oaf library's directory. "
					     "Another possible cause would be bad install "
					     "with a missing nautilus.oafinfo file.\n\n"
					     "Sometimes killing oafd and gconfd fixes "
					     "the problem, but we don't know why.\n\n"
					     "We need a much less confusing message here for Nautilus 1.0.");
			break;
		default:
			/* This should never happen. */
			g_warning ("bad error code from oaf_active_server_register");
		case OAF_REG_ERROR:
			/* Some misc. error (can never happen with current
			 * version of OAF). Show dialog and terminate the
			 * program.
			 */
			/* FIXME: Looks like this does happen with the
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
				/* FIXME: When can this happen? */
				message = _("Nautilus can't be used now, due to an unexpected error.");
				detailed_message = _("Nautilus can't be used now, due to an unexpected error "
						     "from OAF when attempting to locate the factory.");
			}
		}

		if (message != NULL) {
			dialog = nautilus_error_dialog_with_details
				(message, detailed_message, NULL);
			gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
					    gtk_main_quit, NULL);
			need_main_loop = TRUE;
			goto out;
		}
	}

	if (kill_shell) {
		Nautilus_Shell_quit (shell, &ev);
	} else {
		if (start_desktop) {
			Nautilus_Shell_start_desktop (shell, &ev);
		}

		if (stop_desktop) {
			Nautilus_Shell_stop_desktop (shell, &ev);
		}

	  	/* Create the other windows. */
		if (urls != NULL) {
			url_list = nautilus_make_uri_list_from_strv (urls);
			Nautilus_Shell_open_windows (shell, url_list, &ev);
			CORBA_free (url_list);
		} else if (!start_desktop && !stop_desktop) {
			Nautilus_Shell_open_default_window (shell, &ev);
		}
	}

	/* We're done with the shell now, so let it go. */
	Nautilus_Shell_unref (shell, &ev);
	CORBA_Object_release (shell, &ev);

	need_main_loop = nautilus_application_window_list != NULL
		|| nautilus_application_desktop != NULL;

 out:
	CORBA_exception_free (&ev);
	return need_main_loop;
}

static void
nautilus_application_destroy_desktop_window (GtkObject *obj, NautilusApplication *application)
{
	nautilus_application_desktop = NULL;
}

static NautilusDesktopWindow *
nautilus_application_create_desktop_window (NautilusApplication *application)
{
	NautilusDesktopWindow *window;
	
	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);

	window = nautilus_desktop_window_new (application);

	gtk_signal_connect (GTK_OBJECT (window),
			    "destroy", nautilus_application_destroy_desktop_window,
			    application);

	nautilus_application_desktop = window;

	return window;
}

void
nautilus_application_open_desktop (NautilusApplication *application)
{
	NautilusDesktopWindow *desktop_window;

	if (nautilus_application_desktop == NULL) {
		desktop_window = nautilus_application_create_desktop_window (application);
	}
	gtk_widget_show (GTK_WIDGET (desktop_window));
}

void
nautilus_application_close_desktop (void)
{
	if (nautilus_application_desktop != NULL) {
		gtk_widget_destroy (GTK_WIDGET (nautilus_application_desktop));
	}
	
	if (nautilus_application_window_list == NULL && gtk_main_level () > 0) {
		gtk_main_quit ();
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
nautilus_application_destroy_window (GtkObject *obj, NautilusApplication *application)
{
	nautilus_application_window_list = g_slist_remove (nautilus_application_window_list, obj);
	if (nautilus_application_window_list == NULL && nautilus_application_desktop == NULL) {
		gtk_main_quit ();
	}
}

NautilusWindow *
nautilus_application_create_window (NautilusApplication *application)
{
	NautilusWindow *window;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);
	
	window = NAUTILUS_WINDOW (gtk_object_new (nautilus_window_get_type (),
						  "app", GTK_OBJECT (application),
						  "app_id", "nautilus", NULL));
	gtk_signal_connect (GTK_OBJECT (window),
			    "destroy", nautilus_application_destroy_window,
			    application);

	nautilus_application_window_list = g_slist_prepend (nautilus_application_window_list, window);

	/* Do not yet show the window. It will be shown later on if it can
	 * successfully display its initial URI. Otherwise it will be destroyed
	 * without ever having seen the light of day.
	 */

	return window;
}

/*
 * check_for_super_user:
 *
 * Puts out a warning if the user is running nautilus as root.
 */
static gboolean
check_for_and_run_as_super_user (void)
{
	GtkWidget *warning_dlg;
	gint result;
	if (geteuid () != 0) {
		return TRUE;
	}

	warning_dlg = gnome_message_box_new (
		_("You are running Nautilus as root.\n\n"
		  "As root, you can damage your system if you are not careful, and\n"
		  "Nautilus will not stop you from doing it."),
		GNOME_MESSAGE_BOX_WARNING,
		GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);

	result = gnome_dialog_run_and_close (GNOME_DIALOG (warning_dlg));

	/* If they pressed cancel, quit the application */
	if (result == 1) {
		return FALSE;
	}

	return TRUE;
}

/*
 * need_to_show_first_time_druid
 *
 * Determine whether Nautilus needs to show the first time druid.
 * 
 * Note that the flag file indicating whether the druid has been
 * presented is: ~/.nautilus/first-time-wizard.
 *
 * Another alternative could be to use preferences to store this flag
 * However, there because of bug 1229 this is not yet possible.
 *
 * Also, for debugging purposes, it is convenient to have just one file
 * to kill in order to test the startup wizard:
 *
 * rm -f ~/.nautilus/first-time-wizard
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

	druid_flag_file_name = g_strdup_printf ("%s/%s",
						user_directory,
						"first-time-wizard-flag");
	g_free (user_directory);

	result = !g_file_exists (druid_flag_file_name);

	/* Touch the file so that next time around out states changes */
	if (result) {
		FILE *stream;
		
		stream = fopen (druid_flag_file_name, "w");

		/* If for some crazy reason we cant touch the file,
		 * then the worst that will happen is that the user
		 * will get the startup druid again next time, so we
		 * dont report this failure to the user.
		 */
		if (stream) {
			const char *blurb =
				_("Existence of this file indicates that the Nautilus configuration wizard\n"
				  "has been presented.\n\n"
				  "You can manually erase this file to present the wizard again.\n\n");
			
			fwrite (blurb, sizeof (char), strlen (blurb), stream);
			fclose (stream);
		}
	}
	
	g_free (druid_flag_file_name);

	return result;
}
