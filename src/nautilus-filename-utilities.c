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


typedef gboolean (* AppendixParser) (const char *appendix,
                                     size_t      appendix_len,
                                     size_t     *count);
typedef char * (* AppendixCreator) (size_t count);

/**
 * parse_previous_name:
 * @name: Name of the original file
 * @extensionless_length: Length of @name when without an extension
 * @parser_func: Function to parse a detected appendix with.
 * @base_length: (out): Length of the file name without any duplication appendices.
 * @count: (out): The how manyth duplicate the parsed file name represents.
 *
 * Parses a file name to see if it was created as a duplicate of another file.
 */
static void
parse_previous_name (const char     *name,
                     size_t          extensionless_length,
                     AppendixParser  parser_func,
                     size_t         *base_length,
                     size_t         *count)
{
    /* Set default values for early returns */
    *base_length = extensionless_length;
    *count = 0;

    if (extensionless_length <= 4)
    {
        /* Any appendix added by us is at least 4 characters long, no need to check further */
        return;
    }

    /* Duplication appendix ends with bracket, check for it */
    size_t last_character_pos = extensionless_length - 1;
    if (name[last_character_pos] != ')')
    {
        /* Appendix is the last thing before extension, no need to check further */
        return;
    }

    /* Search for opening bracket */
    char *base_end = g_strrstr_len (name, last_character_pos, " (");
    if (base_end == NULL || base_end == name)
    {
        /* Appendix is prefixed with space and bracket and can't be the whole file name */
        return;
    }

    /* Potential bracketed appendix found, try to parse it */
    const char *appendix = base_end + strlen (" (");
    size_t appendix_length = last_character_pos - (appendix - name);

    if (parser_func (appendix, appendix_length, count))
    {
        size_t overhead_length = strlen (" ()");
        *base_length = extensionless_length - appendix_length - overhead_length;
    }
}

/**
 * create_appendix_name:
 * @name: Name of the original file
 * @count_increment: By how much to increase the detected copy number of @name
 * @max_length: Maximum length that resulting file name can have
 * @ignore_extension: Whether to ignore file extensions (should be FALSE for directories)
 * @parser_func: Function to parse a detected appendix with.
 * @appendix_func: Function to create a new appendix from a count.
 *
 * Creates a new name from @name with an appendix added according to @parser_func and
 * @appendix_func, that is no longer than @max_length bytes long.
 *
 * Returns: (transfer full): A file name for a copy of @name.
 */
static char *
create_appendix_name (const char      *name,
                      int              count_increment,
                      int              max_length,
                      gboolean         ignore_extension,
                      AppendixParser   parser_func,
                      AppendixCreator  appendix_func)
{
    g_assert (name[0] != '\0');
    g_assert (count_increment > 0);

    const char *extension = ignore_extension ? "" : nautilus_filename_get_extension (name);
    size_t extensionless_length = ignore_extension ? strlen (name) : extension - name;
    size_t base_length;
    size_t count;
    parse_previous_name (name, extensionless_length, parser_func, &base_length, &count);

    g_autofree char *base = g_strndup (name, base_length);
    g_autofree char *appendix = appendix_func (count + count_increment);

    char *result = g_strdup_printf ("%s (%s)%s", base, appendix, extension);
    nautilus_filename_shorten_base (&result, base, max_length);

    return result;
}

static gboolean
parse_previous_conflict_name (const char *appendix,
                              size_t      appendix_len,
                              size_t     *count)
{
    if (sscanf (appendix, "%zu)", count) == 1)
    {
        g_autofree char *appendix_check = g_strdup_printf ("%zu", *count);

        if (strlen (appendix_check) == appendix_len)
        {
            return TRUE;
        }
    }

    *count = 0;
    return FALSE;
}

static char *
get_conflict_appendix (size_t count)
{
    /* First appendix should be (2), not (1) */
    count = MAX (count, 2);
    return g_strdup_printf ("%zu", count);
}

/**
 * nautilus_filename_for_conflict:
 * @name: Name of the original file
 * @count_increment: By how much to increase the detected copy number of @name
 * @max_length: Maximum length that resulting file name can have
 * @ignore_extension: Whether to ignore file extensions (should be FALSE for directories)
 *
 * Creates a new name for a copy of @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a copy of @name.
 */
char *
nautilus_filename_for_conflict (const char *name,
                                int         count_increment,
                                int         max_length,
                                gboolean    ignore_extension)
{
    return create_appendix_name (name, count_increment, max_length, ignore_extension,
                                 parse_previous_conflict_name, get_conflict_appendix);
}

