/*
 *  nautilus-file-info.h - Information about a file 
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

/* NautilusFileInfo is an interface to the NautilusFile object.  It 
 * provides access to the asynchronous data in the NautilusFile.
 * Extensions are passed objects of this type for operations. */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILE_INFO (nautilus_file_info_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFileInfo, nautilus_file_info, NAUTILUS, FILE_INFO, GObject)

/**
 * SECTION:nautilus-file-info
 * @title: NautilusFileInfo
 * @short_description: File interface for nautilus extensions
 *
 * #NautilusFileInfo provides methods to get and modify information
 * about file objects in the file manager.
 */

/**
 * NautilusFileInfoInterface:
 * @g_iface: The parent interface.
 * @is_gone: Returns whether the file info is gone.
 *           See nautilus_file_info_is_gone() for details.
 * @get_name: Returns the file name as a string.
 *            See nautilus_file_info_get_name() for details.
 * @get_uri: Returns the file URI as a string.
 *           See nautilus_file_info_get_uri() for details.
 * @get_parent_uri: Returns the file parent URI as a string.
 *                  See nautilus_file_info_get_parent_uri() for details.
 * @get_uri_scheme: Returns the file URI scheme as a string.
 *                  See nautilus_file_info_get_uri_scheme() for details.
 * @get_mime_type: Returns the file mime type as a string.
 *                 See nautilus_file_info_get_mime_type() for details.
 * @is_mime_type: Returns whether the file is the given mime type.
 *                See nautilus_file_info_is_mime_type() for details.
 * @is_directory: Returns whether the file is a directory.
 *                See nautilus_file_info_is_directory() for details.
 * @add_emblem: Adds an emblem to this file.
 *              See nautilus_file_info_add_emblem() for details.
 * @get_string_attribute: Returns the specified file attribute as a string.
 *                        See nautilus_file_info_get_string_attribute() for details.
 * @add_string_attribute: Sets the specified string file attribute value.
 *                        See nautilus_file_info_add_string_attribute() for details.
 * @invalidate_extension_info: Invalidates information of the file provided by extensions.
 *                             See nautilus_file_info_invalidate_extension_info() for details.
 * @get_activation_uri: Returns the file activation URI as a string.
 *                      See nautilus_file_info_get_activation_uri() for details.
 * @get_file_type: Returns the file type.
 *                 See nautilus_file_info_get_file_type() for details.
 * @get_location: Returns the file location as a #GFile.
 *                See nautilus_file_info_get_location() for details.
 * @get_parent_location: Returns the file parent location as a #GFile.
 *                       See nautilus_file_info_get_parent_location() for details.
 * @get_parent_info: Returns the file parent #NautilusFileInfo.
 *                   See nautilus_file_info_get_parent_info() for details.
 * @get_mount: Returns the file mount as a #GMount.
 *             See nautilus_file_info_get_mount() for details.
 * @can_write: Returns whether the file is writable.
 *             See nautilus_file_info_can_write() for details.
 *
 * Interface for extensions to provide additional menu items.
 */

/**
 * nautilus_file_info_list_copy:
 * @files: (element-type NautilusFileInfo): the files to copy
 *
 * Returns: (element-type NautilusFileInfo) (transfer full): a copy of @files.
 *  Use #nautilus_file_info_list_free to free the list and unref its contents.
 */
GList            *nautilus_file_info_list_copy            (GList            *files);
/**
 * nautilus_file_info_list_free:
 * @files: (element-type NautilusFileInfo): a list created with #nautilus_file_info_list_copy
 *
 */
void              nautilus_file_info_list_free            (GList            *files);

/**
 * nautilus_file_info_is_gone:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: whether the file has been deleted
 */
gboolean          nautilus_file_info_is_gone              (NautilusFileInfo *file_info);

/* Name and Location */
/**
 * nautilus_file_info_get_file_type:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: a #GFileType for the location of @file_info
 */
