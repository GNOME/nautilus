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
#include <liboaf/liboaf.h>
#include <parser.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
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
#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>

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
	char *default_background_color;
	char *default_background_image;
	int selected_index;
	NautilusDirectory *directory;
	gboolean background_connected;
	int old_width;
};

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

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
static void	nautilus_sidebar_read_theme	    (NautilusSidebar *sidebar);

static void     nautilus_sidebar_size_allocate      (GtkWidget        *widget,
						     GtkAllocation    *allocation);
static void	nautilus_sidebar_theme_changed	    (gpointer user_data);
static void     nautilus_sidebar_update_appearance  (NautilusSidebar  *sidebar);
static void     nautilus_sidebar_update_buttons     (NautilusSidebar  *sidebar);
static void     add_command_buttons                 (NautilusSidebar  *sidebar,
						     GList            *application_list);

/* FIXME bug 1245: hardwired sizes */
#define DEFAULT_TAB_COLOR "rgb:9999/9999/9999"

#define SIDEBAR_MINIMUM_WIDTH 1
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
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
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

	/* load the default background from the current theme */
	nautilus_sidebar_read_theme(sidebar);
	  	
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
	
	/* allocate the index tabs */
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

	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, nautilus_sidebar_theme_changed, sidebar);	

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
	g_free (sidebar->details->default_background_color);
	g_free (sidebar->details->default_background_image);
	
	g_free (sidebar->details);
	
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_sidebar_theme_changed,
					      sidebar);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* utility routines to test if sidebar panel is currently enabled */
static char *
nautilus_sidebar_get_sidebar_panel_key (const char *panel_iid)
{
	g_return_val_if_fail (panel_iid != NULL, NULL);

	return g_strdup_printf ("%s/%s", NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE, panel_iid);
}

static gboolean
nautilus_sidebar_sidebar_panel_enabled (const char *panel_iid)
{
	gboolean enabled;
        gchar	 *key;

	key = nautilus_sidebar_get_sidebar_panel_key (panel_iid);
        enabled = nautilus_preferences_get_boolean (key, FALSE);

        g_free (key);
        return enabled;
}

/* callback to handle resetting the background */
static void
reset_background_callback(GtkWidget *menu_item, GtkWidget *sidebar)
{
	NautilusBackground *background;
	background = nautilus_get_widget_background(sidebar);
	if (background) { 
		nautilus_background_reset(background); 
	}
}

/* callback for sidebar panel menu items to toggle their visibility */
static void
toggle_sidebar_panel(GtkWidget *widget, char *sidebar_id)
{
        gchar	 *key;

	key = nautilus_sidebar_get_sidebar_panel_key (sidebar_id);
	nautilus_preferences_set_boolean(key, !nautilus_preferences_get_boolean(key, FALSE));
	g_free(key); 
}

/* utility routine to add a menu item for each potential sidebar panel */

static void
nautilus_sidebar_add_panel_items(NautilusSidebar *sidebar, GtkWidget *menu)
{
	CORBA_Environment ev;
	const char *query;
        OAF_ServerInfoList *oaf_result;
	int i;
	gboolean enabled;
	GList *name_list;
	GtkWidget *menu_item;
	NautilusViewIdentifier *id;

	CORBA_exception_init (&ev);

	/* ask OAF for all of the sidebars panel */
	query = "nautilus:sidebar_panel_name.defined() AND repo_ids.has ('IDL:Bonobo/Control:1.0')";
	oaf_result = oaf_query (query, NULL, &ev);
	
	/* loop through the results, appending a new menu item for each unique sidebar panel */
	name_list = NULL;
        if (ev._major == CORBA_NO_EXCEPTION && oaf_result != NULL) {
		for (i = 0; i < oaf_result->_length; i++) {
			id = nautilus_view_identifier_new_from_sidebar_panel
				(&oaf_result->_buffer[i]);
			/* check to see if we've seen this one */
			if (g_list_find_custom (name_list, id->name, (GCompareFunc) strcmp) == NULL) {
				name_list = g_list_append (name_list, g_strdup (id->name));
			
				/* add a check menu item */
				menu_item = gtk_check_menu_item_new_with_label(id->name);
				enabled = nautilus_sidebar_sidebar_panel_enabled(id->iid);
				gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_item), enabled);
				gtk_widget_show(menu_item);
				gtk_menu_append (GTK_MENU(menu), menu_item);
				gtk_signal_connect_full (GTK_OBJECT (menu_item), "activate",
							 GTK_SIGNAL_FUNC (toggle_sidebar_panel),
							 NULL, g_strdup(id ->iid), g_free,
							 FALSE, FALSE);
			}
			nautilus_view_identifier_free (id);
		}
	} 
	if (name_list != NULL)
		nautilus_g_list_free_deep(name_list);
		
	if (oaf_result != NULL) {
		CORBA_free (oaf_result);
	}
	
	CORBA_exception_free (&ev);
}

