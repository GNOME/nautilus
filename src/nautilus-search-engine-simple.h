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

#include <glib-object.h>

#pragma once

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE (nautilus_search_engine_simple_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchEngineSimple, nautilus_search_engine_simple, NAUTILUS, SEARCH_ENGINE_SIMPLE, GObject);

NautilusSearchEngineSimple* nautilus_search_engine_simple_new (void);

G_END_DECLS
