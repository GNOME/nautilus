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

#include <math.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-mime-type.h>
#include "ntl-meta-view.h"
#include "nautilus-index-tabs.h"
#include "nautilus-index-title.h"

struct NautilusIndexPanelDetails {
	GtkVBox *container;
	NautilusIndexTitle *title;
	GtkNotebook *notebook;
	NautilusIndexTabs *index_tabs;
	NautilusIndexTabs *title_tab;
	GtkVBox *button_box;
	gboolean has_buttons;
	char *uri;
	int selected_index;
	NautilusDirectory *directory;
	int background_connection;
};

static void     nautilus_index_panel_initialize_class   (GtkObjectClass     *object_klass);
static void     nautilus_index_panel_initialize         (GtkObject          *object);
static gboolean nautilus_index_panel_press_event        (GtkWidget          *widget,
							 GdkEventButton     *event);
static gboolean nautilus_index_panel_leave_event        (GtkWidget          *widget,
							 GdkEventCrossing   *event);
static gboolean nautilus_index_panel_motion_event       (GtkWidget          *widget,
							 GdkEventMotion     *event);
static void     nautilus_index_panel_destroy            (GtkObject          *object);
static void     nautilus_index_panel_drag_data_received (GtkWidget          *widget,
							 GdkDragContext     *context,
							 int                 x,
							 int                 y,
							 GtkSelectionData   *selection_data,
							 guint               info,
							 guint               time);
static void     nautilus_index_panel_update_info        (NautilusIndexPanel *index_panel,
							 const char         *title);
static void     nautilus_index_panel_update_buttons     (NautilusIndexPanel *index_panel);
static void     add_command_buttons                     (NautilusIndexPanel *index_panel,
							 GList              *command_list);

#define DEFAULT_BACKGROUND_COLOR "rgb:DDDD/DDDD/FFFF"
#define DEFAULT_TAB_COLOR "rgb:9999/9999/9999"

#define INDEX_PANEL_WIDTH 136
#define INDEX_PANEL_HEIGHT 400

/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
	TARGET_GNOME_URI_LIST
};

static GtkTargetEntry target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

typedef enum {
	NO_PART,
	BACKGROUND_PART,
	ICON_PART,
	TITLE_TAB_PART,
	TABS_PART
} IndexPanelPart;

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

	widget_class->drag_data_received  = nautilus_index_panel_drag_data_received;
	widget_class->motion_notify_event = nautilus_index_panel_motion_event;
	widget_class->leave_notify_event = nautilus_index_panel_leave_event;
	widget_class->button_press_event  = nautilus_index_panel_press_event;
}

/* utility routine to allocate the box the holds the command buttons */
static void
make_button_box (NautilusIndexPanel *index_panel)
{
	index_panel->details->button_box = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (index_panel->details->button_box), 8);				
	gtk_widget_show (GTK_WIDGET (index_panel->details->button_box));
	gtk_container_add (GTK_CONTAINER (index_panel->details->container),
			   GTK_WIDGET (index_panel->details->button_box));
	index_panel->details->has_buttons = FALSE;
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
	gtk_widget_set_usize (widget, INDEX_PANEL_WIDTH, INDEX_PANEL_HEIGHT);
  	
	/* create the container box */
  	index_panel->details->container = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (index_panel->details->container), 0);				
	gtk_widget_show (GTK_WIDGET (index_panel->details->container));
	gtk_container_add (GTK_CONTAINER (index_panel),
			   GTK_WIDGET (index_panel->details->container));

	/* allocate and install the index title widget */ 
	index_panel->details->title = NAUTILUS_INDEX_TITLE (nautilus_index_title_new ());
	gtk_widget_show (GTK_WIDGET (index_panel->details->title));
	gtk_box_pack_start (GTK_BOX (index_panel->details->container),
			    GTK_WIDGET (index_panel->details->title),
			    FALSE, FALSE, 0);
	
	/* first, allocate the index tabs */
	index_panel->details->index_tabs = NAUTILUS_INDEX_TABS (nautilus_index_tabs_new ());
	index_panel->details->selected_index = -1;

	/* also, allocate the title tab */
	index_panel->details->title_tab = NAUTILUS_INDEX_TABS (nautilus_index_tabs_new ());
	nautilus_index_tabs_set_title_mode (index_panel->details->title_tab, TRUE);	
	
	gtk_widget_show (GTK_WIDGET (index_panel->details->index_tabs));
	gtk_box_pack_end (GTK_BOX (index_panel->details->container),
			  GTK_WIDGET (index_panel->details->index_tabs),
			  FALSE, FALSE, 0);

	/* allocate and install the meta-tabs */
  	index_panel->details->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_object_ref (GTK_OBJECT (index_panel->details->notebook));
	gtk_object_sink (GTK_OBJECT (index_panel->details->notebook));
		
	gtk_notebook_set_show_tabs (index_panel->details->notebook, FALSE);
	
	/* allocate and install the command button container */
	make_button_box (index_panel);
	
	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (index_panel),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   target_table, NAUTILUS_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

