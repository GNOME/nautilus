/*
 * SPDX-FileCopyrightText: 2005 Mr Jamie McCracken
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Jamie McCracken (jamiemcc@gnome.org)
 */

#pragma once

#include "nautilus-search-provider.h"

#include <glib-object.h>

#define NAUTILUS_TYPE_SEARCH_ENGINE_LOCALSEARCH (nautilus_search_engine_localsearch_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSearchEngineLocalsearch, nautilus_search_engine_localsearch,
                      NAUTILUS, SEARCH_ENGINE_LOCALSEARCH, NautilusSearchProvider)

NautilusSearchEngineLocalsearch* nautilus_search_engine_localsearch_new (void);
