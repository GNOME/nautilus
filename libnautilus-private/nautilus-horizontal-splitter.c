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

struct NautilusHorizontalSplitterDetails {
	gboolean down;
	double down_position;
	guint32 down_time;
	int saved_size;
};

/* Bar width currently hardcoded to 7 */
#define BAR_WIDTH 7
#define CLOSED_THRESHOLD 4
#define NOMINAL_SIZE 148
#define SPLITTER_CLICK_SLOP 1
#define SPLITTER_CLICK_TIMEOUT	400

/* NautilusHorizontalSplitterClass methods */
static void     nautilus_horizontal_splitter_initialize_class (NautilusHorizontalSplitterClass *horizontal_splitter_class);
static void     nautilus_horizontal_splitter_initialize       (NautilusHorizontalSplitter      *horizontal_splitter);
static gboolean nautilus_horizontal_splitter_button_press     (GtkWidget                       *widget,
							       GdkEventButton                  *event);
static gboolean nautilus_horizontal_splitter_button_release   (GtkWidget                       *widget,
							       GdkEventButton                  *event);

/* GtkObjectClass methods */
static void     nautilus_horizontal_splitter_destroy          (GtkObject                       *object);

/* GtkWidgetClass methods */
static void     nautilus_horizontal_splitter_draw             (GtkWidget                       *widget,
							       GdkRectangle                    *area);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusHorizontalSplitter,
				   nautilus_horizontal_splitter,
				   E_TYPE_HPANED)

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
	widget_class->button_press_event = nautilus_horizontal_splitter_button_press;
	widget_class->button_release_event = nautilus_horizontal_splitter_button_release;
	
}

static void
nautilus_horizontal_splitter_initialize (NautilusHorizontalSplitter *horizontal_splitter)
{
	horizontal_splitter->details = g_new0 (NautilusHorizontalSplitterDetails, 1);
	e_paned_set_handle_size (E_PANED (horizontal_splitter), BAR_WIDTH);
}

/* GtkObjectClass methods */
static void
nautilus_horizontal_splitter_destroy(GtkObject *object)
{
	NautilusHorizontalSplitter *horizontal_splitter;
	
	horizontal_splitter = NAUTILUS_HORIZONTAL_SPLITTER (object);

	g_free (horizontal_splitter->details);
	
	/* Chain */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
draw_resize_bar (GtkWidget		*widget,
		 GdkWindow		*window,
		 const GdkRectangle	*area)
{
	GtkStyle	*style;
	GdkGC		*gcs[BAR_WIDTH];
	guint		i;

	g_assert (widget != NULL);
	g_assert (window != NULL);
	g_assert (area != NULL);
	g_assert (area->width == BAR_WIDTH);

	style = widget->style;

	g_assert (style != NULL);
	
	gcs[0] = style->fg_gc[GTK_STATE_NORMAL];
	gcs[1] = style->fg_gc[GTK_STATE_SELECTED];
	gcs[2] = style->light_gc[GTK_STATE_ACTIVE];
	gcs[3] = style->bg_gc[GTK_STATE_NORMAL];
	gcs[4] = style->mid_gc[GTK_STATE_ACTIVE];
	gcs[5] = style->dark_gc[GTK_STATE_NORMAL];
	gcs[6] = style->fg_gc[GTK_STATE_NORMAL];
		
	for (i = 0; i < BAR_WIDTH; i++)
	{
		gdk_draw_line (window,
			       gcs[i],
			       area->x + i,
			       area->y,
			       area->x + i,
			       area->y + area->height - 1);
	}
}

/* Number of ridges in the thumb currently hardcoded to 8 */
#define NUM_RIDGES 8

/* These control the layout of the ridges */
#define RIDGE_HEIGHT 2
#define RIDGE_EDGE_OFFSET 2
#define BETWEEN_RIDGE_OFFSET 1

static void
draw_resize_bar_thumb (GtkWidget		*widget,
		       GdkWindow		*window,
		       const GdkRectangle	*area)
{
	EPaned		*paned;
	GtkStyle	*style;
	GdkGC		*light_gc;
	GdkGC		*dark_gc;
	guint		total_thumb_height;

	g_assert (widget != NULL);
	g_assert (window != NULL);
	g_assert (area != NULL);
	g_assert (area->width == BAR_WIDTH);

	style = widget->style;
	g_assert (style != NULL);

	paned = E_PANED (widget);

	light_gc = style->light_gc[GTK_STATE_ACTIVE];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];

	total_thumb_height = (NUM_RIDGES * RIDGE_HEIGHT) + ((NUM_RIDGES - 1) * BETWEEN_RIDGE_OFFSET);

	/* Make sure the thumb aint bigger than the handle */
	if (total_thumb_height > paned->handle_height)
	{
		total_thumb_height = paned->handle_height;
	}

	/* Draw the thumb only if we have enough space for at least one ridge */
	if (total_thumb_height > RIDGE_HEIGHT)
	{
		gint	y = paned->handle_ypos + (paned->handle_height - total_thumb_height) / 2;
		guint	i;
		
		for (i = 0; i < NUM_RIDGES; i++)
		{
			gint x1 = area->x + RIDGE_EDGE_OFFSET;
			gint x2 = area->x + BAR_WIDTH - RIDGE_EDGE_OFFSET;

			gdk_draw_line (window,
				       dark_gc,
				       x1,
				       y,
				       x2,
				       y);
			
			y += BETWEEN_RIDGE_OFFSET;
			
			gdk_draw_line (window,
				       light_gc,
				       x1,
				       y,
				       x2,
				       y);
			
			y += RIDGE_HEIGHT;
		}
	}
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

				/* Simply things by always drawing the full width of the bar. */
				child_area.x = 0;
				child_area.width = BAR_WIDTH;

				draw_resize_bar (widget, paned->handle, &child_area);
				draw_resize_bar_thumb (widget, paned->handle, &child_area);
			}
		}

		/* Redraw the children
		 */
		if (paned->child1 && gtk_widget_intersect (paned->child1, area, &child_area)) {
			gtk_widget_draw (paned->child1, &child_area);
		}

		if (paned->child2 && gtk_widget_intersect (paned->child2, area, &child_area)) {
			gtk_widget_draw (paned->child2, &child_area);
		}
	}
}

