/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-viewport.c - A subclass of GtkViewport with non broken drawing.

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

#include "nautilus-viewport.h"

#include "nautilus-gtk-macros.h"

/* GtkObjectClass methods */
static void nautilus_viewport_initialize_class (NautilusViewportClass *viewport_class);
static void nautilus_viewport_initialize       (NautilusViewport      *viewport);
static void nautilus_viewport_draw             (GtkWidget             *widget,
						GdkRectangle          *area);

static void nautilus_viewport_paint            (GtkWidget             *widget,
						GdkRectangle          *area);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusViewport, nautilus_viewport, GTK_TYPE_VIEWPORT)

/* Class init methods */
static void
nautilus_viewport_initialize_class (NautilusViewportClass *viewport_class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (viewport_class);
	
	/* GtkWidgetClass */
	widget_class->draw = nautilus_viewport_draw;
}

void
nautilus_viewport_initialize (NautilusViewport *viewport)
{
	/* noop */
}

static void
nautilus_viewport_draw (GtkWidget *widget,
			GdkRectangle *area)
{
	GtkViewport *viewport;
	GtkBin *bin;
	GdkRectangle tmp_area;
	GdkRectangle child_area;
	gint border_width;
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_VIEWPORT (widget));
	g_return_if_fail (area != NULL);
	
	if (GTK_WIDGET_DRAWABLE (widget)) {
		viewport = GTK_VIEWPORT (widget);
		bin = GTK_BIN (widget);
		
		border_width = GTK_CONTAINER (widget)->border_width;
		
		tmp_area = *area;
		tmp_area.x -= border_width;
		tmp_area.y -= border_width;
		
		nautilus_viewport_paint (widget, &tmp_area);
		
		tmp_area.x += viewport->hadjustment->value - widget->style->klass->xthickness;
		tmp_area.y += viewport->vadjustment->value - widget->style->klass->ythickness;

		/* The gtk_viewport_draw() version does not adjust the width
		 * and height of the tmp_area for the class x/y thickness.  This
		 * causes some drawing to be clipped on the bottom.  This is a bug
		 * in GTK+.
		 */

		/* FIXME bugzilla.eazel.com xxxx: 
		 * Remove this widget once the fix makes it to GTK+.
		 */
		tmp_area.width += 2 * widget->style->klass->xthickness;
		tmp_area.height += 2 * widget->style->klass->ythickness;
		
		gtk_paint_flat_box(widget->style, viewport->bin_window, 
				   GTK_STATE_NORMAL, GTK_SHADOW_NONE,
				   &tmp_area, widget, "viewportbin",
				   0, 0, -1, -1);
		
		if (bin->child) {
			if (gtk_widget_intersect (bin->child, &tmp_area, &child_area)) {
				gtk_widget_draw (bin->child, &child_area);
			}
		}
	}
}

static void
nautilus_viewport_paint (GtkWidget *widget,
			 GdkRectangle *area)
{
  GtkViewport *viewport;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VIEWPORT (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      viewport = GTK_VIEWPORT (widget);

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, viewport->shadow_type,
		       0, 0, -1, -1);
    }
}

/* Public NautilusViewport methods */
GtkWidget*
nautilus_viewport_new (GtkAdjustment *hadjustment,
		       GtkAdjustment *vadjustment)
{
	NautilusViewport *viewport;

	viewport = NAUTILUS_VIEWPORT (gtk_widget_new (nautilus_viewport_get_type (), NULL));
	
	gtk_viewport_set_hadjustment (GTK_VIEWPORT (viewport), hadjustment);
	gtk_viewport_set_vadjustment (GTK_VIEWPORT (viewport), vadjustment);

	return GTK_WIDGET (viewport);
}
