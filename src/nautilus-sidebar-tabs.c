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
 * This is the tabs widget for the sidebar, which represents closed panels as folder tabs
 */

#include <config.h>
#include "nautilus-sidebar-tabs.h"
#include "nautilus-view-frame.h"

#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-theme.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* constants for the tab piece pixbuf array */

#define	TAB_NORMAL_LEFT			0
#define	TAB_NORMAL_FILL			1
#define TAB_NORMAL_NEXT			2
#define TAB_NORMAL_RIGHT		3
#define TAB_NORMAL_EDGE			4
#define	TAB_PRELIGHT_LEFT		5
#define	TAB_PRELIGHT_FILL		6
#define TAB_PRELIGHT_NEXT		7
#define TAB_PRELIGHT_NEXT_ALT		8

#define TAB_PRELIGHT_RIGHT		9
#define TAB_PRELIGHT_EDGE		10
#define	TAB_ACTIVE_LEFT			11
#define	TAB_ACTIVE_FILL			12
#define TAB_ACTIVE_RIGHT		13
#define TAB_BACKGROUND			14
#define TAB_BACKGROUND_RIGHT		15
#define TAB_ACTIVE_PRELIGHT_LEFT 	16
#define TAB_ACTIVE_PRELIGHT_FILL 	17
#define TAB_ACTIVE_PRELIGHT_RIGHT 	18
#define LAST_TAB_OFFSET			19

/* FIXME: Hard coded font size */
#define DEFAULT_FONT_SIZE 12

#define RIGHT_MARGIN_WIDTH 12

/* data structures */

typedef struct {
	gboolean visible;
	gboolean prelit;
	char *tab_text;
	char *indicator_pixbuf_name;
	GdkPixbuf *indicator_pixbuf;
	int notebook_page;
	Bonobo_EventSource_ListenerId listener_id;
	GtkWidget *tab_view;
	GdkRectangle tab_rect;
} TabItem;

struct NautilusSidebarTabsDetails {
	int tab_count;
	int total_height;
	gboolean title_mode;
	GdkRectangle title_rect;
	EelScalableFont *tab_font;	
	int font_size;
	GdkColor tab_color;
	GdkColor background_color;
	GdkColor line_color;
	GdkColor hilight_color;
	GdkColor prelight_color;
	GdkColor text_color;  
	GdkColor prelit_text_color;

	GdkPixbuf *tab_piece_images[LAST_TAB_OFFSET];
	int tab_height;
	int tab_left_offset;
	char *title;
	gboolean title_prelit;
	GList *tab_items;
};

/* constants */
#define TAB_MARGIN 8
#define TITLE_TAB_OFFSET 8
#define NOMINAL_TAB_HEIGHT 21
#define TAB_H_GAP 8
#define TAB_TOP_GAP 3
#define TAB_ROW_V_OFFSET 3
#define TAB_DEFAULT_LEFT_OFFSET 4
#define THEMED_TAB_TEXT_V_OFFSET 5

/* headers */

static void     nautilus_sidebar_tabs_initialize_class  (NautilusSidebarTabsClass *klass);
static void     nautilus_sidebar_tabs_initialize        (NautilusSidebarTabs      *pixmap);
static int      nautilus_sidebar_tabs_expose            (GtkWidget                *widget,
							 GdkEventExpose           *event);
static void     nautilus_sidebar_tabs_destroy           (GtkObject                *object);
static void     nautilus_sidebar_tabs_size_allocate     (GtkWidget                *widget,
							 GtkAllocation            *allocatoin);
static void     nautilus_sidebar_tabs_load_tab_pieces   (NautilusSidebarTabs      *sidebar_tabs,
							 const char               *tab_piece_directory);
static void     nautilus_sidebar_tabs_unload_tab_pieces (NautilusSidebarTabs      *sidebar_tabs);
static void     draw_or_layout_all_tabs                 (NautilusSidebarTabs      *sidebar_tabs,
							 gboolean                  layout_only);
static TabItem* tab_item_find_by_name                   (NautilusSidebarTabs      *sidebar_tabs,
							 const char               *name);
static void     smooth_font_changed_callback            (gpointer                  callback_data);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSidebarTabs, nautilus_sidebar_tabs, GTK_TYPE_WIDGET)

static void
nautilus_sidebar_tabs_initialize_class (NautilusSidebarTabsClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	
	object_class->destroy = nautilus_sidebar_tabs_destroy;
	widget_class->expose_event = nautilus_sidebar_tabs_expose;
	widget_class->size_allocate = nautilus_sidebar_tabs_size_allocate;
}

/* utilities to set up the text color alternatives */
static void
setup_light_text(NautilusSidebarTabs *sidebar_tabs)
{
	gdk_color_parse ("rgb:ff/ff/ff", &sidebar_tabs->details->text_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->text_color, FALSE, TRUE);
}

static void
setup_dark_text(NautilusSidebarTabs *sidebar_tabs)
{
	gdk_color_parse ("rgb:00/00/00", &sidebar_tabs->details->text_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->text_color, FALSE, TRUE);
}
 
/* load the relevant data from the current theme, which is mainly the tab piece images */
static void
nautilus_sidebar_tabs_load_theme_data (NautilusSidebarTabs *sidebar_tabs)
{
	char *temp_str;
	char *tab_pieces, *tab_piece_path, *tab_piece_theme;
	GdkColor color;
	int intensity;
	
	/* set up the default values */
	sidebar_tabs->details->tab_left_offset = TAB_DEFAULT_LEFT_OFFSET;
	sidebar_tabs->details->tab_height = NOMINAL_TAB_HEIGHT;
	
	/* unload the old theme image if necessary */
	if (sidebar_tabs->details->tab_piece_images[0] != NULL) {
		nautilus_sidebar_tabs_unload_tab_pieces (sidebar_tabs);
	}
				
	/* load the tab_pieces image if necessary */
	tab_pieces = nautilus_theme_get_theme_data ("sidebar", "tab_piece_images");
	tab_piece_theme = nautilus_theme_get_theme_data ("sidebar", "tab_piece_theme");
	
	if (tab_pieces) {
		/* check for "none" to force non-bitmap tabs, necessary since the default has bitmap ones now */
		if (eel_strcmp (tab_pieces, "none") == 0) {
			tab_piece_path = NULL;	
		}  else {
			if (tab_piece_theme) {
				tab_piece_path = nautilus_theme_get_image_path_from_theme (tab_pieces, tab_piece_theme);		
			} else {
				tab_piece_path = nautilus_theme_get_image_path (tab_pieces);
			}
		}
		g_free (tab_pieces);
		g_free (tab_piece_theme);
		
		if (tab_piece_path) {
			nautilus_sidebar_tabs_load_tab_pieces (sidebar_tabs, tab_piece_path);
			g_free (tab_piece_path);
			
			if (sidebar_tabs->details->tab_piece_images[0]) {
				/* load the left offset */
				temp_str = nautilus_theme_get_theme_data ("sidebar", "left_offset");
				if (temp_str) {
					sidebar_tabs->details->tab_left_offset = atoi(temp_str);
					g_free (temp_str);
				}
				sidebar_tabs->details->tab_height = gdk_pixbuf_get_height (sidebar_tabs->details->tab_piece_images[0]);
				
				/* set the text color according to the pixbuf */
				eel_gdk_pixbuf_average_value (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_FILL], &color);
				intensity = (((color.red >> 8) * 77) + ((color.green >> 8) * 150) + ((color.blue >> 8) * 28)) >> 8;	

				if (intensity < 160) {
					setup_light_text (sidebar_tabs);
				} else {
					setup_dark_text (sidebar_tabs);
				}
			}
		}	
	}	

	/* unload the old font if necessary */
	if (sidebar_tabs->details->tab_font != NULL) {
		gtk_object_unref (GTK_OBJECT (sidebar_tabs->details->tab_font));
		sidebar_tabs->details->tab_font = NULL;
	}

	smooth_font_changed_callback (sidebar_tabs);

	/* FIXME: Hard coded font size */
	sidebar_tabs->details->font_size = DEFAULT_FONT_SIZE;
}

