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
 * This is the sidebar widget, which displays overview information
 * hosts individual panels for various views.
 *
 */

#include <config.h>
#include "nautilus-sidebar.h"

#include <math.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-keep-last-vertical-box.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-program-choosing.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>
#include <libnautilus-extensions/nautilus-preferences.h>
#include "nautilus-sidebar-tabs.h"
#include "nautilus-sidebar-title.h"
#include "nautilus-link-set-window.h"

struct NautilusSidebarDetails {
	GtkVBox *container;
	NautilusSidebarTitle *title;
	GtkNotebook *notebook;
	NautilusSidebarTabs *sidebar_tabs;
	NautilusSidebarTabs *title_tab;
	GtkHBox *button_box_centerer;
	GtkVBox *button_box;
	gboolean has_buttons;
	char *uri;
	int selected_index;
	NautilusDirectory *directory;
	int background_connection;
    int old_width;
};


static void     nautilus_sidebar_initialize_class   (GtkObjectClass   *object_klass);
static void     nautilus_sidebar_initialize         (GtkObject        *object);
static gboolean nautilus_sidebar_press_event        (GtkWidget        *widget,
						     GdkEventButton   *event);
static gboolean nautilus_sidebar_leave_event        (GtkWidget        *widget,
						     GdkEventCrossing *event);
static gboolean nautilus_sidebar_motion_event       (GtkWidget        *widget,
						     GdkEventMotion   *event);
static void     nautilus_sidebar_destroy            (GtkObject        *object);
static void     nautilus_sidebar_drag_data_received (GtkWidget        *widget,
						     GdkDragContext   *context,
						     int               x,
						     int               y,
						     GtkSelectionData *selection_data,
						     guint             info,
						     guint             time);
static void     nautilus_sidebar_size_allocate      (GtkWidget        *widget,
						     GtkAllocation    *allocation);
static void     nautilus_sidebar_update_info        (NautilusSidebar  *sidebar,
						     const char       *title);
static void     nautilus_sidebar_update_buttons     (NautilusSidebar  *sidebar);
static void     add_command_buttons                 (NautilusSidebar  *sidebar,
						     GList            *application_list);

/* FIXME bug 1245: hardwired sizes */
#define DEFAULT_BACKGROUND_COLOR "rgb:DDDD/DDDD/FFFF"
#define DEFAULT_TAB_COLOR "rgb:9999/9999/9999"

#define SIDEBAR_MINIMUM_WIDTH 24
#define SIDEBAR_MINIMUM_HEIGHT 400

enum {
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
	TARGET_BGIMAGE,
	TARGET_KEYWORD,
	TARGET_GNOME_URI_LIST
};

static GtkTargetEntry target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "property/bgimage", 0, TARGET_BGIMAGE },
	{ "property/keyword", 0, TARGET_KEYWORD },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

typedef enum {
	NO_PART,
	BACKGROUND_PART,
	ICON_PART,
	TITLE_TAB_PART,
	TABS_PART
} SidebarPart;


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSidebar, nautilus_sidebar, GTK_TYPE_EVENT_BOX)

/* initializing the class object by installing the operations we override */
static void
nautilus_sidebar_initialize_class (GtkObjectClass *object_klass)
{
	GtkWidgetClass *widget_class;
	
	NautilusSidebarClass *klass;

	widget_class = GTK_WIDGET_CLASS (object_klass);
	klass = NAUTILUS_SIDEBAR_CLASS (object_klass);

	object_klass->destroy = nautilus_sidebar_destroy;

	widget_class->drag_data_received  = nautilus_sidebar_drag_data_received;
	widget_class->motion_notify_event = nautilus_sidebar_motion_event;
	widget_class->leave_notify_event = nautilus_sidebar_leave_event;
	widget_class->button_press_event  = nautilus_sidebar_press_event;
	widget_class->size_allocate = nautilus_sidebar_size_allocate;

	/* add the "location changed" signal */
	signals[LOCATION_CHANGED]
		= gtk_signal_new ("location_changed",
			GTK_RUN_FIRST,
			object_klass->type,
			GTK_SIGNAL_OFFSET (NautilusSidebarClass,
				location_changed),
			gtk_marshal_NONE__STRING,
			GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_klass, signals, LAST_SIGNAL);
}

