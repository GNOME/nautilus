/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 *  This is the header file for the index panel widget, which displays overview information
 *  in a vertical panel and hosts the meta-views.
 */

#ifndef NAUTILUS_SIDEBAR_H
#define NAUTILUS_SIDEBAR_H

#include <gtk/gtkeventbox.h>
#include "nautilus-view-frame.h"

#define NAUTILUS_TYPE_SIDEBAR \
	(nautilus_sidebar_get_type ())
#define NAUTILUS_SIDEBAR(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SIDEBAR, NautilusSidebar))
#define NAUTILUS_SIDEBAR_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SIDEBAR, NautilusSidebarClass))
#define NAUTILUS_IS_SIDEBAR(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SIDEBAR))
#define NAUTILUS_IS_SIDEBAR_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SIDEBAR))

typedef struct NautilusSidebarDetails NautilusSidebarDetails;

typedef struct {
	GtkEventBox parent_slot;
	NautilusSidebarDetails *details;
} NautilusSidebar;

typedef struct {
	GtkEventBoxClass parent_slot;
	
	void (*location_changed) (NautilusSidebar *sidebar,
				  const char *location);
} NautilusSidebarClass;

GtkType          nautilus_sidebar_get_type     (void);
NautilusSidebar *nautilus_sidebar_new          (void);
void             nautilus_sidebar_add_panel    (NautilusSidebar   *sidebar,
						NautilusViewFrame *panel);
GtkWidget 	*nautilus_sidebar_create_context_menu (NautilusSidebar *sidebar);

void		 nautilus_sidebar_hide_active_panel_if_matches (NautilusSidebar *sidebar,
							  	const char *sidebar_id);

void             nautilus_sidebar_remove_panel (NautilusSidebar   *sidebar,
						NautilusViewFrame *panel);
void             nautilus_sidebar_set_uri      (NautilusSidebar   *sidebar,
						const char        *new_uri,
						const char        *initial_title);
void             nautilus_sidebar_set_title    (NautilusSidebar   *sidebar,
						const char        *new_title);

#endif /* NAUTILUS_SIDEBAR_H */
