/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the implementation of the property browser window, which
 * gives the user access to an extensible palette of properties which
 * can be dropped on various elements of the user interface to
 * customize them 
 */

#include <config.h>
#include <math.h>
#include <ctype.h>


#include "nautilus-property-browser.h"
#include "nautilus-signaller.h"

#include <parser.h>
#include <xmlmemory.h>


#include <libgnomevfs/gnome-vfs.h>
#include <gnome.h>

#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-customization-data.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>

struct NautilusPropertyBrowserDetails {
	GtkHBox *container;
	
	GtkWidget *content_container;
	GtkWidget *content_frame;
	GtkWidget *content_table;
	
	GtkWidget *category_container;
	GtkWidget *category_table;
	GtkWidget *selected_button;
	
	GtkWidget *title_box;
	GtkWidget *title_label;
	GtkWidget *help_label;
	
	GtkWidget *bottom_box;
	
	GtkWidget *add_button;
	GtkWidget *add_button_label;	
	GtkWidget *remove_button;
	GtkWidget *remove_button_label;
	
	GtkWidget *dialog;
	
	GtkWidget *keyword;
	GtkWidget *emblem_image;
	GtkWidget *file_entry;
	
	char *path;
	char *category;
	char *dragged_file;
	char *drag_type;
	char *image_path;
	
	int category_position;
	int content_table_width;

	GdkPixbuf *property_chit;
		
	gboolean remove_mode;
	gboolean keep_around;
	gboolean has_local;
	gboolean toggle_button_flag;
};

static void  nautilus_property_browser_initialize_class (GtkObjectClass          *object_klass);
static void  nautilus_property_browser_initialize       (GtkObject               *object);
static void  nautilus_property_browser_destroy          (GtkObject               *object);
static void  nautilus_property_browser_preferences_changed (NautilusPropertyBrowser *property_browser);

static void  nautilus_property_browser_update_contents  (NautilusPropertyBrowser *property_browser);
static void  nautilus_property_browser_set_category     (NautilusPropertyBrowser *property_browser,
							 const char              *new_category);
static void  nautilus_property_browser_set_dragged_file (NautilusPropertyBrowser *property_browser,
							 const char              *dragged_file_name);
static void  nautilus_property_browser_set_drag_type    (NautilusPropertyBrowser *property_browser,
							 const char              *new_drag_type);
static void  add_new_button_callback                    (GtkWidget               *widget,
							 NautilusPropertyBrowser *property_browser);
static void  remove_button_callback                     (GtkWidget               *widget,
							 NautilusPropertyBrowser *property_browser);
static gboolean nautilus_property_browser_delete_event_callback (GtkWidget *widget,
							   GdkEvent  *event,
							   gpointer   user_data);
static void  nautilus_property_browser_drag_end         (GtkWidget               *widget,
							 GdkDragContext          *context);
static void  nautilus_property_browser_drag_data_get    (GtkWidget               *widget,
							 GdkDragContext          *context,
							 GtkSelectionData        *selection_data,
							 guint                    info,
							 guint32                  time);
static void  nautilus_property_browser_theme_changed	(gpointer user_data);
void         emit_emblems_changed_signal                (void);

/* misc utilities */
static char *strip_extension                            (const char              *string_to_strip);
static char *get_xml_path                               (NautilusPropertyBrowser *property_browser);

#define BROWSER_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"
#define BROWSER_TITLE_BAR_COLOR  "rgb:D6D6/D6D6/D6D6"

#define THEME_SELECT_COLOR "rgb:FFFF/9999/9999"

#define BROWSER_CATEGORIES_FILE_NAME "browser.xml"

#define PROPERTY_BROWSER_WIDTH 528
#define PROPERTY_BROWSER_HEIGHT 322

#define MAX_ICON_WIDTH 64
#define MAX_ICON_HEIGHT 64
#define COLOR_SQUARE_SIZE 48

#define CONTENT_TABLE_HEIGHT 4

enum {
	PROPERTY_TYPE,
};

static GtkTargetEntry drag_types[] = {
	{ "text/uri-list",  0, PROPERTY_TYPE }
};

static NautilusPropertyBrowser *main_browser = NULL;

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPropertyBrowser, nautilus_property_browser, GTK_TYPE_WINDOW)

/* initializing the class object by installing the operations we override */
static void
nautilus_property_browser_initialize_class (GtkObjectClass *object_klass)
{
	NautilusPropertyBrowserClass *klass;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (object_klass);

	klass = NAUTILUS_PROPERTY_BROWSER_CLASS (object_klass);

	object_klass->destroy = nautilus_property_browser_destroy;
	widget_class->drag_data_get  = nautilus_property_browser_drag_data_get;
	widget_class->drag_end  = nautilus_property_browser_drag_end;
}

/* initialize the instance's fields, create the necessary subviews, etc. */

