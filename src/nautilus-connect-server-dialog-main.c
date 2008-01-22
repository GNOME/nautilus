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
#include <libgnomeui/gnome-url.h>
#include <libgnomeui/gnome-authentication-manager.h>

#include <eel/eel-preferences.h>
#include <eel/eel-stock-dialogs.h>

#include "nautilus-window.h"
#include "nautilus-connect-server-dialog.h"

static int open_dialogs;

static void
dialog_destroyed (GtkWidget *widget,
		  gpointer   user_data)
{
	if (--open_dialogs <= 0)
		gtk_main_quit ();
}

static void
show_uri (const char *uri,
	  GdkScreen  *screen)
{
	GtkDialog *error_dialog;
	GError    *error;
	char      *error_message;

	error = NULL;
	gnome_url_show_on_screen (uri, screen, &error);

	if (error) {
		error_message = g_strdup_printf (_("Can't display location \"%s\""),
						 uri);

		error_dialog = eel_show_error_dialog (error_message,
						      error->message,
						      NULL);

		open_dialogs++;

		g_signal_connect (error_dialog, "destroy",
				  G_CALLBACK (dialog_destroyed), NULL);

		gtk_window_set_screen (GTK_WINDOW (error_dialog), screen);

		g_error_free (error);
		g_free (error_message);
	}
}

void
nautilus_connect_server_dialog_present_uri (NautilusApplication *application,
					    const char *uri,
					    GtkWidget *widget)
{
	show_uri (uri, gtk_widget_get_screen (widget));
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;
	GtkWidget *dialog;
	GOptionContext *context;
	const char **args;
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

	gtk_window_set_default_icon_name ("gnome-fs-directory");


	/* command line arguments, null terminated array */
	dialog = nautilus_connect_server_dialog_new (NULL, args != NULL ? *args : NULL);

	open_dialogs = 1;
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (dialog_destroyed), NULL);

	gtk_widget_show (dialog);

	gtk_main ();
	
	return 0;
}
