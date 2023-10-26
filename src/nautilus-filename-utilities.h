/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>


/**
 * nautilus_filename_create_duplicate:
 * @name: Name of the original file
 * @count: Number of copies of @name already existing
 * @max_length: Maximum length that resulting file name can have
 * @ignore_extension: Whether to ignore file extensions (should be FALSE for directories)
 *
 * Creates a new name for a copy of @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a copy of @name.
 */
char *
nautilus_filename_create_duplicate (const char *name,
                                    int         count_increment,
                                    int         max_length,
                                    gboolean    ignore_extension);

/**
 * nautilus_filename_for_link:
 * @name: Name of the original file
 * @count: Number of links to @name already existing
 * @max_length: Maximum length that resulting file name can have
 *
 * Creates a new name for a link to @name, that is no longer than @max_length
 * bytes long.
 *
 * Returns: (transfer full): A file name for a link to @name.
 */
char *
nautilus_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length);

/**
 * nautilus_filename_shorten_base:
 * @filename: (transfer full): Pointer to a filename that is to be shortened
 * @base: a base from which @filename was constructed
 * @max_length: Maximum length that @filename should have
 *
 * Shortens @filename to a maximum length of @max_length. If it already is
 * shorter than @max_length, it is unchanged, otherwise the old @filename is
 * freed and replaced with a newly allocated one.
 *
 * If @base can not be shortened in a way that a new filename would be shorter
 * than @max_length, @filename also stays unchanged.
 *
 * Returns: Whether @filename was shortened.
 */
gboolean
nautilus_filename_shorten_base (char       **filename,
                                const char  *base,
                                int          max_length);
