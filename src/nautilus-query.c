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

struct _NautilusQuery
{
    GObject parent;

    char *text;
    GFile *location;
    GPtrArray *mime_types;
    gboolean show_hidden;
    GPtrArray *date_range;
    NautilusQueryRecursive recursive;
    NautilusQuerySearchType search_type;
    NautilusQuerySearchContent search_content;

    gboolean searching;
    char **prepared_words;
    GMutex prepared_words_mutex;
};

static void  nautilus_query_class_init (NautilusQueryClass *class);
static void  nautilus_query_init (NautilusQuery *query);

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_DATE_RANGE,
    PROP_LOCATION,
    PROP_MIMETYPES,
    PROP_RECURSIVE,
    PROP_SEARCH_TYPE,
    PROP_SEARCHING,
    PROP_SHOW_HIDDEN,
    PROP_TEXT,
    LAST_PROP
};

static void
finalize (GObject *object)
{
    NautilusQuery *query;

    query = NAUTILUS_QUERY (object);

    g_free (query->text);
    g_strfreev (query->prepared_words);
    g_clear_object (&query->location);
    g_clear_pointer (&query->mime_types, g_ptr_array_unref);
    g_clear_pointer (&query->date_range, g_ptr_array_unref);
    g_mutex_clear (&query->prepared_words_mutex);

    G_OBJECT_CLASS (nautilus_query_parent_class)->finalize (object);
}

