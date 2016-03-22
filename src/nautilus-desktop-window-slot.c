/* nautilus-desktop-window-slot.c
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

#include "nautilus-desktop-window-slot.h"

struct _NautilusDesktopWindowSlot
{
  NautilusWindowSlot parent_instance;
};

G_DEFINE_TYPE (NautilusDesktopWindowSlot, nautilus_desktop_window_slot, NAUTILUS_TYPE_WINDOW_SLOT)

NautilusDesktopWindowSlot *
nautilus_desktop_window_slot_new (NautilusWindow *window)
{
  return g_object_new (NAUTILUS_TYPE_DESKTOP_WINDOW_SLOT,
                       "window", window,
                       NULL);
}

static void
nautilus_desktop_window_slot_class_init (NautilusDesktopWindowSlotClass *klass)
{
}

static void
nautilus_desktop_window_slot_init (NautilusDesktopWindowSlot *self)
{
}
