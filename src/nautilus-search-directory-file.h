/*
 * SPDX-FileCopyrightText: 2003 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include "nautilus-file.h"

#define NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE nautilus_search_directory_file_get_type ()
G_DECLARE_FINAL_TYPE (NautilusSearchDirectoryFile, nautilus_search_directory_file,
                      NAUTILUS, SEARCH_DIRECTORY_FILE,
                      NautilusFile)

void nautilus_search_directory_file_update_display_name (NautilusSearchDirectoryFile *search_file);
