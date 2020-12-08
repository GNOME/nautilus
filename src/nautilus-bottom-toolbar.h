/* nautilus-bottom-toolbar.h
 *
 * Copyright 2020 Christopher Davis <christopherdavis@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_BOTTOM_TOOLBAR nautilus_bottom_toolbar_get_type()

G_DECLARE_FINAL_TYPE (NautilusBottomToolbar, nautilus_bottom_toolbar, NAUTILUS, BOTTOM_TOOLBAR, GtkRevealer)

GtkWidget *nautilus_bottom_toolbar_new                         (void);
gboolean   nautilus_bottom_toolbar_is_operations_button_active (NautilusBottomToolbar *toolbar);
void      *nautilus_bottom_toolbar_on_window_contructed        (NautilusBottomToolbar *toolbar);

G_END_DECLS
