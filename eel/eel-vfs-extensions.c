/* eel-vfs-extensions.c - gnome-vfs extensions.  Its likely some of these will
 *                         be part of gnome-vfs in the future.
 *
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Darin Adler <darin@eazel.com>
 *           Pavel Cisler <pavel@eazel.com>
 *           Mike Fleming  <mfleming@eazel.com>
 *           John Sullivan <sullivan@eazel.com>
 */

#include <config.h>
#include "eel-vfs-extensions.h"
#include "eel-lib-self-check-functions.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "eel-string.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * eel_filename_get_extension_offset:
 * @filename: a null-terminated string representing the basename of a file, with
 *            or without extension.
 *
 * Returns: (nullable) (transfer none): A pointer to the substring containing
 *                                      the dot and extension, or %NULL if there
 *                                      is no extension.
 */
char *
eel_filename_get_extension_offset (const char *filename)
{
    char *end, *end2;
    const char *start;

    if (filename == NULL || filename[0] == '\0')
    {
        return NULL;
    }

    /* basename must have at least one char */
    start = filename + 1;

    end = strrchr (start, '.');
    if (end == NULL || end[1] == '\0')
    {
        return NULL;
    }

    for (size_t i = 1; end[i] != '\0'; i += 1)
    {
        if (isspace (end[i]))
        {
            return NULL;
        }
    }

    if (end != start)
    {
        if (strcmp (end, ".gz") == 0 ||
            strcmp (end, ".bz2") == 0 ||
            strcmp (end, ".sit") == 0 ||
            strcmp (end, ".Z") == 0 ||
            strcmp (end, ".bz") == 0 ||
            strcmp (end, ".xz") == 0)
        {
            end2 = end - 1;
            while (end2 > start &&
                   *end2 != '.')
            {
                end2--;
            }
            if (end2 != start)
            {
                end = end2;
            }
        }
    }

    return end;
}

char *
eel_filename_strip_extension (const char *filename_with_extension)
{
    char *filename, *end;

    if (filename_with_extension == NULL)
    {
        return NULL;
    }

    filename = g_strdup (filename_with_extension);
    end = eel_filename_get_extension_offset (filename);

    if (end && end != filename)
    {
        *end = '\0';
    }

    return filename;
}

void
eel_filename_get_rename_region (const char *filename,
                                int        *start_offset,
                                int        *end_offset)
{
    char *filename_without_extension;

    g_return_if_fail (start_offset != NULL);
    g_return_if_fail (end_offset != NULL);

    *start_offset = 0;
    *end_offset = 0;

    g_return_if_fail (filename != NULL);

    filename_without_extension = eel_filename_strip_extension (filename);
    *end_offset = g_utf8_strlen (filename_without_extension, -1);

    g_free (filename_without_extension);
}
