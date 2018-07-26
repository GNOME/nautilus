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

    GdkPixbuf *pixbuf;

    gboolean sole_owner;
    gint64 last_use_time;

    char *icon_name;

    int scale_factor;
};

static void schedule_reap_cache (void);

G_DEFINE_TYPE (NautilusIconInfo, nautilus_icon_info, G_TYPE_OBJECT);

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
pixbuf_toggle_notify (gpointer  data,
                       GObject  *object,
                       gboolean  is_last_ref)
{
    NautilusIconInfo *self;

    self = data;

    if (!is_last_ref)
    {
        return;
    }

    self->sole_owner = TRUE;

    g_object_remove_toggle_ref (object, pixbuf_toggle_notify, self);

    self->last_use_time = g_get_monotonic_time ();

    schedule_reap_cache ();
}

static void
nautilus_icon_info_finalize (GObject *object)
{
    NautilusIconInfo *self;

    self = NAUTILUS_ICON_INFO (object);

    if (!self->sole_owner && self->pixbuf != NULL)
    {
        g_object_remove_toggle_ref (G_OBJECT (self->pixbuf), pixbuf_toggle_notify, self);
    }

    g_clear_object (&self->pixbuf);
    g_clear_pointer (&self->icon_name, g_free);

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
                                   int        scale_factor)
{
    NautilusIconInfo *info;

    g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), NULL);

    info = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);

    g_set_object (&info->pixbuf, pixbuf);

    info->scale_factor = scale_factor;

    return info;
}

static NautilusIconInfo *
nautilus_icon_info_new_for_icon_info (GtkIconInfo *icon_info,
                                      int          scale_factor)
{
    NautilusIconInfo *info;
    GdkPixbuf *pixbuf;
    const char *filename;

    info = g_object_new (NAUTILUS_TYPE_ICON_INFO, NULL);
    pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
    filename = gtk_icon_info_get_filename (icon_info);
    if (filename != NULL)
    {
        char *basename;
        char *extension;

        basename = g_path_get_basename (filename);
        extension = strrchr (basename, '.');
        if (extension != NULL)
        {
            *extension = '\0';
        }

        info->icon_name = basename;
    }

    info->pixbuf = pixbuf;
    info->scale_factor = scale_factor;

    return info;
}