static void
nautilus_property_browser_initialize (GtkObject *object)
{
	NautilusBackground *background;
 	NautilusPropertyBrowser *property_browser;
 	GtkWidget* widget, *temp_box, *temp_hbox, *temp_frame;
	GtkWidget *viewport;
	char *temp_str;
	
	property_browser = NAUTILUS_PROPERTY_BROWSER (object);
	widget = GTK_WIDGET (object);

	property_browser->details = g_new0 (NautilusPropertyBrowserDetails, 1);

	property_browser->details->path = NULL;
	property_browser->details->category = g_strdup ("backgrounds");
	property_browser->details->dragged_file = NULL;
	property_browser->details->drag_type = NULL;

	/* load the chit frame */
	temp_str = nautilus_pixmap_file ("chit_frame.png");
	property_browser->details->property_chit = gdk_pixbuf_new_from_file (temp_str);
	g_free (temp_str);
	
	/* set the initial size of the property browser */
	gtk_widget_set_usize (widget, PROPERTY_BROWSER_WIDTH, PROPERTY_BROWSER_HEIGHT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);				

	/* set the title */
	gtk_window_set_title(GTK_WINDOW(widget), _("Customization Options"));
	
	/* set up the background */
	
	background = nautilus_get_widget_background (GTK_WIDGET (property_browser));
	nautilus_background_set_color (background, BROWSER_BACKGROUND_COLOR);	
	
	/* create the container box */  
  	property_browser->details->container = GTK_HBOX (gtk_hbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->container), 0);				
	gtk_widget_show (GTK_WIDGET (property_browser->details->container));
	gtk_container_add (GTK_CONTAINER (property_browser),
			   GTK_WIDGET (property_browser->details->container));	

	/* make the category container */
	property_browser->details->category_container = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->category_container), 0 );				
 	property_browser->details->category_position = -1;	
 	
 	viewport = gtk_viewport_new(NULL, NULL);
	background = nautilus_get_widget_background (GTK_WIDGET (viewport));
	nautilus_background_set_color (background, BROWSER_TITLE_BAR_COLOR);	
	
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_OUT);
	gtk_widget_set_usize (viewport, 76, -1);

	gtk_box_pack_start (GTK_BOX (property_browser->details->container), property_browser->details->category_container, FALSE, FALSE, 0);
	gtk_widget_show (property_browser->details->category_container);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->category_container), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* allocate a table to hold the category selector */
  	property_browser->details->category_table = gtk_table_new (1, 4, FALSE);
	gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->category_table); 
	gtk_container_add (GTK_CONTAINER (property_browser->details->category_container), viewport);
	gtk_widget_show (GTK_WIDGET (property_browser->details->category_table));

	/* make the content container vbox */
  	property_browser->details->content_container = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (property_browser->details->content_container);
	gtk_box_pack_start (GTK_BOX (property_browser->details->container), property_browser->details->content_container, TRUE, TRUE, 0);
	
  	/* create the title box */
  	
  	property_browser->details->title_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->title_box), 0);				
 
  	background = nautilus_get_widget_background (GTK_WIDGET (property_browser->details->title_box));
	nautilus_background_set_color (background, BROWSER_TITLE_BAR_COLOR);	

  	gtk_widget_show(property_browser->details->title_box);
	gtk_box_pack_start (GTK_BOX(property_browser->details->content_container), property_browser->details->title_box, FALSE, FALSE, 0);
  	
  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_OUT);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(property_browser->details->title_box), temp_frame);
  	
  	temp_hbox = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(temp_hbox);
  	gtk_container_add(GTK_CONTAINER(temp_frame), temp_hbox);
 	
	/* add the title label */
	property_browser->details->title_label = nautilus_label_new (_("Select A Category:"));
	nautilus_label_set_font_size (NAUTILUS_LABEL (property_browser->details->title_label), 18);

  	gtk_widget_show(property_browser->details->title_label);
	gtk_box_pack_start (GTK_BOX(temp_hbox), property_browser->details->title_label, FALSE, FALSE, 8);
 
 	/* add the help label */
	property_browser->details->help_label = nautilus_label_new  ("");
	nautilus_label_set_font_size (NAUTILUS_LABEL (property_browser->details->help_label), 12);
  	
	gtk_widget_show(property_browser->details->help_label);
	gtk_box_pack_end (GTK_BOX(temp_hbox), property_browser->details->help_label, FALSE, FALSE, 8);
 	 	
  	/* add the bottom box to hold the command buttons */
  	temp_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (temp_box), 0);				
  	gtk_widget_show(temp_box);

  	background = nautilus_get_widget_background (temp_box);
	nautilus_background_set_color (background, BROWSER_TITLE_BAR_COLOR);	

  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_IN);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(temp_box), temp_frame);

  	property_browser->details->bottom_box = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(property_browser->details->bottom_box);
	gtk_box_pack_end (GTK_BOX(property_browser->details->content_container), temp_box, FALSE, FALSE, 0);
  	gtk_container_add (GTK_CONTAINER (temp_frame), property_browser->details->bottom_box);
  	
  	/* create the "add new" button */
  	property_browser->details->add_button = gtk_button_new ();
	gtk_widget_show(property_browser->details->add_button);
	
	property_browser->details->add_button_label = gtk_label_new (_("Add new..."));
	gtk_widget_show(property_browser->details->add_button_label);
	gtk_container_add (GTK_CONTAINER(property_browser->details->add_button), property_browser->details->add_button_label);
	gtk_box_pack_end (GTK_BOX(property_browser->details->bottom_box), property_browser->details->add_button, FALSE, FALSE, 4);
 	  
 	gtk_signal_connect(GTK_OBJECT (property_browser->details->add_button), "clicked", GTK_SIGNAL_FUNC (add_new_button_callback), property_browser);
	
	/* now create the "remove" button */
  	property_browser->details->remove_button = gtk_button_new();
	gtk_widget_show(property_browser->details->remove_button);
	
	property_browser->details->remove_button_label = gtk_label_new (_("Remove..."));	
	gtk_widget_show(property_browser->details->remove_button_label);
	gtk_container_add (GTK_CONTAINER(property_browser->details->remove_button), property_browser->details->remove_button_label);
	gtk_box_pack_end (GTK_BOX (property_browser->details->bottom_box),
			  property_browser->details->remove_button,
			  FALSE,
			  FALSE,
			  4);
	
 	gtk_signal_connect (GTK_OBJECT (property_browser->details->remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (remove_button_callback),
			    property_browser);

	/* now create the actual content, with the category pane and the content frame */	
	
	/* the actual contents are created when necessary */	
  	property_browser->details->content_frame = NULL;

	/* add callback for preference changes */
	nautilus_preferences_add_callback(NAUTILUS_PREFERENCES_CAN_ADD_CONTENT, 
						(NautilusPreferencesCallback) nautilus_property_browser_preferences_changed, 
						property_browser);
	
	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, 
					   nautilus_property_browser_theme_changed,
					   property_browser);	
	
	gtk_signal_connect (GTK_OBJECT (property_browser), "delete_event",
                    	    GTK_SIGNAL_FUNC (nautilus_property_browser_delete_event_callback),
                    	    NULL);

	/* initially, display the top level */
	nautilus_property_browser_set_path(property_browser, BROWSER_CATEGORIES_FILE_NAME);
}

static void
nautilus_property_browser_destroy (GtkObject *object)
{
	NautilusPropertyBrowser *property_browser;

	property_browser = NAUTILUS_PROPERTY_BROWSER (object);
	
	g_free (property_browser->details->path);
	g_free (property_browser->details->category);
	g_free (property_browser->details->dragged_file);
	g_free (property_browser->details->drag_type);
	
	if (property_browser->details->property_chit) {
		gdk_pixbuf_unref (property_browser->details->property_chit);
	}
	
	g_free (property_browser->details);
	
	nautilus_preferences_remove_callback(NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
						(NautilusPreferencesCallback) nautilus_property_browser_preferences_changed, 
						NULL);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_property_browser_theme_changed,
					      property_browser);

	if (object == GTK_OBJECT (main_browser))
		main_browser = NULL;
		
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

/* create a new instance */
NautilusPropertyBrowser *
nautilus_property_browser_new (void)
{
	NautilusPropertyBrowser *browser;

	browser = NAUTILUS_PROPERTY_BROWSER
		(gtk_widget_new (nautilus_property_browser_get_type (), NULL));
	
	gtk_container_set_border_width (GTK_CONTAINER (browser), 0);
  	gtk_window_set_policy (GTK_WINDOW(browser), TRUE, TRUE, FALSE);
  	gtk_widget_show (GTK_WIDGET(browser));
	
	return browser;
}

/* show the main property browser */

void
nautilus_property_browser_show (void)
{
	if (main_browser == NULL) {
		main_browser = nautilus_property_browser_new ();
	} else {
		nautilus_gtk_window_present (GTK_WINDOW (main_browser));
	}
}

static gboolean
nautilus_property_browser_delete_event_callback (GtkWidget *widget,
					   GdkEvent  *event,
					   gpointer   user_data)
{
	/* Hide but don't destroy */
	gtk_widget_hide(widget);
	return TRUE;
}

/* remember the name of the dragged file */
static void
nautilus_property_browser_set_dragged_file (NautilusPropertyBrowser *property_browser,
					    const char *dragged_file_name)
{       
	g_free (property_browser->details->dragged_file);
	property_browser->details->dragged_file = g_strdup (dragged_file_name);
}

/* remember the drag type */
static void
nautilus_property_browser_set_drag_type (NautilusPropertyBrowser *property_browser,
					 const char *new_drag_type)
{       
	g_free (property_browser->details->drag_type);
	property_browser->details->drag_type = g_strdup (new_drag_type);
}

/* drag and drop data get handler */

