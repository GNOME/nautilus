/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus News Item Cell Renderer
 *
 *  Copyright (C) 2000  Red Hat, Inc., Ximian Inc., Jonathan Blandford
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Dave Camp <dave@ximian.com> 
 *          based on the text cell renderer by Jonathan Blandford
 *          <jrb@redhat.com>
 *
 */

#ifndef NAUTILUS_CELL_RENDERER_NEWS_ITEM_H
#define NAUTILUS_CELL_RENDERER_NEWS_ITEM_H

#include <gtk/gtkcellrenderer.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_CELL_RENDERER_NEWS		(nautilus_cell_renderer_news_get_type ())
#define NAUTILUS_CELL_RENDERER_NEWS(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CELL_RENDERER_NEWS, NautilusCellRendererNews))
#define NAUTILUS_CELL_RENDERER_NEWS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CELL_RENDERER_NEWS, NautilusCellRendererNewsClass))
#define NAUTILUS_IS_CELL_RENDERER_NEWS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CELL_RENDERER_NEWS))
#define NAUTILUS_IS_CELL_RENDERER_NEWS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CELL_RENDERER_NEWS))

typedef struct _NautilusCellRendererNews        NautilusCellRendererNews;
typedef struct _NautilusCellRendererNewsClass   NautilusCellRendererNewsClass;
typedef struct _NautilusCellRendererNewsPrivate NautilusCellRendererNewsPrivate;

struct _NautilusCellRendererNews
{
	GtkCellRenderer parent;
	
	NautilusCellRendererNewsPrivate *priv;
};

struct _NautilusCellRendererNewsClass
{
	GtkCellRendererClass parent_class;
};

GType            nautilus_cell_renderer_news_get_type (void);
GtkCellRenderer *nautilus_cell_renderer_news_new      (void);

G_END_DECLS

#endif /* __GTK_CELL_RENDERER_NEWS_H__ */
