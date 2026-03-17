/*
 * Copyright (C) 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* NautilusFileInfo is an interface to the NautilusFile object.
 * It provides access to the asynchronous data in the NautilusFile.
 * Extensions are passed objects of this type for operations. */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _NautilusFileInfo NautilusFileInfo;

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

extern NautilusFileInfo *(*nautilus_file_info_getter) (GFile *location, gboolean create);

G_END_DECLS