static void
nautilus_property_browser_drag_data_get (GtkWidget *widget,
					 GdkDragContext *context,
					 GtkSelectionData *selection_data,
					 guint info,
					 guint32 time)
{
	char  *image_file_name, *image_file_uri;
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(widget);
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (context != NULL);

	switch (info) {
	case PROPERTY_TYPE:
		/* formulate the drag data based on the drag type.  Eventually, we will
		   probably select the behavior from properties in the category xml definition,
		   but for now we hardwire it to the drag_type */
		
		if (!strcmp(property_browser->details->drag_type, "property/keyword")) {
			char* keyword_str = strip_extension(property_browser->details->dragged_file);
		        gtk_selection_data_set(selection_data, selection_data->target, 8, keyword_str, strlen(keyword_str));
			g_free(keyword_str);
			return;	
		}
		else if (!strcmp(property_browser->details->drag_type, "application/x-color")) {
		        GdkColor color;
			guint16 colorArray[4];
			
			gdk_color_parse(property_browser->details->dragged_file, &color);
			colorArray[0] = color.red;
			colorArray[1] = color.green;
			colorArray[2] = color.blue;
			colorArray[3] = 0xffff;
						
			gtk_selection_data_set(selection_data,
			selection_data->target, 16, (const char *) &colorArray[0], 8);
			return;	
		}
		
		image_file_name = g_strdup_printf ("%s/%s/%s",
						   NAUTILUS_DATADIR,
						   property_browser->details->category,
						   property_browser->details->dragged_file);
		
		if (!g_file_exists (image_file_name)) {
			char *user_directory;
			g_free (image_file_name);

			user_directory = nautilus_get_user_directory ();
			image_file_name = g_strdup_printf ("%s/%s/%s",
							   user_directory,
							   property_browser->details->category, 
							   property_browser->details->dragged_file);	

			g_free (user_directory);
		}

		image_file_uri = gnome_vfs_get_uri_from_local_path (image_file_name);
		gtk_selection_data_set (selection_data, selection_data->target, 8, image_file_uri, strlen (image_file_uri));
		g_free (image_file_name);
		g_free (image_file_uri);
		
		break;
	default:
		g_assert_not_reached ();
	}
}

/* drag and drop end handler, where we destroy ourselves, since the transaction is complete */

static void
nautilus_property_browser_drag_end (GtkWidget *widget, GdkDragContext *context)
{
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(widget);
	if (!property_browser->details->keep_around) {
		gtk_widget_hide (GTK_WIDGET (widget));
	}
}

/* utility routine to check if the passed-in uri is an image file */
static gboolean
ensure_uri_is_image(const char *uri)
{	
	gboolean is_image;
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info
		(uri, file_info,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		 | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
        is_image = nautilus_istr_has_prefix (file_info->mime_type, "image/") && (nautilus_strcmp (file_info->mime_type, "image/svg") != 0);
	gnome_vfs_file_info_unref (file_info);
	return is_image;
}

/* create the appropriate pixbuf for the passed in file */
static GdkPixbuf *
make_drag_image(NautilusPropertyBrowser *property_browser, const char* file_name)
{
	GdkPixbuf *pixbuf, *orig_pixbuf;
	char *image_file_name;

	image_file_name = g_strdup_printf ("%s/%s/%s",
					   NAUTILUS_DATADIR,
					   property_browser->details->category,
					   file_name);
	
	if (!g_file_exists (image_file_name)) {
		char *user_directory;
		g_free (image_file_name);

		user_directory = nautilus_get_user_directory ();

		image_file_name = g_strdup_printf ("%s/%s/%s",
						   user_directory,
						   property_browser->details->category,
						   file_name);	

		g_free (user_directory);	
	}
	
	orig_pixbuf = gdk_pixbuf_new_from_file (image_file_name);
	
	/* background properties are always a fixed size, while others are pinned to the max size */
	if (!strcmp(property_browser->details->category, "backgrounds")) {
		pixbuf = nautilus_customization_make_background_chit (orig_pixbuf, property_browser->details->property_chit, TRUE);
	} else {
		pixbuf = nautilus_gdk_pixbuf_scale_down_to_fit (orig_pixbuf, MAX_ICON_WIDTH, MAX_ICON_HEIGHT);
		gdk_pixbuf_unref (orig_pixbuf);
	}

	g_free (image_file_name);

	return pixbuf;
}

/* create a pixbuf and fill it with a color */

static GdkPixbuf*
make_color_drag_image (NautilusPropertyBrowser *property_browser, const char *color_spec, gboolean trim_edges)
{
	GdkPixbuf *color_square;
	int row, col, stride;
	char *pixels, *row_pixels;
	GdkColor color;

	color_square = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, COLOR_SQUARE_SIZE, COLOR_SQUARE_SIZE);
	
	gdk_color_parse(color_spec, &color);
	color.red >>= 8;
	color.green >>= 8;
	color.blue >>= 8;
	
	pixels = gdk_pixbuf_get_pixels (color_square);
	stride = gdk_pixbuf_get_rowstride (color_square);
	
	/* loop through and set each pixel */
	for (row = 0; row < COLOR_SQUARE_SIZE; row++) {
		row_pixels =  (pixels + (row * stride));
		for (col = 0; col < COLOR_SQUARE_SIZE; col++) {		
			*row_pixels++ = color.red;
			*row_pixels++ = color.green;
			*row_pixels++ = color.blue;
			*row_pixels++ = 255;
		}
	}
	
	return nautilus_customization_make_background_chit (color_square, 
							    property_browser->details->property_chit,
							    trim_edges);	
}

/* this callback handles button presses on the category widget. It maintains the select color */

static void
category_clicked_callback (GtkWidget *widget, char *category_name)
{
	gboolean save_flag;
	NautilusPropertyBrowser *property_browser;
	
	property_browser = NAUTILUS_PROPERTY_BROWSER (gtk_object_get_user_data (GTK_OBJECT (widget)));
	
	/* special case the user clicking on the already selected button, since we don't want that to toggle */
	if (widget == GTK_WIDGET(property_browser->details->selected_button)) {
		if (!property_browser->details->toggle_button_flag)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (property_browser->details->selected_button), TRUE);		
		return;
	}	
	
	save_flag = property_browser->details->toggle_button_flag;
	property_browser->details->toggle_button_flag = TRUE;	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (property_browser->details->selected_button), FALSE);
	property_browser->details->toggle_button_flag = save_flag;	
	
	nautilus_property_browser_set_category (property_browser, category_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	property_browser->details->selected_button = widget;
}


/* routines to remove specific category types.  First, handle colors */
/* having trouble removing nodes, so instead I'll mark it invisible - eventually this needs to be fixed */

static void
remove_color (NautilusPropertyBrowser *property_browser, const char* color_value)
{
	/* load the local xml file to remove the color */
	xmlNodePtr cur_node;
	char* xml_path = get_xml_path(property_browser);
	xmlDocPtr document = xmlParseFile (xml_path);
	char *user_directory;
	g_free(xml_path);

	if (document == NULL) {
		return;
	}

	/* find the colors category */
	for (cur_node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     cur_node != NULL; cur_node = cur_node->next) {
		if (strcmp(cur_node->name, "category") == 0) {
			char* category_name =  xmlGetProp (cur_node, "name");
			if (strcmp(category_name, "colors") == 0) {
				/* loop through the colors to find one that matches */
				xmlNodePtr color_node = nautilus_xml_get_children (cur_node);
				while (color_node != NULL) {
					char* color_content = xmlNodeGetContent(color_node);
					if (color_content && !strcmp(color_content, color_value) &&
						!xmlGetProp (color_node, "deleted")) {
						xmlSetProp(color_node, "deleted", "1");
						break;
					}
					else
						color_node = color_node->next;
				}
				break;
			}
		}
	}

	/* write the document back out to the file in the user's home directory */

	user_directory = nautilus_get_user_directory ();	
	xml_path = nautilus_make_path (user_directory, property_browser->details->path);
	g_free (user_directory);
	xmlSaveFile(xml_path, document);
	xmlFreeDoc(document);
	g_free(xml_path);
}

/* remove the background matching the passed in name */

