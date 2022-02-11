
/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Michael Meeks <michael@ximian.com>
*/

#pragma once

#include "nautilus-canvas-container.h"

#define NAUTILUS_TYPE_CANVAS_VIEW_CONTAINER nautilus_canvas_view_container_get_type()

G_DECLARE_FINAL_TYPE (NautilusCanvasViewContainer, nautilus_canvas_view_container,
                      NAUTILUS, CANVAS_VIEW_CONTAINER,
                      NautilusCanvasContainer)

NautilusCanvasContainer *nautilus_canvas_view_container_construct (NautilusCanvasViewContainer *canvas_container,
                                                                   NautilusCanvasView      *view);
NautilusCanvasContainer *nautilus_canvas_view_container_new       (NautilusCanvasView      *view);
