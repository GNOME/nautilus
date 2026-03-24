/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "nautilus-navigation-state.h"

#include <gio/gio.h>

/**
 * nautilus_navigation_position_new:
 *
 * @location: (not nullable): Location of navigation position
 * @selection: (transfer full): Array of selected URIs
 */
static NautilusNavigationPosition *
nautilus_navigation_position_new (GFile *location,
                                  GStrv  selection)
{
    NautilusNavigationPosition *self = g_new0 (NautilusNavigationPosition, 1);

    self->location = g_object_ref (location);
    self->selection = selection;

    return self;
}

static NautilusNavigationPosition *
nautilus_navigation_position_copy (NautilusNavigationPosition *orig)
{
    NautilusNavigationPosition *self = g_new0 (NautilusNavigationPosition, 1);

    g_set_object (&self->location, orig->location);
    self->selection = g_strdupv (orig->selection);

    return self;
}

static void
nautilus_navigation_position_free (NautilusNavigationPosition *self)
{
    g_clear_object (&self->location);
    g_strfreev (self->selection);
    g_free (self);
}

NautilusNavigationState *
nautilus_navigation_state_new (void)
{
    NautilusNavigationState *self = g_new0 (NautilusNavigationState, 1);

    return self;
}

static void
clear_navigation_position_list (GList *list)
{
    g_list_free_full (list, (GDestroyNotify) nautilus_navigation_position_free);
}

void
nautilus_navigation_state_free (NautilusNavigationState *self)
{
    g_clear_pointer (&self->back_list, clear_navigation_position_list);
    g_clear_pointer (&self->forward_list, clear_navigation_position_list);
    g_clear_pointer (&self->current, nautilus_navigation_position_free);
    g_free (self);
}

NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig)
{
    NautilusNavigationState *self = g_new0 (NautilusNavigationState, 1);

    self->back_list = g_list_copy_deep (orig->back_list,
                                        (GCopyFunc) nautilus_navigation_position_copy,
                                        NULL);
    self->forward_list = g_list_copy_deep (orig->forward_list,
                                           (GCopyFunc) nautilus_navigation_position_copy,
                                           NULL);
    self->current = nautilus_navigation_position_copy (orig->current);

    return self;
}

/**
 * @selection: (transfer full): Array of selected URIs
 */
void
nautilus_navigation_state_set_selection (NautilusNavigationState *self,
                                         GStrv                    selection)
{
    g_return_if_fail (self->current != NULL);

    g_strfreev (self->current->selection);
    self->current->selection = selection;
}

/**
 * nautilus_navigation_state_navigate_history:
 *
 * @self: (not nullable): Navigation state
 * @distance: Distance to navigate forward/backward in the state
 */
void
nautilus_navigation_state_navigate_history (NautilusNavigationState *self,
                                            int                      distance)
{
    g_return_if_fail (distance != 0);

    GList *from_list = distance > 0 ? self->forward_list : self->back_list;
    GList *to_list = distance > 0 ? self->back_list : self->forward_list;
    guint from_len = g_list_length (from_list);
    guint abs_distance = MIN (from_len, (guint) ABS (distance));

    g_return_if_fail (from_list != NULL);

    /* Add current position to history */
    if (self->current != NULL)
    {
        to_list = g_list_prepend (to_list, self->current);
    }

    /* Move all skipped nodes to other list */
    for (guint i = 0; i < abs_distance - 1; i += 1)
    {
        GList *moved_node = from_list;

        from_list = g_list_remove_link (from_list, moved_node);
        to_list = g_list_concat (moved_node, to_list);
    }

    /* Set new current position */
    self->current = from_list->data;
    from_list = g_list_delete_link (from_list, from_list);

    /* Store new list pointers */
    self->forward_list = distance > 0 ? from_list : to_list;
    self->back_list = distance > 0 ? to_list : from_list;
}

void
nautilus_navigation_state_navigate_location (NautilusNavigationState *self,
                                             GFile                   *location)
{
    g_clear_pointer (&self->forward_list, clear_navigation_position_list);

    if (location == NULL)
    {
        g_message ("navigating nowhere clearing everything");
        g_clear_pointer (&self->back_list, clear_navigation_position_list);
        g_clear_pointer (&self->current, nautilus_navigation_position_free);
        return;
    }

    if (self->current != NULL)
    {
        self->back_list = g_list_prepend (self->back_list, self->current);
    }

    self->current = nautilus_navigation_position_new (location, NULL);
}
