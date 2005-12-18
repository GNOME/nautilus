/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-choosing.c - functions for selecting and activating
 				 programs for opening/viewing particular files.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-program-choosing.h"

#include "nautilus-mime-actions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-factory.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-url.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdlib.h>

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60


#ifdef HAVE_STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

extern char **environ;

/* Cut and paste from gdkspawn-x11.c */
static gchar **
my_gdk_spawn_make_environment_for_screen (GdkScreen  *screen,
					  gchar     **envp)
{
  gchar **retval = NULL;
  gchar  *display_name;
  gint    display_index = -1;
  gint    i, env_len;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (envp == NULL)
    envp = environ;

  for (env_len = 0; envp[env_len]; env_len++)
    if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
      display_index = env_len;

  retval = g_new (char *, env_len + 1);
  retval[env_len] = NULL;

  display_name = gdk_screen_make_display_name (screen);

  for (i = 0; i < env_len; i++)
    if (i == display_index)
      retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
    else
      retval[i] = g_strdup (envp[i]);

  g_assert (i == env_len);

  g_free (display_name);

  return retval;
}


/**
 * application_cannot_open_location
 * 
 * Handle the case where an application has been selected to be launched,
 * and it cannot handle the current uri scheme.  This can happen
 * because the default application for a file type may not be able
 * to handle some kinds of locations.   We want to tell users that their
 * default application doesn't work here, rather than switching off to
 * a different one without them noticing.
 * 
 * @application: The application that was to be launched.
 * @file: The file whose location was passed as a parameter to the application
 * @parent_window: A window to use as the parent for any error dialogs.
 *  */
static void
application_cannot_open_location (GnomeVFSMimeApplication *application,
				  NautilusFile *file,
				  const char *uri_scheme,
				  GtkWindow *parent_window)
{
#if NEW_MIME_COMPLETE
	GtkDialog *message_dialog;
	LaunchParameters *launch_parameters;
	char *prompt;
	char *message;
	char *file_name;
	int response;

	file_name = nautilus_file_get_display_name (file);

	if (nautilus_mime_has_any_applications_for_file (file)) {
		if (application != NULL) {
			prompt = _("Open Failed, would you like to choose another application?");
			message = g_strdup_printf (_("\"%s\" can't open \"%s\" because \"%s\" can't access files at \"%s\" "
						     "locations."),
						   application->name, file_name, 
						   application->name, uri_scheme);
		} else {
			prompt = _("Open Failed, would you like to choose another action?");
			message = g_strdup_printf (_("The default action can't open \"%s\" because it can't access files at \"%s\" "
						     "locations."),
						   file_name, uri_scheme);
		}
		
		message_dialog = eel_show_yes_no_dialog (prompt, 
		                                         message,
							 GTK_STOCK_OK,
							 GTK_STOCK_CANCEL,
							 parent_window);
		response = gtk_dialog_run (message_dialog);
		gtk_object_destroy (GTK_OBJECT (message_dialog));
		
		if (response == GTK_RESPONSE_YES) {
			launch_parameters = launch_parameters_new (file, parent_window);
			nautilus_choose_application_for_file 
				(file,
				 parent_window,
				 launch_application_callback,
				 launch_parameters);
				 
		}
		g_free (message);
	} else {
		if (application != NULL) {
			prompt = g_strdup_printf (_("\"%s\" can't open \"%s\" because \"%s\" can't access files at \"%s\"."
						    "locations."), application->name, file_name, 
						    application->name, uri_scheme);
			message = _("No other applications are available to view this file.  "
				    "If you copy this file onto your computer, you may be able to open "
				    "it.");
		} else {
			prompt = g_strdup_printf (_("The default action can't open \"%s\" because it can't access files at \"%s\"."
						    "locations."), file_name, uri_scheme);
     			message = _("No other actions are available to view this file.  "
				    "If you copy this file onto your computer, you may be able to open "
				    "it.");
		}
				
		eel_show_info_dialog (prompt, message, parent_window);
		g_free (prompt);
	}	

	g_free (file_name);
#endif
}

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *display,
		    Display   *xdisplay)
{
	gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
		   Display   *xdisplay)
{
	gdk_error_trap_pop ();
}

extern char **environ;

