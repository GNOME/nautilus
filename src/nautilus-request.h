/*
* Copyright (C) 2024 The GNOME project contributors
*
* SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "nautilus-enums.h"
#include "nautilus-types.h"

#include <glib.h>

typedef enum {
    REQUEST_DEEP_COUNT,
    REQUEST_DIRECTORY_COUNT,
    REQUEST_EXTENSION_INFO,
    REQUEST_FILE_INFO,
    REQUEST_FILE_LIST,
    REQUEST_FILESYSTEM_INFO,
    REQUEST_MOUNT,
    REQUEST_THUMBNAIL_BUFFER,
    REQUEST_THUMBNAIL_INFO,
    REQUEST_TYPE_LAST
} RequestType;

/* A request for information about one or more files. */
typedef guint32 Request;
typedef gint32 RequestCounter[REQUEST_TYPE_LAST];

#define REQUEST_WANTS_TYPE(request, type) ((request) & (1<<(type)))

Request
nautilus_request_new (NautilusFileAttributes file_attributes,
                      gboolean               get_file_list);

void
nautilus_request_counter_add (RequestCounter counter,
                              Request        request);
void
nautilus_request_counter_remove (RequestCounter counter,
                                 Request        request);

gboolean
nautilus_request_directory_is_ready (Request            request,
                                     NautilusDirectory *directory);
gboolean
nautilus_request_file_is_ready (Request       request,
                                NautilusFile *file);
