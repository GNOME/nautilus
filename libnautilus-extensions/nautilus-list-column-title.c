/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-list-column-title.c: List column title widget for interacting with list columns

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Pavel Cisler <pavel@eazel.com>

*/

#include <config.h>
#include "nautilus-list-column-title.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"

#include "nautilus-list.h"

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkmain.h>

#include <libgnomeui/gnome-pixmap.h>

#include <string.h>

/* these are from GtkCList, for now we need to copy them here 
 * eventually the target list should be able to describe the values
 */

enum {
	/* this defines the base grid spacing */
	CELL_SPACING = 1,

	/* added the horizontal space at the beginning and end of a row */
	COLUMN_INSET = 3,

	/* from GtkButton */
	CHILD_SPACING = 1,

	/* the width of the column resize windows */
	DRAG_WIDTH = 6
};

static const char * down_xpm[] = {
	"6 5 2 1",
	" 	c None",
	".	c #000000",
	"......",
	"      ",
	" .... ",
	"      ",
	"  ..  "
};

static const char * up_xpm[] = {
	"6 5 2 1",
	" 	c None",
	".	c #000000",
	"  ..  ",
	"      ",
	" .... ",
	"      ",
	"......"
};

#define COLUMN_TITLE_THEME_STYLE_NAME "menu"

struct NautilusListColumnTitleDetails 
{
	/* gc for blitting sort order pixmaps, lazily allocated */
 	GdkGC *copy_area_gc;

 	/* sort order indicator pixmaps, lazily allocated */
	GdkPixmap *up_indicator_pixmap;
	GdkBitmap *up_indicator_mask;
	GdkPixmap *down_indicator_pixmap;
	GdkBitmap *down_indicator_mask;

	/* offscreen drawing idle handler id*/
	guint offscreen_drawing_idle;

	int tracking_column_resize;
		/* index of the column we are currently tracking or -1 */
	int tracking_column_prelight;
		/* index of the column we are currently rolling over or -1 */
	int tracking_column_press;
		/* index of the column we are currently pressing or -1 */
	
	int last_tracking_x;
		/* last horizontal track point so we can only resize when needed */
 	gboolean resize_cursor_on;

};

static void nautilus_list_column_title_initialize_class	(gpointer klass);
static void nautilus_list_column_title_initialize       (gpointer object, gpointer klass);
static void nautilus_list_column_title_paint		(GtkWidget *widget, GtkWidget *draw_target, GdkDrawable *target_drawable, GdkRectangle *area);
static void nautilus_list_column_title_draw 		(GtkWidget *widget, GdkRectangle *box);
static void nautilus_list_column_title_buffered_draw	(GtkWidget *widget);
static void nautilus_list_column_title_queue_buffered_draw(GtkWidget *widget);
static gboolean nautilus_list_column_title_expose 	(GtkWidget *widget, GdkEventExpose *event);
static void nautilus_list_column_title_realize 		(GtkWidget *widget);
static void nautilus_list_column_title_finalize 	(GtkObject *object);
static void nautilus_list_column_title_request 		(GtkWidget *widget, GtkRequisition *requisition);

static gboolean nautilus_list_column_title_motion	(GtkWidget *widget, GdkEventMotion *event);
static gboolean nautilus_list_column_title_leave 	(GtkWidget *widget, GdkEventCrossing *event);

static gboolean nautilus_list_column_title_button_press	(GtkWidget *widget, GdkEventButton *event);
static gboolean nautilus_list_column_title_button_release (GtkWidget *widget, GdkEventButton *event);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusListColumnTitle, nautilus_list_column_title, GTK_TYPE_BIN)
/* generates nautilus_list_column_title_get_type */

