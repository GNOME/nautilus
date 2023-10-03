/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-filename-utilities.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>

#include <stdio.h>

#include <eel/eel-vfs-extensions.h>


/* Translators: This is appended to a file name when a copy of a file is created. */
#define COPY_APPENDIX_FIRST_COPY _("Copy")
/* Translators: This is appended to a file name when a copy of an already copied file is created.
 * `%ld` will be replaced with the how manyth copy number this is. */
#define  COPY_APPENDIX_NTH_COPY _("Copy %ld")

/* Dismantle a file name, separating the base name, the file suffix and figuring out
 * the potential count of a (Copy %d) appendix. */
static char *
parse_previous_duplicate_name (const char  *name,
                               gboolean     ignore_extension,
                               const char **extension,
                               size_t      *count)
{
    size_t extensionless_length;
    char *start_bracket;
    size_t start_bracket_pos;
    size_t last_character_pos;
    size_t str_in_bracket_length;
    size_t detected_count;

    g_assert (name[0] != '\0');

    /* Set default for early returns */
    *count = 0;

    *extension = ignore_extension ? NULL : eel_filename_get_extension_offset (name);

    if (*extension == NULL)
    {
        /* No extension */
        *extension = "";
        extensionless_length = strlen (name);
    }
    else
    {
        extensionless_length = *extension - name;
    }

    if (extensionless_length <= 4)
    {
        /* Any appendix added by us is at least 4 characters long, no need to check further */
        return g_strndup (name, extensionless_length);
    }

    last_character_pos = extensionless_length - 1;

    if (name[last_character_pos] != ')')
    {
        /* Appendix is the last thing before extension, no need to check further */
        return g_strndup (name, extensionless_length);
    }

    start_bracket = g_utf8_strrchr (name, last_character_pos, '(');

    if (start_bracket == NULL)
    {
        /* Appendix is surrounded by brackets, no need to check further */
        return g_strndup (name, extensionless_length);
    }
    if (start_bracket == name)
    {
        /* Appendix can't be whole file name */
        return g_strndup (name, extensionless_length);
    }

    start_bracket_pos = start_bracket - name;
    str_in_bracket_length = last_character_pos - (start_bracket_pos + 1);

    if (strncmp (start_bracket + 1, COPY_APPENDIX_FIRST_COPY, str_in_bracket_length) == 0)
    {
        /* File is a (first) copy */
        *count = 1;
        return g_strndup (name, start_bracket_pos - 1);
    }
    else if (sscanf (start_bracket + 1, COPY_APPENDIX_NTH_COPY, &detected_count))
    {
        /* File is an n-th copy, create n+1-th copy */
        *count = detected_count;
        return g_strndup (name, start_bracket_pos - 1);
    }
    else
    {
        /* Name ends with unrelated bracket, append new copy appendix */
        return g_strndup (name, extensionless_length);
    }
}

static char *
get_formatted_copy_name (const char *base,
                         size_t      count,
                         const char *extension)
{
    if (count == 1)
    {
        return g_strdup_printf ("%s (%s)%s", base, COPY_APPENDIX_FIRST_COPY, extension);
    }
    else
    {
        g_autofree char *appendix = g_strdup_printf (COPY_APPENDIX_NTH_COPY, count);

        return g_strdup_printf ("%s (%s)%s", base, appendix, extension);
    }
}

char *
nautilus_filename_create_duplicate (const char *name,
                                    int         count_increment,
                                    int         max_length,
                                    gboolean    ignore_extension)
{
    const char *extension;
    size_t count;
    g_autofree char *base = parse_previous_duplicate_name (name, ignore_extension, &extension, &count);
    char *result;

    g_assert (count_increment > 0);

    result = get_formatted_copy_name (base, count + count_increment, extension);
    nautilus_filename_shorten_base (&result, base, max_length);

    return result;
}

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
