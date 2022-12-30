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

#include "nautilus-file-info.h"

#include "nautilus-extension-private.h"

G_DEFINE_INTERFACE (NautilusFileInfo, nautilus_file_info, G_TYPE_OBJECT)

NautilusFileInfo * (*nautilus_file_info_getter)(GFile   *location,
                                                gboolean create);

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
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        g_object_unref (G_OBJECT (l->data));
    }

    g_list_free (files);
}

static void
nautilus_file_info_default_init (NautilusFileInfoInterface *klass)
{
}

gboolean
nautilus_file_info_is_gone (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_gone != NULL, FALSE);

    return iface->is_gone (self);
}

GFileType
nautilus_file_info_get_file_type (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), G_FILE_TYPE_UNKNOWN);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_file_type != NULL, G_FILE_TYPE_UNKNOWN);

    return iface->get_file_type (self);
}

char *
nautilus_file_info_get_name (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_name != NULL, NULL);

    return iface->get_name (self);
}

GFile *
nautilus_file_info_get_location (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_location != NULL, NULL);

    return iface->get_location (self);
}
char *
nautilus_file_info_get_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri != NULL, NULL);

    return iface->get_uri (self);
}

char *
nautilus_file_info_get_activation_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_activation_uri != NULL, NULL);

    return iface->get_activation_uri (self);
}

GFile *
nautilus_file_info_get_parent_location (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_location != NULL, NULL);

    return iface->get_parent_location (self);
}

char *
nautilus_file_info_get_parent_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_uri != NULL, NULL);

    return iface->get_parent_uri (self);
}

NautilusFileInfo *
nautilus_file_info_get_parent_info (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_info != NULL, NULL);

    return iface->get_parent_info (self);
}

GMount *
nautilus_file_info_get_mount (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mount != NULL, NULL);

    return iface->get_mount (self);
}

char *
nautilus_file_info_get_uri_scheme (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri_scheme != NULL, NULL);

    return iface->get_uri_scheme (self);
}

char *
nautilus_file_info_get_mime_type (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mime_type != NULL, NULL);

    return iface->get_mime_type (self);
}

gboolean
nautilus_file_info_is_mime_type (NautilusFileInfo *self,
                                 const char       *mime_type)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);
    g_return_val_if_fail (mime_type != NULL, FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_mime_type != NULL, FALSE);

    return iface->is_mime_type (self, mime_type);
}

gboolean
nautilus_file_info_is_directory (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_directory != NULL, FALSE);

    return iface->is_directory (self);
}

gboolean
nautilus_file_info_can_write (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->can_write != NULL, FALSE);

    return iface->can_write (self);
}

void
nautilus_file_info_add_emblem (NautilusFileInfo *self,
                               const char       *emblem_name)
{
    NautilusFileInfoInterface *iface;

    g_return_if_fail (NAUTILUS_IS_FILE_INFO (self));
    g_return_if_fail (emblem_name != NULL && emblem_name[0] != '\0');

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->add_emblem != NULL);

    iface->add_emblem (self, emblem_name);
}

char *
nautilus_file_info_get_string_attribute (NautilusFileInfo *self,
                                         const char       *attribute_name)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);
    g_return_val_if_fail (attribute_name != NULL, NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_string_attribute != NULL, NULL);

    return iface->get_string_attribute (self, attribute_name);
}

void
nautilus_file_info_add_string_attribute (NautilusFileInfo *self,
                                         const char       *attribute_name,
                                         const char       *value)
{
    NautilusFileInfoInterface *iface;

    g_return_if_fail (NAUTILUS_IS_FILE_INFO (self));
    g_return_if_fail (attribute_name != NULL);
    g_return_if_fail (value != NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->add_string_attribute != NULL);

    iface->add_string_attribute (self, attribute_name, value);
}

void
nautilus_file_info_invalidate_extension_info (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_if_fail (NAUTILUS_IS_FILE_INFO (self));

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->invalidate_extension_info != NULL);

    iface->invalidate_extension_info (self);
}

NautilusFileInfo *
nautilus_file_info_lookup (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return nautilus_file_info_getter (location, FALSE);
}

NautilusFileInfo *
nautilus_file_info_create (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return nautilus_file_info_getter (location, TRUE);
}

NautilusFileInfo *
nautilus_file_info_lookup_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return nautilus_file_info_lookup (location);
}

NautilusFileInfo *
nautilus_file_info_create_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return nautilus_file_info_create (location);
}