static void
nautilus_list_column_title_initialize_class (gpointer klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = nautilus_list_column_title_finalize;
	widget_class->draw = nautilus_list_column_title_draw;
	widget_class->expose_event = nautilus_list_column_title_expose;
	widget_class->realize = nautilus_list_column_title_realize;
	widget_class->size_request = nautilus_list_column_title_request;
	widget_class->motion_notify_event = nautilus_list_column_title_motion;
	widget_class->leave_notify_event = nautilus_list_column_title_leave;
	widget_class->button_press_event = nautilus_list_column_title_button_press;
	widget_class->button_release_event = nautilus_list_column_title_button_release;
}

NautilusListColumnTitle *
nautilus_list_column_title_new (void)
{
	return gtk_type_new (nautilus_list_column_title_get_type ());
}

static void
nautilus_list_column_title_initialize (gpointer object, gpointer klass)
{
	NautilusListColumnTitle *column_title;

	column_title = NAUTILUS_LIST_COLUMN_TITLE(object);
	column_title->details = g_new0 (NautilusListColumnTitleDetails, 1);

	/* copy_gc, up/down indicators get allocated lazily when needed */
	column_title->details->copy_area_gc = NULL;
	column_title->details->up_indicator_pixmap = NULL;
	column_title->details->up_indicator_mask = NULL;
	column_title->details->down_indicator_pixmap = NULL;
	column_title->details->down_indicator_mask = NULL;

	column_title->details->offscreen_drawing_idle = 0;

	column_title->details->resize_cursor_on = FALSE;
	column_title->details->tracking_column_resize = -1;
	column_title->details->tracking_column_prelight = -1;
	column_title->details->tracking_column_press = -1;
	column_title->details->last_tracking_x = -1;

	GTK_WIDGET_UNSET_FLAGS (object, GTK_NO_WINDOW);
}

static void
nautilus_list_column_title_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	int attributes_mask;

	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* ask for expose events */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x + GTK_CONTAINER (widget)->border_width;
	attributes.y = widget->allocation.y + GTK_CONTAINER (widget)->border_width;
	attributes.width = widget->allocation.width - GTK_CONTAINER (widget)->border_width * 2;
	attributes.height = widget->allocation.height - GTK_CONTAINER (widget)->border_width * 2;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK 
				| GDK_ENTER_NOTIFY_MASK 
				| GDK_LEAVE_NOTIFY_MASK
				| GDK_BUTTON_PRESS_MASK 
				| GDK_BUTTON_RELEASE_MASK
				| GDK_POINTER_MOTION_MASK 
				| GDK_POINTER_MOTION_HINT_MASK
				| GDK_KEY_PRESS_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	/* give ourselves a background window */
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), 
					 &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
