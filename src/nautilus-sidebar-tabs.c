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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-theme.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

/* constants for the tab piece pixbuf array */

#define	TAB_NORMAL_LEFT		0
#define	TAB_NORMAL_FILL		1
#define TAB_NORMAL_NEXT		2
#define TAB_NORMAL_RIGHT	3
#define TAB_NORMAL_EDGE		4
#define	TAB_PRELIGHT_LEFT	5
#define	TAB_PRELIGHT_FILL	6
#define TAB_PRELIGHT_NEXT	7
#define TAB_PRELIGHT_NEXT_ALT	8

#define TAB_PRELIGHT_RIGHT	9
#define TAB_PRELIGHT_EDGE	10
#define	TAB_ACTIVE_LEFT		11
#define	TAB_ACTIVE_FILL		12
#define TAB_ACTIVE_RIGHT	13
#define TAB_BACKGROUND		14
#define TAB_BACKGROUND_RIGHT	15
#define LAST_TAB_OFFSET		16

/* data structures */

typedef struct {
	gboolean visible;
	gboolean prelit;
	char *tab_text;
	int notebook_page;
	GtkWidget *tab_view;
} TabItem;

struct NautilusSidebarTabsDetails {
	int tab_count;
	int total_height;
	gboolean title_mode;
	
	GdkColor tab_color;
	GdkColor background_color;
	GdkColor line_color;
	GdkColor hilight_color;
	GdkColor prelight_color;
	GdkColor text_color;  
	GdkColor prelit_text_color;

	GdkPixbuf *tab_piece_images[LAST_TAB_OFFSET];
	int tab_left_offset;
	char *title;
	gboolean title_prelit;
	GList *tab_items;
};

/* constants */
#define TAB_MARGIN 8
#define TITLE_TAB_OFFSET 8
#define TAB_HEIGHT 18
#define TAB_H_GAP 8
#define TAB_TOP_GAP 3
#define TAB_ROW_V_OFFSET 3
#define TAB_DEFAULT_LEFT_OFFSET 4

/* headers */

static void     nautilus_sidebar_tabs_initialize_class (NautilusSidebarTabsClass *klass);
static void     nautilus_sidebar_tabs_initialize       (NautilusSidebarTabs      *pixmap);
static int      nautilus_sidebar_tabs_expose           (GtkWidget              *widget,
						      GdkEventExpose         *event);
static void     nautilus_sidebar_tabs_destroy          (GtkObject              *object);
static void     nautilus_sidebar_tabs_size_allocate    (GtkWidget              *widget,
						      GtkAllocation          *allocatoin);
static void     nautilus_sidebar_tabs_size_request     (GtkWidget              *widget,
						      GtkRequisition         *requisition);

static void	nautilus_sidebar_tabs_load_tab_pieces   (NautilusSidebarTabs *sidebar_tabs, const char *tab_piece_directory);
static void	nautilus_sidebar_tabs_unload_tab_pieces (NautilusSidebarTabs *sidebar_tabs);

static int      draw_or_hit_test_all_tabs            (NautilusSidebarTabs      *sidebar_tabs,
						      gboolean                draw_flag,
						      int                     test_x,
						      int                     test_y);
static TabItem* tab_item_find_by_name                (NautilusSidebarTabs      *sidebar_tabs,
						      const char             *name);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSidebarTabs, nautilus_sidebar_tabs, GTK_TYPE_WIDGET)

