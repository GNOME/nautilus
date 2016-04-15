/* nautilus-window-desktop-slot.h
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
#ifndef NAUTILUS_DESKTOP_WINDOW_SLOT_H
#define NAUTILUS_DESKTOP_WINDOW_SLOT_H

#include <src/nautilus-window-slot.h>
#include <src/nautilus-window.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_DESKTOP_WINDOW_SLOT (nautilus_desktop_window_slot_get_type())

G_DECLARE_FINAL_TYPE (NautilusDesktopWindowSlot, nautilus_desktop_window_slot, NAUTILUS, DESKTOP_WINDOW_SLOT, NautilusWindowSlot)


NautilusDesktopWindowSlot *nautilus_desktop_window_slot_new (NautilusWindow *window);

G_END_DECLS

#endif /* NAUTILUS_DESKTOP_WINDOW_SLOT_H */