/* Use the font from preferences */
static void
smooth_font_changed_callback (gpointer callback_data)
{
	EelScalableFont *new_font;
	NautilusSidebarTabs *sidebar_tabs;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (callback_data));

	sidebar_tabs = NAUTILUS_SIDEBAR_TABS (callback_data);
	new_font = nautilus_global_preferences_get_default_smooth_bold_font ();

	if (sidebar_tabs->details->tab_font != NULL) {
		gtk_object_unref (GTK_OBJECT (sidebar_tabs->details->tab_font));
		sidebar_tabs->details->tab_font = NULL;
	}

	sidebar_tabs->details->tab_font = new_font;

	gtk_widget_queue_resize (GTK_WIDGET (sidebar_tabs));
}

/* initialize a newly allocated sidebar tabs object */
static void
nautilus_sidebar_tabs_initialize (NautilusSidebarTabs *sidebar_tabs)
{
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(sidebar_tabs), GTK_NO_WINDOW);
	
	sidebar_tabs->details = g_new0 (NautilusSidebarTabsDetails, 1);
		
	/* set up the default colors used for the structured (non-themed) tabs */
	gdk_color_parse ("rgb:99/99/99", &sidebar_tabs->details->tab_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->tab_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:ee/ee/ee", &sidebar_tabs->details->prelight_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->prelight_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:ff/ff/ff", &sidebar_tabs->details->background_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->background_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:00/00/00", &sidebar_tabs->details->line_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->line_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:d6/d6/d6", &sidebar_tabs->details->hilight_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->hilight_color, FALSE, TRUE);
	
	setup_light_text(sidebar_tabs);
	nautilus_sidebar_tabs_load_theme_data (sidebar_tabs);
	
	/* add callback to be notified for theme changes */
	eel_preferences_add_callback(NAUTILUS_PREFERENCES_THEME, 
				     (EelPreferencesCallback) nautilus_sidebar_tabs_load_theme_data, 
				     sidebar_tabs);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
				      smooth_font_changed_callback,
				      sidebar_tabs);
	
	sidebar_tabs->details->title_prelit = FALSE;
}

GtkWidget *
nautilus_sidebar_tabs_new (void)
{
	return gtk_widget_new (nautilus_sidebar_tabs_get_type (), NULL);
}

static Bonobo_PropertyBag
get_property_bag (TabItem *item)
{
	Bonobo_Control control;
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;

	control = nautilus_view_frame_get_control (NAUTILUS_VIEW_FRAME (item->tab_view));	
	if (control == NULL) {
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_init (&ev);
	property_bag = Bonobo_Control_getProperties (control, &ev);
	if (BONOBO_EX (&ev)) {
		property_bag = CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);
	return property_bag;
}

/* utility to destroy all the storage used by a tab item */
static void
tab_item_destroy (TabItem *item)
{
	Bonobo_PropertyBag property_bag;
	
	g_free (item->tab_text);
	g_free (item->indicator_pixbuf_name);
	
	if (item->indicator_pixbuf != NULL) {
		gdk_pixbuf_unref (item->indicator_pixbuf);
	}

	if (item->listener_id != 0) {
		property_bag = get_property_bag (item);
		if (property_bag != CORBA_OBJECT_NIL) {	
			bonobo_event_source_client_remove_listener
				(property_bag, item->listener_id, NULL);
			bonobo_object_release_unref (property_bag, NULL);
		}
	}
	g_free (item);
}

static void
tab_item_destroy_cover (gpointer item, gpointer callback_data)
{
	g_assert (callback_data == NULL);
	tab_item_destroy (item);
}

static void
nautilus_sidebar_tabs_destroy (GtkObject *object)
{
	NautilusSidebarTabs *sidebar_tabs = NAUTILUS_SIDEBAR_TABS(object);
   	
	/* deallocate the tab piece images, if any */
	if (sidebar_tabs->details->tab_piece_images[0] != NULL) {
		nautilus_sidebar_tabs_unload_tab_pieces (sidebar_tabs);
	}
	
	if (sidebar_tabs->details->tab_font != NULL) {
		gtk_object_unref (GTK_OBJECT (sidebar_tabs->details->tab_font));
		sidebar_tabs->details->tab_font = NULL;
	}
	
	/* release the tab list, if any */
	eel_g_list_free_deep_custom (sidebar_tabs->details->tab_items,
				     tab_item_destroy_cover,
				     NULL);
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME, 
					 (EelPreferencesCallback) nautilus_sidebar_tabs_load_theme_data, 
					 sidebar_tabs);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
					 smooth_font_changed_callback,
					 sidebar_tabs);
	
	g_free (sidebar_tabs->details);
  	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* unload the tab piece images, if any */
