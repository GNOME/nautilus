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
#include <math.h>

#include "ntl-index-panel.h"

#include "ntl-meta-view.h"
#include "nautilus-index-tabs.h"
#include "nautilus-index-title.h"

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-metadata.h>
#include <libnautilus/nautilus-string.h>
#include <gnome.h>

struct _NautilusIndexPanelDetails {
	GtkWidget *index_container;
	GtkWidget *index_title;
	GtkWidget *notebook;
	GtkWidget *index_tabs;
	GtkWidget *title_tab;
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

static void nautilus_index_panel_set_up_info (NautilusIndexPanel *index_panel, const char* new_uri);

#define DEFAULT_BACKGROUND_COLOR "rgb:DDDD/DDDD/FFFF"
#define INDEX_PANEL_WIDTH 136

/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
    TARGET_GNOME_URI_LIST
};

static GtkTargetEntry index_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
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

	/* allocate and install the index title widget */ 
	index_panel->details->index_title = nautilus_index_title_new();
	gtk_widget_show(index_panel->details->index_title);
	gtk_box_pack_start (GTK_BOX (index_panel->details->index_container),
			    index_panel->details->index_title, FALSE, FALSE, 0);
	
	/* first, allocate the index tabs */
	index_panel->details->index_tabs = GTK_WIDGET(nautilus_index_tabs_new());
	index_panel->details->selected_index = -1;

	/* also, allocate the title tab */
	index_panel->details->title_tab = GTK_WIDGET(nautilus_index_tabs_new());
	nautilus_index_tabs_set_title_mode(NAUTILUS_INDEX_TABS(index_panel->details->title_tab), TRUE);	
	
	gtk_widget_show (index_panel->details->index_tabs);
	gtk_box_pack_end (GTK_BOX (index_panel->details->index_container), index_panel->details->index_tabs, FALSE, FALSE, 0);

	/* allocate and install the meta-tabs */
  
	index_panel->details->notebook = gtk_notebook_new ();
	gtk_widget_ref (index_panel->details->notebook);
	gtk_object_sink (GTK_OBJECT (index_panel->details->notebook));

	gtk_widget_set_usize (index_panel->details->notebook, INDEX_PANEL_WIDTH, 200);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(index_panel->details->notebook), FALSE);
	  
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

	gtk_widget_unref (index_panel->details->notebook);

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
		case TARGET_GNOME_URI_LIST:
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
	if (description == NULL) {
		description = cbuf;
		g_snprintf (cbuf, sizeof (cbuf), "%p", meta_view);
	} 
	
	label = gtk_label_new (description);
	gtk_widget_show (label);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (index_panel->details->notebook), GTK_WIDGET (meta_view), label);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->notebook), GTK_WIDGET (meta_view));

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
	
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->notebook), GTK_WIDGET (meta_view));
	g_return_if_fail (page_num >= 0);
	gtk_notebook_remove_page (GTK_NOTEBOOK (index_panel->details->notebook), page_num);
}

/* utility to activate the metaview corresponding to the passed in index  */
static void
nautilus_index_panel_activate_meta_view(NautilusIndexPanel *index_panel, gint which_view)
{
	char *title;
	GtkNotebook *notebook = GTK_NOTEBOOK(index_panel->details->notebook);
	if (index_panel->details->selected_index < 0) {
		gtk_widget_show (index_panel->details->notebook);
		if (index_panel->details->notebook->parent == NULL)
			gtk_box_pack_end (GTK_BOX (index_panel->details->index_container), index_panel->details->notebook, FALSE, FALSE, 0);
		
		gtk_widget_show (index_panel->details->title_tab);
		if (index_panel->details->title_tab->parent == NULL)
			gtk_box_pack_end (GTK_BOX (index_panel->details->index_container), index_panel->details->title_tab, FALSE, FALSE, 0);    
	}
	
	index_panel->details->selected_index = which_view;
	title = nautilus_index_tabs_get_title_from_index(NAUTILUS_INDEX_TABS(index_panel->details->index_tabs), which_view);
	nautilus_index_tabs_set_title(NAUTILUS_INDEX_TABS(index_panel->details->title_tab), title);
	g_free(title);
	
	gtk_notebook_set_page(notebook, which_view);
}

