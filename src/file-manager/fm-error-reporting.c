/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-error-reporting.h - implementation of file manager functions that report
 			  errors to the user.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "fm-error-reporting.h"

#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>

void
fm_report_error_renaming_file (const char *original_name,
			       const char *new_name,
			       GnomeVFSResult error)
{
	GtkWidget *message_box;
	char *message;

	g_return_if_fail (error != GNOME_VFS_OK);

	switch (error) {
		case GNOME_VFS_ERROR_FILEEXISTS:
			message = g_strdup_printf ("The name \"%s\" is already used in this folder.\nPlease use a different name.", 
						   new_name);
			break;
		default:
			/* 
			 * We should invent decent error messages for every case we actually experience.
			 * If you hit this assert, please tell John Sullivan (sullivan@eazel.com).
			 */
			g_warning ("Hit unhandled case %d in fm_report_error_renaming_file", error);
			message = g_strdup_printf ("Sorry, couldn't rename \"%s\" to \"%s\".", original_name, new_name);
	}

	message_box = gnome_message_box_new (message,
					     GNOME_MESSAGE_BOX_ERROR,
					     GNOME_STOCK_BUTTON_OK,
					     NULL);
	g_free (message);
	
	gtk_widget_show (message_box);
}		