static void
nautilus_sidebar_tabs_unload_tab_pieces (NautilusSidebarTabs *sidebar_tabs)
{
	int index;
	for (index = 0; index < LAST_TAB_OFFSET; index++) {
		if (sidebar_tabs->details->tab_piece_images[index]) {
			gdk_pixbuf_unref (sidebar_tabs->details->tab_piece_images[index]);
			sidebar_tabs->details->tab_piece_images[index] = NULL;			
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
nautilus_sidebar_tabs_load_tab_pieces (NautilusSidebarTabs *sidebar_tabs, const char* tab_piece_directory)
{
	sidebar_tabs->details->tab_piece_images[TAB_NORMAL_LEFT]  = load_tab_piece (tab_piece_directory, "left-bumper");
	sidebar_tabs->details->tab_piece_images[TAB_NORMAL_FILL]  = load_tab_piece (tab_piece_directory, "fill");
	sidebar_tabs->details->tab_piece_images[TAB_NORMAL_NEXT]  = load_tab_piece (tab_piece_directory, "middle-normal-normal");
	sidebar_tabs->details->tab_piece_images[TAB_NORMAL_RIGHT] = load_tab_piece (tab_piece_directory, "right-bumper");
	sidebar_tabs->details->tab_piece_images[TAB_NORMAL_EDGE]  = load_tab_piece (tab_piece_directory, "right-top");
	
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_LEFT]  = load_tab_piece (tab_piece_directory, "left-bumper-prelight");
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_FILL]  = load_tab_piece (tab_piece_directory, "fill-prelight");
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_NEXT]  = load_tab_piece (tab_piece_directory, "middle-prelight-normal");
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_NEXT_ALT]  = load_tab_piece (tab_piece_directory, "middle-normal-prelight");	
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_RIGHT] = load_tab_piece (tab_piece_directory, "right-bumper-prelight");
	sidebar_tabs->details->tab_piece_images[TAB_PRELIGHT_EDGE]  = load_tab_piece (tab_piece_directory, "right-top-prelight");
	
	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_LEFT]  = load_tab_piece (tab_piece_directory, "left-bumper-active");
	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_FILL]  = load_tab_piece (tab_piece_directory, "fill-active");
	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_RIGHT] = load_tab_piece (tab_piece_directory, "right-top-active");
	
	sidebar_tabs->details->tab_piece_images[TAB_BACKGROUND]   = load_tab_piece (tab_piece_directory, "fill-empty-space");
	sidebar_tabs->details->tab_piece_images[TAB_BACKGROUND_RIGHT]  = load_tab_piece (tab_piece_directory, "right-empty-space");

	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_PRELIGHT_LEFT]  = load_tab_piece (tab_piece_directory, "left-bumper-active-prelight");
	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_PRELIGHT_FILL]  = load_tab_piece (tab_piece_directory, "fill-active-prelight");
	sidebar_tabs->details->tab_piece_images[TAB_ACTIVE_PRELIGHT_RIGHT] = load_tab_piece (tab_piece_directory, "right-top-active-prelight");

}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it.  */

int nautilus_sidebar_tabs_hit_test (NautilusSidebarTabs *sidebar_tabs, int x, int y)
{
	GList *current_item;
	TabItem *tab_item;
	GdkRectangle *rect_ptr;
	int result;
	
	sidebar_tabs->details->total_height = sidebar_tabs->details->tab_height;
	current_item = sidebar_tabs->details->tab_items;
	
	/* if we're in title mode, see if we're in the title rectangle */
	if (sidebar_tabs->details->title_mode) {
		rect_ptr = &sidebar_tabs->details->title_rect;
		
		if ((x >= rect_ptr->x) && (x < rect_ptr->x + rect_ptr->width) &&
		    (y >= rect_ptr->y) && (y<rect_ptr->y + rect_ptr->height))
			return 0;
		return -1;
	}

	if (current_item == NULL)
		return -1;
			
	/* loop through the items, seeing it the passed in point is in one of the rectangles */
	tab_item = (TabItem*) current_item->data;
	if (!tab_item->visible && current_item->next) {
		tab_item = (TabItem*) current_item->next->data;
	}
			
	result = -1;
	while (current_item != NULL) {
		tab_item = (TabItem*) current_item->data;
		rect_ptr = &tab_item->tab_rect;

		/* hit test even when invisible to provide an easy way to
		 * toggle visibility
		 */
		if ((x >= rect_ptr->x) && (x < rect_ptr->x + rect_ptr->width) &&
		    (y >= rect_ptr->y) && (y< rect_ptr->y + rect_ptr->height))
		   	result = tab_item->notebook_page;
		 	
		current_item = current_item->next;
	}
	return result;
}

/* utility routine to update the total height of all of the tabs */
static int
measure_total_height (NautilusSidebarTabs *sidebar_tabs)
{
	/* relayout the tabs and report the result */
	draw_or_layout_all_tabs (sidebar_tabs, TRUE);
	return sidebar_tabs->details->total_height;
}

/* resize the widget based on the number of tabs */

static void
recalculate_size(NautilusSidebarTabs *sidebar_tabs)
{
	GtkWidget *widget = GTK_WIDGET (sidebar_tabs);
	
	/* layout tabs to make sure height measurement is valid */
	measure_total_height (sidebar_tabs);
  	
	widget->requisition.width = widget->parent ? widget->parent->allocation.width: 136;
	if (sidebar_tabs->details->title_mode)
		widget->requisition.height = sidebar_tabs->details->tab_height;
	else
		widget->requisition.height = sidebar_tabs->details->total_height + TAB_TOP_GAP;
	gtk_widget_queue_resize (widget);
}

static void
nautilus_sidebar_tabs_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusSidebarTabs *sidebar_tabs = NAUTILUS_SIDEBAR_TABS(widget);
	
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	/* layout tabs to make sure height measurement is valid */
	measure_total_height (sidebar_tabs);
 	
	if (!sidebar_tabs->details->title_mode) {
		gint delta_height = widget->allocation.height - (sidebar_tabs->details->total_height + TAB_TOP_GAP);
		widget->allocation.height -= delta_height;
		widget->allocation.y += delta_height;
	}
}

/* convenience routine to composite an image with the proper clipping */
static void
pixbuf_composite (GdkPixbuf *source, GdkPixbuf *destination, int x_offset, int y_offset, int alpha)
{
	int source_width, source_height, dest_width, dest_height;
	double float_x_offset, float_y_offset;
	
	source_width  = gdk_pixbuf_get_width (source);
	source_height = gdk_pixbuf_get_height (source);
	dest_width  = gdk_pixbuf_get_width (destination);
	dest_height = gdk_pixbuf_get_height (destination);
	
	float_x_offset = x_offset;
	float_y_offset = y_offset;
	
	/* clip to the destination size */
	if ((x_offset + source_width) > dest_width) {
		source_width = dest_width - x_offset;
	}
	if ((y_offset + source_height) > dest_height) {
		source_height = dest_height - y_offset;
	}
	
	gdk_pixbuf_composite (source, destination, x_offset, y_offset, source_width, source_height,
			      float_x_offset, float_y_offset, 1.0, 1.0, GDK_PIXBUF_ALPHA_BILEVEL, alpha);
}

