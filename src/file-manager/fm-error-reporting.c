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

#include <libnautilus-extensions/nautilus-file.h>

static void cancel_rename_callback (gpointer callback_data);

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

	nautilus_error_dialog (message, NULL);
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
	case GNOME_VFS_ERROR_NOT_PERMITTED:
		file_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("You do not have the permissions necessary to change the group of \"%s\"."),
					   file_name);
		g_free (file_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d in fm_report_error_setting_group, tell sullivan@eazel.com", error);
		file_name = nautilus_file_get_name (file);
		message = g_strdup_printf (_("Sorry, couldn't change the group of \"%s\"."), file_name);
		g_free (file_name);
	}

	nautilus_error_dialog (message, NULL);
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

	nautilus_error_dialog (message, NULL);
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

	nautilus_error_dialog (message, NULL);
	g_free (message);
}		

typedef struct {
	NautilusFile *file;
	char *new_name;
} RenameCallbackData;

static void
rename_callback_data_free (RenameCallbackData *data)
{
	g_assert (NAUTILUS_IS_FILE (data->file));
	g_assert (data->new_name != NULL);

	nautilus_file_unref (data->file);
	g_free (data->new_name);
	g_free (data);
}

static void
rename_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	RenameCallbackData *data;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (callback_data != NULL);

	data = (RenameCallbackData *) callback_data;

	g_assert (file == data->file);
	g_assert (data->new_name != NULL);

	/* We are done, no need to cancel any more. */
	nautilus_timed_wait_stop (cancel_rename_callback, data);

	/* If rename failed, notify the user. */
	fm_report_error_renaming_file (file, data->new_name, result);

	/* Done with the callback data. */
	rename_callback_data_free (data);
}

static void
cancel_rename_callback (gpointer callback_data)
{
	RenameCallbackData *data;

	g_assert (callback_data != NULL);

	data = (RenameCallbackData *) callback_data;

	g_assert (NAUTILUS_IS_FILE (data->file));
	g_assert (data->new_name != NULL);

	/* Cancel the renaming. */
	nautilus_file_cancel (data->file, rename_callback, callback_data);

	/* Done with the callback data. */
	rename_callback_data_free (data);
}

void
fm_rename_file (NautilusFile *file,
		const char *new_name)
{
	RenameCallbackData *data;
	char *old_name, *wait_message;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (new_name != NULL);

	old_name = nautilus_file_get_name (file);
	wait_message = g_strdup_printf (_("Renaming %s to %s."),
					old_name,
					new_name);
	g_free (old_name);

	/* Start the rename. */
	data = g_new (RenameCallbackData, 1);
	nautilus_file_ref (file);
	data->file = file;
	data->new_name = g_strdup (new_name);
	nautilus_timed_wait_start (cancel_rename_callback,
				   data,
				   _("Cancel Rename?"),
				   wait_message,
				   NULL); /* FIXME: Parent this? */
	nautilus_file_rename (file, new_name,
			      rename_callback, data);
	g_free (wait_message);

}
