/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
    char *uri;

    gdouble relevance;
    gdouble fts_rank;
    gchar *fts_snippet;

	GDateTime *modification_time;
    GDateTime *access_time;
    GDateTime *creation_time;
} NautilusSearchHit;

void
nautilus_search_hit_free (NautilusSearchHit *self);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusSearchHit, nautilus_search_hit_free)



void                nautilus_search_hit_compute_scores        (NautilusSearchHit *hit,
                                                               GDateTime         *now,
                                                               GFile             *query_location);

G_END_DECLS