/* utility routine to allocate the box the holds the command buttons */
static void
make_button_box (NautilusSidebar *sidebar)
{
	sidebar->details->button_box_centerer = GTK_HBOX (gtk_hbox_new (FALSE, 0));
	gtk_widget_show (GTK_WIDGET (sidebar->details->button_box_centerer));
	gtk_box_pack_start_defaults (GTK_BOX (sidebar->details->container),
			    	     GTK_WIDGET (sidebar->details->button_box_centerer));

	sidebar->details->button_box = GTK_VBOX (nautilus_keep_last_vertical_box_new (GNOME_PAD_SMALL));
	gtk_container_set_border_width (GTK_CONTAINER (sidebar->details->button_box), GNOME_PAD);				
	gtk_widget_show (GTK_WIDGET (sidebar->details->button_box));
	gtk_box_pack_start (GTK_BOX (sidebar->details->button_box_centerer),
			    GTK_WIDGET (sidebar->details->button_box),
			    TRUE, FALSE, 0);
	sidebar->details->has_buttons = FALSE;
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
nautilus_sidebar_initialize (GtkObject *object)
{
	NautilusSidebar *sidebar;
	GtkWidget* widget;
	
	sidebar = NAUTILUS_SIDEBAR (object);
	widget = GTK_WIDGET (object);

	sidebar->details = g_new0 (NautilusSidebarDetails, 1);
	
	/* set the minimum size of the sidebar */
	gtk_widget_set_usize (widget, SIDEBAR_MINIMUM_WIDTH, SIDEBAR_MINIMUM_HEIGHT);
  	
	/* create the container box */
  	sidebar->details->container = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (sidebar->details->container), 0);				
	gtk_widget_show (GTK_WIDGET (sidebar->details->container));
	gtk_container_add (GTK_CONTAINER (sidebar),
			   GTK_WIDGET (sidebar->details->container));

	/* allocate and install the index title widget */ 
	sidebar->details->title = NAUTILUS_SIDEBAR_TITLE (nautilus_sidebar_title_new ());
	gtk_widget_show (GTK_WIDGET (sidebar->details->title));
	gtk_box_pack_start (GTK_BOX (sidebar->details->container),
			    GTK_WIDGET (sidebar->details->title),
			    FALSE, FALSE, GNOME_PAD);
	
	/* first, allocate the index tabs */
	sidebar->details->sidebar_tabs = NAUTILUS_SIDEBAR_TABS (nautilus_sidebar_tabs_new ());
	sidebar->details->selected_index = -1;

	/* also, allocate the title tab */
	sidebar->details->title_tab = NAUTILUS_SIDEBAR_TABS (nautilus_sidebar_tabs_new ());
	nautilus_sidebar_tabs_set_title_mode (sidebar->details->title_tab, TRUE);	
	
	gtk_widget_show (GTK_WIDGET (sidebar->details->sidebar_tabs));
	gtk_box_pack_end (GTK_BOX (sidebar->details->container),
			  GTK_WIDGET (sidebar->details->sidebar_tabs),
			  FALSE, FALSE, 0);

	sidebar->details->old_width = widget->allocation.width;
	
	/* allocate and install the panel tabs */
  	sidebar->details->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_object_ref (GTK_OBJECT (sidebar->details->notebook));
	gtk_object_sink (GTK_OBJECT (sidebar->details->notebook));
		
	gtk_notebook_set_show_tabs (sidebar->details->notebook, FALSE);
	
	/* allocate and install the command button container */
	make_button_box (sidebar);

	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (sidebar),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   target_table, NAUTILUS_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

static void
nautilus_sidebar_destroy (GtkObject *object)
{
	NautilusSidebar *sidebar;

	sidebar = NAUTILUS_SIDEBAR (object);

	gtk_object_unref (GTK_OBJECT (sidebar->details->notebook));

	nautilus_directory_unref (sidebar->details->directory);

	g_free (sidebar->details->uri);
	g_free (sidebar->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* create a new instance */
NautilusSidebar *
nautilus_sidebar_new (void)
{
	return NAUTILUS_SIDEBAR (gtk_type_new (nautilus_sidebar_get_type ()));
}

static SidebarPart
hit_test (NautilusSidebar *sidebar,
	  int x, int y)
{
	if (nautilus_point_in_widget (GTK_WIDGET (sidebar->details->sidebar_tabs), x, y)) {
		return TABS_PART;
	}
	
	if (nautilus_point_in_widget (GTK_WIDGET (sidebar->details->title_tab), x, y)) {
		return TITLE_TAB_PART;
	}
	
	if (nautilus_sidebar_title_hit_test_icon (sidebar->details->title, x, y)) {
		return ICON_PART;
	}
	
	if (nautilus_point_in_widget (GTK_WIDGET (sidebar), x, y)) {
		return BACKGROUND_PART;
	}

	return NO_PART;
}

/* FIXME bugzilla.eazel.com 606: 
 * If passed a bogus URI this could block for a long time. 
 */
static gboolean
uri_is_local_image (const char *uri)
{
	GdkPixbuf *pixbuf;
	
	/* FIXME bugzilla.eazel.com 607: 
	 * Perhaps this should not be hardcoded like this. 
	 */
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
receive_dropped_uri_list (NautilusSidebar *sidebar,
			  int x, int y,
			  GtkSelectionData *selection_data)
{
	char **uris;
	gboolean exactly_one;
	NautilusFile *file;

	uris = g_strsplit (selection_data->data, "\r\n", 0);
	exactly_one = uris[0] != NULL && uris[1] == NULL;

	switch (hit_test (sidebar, x, y)) {
	case NO_PART:
	case BACKGROUND_PART:
		if (exactly_one && uri_is_local_image (uris[0])) {
			nautilus_background_set_tile_image_uri
				(nautilus_get_widget_background (GTK_WIDGET (sidebar)),
				 uris[0]);
		}
		else if (exactly_one) {
			gtk_signal_emit (GTK_OBJECT (sidebar),
					 signals[LOCATION_CHANGED],
			 		 uris[0]);	
		}
		break;
	case TABS_PART:
	case TITLE_TAB_PART:
		break;
	case ICON_PART:
		/* handle images dropped on the logo specially */
		/* FIXME bugzilla.eazel.com 605: 
		 * Need feedback for cases where there is more than one URI 
		 * and where the URI is not a local image.
		 */
		if (exactly_one && uri_is_local_image (uris[0])) {
			file = nautilus_file_get (sidebar->details->uri);
			if (file != NULL) {
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
							    NULL,
							    uris[0]);
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_ICON_SCALE,
							    NULL,
							    NULL);
				nautilus_file_unref (file);
			}
		}
		break;
	}

	g_strfreev (uris);
}

static void
receive_dropped_color (NautilusSidebar *sidebar,
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

	switch (hit_test (sidebar, x, y)) {
	case NO_PART:
		g_warning ("dropped color, but not on any part of sidebar");
		break;
	case TABS_PART:
		/* color dropped on main tabs */
		nautilus_sidebar_tabs_receive_dropped_color
			(sidebar->details->sidebar_tabs,
			 x, y, selection_data);
		
		nautilus_directory_set_metadata
			(sidebar->details->directory,
			 NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR,
			 DEFAULT_TAB_COLOR,
			 color_spec);
		
		break;
	case TITLE_TAB_PART:
		/* color dropped on title tab */
		nautilus_sidebar_tabs_receive_dropped_color
			(sidebar->details->title_tab,
			 x, y, selection_data);
		
		nautilus_directory_set_metadata
			(sidebar->details->directory,
			 NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR,
			 DEFAULT_TAB_COLOR,
			 color_spec);
		break;
	case ICON_PART:
	case BACKGROUND_PART:
		/* Let the background change based on the dropped color. */
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (GTK_WIDGET (sidebar)),
			 GTK_WIDGET (sidebar), x, y, selection_data);
		break;
	}
	g_free(color_spec);
}

/* handle receiving a dropped keyword */

static void
receive_dropped_keyword (NautilusSidebar *sidebar,
		       int x, int y,
		       GtkSelectionData *selection_data)
{
	NautilusFile *file;
	GList *keywords, *word;
	char *title;
			
	/* OK, now we've got the keyword, so add it to the metadata */

	file = nautilus_file_get (sidebar->details->uri);
	if (file == NULL)
		return;

	/* Check and see if it's already there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, selection_data->data, (GCompareFunc) strcmp);
	if (word == NULL)
		keywords = g_list_append (keywords, g_strdup (selection_data->data));
	else
		keywords = g_list_remove_link (keywords, word);

	nautilus_file_set_keywords (file, keywords);
	nautilus_file_unref(file);
	
	/* regenerate the display */
	title = nautilus_sidebar_title_get_text(sidebar->details->title);
	nautilus_sidebar_update_info (sidebar, title);  	
	g_free(title);
}

static void  
nautilus_sidebar_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data,
					 guint info, guint time)
{
	NautilusSidebar *sidebar;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR (widget));

	sidebar = NAUTILUS_SIDEBAR (widget);

	switch (info) {
	case TARGET_GNOME_URI_LIST:
	case TARGET_URI_LIST:
		receive_dropped_uri_list (sidebar, x, y, selection_data);
		break;
	case TARGET_COLOR:
		receive_dropped_color (sidebar, x, y, selection_data);
		break;
	case TARGET_BGIMAGE:
		if (hit_test (sidebar, x, y) == BACKGROUND_PART)
			receive_dropped_uri_list (sidebar, x, y, selection_data);
		break;	
	case TARGET_KEYWORD:
		receive_dropped_keyword(sidebar, x, y, selection_data);
		break;
	default:
		g_warning ("unknown drop type");
	}
}

