/* -*- mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
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
 * This is the the implementation of a bit-map based tabs widget for the
 * services summary view, but it's general enough to be used elsewhere.
 * 
 */

#include <config.h>
#include "nautilus-tabs.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-scalable-font.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* constants for the tab piece pixbuf array */

#define TAB_BACKGROUND		0
#define	TAB_ACTIVE_LEFT		1
#define	TAB_ACTIVE_FILL		2
#define TAB_ACTIVE_RIGHT	3

#define	TAB_INACTIVE_LEFT	4
#define	TAB_INACTIVE_FILL	5
#define TAB_INACTIVE_RIGHT	6

#define	TAB_PRELIGHT_LEFT	7
#define	TAB_PRELIGHT_FILL	8
#define TAB_PRELIGHT_RIGHT	9

#define TAB_INACTIVE_ACTIVE	10
#define TAB_ACTIVE_INACTIVE	11
#define TAB_PRELIGHT_ACTIVE	12
#define TAB_ACTIVE_PRELIGHT	13

#define LAST_TAB_OFFSET		14

/* data structures */

typedef struct {
	gboolean prelit;
	gboolean selected;
	char *tab_text;
	int notebook_page;
	GdkRectangle tab_rect;
} TabItem;

struct NautilusTabsDetails {
	int tab_count;
	int total_height;
	
	GdkRectangle title_rect;
	EelScalableFont *tab_font;	
	int font_size;
	int selected_tab;
	
	GdkPixbuf *tab_piece_images[LAST_TAB_OFFSET];
	int tab_height;
	int tab_left_offset;

	GList *tab_items;
};

/* constants */
#define TAB_RIGHT_MARGIN 4

/* signals */
enum {
	TAB_SELECTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* headers */

static void     nautilus_tabs_initialize_class	(NautilusTabsClass *klass);
static void     nautilus_tabs_initialize	(NautilusTabs      *pixmap);
static void	nautilus_tabs_draw (GtkWidget *widget, GdkRectangle *box);

static int      nautilus_tabs_expose		(GtkWidget              *widget,
						 GdkEventExpose         *event);
static void     nautilus_tabs_destroy		(GtkObject              *object);
static void	nautilus_tabs_size_request	(GtkWidget *widget, GtkRequisition *requisition);

static int	nautilus_tabs_hit_test		(NautilusTabs *tabs, int x, int y);

static void	nautilus_tabs_load_tab_pieces	(NautilusTabs *tabs, const char *tab_piece_directory);
static gboolean nautilus_tabs_motion_event      (GtkWidget        *tabs,
						 GdkEventMotion   *event);
static gboolean nautilus_tabs_press_event	(GtkWidget        *widget,
						 GdkEventButton   *event);
static gboolean nautilus_tabs_leave_event	(GtkWidget *tabs_widget, GdkEventCrossing *event);
static void	nautilus_tabs_prelight_tab	(NautilusTabs *tabs, int which_tab);
static void	nautilus_tabs_select_tab	(NautilusTabs *tabs, int which_tab);
static void	nautilus_tabs_unload_tab_pieces (NautilusTabs *tabs);

static void     draw_all_tabs		        (NautilusTabs *tabs);
static TabItem* tab_item_find_by_name		(NautilusTabs	*tabs,
						 const char		*name);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTabs, nautilus_tabs, GTK_TYPE_DRAWING_AREA)

static void
nautilus_tabs_initialize_class (NautilusTabsClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	
	object_class->destroy = nautilus_tabs_destroy;
	widget_class->draw = nautilus_tabs_draw;
	widget_class->expose_event = nautilus_tabs_expose;
	widget_class->size_request = nautilus_tabs_size_request;	
	widget_class->button_press_event  = nautilus_tabs_press_event;
	widget_class->leave_notify_event = nautilus_tabs_leave_event;
	widget_class->motion_notify_event = nautilus_tabs_motion_event;

	/* add the "tab selected" signal */
	signals[TAB_SELECTED] = gtk_signal_new
		("tab_selected",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusTabsClass,
				    tab_selected),
		 gtk_marshal_NONE__INT,
		 GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}
 

