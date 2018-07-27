/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include "nautilus-files-window-slot.h"

#include "nautilus-enums.h"
#include "nautilus-files-view.h"
#include "nautilus-global-preferences.h"
#include "nautilus-view.h"
#include "nautilus-window-slot-private.h"
#include "nautilus-window.h"

struct _NautilusFilesWindowSlot
{
    NautilusWindowSlot parent_instance;
};

G_DEFINE_TYPE (NautilusFilesWindowSlot, nautilus_files_window_slot,
               NAUTILUS_TYPE_WINDOW_SLOT)

enum
{
    SET_MODE,
    LAST_SIGNAL,
};

static unsigned int signals[LAST_SIGNAL];

static void
nautilus_files_window_slot_init (NautilusFilesWindowSlot *self)
{
}

static void
nautilus_files_window_slot_set_mode (NautilusFilesWindowSlot *self,
                                     unsigned int             mode)
{
    NautilusWindowSlot *slot;
    NautilusView *view;
    unsigned int current_mode;
    gboolean searching;
    const char *key;

    slot = NAUTILUS_WINDOW_SLOT (self);
    view = nautilus_window_slot_get_current_view (slot);
    if (!NAUTILUS_IS_FILES_VIEW (view))
    {
        return;
    }
    current_mode = nautilus_files_view_get_view_id (view);
    if (current_mode == mode)
    {
        return;
    }
    searching = nautilus_view_is_searching (view);
    if (searching)
    {
        key = NAUTILUS_PREFERENCES_SEARCH_VIEW;
    }
    else
    {
        key = NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER;
    }

    nautilus_window_slot_set_mode (slot, mode);

    g_settings_set_enum (nautilus_preferences, key, mode);
}

static void
nautilus_files_window_slot_class_init (NautilusFilesWindowSlotClass *klass)
{
    GtkBindingSet *binding_set;

    signals[SET_MODE] = g_signal_new ("set-mode",
                                      G_OBJECT_CLASS_TYPE (klass),
                                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1,
                                      G_TYPE_UINT);

    g_signal_override_class_handler ("set-mode", G_OBJECT_CLASS_TYPE (klass),
                                     G_CALLBACK (nautilus_files_window_slot_set_mode));

    binding_set = gtk_binding_set_by_class (klass);

    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KEY_1, GDK_CONTROL_MASK,
                                  "set-mode",
                                  1,
                                  G_TYPE_UINT, NAUTILUS_VIEW_LIST_ID);
    gtk_binding_entry_add_signal (binding_set,
                                  GDK_KEY_2, GDK_CONTROL_MASK,
                                  "set-mode",
                                  1,
                                  G_TYPE_UINT, NAUTILUS_VIEW_GRID_ID);
}

NautilusWindowSlot *
nautilus_files_window_slot_new_for_window (NautilusWindow *window)
{
    NautilusFilesWindowSlot *slot;

    g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

    slot = g_object_new (NAUTILUS_TYPE_FILES_WINDOW_SLOT,
                         "window", window,
                         NULL);

    return NAUTILUS_WINDOW_SLOT (slot);
}
