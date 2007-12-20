/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*  nautilus-side-pane.c
 * 
 *  Copyright (C) 2002 Ximian, Inc.
 * 
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Dave Camp <dave@ximian.com>
 */

#ifndef NAUTILUS_SIDE_PANE_H
#define NAUTILUS_SIDE_PANE_H

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SIDE_PANE                (nautilus_side_pane_get_type ())
#define NAUTILUS_SIDE_PANE(obj)                (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SIDE_PANE, NautilusSidePane))
#define NAUTILUS_SIDE_PANE_CLASS(klass)        (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SIDE_PANE, NautilusSidePaneClass))
#define NAUTILUS_IS_SIDE_PANE(obj)             (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SIDE_PANE))
#define NAUTILUS_IS_SIDE_PANE_CLASS(klass)     (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SIDE_PANE))

typedef struct _NautilusSidePaneDetails NautilusSidePaneDetails;

typedef struct {
	GtkVBox parent;
	NautilusSidePaneDetails *details;
} NautilusSidePane;

typedef struct {
	GtkVBoxClass parent_slot;

	void (*close_requested) (NautilusSidePane *side_pane);
	void (*switch_page) (NautilusSidePane *side_pane,
			     GtkWidget *child);
} NautilusSidePaneClass;

GType                  nautilus_side_pane_get_type        (void);
NautilusSidePane      *nautilus_side_pane_new             (void);
void                   nautilus_side_pane_add_panel       (NautilusSidePane *side_pane,
							   GtkWidget        *widget,
							   const char       *title,
							   const char       *tooltip);
void                   nautilus_side_pane_remove_panel    (NautilusSidePane *side_pane,
							   GtkWidget        *widget);
void                   nautilus_side_pane_show_panel      (NautilusSidePane *side_pane,
							   GtkWidget        *widget);
void                   nautilus_side_pane_set_panel_image (NautilusSidePane *side_pane,
							   GtkWidget        *widget,
							   GdkPixbuf        *pixbuf);
GtkWidget             *nautilus_side_pane_get_current_panel (NautilusSidePane *side_pane);

G_END_DECLS

#endif /* NAUTILUS_SIDE_PANE_H */