nautilus_list_column_title_finalize (GtkObject *object)
{
	NautilusListColumnTitle *column_title;

	column_title = NAUTILUS_LIST_COLUMN_TITLE(object);

	if (column_title->details->up_indicator_pixmap != NULL) {
		gdk_pixmap_unref (column_title->details->up_indicator_pixmap);
		column_title->details->up_indicator_pixmap = NULL;

		g_assert (column_title->details->up_indicator_mask != NULL);
		gdk_bitmap_unref (column_title->details->up_indicator_mask);
		column_title->details->up_indicator_mask = NULL;
	}
	if (column_title->details->down_indicator_pixmap != NULL) {
		gdk_pixmap_unref (column_title->details->down_indicator_pixmap);
		column_title->details->down_indicator_pixmap = NULL;

		g_assert (column_title->details->down_indicator_mask != NULL);
		gdk_bitmap_unref (column_title->details->down_indicator_mask);
		column_title->details->down_indicator_mask = NULL;
	}

	if (column_title->details->offscreen_drawing_idle != 0) {
		gtk_idle_remove (column_title->details->offscreen_drawing_idle);
		column_title->details->offscreen_drawing_idle = 0;
	}

	if (column_title->details->copy_area_gc != NULL) {
		gdk_gc_destroy (column_title->details->copy_area_gc);
	}

	g_free (column_title->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_list_column_title_request (GtkWidget *widget, GtkRequisition *requisition)
{
	/* size requisition: make sure we have at least a minimal height */

	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (requisition != NULL);

	requisition->width = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			     widget->style->klass->xthickness) * 2;
	requisition->height = (GTK_CONTAINER (widget)->border_width + CHILD_SPACING +
			      widget->style->klass->ythickness) * 2;


	if (GTK_BIN (widget)->child && GTK_WIDGET_VISIBLE (GTK_BIN (widget)->child)) {
		GtkRequisition child_requisition;

		gtk_widget_size_request (GTK_BIN (widget)->child, &child_requisition);

		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;

		requisition->height = MIN (requisition->height, 10);
	}
}

static const char *
get_column_label_at (GtkWidget *column_title, int index)
{
	GtkCList *parent_clist;

	parent_clist = GTK_CLIST (column_title->parent);

	return parent_clist->column[index].title;
}

static void
get_column_frame_at(GtkWidget *column_title, int index, GdkRectangle *result)
{
	GtkCList *parent_clist;
	parent_clist = GTK_CLIST (column_title->parent);

	*result = parent_clist->column_title_area;
	result->x = parent_clist->column[index].area.x - COLUMN_INSET;
	result->y = 0;
	result->width = parent_clist->column[index].area.width
			+ CELL_SPACING + 2 * COLUMN_INSET - 1;
}

static void
load_up_indicator (const char **xpm_data,
		   GdkPixmap **indicator_pixmap, GdkBitmap **indicator_mask)
{
	GdkPixbuf *pixbuf;

	/* sanity */
	*indicator_pixmap = NULL;
	*indicator_mask = NULL;

	pixbuf = gdk_pixbuf_new_from_xpm_data (xpm_data);

	/* can't load, theoretically, we should always be able to load,
	 * but we'll be a good coder and catch possible errors */
	if (pixbuf == NULL) {
		g_warning ("Cannot load up/down indicator, should never happen");
		return;
	}

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, indicator_pixmap, indicator_mask, 127);

	gdk_pixbuf_unref (pixbuf);
}

static void
get_sort_indicator (GtkWidget *widget, gboolean ascending,
		    GdkPixmap **indicator_pixmap, GdkBitmap **indicator_mask)
{
	/* return the sort order pixmap for a given sort direction
	 * allocate the pixmap first time around
	 */
	NautilusListColumnTitle *column_title;

	g_return_if_fail (indicator_pixmap != NULL);
	g_return_if_fail (indicator_mask != NULL);

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);

	if (ascending) {
		if (column_title->details->up_indicator_pixmap == NULL) {
			g_assert (column_title->details->up_indicator_mask == NULL);

			load_up_indicator (up_xpm,
					   &column_title->details->up_indicator_pixmap,
					   &column_title->details->up_indicator_mask);
		}
		*indicator_pixmap = column_title->details->up_indicator_pixmap;
		*indicator_mask = column_title->details->up_indicator_mask;
	} else {
		if (column_title->details->down_indicator_pixmap == NULL) {
			g_assert (column_title->details->down_indicator_mask == NULL);

			load_up_indicator (down_xpm,
					   &column_title->details->down_indicator_pixmap,
					   &column_title->details->down_indicator_mask);
		}
		*indicator_pixmap = column_title->details->down_indicator_pixmap;
		*indicator_mask = column_title->details->down_indicator_mask;
	}
}

/* Add more truncation modes, optimize for performance, move to nautilus-gdk-extensions */
static char *
truncate_string (const char *string, GdkFont *font, int width, int *final_width)
{
	int current_width;
	int ellipsis_width;
	int length;
	int trimmed_length;
	char *result;

	length = strlen (string);
	current_width = gdk_text_width (font, string, length);
	if (current_width <= width) {
		/* trivial case, already fits fine */
		if (final_width  != NULL) {
			*final_width = current_width;
		}
		return g_strdup (string);
	}

	ellipsis_width = gdk_string_width (font, "...");
	if (ellipsis_width > width) {
		/* we can't fit anything */
		if (final_width  != NULL) {
			*final_width = 0;
		}
		return g_strdup ("");
	}

	width -= ellipsis_width;

	for (trimmed_length = length - 1; trimmed_length >= 0; trimmed_length--) {
		current_width = gdk_text_width (font, string, trimmed_length);
		if (current_width <= width)
			break;
	}
	result = (char *) g_malloc (trimmed_length + 3 + 1);
	strncpy (result, string, trimmed_length);
	strcpy (result + trimmed_length, "...");

	if (final_width  != NULL) {
		*final_width = current_width + ellipsis_width;
	}

	return result;
}

