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

#pragma once

#include "nautilus-enums.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_QUERY		(nautilus_query_get_type ())

G_DECLARE_FINAL_TYPE (NautilusQuery, nautilus_query, NAUTILUS, QUERY, GObject)

NautilusQuery* nautilus_query_new      (void);
NautilusQuery* nautilus_query_copy     (NautilusQuery *query);

char *         nautilus_query_get_text           (NautilusQuery *query);
gboolean       nautilus_query_set_text           (NautilusQuery *query, const char *text);

gboolean       nautilus_query_get_show_hidden_files (NautilusQuery *query);
void           nautilus_query_set_show_hidden_files (NautilusQuery *query, gboolean show_hidden);

GFile*         nautilus_query_get_location       (NautilusQuery *query);
void           nautilus_query_set_location       (NautilusQuery *query,
                                                  GFile         *location);

GPtrArray *    nautilus_query_get_mime_types     (NautilusQuery *query);
void           nautilus_query_set_mime_types     (NautilusQuery *query, GPtrArray *mime_types);

gboolean
nautilus_query_can_search_content (NautilusQuery *self);
gboolean
nautilus_query_get_search_content (NautilusQuery *query);
gboolean
nautilus_query_update_search_content (NautilusQuery *self);

NautilusSearchTimeType nautilus_query_get_search_type (NautilusQuery *query);
void                   nautilus_query_set_search_type (NautilusQuery           *query,
                                                       NautilusSearchTimeType   type);

GPtrArray*     nautilus_query_get_date_range     (NautilusQuery *query);
void           nautilus_query_set_date_range     (NautilusQuery *query,
                                                  GPtrArray     *date_range);

gboolean
nautilus_query_recursive (NautilusQuery *self);
gboolean
nautilus_query_recursive_local_only (NautilusQuery *self);
gboolean
nautilus_query_update_recursive_setting (NautilusQuery *self);

gdouble        nautilus_query_matches_string     (NautilusQuery *query, const gchar *string);

gboolean       nautilus_query_has_active_filter  (NautilusQuery *query);
gboolean       nautilus_query_is_empty           (NautilusQuery *query);
gboolean       nautilus_query_is_global          (NautilusQuery *query);
