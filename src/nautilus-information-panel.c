/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

#include <config.h>
#include "ntl-index-panel.h"

#include "ntl-meta-view.h"
#include "nautilus-index-tabs.h"
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-metadata.h>
#include <libnautilus/nautilus-string.h>
#include <gnome.h>
#include <math.h>

struct _NautilusIndexPanelDetails {
	GtkWidget *index_container;
	GtkWidget *per_uri_container;
	GtkWidget *meta_tabs;
	GtkWidget *index_tabs;
	char *uri;
	gint selected_index;
	NautilusDirectory *directory;
	int background_connection;
};

static void nautilus_index_panel_initialize_class (GtkObjectClass *object_klass);
static void nautilus_index_panel_initialize (GtkObject *object);
static gboolean nautilus_index_panel_press_event(GtkWidget *widget, GdkEventButton *event);
static void nautilus_index_panel_destroy (GtkObject *object);
static void nautilus_index_panel_finalize (GtkObject *object);

static void nautilus_index_panel_drag_data_received (GtkWidget *widget, GdkDragContext *context,
						     gint x, gint y,
						     GtkSelectionData *selection_data,
						     guint info, guint time);

static void nautilus_index_panel_set_up_info (NautilusIndexPanel *index_panel, const gchar* new_uri);
static void nautilus_index_panel_set_up_label (NautilusIndexPanel *index_panel, const gchar *uri);
static void nautilus_index_panel_set_up_logo (NautilusIndexPanel *index_panel, const gchar *logo_path);

static GdkFont *select_font(const gchar *text_to_format, gint width, const gchar* font_template);

#define DEFAULT_BACKGROUND_COLOR "rgb:DDDD/DDDD/FFFF"
#define USE_NEW_TABS 0
#define INDEX_PANEL_WIDTH 136

/* drag and drop definitions */

enum {
	TARGET_COLOR,
	TARGET_URI_LIST
};

static GtkTargetEntry index_dnd_target_table[] = {
	{ "application/x-color", 0, TARGET_COLOR },
	{ "text/uri-list",  0, TARGET_URI_LIST }
};

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIndexPanel, nautilus_index_panel, GTK_TYPE_EVENT_BOX)

/* initializing the class object by installing the operations we override */
static void
nautilus_index_panel_initialize_class (GtkObjectClass *object_klass)
{
	GtkWidgetClass *widget_class;
	NautilusIndexPanelClass *klass;

	widget_class = GTK_WIDGET_CLASS (object_klass);
	klass = NAUTILUS_INDEX_PANEL_CLASS (object_klass);

	object_klass->destroy = nautilus_index_panel_destroy;
	object_klass->finalize = nautilus_index_panel_finalize;

	widget_class->drag_data_received = nautilus_index_panel_drag_data_received;
	widget_class->button_press_event = nautilus_index_panel_press_event;
}

/* common routine to make the per-uri container */

static void make_per_uri_container(NautilusIndexPanel *index_panel)
{
	index_panel->details->per_uri_container = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (index_panel->details->per_uri_container), 0);				
	gtk_widget_show (index_panel->details->per_uri_container);
	gtk_box_pack_start (GTK_BOX (index_panel->details->index_container),
			    index_panel->details->per_uri_container, FALSE, FALSE, 0);
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
nautilus_index_panel_initialize (GtkObject *object)
{
	NautilusIndexPanel *index_panel;
	GtkWidget* widget;
	
	index_panel = NAUTILUS_INDEX_PANEL (object);
	widget = GTK_WIDGET (object);

	index_panel->details = g_new0 (NautilusIndexPanelDetails, 1);
	
	/* set the size of the index panel */
	gtk_widget_set_usize (widget, INDEX_PANEL_WIDTH, 400);
 
	/* create the container box */
  	index_panel->details->index_container = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (index_panel->details->index_container), 0);				
	gtk_widget_show (index_panel->details->index_container);
	gtk_container_add (GTK_CONTAINER (index_panel), index_panel->details->index_container);

	/* allocate and install the vbox to hold the per-uri information */ 
	make_per_uri_container (index_panel);

	/* first, install the index tabs */
	index_panel->details->index_tabs = GTK_WIDGET(nautilus_index_tabs_new(INDEX_PANEL_WIDTH));
	index_panel->details->selected_index = -1;

	if (USE_NEW_TABS)
	  {
	    gtk_widget_show (index_panel->details->index_tabs);
	    gtk_box_pack_end (GTK_BOX (index_panel->details->index_container), index_panel->details->index_tabs, FALSE, FALSE, 0);
	  }
	  
	/* allocate and install the meta-tabs */
  
	index_panel->details->meta_tabs = gtk_notebook_new ();
	gtk_widget_set_usize (index_panel->details->meta_tabs, INDEX_PANEL_WIDTH, 200);
	if (USE_NEW_TABS)
	    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(index_panel->details->meta_tabs), FALSE);
	else
	  {
	    gtk_widget_show (index_panel->details->meta_tabs);
	    gtk_box_pack_end (GTK_BOX (index_panel->details->index_container), index_panel->details->meta_tabs, FALSE, FALSE, 0);
 	  }
	  
	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (index_panel),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   index_dnd_target_table, NAUTILUS_N_ELEMENTS (index_dnd_target_table), GDK_ACTION_COPY);
}

