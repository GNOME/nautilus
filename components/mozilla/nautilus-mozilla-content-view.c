/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Ramiro Estrugo <ramiro@eazel.com>
 *  	     Mike Fleming <mfleming@eazel.com>
 *
 */

/*
 * nautilus-mozilla-content-view.c - Mozilla content view component.
 *
 * This component uses the mozilla gecko layout engine via the gtk_moz_embed
 * widget to display and munge html.
 */

#include <config.h>
#include "nautilus-mozilla-content-view.h"

#include "bonobo-extensions.h"
#include "gtkmozembed.h"
#include "mozilla-components.h"
#include "mozilla-events.h"
#include "mozilla-preferences.h"
#include "nautilus-mozilla-embed-extensions.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>

#define nopeDEBUG_ramiro 1
#define nopeDEBUG_mfleming 1
#define nopeDEBUG_pepper 1

#ifdef DEBUG_mfleming
#define DEBUG_MSG(x)	g_print x
#else
#define DEBUG_MSG(x)
#endif

/* Code-copied from nsGUIEvent.h */

enum nsEventStatus {  
	/* The event is ignored, do default processing */
 	nsEventStatus_eIgnore,            
 	/* The event is consumed, don't do default processing */
 	nsEventStatus_eConsumeNoDefault, 
 	/* The event is consumed, but do default processing */
	nsEventStatus_eConsumeDoDefault  
};
 
#define NS_DOM_EVENT_IGNORED ((enum nsEventStatus)nsEventStatus_eIgnore)
#define NS_DOM_EVENT_CONSUMED ((enum nsEventStatus)nsEventStatus_eConsumeNoDefault)

/* Buffer for streaming contents from gnome-vfs into mozilla */
#define VFS_READ_BUFFER_SIZE	(40 * 1024)

/* Menu Path for charset encoding submenu */
#define MENU_VIEW_CHARSET_ENCODING_PATH "/menu/View/Encoding"

/* property bag properties */
enum {
	ICON_NAME,
	COMPONENT_INFO
};


struct NautilusMozillaContentViewDetails {
	char 		*uri;			/* The URI stored here is nautilus's idea of the URI */
	GtkMozEmbed 	*mozilla;		/* If this is NULL, the mozilla widget has not yet been initialized */ 
	NautilusView 	*nautilus_view;
	BonoboPropertyBag *property_bag;
	
	GdkCursor 	*busy_cursor;
	char		*vfs_read_buffer;
	GnomeVFSAsyncHandle *vfs_handle;
						/* set to TRUE during the DOM callbacks
						 * To work around bug 6580, non-user initiated navigations
						 * are not recorded in history
						 */
	gboolean	user_initiated_navigation;

	BonoboUIComponent *ui;

	GSList            *chrome_list;

};

typedef struct NautilusMozillaContentViewChrome {
	GtkWidget                   *toplevel_window;
	GtkMozEmbed                 *mozilla;
	NautilusMozillaContentView  *view;
} NautilusMozillaContentViewChrome;

/* GTK Type System */
static void     nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass);
static void     nautilus_mozilla_content_view_initialize       (NautilusMozillaContentView      *view);
static void     nautilus_mozilla_content_view_destroy          (GtkObject                       *object);


/* Gnome VFS callback functions */
static void	vfs_open_callback				(GnomeVFSAsyncHandle 		*handle,
								 GnomeVFSResult			result,
								 gpointer			data);

static void	vfs_read_callback				(GnomeVFSAsyncHandle		*handle,
								 GnomeVFSResult			result,
								 gpointer			buffer,
								 GnomeVFSFileSize		bytes_requested,
								 GnomeVFSFileSize		bytes_read,
								 gpointer			data);

/* NautilusView callback functions */
static void	view_load_location_callback			(NautilusView 			*nautilus_view,
								 const char 			*location,
								 gpointer 			data);

/* GtkEmbedMoz callback functions */

static void	mozilla_realize_callback			(GtkWidget 			*mozilla,
								 gpointer			user_data);

static void	mozilla_title_changed_callback			(GtkMozEmbed 			*mozilla,
								 gpointer			user_data);

static void	mozilla_location_callback 			(GtkMozEmbed			*mozilla,
								 gpointer			user_data);

static void	mozilla_net_state_callback 			(GtkMozEmbed			*mozilla,
								 gint				state_flags,
								 guint				status_flags,
								 gpointer			user_data);

static void	mozilla_net_start_callback			(GtkMozEmbed			*mozilla,
								 gpointer			user_data);

static void	mozilla_net_stop_callback			(GtkMozEmbed			*mozilla,
								 gpointer			user_data);

static void	mozilla_link_message_callback			(GtkMozEmbed			*mozilla,
								 gpointer			user_data);

static void	mozilla_progress_callback			(GtkMozEmbed			*mozilla,
								 gint				current_progress,
								 gint				max_progress,
								 gpointer			user_data);

static gint	mozilla_dom_key_press_callback			(GtkMozEmbed                    *mozilla,
								 gpointer                       dom_event,
								 gpointer                       user_data);

static gint	mozilla_dom_mouse_click_callback		(GtkMozEmbed			*mozilla,
								 gpointer			dom_event,
								 gpointer			user_data);

static void	mozilla_new_window_callback			(GtkMozEmbed			*mozilla,
								 GtkMozEmbed                    **new_mozilla,
								 guint                          chromemask,
								 NautilusMozillaContentView     *view);

/* Chrome callback functions */

static void     mozilla_chrome_visibility_callback             (GtkMozEmbed                      *mozilla,
								gboolean                         visibility,
								NautilusMozillaContentViewChrome *chrome);

static void     mozilla_chrome_destroy_brsr_callback           (GtkMozEmbed                      *mozilla,
								NautilusMozillaContentViewChrome *chrome);

static void     mozilla_chrome_size_to_callback                (GtkMozEmbed                      *mozilla,
								gint                              width,
								gint                              height,
								NautilusMozillaContentViewChrome *chrome);

static void     mozilla_chrome_title_callback                  (GtkMozEmbed                      *mozilla,
								NautilusMozillaContentViewChrome *chrome);

/* Private NautilusMozillaContentView functions */ 

#ifdef BUSY_CURSOR
static void     set_busy_cursor           			(NautilusMozillaContentView     *view);
static void     clear_busy_cursor				(NautilusMozillaContentView     *view);
#endif

static void	navigate_mozilla_to_nautilus_uri		(NautilusMozillaContentView     *view,
								 const char			*uri);

static void	update_nautilus_uri				(NautilusMozillaContentView	*view,
								 const char			*nautilus_uri);

static void	cancel_pending_vfs_operation			(NautilusMozillaContentView	*view);

/* Utility functions */

static gboolean	is_uri_relative					(const char 			*uri);

static char *	make_full_uri_from_relative			(const char			*base_uri,
								 const char			*uri);

static gboolean uris_identical					(const char			*uri1,
								 const char			*uri2);

static gboolean should_uri_navigate_bypass_nautilus		(const char			*uri);

static gboolean	should_mozilla_load_uri_directly 		(const char			*uri);

#define STRING_LIST_NOT_FOUND -1
static gint	string_list_get_index_of_string 		(const char			*string_list[],
								 guint				num_strings,
								 const char 			*string);

static void	pre_widget_initialize				(void);
static void	post_widget_initialize				(void);

/* BonoboControl callbacks */
static void bonobo_control_activate_callback (BonoboObject *control, gboolean state, gpointer callback_data);

/***********************************************************************************/
/***********************************************************************************/

