/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_GLOBAL_SEARCH_DIRECTORY_PROVIDER_NAME "global-search-directory-provider"

#define NAUTILUS_TYPE_GLOBAL_SEARCH_DIRECTORY (nautilus_global_search_directory_get_type ())
G_DECLARE_FINAL_TYPE (NautilusGlobalSearchDirectory, nautilus_global_search_directory,
                      NAUTILUS, GLOBAL_SEARCH_DIRECTORY, NautilusDirectory);

NautilusGlobalSearchDirectory *nautilus_global_search_directory_new (void);

G_END_DECLS

