/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Mathieu Lacage <mathieu@eazel.com>
 */


#ifndef NAUTILUS_TREE_VIEW_DND_H
#define NAUTILUS_TREE_VIEW_DND_H

#include <eel/eel-dnd.h>
#include "nautilus-tree-view-private.h"

struct NautilusTreeViewDndDetails {

	EelDragInfo *drag_info;

	/* data setup by button_press signal for dragging */

	/* coordinates of the begening of a drag/press event */
	int press_x, press_y;
	/* used to remember between press/release that we pressed 
	   a hot spot. */
	gboolean pressed_hot_spot;
	/* used to remember between motion events that we are 
	   tracking a drag. */
	gboolean drag_pending;
	/* used to remmeber in motion_notify events which buton 
	   was pressed. */
	guint pressed_button;

	/* data used by the drag_motion code */
	GSList *expanded_nodes;

	/* row being highlighted */
	EelCTreeNode *current_prelighted_node;
	GtkStyle *normal_style;
	GtkStyle *highlight_style;
};

#endif /* NAUTILUS_TREE_VIEW_DND_H */