EEL_DEFINE_CLASS_BOILERPLATE (NautilusMozillaContentView,
			      nautilus_mozilla_content_view,
			      GTK_TYPE_VBOX);

static void
nautilus_mozilla_content_view_initialize_class (NautilusMozillaContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_mozilla_content_view_destroy;

	pre_widget_initialize ();
}


/* property bag property access routines to return sidebar icon */
static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
	NautilusMozillaContentView *content_view;
	
	content_view = (NautilusMozillaContentView*) callback_data;

	switch (arg_id) {
        	case ICON_NAME:	
			if (eel_istr_has_prefix (content_view->details->uri, "man:")) {
                   		BONOBO_ARG_SET_STRING (arg, "manual");					
			} else if (eel_istr_has_prefix (content_view->details->uri, "http:")) {
                		BONOBO_ARG_SET_STRING (arg, "i-web");					
			} else {
                		BONOBO_ARG_SET_STRING (arg, "");					
                	}
                	break;

        	case COMPONENT_INFO:
               		BONOBO_ARG_SET_STRING (arg, "");					
                 	break;
        		
        	default:
                	g_warning ("Unhandled arg %d", arg_id);
                	break;
	}
}

/* there are no settable properties, so complain if someone tries to set one */
static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
                g_warning ("Bad Property set on mozilla view: property ID %d", arg_id);
}

static void
nautilus_mozilla_content_view_initialize (NautilusMozillaContentView *view)
{
	view->details = g_new0 (NautilusMozillaContentViewDetails, 1);

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	/* Conjure up the beast.  May God have mercy on our souls. */
	view->details->mozilla = GTK_MOZ_EMBED (gtk_moz_embed_new ());

	/* Do preference/environment setup that needs to happen only once.
	 * We need to do this right after the first gtkmozembed widget gets
	 * created, otherwise the mozilla runtime environment is not properly
	 * setup.
	 */
	post_widget_initialize ();

	/* Add callbacks to the beast */
	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"realize",
				GTK_SIGNAL_FUNC (mozilla_realize_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"title",
				GTK_SIGNAL_FUNC (mozilla_title_changed_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"location",
				GTK_SIGNAL_FUNC (mozilla_location_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"net_state",
				GTK_SIGNAL_FUNC (mozilla_net_state_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"net_start",
				GTK_SIGNAL_FUNC (mozilla_net_start_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"net_stop",
				GTK_SIGNAL_FUNC (mozilla_net_stop_callback),
				view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"link_message",
				GTK_SIGNAL_FUNC (mozilla_link_message_callback),
				view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"progress",
				GTK_SIGNAL_FUNC (mozilla_progress_callback),
				view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"dom_key_press",
				GTK_SIGNAL_FUNC (mozilla_dom_key_press_callback),
				view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"dom_mouse_click",
				GTK_SIGNAL_FUNC (mozilla_dom_mouse_click_callback),
				view);

	gtk_signal_connect (GTK_OBJECT (view->details->mozilla), 
				"new_window",
				GTK_SIGNAL_FUNC (mozilla_new_window_callback),
				view);

	
	gtk_box_pack_start (GTK_BOX (view), GTK_WIDGET (view->details->mozilla), TRUE, TRUE, 1);
	
	gtk_widget_show (GTK_WIDGET (view->details->mozilla));
	
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
				"load_location",
				GTK_SIGNAL_FUNC (view_load_location_callback), 
				view);

	/* Connect to the active signal of the view to merge our menus */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control (view->details->nautilus_view)),
                            "activate",
                            bonobo_control_activate_callback,
                            view);

 	/* allocate a property bag to specify the name of the icon for this component */
	view->details->property_bag = bonobo_property_bag_new (get_bonobo_properties,  set_bonobo_properties, view);
	bonobo_control_set_properties (nautilus_view_get_bonobo_control (view->details->nautilus_view), view->details->property_bag);
	bonobo_property_bag_add (view->details->property_bag, "icon_name", ICON_NAME, BONOBO_ARG_STRING, NULL,
				 _("name of icon for the mozilla view"), 0);
	bonobo_property_bag_add (view->details->property_bag, "summary_info", COMPONENT_INFO, BONOBO_ARG_STRING, NULL,
				 _("mozilla summary info"), 0);

	gtk_widget_show_all (GTK_WIDGET (view));

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void
nautilus_mozilla_content_view_destroy (GtkObject *object)
{
	NautilusMozillaContentView *view;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (object);

	g_free (view->details->uri);
	view->details->uri = NULL;

	if (view->details->busy_cursor != NULL) {
		gdk_cursor_destroy (view->details->busy_cursor);
		view->details->busy_cursor = NULL;
	}

	cancel_pending_vfs_operation (view);

	/* free the property bag */
	if (view->details->property_bag != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (view->details->property_bag));
	}

	/* make sure to destroy any pending dialogs */
	while (view->details->chrome_list) {
		NautilusMozillaContentViewChrome *chrome;
		GSList *tmp_list;

		/* save the list and advance to the next element */
		tmp_list = view->details->chrome_list;
		view->details->chrome_list = view->details->chrome_list->next;

		/* get the chrome and destroy it */
		chrome = (NautilusMozillaContentViewChrome *)tmp_list->data;
		gtk_widget_destroy (chrome->toplevel_window);

		/* and free everything */
		g_free (tmp_list->data);
		g_slist_free (tmp_list);
	}

	g_free (view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
	
	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

/**
 * nautilus_mozilla_content_view_get_bonobo_object:
 *
 * Return the BonoboObject associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusMozillaContentView to get the nautilus_view from..
 * 
 **/
BonoboObject *
nautilus_mozilla_content_view_new (void)
{
	NautilusMozillaContentView *view;
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW, NULL));

	return BONOBO_OBJECT (view->details->nautilus_view);
}


/***********************************************************************************/
/***********************************************************************************/

/*
 * For URI's that use the (not-recommended) GnomeVFSTransform mechanism,
 * such as gnome-help:
 * 
 * Returns NULL if the uri is already a file: URI or it doesn't transform
 * into one.
 * 
 */

static char *
try_transform_nautilus_uri_to_file_scheme (const char *uri)
{
	GnomeVFSURI *vfs_uri;
	char *real_uri;

 	if (strncasecmp ("file:///", uri, strlen ("file:///")) == 0) {
 		return NULL;
	} 

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri != NULL) {
		real_uri = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_NONE);
		gnome_vfs_uri_unref (vfs_uri);
		
		if ( 0 == strncmp ("file:///", real_uri, strlen ("file:///"))) {
			return real_uri;
		}
		g_free (real_uri);
	}

	return NULL;
}

