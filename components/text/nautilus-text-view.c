/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/* text view - display a text file */

#include <config.h>
#include "nautilus-text-view.h"

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-zoomable.h>
#include <eel/eel-background.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <ghttp.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gnome.h>
#include <gtk/gtkeventbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-clipboard.h>
#include <limits.h>

#define MAX_SERVICE_ITEMS 32
#define TEXT_VIEW_CHUNK_SIZE 65536
#define MAX_FILE_SIZE	1024*1024
#define MAX_SELECTION_SIZE 256

#define ADDITIONAL_SERVICES_MENU_PATH	"/menu/Text view/Services Placeholder"

#define NAUTILUS_PREFERENCES_TEXT_VIEW_FONT                     "components/text_view/font"
#define NAUTILUS_PREFERENCES_TEXT_VIEW_STANDARD_FONT_SIZE       "components/text_view/standard_font_size"

struct NautilusTextViewDetails {
	NautilusFile *file;
        GtkWidget *event_box;
	GnomeVFSAsyncHandle *file_handle;	
	char *buffer;
	
	BonoboZoomable *zoomable;
	int zoom_index;
	
	GtkWidget *container;
	GtkWidget *text_display;
	
	int service_item_count;
	GnomeVFSFileSize file_size;
	gboolean service_item_uses_selection[MAX_SERVICE_ITEMS];
};

/* structure for handling service menu item execution */
typedef struct {
	NautilusTextView *text_view;
	char *service_template;
	char *source_mode;
} ServiceMenuItemParameters;

static void nautilus_text_view_initialize_class (NautilusTextViewClass *klass);
static void nautilus_text_view_initialize       (NautilusTextView      *view);
static void nautilus_text_view_destroy          (GtkObject             *object);
static void nautilus_text_view_update           (NautilusTextView      *text_view);
static void text_view_load_location_callback    (NautilusView          *view,
						 const char            *location,
						 NautilusTextView      *text_view);
static void merge_bonobo_menu_items             (BonoboControl         *control,
						 gboolean               state,
						 gpointer               user_data);
static void nautilus_text_view_update_font      (NautilusTextView      *text_view);
static int  update_service_menu_items           (GtkWidget             *widget,
						 GdkEventButton        *event,
						 gpointer              *user_data);
static void zoomable_set_zoom_level_callback    (BonoboZoomable        *zoomable,
						 float                  level,
						 NautilusTextView      *view);
static void zoomable_zoom_in_callback           (BonoboZoomable        *zoomable,
						 NautilusTextView      *directory_view);
static void zoomable_zoom_out_callback          (BonoboZoomable        *zoomable,
						 NautilusTextView      *directory_view);
static void zoomable_zoom_to_fit_callback       (BonoboZoomable        *zoomable,
						 NautilusTextView      *directory_view);
static void nautilus_text_view_load_uri         (NautilusTextView      *view,
						 const char            *uri);
static void font_changed_callback               (gpointer               callback_data);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusTextView,
                                   nautilus_text_view,
                                   NAUTILUS_TYPE_VIEW)

static float text_view_preferred_zoom_levels[] = { .25, .50, .75, 1.0, 1.5, 2.0, 4.0 };
static int   text_view_font_size_deltas[] = { -6, -4, -2, 0, +4, +10, +22 };
static const int max_preferred_zoom_levels = (sizeof (text_view_preferred_zoom_levels) /
                                              sizeof (float)) - 1;

