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
 * This is the tabs widget for the index panel, which represents metaviews as nice tabs as specified
 */

#include <config.h>
#include "nautilus-index-tabs.h"

#include <math.h>
#include <stdio.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

/* data structures */

typedef struct {
	gboolean visible;
	gboolean prelit;
	char *tab_text;
	int notebook_page;
	GtkWidget *tab_view;
} TabItem;

struct NautilusIndexTabsDetails {
	int tab_count;
	int total_height;
	gboolean title_mode;
	
	GdkColor tab_color;
	GdkColor background_color;
	GdkColor line_color;
	GdkColor hilight_color;
	GdkColor prelight_color;
	GdkColor text_color;  
	
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

/* headers */

static void nautilus_index_tabs_initialize_class (NautilusIndexTabsClass *klass);
static void nautilus_index_tabs_initialize       (NautilusIndexTabs      *pixmap);
static int  nautilus_index_tabs_expose           (GtkWidget              *widget,
						  GdkEventExpose         *event);
static void nautilus_index_tabs_destroy          (GtkObject              *object);
static void nautilus_index_tabs_size_allocate     (GtkWidget              *widget,
						  GtkAllocation         *allocatoin);
static void nautilus_index_tabs_size_request     (GtkWidget              *widget,
						  GtkRequisition         *requisition);
static int  draw_or_hit_test_all_tabs            (NautilusIndexTabs      *index_tabs,
						  gboolean                draw_flag,
						  int                     test_x,
						  int                     test_y);

/* static variables */

static GdkFont *tab_font;

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIndexTabs, nautilus_index_tabs, GTK_TYPE_WIDGET)

static void
nautilus_index_tabs_initialize_class (NautilusIndexTabsClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	
	object_class->destroy = nautilus_index_tabs_destroy;
	widget_class->expose_event = nautilus_index_tabs_expose;
	widget_class->size_request = nautilus_index_tabs_size_request;
	widget_class->size_allocate = nautilus_index_tabs_size_allocate;
		
	/* load the font */
	/* FIXME: this shouldn't be hardwired - it should be fetched from preferences */
	tab_font = gdk_font_load ("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");
}

static void
nautilus_index_tabs_initialize (NautilusIndexTabs *index_tabs)
{
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(index_tabs), GTK_NO_WINDOW);
	
	index_tabs->details = g_new0 (NautilusIndexTabsDetails, 1);
	
	/* set up the colors */
	gdk_color_parse ("rgb:9c/9c/9c", &index_tabs->details->tab_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->tab_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:55/55/55", &index_tabs->details->prelight_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->prelight_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:ff/ff/ff", &index_tabs->details->background_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->background_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:00/00/00", &index_tabs->details->line_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->line_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:d6/d6/d6", &index_tabs->details->hilight_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->hilight_color, FALSE, TRUE);
	
	gdk_color_parse ("rgb:ff/ff/ff", &index_tabs->details->text_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->text_color, FALSE, TRUE);

	index_tabs->details->title_prelit = FALSE;
}

GtkWidget*
nautilus_index_tabs_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_index_tabs_get_type ()));
}

/* utility to destroy all the storage used by a tab item */

static void
tab_item_destroy (TabItem *item)
{
	g_free (item->tab_text);
	g_free (item);
}

static void
nautilus_index_tabs_destroy (GtkObject *object)
{
	NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(object);
   	
	/* release the tab list, if any */
	if (index_tabs->details->tab_items != NULL) {
		GList *next_tab;
		for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
			tab_item_destroy ((TabItem*)next_tab->data);
		}
		
		g_list_free (index_tabs->details->tab_items);
	}
	
	g_free (index_tabs->details->title);
	
	g_free (index_tabs->details);
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it */

int nautilus_index_tabs_hit_test(NautilusIndexTabs *index_tabs, int x, int y)
{
	return draw_or_hit_test_all_tabs (index_tabs, FALSE, x, y);
}

/* resize the widget based on the number of tabs */

static void
recalculate_size(NautilusIndexTabs *index_tabs)
{
	GtkWidget *widget = GTK_WIDGET (index_tabs);
	
	/* dummy hit test to make sure height measurement is valid */
	draw_or_hit_test_all_tabs(index_tabs, FALSE, -1000, -1000);
  	
	widget->requisition.width = widget->parent ? widget->parent->allocation.width: 136;
	if (index_tabs->details->title_mode)
		widget->requisition.height = TAB_HEIGHT;
	else
		widget->requisition.height = index_tabs->details->total_height + TAB_TOP_GAP;
	gtk_widget_queue_resize (widget);
}