static void
view_load_location_callback (NautilusView *nautilus_view,
			     const char *location,
			     gpointer data)
{
	NautilusMozillaContentView *view;
	char *file_uri;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (data);

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	/* set to FALSE here so any URI reported in mozilla_location_callback
	 * will not be sent back to nautilus
	 */
	view->details->user_initiated_navigation = FALSE;

	file_uri = try_transform_nautilus_uri_to_file_scheme (location);

	if (file_uri != NULL) {
		/* if this is a gnome-help: uri, transform it into a file: uri
		 * and load again to get around the fact that the help translater doesn't
		 * know how to deal with paths such as "gnome-help:control-center/foo"
		 */
		DEBUG_MSG ((">nautilus_view_report_redirect (%s,%s)\n", location, file_uri));
		nautilus_view_report_redirect (view->details->nautilus_view, location, file_uri, NULL, file_uri);
		navigate_mozilla_to_nautilus_uri (view, file_uri);
		g_free (file_uri);
	} else {
		DEBUG_MSG ((">nautilus_view_report_load_underway\n"));		
		nautilus_view_report_load_underway (nautilus_view);
		navigate_mozilla_to_nautilus_uri (view, location);
	}

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

/***********************************************************************************/
/***********************************************************************************/

typedef struct
{
	char *encoding;
	NautilusMozillaContentView *mozilla_view;
} EncodingMenuData;

static void
charset_encoding_changed_callback (BonoboUIComponent *component,
				   gpointer callback_data,
				   const char *path)
{
	EncodingMenuData *data;

	g_return_if_fail (callback_data != NULL);
	data = callback_data;
	g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (data->mozilla_view));

	/* Change the encoding and reload the page */
	mozilla_charset_set_encoding (data->mozilla_view->details->mozilla,
				      data->encoding);

	gtk_moz_embed_reload (data->mozilla_view->details->mozilla, GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
}

static void
encoding_menu_data_free_cover (gpointer callback_data)
{
	EncodingMenuData *data;
	g_return_if_fail (callback_data != NULL);

	data = callback_data;
	g_free (data->encoding);
}

static void
mozilla_view_create_charset_encoding_submenu (NautilusMozillaContentView *mozilla_view)
{
 	GList *node = NULL;
 	GList *groups = NULL;
	guint num_encodings;
 	guint i;
 	guint menu_index = 0;
	const GtkMozEmbed *mozilla;
	
	g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (mozilla_view));
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (mozilla_view->details->ui));
	g_return_if_fail (GTK_IS_MOZ_EMBED (mozilla_view->details->mozilla));

	mozilla = mozilla_view->details->mozilla;
	
	num_encodings = mozilla_charset_get_num_encodings (mozilla);

	/* Collect the list of encoding groups */
	for (i = 0; i < num_encodings; i++) {
		char *encoding_title;
		char *encoding_group;
		
		encoding_title = mozilla_charset_get_nth_encoding_title (mozilla, i);
		encoding_group = mozilla_charset_find_encoding_group (mozilla,
								      encoding_title);
		
		/* Add new encodings to the list only once */
		if (encoding_group != NULL) {
			if (!g_list_find_custom	(groups, encoding_group, (GCompareFunc) strcoll)) {
				groups = g_list_prepend (groups, encoding_group);
			}
		}
		
		g_free (encoding_title);
	}
	groups = g_list_reverse (groups);
	
	/* Create the encoding group submenus */
	node = groups;
	while (node) {
		char *translated_encoding_group;
		char *encoding_group;
		char *escaped_encoding_group;
		char *path;
		g_assert (node->data != NULL);
		encoding_group = node->data;

		translated_encoding_group = mozilla_charset_encoding_group_get_translated (mozilla,
											   encoding_group);
		if (translated_encoding_group == NULL) {
			translated_encoding_group = g_strdup (encoding_group);
		}
		escaped_encoding_group = bonobo_ui_util_encode_str (encoding_group);
		
		/* HACK: From now onwards we use the groups list to store indeces of items */
		node->data = GINT_TO_POINTER (0);
		
		nautilus_bonobo_add_submenu (mozilla_view->details->ui,
					     MENU_VIEW_CHARSET_ENCODING_PATH,
					     escaped_encoding_group);
		path = g_strdup_printf ("%s/%s", MENU_VIEW_CHARSET_ENCODING_PATH, escaped_encoding_group);
		nautilus_bonobo_set_label (mozilla_view->details->ui,
					   path,
					   translated_encoding_group);
		g_free (path);

		node = node->next;
		g_free (translated_encoding_group);
		g_free (escaped_encoding_group);
		g_free (encoding_group);
	}
	
	/* We start adding items to the root submenu right after the last encoding group submenu */
	menu_index = g_list_length (groups);
	
	/* Add the encoding menu items.  Encodings that belong in groups
	 * get added the submenus we created above.  Encodings without a group
	 * get added the root encodings menu.
	 */
	for (i = 0; i < num_encodings; i++) {
		char *encoding;
		char *encoding_title;
		char *translated_encoding_title;
		char *encoding_group;
		char *ui_path;
		char *verb_name;
		EncodingMenuData *data;
		char *new_item_path;
		guint new_item_index;
		
		encoding = mozilla_charset_get_nth_encoding (mozilla, i);
		encoding_title = mozilla_charset_get_nth_encoding_title (mozilla, i);
		encoding_group = mozilla_charset_find_encoding_group (mozilla,
								      encoding_title);
		translated_encoding_title = mozilla_charset_get_nth_translated_encoding_title (mozilla, i);

		/* Add item to an existing encoding group submenu */
		if (encoding_group != NULL) {
			int enconding_group_index;
			GList *nth_node;
			char *escaped_encoding_group;

			enconding_group_index = mozilla_charset_get_encoding_group_index (mozilla,
											  encoding_group);
			g_assert (enconding_group_index >= 0);
			g_assert ((guint) enconding_group_index < g_list_length (groups));
			
			nth_node = g_list_nth (groups, enconding_group_index);
			g_assert (nth_node != NULL);

			
			escaped_encoding_group = bonobo_ui_util_encode_str (encoding_group);
			
			new_item_path = g_strdup_printf ("%s/%s", MENU_VIEW_CHARSET_ENCODING_PATH,
							 escaped_encoding_group);
			new_item_index = GPOINTER_TO_INT (nth_node->data);

			g_free (escaped_encoding_group);
			
			/* Bump the child index */
			nth_node->data = GINT_TO_POINTER (new_item_index + 1);
		/* Add item to root encoding submenu */
		} else {
			new_item_path = g_strdup (MENU_VIEW_CHARSET_ENCODING_PATH);
			new_item_index = menu_index;
			menu_index++;
		}
		
		nautilus_bonobo_add_numbered_menu_item (mozilla_view->details->ui, 
							new_item_path, 
							new_item_index,
							translated_encoding_title,
							NULL);
		
		/* Add the status tip */
		ui_path = nautilus_bonobo_get_numbered_menu_item_path (mozilla_view->details->ui,
								       new_item_path,
								       new_item_index);
		/* NOTE: The encoding_title comes to us already localized */
		nautilus_bonobo_set_tip (mozilla_view->details->ui,
					 ui_path,
					 translated_encoding_title);
		g_free (ui_path);
		
		/* Add verb to new bookmark menu item */
		verb_name = nautilus_bonobo_get_numbered_menu_item_command  (mozilla_view->details->ui,
									     new_item_path,
									     new_item_index);
		
		data = g_new0 (EncodingMenuData, 1);
		data->encoding = g_strdup (encoding);
		data->mozilla_view = mozilla_view;
		
		bonobo_ui_component_add_verb_full (mozilla_view->details->ui,
						   verb_name, 
						   charset_encoding_changed_callback,
						   data,
						   encoding_menu_data_free_cover);
		g_free (new_item_path);
		g_free (verb_name);
		
		g_free (encoding);
		g_free (encoding_title);
		g_free (translated_encoding_title);
		g_free (encoding_group);
	}
	
	g_list_free (groups);
}

static void
mozilla_view_merge_menus (NautilusMozillaContentView *mozilla_view)
{
	g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (mozilla_view));

	/* This BonoboUIComponent is made automatically, and its lifetime is
	 * controlled automatically. We don't need to explicitly ref or unref it.
	 */
	mozilla_view->details->ui = nautilus_view_set_up_ui (mozilla_view->details->nautilus_view,
							     DATADIR,
							     "nautilus-mozilla-ui.xml",
							     "nautilus-mozilla-view");
	
	/* Create the charset encodings submenu */
	bonobo_ui_component_freeze (mozilla_view->details->ui, NULL);
	mozilla_view_create_charset_encoding_submenu (mozilla_view);
	bonobo_ui_component_thaw (mozilla_view->details->ui, NULL);
}