/* FIXME bugzilla.eazel.com 615:
 * Some of these magic numbers could be replaced with some more dynamic values
 */
enum {
	CELL_TITLE_INSET = 3,
	TITLE_BASELINE_OFFSET = 6,
	SORT_ORDER_INDICATOR_WIDTH = 10,
	SORT_INDICATOR_X_OFFSET = 6,
	SORT_INDICATOR_Y_OFFSET = 3
};

static void
nautilus_list_column_title_paint (GtkWidget *widget, GtkWidget *draw_target, 
				  GdkDrawable *target_drawable, GdkRectangle *area)
{
	NautilusListColumnTitle *column_title;
	GtkCList *parent_clist;
	int index;

	g_assert (GTK_CLIST (widget->parent) != NULL);

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);
	parent_clist = GTK_CLIST (widget->parent);

	for (index = 0; index < parent_clist->columns; index++) {
		GdkRectangle cell_rectangle;
		GdkRectangle cell_redraw_area;
		const char *cell_label;
		int text_x_offset;
		int text_x_available_end;
		int sort_indicator_x_offset;
		GdkPixmap *sort_indicator_pixmap;
		GdkBitmap *sort_indicator_mask;
		gboolean right_justified;

		sort_indicator_x_offset = 0;
		sort_indicator_pixmap = NULL;
		sort_indicator_mask = NULL;
		right_justified = (parent_clist->column[index].justification == GTK_JUSTIFY_RIGHT);

		/* pick the ascending/descending sort indicator if needed */
		if (index == parent_clist->sort_column) {
			get_sort_indicator (widget, 
				parent_clist->sort_type == GTK_SORT_ASCENDING,
				&sort_indicator_pixmap,
				&sort_indicator_mask);
		}
		
		get_column_frame_at (widget, index, &cell_rectangle);
		gdk_rectangle_intersect (&cell_rectangle, area, &cell_redraw_area);

		if (cell_redraw_area.width == 0 || cell_redraw_area.height == 0) {
			/* no work, go on to the next */
			continue;
		}
			
		cell_label = get_column_label_at (widget, index);

		/* FIXME bugzilla.eazel.com 616:
		 * add support for center justification
		 */

		text_x_offset = cell_rectangle.x + CELL_TITLE_INSET;
		text_x_available_end = cell_rectangle.x + cell_rectangle.width - 2 * CELL_TITLE_INSET;

		/* Paint the column tiles as rectangles using "menu" (COLUMN_TITLE_THEME_STYLE_NAME).
		 * Style buttons as used by GtkCList produce round corners in some themes.
		 * Eventually we might consider having a separate style for column titles.
		 */
		gtk_paint_box (widget->style, target_drawable,
			       column_title->details->tracking_column_prelight == index ? 
			       		GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
			       column_title->details->tracking_column_press == index 
					? GTK_SHADOW_IN : GTK_SHADOW_OUT,
			       area, draw_target, COLUMN_TITLE_THEME_STYLE_NAME,
			       cell_rectangle.x, cell_rectangle.y, 
			       cell_rectangle.width, cell_rectangle.height);


		/* Draw the sort indicator if needed */
		if (sort_indicator_pixmap != NULL) {
			int y_offset;

			if (right_justified) {
				sort_indicator_x_offset = cell_rectangle.x + SORT_INDICATOR_X_OFFSET;		
				text_x_offset = sort_indicator_x_offset + CELL_TITLE_INSET 
					+ SORT_ORDER_INDICATOR_WIDTH ;
			} else {
				sort_indicator_x_offset = cell_rectangle.x + cell_rectangle.width 
					   - SORT_INDICATOR_X_OFFSET - SORT_ORDER_INDICATOR_WIDTH;
				text_x_available_end = sort_indicator_x_offset - CELL_TITLE_INSET;
			}
			y_offset = cell_rectangle.y + cell_rectangle.height / 2 
				   - SORT_INDICATOR_Y_OFFSET;

			/* allocate the sort indicator copy gc first time around */
			if (column_title->details->copy_area_gc == NULL) {
				column_title->details->copy_area_gc = gdk_gc_new (widget->window);
				gdk_gc_set_function (column_title->details->copy_area_gc, GDK_COPY);
			}
			/* move the pixmap clip mask and origin to the right spot in the gc */
			gdk_gc_set_clip_mask (column_title->details->copy_area_gc, 
					      sort_indicator_mask);
			gdk_gc_set_clip_origin (column_title->details->copy_area_gc, sort_indicator_x_offset, y_offset);


			gdk_draw_pixmap (target_drawable, column_title->details->copy_area_gc, 
					 sort_indicator_pixmap, 0, 0, sort_indicator_x_offset, y_offset, 
					 -1, -1);
		}
			
		if (cell_label) {
			char *truncated_label;
			int truncanted_width;
			GdkRectangle temporary;

			/* Extend the redraw area vertically to contain the entire cell 
			 * -- seems like if I don't do this, for short exposed areas no text
			 * will get drawn.
			 * This happens when the title is half off-screen and you move it up by a pixel or two.
			 * If you move it up faster, it gets redrawn properly.
			 */
			cell_redraw_area.y = cell_rectangle.y;
			cell_redraw_area.height = cell_rectangle.height;

			/* Clip a little more than the cell rectangle to
			 * not have the text draw over the cell broder.
			 */
			temporary = cell_rectangle;
			/* Eeeek: magic numbers */
			nautilus_rectangle_inset (&temporary, 2, 2);
			gdk_rectangle_intersect (&cell_redraw_area, &temporary, &cell_redraw_area);

			truncated_label = truncate_string (cell_label, widget->style->font, 
				text_x_available_end - text_x_offset, &truncanted_width);
		
			if (right_justified) {
				text_x_offset = text_x_available_end - truncanted_width;
			}

			gtk_paint_string (widget->style, target_drawable, GTK_STATE_NORMAL,
					  &cell_redraw_area, draw_target, "label", 
					  text_x_offset,
					  cell_rectangle.y + cell_rectangle.height - TITLE_BASELINE_OFFSET,
					  truncated_label);
			g_free (truncated_label);
		}
	}
}