/* draw a single tab using the default, non-themed approach */
static int
draw_one_tab_plain (NautilusSidebarTabs *sidebar_tabs, GdkGC *gc, char *tab_name,
		    GdkPixbuf *indicator_pixbuf, int x, int y, gboolean prelight_flag, GdkRectangle *tab_rect)
{  
	int tab_bottom;
	int tab_right;
	int indicator_width;
	EelDimensions name_dimensions;
	int total_width;
	GtkWidget *widget;
	GdkPixbuf *temp_pixbuf;
	GdkColor *foreground_color;
	
	g_assert (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	if (indicator_pixbuf != NULL) {
		indicator_width = gdk_pixbuf_get_width (indicator_pixbuf);
	} else {
		indicator_width = 0;
	}

	/* measure the name and compute the bounding box */
	name_dimensions = eel_scalable_font_measure_text (sidebar_tabs->details->tab_font, 
							  sidebar_tabs->details->font_size,
							  tab_name,
							  strlen (tab_name));
	
	total_width = name_dimensions.width + indicator_width + 2 * TAB_MARGIN;

	widget = GTK_WIDGET (sidebar_tabs);
	
	/* set up the tab rectangle for hit-testing */
	if (tab_rect) {
		tab_rect->x = x;
		tab_rect->y = y;
		tab_rect->width = total_width;
		tab_rect->height = sidebar_tabs->details->tab_height;
	}
	
	/* FIXME bugzilla.gnome.org 40668: 
	 * we must "ellipsize" the name if it doesn't fit, for now, assume it does 
	 */
	 
	foreground_color = prelight_flag ? &sidebar_tabs->details->prelight_color : &sidebar_tabs->details->tab_color;
		
	/* fill the tab rectangle with the tab color */
	gdk_gc_set_foreground (gc, foreground_color);
	gdk_draw_rectangle (widget->window, gc, TRUE, x, y + 1, total_width, sidebar_tabs->details->tab_height - 1); 
	

	/* draw the border */
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, x + 1, y, x + total_width - 2, y);
	gdk_draw_line(widget->window, gc, x, y + 1, x, y + sidebar_tabs->details->tab_height - 1);
	gdk_draw_line(widget->window, gc, x + total_width - 1, y + 1, x + total_width - 1, y + sidebar_tabs->details->tab_height - 1);
	
	/* draw the highlights for extra dimensionality */
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->hilight_color);  
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + total_width - 3, y + 2);
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + 2, y + sidebar_tabs->details->tab_height - 1);
	
	/* allocate the pixbuf and fill it with the background color */
	temp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, name_dimensions.width + indicator_width + 1, name_dimensions.height + 1);
	eel_gdk_pixbuf_fill_rectangle_with_color (temp_pixbuf,
						  eel_gdk_pixbuf_whole_pixbuf,
						  eel_gdk_color_to_rgb (foreground_color));

	/* draw the indicator if necessary */
	if (indicator_pixbuf) {
		pixbuf_composite (indicator_pixbuf, temp_pixbuf, 0, 0, 255);
	}
	
	/* draw the name into the pixbuf using anti-aliased text */
	eel_scalable_font_draw_text (sidebar_tabs->details->tab_font, temp_pixbuf, 
				     1 + indicator_width, 1, 
				     eel_gdk_pixbuf_whole_pixbuf,
				     sidebar_tabs->details->font_size,
				     tab_name, strlen (tab_name),
				     prelight_flag ? EEL_RGB_COLOR_WHITE : EEL_RGB_COLOR_BLACK,
				     EEL_OPACITY_FULLY_OPAQUE);
	
	eel_scalable_font_draw_text (sidebar_tabs->details->tab_font, temp_pixbuf, 
				     indicator_width, 0,
				     eel_gdk_pixbuf_whole_pixbuf,
				     sidebar_tabs->details->font_size,
				     tab_name, strlen (tab_name),
				     prelight_flag ? EEL_RGB_COLOR_BLACK : EEL_RGB_COLOR_WHITE,
				     EEL_OPACITY_FULLY_OPAQUE);
	
	/* blit the pixbuf to the drawable, then release it */

	gdk_pixbuf_render_to_drawable_alpha (temp_pixbuf,
					     widget->window,
					     0, 0,
					     x + TAB_MARGIN, y + 5,
					     name_dimensions.width + indicator_width + 1, name_dimensions.height + 1,
					     GDK_PIXBUF_ALPHA_BILEVEL, 128,
					     GDK_RGB_DITHER_MAX,
					     0, 0);
	
	gdk_pixbuf_unref (temp_pixbuf);	
	
	/* draw the bottom lines */
	tab_bottom = y + sidebar_tabs->details->tab_height - 1;
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	tab_right = x + 2*TAB_MARGIN + name_dimensions.width + indicator_width;
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, tab_right, tab_bottom, widget->parent->allocation.width, tab_bottom);
	gdk_draw_line(widget->window, gc, 0, tab_bottom, x, tab_bottom);
	
	return name_dimensions.width + indicator_width + 2 * TAB_MARGIN;
}

