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

#include <string.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libnautilus-private/nautilus-file.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>

#define NEW_NAME_TAG "Nautilus: new name"
#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH	50

static void cancel_rename (NautilusFile *file);

void
fm_report_error_loading_directory (NautilusFile *file,
			           GnomeVFSResult error,
			           GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == GNOME_VFS_OK) {
		return;
	}

	file_name = nautilus_file_get_display_name (file);

	switch (error) {
	case GNOME_VFS_ERROR_ACCESS_DENIED:
		message = g_strdup_printf (_("You do not have the permissions necessary to view the contents of \"%s\"."),
					   file_name);
		break;
	case GNOME_VFS_ERROR_NOT_FOUND:
		message = g_strdup_printf (_("\"%s\" couldn't be found. Perhaps it has recently been deleted."),
					   file_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d (%s) in fm_report_error_loading_directory, tell sullivan@eazel.com", 
			   error, gnome_vfs_result_to_string (error));
		message = g_strdup_printf (_("Sorry, couldn't display all the contents of \"%s\"."), file_name);
	}

	eel_show_error_dialog (message, _("Error Displaying Folder"), parent_window);

	g_free (file_name);
	g_free (message);
}		

void
fm_report_error_renaming_file (NautilusFile *file,
			       const char *new_name,
			       GnomeVFSResult error,
			       GtkWindow *parent_window)
{
	char *original_name, *original_name_truncated;
	char *new_name_truncated;
	char *message;

	if (error == GNOME_VFS_OK) {
		return;
	}

	/* Truncate names for display since very long file names with no spaces
	 * in them won't get wrapped, and can create insanely wide dialog boxes.
	 */
	original_name = nautilus_file_get_display_name (file);
	original_name_truncated = eel_str_middle_truncate (original_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
	g_free (original_name);
	
	new_name_truncated = eel_str_middle_truncate (new_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
	
	switch (error) {
	case GNOME_VFS_ERROR_FILE_EXISTS:
		message = g_strdup_printf (_("The name \"%s\" is already used in this folder. "
					     "Please use a different name."), 
					   new_name_truncated);
		break;
	case GNOME_VFS_ERROR_NOT_FOUND:
		message = g_strdup_printf (_("There is no \"%s\" in this folder. "
					     "Perhaps it was just moved or deleted?"), 
					   original_name_truncated);
		break;
	case GNOME_VFS_ERROR_ACCESS_DENIED:
		message = g_strdup_printf (_("You do not have the permissions necessary to rename \"%s\"."),
					   original_name_truncated);
		break;
	case GNOME_VFS_ERROR_NOT_PERMITTED:
		if (strchr (new_name, '/') != NULL) {
			message = g_strdup_printf (_("The name \"%s\" is not valid because it contains the character \"/\". "
						     "Please use a different name."),
						   new_name_truncated);
		} else {
			message = g_strdup_printf (_("The name \"%s\" is not valid. "
						     "Please use a different name."),
						   new_name_truncated);
		}
		break;
	case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:
		message = g_strdup_printf (_("Couldn't change the name of \"%s\" because it is on a read-only disk"),
					   original_name_truncated);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d (%s) in fm_report_error_renaming_file, tell sullivan@eazel.com", 
			   error, gnome_vfs_result_to_string (error));
		/* fall through */
	case GNOME_VFS_ERROR_GENERIC:
		message = g_strdup_printf (_("Sorry, couldn't rename \"%s\" to \"%s\"."),
					   original_name_truncated, new_name_truncated);
	}

	g_free (original_name_truncated);
	g_free (new_name_truncated);

	eel_show_error_dialog (message, _("Renaming Error"), parent_window);
	g_free (message);
}

void
fm_report_error_setting_group (NautilusFile *file,
			       GnomeVFSResult error,
			       GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == GNOME_VFS_OK) {
		return;
	}

	file_name = nautilus_file_get_display_name (file);

	switch (error) {
	case GNOME_VFS_ERROR_NOT_PERMITTED:
		message = g_strdup_printf (_("You do not have the permissions necessary to change the group of \"%s\"."),
					   file_name);
		break;
	case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:
		message = g_strdup_printf (_("Couldn't change the group of \"%s\" because it is on a read-only disk"),
					   file_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d (%s) in fm_report_error_setting_group, tell sullivan@eazel.com", 
			   error, gnome_vfs_result_to_string (error));
		file_name = nautilus_file_get_display_name (file);
		message = g_strdup_printf (_("Sorry, couldn't change the group of \"%s\"."), file_name);
		g_free (file_name);
	}

	eel_show_error_dialog (message, _("Error Setting Group"), parent_window);

	g_free (file_name);
	g_free (message);
}		

