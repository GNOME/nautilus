/* nautilus-error-reporting.h - implementation of file manager functions that report
 *                               errors to the user.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: John Sullivan <sullivan@eazel.com>
 */
#define G_LOG_DOMAIN "nautilus-error-reporting"

#include <config.h>

#include "nautilus-error-reporting.h"

#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include "nautilus-file.h"
#include <eel/eel-stock-dialogs.h>

#define NEW_NAME_TAG "Nautilus: new name"

static void finish_rename (NautilusFile *file,
                           gboolean      stop_timer,
                           GError       *error);

void
nautilus_report_error_loading_directory (NautilusFile *file,
                                         GError       *error,
                                         GtkWidget    *parent)
{
    g_autofree char *message = NULL;

    if (error == NULL ||
        error->message == NULL)
    {
        return;
    }

    if (error->domain == G_IO_ERROR &&
        error->code == G_IO_ERROR_NOT_MOUNTED)
    {
        /* This case is retried automatically */
        return;
    }

    if (error->domain == G_IO_ERROR)
    {
        const char *file_name = nautilus_file_get_display_name (file);

        switch (error->code)
        {
            case G_IO_ERROR_PERMISSION_DENIED:
            {
                message = g_strdup_printf (_("You do not have the permissions necessary to view the contents of “%s”."),
                                           file_name);
            }
            break;

            case G_IO_ERROR_NOT_FOUND:
            {
                message = g_strdup_printf (_("“%s” could not be found. Perhaps it has recently been deleted."),
                                           file_name);
            }
            break;

            default:
            {
                message = g_strdup_printf (_("Sorry, could not display all the contents of “%s”: %s"),
                                           file_name,
                                           error->message);
            }
            break;
        }
    }
    else
    {
        message = g_strdup (error->message);
    }

    nautilus_show_ok_dialog (_("This location could not be displayed."),
                             message,
                             parent);
}

void
nautilus_report_error_setting_group (NautilusFile *file,
                                     GError       *error,
                                     GtkWidget    *parent)
{
    g_autofree char *message = NULL;

    if (error == NULL)
    {
        return;
    }

    if (error->domain == G_IO_ERROR)
    {
        switch (error->code)
        {
            case G_IO_ERROR_PERMISSION_DENIED:
            {
                message = g_strdup_printf (_("You do not have the permissions necessary to change the group of “%s”."),
                                           nautilus_file_get_display_name (file));
            }
            break;

            default:
            {
            }
            break;
        }
    }

    if (message == NULL)
    {
        /* We should invent decent error messages for every case we actually experience. */
        g_warning ("Hit unhandled case %s:%d in nautilus_report_error_setting_group",
                   g_quark_to_string (error->domain), error->code);
        /* fall through */
        message = g_strdup_printf (_("Sorry, could not change the group of “%s”: %s"),
                                   nautilus_file_get_display_name (file),
                                   error->message);
    }


    nautilus_show_ok_dialog (_("The group could not be changed."),
                             message,
                             parent);
}

void
nautilus_report_error_setting_owner (NautilusFile *file,
                                     GError       *error,
                                     GtkWidget    *parent)
{
    g_autofree char *message = NULL;

    if (error == NULL)
    {
        return;
    }

    message = g_strdup_printf (_("Sorry, could not change the owner of “%s”: %s"),
                               nautilus_file_get_display_name (file),
                               error->message);

    nautilus_show_ok_dialog (_("The owner could not be changed."),
                             message,
                             parent);
}

void
nautilus_report_error_setting_permissions (NautilusFile *file,
                                           GError       *error,
                                           GtkWidget    *parent)
{
    g_autofree char *message = NULL;

    if (error == NULL)
    {
        return;
    }

    message = g_strdup_printf (_("Sorry, could not change the permissions of “%s”: %s"),
                               nautilus_file_get_display_name (file),
                               error->message);

    nautilus_show_ok_dialog (_("The permissions could not be changed."),
                             message,
                             parent);
}

typedef struct _NautilusRenameData
{
    char *name;
    GtkWidget *parent;
} NautilusRenameData;

