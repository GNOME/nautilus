/* Nautilus
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * This is the index panel widget, which displays overview information
 * in a vertical panel and hosts the meta-views.
 *
 */

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#include <gnome.h>
#include "nautilus.h"
#include "ntl-index-panel.h"
#include <libgnomevfs/gnome-vfs-uri.h>

static void nautilus_index_panel_class_init(NautilusIndexPanelClass *klass);
static void nautilus_index_panel_init(NautilusIndexPanel *icon_view);
static void nautilus_index_panel_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint time);
void nautilus_index_panel_set_meta_tabs(GtkWidget* widget, GtkWidget* new_tabs);
void nautilus_index_panel_set_uri(GtkWidget *widget, const gchar *new_uri);
void nautilus_index_panel_add_meta_view(GtkWidget* widget, NautilusView *meta_view);
void nautilus_index_panel_remove_meta_view(GtkWidget* widget, NautilusView *meta_view);

void nautilus_index_panel_set_up_background(GtkWidget* widget, const gchar *background_data);
void nautilus_index_panel_set_up_info(GtkWidget* widget, const gchar* new_uri);
void nautilus_index_panel_set_up_label(GtkWidget* widget, const gchar *uri);
void nautilus_index_panel_set_up_logo(GtkWidget* widget, const gchar *logo_path);

GdkFont *select_font(const gchar *text_to_format, gint width, const gchar* font_template);

/* drag and drop definitions */

enum dnd_targets_enum
{
  TARGET_STRING,
  TARGET_COLOR,
  TARGET_URI_LIST
};

static GtkTargetEntry index_dnd_target_table[] = 
{
  { "application/x-color", 0, TARGET_COLOR },
  { "text/uri-list",  0, TARGET_URI_LIST }
};

/* private globals */

/* the get_type routine is boilerplate code that registers the class and returns a unique type integer */

guint 
nautilus_index_panel_get_type (void)
{
  static guint index_panel_type = 0;

  if (!index_panel_type)
    {
      static const GtkTypeInfo index_panel_info =
      {
        "NautilusIndexPanel",
	sizeof (NautilusIndexPanel),
	sizeof (NautilusIndexPanelClass),
	(GtkClassInitFunc) nautilus_index_panel_class_init,
	(GtkObjectInitFunc) nautilus_index_panel_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      index_panel_type = gtk_type_unique (gtk_event_box_get_type (), &index_panel_info);
    }

  return index_panel_type;
}

/* initializing the class object by installing the operations we override */
static void
nautilus_index_panel_class_init (NautilusIndexPanelClass *class)
{
  GtkWidgetClass *widget_class;
  widget_class = (GtkWidgetClass*) class;
  widget_class->drag_data_received = nautilus_index_panel_drag_data_received;
}

/* common routine to make the per-uri container */

static void make_per_uri_container(NautilusIndexPanel *index_panel)
{
  index_panel->per_uri_container = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER (index_panel->per_uri_container), 0);				
  gtk_widget_show(index_panel->per_uri_container);
  gtk_box_pack_start(GTK_BOX(index_panel->index_container), index_panel->per_uri_container, FALSE, FALSE, 0);
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
nautilus_index_panel_init (NautilusIndexPanel *index_panel)
{
  GtkWidget* widget = GTK_WIDGET(index_panel);
  
  index_panel->index_container = NULL;
  index_panel->per_uri_container = NULL;
  index_panel->uri = NULL;

  /* set the size of the index panel */
  
  gtk_widget_set_usize(widget, 136, 400);
 
  /* create the container box */
  
  index_panel->index_container = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER (index_panel->index_container), 0);				
  gtk_widget_show(index_panel->index_container);
  gtk_container_add(GTK_CONTAINER(index_panel), index_panel->index_container);

  /* allocate and install the vbox to hold the per-uri information */ 
  make_per_uri_container(index_panel);

  /* allocate and install the meta-tabs (for now it's a notebook) */
  
  index_panel->meta_tabs = gtk_notebook_new();
  gtk_widget_set_usize(index_panel->meta_tabs, 136, 200);
  gtk_widget_show(index_panel->meta_tabs);
  gtk_box_pack_end(GTK_BOX(index_panel->index_container), index_panel->meta_tabs, FALSE, FALSE, 0);
 
  /* prepare ourselves to receive dropped objects */
  gtk_drag_dest_set (GTK_WIDGET (index_panel), GTK_DEST_DEFAULT_MOTION | 
                                               GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
                                               index_dnd_target_table, 2, GDK_ACTION_COPY);
}