static void
nautilus_index_tabs_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(widget);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	/* dummy hit test to mesure height */
	draw_or_hit_test_all_tabs(index_tabs, FALSE, -1000, -1000);
	
	if (!index_tabs->details->title_mode) {
		 gint delta_height = widget->allocation.height - (index_tabs->details->total_height + TAB_TOP_GAP);
         widget->allocation.height -= delta_height;
         widget->allocation.y += delta_height;
    }
}

static void
nautilus_index_tabs_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(widget);
	
	/* dummy hit test to make sure height measurement is valid */
	draw_or_hit_test_all_tabs(index_tabs, FALSE, -1000, -1000);
	requisition->width = widget->parent ? widget->parent->allocation.width: 136;  
	if (index_tabs->details->title_mode)
		requisition->height = TAB_HEIGHT;
	else
		requisition->height = index_tabs->details->total_height + TAB_TOP_GAP;
}

/* draw a single tab at the passed-in position, return the total width */

static int
draw_one_tab (NautilusIndexTabs *index_tabs, GdkGC *gc,
	      char *tab_name, int x, int y, gboolean prelight_flag)
{  
	int text_y_offset, tab_bottom, tab_right;
	/* measure the name and compute the bounding box */
	int name_width = gdk_string_width (tab_font, tab_name);
	int total_width = name_width + 2*TAB_MARGIN;
	GtkWidget *widget = GTK_WIDGET (index_tabs);
	
	/* FIXME: we must "ellipsize" the name if it doesn't fit, for now, assume it does */
		
	/* fill the tab rectangle with the tab color */
	
	gdk_gc_set_foreground (gc, prelight_flag ? &index_tabs->details->prelight_color : &index_tabs->details->tab_color);
	gdk_draw_rectangle (widget->window, gc, TRUE, x, y + 1, total_width, TAB_HEIGHT - 1); 
	
	/* draw the border */
	gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, x + 1, y, x + total_width - 2, y);
	gdk_draw_line(widget->window, gc, x, y + 1, x, y + TAB_HEIGHT - 1);
	gdk_draw_line(widget->window, gc, x + total_width - 1, y + 1, x + total_width - 1, y + TAB_HEIGHT - 1);
	
	/* draw the highlights for extra dimensionality */
	gdk_gc_set_foreground (gc, &index_tabs->details->hilight_color);  
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + total_width - 3, y + 2);
	gdk_draw_line(widget->window, gc, x + 2, y + 2, x + 2, y + TAB_HEIGHT - 1);
		
	/* draw the metaview name */
	text_y_offset = y + (TAB_HEIGHT >> 1) + 5;  
	gdk_gc_set_foreground (gc, &index_tabs->details->text_color);  
	gdk_draw_string (widget->window, tab_font, gc, x + TAB_MARGIN, text_y_offset, tab_name);
	
	
	/* draw the bottom lines */
	tab_bottom = y + TAB_HEIGHT - 1;
	gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
	tab_right = x + 2*TAB_MARGIN + name_width;
	gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
	gdk_draw_line(widget->window, gc, tab_right, tab_bottom, widget->parent->allocation.width, tab_bottom);
	gdk_draw_line(widget->window, gc, 0, tab_bottom, x, tab_bottom);
	
	return name_width + 2*TAB_MARGIN;
}

/* draw or hit test all of the currently visible tabs */

