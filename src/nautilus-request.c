/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-request.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"

#define REQUEST_SET_TYPE(request, type) (request) |= (1 << (type))
Request
nautilus_request_new (NautilusFileAttributes file_attributes,
                      gboolean               get_file_list)
{
    Request request = 0;

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_DIRECTORY_COUNT);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_DEEP_COUNT);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_EXTENSION_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO)
    {
        REQUEST_SET_TYPE (request, REQUEST_FILESYSTEM_INFO);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_INFO) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_MOUNT)
    {
        REQUEST_SET_TYPE (request, REQUEST_MOUNT);
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_INFO) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_THUMBNAIL_INFO);
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_BUFFER)
    {
        REQUEST_SET_TYPE (request, REQUEST_THUMBNAIL_BUFFER);
        REQUEST_SET_TYPE (request, REQUEST_THUMBNAIL_INFO);
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if (get_file_list)
    {
        REQUEST_SET_TYPE (request, REQUEST_FILE_LIST);
    }

    return request;
}
#undef REQUEST_SET_TYPE

void
nautilus_request_counter_add (RequestCounter counter,
                              Request        request)
{
    for (uint i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (REQUEST_WANTS_TYPE (request, i))
        {
            counter[i]++;
        }
    }
}

void
nautilus_request_counter_remove (RequestCounter counter,
                                 Request        request)
{
    for (uint i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (REQUEST_WANTS_TYPE (request, i))
        {
            counter[i]--;
        }
    }
}

static gboolean
file_is_ready (Request       request,
               NautilusFile *file)
{

    if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT) &&
        nautilus_file_needs_directory_count (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO) &&
        nautilus_file_needs_file_info (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_FILESYSTEM_INFO) &&
        nautilus_file_needs_filesystem_info (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT) &&
        nautilus_file_needs_deep_count (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL_BUFFER) &&
        nautilus_file_needs_thumbnail_buffer (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL_INFO) &&
        nautilus_file_needs_thumbnail_info (file))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT) &&
        nautilus_file_needs_mount (file))
    {
        return FALSE;
    }

    return TRUE;
}

gboolean
nautilus_request_file_is_ready (Request       request,
                                NautilusFile *file)
{
    g_return_val_if_fail (file != NULL, FALSE);
    g_return_val_if_fail (!REQUEST_WANTS_TYPE (request, REQUEST_FILE_LIST), FALSE);

    return file_is_ready (request, file);
}

gboolean
nautilus_request_directory_is_ready (Request            request,
                                     NautilusDirectory *directory)
{
    g_return_val_if_fail (directory != NULL, FALSE);

    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_LIST) &&
        !nautilus_directory_has_file_list (directory))
    {
        return FALSE;
    }

    for (GList *node = directory->details->file_list; node != NULL; node = node->next) 
    {
        NautilusFile *file = node->data;

        if (!file_is_ready (request, file))
        {
            return FALSE;
        }
    }

    return TRUE;
}