/* check to see if the background matches the default */
static gboolean
nautilus_sidebar_background_is_default (NautilusSidebar *sidebar)
{
	char *background_color, *background_image;
	gboolean is_default;
	
	background_color = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
							    NULL);
	background_image = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
								    NULL);
	
	is_default = background_color == NULL && background_image == NULL;
	g_free (background_color);
	g_free (background_image);
	
	return is_default;
}

/* create the context menu */
GtkWidget *
nautilus_sidebar_create_context_menu (NautilusSidebar *sidebar)
{
	GtkWidget *menu, *menu_item;
	NautilusBackground *background;
	gboolean has_background;

	background = nautilus_get_widget_background (GTK_WIDGET(sidebar));
	has_background = background && !nautilus_sidebar_background_is_default (sidebar);
	
	menu = gtk_menu_new ();
	
	/* add the reset background item, possibly disabled */
	menu_item = gtk_menu_item_new_with_label (_("Reset Background"));
 	gtk_widget_show (menu_item);
	gtk_menu_append (GTK_MENU(menu), menu_item);
        gtk_widget_set_sensitive (menu_item, has_background);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate", reset_background_callback, sidebar);

	/* add a separator */
	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_append (GTK_MENU(menu), menu_item);
	
	/* add the sidebar panels */
	nautilus_sidebar_add_panel_items(sidebar, menu);
	return menu;
}

/* create a new instance */
NautilusSidebar *
nautilus_sidebar_new (void)
{
	return NAUTILUS_SIDEBAR (gtk_type_new (nautilus_sidebar_get_type ()));
}

/* utility routine to handle mapping local file names to a uri */
static char*
map_local_data_file (char *file_name)
{
	char *temp_str;
	if (file_name && !nautilus_istr_has_prefix (file_name, "file://")) {

		if (nautilus_str_has_prefix (file_name, "./")) {
			temp_str = nautilus_theme_get_image_path (file_name + 2);
		} else {
			temp_str = g_strdup_printf ("%s/%s", NAUTILUS_DATADIR, file_name);
		}
		
		g_free (file_name);
		file_name = nautilus_get_uri_from_local_path (temp_str);
		g_free (temp_str);
	}
	return file_name;
}

/* read the theme file and set up the default backgrounds and images accordingly */
static void
nautilus_sidebar_read_theme (NautilusSidebar *sidebar)
{
	char *background_color, *background_image;
	
	background_color = nautilus_theme_get_theme_data ("sidebar", NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR);
	background_image = nautilus_theme_get_theme_data ("sidebar", NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE);
	
	g_free(sidebar->details->default_background_color);
	sidebar->details->default_background_color = NULL;
	g_free(sidebar->details->default_background_image);
	sidebar->details->default_background_image = NULL;
			
	if (background_color && strlen (background_color)) {
		sidebar->details->default_background_color = g_strdup(background_color);
	}
			
	/* set up the default background image */
	
	background_image = map_local_data_file (background_image);
	if (background_image && strlen (background_image)) {
		sidebar->details->default_background_image = g_strdup(background_image);
	}

	g_free (background_color);
	g_free (background_image);
}

/* handler for handling theme changes */

