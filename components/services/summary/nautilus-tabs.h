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
 * This is the header file for an image-based folder tabs widget.
 */

#ifndef NAUTILUS_TABS_H
#define NAUTILUS_TABS_H

#include <gtk/gtkdrawingarea.h>

#define NAUTILUS_TYPE_TABS	(nautilus_tabs_get_type ())
#define NAUTILUS_TABS(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_TABS, NautilusTabs))
#define NAUTILUS_TABS_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TABS, NautilusTabsClass))
#define NAUTILUS_IS_TABS(obj)	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_TABS))
#define NAUTILUS_IS_TABS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TABS))

typedef struct NautilusTabsDetails NautilusTabsDetails;

typedef struct
{
	GtkDrawingArea parent;
	NautilusTabsDetails *details;  
} NautilusTabs;

typedef struct
{
	GtkDrawingAreaClass parent_class;

	void (*tab_selected)		(NautilusTabs *tabs, int which_tab);
} NautilusTabsClass;

GtkType    nautilus_tabs_get_type              (void);
GtkWidget *nautilus_tabs_new                   (void);
gboolean   nautilus_tabs_add_tab               (NautilusTabs *tabs,
							const char          *name,
							int                  page_number);
void       nautilus_tabs_remove_tab            (NautilusTabs *tabs,
							const char          *name);

#endif /* NAUTILUS_TABS_H */
