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
#ifndef NAUTILUS_VIEW_ICON_UI_H
#define NAUTILUS_VIEW_ICON_UI_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view-icon-controller.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ICON_UI (nautilus_view_icon_ui_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewIconUi, nautilus_view_icon_ui, NAUTILUS, VIEW_ICON_UI, GtkFlowBox)

NautilusViewIconUi * nautilus_view_icon_ui_new (NautilusViewIconController *controller);
/* TODO: this should become the "nautilus_view_set_selection" once we have a proper
 * MVC also in the nautilus-view level. */
void nautilus_view_icon_ui_set_selection (NautilusViewIconUi *self,
                                          GQueue             *selection);

G_END_DECLS

#endif /* NAUTILUS_VIEW_ICON_UI_H */