/* add a new panel to the sidebar */
void
nautilus_sidebar_add_panel (NautilusSidebar *sidebar, NautilusViewFrame *panel)
{
	GtkWidget *label;
	char *description;
	int page_num;
	
	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar));
	g_return_if_fail (NAUTILUS_IS_VIEW_FRAME (panel));
	
	description = nautilus_view_frame_get_label (panel);

	label = gtk_label_new (description);

	gtk_widget_show (label);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (sidebar->details->notebook),
				  GTK_WIDGET (panel), label);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (sidebar->details->notebook),
					  GTK_WIDGET (panel));

	/* tell the index tabs about it */
	nautilus_sidebar_tabs_add_view (sidebar->details->sidebar_tabs,
				      description, GTK_WIDGET (panel), page_num);
	
	g_free (description);

	gtk_widget_show (GTK_WIDGET (panel));
}

/* remove the passed-in panel from the sidebar */
void
nautilus_sidebar_remove_panel (NautilusSidebar *sidebar,
				       NautilusViewFrame *panel)
{
	int page_num;
	char *description;
	
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (sidebar->details->notebook),
					  GTK_WIDGET (panel));
	g_return_if_fail (page_num >= 0);

	gtk_notebook_remove_page (GTK_NOTEBOOK (sidebar->details->notebook),
				  page_num);

	description = nautilus_view_frame_get_label (panel);

	/* Remove the tab associated with this panel */
	nautilus_sidebar_tabs_remove_view (sidebar->details->sidebar_tabs, description);

	g_free (description);
}