static void
nautilus_list_column_title_draw (GtkWidget *widget, GdkRectangle *area)
{
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (area != NULL);

	if (!GTK_WIDGET_DRAWABLE (widget)) {
		return;
	}

	nautilus_list_column_title_paint (widget, widget, widget->window, area);
}

static void
nautilus_list_column_title_buffered_draw (GtkWidget *widget)
{
	/* draw using an offscreen_pixmap */
	GdkRectangle redraw_area;
	NautilusListColumnTitle *column_title;
	GdkPixmap *offscreen_pixmap;
	GdkGC *offscreen_blitting_gc;

	/* don't do anything if not drawable */
	if ( ! GTK_WIDGET_DRAWABLE (widget)) {
		return;
	}

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);
	
	redraw_area.x = 0;
	redraw_area.y = 0;
	redraw_area.width = widget->allocation.width;
	redraw_area.height = widget->allocation.height;

	/* allocate a new offscreen_pixmap */
	offscreen_pixmap = gdk_pixmap_new (widget->window, 
					   redraw_area.width, 
					   redraw_area.height, -1);

	/* Erase the offscreen background.
	 * We are using the GtkStyle call to draw the background - this is a tiny bit
	 * less efficient but gives us the convenience of setting up the right colors and
	 * gc for the style we are using to blit the column titles.
	 */
	gtk_paint_box (widget->style, offscreen_pixmap,
		       GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		       &redraw_area, widget,
		       COLUMN_TITLE_THEME_STYLE_NAME,
		       redraw_area.x, redraw_area.y, 
		       redraw_area.width, redraw_area.height);

	/* render the column titles into the offscreen */
	nautilus_list_column_title_paint (widget, widget,
					  offscreen_pixmap, &redraw_area);

	/* allocate a gc to blit the offscreen */
	offscreen_blitting_gc = gdk_gc_new (widget->window);

	/* blit the offscreen into the real view */
	gdk_draw_pixmap (widget->window, offscreen_blitting_gc,
			 offscreen_pixmap, 0, 0, 0, 0, -1, -1);

	gdk_pixmap_unref (offscreen_pixmap);
	gdk_gc_destroy (offscreen_blitting_gc);
}

