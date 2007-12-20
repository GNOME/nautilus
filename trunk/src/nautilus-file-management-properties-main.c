/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-management-properties-main.c - Start the nautilus-file-management preference dialog.

   Copyright (C) 2002 Jan Arne Petersen

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

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#include <config.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkmain.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libnautilus-private/nautilus-module.h>

#include <libintl.h>

#include <eel/eel-preferences.h>

#include "nautilus-file-management-properties.h"

static void
nautilus_file_management_properties_main_close_callback (GtkDialog *dialog,
							 int response_id)
{
	if (response_id == GTK_RESPONSE_CLOSE) {
		gtk_main_quit ();
	}
}

int
main (int argc, char *argv[])
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("file-managment-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    NULL, NULL);

	eel_preferences_init ("/apps/nautilus");

	nautilus_module_init ();

	nautilus_file_management_properties_dialog_show (G_CALLBACK (nautilus_file_management_properties_main_close_callback), NULL);

	gtk_main ();
	
	return 0;
}
