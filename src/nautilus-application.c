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

#include <bonobo.h>
#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-list-view.h"
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <liboaf/liboaf.h>

#include "nautilus-desktop-window.h"
#include "nautilus-first-time-druid.h"

#include <libnautilus-extensions/nautilus-icon-factory.h>

static CORBA_boolean manufactures                                (PortableServer_Servant    servant,
								  const CORBA_char         *iid,
								  CORBA_Environment        *ev);
static CORBA_Object  create_object                               (PortableServer_Servant    servant,
								  const CORBA_char         *iid,
								  const Bonobo_stringlist  *params,
								  CORBA_Environment        *ev);
static void          nautilus_application_initialize             (NautilusApplication      *application);
static void          nautilus_application_initialize_class       (NautilusApplicationClass *klass);
static void          nautilus_application_destroy                (GtkObject                *object);
static void          nautilus_application_check_user_directories (NautilusApplication      *application);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusApplication, nautilus_application, BONOBO_OBJECT_TYPE)

static POA_Bonobo_GenericFactory__epv factory_epv = {
	NULL,
	&manufactures,
	&create_object
};
static PortableServer_ServantBase__epv base_epv;
static POA_Bonobo_GenericFactory__vepv vepv = {
	&base_epv,
	&factory_epv
};

static CORBA_boolean
manufactures (PortableServer_Servant servant,
	      const CORBA_char *iid,
	      CORBA_Environment *ev)
{
	return strcmp (iid, "OAFIID:nautilus_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058") == 0
		|| strcmp (iid, "OAFIID:nautilus_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c") == 0;
}

static CORBA_Object
create_object (PortableServer_Servant servant,
	       const CORBA_char *iid,
	       const Bonobo_stringlist *params,
	       CORBA_Environment *ev)
{
	FMDirectoryView *directory_view;
	NautilusView *view;

	if (strcmp (iid, "OAFIID:nautilus_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058") == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_icon_view_get_type (), NULL));
	} else if (strcmp (iid, "OAFIID:nautilus_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c") == 0) {
		directory_view = FM_DIRECTORY_VIEW (gtk_object_new (fm_list_view_get_type (), NULL));
	} else {
		return CORBA_OBJECT_NIL;
	}
        
	view = fm_directory_view_get_nautilus_view (directory_view);
	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (view)), ev);
}

static CORBA_Object
create_factory (PortableServer_POA poa,
		NautilusApplication *bonobo_object,
		CORBA_Environment *ev)
{
	POA_Bonobo_GenericFactory *servant;

	servant = g_new0 (POA_Bonobo_GenericFactory, 1);
	servant->vepv = &vepv;
	POA_Bonobo_GenericFactory__init ((PortableServer_Servant) servant, ev);
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
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
	
	/* Create an undo manager */
	application->undo_manager = nautilus_undo_manager_new ();

	CORBA_exception_init (&ev);

	corba_object = create_factory (bonobo_poa (), application, &ev);
	bonobo_object_construct (BONOBO_OBJECT (application), corba_object);

	CORBA_exception_free (&ev);
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
	 * calling this function would have NOT cause the user to lose
	 * preferences.  The only effect would be to leak those
	 * objects - which would be collected at exit() time anyway,
	 * but it adds noise to memory profile tool runs.
	 */
	nautilus_global_preferences_shutdown ();

	nautilus_bookmarks_exiting ();

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
display_caveat (GtkWindow *parent_window)
{
	GtkWidget *dialog;
	GtkWidget *frame;
	GtkWidget *pixmap;
	GtkWidget *hbox;
	GtkWidget *text;
	char *file_name;

	dialog = gnome_dialog_new (_("Nautilus: caveat"),
				   GNOME_STOCK_BUTTON_OK,
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

  	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
  	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
  	gtk_widget_show (hbox);
  	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), 
  			    hbox,
  			    FALSE, FALSE, 0);

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
	
	file_name = nautilus_pixmap_file ("About_Image.png");
	pixmap = gnome_pixmap_new_from_file (file_name);
	g_free (file_name);
	gtk_widget_show (pixmap);
	gtk_container_add (GTK_CONTAINER (frame), pixmap);

  	text = gtk_label_new
		(_("The Nautilus shell is under development; it's not "
  		   "ready for daily use. Many features, including some "
  		   "of the best ones, are not yet done, partly done, or "
  		   "unstable. The program doesn't look or act the way "
  		   "it will in version 1.0."
		   "\n\n"
		   "If you do decide to test this version of Nautilus, "
		   "beware. The program could do something "
		   "unpredictable and may even delete or overwrite "
		   "files on your computer."
		   "\n\n"
		   "For more information, visit http://nautilus.eazel.com."));
    	gtk_label_set_line_wrap (GTK_LABEL (text), TRUE);
	gtk_widget_show (text);
  	gtk_box_pack_start (GTK_BOX (hbox), text, FALSE, FALSE, 0);

  	gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);

	if (parent_window != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG (dialog), parent_window);
	}

	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