static void
nautilus_index_panel_destroy (GtkObject *object)
{
	NautilusIndexPanel *index_panel;

	index_panel = NAUTILUS_INDEX_PANEL (object);

	gtk_object_unref (GTK_OBJECT (index_panel->details->notebook));

	nautilus_directory_unref (index_panel->details->directory);

	g_free (index_panel->details->uri);
	g_free (index_panel->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* create a new instance */
NautilusIndexPanel *
nautilus_index_panel_new (void)
{
	return NAUTILUS_INDEX_PANEL (gtk_type_new (nautilus_index_panel_get_type ()));
}

static IndexPanelPart
hit_test (NautilusIndexPanel *index_panel,
	  int x, int y)
{
	if (nautilus_point_in_widget (GTK_WIDGET (index_panel->details->index_tabs), x, y)) {
		return TABS_PART;
	}
	
	if (nautilus_point_in_widget (GTK_WIDGET (index_panel->details->title_tab), x, y)) {
		return TITLE_TAB_PART;
	}
	
	if (nautilus_index_title_hit_test_icon (index_panel->details->title, x, y)) {
		return ICON_PART;
	}
	
	if (nautilus_point_in_widget (GTK_WIDGET (index_panel), x, y)) {
		return BACKGROUND_PART;
	}

	return NO_PART;
}

/* FIXME: If passed a bogus URI this could block for a long time. */
static gboolean
uri_is_local_image (const char *uri)
{
	GdkPixbuf *pixbuf;
	
	/* FIXME: Perhaps this should not be hardcoded like this. */
	if (!nautilus_str_has_prefix (uri, "file://")) {
		return FALSE;
	}
	pixbuf = gdk_pixbuf_new_from_file (uri + 7);
	if (pixbuf == NULL) {
		return FALSE;
	}
	gdk_pixbuf_unref (pixbuf);
	return TRUE;
}

static void
receive_dropped_uri_list (NautilusIndexPanel *index_panel,
			  int x, int y,
			  GtkSelectionData *selection_data)
{
	char **uris;
	gboolean exactly_one;
	NautilusFile *file;

	uris = g_strsplit (selection_data->data, "\r\n", 0);
	exactly_one = uris[0] != NULL && uris[1] == NULL;

	/* FIXME: handle background images and keywords soon */
	/* FIXME: handle files by setting the location to the file */
	
	switch (hit_test (index_panel, x, y)) {
	case NO_PART:
	case BACKGROUND_PART:
	case TABS_PART:
	case TITLE_TAB_PART:
		break;
	case ICON_PART:
		/* handle images dropped on the logo specially */
		/* FIXME: Need feedback for cases where there is more than one URI
		 * and where the URI is not alocal image.
		 */
		if (exactly_one && uri_is_local_image (uris[0])) {
			file = nautilus_file_get (index_panel->details->uri);
			if (file != NULL) {
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
							    NULL,
							    uris[0]);
				nautilus_file_unref (file);
			}
		}
		break;
	}

	g_strfreev (uris);
}

static void
receive_dropped_color (NautilusIndexPanel *index_panel,
		       int x, int y,
		       GtkSelectionData *selection_data)
{
	guint16 *channels;
	char *color_spec;

	if (selection_data->length != 8 || selection_data->format != 16) {
		g_warning ("received invalid color data");
		return;
	}
	
	channels = (guint16 *) selection_data->data;
	color_spec = g_strdup_printf ("rgb:%04hX/%04hX/%04hX", channels[0], channels[1], channels[2]);

	switch (hit_test (index_panel, x, y)) {
	case NO_PART:
		g_warning ("dropped color, but not on any part of sidebar");
		break;
	case TABS_PART:
		/* color dropped on main tabs */
		nautilus_index_tabs_receive_dropped_color
			(index_panel->details->index_tabs,
			 x, y, selection_data);
		
		nautilus_directory_set_metadata (index_panel->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR,
					 DEFAULT_TAB_COLOR,
					 color_spec);
		
		break;
	case TITLE_TAB_PART:
		/* color dropped on title tab */
		nautilus_index_tabs_receive_dropped_color
			(index_panel->details->title_tab,
			 x, y, selection_data);
		
		nautilus_directory_set_metadata (index_panel->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR,
					 DEFAULT_TAB_COLOR,
					 color_spec);
		break;
	case ICON_PART:
	case BACKGROUND_PART:
		/* Let the background change based on the dropped color. */
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (GTK_WIDGET (index_panel)),
			 GTK_WIDGET (index_panel), x, y, selection_data);
		break;
	}
	g_free(color_spec);
}