static void
nautilus_sidebar_tabs_initialize_class (NautilusSidebarTabsClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	
	object_class->destroy = nautilus_sidebar_tabs_destroy;
	widget_class->expose_event = nautilus_sidebar_tabs_expose;
	widget_class->size_request = nautilus_sidebar_tabs_size_request;
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
 
/* load the data required by the current theme */
static void
nautilus_sidebar_tabs_load_theme_data (NautilusSidebarTabs *sidebar_tabs)
{
	char *temp_str;
	char *tab_pieces, *tab_piece_path;
	GdkColor color;
	int intensity;
	
	/* set up the default values */
	sidebar_tabs->details->tab_left_offset = TAB_DEFAULT_LEFT_OFFSET;
	
	/* unload the old theme image if necessary */
	if (sidebar_tabs->details->tab_piece_images[0] != NULL) {
		nautilus_sidebar_tabs_unload_tab_pieces (sidebar_tabs);
	}
				
	/* load the tab_pieces image if necessary */
	tab_pieces = nautilus_theme_get_theme_data ("sidebar", "TAB_PIECE_IMAGES");
	if (tab_pieces) {
		tab_piece_path = nautilus_theme_get_image_path (tab_pieces);
		g_free (tab_pieces);
		
		if (tab_piece_path) {
			nautilus_sidebar_tabs_load_tab_pieces (sidebar_tabs, tab_piece_path);
			g_free (tab_piece_path);
			
			if (sidebar_tabs->details->tab_piece_images[0]) {
				/* load the left offset */
				temp_str = nautilus_theme_get_theme_data ("sidebar", "LEFT_OFFSET");
				if (temp_str) {
					sidebar_tabs->details->tab_left_offset = atoi(temp_str);
					g_free (temp_str);
				}
				
				/* set the text color according to the pixbuf */
				nautilus_gdk_pixbuf_average_value (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_FILL], &color);
				intensity = (((color.red >> 8) * 77) + ((color.green >> 8) * 150) + ((color.blue >> 8) * 28)) >> 8;	

				if (intensity < 160) {
					setup_light_text (sidebar_tabs);
				} else {
					setup_dark_text (sidebar_tabs);
				}
			}
		}	
	}	
}

/* initialize a newly allocated object */
static void
nautilus_sidebar_tabs_initialize (NautilusSidebarTabs *sidebar_tabs)
{
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(sidebar_tabs), GTK_NO_WINDOW);
	
	sidebar_tabs->details = g_new0 (NautilusSidebarTabsDetails, 1);

	/* Initialize private members */
	sidebar_tabs->details->tab_items = NULL;
	sidebar_tabs->details->tab_count = 0;
	sidebar_tabs->details->total_height = 0;
	sidebar_tabs->details->title_mode = FALSE;
	sidebar_tabs->details->title = NULL;
	sidebar_tabs->details->title_prelit = FALSE;
		
	/* set up the colors */
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
	nautilus_preferences_add_callback(NAUTILUS_PREFERENCES_THEME, 
						(NautilusPreferencesCallback) nautilus_sidebar_tabs_load_theme_data, 
						sidebar_tabs);
	
	sidebar_tabs->details->title_prelit = FALSE;
}

GtkWidget*
nautilus_sidebar_tabs_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_sidebar_tabs_get_type ()));
}

/* utility to destroy all the storage used by a tab item */

static void
tab_item_destroy (TabItem *item)
{
	g_free (item->tab_text);
	g_free (item);
}

static void
nautilus_sidebar_tabs_destroy (GtkObject *object)
{
	NautilusSidebarTabs *sidebar_tabs = NAUTILUS_SIDEBAR_TABS(object);
   	
	/* release the tab list, if any */
	if (sidebar_tabs->details->tab_piece_images[0] != NULL) {
		nautilus_sidebar_tabs_unload_tab_pieces (sidebar_tabs);
	}		
	
	if (sidebar_tabs->details->tab_items)
		g_list_free (sidebar_tabs->details->tab_items);

	nautilus_preferences_remove_callback(NAUTILUS_PREFERENCES_THEME, 
						(NautilusPreferencesCallback) nautilus_sidebar_tabs_load_theme_data, 
						sidebar_tabs);
		
	g_free (sidebar_tabs->details);
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it */

int nautilus_sidebar_tabs_hit_test(NautilusSidebarTabs *sidebar_tabs, int x, int y)
{
	return draw_or_hit_test_all_tabs (sidebar_tabs, FALSE, x, y);
}

/* resize the widget based on the number of tabs */

static void
recalculate_size(NautilusSidebarTabs *sidebar_tabs)
{
	GtkWidget *widget = GTK_WIDGET (sidebar_tabs);
	
	/* dummy hit test to make sure height measurement is valid */
	draw_or_hit_test_all_tabs(sidebar_tabs, FALSE, -1000, -1000);
  	
	widget->requisition.width = widget->parent ? widget->parent->allocation.width: 136;
	if (sidebar_tabs->details->title_mode)
		widget->requisition.height = TAB_HEIGHT;
	else
		widget->requisition.height = sidebar_tabs->details->total_height + TAB_TOP_GAP;
	gtk_widget_queue_resize (widget);
}

static void
nautilus_sidebar_tabs_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusSidebarTabs *sidebar_tabs = NAUTILUS_SIDEBAR_TABS(widget);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	/* dummy hit test to mesure height */
	draw_or_hit_test_all_tabs(sidebar_tabs, FALSE, -1000, -1000);
	
	if (!sidebar_tabs->details->title_mode) {
		 gint delta_height = widget->allocation.height - (sidebar_tabs->details->total_height + TAB_TOP_GAP);
         widget->allocation.height -= delta_height;
         widget->allocation.y += delta_height;
    }
}

