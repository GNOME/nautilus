/*
 *  nautilus-file-info.c - Information about a file
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <gio/gio.h>
#include <glib.h>

#include "nautilus-file-info.h"

#include "nautilus-file-generated.h"

struct _NautilusFileInfo {
    GObject parent;

    char *uri;
};

G_DEFINE_TYPE (NautilusFileInfo, nautilus_file_info, G_TYPE_OBJECT)

#include "nautilus-extension-private.h"

static NautilusFileImpl *proxy = NULL;


GList *
nautilus_file_info_list_copy (GList *files)
{
    GList *ret;
    GList *l;

    ret = g_list_copy (files);
    for (l = ret; l != NULL; l = l->next)
    {
        g_object_ref (G_OBJECT (l->data));
    }

    return ret;
}

void
nautilus_file_info_list_free (GList *files)
{
    g_list_free_full (files, g_object_unref);
}

gboolean
nautilus_file_info_is_gone (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    gboolean result;

    nautilus_file_impl_call_is_gone_sync (proxy, self->uri, &result, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return FALSE;
    }

    return result;
}

GFileType
nautilus_file_info_get_file_type (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    GFileType file_type;

    nautilus_file_impl_call_get_file_type_sync (proxy, self->uri, &file_type, NULL, &error);
    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return G_FILE_TYPE_UNKNOWN;
    }

    return file_type;
}

char *
nautilus_file_info_get_name (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    char *name;

    nautilus_file_impl_call_get_name_sync (proxy, self->uri, &name, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return name;
}

GFile *
nautilus_file_info_get_location (NautilusFileInfo *self)
{
    return g_file_new_for_uri (self->uri);
}

char *
nautilus_file_info_get_uri (NautilusFileInfo *self)
{
    return g_strdup (self->uri);
}

char *
nautilus_file_info_get_activation_uri (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    char *activation_uri;
    nautilus_file_impl_call_get_activation_uri_sync (proxy, self->uri, &activation_uri, NULL, &error);
    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return activation_uri;
}

char *
nautilus_file_info_get_parent_uri (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    char *parent_uri;

    nautilus_file_impl_call_get_parent_uri_sync (proxy, self->uri, &parent_uri, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return parent_uri;
}

GFile *
nautilus_file_info_get_parent_location (NautilusFileInfo *self)
{
    g_autofree char *parent_uri = nautilus_file_info_get_parent_uri (self);

    return g_file_new_for_uri (parent_uri);
}

NautilusFileInfo *
nautilus_file_info_get_parent_info (NautilusFileInfo *self)
{
    g_autofree char *parent_uri = nautilus_file_info_get_parent_uri (self);

    return nautilus_file_info_new (parent_uri);
}

GMount *
nautilus_file_info_get_mount (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) location = g_file_new_for_uri (self->uri);
    gboolean has_mount;

    nautilus_file_impl_call_has_mount_sync (proxy, self->uri, &has_mount, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    if (has_mount)
    {
        return g_file_find_enclosing_mount (location, NULL, NULL);
    }

    return NULL;
}

char *
nautilus_file_info_get_uri_scheme (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    char *uri_scheme;

    nautilus_file_impl_call_get_uri_scheme_sync (proxy, self->uri, &uri_scheme, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return uri_scheme;
}

char *
nautilus_file_info_get_mime_type (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    char *mime_type;

    nautilus_file_impl_call_get_mime_type_sync (proxy, self->uri, &mime_type, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return mime_type;
}

gboolean
nautilus_file_info_is_mime_type (NautilusFileInfo *self,
                                 const char       *mime_type)
{
    g_autoptr (GError) error = NULL;
    gboolean result;

    nautilus_file_impl_call_is_mime_type_sync (proxy, self->uri, mime_type, &result, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return FALSE;
    }

    return result;
}

gboolean
nautilus_file_info_is_directory (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    gboolean result;

    nautilus_file_impl_call_is_directory_sync (proxy, self->uri, &result, NULL, &error);
    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return FALSE;
    }

    return result;
}

gboolean
nautilus_file_info_can_write (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;
    gboolean result;

    nautilus_file_impl_call_can_write_sync (proxy, self->uri, &result, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return FALSE;
    }

    return result;
}

void
nautilus_file_info_add_emblem (NautilusFileInfo *self,
                               const char       *emblem_name)
{
    g_autoptr (GError) error = NULL;

    nautilus_file_impl_call_add_emblem_sync (proxy, self->uri, emblem_name, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
    }
}

char *
nautilus_file_info_get_string_attribute (NautilusFileInfo *self,
                                         const char       *attribute_name)
{
    g_autoptr (GError) error = NULL;
    char *string_attribute;

    nautilus_file_impl_call_get_string_attribute_sync (proxy, self->uri, attribute_name, &string_attribute, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
        return NULL;
    }

    return string_attribute;
}

void
nautilus_file_info_add_string_attribute (NautilusFileInfo *self,
                                         const char       *attribute_name,
                                         const char       *value)
{
    g_autoptr (GError) error = NULL;

    nautilus_file_impl_call_add_string_attribute_sync (proxy, self->uri, attribute_name, value, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
    }
}

void
nautilus_file_info_invalidate_extension_info (NautilusFileInfo *self)
{
    g_autoptr (GError) error = NULL;

    nautilus_file_impl_call_invalidate_extension_info_sync (proxy, self->uri, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Error calling FileImpl: %s", error->message);
    }
}

NautilusFileInfo *
nautilus_file_info_lookup (GFile *location)
{
    return nautilus_file_info_create (location);
}

NautilusFileInfo *
nautilus_file_info_create (GFile *location)
{
    g_autofree char *uri = g_file_get_uri (location);

    return nautilus_file_info_new (uri);
}

NautilusFileInfo *
nautilus_file_info_lookup_for_uri (const char *uri)
{
    return nautilus_file_info_new (uri);
}

NautilusFileInfo *
nautilus_file_info_create_for_uri (const char *uri)
{
    return nautilus_file_info_new (uri);
}

NautilusFileInfo *
nautilus_file_info_new (const char *uri)
{
    NautilusFileInfo *file_info = g_object_new (NAUTILUS_TYPE_FILE_INFO, NULL);

    file_info->uri = g_strdup (uri);

    return file_info;
}

static void
nautilus_file_info_class_init (NautilusFileInfoClass *klass)
{
    GError *error = NULL;

    proxy = nautilus_file_impl_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       "org.gnome.Nautilus" PROFILE,
                                                       "/org/gnome/Nautilus" PROFILE "/FileImpl",
                                                       NULL, &error);
    if (error != NULL)
    {
        g_warning ("Error retrieving file impl proxy: %s", error->message);
    }
}

static void
nautilus_file_info_init (NautilusFileInfo *self)
{

}
