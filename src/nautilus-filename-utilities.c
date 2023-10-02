/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-filename-utilities.h"

#include <glib.h>
#include <glib/gi18n.h>


char *
nautilus_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length)
{
    char *result;

    g_assert (name != NULL);

    if (count < 1)
    {
        g_warning ("bad count in nautilus_filename_for_link");
        count = 1;
    }

    if (count == 1)
    {
        /* Translators: File name for new symlink. %s is target's name */
        result = g_strdup_printf (_("Link to %s"), name);
    }
    else
    {
        /* Translators: File name for new symlink. %s is target's name, %lu is number of symlink. */
        result = g_strdup_printf (_("Link to %s (%lu)"), name, count);
    }

    nautilus_filename_shorten_base (&result, name, max_length);

    return result;
}

gboolean
nautilus_filename_shorten_base (char       **filename,
                                const char  *base,
                                int          max_length)
{
    size_t filename_length = strlen (*filename);

    if (max_length <= 0 || filename_length <= max_length)
    {
        return FALSE;
    }
    else
    {
        char *new_filename;
        size_t base_length = strlen (base);
        size_t suffix_length = filename_length - base_length;
        size_t reduce_by_num_bytes = filename_length - max_length;
        size_t reduced_length = base_length - reduce_by_num_bytes;
        const char *reduce_pos = base + base_length;

        if (reduce_by_num_bytes > base_length)
        {
            return FALSE;
        }

        /* Search  */
        do
        {
            reduce_pos = g_utf8_find_prev_char (base, reduce_pos);
        }
        while (reduce_pos - base > reduced_length);

        /* Recalculate length, as it could be off by some bytes due to UTF-8 */
        reduced_length = reduce_pos - base;

        new_filename = g_new0 (char, suffix_length + reduced_length);
        strncpy (new_filename, base, reduced_length);
        strncpy (new_filename + reduced_length, *filename + base_length, suffix_length);

        g_free (*filename);
        *filename = new_filename;

        return TRUE;
    }
}