static void
remove_background(NautilusPropertyBrowser *property_browser, const char* background_name)
{
	char *background_path, *background_uri;
	char *user_directory;

	user_directory = nautilus_get_user_directory ();

	/* build the pathname of the background */
	background_path = g_strdup_printf ("%s/backgrounds/%s",
					   user_directory,
					   background_name);
	background_uri = gnome_vfs_get_uri_from_local_path (background_path);
	g_free (background_path);

	g_free (user_directory);	

	/* delete the background from the background directory */
	if (gnome_vfs_unlink (background_uri) != GNOME_VFS_OK) {
		char *message = g_strdup_printf (_("Sorry, but background %s couldn't be deleted."), background_name);
		nautilus_error_dialog (message, _("Couldn't delete background"), GTK_WINDOW (property_browser));
		g_free (message);
	}
	
	g_free (background_uri);
}

/* remove the emblem matching the passed in name */

static void
remove_emblem (NautilusPropertyBrowser *property_browser, const char* emblem_name)
{
	/* build the pathname of the emblem */
	char *emblem_path, *emblem_uri;
	char *user_directory;

	user_directory = nautilus_get_user_directory ();

	emblem_path = g_strdup_printf ("%s/emblems/%s",
				       user_directory,
				       emblem_name);
	emblem_uri = gnome_vfs_get_uri_from_local_path (emblem_path);
	g_free (emblem_path);

	g_free (user_directory);

	/* delete the emblem from the emblem directory */
	if (gnome_vfs_unlink (emblem_uri) != GNOME_VFS_OK) {
		char *message = g_strdup_printf (_("Sorry, but emblem %s couldn't be deleted."), emblem_name);
		nautilus_error_dialog (message, _("Couldn't delete background"), GTK_WINDOW (property_browser));
		g_free (message);
	}
	else {
		emit_emblems_changed_signal ();
	}
	g_free (emblem_uri);
}

/* handle removing the passed in element */

static void
nautilus_property_browser_remove_element (NautilusPropertyBrowser *property_browser, const char* element_name)
{
	/* lookup category and get mode, then case out and handle the modes */
	if (!strcmp(property_browser->details->category, "backgrounds")) {
		remove_background(property_browser, element_name);
	} else if (!strcmp(property_browser->details->category, "colors")) {
		remove_color(property_browser, element_name);
	} else if (!strcmp(property_browser->details->category, "emblems")) {
		remove_emblem(property_browser, element_name);
	}

}

/* Callback used when the color selection dialog is destroyed */
static gboolean
dialog_destroy (GtkWidget *widget, gpointer data)
{
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(data);
	property_browser->details->dialog = NULL;
	return FALSE;
}

/* fetch the path of the xml file.  First, try to find it in the home directory, but it
   we can't find it there, try the shared directory */
   
static char *
get_xml_path(NautilusPropertyBrowser *property_browser)
{
	char *xml_path;
	char *user_directory;

	user_directory = nautilus_get_user_directory ();

	/* first try the user's home directory */
	xml_path = nautilus_make_path (user_directory,
				       property_browser->details->path);
	g_free (user_directory);
	if (g_file_exists (xml_path)) {
		return xml_path;
	}
	g_free (xml_path);
	
	/* next try the shared directory */
	xml_path = nautilus_make_path (NAUTILUS_DATADIR,
				       property_browser->details->path);
	if (g_file_exists (xml_path)) {
		return xml_path;
	}
	g_free (xml_path);

	return NULL;
}

/* utility to set up the emblem image from the passed-in file */

static void
set_emblem_image_from_file(NautilusPropertyBrowser *property_browser)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	pixbuf = gdk_pixbuf_new_from_file (property_browser->details->image_path);			
	scaled_pixbuf = nautilus_gdk_pixbuf_scale_down_to_fit (pixbuf, MAX_ICON_WIDTH, MAX_ICON_HEIGHT);			
	gdk_pixbuf_unref (pixbuf);
    	gdk_pixbuf_render_pixmap_and_mask (scaled_pixbuf, &pixmap, &mask, NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
	gdk_pixbuf_unref (scaled_pixbuf);
	
	if (property_browser->details->emblem_image == NULL) {
		property_browser->details->emblem_image = gtk_pixmap_new(pixmap, mask);
		gtk_widget_show(property_browser->details->emblem_image);
	} else {
		gtk_pixmap_set (GTK_PIXMAP (property_browser->details->emblem_image),
				pixmap, mask);
	}
}

/* this callback is invoked when a file is selected by the file selection */
static void
emblem_image_file_changed (GtkWidget *entry, NautilusPropertyBrowser *property_browser)
{
	char *new_uri;

	new_uri = gnome_vfs_get_uri_from_local_path (gtk_entry_get_text (GTK_ENTRY(entry)));
	if (!ensure_uri_is_image (new_uri)) {
		char *message = g_strdup_printf
			(_("Sorry, but '%s' is not a usable image file!"),
			 gtk_entry_get_text(GTK_ENTRY(entry)));
		nautilus_error_dialog (message, _("Not an Image"), GTK_WINDOW (property_browser));
		g_free (message);
		
		gtk_entry_set_text(GTK_ENTRY(entry), property_browser->details->image_path);
		g_free(new_uri);
		return;
	}
	
	g_free (new_uri);
	g_free (property_browser->details->image_path);
	property_browser->details->image_path = gtk_entry_get_text(GTK_ENTRY(entry));
	if (property_browser->details->image_path)
		property_browser->details->image_path = g_strdup(property_browser->details->image_path);
	
	/* set up the pixmap in the dialog */
	
	set_emblem_image_from_file(property_browser);
}

/* here's where we create the emblem dialog */

static GtkWidget*
nautilus_emblem_dialog_new(NautilusPropertyBrowser *property_browser)
{
	GtkWidget *widget, *entry;
	GtkWidget *dialog = gnome_dialog_new(_("Create a New Emblem:"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	GtkWidget *table = gtk_table_new(2, 2, FALSE);

	/* make the keyword label and field */	
	
	widget = gtk_label_new(_("Keyword:"));
	gtk_widget_show(widget);
	gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 4, 4);
	
  	property_browser->details->keyword = gtk_entry_new_with_max_length (24);
	gtk_widget_show(property_browser->details->keyword);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->keyword, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4, 4);

	/* default image is the generic emblem */
	g_free(property_browser->details->image_path);
		
	property_browser->details->image_path = nautilus_pixmap_file ("emblem-generic.png"); 
	property_browser->details->emblem_image = NULL; /* created lazily by set_emblem_image */
	set_emblem_image_from_file(property_browser);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->emblem_image, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 4, 4);
 
	/* set up a gnome file entry to pick the image file */
	property_browser->details->file_entry = gnome_file_entry_new ("nautilus", _("Select an image file for the new emblem:"));
	gnome_file_entry_set_default_path(GNOME_FILE_ENTRY(property_browser->details->file_entry), property_browser->details->image_path);
	
	gtk_widget_show(property_browser->details->file_entry);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->file_entry, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 4);
	
	/* connect to the activate signal of the entry to change images */
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY(property_browser->details->file_entry));
	gtk_entry_set_text(GTK_ENTRY(entry), property_browser->details->image_path);
	
	gtk_signal_connect (GTK_OBJECT (entry), "activate", (GtkSignalFunc) emblem_image_file_changed, property_browser);
		
	/* install the table in the dialog */
	
	gtk_widget_show(table);	
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), table, TRUE, TRUE, GNOME_PAD);
	gnome_dialog_set_default(GNOME_DIALOG(dialog), GNOME_OK);
	
	return dialog;
}

/* add the newly selected file to the browser images */

