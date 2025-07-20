/*
 * Copyright (C) 2005 Novell, Inc.
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
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

void
nautilus_search_engine_start (NautilusSearchEngine *self,
                              NautilusQuery        *query);
void
nautilus_search_engine_stop (NautilusSearchEngine *self);

G_END_DECLS