/* create a new instance */
GtkWidget*
nautilus_index_panel_new (void)
{
  return GTK_WIDGET(gtk_type_new (nautilus_index_panel_get_type ()));
}

/* drag and drop handler for index panel */

static void  
nautilus_index_panel_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint time)
{
  /* FIXME: set up the new color here */
  printf("something dropped on index panel: %d\n", info);
}

/* add a new meta-view to the index panel */
void nautilus_index_panel_add_meta_view(GtkWidget* widget, NautilusView *meta_view)
{
  GtkWidget *label;
  const char *description;
  char cbuf[32];
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;

  g_return_if_fail(NAUTILUS_IS_META_VIEW(meta_view));

  description = nautilus_meta_view_get_label(NAUTILUS_META_VIEW(meta_view));
  if (!description)
    {
      description = cbuf;
      g_snprintf(cbuf, sizeof(cbuf), "%p", meta_view);
    } 
  label = gtk_label_new(description);
  gtk_widget_show(label);
  
  /*
  gtk_signal_connect(GTK_OBJECT(label), "button_press_event",
		     GTK_SIGNAL_FUNC(nautilus_window_send_show_properties), meta_view);
  */
  
  gtk_notebook_prepend_page(GTK_NOTEBOOK(index_panel->meta_tabs), GTK_WIDGET(meta_view), label);
  gtk_widget_show(GTK_WIDGET(meta_view));
}

/* remove the passed-in meta-view from the index panel */
void nautilus_index_panel_remove_meta_view(GtkWidget* widget, NautilusView *meta_view)
{
  gint pagenum;
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;
  pagenum = gtk_notebook_page_num(GTK_NOTEBOOK(index_panel->meta_tabs), GTK_WIDGET(meta_view));
  g_return_if_fail(pagenum >= 0);
  gtk_notebook_remove_page(GTK_NOTEBOOK(index_panel->meta_tabs), pagenum);
}

/* set up the index panel's background. Darin's background stuff will soon replace this,
   but for now just set up the color */

void nautilus_index_panel_set_up_background(GtkWidget* widget, const gchar *background_data)
{
  GdkColor temp_color;
  GtkStyle *temp_style;

  gdk_color_parse(background_data, &temp_color);          
  gdk_color_alloc(gtk_widget_get_colormap(widget), &temp_color);			
  temp_style = gtk_style_new();
  temp_style->bg[GTK_STATE_NORMAL] = temp_color;
  gtk_widget_set_style(widget, gtk_style_attach(temp_style, widget->window));		
}

/* set up the logo image */
void nautilus_index_panel_set_up_logo(GtkWidget* widget, const gchar *logo_path)
{
  const gchar *file_name;
  GtkWidget *pix_widget;
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;
    
  file_name = gnome_pixmap_file(logo_path);
  pix_widget = (GtkWidget*) gnome_pixmap_new_from_file(file_name);		
  gtk_widget_show(pix_widget);
  gtk_box_pack_start(GTK_BOX(index_panel->per_uri_container), pix_widget, 0, 0, 0);
  g_free((gchar*)file_name);
}

/* utility routine (FIXME: should be located elsewhere) to find the largest font that fits */