static void
nautilus_index_panel_destroy (GtkObject *object)
{
	NautilusIndexPanel *index_panel;

	index_panel = NAUTILUS_INDEX_PANEL (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_index_panel_finalize (GtkObject *object)
{
	NautilusIndexPanel *index_panel;

	index_panel = NAUTILUS_INDEX_PANEL (object);

	g_free (index_panel->details->uri);
	g_free (index_panel->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

/* create a new instance */
NautilusIndexPanel *
nautilus_index_panel_new (void)
{
	return NAUTILUS_INDEX_PANEL (gtk_type_new (nautilus_index_panel_get_type ()));
}

/* drag and drop handler for index panel */

static void  
nautilus_index_panel_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 gint x, gint y,
					 GtkSelectionData *selection_data, guint info, guint time)
{
	g_return_if_fail (NAUTILUS_IS_INDEX_PANEL (widget));

	switch (info)
		{
		case TARGET_URI_LIST: 	
			g_message("dropped data on index panel: %s", selection_data->data);
      
			/* handle background images and keywords soon */
  			
			/* handle images dropped on the logo specially */
  			
			/* handle files by setting the location to the file */
  			
			break;
  		
      
 		case TARGET_COLOR:
			/* Let the background change based on the dropped color. */
			nautilus_background_receive_dropped_color
				(nautilus_get_widget_background (widget),
				 widget, x, y, selection_data);
			break;
      
		default:
			g_warning ("unknown drop type");
			break;	
		}
}

/* add a new meta-view to the index panel */
void
nautilus_index_panel_add_meta_view (NautilusIndexPanel *index_panel, NautilusView *meta_view)
{
  GtkWidget *label;
  const char *description;
  char cbuf[32];
  gint page_num;
  
  g_return_if_fail (NAUTILUS_IS_INDEX_PANEL (index_panel));
  g_return_if_fail (NAUTILUS_IS_META_VIEW (meta_view));

  description = nautilus_meta_view_get_label (NAUTILUS_META_VIEW (meta_view));
  if (description == NULL)
    {
      description = cbuf;
      g_snprintf (cbuf, sizeof (cbuf), "%p", meta_view);
    } 
	
  label = gtk_label_new (description);
  gtk_widget_show (label);

  gtk_notebook_prepend_page (GTK_NOTEBOOK (index_panel->details->meta_tabs), GTK_WIDGET (meta_view), label);
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->meta_tabs), GTK_WIDGET (meta_view));
  
  /* tell the index tabs about it */
  nautilus_index_tabs_add_view(NAUTILUS_INDEX_TABS(index_panel->details->index_tabs),
                               description, GTK_WIDGET(meta_view), page_num);
    
  gtk_widget_show (GTK_WIDGET (meta_view));
}

/* remove the passed-in meta-view from the index panel */
void
nautilus_index_panel_remove_meta_view (NautilusIndexPanel *index_panel, NautilusView *meta_view)
{
  gint page_num;

  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->meta_tabs), GTK_WIDGET (meta_view));
  g_return_if_fail (page_num >= 0);
  gtk_notebook_remove_page (GTK_NOTEBOOK (index_panel->details->meta_tabs), page_num);
}

/* utility to activate the metaview corresponding to the passed in index  */
static void
nautilus_index_panel_activate_meta_view(NautilusIndexPanel *index_panel, gint which_view)
{
}

/* hit-test the index tabs and activate if necessary */

static gboolean
nautilus_index_panel_press_event (GtkWidget *widget, GdkEventButton *event)
{
  NautilusIndexPanel *index_panel = NAUTILUS_INDEX_PANEL (widget);
  gint rounded_y = floor(event->y + .5);
  /* if the click is in the tabs, tell them about it */
  if (rounded_y >= index_panel->details->index_tabs->allocation.y)
    {
      gint which_tab = nautilus_index_tabs_hit_test(NAUTILUS_INDEX_TABS(index_panel->details->index_tabs), event->x, event->y);
      if (which_tab >= 0)
      	nautilus_index_panel_activate_meta_view(index_panel, which_tab);
    } 
  return TRUE;
}

/* set up the logo image */
void
nautilus_index_panel_set_up_logo (NautilusIndexPanel *index_panel, const gchar *logo_path)
{
  gchar *file_name;
  GtkWidget *pix_widget;
    
  file_name = gnome_pixmap_file (logo_path);
  pix_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
  gtk_widget_show (pix_widget);
  gtk_box_pack_start (GTK_BOX (index_panel->details->per_uri_container), pix_widget, 0, 0, 0);
  g_free (file_name);
}