/* utility to draw a single tab piece into a pixbuf */
static int
draw_tab_piece_aa (NautilusSidebarTabs *sidebar_tabs, GdkPixbuf *dest_pixbuf, int x, int y, int x_limit, int which_piece)
{
	GtkWidget *widget;
	GdkPixbuf *pixbuf;
	int width, height;
	int dest_width, dest_height;
	int blit_width, blit_height;
		
	widget = GTK_WIDGET (sidebar_tabs);
	pixbuf = sidebar_tabs->details->tab_piece_images[which_piece];
	
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

/* draw a single tab using the theme image to define it's appearance.  This does not
   draw the right edge of the tab, as we don't have enough info about the next tab
   to do it properly at this time.  We draw into an offscreen pixbuf so we can get nice anti-aliased text */
static int
draw_one_tab_themed (NautilusSidebarTabs *sidebar_tabs, GdkPixbuf *tab_pixbuf, GdkPixbuf *indicator_pixbuf,
		     char *tab_name, int x, int y, gboolean prelight_flag, 
		     gboolean first_flag, gboolean prev_invisible, int text_h_offset,
		     GdkRectangle *tab_rect)
{  
	GtkWidget *widget;
	EelDimensions name_dimensions;
	int piece_width, tab_width;
	int current_pos, right_edge_pos;
	int text_x_pos, left_width;
	int highlight_offset;
	int text_x;
	int text_y;
	int indicator_width;
	
	if (indicator_pixbuf != NULL) {
		indicator_width = gdk_pixbuf_get_width (indicator_pixbuf);
	} else {
		indicator_width = 0;
	}

	left_width = 0;
	
	widget = GTK_WIDGET (sidebar_tabs);
	highlight_offset = prelight_flag && !sidebar_tabs->details->title_mode ? TAB_PRELIGHT_LEFT : 0; 
	if (sidebar_tabs->details->title_mode) {
		highlight_offset += TAB_ACTIVE_LEFT - TAB_NORMAL_LEFT;
		if (prelight_flag) {
			highlight_offset += TAB_ACTIVE_PRELIGHT_LEFT - TAB_ACTIVE_LEFT;
		}
	}
	
	/* measure the size of the name */
	name_dimensions = eel_scalable_font_measure_text (sidebar_tabs->details->tab_font,
							  sidebar_tabs->details->font_size,
							  tab_name,
							  strlen (tab_name));
	/* draw the left edge piece */
	current_pos = x - widget->allocation.x;
	if (first_flag) {
		left_width = draw_tab_piece_aa (sidebar_tabs, tab_pixbuf, x, y - widget->allocation.y,
						-1, TAB_NORMAL_LEFT + highlight_offset);
		current_pos += left_width;
	}
	
	/* draw the middle portion in a loop */	
	text_x_pos = current_pos;
	right_edge_pos = current_pos + name_dimensions.width + indicator_width;
	while (current_pos < right_edge_pos) {
		piece_width = draw_tab_piece_aa (sidebar_tabs, tab_pixbuf, current_pos, y - widget->allocation.y, 
						 right_edge_pos, TAB_NORMAL_FILL + highlight_offset);
		current_pos += piece_width;
	}
				
	if (!first_flag && !prev_invisible) {
		text_x_pos += text_h_offset;
	}

	text_x = text_x_pos + 1;
	text_y = y - widget->allocation.y + THEMED_TAB_TEXT_V_OFFSET;
	
	/* make sure we can at least draw some of it */
	if (text_x < gdk_pixbuf_get_width (tab_pixbuf)) {	
		if (indicator_pixbuf) {
			
			pixbuf_composite (indicator_pixbuf, tab_pixbuf, text_x, text_y, 255);
			text_x += indicator_width;
		} 

		eel_scalable_font_draw_text (sidebar_tabs->details->tab_font, tab_pixbuf, 
					     text_x, text_y,
					     eel_gdk_pixbuf_whole_pixbuf,
					     sidebar_tabs->details->font_size,
					     tab_name, strlen (tab_name),
					     EEL_RGB_COLOR_BLACK,
					     EEL_OPACITY_FULLY_OPAQUE);
		text_x -= 1;
		text_y -= 1;

		eel_scalable_font_draw_text (sidebar_tabs->details->tab_font, tab_pixbuf,
					     text_x, text_y,
					     eel_gdk_pixbuf_whole_pixbuf,
					     sidebar_tabs->details->font_size,
					     tab_name, strlen (tab_name),
					     EEL_RGB_COLOR_WHITE,
					     EEL_OPACITY_FULLY_OPAQUE);
	}
	
	/* set up the bounds rectangle for later hit-testing */
	if (tab_rect) {
		tab_rect->x = x;
		tab_rect->y = y;
		tab_rect->width = current_pos - x + indicator_width;
		tab_rect->height = sidebar_tabs->details->tab_height;
	}
	
	/* return the total tab width */
	tab_width = left_width + name_dimensions.width + indicator_width;
	return tab_width;	
}

static int
get_text_offset (void)
{
	int offset;
	char *temp_str;
	
	temp_str = nautilus_theme_get_theme_data ("sidebar", "text_h_offset");
	if (temp_str) {
		offset = atoi (temp_str);
		g_free (temp_str);
	} else {
		offset = 0;
	}

	return offset;
}

/* utility to return the width of the passed in tab */
static int
get_tab_width (NautilusSidebarTabs *sidebar_tabs, TabItem *this_tab, gboolean is_themed, gboolean first_flag)
{
	int edge_width, indicator_width;
	EelDimensions name_dimensions;
	
	if (this_tab == NULL)
		return 0;
	
	if (this_tab->indicator_pixbuf != NULL) {
		indicator_width = gdk_pixbuf_get_width (this_tab->indicator_pixbuf);
	} else {
		indicator_width = 0;
	}
	
	if (is_themed) {
		if (first_flag) {
			edge_width = gdk_pixbuf_get_width (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_LEFT]);
		} else {
			edge_width = 0;
		}

		name_dimensions = eel_scalable_font_measure_text (sidebar_tabs->details->tab_font,
								  sidebar_tabs->details->font_size,
								  this_tab->tab_text,
								  strlen (this_tab->tab_text));
	
	} else {	
		edge_width = 2 * TAB_MARGIN;
		name_dimensions.width = gdk_string_width (GTK_WIDGET (sidebar_tabs)->style->font, this_tab->tab_text);
	}		
	return name_dimensions.width + edge_width + indicator_width;
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

/* draw or hit test all of the currently visible tabs */
static void
draw_or_layout_all_tabs (NautilusSidebarTabs *sidebar_tabs, gboolean layout_only)
{
	GdkGC		*temp_gc;
	GdkPixbuf	*tab_pixbuf;
	TabItem		*prev_item;
	int		tab_width;
	int		extra_fill;
	GList		*next_tab;
	GtkWidget	*widget;  
	int		x_pos, y_pos;
	int		highlight_offset;
	int		last_x_pos, last_y_pos;
	int		cur_x_pos, extra_width;
	int		y_top, fill_height;
	int		total_width, total_height, tab_select;
	int		piece_width, text_h_offset;
	int		new_tab_width, end_piece_width;
	int		dest_x, dest_y;
	
	gboolean	is_themed, changed_rows, needs_compositing;
	gboolean	first_flag, prev_invisible;
	
	g_assert (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	temp_gc = NULL;
	tab_pixbuf = NULL;
	text_h_offset = 0;

	next_tab = sidebar_tabs->details->tab_items;
			
	widget = GTK_WIDGET (sidebar_tabs);  
	if (widget->allocation.width <= 0 || widget->allocation.height <= 0)
		return;

	is_themed = sidebar_tabs->details->tab_piece_images[0] != NULL;
	if (is_themed) {
		text_h_offset = get_text_offset ();
		
		/* allocate a pixbuf to draw into, and clear it */
		if (!layout_only) {
			tab_pixbuf = allocate_cleared_pixbuf (widget->allocation.width, widget->allocation.height);
		}
	}

	/* set up the initial positions from the widget */
	x_pos = widget->allocation.x + sidebar_tabs->details->tab_left_offset;
	y_pos = widget->allocation.y + widget->allocation.height - sidebar_tabs->details->tab_height;
	total_height = sidebar_tabs->details->tab_height;
	total_width = widget->allocation.x + widget->allocation.width;
	
	if (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_RIGHT]) {
		end_piece_width = gdk_pixbuf_get_width (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_RIGHT]);
	} else {
		end_piece_width = 0;
	}
		
	/* allocate a graphic context and clear the space below the top tabs to the background color */
	y_top = widget->allocation.y + sidebar_tabs->details->tab_height + TAB_TOP_GAP;
	fill_height = widget->allocation.y + widget->allocation.height - y_top;

	if (!layout_only) {
		temp_gc = gdk_gc_new (widget->window); 
	
		if (is_themed) {
			g_assert (tab_pixbuf != NULL);
			draw_pixbuf_tiled_aa (sidebar_tabs->details->tab_piece_images[TAB_BACKGROUND], tab_pixbuf, sidebar_tabs->details->tab_height + TAB_TOP_GAP);
		} else {
			gdk_gc_set_foreground (temp_gc, &sidebar_tabs->details->background_color);
			gdk_draw_rectangle (widget->window, temp_gc, TRUE, widget->allocation.x, y_top, widget->allocation.width, fill_height); 
		}
	}
	
	/* here's the main loop where we draw as many tabs in a row as will fit.  Since we need to know
	   the state of the next tab to draw the right edge of the current one, things are kind of 
	   complicated - we draw the right edge after positioning the next tab */
		
	first_flag = TRUE; /* keep track of first tab in a row, since it has a different left edge */
	prev_item = NULL;
	while (next_tab != NULL) {
		TabItem *this_item = next_tab->data;
		
		if (this_item->visible && !layout_only) {
			prev_invisible = prev_item && !prev_item->visible;
			if (is_themed) {
				g_assert (tab_pixbuf != NULL);
				draw_one_tab_themed (sidebar_tabs, tab_pixbuf, this_item->indicator_pixbuf, this_item->tab_text, x_pos, y_pos,
						     this_item->prelit, first_flag, prev_invisible,
						     text_h_offset, &this_item->tab_rect);
			} else {
				g_assert (temp_gc != NULL);
				draw_one_tab_plain (sidebar_tabs, temp_gc,  this_item->tab_text, this_item->indicator_pixbuf,
						    x_pos, y_pos, this_item->prelit, &this_item->tab_rect);		
			}
		} 
		tab_width = get_tab_width (sidebar_tabs, this_item, is_themed, first_flag);
		
		first_flag = FALSE;
		
		prev_item = this_item;
		next_tab = next_tab->next;
		if (next_tab != NULL)
			this_item = (TabItem*) next_tab->data;	
		else
			this_item = NULL;
			
		last_x_pos = x_pos;
		last_y_pos = y_pos;
		
		/* bump the x-position, and see if it fits */
		x_pos += tab_width;
		if (!is_themed)
			x_pos += TAB_H_GAP;
		
		new_tab_width = get_tab_width (sidebar_tabs, this_item, is_themed, FALSE) - text_h_offset;		
		if ((x_pos + new_tab_width) > (total_width - end_piece_width - RIGHT_MARGIN_WIDTH)) {
			/* wrap to the next line */
			x_pos = widget->allocation.x + sidebar_tabs->details->tab_left_offset;     
			if (is_themed)
				y_pos -= sidebar_tabs->details->tab_height;
			else
				y_pos -= sidebar_tabs->details->tab_height + TAB_ROW_V_OFFSET;
			
			first_flag = TRUE;
			if ((next_tab != NULL) && ((next_tab->next != NULL) || this_item->visible)) {
				total_height += sidebar_tabs->details->tab_height;
				if (!is_themed)
					total_height += TAB_ROW_V_OFFSET;
			}
		}            
	
		/* finish drawing the right edge */
		if (is_themed) {	
			changed_rows = y_pos != last_y_pos;
			needs_compositing = FALSE;
			
			if (changed_rows && this_item != NULL /* && this_item->visible */) {
				if (prev_item->prelit) {
					tab_select = TAB_PRELIGHT_RIGHT;
					extra_fill = TAB_PRELIGHT_FILL;
				} else {
					tab_select = TAB_NORMAL_RIGHT;
					extra_fill = TAB_NORMAL_FILL;
				}
				
				/* we must do some extra drawing of the fill pattern here, to stretch it out to the edge */
				extra_width = gdk_pixbuf_get_width (sidebar_tabs->details->tab_piece_images[extra_fill]);
				cur_x_pos = last_x_pos + tab_width;				
				last_x_pos = total_width - gdk_pixbuf_get_width (sidebar_tabs->details->tab_piece_images[tab_select]) - tab_width; 
				
				if (prev_item->visible) {
					int x = widget->allocation.x;
					int y = widget->allocation.y;
					
					while (cur_x_pos < total_width) {
						if (!layout_only)
							draw_tab_piece_aa (sidebar_tabs, tab_pixbuf, cur_x_pos - x,
									   last_y_pos - y, total_width, extra_fill);				
						cur_x_pos += extra_width;
					}
				}
			} else if ((this_item == NULL) || !this_item->visible) {
				tab_select = prev_item->prelit ? TAB_PRELIGHT_EDGE : TAB_NORMAL_EDGE;
				needs_compositing = TRUE;
			} else {
				if (prev_item->prelit) {
					tab_select = TAB_PRELIGHT_NEXT;
				} else if (this_item->prelit) {
					tab_select = TAB_PRELIGHT_NEXT_ALT;	
				} else {
					tab_select = TAB_NORMAL_NEXT;		
				}
			}	
			if (!prev_item->visible) {
				if (this_item && this_item->prelit) {
					highlight_offset = TAB_PRELIGHT_LEFT;
				} else {
					highlight_offset = 0;
				}
				tab_select = (!changed_rows && this_item) ? TAB_NORMAL_LEFT + highlight_offset : -1;
			}
			piece_width = 0;
			if (tab_select >= 0) {	
				GdkPixbuf *temp_pixbuf = sidebar_tabs->details->tab_piece_images[tab_select];
				piece_width = gdk_pixbuf_get_width (temp_pixbuf);
				dest_x = last_x_pos + tab_width - widget->allocation.x;
				dest_y = last_y_pos - widget->allocation.y;
				
				if (!layout_only) {
					if (needs_compositing) {
						int src_width = gdk_pixbuf_get_width (temp_pixbuf);
						int src_height = gdk_pixbuf_get_height (temp_pixbuf);
						int dest_width = gdk_pixbuf_get_width (tab_pixbuf);
						int dest_height = gdk_pixbuf_get_height (tab_pixbuf);
						
						/* clip it in both dimensions */
						if ((dest_x + src_width) > dest_width) {
							src_width = dest_width - dest_x;
						}
						if ((dest_y + src_height) > dest_height) {
							src_height = dest_height - dest_y;
						}
						
						gdk_pixbuf_composite (temp_pixbuf, tab_pixbuf,
								      dest_x,
								      dest_y,
								      src_width,
								      src_height,
								      dest_x,
								      dest_y,
								      1.0, 1.0,
								      GDK_INTERP_BILINEAR,
								      0xFF);	
					
					} else {
						piece_width = draw_tab_piece_aa (sidebar_tabs, tab_pixbuf,  
										 				 dest_x, dest_y, -1, tab_select);
					}
				} 
				prev_item->tab_rect.width = last_x_pos + tab_width + piece_width - prev_item->tab_rect.x;
			} 
				
			if (!changed_rows)
				x_pos += piece_width;
		}
	}  
	
	/* draw the off-screen buffer to the screen, then release it */
	if (!layout_only) {
		gdk_gc_unref(temp_gc);
		if (is_themed) {
			/* draw the pixbuf onto the widget and release it */
			gdk_pixbuf_render_to_drawable_alpha (tab_pixbuf,
							     widget->window,
							     0, 0,
							     widget->allocation.x, widget->allocation.y,
							     widget->allocation.width, widget->allocation.height,
							     GDK_PIXBUF_ALPHA_BILEVEL, 128,
							     GDK_RGB_DITHER_MAX,
							     0, 0);

			gdk_pixbuf_unref (tab_pixbuf);
		}
	}
	
	sidebar_tabs->details->total_height = total_height;
}

