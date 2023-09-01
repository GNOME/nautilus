/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-directory.h"

#include <glib.h>


typedef enum
{
    NAUTILUS_FILENAME_MESSAGE_DOT,
    NAUTILUS_FILENAME_MESSAGE_DOTDOT,
    NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FILE,
    NAUTILUS_FILENAME_MESSAGE_DUPLICATE_FOLDER,
    NAUTILUS_FILENAME_MESSAGE_HIDDEN,
    NAUTILUS_FILENAME_MESSAGE_NONE,
    NAUTILUS_FILENAME_MESSAGE_SLASH,
    NAUTILUS_FILENAME_MESSAGE_TOO_LONG,
} NautilusFileNameMessage;

NautilusFileNameMessage
nautilus_filename_message_from_name (const char        *name,
                                     NautilusDirectory *containing_directory,
                                     const char        *orig_name);

gboolean
nautilus_filename_message_is_valid (NautilusFileNameMessage message);

const char *
nautilus_filename_message_archive_error (NautilusFileNameMessage message);
const char *
nautilus_filename_message_file_error (NautilusFileNameMessage message);
const char *
nautilus_filename_message_folder_error (NautilusFileNameMessage message);