static void
nautilus_text_view_initialize_class (NautilusTextViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_text_view_destroy;

	/* Text view component fonts */
	eel_preferences_default_set_string (NAUTILUS_PREFERENCES_TEXT_VIEW_FONT,
                                            EEL_USER_LEVEL_INTERMEDIATE,
                                            "fixed");
	eel_preferences_default_set_integer (NAUTILUS_PREFERENCES_TEXT_VIEW_STANDARD_FONT_SIZE,
                                             EEL_USER_LEVEL_INTERMEDIATE,
                                             14);
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_text_view_initialize (NautilusTextView *text_view)
{
	GtkWidget *scrolled_window;
	
	text_view->details = g_new0 (NautilusTextViewDetails, 1);

        text_view->details->event_box = gtk_event_box_new ();

        nautilus_view_construct (NAUTILUS_VIEW (text_view), text_view->details->event_box);

	/* set up the zoomable interface */
	text_view->details->zoomable = bonobo_zoomable_new ();
	text_view->details->zoom_index = 3;
	
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_callback), text_view);
	gtk_signal_connect (GTK_OBJECT (text_view->details->zoomable), "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_callback), text_view);
	
	bonobo_zoomable_set_parameters_full (text_view->details->zoomable,
					     1.0, .25, 4.0, TRUE, TRUE, FALSE,
					     text_view_preferred_zoom_levels, NULL,
					     EEL_N_ELEMENTS (text_view_preferred_zoom_levels));
	
	bonobo_object_add_interface (BONOBO_OBJECT (text_view),
				     BONOBO_OBJECT (text_view->details->zoomable));
 
    	
	gtk_signal_connect (GTK_OBJECT (text_view), 
			    "load_location",
			    text_view_load_location_callback, 
			    text_view);
			    	
	/* allocate a vbox to contain the text widget */
	text_view->details->container = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (text_view->details->container), 0);
	gtk_container_add (GTK_CONTAINER (text_view->details->event_box), GTK_WIDGET (text_view->details->container));
	
	/* allocate the text object */
	text_view->details->text_display = gtk_text_new (NULL, NULL);
        gtk_widget_ref (text_view->details->text_display);
	gtk_text_set_editable (GTK_TEXT (text_view->details->text_display), FALSE);

	/* add signal handlers to the text field to enable/disable the service menu items */
	gtk_signal_connect_after (GTK_OBJECT (text_view->details->text_display),
                                  "button_release_event",
                                  GTK_SIGNAL_FUNC (update_service_menu_items), 
                                  text_view);
	gtk_signal_connect_after (GTK_OBJECT (text_view->details->text_display),
                                  "key_press_event",
                                  GTK_SIGNAL_FUNC (update_service_menu_items), 
                                  text_view);

	/* set the font of the text object */
	nautilus_text_view_update_font (text_view);

	/* Keep track of font changes */
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_TEXT_VIEW_FONT,
                                                  font_changed_callback,
                                                  text_view,
                                                  GTK_OBJECT (text_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_TEXT_VIEW_STANDARD_FONT_SIZE,
                                                  font_changed_callback,
                                                  text_view,
                                                  GTK_OBJECT (text_view));
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), text_view->details->text_display);
	gtk_container_add (GTK_CONTAINER (text_view->details->container), scrolled_window);

	/* get notified when we are activated so we can merge in our menu items */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(NAUTILUS_VIEW (text_view))),
                            "activate",
                            merge_bonobo_menu_items,
                            text_view);
		 	
	/* finally, we can show everything */	
	gtk_widget_show_all (GTK_WIDGET (text_view->details->event_box));
}

static void
detach_file (NautilusTextView *text_view)
{
        nautilus_file_unref (text_view->details->file);
        text_view->details->file = NULL;
}

