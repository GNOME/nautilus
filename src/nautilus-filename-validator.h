/*
 * Copyright (C) 2016 the Nautilus developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "nautilus-types.h"

#include <glib.h>
#include <glib-object.h>

#define NAUTILUS_TYPE_FILENAME_VALIDATOR nautilus_filename_validator_get_type ()
G_DECLARE_FINAL_TYPE (NautilusFilenameValidator, nautilus_filename_validator, NAUTILUS, FILENAME_VALIDATOR, GObject)

gboolean nautilus_filename_validator_get_has_feedback        (NautilusFilenameValidator *self);
gchar * nautilus_filename_validator_get_new_name             (NautilusFilenameValidator *self);
gboolean nautilus_filename_validator_get_will_overwrite      (NautilusFilenameValidator *self);

void    nautilus_filename_validator_set_containing_directory (NautilusFilenameValidator *self,
                                                              NautilusDirectory         *directory);
void    nautilus_filename_validator_set_target_is_folder     (NautilusFilenameValidator *self,
                                                              gboolean                   is_folder);
void    nautilus_filename_validator_set_original_name        (NautilusFilenameValidator *self,
                                                              const char                *original_name);
void    nautilus_filename_validator_validate                 (NautilusFilenameValidator *self);
void    nautilus_filename_validator_try_accept               (NautilusFilenameValidator *self);
