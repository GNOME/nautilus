/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>


char *
nautilus_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length);

const char *
nautilus_filename_get_extension (const char *filename);

gboolean
nautilus_filename_shorten_base (char       **filename,
                                const char  *base,
                                int          max_length);