static void
nautilus_query_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    NautilusQuery *self = NAUTILUS_QUERY (object);

    switch (prop_id)
    {
        case PROP_DATE_RANGE:
        {
            g_value_set_pointer (value, self->date_range);
        }
        break;

        case PROP_LOCATION:
        {
            g_value_set_object (value, self->location);
        }
        break;

        case PROP_MIMETYPES:
        {
            g_value_set_pointer (value, self->mime_types);
        }
        break;

        case PROP_RECURSIVE:
        {
            g_value_set_enum (value, self->recursive);
        }
        break;

        case PROP_SEARCH_TYPE:
        {
            g_value_set_enum (value, self->search_type);
        }
        break;

        case PROP_SEARCHING:
        {
            g_value_set_boolean (value, self->searching);
        }
        break;

        case PROP_SHOW_HIDDEN:
        {
            g_value_set_boolean (value, self->show_hidden);
        }
        break;

        case PROP_TEXT:
        {
            g_value_set_string (value, self->text);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_query_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    NautilusQuery *self = NAUTILUS_QUERY (object);

    switch (prop_id)
    {
        case PROP_DATE_RANGE:
        {
            nautilus_query_set_date_range (self, g_value_get_pointer (value));
        }
        break;

        case PROP_LOCATION:
        {
            nautilus_query_set_location (self, g_value_get_object (value));
        }
        break;

        case PROP_MIMETYPES:
        {
            nautilus_query_set_mime_types (self, g_value_get_pointer (value));
        }
        break;

        case PROP_RECURSIVE:
        {
            nautilus_query_set_recursive (self, g_value_get_enum (value));
        }
        break;

        case PROP_SEARCH_TYPE:
        {
            nautilus_query_set_search_type (self, g_value_get_enum (value));
        }
        break;

        case PROP_SEARCHING:
        {
            nautilus_query_set_searching (self, g_value_get_boolean (value));
        }
        break;

        case PROP_SHOW_HIDDEN:
        {
            nautilus_query_set_show_hidden_files (self, g_value_get_boolean (value));
        }
        break;

        case PROP_TEXT:
        {
            nautilus_query_set_text (self, g_value_get_string (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
    gobject_class->get_property = nautilus_query_get_property;
    gobject_class->set_property = nautilus_query_set_property;

    /**
     * NautilusQuery::date-range:
     *
     * The date range of the query.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_DATE_RANGE,
                                     g_param_spec_pointer ("date-range",
                                                           "Date range of the query",
                                                           "The range date of the query",
                                                           G_PARAM_READWRITE));

    /**
     * NautilusQuery::location:
     *
     * The location of the query.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_LOCATION,
                                     g_param_spec_object ("location", NULL, NULL,
                                                          G_TYPE_FILE,
                                                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

    /**
     * NautilusQuery::mimetypes: (type GPtrArray) (element-type gchar*)
     *
     * MIME types the query holds. An empty array means "Any type".
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_MIMETYPES,
                                     g_param_spec_pointer ("mimetypes",
                                                           "MIME types of the query",
                                                           "The MIME types of the query",
                                                           G_PARAM_READWRITE));

    /**
     * NautilusQuery::recursive:
     *
     * Whether the query is being performed on subdirectories or not.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_RECURSIVE,
                                     g_param_spec_enum ("recursive",
                                                        "Whether the query is being performed on subdirectories",
                                                        "Whether the query is being performed on subdirectories or not",
                                                        NAUTILUS_TYPE_QUERY_RECURSIVE,
                                                        NAUTILUS_QUERY_RECURSIVE_ALWAYS,
                                                        G_PARAM_READWRITE));

    /**
     * NautilusQuery::search-type:
     *
     * The search type of the query.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCH_TYPE,
                                     g_param_spec_enum ("search-type",
                                                        "Type of the query",
                                                        "The type of the query",
                                                        NAUTILUS_TYPE_QUERY_SEARCH_TYPE,
                                                        NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED,
                                                        G_PARAM_READWRITE));

    /**
     * NautilusQuery::searching:
     *
     * Whether the query is being performed or not.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SEARCHING,
                                     g_param_spec_boolean ("searching",
                                                           "Whether the query is being performed",
                                                           "Whether the query is being performed or not",
                                                           FALSE,
                                                           G_PARAM_READWRITE));

    /**
     * NautilusQuery::show-hidden:
     *
     * Whether the search should include hidden files.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_SHOW_HIDDEN,
                                     g_param_spec_boolean ("show-hidden",
                                                           "Show hidden files",
                                                           "Whether the search should show hidden files",
                                                           FALSE,
                                                           G_PARAM_READWRITE));

    /**
     * NautilusQuery::text:
     *
     * The search string.
     *
     */
    g_object_class_install_property (gobject_class,
                                     PROP_TEXT,
                                     g_param_spec_string ("text",
                                                          "Text of the search",
                                                          "The text string of the search",
                                                          NULL,
                                                          G_PARAM_READWRITE));
}

static void
nautilus_query_init (NautilusQuery *query)
{
    query->mime_types = g_ptr_array_new ();
    query->show_hidden = TRUE;
    query->search_type = g_settings_get_enum (nautilus_preferences, "search-filter-time-type");
    query->search_content = NAUTILUS_QUERY_SEARCH_CONTENT_SIMPLE;
    g_mutex_init (&query->prepared_words_mutex);
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
    gchar *prepared_string, *ptr;
    gboolean found;
    gdouble retval;
    gint idx, nonexact_malus;

    if (!query->text)
    {
        return -1;
    }

    g_mutex_lock (&query->prepared_words_mutex);
    if (!query->prepared_words)
    {
        prepared_string = prepare_string_for_compare (query->text);
        query->prepared_words = g_strsplit (prepared_string, " ", -1);
        g_free (prepared_string);
    }

    prepared_string = prepare_string_for_compare (string);
    found = TRUE;
    ptr = NULL;
    nonexact_malus = 0;

    for (idx = 0; query->prepared_words[idx] != NULL; idx++)
    {
        if ((ptr = strstr (prepared_string, query->prepared_words[idx])) == NULL)
        {
            found = FALSE;
            break;
        }

        nonexact_malus += strlen (ptr) - strlen (query->prepared_words[idx]);
    }
    g_mutex_unlock (&query->prepared_words_mutex);

    if (!found)
    {
        g_free (prepared_string);
        return -1;
    }

    /* The rank value depends on the numbers of letters before and after the match.
     * To make the prefix matches prefered over sufix ones, the number of letters
     * after the match is divided by a factor, so that it decreases the rank by a
     * smaller amount.
     */
    retval = MAX (MIN_RANK, MAX_RANK - (gdouble) (ptr - prepared_string) - (gdouble) nonexact_malus / RANK_SCALE_FACTOR);
    g_free (prepared_string);

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

void
nautilus_query_set_text (NautilusQuery *query,
                         const char    *text)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    g_free (query->text);
    query->text = g_strstrip (g_strdup (text));

    g_mutex_lock (&query->prepared_words_mutex);
    g_strfreev (query->prepared_words);
    query->prepared_words = NULL;
    g_mutex_unlock (&query->prepared_words_mutex);

    g_object_notify (G_OBJECT (query), "text");
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
        g_object_notify (G_OBJECT (query), "location");
    }
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

    g_object_notify (G_OBJECT (query), "mimetypes");
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

    if (query->show_hidden != show_hidden)
    {
        query->show_hidden = show_hidden;
        g_object_notify (G_OBJECT (query), "show-hidden");
    }
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
    if (!query || !query->text || query->text[0] == '\0')
    {
        return g_strdup (_("Search"));
    }

    return g_strdup_printf (_("Search for “%s”"), query->text);
}

NautilusQuerySearchContent
nautilus_query_get_search_content (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), -1);

    return query->search_content;
}

void
nautilus_query_set_search_content (NautilusQuery              *query,
                                   NautilusQuerySearchContent  content)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    if (query->search_content != content)
    {
        query->search_content = content;
        g_object_notify (G_OBJECT (query), "search-type");
    }
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

    if (query->search_type != type)
    {
        query->search_type = type;
        g_object_notify (G_OBJECT (query), "search-type");
    }
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

    g_object_notify (G_OBJECT (query), "date-range");
}

gboolean
nautilus_query_get_searching (NautilusQuery *query)
{
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

    return query->searching;
}

void
nautilus_query_set_searching (NautilusQuery *query,
                              gboolean       searching)
{
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    searching = !!searching;

    if (query->searching != searching)
    {
        query->searching = searching;

        g_object_notify (G_OBJECT (query), "searching");
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

    if (query->recursive != recursive)
    {
        query->recursive = recursive;

        g_object_notify (G_OBJECT (query), "recursive");
    }
}

gboolean
nautilus_query_is_empty (NautilusQuery *query)
{
    if (!query)
    {
        return TRUE;
    }

    if (!query->date_range &&
        (!query->text || (query->text && query->text[0] == '\0')) &&
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
