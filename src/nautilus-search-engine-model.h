/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include "nautilus-search-provider.h"

#include <glib-object.h>

#define NAUTILUS_TYPE_SEARCH_ENGINE_MODEL (nautilus_search_engine_model_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSearchEngineModel, nautilus_search_engine_model,
                      NAUTILUS, SEARCH_ENGINE_MODEL, NautilusSearchProvider);

NautilusSearchEngineModel* nautilus_search_engine_model_new       (void);
