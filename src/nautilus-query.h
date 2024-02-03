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

#include <glib-object.h>
#include <gio/gio.h>

typedef enum {
        NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS,
        NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED,
        NAUTILUS_QUERY_SEARCH_TYPE_CREATED
} NautilusQuerySearchType;

typedef enum {
        NAUTILUS_QUERY_SEARCH_CONTENT_SIMPLE,
        NAUTILUS_QUERY_SEARCH_CONTENT_FULL_TEXT,
} NautilusQuerySearchContent;

typedef enum {
        NAUTILUS_QUERY_RECURSIVE_NEVER,
        NAUTILUS_QUERY_RECURSIVE_ALWAYS,
        NAUTILUS_QUERY_RECURSIVE_LOCAL_ONLY,
        NAUTILUS_QUERY_RECURSIVE_INDEXED_ONLY,
} NautilusQueryRecursive;

#define NAUTILUS_TYPE_QUERY		(nautilus_query_get_type ())

G_DECLARE_FINAL_TYPE (NautilusQuery, nautilus_query, NAUTILUS, QUERY, GObject)

NautilusQuery* nautilus_query_new      (void);

char *         nautilus_query_get_text           (NautilusQuery *query);
void           nautilus_query_set_text           (NautilusQuery *query, const char *text);

gboolean       nautilus_query_get_show_hidden_files (NautilusQuery *query);
void           nautilus_query_set_show_hidden_files (NautilusQuery *query, gboolean show_hidden);

GFile*         nautilus_query_get_location       (NautilusQuery *query);
void           nautilus_query_set_location       (NautilusQuery *query,
                                                  GFile         *location);

GPtrArray *    nautilus_query_get_mime_types     (NautilusQuery *query);
void           nautilus_query_set_mime_types     (NautilusQuery *query, GPtrArray *mime_types);

NautilusQuerySearchContent nautilus_query_get_search_content (NautilusQuery *query);
void                       nautilus_query_set_search_content (NautilusQuery              *query,
                                                              NautilusQuerySearchContent  content);

NautilusQuerySearchType nautilus_query_get_search_type (NautilusQuery *query);
void                    nautilus_query_set_search_type (NautilusQuery           *query,
                                                        NautilusQuerySearchType  type);

GPtrArray*     nautilus_query_get_date_range     (NautilusQuery *query);
void           nautilus_query_set_date_range     (NautilusQuery *query,
                                                  GPtrArray     *date_range);

NautilusQueryRecursive nautilus_query_get_recursive (NautilusQuery *query);
void                   nautilus_query_set_recursive (NautilusQuery          *query,
                                                     NautilusQueryRecursive  recursive);

gboolean       nautilus_query_get_searching      (NautilusQuery *query);

void           nautilus_query_set_searching      (NautilusQuery *query,
                                                  gboolean       searching);

gdouble        nautilus_query_matches_string     (NautilusQuery *query, const gchar *string);

char *         nautilus_query_to_readable_string (NautilusQuery *query);

gboolean       nautilus_query_is_empty           (NautilusQuery *query);
gboolean       nautilus_query_is_global          (NautilusQuery *query);
