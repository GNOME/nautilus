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
#include "nautilus-program-chooser.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdlib.h>

typedef struct {
	NautilusFile *file;
	GtkWindow *parent_window;
	NautilusApplicationChoiceCallback callback;
	gpointer callback_data;
} ChooseApplicationCallbackData;

typedef struct {
	NautilusFile *file;
	GtkWindow *parent_window;
	NautilusComponentChoiceCallback callback;
	gpointer callback_data;
} ChooseComponentCallbackData;

static GHashTable *choose_application_hash_table, *choose_component_hash_table;

static guint
choose_application_hash (gconstpointer p)
{
	const ChooseApplicationCallbackData *data;

	data = p;
	return GPOINTER_TO_UINT (data->file)
		^ GPOINTER_TO_UINT (data->callback)
		^ GPOINTER_TO_UINT (data->callback_data);
}

static gboolean
choose_application_equal (gconstpointer a,
			  gconstpointer b)
{
	const ChooseApplicationCallbackData *data_a, *data_b;

	data_a = a;
	data_b = a;
	return data_a->file == data_b->file
		&& data_a->callback == data_b->callback
		&& data_a->callback_data == data_b->callback_data;
}

static void
choose_application_destroy (ChooseApplicationCallbackData *choose_data)
{
	nautilus_file_unref (choose_data->file);
	if (choose_data->parent_window != NULL) {
		gtk_object_unref (GTK_OBJECT (choose_data->parent_window));
	}
	g_free (choose_data);
}

static guint
choose_component_hash (gconstpointer p)
{
	const ChooseApplicationCallbackData *data;

	data = p;
	return GPOINTER_TO_UINT (data->file)
		^ GPOINTER_TO_UINT (data->callback)
		^ GPOINTER_TO_UINT (data->callback_data);
}

static gboolean
choose_component_equal (gconstpointer a,
			gconstpointer b)
{
	const ChooseApplicationCallbackData *data_a, *data_b;

	data_a = a;
	data_b = a;
	return data_a->file == data_b->file
		&& data_a->callback == data_b->callback
		&& data_a->callback_data == data_b->callback_data;
}

static void
choose_component_destroy (ChooseComponentCallbackData *choose_data)
{
	nautilus_file_unref (choose_data->file);
	if (choose_data->parent_window != NULL) {
		gtk_object_unref (GTK_OBJECT (choose_data->parent_window));
	}
	g_free (choose_data);
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

static void
choose_component_callback (NautilusFile *file,
			   gpointer callback_data)
{
	ChooseComponentCallbackData *choose_data;
	NautilusViewIdentifier *identifier;
	GnomeDialog *dialog;

	choose_data = callback_data;

	/* Remove from the hash table. */
	g_assert (g_hash_table_lookup (choose_component_hash_table,
				       choose_data) == choose_data);
	g_hash_table_remove (choose_component_hash_table,
			     choose_data);

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */

	identifier = NULL;
	dialog = NULL;
	if (nautilus_mime_has_any_components_for_file (file)) {
		dialog = set_up_program_chooser (file, GNOME_VFS_MIME_ACTION_TYPE_COMPONENT,
						 choose_data->parent_window);
		if (gnome_dialog_run (dialog) == GNOME_OK) {
			identifier = nautilus_program_chooser_get_component (dialog);
		}
	} else {
		nautilus_program_chooser_show_no_choices_message (GNOME_VFS_MIME_ACTION_TYPE_COMPONENT,
								  file,
								  choose_data->parent_window);
	}
	 
	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel UI
	 * if desired.
	 */
	(* choose_data->callback) (identifier, choose_data->callback_data);

	if (dialog != NULL) {
		/* Destroy only after callback, since view identifier will
		 * be destroyed too.
		 */
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}

	choose_component_destroy (choose_data);
}

void
nautilus_choose_component_for_file (NautilusFile *file,
				    GtkWindow *parent_window,
				    NautilusComponentChoiceCallback callback,
				    gpointer callback_data)
{
	ChooseComponentCallbackData *choose_data;
	GList *attributes;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window));
	g_return_if_fail (callback != NULL);

	/* Grab refs to the objects so they will still be around at
	 * callback time.
	 */
	nautilus_file_ref (file);
	if (parent_window != NULL) {
		gtk_object_ref (GTK_OBJECT (parent_window));
	}

	/* Create data to pass through. */
	choose_data = g_new (ChooseComponentCallbackData, 1);
	choose_data->file = file;
	choose_data->parent_window = parent_window;
	choose_data->callback = callback;
	choose_data->callback_data = callback_data;

	/* Put pending entry into choose hash table. */
	if (choose_component_hash_table == NULL) {
		choose_component_hash_table = eel_g_hash_table_new_free_at_exit
			(choose_component_hash,
			 choose_component_equal,
			 "choose component");
	}
	g_hash_table_insert (choose_component_hash_table,
			     choose_data, choose_data);
	
	/* Do the rest of the work when the attributes are ready. */
	attributes = nautilus_mime_actions_get_full_file_attributes ();
	nautilus_file_call_when_ready (file,
				       attributes,
				       choose_component_callback,
				       choose_data);
	g_list_free (attributes);
}

