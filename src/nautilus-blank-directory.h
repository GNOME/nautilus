/*
 * Copyright (C) 2023 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_BLANK_DIRECTORY_PROVIDER_NAME "blank-directory-provider"

#define NAUTILUS_TYPE_BLANK_DIRECTORY (nautilus_blank_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusBlankDirectory, nautilus_blank_directory,
                      NAUTILUS, BLANK_DIRECTORY, NautilusDirectory);

G_END_DECLS
