
/*
 * Nautilus
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Dave Camp <dave@ximian.com>
 */

/* nautilus-tree-view-drag-dest.h: Handles drag and drop for treeviews which 
 *                                 contain a hierarchy of files
 */

#pragma once

#include <gtk/gtk.h>

#include "nautilus-file.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TREE_VIEW_DRAG_DEST nautilus_tree_view_drag_dest_get_type ()
G_DECLARE_FINAL_TYPE (NautilusTreeViewDragDest, nautilus_tree_view_drag_dest,
                      NAUTILUS, TREE_VIEW_DRAG_DEST,
                      GObject)

NautilusTreeViewDragDest *nautilus_tree_view_drag_dest_new (GtkTreeView *tree_view);

G_END_DECLS