static int
draw_or_hit_test_all_tabs (NautilusIndexTabs *index_tabs, gboolean draw_flag, int test_x, int test_y)
{
	GdkGC *temp_gc;
	int name_width, tab_width;
	GList *next_tab = index_tabs->details->tab_items;
	GtkWidget *widget = GTK_WIDGET(index_tabs);  
	int x_pos = widget->allocation.x + 4;
	int y_pos = widget->allocation.y + widget->allocation.height - TAB_HEIGHT;
	int total_height = TAB_HEIGHT;
	
	/* handle hit-testing for title mode */  
	if (index_tabs->details->title_mode && !draw_flag) {
		int edge_width =  2 * TAB_MARGIN;     
		if (index_tabs->details->title == NULL) {
			return -1;
		}
		name_width = gdk_string_width (tab_font, index_tabs->details->title);
		index_tabs->details->total_height = total_height;
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
		gdk_gc_set_foreground (temp_gc, &index_tabs->details->background_color);
		gdk_draw_rectangle (widget->window, temp_gc, TRUE, widget->allocation.x, y_top, widget->allocation.width, fill_height); 
	}
	
	/* draw as many tabs per row as will fit */
	
	while (next_tab != NULL) {
		TabItem *this_item = next_tab->data;
		
		if (draw_flag && this_item->visible)
			tab_width = draw_one_tab(index_tabs, temp_gc, this_item->tab_text, x_pos, y_pos, this_item->prelit);
		else {   
			int edge_width = 2 * TAB_MARGIN;
			name_width = gdk_string_width(tab_font, this_item->tab_text);
			tab_width = name_width + edge_width;
			if (!draw_flag && (test_y >= y_pos) && (test_y <= (y_pos + TAB_HEIGHT)) &&
			    (test_x >= x_pos) && (test_x <= x_pos + tab_width))	  
				return this_item->notebook_page;
		}
		
		next_tab = next_tab->next;
		if (next_tab != NULL)
			this_item = (TabItem*) next_tab->data;	
		
		/* bump the x-position, and see if it fits */
		x_pos += tab_width + TAB_H_GAP;
		
		if (x_pos > (widget->allocation.x + widget->allocation.width - 48)) {
			/* wrap to the next line */
			x_pos = widget->allocation.x + 4;     
			y_pos -= TAB_HEIGHT + TAB_ROW_V_OFFSET; 
			if ((next_tab != NULL) && ((next_tab->next != NULL) || this_item->visible))
				total_height += TAB_HEIGHT + TAB_ROW_V_OFFSET;
		}                  
	}  
	
	if (draw_flag)
		gdk_gc_unref(temp_gc);
	index_tabs->details->total_height = total_height;
	return -1;
}

/* find a tab with a given name, or return NULL if we can't find one */

static GList*
find_tab (NautilusIndexTabs *index_tabs, const char *search_name)
{
	GList *next_tab;
	for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		if (strcmp (item->tab_text, search_name) == 0)
			return next_tab;
	}
	
	return NULL;
}

/* handle an expose event by drawing the tabs */

static int
nautilus_index_tabs_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusIndexTabs *index_tabs;
	
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_INDEX_TABS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	if (widget->window == NULL) {
		return FALSE;
	}
	
	index_tabs = NAUTILUS_INDEX_TABS (widget);
	
	/* draw the tabs */
	if (index_tabs->details->title_mode) {
		GdkGC* temp_gc = gdk_gc_new(widget->window); 
		int x_pos = widget->allocation.x;
		int y_pos = widget->allocation.y;
		
		draw_one_tab (index_tabs, temp_gc, index_tabs->details->title, x_pos + TITLE_TAB_OFFSET, y_pos, index_tabs->details->title_prelit);
		gdk_gc_unref (temp_gc);
	} else {
		if (index_tabs->details->tab_count > 0) {
			draw_or_hit_test_all_tabs (index_tabs, TRUE, 0, 0);
		}
	}
	
	return FALSE;
}

/* add a new tab entry, return TRUE if we succeed */

gboolean
nautilus_index_tabs_add_view (NautilusIndexTabs *index_tabs, const char *name, GtkWidget *new_view, int page_num)
{
	/* check to see if we already have one with this name, if so, refuse to add it */   
	TabItem *new_tab_item;
	GList *item = find_tab(index_tabs, name);
	
	if (item) {
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
	if (index_tabs->details->tab_items)
		index_tabs->details->tab_items = g_list_append(index_tabs->details->tab_items, new_tab_item);
	else {
		index_tabs->details->tab_items = g_list_alloc(); 
		index_tabs->details->tab_items->data = new_tab_item;
	}
	
	index_tabs->details->tab_count += 1;
	recalculate_size(index_tabs);
	gtk_widget_queue_draw(GTK_WIDGET(index_tabs));
	
	return TRUE;
}

/* return the name of the tab with the passed in index */

char*
nautilus_index_tabs_get_title_from_index(NautilusIndexTabs *index_tabs, int which_tab)
{
	GList *next_tab;
	for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		if (item->notebook_page == which_tab)
			return g_strdup (item->tab_text);
	}
	
	/* shouldn't ever get here... */
	return g_strdup ("");
}

