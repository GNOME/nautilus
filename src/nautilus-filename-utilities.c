/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-filename-utilities.h"

#include <glib.h>
#include <glib/gi18n.h>


/**
 * nautilus_filename_shorten_base:
 * @filename: (inout): Pointer to a filename that is to be shortened
 * @base: a base from which @filename was constructed
 * @max_length: Maximum length that @filename should have
 *
 * Shortens @filename to a maximum length of @max_length. If it already is
 * shorter than @max_length, it is unchanged, otherwise the old @filename is
 * freed and replaced with a newly allocated one.
 *
 * If @base can not be shortened in a way that a new filename would be shorter
 * than @max_length, @filename also stays unchanged.
 *
 * Returns: Whether @filename was shortened.
 */
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
        size_t base_length = strlen (base);
        size_t suffix_length = filename_length - base_length;
        size_t reduce_by_num_bytes = filename_length - max_length;
        size_t reduced_length = base_length - reduce_by_num_bytes;

        if (reduce_by_num_bytes > base_length)
        {
            return FALSE;
        }

        /* Search  */
        const char *reduce_pos = base + base_length;
        do
        {
            reduce_pos = g_utf8_find_prev_char (base, reduce_pos);
        }
        while (reduce_pos - base > reduced_length);

        /* Recalculate length, as it could be off by some bytes due to UTF-8 */
        reduced_length = reduce_pos - base;

        char *new_filename = g_new0 (char, suffix_length + reduced_length);
        strncpy (new_filename, base, reduced_length);
        strncpy (new_filename + reduced_length, *filename + base_length, suffix_length);

        g_free (*filename);
        *filename = new_filename;

        return TRUE;
    }
}