static void  
nautilus_index_panel_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data,
					 guint info, guint time)
{
	NautilusIndexPanel *index_panel;

	g_return_if_fail (NAUTILUS_IS_INDEX_PANEL (widget));

	index_panel = NAUTILUS_INDEX_PANEL (widget);

	switch (info) {
	case TARGET_GNOME_URI_LIST:
	case TARGET_URI_LIST:
		receive_dropped_uri_list (index_panel, x, y, selection_data);
		break;
		
	case TARGET_COLOR:
		receive_dropped_color (index_panel, x, y, selection_data);
		break;
		
	default:
		g_warning ("unknown drop type");
	}
}

/* add a new meta-view to the index panel */
void
nautilus_index_panel_add_meta_view (NautilusIndexPanel *index_panel, NautilusView *meta_view)
{
	GtkWidget *label;
	char cbuf[32];
	const char *description;
	int page_num;
	
	g_return_if_fail (NAUTILUS_IS_INDEX_PANEL (index_panel));
	g_return_if_fail (NAUTILUS_IS_META_VIEW (meta_view));
	
	description = nautilus_meta_view_get_label (NAUTILUS_META_VIEW (meta_view));
	if (description == NULL) {
		/* FIXME: Why is a hex address better than an empty string? */
		g_snprintf (cbuf, sizeof (cbuf), "%p", meta_view);
		description = cbuf;
	} 
	
	label = gtk_label_new (description);
	gtk_widget_show (label);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (index_panel->details->notebook),
				  GTK_WIDGET (meta_view), label);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->notebook),
					  GTK_WIDGET (meta_view));

	/* tell the index tabs about it */
	nautilus_index_tabs_add_view (index_panel->details->index_tabs,
				      description, GTK_WIDGET (meta_view), page_num);
	
	gtk_widget_show (GTK_WIDGET (meta_view));
}

/* remove the passed-in meta-view from the index panel */
void
nautilus_index_panel_remove_meta_view (NautilusIndexPanel *index_panel,
				       NautilusView *meta_view)
{
	int page_num;
	
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (index_panel->details->notebook),
					  GTK_WIDGET (meta_view));
	if (page_num < 0) {
		return;
	}

	gtk_notebook_remove_page (GTK_NOTEBOOK (index_panel->details->notebook),
				  page_num);
}

