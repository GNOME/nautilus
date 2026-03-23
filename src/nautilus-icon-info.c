/* nautilus-icon-info.c
 * Copyright (C) 2007  Red Hat, Inc.,  Alexander Larsson <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-icon-info.h"

#include "nautilus-enums.h"
#include <nautilus-hash-queue.h>

#include <glycin.h>
#include <glycin-gtk4.h>

static GtkIconPaintable *
lookup_themed_icon (GIcon *icon,
                    int    size,
                    float  scale);

GIcon *
nautilus_icon_info_get_default_file_icon (void)
{
    static GIcon *fallback_icon = NULL;

    if (G_UNLIKELY (fallback_icon == NULL))
    {
        char *icon_names[3] = {"application-x-generic", "text-x-generic", NULL};

        fallback_icon = g_themed_icon_new_from_names (icon_names, -1);
    }

    return fallback_icon;
}

static GdkPaintable *
nautilus_icon_info_get_fallback (int size,
                                 int scale)
{
    static GdkPaintable *fallback_paintable = NULL;

    if (G_UNLIKELY (fallback_paintable == NULL))
    {
        GIcon *icon = nautilus_icon_info_get_default_file_icon ();

        fallback_paintable = GDK_PAINTABLE (lookup_themed_icon (icon, size, scale));
    }

    return g_object_ref (fallback_paintable);
}

typedef struct
{
    GIcon *icon;
    int scale;
    int size;
} LoadableIconKey;

typedef struct
{
    char *icon_name;
    int scale;
    int size;
} ThemedIconKey;

static NautilusHashQueue *loadable_icon_cache = NULL;
static NautilusHashQueue *themed_icon_cache = NULL;

#define LOADABLE_ICON_CACHE_COUNT_LIMIT 100
#define THEMED_ICON_CACHE_COUNT_LIMIT 200

void
nautilus_icon_info_clear_caches (void)
{
    g_clear_pointer (&loadable_icon_cache, nautilus_hash_queue_destroy);
    g_clear_pointer (&themed_icon_cache, nautilus_hash_queue_destroy);
}

static guint
int_hash (int v)
{
    return g_direct_hash (GINT_TO_POINTER (v));
}

static guint
loadable_icon_key_hash (LoadableIconKey *key)
{
    return g_icon_hash (key->icon) ^ int_hash (key->scale) ^ int_hash (key->size);
}

static gboolean
loadable_icon_key_equal (const LoadableIconKey *a,
                         const LoadableIconKey *b)
{
    return a->size == b->size &&
           a->scale == b->scale &&
           g_icon_equal (a->icon, b->icon);
}

static LoadableIconKey *
loadable_icon_key_new (GIcon *icon,
                       int    scale,
                       int    size)
{
    LoadableIconKey *key;

    key = g_slice_new (LoadableIconKey);
    key->icon = g_object_ref (icon);
    key->scale = scale;
    key->size = size;

    return key;
}

static void
loadable_icon_key_free (LoadableIconKey *key)
{
    g_object_unref (key->icon);
    g_slice_free (LoadableIconKey, key);
}

static GdkPaintable *
loadable_icon_cache_get (LoadableIconKey *key)
{
    if (loadable_icon_cache == NULL)
    {
        return NULL;
    }

    GdkPaintable *paintable = nautilus_hash_queue_find_item (loadable_icon_cache, key);

    if (paintable != NULL)
    {
        nautilus_hash_queue_move_existing_to_tail (loadable_icon_cache, key);
    }

    return paintable;
}

static void
loadable_icon_cache_add (LoadableIconKey *key,
                         GdkPaintable    *paintable)
{
    if (G_UNLIKELY (loadable_icon_cache == NULL))
    {
        loadable_icon_cache = nautilus_hash_queue_new ((GHashFunc) loadable_icon_key_hash,
                                                       (GEqualFunc) loadable_icon_key_equal,
                                                       (GDestroyNotify) loadable_icon_key_free,
                                                       (GDestroyNotify) g_object_unref);
    }

    if (nautilus_hash_queue_reenqueue (loadable_icon_cache, key, paintable) &&
        nautilus_hash_queue_get_length (loadable_icon_cache) > LOADABLE_ICON_CACHE_COUNT_LIMIT)
    {
        nautilus_hash_queue_remove_head (loadable_icon_cache);
    }
}

static guint
themed_icon_key_hash (ThemedIconKey *key)
{
    return g_str_hash (key->icon_name) ^ int_hash (key->scale) ^ int_hash (key->size);
}

static gboolean
themed_icon_key_equal (const ThemedIconKey *a,
                       const ThemedIconKey *b)
{
    return a->size == b->size &&
           a->scale == b->scale &&
           g_str_equal (a->icon_name, b->icon_name);
}

static ThemedIconKey *
themed_icon_key_new (const char *icon_name,
                     int         scale,
                     int         size)
{
    ThemedIconKey *key;

    key = g_slice_new (ThemedIconKey);
    key->icon_name = g_strdup (icon_name);
    key->scale = scale;
    key->size = size;

    return key;
}

static void
themed_icon_key_free (ThemedIconKey *key)
{
    g_free (key->icon_name);
    g_slice_free (ThemedIconKey, key);
}

static GdkPaintable *
themed_icon_cache_get (ThemedIconKey *key)
{
    if (themed_icon_cache == NULL)
    {
        return NULL;
    }

    GdkPaintable *paintable = GDK_PAINTABLE (nautilus_hash_queue_find_item (themed_icon_cache, key));

    if (paintable != NULL)
    {
        nautilus_hash_queue_move_existing_to_tail (themed_icon_cache, key);
    }

    return paintable;
}

static void
themed_icon_cache_add (ThemedIconKey *key,
                       GdkPaintable  *paintable)
{
    if (G_UNLIKELY (themed_icon_cache == NULL))
    {
        themed_icon_cache = nautilus_hash_queue_new ((GHashFunc) themed_icon_key_hash,
                                                     (GEqualFunc) themed_icon_key_equal,
                                                     (GDestroyNotify) themed_icon_key_free,
                                                     (GDestroyNotify) g_object_unref);
    }

    if (nautilus_hash_queue_reenqueue (themed_icon_cache, key, paintable) &&
        nautilus_hash_queue_get_length (themed_icon_cache) > THEMED_ICON_CACHE_COUNT_LIMIT)
    {
        nautilus_hash_queue_remove_head (themed_icon_cache);
    }
}

static GtkIconPaintable *
lookup_themed_icon (GIcon *icon,
                    int    size,
                    float  scale)
{
    GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

    if (!gtk_icon_theme_has_gicon (theme, icon))
    {
        return NULL;
    }

    const gchar *generic_app_icon_name = "application-x-generic";
    g_autoptr (GtkIconPaintable) icon_paintable = gtk_icon_theme_lookup_by_gicon (theme, icon, size, scale,
                                                                                  GTK_TEXT_DIR_NONE, 0);
    const gchar *icon_name = gtk_icon_paintable_get_icon_name (icon_paintable);

    if (G_IS_THEMED_ICON (icon) &&
        g_strcmp0 (generic_app_icon_name, icon_name) == 0)
    {
        /* GTK prefers generic icons in the main theme over exact match in
         * other themes even when the match is meaningless like the empty file
         * icon 'application-x-generic'. */
        const gchar **names = (const gchar **) g_themed_icon_get_names (G_THEMED_ICON (icon));

        if (g_strcmp0 (generic_app_icon_name, names[0]) != 0 &&
            gtk_icon_theme_has_icon (theme, names[0]))
        {
            return gtk_icon_theme_lookup_icon (theme, names[0], NULL, size, scale,
                                               GTK_TEXT_DIR_NONE, 0);
        }
    }

    return g_steal_pointer (&icon_paintable);
}