static void
nautilus_sidebar_tabs_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	NautilusSidebarTabs *sidebar_tabs = NAUTILUS_SIDEBAR_TABS(widget);
	
	/* dummy hit test to make sure height measurement is valid */
	draw_or_hit_test_all_tabs(sidebar_tabs, FALSE, -1000, -1000);
	requisition->width = widget->parent ? widget->parent->allocation.width: 136;  
	if (sidebar_tabs->details->title_mode)
		requisition->height = TAB_HEIGHT;
	else
		requisition->height = sidebar_tabs->details->total_height + TAB_TOP_GAP;
}

/* draw a single tab using the default, non-themed approach */
static int
draw_one_tab_plain (NautilusSidebarTabs *sidebar_tabs, GdkGC *gc,
	      char *tab_name, int x, int y, gboolean prelight_flag)
{  
	int		text_y_offset;
	int		tab_bottom;
	int		tab_right;
	int		name_width;
	int		total_width;
	GtkWidget	*widget;

	g_assert (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	/* measure the name and compute the bounding box */
	name_width = gdk_string_width (GTK_WIDGET (sidebar_tabs)->style->font, tab_name);
	total_width = name_width + 2*TAB_MARGIN;

	widget = GTK_WIDGET (sidebar_tabs);
	
	/* FIXME bugzilla.eazel.com 668: 
	 * we must "ellipsize" the name if it doesn't fit, for now, assume it does 
	 */
		
	/* fill the tab rectangle with the tab color */
	
	gdk_gc_set_foreground (gc, prelight_flag ? &sidebar_tabs->details->prelight_color : &sidebar_tabs->details->tab_color);
	gdk_draw_rectangle (widget->window, gc, TRUE, x, y + 1, total_width, TAB_HEIGHT - 1); 
	
	/* draw the border */
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, x + 1, y, x + total_width - 2, y);
	gdk_draw_line(widget->window, gc, x, y + 1, x, y + TAB_HEIGHT - 1);
	gdk_draw_line(widget->window, gc, x + total_width - 1, y + 1, x + total_width - 1, y + TAB_HEIGHT - 1);
	
	/* draw the highlights for extra dimensionality */
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->hilight_color);  
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + total_width - 3, y + 2);
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + 2, y + TAB_HEIGHT - 1);
		
	/* draw the name */
	text_y_offset = y + (TAB_HEIGHT >> 1) + 5;  
	gdk_gc_set_foreground (gc, prelight_flag ? &sidebar_tabs->details->prelit_text_color : &sidebar_tabs->details->text_color);  
	gdk_draw_string (widget->window,
			 GTK_WIDGET (sidebar_tabs)->style->font,
			 gc, x + TAB_MARGIN, text_y_offset, tab_name);
	
	
	/* draw the bottom lines */
	tab_bottom = y + TAB_HEIGHT - 1;
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	tab_right = x + 2*TAB_MARGIN + name_width;
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, tab_right, tab_bottom, widget->parent->allocation.width, tab_bottom);
	gdk_draw_line(widget->window, gc, 0, tab_bottom, x, tab_bottom);
	
	return name_width + 2*TAB_MARGIN;
}

