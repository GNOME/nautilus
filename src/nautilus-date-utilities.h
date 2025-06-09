/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>
#include <time.h>


void
nautilus_date_setup_preferences (void);

char *
nautilus_date_to_str (GDateTime *timestamp,
                      gboolean   use_short_format);

char *
nautilus_date_preview_detailed_format (GDateTime *timestamp,
                                       gboolean   use_detailed);

/* This is meant to be upstreamed to GLib, but lives in-tree for now */

/**
 * g_set_date_time: (skip)
 * @date_time_pointer: (inout) (not optional) (nullable): a pointer to either
 *   a #GDateTime or `NULL`
 * @new_date_time: (nullable): a #GDateTime to assign to @date_time_pointer
 *
 * Updates a pointer to a #GDateTime to @new_date_time, adjusts the ref-counts
 * accordingly and returns whether @date_time_pointer was changed.
 *
 * If @new_date_time matches the previous date time, this function is a no-op.
 * If @new_date_time is different, its ref-count will be increased and it will
 * be assigned to @date_time_pointer.
 * The previous date time pointed to by @date_time_pointer will have its
 * ref-count decreased.
 *
 * @date_time_pointer must not be `NULL`, but can point to a `NULL` value.
 *
 * Returns: true if the value of @date_time_pointer changed, false otherwise
 */
static inline gboolean
g_set_date_time (GDateTime **date_time_pointer,
                 GDateTime  *new_date_time)
{
    GDateTime *old_date_time;

    if (*date_time_pointer == new_date_time ||
        (*date_time_pointer != NULL &&
         new_date_time != NULL &&
         g_date_time_compare (*date_time_pointer, new_date_time) == 0))
    {
        return FALSE;
    }

    if (new_date_time != NULL)
    {
        g_date_time_ref (new_date_time);
    }

    old_date_time = *date_time_pointer;
    *date_time_pointer = new_date_time;

    if (old_date_time != NULL)
    {
        g_date_time_unref (old_date_time);
    }

    return TRUE;
}
