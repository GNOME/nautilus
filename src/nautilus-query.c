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

#include "nautilus-date-utilities.h"
#include "nautilus-enum-types.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-scheme.h"

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
    NautilusSpeedTradeoffValue recursion_tradeoff;
    NautilusSearchTimeType search_type;
    gboolean search_content;

    GPtrArray *prepared_words;
};

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

static NautilusSpeedTradeoffValue
get_recursion_tradeoff (GFile *location)
{
    NautilusSpeedTradeoffValue tradeoff = g_settings_get_enum (
        nautilus_preferences, "recursive-search");

    if (tradeoff != NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY)
    {
        return tradeoff;
    }
    else if (location == NULL)
    {
        /* Local-only without location -> never */
        return NAUTILUS_SPEED_TRADEOFF_NEVER;
    }

    g_autoptr (NautilusFile) file = nautilus_file_get_existing (location);
    if (file != NULL && !nautilus_file_is_remote (file))
    {
        /* It's up to the search engine to check whether it can proceed with
         * deep search in the current directory or not. */
        return NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY;
    }
    else
    {
        return NAUTILUS_SPEED_TRADEOFF_NEVER;
    }
}

static void
finalize (GObject *object)
{
    NautilusQuery *query;

    query = NAUTILUS_QUERY (object);

    g_free (query->text);
    g_clear_pointer (&query->prepared_words, g_ptr_array_unref);
    g_clear_object (&query->location);
    g_clear_pointer (&query->mime_types, g_ptr_array_unref);
    g_clear_pointer (&query->date_range, g_ptr_array_unref);

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
    nautilus_query_update_recursive_setting (query);
    nautilus_query_update_search_content (query);
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

NautilusQuery *
nautilus_query_copy (NautilusQuery *query)
{
    NautilusQuery *copy = g_object_new (NAUTILUS_TYPE_QUERY, NULL);
    g_autoptr (GPtrArray) mime_types = nautilus_query_get_mime_types (query);

    copy->text = nautilus_query_get_text (query);
    copy->location = nautilus_query_get_location (query);
    g_set_ptr_array (&copy->mime_types, mime_types);
    copy->show_hidden = query->show_hidden;
    copy->date_range = nautilus_query_get_date_range (query);
    copy->recursion_tradeoff = query->recursion_tradeoff;
    copy->search_type = query->search_type;
    copy->search_content = query->search_content;
    g_set_ptr_array (&copy->prepared_words, query->prepared_words);

    return copy;
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

    g_autoptr (GPtrArray) prepared_words = NULL;
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

    g_set_ptr_array (&query->prepared_words, prepared_words);

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

    if (g_set_object (&query->location, location))
    {
        nautilus_query_update_recursive_setting (query);
        nautilus_query_update_search_content (query);
    }
}

/**
 * nautilus_query_get_mime_type:
 * @query: A #NautilusQuery
 *
 * Retrieves the current MIME Types filter from @query. Its content must not be
 * modified.
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

    g_set_ptr_array (&query->mime_types, mime_types);
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

gboolean
nautilus_query_get_search_content (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), -1);

    return query->search_content;
}


/** Returns: whether full text search is available */
gboolean
nautilus_query_can_search_content (NautilusQuery *self)
{
    if (self->location == NULL)
    {
        return TRUE;
    }
    else if (g_file_has_uri_scheme (self->location, SCHEME_NETWORK))
    {
        return FALSE;
    }
    else if (nautilus_query_recursive_local_only (self))
    {
        g_autoptr (NautilusFile) file = nautilus_file_get (self->location);
        return !nautilus_file_is_remote (file);
    }
    else
    {
        return TRUE;
    }
}

/**
 * Returns: Whether the query has changed
 */
gboolean
nautilus_query_update_search_content (NautilusQuery *self)
{
    gboolean old_search_content = self->search_content;

    self->search_content = nautilus_query_can_search_content (self) &&
                           g_settings_get_boolean (nautilus_preferences,
                                                   NAUTILUS_PREFERENCES_FTS_ENABLED);

    return old_search_content != self->search_content;
}

NautilusSearchTimeType
nautilus_query_get_search_type (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), -1);

    return query->search_type;
}

void
nautilus_query_set_search_type (NautilusQuery          *query,
                                NautilusSearchTimeType  type)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    query->search_type = type;
}

/**
 * nautilus_query_get_date_range:
 * @query: a #NautilusQuery
 *
 * Retrieves the #GptrArray composed of #GDateTime representing the date range.
 *
 * Returns: (transfer full): the #GptrArray composed of #GDateTime representing the date range.
 */
GPtrArray *
nautilus_query_get_date_range (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

    return query->date_range != NULL ? g_ptr_array_ref (query->date_range) : NULL;
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

/** Returns: whether recursive search is generally enabled */
gboolean
nautilus_query_recursive (NautilusQuery *self)
{
    return self->recursion_tradeoff != NAUTILUS_SPEED_TRADEOFF_NEVER;
}

/** Returns: whether recursive search is only enabled for local paths */
gboolean
nautilus_query_recursive_local_only (NautilusQuery *self)
{
    return self->recursion_tradeoff == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY;
}

/**
 * Returns: Whether the query has changed
 */
gboolean
nautilus_query_update_recursive_setting (NautilusQuery *self)
{
    NautilusSpeedTradeoffValue old_tradeoff = self->recursion_tradeoff;

    self->recursion_tradeoff = get_recursion_tradeoff (self->location);

    return old_tradeoff != self->recursion_tradeoff;
}

gboolean
nautilus_query_has_active_filter (NautilusQuery *self)
{
    return self->date_range != NULL ||
           self->mime_types->len > 0 ||
           !self->search_content;
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