/* BonoboControl callbacks */
static void
bonobo_control_activate_callback (BonoboObject *control, gboolean state, gpointer callback_data)
{
        NautilusMozillaContentView *mozilla_view;

	g_return_if_fail (BONOBO_IS_CONTROL (control));
	g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (callback_data));
	
        mozilla_view = NAUTILUS_MOZILLA_CONTENT_VIEW (callback_data);

	if (state) {
		mozilla_view_merge_menus (mozilla_view);
	}

        /* 
         * Nothing to do on deactivate case, which never happens because
         * of the way Nautilus content views are handled.
         */
}

/***********************************************************************************/
/***********************************************************************************/

static void
mozilla_realize_callback (GtkWidget *mozilla, gpointer user_data)
{
	NautilusMozillaContentView	*view;
	char *uri;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

	if (view->details->uri != NULL) {
		DEBUG_MSG (("=%s navigating to uri after realize '%s'\n", __FUNCTION__, view->details->uri));

		uri = g_strdup (view->details->uri);
		navigate_mozilla_to_nautilus_uri (view, uri);
		g_free (uri);
	}

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void
mozilla_title_changed_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_title;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

	new_title = mozilla_get_document_title (view->details->mozilla);

	DEBUG_MSG (("=%s : new title='%s'\n", __FUNCTION__, new_title));

	if (new_title != NULL && strcmp (new_title, "") != 0) {
		DEBUG_MSG ((">nautilus_view_set_title '%s'\n", new_title));
		nautilus_view_set_title (view->details->nautilus_view, new_title);
	}
	
	g_free (new_title);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void
mozilla_new_window_callback (GtkMozEmbed *mozilla, GtkMozEmbed **new_mozilla, guint chromemask, NautilusMozillaContentView *view)
{
	static GnomeDialog *dialog;
	NautilusMozillaContentViewChrome *chrome;

	/* it's a chrome window so just create a simple shell to play with. */
	if (chromemask & GTK_MOZ_EMBED_FLAG_OPENASCHROME) {

		chrome = g_new0 (NautilusMozillaContentViewChrome, 1);
		if (!chrome) {
			return;
		}

		/* save this in this view's chrome list */
		view->details->chrome_list = g_slist_append (view->details->chrome_list, chrome);

		chrome->view = view;

		chrome->toplevel_window = gtk_window_new (GTK_WINDOW_DIALOG);
		chrome->mozilla = GTK_MOZ_EMBED (gtk_moz_embed_new());

		gtk_container_add (GTK_CONTAINER (chrome->toplevel_window), GTK_WIDGET (chrome->mozilla));

		/* set up all the signals that we care about for chrome windows. */
		gtk_signal_connect (GTK_OBJECT (chrome->mozilla), "visibility",
				    GTK_SIGNAL_FUNC (mozilla_chrome_visibility_callback),
				    chrome);
		gtk_signal_connect (GTK_OBJECT (chrome->mozilla), "destroy_browser",
				    GTK_SIGNAL_FUNC (mozilla_chrome_destroy_brsr_callback),
				    chrome);
		gtk_signal_connect (GTK_OBJECT (chrome->mozilla), "size_to",
				    GTK_SIGNAL_FUNC (mozilla_chrome_size_to_callback),
				    chrome);
		gtk_signal_connect (GTK_OBJECT (chrome->mozilla), "title",
				    GTK_SIGNAL_FUNC (mozilla_chrome_title_callback),
				    chrome);

		/* and realize the widgets */
		gtk_widget_realize (chrome->toplevel_window);
		gtk_widget_realize (GTK_WIDGET(chrome->mozilla));

		/* save the new embed object */
		*new_mozilla = chrome->mozilla;
		return;
	}

	if (dialog == NULL) {
		dialog = eel_show_warning_dialog (_("A JavaScript function (small software program) on this page "
						    "tried to open a new window, but Nautilus does not support the "
						    "opening new windows by JavaScript.\n\n"
						    "Try viewing the page in a different web browser, such as Mozilla."),
						  _("Nautilus JavaScript Warning"),
						  NULL);
		eel_nullify_when_destroyed (&dialog);
	}
}

static void
mozilla_chrome_visibility_callback (GtkMozEmbed *mozilla, gboolean visibility, NautilusMozillaContentViewChrome *chrome)
{
	/* hide? */
	if (!visibility) {
		gtk_widget_hide (chrome->toplevel_window);
		return;
	}
	/* else show */
	gtk_widget_show (GTK_WIDGET(chrome->mozilla));
	gtk_widget_show (chrome->toplevel_window);
}

static void
mozilla_chrome_destroy_brsr_callback (GtkMozEmbed *mozilla, NautilusMozillaContentViewChrome *chrome)
{
	GSList *tmp_list;
	gtk_widget_destroy (chrome->toplevel_window);
	tmp_list = g_slist_find (chrome->view->details->chrome_list, chrome);
	chrome->view->details->chrome_list = g_slist_remove_link (chrome->view->details->chrome_list, tmp_list);
	g_free (tmp_list->data);
	g_slist_free (tmp_list);
}

static void
mozilla_chrome_size_to_callback (GtkMozEmbed *mozilla, gint width, gint height,	NautilusMozillaContentViewChrome *chrome)
{
	gtk_widget_set_usize (GTK_WIDGET (chrome->mozilla), width, height);
}

static void
mozilla_chrome_title_callback (GtkMozEmbed *mozilla, NautilusMozillaContentViewChrome *chrome)
{
	char *new_title;

	new_title = gtk_moz_embed_get_title (chrome->mozilla);

	if (new_title) {
		if (strcmp (new_title, "") != 0) {
			gtk_window_set_title (GTK_WINDOW (chrome->toplevel_window), new_title);
		}
		g_free (new_title);
	}
}

static void
mozilla_location_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*new_location;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

	/* If we were streaming in content, stop now */
	cancel_pending_vfs_operation (view);

	new_location = gtk_moz_embed_get_location (view->details->mozilla);

	DEBUG_MSG (("=%s : current='%s' new='%s'\n", __FUNCTION__, view->details->uri, new_location));

	/*
	 * FIXME bug 7114
	 * The user-initiated navigation check here is an attempt to be able
	 * to prevent redirects that were handled by Mozilla from showing
	 * up in the Nautilus history.
	 * 
	 * The typical flow is like this:
	 * 1) view_load_location_callback is called with uri eg "http://www.amazon.com"
	 * 2) Mozilla goes to that URI and recieves a 302 eg "http://www.amazon.com/crap"
	 * 3) the mozilla wrapper calls report_location_change on the new URI, which
	 *    is given a second entry in the history.
	 *    
	 * To work around this, we only call report_location_change when
	 * the navigation was initiated with a DOM event.
	 * 
	 * This means we don't record certain navigation events (eg,
	 * javascript-initiated) in the history.
	 */

	if (view->details->uri == NULL 
	    || (!uris_identical (new_location, view->details->uri))) {
		if (view->details->user_initiated_navigation) {
			update_nautilus_uri (view, new_location);
		} else {
			DEBUG_MSG (("=%s : Navigation not user initiated, reporting as redirect\n", __FUNCTION__));

			DEBUG_MSG ((">nautilus_view_report_redirect (%s,%s)\n", view->details->uri, new_location));

			nautilus_view_report_redirect (view->details->nautilus_view, 
				view->details->uri, new_location, NULL, new_location);

			g_free (view->details->uri);
			view->details->uri = g_strdup (new_location);
		}
	} else {
		DEBUG_MSG (("=%s : URI's identical, ignoring request\n", __FUNCTION__));
	}

	view->details->user_initiated_navigation = FALSE;

	g_free (new_location);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

#if defined(DEBUG_ramiro)

#define PRINT_FLAG(bits, mask, message)		\
G_STMT_START {					\
  if ((bits) & (mask)) {			\
	  g_print ("%s ", (message));		\
  }						\
} G_STMT_END

static void
debug_print_state_flags (gint state_flags)
{
	g_print ("state_flags = ");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_START, "start");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_REDIRECTING, "redirecting");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_TRANSFERRING, "transferring");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_NEGOTIATING, "negotiating");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_STOP, "stop");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_REQUEST, "is_request");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_DOCUMENT, "is_document");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_NETWORK, "is_network");
	PRINT_FLAG (state_flags, GTK_MOZ_EMBED_FLAG_IS_WINDOW, "is_window");
	g_print ("\n");
}

