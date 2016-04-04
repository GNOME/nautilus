/* nautilus-desktop-canvas-view-container.c
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

#include "nautilus-desktop-canvas-view-container.h"

struct _NautilusDesktopCanvasViewContainer
{
  NautilusCanvasViewContainer parent_instance;
};

G_DEFINE_TYPE (NautilusDesktopCanvasViewContainer, nautilus_desktop_canvas_view_container, NAUTILUS_TYPE_CANVAS_VIEW_CONTAINER)

NautilusDesktopCanvasViewContainer *
nautilus_desktop_canvas_view_container_new (void)
{
  return g_object_new (NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW_CONTAINER, NULL);
}

static void
nautilus_desktop_canvas_view_container_class_init (NautilusDesktopCanvasViewContainerClass *klass)
{
}

static void
nautilus_desktop_canvas_view_container_init (NautilusDesktopCanvasViewContainer *self)
{
}