static char **
make_spawn_environment_for_sn_context (SnLauncherContext *sn_context,
				       char             **envp)
{
	char **retval;
	int    i, j;

	retval = NULL;
	
	if (envp == NULL) {
		envp = environ;
	}
	
	for (i = 0; envp[i]; i++) {
		/* Count length */
	}

	retval = g_new (char *, i + 2);

	for (i = 0, j = 0; envp[i]; i++) {
		if (!g_str_has_prefix (envp[i], "DESKTOP_STARTUP_ID=")) {
			retval[j] = g_strdup (envp[i]);
			++j;
	        }
	}

	retval[j] = g_strdup_printf ("DESKTOP_STARTUP_ID=%s",
				     sn_launcher_context_get_startup_id (sn_context));
	++j;
	retval[j] = NULL;

	return retval;
}

/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn't (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don't end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don't monitor
 * the sequence (don't use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH (30 /* seconds */ * 1000)

typedef struct
{
	GdkScreen *screen;
	GSList *contexts;
	guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
	StartupTimeoutData *std;

	std = data;

	g_slist_foreach (std->contexts,
			 (GFunc) sn_launcher_context_unref,
			 NULL);
	g_slist_free (std->contexts);

	if (std->timeout_id != 0) {
		g_source_remove (std->timeout_id);
		std->timeout_id = 0;
	}

	g_free (std);
}

static gboolean
startup_timeout (void *data)
{
	StartupTimeoutData *std;
	GSList *tmp;
	GTimeVal now;
	int min_timeout;

	std = data;

	min_timeout = STARTUP_TIMEOUT_LENGTH;
	
	g_get_current_time (&now);
	
	tmp = std->contexts;
	while (tmp != NULL) {
		SnLauncherContext *sn_context;
		GSList *next;
		long tv_sec, tv_usec;
		double elapsed;
		
		sn_context = tmp->data;
		next = tmp->next;
		
		sn_launcher_context_get_last_active_time (sn_context,
							  &tv_sec, &tv_usec);

		elapsed =
			((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
			  (now.tv_usec - tv_usec))) / 1000.0;

		if (elapsed >= STARTUP_TIMEOUT_LENGTH) {
			std->contexts = g_slist_remove (std->contexts,
							sn_context);
			sn_launcher_context_complete (sn_context);
			sn_launcher_context_unref (sn_context);
		} else {
			min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
		}
		
		tmp = next;
	}

	if (std->contexts == NULL) {
		std->timeout_id = 0;
	} else {
		std->timeout_id = g_timeout_add (min_timeout,
						 startup_timeout,
						 std);
	}

	/* always remove this one, but we may have reinstalled another one. */
	return FALSE;
}

static void
add_startup_timeout (GdkScreen         *screen,
		     SnLauncherContext *sn_context)
{
	StartupTimeoutData *data;

	data = g_object_get_data (G_OBJECT (screen), "nautilus-startup-data");
	if (data == NULL) {
		data = g_new (StartupTimeoutData, 1);
		data->screen = screen;
		data->contexts = NULL;
		data->timeout_id = 0;
		
		g_object_set_data_full (G_OBJECT (screen), "nautilus-startup-data",
					data, free_startup_timeout);		
	}

	sn_launcher_context_ref (sn_context);
	data->contexts = g_slist_prepend (data->contexts, sn_context);
	
	if (data->timeout_id == 0) {
		data->timeout_id = g_timeout_add (STARTUP_TIMEOUT_LENGTH,
						  startup_timeout,
						  data);		
	}
}

/* FIXME: This is the wrong way to do this; there should be some event
 * (e.g. button press) available with a good time.  A function like
 * this should not be needed.
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
	Window xwindow;
	XEvent event;
	
	{
		XSetWindowAttributes attrs;
		Atom atom_name;
		Atom atom_type;
		char* name;
		
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;
		
		xwindow =
			XCreateWindow (xdisplay,
				       RootWindow (xdisplay, 0),
				       -100, -100, 1, 1,
				       0,
				       CopyFromParent,
				       CopyFromParent,
				       (Visual *)CopyFromParent,
				       CWOverrideRedirect | CWEventMask,
				       &attrs);
		
		atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
		g_assert (atom_name != None);
		atom_type = XInternAtom (xdisplay, "STRING", TRUE);
		g_assert (atom_type != None);
		
		name = "Fake Window";
		XChangeProperty (xdisplay, 
				 xwindow, atom_name,
				 atom_type,
				 8, PropModeReplace, name, strlen (name));
	}
	
	XWindowEvent (xdisplay,
		      xwindow,
		      PropertyChangeMask,
		      &event);
	
	XDestroyWindow(xdisplay, xwindow);
	
	return event.xproperty.time;
}
#endif /* HAVE_STARTUP_NOTIFICATION */





/**
 * nautilus_launch_show_file:
 *
 * Shows a file using gnome_url_show.
 *
 * @file: the file whose uri will be shown.
 * @parent_window: window to use as parent for error dialog.
 */
void nautilus_launch_show_file (NautilusFile *file,
                                GtkWindow    *parent_window)
{
	GnomeVFSResult result;
	GnomeVFSMimeApplication *application;
	GdkScreen *screen;
	char **envp;
	char *uri, *uri_scheme;
	char *error_message, *detail_message;
        char *full_uri_for_display;
        char *uri_for_display;
	GnomeVFSURI *vfs_uri;
#ifdef HAVE_STARTUP_NOTIFICATION
	SnLauncherContext *sn_context;
	SnDisplay *sn_display;
	gboolean startup_notify;

	startup_notify = FALSE;
#endif

	g_return_if_fail (!nautilus_file_needs_slow_mime_type (file));

	uri = NULL;
	if (nautilus_file_is_nautilus_link (file)) {
		uri = nautilus_file_get_activation_uri (file);
	}
	
	if (uri == NULL) {
		uri = nautilus_file_get_uri (file);
	}
		
	application = nautilus_mime_get_default_application_for_file (file);
	
	screen = gtk_window_get_screen (parent_window);
	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);
	
#ifdef HAVE_STARTUP_NOTIFICATION
	sn_display = sn_display_new (gdk_display,
				     sn_error_trap_push,
				     sn_error_trap_pop);
	
	/* Only initiate notification if application supports it. */
	if (application) {
		startup_notify = gnome_vfs_mime_application_supports_startup_notification (application);
	} else {
		startup_notify = FALSE;
	}
	
	if (startup_notify == TRUE) {
		char *name;
		char *icon;

		sn_context = sn_launcher_context_new (sn_display,
						      screen ? gdk_screen_get_number (screen) :
						      DefaultScreen (gdk_display));
		
		name = nautilus_file_get_display_name (file);

		if (name != NULL) {
			char *description;
			
			sn_launcher_context_set_name (sn_context, name);
			
			description = g_strdup_printf (_("Opening %s"), name);
			
			sn_launcher_context_set_description (sn_context, description);

			g_free (name);
			g_free (description);
		}

		icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
		if (icon != NULL) {
			sn_launcher_context_set_icon_name (sn_context, icon);
			g_free (icon);
		}
		
		if (!sn_launcher_context_get_initiated (sn_context)) {
			const char *binary_name;
			char **old_envp;
			Time timestamp;

			timestamp = slowly_and_stupidly_obtain_timestamp (GDK_WINDOW_XDISPLAY (GTK_WIDGET (parent_window)->window));

			binary_name = gnome_vfs_mime_application_get_binary_name (application);
		
			sn_launcher_context_set_binary_name (sn_context,
							     binary_name);
			
			sn_launcher_context_initiate (sn_context,
						      g_get_prgname () ? g_get_prgname () : "unknown",
						      binary_name,
						      timestamp);

			old_envp = envp;
			envp = make_spawn_environment_for_sn_context (sn_context, envp);
			g_strfreev (old_envp);
		}
	} else {
		sn_context = NULL;
	}
#endif /* HAVE_STARTUP_NOTIFICATION */
	
	result = gnome_vfs_url_show_with_env (uri, envp);

#ifdef HAVE_STARTUP_NOTIFICATION
	if (sn_context != NULL) {
		if (result != GNOME_VFS_OK) {
			sn_launcher_context_complete (sn_context); /* end sequence */
		} else {
			add_startup_timeout (screen ? screen :
					     gdk_display_get_default_screen (gdk_display_get_default ()),
					     sn_context);
		}
		sn_launcher_context_unref (sn_context);
	}
	
	sn_display_unref (sn_display);
#endif /* HAVE_STARTUP_NOTIFICATION */
	
	full_uri_for_display = eel_format_uri_for_display (uri);
	/* Truncate the URI so it doesn't get insanely wide. Note that even
	 * though the dialog uses wrapped text, if the URI doesn't contain
	 * white space then the text-wrapping code is too stupid to wrap it.
	 */
	uri_for_display = eel_str_middle_truncate
		(full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
	g_free (full_uri_for_display);
	
	error_message = detail_message = NULL;
	
	switch (result) {
	case GNOME_VFS_OK:
		break;
		
	case GNOME_VFS_ERROR_NOT_SUPPORTED:
		uri_scheme = nautilus_file_get_uri_scheme (file);
		application_cannot_open_location (NULL,
						  file,
						  uri_scheme,
						  parent_window);
		g_free (uri_scheme);
		break;
		
	case GNOME_VFS_ERROR_NO_DEFAULT:
	case GNOME_VFS_ERROR_NO_HANDLER:
#if NEW_MIME_COMPLETE
		nautilus_program_chooser_show_no_choices_message
					(action_type, file, parent_window);
		break;
#endif
		error_message = g_strdup_printf (_("Couldn't display \"%s\"."),
						 uri_for_display);
		/* TODO: This really needs to be something saying "no app
		 * handling this file type", but there is a string freeze. */
		detail_message = g_strdup ("");
		break;
		
	case GNOME_VFS_ERROR_LAUNCH:
		/* TODO: These strings suck pretty badly, but we're in string-freeze,
		 * and I found these in other places to reuse. We should make them
		 * better later. */
		error_message = g_strdup_printf (_("Couldn't display \"%s\"."),
						 uri_for_display);
		detail_message = g_strdup (_("There was an error launching the application."));		
		break;
	default:

		switch (nautilus_file_get_file_info_result (file)) {
		case GNOME_VFS_ERROR_ACCESS_DENIED:
			error_message = g_strdup_printf (_("Couldn't display \"%s\"."),
							 uri_for_display);
			detail_message = g_strdup (_("The attempt to log in failed."));		
			break;
		case GNOME_VFS_ERROR_NOT_PERMITTED:
			error_message = g_strdup_printf (_("Couldn't display \"%s\"."),
							 uri_for_display);
			detail_message = g_strdup (_("Access was denied."));		
			break;
		case GNOME_VFS_ERROR_INVALID_HOST_NAME:
		case GNOME_VFS_ERROR_HOST_NOT_FOUND:
			vfs_uri = gnome_vfs_uri_new (uri);
			error_message = g_strdup_printf (_("Couldn't display \"%s\", because no host \"%s\" could be found."),
							 uri_for_display,
							 gnome_vfs_uri_get_host_name (vfs_uri));
			detail_message = g_strdup (_("Check that the spelling is correct and that your proxy settings are correct."));
			gnome_vfs_uri_unref (vfs_uri);
			break;
		case GNOME_VFS_ERROR_INVALID_URI:
			error_message = g_strdup_printf
				(_("\"%s\" is not a valid location."),
				 uri_for_display);
			detail_message = g_strdup 
				(_("Please check the spelling and try again."));
			break;
		case GNOME_VFS_ERROR_NOT_FOUND:
			error_message = g_strdup_printf
				(_("Couldn't find \"%s\"."), 
				 uri_for_display);
			detail_message = g_strdup 
				(_("Please check the spelling and try again."));
			break;
		case GNOME_VFS_ERROR_CANCELLED:
			break;
		case GNOME_VFS_OK:
		default:
#if NEW_MIME_COMPLETE
			nautilus_program_chooser_show_invalid_message
				(action_type, file, parent_window);
#endif
			break;
		}

		
	}

	if (error_message != NULL) {
		eel_show_error_dialog (error_message, detail_message, parent_window);
		
		g_free (error_message);
		g_free (detail_message);
	}
	
	g_free (uri_for_display);
	
	if (application != NULL) 
		gnome_vfs_mime_application_free (application);

	g_strfreev (envp);
	g_free (uri);
}

/**
 * nautilus_launch_application:
 * 
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs. 
 * 
 * @application: The application to be launched.
 * @file: The file whose location should be passed as a parameter to the application
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
nautilus_launch_application (GnomeVFSMimeApplication *application, 
			     NautilusFile *file,
			     GtkWindow *parent_window)
{
	GdkScreen       *screen;
	char		*uri;
	char            *uri_scheme;
	GList            uris;
	char           **envp;
	GnomeVFSResult   result;
#ifdef HAVE_STARTUP_NOTIFICATION
	SnLauncherContext *sn_context;
	SnDisplay *sn_display;
#endif

	uri = NULL;
	if (nautilus_file_is_nautilus_link (file)) {
		uri = nautilus_file_get_activation_uri (file);
	}
	
	if (uri == NULL) {
		uri = nautilus_file_get_uri (file);
	}

	uris.next = NULL;
	uris.prev = NULL;
	uris.data = uri;
	
	screen = gtk_window_get_screen (parent_window);
	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);
	
#ifdef HAVE_STARTUP_NOTIFICATION
	sn_display = sn_display_new (gdk_display,
				     sn_error_trap_push,
				     sn_error_trap_pop);

	
	/* Only initiate notification if application supports it. */
	if (gnome_vfs_mime_application_supports_startup_notification (application))
	{ 
		char *name;
		char *icon;

		sn_context = sn_launcher_context_new (sn_display,
						      screen ? gdk_screen_get_number (screen) :
						      DefaultScreen (gdk_display));
		
		name = nautilus_file_get_display_name (file);
		if (name != NULL) {
			char *description;
			
			sn_launcher_context_set_name (sn_context, name);
			
			description = g_strdup_printf (_("Opening %s"), name);
			
			sn_launcher_context_set_description (sn_context, description);

			g_free (name);
			g_free (description);
		}

		icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
		if (icon != NULL) {
			sn_launcher_context_set_icon_name (sn_context, icon);
			g_free (icon);
		}
		
		if (!sn_launcher_context_get_initiated (sn_context)) {
			const char *binary_name;
			char **old_envp;
			Time timestamp;

			timestamp = slowly_and_stupidly_obtain_timestamp (GDK_WINDOW_XDISPLAY (GTK_WIDGET (parent_window)->window));
			
			binary_name = gnome_vfs_mime_application_get_binary_name (application);
		
			sn_launcher_context_set_binary_name (sn_context,
							     binary_name);
			
			sn_launcher_context_initiate (sn_context,
						      g_get_prgname () ? g_get_prgname () : "unknown",
						      binary_name,
						      timestamp);

			old_envp = envp;
			envp = make_spawn_environment_for_sn_context (sn_context, envp);
			g_strfreev (old_envp);
		}
	} else {
		sn_context = NULL;
	}
#endif /* HAVE_STARTUP_NOTIFICATION */
	
	result = gnome_vfs_mime_application_launch_with_env (application, &uris, envp);

#ifdef HAVE_STARTUP_NOTIFICATION
	if (sn_context != NULL) {
		if (result != GNOME_VFS_OK) {
			sn_launcher_context_complete (sn_context); /* end sequence */
		} else {
			add_startup_timeout (screen ? screen :
					     gdk_display_get_default_screen (gdk_display_get_default ()),
					     sn_context);
		}
		sn_launcher_context_unref (sn_context);
	}
	
	sn_display_unref (sn_display);
#endif /* HAVE_STARTUP_NOTIFICATION */

	switch (result) {
	case GNOME_VFS_OK:
		break;

	case GNOME_VFS_ERROR_NOT_SUPPORTED:
		uri_scheme = nautilus_file_get_uri_scheme (file);
		application_cannot_open_location (application,
						  file,
						  uri_scheme,
						  parent_window);
		g_free (uri_scheme);
		
		break;

	default:
#if NEW_MIME_COMPLETE
		nautilus_program_chooser_show_invalid_message
			(GNOME_VFS_MIME_ACTION_TYPE_APPLICATION, file, parent_window);
			 
#endif
		break;
	}
	
	g_free (uri);
	g_strfreev (envp);
}