/* routine to toggle the open/closed state of the splitter */
static void
toggle_splitter_position (NautilusHorizontalSplitter *splitter)
{
	int position;

	position = e_paned_get_position (E_PANED (splitter));

	if (position >= CLOSED_THRESHOLD) {
		splitter->details->saved_size = position;
		position = 0;
	} else {
		position = splitter->details->saved_size;
		if (splitter->details->saved_size < CLOSED_THRESHOLD) {
			position = NOMINAL_SIZE;
		}
	}
	
	e_paned_set_position (E_PANED (splitter), position);
}

/* NautilusHorizontalSplitter public methods */
GtkWidget *
nautilus_horizontal_splitter_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_horizontal_splitter_get_type ()));
}

/* handle mouse downs by remembering the position and the time */
static gboolean
nautilus_horizontal_splitter_button_press (GtkWidget *widget, GdkEventButton *event)
{
	NautilusHorizontalSplitter *splitter;
	
	splitter = NAUTILUS_HORIZONTAL_SPLITTER (widget);

	if (event->window != E_PANED (widget)->handle) {
		splitter->details->down = FALSE;
	} else {
		splitter->details->down = TRUE;
		splitter->details->down_position = event->x;
		splitter->details->down_time = event->time;
	}

	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_press_event, (widget, event));
}

/* handle mouse ups by seeing if it was a tap and toggling the open state accordingly */
static gboolean
nautilus_horizontal_splitter_button_release (GtkWidget *widget, GdkEventButton *event)
{
	NautilusHorizontalSplitter *splitter;
	int delta, delta_time;
	
	splitter = NAUTILUS_HORIZONTAL_SPLITTER (widget);

	if (event->window == E_PANED (widget)->handle
	    && splitter->details->down) {
		delta = abs (event->x - splitter->details->down_position);
		delta_time = abs (splitter->details->down_time - event->time);
		if (delta < SPLITTER_CLICK_SLOP && delta_time < SPLITTER_CLICK_TIMEOUT)  {
			toggle_splitter_position(splitter);
		}
	}

	splitter->details->down = FALSE;
	
	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_release_event, (widget, event));
}