static void
add_background_to_browser (GtkWidget *widget, gpointer *data)
{
	gboolean is_image;
	char *directory_path, *source_file_name, *destination_name;
	char *path_uri;
	char *user_directory;	
	char *directory_uri;
	GnomeVFSResult result;
	
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(data);

	/* get the file path from the file selection widget */
	char *path_name = g_strdup(gtk_file_selection_get_filename (GTK_FILE_SELECTION (property_browser->details->dialog)));
	
	gtk_widget_destroy (property_browser->details->dialog);
	property_browser->details->dialog = NULL;

	/* fetch the mime type and make sure that the file is an image */
	path_uri = gnome_vfs_get_uri_from_local_path (path_name);
	is_image = ensure_uri_is_image(path_uri);
	g_free(path_uri);	
	
	if (!is_image) {
		char *message = g_strdup_printf (_("Sorry, but '%s' is not a usable image file!"), path_name);
		nautilus_error_dialog (message, _("Not an Image"), NULL);
		g_free (message);
		g_free (path_name);
		g_free (path_uri);	
		return;
	}

	user_directory = nautilus_get_user_directory ();

	/* copy the image file to the backgrounds directory */
	/* FIXME bugzilla.eazel.com 1250: 
	 * do we need to do this with gnome-vfs? 
	 */
	directory_path = nautilus_make_path (user_directory, property_browser->details->category);
	g_free (user_directory);
	source_file_name = strrchr (path_name, '/');
	destination_name = nautilus_make_path (directory_path, source_file_name + 1);
	
	/* make the directory if it doesn't exist */
	if (!g_file_exists(directory_path)) {
		directory_uri = gnome_vfs_get_uri_from_local_path (directory_path);
		gnome_vfs_make_directory (directory_uri,
					  GNOME_VFS_PERM_USER_ALL
					  | GNOME_VFS_PERM_GROUP_ALL
					  | GNOME_VFS_PERM_OTHER_READ);
		g_free (directory_uri);
	}
		
	g_free(directory_path);
	
	result = nautilus_copy_uri_simple (path_name, destination_name);		
	if (result != GNOME_VFS_OK) {
		char *message = g_strdup_printf (_("Sorry, but the background %s couldn't be installed."), path_name);
		nautilus_error_dialog (message, _("Couldn't install background"), GTK_WINDOW (property_browser));
		g_free (message);
	}
				
	g_free(path_name);	
	g_free(destination_name);
	
	/* update the property browser's contents to show the new one */
	nautilus_property_browser_update_contents(property_browser);
}

/* here's where we initiate adding a new background by putting up a file selector */

static void
add_new_background(NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show(property_browser->details->dialog);
		if (property_browser->details->dialog->window)
			gdk_window_raise(property_browser->details->dialog->window);

	} else {
		GtkFileSelection *file_dialog;

		property_browser->details->dialog = gtk_file_selection_new
			(_("Select an image file to add as a background:"));
		file_dialog = GTK_FILE_SELECTION (property_browser->details->dialog);
		
		gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				    "destroy",
				    (GtkSignalFunc) dialog_destroy,
				    property_browser);
		gtk_signal_connect (GTK_OBJECT (file_dialog->ok_button),
				    "clicked",
				    (GtkSignalFunc) add_background_to_browser,
				    property_browser);
		gtk_signal_connect_object (GTK_OBJECT (file_dialog->cancel_button),
					   "clicked",
					   (GtkSignalFunc) gtk_widget_destroy,
					   GTK_OBJECT(file_dialog));

		gtk_window_set_position (GTK_WINDOW (file_dialog), GTK_WIN_POS_MOUSE);
		gtk_widget_show (GTK_WIDGET(file_dialog));
	}
}

/* here's where we add the passed in color to the file that defines the colors */

static void
add_color_to_file(NautilusPropertyBrowser *property_browser, const char *color_spec)
{
	xmlNodePtr cur_node;
	char* xml_path = get_xml_path(property_browser);
	xmlDocPtr document = xmlParseFile (xml_path);
	char *user_directory;

	g_free(xml_path);

	if (document == NULL) {
		return;
	}

	/* find the colors category */
	for (cur_node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     cur_node != NULL; cur_node = cur_node->next) {
		if (strcmp(cur_node->name, "category") == 0) {
			char* category_name =  xmlGetProp (cur_node, "name");
			if (strcmp(category_name, "colors") == 0) {
				/* add a new color node */
				xmlNodePtr new_color_node = xmlNewChild(cur_node, NULL, "color", NULL);
				xmlNodeSetContent(new_color_node, color_spec);
				xmlSetProp(new_color_node, "local", "1");
				break;
			}
		}
	}

	user_directory = nautilus_get_user_directory ();
	
	/* write the document back out to the file in the user's home directory */
	xml_path = nautilus_make_path (user_directory, property_browser->details->path);
	g_free (user_directory);
	xmlSaveFile(xml_path, document);
	xmlFreeDoc(document);
	g_free(xml_path);
}

/* handle the OK button being pushed on the color selector */
static void
add_color_to_browser (GtkWidget *widget, gpointer *data)
{
	char* color_spec;
	gdouble color[4];
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(data);
	
	gtk_color_selection_get_color (GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (property_browser->details->dialog)->colorsel), color);
	gtk_widget_destroy (property_browser->details->dialog);
	property_browser->details->dialog = NULL;	

	color_spec = g_strdup_printf
		("rgb:%04hX/%04hX/%04hX",
		 (gushort) (color[0] * 65535.0 + 0.5),
		 (gushort) (color[1] * 65535.0 + 0.5),
		 (gushort) (color[2] * 65535.0 + 0.5));
	add_color_to_file(property_browser, color_spec);
	nautilus_property_browser_update_contents(property_browser);
	g_free(color_spec);	
}


/* here's the routine to add a new color, by putting up a color selector */

static void
add_new_color(NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show(property_browser->details->dialog);
		if (property_browser->details->dialog->window)
			gdk_window_raise(property_browser->details->dialog->window);

	} else {
		GtkColorSelectionDialog *color_dialog;

		property_browser->details->dialog = gtk_color_selection_dialog_new (_("Select a color to add:"));
		color_dialog = GTK_COLOR_SELECTION_DIALOG (property_browser->details->dialog);
		
		gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				    "destroy",
				    (GtkSignalFunc) dialog_destroy, property_browser);
		gtk_signal_connect (GTK_OBJECT (color_dialog->ok_button),
				    "clicked",
				    (GtkSignalFunc) add_color_to_browser, property_browser);
		gtk_signal_connect_object (GTK_OBJECT (color_dialog->cancel_button),
					   "clicked",
					   (GtkSignalFunc) gtk_widget_destroy,
					   GTK_OBJECT (color_dialog));
		gtk_widget_hide(color_dialog->help_button);

		gtk_window_set_position (GTK_WINDOW (color_dialog), GTK_WIN_POS_MOUSE);
		gtk_widget_show (GTK_WIDGET(color_dialog));
	}
}


