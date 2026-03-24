/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#include "nautilus-navigation-state.h"

#include "nautilus-file.h"

#include <gio/gio.h>

typedef struct
{
    union
    {
        NautilusFile *active;
        GFile *inactive;
    } file;
    GStrv selection;
} NavigationPosition;

/**
 * navigation_position_new:
 *
 * @file: (not nullable): Location of navigation position
 */
static NavigationPosition *
navigation_position_new (NautilusFile *file)
{
    NavigationPosition *self = g_new0 (NavigationPosition, 1);

    self->file.active = nautilus_file_ref (file);

    return self;
}

static NavigationPosition *
navigation_position_copy_active (NavigationPosition *orig)
{
    NavigationPosition *self = g_new0 (NavigationPosition, 1);

    g_set_object (&self->file.active, orig->file.active);
    self->selection = g_strdupv (orig->selection);

    return self;
}

static NavigationPosition *
navigation_position_copy_inactive (NavigationPosition *orig)
{
    NavigationPosition *self = g_new0 (NavigationPosition, 1);
    GFile *location = nautilus_file_get_location (orig->file.active);

    g_set_object (&self->file.inactive, location);
    self->selection = g_strdupv (orig->selection);

    return self;
}

static void
navigation_position_free (NavigationPosition *self)
{
    g_clear_object ((GObject **) &self->file);
    g_strfreev (self->selection);
    g_free (self);
}

static void
clear_navigation_position_list (GList *list)
{
    g_list_free_full (list, (GDestroyNotify) navigation_position_free);
}

static void
navigation_position_activate (NavigationPosition *self)
{
    g_autoptr (GFile) location = self->file.inactive;

    self->file.active = nautilus_file_get (location);
}

struct _NautilusNavigationState
{
    GObject parent_instance;

    gboolean is_active;
    GList /*<owned NavigationPosition>*/ *back_list;
    GList /*<owned NavigationPosition>*/ *forward_list;
    NavigationPosition *current;
};

G_DEFINE_FINAL_TYPE (NautilusNavigationState, nautilus_navigation_state, G_TYPE_OBJECT)

static void
nautilus_navigation_state_finalize (GObject *object)
{
    NautilusNavigationState *self = (NautilusNavigationState *) object;

    g_clear_pointer (&self->back_list, clear_navigation_position_list);
    g_clear_pointer (&self->forward_list, clear_navigation_position_list);
    g_clear_pointer (&self->current, navigation_position_free);

    G_OBJECT_CLASS (nautilus_navigation_state_parent_class)->finalize (object);
}

static void
nautilus_navigation_state_class_init (NautilusNavigationStateClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_navigation_state_finalize;
}

static void
nautilus_navigation_state_init (NautilusNavigationState *self)
{
}

NautilusNavigationState *
nautilus_navigation_state_new (void)
{
    NautilusNavigationState *self = g_object_new (NAUTILUS_TYPE_NAVIGATION_STATE, NULL);

    self->is_active = TRUE;

    return self;
}

NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig)
{
    g_return_val_if_fail (orig->is_active, NULL);

    NautilusNavigationState *self = g_object_new (NAUTILUS_TYPE_NAVIGATION_STATE, NULL);

    self->back_list = g_list_copy_deep (orig->back_list,
                                        (GCopyFunc) navigation_position_copy_inactive,
                                        NULL);
    self->forward_list = g_list_copy_deep (orig->forward_list,
                                           (GCopyFunc) navigation_position_copy_inactive,
                                           NULL);
    self->current = navigation_position_copy_active (orig->current);

    return self;
}

gboolean
nautilus_navigation_state_has_current (NautilusNavigationState *self)
{
    return self->current != NULL;
}

gboolean
nautilus_navigation_state_has_backward (NautilusNavigationState *self)
{
    return self->back_list != NULL;
}

gboolean
nautilus_navigation_state_has_forward (NautilusNavigationState *self)
{
    return self->forward_list != NULL;
}

/**
 * nautilus_navigation_state_get_nth:
 *
 * @self: an active navigaton state
 * @distance: Distance of position in the state relative to the current one
 * @selection: (nullable): Optional pointer to set n-th position's selection
 */
NautilusFile *
nautilus_navigation_state_get_nth (NautilusNavigationState  *self,
                                   int                       distance,
                                   GList                   **selection_ptr)
{
    /* Current (distance == 0) is always available, even for inactive states */
    g_return_val_if_fail (self->is_active || distance == 0, NULL);

    NavigationPosition *position;

    if (distance == 0)
    {
        position = self->current;
    }
    else
    {
        GList *list = (distance < 0) ? self->back_list : self->forward_list;

        g_return_val_if_fail (list != NULL, NULL);

        /* Reduce distance by 1 as list indexing starts with 0 */
        guint list_distance = ABS (distance) - 1;
        guint len = g_list_length (list);

        /* If distance is out of bounds, use the list's end */
        if (list_distance >= len)
        {
            g_warning ("Requested navigation position out of range (%d/%d)", list_distance, len);
            list_distance = len - 1;
        }

        position = g_list_nth_data (list, list_distance);
    }

    if (selection_ptr != NULL && position->selection != NULL)
    {
        *selection_ptr = nautilus_file_list_from_uris (position->selection);
    }

    return position->file.active;
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

void
nautilus_navigation_state_activate (NautilusNavigationState *self)
{
    g_list_foreach (self->back_list, (GFunc) navigation_position_activate, NULL);
    g_list_foreach (self->forward_list, (GFunc) navigation_position_activate, NULL);

    self->is_active = TRUE;
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
nautilus_navigation_state_navigate_file (NautilusNavigationState *self,
                                         NautilusFile            *file)
{
    g_return_if_fail (self->is_active);

    g_clear_pointer (&self->forward_list, clear_navigation_position_list);

    if (file == NULL)
    {
        g_clear_pointer (&self->back_list, clear_navigation_position_list);
        g_clear_pointer (&self->current, navigation_position_free);
        return;
    }

    if (self->current != NULL)
    {
        self->back_list = g_list_prepend (self->back_list, self->current);
    }

    self->current = navigation_position_new (file);
}

GStrv
nautilus_navigation_state_get_history (NautilusNavigationState *self,
                                       gboolean                 backwards)
{
    g_return_val_if_fail (self->is_active, NULL);

    g_autoptr (GStrvBuilder) strv_builder = g_strv_builder_new ();
    GList *list = backwards ? self->back_list : self->forward_list;

    for (GList *l = list; l != NULL; l = l->next)
    {
        NavigationPosition *position = l->data;
        NautilusFile *file = position->file.active;
        const char *name = nautilus_file_get_display_name (file);

        g_strv_builder_add (strv_builder, name);
    }

    return g_strv_builder_end (strv_builder);
}
