/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-chooser.c - implementation for window that lets user choose 
                                a program from a list

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
#include "nautilus-program-chooser.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkvbox.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

GnomeDialog *
nautilus_program_chooser_new (NautilusProgramChooserType type,
			      NautilusFile *file)
{
	GtkWidget *window;
	GtkWidget *prompt_label;
	char *file_name, *prompt;
	const char *title;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	file_name = nautilus_file_get_name (file);

	switch (type) {
		case NAUTILUS_PROGRAM_CHOOSER_APPLICATIONS:
			title = _("Nautilus: Choose an application");
			prompt = g_strdup_printf (_("Choose an application with which to open \"%s\"\n\n(Doesn't do anything yet)"), file_name);
			break;
		case NAUTILUS_PROGRAM_CHOOSER_COMPONENTS:
		default:
			title = _("Nautilus: Choose a viewer");
			prompt = g_strdup_printf (_("Choose a viewer with which to display \"%s\"\n\n(Doesn't do anything yet)"), file_name);
			break;
	}
	g_free (file_name);

	window = gnome_dialog_new (title, 
				   GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, 
				   NULL);
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);

	prompt_label = gtk_label_new (prompt);
	gtk_widget_show (prompt_label);
  	g_free (prompt);

  	gtk_box_pack_start_defaults (GTK_BOX (GNOME_DIALOG (window)->vbox), 
  				     prompt_label);

	/* Buttons close this dialog. */
  	gnome_dialog_set_close (GNOME_DIALOG (window), TRUE);

  	return GNOME_DIALOG (window);
}