GdkPaintable *
nautilus_icon_info_lookup (GIcon *icon,
                           int    size,
                           int    scale)
{
    GdkPaintable *paintable;

    if (G_IS_LOADABLE_ICON (icon))
    {
        LoadableIconKey lookup_key;
        LoadableIconKey *key;

        lookup_key.icon = icon;
        lookup_key.scale = scale;
        lookup_key.size = size;

        paintable = loadable_icon_cache_get (&lookup_key);
        if (paintable != NULL)
        {
            return g_object_ref (paintable);
        }

        g_autoptr (GInputStream) stream = g_loadable_icon_load (G_LOADABLE_ICON (icon),
                                                                size * scale,
                                                                NULL, NULL, NULL);
        if (stream)
        {
            g_autoptr (GlyLoader) loader = gly_loader_new_for_stream (stream);
            g_autoptr (GlyImage) image = gly_loader_load (loader, NULL);

            if (image != NULL)
            {
                g_autoptr (GlyFrameRequest) frame_request = gly_frame_request_new ();
                g_autoptr (GlyFrame) frame = NULL;

                gly_frame_request_set_scale (frame_request, size * scale, size * scale);
                frame = gly_image_get_specific_frame (image, frame_request, NULL);

                if (frame != NULL)
                {
                    double iw = gly_frame_get_width (frame);
                    double ih = gly_frame_get_height (frame);

                    double scale_factor = MIN ((double) size / iw, (double) size / ih);

                    double width = iw * scale_factor;
                    double height = ih * scale_factor;

                    g_autoptr (GdkTexture) texture = gly_gtk_frame_get_texture (frame);
                    g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();

                    gdk_paintable_snapshot (GDK_PAINTABLE (texture),
                                            GDK_SNAPSHOT (snapshot),
                                            width, height);
                    paintable = gtk_snapshot_to_paintable (snapshot, NULL);
                }
            }
        }

        if (paintable == NULL)
        {
            return nautilus_icon_info_get_fallback (size, scale);
        }

        key = loadable_icon_key_new (icon, scale, size);
        loadable_icon_cache_add (key, g_object_ref (paintable));

        return paintable;
    }
    else if (G_IS_THEMED_ICON (icon))
    {
        g_autoptr (GtkIconPaintable) icon_paintable = lookup_themed_icon (icon, size, scale);

        if (icon_paintable == NULL)
        {
            return nautilus_icon_info_get_fallback (size, scale);
        }

        ThemedIconKey lookup_key;
        ThemedIconKey *key;
        const char *icon_name;

        icon_name = gtk_icon_paintable_get_icon_name (icon_paintable);

        lookup_key.icon_name = (char *) icon_name;
        lookup_key.scale = scale;
        lookup_key.size = size;

        paintable = themed_icon_cache_get (&lookup_key);

        if (paintable != NULL)
        {
            return g_object_ref (paintable);
        }

        paintable = GDK_PAINTABLE (g_steal_pointer (&icon_paintable));
        key = themed_icon_key_new (icon_name, scale, size);
        themed_icon_cache_add (key, g_object_ref (paintable));

        return paintable;
    }
    else
    {
        /* Only GLoadableIcon and GThemedIcon are known and supported */
        g_assert_not_reached ();
    }
}
