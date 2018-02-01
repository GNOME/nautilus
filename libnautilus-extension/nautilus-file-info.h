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

#ifndef NAUTILUS_FILE_INFO_H
#define NAUTILUS_FILE_INFO_H

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILE_INFO (nautilus_file_info_get_type ())

G_DECLARE_INTERFACE (NautilusFileInfo, nautilus_file_info, NAUTILUS, FILE_INFO, GObject)

/**
 * NautilusFileInfoInterface:
 * @g_iface: The parent interface.
 * @is_gone: Returns whether the file info is gone.
 *   See nautilus_file_info_is_gone() for details.
 * @get_name: Returns the file name as a string.
 *   See nautilus_file_info_get_name() for details.
 * @get_uri: Returns the file URI as a string.
 *   See nautilus_file_info_get_uri() for details.
 * @get_parent_uri: Returns the file parent URI as a string.
 *   See nautilus_file_info_get_parent_uri() for details.
 * @get_uri_scheme: Returns the file URI scheme as a string.
 *   See nautilus_file_info_get_uri_scheme() for details.
 * @get_mime_type: Returns the file mime type as a string.
 *   See nautilus_file_info_get_mime_type() for details.
 * @is_mime_type: Returns whether the file is the given mime type.
 *   See nautilus_file_info_is_mime_type() for details.
 * @is_directory: Returns whether the file is a directory.
 *   See nautilus_file_info_is_directory() for details.
 * @add_emblem: Adds an emblem to this file.
 *   See nautilus_file_info_add_emblem() for details.
 * @get_string_attribute: Returns the specified file attribute as a string.
 *   See nautilus_file_info_get_string_attribute() for details.
 * @add_string_attribute: Sets the specified string file attribute value.
 *   See nautilus_file_info_add_string_attribute() for details.
 * @invalidate_extension_info: Invalidates information of the file provided by extensions.
 *   See nautilus_file_info_invalidate_extension_info() for details.
 * @get_activation_uri: Returns the file activation URI as a string.
 *   See nautilus_file_info_get_activation_uri() for details.
 * @get_file_type: Returns the file type.
 *   See nautilus_file_info_get_file_type() for details.
 * @get_location: Returns the file location as a #GFile.
 *   See nautilus_file_info_get_location() for details.
 * @get_parent_location: Returns the file parent location as a #GFile.
 *   See nautilus_file_info_get_parent_location() for details.
 * @get_parent_info: Returns the file parent #NautilusFileInfo.
 *   See nautilus_file_info_get_parent_info() for details.
 * @get_mount: Returns the file mount as a #GMount.
 *   See nautilus_file_info_get_mount() for details.
 * @can_write: Returns whether the file is writable.
 *   See nautilus_file_info_can_write() for details.
 *
 * Interface for extensions to provide additional menu items.
 */
struct _NautilusFileInfoInterface
{
    GTypeInterface g_iface;

    gboolean          (*is_gone)                   (NautilusFileInfo *file_info);

    char             *(*get_name)                  (NautilusFileInfo *file_info);
    char             *(*get_uri)                   (NautilusFileInfo *file_info);
    char             *(*get_parent_uri)            (NautilusFileInfo *file_info);
    char             *(*get_uri_scheme)            (NautilusFileInfo *file_info);

    char             *(*get_mime_type)             (NautilusFileInfo *file_info);
    gboolean          (*is_mime_type)              (NautilusFileInfo *file_info,
                                                    const char       *mime_type);
    gboolean          (*is_directory)              (NautilusFileInfo *file_info);

    void              (*add_emblem)                (NautilusFileInfo *file_info,
                                                    const char       *emblem_name);
    char             *(*get_string_attribute)      (NautilusFileInfo *file_info,
                                                    const char       *attribute_name);
    void              (*add_string_attribute)      (NautilusFileInfo *file_info,
                                                    const char       *attribute_name,
                                                    const char       *value);
    void              (*invalidate_extension_info) (NautilusFileInfo *file_info);

    char             *(*get_activation_uri)        (NautilusFileInfo *file_info);

    GFileType         (*get_file_type)             (NautilusFileInfo *file_info);
    GFile            *(*get_location)              (NautilusFileInfo *file_info);
    GFile            *(*get_parent_location)       (NautilusFileInfo *file_info);
    NautilusFileInfo *(*get_parent_info)           (NautilusFileInfo *file_info);
    GMount           *(*get_mount)                 (NautilusFileInfo *file_info);
    gboolean          (*can_write)                 (NautilusFileInfo *file_info);
};

GList            *nautilus_file_info_list_copy            (GList            *files);
void              nautilus_file_info_list_free            (GList            *files);

/* Return true if the file has been deleted */
gboolean          nautilus_file_info_is_gone              (NautilusFileInfo *file_info);

/* Name and Location */
GFileType         nautilus_file_info_get_file_type        (NautilusFileInfo *file_info);
GFile            *nautilus_file_info_get_location         (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_name             (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_uri              (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_activation_uri   (NautilusFileInfo *file_info);
GFile            *nautilus_file_info_get_parent_location  (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_parent_uri       (NautilusFileInfo *file_info);
GMount           *nautilus_file_info_get_mount            (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_uri_scheme       (NautilusFileInfo *file_info);
/* It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nautilus.
 */
NautilusFileInfo *nautilus_file_info_get_parent_info      (NautilusFileInfo *file_info);

/* File Type */
char *            nautilus_file_info_get_mime_type        (NautilusFileInfo *file_info);
gboolean          nautilus_file_info_is_mime_type         (NautilusFileInfo *file_info,
                                                           const char       *mime_type);
gboolean          nautilus_file_info_is_directory         (NautilusFileInfo *file_info);
gboolean          nautilus_file_info_can_write            (NautilusFileInfo *file_info);


/* Modifying the NautilusFileInfo */
void              nautilus_file_info_add_emblem           (NautilusFileInfo *file_info,
                                                           const char       *emblem_name);
char             *nautilus_file_info_get_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name);
void              nautilus_file_info_add_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name,
                                                           const char       *value);

/* Invalidating file info */
void              nautilus_file_info_invalidate_extension_info (NautilusFileInfo *file_info);

NautilusFileInfo *nautilus_file_info_lookup                (GFile *location);
NautilusFileInfo *nautilus_file_info_create                (GFile *location);
NautilusFileInfo *nautilus_file_info_lookup_for_uri        (const char *uri);
NautilusFileInfo *nautilus_file_info_create_for_uri        (const char *uri);

G_END_DECLS

#endif
