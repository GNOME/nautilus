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

#include <math.h>
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
  int total_height;
  gboolean title_mode;
  
  GdkColor tab_color;
  GdkColor line_color;
  GdkColor shadow_color;
  GdkColor text_color;  
  
  gchar *title;
  GList *tab_items;
};

/* constants */
#define TAB_INDENT 1
#define TITLE_TAB_OFFSET 0

/* headers */

static void nautilus_index_tabs_class_init (NautilusIndexTabsClass  *klass);
static void nautilus_index_tabs_init       (NautilusIndexTabs *pixmap);
static gint nautilus_index_tabs_expose     (GtkWidget *widget, GdkEventExpose  *event);
static void nautilus_index_tabs_finalize   (GtkObject *object);
static void nautilus_index_tabs_size_request(GtkWidget *widget, GtkRequisition *requisition);
static gint draw_or_hit_test_all_tabs(NautilusIndexTabs *index_tabs, gboolean draw_flag, gint test_x, gint test_y);

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
  widget_class->size_request = nautilus_index_tabs_size_request;
  
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
  index_tabs->details->total_height = 0;
  index_tabs->details->tab_items = NULL;
  index_tabs->details->title_mode = FALSE;
  index_tabs->details->title = NULL;
  
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
nautilus_index_tabs_new ()
{
  NautilusIndexTabs *index_tabs;   
  index_tabs = gtk_type_new (nautilus_index_tabs_get_type ());   
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
 
 if (index_tabs->details->title != NULL)
   g_free(index_tabs->details->title);
   
 g_free(index_tabs->details);
  	
 (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* determine the tab associated with the passed-in coordinates, and pass back the notebook
   page index associated with it */

gint nautilus_index_tabs_hit_test(NautilusIndexTabs *index_tabs, double x, double y)
{
  return draw_or_hit_test_all_tabs(index_tabs, FALSE, floor(x + .5), floor(y + .5));
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
    widget->requisition.height = tab_left_edge->art_pixbuf->height;
  else
    widget->requisition.height = index_tabs->details->total_height;
  gtk_widget_queue_resize (widget);
}

static void
nautilus_index_tabs_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
  NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(widget);
  
  /* dummy hit test to make sure height measurement is valid */
  draw_or_hit_test_all_tabs(index_tabs, FALSE, -1000, -1000);
  requisition->width = widget->parent ? widget->parent->allocation.width: 136;  
  if (index_tabs->details->title_mode)
    requisition->height = tab_left_edge->art_pixbuf->height;
  else
    requisition->height = index_tabs->details->total_height;
}

/* draw a single tab at the passed-in position, return the total width */

static gint
draw_one_tab(NautilusIndexTabs *index_tabs, GdkGC *gc, gchar *tab_name, gint x, gint y, gboolean right_flag)
{  
   gint text_y_offset, tab_bottom, tab_right;
  /* measure the name and compute the bounding box */
  gint name_width = gdk_string_width(tab_font, tab_name) - 2*TAB_INDENT;
  GtkWidget *widget = GTK_WIDGET(index_tabs);
  
  /* FIXME: we must "ellipsize" the name if it doesn't fit, for now, assume it does */
  
  /* position the tab for the right side, if necessary */
  if (right_flag)
  	x = widget->allocation.width - name_width - tab_left_edge->art_pixbuf->width - tab_right_edge->art_pixbuf->width + 8;
    
  /* clear the tab rectangle */

  gdk_gc_set_foreground (gc, &index_tabs->details->tab_color);
  gdk_draw_rectangle (widget->window, gc, TRUE, x + tab_left_edge->art_pixbuf->width, y, name_width, tab_left_edge->art_pixbuf->height); 
  
  /* kludge: extend the bottom line to the left and right in case the edge bitmaps are not the same size */
  gdk_draw_line(widget->window, gc, x, y + tab_right_edge->art_pixbuf->height, x + name_width  + tab_left_edge->art_pixbuf->width + tab_right_edge->art_pixbuf->width - 4, y + tab_right_edge->art_pixbuf->height); 
    
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
  gdk_draw_string (widget->window, tab_font, gc, x + tab_right_edge->art_pixbuf->width - TAB_INDENT, text_y_offset, tab_name);
  
  /* draw the top connecting line, in two shades of gray */
  gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
  gdk_draw_line(widget->window, gc, x + tab_left_edge->art_pixbuf->width, y, x + tab_left_edge->art_pixbuf->width + name_width - 1, y);
  gdk_gc_set_foreground (gc, &index_tabs->details->shadow_color);  
  gdk_draw_line(widget->window, gc, x + tab_left_edge->art_pixbuf->width, y + 1, x + tab_left_edge->art_pixbuf->width + name_width - 1, y + 1);
    
  /* draw the left bottom line */
  tab_bottom = y + tab_left_edge->art_pixbuf->height - 2;
  tab_right = x + tab_left_edge->art_pixbuf->width + name_width + tab_right_edge->art_pixbuf->width - 2*TAB_INDENT - 1;
  gdk_gc_set_foreground (gc, &index_tabs->details->line_color);  
  gdk_draw_line(widget->window, gc, tab_right, tab_bottom, widget->parent->allocation.width, tab_bottom);

  /* draw the right bottom line, too */
  gdk_draw_line(widget->window, gc, 0, tab_bottom, x, tab_bottom);

  return name_width + tab_left_edge->art_pixbuf->width + tab_right_edge->art_pixbuf->width - 2*TAB_INDENT;
}

/* draw or hit test all of the currently visible tabs */

static gint
draw_or_hit_test_all_tabs(NautilusIndexTabs *index_tabs, gboolean draw_flag, gint test_x, gint test_y)
{
  GdkGC *temp_gc;
  gint name_width, tab_width;
  GList *next_tab = index_tabs->details->tab_items;
  GtkWidget *widget = GTK_WIDGET(index_tabs);  
  gint tab_height = tab_left_edge->art_pixbuf->height + 4;
  gint x_pos = widget->allocation.x - 3;
  gint y_pos = widget->allocation.y + widget->allocation.height - tab_left_edge->art_pixbuf->height;
  gint total_height = tab_height;
    
  /* handle hit-testing for title mode */  
  if (index_tabs->details->title_mode && !draw_flag)
    {
      gint edge_width =  tab_left_edge->art_pixbuf->width + tab_right_edge->art_pixbuf->width;     
      name_width = gdk_string_width(tab_font, index_tabs->details->title) - (2 * TAB_INDENT);
      if ((test_x >= TITLE_TAB_OFFSET) && (test_x < (TITLE_TAB_OFFSET + name_width + edge_width)))
        return 0;
      return -1;
    }

  /* allocate a graphic context and clear the space below the top tabs to the background color */
  if (draw_flag)
    {
      gint y_top = widget->allocation.y + tab_left_edge->art_pixbuf->height + 2;
      gint fill_height = widget->allocation.y + widget->allocation.height - y_top;
      temp_gc = gdk_gc_new(widget->window); 
      gdk_gc_set_foreground (temp_gc, &index_tabs->details->tab_color);
      gdk_draw_rectangle (widget->window, temp_gc, TRUE, widget->allocation.x, y_top, widget->allocation.width, fill_height); 
    }
      
  /* draw as many tabs per row as will fit */
    
  while (next_tab != NULL)
    {
      tabItem *this_item = (tabItem*) next_tab->data;
      
      if (draw_flag && this_item->visible)
      	tab_width = draw_one_tab(index_tabs, temp_gc, this_item->tab_text, x_pos, y_pos, FALSE);
      else 
        {   
          gint edge_width =  tab_left_edge->art_pixbuf->width + tab_right_edge->art_pixbuf->width;
          name_width = gdk_string_width(tab_font, this_item->tab_text) - (4 * TAB_INDENT);
 	  tab_width = name_width + edge_width;
          if (!draw_flag && (test_y >= y_pos) && (test_y <= (y_pos + tab_left_edge->art_pixbuf->height)) &&
             (test_x >= x_pos) && (test_x <= x_pos + tab_width))	  
	    return this_item->notebook_page;
        }
        
      next_tab = next_tab->next;

      /* bump the x-position, and see if it fits */
      x_pos += tab_width - 10;
      
      if (x_pos > (widget->allocation.x + widget->allocation.width - 32))
        {
 	/* wrap to the next line */
 	  x_pos = widget->allocation.x - 3;     
          y_pos -= tab_height; 
          if (next_tab != NULL)
            total_height += tab_height;
        }                  
    }  
  
  if (draw_flag)
    gdk_gc_unref(temp_gc);
  index_tabs->details->total_height = total_height;
  return -1;
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

  if (widget->window == NULL)
  	return FALSE;
	
  index_tabs = NAUTILUS_INDEX_TABS (widget);

  /* draw the tabs */
  if (index_tabs->details->title_mode)
    {
      GdkGC* temp_gc = gdk_gc_new(widget->window); 
      gint x_pos = widget->allocation.x;
      gint y_pos = widget->allocation.y;
      
      draw_one_tab(index_tabs, temp_gc, index_tabs->details->title, x_pos + TITLE_TAB_OFFSET, y_pos, FALSE);  
      gdk_gc_unref(temp_gc);   
    }
  else
    if (index_tabs->details->tab_count > 0)
      draw_or_hit_test_all_tabs(index_tabs, TRUE, 0, 0);
     
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
 gtk_widget_queue_draw(GTK_WIDGET(index_tabs));
  
 return TRUE;
}

/* return the name of the tab with the passed in index */

gchar* nautilus_index_tabs_get_title_from_index(NautilusIndexTabs *index_tabs, gint which_tab)
{
  GList *next_tab;
  for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next)
    {
      tabItem *item = (tabItem*) next_tab->data;
      if (item->notebook_page == which_tab)
        return strdup(item->tab_text);
    }
  
  /* shouldn't ever get here... */
  return strdup("");
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
 gtk_widget_queue_draw(GTK_WIDGET(index_tabs));
}

