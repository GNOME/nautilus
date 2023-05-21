/*
 *  eel-string.c: String routines to augment <string.h>.
 *
 *  Copyright (C) 2000 Eazel, Inc.
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
 */

#include <config.h>
#include "eel-string.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#if !defined (EEL_OMIT_SELF_CHECK)
#include "eel-lib-self-check-functions.h"
#endif

/**
 * eel_str_capitalize:
 * @string: input string
 *
 * Returns: a newly allocated copy of @string,
 * with the first letter capitalized.
 * If @string is %NULL, returns %NULL.
 */
char *
eel_str_capitalize (const char *string)
{
    char *capitalized = NULL;

    if (string == NULL)
    {
        return NULL;
    }

    if (g_utf8_validate (string, -1, NULL))
    {
        g_autofree gunichar *ucs4 = NULL;
        ucs4 = g_utf8_to_ucs4 (string, -1, NULL, NULL, NULL);
        if (ucs4 != NULL)
        {
            ucs4[0] = g_unichar_toupper (ucs4[0]);
            capitalized = g_ucs4_to_utf8 (ucs4, -1, NULL, NULL, NULL);
        }
    }

    if (capitalized == NULL)
    {
        return g_strdup (string);
    }

    return capitalized;
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
 * eel_str_get_common_prefix:
 * @strs: a list of strings
 * @min_required_len: the minimum number of characters required in prefix
 *
 * Returns: (transfer full): the common prefix for strings in @strs.
 * If no such prefix exists or if the common prefix is smaller than
 * @min_required_len, %NULL is returned.
 */
char *
eel_str_get_common_prefix (GList *strs,
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

/**************** Custom printf ***********/

typedef struct
{
    const char *start;
    const char *end;
    GString *format;
    int arg_pos;
    int width_pos;
    int width_format_index;
    int precision_pos;
    int precision_format_index;
} ConversionInfo;

enum
{
    ARG_TYPE_INVALID,
    ARG_TYPE_INT,
    ARG_TYPE_LONG,
    ARG_TYPE_LONG_LONG,
    ARG_TYPE_SIZE,
    ARG_TYPE_LONG_DOUBLE,
    ARG_TYPE_DOUBLE,
    ARG_TYPE_POINTER
};

#if !defined (EEL_OMIT_SELF_CHECK)

void
eel_self_check_string (void)
{
    EEL_CHECK_STRING_RESULT (eel_str_capitalize (NULL), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_capitalize (""), "");
    EEL_CHECK_STRING_RESULT (eel_str_capitalize ("foo"), "Foo");
    EEL_CHECK_STRING_RESULT (eel_str_capitalize ("Foo"), "Foo");
}

#endif /* !EEL_OMIT_SELF_CHECK */