/* find a tab with a given name, or return NULL if we can't find one */
static TabItem *
tab_item_find_by_name (NautilusSidebarTabs *sidebar_tabs, const char *name)
{
	GList *node;
	TabItem *tab_item;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (node = sidebar_tabs->details->tab_items; node != NULL; node = node->next) {
		tab_item = node->data;

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
nautilus_sidebar_tabs_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusSidebarTabs *sidebar_tabs;
	int tab_width, text_offset;
	
	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	if (widget->window == NULL) {
		return FALSE;
	}

	if (widget->parent->allocation.width <= 4) {
		return FALSE;
	}
		
	sidebar_tabs = NAUTILUS_SIDEBAR_TABS (widget);

	text_offset = get_text_offset ();
	
	/* draw the tabs */
	if (sidebar_tabs->details->title_mode) {
		GdkPixbuf *pixbuf;
		GdkGC* temp_gc = gdk_gc_new(widget->window); 
		int tab_height;
		int x_pos = widget->allocation.x;
		int y_pos = widget->allocation.y;
		
		if (sidebar_tabs->details->tab_piece_images[0]) {
			
			tab_height = gdk_pixbuf_get_height (sidebar_tabs->details->tab_piece_images[0]);
			pixbuf = allocate_cleared_pixbuf (widget->allocation.width, tab_height);
			
			tab_width = draw_one_tab_themed (sidebar_tabs, pixbuf, NULL, sidebar_tabs->details->title, 0, 0, 
							 sidebar_tabs->details->title_prelit, TRUE, FALSE, text_offset, &sidebar_tabs->details->title_rect);
			/* draw the right edge */
			draw_tab_piece_aa (sidebar_tabs, pixbuf,  tab_width, 0, -1,
					   sidebar_tabs->details->title_prelit ? TAB_ACTIVE_PRELIGHT_RIGHT : TAB_ACTIVE_RIGHT);
			
			/* transfer the pixmap to the screen */
			gdk_pixbuf_render_to_drawable_alpha (pixbuf,
							     widget->window,
							     0, 0,
							     x_pos, y_pos,
							     widget->allocation.width, tab_height,
							     GDK_PIXBUF_ALPHA_BILEVEL, 128,
							     GDK_RGB_DITHER_MAX,
							     0, 0);
					
			gdk_pixbuf_unref (pixbuf); 
		} else {
			draw_one_tab_plain (sidebar_tabs, temp_gc, sidebar_tabs->details->title, NULL,
					    x_pos + TITLE_TAB_OFFSET, y_pos, sidebar_tabs->details->title_prelit, &sidebar_tabs->details->title_rect);
		}
		gdk_gc_unref (temp_gc);
		
	} else {
		if (sidebar_tabs->details->tab_count > 0) {
			draw_or_layout_all_tabs (sidebar_tabs, FALSE);
		}
	}
	
	return FALSE;
}

static char *
get_tab_image_name (TabItem *tab_item)
{
	Bonobo_PropertyBag property_bag;
	char *tab_image_name;
	
	property_bag = get_property_bag (tab_item);
	if (property_bag == CORBA_OBJECT_NIL) {
		return NULL;
	}

	tab_image_name = bonobo_property_bag_client_get_value_string
		(property_bag, "tab_image", NULL);
	bonobo_object_release_unref (property_bag, NULL);
	return tab_image_name;
}

/* update the indicator image for the passed in tab */
static void
nautilus_sidebar_tabs_update_tab_item (NautilusSidebarTabs *sidebar_tabs, TabItem *tab_item)
{
	char *tab_image_name, *image_path;
	
	tab_image_name = get_tab_image_name (tab_item);

	/* see if the indicator icon changed; if so, fetch the new one */
	if (eel_strcmp (tab_image_name, tab_item->indicator_pixbuf_name) != 0) {

		g_free (tab_item->indicator_pixbuf_name);
		tab_item->indicator_pixbuf_name = g_strdup (tab_image_name);	

		if (tab_item->indicator_pixbuf != NULL) {
			gdk_pixbuf_unref (tab_item->indicator_pixbuf);
			tab_item->indicator_pixbuf = NULL;	
		}
		if (tab_image_name != NULL) {
			image_path = nautilus_theme_get_image_path (tab_image_name);
			if (image_path != NULL) {
				tab_item->indicator_pixbuf = gdk_pixbuf_new_from_file (image_path);	
				g_free (image_path);
			}
		}
		
		recalculate_size (sidebar_tabs);				
		gtk_widget_queue_draw (GTK_WIDGET (sidebar_tabs));
	}
	
	g_free (tab_image_name);
}

static TabItem *
get_tab_item_from_view (NautilusSidebarTabs *sidebar_tabs, GtkWidget *view)
{
	GList *node;
	TabItem *tab_item;
	
	for (node = sidebar_tabs->details->tab_items; node != NULL; node = node->next) {
		tab_item = node->data;
		if (tab_item->tab_view == view) {
			return tab_item;
		}
	}
	return NULL;
}

/* check all of the tabs to see if their indicator pixmaps are ready for updating */
static void
nautilus_sidebar_tabs_update_all_indicators (NautilusSidebarTabs *sidebar_tabs)
{
	GList *node;
	TabItem *tab_item;

	for (node = sidebar_tabs->details->tab_items; node != NULL; node = node->next) {
		tab_item = node->data;
		nautilus_sidebar_tabs_update_tab_item (sidebar_tabs, tab_item);				
	}
}

/* check all of the tabs to see if their indicator pixmaps are ready for updating */
static void
nautilus_sidebar_tabs_update_indicator (NautilusSidebarTabs *sidebar_tabs, GtkWidget *view)
{
	GList *node;
	TabItem *tab_item;
	
	for (node = sidebar_tabs->details->tab_items; node != NULL; node = node->next) {
		tab_item = node->data;
		if (tab_item->tab_view == view) {
			nautilus_sidebar_tabs_update_tab_item (sidebar_tabs, tab_item);				
			break;
		}
	}
}

static void
tab_indicator_changed_callback (BonoboListener *listener,
				char *event_name,
				CORBA_any *arg,
				CORBA_Environment *ev,
				gpointer callback_data)
{
	NautilusSidebarTabs *sidebar_tabs;

	sidebar_tabs = NAUTILUS_SIDEBAR_TABS (callback_data);
	nautilus_sidebar_tabs_update_all_indicators (sidebar_tabs);	
}

/* listen for changes on the tab_image property */
void
nautilus_sidebar_tabs_connect_view (NautilusSidebarTabs *sidebar_tabs, GtkWidget *view)
{
	TabItem *tab_item;
	Bonobo_PropertyBag property_bag;
	
	tab_item = get_tab_item_from_view (sidebar_tabs, view);
	if (tab_item == NULL) {
		return;
	}
	
	property_bag = get_property_bag (tab_item);
	if (property_bag != CORBA_OBJECT_NIL) {
		tab_item->listener_id = bonobo_event_source_client_add_listener
			(property_bag, tab_indicator_changed_callback, 
			 "Bonobo/Property:change:tab_image", NULL, sidebar_tabs); 
		bonobo_object_release_unref (property_bag, NULL);
	}

	nautilus_sidebar_tabs_update_indicator (sidebar_tabs, view);
}

/* add a new tab entry, return TRUE if we succeed */

gboolean
nautilus_sidebar_tabs_add_view (NautilusSidebarTabs *sidebar_tabs, const char *name, GtkWidget *new_view, int page_num)
{
	TabItem *new_tab_item;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (new_view != NULL, FALSE);

	/* Check to see if we already have one with this name, if so, refuse to add it */   
	if (tab_item_find_by_name (sidebar_tabs, name)) {
		g_warning ("nautilus_sidebar_tabs_add_view: Trying to add duplicate item '%s'", name);
		return FALSE;
	}
	
	/* allocate a new entry, and initialize it */
	new_tab_item = g_new0 (TabItem, 1);
	new_tab_item->tab_text = g_strdup (name);
	new_tab_item->visible = TRUE;
	new_tab_item->prelit = FALSE;
	new_tab_item->tab_view = new_view;
	new_tab_item->notebook_page = page_num;

	/* add it to the list */
	sidebar_tabs->details->tab_items = g_list_append (sidebar_tabs->details->tab_items, new_tab_item);
	
	sidebar_tabs->details->tab_count += 1;
	recalculate_size (sidebar_tabs);
	gtk_widget_queue_draw (GTK_WIDGET (sidebar_tabs));
	
	return TRUE;
}

/* return the name of the tab with the passed in index */

char*
nautilus_sidebar_tabs_get_title_from_index (NautilusSidebarTabs *sidebar_tabs, int which_tab)
{
	GList *next_tab;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs), NULL);

	for (next_tab = sidebar_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		if (item->notebook_page == which_tab)
			return g_strdup (item->tab_text);
	}
	
	/* shouldn't ever get here... */
	return g_strdup ("");
}

