/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-types.h"

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_HIT (nautilus_search_hit_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchHit, nautilus_search_hit, NAUTILUS, SEARCH_HIT, GObject);

NautilusSearchHit * nautilus_search_hit_new                   (const char        *uri);

void                nautilus_search_hit_set_fts_rank          (NautilusSearchHit *hit,
							       gdouble            fts_rank);
void                nautilus_search_hit_set_modification_time (NautilusSearchHit *hit,
							       GDateTime         *date);
void                nautilus_search_hit_set_access_time       (NautilusSearchHit *hit,
							       GDateTime         *date);
void                nautilus_search_hit_set_creation_time     (NautilusSearchHit *hit,
							       GDateTime         *date);
void                nautilus_search_hit_set_fts_snippet       (NautilusSearchHit *hit,
                                                               const gchar       *snippet);
void                nautilus_search_hit_compute_scores        (NautilusSearchHit *hit,
                                                               GDateTime         *now,
                                                               GFile             *query_location);

const char *        nautilus_search_hit_get_uri               (NautilusSearchHit *hit);
gdouble             nautilus_search_hit_get_relevance         (NautilusSearchHit *hit);
const gchar *       nautilus_search_hit_get_fts_snippet       (NautilusSearchHit *hit);

G_END_DECLS