/**
 * nautilus_launch_application_from_command:
 * 
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 * 
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @parameter: Passed as a parameter to the application as is.
 */
void
nautilus_launch_application_from_command (GdkScreen  *screen,
					  const char *name,
					  const char *command_string, 
					  const char *parameter, 
					  gboolean use_terminal)
{
	char *full_command;
	char *quoted_parameter; 

	if (parameter != NULL) {
		quoted_parameter = g_shell_quote (parameter);
		full_command = g_strconcat (command_string, " ", quoted_parameter, NULL);
		g_free (quoted_parameter);
	} else {
		full_command = g_strdup (command_string);
	}

	if (use_terminal) {
		eel_gnome_open_terminal_on_screen (full_command, screen);
	} else {
	    	eel_gnome_shell_execute_on_screen (full_command, screen);
	}

	g_free (full_command);
}

void
nautilus_launch_desktop_file (GdkScreen   *screen,
			      const char  *desktop_file_uri,
			      const GList *parameter_uris,
			      GtkWindow   *parent_window)
{
	GError *error;
	GnomeDesktopItem *ditem;
	GnomeDesktopItemLaunchFlags flags;
	const char *command_string;
	char *local_path, *message;
	const GList *p;
	int total, count;
	char **envp;
#ifdef HAVE_STARTUP_NOTIFICATION
	Time timestamp;
#endif

	/* strip the leading command specifier */
	if (eel_str_has_prefix (desktop_file_uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER)) {
		desktop_file_uri += strlen (NAUTILUS_DESKTOP_COMMAND_SPECIFIER);
	}

	/* Don't allow command execution from remote locations where the
	 * uri scheme isn't file:// (This is because files on for example
	 * nfs are treated as remote) to partially mitigate the security
	 * risk of executing arbitrary commands.
	 */
	if (!eel_vfs_has_capability (desktop_file_uri,
				     EEL_VFS_CAPABILITY_SAFE_TO_EXECUTE)) {
		eel_show_error_dialog
			(_("Sorry, but you can't execute commands from "
			   "a remote site."), 
			 _("This is disabled due to security considerations."),
			 parent_window);
			 
		return;
	}
	
	error = NULL;
	ditem = gnome_desktop_item_new_from_uri (desktop_file_uri, 0,
						&error);	
	if (error != NULL) {
		message = g_strconcat (_("Details: "), error->message, NULL);
		eel_show_error_dialog
			(_("There was an error launching the application."),
			 message,
			 parent_window);			
			 
		g_error_free (error);
		g_free (message);
		return;
	}
	
	/* count the number of uris with local paths */
	count = 0;
	total = g_list_length ((GList *) parameter_uris);
	for (p = parameter_uris; p != NULL; p = p->next) {
		local_path = gnome_vfs_get_local_path_from_uri ((const char *) p->data);
		if (local_path != NULL) {
			g_free (local_path);
			count++;
		}
	}

	/* check if this app only supports local files */
	command_string = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC);
	if ((strstr (command_string, "%F") || strstr (command_string, "%f"))
		&& !(strstr (command_string, "%U") || strstr (command_string, "%u"))
		&& parameter_uris != NULL) {
	
		if (count == 0) {
			/* all files are non-local */
			eel_show_error_dialog
				(_("This drop target only supports local files."),
				 _("To open non-local files copy them to a local folder and then"
				   " drop them again."),
				 parent_window);
			
			gnome_desktop_item_unref (ditem);
			return;

		} else if (count != total) {
			/* some files were non-local */
			eel_show_warning_dialog
				(_("This drop target only supports local files."),
				 _("To open non-local files copy them to a local folder and then"
				   " drop them again. The local files you dropped have already been opened."),
				 parent_window);
		}		
	}

	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);
	
	/* we append local paths only if all parameters are local */
	if (count == total) {
		flags = GNOME_DESKTOP_ITEM_LAUNCH_APPEND_PATHS;
	} else {
		flags = GNOME_DESKTOP_ITEM_LAUNCH_APPEND_URIS;
	}

	error = NULL;

#ifdef HAVE_STARTUP_NOTIFICATION
	timestamp = slowly_and_stupidly_obtain_timestamp (GDK_WINDOW_XDISPLAY (GTK_WIDGET (parent_window)->window));
	gnome_desktop_item_set_launch_time (ditem, timestamp);
#endif
	gnome_desktop_item_launch_with_env (ditem, (GList *) parameter_uris,
					    flags, envp,
					    &error);
	if (error != NULL) {
		message = g_strconcat (_("Details: "), error->message, NULL);
		eel_show_error_dialog
			(_("There was an error launching the application."),
			 message,
			 parent_window);			
			 
		g_error_free (error);
		g_free (message);
	}
	
	gnome_desktop_item_unref (ditem);
	g_strfreev (envp);
}
