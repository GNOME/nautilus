/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>
#include <time.h>


void
nautilus_date_setup_preferences (void);

char *
nautilus_date_to_str (GDateTime *timestamp,
                      gboolean   use_short_format);