/* initialize a newly allocated tabs object */
static void
nautilus_tabs_initialize (NautilusTabs *tabs)
{
	GtkWidget *widget = GTK_WIDGET (tabs);
	char *gray_tab_directory_path;
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget) | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	
	
	tabs->details = g_new0 (NautilusTabsDetails, 1);
	tabs->details->selected_tab = -1;
		
	/* FIXME bugzilla.eazel.com 5456: 
	 * Hard coded font size.
	 */
	tabs->details->tab_font = eel_scalable_font_get_default_bold_font ();
	tabs->details->font_size = 14;
	
	gray_tab_directory_path = nautilus_theme_get_image_path ("gray_tab_pieces");
	nautilus_tabs_load_tab_pieces (tabs, gray_tab_directory_path);
	g_free (gray_tab_directory_path);
		
	gtk_widget_add_events (widget, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
}

GtkWidget *
nautilus_tabs_new (void)
{
	return gtk_widget_new (nautilus_tabs_get_type (), NULL);
}

/* utility to destroy all the storage used by a tab item */

static void
tab_item_destroy (TabItem *item)
{
	g_free (item->tab_text);
	g_free (item);
}

static void
tab_item_destroy_cover (gpointer item, gpointer callback_data)
{
	g_assert (callback_data == NULL);
	tab_item_destroy (item);
}