/* Do all buffered drawing in an idle, this means it's only done after all
 * events have been processed and thus we don't do it unneccessairly */
static gboolean
offscreen_drawing_idle_handler (gpointer data)
{
	GtkWidget *widget;
	NautilusListColumnTitle *column_title;

	g_assert (GTK_IS_WIDGET (data));
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (data));

	widget = GTK_WIDGET (data);
	column_title = NAUTILUS_LIST_COLUMN_TITLE (data);

	nautilus_list_column_title_buffered_draw (widget);

	column_title->details->offscreen_drawing_idle = 0;

	return FALSE;
}

/* queue a buffered_draw to be called later after all other events
 * are processed.  Increasing performance and reducing memory load. */
static void
nautilus_list_column_title_queue_buffered_draw (GtkWidget *widget)
{
	NautilusListColumnTitle *column_title;

	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));

	column_title = NAUTILUS_LIST_COLUMN_TITLE (widget);
 
	if (column_title->details->offscreen_drawing_idle == 0) {
		column_title->details->offscreen_drawing_idle =
			gtk_idle_add (offscreen_drawing_idle_handler, widget);
	}
}

static gboolean
nautilus_list_column_title_expose (GtkWidget *widget, GdkEventExpose *event)
{
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (event != NULL);
	
	if (!GTK_WIDGET_DRAWABLE (widget)) {
		return FALSE;
	}

	nautilus_list_column_title_paint (widget, widget, widget->window, &event->area);

	return FALSE;
}

static int
in_column_rect (GtkWidget *widget, int x, int y)
{
	/* return the index of the column we hit or -1 */

	GtkCList *parent_clist;
	int index;

	parent_clist = GTK_CLIST (widget->parent);
	
	for (index = 0; index < parent_clist->columns; index++) {
		/* hit testing for column resizing */
		GdkRectangle cell_rectangle;
		
		get_column_frame_at (widget, index, &cell_rectangle);

		/* inset by a pixel so that you have to move past the border
		 * to be considered inside the rect
		 * nautilus_list_column_title_leave depends on this 
		 */
		nautilus_rectangle_inset (&cell_rectangle, 1, 0);

		
		if (nautilus_rectangle_contains (&cell_rectangle, x, y))
			return index;
	}

	return -1;
}

static int
in_resize_rect (GtkWidget *widget, int x, int y)
{
	/* return the index of the resize rect of a column we hit or -1 */

	GtkCList *parent_clist;
	int index;

	parent_clist = GTK_CLIST (widget->parent);
	
	for (index = 0; index < parent_clist->columns; index++) {
		/* hit testing for column resizing */
		GdkRectangle resize_rectangle;
		
		get_column_frame_at (widget, index, &resize_rectangle);

		nautilus_rectangle_inset (&resize_rectangle, 1, 0);

		resize_rectangle.x = resize_rectangle.x + resize_rectangle.width - DRAG_WIDTH / 2;
		resize_rectangle.width = DRAG_WIDTH;

		if (nautilus_rectangle_contains (&resize_rectangle, x, y))
			return index;
	}

	return -1;
}

