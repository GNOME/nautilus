/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 */

#pragma once

#include <glib-object.h>
#include "nautilus-query.h"

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
							       NautilusQuery     *query);

const char *        nautilus_search_hit_get_uri               (NautilusSearchHit *hit);
gdouble             nautilus_search_hit_get_relevance         (NautilusSearchHit *hit);
const gchar *       nautilus_search_hit_get_fts_snippet       (NautilusSearchHit *hit);

G_END_DECLS