/* utility to activate the metaview corresponding to the passed in index  */
static void
nautilus_index_panel_activate_meta_view (NautilusIndexPanel *index_panel, int which_view)
{
	char *title;
	GtkNotebook *notebook;

	notebook = index_panel->details->notebook;
	if (index_panel->details->selected_index < 0) {
		gtk_widget_show (GTK_WIDGET (notebook));
		if (GTK_WIDGET (notebook)->parent == NULL) {
			gtk_box_pack_end (GTK_BOX (index_panel->details->container),
					  GTK_WIDGET (notebook),
					  TRUE, TRUE, 0);
		}
		
		gtk_widget_show (GTK_WIDGET (index_panel->details->title_tab));
		if (GTK_WIDGET (index_panel->details->title_tab)->parent == NULL) {
			gtk_box_pack_end (GTK_BOX (index_panel->details->container),
					  GTK_WIDGET (index_panel->details->title_tab),
					  FALSE, FALSE, 0);
		}
	}
	
	index_panel->details->selected_index = which_view;
	title = nautilus_index_tabs_get_title_from_index (index_panel->details->index_tabs,
							  which_view);
	nautilus_index_tabs_set_title (index_panel->details->title_tab, title);
	nautilus_index_tabs_prelight_tab (index_panel->details->title_tab, -1);
    
	g_free (title);
	
	/* hide the buttons, since they look confusing when partially overlapped */
	gtk_widget_hide (GTK_WIDGET (index_panel->details->button_box));
	
	gtk_notebook_set_page (notebook, which_view);
}

/* utility to deactivate the active metaview */
static void
nautilus_index_panel_deactivate_meta_view(NautilusIndexPanel *index_panel)
{
	if (index_panel->details->selected_index >= 0) {
		gtk_widget_hide (GTK_WIDGET (index_panel->details->notebook));
		gtk_widget_hide (GTK_WIDGET (index_panel->details->title_tab));
	}
	
	gtk_widget_show (GTK_WIDGET (index_panel->details->button_box));
	index_panel->details->selected_index = -1;
	nautilus_index_tabs_select_tab (index_panel->details->index_tabs, -1);
}

/* handle mouse motion events by passing it to the tabs if necessary for pre-lighting */
static gboolean
nautilus_index_panel_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y;
	int which_tab;
	int title_top, title_bottom;
	NautilusIndexPanel *index_panel;
	NautilusIndexTabs *index_tabs, *title_tab;

	index_panel = NAUTILUS_INDEX_PANEL (widget);

	gtk_widget_get_pointer(widget, &x, &y);
	
	/* if the click is in the main tabs, tell them about it */
	index_tabs = index_panel->details->index_tabs;
	if (y >= GTK_WIDGET (index_tabs)->allocation.y) {
		which_tab = nautilus_index_tabs_hit_test (index_tabs, x, y);
		nautilus_index_tabs_prelight_tab (index_tabs, which_tab);
	}

	/* also handle prelighting in the title tab if necessary */
	if (index_panel->details->selected_index >= 0) {
		title_tab = index_panel->details->title_tab;
		title_top = GTK_WIDGET (title_tab)->allocation.y;
		title_bottom = title_top + GTK_WIDGET (title_tab)->allocation.height;
		if (y >= title_top && y < title_bottom) {
			which_tab = nautilus_index_tabs_hit_test (title_tab, x, y);
		} else {
			which_tab = -1;
		}
		nautilus_index_tabs_prelight_tab (title_tab, which_tab);
	}

	return TRUE;
}

/* handle the leave event by turning off the preliting */

static gboolean
nautilus_index_panel_leave_event (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusIndexPanel *index_panel;
	NautilusIndexTabs *index_tabs;

	index_panel = NAUTILUS_INDEX_PANEL (widget);
	index_tabs = index_panel->details->index_tabs; 
	nautilus_index_tabs_prelight_tab (index_tabs, -1);

	return TRUE;
}

/* hit-test the index tabs and activate if necessary */

