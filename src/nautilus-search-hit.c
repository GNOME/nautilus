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

    if (hit->fts_rank > 0)
    {
        match_bonus = MIN (500, 10.0 * hit->fts_rank);
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

void
nautilus_search_hit_free (NautilusSearchHit *self)
{
    if (self == NULL)
    {
        return;
    }

    g_free (self->uri);
    g_clear_pointer (&self->modification_time, g_date_time_unref);
    g_clear_pointer (&self->access_time, g_date_time_unref);
    g_clear_pointer (&self->creation_time, g_date_time_unref);
    g_free (self->fts_snippet);

    g_free (self);
}