static void
debug_print_status_flags (guint status_flags)
{
	g_print ("status_flags = ");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_DNS, "failed_dns");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_CONNECT, "failed_connect");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT, "failed_timeout");
	PRINT_FLAG (status_flags, GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED, "failed_usercanceled");
	g_print ("\n");
}
#endif

static void
mozilla_net_state_callback (GtkMozEmbed	*mozilla,
			    gint	state_flags,
			    guint	status_flags,
			    gpointer	user_data)
{
 	NautilusMozillaContentView	*view;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

#if defined(DEBUG_ramiro)
	g_print ("%s\n", __FUNCTION__);
	debug_print_state_flags (state_flags);
	debug_print_status_flags (status_flags);
	g_print ("\n\n");
#endif

/* FIXME: Busy cursor code removed because the cursor can stay
 * up on man: pages
 */
#ifdef BUSY_CURSOR
	/* win_start */
	if (state_flags & GTK_MOZ_EMBED_FLAG_START) {
		set_busy_cursor (view);
	}

	/* win_stop */
	if (state_flags & GTK_MOZ_EMBED_FLAG_STOP) {
		clear_busy_cursor (view);
	}
#endif

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void
mozilla_net_start_callback (GtkMozEmbed 	*mozilla,
			    gpointer		user_data)
{
 	NautilusMozillaContentView	*view;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == GTK_MOZ_EMBED (view->details->mozilla));

	DEBUG_MSG ((">nautilus_view_report_load_underway\n"));
	nautilus_view_report_load_underway (view->details->nautilus_view);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void
mozilla_net_stop_callback (GtkMozEmbed 	*mozilla,
			   gpointer	user_data)
{
 	NautilusMozillaContentView	*view;

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

	DEBUG_MSG ((">nautilus_view_report_load_complete\n"));
	nautilus_view_report_load_complete (view->details->nautilus_view);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}


static void
mozilla_link_message_callback (GtkMozEmbed *mozilla, gpointer user_data)
{
 	NautilusMozillaContentView	*view;
	char				*link_message;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	/* DEBUG_MSG (("+%s\n", __FUNCTION__)); */

	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

	link_message = gtk_moz_embed_get_link_message (view->details->mozilla);

	/* You could actually use make_full_uri_from_relative here
	 * to translate things like fragment links to full URI's.
	 * I just didn't.  Usually, Mozilla sends back full URI's here (except
	 * for fragments inside a document).
	 */

	/* DEBUG_MSG (("=%s new link message '%s'\n", __FUNCTION__, link_message)); */

	nautilus_view_report_status (view->details->nautilus_view, link_message);
	g_free (link_message);

	/* DEBUG_MSG (("-%s\n", __FUNCTION__)) */
}

static void
mozilla_progress_callback (GtkMozEmbed *mozilla,
			   gint         current_progress,
			   gint         max_progress,
			   gpointer     user_data)
{
 	NautilusMozillaContentView	*view;

	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);
	g_assert (GTK_MOZ_EMBED (mozilla) == view->details->mozilla);

#ifdef DEBUG_ramiro
	g_print ("mozilla_progress_callback (max = %d, current = %d)\n", max_progress, current_progress);
#endif

	/* NOTE:
	 * "max_progress" will be -1 if the filesize cannot be determined
	 * On occasion, it appears that current_progress may actuall exceed max_progress
	 */

	if (max_progress == -1 || max_progress == 0) {
		DEBUG_MSG ((">nautilus_view_report_load_progress %f\n", 0.0));
		nautilus_view_report_load_progress (view->details->nautilus_view, 0);
	} else if (max_progress < current_progress) {
		DEBUG_MSG ((">nautilus_view_report_load_progress %f\n", 1.0));
		nautilus_view_report_load_progress (view->details->nautilus_view, 1.0);
	} else {
		DEBUG_MSG ((">nautilus_view_report_load_progress %f\n", (double)current_progress / (double)max_progress));
		nautilus_view_report_load_progress (view->details->nautilus_view, (double)current_progress / (double)max_progress);
	}
}

static gint
mozilla_dom_key_press_callback (GtkMozEmbed                     *mozilla,
				gpointer                         dom_event,
				gpointer                         user_data)
{
	g_return_val_if_fail (dom_event != NULL, NS_DOM_EVENT_IGNORED);

	DEBUG_MSG (("+%s\n", __FUNCTION__));

	/* If this keyboard event is going to trigger a URL navigation, we need
	 * to fake it out like the mouse event below
	 */

	if (mozilla_events_is_key_return (dom_event)) {
		return mozilla_dom_mouse_click_callback (mozilla, dom_event, user_data);
	} else {
		return NS_DOM_EVENT_IGNORED;
	}

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

/*
 * The dom_mouse_click/key_press handler is here primarally to catch navigation
 * events to wierd nautilus-only URI schemes such as "eazel:"
 * 
 * It only catches navigations started from an anchor tag <A>, so in particular
 * form submissions can't be used to navigate to a nautilus-specific URI scheme
 */

static gint
mozilla_dom_mouse_click_callback (GtkMozEmbed *mozilla,
				  gpointer	dom_event,
				  gpointer	user_data)
{
 	NautilusMozillaContentView	*view;
	char				*href;
	char 				*href_full;
	gint				ret;

	href = NULL;
	href_full = NULL;
	ret = NS_DOM_EVENT_IGNORED;

	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla), NS_DOM_EVENT_IGNORED);
	g_return_val_if_fail (dom_event != NULL, NS_DOM_EVENT_IGNORED);
	g_return_val_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (user_data), NS_DOM_EVENT_IGNORED);
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (user_data);

	g_return_val_if_fail (GTK_MOZ_EMBED (mozilla) == view->details->mozilla, TRUE);

	DEBUG_MSG (("+%s\n", __FUNCTION__));

#ifdef DEBUG_ramiro
	g_print ("%s (%p)\n", __FUNCTION__, dom_event);
