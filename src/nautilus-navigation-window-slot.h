/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-slot.h: Nautilus navigation window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NAUTILUS_NAVIGATION_WINDOW_SLOT_H
#define NAUTILUS_NAVIGATION_WINDOW_SLOT_H

#include "nautilus-window-slot.h"

typedef struct NautilusNavigationWindowSlot NautilusNavigationWindowSlot;
typedef struct NautilusNavigationWindowSlotClass NautilusNavigationWindowSlotClass;


#define NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT         (nautilus_navigation_window_slot_get_type())
#define NAUTILUS_NAVIGATION_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_NAVIGATION_WINDOW_SLOT_CLASS, NautilusNavigationWindowSlotClass))
#define NAUTILUS_NAVIGATION_WINDOW_SLOT(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT, NautilusNavigationWindowSlot))
#define NAUTILUS_IS_NAVIGATION_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT))
#define NAUTILUS_IS_NAVIGATION_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT))
#define NAUTILUS_NAVIGATION_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_NAVIGATION_WINDOW_SLOT, NautilusNavigationWindowSlotClass))
  
typedef enum {
	NAUTILUS_BAR_PATH,
	NAUTILUS_BAR_NAVIGATION,
	NAUTILUS_BAR_SEARCH
} NautilusBarMode;

struct NautilusNavigationWindowSlot {
	NautilusWindowSlot parent;

	NautilusBarMode bar_mode;
	GtkTreeModel *viewer_model;
	int num_viewers;

	/* Back/Forward chain, and history list. 
	 * The data in these lists are NautilusBookmark pointers. 
	 */
	GList *back_list, *forward_list;

	/* Current views stuff */
	GList *sidebar_panels;
};

struct NautilusNavigationWindowSlotClass {
	NautilusWindowSlotClass parent;
};

GType nautilus_navigation_window_slot_get_type (void);

gboolean nautilus_navigation_window_slot_should_close_with_mount (NautilusNavigationWindowSlot *slot,
								  GMount *mount);

void nautilus_navigation_window_slot_clear_forward_list (NautilusNavigationWindowSlot *slot);
void nautilus_navigation_window_slot_clear_back_list    (NautilusNavigationWindowSlot *slot);

#endif /* NAUTILUS_NAVIGATION_WINDOW_SLOT_H */