nautilus_application_check_user_directories (NautilusApplication *application)
{
	const char		*user_directory;
	const char		*user_main_directory;
	const char		*desktop_directory;
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
	    
	if (!g_file_test (user_main_directory, G_FILE_TEST_ISDIR)) {
		nautilus_string_list_insert (dir_list, "User Main Directory");
	}
	    
	if (!g_file_test (desktop_directory, G_FILE_TEST_ISDIR)) {
		nautilus_string_list_insert (dir_list, "Desktop Directory");
	}

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

void
nautilus_application_startup (NautilusApplication *application,
			      gboolean manage_desktop,
			      const char *urls[])
{
	OAF_RegistrationResult result;
	const char *message, *detailed_message;
	GnomeDialog *dialog;
	const char **p;
	NautilusWindow *window;
	NautilusWindow *first_window;

	/* check if this is the first time running the program by seeing
	   if the user_main_directory exists; if not, run the first time druid 
	   instead of launching the application */
	if (!nautilus_user_main_directory_exists()) {
		nautilus_first_time_druid_show(application, manage_desktop, urls);
		return;
	}	

	/* Try to register the file manager view factory with OAF. */
	result = oaf_active_server_register
		("OAFIID:nautilus_file_manager_factory:bd1e1862-92d7-4391-963e-37583f0daef3",
		 bonobo_object_corba_objref (BONOBO_OBJECT (application)));
	switch (result) {
	case OAF_REG_SUCCESS:
		/* We are registered with OAF and all is right with the world. */
		message = NULL;
		break;
	case OAF_REG_NOT_LISTED:
		/* Can't register myself due to trouble locating the
		 * nautilus.oafinfo file. This has happened when you
		 * launch Nautilus with a PATH that doesn't include
		 * directory containg the oafd executable and oafd is
		 * not already running. It could also happen if the
		 * nautilus.oafinfo file was not present for some
		 * reason. Sometimes killing oafd and gconfd fixes
		 * this problem but we don't exactly understand why,
		 * since neither of the above causes explain it.
		 */
		message = _("Nautilus can't be used now. "
			    "Rebooting the computer or installing "
			    "Nautilus again may fix the problem.");
		/* FIXME: Add technical details here. They should come
		 * in the form of more detailed message that replaces
		 * the novice message if you press a button. The more
		 * detailed message should be complete and stand alone
		 * since it replaces the novice message.
		 */
		detailed_message = _("Nautilus can't be used now. "
				     "Rebooting the computer or installing "
				     "Nautilus again may fix the problem. "
				     "Check out all of this excellent detail!");
		break;
	case OAF_REG_ALREADY_ACTIVE:
		/* Another copy of Nautilus is already
		 * running. Eventually we want to "glom on" to this
		 * old copy.
		 */
		message = _("Nautilus is already running. Soon, instead of presenting this dialog, the already-running copy of Nautilus will respond by opening windows.");
		detailed_message = NULL;
		break;
	default:
		/* This should never happen. */
		g_warning ("bad error code from oaf_active_server_register");
	case OAF_REG_ERROR:
		/* Some misc. error (can never happen with current
		 * version of OAF). Show dialog and terminate the
		 * program.
		 */
		message = _("Nautilus can't be used now, due to an unexpected error.");
		detailed_message = NULL;
		break;
	}
	if (message != NULL) {
		dialog = nautilus_error_dialog_with_details
			(message, detailed_message, NULL);
		gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
				    gtk_main_quit, NULL);
		return;
	}

	/* Check the user's ~/.nautilus directories and post warnings
	 * if there are problems
	 */
	nautilus_application_check_user_directories (application);

	/* Set up the desktop. */
	if (manage_desktop) {
		gtk_widget_show (GTK_WIDGET (nautilus_desktop_window_new (application)));
	}

  	/* Create the other windows. */
	first_window = NULL;
	if (urls != NULL) {
		for (p = urls; *p != NULL; p++) {
			window = nautilus_application_create_window (application);
			nautilus_window_goto_uri (window, *p);
			if (first_window == NULL) {
				first_window = window;
			}
		}
	}
	/* FIXME bugzilla.eazel.com 1051: Change this logic back so it won't
	 * make a new window when asked to manage the desktop, once we have
	 * a way to get rid of the desktop.
	 */
	if (/* !manage_desktop && */ first_window == NULL) {
		first_window = nautilus_application_create_window (application);
		nautilus_window_go_home (first_window);
	}

	/* Show the "not ready for prime time" dialog after the first
	 * window appears, so it's on top.
	 */
	/* FIXME bugzilla.eazel.com 1256: It's not on top of the
         * windows other than the first one.
	 */
	if (g_getenv ("NAUTILUS_NO_CAVEAT_DIALOG") == NULL) {
	  	if (first_window == NULL) {
			display_caveat (NULL);
		} else {
			gtk_signal_connect (GTK_OBJECT (first_window), "show",
					    display_caveat, first_window);
		}
  	}
}

static void
nautilus_application_destroy_window (GtkObject *obj, NautilusApplication *application)
{
	application->windows = g_slist_remove (application->windows, obj);
	if (application->windows == NULL) {
  		nautilus_application_quit();
	}
}

void 
nautilus_application_quit (void)
{
	gtk_main_quit ();
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

	application->windows = g_slist_prepend (application->windows, window);

	/* Do not yet show the window. It will be shown later on if it can
	 * successfully display its initial URI. Otherwise it will be destroyed
	 * without ever having seen the light of day.
	 */

	return window;
}
