/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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
 *
 */

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "nautilus-index-tabs.h"


/* data structures */

struct _tabItem
{
  gboolean visible;
  gchar *tab_text;
  gint notebook_page;
  GtkWidget *tab_view;
};

struct _NautilusIndexTabsDetails
{
  int tab_count;
  int selected_tab;	 
  int panel_width;

  GdkColor tab_color;
  GdkColor line_color;
  GdkColor shadow_color;
  GdkColor text_color;
  
  GList *tab_items;
};

/* constants */
#define tab_indent 1

/* headers */

static void nautilus_index_tabs_class_init (NautilusIndexTabsClass  *klass);
static void nautilus_index_tabs_init       (NautilusIndexTabs *pixmap);
static gint nautilus_index_tabs_expose     (GtkWidget *widget, GdkEventExpose  *event);
static void nautilus_index_tabs_finalize   (GtkObject *object);

/* static variables */

static GtkWidgetClass *parent_class;
static GdkPixbuf *tab_left_edge;
static GdkPixbuf *tab_right_edge;
static GdkFont *tab_font;

GtkType
nautilus_index_tabs_get_type (void)
{
  static GtkType index_tab_type = 0;

  if (!index_tab_type)
    {
      static const GtkTypeInfo index_tab_info =
      {
	"NautilusIndexTabs",
	sizeof (NautilusIndexTabs),
	sizeof (NautilusIndexTabsClass),
	(GtkClassInitFunc) nautilus_index_tabs_class_init,
	(GtkObjectInitFunc) nautilus_index_tabs_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      index_tab_type = gtk_type_unique (GTK_TYPE_WIDGET, &index_tab_info);
    }

  return index_tab_type;
}

static void
nautilus_index_tabs_class_init (NautilusIndexTabsClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  gchar *file_name;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_widget_get_type ());

  object_class->finalize = nautilus_index_tabs_finalize;
  widget_class->expose_event = nautilus_index_tabs_expose;

  /* load the images for the left and right edge */
  
  file_name = gnome_pixmap_file("nautilus/tableft.png");
  tab_left_edge = gdk_pixbuf_new_from_file(file_name);
  g_free (file_name);

  file_name = gnome_pixmap_file("nautilus/tabright.png");
  tab_right_edge = gdk_pixbuf_new_from_file(file_name);
  g_free (file_name);

  /* load the font */
  tab_font = gdk_font_load("-bitstream-courier-medium-r-normal-*-12-*-*-*-*-*-*-*");
}

static void
nautilus_index_tabs_init (NautilusIndexTabs *index_tabs)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(index_tabs), GTK_NO_WINDOW);
	
  index_tabs->details = g_new0 (NautilusIndexTabsDetails, 1);
  
  index_tabs->details->tab_count = 0;
  index_tabs->details->selected_tab = -1;
  index_tabs->details->tab_items = NULL;

  /* set up the colors */
  gdk_color_parse("rgb:ff/ff/ff", &index_tabs->details->tab_color);
  gdk_colormap_alloc_color(gtk_widget_get_colormap(GTK_WIDGET(index_tabs)), 
                           &index_tabs->details->tab_color, FALSE, TRUE);
  
  gdk_color_parse("rgb:00/00/00", &index_tabs->details->line_color);
  gdk_colormap_alloc_color(gtk_widget_get_colormap(GTK_WIDGET(index_tabs)), 
                           &index_tabs->details->line_color, FALSE, TRUE);

  gdk_color_parse("rgb:bd/bd/bd", &index_tabs->details->shadow_color);
  gdk_colormap_alloc_color(gtk_widget_get_colormap(GTK_WIDGET(index_tabs)), 
                           &index_tabs->details->shadow_color, FALSE, TRUE);

  gdk_color_parse("rgb:00/00/77", &index_tabs->details->text_color);
  gdk_colormap_alloc_color(gtk_widget_get_colormap(GTK_WIDGET(index_tabs)), 
                           &index_tabs->details->text_color, FALSE, TRUE);
}

GtkWidget*
nautilus_index_tabs_new (gint container_width)
{
  NautilusIndexTabs *index_tabs;   
  index_tabs = gtk_type_new (nautilus_index_tabs_get_type ());   
  index_tabs->details->panel_width = container_width;
  return GTK_WIDGET (index_tabs);
}

