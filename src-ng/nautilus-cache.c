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

#include "nautilus-cache.h"

typedef struct
{
    gpointer value;
    GDestroyNotify destroy_func;

    NautilusCacheState state;

    GMutex mutex;
} CacheItemDetails;

struct _NautilusCache
{
    GObject parent_instance;

    GHashTable *items;
};

G_DEFINE_TYPE (NautilusCache, nautilus_cache, G_TYPE_OBJECT)

static CacheItemDetails *
cache_item_details_new (GDestroyNotify destroy_func)
{
    CacheItemDetails *details;

    details = g_new0 (CacheItemDetails, 1);

    details->destroy_func = destroy_func;

    g_mutex_init (&details->mutex);

    return details;
}

static void
cache_item_details_destroy (CacheItemDetails *details)
{
    if (details->destroy_func != NULL)
    {
        details->destroy_func (details->value);
    }
    g_mutex_clear (&details->mutex);
    g_free (details);
}

static void
finalize (GObject *object)
{
    NautilusCache *self;

    self = NAUTILUS_CACHE (object);

    g_hash_table_destroy (self->items);

    G_OBJECT_CLASS (nautilus_cache_parent_class)->finalize (object);
}

static void
nautilus_cache_class_init (NautilusCacheClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;
}

static void
nautilus_cache_init (NautilusCache *self)
{
    self->items =
        g_hash_table_new_full (g_direct_hash, g_direct_equal,
                               NULL,
                               (GDestroyNotify) cache_item_details_destroy);
}

NautilusCacheState
nautilus_cache_item_get_state (NautilusCache *cache,
                               gssize         item)
{
    CacheItemDetails *details;

    g_return_val_if_fail (NAUTILUS_IS_CACHE (cache), NAUTILUS_CACHE_INVALID);
    g_return_val_if_fail (g_hash_table_contains (cache->items,
                                                 GINT_TO_POINTER (item)),
                          NAUTILUS_CACHE_INVALID);

    details = g_hash_table_lookup (cache->items, GINT_TO_POINTER (item));

    return details->state;
}

void
nautilus_cache_item_invalidate  (NautilusCache      *cache,
                                 gssize              item,
                                 gboolean            destroy)
{
    CacheItemDetails *details;

    g_return_if_fail (NAUTILUS_IS_CACHE (cache));
    g_return_if_fail (g_hash_table_contains (cache->items,
                                             GINT_TO_POINTER (item)));

    details = g_hash_table_lookup (cache->items, GINT_TO_POINTER (item));

    g_mutex_lock (&details->mutex);

    /* There might be cases where the cache holding a ref on an object is a
     * problem.
     */
    if (destroy &&
        details->destroy_func != NULL && G_LIKELY (details->value != NULL))
    {
        details->destroy_func (details->value);
    }

    details->state = NAUTILUS_CACHE_INVALID;

    g_mutex_unlock (&details->mutex);
}

void
nautilus_cache_item_set_pending (NautilusCache *cache,
                                 gssize         item)
{
    CacheItemDetails *details;

    g_return_if_fail (NAUTILUS_IS_CACHE (cache));
    g_return_if_fail (g_hash_table_contains (cache->items,
                                             GINT_TO_POINTER (item)));

    details = g_hash_table_lookup (cache->items, GINT_TO_POINTER (item));

    g_mutex_lock (&details->mutex);

    /* This whole method feels quite clunky.
     * Changing the state from valid to pending should probably be treated
     * as programmer error (or a bug).
     */
    g_warn_if_fail (details->state == NAUTILUS_CACHE_INVALID);
    if (details->state != NAUTILUS_CACHE_INVALID)
    {
        g_mutex_unlock (&details->mutex);
        return;
    }

    details->state = NAUTILUS_CACHE_PENDING;

    g_mutex_unlock (&details->mutex);
}

gpointer
nautilus_cache_item_get_value (NautilusCache    *cache,
                               gssize            item,
                               NautilusCopyFunc  copy_func)
{
    CacheItemDetails *details;

    g_return_val_if_fail (NAUTILUS_IS_CACHE (cache), NULL);
    g_return_val_if_fail (g_hash_table_contains (cache->items,
                                                 GINT_TO_POINTER (item)),
                          NULL);

    details = g_hash_table_lookup (cache->items, GINT_TO_POINTER (item));

    if (copy_func == NULL)
    {
        return details->value;
    }
    else
    {
        gpointer ret;

        g_mutex_lock (&details->mutex);

        ret = copy_func (details->value);

        g_mutex_unlock (&details->mutex);

        return ret;
    }
}

void
nautilus_cache_item_set_value (NautilusCache *cache,
                               gssize         item,
                               gpointer       value)
{
    CacheItemDetails *details;

    g_return_if_fail (NAUTILUS_IS_CACHE (cache));
    g_return_if_fail (g_hash_table_contains (cache->items,
                                             GINT_TO_POINTER (item)));

    details = g_hash_table_lookup (cache->items, GINT_TO_POINTER (item));

    g_mutex_lock (&details->mutex);

    /* We’ll treat this as a cancellation of the update. */
    if (details->state != NAUTILUS_CACHE_PENDING)
    {
        g_mutex_unlock (&details->mutex);

        return;
    }

    if (details->destroy_func != NULL && G_LIKELY (details->value != NULL))
    {
        g_clear_pointer (&details->value, details->destroy_func);
    }

    details->value = value;
    details->state = NAUTILUS_CACHE_VALID;

    g_mutex_unlock (&details->mutex);
}

gssize
nautilus_cache_install_item (NautilusCache    *cache,
                             GDestroyNotify    destroy_func)
{
    CacheItemDetails *details;
    guint size;

    g_return_val_if_fail (NAUTILUS_IS_CACHE (cache), -1);

    details = cache_item_details_new (destroy_func);
    size = g_hash_table_size (cache->items);

    g_hash_table_insert (cache->items, GUINT_TO_POINTER (size), details);

    /* This is a bit evil, but is unlikely to cause problems… ever. */
    return (gssize) size;
}

NautilusCache *
nautilus_cache_new (void)
{
    return g_object_new (NAUTILUS_TYPE_CACHE, NULL);
}
