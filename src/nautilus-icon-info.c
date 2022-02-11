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
    gint64 last_use_time;
    GdkPixbuf *pixbuf;

    char *icon_name;

    gint orig_scale;
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
    return icon->pixbuf == NULL;
}

static void
pixbuf_toggle_notify (gpointer  info,
                      GObject  *object,
                      gboolean  is_last_ref)
{
    NautilusIconInfo *icon = info;

    if (is_last_ref)
    {
        icon->sole_owner = TRUE;
        g_object_remove_toggle_ref (object,
                                    pixbuf_toggle_notify,
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

    if (!icon->sole_owner && icon->pixbuf)
    {
        g_object_remove_toggle_ref (G_OBJECT (icon->pixbuf),
                                    pixbuf_toggle_notify,
                                    icon);
    }

    if (icon->pixbuf)
    {
        g_object_unref (icon->pixbuf);
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
nautilus_icon_info_new_for_pixbuf (GdkPixbuf *pixbuf,
                                   gint       scale)
{
    NautilusIconInfo *icon;

    icon = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);

    if (pixbuf)
    {
        icon->pixbuf = g_object_ref (pixbuf);
    }

    icon->orig_scale = scale;

    return icon;
}

static NautilusIconInfo *
nautilus_icon_info_new_for_icon_info (GtkIconInfo *icon_info,
                                      gint         scale)
{
    NautilusIconInfo *icon;
    const char *filename;
    char *basename, *p;

    icon = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);

    icon->pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

    filename = gtk_icon_info_get_filename (icon_info);
    if (filename != NULL)
    {
        basename = g_path_get_basename (filename);
        p = strrchr (basename, '.');
        if (p)
        {
            *p = 0;
        }
        icon->icon_name = basename;
    }

    icon->orig_scale = scale;

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
    char *filename;
    int scale;
    int size;
} ThemedIconKey;

static GHashTable *loadable_icon_cache = NULL;
static GHashTable *themed_icon_cache = NULL;
static guint reap_cache_timeout = 0;

#define MICROSEC_PER_SEC ((guint64) 1000000L)

static guint time_now;

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
    return g_str_hash (key->filename) ^ key->size;
}

static gboolean
themed_icon_key_equal (const ThemedIconKey *a,
                       const ThemedIconKey *b)
{
    return a->size == b->size &&
           a->scale == b->scale &&
           g_str_equal (a->filename, b->filename);
}

static ThemedIconKey *
themed_icon_key_new (const char *filename,
                     int         scale,
                     int         size)
{
    ThemedIconKey *key;

    key = g_slice_new (ThemedIconKey);
    key->filename = g_strdup (filename);
    key->scale = scale;
    key->size = size;

    return key;
}

static void
themed_icon_key_free (ThemedIconKey *key)
{
    g_free (key->filename);
    g_slice_free (ThemedIconKey, key);
}

NautilusIconInfo *
nautilus_icon_info_lookup (GIcon *icon,
                           int    size,
                           int    scale)
{
    NautilusIconInfo *icon_info;
    g_autoptr (GtkIconInfo) gtkicon_info = NULL;

    if (G_IS_LOADABLE_ICON (icon))
    {
        GdkPixbuf *pixbuf;
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

        pixbuf = NULL;
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

        icon_info = nautilus_icon_info_new_for_pixbuf (pixbuf, scale);

        key = loadable_icon_key_new (icon, scale, size);
        g_hash_table_insert (loadable_icon_cache, key, icon_info);

        return g_object_ref (icon_info);
    }

    gtkicon_info = gtk_icon_theme_lookup_by_gicon_for_scale (gtk_icon_theme_get_default (),
                                                             icon, size, scale, 0);
    if (gtkicon_info == NULL)
    {
        return nautilus_icon_info_new_for_pixbuf (NULL, scale);
    }

    if (G_IS_THEMED_ICON (icon))
    {
        ThemedIconKey lookup_key;
        ThemedIconKey *key;
        const char *filename;

        if (themed_icon_cache == NULL)
        {
            themed_icon_cache =
                g_hash_table_new_full ((GHashFunc) themed_icon_key_hash,
                                       (GEqualFunc) themed_icon_key_equal,
                                       (GDestroyNotify) themed_icon_key_free,
                                       (GDestroyNotify) g_object_unref);
        }

        filename = gtk_icon_info_get_filename (gtkicon_info);
        if (filename == NULL)
        {
            g_object_unref (gtkicon_info);
            return nautilus_icon_info_new_for_pixbuf (NULL, scale);
        }

        lookup_key.filename = (char *) filename;
        lookup_key.scale = scale;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (themed_icon_cache, &lookup_key);
        if (!icon_info)
        {
            icon_info = nautilus_icon_info_new_for_icon_info (gtkicon_info, scale);

            key = themed_icon_key_new (filename, scale, size);
            g_hash_table_insert (themed_icon_cache, key, icon_info);
        }

        return g_object_ref (icon_info);
    }
    else
    {
        g_autoptr (GdkPixbuf) pixbuf = NULL;

        pixbuf = gtk_icon_info_load_icon (gtkicon_info, NULL);
        return nautilus_icon_info_new_for_pixbuf (pixbuf, scale);
    }
}

static GdkPixbuf *
nautilus_icon_info_get_pixbuf_nodefault (NautilusIconInfo *icon)
{
    GdkPixbuf *res;

    if (icon->pixbuf == NULL)
    {
        res = NULL;
    }
    else
    {
        res = g_object_ref (icon->pixbuf);

        if (icon->sole_owner)
        {
            icon->sole_owner = FALSE;
            g_object_add_toggle_ref (G_OBJECT (res),
                                     pixbuf_toggle_notify,
                                     icon);
        }
    }

    return res;
}


GdkPixbuf *
nautilus_icon_info_get_pixbuf (NautilusIconInfo *icon)
{
    GdkPixbuf *res;

    res = nautilus_icon_info_get_pixbuf_nodefault (icon);
    if (res == NULL)
    {
        res = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/text-x-preview.png",
                                            NULL);
    }

    return res;
}

GdkPixbuf *
nautilus_icon_info_get_pixbuf_at_size (NautilusIconInfo *icon,
                                       gsize             forced_size)
{
    GdkPixbuf *pixbuf, *scaled_pixbuf;
    int w, h, s;
    double scale;

    pixbuf = nautilus_icon_info_get_pixbuf (icon);

    w = gdk_pixbuf_get_width (pixbuf) / icon->orig_scale;
    h = gdk_pixbuf_get_height (pixbuf) / icon->orig_scale;
    s = MAX (w, h);
    if (s == forced_size)
    {
        return pixbuf;
    }

    scale = (double) forced_size / s;

    /* Neither of these can be 0. */
    w = MAX (w * scale, 1);
    h = MAX (h * scale, 1);

    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                             w, h,
                                             GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    return scaled_pixbuf;
}

const char *
nautilus_icon_info_get_used_name (NautilusIconInfo *icon)
{
    return icon->icon_name;
}