/* utility to destroy all the storage used by a tab item */

static void
tab_item_destroy(tabItem *item)
{
  if (item->tab_text)
    g_free(item->tab_text);
  g_free(item);
}

static void
nautilus_index_tabs_finalize (GtkObject *object)
{
  NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(object);
   	
  /* release the tab list, if any */
  if (index_tabs->details->tab_items != NULL)
    {
      GList *next_tab;
      for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next)
      		tab_item_destroy ((tabItem*)next_tab->data);
      g_list_free (index_tabs->details->tab_items);

    }

 (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it */

gint nautilus_index_tabs_hit_test(NautilusIndexTabs *index_tabs, double x, double y)
{
  return 0; /* for now */
}

/* resize the widget based on the number of tabs */

static void
recalculate_size(NautilusIndexTabs *index_tabs)
{
  GtkWidget *widget = GTK_WIDGET (index_tabs);
  gint tab_rows = (index_tabs->details->tab_count + 1) >> 1;
  widget->requisition.width = index_tabs->details->panel_width;
  widget->requisition.height = (tab_left_edge->art_pixbuf->height + 3) * tab_rows + 3;
  gtk_widget_queue_resize (widget);
}

/* draw a single tab at the passed-in position */

static void
draw_one_tab(NautilusIndexTabs *index_tabs, GdkGC *gc, gchar *tab_name, gint x, gint y, gboolean right_flag)
{  
  GdkGCValues saved_values;
  gint text_y_offset, tab_bottom, tab_right;
  /* measure the name and compute the bounding box */
  gint name_width = gdk_string_width(tab_font, tab_name) - 2*tab_indent;
  GtkWidget *widget = GTK_WIDGET(index_tabs);
  
  /* FIXME: we must "ellipsize" the name if it doesn't fit, for now, assume it does */
  
  /* position the tab for the right side, if necessary */
  if (right_flag)
  	x = widget->allocation.width - name_width - tab_left_edge->art_pixbuf->width - tab_right_edge->art_pixbuf->width + 8;
    
  /* clear the tab rectangle */

  gdk_gc_get_values(gc, &saved_values);
  gdk_gc_set_foreground (gc, &index_tabs->details->tab_color);
  gdk_draw_rectangle (widget->window, gc, TRUE, x + tab_left_edge->art_pixbuf->width, y, name_width, tab_left_edge->art_pixbuf->height); 
  gdk_gc_set_foreground (gc, &saved_values.foreground);
    
  /* draw the two edges with the pixbufs */
  gdk_pixbuf_render_to_drawable_alpha (tab_left_edge, widget->window, 0, 0, x, y,
						     tab_left_edge->art_pixbuf->width, tab_left_edge->art_pixbuf->height,
						     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX, 0, 0);
  
  gdk_pixbuf_render_to_drawable_alpha (tab_right_edge, widget->window, 0, 0, x + tab_right_edge->art_pixbuf->width + name_width, y,
						     tab_right_edge->art_pixbuf->width, tab_right_edge->art_pixbuf->height,
						     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX, 0, 0);						       
  /* draw the metaview name */

  text_y_offset = y + (tab_right_edge->art_pixbuf->height >> 1) + 4;  
  gdk_gc_set_foreground (gc, &index_tabs->details->text_color);  
  gdk_draw_string (widget->window, tab_font, gc, x + tab_right_edge->art_pixbuf->width - tab_indent, text_y_offset, tab_name);
  
  /* draw the top connecting line, in two shades of gray */
  
  gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
  gdk_draw_line(widget->window, gc, x + tab_left_edge->art_pixbuf->width, y, x + tab_left_edge->art_pixbuf->width + name_width - 1, y);
  gdk_gc_set_foreground (gc, &index_tabs->details->shadow_color);  
  gdk_draw_line(widget->window, gc, x + tab_left_edge->art_pixbuf->width, y + 1, x + tab_left_edge->art_pixbuf->width + name_width - 1, y + 1);
    
  /* draw the left bottom line */
  tab_bottom = y + tab_left_edge->art_pixbuf->height;
  tab_right = x + tab_left_edge->art_pixbuf->width + name_width + tab_right_edge->art_pixbuf->width;
  gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
  gdk_draw_line(widget->window, gc, tab_right, tab_bottom, index_tabs->details->panel_width, tab_bottom);

  /* draw the right bottom line */
}

/* draw all of the currently visible tabs */

static void
draw_all_tabs(NautilusIndexTabs *index_tabs)
{
  GList *next_tab = index_tabs->details->tab_items;
  GtkWidget *widget = GTK_WIDGET(index_tabs);  
  GdkGC* temp_gc = gdk_gc_new(widget->window);
  gint tab_height = tab_left_edge->art_pixbuf->height + 4;
  gint x_pos = widget->allocation.x - 3;
  gint y_pos = widget->allocation.y + widget->allocation.height - tab_left_edge->art_pixbuf->height;
  
  /* we must draw two items at a time, drawing the second one first, because the first one overlaps the second */
    
  while (next_tab != NULL)
    {
      tabItem *this_item = (tabItem*) next_tab->data;
      
      /* draw the second tab first, if there is one */
      if (next_tab->next)
        {
	  tabItem *second_item = next_tab->next->data;
          draw_one_tab(index_tabs, temp_gc, second_item->tab_text, x_pos, y_pos - 3, TRUE);	
	}
	
      draw_one_tab(index_tabs, temp_gc, this_item->tab_text, x_pos, y_pos, FALSE);
      
      next_tab = next_tab->next;
      if (next_tab)
      	next_tab = next_tab->next;
      
      y_pos += tab_height;
    }  
  gdk_gc_unref(temp_gc);
}

/* find a tab with a given name, or return NULL if we can't find one */

static GList*
find_tab(NautilusIndexTabs *index_tabs, const gchar *search_name)
{
  GList *next_tab;
  for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next)
    {
      tabItem *item = (tabItem*) next_tab->data;
      if (!strcmp(item->tab_text, search_name))
        return next_tab;
    }
    
  return NULL;
}

