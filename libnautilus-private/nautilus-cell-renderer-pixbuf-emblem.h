/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-cell-renderer-pixbuf-emblem.h: cell renderer which can render
   an emblem on top of a pixbuf (for use in FMListView and FMTreeView)
 
   Copyright (C) 2003 Juerg Billeter
  
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

   This is based on GtkCellRendererPixbuf written by
   Jonathan Blandford <jrb@redhat.com>
  
   Author: Juerg Billeter <j@bitron.ch>
*/

#ifndef NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM_H
#define NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM nautilus_cell_renderer_pixbuf_emblem_get_type()
#define NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM, NautilusCellRendererPixbufEmblem))
#define NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM, NautilusCellRendererPixbufEmblemClass))
#define NAUTILUS_IS_CELL_RENDERER_PIXBUF_EMBLEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM))
#define NAUTILUS_IS_CELL_RENDERER_PIXBUF_EMBLEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM))
#define NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CELL_RENDERER_PIXBUF_EMBLEM, NautilusCellRendererPixbufEmblemClass))

typedef struct _NautilusCellRendererPixbufEmblem NautilusCellRendererPixbufEmblem;
typedef struct _NautilusCellRendererPixbufEmblemClass NautilusCellRendererPixbufEmblemClass;

struct _NautilusCellRendererPixbufEmblem {
	GtkCellRenderer	parent;
	
	/*< private >*/
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_expander_open;
	GdkPixbuf *pixbuf_expander_closed;
	GdkPixbuf *pixbuf_emblem;
};

struct _NautilusCellRendererPixbufEmblemClass {
	GtkCellRendererClass parent_class;
};

GType		 nautilus_cell_renderer_pixbuf_emblem_get_type (void);
GtkCellRenderer *nautilus_cell_renderer_pixbuf_emblem_new      (void);

#endif /* NAUTILUS_CELL_RENDERER_PIXBUF_EMBLEM_H */