/* remove the specified tab entry */

void
nautilus_index_tabs_remove_view (NautilusIndexTabs *index_tabs, const char *name)
{
	/* first, look up the item */
	GList *item = find_tab (index_tabs, name);
	if (item == NULL)
		return;
	
	/* unlink the item from the list and dispose of it */
	index_tabs->details->tab_items = g_list_remove (index_tabs->details->tab_items, item);
	tab_item_destroy (index_tabs->details->tab_items->data);
	g_list_free (item);
	
	index_tabs->details->tab_count -= 1;
	recalculate_size (index_tabs);
	gtk_widget_queue_draw (GTK_WIDGET (index_tabs));
}

/* prelight a tab, from its associated notebook page number, by setting the prelight flag of
   the proper tab and clearing the others */

void
nautilus_index_tabs_prelight_tab(NautilusIndexTabs *index_tabs, int which_tab)
{
	GList *next_tab;
	gboolean is_prelit;
	gboolean changed = FALSE;
	
	if (index_tabs->details->title_mode) {
		gboolean is_prelit = which_tab != -1;
		if (index_tabs->details->title_prelit != is_prelit) {
			index_tabs->details->title_prelit = is_prelit;
			changed = TRUE;
		}
	}
	else	
		for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
			TabItem *item = next_tab->data;
			is_prelit = (item->notebook_page == which_tab);
			if (item->prelit != is_prelit) {
				item->prelit = is_prelit;
				changed = TRUE;
			}
		}
	
	if (changed)
		gtk_widget_queue_draw(GTK_WIDGET(index_tabs));	
}

/* select a tab, from its associated notebook page number, by making it invisible 
   and all the others visible */

void
nautilus_index_tabs_select_tab (NautilusIndexTabs *index_tabs, int which_tab)
{
	GList *next_tab;
	for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next) {
		TabItem *item = next_tab->data;
		item->visible = (item->notebook_page != which_tab);
		item->prelit = FALSE;
	}
	
	recalculate_size(index_tabs);
	gtk_widget_queue_draw(GTK_WIDGET(index_tabs));	
}

/* set the background color associated with a tab */

void
nautilus_index_tabs_set_color (NautilusIndexTabs *index_tabs,
			       const char *color_spec)
{
	gdk_color_parse (color_spec, &index_tabs->details->tab_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->tab_color, FALSE, TRUE);
	gtk_widget_queue_draw (GTK_WIDGET(index_tabs));	
}
 

/* receive a dropped color */

void
nautilus_index_tabs_receive_dropped_color (NautilusIndexTabs *index_tabs,
					   int x, int y,
					   GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;
	
	/* Convert the selection data into a color spec. */
	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	
	channels = (guint16 *) selection_data->data;
	color_spec = g_strdup_printf ("rgb:%04hX/%04hX/%04hX", channels[0], channels[1], channels[2]);
	
	gdk_color_parse (color_spec, &index_tabs->details->tab_color);
	g_free (color_spec);

	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (index_tabs)), 
				  &index_tabs->details->tab_color, FALSE, TRUE);
	
	gtk_widget_queue_draw (GTK_WIDGET(index_tabs));	
}

/* set the title (used in title mode only) */
void
nautilus_index_tabs_set_title (NautilusIndexTabs *index_tabs, const char *new_title)
{
	g_free(index_tabs->details->title);
	index_tabs->details->title = g_strdup (new_title);
}

/* set the title mode boolean */
void
nautilus_index_tabs_set_title_mode (NautilusIndexTabs *index_tabs, gboolean is_title_mode)
{
	if (index_tabs->details->title_mode != !!is_title_mode) {
		index_tabs->details->title_mode = !!is_title_mode;
		recalculate_size (index_tabs);
		gtk_widget_queue_draw (GTK_WIDGET (index_tabs));	    
	}
}

/* set the visibility of the selected tab */

void
nautilus_index_tabs_set_visible(NautilusIndexTabs *index_tabs,
				const char *name,
				gboolean is_visible)
{
	/* first, look up the item */
	TabItem *this_item;
	GList *item = find_tab (index_tabs, name);
	
	if (item == NULL) {
		return;
	}
	
	this_item = item->data;
	if (this_item->visible != !!is_visible) {
		this_item->visible = !!is_visible;
		gtk_widget_queue_draw (GTK_WIDGET (index_tabs));	
	}
}
