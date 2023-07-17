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

/**
 * NautilusFileInfo:
 *
 * File interface for nautilus extensions.
 *
 * `NautilusFileInfo` provides methods to get and modify information
 * about file objects in the file manager.
 */

G_DEFINE_INTERFACE (NautilusFileInfo, nautilus_file_info, G_TYPE_OBJECT)

NautilusFileInfo * (*nautilus_file_info_getter)(GFile   *location,
                                                gboolean create);

/**
 * nautilus_file_info_list_copy:
 * @files: (element-type NautilusFileInfo): the files to copy
 *
 * Deep copy a list of `NautilusFileInfo`.
 *
 * Returns: (element-type NautilusFileInfo) (transfer full): a copy of @files.
 *  Use [func@FileInfo.list_free] to free the list and unref its contents.
 */
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

/**
 * nautilus_file_info_list_free:
 * @files: (element-type NautilusFileInfo): a list created with [func@FileInfo.list_copy]
 *
 * Deep free a list of `NautilusFileInfo`.
 *
 */
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

/**
 * nautilus_file_info_is_gone:
 *
 * Get whether a `NautilusFileInfo` is gone.
 *
 * Returns: whether the file is gone.
 */
gboolean
nautilus_file_info_is_gone (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_gone != NULL, FALSE);

    return iface->is_gone (self);
}

/**
 * nautilus_file_info_get_file_type:
 *
 * Get the cached [enum@Gio.FileType].
 *
 * Returns: the file type
 */
GFileType
nautilus_file_info_get_file_type (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), G_FILE_TYPE_UNKNOWN);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_file_type != NULL, G_FILE_TYPE_UNKNOWN);

    return iface->get_file_type (self);
}

/**
 * nautilus_file_info_get_name:
 *
 * Gets the name.
 *
 * Returns: the file name of @file_info
 */
char *
nautilus_file_info_get_name (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_name != NULL, NULL);

    return iface->get_name (self);
}

/**
 * nautilus_file_info_get_location:
 *
 * Get the corresponding [iface@Gio.File]
 *
 * Returns: (transfer full): the corresponding location.
 */
GFile *
nautilus_file_info_get_location (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_location != NULL, NULL);

    return iface->get_location (self);
}

/**
 * nautilus_file_info_get_uri:
 *
 * Gets the URI.
 *
 * Returns: the file URI of @file_info
 */
char *
nautilus_file_info_get_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri != NULL, NULL);

    return iface->get_uri (self);
}

/**
 * nautilus_file_info_get_activation_uri:
 *
 * Gets the activation uri.
 *
 * The activation uri may differ from the actual URI if e.g. the file is a .desktop
 * file or a Nautilus XML link file.
 *
 * Returns: the activation URI of @file_info
 */
char *
nautilus_file_info_get_activation_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_activation_uri != NULL, NULL);

    return iface->get_activation_uri (self);
}

/**
 * nautilus_file_info_get_parent_location:
 *
 * Gets the parent location.
 *
 * Returns: (allow-none) (transfer full): a #GFile for the parent location of @file_info,
 *   or %NULL if @file_info has no parent
 */
GFile *
nautilus_file_info_get_parent_location (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_location != NULL, NULL);

    return iface->get_parent_location (self);
}

/**
 * nautilus_file_info_get_parent_uri:
 *
 * Get the parent `NautilusFileInfo` uri.
 *
 * Returns: the URI for the parent location of @file_info, or the empty string
 *   if it has none
 */
char *
nautilus_file_info_get_parent_uri (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_uri != NULL, NULL);

    return iface->get_parent_uri (self);
}

/**
 * nautilus_file_info_get_parent_info:
 *
 * Get the parent `NautilusFileInfo`.
 *
 * It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nautilus.
 *
 * Returns: (nullable) (transfer full): a #NautilusFileInfo for the parent of @file_info,
 *                                      or %NULL if @file_info has no parent.
 */
NautilusFileInfo *
nautilus_file_info_get_parent_info (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_info != NULL, NULL);

    return iface->get_parent_info (self);
}

/**
 * nautilus_file_info_get_mount:
 *
 * Gets the cached mount.
 *
 * This only returns the [iface@Gio.Mount] if Nautilus has already cached it.
 * The return value may be `NULL` even if the `NautilusFileInfo` has a corresponding
 * mount in which case you can call [method@Gio.File.find_enclosing_mount_async].
 *
 * Returns: (nullable) (transfer full): the mount of @file_info,
 *                                      or %NULL if @file_info has no mount
 */
