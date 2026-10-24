/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define G_LOG_DOMAIN "nautilus-search-hit"

#include <config.h>

#include <string.h>
#include <gio/gio.h>

#include "nautilus-date-utilities.h"
#include "nautilus-search-hit.h"

struct _NautilusSearchHit
{
    GObject parent_instance;

    char *uri;

    GDateTime *modification_time;
    GDateTime *access_time;
    GDateTime *creation_time;
    double rank;
    gchar *fts_snippet;

    gdouble relevance;
};

G_DEFINE_FINAL_TYPE (NautilusSearchHit, nautilus_search_hit, G_TYPE_OBJECT)

void
nautilus_search_hit_compute_scores (NautilusSearchHit *hit,
                                    GDateTime         *now,
                                    GFile             *query_location)
{
    guint dir_count = 0;
    GTimeSpan m_diff = G_MAXINT64;
    GTimeSpan a_diff = G_MAXINT64;
    GTimeSpan t_diff = G_MAXINT64;
    gdouble recent_bonus = 0.0;
    gdouble proximity_bonus = 0.0;
    gdouble match_bonus = 0.0;

    if (query_location != NULL)
    {
        g_autoptr (GFile) hit_location = g_file_new_for_uri (hit->uri);
        g_autofree gchar *relative_path = g_file_get_relative_path (query_location, hit_location);

        for (gchar *c = relative_path; c != NULL && *c != '\0'; c++)
        {
            if (*c == G_DIR_SEPARATOR)
            {
                dir_count++;
            }
        }

        if (dir_count < 10)
        {
            proximity_bonus = 10000.0 - 1000.0 * dir_count;
        }
    }

    /* Recency bonus is useful for recursive search, but unwanted for results
     * from the current folder, which should always sort by filename match,
     * which makes prefix matches sort first. */
    if (dir_count != 0 || query_location == NULL)
    {
        if (hit->modification_time != NULL)
        {
            m_diff = g_date_time_difference (now, hit->modification_time);
        }
        if (hit->access_time != NULL)
        {
            a_diff = g_date_time_difference (now, hit->access_time);
        }
        m_diff /= G_TIME_SPAN_DAY;
        a_diff /= G_TIME_SPAN_DAY;
        t_diff = MIN (m_diff, a_diff);
        if (t_diff > 90)
        {
            recent_bonus = 0.0;
        }
        else if (t_diff > 30)
        {
            recent_bonus = 10.0;
        }
        else if (t_diff > 14)
        {
            recent_bonus = 30.0;
        }
        else if (t_diff > 7)
        {
            recent_bonus = 50.0;
        }
        else if (t_diff > 1)
        {
            recent_bonus = 70.0;
        }
        else
        {
            recent_bonus = 100.0;
        }
    }

    if (hit->rank > 0)
    {
        match_bonus = MIN (500, 10.0 * hit->rank);
    }
    else
    {
        match_bonus = 0.0;
    }

    hit->relevance = recent_bonus + proximity_bonus + match_bonus;

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_debug ("Hit %s computed relevance %.2f (%.2f + %.2f + %.2f)",
                 hit->uri, hit->relevance, proximity_bonus, recent_bonus, match_bonus);
    }
}

const char *
nautilus_search_hit_get_uri (NautilusSearchHit *hit)
{
    return hit->uri;
}

gdouble
nautilus_search_hit_get_relevance (NautilusSearchHit *hit)
{
    return hit->relevance;
}

const gchar *
nautilus_search_hit_get_fts_snippet (NautilusSearchHit *hit)
{
    return hit->fts_snippet;
}

static void
nautilus_search_hit_finalize (GObject *object)
{
    NautilusSearchHit *hit = NAUTILUS_SEARCH_HIT (object);

    g_free (hit->uri);
    g_clear_pointer (&hit->access_time, g_date_time_unref);
    g_clear_pointer (&hit->modification_time, g_date_time_unref);
    g_clear_pointer (&hit->creation_time, g_date_time_unref);
    g_free (hit->fts_snippet);

    G_OBJECT_CLASS (nautilus_search_hit_parent_class)->finalize (object);
}

static void
nautilus_search_hit_class_init (NautilusSearchHitClass *class)
{
    GObjectClass *object_class;

    object_class = (GObjectClass *) class;

    object_class->finalize = nautilus_search_hit_finalize;
}

static void
nautilus_search_hit_init (NautilusSearchHit *hit)
{
}

/**
 * nautilus_search_hit_new:
 * @uri: (tranfer full): URI of the search hit
 * @rank: Full-text search rank of the search hit
 * @fts_snippet: (tranfer full) (nullable): Text snippet from full-text search results
 * @modification_time: (tranfer full) (nullable): Modification time
 * @access_time:  (tranfer full) (nullable): Access time
 * @creation_time:  (tranfer full) (nullable): Creation time
 *
 * Creates a new #NautilusSearchHit.
 *
 * Returns: (tranfer full): A new #NautilusSearchHit.
 */
NautilusSearchHit *
nautilus_search_hit_new (char      *uri,
                         double     rank,
                         char      *fts_snippet,
                         GDateTime *modification_time,
                         GDateTime *access_time,
                         GDateTime *creation_time)
{
    NautilusSearchHit *hit = g_object_new (NAUTILUS_TYPE_SEARCH_HIT, NULL);

    hit->uri = uri;
    hit->rank = rank;
    hit->fts_snippet = fts_snippet;
    hit->modification_time = modification_time;
    hit->access_time = access_time;
    hit->creation_time = creation_time;

    return hit;
}