/* remove the specified tab entry */

void
nautilus_sidebar_tabs_remove_view (NautilusSidebarTabs *sidebar_tabs, const char *name)
{
	GList *next_tab;
	TabItem *tab_item;
	int old_page_number;
	
	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	g_return_if_fail (name != NULL);

	/* Look up the item */
	tab_item = tab_item_find_by_name (sidebar_tabs, name);

	if (tab_item == NULL) {
		g_warning ("nautilus_sidebar_tabs_remove_view: Trying to remove a non-existing item '%s'", name);
		return;
	}
	
	/* Remove the item from the list */
	sidebar_tabs->details->tab_items = g_list_remove (sidebar_tabs->details->tab_items, tab_item);

	old_page_number = tab_item->notebook_page;
 	tab_item_destroy (tab_item);
	
	/* decrement all page numbers greater than the one we're removing */
	for (next_tab = sidebar_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		if (item->notebook_page >= old_page_number)
			item->notebook_page -= 1;
	}
	
	sidebar_tabs->details->tab_count -= 1;
	
	recalculate_size (sidebar_tabs);
	gtk_widget_queue_draw (GTK_WIDGET (sidebar_tabs));
}

/* prelight a tab, from its associated notebook page number, by setting the prelight flag of
   the proper tab and clearing the others */

