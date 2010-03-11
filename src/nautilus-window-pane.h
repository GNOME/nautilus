/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-pane.h: Nautilus window pane

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

   Author: Holger Berndt <berndth@gmx.de>
*/

#ifndef NAUTILUS_WINDOW_PANE_H
#define NAUTILUS_WINDOW_PANE_H

#include "nautilus-window.h"

#define NAUTILUS_TYPE_WINDOW_PANE	 (nautilus_window_pane_get_type())
#define NAUTILUS_WINDOW_PANE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_WINDOW_PANE, NautilusWindowPaneClass))
#define NAUTILUS_WINDOW_PANE(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW_PANE, NautilusWindowPane))
#define NAUTILUS_IS_WINDOW_PANE(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW_PANE))
#define NAUTILUS_IS_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_WINDOW_PANE))
#define NAUTILUS_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_WINDOW_PANE, NautilusWindowPaneClass))

typedef struct _NautilusWindowPaneClass NautilusWindowPaneClass;

struct _NautilusWindowPaneClass {
	GObjectClass parent_class;

	void (*show) (NautilusWindowPane *pane);
	void (*set_active) (NautilusWindowPane *pane,
			    gboolean is_active);
	void (*sync_search_widgets) (NautilusWindowPane *pane);
	void (*sync_location_widgets) (NautilusWindowPane *pane);
};

/* A NautilusWindowPane is a layer between a slot and a window.
 * Each slot is contained in one pane, and each pane can contain
 * one or more slots. It also supports the notion of an "active slot".
 * On the other hand, each pane is contained in a window, while each
 * window can contain one or multiple panes. Likewise, the window has
 * the notion of an "active pane".
 *
 * A spatial window has only one pane, which contains a single slot.
 * A navigation window may have one or more panes.
 */
struct _NautilusWindowPane {
	GObject parent;

	/* hosting window */
	NautilusWindow *window;
	gboolean visible;

	/* available slots, and active slot.
	 * Both of them may never be NULL. */
	GList *slots;
	GList *active_slots;
	NautilusWindowSlot *active_slot;

	/* whether or not this pane is active */
	gboolean is_active;
};

GType nautilus_window_pane_get_type (void);
NautilusWindowPane *nautilus_window_pane_new (NautilusWindow *window);


void nautilus_window_pane_show (NautilusWindowPane *pane);
void nautilus_window_pane_zoom_in (NautilusWindowPane *pane);
void nautilus_window_pane_zoom_to_level (NautilusWindowPane *pane, NautilusZoomLevel level);
void nautilus_window_pane_zoom_out (NautilusWindowPane *pane);
void nautilus_window_pane_zoom_to_default (NautilusWindowPane *pane);
void nautilus_window_pane_sync_location_widgets (NautilusWindowPane *pane);
void nautilus_window_pane_sync_search_widgets  (NautilusWindowPane *pane);
void nautilus_window_pane_set_active (NautilusWindowPane *pane, gboolean is_active);
void nautilus_window_pane_slot_close (NautilusWindowPane *pane, NautilusWindowSlot *slot);

NautilusWindowSlot* nautilus_window_pane_get_slot_for_content_box (NautilusWindowPane *pane, GtkWidget *content_box);
void nautilus_window_pane_switch_to (NautilusWindowPane *pane);
void nautilus_window_pane_grab_focus (NautilusWindowPane *pane);


#endif /* NAUTILUS_WINDOW_PANE_H */
