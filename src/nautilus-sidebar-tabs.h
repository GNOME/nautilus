/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/*
 * This is the header file for the tabs widget for the index panel.
 */

#ifndef NAUTILUS_SIDEBAR_TABS_H
#define NAUTILUS_SIDEBAR_TABS_H

#include <gtk/gtkmisc.h>

#define NAUTILUS_TYPE_SIDEBAR_TABS            (nautilus_sidebar_tabs_get_type ())
#define NAUTILUS_SIDEBAR_TABS(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SIDEBAR_TABS, NautilusSidebarTabs))
#define NAUTILUS_SIDEBAR_TABS_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SIDEBAR_TABS, NautilusSidebarTabsClass))
#define NAUTILUS_IS_SIDEBAR_TABS(obj)	      (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SIDEBAR_TABS))
#define NAUTILUS_IS_SIDEBAR_TABS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SIDEBAR_TABS))

typedef struct NautilusSidebarTabsDetails NautilusSidebarTabsDetails;

typedef struct
{
	GtkMisc base_slot;
	NautilusSidebarTabsDetails *details;  
} NautilusSidebarTabs;

typedef struct
{
	GtkMiscClass base_slot;
} NautilusSidebarTabsClass;

GtkType    nautilus_sidebar_tabs_get_type              (void);
GtkWidget *nautilus_sidebar_tabs_new                   (void);
gboolean   nautilus_sidebar_tabs_add_view              (NautilusSidebarTabs *sidebar_tabs,
							const char          *name,
							GtkWidget           *new_view,
							int                  page_number);
void	   nautilus_sidebar_tabs_connect_view 	       (NautilusSidebarTabs *sidebar_tabs,
							GtkWidget *view);

char *     nautilus_sidebar_tabs_get_title_from_index  (NautilusSidebarTabs *sidebar_tabs,
							int                  which_tab);
int        nautilus_sidebar_tabs_hit_test              (NautilusSidebarTabs *sidebar_tabs,
							int                  x,
							int                  y);
void       nautilus_sidebar_tabs_set_color             (NautilusSidebarTabs *sidebar_tabs,
							const char          *color_spec);
void       nautilus_sidebar_tabs_receive_dropped_color (NautilusSidebarTabs *sidebar_tabs,
							int                  x,
							int                  y,
							GtkSelectionData    *selection_data);
void       nautilus_sidebar_tabs_remove_view           (NautilusSidebarTabs *sidebar_tabs,
							const char          *name);
void       nautilus_sidebar_tabs_prelight_tab          (NautilusSidebarTabs *sidebar_tabs,
							int                  which_tab);
void       nautilus_sidebar_tabs_select_tab            (NautilusSidebarTabs *sidebar_tabs,
							int                  which_tab);
void       nautilus_sidebar_tabs_set_title             (NautilusSidebarTabs *sidebar_tabs,
							const char          *new_title);
void       nautilus_sidebar_tabs_set_title_mode        (NautilusSidebarTabs *sidebar_tabs,
							gboolean             is_title_mode);
void       nautilus_sidebar_tabs_set_visible           (NautilusSidebarTabs *sidebar_tabs,
							const char          *name,
							gboolean             is_visible);

#endif /* NAUTILUS_SIDEBAR_TABS_H */
