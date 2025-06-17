/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include "nautilus-query.h"

#include <glib/gi18n.h>

#include "nautilus-enum-types.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"

#define RANK_SCALE_FACTOR 100
#define MIN_RANK 10.0
#define MAX_RANK 50.0

static void
prepared_word_free (GString *string)
{
    g_string_free (string, TRUE);
}

struct _NautilusQuery
{
    GObject parent;

    char *text;
    GFile *location;
    /* MIME types - an empty array means "Any type" */
    GPtrArray *mime_types;
    gboolean show_hidden;
    GPtrArray *date_range;
    NautilusQueryRecursive recursive;
    NautilusQuerySearchType search_type;
    gboolean search_content;

    GPtrArray *prepared_words;
    GRWLock prepared_words_rwlock;
};

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
    NautilusQuery *query;

    query = NAUTILUS_QUERY (object);

    g_free (query->text);
    if (query->prepared_words != NULL)
    {
        g_ptr_array_free (query->prepared_words, TRUE);
    }
    g_clear_object (&query->location);
    g_clear_pointer (&query->mime_types, g_ptr_array_unref);
    g_clear_pointer (&query->date_range, g_ptr_array_unref);
    g_rw_lock_clear (&query->prepared_words_rwlock);

    G_OBJECT_CLASS (nautilus_query_parent_class)->finalize (object);
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
}

static void
nautilus_query_init (NautilusQuery *query)
{
    query->mime_types = g_ptr_array_new ();
    query->show_hidden = TRUE;
    query->search_type = g_settings_get_enum (nautilus_preferences, "search-filter-time-type");
    query->search_content = FALSE;
    g_rw_lock_init (&query->prepared_words_rwlock);
}

static gchar *
prepare_string_for_compare (const gchar *string)
{
    gchar *normalized, *res;

    normalized = g_utf8_normalize (string, -1, G_NORMALIZE_NFD);
    res = g_utf8_strdown (normalized, -1);
    g_free (normalized);

    return res;
}

gdouble
nautilus_query_matches_string (NautilusQuery *query,
                               const gchar   *string)
{
    g_autofree gchar *prepared_string = NULL;
    gchar *ptr = NULL;
    gboolean found = TRUE;
    gdouble retval;
    gint nonexact_malus = 0;

    if (query->text == NULL)
    {
        return 0;
    }

    prepared_string = prepare_string_for_compare (string);

    g_rw_lock_reader_lock (&query->prepared_words_rwlock);

    for (guint idx = 0; idx < query->prepared_words->len; idx++)
    {
        GString *word = query->prepared_words->pdata[idx];

        if ((ptr = strstr (prepared_string, word->str)) == NULL)
        {
            found = FALSE;
            break;
        }

        nonexact_malus += strlen (ptr) - word->len;
    }

    g_rw_lock_reader_unlock (&query->prepared_words_rwlock);

    if (!found)
    {
        return -1;
    }

    /* The rank value depends on the numbers of letters before and after the match.
     * To make the prefix matches prefered over sufix ones, the number of letters
     * after the match is divided by a factor, so that it decreases the rank by a
     * smaller amount.
     */
    retval = MAX (MIN_RANK, MAX_RANK - (gdouble) (ptr - prepared_string) - (gdouble) nonexact_malus / RANK_SCALE_FACTOR);

    return retval;
}

NautilusQuery *
nautilus_query_new (void)
{
    return g_object_new (NAUTILUS_TYPE_QUERY, NULL);
}


char *
nautilus_query_get_text (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

    return g_strdup (query->text);
}

gboolean
nautilus_query_set_text (NautilusQuery *query,
                         const char    *text)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

    /* This is the only place that sets a query text.
     * Treat empty strings as setting NULL. */
    g_autofree gchar *stripped_text = g_strstrip (g_strdup (text));
    const char *settable_text = (stripped_text == NULL || stripped_text[0] == '\0')
                                ? NULL : stripped_text;

    if (!g_set_str (&query->text, settable_text))
    {
        return FALSE;
    }

    GPtrArray *prepared_words = NULL;
    if (query->text != NULL)
    {
        g_autofree gchar *prepared_query = prepare_string_for_compare (query->text);
        g_auto (GStrv) split_query = g_strsplit (prepared_query, " ", -1);
        guint split_num = g_strv_length (split_query);

        prepared_words = g_ptr_array_new_full (split_num, (GDestroyNotify) prepared_word_free);
        for (guint i = 0; i < split_num; i += 1)
        {
            GString *word = g_string_new (split_query[i]);
            g_ptr_array_add (prepared_words, word);
        }
    }

    g_rw_lock_writer_lock (&query->prepared_words_rwlock);

    if (query->prepared_words != NULL)
    {
        g_ptr_array_free (query->prepared_words, TRUE);
    }
    query->prepared_words = prepared_words;

    g_rw_lock_writer_unlock (&query->prepared_words_rwlock);

    return TRUE;
}