void
nautilus_report_error_renaming_file (NautilusFile *file,
                                     const char   *new_name,
                                     GError       *error,
                                     GtkWidget    *parent)
{
    g_autofree char *message = NULL;

    const char *file_name = nautilus_file_get_display_name (file);

    if (error->domain == G_IO_ERROR)
    {
        switch (error->code)
        {
            case G_IO_ERROR_EXISTS:
            {
                message = g_strdup_printf (_("The name “%s” is already used in this location. "
                                             "Please use a different name."),
                                           new_name);
            }
            break;

            case G_IO_ERROR_NOT_FOUND:
            {
                message = g_strdup_printf (_("There is no “%s” in this location. "
                                             "Perhaps it was just moved or deleted?"),
                                           file_name);
            }
            break;

            case G_IO_ERROR_PERMISSION_DENIED:
            {
                message = g_strdup_printf (_("You do not have the permissions necessary to rename “%s”."),
                                           file_name);
            }
            break;

            case G_IO_ERROR_INVALID_ARGUMENT:
            case G_IO_ERROR_INVALID_FILENAME:
            {
                const char *invalid_chars = FAT_FORBIDDEN_CHARACTERS;
                const char *forbidden_char = strpbrk (new_name, invalid_chars);
                if (forbidden_char != NULL)
                {
                    message = g_strdup_printf (_("The name “%s” is not valid because it contains the character “%c”. "
                                                 "Please use a different name."),
                                               new_name, *forbidden_char);
                }
                if (message == NULL)
                {
                    message = g_strdup_printf (_("The name “%s” is not valid. "
                                                 "Please use a different name."),
                                               new_name);
                }
            }
            break;

            case G_IO_ERROR_FILENAME_TOO_LONG:
            {
                message = g_strdup_printf (_("The name “%s” is too long. "
                                             "Please use a different name."),
                                           new_name);
            }
            break;

            case G_IO_ERROR_BUSY:
            {
                message = g_strdup_printf (_("Could not rename “%s” because a process is using it."
                                             " If it's open in another application, close it before"
                                             " renaming it."),
                                           file_name);
            }
            break;

            default:
            {
            }
            break;
        }
    }

    if (message == NULL)
    {
        /* We should invent decent error messages for every case we actually experience. */
        g_warning ("Hit unhandled case %s:%d in nautilus_report_error_renaming_file",
                   g_quark_to_string (error->domain), error->code);
        /* fall through */
        message = g_strdup_printf (_("Sorry, could not rename “%s” to “%s”: %s"),
                                   file_name, new_name,
                                   error->message);
    }

    nautilus_show_ok_dialog (_("The item could not be renamed."),
                             message,
                             parent);
}

static void
nautilus_rename_data_free (NautilusRenameData *data)
{
    g_free (data->name);
    g_free (data);
}

static void
rename_callback (NautilusFile *file,
                 GFile        *result_location,
                 GError       *error,
                 gpointer      callback_data)
{
    NautilusRenameData *data;
    gboolean cancelled = FALSE;

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (callback_data == NULL);

    data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
    g_assert (data != NULL);

    if (error)
    {
        if (!(error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED))
        {
            /* If rename failed, notify the user. */
            nautilus_report_error_renaming_file (file, data->name, error, data->parent);
        }
        else
        {
            cancelled = TRUE;
        }
    }

    finish_rename (file, !cancelled, error);
}

static void
cancel_rename_callback (gpointer callback_data)
{
    nautilus_file_cancel (NAUTILUS_FILE (callback_data), rename_callback, NULL);
}

static void
finish_rename (NautilusFile *file,
               gboolean      stop_timer,
               GError       *error)
{
    NautilusRenameData *data;

    data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
    if (data == NULL)
    {
        return;
    }

    /* Cancel both the rename and the timed wait. */
    nautilus_file_cancel (file, rename_callback, NULL);
    if (stop_timer)
    {
        eel_timed_wait_stop (cancel_rename_callback, file);
    }

    /* Let go of file name. */
    g_object_set_data (G_OBJECT (file), NEW_NAME_TAG, NULL);
}

void
nautilus_rename_file (NautilusFile *file,
                      const char   *new_name,
                      GtkWidget    *parent)
{
    g_autoptr (GError) error = NULL;
    NautilusRenameData *data;
    g_autofree char *wait_message = NULL;
    g_autofree char *uri = NULL;

    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (new_name != NULL);

    /* Stop any earlier rename that's already in progress. */
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
    finish_rename (file, TRUE, error);

    data = g_new0 (NautilusRenameData, 1);
    data->name = g_strdup (new_name);
    data->parent = parent;

    /* Attach the new name to the file. */
    g_object_set_data_full (G_OBJECT (file),
                            NEW_NAME_TAG,
                            data, (GDestroyNotify) nautilus_rename_data_free);

    /* Start the timed wait to cancel the rename. */
    wait_message = g_strdup_printf (_("Renaming “%s” to “%s”."),
                                    nautilus_file_get_display_name (file),
                                    new_name);
    eel_timed_wait_start (cancel_rename_callback, file, wait_message,
                          NULL);     /* FIXME bugzilla.gnome.org 42395: Parent this? */

    uri = nautilus_file_get_uri (file);
    g_debug ("Renaming file %s to %s", uri, new_name);

    /* Start the rename. */
    nautilus_file_rename (file, new_name,
                          rename_callback, NULL);
}
