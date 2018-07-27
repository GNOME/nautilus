/* nautilus-other-locations-window-slot.c
 *
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-other-locations-window-slot.h"

#include "nautilus-file.h"
#include "nautilus-places-view.h"
#include "nautilus-view.h"

struct _NautilusOtherLocationsWindowSlot
{
    NautilusWindowSlot parent_instance;
};

G_DEFINE_TYPE (NautilusOtherLocationsWindowSlot, nautilus_other_locations_window_slot, NAUTILUS_TYPE_WINDOW_SLOT)

static gboolean
real_handles_location (NautilusWindowSlot *self,
                       GFile              *location)
{
    NautilusFile *file;
    gboolean handles_location;

    file = nautilus_file_get (location);
    handles_location = nautilus_file_is_other_locations (file);
    nautilus_file_unref (file);

    return handles_location;
}

static NautilusView *
real_get_view_for_location (NautilusWindowSlot *self,
                            GFile              *location)
{
    return NAUTILUS_VIEW (nautilus_places_view_new ());
}

NautilusOtherLocationsWindowSlot *
nautilus_other_locations_window_slot_new (NautilusWindow *window)
{
    return g_object_new (NAUTILUS_TYPE_OTHER_LOCATIONS_WINDOW_SLOT,
                         "window", window,
                         NULL);
}

static void
nautilus_other_locations_window_slot_class_init (NautilusOtherLocationsWindowSlotClass *klass)
{
    NautilusWindowSlotClass *parent_class = NAUTILUS_WINDOW_SLOT_CLASS (klass);

    parent_class->get_view_for_location = real_get_view_for_location;
    parent_class->handles_location = real_handles_location;
}

static void
nautilus_other_locations_window_slot_init (NautilusOtherLocationsWindowSlot *self)
{
    GAction *action;
    GActionGroup *action_group;

    /* Disable the ability to change between types of views */
    action_group = gtk_widget_get_action_group (GTK_WIDGET (self), "slot");

    action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "files-view-mode-toggle");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}
