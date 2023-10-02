/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-filename-utilities.h"

#include <glib.h>
#include <glib/gi18n.h>


/**
 * nautilus_filename_for_link:
 * @name: Name of the original file
 * @count: Number of links to @name already existing
 * @max_length: Maximum length that resulting file name can have
 *
 * Creates a new name for a link to @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a link to @name.
 */
char *
nautilus_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length)
{
    g_assert (name != NULL);

    char *result;
    if (count == 0)
    {
        /* Use identical name */
        result = g_strdup (name);
    }
    else if (count == 1)
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

/**
 * nautilus_filename_get_extension:
 * @filename: a null-terminated file name.
 *
 * Returns: (transfer none): A pointer, to the dot of the extension substring, or
 *                           the null-terminating position if there is no extension.
 */
const char *
nautilus_filename_get_extension (const char *filename)
{
    g_assert (filename != NULL);

    if (filename[0] == '\0')
    {
        return filename;
    }

    /* basename must have at least one char */
    const char *start = g_utf8_next_char (filename);
    size_t search_length = strlen (start);
    gchar *extension = g_utf8_strrchr (start, search_length, '.');

    if (extension == NULL || *g_utf8_next_char (extension) == '\0')
    {
        return start + search_length;
    }

    /** Make sure there are no whitespaces in found extension */
    for (const char *c = extension; *c != '\0'; c = g_utf8_next_char (c))
    {
        if (g_unichar_isspace (g_utf8_get_char (c)))
        {
            return start + search_length;
        }
    }

    /* Special case .tar extensions.
     * This will also catch .tar.jpg, but such cases seem contrived and this
     * is better than maintaing a list of all possible .tar extensions. */
    size_t tar_extension_length = strlen (".tar");
    if (extension - filename > tar_extension_length &&
        strncmp (extension - tar_extension_length, ".tar", tar_extension_length) == 0)
    {
        return extension - tar_extension_length;
    }

    return extension;
}

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
