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

#include "nautilus-glib-extensions.h"
#include "nautilus-mime-actions.h"
#include "nautilus-program-chooser.h"
#include "nautilus-stock-dialogs.h"
#include "nautilus-string.h"
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdlib.h>

/* FIXME bugzilla.eazel.com 4539: Eliminate this soon. */
#include "nautilus-wait-until-ready.h"

static gboolean
any_programs_available_for_file (GnomeVFSMimeActionType action_type, NautilusFile *file)
{
	gboolean result;

	nautilus_mime_actions_wait_for_full_file_attributes (file);

	if (action_type == GNOME_VFS_MIME_ACTION_TYPE_COMPONENT) {
		result = nautilus_mime_has_any_components_for_file (file);
	} else {
		g_assert (action_type == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
		result = nautilus_mime_has_any_applications_for_file (file);
	}

	return result;
}

/**
 * set_up_program_chooser:
 * 
 * Create but don't yet run a program-choosing dialog.
 * The caller should run the dialog and destroy it.
 * 
 * @file: Which NautilusFile programs are being chosen for.
 * @type: Which type of program is being chosen.
 * @parent: Optional window to parent the dialog on.
 * 
 * Return value: The program-choosing dialog, ready to be run.
 */
static GnomeDialog *
set_up_program_chooser (NautilusFile *file, 
			GnomeVFSMimeActionType type, 
			GtkWindow *parent)
{
	GnomeDialog *dialog;

	g_assert (NAUTILUS_IS_FILE (file));

	dialog = nautilus_program_chooser_new (type, file);
	if (parent != NULL) {
		gnome_dialog_set_parent (dialog, parent);
	}

	/* Don't destroy on close because callers will need 
	 * to extract some information from the dialog after 
	 * it closes.
	 */
	gnome_dialog_close_hides (dialog, TRUE);

	return dialog;	
}


/**
 * nautilus_choose_component_for_file:
 * 
 * Lets user choose a component with which to view a given file.
 * 
 * @file: The NautilusFile to be viewed.
 * @parent_window: If supplied, the component-choosing dialog is parented
 * on this window.
 * @callback: Callback called when choice has been made.
 * @callback_data: Parameter passed back when callback is called.
 */
void
nautilus_choose_component_for_file (NautilusFile *file,
				    GtkWindow *parent_window,
				    NautilusComponentChoiceCallback callback,
				    gpointer callback_data)
{
	NautilusViewIdentifier *identifier;
	GnomeDialog *dialog;
	gboolean any_choices;
	GnomeVFSMimeActionType action_type;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (callback != NULL);

	action_type = GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */
	any_choices = any_programs_available_for_file (action_type, file);

	identifier = NULL;
	dialog = NULL;

	if (any_choices) {
		dialog = set_up_program_chooser (file, action_type, parent_window);

		if (gnome_dialog_run (dialog) == GNOME_OK) {
			identifier = nautilus_program_chooser_get_component (dialog);
		}
	} else {
		nautilus_program_chooser_show_no_choices_message (action_type, file, parent_window);
	}
	 

	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel
	 * UI if desired.
	 */
	(* callback) (identifier, callback_data);

	if (any_choices) {
		/* Destroy only after callback, since view identifier will
		 * be destroyed too.
		 */
		g_assert (dialog != NULL);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}				    

/**
 * nautilus_choose_application_for_file:
 * 
 * Lets user choose an application with which to open a given file.
 * 
 * @file: The NautilusFile to be viewed.
 * @parent_window: If supplied, the application-choosing dialog is parented
 * on this window.
 * @callback: Callback called when choice has been made.
 * @callback_data: Parameter passed back when callback is called.
 */
void
nautilus_choose_application_for_file (NautilusFile *file,
				      GtkWindow *parent_window,
				      NautilusApplicationChoiceCallback callback,
				      gpointer callback_data)
{
	GnomeDialog *dialog;
	GnomeVFSMimeApplication *application;
	gboolean any_choices;
	GnomeVFSMimeActionType action_type;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (callback != NULL);

	action_type = GNOME_VFS_MIME_ACTION_TYPE_APPLICATION;

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */
	any_choices = any_programs_available_for_file (action_type, file);

	application = NULL;
	dialog = NULL;

	if (any_choices) {
		dialog = set_up_program_chooser 
			(file, action_type, parent_window);

		if (gnome_dialog_run (dialog) == GNOME_OK) {
			application = nautilus_program_chooser_get_application (dialog);
		}
	} else {
		nautilus_program_chooser_show_no_choices_message (action_type, file, parent_window);
	}	 

	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel
	 * UI if desired.
	 */
	(* callback) (application, callback_data);

	if (any_choices) {
		/* Destroy only after callback, since application struct will
		 * be destroyed too.
		 */
		g_assert (dialog != NULL);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}				    

/**
 * nautilus_launch_application:
 * 
 * Fork off a process to launch an application with a given uri as
 * a parameter. Provide a parent window for error dialogs.
 * 
 * @application: The application to be launched.
 * @uri: Passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 * 
 */
void
nautilus_launch_application (GnomeVFSMimeApplication *application, 
			     const char *uri, 
			     GtkWindow *parent_window)
{
	GnomeDialog *dialog;
	char *command_string;
	char *parameter;
	char *prompt;

	/* If the program can open URIs, always use a URI. This
	 * prevents any possible ambiguity for cases where a path
	 * would looks like a URI.
	 */
	if (application->can_open_uris) {
		parameter = g_strdup (uri);
	} else {
		parameter = gnome_vfs_get_local_path_from_uri (uri);
		if (parameter == NULL) {
			/* This application can't deal with this URI,
			 * because it can only handle local
			 * files. Tell user. Some day we could offer
			 * to copy it locally for the user, if we knew
			 * where to put it, and who would delete it
			 * when done.
			 */
			prompt = g_strdup_printf (_("Sorry, %s can only open local files, and "
						    "\"%s\" is remote. If you want to open it "
						    "with %s, make a local copy first."), 
						  application->name, uri, application->name);
			dialog = nautilus_error_dialog (prompt, _("Can't open remote file"), parent_window);
			g_free (prompt);
			return;
		}
	}
	
	if (application->requires_terminal) {
		command_string = g_strconcat ("gnome-terminal -x ", application->command, NULL);
	} else {
		command_string = g_strdup (application->command);
	}
	
	nautilus_launch_application_from_command (command_string, parameter);
	
	g_free (parameter);
	g_free (command_string);
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
nautilus_launch_application_from_command (const char *command_string, const char *parameter)
{
	char *full_command, *quoted_parameter, *quoted_command;

	if (parameter != NULL) {
		quoted_parameter = nautilus_shell_quote (parameter);
		quoted_command = nautilus_shell_quote (command_string);
		full_command = g_strconcat (quoted_command, " ", quoted_parameter, " &", NULL);
		g_free (quoted_command);
		g_free (quoted_parameter);
	} else {
		quoted_command = nautilus_shell_quote (command_string);
		full_command = g_strconcat (quoted_command, " &", NULL);
		g_free (quoted_command);
	}

	system (full_command);

	g_free (full_command);
}