static void
nautilus_text_view_destroy (GtkObject *object)
{
	NautilusTextView *text_view;
	
        text_view = NAUTILUS_TEXT_VIEW (object);

        detach_file (text_view);

	if (text_view->details->file_handle) {
		gnome_vfs_async_cancel (text_view->details->file_handle);
	}
	
        gtk_widget_unref (text_view->details->text_display);
	g_free (text_view->details->buffer);

	g_free (text_view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* here's a callback for the async close, which does nothing */
static void
file_close_callback (GnomeVFSAsyncHandle *handle,
			  GnomeVFSResult result,
			  gpointer callback_data)
{
}

/* this routine is called when we're finished reading to deallocate the buffer and
 * put up an error message if necessary
 */
static void
done_file_read (NautilusTextView *text_view, gboolean succeeded)
{
	if (text_view->details->buffer != NULL) {
		g_free (text_view->details->buffer);
		text_view->details->buffer = NULL;		
	}
	
	if (text_view->details->file_handle != NULL) {
		gnome_vfs_async_close (text_view->details->file_handle,
				       file_close_callback,
				       NULL);
	}
}
 
/* this callback handles the next chunk of data from the file by copying it into the
 * text field and reading more if necessary */
static void
file_read_callback (GnomeVFSAsyncHandle *vfs_handle,
		    GnomeVFSResult result,
		    gpointer buffer,
		    GnomeVFSFileSize bytes_requested,
		    GnomeVFSFileSize bytes_read,
		    gpointer callback_data)
{
	NautilusTextView *text_view;
        GtkText *display;
        char *name, *message;

	text_view = NAUTILUS_TEXT_VIEW (callback_data);

	text_view->details->file_size += bytes_read;

	/* at EOF, bytes_read is 0 and result is GNOME_VFS_OK */
	if (result == GNOME_VFS_OK && bytes_read > 0) {
		/* write the buffer into the text field */
                display = GTK_TEXT (text_view->details->text_display);
		if (!GTK_OBJECT_DESTROYED (display)) {
                        gtk_text_freeze (display);
                        gtk_text_set_point (display,
                                            gtk_text_get_length (display));
                        gtk_text_insert (display,
                                         NULL, NULL, NULL,
                                         buffer, bytes_read);
                        gtk_text_thaw (display);
                }
                
		/* read more if necessary */		
		if (text_view->details->file_size < MAX_FILE_SIZE) {
                        gnome_vfs_async_read (text_view->details->file_handle,
                                              text_view->details->buffer,
                                              TEXT_VIEW_CHUNK_SIZE,
                                              file_read_callback,
                                              callback_data);
                        return;
		} else {
                        name = nautilus_file_get_display_name (text_view->details->file);
                        message = g_strdup_printf (_("Sorry, but %s is too large for Nautilus to load all of it."),
                                                   name);
                        
			eel_show_error_dialog (message, _("File too large"), NULL);
                        
			g_free (name);
			g_free (message);
		}
	}

	done_file_read (text_view, result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF);
}


/* this callback gets invoked when the open call has completed */
static void
file_opened_callback (GnomeVFSAsyncHandle *vfs_handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	NautilusTextView *text_view;
	text_view = NAUTILUS_TEXT_VIEW (callback_data);
	
	if (result != GNOME_VFS_OK) {
		text_view->details->file_handle = NULL;
		done_file_read (text_view, FALSE);
		return;
	}

	text_view->details->file_size = 0;
	
	/* read the next chunck of the file */
	gnome_vfs_async_read (text_view->details->file_handle,
			      text_view->details->buffer,
			      TEXT_VIEW_CHUNK_SIZE,
			      file_read_callback,
			      callback_data);
}

/* callback to handle reading a chunk of the file
 * here's where we do the real work of inserting the text from the file into the view
 * Since the file may be large, and possibly remote, we load the text asynchronously and
 * progressively, one chunk at a time.
 */
static void
nautilus_text_view_update (NautilusTextView *text_view) 
{
	char *uri;
	
	uri = nautilus_file_get_uri (text_view->details->file);
	gtk_editable_delete_text (GTK_EDITABLE (text_view->details->text_display), 0, -1);   

	if (text_view->details->file_handle) {
		gnome_vfs_async_cancel (text_view->details->file_handle);
	}
	
	/* if necessary, allocate the buffer */
	if (text_view->details->buffer == NULL) {
		text_view->details->buffer = g_malloc (TEXT_VIEW_CHUNK_SIZE);
	}
	
	/* kick things off by opening the file asynchronously */
	gnome_vfs_async_open (&text_view->details->file_handle,
			      uri,
			      GNOME_VFS_OPEN_READ,
			      file_opened_callback,
			      text_view);
        g_free (uri);
}


static void
nautilus_text_view_load_uri (NautilusTextView *text_view, 
                             const char *uri)
{
 
	detach_file (text_view);
	text_view->details->file = nautilus_file_get (uri);
	
	nautilus_text_view_update (text_view);
}

static void
text_view_load_location_callback (NautilusView *view, 
                                   const char *location,
                                   NautilusTextView *text_view)
{
	nautilus_view_report_load_underway (NAUTILUS_VIEW (text_view));
	nautilus_text_view_load_uri (text_view, location);
	nautilus_view_report_load_complete (NAUTILUS_VIEW (text_view));
}

/* update the font and redraw */
static void
nautilus_text_view_update_font (NautilusTextView *text_view)
{
	char *font_name;
	GdkFont *font;
	int font_size;

	font_size = eel_preferences_get_integer (NAUTILUS_PREFERENCES_TEXT_VIEW_STANDARD_FONT_SIZE);

	font_size += text_view_font_size_deltas[text_view->details->zoom_index];

	font_name = eel_preferences_get (NAUTILUS_PREFERENCES_TEXT_VIEW_FONT);
	g_return_if_fail (font_name != NULL);

	font = nautilus_font_factory_get_font_by_family (font_name, font_size);
	g_free (font_name);

	g_return_if_fail (font != NULL);
	
	eel_gtk_widget_set_font (text_view->details->text_display, font);
	gdk_font_unref (font);

	gtk_editable_changed (GTK_EDITABLE (text_view->details->text_display));
}

/* handle merging in the menu items */
static void
handle_ui_event (BonoboUIComponent *ui,
		 const char *id,
		 Bonobo_UIComponent_EventType type,
		 const char *state,
		 NautilusTextView *view)
{
	if (type == Bonobo_UIComponent_STATE_CHANGED
            && eel_str_is_equal (state, "1")) {
                eel_preferences_set (NAUTILUS_PREFERENCES_TEXT_VIEW_FONT, id);
	}
}

static char *
get_selected_text (GtkEditable *text_widget)
{
	if (!text_widget->has_selection || text_widget->selection_start_pos == text_widget->selection_end_pos) {
		return NULL;
	}

	return gtk_editable_get_chars (text_widget, text_widget->selection_start_pos, text_widget->selection_end_pos);
}

/* here's the callback to handle the actual work for service menu items */
static void
handle_service_menu_item (BonoboUIComponent *ui, gpointer user_data, const char *verb)
{
	char *selected_text, *mapped_text, *uri;
	ServiceMenuItemParameters *parameters;
	int text_size;
	char *text_ptr;
	
	parameters = (ServiceMenuItemParameters *) user_data;
	
	/* determine if we should operate on the whole document or just the selection */
	if (eel_strcmp (parameters->source_mode, "document") == 0) {
		selected_text = gtk_editable_get_chars (GTK_EDITABLE (parameters->text_view->details->text_display), 0, -1);
		if (selected_text && strlen (selected_text) > 0) {
			/* formulate the url */
			mapped_text = gnome_vfs_escape_string (selected_text);
			uri = g_strdup_printf (parameters->service_template, mapped_text);
			g_free (mapped_text);
			
			/* load the resultant page through gnome-vfs */
                        /* FIXME: This uses sync. I/O. */
			if (eel_read_entire_file (uri, &text_size, &text_ptr) == GNOME_VFS_OK) {
 				gtk_editable_delete_text (GTK_EDITABLE (parameters->text_view->details->text_display), 0, -1);   
				gtk_text_insert (GTK_TEXT (parameters->text_view->details->text_display),
                                                 NULL, NULL, NULL,
                                                 text_ptr, text_size);
				g_free (text_ptr);
			}
			
			g_free (uri);
		}
		g_free (selected_text);
	} else {
		selected_text = get_selected_text (GTK_EDITABLE (parameters->text_view->details->text_display));
		if (selected_text != NULL) {
			/* truncate the selection to a reasonable size */
			if (strlen (selected_text) > MAX_SELECTION_SIZE) {
				selected_text [MAX_SELECTION_SIZE] = '\0';
			}
			
			/* formulate the url */
			mapped_text = gnome_vfs_escape_string (selected_text);
			uri = g_strdup_printf (parameters->service_template, mapped_text);
			
			/* goto the url */	
			nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (parameters->text_view), uri);

			g_free (uri);
			g_free (selected_text);
			g_free (mapped_text);
		}
		
	}
}

/* utility routines to add service items to the services menu by iterating the services/text directory */

static ServiceMenuItemParameters *
service_menu_item_parameters_new (NautilusTextView *text_view, const char *service_template, const char *source_mode)
{
	ServiceMenuItemParameters *result;

	result = g_new0 (ServiceMenuItemParameters, 1);
	result->text_view = text_view;
	result->service_template = g_strdup (service_template);
	if (source_mode != NULL) {
		result->source_mode = g_strdup (source_mode);
	} else {
		result->source_mode = NULL;
	}
	
	return result;
}

static void
service_menu_item_parameters_free (ServiceMenuItemParameters *parameters)
{
	g_free (parameters->service_template);
	g_free (parameters->source_mode);
	g_free (parameters);
}			      

/* add a service menu entry from the passed in xml file */
static void
add_one_service (NautilusTextView *text_view, BonoboControl *control, const char *xml_path, int* index)
{
	xmlDocPtr service_definition;
	xmlNodePtr service_node;
	char *label, *escaped_label;
	char *tooltip, *template;
	char *verb_name, *item_path, *verb_path, *source_mode;
	ServiceMenuItemParameters *parameters;
	BonoboUIComponent *ui;
	
	/* load and parse the xml file */
	service_definition = xmlParseFile(xml_path);
	ui = bonobo_control_get_ui_component (control);
	 	
	if (service_definition != NULL) {
		service_node = xmlDocGetRootElement (service_definition);
	
		/* extract the label and template */
		label = eel_xml_get_property_translated (service_node, "label");	
		tooltip = eel_xml_get_property_translated (service_node, "tooltip");	
		template = xmlGetProp (service_node, "template");
		source_mode = xmlGetProp (service_node, "source");
		
		if (label != NULL && template != NULL) {
			/* allocate a structure containing the text_view and template to pass in as the user data */
			escaped_label = eel_str_double_underscores (label);
			parameters = service_menu_item_parameters_new (text_view, template, source_mode);
		
			text_view->details->service_item_uses_selection[*index] = 
				eel_strcmp (source_mode, "document") != 0;
			
			/* use bonobo to add the menu item */
			nautilus_bonobo_add_numbered_menu_item 
				(ui, 
		 		ADDITIONAL_SERVICES_MENU_PATH,
		 		*index,
		 		escaped_label, 
		 		NULL);
			g_free (escaped_label);

			/* set the tooltip if one was present */
			if (tooltip) {
				item_path = nautilus_bonobo_get_numbered_menu_item_path
						(ui, ADDITIONAL_SERVICES_MENU_PATH, *index);	
				nautilus_bonobo_set_tip (ui, item_path, tooltip);
				g_free (item_path);
			}
			
			verb_name = nautilus_bonobo_get_numbered_menu_item_command 
				(ui, ADDITIONAL_SERVICES_MENU_PATH, *index);	
			bonobo_ui_component_add_verb_full (ui, verb_name,
							   handle_service_menu_item,
							   parameters,
							  (GDestroyNotify) service_menu_item_parameters_free);	   
			
			/* initially disable the item unless it's document-based; it will be enabled if there's a selection */
			verb_path = g_strdup_printf ("/commands/%s", verb_name);
			if (eel_strcmp (source_mode, "document") != 0) {
				nautilus_bonobo_set_sensitive (ui, verb_path, FALSE);
			}
			g_free (verb_name);	
			g_free (verb_path);
			
			*index += 1;
		}

		xmlFree (label);
		xmlFree (template);
		xmlFree (tooltip);
		
		/* release the xml file */
		xmlFreeDoc (service_definition);	
	}
}

/* iterate through the passed in services directory to add services to the menu.
 * Don't add a service with the same name as one that has already been added.
 */
static GList *
add_services_to_menu (NautilusTextView *text_view,
			BonoboControl *control,
			const char *services_directory,
			GList *added_services,
			int* index)
{
	GnomeVFSResult result;
	GList *file_list, *element;	
	GnomeVFSFileInfo *current_file_info;
	char *services_uri, *service_xml_path;
	
	services_uri = gnome_vfs_get_uri_from_local_path (services_directory);
	result = gnome_vfs_directory_list_load (&file_list, services_uri, GNOME_VFS_FILE_INFO_DEFAULT, NULL);
	
	if (result != GNOME_VFS_OK) {
		g_free (services_uri);
		return added_services;
	}

	/* iterate through the directory */
	for (element = file_list; element != NULL; element = element->next) {
		current_file_info = element->data;

		if (*index >= MAX_SERVICE_ITEMS) {
			break;
		}
		
		if (eel_istr_has_suffix (current_file_info->name, ".xml")) {		
			if (g_list_find_custom (added_services, current_file_info->name,
						(GCompareFunc) strcmp) == NULL) {
				service_xml_path = nautilus_make_path (services_directory, current_file_info->name);
				add_one_service (text_view, control, service_xml_path, index);
				added_services = g_list_prepend (added_services, g_strdup (current_file_info->name));
				g_free (service_xml_path);
			}
		}
	}
	
	g_free (services_uri);
	gnome_vfs_file_info_list_free (file_list);	
	return added_services;
}

/* build the services menu from the shared and private services/text directories */
static void
nautilus_text_view_build_service_menu (NautilusTextView *text_view, BonoboControl *control)
{
	char *services_directory, *user_directory, *nautilus_datadir;
	GList *added_services;
	int index;

	index = 0;
	
	/* first, add the services from the global directory */
	nautilus_datadir = nautilus_make_path (DATADIR, "nautilus");
	services_directory = nautilus_make_path (nautilus_datadir, "services/text");
	added_services = add_services_to_menu (text_view, control, services_directory, NULL, &index);
	g_free (nautilus_datadir);
	g_free (services_directory);
	
	/* now add services from the user-specific directory, if any */	
	user_directory = nautilus_get_user_directory ();
	services_directory = nautilus_make_path (user_directory, "services/text");
	added_services = add_services_to_menu (text_view, control, services_directory, added_services, &index);
	g_free (services_directory);
	g_free (user_directory);

	/* add any services that are present in the updates folder */
	user_directory = nautilus_get_user_directory ();
	services_directory = nautilus_make_path (user_directory, "updates/services/text");
	added_services = add_services_to_menu (text_view, control, services_directory, added_services, &index);
	g_free (services_directory);
	g_free (user_directory);
	
	text_view->details->service_item_count = index;	
	eel_g_list_free_deep (added_services);
}

/* handle updating the service menu items according to the selection state of the text display */
static int
update_service_menu_items (GtkWidget *widget, GdkEventButton *event, gpointer *user_data)
{
	NautilusTextView *text_view;
	BonoboUIComponent *ui;
	BonoboControl *control;
	GtkEditable *text_widget;
	gboolean has_selection;
	char *verb_name, *verb_path;
	int index;
	
	text_view = NAUTILUS_TEXT_VIEW (user_data);
	control = nautilus_view_get_bonobo_control (NAUTILUS_VIEW (text_view));	
	ui = bonobo_control_get_ui_component (control);
	
	/* determine if there is a selection */
	text_widget = GTK_EDITABLE (text_view->details->text_display);
	has_selection = text_widget->has_selection && text_widget->selection_start_pos != text_widget->selection_end_pos;
	
	/* iterate through the menu items, handling the selection state */
	
	for (index = 0; index < text_view->details->service_item_count; index++) {
		verb_name = nautilus_bonobo_get_numbered_menu_item_command 
				(ui, ADDITIONAL_SERVICES_MENU_PATH, index);	
		
		verb_path = g_strdup_printf ("/commands/%s", verb_name);
		if (text_view->details->service_item_uses_selection[index]) {
			nautilus_bonobo_set_sensitive (ui, 
                                                       verb_path,
                                                       has_selection);
		}
		g_free (verb_name);
		g_free (verb_path);
	}
	
	return FALSE;
}

/* this routine is invoked when the view is activated to merge in our menu items */
static void
merge_bonobo_menu_items (BonoboControl *control, gboolean state, gpointer user_data)
{
 	NautilusTextView *text_view;

	g_assert (BONOBO_IS_CONTROL (control));
	
	text_view = NAUTILUS_TEXT_VIEW (user_data);

	if (state) {
                /* FIXME bugzilla.gnome.org 41274:
                 * See nautilus-text-view-ui.xml.  Need to query system for
                 * available fonts instead of hard coding the font list
                 */

                /* FIXME bugzilla.gnome.org 47907: 
                 * The "GTK System Font" string is hard coded in many places.
                 * Please see nautilus-text-view-ui.xml.
                 */
		nautilus_view_set_up_ui (NAUTILUS_VIEW (text_view),
				         DATADIR,
					 "nautilus-text-view-ui.xml",
					 "nautilus-text-view");

		nautilus_text_view_build_service_menu (text_view, control);

                gtk_signal_connect (GTK_OBJECT (bonobo_control_get_ui_component (control)),
                                    "ui_event", handle_ui_event, text_view);
                
		nautilus_clipboard_set_up_editable_in_control (GTK_EDITABLE (text_view->details->text_display),
						       		control,
						       		FALSE);

	}

        /* Note that we do nothing if state is FALSE. Nautilus content
         * views are never explicitly deactivated
	 */
}

/* handle the zoomable signals */
static void
nautilus_text_view_zoom_to_level (NautilusTextView *text_view, int zoom_index)
{
	int pinned_zoom_index;
	pinned_zoom_index = zoom_index;

	if (pinned_zoom_index < 0) {
		pinned_zoom_index = 0;
	} else if (pinned_zoom_index > max_preferred_zoom_levels) {
		pinned_zoom_index = max_preferred_zoom_levels;
	}
	
	if (pinned_zoom_index != text_view->details->zoom_index) {
		text_view->details->zoom_index = pinned_zoom_index;
		bonobo_zoomable_report_zoom_level_changed (text_view->details->zoomable,
                                                           text_view_preferred_zoom_levels[pinned_zoom_index]);		
		nautilus_text_view_update_font (text_view);
	}
 }

static void
nautilus_text_view_bump_zoom_level (NautilusTextView *text_view, int increment)
{
	nautilus_text_view_zoom_to_level (text_view, text_view->details->zoom_index + increment);
}

static void
zoomable_zoom_in_callback (BonoboZoomable *zoomable, NautilusTextView *text_view)
{
	nautilus_text_view_bump_zoom_level (text_view, 1);
}

static void
zoomable_zoom_out_callback (BonoboZoomable *zoomable, NautilusTextView *text_view)
{
	nautilus_text_view_bump_zoom_level (text_view, -1);
}

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++) {
		float this, epsilon;

		/* if we're close to a zoom level */
		this = text_view_preferred_zoom_levels [i];
		epsilon = this * 0.01;

		if (zoom_level < this + epsilon)
			return i;
	}

	return max_preferred_zoom_levels;
}

static void
zoomable_set_zoom_level_callback (BonoboZoomable *zoomable, float level, NautilusTextView *view)
{
	nautilus_text_view_zoom_to_level (view, zoom_index_from_float (level));
}

static void
zoomable_zoom_to_fit_callback (BonoboZoomable *zoomable, NautilusTextView *view)
{
	nautilus_text_view_zoom_to_level (view, zoom_index_from_float (1.0));
}

static void
font_changed_callback (gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_TEXT_VIEW (callback_data));

	nautilus_text_view_update_font (NAUTILUS_TEXT_VIEW (callback_data));
}
