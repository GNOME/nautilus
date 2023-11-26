/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Original Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>


/**
 * Dialog responses can be combined via bit operations, allowing to represent multiple
 * response options in a single value. To add a new response, remember to also add it to
 * nautilus_dialog_response_from_string() and nautilus_dialog_with_responses().
 * Note that cancelling is always an option.
 */
typedef enum
{
    RESPONSE_CANCEL      = 0,
    RESPONSE_COPY_FORCE  = 1 << 1,
    RESPONSE_DELETE      = 1 << 2,
    RESPONSE_DELETE_ALL  = 1 << 3,
    RESPONSE_EMPTY_TRASH = 1 << 4,
    RESPONSE_MERGE       = 1 << 5,
    RESPONSE_PROCEED     = 1 << 6,
    RESPONSE_REPLACE     = 1 << 7,
    RESPONSE_RETRY       = 1 << 8,
    RESPONSE_SKIP        = 1 << 9,
    RESPONSE_SKIP_ALL    = 1 << 10,
    RESPONSE_SKIP_FILES  = 1 << 11,
} NautilusDialogResponse;

NautilusDialogResponse
nautilus_dialog_response_from_string (const char *response);

AdwAlertDialog *
nautilus_dialog_with_responses (const char            *heading,
                                const char            *body,
                                const char            *details,
                                gboolean               delay_interactivity,
                                NautilusDialogResponse responses);
