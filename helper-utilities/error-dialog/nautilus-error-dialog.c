/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-error-dialog.c - A very simple program used to post an
 * error dialog.
 */

#include <config.h>

#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <gnome.h>
#include <popt.h>

int main (int argc, char *argv[])
{
	GnomeDialog *error_dialog;
	poptContext popt_context;

	char *title = NULL;
	char *message = NULL;

	const char *default_title = "Default Title";
	const char *default_message = "Default Message";

	const char *the_title = NULL;
	const char *the_message = NULL;
	
	struct poptOption options[] = {
		{ "message", '\0', POPT_ARG_STRING, &message, 0, N_("Message."), NULL },
		{ "title", '\0', POPT_ARG_STRING, &title, 0, N_("Title."), NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	
        gnome_init_with_popt_table ("nautilus-error-dialog",
				    VERSION,
				    argc,
				    argv,
				    options,
				    0,
				    &popt_context);

	the_title = title ? title : default_title;
	the_message = message ? message : default_message;
	
 	error_dialog = nautilus_error_dialog (the_message, the_title, NULL);

	gnome_dialog_run_and_close (GNOME_DIALOG (error_dialog));

	return 0;
}
