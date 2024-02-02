/*
 * Copyright (C) 2023 Peter Eisenmann <p3732@getgoogleoff.me>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

#define SCHEME_ADMIN "admin"
#define SCHEME_BURN "burn"
#define SCHEME_SEARCH "x-nautilus-search"
#define SCHEME_OTHER_LOCATIONS "other-locations"
#define SCHEME_NETWORK "network"
#define SCHEME_RECENT "recent"
#define SCHEME_STARRED "starred"
#define SCHEME_TRASH "trash"

gboolean
nautilus_scheme_is_internal (const char *scheme);