/* Translators: This is appended to a file name when a copy of a file is created. */
#define COPY_APPENDIX_FIRST_COPY C_("Noun", "Copy")
/* Translators: This is appended to a file name when a copy of an already copied file is created.
 * `%zu` will be replaced with a number one higher than that of the previous copy. */
#define  COPY_APPENDIX_NTH_COPY C_("Noun", "Copy %zu")

static gboolean
parse_previous_copy_name (const char *appendix,
                          size_t      appendix_len,
                          size_t     *count)
{
    if (strncmp (appendix, COPY_APPENDIX_FIRST_COPY, appendix_len) == 0)
    {
        /* File is a (first) copy */
        *count = 1;
        return TRUE;
    }
    else if (sscanf (appendix, COPY_APPENDIX_NTH_COPY, count))
    {
        g_autofree char *appendix_check = g_strdup_printf (COPY_APPENDIX_NTH_COPY, *count);

        if (strlen (appendix_check) == appendix_len)
        {
            /* File is an n-th copy, create n+1-th copy */
            return TRUE;
        }
    }

    /* No copy detected */
    *count = 0;
    return FALSE;
}

static char *
get_copy_appendix (size_t count)
{
    return (count == 1) ?
           g_strdup (COPY_APPENDIX_FIRST_COPY) :
           g_strdup_printf (COPY_APPENDIX_NTH_COPY, count);
}

/**
 * nautilus_filename_for_copy:
 * @name: Name of the original file
 * @count_increment: By how much to increase the detected copy number of @name
 * @max_length: Maximum length that resulting file name can have
 * @ignore_extension: Whether to ignore file extensions (should be FALSE for directories)
 *
 * Creates a new name for a copy of @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a copy of @name.
 */
char *
nautilus_filename_for_copy (const char *name,
                            int         count_increment,
                            int         max_length,
                            gboolean    ignore_extension)
{
    return create_appendix_name (name, count_increment, max_length, ignore_extension,
                                 parse_previous_copy_name, get_copy_appendix);
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
        /* Translators: File name for new symlink. %s is target's name, %zu is number of symlink. */
        result = g_strdup_printf (_("Link to %s (%zu)"), name, count);
    }

    nautilus_filename_shorten_base (&result, name, max_length);

    return result;
}

/**
 * nautilus_filename_get_common_prefix:
 * @strv: array of strings
 * @min_required_len: the minimum number of characters required in the prefix
 *
 * Returns: (transfer full): the common prefix for strings in @strv.
 * If no such prefix exists or if the common prefix is smaller than
 * @min_required_len, %NULL is returned.
 */
char *
nautilus_filename_get_common_prefix (const char * const *strv,
                                     int                 min_required_len)
{
    if (strv == NULL || strv[0] == NULL)
    {
        return NULL;
    }

    glong common_len = g_utf8_strlen (strv[0], -1);

    for (guint i = 1; strv[i] != NULL; i++)
    {
        const char *character_a = strv[0];
        const char *character_b = strv[i];

        common_len = MIN (common_len, g_utf8_strlen (strv[i], -1));
        if (common_len < min_required_len)
        {
            return NULL;
        }

        for (guint pos = 0; pos < common_len; pos++)
        {
            if (g_utf8_get_char (character_a) != g_utf8_get_char (character_b))
            {
                common_len = pos;
                break;
            }

            character_a = g_utf8_next_char (character_a);
            character_b = g_utf8_next_char (character_b);
        }
    }

    if (common_len < min_required_len)
    {
        return NULL;
    }

    return g_utf8_substring (strv[0], 0, common_len);
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
 * nautilus_filename_get_extension_char_offset:
 * @filename: a null-terminated file name.
 *
 * Returns: The offset in charcters at which the extension of @filename starts,
 *          or the offset to the end of the string if there is no extension.
 */
int
nautilus_filename_get_extension_char_offset (const char *filename)
{
    const char *extension = nautilus_filename_get_extension (filename);

    return g_utf8_strlen (filename, extension - filename);
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

/**
 * nautilus_filename_strip_extension:
 * @filename: a null-terminated file name.
 *
 * Returns: (transfer full): A copy of filename without an extension.
 */
char *
nautilus_filename_strip_extension (const char *filename)
{
    const char *extension = nautilus_filename_get_extension (filename);

    return g_strndup (filename, extension - filename);
}
