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

G_DECLARE_INTERFACE (NautilusFileInfo, nautilus_file_info, NAUTILUS, FILE_INFO, GObject)

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
gboolean          nautilus_file_info_is_gone              (NautilusFileInfo *file_info);
GFileType         nautilus_file_info_get_file_type        (NautilusFileInfo *file_info);
GFile            *nautilus_file_info_get_location         (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_name             (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_uri              (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_activation_uri   (NautilusFileInfo *file_info);
GFile            *nautilus_file_info_get_parent_location  (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_parent_uri       (NautilusFileInfo *file_info);
GMount           *nautilus_file_info_get_mount            (NautilusFileInfo *file_info);
char             *nautilus_file_info_get_uri_scheme       (NautilusFileInfo *file_info);
NautilusFileInfo *nautilus_file_info_get_parent_info      (NautilusFileInfo *file_info);
char *            nautilus_file_info_get_mime_type        (NautilusFileInfo *file_info);
gboolean          nautilus_file_info_is_mime_type         (NautilusFileInfo *file_info,
                                                           const char       *mime_type);
gboolean          nautilus_file_info_is_directory         (NautilusFileInfo *file_info);
gboolean          nautilus_file_info_can_write            (NautilusFileInfo *file_info);
void              nautilus_file_info_add_emblem           (NautilusFileInfo *file_info,
                                                           const char       *emblem_name);
char             *nautilus_file_info_get_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name);
void              nautilus_file_info_add_string_attribute (NautilusFileInfo *file_info,
                                                           const char       *attribute_name,
                                                           const char       *value);
void              nautilus_file_info_invalidate_extension_info (NautilusFileInfo *file_info);
NautilusFileInfo *nautilus_file_info_lookup                (GFile *location);
NautilusFileInfo *nautilus_file_info_create                (GFile *location);
NautilusFileInfo *nautilus_file_info_lookup_for_uri        (const char *uri);
NautilusFileInfo *nautilus_file_info_create_for_uri        (const char *uri);
G_END_DECLS