GMount *
nautilus_file_info_get_mount (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mount != NULL, NULL);

    return iface->get_mount (self);
}

/**
 * nautilus_file_info_get_uri_scheme:
 *
 * Get the uri scheme.
 *
 * Returns: the URI scheme of @file_info
 */
char *
nautilus_file_info_get_uri_scheme (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri_scheme != NULL, NULL);

    return iface->get_uri_scheme (self);
}

/**
 * nautilus_file_info_get_mime_type:
 *
 * Get the cached mime_type.
 *
 * Returns: (transfer full): the MIME type of @file_info
 */
char *
nautilus_file_info_get_mime_type (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), NULL);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mime_type != NULL, NULL);

    return iface->get_mime_type (self);
}

/**
 * nautilus_file_info_is_mime_type:
 *
 * Gets whether the mime_type of the `NautilusFileInfo` matches the given type.
 *
 * Returns: %TRUE when the MIME type of @file_info matches @mime_type, and
 *   %FALSE otherwise
 */
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

/**
 * nautilus_file_info_is_directory:
 *
 * Gets whether the `NautilusFileInfo` is a directory.
 *
 * Uses the cached [enum@Gio.FileType] matches `G_FILE_TYPE_DIRECTORY` without
 * doing any blocking i/o.
 *
 * Returns: %TRUE when @file_info is a directory, and %FALSE otherwise
 */
gboolean
nautilus_file_info_is_directory (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_directory != NULL, FALSE);

    return iface->is_directory (self);
}

/**
 * nautilus_file_info_can_write:
 *
 * Gets whether the `NautilusFileInfo` is writeable.
 *
 * Returns: %TRUE when @file_info is writeable, and %FALSE otherwise
 */
gboolean
nautilus_file_info_can_write (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (self), FALSE);

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->can_write != NULL, FALSE);

    return iface->can_write (self);
}

/**
 * nautilus_file_info_add_emblem:
 * @emblem_name: the name of an emblem
 *
 * Add an emblem.
 */
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

/**
 * nautilus_file_info_get_string_attribute:
 * @attribute_name: the name of an attribute
 *
 * Get the attribute's value.
 *
 * Returns: (nullable): the value for the given @attribute_name, or %NULL if
 *   there is none
 */
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

/**
 * nautilus_file_info_add_string_attribute:
 * @attribute_name: the name of an attribute
 * @value: the value of an attribute
 *
 * Set's the attributes value or replacing the existing value (if one exists).
 *
 * This function is necessary to set the value of the `NautilusFileInfo`'s
 * correspond attribute for a [property@Column:attribute].
 */
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

/**
 * nautilus_file_info_invalidate_extension_info:
 *
 * Invalidate the current extension information.
 *
 * This removes any information, such as emblems or or string attributes, that
 * were added to the `NautilusFileInfo` from any extension.
 *
 */
void
nautilus_file_info_invalidate_extension_info (NautilusFileInfo *self)
{
    NautilusFileInfoInterface *iface;

    g_return_if_fail (NAUTILUS_IS_FILE_INFO (self));

    iface = NAUTILUS_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->invalidate_extension_info != NULL);

    iface->invalidate_extension_info (self);
}

/**
 * nautilus_file_info_lookup:
 * @location: the location for which to look up a corresponding #NautilusFileInfo object
 *
 * Get an existing `NautilusFileInfo` or `NULL` if it does not exist in the
 * application cache.
 *
 * Returns: (nullable) (transfer full):
 */
NautilusFileInfo *
nautilus_file_info_lookup (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return nautilus_file_info_getter (location, FALSE);
}

/**
 * nautilus_file_info_create:
 * @location: the location to create the file info for
 *
 * Get an existing `NautilusFileInfo` (if it exists) or create a new one is it
 * does not exist.
 *
 * Returns: (transfer full):
 */
NautilusFileInfo *
nautilus_file_info_create (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return nautilus_file_info_getter (location, TRUE);
}

/**
 * nautilus_file_info_lookup_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Get an existing `NautilusFileInfo` or `NULL` if it does not exist in the
 * application cache.
 *
 * Returns: (nullable) (transfer full):
 */
NautilusFileInfo *
nautilus_file_info_lookup_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return nautilus_file_info_lookup (location);
}

/**
 * nautilus_file_info_create_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Get an existing `NautilusFileInfo` (if it exists) or create a new one is it
 * does not exist.
 *
 * Returns: (transfer full):
 */
NautilusFileInfo *
nautilus_file_info_create_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return nautilus_file_info_create (location);
}