/* utility to activate the panel corresponding to the passed in index  */
static void
nautilus_sidebar_activate_panel (NautilusSidebar *sidebar, int which_view)
{
	char *title;
	GtkNotebook *notebook;

	notebook = sidebar->details->notebook;
	if (sidebar->details->selected_index < 0) {
		gtk_widget_show (GTK_WIDGET (notebook));
		if (GTK_WIDGET (notebook)->parent == NULL) {
			gtk_box_pack_end (GTK_BOX (sidebar->details->container),
					  GTK_WIDGET (notebook),
					  TRUE, TRUE, 0);
		}
		
		gtk_widget_show (GTK_WIDGET (sidebar->details->title_tab));
		if (GTK_WIDGET (sidebar->details->title_tab)->parent == NULL) {
			gtk_box_pack_end (GTK_BOX (sidebar->details->container),
					  GTK_WIDGET (sidebar->details->title_tab),
					  FALSE, FALSE, 0);
		}
	}
	
	sidebar->details->selected_index = which_view;
	title = nautilus_sidebar_tabs_get_title_from_index (sidebar->details->sidebar_tabs,
							  which_view);
	nautilus_sidebar_tabs_set_title (sidebar->details->title_tab, title);
	nautilus_sidebar_tabs_prelight_tab (sidebar->details->title_tab, -1);
    
	g_free (title);
	
	/* hide the buttons, since they look confusing when partially overlapped */
	gtk_widget_hide (GTK_WIDGET (sidebar->details->button_box));
	
	gtk_notebook_set_page (notebook, which_view);
}