#endif

	href = mozilla_events_get_href_for_event (dom_event);

	if (href != NULL) {

		href_full = make_full_uri_from_relative (view->details->uri, href);

		DEBUG_MSG (("=%s href='%s' full='%s'\n", __FUNCTION__, href, href_full));

		if (href[0] == '#') {
			/* a navigation to an anchor within the same page */
			view->details->user_initiated_navigation = TRUE;

			/* bugzilla.mozilla.org 70311 -- anchor navigation
			 * doesn't work on documents that have been streamed
			 * into gtkmozembed.  So we do it outselves.
			 */
			if (should_mozilla_load_uri_directly (href_full)) {
				DEBUG_MSG (("=%s : anchor navigation in normal uri, allowing navigate to continue\n", __FUNCTION__));
				ret = NS_DOM_EVENT_IGNORED;
			} else {
				char *unescaped_anchor;

				/* href+1 to skip the fragment identifier */
				unescaped_anchor = gnome_vfs_unescape_string (href+1, NULL);

				DEBUG_MSG (("=%s : anchor navigation in gnome-vfs uri, navigate by hand to anchor '%s'\n", __FUNCTION__, unescaped_anchor));

				mozilla_navigate_to_anchor (view->details->mozilla, unescaped_anchor);
				g_free (unescaped_anchor);
				ret = NS_DOM_EVENT_CONSUMED;
			}
		} else if (0 == strncmp (href, "javascript:", strlen ("javascript:"))) {
			/* This is a bullshit javascript uri, let it pass */
			DEBUG_MSG (("=%s : javascript uri, allowing navigate to continue\n", __FUNCTION__));
			ret = NS_DOM_EVENT_IGNORED;			
		} else if (should_uri_navigate_bypass_nautilus (href_full)) {
			view->details->user_initiated_navigation = TRUE;

			if (should_mozilla_load_uri_directly (href_full)
			     || is_uri_relative (href)) {
				/* If the URI doesn't need to be translated and we can load it directly,
				 * then just keep going...report_location_change will happen in the
				 * mozilla_location_callback.
				 */
				/* This is cases (0), (1), and (2) above */
				DEBUG_MSG (("=%s : allowing navigate to continue\n", __FUNCTION__));
				ret = NS_DOM_EVENT_IGNORED;
			} else {
				/* Otherwise, cancel the current navigation and do it ourselves */
				/* This is case (3) above */
				/* FIXME: form posting in this case does not work */
				DEBUG_MSG (("=%s : handling navigate ourselves\n", __FUNCTION__));

				navigate_mozilla_to_nautilus_uri (view, href_full);
				update_nautilus_uri (view, href_full);

				ret = NS_DOM_EVENT_CONSUMED;
			}
		} else {
			/* With some schemes, navigation needs to be funneled through nautilus. */
			DEBUG_MSG (("=%s : funnelling navigation through nautilus\n", __FUNCTION__));
			DEBUG_MSG ((">nautilus_view_open_location_in_this_window '%s'", href_full));
			nautilus_view_open_location_in_this_window (view->details->nautilus_view, href_full);

			ret = NS_DOM_EVENT_CONSUMED;
		}
	} else {
		DEBUG_MSG (("=%s no href, ignoring\n", __FUNCTION__));
	}

	g_free (href_full);
	g_free (href);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
	
	return ret;
}


/***********************************************************************************/
/***********************************************************************************/

/**
 * vfs_open_callback
 *
 * Callback for gnome_vfs_async_open. Attempt to read data from handle
 * and pass to mozilla streaming callback.
 * 
 **/
static void
vfs_open_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer data)
{
	NautilusMozillaContentView *view = data;

	DEBUG_MSG (("+%s GnomeVFSResult: %u\n", __FUNCTION__, (unsigned)result));

	if (result != GNOME_VFS_OK)
	{
		gtk_moz_embed_close_stream (view->details->mozilla);
		/* NOTE: the view may go away after a call to report_load_failed */
		DEBUG_MSG ((">nautilus_view_report_load_failed\n"));
		nautilus_view_report_load_failed (view->details->nautilus_view);
	} else {
		if (view->details->vfs_read_buffer == NULL) {
			view->details->vfs_read_buffer = g_malloc (VFS_READ_BUFFER_SIZE);
		}
		gtk_moz_embed_open_stream (view->details->mozilla, "file:///", "text/html");
		gnome_vfs_async_read (handle, view->details->vfs_read_buffer, VFS_READ_BUFFER_SIZE, vfs_read_callback, view);
	}
	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

/**
 * vfs_read_callback:
 *
 * Read data from buffer and copy into mozilla stream.
 **/

static void
vfs_read_callback (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer buffer,
		   GnomeVFSFileSize bytes_requested,
		   GnomeVFSFileSize bytes_read,
		   gpointer data)
{
	NautilusMozillaContentView *view = data;

	DEBUG_MSG (("+%s %ld/%ld bytes\n", __FUNCTION__, (long)bytes_requested, (long) bytes_read));

	if (bytes_read != 0) {
		gtk_moz_embed_append_data (view->details->mozilla, buffer, bytes_read);
	}

	if (bytes_read == 0 || result != GNOME_VFS_OK) {
		gtk_moz_embed_close_stream (view->details->mozilla);
		view->details->vfs_handle = NULL;
		g_free (view->details->vfs_read_buffer);
		view->details->vfs_read_buffer = NULL;
		
		gnome_vfs_async_close (handle, (GnomeVFSAsyncCloseCallback) gtk_true, NULL);

		DEBUG_MSG ((">nautilus_view_report_load_complete\n"));
		nautilus_view_report_load_complete (view->details->nautilus_view);

		DEBUG_MSG (("=%s load complete\n", __FUNCTION__));
    	} else {
		gnome_vfs_async_read (handle, view->details->vfs_read_buffer, VFS_READ_BUFFER_SIZE, vfs_read_callback, view);
	}

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

/***********************************************************************************/
/***********************************************************************************/

#ifdef BUSY_CURSOR
static void
set_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	if (!view->details->busy_cursor) {
		view->details->busy_cursor = gdk_cursor_new (GDK_WATCH);
	}

	g_assert (view->details->busy_cursor != NULL);
	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->details->mozilla)));

	gdk_window_set_cursor (GTK_WIDGET (view->details->mozilla)->window, 
			       view->details->busy_cursor);

	gdk_flush ();
}

static void
clear_busy_cursor (NautilusMozillaContentView *view)
{
        g_return_if_fail (NAUTILUS_IS_MOZILLA_CONTENT_VIEW (view));

	g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (view->details->mozilla)));
	
	gdk_window_set_cursor (GTK_WIDGET (view->details->mozilla)->window, NULL);
}
#endif

static void
cancel_pending_vfs_operation (NautilusMozillaContentView *view)
{
	if (view->details->vfs_handle != NULL) {
		gnome_vfs_async_cancel (view->details->vfs_handle);
		gtk_moz_embed_close_stream (view->details->mozilla);
	}

	view->details->vfs_handle = NULL;
	g_free (view->details->vfs_read_buffer);
	view->details->vfs_read_buffer = NULL;
}


/* this takes a "nautilus" uri, not a "mozilla" uri */
static void
navigate_mozilla_to_nautilus_uri (NautilusMozillaContentView    *view,
			 	  const char			*uri)
{
	char *old_uri;

	cancel_pending_vfs_operation (view);

	if (!GTK_WIDGET_REALIZED (view->details->mozilla)) {

		/* Doing certain things to gtkmozembed before
		 * the widget has realized (specifically, opening
		 * content streams) can cause crashes.  To avoid
		 * this, we postpone all navigations
		 * until the widget has realized (we believe
		 * premature realization may cause other issues)
		 */

		DEBUG_MSG (("=%s: Postponing navigation request to widget realization\n", __FUNCTION__));
		/* Note that view->details->uri is still set below */
	} else {
		if (should_mozilla_load_uri_directly (uri)) {

			/* See if the current URI is the same as what mozilla already
			 * has.  If so, issue a reload rather than a load.
			 * We ask mozilla for it's uri rather than using view->details->uri because,
			 * from time to time, our understanding of mozilla's URI can become inaccurate
			 * (in particular, certain errors may cause embedded mozilla to not change
			 * locations)
			 */

			old_uri = gtk_moz_embed_get_location (view->details->mozilla);

			if (old_uri != NULL && uris_identical (uri, old_uri)) {
				DEBUG_MSG (("=%s uri's identical, telling mozilla to reload\n", __FUNCTION__));
				gtk_moz_embed_reload (view->details->mozilla,
					GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE);
			} else {
				gtk_moz_embed_load_url (view->details->mozilla, uri);
			}

			g_free (old_uri);
		} else {
			DEBUG_MSG (("=%s loading URI via gnome-vfs\n", __FUNCTION__));
			gnome_vfs_async_open (&(view->details->vfs_handle), uri, GNOME_VFS_OPEN_READ, vfs_open_callback, view);
		}
	}

	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	DEBUG_MSG (("=%s current URI is now '%s'\n", __FUNCTION__, view->details->uri));
}