/* utility to draw the specified portion of a tab */

static int
draw_tab_piece (NautilusSidebarTabs *sidebar_tabs, GdkGC *gc, int x, int y, int which_piece)
{
	GtkWidget *widget;
	GdkPixbuf *pixbuf;
	int width, height;
		
	widget = GTK_WIDGET (sidebar_tabs);
	pixbuf = sidebar_tabs->details->tab_piece_images[which_piece];
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	
	gdk_pixbuf_render_to_drawable_alpha (pixbuf,
					widget->window,
					0, 0,
					x, y,
					width, height,
					GDK_PIXBUF_ALPHA_BILEVEL, 128,
					GDK_RGB_DITHER_MAX,
					0, 0);
	return width;
}

/* draw a single tab using the theme image to define it's appearance */
static int
draw_one_tab_themed (NautilusSidebarTabs *sidebar_tabs, GdkGC *gc,
	      		char *tab_name, int x, int y, gboolean prelight_flag, 
	      		gboolean first_flag, gboolean prev_invisible, int text_h_offset)
{  
	GtkWidget *widget;
	int name_width, piece_width;
	int current_pos, right_edge_pos;
	int text_x_pos, text_y_offset;
	int highlight_offset;
	
	widget = GTK_WIDGET (sidebar_tabs);
	/* FIXME: can't prelight active state yet */
	highlight_offset = prelight_flag && !sidebar_tabs->details->title_mode ? TAB_PRELIGHT_LEFT : 0; 
	if (sidebar_tabs->details->title_mode) {
		highlight_offset += TAB_ACTIVE_LEFT - TAB_NORMAL_LEFT;
	}
	
	/* measure the size of the name */
	name_width = gdk_string_width (GTK_WIDGET (sidebar_tabs)->style->font, tab_name);

	/* draw the left edge piece */
	current_pos = x;
	if (first_flag) {
		piece_width = draw_tab_piece (sidebar_tabs, gc, current_pos, y, TAB_NORMAL_LEFT + highlight_offset);
		current_pos += piece_width;
	}
	
	/* draw the middle portion in a loop */	
	text_x_pos = current_pos;
	right_edge_pos = current_pos + name_width;
	while (current_pos < right_edge_pos) {
		piece_width = draw_tab_piece (sidebar_tabs, gc, current_pos, y, TAB_NORMAL_FILL + highlight_offset);
		current_pos += piece_width;
	}
				
	/* draw the name */
	text_y_offset = y + (TAB_HEIGHT >> 1) + 5;
	if (!first_flag && !prev_invisible) {
		text_x_pos += text_h_offset;
	}
	
	gdk_gc_set_foreground (gc, &sidebar_tabs->details->text_color);  
	gdk_draw_string (widget->window,
			 GTK_WIDGET (sidebar_tabs)->style->font,
			 gc, text_x_pos, text_y_offset, tab_name);
	
	/* return the total tab width */
	return current_pos - x;	
}

static int
get_text_offset (void)
{
	int offset;
	char *temp_str;
	
	temp_str = nautilus_theme_get_theme_data ("sidebar", "TEXT_H_OFFSET");
	if (temp_str) {
		offset = atoi (temp_str);
		g_free (temp_str);
	} else {
		offset = 0;
	}

	return offset;
}

