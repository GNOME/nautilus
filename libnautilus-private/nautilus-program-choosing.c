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
#include "nautilus-program-chooser.h"
#include "nautilus-string.h"

#include <libgnomeui/gnome-uidefs.h>

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
			NautilusProgramChooserType type, 
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

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (callback != NULL);

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */
	dialog = set_up_program_chooser 
		(file, NAUTILUS_PROGRAM_CHOOSER_COMPONENTS, parent_window);

	if (gnome_dialog_run (dialog) == GNOME_OK) {
		/* FIXME: Need to extract result from dialog! */
		identifier = NULL;
	} else {
		identifier = NULL;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));

	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel
	 * UI if desired.
	 */
	(* callback) (identifier, callback_data);
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
	char *command_string;
	GnomeDialog *dialog;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (callback != NULL);

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */
	dialog = set_up_program_chooser 
		(file, NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS, parent_window);

	if (gnome_dialog_run (dialog) == GNOME_OK) {
		/* FIXME: Need to extract result from dialog! */
#ifdef TESTING_LAUNCH
		/* FIXME: investigate why passing wrong text here ("gnotepad")
		 * causes an X error.
		 */
		command_string = "gnp";
#else		
		command_string = NULL;
#endif		
	} else {
		command_string = NULL;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel
	 * UI if desired.
	 */
	(* callback) (command_string, callback_data);
}				    

/**
 * nautilus_launch_application:
 * 
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 * 
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @uri: Passed as a parameter to the application. "file://" is stripped
 * from the beginning if present.
 */
void
nautilus_launch_application (const char *command_string, const char *uri)
{
	const char *uri_parameter;
	pid_t new_process_id;

	if (nautilus_str_has_prefix (uri, "file://")) {
		uri_parameter = uri + 7;
	} else {
		uri_parameter = uri;
	}

	new_process_id = fork();
	if (new_process_id == 0) {
		execlp (command_string, command_string, uri_parameter, NULL);
		exit (0);
	}	
}