static void
nautilus_tabs_destroy (GtkObject *object)
{
	NautilusTabs *tabs = NAUTILUS_TABS(object);
   	
	/* deallocate the tab piece images, if any */
	if (tabs->details->tab_piece_images[0] != NULL) {
		nautilus_tabs_unload_tab_pieces (tabs);
	}
	
	if (tabs->details->tab_font != NULL) {
		gtk_object_unref (GTK_OBJECT (tabs->details->tab_font));
	}
	
	/* release the tab list, if any */
	eel_g_list_free_deep_custom (tabs->details->tab_items,
					  tab_item_destroy_cover,
					  NULL);
	g_free (tabs->details);
  	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* unload the tab piece images, if any */
static void
nautilus_tabs_unload_tab_pieces (NautilusTabs *tabs)
{
	int index;
	for (index = 0; index < LAST_TAB_OFFSET; index++) {
		if (tabs->details->tab_piece_images[index]) {
			gdk_pixbuf_unref (tabs->details->tab_piece_images[index]);
			tabs->details->tab_piece_images[index] = NULL;			
		}
	}	
}

/* load the tab piece images */

static GdkPixbuf *
load_tab_piece (const char *piece_directory, const char *piece_name)
{
	GdkPixbuf *pixbuf;
	gchar *image_path;
	image_path = g_strdup_printf ("%s/%s.png", piece_directory, piece_name);	
	pixbuf = gdk_pixbuf_new_from_file (image_path);
	if (pixbuf == NULL) {
		g_warning ("cant load tab piece: %s", image_path);
	}
	
	g_free (image_path);
	return pixbuf;
}

static void
nautilus_tabs_load_tab_pieces (NautilusTabs *tabs, const char* tab_piece_directory)
{
	tabs->details->tab_piece_images[TAB_BACKGROUND]  = load_tab_piece (tab_piece_directory, "fill-background");
	
	tabs->details->tab_piece_images[TAB_ACTIVE_LEFT]  = load_tab_piece (tab_piece_directory, "active-left-bumper");
	tabs->details->tab_piece_images[TAB_ACTIVE_FILL]  = load_tab_piece (tab_piece_directory, "active-fill");
	tabs->details->tab_piece_images[TAB_ACTIVE_RIGHT] = load_tab_piece (tab_piece_directory, "active-right-bumper");
	
	tabs->details->tab_piece_images[TAB_INACTIVE_LEFT]  = load_tab_piece (tab_piece_directory,  "inactive-left-bumper");
	tabs->details->tab_piece_images[TAB_INACTIVE_FILL]  = load_tab_piece (tab_piece_directory,  "inactive-fill");
	tabs->details->tab_piece_images[TAB_INACTIVE_RIGHT] = load_tab_piece (tab_piece_directory, "inactive-right-bumper");
	
	tabs->details->tab_piece_images[TAB_PRELIGHT_LEFT]  = load_tab_piece (tab_piece_directory, "prelight-left-bumper");
	tabs->details->tab_piece_images[TAB_PRELIGHT_FILL]  = load_tab_piece (tab_piece_directory, "prelight-fill");
	tabs->details->tab_piece_images[TAB_PRELIGHT_RIGHT] = load_tab_piece (tab_piece_directory, "prelight-right-bumper");
	
	tabs->details->tab_piece_images[TAB_INACTIVE_ACTIVE] = load_tab_piece (tab_piece_directory, "inactive-active-bridge");
	tabs->details->tab_piece_images[TAB_ACTIVE_INACTIVE] = load_tab_piece (tab_piece_directory, "active-inactive-bridge");
	tabs->details->tab_piece_images[TAB_PRELIGHT_ACTIVE] = load_tab_piece (tab_piece_directory, "prelight-active-bridge");
	tabs->details->tab_piece_images[TAB_ACTIVE_PRELIGHT] = load_tab_piece (tab_piece_directory,  "active-prelight-bridge");
}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it.  */

static int
nautilus_tabs_hit_test (NautilusTabs *tabs, int x, int y)
{
	GList *current_item;
	TabItem *tab_item;
	GdkRectangle *rect_ptr;
	int result;
	
	tabs->details->total_height = tabs->details->tab_height;
	current_item = tabs->details->tab_items;
	
	if (current_item == NULL)
		return -1;
			
	/* loop through the items, seeing if the passed in point is in one of the rectangles */
	tab_item = (TabItem*) current_item->data;
	if (current_item->next) {
		tab_item = (TabItem*) current_item->next->data;
	}
			
	result = -1;
	while (current_item != NULL) {
		tab_item = (TabItem*) current_item->data;
		rect_ptr = &tab_item->tab_rect;

		if ((x >= rect_ptr->x) && (x < rect_ptr->x + rect_ptr->width) &&
		     	(y >= rect_ptr->y) && (y< rect_ptr->y + rect_ptr->height))
		   	result = tab_item->notebook_page;
		 	
		current_item = current_item->next;
	}
	return result;
}

/* utility routine to the height of the tabs */
/* this assumes there's only one row of them */
static int
measure_height (NautilusTabs *tabs)
{
	return gdk_pixbuf_get_height (tabs->details->tab_piece_images[TAB_ACTIVE_FILL]);	
}

/* resize the widget based on the number of tabs */

static void
recalculate_size(NautilusTabs *tabs)
{
	GtkWidget *widget = GTK_WIDGET (tabs);
	
	/* layout tabs to make sure height measurement is valid */
	tabs->details->total_height = measure_height (tabs);
  	
	widget->requisition.width = widget->parent ? widget->parent->allocation.width: 136;
	widget->requisition.height = tabs->details->total_height;
	gtk_widget_queue_resize (widget);
}

/* handle setting the size */
static void
nautilus_tabs_size_request (GtkWidget *widget, GtkRequisition *requisition)
{	
	requisition->width = widget->parent->allocation.width;
   	requisition->height = measure_height (NAUTILUS_TABS (widget));;	
}

/* utility to draw a single tab piece into a pixbuf */
static int
draw_tab_piece_aa (NautilusTabs *tabs, GdkPixbuf *dest_pixbuf, int x, int y, int x_limit, int which_piece)
{
	GtkWidget *widget;
	GdkPixbuf *pixbuf;
	int width, height;
	int dest_width, dest_height;
	int blit_width, blit_height;
		
	widget = GTK_WIDGET (tabs);
	pixbuf = tabs->details->tab_piece_images[which_piece];

	/* if there's no pixbuf, just exit, returning a nominal size */
	if (pixbuf == NULL) {
		return 32;
	}
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	dest_width = gdk_pixbuf_get_width (dest_pixbuf);
	dest_height = gdk_pixbuf_get_height (dest_pixbuf);
	
	/* trim tab piece to fit within the destination and the passed in limits */

	if (x_limit > 0) {
		if (x_limit < dest_width) {
			dest_width = x_limit;
		}
	}
		
	if (x + width > dest_width) {
		blit_width = dest_width - x;
	} else {
		blit_width = width;
	}
	
	
	if (y + height > dest_height) {
		blit_height = dest_height - y;
	} else {
		blit_height = height;
	}


	if (x >= 0 && y >= 0 && width > 0 && height > 0) {
		gdk_pixbuf_copy_area (pixbuf,
				0, 0,
				blit_width, blit_height,
				dest_pixbuf,
				x, y);
	}
	return width;
}

/* fill the canvas buffer with a tiled pixmap */

static void
draw_pixbuf_tiled_aa (GdkPixbuf *src_pixbuf, GdkPixbuf *dest_pixbuf, int offset)
{
	int x, y;
	int start_x, start_y;
	int end_x, end_y;
	int tile_width, tile_height;
	int blit_width, blit_height;
	
	tile_width = gdk_pixbuf_get_width (src_pixbuf);
	tile_height = gdk_pixbuf_get_height (src_pixbuf);
	
	start_x = 0;
	start_y = offset;	
	end_x = gdk_pixbuf_get_width (dest_pixbuf);
	end_y = gdk_pixbuf_get_height (dest_pixbuf);

	for (y = start_y; y < end_y; y += tile_height) {
		for (x = start_x; x < end_x; x += tile_width) {

			if (x + tile_width < end_x) {
				blit_width = tile_width;
			} else {
				blit_width = end_x - x;
			}
			
			if (y + tile_height < end_y) {
				blit_height = tile_height;
			} else {
				blit_height = end_y - y;
			}
			
			gdk_pixbuf_copy_area (src_pixbuf,
				0, 0,
				blit_width, blit_height,
				dest_pixbuf,
				x, y);
		}
	}
}

/* allocate_cleared_pixbuf allocates a new pixbuf with the passed-in dimensions and
   clears it to be transparent */
static GdkPixbuf*
allocate_cleared_pixbuf (int width, int height)
{
	guchar		*pixels_ptr;
	GdkPixbuf	*pixbuf;
	int		y_index, row_stride;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		
	/* clear it */	
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels_ptr = gdk_pixbuf_get_pixels (pixbuf);	
			
	for (y_index = 0; y_index < height; y_index++) {
      		memset (pixels_ptr, 0, row_stride);
		pixels_ptr += row_stride; 
	}		

	return pixbuf;
}

static int
measure_tab_name (NautilusTabs *tabs, const char *tab_name)
{
	return eel_scalable_font_text_width (tabs->details->tab_font,
						  tabs->details->font_size,
						  tab_name,
						  strlen (tab_name));
}

/* utility to draw the tab label */
static void
draw_tab_label (NautilusTabs *tabs, GdkPixbuf *tab_pixbuf, int x_pos, const char* label, gboolean is_active, gboolean is_prelit)
{
	int text_x, text_y;
	uint text_color;
	
	text_x = x_pos + 1;
	text_y = 5; /* calculate this to center font in label? */
	
	/* make sure we can draw at least some of it */
	if (text_x < gdk_pixbuf_get_width (tab_pixbuf)) {	
		eel_scalable_font_draw_text (tabs->details->tab_font, tab_pixbuf, 
					  text_x, text_y,
					  NULL,
					  tabs->details->font_size,
					  label, strlen (label),
					  EEL_RGB_COLOR_BLACK, EEL_OPACITY_FULLY_OPAQUE);
		text_x -= 1;
		text_y -= 1;

		if (is_active) {
			text_color = EEL_RGB_COLOR_WHITE;
		} else {
			if (is_prelit) {
				text_color = EEL_RGBA_COLOR_PACK (241, 241, 241, 255);		
			} else {
				text_color = EEL_RGBA_COLOR_PACK (223, 223, 223, 255);		
			}
		}
	
		eel_scalable_font_draw_text (tabs->details->tab_font, tab_pixbuf,
					  text_x, text_y,
					  NULL,
					  tabs->details->font_size,
					  label, strlen (label),
					  text_color,
					  EEL_OPACITY_FULLY_OPAQUE);
	}
}

/* draw or layout all of the tabs.
 *
 *   NOTE: currently, this only supports two tabs since it was written quickly for the summary view,
 *   which only requires two.  We should rewrite this to support any number of tabs when we have time.
 */
   
static void
draw_all_tabs (NautilusTabs *tabs)
{
	int x_pos;
	TabItem *left_tab, *right_tab;
	GdkPixbuf *tab_pixbuf;
	GtkWidget *widget;
	int name_width;
	int text_x_pos, right_edge_pos;
	int left_bumper_piece, fill_piece_1;
	int transition_type_piece, fill_piece_2, right_bumper_piece;
			
	g_assert (NAUTILUS_IS_TABS (tabs));
	
	/* check if there's work to do; if there aren't any tabs or the widget isn't allocated yet, return */
	widget = GTK_WIDGET (tabs);
	if (widget->allocation.width <= 0 || widget->allocation.height <= 0) {
		return;
	}
	if (tabs->details->tab_items == NULL) {
		return;
	}
	
	/* set up the pointers to the two tab items */
	left_tab = (TabItem *) tabs->details->tab_items->data;
	if (tabs->details->tab_items->next) {
		right_tab =  (TabItem *) tabs->details->tab_items->next->data;
	} else {
		right_tab = NULL;
	}
		
	/* determine the images to use for the different tab pieces, given the selected and prelight state
	   of the two tabs. This obviously needs to be rewritten to handle more than two tabs, and should probably use
	   a table instead of the if statements */
	
	if (left_tab->selected) {
		if (left_tab->prelit) {
			left_bumper_piece = TAB_PRELIGHT_LEFT;
			fill_piece_1 = TAB_PRELIGHT_FILL;

		} else {
			left_bumper_piece = TAB_ACTIVE_LEFT;
			fill_piece_1 = TAB_ACTIVE_FILL;
		}

		transition_type_piece = TAB_ACTIVE_INACTIVE;
		
		if (right_tab) {
			if (right_tab->prelit) {
				transition_type_piece = TAB_ACTIVE_PRELIGHT;
				fill_piece_2 = TAB_PRELIGHT_FILL;
				right_bumper_piece = TAB_PRELIGHT_RIGHT;
			} else {
				fill_piece_2 = TAB_INACTIVE_FILL;
				right_bumper_piece = TAB_INACTIVE_RIGHT;
			}
			
		} else {
			transition_type_piece = TAB_ACTIVE_RIGHT;
			fill_piece_2 = TAB_INACTIVE_FILL;
			right_bumper_piece = TAB_INACTIVE_RIGHT;
		}
	} else {
		
		transition_type_piece = TAB_INACTIVE_ACTIVE;

		if (left_tab->prelit) {
			left_bumper_piece = TAB_PRELIGHT_LEFT;
			fill_piece_1 = TAB_PRELIGHT_FILL;
			transition_type_piece = TAB_PRELIGHT_ACTIVE;
		} else {
			left_bumper_piece = TAB_INACTIVE_LEFT;
			fill_piece_1 = TAB_INACTIVE_FILL;
		}

		if (right_tab) {
			fill_piece_2 = right_tab->prelit ? TAB_PRELIGHT_FILL : TAB_ACTIVE_FILL;
			right_bumper_piece =  right_tab->prelit ? TAB_PRELIGHT_RIGHT : TAB_ACTIVE_RIGHT;
		} else {
			fill_piece_2 = TAB_ACTIVE_FILL;
			right_bumper_piece = TAB_ACTIVE_RIGHT;
		}
	}
	
	/* allocate a pixbuf to draw into, and clear it */
	tab_pixbuf = allocate_cleared_pixbuf (widget->allocation.width, widget->allocation.height);
	x_pos = 0;

	/* first, fill the area with the tab background */		
	draw_pixbuf_tiled_aa (tabs->details->tab_piece_images[TAB_BACKGROUND], tab_pixbuf, 0);
	
	/* draw the first tab's left bumper */
	x_pos += draw_tab_piece_aa (tabs, tab_pixbuf, x_pos, 0, -1, left_bumper_piece);
	
	/* measure the text to determine the first tab's size */
	name_width = measure_tab_name (tabs, left_tab->tab_text) + TAB_RIGHT_MARGIN;
	
	/* set up the first tab's rectangle for later hit-testing */
	left_tab->tab_rect.x = x_pos;
	left_tab->tab_rect.y = 0;
	left_tab->tab_rect.width = name_width;
	left_tab->tab_rect.height = widget->allocation.height;
	
	/* draw the first tab's fill area in a loop */
	text_x_pos = x_pos;
	right_edge_pos = x_pos + name_width;
	while (x_pos < right_edge_pos) {
		x_pos += draw_tab_piece_aa (tabs, tab_pixbuf, x_pos, 0, right_edge_pos, fill_piece_1);
	}
	x_pos = right_edge_pos;
	
	/* draw the first tab's label */
	draw_tab_label (tabs, tab_pixbuf, text_x_pos, left_tab->tab_text, left_tab->selected, left_tab->prelit);
	
	/* draw the transition piece */
	x_pos += draw_tab_piece_aa (tabs, tab_pixbuf, x_pos, 0, -1, transition_type_piece);
	/* measure the text to determine the second tab's size */
	if (right_tab != NULL) {
		name_width = measure_tab_name (tabs, right_tab->tab_text) + TAB_RIGHT_MARGIN;
	
		/* adjust position for transition piece - this shouldn't be hardwired */
		x_pos -= 12;
		
		/* set up the second tab's rectangle for hit-testing */
		right_tab->tab_rect.x = x_pos;
		right_tab->tab_rect.y = 0;
		right_tab->tab_rect.width = name_width;
		right_tab->tab_rect.height = widget->allocation.height;
	
		/* draw the second tab's fill area */
		text_x_pos = x_pos;
		right_edge_pos = x_pos + name_width;
		while (x_pos < right_edge_pos) {
			x_pos += draw_tab_piece_aa (tabs, tab_pixbuf, x_pos, 0, right_edge_pos, fill_piece_2);
		}
		x_pos = right_edge_pos;
		
		/* draw the second tab's label */
		draw_tab_label (tabs, tab_pixbuf, text_x_pos, right_tab->tab_text, right_tab->selected, right_tab->prelit);
	
		/* draw the second tab's right bumper piece */
		draw_tab_piece_aa (tabs, tab_pixbuf, x_pos, 0, -1, right_bumper_piece);
	}
		
	/* draw the off-screen buffer to the screen, then release it */
	gdk_pixbuf_render_to_drawable_alpha (tab_pixbuf,
			widget->window,
			0, 0,
			0, 0,
			widget->allocation.width, widget->allocation.height,
			GDK_PIXBUF_ALPHA_BILEVEL, 128,
			GDK_RGB_DITHER_MAX,
			0, 0);

	gdk_pixbuf_unref (tab_pixbuf);
}

/* find a tab with a given name, or return NULL if we can't find one */
static TabItem *
tab_item_find_by_name (NautilusTabs *tabs, const char *name)
{
	GList *iterator;

	g_return_val_if_fail (NAUTILUS_IS_TABS (tabs), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (iterator = tabs->details->tab_items; iterator != NULL; iterator = iterator->next) {
		TabItem *tab_item = iterator->data;

		g_assert (tab_item != NULL);
		g_assert (tab_item->tab_text != NULL);

		if (strcmp (tab_item->tab_text, name) == 0) {
			return tab_item;
		}
	}

	return NULL;
}

/* handle an expose event by drawing the tabs */

static int
nautilus_tabs_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusTabs *tabs;
	
	g_return_val_if_fail (NAUTILUS_IS_TABS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	if (widget->window == NULL) {
		return FALSE;
	}
		
	tabs = NAUTILUS_TABS (widget);

	if (tabs->details->tab_count > 0) {
		draw_all_tabs (tabs);
	}	
	return FALSE;
}

static void
nautilus_tabs_draw (GtkWidget *widget, GdkRectangle *box)
{ 
	/* Clear the widget get the default widget background before drawing our stuff */	
	gdk_window_clear_area (widget->window,
			       0,
			       0,
			       widget->allocation.width,
			       widget->allocation.height);
	draw_all_tabs (NAUTILUS_TABS (widget));
}

/* add a new tab entry, return TRUE if we succeed */

gboolean
nautilus_tabs_add_tab (NautilusTabs *tabs, const char *name, int page_num)
{
	TabItem *new_tab_item;

	g_return_val_if_fail (NAUTILUS_IS_TABS (tabs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	/* Check to see if we already have one with this name, if so, refuse to add it */   
	if (tab_item_find_by_name (tabs, name)) {
		g_warning ("nautilus_tabs_add_view: Trying to add duplicate item '%s'", name);
		return FALSE;
	}
	
	/* allocate a new entry, and initialize it */
	new_tab_item = g_new0 (TabItem, 1);
	new_tab_item->tab_text = g_strdup (name);
	new_tab_item->prelit = FALSE;
	new_tab_item->selected = FALSE;
	new_tab_item->notebook_page = page_num;
	
	/* add it to the list */
	tabs->details->tab_items = g_list_append (tabs->details->tab_items, new_tab_item);
	if (tabs->details->selected_tab == -1) {
		tabs->details->selected_tab = page_num;
		new_tab_item->selected = TRUE;		
	}
	
	tabs->details->tab_count += 1;
	recalculate_size (tabs);
	gtk_widget_queue_draw (GTK_WIDGET (tabs));
	
	return TRUE;
}


/* remove the specified tab entry */
void
nautilus_tabs_remove_tab (NautilusTabs *tabs, const char *name)
{
	GList *next_tab;
	TabItem *tab_item;
	int old_page_number;
	
	g_return_if_fail (NAUTILUS_IS_TABS (tabs));
	g_return_if_fail (name != NULL);

	/* Look up the item */
	tab_item = tab_item_find_by_name (tabs, name);

	if (tab_item == NULL) {
		g_warning ("nautilus_tabs_remove_view: Trying to remove a non-existing item '%s'", name);
		return;
	}
	
	/* Remove the item from the list */
	tabs->details->tab_items = g_list_remove (tabs->details->tab_items, tab_item);

	old_page_number = tab_item->notebook_page;
 	tab_item_destroy (tab_item);
	
	/* decrement all page numbers greater than the one we're removing */
	for (next_tab = tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		if (item->notebook_page >= old_page_number)
			item->notebook_page -= 1;
	}
	
	tabs->details->tab_count -= 1;
	
	recalculate_size (tabs);
	gtk_widget_queue_draw (GTK_WIDGET (tabs));
}

/* prelight a tab, from its associated notebook page number, by setting the prelight flag of
   the proper tab and clearing the others.  Don't allow prelighting of the selected tab */
static void
nautilus_tabs_prelight_tab (NautilusTabs *tabs, int which_tab)
{
	GList *next_tab;
	gboolean is_prelit;
	gboolean changed = FALSE;

	g_return_if_fail (NAUTILUS_IS_TABS (tabs));
	
	for (next_tab = tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		is_prelit = (item->notebook_page == which_tab);
		if (item->prelit != is_prelit && (!item->selected || !is_prelit)) {
			item->prelit = is_prelit;
			changed = TRUE;
		}
	}
	
	if (changed)
		gtk_widget_queue_draw (GTK_WIDGET (tabs));	
}

/* select a tab, in a similar fashion */
static void
nautilus_tabs_select_tab (NautilusTabs *tabs, int which_tab)
{
	GList *next_tab;
	gboolean is_selected;
	gboolean changed = FALSE;

	g_return_if_fail (NAUTILUS_IS_TABS (tabs));

	if (tabs->details->selected_tab	 == which_tab) {
		return;
	}	
	
	tabs->details->selected_tab = which_tab;
	
	for (next_tab = tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		is_selected = (item->notebook_page == which_tab);
		if (item->selected != is_selected) {
			item->selected = is_selected;
			if (is_selected) {
				item->prelit = FALSE;
			}
			changed = TRUE;
		}
	}
	
	if (changed)
		gtk_widget_queue_draw (GTK_WIDGET (tabs));	

	gtk_signal_emit (GTK_OBJECT (tabs),
		signals[TAB_SELECTED], which_tab);	

}

/* handle mouse clicks by selecting a tab if necessary */
static gboolean
nautilus_tabs_press_event (GtkWidget *widget, GdkEventButton *event)
{
	int which_tab;
	
	which_tab = nautilus_tabs_hit_test (NAUTILUS_TABS (widget), event->x, event->y);
	if (which_tab >= 0) {
		nautilus_tabs_select_tab (NAUTILUS_TABS (widget), which_tab);
	}
	return TRUE;
}

/* handle the leave event by turning off the preliting */
static gboolean
nautilus_tabs_leave_event (GtkWidget *tabs_widget, GdkEventCrossing *event)
{
	nautilus_tabs_prelight_tab (NAUTILUS_TABS (tabs_widget), -1);
	return TRUE;
}

/* handle mouse motion events by passing it to the tabs if necessary for pre-lighting */
static gboolean
nautilus_tabs_motion_event (GtkWidget *tabs_widget, GdkEventMotion *event)
{
	int x, y;
	int which_tab;

	gtk_widget_get_pointer (tabs_widget, &x, &y);
	
	
	/* if the motion is in the main tabs, tell them about it */
	which_tab = nautilus_tabs_hit_test (NAUTILUS_TABS (tabs_widget), x, y);

	nautilus_tabs_prelight_tab (NAUTILUS_TABS (tabs_widget), which_tab);
		
	return TRUE;
}
