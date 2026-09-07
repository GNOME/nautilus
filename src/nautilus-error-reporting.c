/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */
#define G_LOG_DOMAIN "nautilus-error-reporting"

#include <config.h>

#include "nautilus-error-reporting.h"

#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include "nautilus-file.h"

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
