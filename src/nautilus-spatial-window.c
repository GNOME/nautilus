/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *  	     John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-window.c: Implementation of the main window object */

#include <config.h>
#include "nautilus-spatial-window.h"
#include "nautilus-window-private.h"

#include "nautilus-application.h"
#include "nautilus-desktop-window.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-location-dialog.h"
#include "nautilus-main.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-zoom-control.h"
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-generous-bin.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-drag-window.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-horizontal-splitter.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-sidebar-functions.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-undo.h>
#include <math.h>
#include <sys/time.h>

#define MAX_TITLE_LENGTH 180

struct _NautilusSpatialWindowDetails {        
	char *last_geometry;	
        guint save_geometry_timeout_id;	  
	
	GtkWidget *content_box;
};

GNOME_CLASS_BOILERPLATE (NautilusSpatialWindow, nautilus_spatial_window,
			 NautilusWindow, NAUTILUS_TYPE_WINDOW)

static gboolean
save_window_geometry_timeout (gpointer callback_data)
{
	NautilusSpatialWindow *window;
	
	window = NAUTILUS_SPATIAL_WINDOW (callback_data);
	
	nautilus_spatial_window_save_geometry (window);

	window->details->save_geometry_timeout_id = 0;

	return FALSE;
}

static gboolean
nautilus_spatial_window_configure_event (GtkWidget *widget,
					GdkEventConfigure *event)
{
	NautilusSpatialWindow *window;
	char *geometry_string;
	
	window = NAUTILUS_SPATIAL_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->configure_event (widget, event);
	
	/* Only save the geometry if the user hasn't resized the window
	 * for half a second. Otherwise delay the callback another half second.
	 */
	if (window->details->save_geometry_timeout_id != 0) {
		g_source_remove (window->details->save_geometry_timeout_id);
	}
	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (window)) && !NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
	
		/* If the last geometry is NULL the window must have just
		 * been shown. No need to save geometry to disk since it
		 * must be the same.
		 */
		if (window->details->last_geometry == NULL) {
			window->details->last_geometry = geometry_string;
			return FALSE;
		}
	
		/* Don't save geometry if it's the same as before. */
		if (!strcmp (window->details->last_geometry, 
			     geometry_string)) {
			g_free (geometry_string);
			return FALSE;
		}

		g_free (window->details->last_geometry);
		window->details->last_geometry = geometry_string;

		window->details->save_geometry_timeout_id = 
			g_timeout_add (500, save_window_geometry_timeout, window);
	}
	
	return FALSE;
}

static void
nautilus_spatial_window_unrealize (GtkWidget *widget)
{
	NautilusSpatialWindow *window;
	
	window = NAUTILUS_SPATIAL_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

	if (window->details->save_geometry_timeout_id != 0) {
		g_source_remove (window->details->save_geometry_timeout_id);
		window->details->save_geometry_timeout_id = 0;
		nautilus_spatial_window_save_geometry (window);
	}
}

