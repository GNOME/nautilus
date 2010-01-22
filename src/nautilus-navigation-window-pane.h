/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-pane.h: Nautilus navigation window pane
 
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

#ifndef NAUTILUS_NAVIGATION_WINDOW_PANE_H
#define NAUTILUS_NAVIGATION_WINDOW_PANE_H

#include "nautilus-window-pane.h"
#include "nautilus-navigation-window-slot.h"

#define NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE     (nautilus_navigation_window_pane_get_type())
#define NAUTILUS_NAVIGATION_WINDOW_PANE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE, NautilusNavigationWindowPaneClass))
#define NAUTILUS_NAVIGATION_WINDOW_PANE(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE, NautilusNavigationWindowPane))
#define NAUTILUS_IS_NAVIGATION_WINDOW_PANE(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE))
#define NAUTILUS_IS_NAVIGATION_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE))
#define NAUTILUS_NAVIGATION_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE, NautilusNavigationWindowPaneClass))

typedef struct _NautilusNavigationWindowPaneClass NautilusNavigationWindowPaneClass;
typedef struct _NautilusNavigationWindowPane      NautilusNavigationWindowPane;

struct _NautilusNavigationWindowPaneClass {
	NautilusWindowPaneClass parent_class;
};

struct _NautilusNavigationWindowPane {
	NautilusWindowPane parent;

	GtkWidget *widget;

	/* location bar */
	GtkWidget *location_bar;
	GtkWidget *navigation_bar;
	GtkWidget *path_bar;
	GtkWidget *search_bar;

	gboolean temporary_navigation_bar;
	gboolean temporary_location_bar;
	gboolean temporary_search_bar;

	/* notebook */
	GtkWidget *notebook;

	/* split view */
	GtkWidget *split_view_hpane;
};

GType    nautilus_navigation_window_pane_get_type (void);

NautilusNavigationWindowPane* nautilus_navigation_window_pane_new (NautilusWindow *window);

/* location bar */
void     nautilus_navigation_window_pane_setup             (NautilusNavigationWindowPane *pane);

void     nautilus_navigation_window_pane_hide_location_bar (NautilusNavigationWindowPane *pane, gboolean save_preference);
void     nautilus_navigation_window_pane_show_location_bar (NautilusNavigationWindowPane *pane, gboolean save_preference);
gboolean nautilus_navigation_window_pane_location_bar_showing (NautilusNavigationWindowPane *pane);
void     nautilus_navigation_window_pane_hide_path_bar (NautilusNavigationWindowPane *pane);
void     nautilus_navigation_window_pane_show_path_bar (NautilusNavigationWindowPane *pane);
gboolean nautilus_navigation_window_pane_path_bar_showing (NautilusNavigationWindowPane *pane);
gboolean nautilus_navigation_window_pane_search_bar_showing (NautilusNavigationWindowPane *pane);
void     nautilus_navigation_window_pane_set_bar_mode  (NautilusNavigationWindowPane *pane, NautilusBarMode mode);
void     nautilus_navigation_window_pane_show_location_bar_temporarily (NautilusNavigationWindowPane *pane);
void     nautilus_navigation_window_pane_show_navigation_bar_temporarily (NautilusNavigationWindowPane *pane);
void     nautilus_navigation_window_pane_always_use_location_entry (NautilusNavigationWindowPane *pane, gboolean use_entry);
gboolean nautilus_navigation_window_pane_hide_temporary_bars (NautilusNavigationWindowPane *pane);
/* notebook */
void     nautilus_navigation_window_pane_add_slot_in_tab (NautilusNavigationWindowPane *pane, NautilusWindowSlot *slot, NautilusWindowOpenSlotFlags flags);
void     nautilus_navigation_window_pane_remove_page (NautilusNavigationWindowPane *pane, int page_num);

#endif /* NAUTILUS_NAVIGATION_WINDOW_PANE_H */
