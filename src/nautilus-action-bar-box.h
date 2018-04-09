/* nautilus-action-bar.h
 *
 * Copyright (C) 2018 Carlos Soriano <csoriano@redhat.com>
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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_ACTION_BAR_BOX (nautilus_action_bar_box_get_type())

G_DECLARE_FINAL_TYPE (NautilusActionBarBox, nautilus_action_bar_box, NAUTILUS, ACTION_BAR_BOX, GtkBox)

NautilusActionBarBox *nautilus_action_bar_box_new (void);

G_END_DECLS