/* here's where we handle clicks in the emblem dialog buttons */
static void
emblem_dialog_clicked (GtkWidget *dialog, int which_button, NautilusPropertyBrowser *property_browser)
{
	char *directory_uri;
	GnomeVFSResult result;
	
	if (which_button == GNOME_OK) {
		char *destination_name, *extension;
		char* new_keyword = gtk_entry_get_text(GTK_ENTRY(property_browser->details->keyword));
		char *user_directory;	
		char *directory_path;

		if (new_keyword == NULL || strlen (new_keyword) == 0) {
			nautilus_error_dialog (_("Sorry, but you must specify a keyword for the new emblem."), 
						_("Couldn't install emblem"), GTK_WINDOW (property_browser));
		} else {		
			user_directory = nautilus_get_user_directory ();

			/* get the path for emblems in the user's home directory */
			directory_path = nautilus_make_path (user_directory, property_browser->details->category);
			g_free (user_directory);

			/* make the directory if it doesn't exist */
			if (!g_file_exists (directory_path)) {
				directory_uri = gnome_vfs_get_uri_from_local_path (directory_path);
				gnome_vfs_make_directory(directory_uri,
						 	GNOME_VFS_PERM_USER_ALL
						 	| GNOME_VFS_PERM_GROUP_ALL
						 	| GNOME_VFS_PERM_OTHER_READ);
				g_free(directory_uri);
			}

			/* formulate the destination file name */
			extension = strrchr(property_browser->details->image_path, '.');
			destination_name = g_strdup_printf("%s/%s.%s", directory_path, new_keyword, extension + 1);
			g_free(directory_path);
				
			/* perform the actual copy */
			result = nautilus_copy_uri_simple (property_browser->details->image_path, destination_name);		
		
			if (result != GNOME_VFS_OK) {
				char *message = g_strdup_printf (_("Sorry, but the image at %s couldn't be installed as an emblem."), property_browser->details->image_path);
				nautilus_error_dialog (message, _("Couldn't install emblem"), GTK_WINDOW (property_browser));
				g_free (message);
			} else {
				emit_emblems_changed_signal ();	
			}
			
			g_free(destination_name);
				
			nautilus_property_browser_update_contents(property_browser);
		}
	}
	
	gtk_widget_destroy(dialog);
	
	property_browser->details->keyword = NULL;
	property_browser->details->emblem_image = NULL;
	property_browser->details->file_entry = NULL;
}

/* here's the routine to add a new emblem, by putting up an emblem dialog */

static void
add_new_emblem(NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show(property_browser->details->dialog);
		if (property_browser->details->dialog->window)
			gdk_window_raise(property_browser->details->dialog->window);

	} else {
		property_browser->details->dialog = nautilus_emblem_dialog_new (property_browser);		
		gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				    "destroy",
				    (GtkSignalFunc) dialog_destroy, property_browser);
		gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				    "clicked",
				    (GtkSignalFunc) emblem_dialog_clicked, property_browser);
		gtk_window_set_position (GTK_WINDOW (property_browser->details->dialog), GTK_WIN_POS_MOUSE);
		gtk_widget_show (GTK_WIDGET(property_browser->details->dialog));
	}
}

/* handle the add_new button */

static void
add_new_button_callback(GtkWidget *widget, NautilusPropertyBrowser *property_browser)
{
	/* handle remove mode, where we act as a cancel button */
	if (property_browser->details->remove_mode) {
		property_browser->details->remove_mode = FALSE;
		nautilus_property_browser_update_contents(property_browser);
		gtk_widget_show (property_browser->details->help_label);
		return;
	}
	
	/* case out on the category */
	if (strcmp(property_browser->details->category, "backgrounds") == 0) {
		add_new_background(property_browser);
	} else if (!strcmp(property_browser->details->category, "colors")) {
		add_new_color(property_browser);
	} else if (!strcmp(property_browser->details->category, "emblems")) {
		add_new_emblem(property_browser);
	}
}

/* handle the "remove" button */
static void
remove_button_callback(GtkWidget *widget, NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->remove_mode) {
		return;
	}
	
	property_browser->details->remove_mode = TRUE;
	gtk_widget_hide (property_browser->details->help_label);
	nautilus_property_browser_update_contents(property_browser);	
}

/* this callback handles clicks on the image or color based content content elements */

static void
element_clicked_callback(GtkWidget *widget, GdkEventButton *event, char *element_name)
{
	GtkTargetList *target_list;	
	GdkDragContext *context;
	GtkWidget *pixwidget;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap_for_dragged_file;
	GdkBitmap *mask_for_dragged_file;
	int pixmap_width, pixmap_height;
	int x_delta, y_delta;
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(gtk_object_get_user_data(GTK_OBJECT(widget)));
	
	/* handle remove mode by removing the element */
	if (property_browser->details->remove_mode) {
		nautilus_property_browser_remove_element(property_browser, element_name);
		property_browser->details->remove_mode = FALSE;
		nautilus_property_browser_update_contents(property_browser);
		gtk_widget_show (property_browser->details->help_label);
		return;
	}
	
	/* set up the drag and drop type corresponding to the category */
	drag_types[0].target = property_browser->details->drag_type;
	target_list = gtk_target_list_new (drag_types, NAUTILUS_N_ELEMENTS (drag_types));	
	nautilus_property_browser_set_dragged_file(property_browser, element_name);
	
	context = gtk_drag_begin (GTK_WIDGET (property_browser),
				  target_list,
				  GDK_ACTION_MOVE | GDK_ACTION_COPY,
				  event->button,
				  (GdkEvent *) event);

	/* compute the offsets for dragging */
	
	x_delta = floor(event->x + .5);
	y_delta = floor(event->y + .5) ;
	
	if (strcmp(property_browser->details->drag_type, "application/x-color")) {
		/*it's not a color, so, for now, it must be an image */
		
		pixwidget = GTK_BIN(widget)->child;
		gtk_pixmap_get(GTK_PIXMAP(pixwidget), &pixmap_for_dragged_file, &mask_for_dragged_file);
		gdk_window_get_size((GdkWindow*) pixmap_for_dragged_file, &pixmap_width, &pixmap_height);

		x_delta -= (widget->allocation.width - pixmap_width) >> 1;
		y_delta -= (widget->allocation.height - pixmap_height) >> 1;

		pixbuf = make_drag_image(property_browser, element_name);
	} else {
		pixbuf = make_color_drag_image(property_browser, element_name, TRUE);
	}

	
        /* set the pixmap and mask for dragging */       
	if (pixbuf != NULL) {
		gdk_pixbuf_render_pixmap_and_mask
			(pixbuf,
			 &pixmap_for_dragged_file,
			 &mask_for_dragged_file,
			 NAUTILUS_STANDARD_ALPHA_THRESHHOLD);

		gdk_pixbuf_unref (pixbuf);	
		gtk_drag_set_icon_pixmap
			(context,
			 gtk_widget_get_colormap (GTK_WIDGET (property_browser)),
			 pixmap_for_dragged_file,
			 mask_for_dragged_file,
			 x_delta, y_delta);
	}
	
	/* optionally (if the shift key is down) hide the property browser - it will later be destroyed when the drag ends */	
	property_browser->details->keep_around = (event->state & GDK_SHIFT_MASK) == 0;
	if (!property_browser->details->keep_around)
		gtk_widget_hide(GTK_WIDGET(property_browser));
}


/* utility routine to strip the extension from the passed in string */
static char*
strip_extension (const char* string_to_strip)
{
	char *result_str, *temp_str;
	if (string_to_strip == NULL)
		return NULL;
	
	result_str = g_strdup(string_to_strip);
	temp_str = strrchr(result_str, '.');
	if (temp_str)
		*temp_str = '\0';
	return result_str;
}

/* handle preferences changing by updating the browser contents */

static void
nautilus_property_browser_preferences_changed (NautilusPropertyBrowser *property_browser)
{
	nautilus_property_browser_update_contents(property_browser);
}

/* utility routine to add the passed-in widget to the content table */

