/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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

#ifndef NTL_INDEX_PANEL_H
#define NTL_INDEX_PANEL_H

#include <gtk/gtkeventbox.h>
#include "ntl-view.h"

typedef struct NautilusIndexPanel NautilusIndexPanel;
typedef struct NautilusIndexPanelClass  NautilusIndexPanelClass;

#define NAUTILUS_TYPE_INDEX_PANEL \
	(nautilus_index_panel_get_type ())
#define NAUTILUS_INDEX_PANEL(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INDEX_PANEL, NautilusIndexPanel))
#define NAUTILUS_INDEX_PANEL_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INDEX_PANEL, NautilusIndexPanelClass))
#define NAUTILUS_IS_INDEX_PANEL(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INDEX_PANEL))
#define NAUTILUS_IS_INDEX_PANEL_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INDEX_PANEL))

typedef struct NautilusIndexPanelDetails NautilusIndexPanelDetails;

struct NautilusIndexPanel
{
	GtkEventBox event_box;
	NautilusIndexPanelDetails *details;
};

struct NautilusIndexPanelClass
{
	GtkEventBoxClass parent_class;
};

GtkType             nautilus_index_panel_get_type         (void);
NautilusIndexPanel *nautilus_index_panel_new              (void);
void                nautilus_index_panel_add_meta_view    (NautilusIndexPanel *panel,
							   NautilusViewFrame  *meta_view);
void                nautilus_index_panel_remove_meta_view (NautilusIndexPanel *panel,
							   NautilusViewFrame  *meta_view);
void                nautilus_index_panel_set_uri          (NautilusIndexPanel *panel,
							   const char         *new_uri,
							   const char         *initial_title);
void                nautilus_index_panel_set_title        (NautilusIndexPanel *panel,
							   const char         *new_title);

#endif /* NTL_INDEX_PANEL_H */
