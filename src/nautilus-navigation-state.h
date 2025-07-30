/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <glib-object.h>

#define NAUTILUS_TYPE_NAVIGATION_STATE (nautilus_navigation_state_get_type())
G_DECLARE_FINAL_TYPE (NautilusNavigationState, nautilus_navigation_state,
                      NAUTILUS, NAVIGATION_STATE, GObject)

NautilusNavigationState *
nautilus_navigation_state_new (void);

NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig);
