/* nautilus-desktop-canvas-view-container.h
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
#ifndef NAUTILUS_DESKTOP_CANVAS_VIEW_CONTAINER_H
#define NAUTILUS_DESKTOP_CANVAS_VIEW_CONTAINER_H

#include <glib.h>
#include "nautilus-canvas-view-container.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW_CONTAINER (nautilus_desktop_canvas_view_container_get_type())

G_DECLARE_FINAL_TYPE (NautilusDesktopCanvasViewContainer, nautilus_desktop_canvas_view_container, NAUTILUS, DESKTOP_CANVAS_VIEW_CONTAINER, NautilusCanvasViewContainer)

NautilusDesktopCanvasViewContainer *nautilus_desktop_canvas_view_container_new (void);

G_END_DECLS

#endif /* NAUTILUS_DESKTOP_CANVAS_VIEW_CONTAINER_H */