/* draw or hit test all of the currently visible tabs */
static int
draw_or_hit_test_all_tabs (NautilusSidebarTabs *sidebar_tabs, gboolean draw_flag, int test_x, int test_y)
{
	GdkGC		*temp_gc;
	GdkRectangle	temp_rect;
	TabItem		*prev_item;
	int		name_width;
	int		tab_width;
	int		extra_fill;
	GList		*next_tab;
	GtkWidget	*widget;  
	int		x_pos, y_pos;
	int		last_x_pos, last_y_pos;
	int		total_width, total_height, tab_select;
	int		piece_width, text_h_offset;
	gboolean	is_themed;
	gboolean	first_flag, prev_invisible;
	
	g_assert (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs));

	next_tab = sidebar_tabs->details->tab_items;
	
	widget = GTK_WIDGET (sidebar_tabs);  
	is_themed = sidebar_tabs->details->tab_piece_images[0] != NULL;
	if (is_themed) {
		text_h_offset = get_text_offset ();
	}
	
	x_pos = widget->allocation.x + sidebar_tabs->details->tab_left_offset;
	y_pos = widget->allocation.y + widget->allocation.height - TAB_HEIGHT;
	total_height = TAB_HEIGHT;
	total_width = widget->allocation.x + widget->allocation.width;
	
	/* handle hit-testing for title mode */  
	if (sidebar_tabs->details->title_mode && !draw_flag) {
		int edge_width =  2 * TAB_MARGIN;     
		if (sidebar_tabs->details->title == NULL) {
			return -1;
		}
		name_width = gdk_string_width (GTK_WIDGET (sidebar_tabs)->style->font,
					       sidebar_tabs->details->title);
		sidebar_tabs->details->total_height = total_height;
		if ((test_x >= TITLE_TAB_OFFSET) && (test_x < (TITLE_TAB_OFFSET + name_width + edge_width))) {
			return 0;
		}
		return -1;
	}
	
	/* allocate a graphic context and clear the space below the top tabs to the background color */
	if (draw_flag) {
		int y_top = widget->allocation.y + TAB_HEIGHT + TAB_TOP_GAP;
		int fill_height = widget->allocation.y + widget->allocation.height - y_top;
		temp_gc = gdk_gc_new(widget->window); 

		if (is_themed) {
			temp_rect.x = widget->allocation.x;
			temp_rect.y = y_top;
			temp_rect.width = widget->allocation.width;
			temp_rect.height = fill_height;
			
			nautilus_gdk_pixbuf_render_to_drawable_tiled (sidebar_tabs->details->tab_piece_images[TAB_BACKGROUND],
				widget->window, temp_gc,
				&temp_rect, GDK_RGB_DITHER_NORMAL,
				0, 0);

		} else {
			gdk_gc_set_foreground (temp_gc, &sidebar_tabs->details->background_color);
			gdk_draw_rectangle (widget->window, temp_gc, TRUE, widget->allocation.x, y_top, widget->allocation.width, fill_height); 
		}
	}
	
	/* draw as many tabs per row as will fit */
	
	first_flag = TRUE;
	prev_item = NULL;
	while (next_tab != NULL) {
		TabItem *this_item = next_tab->data;
		
		if (draw_flag && this_item->visible) {
			prev_invisible = prev_item && !prev_item->visible;
			if (is_themed)
				tab_width = draw_one_tab_themed (sidebar_tabs, temp_gc, this_item->tab_text, x_pos, y_pos,
								 this_item->prelit, first_flag, prev_invisible,
								 text_h_offset);
			else
				tab_width = draw_one_tab_plain (sidebar_tabs, temp_gc, this_item->tab_text,
								x_pos, y_pos, this_item->prelit);		
		} else {   
			int edge_width = 2 * TAB_MARGIN;
			name_width = gdk_string_width(GTK_WIDGET (sidebar_tabs)->style->font,
						      this_item->tab_text);
			tab_width = name_width + edge_width;
			if (!draw_flag && (test_y >= y_pos) && (test_y <= (y_pos + TAB_HEIGHT)) &&
			    (test_x >= x_pos) && (test_x <= x_pos + tab_width))	  
				return this_item->notebook_page;
		}
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
			
		if (x_pos > (total_width - 48)) {
			/* wrap to the next line */
			x_pos = widget->allocation.x + sidebar_tabs->details->tab_left_offset;     
			if (is_themed)
				y_pos -= gdk_pixbuf_get_height (sidebar_tabs->details->tab_piece_images[TAB_NORMAL_LEFT]);
			else
				y_pos -= TAB_HEIGHT + TAB_ROW_V_OFFSET;
			first_flag = TRUE;
			if ((next_tab != NULL) && ((next_tab->next != NULL) || this_item->visible)) {
				total_height += TAB_HEIGHT;
				if (!is_themed)
					total_height += TAB_ROW_V_OFFSET;
			}
		}                  
	
		/* now that we know about the next tab, we can draw the proper right piece of the tab */
		if (is_themed && draw_flag) {
			
			if (y_pos != last_y_pos && this_item != NULL && this_item->visible) {
				if (prev_item->prelit) {
					tab_select = TAB_PRELIGHT_RIGHT;
					extra_fill = TAB_PRELIGHT_FILL;
				} else {
					tab_select = TAB_NORMAL_RIGHT;
					extra_fill = TAB_NORMAL_FILL;
				}
				
				/* we must do some extra drawing of the fill pattern here, to stretch it out to the edge */
				if (prev_item->visible)
					draw_tab_piece (sidebar_tabs, temp_gc,  last_x_pos + tab_width, last_y_pos, extra_fill);				
				last_x_pos = total_width - gdk_pixbuf_get_width (sidebar_tabs->details->tab_piece_images[tab_select]) - tab_width; 
			} else if ((this_item == NULL) || !this_item->visible)
				tab_select = prev_item->prelit ? TAB_PRELIGHT_EDGE : TAB_NORMAL_EDGE;
			else {
				if (prev_item->prelit) {
					tab_select = TAB_PRELIGHT_NEXT;
				} else if (this_item->prelit) {
					tab_select = TAB_PRELIGHT_NEXT_ALT;				
				} else {
					tab_select = TAB_NORMAL_NEXT;		
				}
			}	
			if (!prev_item->visible)
				tab_select = y_pos == last_y_pos ? TAB_NORMAL_LEFT : -1;
			
			if (tab_select >= 0)	
				piece_width = draw_tab_piece (sidebar_tabs, temp_gc,  last_x_pos + tab_width, last_y_pos, tab_select);
			
			if (y_pos == last_y_pos)
				x_pos += piece_width;
		}
	}  
	
	if (draw_flag)
		gdk_gc_unref(temp_gc);
	sidebar_tabs->details->total_height = total_height;
	return -1;
}