/* utility to deactivate the active panel */
static void
nautilus_sidebar_deactivate_panel(NautilusSidebar *sidebar)
{
	if (sidebar->details->selected_index >= 0) {
		gtk_widget_hide (GTK_WIDGET (sidebar->details->notebook));
		gtk_widget_hide (GTK_WIDGET (sidebar->details->title_tab));
	}
	
	gtk_widget_show (GTK_WIDGET (sidebar->details->button_box));
	sidebar->details->selected_index = -1;
	nautilus_sidebar_tabs_select_tab (sidebar->details->sidebar_tabs, -1);
}

/* handle mouse motion events by passing it to the tabs if necessary for pre-lighting */
static gboolean
nautilus_sidebar_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y;
	int which_tab;
	int title_top, title_bottom;
	NautilusSidebar *sidebar;
	NautilusSidebarTabs *sidebar_tabs, *title_tab;

	sidebar = NAUTILUS_SIDEBAR (widget);

	gtk_widget_get_pointer(widget, &x, &y);
	
	/* if the click is in the main tabs, tell them about it */
	sidebar_tabs = sidebar->details->sidebar_tabs;
	if (y >= GTK_WIDGET (sidebar_tabs)->allocation.y) {
		which_tab = nautilus_sidebar_tabs_hit_test (sidebar_tabs, x, y);
		nautilus_sidebar_tabs_prelight_tab (sidebar_tabs, which_tab);
	}

	/* also handle prelighting in the title tab if necessary */
	if (sidebar->details->selected_index >= 0) {
		title_tab = sidebar->details->title_tab;
		title_top = GTK_WIDGET (title_tab)->allocation.y;
		title_bottom = title_top + GTK_WIDGET (title_tab)->allocation.height;
		if (y >= title_top && y < title_bottom) {
			which_tab = nautilus_sidebar_tabs_hit_test (title_tab, x, y);
		} else {
			which_tab = -1;
		}
		nautilus_sidebar_tabs_prelight_tab (title_tab, which_tab);
	}

	return TRUE;
}

/* handle the leave event by turning off the preliting */

static gboolean
nautilus_sidebar_leave_event (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusSidebar *sidebar;
	NautilusSidebarTabs *sidebar_tabs;

	sidebar = NAUTILUS_SIDEBAR (widget);
	sidebar_tabs = sidebar->details->sidebar_tabs; 
	nautilus_sidebar_tabs_prelight_tab (sidebar_tabs, -1);

	return TRUE;
}

/* hit-test the index tabs and activate if necessary */

