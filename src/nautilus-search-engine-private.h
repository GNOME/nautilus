/*
 * Copyright (C) 2018 Canonical Ltd.
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
 * Author: Marco Trevisan <marco@ubuntu.com>
 *
 */

#pragma once

#include "nautilus-query.h"

typedef enum {
        NAUTILUS_SEARCH_ENGINE_TYPE_NON_INDEXED,
        NAUTILUS_SEARCH_ENGINE_TYPE_INDEXED,
} NautilusSearchEngineType;

gboolean is_recursive_search (NautilusSearchEngineType engine_type, NautilusQueryRecursive recursive, GFile *location);
