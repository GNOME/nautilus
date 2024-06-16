/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include <glib.h>

typedef enum
{
    NAUTILUS_MODE_BROWSE = 0,
    NAUTILUS_MODE_OPEN_FILE,
    NAUTILUS_MODE_OPEN_FILES,
    NAUTILUS_MODE_OPEN_FOLDER,
    NAUTILUS_MODE_OPEN_FOLDERS,
    NAUTILUS_MODE_SAVE_FILE,
    NAUTILUS_MODE_SAVE_FILES,
} NautilusMode;

#define nautilus_mode_is_save(mode) \
        ((mode) == NAUTILUS_MODE_SAVE_FILE || \
         (mode) == NAUTILUS_MODE_SAVE_FILES)

#define nautilus_mode_is_open(mode) \
        ((mode) == NAUTILUS_MODE_OPEN_FILE || \
         (mode) == NAUTILUS_MODE_OPEN_FILES || \
         (mode) == NAUTILUS_MODE_OPEN_FOLDER || \
         (mode) == NAUTILUS_MODE_OPEN_FOLDERS)

#define nautilus_mode_is_single_selection(mode) \
        ((mode) == NAUTILUS_MODE_OPEN_FILE || \
         (mode) == NAUTILUS_MODE_OPEN_FOLDER || \
         (mode) == NAUTILUS_MODE_SAVE_FILE || \
         (mode) == NAUTILUS_MODE_SAVE_FILES)