void
nautilus_cancel_choose_component_for_file (NautilusFile *file,
					   NautilusComponentChoiceCallback callback,
					   gpointer callback_data)
{
	ChooseComponentCallbackData search_criteria;
	ChooseComponentCallbackData *choose_data;

	if (choose_component_hash_table == NULL) {
		return;
	}

	/* Search for an existing choose in progress. */
	search_criteria.file = file;
	search_criteria.callback = callback;
	search_criteria.callback_data = callback_data;
	choose_data = g_hash_table_lookup (choose_component_hash_table,
					   &search_criteria);
	if (choose_data == NULL) {
		return;
	}

	/* Stop it. */
	g_hash_table_remove (choose_component_hash_table,
			     choose_data);
	nautilus_file_cancel_call_when_ready (file,
					      choose_component_callback,
					      choose_data);
	choose_component_destroy (choose_data);
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

static void
choose_application_callback (NautilusFile *file,
			     gpointer callback_data)
{
	ChooseApplicationCallbackData *choose_data;
	GnomeDialog *dialog;
	GnomeVFSMimeApplication *application;

	choose_data = callback_data;

	/* Remove from the hash table. */
	g_assert (g_hash_table_lookup (choose_application_hash_table,
				       choose_data) == choose_data);
	g_hash_table_remove (choose_application_hash_table,
			     choose_data);

	/* The API uses a callback so we can do this non-modally in the future,
	 * but for now we just use a modal dialog.
	 */
	application = NULL;
	dialog = NULL;

	if (nautilus_mime_has_any_applications_for_file_type (file)) {
		dialog = set_up_program_chooser	(file, GNOME_VFS_MIME_ACTION_TYPE_APPLICATION,
						 choose_data->parent_window);
		if (gnome_dialog_run (dialog) == GNOME_OK) {
			application = nautilus_program_chooser_get_application (dialog);
		}
	} else {
		nautilus_program_chooser_show_no_choices_message (GNOME_VFS_MIME_ACTION_TYPE_APPLICATION,
								  file,
								  choose_data->parent_window);
	}	 

	/* Call callback even if identifier is NULL, so caller can
	 * free callback_data if necessary and present some cancel
	 * UI if desired.
	 */
	(* choose_data->callback) (application, choose_data->callback_data);

	if (dialog != NULL) {
		/* Destroy only after callback, since application struct will
		 * be destroyed too.
		 */
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}

	choose_application_destroy (choose_data);
}

void
nautilus_choose_application_for_file (NautilusFile *file,
				      GtkWindow *parent_window,
				      NautilusApplicationChoiceCallback callback,
				      gpointer callback_data)
{
	ChooseApplicationCallbackData *choose_data;
	GList *attributes;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window));
	g_return_if_fail (callback != NULL);

	/* Grab refs to the objects so they will still be around at
	 * callback time.
	 */
	nautilus_file_ref (file);
	if (parent_window != NULL) {
		gtk_object_ref (GTK_OBJECT (parent_window));
	}

	/* Create data to pass through. */
	choose_data = g_new (ChooseApplicationCallbackData, 1);
	choose_data->file = file;
	choose_data->parent_window = parent_window;
	choose_data->callback = callback;
	choose_data->callback_data = callback_data;

	/* Put pending entry into choose hash table. */
	if (choose_application_hash_table == NULL) {
		choose_application_hash_table = eel_g_hash_table_new_free_at_exit
			(choose_application_hash,
			 choose_application_equal,
			 "choose application");
	}
	g_hash_table_insert (choose_application_hash_table,
			     choose_data, choose_data);
	
	/* Do the rest of the work when the attributes are ready. */
	attributes = nautilus_mime_actions_get_full_file_attributes ();
	nautilus_file_call_when_ready (file,
				       attributes,
				       choose_application_callback,
				       choose_data);
	g_list_free (attributes);
}