/* select a tab, from its associated notebook page number, by making it invisible and all
   the others visible */
   
void
nautilus_index_tabs_select_tab(NautilusIndexTabs *index_tabs, gint which_tab)
{
  GList *next_tab;
  for (next_tab = index_tabs->details->tab_items; next_tab != NULL; next_tab = next_tab->next)
    {
      tabItem *item = (tabItem*) next_tab->data;
      item->visible = (item->notebook_page != which_tab);
    }
  
  gtk_widget_queue_draw(GTK_WIDGET(index_tabs));	
}

/* set the title (used in title mode only) */
void nautilus_index_tabs_set_title(NautilusIndexTabs* index_tabs, const gchar *new_title)
{
  if (index_tabs->details->title != NULL)
    g_free(index_tabs->details->title);
  
  index_tabs->details->title = strdup(new_title);
}

/* set the title mode boolean */
void nautilus_index_tabs_set_title_mode(NautilusIndexTabs* index_tabs, gboolean is_title_mode)
{
if (index_tabs->details->title_mode != is_title_mode)
  {
    index_tabs->details->title_mode = is_title_mode;
    recalculate_size(index_tabs);   
    gtk_widget_queue_draw(GTK_WIDGET(index_tabs));	    
  }
}

/* set the visibility of the selected tab */

void
nautilus_index_tabs_set_visible(NautilusIndexTabs* index_tabs, const gchar *name, gboolean is_visible)
{
  /* first, look up the item */
  tabItem *this_item;
  GList *item = find_tab(index_tabs, name);
  
  if (item == NULL)
  	return;

  this_item = (tabItem*) item->data;
  if (this_item->visible != is_visible)
    {
      this_item->visible = is_visible;
      gtk_widget_queue_draw(GTK_WIDGET(index_tabs));	
    }
}