GdkFont *select_font(const gchar *text_to_format, gint width, const gchar* font_template)
{
  GdkFont *candidate_font;
  gchar font_name[512];
  gint this_width;
  gint font_sizes[8] = { 28, 24, 18, 14, 12, 10, 8 };
  gint font_index = 0;
	
  while (font_index < 8)
    {
      g_snprintf(font_name, sizeof(font_name), font_template, font_sizes[font_index]);
      candidate_font = gdk_font_load(font_name);
      this_width = gdk_string_width(candidate_font, text_to_format);
      if (this_width < width)
	return candidate_font;
      else
      	gdk_font_unref(candidate_font);	
      font_index += 1;
		
    }
  return candidate_font;
}

/* set up the label */

void nautilus_index_panel_set_up_label(GtkWidget* widget, const gchar *uri)
{
  GtkWidget *label_widget;
  const gchar *file_name;
  GnomeVFSURI *vfs_uri;
  GdkFont* label_font;
  gchar *temp_uri = strdup(uri); 
  gint slash_pos = strlen(temp_uri) - 1;
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;

  /* we must remove the trailing slash for vfs_get_basename to work right for us */
  if ((temp_uri[slash_pos] == '/') && (slash_pos > 0))
  	temp_uri[slash_pos] = '\0';

  vfs_uri = gnome_vfs_uri_new(temp_uri);
  file_name = gnome_vfs_uri_get_basename(vfs_uri);	
  gnome_vfs_uri_destroy(vfs_uri);
  
  label_widget = gtk_label_new(file_name);	
  gtk_box_pack_start(GTK_BOX(index_panel->per_uri_container), label_widget, 0, 0, 0);
 
  label_font = select_font(file_name, widget->allocation.width - 4, "-bitstream-courier-medium-r-normal-*-%d-*-*-*-*-*-*-*");	
  if (label_font != NULL)
    {
      GtkStyle *temp_style;
      gtk_widget_realize(label_widget);	
      temp_style = gtk_style_new();	  	
      temp_style->font = label_font;
      gtk_widget_set_style(label_widget, gtk_style_attach(temp_style, label_widget->window));
    }
 
  gtk_widget_show(label_widget);
  g_free((gchar*) file_name);
  g_free(temp_uri);
}

/* this routine populates the index panel with the per-uri information */

void nautilus_index_panel_set_up_info(GtkWidget* widget, const gchar* new_uri)
{       
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;

  /* set up the background from the metadata.  At first, just use hardwired backgrounds */
  nautilus_index_panel_set_up_background(widget, "rgb:DD/DD/FF");
   			
  /* next, install the logo image. */
  /* For now, just use a fixed folder image */	
  nautilus_index_panel_set_up_logo(widget, "nautilus/i-directory.png");
	
  /* add the name, discarding all but the last part of the path */
  /* soon, we'll use the biggest font that fit, for now don't worry about it */
  nautilus_index_panel_set_up_label(widget, new_uri);
    
  /* format and install the type-dependent descriptive info  */
		
  /* add the description text, if any.  Try to fetch it from the notes file if none is present */
	
  /* add keywords if we got any */				
}

/* here is the key routine that populates the index panel with the appropriate information when the uri changes */

void nautilus_index_panel_set_uri(GtkWidget* widget, const gchar* new_uri)
{       
  NautilusIndexPanel *index_panel = (NautilusIndexPanel*) widget;
  
  /* there's nothing to do if the uri is the same as the current one */ 
  
  if (index_panel->uri && !strcmp(index_panel->uri, new_uri))
    return;
  
  if (index_panel->uri)
    g_free(index_panel->uri);

  index_panel->uri = g_strdup(new_uri);
   
  /* get rid of the old widgets in the per_uri container */
  gtk_widget_destroy(index_panel->per_uri_container);
  make_per_uri_container(index_panel);
 
  /* populate the per-uri box with the info */
  nautilus_index_panel_set_up_info(widget, new_uri);  	
 }

