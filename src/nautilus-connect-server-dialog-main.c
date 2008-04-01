/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-connect-server-main.c - Start the "Connect to Server" dialog.
 * Nautilus
 *
 * Copyright (C) 2005 Vincent Untz
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Vincent Untz <vincent@vuntz.net>
 */

#include <config.h>

#include <glib/gi18n.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-authentication-manager.h>

#include <eel/eel-app-launch-context.h>
#include <eel/eel-preferences.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-mount-operation.h>

#include <libnautilus-private/nautilus-icon-names.h>

#include "nautilus-window.h"
#include "nautilus-connect-server-dialog.h"

static int open_dialogs;

static void
main_dialog_destroyed (GtkWidget *widget,
		       gpointer   user_data)
{
	/* this only happens when user clicks "cancel"
	 * on the main dialog or when we are all done.
	 */
	gtk_main_quit ();
}

static void
error_dialog_destroyed (GtkWidget *widget,
			GtkWidget *main_dialog)
{
	if (--open_dialogs <= 0)
		gtk_widget_destroy (main_dialog);
}

static void
display_error_dialog (GError *error, 
		      const char *uri,
		      GtkWidget *parent)
{
	GtkDialog *error_dialog;
	char *error_message;

	error_message = g_strdup_printf (_("Cannot display location \"%s\""),
					 uri);
	error_dialog = eel_show_error_dialog (error_message,
					      error->message,
					      NULL);

	open_dialogs++;

	g_signal_connect (error_dialog, "destroy",
			  G_CALLBACK (error_dialog_destroyed), parent);

	gtk_window_set_screen (GTK_WINDOW (error_dialog),
			       gtk_widget_get_screen (parent));

	g_free (error_message);
}

static void
show_uri (const char *uri,
	  GtkWidget  *widget)
{
	GError    *error;
	EelAppLaunchContext *launch_context;

	launch_context = eel_app_launch_context_new ();
	eel_app_launch_context_set_screen (launch_context,
					   gtk_widget_get_screen (widget));

	error = NULL;
	g_app_info_launch_default_for_uri (uri,
					   G_APP_LAUNCH_CONTEXT (launch_context),
					   &error);

	g_object_unref (launch_context);

	if (error) {
		display_error_dialog (error, uri, widget);
		g_error_free (error);
	} else {
		/* everything is OK, destroy the main dialog and quit */
		gtk_widget_destroy (widget);
	}
}

static void
mount_enclosing_ready_cb (GFile *location,
			  GAsyncResult *res,
			  GtkWidget *widget)
{
	char *uri;
	GError *error = NULL;
	
	g_file_mount_enclosing_volume_finish (location,
					      res, &error);
	uri = g_file_get_uri (location);
	if (error) {
		display_error_dialog (error, uri, widget);
	} else {
		/* volume is mounted, show it */
		show_uri (uri, widget);
		g_object_unref (location);
	}
	g_free (uri);
}

void
nautilus_connect_server_dialog_present_uri (NautilusApplication *application,
					    GFile *location,
					    GtkWidget *widget)
{
	GMountOperation *op;

	op = eel_mount_operation_new (GTK_WINDOW (widget));
	g_file_mount_enclosing_volume (location,
				       0, op,
				       NULL,
				       (GAsyncReadyCallback) mount_enclosing_ready_cb,
				       widget);
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;
	GtkWidget *dialog;
	GOptionContext *context;
	const char **args;
	GFile *location;
	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL,  N_("[URI]") },
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	args = NULL;
	/* Translators: This is the --help description gor the connect to server app,
	   the initial newlines are between the command line arg and the description */
	context = g_option_context_new (N_("\n\nAdd connect to server mount"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

	program = gnome_program_init ("nautilus-connect-server", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      NULL);

	gnome_authentication_manager_init ();

	eel_preferences_init ("/apps/nautilus");

	gtk_window_set_default_icon_name (NAUTILUS_ICON_FOLDER);


	/* command line arguments, null terminated array */
	location = NULL;
	if (args) {
		location = g_file_new_for_commandline_arg (*args);
	}

	dialog = nautilus_connect_server_dialog_new (NULL, location);

	if (location) {
		g_object_unref (location);
	}

	open_dialogs = 0;
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (main_dialog_destroyed), NULL);

	gtk_widget_show (dialog);

	gtk_main ();
	
	return 0;
}
