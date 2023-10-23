/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>


char *
nautilus_filename_create_duplicate (const char *name,
                                    int         count_increment,
                                    int         max_length,
                                    gboolean    ignore_extension);

char *
nautilus_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length);

char *
nautilus_filename_get_common_prefix (GList *strs,
                                     int    min_required_len);

const char *
nautilus_filename_get_extension (const char *filename);

gboolean
nautilus_filename_shorten_base (char       **filename,
                                const char  *base,
                                int          max_length);