static void
nautilus_spatial_window_destroy (GtkObject *object)
{
	NautilusSpatialWindow *window;

	window = NAUTILUS_SPATIAL_WINDOW (object);

	window->details->content_box = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
nautilus_spatial_window_finalize (GObject *object)
{
	NautilusSpatialWindow *window;
	
	window = NAUTILUS_SPATIAL_WINDOW (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
nautilus_spatial_window_save_geometry (NautilusSpatialWindow *window)
{
	char *geometry_string;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (GTK_WIDGET(window)->window &&
	    !(gdk_window_get_state (GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_MAXIMIZED)) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
		
		nautilus_file_set_metadata (NAUTILUS_WINDOW (window)->details->viewed_file,
					    NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY,
					    NULL,
					    geometry_string);
		
		g_free (geometry_string);
	}
}

void
nautilus_spatial_window_save_scroll_position (NautilusSpatialWindow *window)
{
	NautilusWindow *parent;
	char *scroll_string;

	parent = NAUTILUS_WINDOW(window);
	scroll_string = nautilus_view_frame_get_first_visible_file (parent->content_view);
	nautilus_file_set_metadata (parent->details->viewed_file,
				    NAUTILUS_METADATA_KEY_WINDOW_SCROLL_POSITION,
				    NULL,
				    scroll_string);
	g_free (scroll_string);
}

static void
nautilus_spatial_window_show (GtkWidget *widget)
{	
	NautilusSpatialWindow *window;

	window = NAUTILUS_SPATIAL_WINDOW (widget);

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
file_menu_close_parent_windows_callback (BonoboUIComponent *component, 
					      gpointer user_data, 
					      const char *verb)
{
	nautilus_application_close_parent_windows (NAUTILUS_SPATIAL_WINDOW (user_data));
}

static void
real_prompt_for_location (NautilusWindow *window)
{
	GtkWidget *dialog;
	
	dialog = nautilus_location_dialog_new (window);
	gtk_widget_show (dialog);
}

static void
real_set_title (NautilusWindow *window, const char *title)
{

	if (title[0] == '\0') {
		gtk_window_set_title (GTK_WINDOW (window), _("Nautilus"));
	} else {
		char *window_title;

		window_title = eel_str_middle_truncate (title, MAX_TITLE_LENGTH);
		gtk_window_set_title (GTK_WINDOW (window), window_title);
		g_free (window_title);
	}
}

static void
real_merge_menus (NautilusWindow *nautilus_window)
{
	NautilusSpatialWindow *window;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Close Parent Folders", file_menu_close_parent_windows_callback),
		BONOBO_UI_VERB_END
	};
	
	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS,
			 merge_menus, (nautilus_window));

	window = NAUTILUS_SPATIAL_WINDOW (nautilus_window);

	bonobo_ui_util_set_ui (NAUTILUS_WINDOW (window)->details->shell_ui,
			       DATADIR,
			       "nautilus-spatial-window-ui.xml",
			       "nautilus", NULL);

	bonobo_ui_component_add_verb_list_with_data (nautilus_window->details->shell_ui,
						     verbs, window);
}

static void
real_set_content_view_widget (NautilusWindow *window,
			      NautilusViewFrame *new_view)
{
	EEL_CALL_PARENT (NAUTILUS_WINDOW_CLASS, set_content_view_widget,
			 (window, new_view));
	
	gtk_container_add (GTK_CONTAINER (NAUTILUS_SPATIAL_WINDOW (window)->details->content_box),
			   GTK_WIDGET (new_view));
}

static gboolean
real_delete_event (GtkWidget *window, GdkEventAny *event)
{
	nautilus_spatial_window_save_scroll_position (NAUTILUS_SPATIAL_WINDOW (window));

	return FALSE;
}

static void 
real_get_default_size(NautilusWindow *window, guint *default_width, guint *default_height)
{
   if(default_width) {
      *default_width = NAUTILUS_SPATIAL_WINDOW_DEFAULT_WIDTH;
   }
   if(default_height) {
      *default_height = NAUTILUS_SPATIAL_WINDOW_DEFAULT_HEIGHT;	
   }
}

static void
nautilus_spatial_window_instance_init (NautilusSpatialWindow *window)
{
	window->details = g_new0 (NautilusSpatialWindowDetails, 1);
	window->affect_spatial_window_on_next_location_change = TRUE;

	window->details->content_box = 
		gtk_widget_new (EEL_TYPE_GENEROUS_BIN, NULL);
	gtk_widget_show (window->details->content_box);
	bonobo_window_set_contents (BONOBO_WINDOW (window), 
				    window->details->content_box);
}

static void
nautilus_spatial_window_class_init (NautilusSpatialWindowClass *class)
{
	NAUTILUS_WINDOW_CLASS (class)->window_type = Nautilus_WINDOW_SPATIAL;

	G_OBJECT_CLASS (class)->finalize = nautilus_spatial_window_finalize;
	GTK_OBJECT_CLASS (class)->destroy = nautilus_spatial_window_destroy;
	GTK_WIDGET_CLASS (class)->show = nautilus_spatial_window_show;
	GTK_WIDGET_CLASS (class)->configure_event = nautilus_spatial_window_configure_event;
	GTK_WIDGET_CLASS (class)->unrealize = nautilus_spatial_window_unrealize;

	NAUTILUS_WINDOW_CLASS (class)->prompt_for_location = 
		real_prompt_for_location;
	NAUTILUS_WINDOW_CLASS (class)->set_title = 
		real_set_title;
	NAUTILUS_WINDOW_CLASS (class)->merge_menus = 
		real_merge_menus;
	NAUTILUS_WINDOW_CLASS (class)->set_content_view_widget = 
		real_set_content_view_widget;
	GTK_WIDGET_CLASS (class)->delete_event =
		real_delete_event;
	NAUTILUS_WINDOW_CLASS(class)->get_default_size = real_get_default_size;
}