/* utility routine (FIXME: should be located elsewhere) to find the largest font that fits */

GdkFont *
select_font(const gchar *text_to_format, gint width, const gchar* font_template)
{
	GdkFont *candidate_font = NULL;
	gchar *font_name;
	gint this_width;
	gint font_sizes[8] = { 28, 24, 18, 14, 12, 10, 8 };
	gint font_index;
	
	for (font_index = 0; font_index < NAUTILUS_N_ELEMENTS (font_sizes); font_index++) {
		if (candidate_font != NULL)
			gdk_font_unref (candidate_font);
		
		font_name = g_strdup_printf (font_template, font_sizes[font_index]);
		candidate_font = gdk_font_load (font_name);
		g_free (font_name);
		
		this_width = gdk_string_width (candidate_font, text_to_format);
		if (this_width < width)
			return candidate_font;
	}
	
	return candidate_font;
}

/* set up the label */

void
nautilus_index_panel_set_up_label (NautilusIndexPanel *index_panel, const gchar *uri)
{
	GnomeVFSURI *vfs_uri;
	GtkWidget *label_widget;
	char *file_name;
	GdkFont *label_font;
	
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL)
		return;
	
	file_name = gnome_vfs_uri_extract_short_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	if (file_name == NULL)
		return;

	label_widget = gtk_label_new (file_name);
	gtk_box_pack_start (GTK_BOX (index_panel->details->per_uri_container), label_widget, 0, 0, 0);
	
	label_font = select_font(file_name, GTK_WIDGET (index_panel)->allocation.width - 4,
				 "-bitstream-courier-medium-r-normal-*-%d-*-*-*-*-*-*-*");
	
	if (label_font != NULL) {
		GtkStyle *temp_style;
		gtk_widget_realize (label_widget);	
		temp_style = gtk_style_new ();	  	
		temp_style->font = label_font;
		gtk_widget_set_style (label_widget, gtk_style_attach (temp_style, label_widget->window));
	}
	
	gtk_widget_show(label_widget);

	g_free (file_name);
}

static void
nautilus_index_panel_background_changed (NautilusIndexPanel *index_panel)
{
	NautilusBackground *background;
	char *color_spec;
	
	background = nautilus_get_widget_background (GTK_WIDGET (index_panel));
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (index_panel->details->directory,
					 INDEX_PANEL_BACKGROUND_COLOR_METADATA_KEY,
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);
}

/* this routine populates the index panel with the per-uri information */

void
nautilus_index_panel_set_up_info (NautilusIndexPanel *index_panel, const gchar* new_uri)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *background_color;

	directory = nautilus_directory_get (new_uri);
	if (index_panel->details->directory != NULL)
		gtk_object_unref (GTK_OBJECT (index_panel->details->directory));
	index_panel->details->directory = directory;
	if(!directory)
		return;
	
	/* Connect the background changed signal to code that writes the color. */
	background = nautilus_get_widget_background (GTK_WIDGET (index_panel));
        if (index_panel->details->background_connection == 0)
		index_panel->details->background_connection =
			gtk_signal_connect_object (GTK_OBJECT (background),
						   "changed",
						   nautilus_index_panel_background_changed,
						   GTK_OBJECT (index_panel));

	/* Set up the background color from the metadata. */
	background_color = nautilus_directory_get_metadata (directory,
							    INDEX_PANEL_BACKGROUND_COLOR_METADATA_KEY,
							    DEFAULT_BACKGROUND_COLOR);
	nautilus_background_set_color (background, background_color);
	g_free (background_color);
	
	/* next, install the logo image. */
	/* For now, just use a fixed folder image */	
	nautilus_index_panel_set_up_logo (index_panel, "nautilus/i-directory.png");
	
	/* add the name, discarding all but the last part of the path */
	/* soon, we'll use the biggest font that fit, for now don't worry about it */
	nautilus_index_panel_set_up_label (index_panel, new_uri);
	
	/* format and install the type-dependent descriptive info  */
	
	/* add the description text, if any.  Try to fetch it from the notes file if none is present */
	
	/* add keywords if we got any */				
}

/* here is the key routine that populates the index panel with the appropriate information when the uri changes */

void
nautilus_index_panel_set_uri (NautilusIndexPanel *index_panel, const gchar* new_uri)
{       
	/* there's nothing to do if the uri is the same as the current one */ 
	
	if (nautilus_strcmp (index_panel->details->uri, new_uri) == 0)
		return;
	
	g_free (index_panel->details->uri);
	index_panel->details->uri = g_strdup (new_uri);
	
	/* get rid of the old widgets in the per_uri container */
	gtk_widget_destroy (index_panel->details->per_uri_container);
	make_per_uri_container (index_panel);
	
	/* populate the per-uri box with the info */
	nautilus_index_panel_set_up_info (index_panel, new_uri);  	
}