typedef struct
{
    GLoadableIcon *icon;
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
        if (time_now - icon->last_use_time > 30 * G_TIME_SPAN_SECOND)
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

static unsigned int
loadable_icon_key_hash (gconstpointer key)
{
    const LoadableIconKey *loadable_icon_key;
    unsigned int hash;

    loadable_icon_key = key;
    hash = g_icon_hash (loadable_icon_key->icon);

    return hash ^ loadable_icon_key->scale ^ loadable_icon_key->size;
}

static gboolean
loadable_icon_key_equal (gconstpointer a,
                         gconstpointer b)
{
    const LoadableIconKey *lhs;
    const LoadableIconKey *rhs;

    lhs = a;
    rhs = b;

    return lhs->size == rhs->size &&
           lhs->scale == rhs->scale &&
           g_icon_equal (G_ICON (lhs->icon), G_ICON (rhs->icon));
}

static LoadableIconKey *
loadable_icon_key_new (GLoadableIcon *icon,
                       int            scale,
                       int            size)
{
    LoadableIconKey *key;

    key = g_slice_new (LoadableIconKey);
    key->icon = g_object_ref (icon);
    key->scale = scale;
    key->size = size;

    return key;
}

static void
loadable_icon_key_free (gpointer data)
{
    LoadableIconKey *key;

    key = data;

    g_object_unref (key->icon);
    g_slice_free (LoadableIconKey, key);
}

static unsigned int
themed_icon_key_hash (gconstpointer key)
{
    const ThemedIconKey *themed_icon_key;

    themed_icon_key = key;

    return g_str_hash (themed_icon_key->filename) ^ themed_icon_key->size;
}

static gboolean
themed_icon_key_equal (gconstpointer a,
                       gconstpointer b)
{
    const ThemedIconKey *lhs;
    const ThemedIconKey *rhs;

    lhs = a;
    rhs = b;

    return lhs->size == rhs->size &&
           lhs->scale == rhs->scale &&
           g_str_equal (lhs->filename, rhs->filename);
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
themed_icon_key_free (gpointer data)
{
    ThemedIconKey *key;

    key = data;

    g_free (key->filename);
    g_slice_free (ThemedIconKey, key);
}

static NautilusIconInfo *
nautilus_icon_info_look_up_loadable_icon (GLoadableIcon *icon,
                                          int            size,
                                          int            scale)
{
    LoadableIconKey lookup_key;
    g_autoptr (GInputStream) stream = NULL;
    GdkPixbuf *pixbuf;
    LoadableIconKey *key;
    NautilusIconInfo *info;

    if (loadable_icon_cache == NULL)
    {
        loadable_icon_cache = g_hash_table_new_full (loadable_icon_key_hash,
                                                     loadable_icon_key_equal,
                                                     loadable_icon_key_free,
                                                     g_object_unref);
    }

    lookup_key.icon = icon;
    lookup_key.scale = scale;
    lookup_key.size = size * scale;

    info = g_hash_table_lookup (loadable_icon_cache, &lookup_key);
    if (info != NULL)
    {
        return g_object_ref (info);
    }
    stream = g_loadable_icon_load (icon, size * scale, NULL, NULL, NULL);
    if (stream != NULL)
    {
        pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                      size * scale, size * scale,
                                                      TRUE,
                                                      NULL, NULL);
    }
    else
    {
        pixbuf = NULL;
    }
    key = loadable_icon_key_new (icon, scale, size);
    info = nautilus_icon_info_new_for_pixbuf (pixbuf, scale);

    g_hash_table_insert (loadable_icon_cache, key, info);

    return g_object_ref (info);
}

static NautilusIconInfo *
nautilus_icon_info_look_up_themed_icon (GThemedIcon *icon,
                                        int          size,
                                        int          scale)
{
    GtkIconTheme *icon_theme;
    const char * const *names;
    g_autoptr (GtkIconInfo) gtkicon_info = NULL;
    const char *filename;
    ThemedIconKey lookup_key;
    ThemedIconKey *key;
    NautilusIconInfo *info;

    if (themed_icon_cache == NULL)
    {
        themed_icon_cache = g_hash_table_new_full (themed_icon_key_hash,
                                                   themed_icon_key_equal,
                                                   themed_icon_key_free,
                                                   g_object_unref);
    }

    icon_theme = gtk_icon_theme_get_default ();
    names = g_themed_icon_get_names (icon);
    gtkicon_info = gtk_icon_theme_choose_icon_for_scale (icon_theme, (const char **) names,
                                                         size, scale,
                                                         GTK_ICON_LOOKUP_FORCE_SIZE);
    if (gtkicon_info == NULL)
    {
        return nautilus_icon_info_new_for_pixbuf (NULL, scale);
    }
    filename = gtk_icon_info_get_filename (gtkicon_info);
    if (filename == NULL)
    {
        return nautilus_icon_info_new_for_pixbuf (NULL, scale);
    }

    lookup_key.filename = (char *) filename;
    lookup_key.scale = scale;
    lookup_key.size = size;

    info = g_hash_table_lookup (themed_icon_cache, &lookup_key);
    if (info != NULL)
    {
        return g_object_ref (info);
    }
    key = themed_icon_key_new (filename, scale, size);
    info = nautilus_icon_info_new_for_icon_info (gtkicon_info, scale);

    g_hash_table_insert (themed_icon_cache, key, info);

    return g_object_ref (info);
}

NautilusIconInfo *
nautilus_icon_info_lookup (GIcon *icon,
                           int    size,
                           int    scale)
{
    if (G_IS_LOADABLE_ICON (icon))
    {
        return nautilus_icon_info_look_up_loadable_icon (G_LOADABLE_ICON (icon), size, scale);
    }
    else if (G_IS_THEMED_ICON (icon))
    {
        return nautilus_icon_info_look_up_themed_icon (G_THEMED_ICON (icon), size, scale);
    }
    else
    {
        g_autoptr (GtkIconInfo) gtk_icon_info = NULL;
        g_autoptr (GdkPixbuf) pixbuf = NULL;

        gtk_icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (gtk_icon_theme_get_default (),
                                                                  icon,
                                                                  size,
                                                                  scale,
                                                                  GTK_ICON_LOOKUP_FORCE_SIZE);
        if (gtk_icon_info != NULL)
        {
            pixbuf = gtk_icon_info_load_icon (gtk_icon_info, NULL);
        }

        return nautilus_icon_info_new_for_pixbuf (pixbuf, scale);
    }
}

NautilusIconInfo *
nautilus_icon_info_lookup_from_name (const char *name,
                                     int         size,
                                     int         scale)
{
    g_autoptr (GIcon) icon = NULL;

    icon = g_themed_icon_new (name);

    return nautilus_icon_info_lookup (icon, size, scale);
}

static GdkPixbuf *
nautilus_icon_info_get_pixbuf_nodefault (NautilusIconInfo *self)
{
    GdkPixbuf *pixbuf;

    if (self->pixbuf == NULL)
    {
        return NULL;
    }

    pixbuf = g_object_ref (self->pixbuf);

    if (self->sole_owner)
    {
        self->sole_owner = FALSE;

        g_object_add_toggle_ref (G_OBJECT (pixbuf), pixbuf_toggle_notify, self);
    }

    return pixbuf;
}

GdkPixbuf *
nautilus_icon_info_get_pixbuf (NautilusIconInfo *self,
                               gboolean          fallback,
                               int               size)
{
    GdkPixbuf *pixbuf;

    pixbuf = nautilus_icon_info_get_pixbuf_nodefault (self);
    if (pixbuf == NULL)
    {
        if (!fallback)
        {
            return pixbuf;
        }

        pixbuf = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/text-x-preview.png",
                                               NULL);
    }

    g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

    if (size != -1)
    {
        int width;
        int height;
        int max;
        double scale_factor;

        width = gdk_pixbuf_get_width (pixbuf) / self->scale_factor;
        height = gdk_pixbuf_get_height (pixbuf) / self->scale_factor;
        max = MAX (width, height);
        if (max == size)
        {
            return pixbuf;
        }
        scale_factor = (double) size / (double) max;

        g_object_unref (pixbuf);

        width = MAX (width * scale_factor, 1);
        height = MAX (height * scale_factor, 1);

        pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                          width, height,
                                          GDK_INTERP_BILINEAR);
    }

    return pixbuf;
}

GdkTexture *
nautilus_icon_info_get_texture (NautilusIconInfo *self,
                                gboolean          fallback,
                                int               size)
{
    g_autoptr (GdkPixbuf) pixbuf = NULL;

    pixbuf = nautilus_icon_info_get_pixbuf (self, fallback, size);

    return gdk_texture_new_for_pixbuf (pixbuf);
}

const char *
nautilus_icon_info_get_used_name (NautilusIconInfo *self)
{
    g_return_val_if_fail (NAUTILUS_IS_ICON_INFO (self), NULL);

    return self->icon_name;
}