static void
nautilus_sidebar_theme_changed (gpointer user_data)
{
	NautilusSidebar *sidebar;
	
	sidebar = NAUTILUS_SIDEBAR (user_data);
	nautilus_sidebar_read_theme (sidebar);
	nautilus_sidebar_update_appearance (sidebar);
	gtk_widget_queue_draw (GTK_WIDGET (sidebar)) ;	
}

/* hit testing */

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

/* utility to test if a uri refers to a local image */
static gboolean
uri_is_local_image (const char *uri)
{
	GdkPixbuf *pixbuf;
	char *image_path;
	
	if (nautilus_is_remote_uri (uri)) {
		return FALSE;
	}
	
	image_path = nautilus_get_local_path_from_uri (uri);
	pixbuf = gdk_pixbuf_new_from_file (image_path);
	g_free (image_path);
	
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
		/* FIXME: Does this work for all images, or only background images?
		 * Other views handle background images differently from other URIs.
		 */
		if (exactly_one && uri_is_local_image (uris[0])) {
			nautilus_background_receive_dropped_background_image
				(nautilus_get_widget_background (GTK_WIDGET (sidebar)),
				 uris[0]);
		} else if (exactly_one) {
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
		
		/* regenerate the display */
		nautilus_sidebar_update_appearance (sidebar);  	
		
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

	/* FIXME: This is a cut and paste copy of code that's in the icon dnd code. */
			
	/* OK, now we've got the keyword, so add it to the metadata */

	/* FIXME bugzilla.eazel.com 866: Can't expect to read the
	 * keywords list instantly here. We might need to read the
	 * metafile first.
	 */
	file = nautilus_file_get (sidebar->details->uri);
	if (file == NULL)
		return;

	/* Check and see if it's already there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, selection_data->data, (GCompareFunc) strcmp);
	if (word == NULL) {
		keywords = g_list_append (keywords, g_strdup (selection_data->data));
	} else {
		keywords = g_list_remove_link (keywords, word);
		g_free (word->data);
		g_list_free (word);
	}

	nautilus_file_set_keywords (file, keywords);
	nautilus_file_unref (file);
	
	/* regenerate the display */
	nautilus_sidebar_update_appearance (sidebar);  	
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


	/* FIXME bugzilla.eazel.com 1840: 
	 * This g_return_if_fail gets hit in cases where the sidebar panel
	 * fails when loading, which causes the panel's tab to be left behind.
	 */
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
	gtk_widget_hide (GTK_WIDGET (sidebar->details->button_box_centerer));
	gtk_widget_hide (GTK_WIDGET (sidebar->details->title));
	
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
	
	gtk_widget_show (GTK_WIDGET (sidebar->details->button_box_centerer));
	gtk_widget_show (GTK_WIDGET (sidebar->details->title));
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
	GtkWidget *menu;
	NautilusSidebar *sidebar;
	NautilusSidebarTabs *sidebar_tabs;
	NautilusSidebarTabs *title_tab;
	int rounded_y;
	int which_tab;
		
	sidebar = NAUTILUS_SIDEBAR (widget);

	/* handle the context menu */
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		menu = nautilus_sidebar_create_context_menu (sidebar);	
		nautilus_pop_up_context_menu (GTK_MENU(menu),
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
				      event->button);
		return TRUE;
	}
	
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

/* handle the background changed signal by writing out the settings to metadata */
static void
background_settings_changed_callback (NautilusBackground *background, NautilusSidebar *sidebar)
{
	char *color_spec, *image;

	g_assert (NAUTILUS_IS_BACKGROUND (background));
	g_assert (NAUTILUS_IS_SIDEBAR (sidebar));

	if (sidebar->details->directory == NULL) {
		return;
	}
	
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
					 sidebar->details->default_background_color,
					 color_spec);
	g_free (color_spec);

	image = nautilus_background_get_tile_image_uri (background);
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
					 sidebar->details->default_background_image,
					 image);	
	g_free (image);
}

/* we generally want to ignore the appearance changed signal, but we need it to redraw the
   the sidebar in the case where we're loading the background image, so check for that */