/* find a tab with a given name, or return NULL if we can't find one */
static TabItem *
tab_item_find_by_name (NautilusSidebarTabs *sidebar_tabs, const char *name)
{
	GList *iterator;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (sidebar_tabs), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (iterator = sidebar_tabs->details->tab_items; iterator != NULL; iterator = iterator->next) {
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
nautilus_sidebar_tabs_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusSidebarTabs *sidebar_tabs;
	int tab_width, text_offset;
	
	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TABS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	if (widget->window == NULL) {
		return FALSE;
	}
	
	sidebar_tabs = NAUTILUS_SIDEBAR_TABS (widget);

	text_offset = get_text_offset ();
	
	/* draw the tabs */
	if (sidebar_tabs->details->title_mode) {
		GdkGC* temp_gc = gdk_gc_new(widget->window); 
		int x_pos = widget->allocation.x;
		int y_pos = widget->allocation.y;
		
		if (sidebar_tabs->details->tab_piece_images[0]) {
			tab_width = draw_one_tab_themed (sidebar_tabs, temp_gc, sidebar_tabs->details->title, x_pos, y_pos, 
							 sidebar_tabs->details->title_prelit, TRUE, FALSE, text_offset);
			/* draw the right edge piece */
			draw_tab_piece (sidebar_tabs, temp_gc,  x_pos + tab_width, y_pos, TAB_ACTIVE_RIGHT);
		} else {
			draw_one_tab_plain (sidebar_tabs, temp_gc, sidebar_tabs->details->title,
					    x_pos + TITLE_TAB_OFFSET, y_pos, sidebar_tabs->details->title_prelit);		
		}
		gdk_gc_unref (temp_gc);
	} else {
		if (sidebar_tabs->details->tab_count > 0) {
			draw_or_hit_test_all_tabs (sidebar_tabs, TRUE, 0, 0);
		}
	}
	
	return FALSE;
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
	sidebar_tabs->details->tab_items = g_list_append(sidebar_tabs->details->tab_items, new_tab_item);
	
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