static gboolean
nautilus_sidebar_press_event (GtkWidget *widget, GdkEventButton *event)
{
	int title_top, title_bottom;
	NautilusSidebar *sidebar;
	NautilusSidebarTabs *sidebar_tabs;
	NautilusSidebarTabs *title_tab;
	int rounded_y;
	int which_tab;
		
	sidebar = NAUTILUS_SIDEBAR (widget);
	sidebar_tabs = sidebar->details->sidebar_tabs;
	title_tab = sidebar->details->title_tab;
	rounded_y = floor (event->y + .5);

	/* if the click is in the main tabs, tell them about it */
	if (rounded_y >= GTK_WIDGET (sidebar->details->sidebar_tabs)->allocation.y) {
		which_tab = nautilus_sidebar_tabs_hit_test (sidebar_tabs, event->x, event->y);
		if (which_tab >= 0) {
			nautilus_sidebar_tabs_select_tab (sidebar_tabs, which_tab);
			nautilus_sidebar_activate_panel (sidebar, which_tab);
			gtk_widget_queue_draw (widget);	
		}
	} 
	
	/* also handle clicks in the title tab if necessary */
	if (sidebar->details->selected_index >= 0) {
		title_top = GTK_WIDGET (sidebar->details->title_tab)->allocation.y;
		title_bottom = title_top + GTK_WIDGET (sidebar->details->title_tab)->allocation.height;
		if (rounded_y >= title_top && rounded_y <= title_bottom) {
			which_tab = nautilus_sidebar_tabs_hit_test (title_tab, event->x, event->y);
			if (which_tab >= 0) {
				/* the user clicked in the title tab, so deactivate the panel */
				nautilus_sidebar_deactivate_panel (sidebar);
			}
		}
	}
	return TRUE;
}

static void
nautilus_sidebar_background_changed (NautilusSidebar *sidebar)
{
	NautilusBackground *background;
	char *color_spec, *image;
	
	if (sidebar->details->directory == NULL) {
		return;
	}
	
	background = nautilus_get_widget_background (GTK_WIDGET (sidebar));
	
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);

	image = nautilus_background_get_tile_image_uri (background);
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
					 NULL,
					 image);	
	g_free (image);
}

static void
command_button_callback (GtkWidget *button, char *command_str)
{
	NautilusSidebar *sidebar;
	
	sidebar = NAUTILUS_SIDEBAR (gtk_object_get_user_data (GTK_OBJECT (button)));

	nautilus_launch_application (command_str, sidebar->details->uri);	
}

/* interpret commands for buttons specified by metadata. Handle some built-in ones explicitly, or fork
   a shell to handle general ones */
/* for now, we only handle a few built in commands */
static void
metadata_button_callback (GtkWidget *button, char *command_str)
{
	GtkWindow *window;
	NautilusSidebar *sidebar;
	
	sidebar = NAUTILUS_SIDEBAR (gtk_object_get_user_data (GTK_OBJECT (button)));
	if (!strcmp(command_str, "#linksets")) {
		window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar)));
		nautilus_link_set_toggle_configure_window(sidebar->details->uri, window);
	}
}

static void
nautilus_sidebar_chose_application_callback (GnomeVFSMimeApplication *application,
					     gpointer callback_data)
{
	g_assert (NAUTILUS_IS_SIDEBAR (callback_data));

	if (application != NULL) {
		nautilus_launch_application 
			(application->command, 
			 NAUTILUS_SIDEBAR (callback_data)->details->uri);
	}
}

static void
open_with_callback (GtkWidget *button, gpointer ignored)
{
	NautilusSidebar *sidebar;
	NautilusFile *file;
	
	sidebar = NAUTILUS_SIDEBAR (gtk_object_get_user_data (GTK_OBJECT (button)));

	file = nautilus_file_get (sidebar->details->uri);
	g_return_if_fail (file != NULL);

	nautilus_choose_application_for_file
		(file,
		 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))),
		 nautilus_sidebar_chose_application_callback,
		 sidebar);

	nautilus_file_unref (file);
}

/* utility routine that allocates the command buttons from the command list */

