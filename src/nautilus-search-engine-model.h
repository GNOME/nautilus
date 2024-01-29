/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#pragma once

#include "nautilus-directory.h"

#define NAUTILUS_TYPE_SEARCH_ENGINE_MODEL (nautilus_search_engine_model_get_type ())
G_DECLARE_FINAL_TYPE (NautilusSearchEngineModel, nautilus_search_engine_model, NAUTILUS, SEARCH_ENGINE_MODEL, GObject)

NautilusSearchEngineModel* nautilus_search_engine_model_new       (void);
void                       nautilus_search_engine_model_set_model (NautilusSearchEngineModel *model,
								   NautilusDirectory         *directory);
NautilusDirectory *        nautilus_search_engine_model_get_model (NautilusSearchEngineModel *model);
