/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-file-changes.h"

#include "nautilus-file.h"
#include "nautilus-signal-utilities.h"

typedef struct
{
    NautilusFileChange type;
    GFile *location;
} Change;

typedef struct
{
    NautilusFileChange type;
    GFile *location_from;
    GFile *location_to;
} MoveChange;

static void move_change_free (MoveChange *change);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MoveChange, move_change_free)

static void
move_change_free (MoveChange *change)
{
    g_clear_object (&change->location_to);
    g_clear_object (&change->location_from);
    g_free (change);
}

static void
emit_signal_for_change (Change *change)
{
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusFile) parent = NULL;

    file = nautilus_file_new (change->location);
    if (file == NULL)
    {
        return;
    }
    parent = nautilus_file_get_parent (file);

    switch (change->type)
    {
        case NAUTILUS_FILE_CHANGE_RENAMED:
        {
            g_autoptr (MoveChange) move_change = NULL;

            move_change = (MoveChange *) change;

            nautilus_emit_signal_in_main_context_by_name (file,
                                                          NULL,
                                                          "renamed",
                                                          move_change->location_to);

            if (parent == NULL)
            {
                break;
            }

            nautilus_emit_signal_in_main_context_by_name (parent,
                                                          NULL,
                                                          "children-changed");
        }
        break;

        case NAUTILUS_FILE_CHANGE_MOVED:
        {
        }
        break;
    }
}

static void
notify_file_moved_or_renamed (GFile    *from,
                              GFile    *to,
                              gboolean  move_is_rename)
{
    MoveChange *change;

    change = g_new0 (MoveChange, 1);

    change->type = move_is_rename? NAUTILUS_FILE_CHANGE_RENAMED
                                 : NAUTILUS_FILE_CHANGE_MOVED;
    change->location_from = g_object_ref (from);
    change->location_to = g_object_ref (to);

    emit_signal_for_change ((Change *) change);
}

void
nautilus_notify_file_renamed (GFile *location,
                              GFile *new_location)
{
    notify_file_moved_or_renamed (location, new_location, TRUE);
}
