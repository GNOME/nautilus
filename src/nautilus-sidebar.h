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

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include "nautilus.h"
#include <libnautilus/nautilus-background.h>

typedef struct _NautilusIndexPanel NautilusIndexPanel;
typedef struct _NautilusIndexPanelClass  NautilusIndexPanelClass;

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

struct _NautilusIndexPanel
{
	GtkEventBox event_box;
	GtkWidget *index_container;
	GtkWidget *per_uri_container;
	GtkWidget *meta_tabs;
	gchar *uri;
	NautilusBackground *background;
};

struct _NautilusIndexPanelClass
{
	GtkEventBoxClass parent_class;
};

GtkType             nautilus_index_panel_get_type         (void);
NautilusIndexPanel *nautilus_index_panel_new              (void);
void                nautilus_index_panel_add_meta_view    (NautilusIndexPanel *panel,
							   NautilusView       *meta_view);
void                nautilus_index_panel_remove_meta_view (NautilusIndexPanel *panel,
							   NautilusView       *meta_view);
void                nautilus_index_panel_set_uri          (NautilusIndexPanel *panel,
							   const gchar        *new_uri);

#endif /* NTL_INDEX_PANEL_H */