GFileType         nautilus_file_info_get_file_type        (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_location:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: (transfer full): a #GFile for the location of @file_info
 */
GFile            *nautilus_file_info_get_location         (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_name:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: the file name of @file_info
 */
char             *nautilus_file_info_get_name             (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_uri:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: the file URI of @file_info
 */
char             *nautilus_file_info_get_uri              (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_activation_uri:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: the activation URI of @file_info, which may differ from the actual
 *   URI if e.g. the file is a .desktop file or a Nautilus XML link file
 */
char             *nautilus_file_info_get_activation_uri   (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_parent_location:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: (allow-none) (transfer full): a #GFile for the parent location of @file_info,
 *   or %NULL if @file_info has no parent
 */
GFile            *nautilus_file_info_get_parent_location  (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_parent_uri:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: the URI for the parent location of @file_info, or the empty string
 *   if it has none
 */
char             *nautilus_file_info_get_parent_uri       (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_mount:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: (nullable) (transfer full): a #GMount for the mount of @file_info,
 *                                      or %NULL if @file_info has no mount
 */
GMount           *nautilus_file_info_get_mount            (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_uri_scheme:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: the URI scheme of @file_info
 */
char             *nautilus_file_info_get_uri_scheme       (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_get_parent_info:
 * @file_info: a #NautilusFileInfo
 *
 * It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nautilus.
 *
 * Returns: (nullable) (transfer full): a #NautilusFileInfo for the parent of @file_info,
 *                                      or %NULL if @file_info has no parent.
 */
NautilusFileInfo *nautilus_file_info_get_parent_info      (NautilusFileInfo *file_info);

/**
 * nautilus_file_info_get_mime_type:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: (transfer full): the MIME type of @file_info
 */
char *            nautilus_file_info_get_mime_type        (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_is_mime_type:
 * @file_info: a #NautilusFileInfo
 * @mime_type: a MIME type
 *
 * Returns: %TRUE when the MIME type of @file_info matches @mime_type, and
 *   %FALSE otherwise
 */
gboolean          nautilus_file_info_is_mime_type         (NautilusFileInfo *file_info,
                                                           const char       *mime_type);
/**
 * nautilus_file_info_is_directory:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: %TRUE when @file_info is a directory, and %FALSE otherwise
 */
gboolean          nautilus_file_info_is_directory         (NautilusFileInfo *file_info);
/**
 * nautilus_file_info_can_write:
 * @file_info: a #NautilusFileInfo
 *
 * Returns: %TRUE when @file_info is writeable, and %FALSE otherwise
 */
gboolean          nautilus_file_info_can_write            (NautilusFileInfo *file_info);


/* Modifying the NautilusFileInfo */
/**
 * nautilus_file_info_add_emblem:
 * @file_info: a #NautilusFileInfo
 * @emblem_name: the name of an emblem
 */
void              nautilus_file_info_add_emblem           (NautilusFileInfo *file_info,
                                                           const char       *emblem_name);
/**
 * nautilus_file_info_get_string_attribute:
 * @file_info: a #NautilusFileInfo
 * @attribute_name: the name of an attribute
 *
 * Returns: (nullable): the value for the given @attribute_name, or %NULL if
 *   there is none
 */
char             *nautilus_file_info_get_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name);
/**
 * nautilus_file_info_add_string_attribute:
 * @file_info: a #NautilusFileInfo
 * @attribute_name: the name of an attribute
 * @value: the name of an attribute
 */
void              nautilus_file_info_add_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name,
                                                           const char       *value);

/* Invalidating file info */
/**
 * nautilus_file_info_invalidate_extension_info:
 * @file_info: a #NautilusFileInfo
 */
void              nautilus_file_info_invalidate_extension_info (NautilusFileInfo *file_info);

/**
 * nautilus_file_info_lookup:
 * @location: the location for which to look up a corresponding #NautilusFileInfo object
 *
 * Returns: (nullable) (transfer full): a #NautilusFileInfo
 */
NautilusFileInfo *nautilus_file_info_lookup                (GFile *location);
/**
 * nautilus_file_info_create:
 * @location: the location to create the file info for
 *
 * Returns: (transfer full): a #NautilusFileInfo
 */
NautilusFileInfo *nautilus_file_info_create                (GFile *location);
/**
 * nautilus_file_info_lookup_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (nullable) (transfer full): a #NautilusFileInfo
 */
NautilusFileInfo *nautilus_file_info_lookup_for_uri        (const char *uri);
/**
 * nautilus_file_info_create_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (transfer full): a #NautilusFileInfo
 */
NautilusFileInfo *nautilus_file_info_create_for_uri        (const char *uri);
NautilusFileInfo *nautilus_file_info_new                   (const char *uri);

G_END_DECLS
