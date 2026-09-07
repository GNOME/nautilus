/*
 * nautilus-search-directory.h: Subclass of NautilusDirectory to implement
 * a virtual directory consisting of the search directory and the search icons
 *
 * SPDX-FileCopyrightText: 2005 Novell, Inc
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-directory.h"

#include "nautilus-types.h"

#define NAUTILUS_SEARCH_DIRECTORY_PROVIDER_NAME "search-directory-provider"
#define NAUTILUS_TYPE_SEARCH_DIRECTORY (nautilus_search_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusSearchDirectory, nautilus_search_directory,
                      NAUTILUS, SEARCH_DIRECTORY, NautilusDirectory)

char   *nautilus_search_directory_generate_new_uri     (void);

GFile *
nautilus_search_directory_get_search_location (NautilusSearchDirectory *self);

NautilusQuery *nautilus_search_directory_get_query       (NautilusSearchDirectory *self);
void           nautilus_search_directory_set_query       (NautilusSearchDirectory *self,
							  NautilusQuery           *query);
