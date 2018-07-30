
/* Nautilus - Canvas item for floating selection.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * Copyright (C) 2011 Red Hat Inc.
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
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <eel/eel-canvas.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SELECTION_CANVAS_ITEM nautilus_selection_canvas_item_get_type ()
G_DECLARE_FINAL_TYPE (NautilusSelectionCanvasItem, nautilus_selection_canvas_item,
                      NAUTILUS, SELECTION_CANVAS_ITEM,
                      EelCanvasItem)

G_END_DECLS
