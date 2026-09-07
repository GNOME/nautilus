/*
 * SPDX-FileCopyrightText: 2018 Canonical Ltd
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#pragma once

#include "nautilus-search-provider.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE_RECENT (nautilus_search_engine_recent_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSearchEngineRecent, nautilus_search_engine_recent,
                      NAUTILUS, SEARCH_ENGINE_RECENT, NautilusSearchProvider);

NautilusSearchEngineRecent *nautilus_search_engine_recent_new (void);

G_END_DECLS
