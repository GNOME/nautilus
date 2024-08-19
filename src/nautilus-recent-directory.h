/*
 * Copyright (C) 2024 Corey Berla <coreyberla@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_RECENT_DIRECTORY_PROVIDER_NAME "recent-directory-provider"

#define NAUTILUS_TYPE_RECENT_DIRECTORY (nautilus_recent_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusRecentDirectory, nautilus_recent_directory, NAUTILUS, RECENT_DIRECTORY, NautilusDirectory);

void         nautilus_recent_directory_set_include_windows   (NautilusRecentDirectory *self,
                                                              gboolean                 include);

G_END_DECLS