static void
background_appearance_changed_callback (NautilusBackground *background, NautilusSidebar *sidebar)
{
	gboolean is_default_color, is_default_image;
	char *background_image, *background_color;

	background_color = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
							    sidebar->details->default_background_color);

	background_image = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
							    sidebar->details->default_background_image);
	
	nautlius_sidebar_title_select_text_color (sidebar->details->title);
	
	is_default_color = !nautilus_strcmp(background_color, sidebar->details->default_background_color);
	is_default_image = !nautilus_strcmp(background_image, sidebar->details->default_background_image);
	
	if (is_default_color && is_default_image) {
		nautilus_sidebar_update_appearance (sidebar);  	
	}
	g_free (background_color);
	g_free (background_image);
}

/* handle the background reset signal by writing out NULL to metadata and setting the backgrounds
   fields to their default values */
static void
background_reset_callback (NautilusBackground *background, NautilusSidebar *sidebar)
{
	char *combine_mode;
	
	if (sidebar->details->directory == NULL) {
		return;
	}
	
	/* set up the defaults, but don't write the metdata */
	gtk_signal_handler_block_by_func (GTK_OBJECT(background),
					  background_settings_changed_callback,
					  sidebar);
	nautilus_background_set_color (background, sidebar->details->default_background_color);	
	nautilus_background_set_tile_image_uri (background, sidebar->details->default_background_image);
	
	combine_mode = nautilus_theme_get_theme_data ("sidebar", "COMBINE");
	nautilus_background_set_combine_mode (background, combine_mode != NULL);
	g_free (combine_mode);
	
	gtk_signal_handler_unblock_by_func (GTK_OBJECT(background),
					    background_settings_changed_callback,
					    sidebar);
					   
	/* reset the metadata */
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
					 sidebar->details->default_background_color,
					 NULL);
	nautilus_directory_set_metadata (sidebar->details->directory,
					 NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
					 sidebar->details->default_background_image,
					 NULL);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (background),
				      "reset");
}

static void
command_button_callback (GtkWidget *button, char *id_str)
{
	NautilusSidebar *sidebar;
	GnomeVFSMimeApplication *application;
	
	sidebar = NAUTILUS_SIDEBAR (gtk_object_get_user_data (GTK_OBJECT (button)));

	application = gnome_vfs_mime_application_new_from_id (id_str);

	nautilus_launch_application (application, sidebar->details->uri);	

	gnome_vfs_mime_application_free (application);
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
			(application, 
			 NAUTILUS_SIDEBAR (callback_data)->details->uri);
	}
}

static void
open_with_callback (GtkWidget *button, gpointer ignored)
{
	NautilusSidebar *sidebar;
	NautilusFile *file;
	
	sidebar = NAUTILUS_SIDEBAR (gtk_object_get_user_data (GTK_OBJECT (button)));
	
	/* FIXME bugzilla.eazel.com 866: Can't expect to put this
	 * window up instantly. We might need to read the metafile
	 * first.
	 */
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
	char *id_string, *temp_str;
	GList *p;
	GtkWidget *temp_button;
	GnomeVFSMimeApplication *application;

	/* There's always at least the "Open with..." button */
	sidebar->details->has_buttons = TRUE;

	for (p = application_list; p != NULL; p = p->next) {
	        application = p->data;	        

		temp_str = g_strdup_printf (_("Open with %s"), application->name);
	        temp_button = gtk_button_new_with_label (temp_str);
		g_free (temp_str);
		gtk_box_pack_start (GTK_BOX (sidebar->details->button_box), 
				    temp_button, 
				    FALSE, FALSE, 
				    0);

		/* FIXME: Security hole? Can't use a string from the
		 * MIME file as a printf format string without first
		 * checking it over somehow.
		 */
		temp_str = g_strdup_printf
			("'%s'", 
			 nautilus_istr_has_prefix (sidebar->details->uri, "file://")
			 ? sidebar->details->uri + 7 : sidebar->details->uri);
		id_string = g_strdup_printf (application->id, temp_str); 		
		g_free (temp_str);

		nautilus_gtk_signal_connect_free_data 
			(GTK_OBJECT (temp_button), "clicked",
			 GTK_SIGNAL_FUNC (command_button_callback), id_string);

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
			gtk_widget_hide (GTK_WIDGET (sidebar->details->button_box_centerer));
			gtk_widget_hide (GTK_WIDGET (sidebar->details->title));
		}
	}
}

