/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-horizontal-splitter.c - A horizontal splitter with a semi gradient look

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "nautilus-horizontal-splitter.h"
#include "nautilus-gtk-macros.h"

struct _NautilusHorizontalSplitterDetail
{
	gint	fixme;
};

/* NautilusHorizontalSplitterClass methods */
static void nautilus_horizontal_splitter_initialize_class (NautilusHorizontalSplitterClass *horizontal_splitter_class);
static void nautilus_horizontal_splitter_initialize       (NautilusHorizontalSplitter      *horizontal_splitter);



/* GtkObjectClass methods */
static void nautilus_horizontal_splitter_destroy          (GtkObject                       *object);


/* GtkWidgetClass methods */
static void nautilus_horizontal_splitter_draw             (GtkWidget                       *widget,
							   GdkRectangle                    *area);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusHorizontalSplitter,
				   nautilus_horizontal_splitter,
				   E_TYPE_HPANED);

/* GtkObjectClass methods */
static void
nautilus_horizontal_splitter_initialize_class (NautilusHorizontalSplitterClass *horizontal_splitter_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (horizontal_splitter_class);
	widget_class = GTK_WIDGET_CLASS (horizontal_splitter_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_horizontal_splitter_destroy;

	/* GtkWidgetClass */
	widget_class->draw = nautilus_horizontal_splitter_draw;
}

static void
nautilus_horizontal_splitter_initialize (NautilusHorizontalSplitter *horizontal_splitter)
{
	horizontal_splitter->detail = g_new (NautilusHorizontalSplitterDetail, 1);

	horizontal_splitter->detail->fixme = 666;

	e_paned_set_handle_size (E_PANED (horizontal_splitter), 7);
}

/* GtkObjectClass methods */
static void
nautilus_horizontal_splitter_destroy(GtkObject *object)
{
	NautilusHorizontalSplitter *horizontal_splitter;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_HORIZONTAL_SPLITTER (object));
	
	horizontal_splitter = NAUTILUS_HORIZONTAL_SPLITTER (object);

	g_free (horizontal_splitter->detail);
	
	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* GtkWidgetClass methods */
static void
nautilus_horizontal_splitter_draw (GtkWidget    *widget,
				   GdkRectangle *area)
{
	EPaned *paned;
	GdkRectangle handle_area, child_area;
	guint16 border_width;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_PANED (widget));

	if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)) {
		paned = E_PANED (widget);
		border_width = GTK_CONTAINER (paned)->border_width;

		gdk_window_clear_area (widget->window,
				       area->x, area->y, area->width,
				       area->height);

		if (e_paned_handle_shown(paned)) {
			handle_area.x = paned->handle_xpos;
			handle_area.y = paned->handle_ypos;
			handle_area.width = paned->handle_size;
			handle_area.height = paned->handle_height;
	  
			if (gdk_rectangle_intersect (&handle_area, area, &child_area)) {
				child_area.x -= paned->handle_xpos;
				child_area.y -= paned->handle_ypos;
				
				gtk_paint_handle (widget->style,
						  paned->handle,
						  GTK_STATE_NORMAL,
						  GTK_SHADOW_NONE,
						  &child_area,
						  widget,
						  "paned",
						  0, 0, -1, -1,
						  GTK_ORIENTATION_VERTICAL);
				
			}
		}

		/* Redraw the children
		 */
		if (paned->child1 && gtk_widget_intersect (paned->child1, area, &child_area)) {
			gtk_widget_draw(paned->child1, &child_area);
		}

		if (paned->child2 && gtk_widget_intersect(paned->child2, area, &child_area)) {
			gtk_widget_draw(paned->child2, &child_area);
		}
	}
}

/* NautilusHorizontalSplitter public methods */
GtkWidget*
nautilus_horizontal_splitter_new (void)
{
	NautilusHorizontalSplitter *horizontal_splitter;

	horizontal_splitter = gtk_type_new (nautilus_horizontal_splitter_get_type ());

	return GTK_WIDGET (horizontal_splitter);
}