typedef struct {
	NautilusFile *file;
	GtkWindow *parent_window;
} LaunchParameters;

static LaunchParameters *
launch_parameters_new (NautilusFile *file,
		       GtkWindow *parent_window)
{
	LaunchParameters *launch_parameters;

	launch_parameters = g_new0 (LaunchParameters, 1);
	nautilus_file_ref (file);
	launch_parameters->file = file;
	gtk_widget_ref (GTK_WIDGET (parent_window));
	launch_parameters->parent_window = parent_window;

	return launch_parameters;
}

static void
launch_parameters_free (LaunchParameters *launch_parameters)
{
	g_assert (launch_parameters != NULL);

	nautilus_file_unref (launch_parameters->file);
	gtk_widget_unref (GTK_WIDGET (launch_parameters->parent_window));
	
	g_free (launch_parameters);
}

static void
launch_application_callback (GnomeVFSMimeApplication *application,
			     gpointer callback_data)
{
	LaunchParameters *launch_parameters;

	g_assert (callback_data != NULL);

	launch_parameters = (LaunchParameters *) callback_data;

	if (application != NULL) {
		g_assert (NAUTILUS_IS_FILE (launch_parameters->file));
		
		nautilus_launch_application (application, 
					     launch_parameters->file,
					     launch_parameters->parent_window);
	}

	launch_parameters_free (launch_parameters);
	
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
	GnomeDialog *message_dialog;
	LaunchParameters *launch_parameters;
	char *message;
	char *file_name;

	file_name = nautilus_file_get_display_name (file);

	if (nautilus_mime_has_any_applications_for_file (file)) {
		message = g_strdup_printf (_("\"%s\" can't open \"%s\" because \"%s\" can't access files at \"%s\" "
					     "locations.  Would you like to choose another application?"),
					   application->name, file_name, 
					   application->name, uri_scheme);
		message_dialog = eel_show_yes_no_dialog (message, 
							       _("Can't Open Location"), 
							       GNOME_STOCK_BUTTON_OK, 
							       GNOME_STOCK_BUTTON_CANCEL,
							       parent_window);
		if (gnome_dialog_run (message_dialog) == GNOME_OK) {
			launch_parameters = launch_parameters_new (file, parent_window);
			nautilus_choose_application_for_file 
				(file,
				 parent_window,
				 launch_application_callback,
				 launch_parameters);
				 
		}
		
	}
	else {
		message = g_strdup_printf (_("\"%s\" can't open \"%s\" because \"%s\" can't access files at \"%s\" "
					     "locations.  No other applications are available to view this file.  "
					     "If you copy this file onto your computer, you may be able to open "
					     "it."), application->name, file_name, 
					   application->name, uri_scheme);
		eel_show_info_dialog (message, _("Can't Open Location"), parent_window);
	}

	g_free (file_name);
	g_free (message);
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
	char *parameter;
	char *uri_scheme, *uri;

	uri_scheme = nautilus_file_get_uri_scheme (file);

	/* If the program can open URIs, always use a URI. This
	 * prevents any possible ambiguity for cases where a path
	 * would looks like a URI.
	 */
	if (application->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS ||
	    ((application->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS_FOR_NON_FILES) &&
	     eel_strcasecmp (uri_scheme, "file") != 0)) {
		/* Check to be sure that the application also supports this particular URI scheme */
		if (g_list_find_custom (application->supported_uri_schemes,
					uri_scheme,
					eel_strcmp_compare_func) == NULL) {
			application_cannot_open_location (application,
							  file,
							  uri_scheme,
							  parent_window);
			g_free (uri_scheme);
			return;
		}
		parameter = nautilus_file_get_uri (file);
	} else {
		uri = nautilus_file_get_uri (file);
		parameter = gnome_vfs_get_local_path_from_uri (uri);
		g_free (uri);

		if (parameter == NULL) {
			/* This application can't deal with this URI,
			 * because it can only handle local
			 * files. Tell user. Some day we could offer
			 * to copy it locally for the user, if we knew
			 * where to put it, and who would delete it
			 * when done.
			 */
			application_cannot_open_location (application,
							  file,
							  uri_scheme,
							  parent_window);
			g_free (uri_scheme);
			return;
		}
	}
	g_free (uri_scheme);

	nautilus_launch_application_from_command (application->name,
						  application->command,
						  parameter, 
						  application->requires_terminal);

	g_free (parameter);
}