/* utility to deactivate the active metaview */
static void
nautilus_index_panel_deactivate_meta_view(NautilusIndexPanel *index_panel)
{
	if (index_panel->details->selected_index >= 0) {
		gtk_widget_hide (index_panel->details->notebook);
		gtk_widget_hide (index_panel->details->title_tab);
	}
	
	index_panel->details->selected_index = -1;
	nautilus_index_tabs_select_tab(NAUTILUS_INDEX_TABS(index_panel->details->index_tabs), -1);
}

/* hit-test the index tabs and activate if necessary */

static gboolean
nautilus_index_panel_press_event (GtkWidget *widget, GdkEventButton *event)
{
	gint title_top, title_bottom;
	NautilusIndexPanel *index_panel = NAUTILUS_INDEX_PANEL (widget);
	NautilusIndexTabs *index_tabs = NAUTILUS_INDEX_TABS(index_panel->details->index_tabs);
	NautilusIndexTabs *title_tab = NAUTILUS_INDEX_TABS(index_panel->details->title_tab);
	gint rounded_y = floor(event->y + .5);
		
	/* if the click is in the main tabs, tell them about it */
	if (rounded_y >= index_panel->details->index_tabs->allocation.y) {
		gint which_tab = nautilus_index_tabs_hit_test(index_tabs, event->x, event->y);
		if (which_tab >= 0) {
			nautilus_index_tabs_select_tab(index_tabs, which_tab);
			nautilus_index_panel_activate_meta_view(index_panel, which_tab);
			gtk_widget_queue_draw(widget);	
		}
	} 
	
	/* also handle clicks in the title tab if necessary */
	if (index_panel->details->selected_index >= 0) {
		title_top = index_panel->details->title_tab->allocation.y;
		title_bottom = title_top + index_panel->details->title_tab->allocation.height;
		if ((rounded_y >= title_top) && (rounded_y <= title_bottom)) {
			gint which_tab = nautilus_index_tabs_hit_test(title_tab, event->x, event->y);
			if (which_tab >= 0) {
				/* the user clicked in the title tab, so deactivate the metaview */
				nautilus_index_panel_deactivate_meta_view(index_panel);
			}
			
		}
	}
	return TRUE;
}


static void
nautilus_index_panel_background_changed (NautilusIndexPanel *index_panel)
{
	NautilusBackground *background;
	char *color_spec;
	
	if (index_panel->details->directory == NULL)
		return;
	
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
nautilus_index_panel_set_up_info (NautilusIndexPanel *index_panel, const char* new_uri)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *background_color;

	directory = nautilus_directory_get (new_uri);
	if (index_panel->details->directory != NULL)
		gtk_object_unref (GTK_OBJECT (index_panel->details->directory));
	index_panel->details->directory = directory;
	
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
	
	/* tell the title widget about it */
	nautilus_index_title_set_uri(NAUTILUS_INDEX_TITLE(index_panel->details->index_title), new_uri);
		
	/* format and install the type-dependent descriptive info  */
	
	/* add the description text, if any.  Try to fetch it from the notes file if none is present */
	
	/* add keywords if we got any */				
}

/* here is the key routine that populates the index panel with the appropriate information when the uri changes */

void
nautilus_index_panel_set_uri (NautilusIndexPanel *index_panel, const char* new_uri)
{       
	/* there's nothing to do if the uri is the same as the current one */ 
	
	if (nautilus_strcmp (index_panel->details->uri, new_uri) == 0)
		return;
	
	g_free (index_panel->details->uri);
	index_panel->details->uri = g_strdup (new_uri);
		
	/* populate the per-uri box with the info */
	nautilus_index_panel_set_up_info (index_panel, new_uri);  	
}