static void
add_to_content_table (NautilusPropertyBrowser *property_browser, GtkWidget* widget, int position, int padding)
{
	int column_pos = position % property_browser->details->content_table_width;
	int row_pos = position / property_browser->details->content_table_width;
  	
	gtk_table_attach (GTK_TABLE (property_browser->details->content_table),
			  widget, column_pos, column_pos + 1, row_pos ,row_pos + 1, 
			  GTK_FILL, GTK_FILL, padding, padding);
}


/* make_properties_from_directories generates widgets corresponding all of the objects 
   in the public and private directories */

static GtkWidget *
make_property_tile (NautilusPropertyBrowser *property_browser, GtkWidget *pixmap_widget, GtkWidget *label, const char* property_name)
{
	
	NautilusBackground *background;
	
	GtkWidget *temp_vbox, *event_box;
	
	temp_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(temp_vbox);

	event_box = gtk_event_box_new();
	gtk_widget_show(event_box);
		
	background = nautilus_get_widget_background (GTK_WIDGET (event_box));
	nautilus_background_set_color (background, BROWSER_BACKGROUND_COLOR);	

	gtk_widget_show (pixmap_widget);
	gtk_container_add(GTK_CONTAINER(event_box), pixmap_widget);
	gtk_box_pack_start(GTK_BOX(temp_vbox), event_box, FALSE, FALSE, 0);

	if (label != NULL) {
		nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET(label), NAUTILUS_BACKGROUND_SOLID);		
		nautilus_buffered_widget_set_background_color
			(NAUTILUS_BUFFERED_WIDGET(label), NAUTILUS_RGB_COLOR_WHITE);		

			gtk_box_pack_start (GTK_BOX(temp_vbox), label, FALSE, FALSE, 0);
			gtk_widget_show(label);
	}
	
	gtk_object_set_user_data (GTK_OBJECT(event_box), property_browser);
	gtk_signal_connect_full
		(GTK_OBJECT (event_box),
		 "button_press_event", 
		 GTK_SIGNAL_FUNC (element_clicked_callback),
		 NULL,
		 g_strdup (property_name),
		 g_free,
		 FALSE,
		 FALSE);
	
	return temp_vbox;
}

static int
make_properties_from_directories (NautilusPropertyBrowser *property_browser)
{
	NautilusCustomizationData *customization_data;
	char *emblem_name;
	GtkWidget *pixmap_widget;
	GtkWidget *label;
	int index;


	/* make room for the reset property if necessary */
	index = (strcmp (property_browser->details->category, "backgrounds") == 0);
	
		
	customization_data = nautilus_customization_data_new (property_browser->details->category,
							      !property_browser->details->remove_mode,
							      FALSE,
							      MAX_ICON_WIDTH,
							      MAX_ICON_HEIGHT);
	if (customization_data == NULL) {
		return index;
	}

	/* interate through the set of emblems and display each */
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &emblem_name,
									 &pixmap_widget,
									 &label) == GNOME_VFS_OK) {
		GtkWidget *temp_vbox;
		
		/* allocate a pixmap and insert it into the table */
		temp_vbox = make_property_tile (property_browser, pixmap_widget, label, emblem_name);
				
		/* put the reset item in the pole position */
		add_to_content_table(property_browser, temp_vbox, 
				     strcmp(emblem_name, RESET_IMAGE_NAME) ? index++ : 0, 2);			
		g_free (emblem_name);
	}
	
	property_browser->details->has_local = nautilus_customization_data_private_data_was_displayed (customization_data);	
	nautilus_customization_data_destroy (customization_data);

	return index;
}

/* generate properties from the children of the passed in node */
/* for now, we just handle color nodes */

static void
make_properties_from_xml_node (NautilusPropertyBrowser *property_browser, xmlNodePtr node)
{
	xmlNode *current_node;
	GdkPixbuf *pixbuf;
	GtkWidget *pixmap_widget, *label;
	int index;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	gboolean local_only = property_browser->details->remove_mode;
	
	index = 0;
	property_browser->details->has_local = FALSE;
	
	for (current_node = nautilus_xml_get_children (node);
	     current_node != NULL; current_node = current_node->next) {
	        GtkWidget *new_property;
	
		char* color_str = xmlNodeGetContent (current_node);
		char* name_str = xmlGetProp (current_node, "name");
		char* local = xmlGetProp(current_node, "local");
		char* deleted = xmlGetProp(current_node, "deleted");
		
		if (local && !deleted) {
			property_browser->details->has_local = TRUE;
		}
			
		if (!deleted && (!local_only || (local != NULL))) {
			
			/* make the image from the color spec */

			pixbuf = make_color_drag_image (property_browser, color_str, FALSE);			
			gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
			gdk_pixbuf_unref (pixbuf);
			pixmap_widget = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));

			/* make the label from the name */
			label = nautilus_label_new (name_str);
			nautilus_label_set_font_size (NAUTILUS_LABEL (label), 12);

			/* make the tile from the pixmap and name */
			new_property = make_property_tile (property_browser, pixmap_widget, label, color_str);
			add_to_content_table (property_browser, new_property, index++, 12);				
		}
	}
}

/* handle theme changes by updating the browser contents */

static void
nautilus_property_browser_theme_changed (gpointer user_data)
{
	NautilusPropertyBrowser *property_browser;
	
	property_browser = NAUTILUS_PROPERTY_BROWSER(user_data);
	nautilus_property_browser_update_contents (property_browser);
}

/* make_category generates widgets corresponding all of the objects in the passed in directory */
static void
make_category(NautilusPropertyBrowser *property_browser, const char* path, const char* mode, xmlNodePtr node, const char *description)
{

	/* set up the description in the help label */
	nautilus_label_set_text (NAUTILUS_LABEL (property_browser->details->help_label), description);
	
	/* case out on the mode */
	if (strcmp(mode, "directory") == 0)
		make_properties_from_directories (property_browser);
	else if (strcmp(mode, "inline") == 0)
		make_properties_from_xml_node (property_browser, node);

}

