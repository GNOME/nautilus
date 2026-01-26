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

#include <glycin.h>
#include <glycin-gtk4.h>

struct _NautilusIconInfo
{
    GObject parent;

    gboolean only_in_cache;
    guint64 last_use_time;
    GdkPaintable *paintable;

    char *icon_name;
};

static GtkIconPaintable *
lookup_themed_icon (GIcon *icon,
                    int    size,
                    float  scale);
static void schedule_reap_cache (void);

G_DEFINE_TYPE (NautilusIconInfo,
               nautilus_icon_info,
               G_TYPE_OBJECT);

static void
nautilus_icon_info_init (NautilusIconInfo *icon)
{
    icon->last_use_time = g_get_monotonic_time ();
    icon->only_in_cache = TRUE;
}

static void
paintable_toggle_notify (gpointer  info,
                         GObject  *object,
                         gboolean  is_last_ref)
{
    NautilusIconInfo *icon = info;

    icon->only_in_cache = is_last_ref;

    if (is_last_ref)
    {
        icon->last_use_time = g_get_monotonic_time ();
        schedule_reap_cache ();
    }
}

static void
nautilus_icon_info_finalize (GObject *object)
{
    NautilusIconInfo *icon;

    icon = NAUTILUS_ICON_INFO (object);

    if (icon->only_in_cache)
    {
        /* Cleaning up the last reference from the cache */
        g_object_remove_toggle_ref (G_OBJECT (icon->paintable),
                                    paintable_toggle_notify,
                                    icon);
    }
    else
    {
        g_object_unref (icon->paintable);
    }
    g_free (icon->icon_name);

    G_OBJECT_CLASS (nautilus_icon_info_parent_class)->finalize (object);
}

static void
nautilus_icon_info_class_init (NautilusIconInfoClass *icon_info_class)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass *) icon_info_class;

    gobject_class->finalize = nautilus_icon_info_finalize;
}

NautilusIconInfo *
nautilus_icon_info_new_for_paintable (GdkPaintable *paintable)
{
    g_return_val_if_fail (paintable != NULL, NULL);

    NautilusIconInfo *self = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);

    self->paintable = paintable;
    g_object_add_toggle_ref (G_OBJECT (self->paintable),
                             paintable_toggle_notify,
                             self);

    return self;
}

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

static NautilusIconInfo *
nautilus_icon_info_get_fallback (int size,
                                 int scale)
{
    static NautilusIconInfo *fallback_info = NULL;

    if (G_UNLIKELY (fallback_info == NULL))
    {
        GIcon *icon = nautilus_icon_info_get_default_file_icon ();
        g_autoptr (GtkIconPaintable) paintable = lookup_themed_icon (icon, size, scale);

        g_assert (paintable != NULL);

        fallback_info = nautilus_icon_info_new_for_paintable (GDK_PAINTABLE (paintable));
    }

    return g_object_ref (fallback_info);
}