static void
add_command_buttons (NautilusSidebar *sidebar, GList *application_list)
{
	char *command_string, *temp_str;
	GList *p;
	GtkWidget *temp_button;
	GnomeVFSMimeApplication *application;

	/* There's always at least the "Open with..." button */
	sidebar->details->has_buttons = TRUE;

	for (p = application_list; p != NULL; p = p->next) {
	        application = p->data;	        

		temp_str = g_strdup_printf (_("Open with %s"), application->name);
	        temp_button = gtk_button_new_with_label (temp_str);		    
		gtk_box_pack_start (GTK_BOX (sidebar->details->button_box), 
				    temp_button, 
				    FALSE, FALSE, 
				    0);

		temp_str = g_strdup_printf("'%s'", 
		             nautilus_str_has_prefix (sidebar->details->uri, "file://") ?
			     sidebar->details->uri + 7 : sidebar->details->uri);

		command_string = g_strdup_printf (application->command, temp_str); 		
		g_free(temp_str);

		nautilus_gtk_signal_connect_free_data 
			(GTK_OBJECT (temp_button), "clicked",
			 GTK_SIGNAL_FUNC (command_button_callback), command_string);
                gtk_object_set_user_data (GTK_OBJECT (temp_button), sidebar);
		
		gtk_widget_show (temp_button);
	}

	/* Catch-all button after all the others. */
	temp_button = gtk_button_new_with_label (_("Open with..."));
	gtk_signal_connect (GTK_OBJECT (temp_button),  "clicked",
			    open_with_callback, NULL);
	gtk_object_set_user_data (GTK_OBJECT (temp_button), sidebar);
	gtk_widget_show (temp_button);
	gtk_box_pack_start (GTK_BOX (sidebar->details->button_box),
			    temp_button, FALSE, FALSE, 0);
}

/* utility to construct command buttons for the sidebar from the passed in metadata string */

static void
add_buttons_from_metadata(NautilusSidebar *sidebar, const char *button_data)
{
	char **terms;
	char *current_term, *temp_str;
	char *button_name, *command_string;
	const char *term;
	int index;
	GtkWidget *temp_button;
	
	/* split the button specification into a set of terms */	
	button_name = NULL;
	terms = g_strsplit (button_data, ";", 0);	
	
	/* for each term, either create a button or attach a property to one */
	for (index = 0; (term = terms[index]) != NULL; index++) {
		current_term = g_strdup(term);
		temp_str = strchr(current_term, '=');
		if (temp_str) {
			*temp_str = '\0';
			if (!g_strcasecmp(current_term, "button")) {
				button_name = g_strdup(temp_str + 1);
			} else if (!g_strcasecmp(current_term, "script")) {
			        if (button_name != NULL) {
			        	temp_button = gtk_button_new_with_label (button_name);		    
					gtk_box_pack_start (GTK_BOX (sidebar->details->button_box), 
						    temp_button, 
						    FALSE, FALSE, 
						    0);
					sidebar->details->has_buttons = TRUE;

					command_string = g_strdup(temp_str + 1);		
					g_free(button_name);

					nautilus_gtk_signal_connect_free_data 
						(GTK_OBJECT (temp_button), "clicked",
					 	GTK_SIGNAL_FUNC (metadata_button_callback), command_string);
		                	gtk_object_set_user_data (GTK_OBJECT (temp_button), sidebar);
				
					gtk_widget_show (temp_button);			
				}
			}
		}
		g_free(current_term);
	}	
	g_strfreev (terms);
}

/**
 * nautilus_sidebar_update_buttons:
 * 
 * Update the list of program-launching buttons based on the current uri.
 */
