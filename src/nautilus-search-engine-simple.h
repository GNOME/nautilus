/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "nautilus-search-provider.h"

#include <glib-object.h>

#pragma once

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE (nautilus_search_engine_simple_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchEngineSimple, nautilus_search_engine_simple,
                      NAUTILUS, SEARCH_ENGINE_SIMPLE, NautilusSearchProvider);

NautilusSearchEngineSimple* nautilus_search_engine_simple_new (void);

G_END_DECLS