static char *
get_xalf_prefix (const char *name)
{
	char *xalf_executable;
	GString *s;
	int argc, i;
	char **argv;
	char *quoted;
	char *prefix;

	/* FIXME bugzilla.gnome.org 48206: At time I am writing this,
	 * xalf is still pretty buggy, and Nautilus uses it a lot more
	 * than the Panel does with no way to turn it off for
	 * individual programs the way you can in the Panel. Sadly,
	 * Ximian GNOME 1.4 ships with xalf on by default. So we did
	 * this lame thing and turned it off unless you define this
	 * environment variable. Some day we can remove this.
	 */
	if (g_getenv ("NAUTILUS_USE_XALF") == NULL) {
		return g_strdup ("");
	}
	if (!gnome_config_get_bool ("/xalf/settings/enabled=true")) {
		return g_strdup ("");
	}
	xalf_executable = gnome_is_program_in_path ("xalf");
	if (xalf_executable == NULL) {
		return g_strdup ("");
	}

	s = g_string_new (xalf_executable);
	g_string_append (s, " --title ");
	quoted = eel_shell_quote (name);
	g_string_append (s, quoted);
	g_free (quoted);
	g_string_append_c (s, ' ');

	gnome_config_get_vector ("/xalf/settings/options",
				 &argc, &argv);
	for (i = 0; i < argc; i++) {
		quoted = eel_shell_quote (argv[i]);
		g_free (argv[i]);

		g_string_append (s, quoted);
		g_string_append_c (s, ' ');

		g_free (quoted);
	}
	g_free (argv);

	prefix = s->str;
	g_string_free (s, FALSE);
	return prefix;
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
nautilus_launch_application_from_command (const char *name,
					  const char *command_string, 
					  const char *parameter, 
					  gboolean use_terminal)
{
	char *full_command;
	char *quoted_parameter; 
	char *final_command;
	char *xalf_prefix;

	if (parameter != NULL) {
		quoted_parameter = eel_shell_quote (parameter);
		full_command = g_strconcat (command_string, " ", quoted_parameter, NULL);
		g_free (quoted_parameter);
	} else {
		full_command = g_strdup (command_string);
	}

	xalf_prefix = get_xalf_prefix (name);

	if (use_terminal) {
		final_command = g_strconcat (xalf_prefix, full_command, NULL);
		eel_gnome_open_terminal (final_command);
	} else {
		final_command = g_strconcat (xalf_prefix, full_command, " &", NULL);
		system (final_command);
	}

	g_free (final_command);
	g_free (full_command);
	g_free (xalf_prefix);
}