void
nautilus_sidebar_update_buttons (NautilusSidebar *sidebar)
{
	char *button_data;
	GList *short_application_list;
	
	/* dispose of any existing buttons */
	if (sidebar->details->has_buttons) {
		gtk_container_remove (GTK_CONTAINER (sidebar->details->container),
				      GTK_WIDGET (sidebar->details->button_box_centerer)); 
		make_button_box (sidebar);
	}

	/* create buttons from directory metadata if necessary */
	
	button_data = nautilus_directory_get_metadata (sidebar->details->directory,
				NAUTILUS_METADATA_KEY_SIDEBAR_BUTTONS,
				NULL);
	if (button_data) {
		add_buttons_from_metadata(sidebar, button_data);
		g_free(button_data);
	}
	
	/* Make buttons for each item in short list + "Open with..." catchall,
	 * unless there aren't any applications at all in complete list. 
	 */
	if (nautilus_mime_has_any_applications_for_uri (sidebar->details->uri)) {
		short_application_list = 
			nautilus_mime_get_short_list_applications_for_uri (sidebar->details->uri);
		add_command_buttons (sidebar, short_application_list);
		gnome_vfs_mime_application_list_free (short_application_list);

		/* Hide button box if a sidebar panel is showing. */
		if (sidebar->details->selected_index != -1) {
			gtk_widget_hide (GTK_WIDGET (sidebar->details->button_box));
		}
	}
}

/* this routine populates the sidebar with the per-uri information */

void
nautilus_sidebar_update_info (NautilusSidebar *sidebar,
				  const char* initial_title)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *background_color, *color_spec;
	char *background_image;
	
	directory = nautilus_directory_get (sidebar->details->uri);
	nautilus_directory_unref (sidebar->details->directory);
	sidebar->details->directory = directory;
	
	/* Connect the background changed signal to code that writes the color. */
	background = nautilus_get_widget_background (GTK_WIDGET (sidebar));
        if (sidebar->details->background_connection == 0) {
		sidebar->details->background_connection =
			gtk_signal_connect_object (GTK_OBJECT (background),
						   "changed",
						   nautilus_sidebar_background_changed,
						   GTK_OBJECT (sidebar));
	}

	/* Set up the background color and image from the metadata. */
	background_color = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
							    DEFAULT_BACKGROUND_COLOR);
	background_image = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
							    NULL);
	
	nautilus_background_set_color (background, background_color);	
	g_free (background_color);
	
	nautilus_background_set_tile_image_uri (background, background_image);
	g_free (background_image);
	
	/* set up the color for the tabs */
	color_spec = nautilus_directory_get_metadata (directory,
						      NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR,
						      DEFAULT_TAB_COLOR);
	nautilus_sidebar_tabs_set_color(sidebar->details->sidebar_tabs, color_spec);
	g_free (color_spec);

	color_spec = nautilus_directory_get_metadata (directory,
						      NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR,
						      DEFAULT_TAB_COLOR);
	nautilus_sidebar_tabs_set_color(sidebar->details->title_tab, color_spec);
	g_free (color_spec);

	
	/* tell the title widget about it */
	nautilus_sidebar_title_set_uri (sidebar->details->title,
		                      sidebar->details->uri,
				      initial_title);
	
	/* set up the command buttons */
	nautilus_sidebar_update_buttons (sidebar);
}

/* here is the key routine that populates the sidebar with the appropriate information when the uri changes */

void
nautilus_sidebar_set_uri (NautilusSidebar *sidebar, 
			      const char* new_uri,
			      const char* initial_title)
{       
	/* there's nothing to do if the uri is the same as the current one */ 
	if (nautilus_strcmp (sidebar->details->uri, new_uri) == 0) {
		return;
	}
	
	g_free (sidebar->details->uri);
	sidebar->details->uri = g_strdup (new_uri);
		
	/* populate the per-uri box with the info */
	nautilus_sidebar_update_info (sidebar, initial_title);  	
}

void
nautilus_sidebar_set_title (NautilusSidebar *sidebar, const char* new_title)
{       
	nautilus_sidebar_title_set_text (sidebar->details->title,
					 new_title);
}

/* we override size allocate so we can remember our size when it changes, since the paned widget
   doesn't generate a signal */
   
static void
nautilus_sidebar_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusSidebar *sidebar = NAUTILUS_SIDEBAR(widget);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	/* remember the size if it changed */
	
	if (widget->allocation.width != sidebar->details->old_width) {
		sidebar->details->old_width = widget->allocation.width;
 		nautilus_preferences_set_enum(NAUTILUS_PREFERENCES_SIDEBAR_WIDTH, widget->allocation.width);
	
	}	
}
