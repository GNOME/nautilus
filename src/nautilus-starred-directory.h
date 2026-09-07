/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_STARRED_DIRECTORY_PROVIDER_NAME "starred-directory-provider"

#define NAUTILUS_TYPE_STARRED_DIRECTORY (nautilus_starred_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusFavoriteDirectory, nautilus_starred_directory, NAUTILUS, STARRED_DIRECTORY, NautilusDirectory);

NautilusFavoriteDirectory* nautilus_starred_directory_new      (void);

G_END_DECLS
