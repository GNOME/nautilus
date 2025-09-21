/* nautilus-pathbar.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * 
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_PATH_BAR (nautilus_path_bar_get_type ())
G_DECLARE_FINAL_TYPE (NautilusPathBar, nautilus_path_bar, NAUTILUS, PATH_BAR, GtkBox)

void     nautilus_path_bar_set_path                       (NautilusPathBar *path_bar,
                                                           GFile           *file);

void     nautilus_path_bar_set_extensions_background_menu (NautilusPathBar *path_bar,
                                                           GMenuModel      *menu);

void     nautilus_path_bar_show_current_location_menu     (NautilusPathBar *path_bar);