void
nautilus_sidebar_tabs_prelight_tab (NautilusSidebarTabs *sidebar_tabs, int which_tab)
{
	GList *next_tab;
	gboolean is_prelit;
	gboolean changed = FALSE;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	
	if (sidebar_tabs->details->title_mode) {
		gboolean is_prelit = which_tab != -1;
		if (sidebar_tabs->details->title_prelit != is_prelit) {
			sidebar_tabs->details->title_prelit = is_prelit;
			changed = TRUE;
		}
	}
	else	
		for (next_tab = sidebar_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
			TabItem *item = next_tab->data;
			is_prelit = (item->notebook_page == which_tab);
			if (item->prelit != is_prelit) {
				item->prelit = is_prelit;
				changed = TRUE;
			}
		}
	
	if (changed)
		gtk_widget_queue_draw(GTK_WIDGET(sidebar_tabs));	
}

/* select a tab, from its associated notebook page number, by making it invisible 
   and all the others visible */

void
nautilus_sidebar_tabs_select_tab (NautilusSidebarTabs *sidebar_tabs, int which_tab)
{
	GList *next_tab;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	for (next_tab = sidebar_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		item->visible = (item->notebook_page != which_tab);
		item->prelit = FALSE;
	}
	
	recalculate_size(sidebar_tabs);
	gtk_widget_queue_draw(GTK_WIDGET(sidebar_tabs));	
}

/* utility routine that returns true if the passed-in color is lighter than average */   
static gboolean
is_light_color(GdkColor *color)
{
	int intensity = (((color->red >> 8) * 77) + ((color->green >> 8) * 150) + ((color->blue >> 8) * 28)) >> 8;	
	return intensity > 160; /* biased slightly toward dark so default of 0x999999 uses light text light Susan specified */
}

/* set the background color associated with a tab */

void
nautilus_sidebar_tabs_set_color (NautilusSidebarTabs *sidebar_tabs,
				 const char *color_spec)
{
	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	g_return_if_fail (color_spec != NULL);
	
	gdk_color_parse (color_spec, &sidebar_tabs->details->tab_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (sidebar_tabs)), 
				  &sidebar_tabs->details->tab_color, FALSE, TRUE);

	if (sidebar_tabs->details->tab_piece_images[0] == NULL) {
		if (is_light_color(&sidebar_tabs->details->tab_color))
			setup_dark_text(sidebar_tabs);
		else
			setup_light_text(sidebar_tabs);
	}
		
	gtk_widget_queue_draw (GTK_WIDGET(sidebar_tabs));	
}

/* receive a dropped color */

void
nautilus_sidebar_tabs_receive_dropped_color (NautilusSidebarTabs *sidebar_tabs,
					     int x, int y,
					     GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	g_return_if_fail (selection_data != NULL);
	
	/* Convert the selection data into a color spec. */
	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	
	channels = (guint16 *) selection_data->data;
	color_spec = g_strdup_printf ("rgb:%04hX/%04hX/%04hX", channels[0], channels[1], channels[2]);
	nautilus_sidebar_tabs_set_color(sidebar_tabs, color_spec);
	g_free (color_spec);
}

/* set the title (used in title mode only) */

void
nautilus_sidebar_tabs_set_title (NautilusSidebarTabs *sidebar_tabs, const char *new_title)
{
	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	g_return_if_fail (new_title != NULL);

	g_free(sidebar_tabs->details->title);
	sidebar_tabs->details->title = g_strdup (new_title);
}

/* set the title mode boolean */
void
nautilus_sidebar_tabs_set_title_mode (NautilusSidebarTabs *sidebar_tabs, gboolean is_title_mode)
{
	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	if (sidebar_tabs->details->title_mode != !!is_title_mode) {
		sidebar_tabs->details->title_mode = !!is_title_mode;
		recalculate_size (sidebar_tabs);
		gtk_widget_queue_draw (GTK_WIDGET (sidebar_tabs));	    
	}
}

/* set the visibility of the selected tab */

void
nautilus_sidebar_tabs_set_visible (NautilusSidebarTabs *sidebar_tabs,
				   const char *name,
				   gboolean is_visible)
{
	TabItem *tab_item;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));
	g_return_if_fail (name != NULL);

	/* Look up the item */
	tab_item = tab_item_find_by_name (sidebar_tabs, name);

	if (tab_item == NULL) {
		g_warning ("nautilus_sidebar_tabs_set_visible: Trying to munge a non-existing item '%s'", name);
		return;
	}

	if (tab_item->visible != is_visible) {
		tab_item->visible = is_visible;
		gtk_widget_queue_draw (GTK_WIDGET (sidebar_tabs));
	}
}
