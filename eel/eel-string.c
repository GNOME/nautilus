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
 * eel_str_double_underscores:
 * @string: input string
 *
 * This is used if you want to preserve underscore characters
 * when creating a label with gtk_label_new_with_mnemonic().
 *
 * Returns: a newly allocated copy of @string,
 * with a doubled number of underscores.
 * If @string doesn't contain underscores, returns a copy of it.
 * If @string is %NULL, returns %NULL.
 */
char *
eel_str_double_underscores (const char *string)
{
    int underscores;
    const char *p;
    char *q;
    char *escaped;

    if (string == NULL)
    {
        return NULL;
    }

    underscores = 0;
    for (p = string; *p != '\0'; p++)
    {
        underscores += (*p == '_');
    }

    if (underscores == 0)
    {
        return g_strdup (string);
    }

    escaped = g_new (char, strlen (string) + underscores + 1);
    for (p = string, q = escaped; *p != '\0'; p++, q++)
    {
        /* Add an extra underscore. */
        if (*p == '_')
        {
            *q++ = '_';
        }
        *q = *p;
    }
    *q = '\0';

    return escaped;
}

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
 * eel_str_middle_truncate:
 * @string: (not nullable): input string
 * truncate_length: length of the truncated string
 *
 * Returns: (transfer full): a newly-allocated copy of @string with its middle
 * truncated and replaced with ellipsis to fit into @truncate_length characters.
 * If length of @string is already small enough, returns a copy of @string.
 */
gchar *
eel_str_middle_truncate (const gchar *string,
                         guint        truncate_length)
{
    const gchar ellipsis[] = "…";
    glong ellipsis_length;
    glong length;
    glong num_left_chars;
    glong num_right_chars;
    g_autofree gchar *left_substring = NULL;
    g_autofree gchar *right_substring = NULL;

    g_return_val_if_fail (string != NULL, NULL);
    g_return_val_if_fail (truncate_length > 0, NULL);

    ellipsis_length = g_utf8_strlen (ellipsis, -1);

    /* Our ellipsis string + one character on each side. */
    if (truncate_length < ellipsis_length + 2)
    {
        return g_strdup (string);
    }

    length = g_utf8_strlen (string, -1);

    if (length <= truncate_length)
    {
        return g_strdup (string);
    }

    num_left_chars = (truncate_length - ellipsis_length) / 2;
    num_right_chars = truncate_length - num_left_chars - ellipsis_length;

    g_assert (num_left_chars > 0);
    g_assert (num_right_chars > 0);

    left_substring = g_utf8_substring (string, 0, num_left_chars);
    right_substring = g_utf8_substring (string, length - num_right_chars, length);

    return g_strconcat (left_substring, ellipsis, right_substring, NULL);
}

/**
 * eel_str_replace_substring:
 * @string: input string
 * @substring: (not nullable): string to be replaced
 * @replacement: string used as replacement
 *
 * Returns: (transfer full): a copy of @string with all occurences of @substring
 * replaced with @replacement.
 */
char *
eel_str_replace_substring (const char *string,
                           const char *substring,
                           const char *replacement)
{
    int substring_length, replacement_length, result_length, remaining_length;
    const char *p, *substring_position;
    char *result, *result_position;

    g_return_val_if_fail (substring != NULL, g_strdup (string));
    g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

    if (string == NULL)
    {
        return NULL;
    }

    substring_length = substring ? strlen (substring) : 0;
    replacement_length = replacement ? strlen (replacement) : 0;

    result_length = strlen (string);
    for (p = string;; p = substring_position + substring_length)
    {
        substring_position = strstr (p, substring);
        if (substring_position == NULL)
        {
            break;
        }
        result_length += replacement_length - substring_length;
    }

    result = g_malloc (result_length + 1);

    result_position = result;
    for (p = string;; p = substring_position + substring_length)
    {
        substring_position = strstr (p, substring);
        if (substring_position == NULL)
        {
            remaining_length = strlen (p);
            memcpy (result_position, p, remaining_length);
            result_position += remaining_length;
            break;
        }
        memcpy (result_position, p, substring_position - p);
        result_position += substring_position - p;
        memcpy (result_position, replacement, replacement_length);
        result_position += replacement_length;
    }
    g_assert (result_position - result == result_length);
    result_position[0] = '\0';

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
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores (NULL), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores (""), "");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("_"), "__");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo"), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_bar"), "foo__bar");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_bar_2"), "foo__bar__2");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("_foo"), "__foo");
    EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_"), "foo__");

    EEL_CHECK_STRING_RESULT (eel_str_capitalize (NULL), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_capitalize (""), "");
    EEL_CHECK_STRING_RESULT (eel_str_capitalize ("foo"), "Foo");
    EEL_CHECK_STRING_RESULT (eel_str_capitalize ("Foo"), "Foo");

    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 0), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 1), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 3), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 4), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 5), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 6), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 7), "foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 0), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 1), "a_much_longer_foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 2), "a_much_longer_foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 3), "a…o");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 4), "a…oo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 5), "a_…oo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 6), "a_…foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 7), "a_m…foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 8), "a_m…_foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 9), "a_mu…_foo");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 8), "som…even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 8), "som…_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 9), "some…even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 9), "some…_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 10), "some…_even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 10), "some…g_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 11), "somet…_even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 11), "somet…g_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 12), "somet…g_even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 12), "somet…ng_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 13), "someth…g_even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 13), "something_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 14), "something_even");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 13), "something_odd");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("ääääääääää", 5), "ää…ää");
    EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("あぃいぅうぇえぉ", 7), "あぃい…ぇえぉ");

    EEL_CHECK_STRING_RESULT (eel_str_replace_substring (NULL, "foo", NULL), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring (NULL, "foo", "bar"), NULL);
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("bar", "foo", NULL), "bar");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("", "foo", ""), "");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("", "foo", "bar"), "");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("bar", "foo", ""), "bar");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("xxx", "x", "foo"), "foofoofoo");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("fff", "f", "foo"), "foofoofoo");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("foofoofoo", "foo", "f"), "fff");
    EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("foofoofoo", "f", ""), "oooooo");
}

#endif /* !EEL_OMIT_SELF_CHECK */
