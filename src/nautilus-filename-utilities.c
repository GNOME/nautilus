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

/**
 * parse_previous_duplicate_name:
 * @name: Name of the original file
 * @extensionless_length: Length of @name when without an extension
 * @base_length: (out): Length of the file name without any duplication suffixes.
 *                      Only set when a duplicate is detected.
 * @count: (out): The how manyth duplicate the parsed file name represents.
 *                Only set when a duplicate is detected.
 *
 * Parses a file name to see if it was created as a duplicate of another file.
 */
static void
parse_previous_duplicate_name (const char *name,
                               size_t      extensionless_length,
                               size_t     *base_length,
                               size_t     *count)
{
    /* Set default values for early returns */
    *base_length = extensionless_length;
    *count = 0;

    if (extensionless_length <= 4)
    {
        /* Any appendix added by us is at least 4 characters long, no need to check further */
        return;
    }

    /* Duplication suffix ends with bracket, check for it */
    size_t last_character_pos = extensionless_length - 1;
    if (name[last_character_pos] != ')')
    {
        /* Appendix is the last thing before extension, no need to check further */
        return;
    }

    /* Search for opening bracket */
    char *start_bracket = g_utf8_strrchr (name, last_character_pos, '(');
    if (start_bracket == NULL || start_bracket == name)
    {
        /* Appendix is surrounded by brackets and can't be the whole file name */
        return;
    }

    /* Potential bracketed suffix detected, check if it's a known copy suffix */
    size_t start_bracket_pos = start_bracket - name;
    size_t str_in_bracket_length = last_character_pos - (start_bracket_pos + 1);

    if (strncmp (start_bracket + 1, COPY_APPENDIX_FIRST_COPY, str_in_bracket_length) == 0)
    {
        /* File is a (first) copy */
        *count = 1;
        *base_length = start_bracket_pos - 1;
    }
    else if (sscanf (start_bracket + 1, COPY_APPENDIX_NTH_COPY, count))
    {
        /* File is an n-th copy, create n+1-th copy */
        *base_length = start_bracket_pos - 1;
    }
    else
    {
        /* No copy detected */
        *count = 0;
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

/**
 * nautilus_filename_create_duplicate:
 * @name: Name of the original file
 * @count: Number of copies of @name already existing
 * @max_length: Maximum length that resulting file name can have
 * @ignore_extension: Whether to ignore file extensions (should be FALSE for directories)
 *
 * Creates a new name for a copy of @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a copy of @name.
 */
char *
nautilus_filename_create_duplicate (const char *name,
                                    int         count_increment,
                                    int         max_length,
                                    gboolean    ignore_extension)
{
    g_assert (name[0] != '\0');
    g_assert (count_increment > 0);

    const char *extension = ignore_extension ? "" : nautilus_filename_get_extension (name);
    size_t extensionless_length = ignore_extension ? strlen (name) : extension - name;
    size_t base_length;
    size_t count;
    parse_previous_duplicate_name (name, extensionless_length, &base_length, &count);

    g_autofree char *base = g_strndup (name, base_length);
    char *result = get_formatted_copy_name (base, count + count_increment, extension);
    nautilus_filename_shorten_base (&result, base, max_length);

    return result;
}

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
 * get_common_prefix_length:
 * @str_a: first string
 * @str_b: second string
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: the size of the common prefix of two strings, in characters.
 * If there's no common prefix, or the common prefix is smaller than
 * min_required_len, this will return -1
 */
static int
get_common_prefix_length (char *str_a,
                          char *str_b,
                          int   min_required_len)
{
    int a_len;
    int b_len;
    int intersection_len;
    int matching_chars;
    char *a;
    char *b;

    a_len = g_utf8_strlen (str_a, -1);
    b_len = g_utf8_strlen (str_b, -1);

    intersection_len = MIN (a_len, b_len);
    if (intersection_len < min_required_len)
    {
        return -1;
    }

    matching_chars = 0;
    a = str_a;
    b = str_b;
    while (matching_chars < intersection_len)
    {
        if (g_utf8_get_char (a) != g_utf8_get_char (b))
        {
            break;
        }

        ++matching_chars;

        a = g_utf8_next_char (a);
        b = g_utf8_next_char (b);
    }

    if (matching_chars < min_required_len)
    {
        return -1;
    }

    return matching_chars;
}

/**
 * nautilus_filename_get_common_prefix:
 * @strs: a list of strings
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: (transfer full): the common prefix for strings in @strs.
 * If no such prefix exists or if the common prefix is smaller than
 * @min_required_len, %NULL is returned.
 */
char *
nautilus_filename_get_common_prefix (GList *strs,
                                     int    min_required_len)
{
    GList *l;
    char *common_part;
    char *name;
    char *truncated;
    int matching_chars;

    if (strs == NULL)
    {
        return NULL;
    }

    common_part = NULL;
    for (l = strs; l != NULL; l = l->next)
    {
        name = l->data;
        if (name == NULL)
        {
            g_free (common_part);
            return NULL;
        }

        if (l->prev == NULL)
        {
            common_part = g_strdup (name);
            continue;
        }

        matching_chars = get_common_prefix_length (common_part, name, min_required_len);

        if (matching_chars == -1)
        {
            g_free (common_part);
            return NULL;
        }

        truncated = g_utf8_substring (common_part, 0, matching_chars);
        g_free (common_part);
        common_part = truncated;
    }

    matching_chars = g_utf8_strlen (common_part, -1);
    if (matching_chars < min_required_len)
    {
        g_free (common_part);
        return NULL;
    }

    return common_part;
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
