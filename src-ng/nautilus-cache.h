/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib-object.h>

#define NAUTILUS_TYPE_CACHE (nautilus_cache_get_type ())

G_DECLARE_FINAL_TYPE (NautilusCache, nautilus_cache, NAUTILUS, CACHE, GObject)

/* GCopyFunc has too many parameters for our taste. */
typedef gpointer (*NautilusCopyFunc) (gpointer data);
#define NAUTILUS_COPY_FUNC(x) ((NautilusCopyFunc) x)

typedef enum
{
    NAUTILUS_CACHE_INVALID,
    NAUTILUS_CACHE_PENDING,
    NAUTILUS_CACHE_VALID
} NautilusCacheState;

void nautilus_cache_invalidate_all (NautilusCache *cache);

NautilusCacheState nautilus_cache_item_get_state (NautilusCache *cache,
                                                  gssize         item);

void nautilus_cache_item_invalidate  (NautilusCache      *cache,
                                      gssize              item,
                                      gboolean            destroy);
void nautilus_cache_item_set_pending (NautilusCache      *cache,
                                      gssize              item);

gpointer nautilus_cache_item_get_value (NautilusCache    *cache,
                                        gssize            item,
                                        NautilusCopyFunc  copy_func);
void     nautilus_cache_item_set_value (NautilusCache    *cache,
                                        gssize            item,
                                        gpointer          value);

/* TODO: TTL for highly volatile items? */
gssize nautilus_cache_install_item (NautilusCache  *cache,
                                    GDestroyNotify  destroy_func);

NautilusCache *nautilus_cache_new (void);
