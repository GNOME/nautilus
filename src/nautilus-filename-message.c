/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-filename-message.h"

#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-file-utilities.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>


gboolean
nautilus_filename_message_is_valid (NautilusFileNameMessage message)
{
    return message == NAUTILUS_FILENAME_MESSAGE_NONE ||
           message == NAUTILUS_FILENAME_MESSAGE_HIDDEN;
}

static gboolean
is_duplicate (const char        *name,
              NautilusDirectory *containing_directory,
              gboolean          *duplicate_is_folder)
{
    g_autoptr (NautilusFile) existing_file = nautilus_directory_get_file_by_name (containing_directory, name);

    *duplicate_is_folder = existing_file != NULL && nautilus_file_is_directory (existing_file);
    return existing_file != NULL;
}

static gboolean
is_name_too_long (size_t             name_length,
                  NautilusDirectory *containing_directory)
{
    g_autoptr (GFile) location = nautilus_directory_get_location (containing_directory);
    glong max_name_length = nautilus_get_max_child_name_length_for_location (location);

    if (max_name_length == -1)
    {
        /* We don't know, so let's give it a chance */
        return FALSE;
    }
    else
    {
        return name_length > max_name_length;
    }
}

NautilusFileNameMessage
nautilus_filename_message_from_name (const char        *name,
                                     NautilusDirectory *containing_directory,
                                     const char        *orig_name)
{
    gboolean duplicate_is_folder = FALSE;

    if (name == NULL || name[0] == '\0')
    {
        return NAUTILUS_FILENAME_MESSAGE_NONE;
    }
    else if (orig_name != NULL && strcmp (name, orig_name) == 0)
    {
        return NAUTILUS_FILENAME_MESSAGE_NONE;
    }
    else if (strstr (name, "/") != NULL)
    {
        return NAUTILUS_FILENAME_MESSAGE_SLASH;
    }
    else if (g_str_equal (name, "."))
    {
        return NAUTILUS_FILENAME_MESSAGE_DOT;
    }
    else if (g_str_equal (name, ".."))
    {
        return NAUTILUS_FILENAME_MESSAGE_DOTDOT;
    }
    else if (is_name_too_long (strlen (name), containing_directory))
    {
        return NAUTILUS_FILENAME_MESSAGE_TOO_LONG;
    }
    else if (is_duplicate (name, containing_directory, &duplicate_is_folder))
    {
        return duplicate_is_folder ? NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FOLDER :
                                     NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FILE;
    }
    else if (g_str_has_prefix (name, "."))
    {
        return NAUTILUS_FILENAME_MESSAGE_HIDDEN;
    }
    else
    {
        return NAUTILUS_FILENAME_MESSAGE_NONE;
    }
}

const char *
nautilus_filename_message_archive_error (NautilusFileNameMessage message)
{
    switch (message)
    {
        case NAUTILUS_FILENAME_MESSAGE_SLASH:
        {
            return _("Archive names cannot contain “/”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FILE:
        {
            return _("A file with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FOLDER:
        {
            return _("A folder with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOT:
        {
            return _("An archive cannot be called “.”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOTDOT:
        {
            return _("An archive cannot be called “..”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_TOO_LONG:
        {
            return _("Archive name is too long.");
        }

        case NAUTILUS_FILENAME_MESSAGE_HIDDEN:
        {
            return _("Archives with “.” at the beginning of their name are hidden.");
        }

        case NAUTILUS_FILENAME_MESSAGE_NONE:
        default:
        {
            return NULL;
        }
    }
}

const char *
nautilus_filename_message_file_error (NautilusFileNameMessage message)
{
    switch (message)
    {
        case NAUTILUS_FILENAME_MESSAGE_SLASH:
        {
            return _("File names cannot contain “/”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FILE:
        {
            return _("A file with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FOLDER:
        {
            return _("A folder with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOT:
        {
            return _("A file cannot be called “.”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOTDOT:
        {
            return _("A file cannot be called “..”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_TOO_LONG:
        {
            return _("File name is too long.");
        }

        case NAUTILUS_FILENAME_MESSAGE_HIDDEN:
        {
            return _("Files with “.” at the beginning of their name are hidden.");
        }

        case NAUTILUS_FILENAME_MESSAGE_NONE:
        default:
        {
            return NULL;
        }
    }
}

const char *
nautilus_filename_message_folder_error (NautilusFileNameMessage message)
{
    switch (message)
    {
        case NAUTILUS_FILENAME_MESSAGE_SLASH:
        {
            return _("Folder names cannot contain “/”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FILE:
        {
            return _("A file with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FOLDER:
        {
            return _("A folder with that name already exists.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOT:
        {
            return _("A folder cannot be called “.”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_DOTDOT:
        {
            return _("A folder cannot be called “..”.");
        }

        case NAUTILUS_FILENAME_MESSAGE_TOO_LONG:
        {
            return _("Folder name is too long.");
        }

        case NAUTILUS_FILENAME_MESSAGE_HIDDEN:
        {
            return _("Folders with “.” at the beginning of their name are hidden.");
        }

        case NAUTILUS_FILENAME_MESSAGE_NONE:
        default:
        {
            return NULL;
        }
    }
}
