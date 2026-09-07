/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib.h>

#include "nautilus-types.h"

void nautilus_keyfile_metadata_set_string (NautilusFile *file,
                                           const char *keyfile_filename,
                                           const gchar *name,
                                           const gchar *key,
                                           const gchar *string);

void nautilus_keyfile_metadata_set_stringv (NautilusFile *file,
                                            const char *keyfile_filename,
                                            const char *name,
                                            const char *key,
                                            const char * const *stringv);

gboolean nautilus_keyfile_metadata_update_from_keyfile (NautilusFile *file,
                                                        const char *keyfile_filename,
                                                        const gchar *name);
