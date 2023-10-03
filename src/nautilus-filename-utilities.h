/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>


gboolean
nautilus_filename_shorten_base (char       **filename,
                                const char  *base,
                                int          max_length);