void
nautilus_sidebar_update_appearance (NautilusSidebar *sidebar)
{
	NautilusBackground *background;
	char *background_color, *color_spec;
	char *background_image, *combine_mode;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar));
	
	/* Connect the background changed signal to code that writes the color. */
	background = nautilus_get_widget_background (GTK_WIDGET (sidebar));
	if (!sidebar->details->background_connected) {
		sidebar->details->background_connected = TRUE;
		gtk_signal_connect (GTK_OBJECT (background),
				    "settings_changed",
				    background_settings_changed_callback,
				    sidebar);
		gtk_signal_connect (GTK_OBJECT (background),
				    "appearance_changed",
				    background_appearance_changed_callback,
				    sidebar);
		gtk_signal_connect (GTK_OBJECT (background),
				    "reset",
				    background_reset_callback,
				    sidebar);
	}
	
	/* Set up the background color and image from the metadata. */
	background_image = NULL;
	background_color = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
							    NULL);
	if (background_color == NULL) {
		background_color = g_strdup (sidebar->details->default_background_color);
		background_image = nautilus_directory_get_metadata (sidebar->details->directory,
							    NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
							    sidebar->details->default_background_image);
	}	
		
	/* disable the settings_changed callback, so the background doesn't get
	   written out, since it might be the theme-dependent default */
	gtk_signal_handler_block_by_func (GTK_OBJECT (background),
					  background_settings_changed_callback,
					  sidebar);
	
	nautilus_background_set_color (background, background_color);	
	g_free (background_color);
	
	nautilus_background_set_tile_image_uri (background, background_image);
	g_free (background_image);

	combine_mode = nautilus_theme_get_theme_data ("sidebar", "COMBINE");
	nautilus_background_set_combine_mode (background, combine_mode != NULL);
	g_free (combine_mode);
	
	/* set up the color for the tabs */
	color_spec = nautilus_directory_get_metadata (sidebar->details->directory,
						      NAUTILUS_METADATA_KEY_SIDEBAR_TAB_COLOR,
						      DEFAULT_TAB_COLOR);
	nautilus_sidebar_tabs_set_color(sidebar->details->sidebar_tabs, color_spec);
	g_free (color_spec);

	color_spec = nautilus_directory_get_metadata (sidebar->details->directory,
						      NAUTILUS_METADATA_KEY_SIDEBAR_TITLE_TAB_COLOR,
						      DEFAULT_TAB_COLOR);
	nautilus_sidebar_tabs_set_color(sidebar->details->title_tab, color_spec);
	g_free (color_spec);

	/* re-enable the background_changed signal */
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (background),
					    background_settings_changed_callback,
					    sidebar);
}

/* here is the key routine that populates the sidebar with the appropriate information when the uri changes */

void
nautilus_sidebar_set_uri (NautilusSidebar *sidebar, 
			      const char* new_uri,
			      const char* initial_title)
{       
	NautilusDirectory *directory;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR (sidebar));
	g_return_if_fail (new_uri != NULL);
	g_return_if_fail (initial_title != NULL);

	/* there's nothing to do if the uri is the same as the current one */ 
	if (nautilus_strcmp (sidebar->details->uri, new_uri) == 0) {
		return;
	}
	
	g_free (sidebar->details->uri);
	sidebar->details->uri = g_strdup (new_uri);
		
	directory = nautilus_directory_get (sidebar->details->uri);
	nautilus_directory_unref (sidebar->details->directory);
	sidebar->details->directory = directory;
		
	nautilus_sidebar_update_appearance (sidebar);

	/* tell the title widget about it */
	nautilus_sidebar_title_set_uri (sidebar->details->title,
					sidebar->details->uri,
					initial_title);
	
	/* set up the command buttons */
	nautilus_sidebar_update_buttons (sidebar);
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
