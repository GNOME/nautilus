/* nautilus-list-view-dnd.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#include <gdk/gdk.h>

#include "nautilus-list-view.h"

#include "nautilus-dnd.h"

void nautilus_list_view_dnd_init (NautilusListView *list_view);
void nautilus_list_view_dnd_drag_begin (NautilusListView *list_view,
                                        gdouble           offset_x,
                                        gdouble           offset_y,
                                        const GdkEvent   *event);
NautilusDragInfo *
nautilus_list_view_dnd_get_drag_source_data (NautilusListView *list_view);