static void
update_nautilus_uri (NautilusMozillaContentView *view, const char *nautilus_uri)
{
	g_free (view->details->uri);
	view->details->uri = g_strdup (nautilus_uri);

	DEBUG_MSG (("=%s current URI is now '%s'\n", __FUNCTION__, view->details->uri));

	DEBUG_MSG ((">nautilus_view_report_location_change '%s'\n", nautilus_uri));
	nautilus_view_report_location_change (view->details->nautilus_view,
					      nautilus_uri,
					      NULL,
					      nautilus_uri);
}

/***********************************************************************************/
/***********************************************************************************/

static gboolean
is_uri_relative (const char *uri)
{
	const char *current;

	/* RFC 2396 section 3.1 */
	for (current = uri ; 
		*current
		&& 	((*current >= 'a' && *current <= 'z')
			 || (*current >= 'A' && *current <= 'Z')
			 || (*current >= '0' && *current <= '9')
			 || ('-' == *current)
			 || ('+' == *current)
			 || ('.' == *current)) ;
	     current++);

	return  !(':' == *current);
}

/*
 * Remove "./" segments
 * Compact "../" segments inside the URI
 * Remove "." at the end of the URL 
 * Leave any ".."'s at the beginning of the URI
 */

/* in case if you were wondering, this is probably one of the least time-efficient ways to do this*/
static void
remove_internal_relative_components (char *uri_current)
{
	char *segment_prev, *segment_cur;
	size_t len_prev, len_cur;

	len_prev = len_cur = 0;
	segment_prev = NULL;

	segment_cur = uri_current;

	while (*segment_cur) {
		len_cur = strcspn (segment_cur, "/");

		if (len_cur == 1 && segment_cur[0] == '.') {
			/* Remove "." 's */
			if (segment_cur[1] == '\0') {
				segment_cur[0] = '\0';
				break;
			} else {
				memmove (segment_cur, segment_cur + 2, strlen (segment_cur + 2) + 1);
				continue;
			}
		} else if (len_cur == 2 && segment_cur[0] == '.' && segment_cur[1] == '.' ) {
			/* Remove ".."'s (and the component to the left of it) that aren't at the
			 * beginning or to the right of other ..'s
			 */
			if (segment_prev) {
				if (! (len_prev == 2
				       && segment_prev[0] == '.'
				       && segment_prev[1] == '.')) {
				       	if (segment_cur[2] == '\0') {
						segment_prev[0] = '\0';
						break;
				       	} else {
						memmove (segment_prev, segment_cur + 3, strlen (segment_cur + 3) + 1);

						segment_cur = segment_prev;
						len_cur = len_prev;

						/* now we find the previous segment_prev */
						if (segment_prev == uri_current) {
							segment_prev = NULL;
						} else if (segment_prev - uri_current >= 2) {
							segment_prev -= 2;
							for ( ; segment_prev > uri_current && segment_prev[0] != '/' 
							      ; segment_prev-- );
							if (segment_prev[0] == '/') {
								segment_prev++;
							}
						}
						continue;
					}
				}
			}
		}

		/*Forward to next segment */

		if (segment_cur [len_cur] == '\0') {
			break;
		}
		 
		segment_prev = segment_cur;
		len_prev = len_cur;
		segment_cur += len_cur + 1;	
	}
	
}

/* If I had known this relative uri code would have ended up this long, I would
 * have done it a different way
 */
static char *
make_full_uri_from_relative (const char *base_uri, const char *uri)
{
	char *result = NULL;

	g_return_val_if_fail (base_uri != NULL, g_strdup (uri));
	g_return_val_if_fail (uri != NULL, NULL);

	/* See section 5.2 in RFC 2396 */

	/* FIXME bugzilla.gnome.org 44413: This function does not take
	 * into account a BASE tag in an HTML document, so its
	 * functionality differs from what Mozilla itself would do.
	 */

	if (is_uri_relative (uri)) {
		char *mutable_base_uri;
		char *mutable_uri;

		char *uri_current;
		size_t base_uri_length;
		char *separator;

		/* We may need one extra character
		 * to append a "/" to uri's that have no "/"
		 * (such as help:)
		 */

		mutable_base_uri = g_malloc(strlen(base_uri)+2);
		strcpy (mutable_base_uri, base_uri);
		
		uri_current = mutable_uri = g_strdup (uri);

		/* Chew off Fragment and Query from the base_url */

		separator = strrchr (mutable_base_uri, '#'); 

		if (separator) {
			*separator = '\0';
		}

		separator = strrchr (mutable_base_uri, '?');

		if (separator) {
			*separator = '\0';
		}

		if ('/' == uri_current[0] && '/' == uri_current [1]) {
			/* Relative URI's beginning with the authority
			 * component inherit only the scheme from their parents
			 */

			separator = strchr (mutable_base_uri, ':');

			if (separator) {
				separator[1] = '\0';
			}			  
		} else if ('/' == uri_current[0]) {
			/* Relative URI's beginning with '/' absolute-path based
			 * at the root of the base uri
			 */

			separator = strchr (mutable_base_uri, ':');

			/* g_assert (separator), really */
			if (separator) {
				/* If we start with //, skip past the authority section */
				if ('/' == separator[1] && '/' == separator[2]) {
					separator = strchr (separator + 3, '/');
					if (separator) {
						separator[0] = '\0';
					}
				} else {
				/* If there's no //, just assume the scheme is the root */
					separator[1] = '\0';
				}
			}
		} else if ('#' != uri_current[0]) {
			/* Handle the ".." convention for relative uri's */

			/* If there's a trailing '/' on base_url, treat base_url
			 * as a directory path.
			 * Otherwise, treat it as a file path, and chop off the filename
			 */

			base_uri_length = strlen (mutable_base_uri);
			if ('/' == mutable_base_uri[base_uri_length-1]) {
				/* Trim off '/' for the operation below */
				mutable_base_uri[base_uri_length-1] = 0;
			} else {
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				}
			}

			remove_internal_relative_components (uri_current);

			/* handle the "../"'s at the beginning of the relative URI */
			while (0 == strncmp ("../", uri_current, 3)) {
				uri_current += 3;
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				} else {
					/* <shrug> */
					break;
				}
			}

			/* handle a ".." at the end */
			if (uri_current[0] == '.' && uri_current[1] == '.' 
			    && uri_current[2] == '\0') {

			    	uri_current += 2;
				separator = strrchr (mutable_base_uri, '/');
				if (separator) {
					*separator = '\0';
				}
			}

			/* Re-append the '/' */
			mutable_base_uri [strlen(mutable_base_uri)+1] = '\0';
			mutable_base_uri [strlen(mutable_base_uri)] = '/';
		}

		result = g_strconcat (mutable_base_uri, uri_current, NULL);
		g_free (mutable_base_uri); 
		g_free (mutable_uri); 

		DEBUG_MSG (("Relative URI converted base '%s' uri '%s' to '%s'\n", base_uri, uri, result));
	} else {
		result = g_strdup (uri);
	}
	
	return result;
}

