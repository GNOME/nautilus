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

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libnautilus-extensions/nautilus-stock-dialogs.h>

void
fm_report_error_renaming_file (NautilusFile *file,
			       const char *new_name,
			       GnomeVFSResult error)
{
	char *original_name;
	char *message;

	switch (error) {
	case GNOME_VFS_OK:
		return;
	case GNOME_VFS_ERROR_FILE_EXISTS:
		message = g_strdup_printf (_("The name \"%s\" is already used in this directory.\nPlease use a different name."), 
					   new_name);
		break;
	case GNOME_VFS_ERROR_ACCESS_DENIED:
		original_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("You do not have the permissions necessary to rename \"%s.\""),
					   original_name);
		g_free (original_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d in fm_report_error_renaming_file, tell sullivan@eazel.com", error);
		/* fall through */
		original_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("Sorry, couldn't rename \"%s\" to \"%s\"."),
					   original_name, new_name);
		g_free (original_name);
	}

	nautilus_error_dialog (message);
	g_free (message);
}

void
fm_report_error_setting_group (NautilusFile *file,
			       GnomeVFSResult error)
{
	char *file_name;
	char *message;

	switch (error) {
	case GNOME_VFS_OK:
		return;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d in fm_report_error_setting_group, tell sullivan@eazel.com", error);
		file_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("Sorry, couldn't change the group of \"%s\"."), file_name);
		g_free (file_name);
	}

	nautilus_error_dialog (message);
	g_free (message);
}		

void
fm_report_error_setting_owner (NautilusFile *file,
			       GnomeVFSResult error)
{
	char *file_name;
	char *message;

	switch (error) {
	case GNOME_VFS_OK:
		return;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d in fm_report_error_setting_owner, tell sullivan@eazel.com", error);
		file_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("Sorry, couldn't change the owner of \"%s\"."), file_name);
		g_free (file_name);
	}

	nautilus_error_dialog (message);
	g_free (message);
}		

void
fm_report_error_setting_permissions (NautilusFile *file,
			       	     GnomeVFSResult error)
{
	char *file_name;
	char *message;

	switch (error) {
	case GNOME_VFS_OK:
		return;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d in fm_report_error_setting_permissions, tell sullivan@eazel.com", error);
		file_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("Sorry, couldn't change the permissions of \"%s\"."), file_name);
		g_free (file_name);
	}

	nautilus_error_dialog (message);
	g_free (message);
}		
