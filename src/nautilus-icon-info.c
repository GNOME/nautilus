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

struct _NautilusIconInfo
{
    GObject parent;

    gboolean sole_owner;
    guint64 last_use_time;
    GdkPaintable *paintable;

    char *icon_name;
};

static void schedule_reap_cache (void);

G_DEFINE_TYPE (NautilusIconInfo,
               nautilus_icon_info,
               G_TYPE_OBJECT);

static void
nautilus_icon_info_init (NautilusIconInfo *icon)
{
    icon->last_use_time = g_get_monotonic_time ();
    icon->sole_owner = TRUE;
}

gboolean
nautilus_icon_info_is_fallback (NautilusIconInfo *icon)
{
    return icon->paintable == NULL;
}

static void
paintable_toggle_notify (gpointer  info,
                         GObject  *object,
                         gboolean  is_last_ref)
{
    NautilusIconInfo *icon = info;

    if (is_last_ref)
    {
        icon->sole_owner = TRUE;
        g_object_remove_toggle_ref (object,
                                    paintable_toggle_notify,
                                    info);
        icon->last_use_time = g_get_monotonic_time ();
        schedule_reap_cache ();
    }
}

static void
nautilus_icon_info_finalize (GObject *object)
{
    NautilusIconInfo *icon;

    icon = NAUTILUS_ICON_INFO (object);

    if (!icon->sole_owner && icon->paintable)
    {
        g_object_remove_toggle_ref (G_OBJECT (icon->paintable),
                                    paintable_toggle_notify,
                                    icon);
    }

    if (icon->paintable)
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
nautilus_icon_info_new_for_paintable (GdkPaintable *paintable,
                                      gint          scale)
{
    NautilusIconInfo *icon;

    icon = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);

    if (paintable != NULL)
    {
        icon->paintable = g_object_ref (paintable);
    }

    return icon;
}

static NautilusIconInfo *
nautilus_icon_info_new_for_icon_paintable (GtkIconPaintable *icon_paintable,
                                           gint              scale)
{
    NautilusIconInfo *icon;
    g_autoptr (GFile) file = NULL;
    char *basename, *p;

    icon = nautilus_icon_info_new_for_paintable (GDK_PAINTABLE (icon_paintable), scale);

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

#define MICROSEC_PER_SEC ((guint64) 1000000L)

static guint64 time_now;

static gboolean
reap_old_icon (gpointer key,
               gpointer value,
               gpointer user_info)
{
    NautilusIconInfo *icon = value;
    gboolean *reapable_icons_left = user_info;

    if (icon->sole_owner)
    {
        if (time_now - icon->last_use_time > 30 * MICROSEC_PER_SEC)
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
    gboolean reapable_icons_left;

    reapable_icons_left = TRUE;

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
loadable_icon_key_hash (LoadableIconKey *key)
{
    return g_icon_hash (key->icon) ^ key->scale ^ key->size;
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
    return g_str_hash (key->icon_name) ^ key->size;
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

NautilusIconInfo *
nautilus_icon_info_lookup (GIcon *icon,
                           int    size,
                           int    scale)
{
    NautilusIconInfo *icon_info;
    g_autoptr (GtkIconPaintable) icon_paintable = NULL;

    if (G_IS_LOADABLE_ICON (icon))
    {
        g_autoptr (GdkPixbuf) pixbuf = NULL;
        g_autoptr (GdkPaintable) paintable = NULL;
        LoadableIconKey lookup_key;
        LoadableIconKey *key;
        GInputStream *stream;

        if (loadable_icon_cache == NULL)
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

        stream = g_loadable_icon_load (G_LOADABLE_ICON (icon),
                                       size * scale,
                                       NULL, NULL, NULL);
        if (stream)
        {
            pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                          size * scale, size * scale,
                                                          TRUE,
                                                          NULL, NULL);
            g_input_stream_close (stream, NULL, NULL);
            g_object_unref (stream);
        }

        if (pixbuf != NULL)
        {
            double width = gdk_pixbuf_get_width (pixbuf) / scale;
            double height = gdk_pixbuf_get_height (pixbuf) / scale;
            g_autoptr (GdkTexture) texture = gdk_texture_new_for_pixbuf (pixbuf);
            g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();

            gdk_paintable_snapshot (GDK_PAINTABLE (texture),
                                    GDK_SNAPSHOT (snapshot),
                                    width, height);
            paintable = gtk_snapshot_to_paintable (snapshot, NULL);
        }

        icon_info = nautilus_icon_info_new_for_paintable (paintable, scale);

        key = loadable_icon_key_new (icon, scale, size);
        g_hash_table_insert (loadable_icon_cache, key, icon_info);

        return g_object_ref (icon_info);
    }

    GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    if (!gtk_icon_theme_has_gicon (theme, icon))
    {
        return nautilus_icon_info_new_for_paintable (NULL, scale);
    }

    icon_paintable = gtk_icon_theme_lookup_by_gicon (theme, icon, size, scale, GTK_TEXT_DIR_NONE, 0);

    if (G_IS_THEMED_ICON (icon))
    {
        ThemedIconKey lookup_key;
        ThemedIconKey *key;
        const char *icon_name;

        if (themed_icon_cache == NULL)
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
            icon_info = nautilus_icon_info_new_for_icon_paintable (icon_paintable, scale);

            key = themed_icon_key_new (icon_name, scale, size);
            g_hash_table_insert (themed_icon_cache, key, icon_info);
        }

        return g_object_ref (icon_info);
    }
    else
    {
        return nautilus_icon_info_new_for_icon_paintable (icon_paintable, scale);
    }
}

static GdkPaintable *
nautilus_icon_info_get_paintable_nodefault (NautilusIconInfo *icon)
{
    GdkPaintable *res;

    if (icon->paintable == NULL)
    {
        res = NULL;
    }
    else
    {
        res = g_object_ref (icon->paintable);

        if (icon->sole_owner)
        {
            icon->sole_owner = FALSE;
            g_object_add_toggle_ref (G_OBJECT (res),
                                     paintable_toggle_notify,
                                     icon);
        }
    }

    return res;
}

GdkPaintable *
nautilus_icon_info_get_paintable (NautilusIconInfo *icon)
{
    GdkPaintable *res;

    res = nautilus_icon_info_get_paintable_nodefault (icon);
    if (res == NULL)
    {
        res = GDK_PAINTABLE (gdk_texture_new_from_resource ("/org/gnome/nautilus/text-x-preview.png"));
    }

    return res;
}

GdkTexture *
nautilus_icon_info_get_texture (NautilusIconInfo *icon)
{
    g_autoptr (GdkPaintable) paintable = NULL;
    GdkTexture *res;

    paintable = nautilus_icon_info_get_paintable_nodefault (icon);
    if (GDK_IS_TEXTURE (paintable))
    {
        res = GDK_TEXTURE (g_steal_pointer (&paintable));
    }
    else
    {
        res = gdk_texture_new_from_resource ("/org/gnome/nautilus/text-x-preview.png");
    }

    return res;
}

const char *
nautilus_icon_info_get_used_name (NautilusIconInfo *icon)
{
    return icon->icon_name;
}