void
fm_report_error_setting_owner (NautilusFile *file,
			       GnomeVFSResult error,
			       GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == GNOME_VFS_OK) {
		return;
	}

	file_name = nautilus_file_get_display_name (file);

	switch (error) {
	case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:
		message = g_strdup_printf (_("Couldn't change the owner of \"%s\" because it is on a read-only disk"),
					   file_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d (%s) in fm_report_error_setting_owner, tell sullivan@eazel.com", 
			   error, gnome_vfs_result_to_string (error));
		message = g_strdup_printf (_("Sorry, couldn't change the owner of \"%s\"."), file_name);
	}

	eel_show_error_dialog (message, _("Error Setting Owner"), parent_window);

	g_free (file_name);
	g_free (message);
}		

void
fm_report_error_setting_permissions (NautilusFile *file,
			       	     GnomeVFSResult error,
			       	     GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == GNOME_VFS_OK) {
		return;
	}

	file_name = nautilus_file_get_display_name (file);

	switch (error) {
	case GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM:
		message = g_strdup_printf (_("Couldn't change the permissions of \"%s\" because it is on a read-only disk"),
					   file_name);
		break;
	default:
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %d (%s) in fm_report_error_setting_permissions, tell sullivan@eazel.com", 
			   error, gnome_vfs_result_to_string (error));
		message = g_strdup_printf (_("Sorry, couldn't change the permissions of \"%s\"."), file_name);
	}

	eel_show_error_dialog (message, _("Error Setting Permissions"), parent_window);

	g_free (file_name);
	g_free (message);
}		

static void
rename_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	char *name;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (callback_data == NULL);
	name = gtk_object_get_data (GTK_OBJECT (file), NEW_NAME_TAG);
	g_assert (name != NULL);

	/* If rename failed, notify the user. */
	fm_report_error_renaming_file (file, name, result, NULL);

	cancel_rename (file);
}

static void
cancel_rename_callback (gpointer callback_data)
{
	cancel_rename (NAUTILUS_FILE (callback_data));
}

static void
cancel_rename (NautilusFile *file)
{
	char *name;

	name = gtk_object_get_data (GTK_OBJECT (file), NEW_NAME_TAG);
	if (name == NULL) {
		return;
	}

	/* Cancel both the rename and the timed wait. */
	nautilus_file_cancel (file, rename_callback, NULL);
	eel_timed_wait_stop (cancel_rename_callback, file);

	/* Let go of file name. */
	gtk_object_remove_data (GTK_OBJECT (file), NEW_NAME_TAG);
}

void
fm_rename_file (NautilusFile *file,
		const char *new_name)
{
	char *old_name, *wait_message;

	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (new_name != NULL);

	/* Stop any earlier rename that's already in progress. */
	cancel_rename (file);

	/* Attach the new name to the file. */
	gtk_object_set_data_full (GTK_OBJECT (file),
				  NEW_NAME_TAG,
				  g_strdup (new_name),
				  g_free);

	/* Start the timed wait to cancel the rename. */
	old_name = nautilus_file_get_display_name (file);
	wait_message = g_strdup_printf (_("Renaming \"%s\" to \"%s\"."),
					old_name,
					new_name);
	g_free (old_name);
	eel_timed_wait_start (cancel_rename_callback, file,
				   _("Cancel Rename?"), wait_message,
				   NULL); /* FIXME bugzilla.gnome.org 42395: Parent this? */
	g_free (wait_message);

	/* Start the rename. */
	nautilus_file_rename (file, new_name,
			      rename_callback, NULL);
}