/* handle an expose event by drawing the tabs */

static gint
nautilus_index_tabs_expose (GtkWidget *widget, GdkEventExpose *event)
{
  NautilusIndexTabs *index_tabs;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (NAUTILUS_IS_INDEX_TABS (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  index_tabs = NAUTILUS_INDEX_TABS (widget);

  /* draw the tabs */
  if (index_tabs->details->tab_count > 0)
     draw_all_tabs(index_tabs);
     
  return FALSE;
}

/* add a new tab entry, return TRUE if we succeed */

gboolean
nautilus_index_tabs_add_view(NautilusIndexTabs *index_tabs, const gchar *name, GtkWidget *new_view, gint page_num)
{
  /* check to see if we already have one with this name, if so, refuse to add it */
  
  tabItem *new_tab_item;
  GList *item = find_tab(index_tabs, name);
  if (item)
	return FALSE;  
 
 /* allocate a new entry, and initialize it */
 new_tab_item = g_new0 (tabItem, 1);
 new_tab_item->tab_text = strdup(name);
 new_tab_item->visible = TRUE;
 new_tab_item->tab_view = new_view;
 new_tab_item->notebook_page = page_num;
 
 /* add it to the list */
 if (index_tabs->details->tab_items)
 	index_tabs->details->tab_items = g_list_append(index_tabs->details->tab_items, new_tab_item);
 else
   {
     index_tabs->details->tab_items = g_list_alloc(); 
     index_tabs->details->tab_items->data = new_tab_item;
   }
 
 index_tabs->details->tab_count += 1;
 recalculate_size(index_tabs);
 
 return TRUE;
}

/* remove the specified tab entry */

void
nautilus_index_tabs_remove_view(NautilusIndexTabs *index_tabs, const gchar *name)
{
  /* first, look up the item */
  GList *item = find_tab(index_tabs, name);
  if (item == NULL)
  	return;

  /* unlink the item from the list and dispose of it */
  index_tabs->details->tab_items = g_list_remove(index_tabs->details->tab_items, item);
  tab_item_destroy ((tabItem *)index_tabs->details->tab_items->data);
  g_list_free(item);

 index_tabs->details->tab_count -= 1;
 recalculate_size(index_tabs);
}
