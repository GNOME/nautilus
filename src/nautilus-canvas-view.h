/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-canvas-view.h - interface for canvas view of directory.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#pragma once

#include "nautilus-files-view.h"

#include "nautilus-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_CANVAS_VIEW nautilus_canvas_view_get_type()

G_DECLARE_FINAL_TYPE (NautilusCanvasView, nautilus_canvas_view, NAUTILUS, CANVAS_VIEW, NautilusFilesView)

int     nautilus_canvas_view_compare_files (NautilusCanvasView   *canvas_view,
					  NautilusFile *a,
					  NautilusFile *b);

NautilusFilesView * nautilus_canvas_view_new (NautilusWindowSlot *slot);

NautilusCanvasContainer * nautilus_canvas_view_get_canvas_container (NautilusCanvasView *view);

G_END_DECLS