static NautilusIconInfo *
nautilus_icon_info_new_for_icon_paintable (GtkIconPaintable *icon_paintable)
{
    NautilusIconInfo *icon;
    g_autoptr (GFile) file = NULL;
    char *basename, *p;

    icon = nautilus_icon_info_new_for_paintable (GDK_PAINTABLE (icon_paintable));

    file = gtk_icon_paintable_get_file (icon_paintable);
    if (file != NULL)
    {
        basename = g_file_get_basename (file);
        p = strrchr (basename, '.');
        if (p)
        {
            *p = 0;
        }
        icon->icon_name = basename;
    }
    else
    {
        icon->icon_name = g_strdup (gtk_icon_paintable_get_icon_name (icon_paintable));
    }

    return icon;
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

static GHashTable *loadable_icon_cache = NULL;
static GHashTable *themed_icon_cache = NULL;
static guint reap_cache_timeout = 0;

static guint64 time_now;

static gboolean
reap_old_icon (gpointer key,
               gpointer value,
               gpointer user_info)
{
    NautilusIconInfo *icon = value;
    gboolean *reapable_icons_left = user_info;

    if (icon->only_in_cache)
    {
        if (time_now - icon->last_use_time > 30 * G_USEC_PER_SEC)
        {
            /* This went unused 30 secs ago. reap */
            return TRUE;
        }
        else
        {
            /* We can reap this soon */
            *reapable_icons_left = TRUE;
        }
    }

    return FALSE;
}

static gboolean
reap_cache (gpointer data)
{
    gboolean reapable_icons_left = FALSE;

    time_now = g_get_monotonic_time ();

    if (loadable_icon_cache)
    {
        g_hash_table_foreach_remove (loadable_icon_cache,
                                     reap_old_icon,
                                     &reapable_icons_left);
    }

    if (themed_icon_cache)
    {
        g_hash_table_foreach_remove (themed_icon_cache,
                                     reap_old_icon,
                                     &reapable_icons_left);
    }

    if (reapable_icons_left)
    {
        return TRUE;
    }
    else
    {
        reap_cache_timeout = 0;
        return FALSE;
    }
}

static void
schedule_reap_cache (void)
{
    if (reap_cache_timeout == 0)
    {
        reap_cache_timeout = g_timeout_add_seconds_full (0, 5,
                                                         reap_cache,
                                                         NULL, NULL);
    }
}

void
nautilus_icon_info_clear_caches (void)
{
    if (loadable_icon_cache)
    {
        g_hash_table_remove_all (loadable_icon_cache);
    }

    if (themed_icon_cache)
    {
        g_hash_table_remove_all (themed_icon_cache);
    }
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

NautilusIconInfo *
nautilus_icon_info_lookup (GIcon *icon,
                           int    size,
                           int    scale)
{
    NautilusIconInfo *icon_info;

    if (G_IS_LOADABLE_ICON (icon))
    {
        g_autoptr (GdkPaintable) paintable = NULL;
        LoadableIconKey lookup_key;
        LoadableIconKey *key;

        if (G_UNLIKELY (loadable_icon_cache == NULL))
        {
            loadable_icon_cache =
                g_hash_table_new_full ((GHashFunc) loadable_icon_key_hash,
                                       (GEqualFunc) loadable_icon_key_equal,
                                       (GDestroyNotify) loadable_icon_key_free,
                                       (GDestroyNotify) g_object_unref);
        }

        lookup_key.icon = icon;
        lookup_key.scale = scale;
        lookup_key.size = size * scale;

        icon_info = g_hash_table_lookup (loadable_icon_cache, &lookup_key);
        if (icon_info)
        {
            return g_object_ref (icon_info);
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
                g_autoptr (GlyFrame) frame = gly_image_next_frame (image, NULL);

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

        icon_info = nautilus_icon_info_new_for_paintable (paintable);

        key = loadable_icon_key_new (icon, scale, size);
        g_hash_table_insert (loadable_icon_cache, key, icon_info);

        return g_object_ref (icon_info);
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

        if (G_UNLIKELY (themed_icon_cache == NULL))
        {
            themed_icon_cache =
                g_hash_table_new_full ((GHashFunc) themed_icon_key_hash,
                                       (GEqualFunc) themed_icon_key_equal,
                                       (GDestroyNotify) themed_icon_key_free,
                                       (GDestroyNotify) g_object_unref);
        }

        icon_name = gtk_icon_paintable_get_icon_name (icon_paintable);

        lookup_key.icon_name = (char *) icon_name;
        lookup_key.scale = scale;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (themed_icon_cache, &lookup_key);
        if (!icon_info)
        {
            icon_info = nautilus_icon_info_new_for_icon_paintable (icon_paintable);

            key = themed_icon_key_new (icon_name, scale, size);
            g_hash_table_insert (themed_icon_cache, key, icon_info);
        }

        return g_object_ref (icon_info);
    }
    else
    {
        /* Only GLoadableIcon and GThemedIcon are known and supported */
        g_assert_not_reached ();
    }
}

GdkPaintable *
nautilus_icon_info_get_paintable (NautilusIconInfo *icon)
{
    return g_object_ref (icon->paintable);
}

const char *
nautilus_icon_info_get_used_name (NautilusIconInfo *icon)
{
    return icon->icon_name;
}