static void
show_hide_resize_cursor_if_needed (GtkWidget *widget, gboolean on)
{
	NautilusListColumnTitle *column_title;

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);

	if (on == column_title->details->resize_cursor_on)
		/* already set right */
		return;

	if (on) {
		/* switch to a resize cursor */
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
		gdk_window_set_cursor (widget->window, cursor);
		gdk_cursor_destroy (cursor);
	} else 
		/* restore to old cursor */
		gdk_window_set_cursor (widget->window, NULL);

	column_title->details->resize_cursor_on = on;
}

static gboolean
track_prelight (GtkWidget *widget, int mouse_x, int mouse_y)
{
	NautilusListColumnTitle *column_title;
	int over_column;

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);

	/* see if we need to update the prelight state of a column */
	over_column = in_column_rect (widget, mouse_x, mouse_y);

	if (column_title->details->tracking_column_resize != -1) {
		/* resizing a column, don't prelight */
		over_column = -1;
	}

	if (column_title->details->tracking_column_press != -1) {
		/* pressing a column, don't prelight */
		over_column = -1;
	}

	if (column_title->details->tracking_column_prelight == over_column) {
		/* no change */
		return FALSE;
	}

	/* update state and tell callers to redraw */
	column_title->details->tracking_column_prelight = over_column;
	
	return TRUE;
}

static gboolean
nautilus_list_column_title_motion (GtkWidget *widget, GdkEventMotion *event)
{
	NautilusListColumnTitle *column_title;
	GtkWidget *parent_list;
	int mouse_x, mouse_y;
	gboolean title_update_needed;
	
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (NAUTILUS_IS_LIST (widget->parent));

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);
	parent_list = GTK_WIDGET (widget->parent);
	title_update_needed = FALSE;

	gdk_window_get_pointer (widget->window, &mouse_x, &mouse_y, NULL);

	if (column_title->details->tracking_column_resize != -1) {
		/* we are currently tracking a column */
		if (column_title->details->last_tracking_x != mouse_x) {
			/* mouse did move horizontally since last time */
			column_title->details->last_tracking_x = mouse_x;
			NAUTILUS_INVOKE_METHOD
				(NAUTILUS_LIST_CLASS, parent_list,
				 column_resize_track,
				 (parent_list, column_title->details->tracking_column_resize));
			title_update_needed = TRUE;
		}
	} else {
		/* make sure we are showing the right cursor */
		show_hide_resize_cursor_if_needed (widget, 
			in_resize_rect (widget, mouse_x, mouse_y) != -1);
	}

	/* see if we need to update the prelight state of a column */
	title_update_needed |= track_prelight (widget, mouse_x, mouse_y);

	if (title_update_needed) {
		nautilus_list_column_title_queue_buffered_draw (widget);
	}

	return TRUE;
}

static gboolean
nautilus_list_column_title_leave (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusListColumnTitle *column_title;

	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (NAUTILUS_IS_LIST (widget->parent));

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);

	/* see if we need to update the prelight state of a column */
	if (column_title->details->tracking_column_prelight != -1) {
		column_title->details->tracking_column_prelight = -1;
		gtk_widget_set_state (widget, GTK_STATE_NORMAL);
	}
	nautilus_list_column_title_queue_buffered_draw (widget);
	return TRUE;
}