GFile *
nautilus_query_get_location (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

    if (query->location == NULL)
    {
        return NULL;
    }

    return g_object_ref (query->location);
}

void
nautilus_query_set_location (NautilusQuery *query,
                             GFile         *location)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    g_set_object (&query->location, location);
}

/**
 * nautilus_query_get_mime_type:
 * @query: A #NautilusQuery
 *
 * Retrieves the current MIME Types filter from @query. Its content must not be
 * modified. It can be read by multiple threads.
 *
 * Returns: (transfer container) A #GPtrArray reference with MIME type name strings.
 */
GPtrArray *
nautilus_query_get_mime_types (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

    return g_ptr_array_ref (query->mime_types);
}

/**
 * nautilus_query_set_mime_types:
 * @query: A #NautilusQuery
 * @mime_types: (transfer none): A #GPtrArray of MIME type strings
 *
 * Set a new MIME types filter for @query. Once set, the filter must not be
 * modified, and it can only be replaced by setting another filter.
 *
 * Search engines that are already running for a previous filter will ignore the
 * new filter. So, the caller must ensure that the search will be reloaded
 * afterwards.
 */
void
nautilus_query_set_mime_types (NautilusQuery *query,
                               GPtrArray     *mime_types)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));
    g_return_if_fail (mime_types != NULL);

    g_clear_pointer (&query->mime_types, g_ptr_array_unref);
    query->mime_types = g_ptr_array_ref (mime_types);
}

gboolean
nautilus_query_get_show_hidden_files (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

    return query->show_hidden;
}

void
nautilus_query_set_show_hidden_files (NautilusQuery *query,
                                      gboolean       show_hidden)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    query->show_hidden = show_hidden;
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
    if (query == NULL || query->text == NULL)
    {
        return g_strdup (_("Search"));
    }

    return g_strdup_printf (_("Search for “%s”"), query->text);
}

gboolean
nautilus_query_get_search_content (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), -1);

    return query->search_content;
}

void
nautilus_query_set_search_content (NautilusQuery *query,
                                   gboolean       search_content)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    query->search_content = search_content;
}

NautilusQuerySearchType
nautilus_query_get_search_type (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), -1);

    return query->search_type;
}

void
nautilus_query_set_search_type (NautilusQuery           *query,
                                NautilusQuerySearchType  type)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    query->search_type = type;
}

/**
 * nautilus_query_get_date_range:
 * @query: a #NautilusQuery
 *
 * Retrieves the #GptrArray composed of #GDateTime representing the date range.
 * This function is thread safe.
 *
 * Returns: (transfer full): the #GptrArray composed of #GDateTime representing the date range.
 */
GPtrArray *
nautilus_query_get_date_range (NautilusQuery *query)
{
    static GMutex mutex;

    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

    g_mutex_lock (&mutex);
    if (query->date_range)
    {
        g_ptr_array_ref (query->date_range);
    }
    g_mutex_unlock (&mutex);

    return query->date_range;
}

void
nautilus_query_set_date_range (NautilusQuery *query,
                               GPtrArray     *date_range)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    g_clear_pointer (&query->date_range, g_ptr_array_unref);
    if (date_range)
    {
        query->date_range = g_ptr_array_ref (date_range);
    }
}

NautilusQueryRecursive
nautilus_query_get_recursive (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query),
                          NAUTILUS_QUERY_RECURSIVE_ALWAYS);

    return query->recursive;
}

void
nautilus_query_set_recursive (NautilusQuery          *query,
                              NautilusQueryRecursive  recursive)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    query->recursive = recursive;
}

gboolean
nautilus_query_is_empty (NautilusQuery *query)
{
    if (!query)
    {
        return TRUE;
    }

    if (!query->date_range &&
        query->text == NULL &&
        query->mime_types->len == 0)
    {
        return TRUE;
    }

    return FALSE;
}

gboolean
nautilus_query_is_global (NautilusQuery *self)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (self), FALSE);

    return (self->location == NULL);
}