/* this is a utility routine to generate a category link widget and install it in the browser */
static void
make_category_link (NautilusPropertyBrowser *property_browser, char* name, char *display_name, char* image)
{
	GtkWidget *label, *pix_widget, *button, *temp_vbox;
	char *file_name = nautilus_pixmap_file (image); 
	GtkWidget* temp_box = gtk_vbox_new (FALSE, 0);
	
	pix_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_widget_show (pix_widget);
	gtk_box_pack_start (GTK_BOX (temp_box), pix_widget, FALSE, FALSE, 0);
	
	button = gtk_toggle_button_new();
	gtk_widget_show(button);
	gtk_widget_set_usize(button, 80, 60);
	
	/* if the button represents the current category, highlight it */
	
	if (property_browser->details->category && !strcmp(property_browser->details->category, name)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		property_browser->details->selected_button = button;		
	}
	
	/* put the button in a vbox so it won't grow vertically */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (temp_vbox);	
	gtk_box_pack_start (GTK_BOX (temp_vbox), button, FALSE, FALSE, 1);	

	/* use the name as a label */
	label = gtk_label_new (display_name);
			
	gtk_box_pack_start (GTK_BOX (temp_box), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	gtk_table_attach (GTK_TABLE (property_browser->details->category_table),
			  temp_vbox, 0, 1,
			  property_browser->details->category_position, property_browser->details->category_position + 1, 
			  GTK_FILL, GTK_FILL, 4, 4);
	
	property_browser->details->category_position += 1;
	
	gtk_container_add (GTK_CONTAINER (button), temp_box);
	gtk_widget_show (temp_box);
	
	/* add a signal to handle clicks */
	gtk_object_set_user_data (GTK_OBJECT(button), property_browser);
	gtk_signal_connect_full
		(GTK_OBJECT (button),
		 "clicked",
		 GTK_SIGNAL_FUNC (category_clicked_callback),
		 NULL,
		 g_strdup (name),
		 g_free,
		 FALSE,
		 FALSE);
	
	g_free (file_name);
}

/* extract the number of columns for the current category from the xml file */
static void
set_up_category_width (NautilusPropertyBrowser *property_browser, xmlDocPtr document)
{
	char *column_str, *category_name;
	xmlNodePtr cur_node;
	
	/* set up the default */
	property_browser->details->content_table_width = 5;
	
	for (cur_node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     cur_node != NULL; cur_node = cur_node->next) {
		if (strcmp(cur_node->name, "category") == 0) {
			category_name =  xmlGetProp (cur_node, "name");
			if (!nautilus_strcmp (category_name, property_browser->details->category)) {
				column_str = xmlGetProp (cur_node, "columns");
				if (column_str) {
					property_browser->details->content_table_width = atoi (column_str);	
					return;
				}
			}
		}
	}
}

/* update_contents populates the property browser with information specified by the path and other state variables */
void
nautilus_property_browser_update_contents (NautilusPropertyBrowser *property_browser)
{
 	char *xml_path;
	xmlNodePtr cur_node;
 	xmlDocPtr document;
 	NautilusBackground *background;
	GtkWidget *viewport;
	gboolean show_buttons, got_categories;

	/* load the xml document corresponding to the path and selection */
	xml_path = get_xml_path(property_browser);
	document = xmlParseFile (xml_path);
	g_free(xml_path);
	
	if (document == NULL) {
		return;
	}
		
	/* remove the existing content box, if any, and allocate a new one */
	if (property_browser->details->content_frame) {
		gtk_widget_destroy(property_browser->details->content_frame);
	}
	
	/* set up the content_table_width field so we know how many columns to put in the table */
	set_up_category_width (property_browser, document);
	
	/* allocate a new container, with a scrollwindow and viewport */
	
	property_browser->details->content_frame = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->content_frame), 0);				
 	
 	viewport = gtk_viewport_new(NULL, NULL);
	gtk_widget_show(viewport);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
	background = nautilus_get_widget_background (viewport);
	nautilus_background_set_color (background, BROWSER_BACKGROUND_COLOR);	
	gtk_container_add (GTK_CONTAINER (property_browser->details->content_container), property_browser->details->content_frame);
	gtk_widget_show (property_browser->details->content_frame);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->content_frame), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* allocate a table to hold the content widgets */
  	property_browser->details->content_table = gtk_table_new(property_browser->details->content_table_width, CONTENT_TABLE_HEIGHT, FALSE);
	gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->content_table); 
	gtk_container_add (GTK_CONTAINER (property_browser->details->content_frame), viewport);
	gtk_widget_show (GTK_WIDGET (property_browser->details->content_table));
	
	/* iterate through the xml file to generate the widgets */
	got_categories = property_browser->details->category_position >= 0;
	if (!got_categories) {
		property_browser->details->category_position = 0;
	}
		
	for (cur_node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     cur_node != NULL; cur_node = cur_node->next) {
		if (strcmp(cur_node->name, "category") == 0) {
			char* category_name =  xmlGetProp (cur_node, "name");
			char* category_image = xmlGetProp (cur_node, "image");
			char* category_type  = xmlGetProp (cur_node, "type");
			char* category_description = xmlGetProp (cur_node, "description");
			char* display_name = xmlGetProp (cur_node, "display_name");
				
			if (property_browser->details->category && !strcmp(property_browser->details->category, category_name)) {
				char *category_path = xmlGetProp (cur_node, "path");
				char *category_mode = xmlGetProp (cur_node, "mode");
				
				make_category (property_browser, category_path, category_mode, cur_node, category_description);
				nautilus_property_browser_set_drag_type (property_browser, category_type);
			}
			
			if (!got_categories) {
				make_category_link (property_browser, category_name, display_name, category_image);
			}
		}
	}
	
	/* release the  xml document and we're done */
	xmlFreeDoc (document);

	/* update the title and button */

	show_buttons = nautilus_preferences_get_boolean(NAUTILUS_PREFERENCES_CAN_ADD_CONTENT, FALSE);

	if (property_browser->details->category == NULL) {
		nautilus_label_set_text(NAUTILUS_LABEL(property_browser->details->title_label), _("Select A Category:"));
		gtk_widget_hide(property_browser->details->add_button);
		gtk_widget_hide(property_browser->details->remove_button);
	
	} else {
		char *label_text, *temp_str;
				
		if (property_browser->details->remove_mode) {
			temp_str = g_strdup(_("Cancel Remove"));		
		} else {
			temp_str = g_strdup_printf(_("Add a new %s"), property_browser->details->category);	
			temp_str[strlen(temp_str) - 1] = '\0'; /* trim trailing s */
		}
		
		/* enable the "add new" button and update it's name */		
		
		gtk_label_set (GTK_LABEL(property_browser->details->add_button_label), temp_str);
		if (show_buttons)
			gtk_widget_show(property_browser->details->add_button);
		else
			gtk_widget_hide(property_browser->details->add_button);
			
		if (property_browser->details->remove_mode) {
			char *temp_category = g_strdup (property_browser->details->category);
			temp_category[strlen(temp_category) - 1] = '\0'; /* strip trailing s */
			label_text = g_strdup_printf(_("Click on a %s to remove it"), temp_category);		
			g_free(temp_category);
		} else {	
			label_text = g_strdup_printf ("%s:", property_browser->details->category);		
			label_text[0] = toupper (label_text[0]);
		}
		
		nautilus_label_set_text (NAUTILUS_LABEL(property_browser->details->title_label), label_text);

		/* enable the remove button (if necessary) and update its name */
		
		g_free(temp_str);
		temp_str = g_strdup_printf (_("Remove a %s"),
					    property_browser->details->category);		
				
		if (!show_buttons
		    || property_browser->details->remove_mode
		    || !property_browser->details->has_local)
			gtk_widget_hide(property_browser->details->remove_button);
		else
			gtk_widget_show(property_browser->details->remove_button);
		
		temp_str[strlen(temp_str) - 1] = '\0'; /* trim trailing s */
		gtk_label_set (GTK_LABEL(property_browser->details->remove_button_label), temp_str);
		
		g_free(label_text);
		g_free(temp_str);
	}
}

/* set the category and regenerate contents as necessary */

static void
nautilus_property_browser_set_category (NautilusPropertyBrowser *property_browser,
					const char *new_category)
{       
	/* there's nothing to do if the category is the same as the current one */ 
	if (nautilus_strcmp (property_browser->details->category, new_category) == 0) {
		return;
	}
	
	g_free (property_browser->details->category);
	property_browser->details->category = g_strdup (new_category);
		
	/* populate the per-uri box with the info */
	nautilus_property_browser_update_contents (property_browser);  	
}


/* here is the routine that populates the property browser with the appropriate information 
   when the path changes */

void
nautilus_property_browser_set_path (NautilusPropertyBrowser *property_browser, 
				    const char *new_path)
{       
	/* there's nothing to do if the uri is the same as the current one */ 
	if (nautilus_strcmp (property_browser->details->path, new_path) == 0) {
		return;
	}
	
	g_free (property_browser->details->path);
	property_browser->details->path = g_strdup (new_path);
	
	/* populate the per-uri box with the info */
	nautilus_property_browser_update_contents (property_browser);  	
}

void
emit_emblems_changed_signal (void)
{
	gtk_signal_emit_by_name (GTK_OBJECT (nautilus_signaller_get_current ()),
			 	 "emblems_changed");
}

