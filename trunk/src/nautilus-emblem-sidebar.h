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
 *
 *  This is the header file for the index panel widget, which displays oversidebar information
 *  in a vertical panel and hosts the meta-sidebars.
 */

#ifndef NAUTILUS_EMBLEM_SIDEBAR_H
#define NAUTILUS_EMBLEM_SIDEBAR_H

#include <gtk/gtkvbox.h>

#define NAUTILUS_TYPE_EMBLEM_SIDEBAR \
	(nautilus_emblem_sidebar_get_type ())
#define NAUTILUS_EMBLEM_SIDEBAR(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_EMBLEM_SIDEBAR, NautilusEmblemSidebar))
#define NAUTILUS_EMBLEM_SIDEBAR_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_EMBLEM_SIDEBAR, NautilusEmblemSidebarClass))
#define NAUTILUS_IS_EMBLEM_SIDEBAR(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_EMBLEM_SIDEBAR))
#define NAUTILUS_IS_EMBLEM_SIDEBAR_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_EMBLEM_SIDEBAR))

#define NAUTILUS_EMBLEM_SIDEBAR_ID "NautilusEmblemSidebar"

typedef struct NautilusEmblemSidebarDetails NautilusEmblemSidebarDetails;

typedef struct {
	GtkVBox parent_slot;
	NautilusEmblemSidebarDetails *details;
} NautilusEmblemSidebar;

typedef struct {
	GtkVBoxClass parent_slot;
	
} NautilusEmblemSidebarClass;

GType	nautilus_emblem_sidebar_get_type     (void);
void    nautilus_emblem_sidebar_register     (void);

#endif /* NAUTILUS_EMBLEM_SIDEBAR_H */