static gboolean
nautilus_index_panel_press_event (GtkWidget *widget, GdkEventButton *event)
{
	int title_top, title_bottom;
	NautilusIndexPanel *index_panel;
	NautilusIndexTabs *index_tabs;
	NautilusIndexTabs *title_tab;
	int rounded_y;
	int which_tab;
		
	index_panel = NAUTILUS_INDEX_PANEL (widget);
	index_tabs = index_panel->details->index_tabs;
	title_tab = index_panel->details->title_tab;
	rounded_y = floor (event->y + .5);

	/* if the click is in the main tabs, tell them about it */
	if (rounded_y >= GTK_WIDGET (index_panel->details->index_tabs)->allocation.y) {
		which_tab = nautilus_index_tabs_hit_test (index_tabs, event->x, event->y);
		if (which_tab >= 0) {
			nautilus_index_tabs_select_tab (index_tabs, which_tab);
			nautilus_index_panel_activate_meta_view (index_panel, which_tab);
			gtk_widget_queue_draw (widget);	
		}
	} 
	
	/* also handle clicks in the title tab if necessary */
	if (index_panel->details->selected_index >= 0) {
		title_top = GTK_WIDGET (index_panel->details->title_tab)->allocation.y;
		title_bottom = title_top + GTK_WIDGET (index_panel->details->title_tab)->allocation.height;
		if (rounded_y >= title_top && rounded_y <= title_bottom) {
			which_tab = nautilus_index_tabs_hit_test (title_tab, event->x, event->y);
			if (which_tab >= 0) {
				/* the user clicked in the title tab, so deactivate the metaview */
				nautilus_index_panel_deactivate_meta_view (index_panel);
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
	
	if (index_panel->details->directory == NULL) {
		return;
	}
	
	background = nautilus_get_widget_background (GTK_WIDGET (index_panel));
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (index_panel->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);
}

/* utility to fork a process to actually execute the button command */

static void
command_button_cb (GtkWidget *button, char *command_str)
{
	pid_t button_pid; 
	NautilusIndexPanel *index_panel;
	char *parameter_ptr;
	
	index_panel = NAUTILUS_INDEX_PANEL (gtk_object_get_user_data (GTK_OBJECT (button)));
	
	parameter_ptr = index_panel->details->uri;
	if (nautilus_str_has_prefix (parameter_ptr, "file://")) {
		parameter_ptr +=  7;
	}
	
	button_pid = fork();
	if (button_pid == 0) {
		execlp (command_str, command_str, parameter_ptr, NULL);
		exit (0);
	}
}

/* utility routine that allocates the command buttons from the command list */

static void
add_command_buttons(NautilusIndexPanel *index_panel, GList *command_list)
{
	char *command_string, *temp_str;
	GList *p;
	GtkWidget *temp_button, *temp_label;
	NautilusCommandInfo *info;
	
	for (p = command_list; p != NULL; p = p->next) {
	        info = p->data;
	        
		index_panel->details->has_buttons = TRUE;
		
	        temp_button = gtk_button_new ();		    
	        temp_label = gtk_label_new (info->display_name);
	        gtk_widget_show (temp_label);
		gtk_container_add (GTK_CONTAINER (temp_button), temp_label); 	
		gtk_box_pack_start (GTK_BOX (index_panel->details->button_box), temp_button, FALSE, TRUE, 2);
		gtk_button_set_relief (GTK_BUTTON (temp_button), GTK_RELIEF_NORMAL);
		gtk_widget_set_usize (GTK_WIDGET (temp_button), 80, 20);

		/* FIXME: we must quote the uri in case it has blanks */
		if (nautilus_str_has_prefix (index_panel->details->uri, "file://")) {
			temp_str = index_panel->details->uri + 7;
		} else {
			temp_str = index_panel->details->uri;
		}
		command_string = g_strdup_printf (info->command_string, temp_str); 		
		
		gtk_signal_connect (GTK_OBJECT (temp_button), "clicked",
				    GTK_SIGNAL_FUNC (command_button_cb), command_string);
                gtk_object_set_user_data (GTK_OBJECT (temp_button), index_panel);
		
		gtk_widget_show (temp_button);
	}
}

/* here's where we set up the command buttons, based on the mime-type of the associated URL */
/* FIXME:  eventually, we need a way to override/augment the type from info in the metadata */

void
nautilus_index_panel_update_buttons (NautilusIndexPanel *index_panel)
{
	NautilusFile *file;
	GList *command_list;
	const char *mime_type;
	
	/* dispose any existing buttons */
	if (index_panel->details->has_buttons) {
		gtk_container_remove (GTK_CONTAINER (index_panel->details->container),
				      GTK_WIDGET (index_panel->details->button_box)); 
		make_button_box (index_panel);
	}
	
	/* allocate a file object and fetch the associated mime-type */
	
	file = nautilus_file_get (index_panel->details->uri);
	if (file != NULL) {
		mime_type = nautilus_file_get_mime_type (file);
	
		/* generate a command list from the mime-type */
		if (mime_type != NULL) {
			command_list = nautilus_mime_type_get_commands (mime_type);	
			/* install a button for each command in the list */
			if (command_list != NULL) {
				add_command_buttons (index_panel, command_list);
			        nautilus_mime_type_dispose_list (command_list);
			
				if (index_panel->details->selected_index != -1)
					gtk_widget_hide (GTK_WIDGET (index_panel->details->button_box));
			}
		}

		nautilus_file_unref (file);	
	}
}

/* this routine populates the index panel with the per-uri information */

void
nautilus_index_panel_update_info (NautilusIndexPanel *index_panel,
				  const char* initial_title)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *background_color, *color_spec;

	directory = nautilus_directory_get (index_panel->details->uri);
	nautilus_directory_unref (index_panel->details->directory);
	index_panel->details->directory = directory;
	
	/* Connect the background changed signal to code that writes the color. */
	background = nautilus_get_widget_background (GTK_WIDGET (index_panel));
        if (index_panel->details->background_connection == 0) {
		index_panel->details->background_connection =
			gtk_signal_connect_object (GTK_OBJECT (background),
						   "changed",
						   nautilus_index_panel_background_changed,
						   GTK_OBJECT (index_panel));
	}

	/* Set up the background color from the metadata. */
	background_color = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
							    DEFAULT_BACKGROUND_COLOR);
	nautilus_background_set_color (background, background_color);
	g_free (background_color);
	
	
	/* set up the color for the tabs */
	color_spec = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR,
							    DEFAULT_TAB_COLOR);
	nautilus_index_tabs_set_color(index_panel->details->index_tabs, color_spec);
	g_free (color_spec);

	color_spec = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR,
							    DEFAULT_TAB_COLOR);
	nautilus_index_tabs_set_color(index_panel->details->title_tab, color_spec);
	g_free (color_spec);

	
	/* tell the title widget about it */
	nautilus_index_title_set_uri (index_panel->details->title,
		                      index_panel->details->uri,
				      initial_title);
			
	/* add keywords if we got any */				

	/* set up the command buttons */
	nautilus_index_panel_update_buttons (index_panel);
}

/* here is the key routine that populates the index panel with the appropriate information when the uri changes */

void
nautilus_index_panel_set_uri (NautilusIndexPanel *index_panel, 
			      const char* new_uri,
			      const char* initial_title)
{       
	/* there's nothing to do if the uri is the same as the current one */ 
	if (nautilus_strcmp (index_panel->details->uri, new_uri) == 0) {
		return;
	}
	
	g_free (index_panel->details->uri);
	index_panel->details->uri = g_strdup (new_uri);
		
	/* populate the per-uri box with the info */
	nautilus_index_panel_update_info (index_panel, initial_title);  	
}

void
nautilus_index_panel_set_title (NautilusIndexPanel *index_panel, const char* new_title)
{       
	nautilus_index_title_set_text (index_panel->details->title,
				       new_title);
}
