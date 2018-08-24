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

#include "nautilus-directory.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-search-provider.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE		(nautilus_search_engine_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusSearchEngine, nautilus_search_engine, NAUTILUS, SEARCH_ENGINE, GObject)

struct _NautilusSearchEngineClass
{
  GObjectClass parent_class;
};

NautilusSearchEngine *nautilus_search_engine_new                (void);
NautilusSearchEngineModel *
                      nautilus_search_engine_get_model_provider (NautilusSearchEngine *engine);

G_END_DECLS

void nautilus_search_engine_start_all_engines (NautilusSearchProvider *provider);
void nautilus_search_engine_start_tracker (NautilusSearchProvider *provider);
void nautilus_search_engine_start_recent (NautilusSearchProvider *provider);
void nautilus_search_engine_start_model (NautilusSearchProvider *provider);
void nautilus_search_engine_start_simple (NautilusSearchProvider *provider);