static gboolean
nautilus_list_column_title_button_press (GtkWidget *widget, GdkEventButton *event)
{
	NautilusListColumnTitle *column_title;
	GtkWidget *parent_list;
	int grab_result;

	g_assert (event != NULL);
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (NAUTILUS_IS_LIST (widget->parent));
	g_assert (event->type != GDK_BUTTON_PRESS
		|| NAUTILUS_LIST_COLUMN_TITLE(widget)->details->tracking_column_resize == -1);

	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);
	parent_list = GTK_WIDGET (widget->parent);


	if (event->type == GDK_BUTTON_PRESS) {
		int resized_column;
		int clicked_column;
		
		resized_column = in_resize_rect (widget, (int)event->x, (int)event->y);
		clicked_column = in_column_rect (widget, (int)event->x, (int)event->y);

		if (resized_column != -1) {
			GdkCursor *cursor;

			/* during the drag, use the resize cursor */ 
			cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);

			/* grab the pointer events so that we get them even when
			 * the mouse tracks out of the widget window
			 */
			grab_result = gdk_pointer_grab (widget->window, FALSE,
						        GDK_POINTER_MOTION_HINT_MASK 
						        | GDK_BUTTON1_MOTION_MASK 
						        | GDK_BUTTON_RELEASE_MASK,
						        NULL, cursor, event->time);
			gdk_cursor_destroy (cursor);

			if (grab_result != 0) {
				/* failed to grab the pointer, give up 
				 * 
				 * The grab results are not very well documented
				 * looks like they may be Success, GrabSuccess, AlreadyGrabbed
				 * or anything else the low level X calls in gdk_pointer_grab
				 * decide to return.
				 */
				return FALSE;
			}

			/* set up new state */
			column_title->details->tracking_column_resize = resized_column;
			column_title->details->tracking_column_prelight = -1;

			/* FIXME bugzilla.eazel.com 617:
			 * use a "resized" state here ?
			 */
			gtk_widget_set_state (widget, GTK_STATE_NORMAL);

			/* start column resize tracking */
			NAUTILUS_INVOKE_METHOD
				(NAUTILUS_LIST_CLASS, parent_list,
				 column_resize_track_start,
				 (parent_list, resized_column));

			return FALSE;
		}
		
		if (clicked_column != -1) {
			/* clicked a column, draw the pressed column title */
			column_title->details->tracking_column_prelight = -1;
			column_title->details->tracking_column_press = clicked_column;
			gtk_widget_set_state (widget, GTK_STATE_ACTIVE);

			/* grab the pointer events so that we get release events even when
			 * the mouse tracks out of the widget window
			 */
			grab_result = gdk_pointer_grab (widget->window, FALSE,
						        GDK_BUTTON_RELEASE_MASK,
						        NULL, NULL, event->time);

			if (grab_result != 0) {
				/* failed to grab the pointer, give up */
				return FALSE;
			}

			nautilus_list_column_title_queue_buffered_draw (widget);
		}
		
	}	

	return FALSE;
}

static gboolean 
nautilus_list_column_title_button_release (GtkWidget *widget, GdkEventButton *event)
{
	NautilusListColumnTitle *column_title;
	GtkWidget *parent_list;

	g_assert (event != NULL);
	g_assert (NAUTILUS_IS_LIST_COLUMN_TITLE (widget));
	g_assert (NAUTILUS_IS_LIST (widget->parent));


	column_title = NAUTILUS_LIST_COLUMN_TITLE(widget);
	parent_list = GTK_WIDGET (widget->parent);

	/* let go of all the pointer events */
	if ((column_title->details->tracking_column_resize != -1
	    || column_title->details->tracking_column_press != -1)
		&& gdk_pointer_is_grabbed ())
		gdk_pointer_ungrab (event->time);

	if (column_title->details->tracking_column_resize != -1) {
			
		/* end column resize tracking */
		NAUTILUS_INVOKE_METHOD
			(NAUTILUS_LIST_CLASS, parent_list,
			 column_resize_track_end,
			 (parent_list, column_title->details->tracking_column_resize));
		column_title->details->tracking_column_resize = -1;

	} else if (column_title->details->tracking_column_press != -1) {

		/* column title press -- change the sort order */
		gtk_signal_emit_by_name (GTK_OBJECT (parent_list), "click_column", 
					 column_title->details->tracking_column_press);
		/* end press tracking */
		column_title->details->tracking_column_press = -1;
	}
	
	track_prelight (widget, (int)event->x, (int)event->y);
	gtk_widget_set_state (widget, 
		column_title->details->tracking_column_prelight != -1 ? 
		GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);

	nautilus_list_column_title_queue_buffered_draw (widget);

	return FALSE;
}
