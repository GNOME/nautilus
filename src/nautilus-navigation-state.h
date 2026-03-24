/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#pragma once

#include "nautilus-types.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

#define NAUTILUS_TYPE_NAVIGATION_STATE (nautilus_navigation_state_get_type())

G_DECLARE_FINAL_TYPE (NautilusNavigationState, nautilus_navigation_state,
                      NAUTILUS, NAVIGATION_STATE, GObject)

NautilusNavigationState *
nautilus_navigation_state_new (void);
NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig);

gboolean
nautilus_navigation_state_has_current (NautilusNavigationState *self);
gboolean
nautilus_navigation_state_has_backward (NautilusNavigationState *self);
gboolean
nautilus_navigation_state_has_forward (NautilusNavigationState *self);

NautilusFile *
nautilus_navigation_state_get_nth (NautilusNavigationState  *self,
                                   int                       distance,
                                   GList                   **selection_ptr);

void
nautilus_navigation_state_set_selection (NautilusNavigationState *self,
                                         GStrv                    selection);

void
nautilus_navigation_state_activate (NautilusNavigationState *self);
void
nautilus_navigation_state_navigate_history (NautilusNavigationState *self,
                                            int                      distance);
void
nautilus_navigation_state_navigate_file (NautilusNavigationState *self,
                                         NautilusFile            *file);

GStrv
nautilus_navigation_state_get_history (NautilusNavigationState *self,
                                       gboolean                 backwards);
