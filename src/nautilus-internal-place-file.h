/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include "nautilus-file.h"

#define NAUTILUS_TYPE_INTERNAL_PLACE_FILE nautilus_internal_place_file_get_type ()
G_DECLARE_FINAL_TYPE (NautilusInternalPlaceFile, nautilus_internal_place_file,
                      NAUTILUS, INTERNAL_PLACE_FILE,
                      NautilusFile)
