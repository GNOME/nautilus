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

typedef struct
{
    GFile *location;
    GStrv  selection;
} NautilusNavigationPosition;

struct _NautilusNavigationState
{
    GList /*<owned NautilusNavigationPosition>*/ *back_list;
    GList /*<owned NautilusNavigationPosition>*/ *forward_list;
    NautilusNavigationPosition *current;
};

NautilusNavigationState *
nautilus_navigation_state_new (void);
void
nautilus_navigation_state_free (NautilusNavigationState *self);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusNavigationState, nautilus_navigation_state_free)

NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig);

void
nautilus_navigation_state_set_selection (NautilusNavigationState *self,
                                         GStrv                    selection);

void
nautilus_navigation_state_navigate_history (NautilusNavigationState *self,
                                            int                      distance);
void
nautilus_navigation_state_navigate_location (NautilusNavigationState *self,
                                             GFile                   *location);
