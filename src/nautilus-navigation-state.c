/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nautilus-navigation-state.h"

#include <glib-object.h>

struct _NautilusNavigationState
{
    /* List<GFile> with most recently visited sorted first */
    GList *back_list;
    /* List<GFile> with most recently visited sorted first */
    GList *forward_list;
    GFile *current_location;
    NautilusQuery *current_search_query;
};

G_DEFINE_FINAL_TYPE (NautilusNavigationState, nautilus_navigation_state, G_TYPE_OBJECT)

void
free_navigation_state (gpointer data)
{
    NautilusNavigationState *navigation_state = data;

    g_list_free_full (navigation_state->back_list, g_object_unref);
    g_list_free_full (navigation_state->forward_list, g_object_unref);
    g_clear_object (&navigation_state->current_location_bookmark);
    g_clear_object (&navigation_state->current_search_query);

    g_free (navigation_state);
}


static void
nautilus_navigation_state_init (NautilusWindowSlot *self)
{
}

static void
nautilus_navigation_state_dispose (GObject *object)
{
}

static void
nautilus_navigation_state_finalize (GObject *object)
{
    NautilusWindowSlot *self;
    self = NAUTILUS_WINDOW_SLOT (object);
    g_clear_pointer (&self->title, g_free);

    G_OBJECT_CLASS (nautilus_navigation_state_parent_class)->finalize (object);
}

static void
nautilus_navigation_state_class_init (NautilusWindowSlotClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    oclass->dispose = nautilus_navigation_state_dispose;
    oclass->finalize = nautilus_navigation_state_finalize;
}

NautilusNavigationState *
nautilus_navigation_state_new (void)
{
    return g_object_new (NAUTILUS_TYPE_NAVIGATION_STATE, NULL);
}

NautilusNavigationState *
nautilus_navigation_state_copy (NautilusNavigationState *orig)
{
    NautilusNavigationState *state = nautilus_navigation_state_new ();
    
    //TODO state->;
}