#define TEST_PARTIAL(partial, expected) \
	tmp = make_full_uri_from_relative (base_uri, partial);					\
	if ( 0 != strcmp (tmp, expected) ) {							\
		g_message ("Test failed: partial '%s' expected '%s' got '%s'", partial, expected, tmp);	\
		success = FALSE;								\
		g_free (tmp);									\
	}

gboolean test_make_full_uri_from_relative (void);

gboolean
test_make_full_uri_from_relative (void)
{
	const char * base_uri = "http://a/b/c/d;p?q";
	char *tmp;
	gboolean success = TRUE;

	/* Commented out cases do no t work and should; they are marked above
	 * as fixmes
	 */

	/* From the RFC */

	TEST_PARTIAL ("g", "http://a/b/c/g");
	TEST_PARTIAL ("./g", "http://a/b/c/g");
	TEST_PARTIAL ("g/", "http://a/b/c/g/");
	TEST_PARTIAL ("/g", "http://a/g");

	TEST_PARTIAL ("//g", "http://g");
	
	TEST_PARTIAL ("?y", "http://a/b/c/?y");
	TEST_PARTIAL ("g?y", "http://a/b/c/g?y");
	TEST_PARTIAL ("#s", "http://a/b/c/d;p#s");
	TEST_PARTIAL ("g#s", "http://a/b/c/g#s");
	TEST_PARTIAL ("g?y#s", "http://a/b/c/g?y#s");
	TEST_PARTIAL (";x", "http://a/b/c/;x");
	TEST_PARTIAL ("g;x", "http://a/b/c/g;x");
	TEST_PARTIAL ("g;x?y#s", "http://a/b/c/g;x?y#s");

	TEST_PARTIAL (".", "http://a/b/c/");
	TEST_PARTIAL ("./", "http://a/b/c/");

	TEST_PARTIAL ("..", "http://a/b/");
	TEST_PARTIAL ("../g", "http://a/b/g");
	TEST_PARTIAL ("../..", "http://a/");
	TEST_PARTIAL ("../../", "http://a/");
	TEST_PARTIAL ("../../g", "http://a/g");

	/* Others */
	TEST_PARTIAL ("g/..", "http://a/b/c/");
	TEST_PARTIAL ("g/../", "http://a/b/c/");
	TEST_PARTIAL ("g/../g", "http://a/b/c/g");

	base_uri = "help:control-center";

	TEST_PARTIAL ("index.html#gnomecc-intro", "help:control-center/index.html#gnomecc-intro");

	return success;
}


gboolean
uris_identical (const char *uri1, const char *uri2)
{
	/*
	 * FIXME: the dns portion of the authority is case-insensitive,
	 * as is the scheme, but the rest of the URI is case-sensitive.  We just
	 * treat the whole thing as case-sensitive, which is mostly
	 * OK, especially since false negatives here aren't the end
	 * of the world
	 */

	return (strcmp (uri1, uri2) == 0);
}

/*
 * returns TRUE if a specified URI can be loaded by the mozilla-content-view
 * If TRUE: Nautilus is informed via report_location_change and navigation proceeds immediately
 * If FALSE: Nautilus is informed via open_location_in_this_window and we wait to be called back
 */

static gboolean
should_uri_navigate_bypass_nautilus (const char *uri)
{
	static const char *handled_by_nautilus[] =
	{
		/* URI's that use libvfs-help and the evil GnomeVFSTransform are
		 * deliberately excluded
		 */
#if 0
		"help",
		"gnome-help",
		"ghelp",
#endif
		"man",
		"info",
		"http",
		"file",
		"eazel-services"
	};

	g_return_val_if_fail (uri != NULL, FALSE);
	
	return string_list_get_index_of_string (handled_by_nautilus, EEL_N_ELEMENTS (handled_by_nautilus),
						uri) != STRING_LIST_NOT_FOUND;
}

/*
 * This a list of URI schemes that mozilla should load directly, rather than load through gnome-vfs
 */
static gboolean
should_mozilla_load_uri_directly (const char *uri)
{
	static const char *handled_by_mozilla[] =
	{
		"http",
		"file",
		"eazel-services"
	};

	return string_list_get_index_of_string (handled_by_mozilla, EEL_N_ELEMENTS (handled_by_mozilla),
						uri) != STRING_LIST_NOT_FOUND;
}

static gint
string_list_get_index_of_string (const char *string_list[], guint num_strings, const char *string)
{
	guint i;

	g_return_val_if_fail (string != NULL, STRING_LIST_NOT_FOUND);
	g_return_val_if_fail (string_list != NULL, STRING_LIST_NOT_FOUND);
	g_return_val_if_fail (num_strings > 0, STRING_LIST_NOT_FOUND);
	
	for (i = 0; i < num_strings; i++) {
		g_assert (string_list[i] != NULL);
		if (strlen (string) >= strlen (string_list[i]) 
		    && (strncasecmp (string, string_list[i], strlen (string_list[i])) == 0)) {
			return i;
		}
	}
	
	return STRING_LIST_NOT_FOUND;
}


/*
 * one-time initialization that need to happen before the first GtkMozEmbed widget
 * is created
 */ 

/* The "Mozilla Profile" directory is the place where mozilla stores 
 * things like cookies and cache.  Here we tell the mozilla embedding
 * widget to use ~/.nautilus/MozillaProfile for this purpose.
 *
 * We need mozilla 0.8 to support this feature.
 */

static void
pre_widget_initialize (void)
{
	const char *profile_directory_name = "MozillaProfile";
	char *profile_base_path;
	char *profile_path;
	char *cache_path;
	
	profile_base_path = g_strdup_printf ("%s/.nautilus", g_get_home_dir ());
	profile_path = g_strdup_printf ("%s/.nautilus/%s", g_get_home_dir (), profile_directory_name);
	cache_path = g_strdup_printf ("%s/.nautilus/%s/Cache", g_get_home_dir (), profile_directory_name);

	/* Create directories if they don't already exist */ 
	mkdir (profile_path, 0777);
	mkdir (cache_path, 0777);

#ifdef MOZILLA_HAVE_PROFILES_SUPPORT
	/* this will be in Mozilla 0.8 */
	/* Its a bug in mozilla embedding that we need to cast the const away */
	gtk_moz_embed_set_profile_path (profile_base_path, (char *) profile_directory_name);
#endif

	g_free (cache_path);
	g_free (profile_path);
	g_free (profile_base_path);
}


/*
 * one-time initialization that need to happen after the first GtkMozEmbed widget
 * is created
 */ 
static void
post_widget_initialize (void)
{
	static gboolean once = FALSE;
	char *cache_dir;
	
	if (once == TRUE) {
		return;
	}

	once = TRUE;

	/* Tell the security manager to allow ftp:// and file:// content through. */
	mozilla_preference_set_boolean ("security.checkloaduri", FALSE);

	/* Change http protocol user agent to include the string 'Nautilus' */
	mozilla_preference_set ("general.useragent.misc", "Nautilus/1.0Final");

	/* We dont want to use the proxy for localhost */
	mozilla_preference_set ("network.proxy.no_proxies_on", "localhost");

	/* Setup routing of proxy preferences from gconf to mozilla */
	mozilla_gconf_listen_for_proxy_changes ();

	cache_dir = g_strdup_printf ("%s/.nautilus/MozillaProfile/Cache", g_get_home_dir ());

	mozilla_preference_set ("browser.cache.directory", cache_dir);

	g_free (cache_dir);

	cache_dir = NULL;
}
