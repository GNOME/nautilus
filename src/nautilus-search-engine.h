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

#include "nautilus-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    NAUTILUS_SEARCH_TYPE_LOCALSEARCH = 1 << 0,
    NAUTILUS_SEARCH_TYPE_MODEL       = 1 << 1,
    NAUTILUS_SEARCH_TYPE_RECENT      = 1 << 2,
    NAUTILUS_SEARCH_TYPE_SIMPLE      = 1 << 3,

    NAUTILUS_SEARCH_TYPE_FOLDER = NAUTILUS_SEARCH_TYPE_LOCALSEARCH |
                                  NAUTILUS_SEARCH_TYPE_MODEL |
                                  NAUTILUS_SEARCH_TYPE_SIMPLE,
    /* This is used for both "Search Everywhere" and shell search provider. */
    NAUTILUS_SEARCH_TYPE_GLOBAL = NAUTILUS_SEARCH_TYPE_LOCALSEARCH |
                                  NAUTILUS_SEARCH_TYPE_RECENT,

    NAUTILUS_SEARCH_TYPE_ALL    = NAUTILUS_SEARCH_TYPE_FOLDER | NAUTILUS_SEARCH_TYPE_GLOBAL,
} NautilusSearchType;

#define NAUTILUS_TYPE_SEARCH_ENGINE		(nautilus_search_engine_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchEngine, nautilus_search_engine, NAUTILUS, SEARCH_ENGINE, GObject)

NautilusSearchEngine *
nautilus_search_engine_new (NautilusSearchType search_type);

void
nautilus_search_engine_set_search_type (NautilusSearchEngine *self,
                                        NautilusSearchType search_type);

G_END_DECLS