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
#include "nautilus-icon-info.h"
#include "nautilus-recent.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <eel/eel-app-launch-context.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-config.h>
#include <glib/gi18n.h>
#include <gio/gfileicon.h>
#include <gio/gthemedicon.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnome/gnome-url.h>
#include <libgnomeui/gnome-uidefs.h>
#include <stdlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

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
application_cannot_open_location (GAppInfo *application,
				  NautilusFile *file,
				  const char *uri_scheme,
				  GtkWindow *parent_window)
{
#ifdef NEW_MIME_COMPLETE
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
						   g_app_info_get_name (application), file_name, 
						   g_app_info_get_name (application), uri_scheme);
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
			prompt = g_strdup_printf (_("\"%s\" can't open \"%s\" because \"%s\" can't access files at \"%s\" "
						    "locations."), g_app_info_get_name (application), file_name, 
						    g_app_info_get_name (application), uri_scheme);
			message = _("No other applications are available to view this file.  "
				    "If you copy this file onto your computer, you may be able to open "
				    "it.");
		} else {
			prompt = g_strdup_printf (_("The default action can't open \"%s\" because it can't access files at \"%s\" "
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


/**
 * nautilus_launch_application:
 * 
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs. 
 * 
 * @application: The application to be launched.
 * @files: The files whose locations should be passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
nautilus_launch_application (GAppInfo *application, 
			     GList *files,
			     GtkWindow *parent_window)
{
	char		*uri;
	char            *uri_scheme;
	GList           *locations, *l;
	GFile *location;
	NautilusFile    *file;
	gboolean        result;
	GError *error;
	EelAppLaunchContext *launch_context;
	NautilusIconInfo *icon;

	g_assert (files != NULL);

	locations = NULL;
	for (l = files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		
		location = NULL;

		if (nautilus_file_is_nautilus_link (file)) {
			uri = nautilus_file_get_activation_uri (file);
			location = g_file_new_for_uri (uri);
			g_free (uri);
		}
		
		if (location == NULL) {
			location = nautilus_file_get_location (file);
		}

		locations = g_list_prepend (locations, location);
	}
	locations = g_list_reverse (locations);

	launch_context = eel_app_launch_context_new ();
	if (parent_window)
		eel_app_launch_context_set_screen (launch_context,
						     gtk_window_get_screen (parent_window));

	file = NAUTILUS_FILE (files->data);
	icon = nautilus_file_get_icon (file, 48, 0);
	if (icon) {
		eel_app_launch_context_set_icon_name (launch_context,
							nautilus_icon_info_get_used_name (icon));
		g_object_unref (icon);
	}
	
	error = NULL;
	result = g_app_info_launch (application,
				    locations,
				    G_APP_LAUNCH_CONTEXT (launch_context),
				    &error);

	g_object_unref (launch_context);
	
	if (!result) {
		if (error->domain == G_IO_ERROR &&
		    error->code == G_IO_ERROR_NOT_SUPPORTED) {
			uri_scheme = nautilus_file_get_uri_scheme (NAUTILUS_FILE (files->data));
			application_cannot_open_location (application,
							  file,
							  uri_scheme,
							  parent_window);
			g_free (uri_scheme);
		} else {
#ifdef NEW_MIME_COMPLETE
			nautilus_program_chooser_show_invalid_message
				(GNOME_VFS_MIME_ACTION_TYPE_APPLICATION, file, parent_window);
#else
			g_warning ("Can't open app: %s\n", error->message);
#endif
		}
	} else {
		for (l = files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);
			
			nautilus_recent_add_file (file, application);
		}
	}

	eel_g_object_list_free (locations);
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
	    	gdk_spawn_command_line_on_screen (screen, full_command, NULL);
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
	GFile *file;
	Time timestamp;

	/* strip the leading command specifier */
	if (eel_str_has_prefix (desktop_file_uri, NAUTILUS_DESKTOP_COMMAND_SPECIFIER)) {
		desktop_file_uri += strlen (NAUTILUS_DESKTOP_COMMAND_SPECIFIER);
	}

	/* Don't allow command execution from remote locations where the
	 * uri scheme isn't file:// (This is because files on for example
	 * nfs are treated as remote) to partially mitigate the security
	 * risk of executing arbitrary commands.
	 */
	file = g_file_new_for_uri (desktop_file_uri);
	if (!g_file_is_native (file)) {
		g_object_unref (file);
		eel_show_error_dialog
			(_("Sorry, but you can't execute commands from "
			   "a remote site."), 
			 _("This is disabled due to security considerations."),
			 parent_window);
			 
		return;
	}
	g_object_unref (file);
	
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
		local_path = g_filename_from_uri ((const char *) p->data, NULL, NULL);
		if (local_path != NULL) {
			g_free (local_path);
			count++;
		}
	}

	/* check if this app only supports local files */
	command_string = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_EXEC);
	if (command_string != NULL && (strstr (command_string, "%F") || strstr (command_string, "%f"))
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

	timestamp = slowly_and_stupidly_obtain_timestamp (GDK_WINDOW_XDISPLAY (GTK_WIDGET (parent_window)->window));
	gnome_desktop_item_set_launch_time (ditem, timestamp);
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
