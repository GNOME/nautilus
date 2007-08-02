/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-cell-renderer-text-ellipsized.c: Cell renderer for text which
   will use pango ellipsization but deactivate it temporarily for the size
   calculation to get the size based on the actual text length.
 
   Copyright (C) 2007 Martin Wehner
  
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
  
   Author: Martin Wehner <martin.wehner@gmail.com>
*/

#include "nautilus-cell-renderer-text-ellipsized.h"

#define ELLIPSIZE_PROP "ellipsize"

static void nautilus_cell_renderer_text_ellipsized_init       (NautilusCellRendererTextEllipsizedClass	*cell);
static void nautilus_cell_renderer_text_ellipsized_class_init (NautilusCellRendererTextEllipsizedClass	*klass);
static void nautilus_cell_renderer_text_ellipsized_get_size   (GtkCellRenderer				*cell,
							       GtkWidget				*widget,
							       GdkRectangle				*rectangle,
							       gint					*x_offset,
							       gint					*y_offset,
							       gint					*width,
							       gint					*height);

static gpointer parent_class;

GType
nautilus_cell_renderer_text_ellipsized_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info =
		{
			sizeof (NautilusCellRendererTextEllipsizedClass),
			NULL,
			NULL,
			(GClassInitFunc) nautilus_cell_renderer_text_ellipsized_class_init,
			NULL,
			NULL,
			sizeof (NautilusCellRendererTextEllipsized),
			0,
			(GInstanceInitFunc) nautilus_cell_renderer_text_ellipsized_init
		};

		type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT,
					       "NautilusCellRendererTextEllipsized",
					       &info, 0);
	}

	return type;
}


static void
nautilus_cell_renderer_text_ellipsized_init (NautilusCellRendererTextEllipsizedClass *cell)
{
	g_object_set (cell, ELLIPSIZE_PROP, PANGO_ELLIPSIZE_END, NULL);
}

static void
nautilus_cell_renderer_text_ellipsized_class_init (NautilusCellRendererTextEllipsizedClass *klass)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	cell_class->get_size = nautilus_cell_renderer_text_ellipsized_get_size;
}

GtkCellRenderer *
nautilus_cell_renderer_text_ellipsized_new (void)
{
	return g_object_new (NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED, NULL);
}

static void
nautilus_cell_renderer_text_ellipsized_get_size (GtkCellRenderer *cell,
						 GtkWidget       *widget,
						 GdkRectangle    *cell_area,
						 gint            *x_offset,
						 gint            *y_offset,
						 gint            *width,
						 gint            *height)
{
	g_object_set (cell, ELLIPSIZE_PROP, PANGO_ELLIPSIZE_NONE, NULL);

	(* GTK_CELL_RENDERER_CLASS (parent_class)->get_size) (cell, widget, cell_area,
							      x_offset, y_offset,
							      width, height);

	g_object_set (cell, ELLIPSIZE_PROP, PANGO_ELLIPSIZE_END, NULL);
}

