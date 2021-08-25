/* nautilus-view-icon-ui.h
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

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view-icon-controller.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ICON_UI (nautilus_view_icon_ui_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewIconUi, nautilus_view_icon_ui, NAUTILUS, VIEW_ICON_UI, GtkFlowBox)

NautilusViewIconUi * nautilus_view_icon_ui_new (NautilusViewIconController *controller);

G_END_DECLS
