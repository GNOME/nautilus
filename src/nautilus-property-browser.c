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
#include "nautilus-property-browser.h"

#include "nautilus-signaller.h"
#include <ctype.h>
#include <eel/eel-background.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image-table.h>
#include <eel/eel-image.h>
#include <eel/eel-label.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-viewport.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gtk/gtkcolorsel.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkselection.h>
#include <gtk/gtktable.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkviewport.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-color-picker.h>
#include <libgnomeui/gnome-icon-entry.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-customization-data.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-drag-window.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-theme.h>
#include <math.h>

/* property types */

typedef enum {
	NAUTILUS_PROPERTY_NONE,
	NAUTILUS_PROPERTY_PATTERN,
	NAUTILUS_PROPERTY_COLOR,
	NAUTILUS_PROPERTY_EMBLEM
} NautilusPropertyType;

struct NautilusPropertyBrowserDetails {
	GtkHBox *container;
	
	GtkWidget *content_container;
	GtkWidget *content_frame;
	GtkWidget *content_table;
	
	GtkWidget *category_container;
	GtkWidget *category_box;
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
	
	GtkWidget *color_picker;
	GtkWidget *color_name;
	
	GList *keywords;
	
	char *path;
	char *category;
	char *dragged_file;
	char *drag_type;
	char *image_path;
	
	NautilusPropertyType category_type;
	
	int category_position;

	GdkPixbuf *property_chit;
		
	gboolean remove_mode;
	gboolean keep_around;
	gboolean has_local;
	gboolean toggle_button_flag;
};

static void     nautilus_property_browser_initialize_class      (GtkObjectClass                *object_klass);
static void     nautilus_property_browser_initialize            (GtkObject                     *object);
static void     nautilus_property_browser_destroy               (GtkObject                     *object);
static void     nautilus_property_browser_preferences_changed   (NautilusPropertyBrowser       *property_browser);
static void     nautilus_property_browser_update_contents       (NautilusPropertyBrowser       *property_browser);
static void     nautilus_property_browser_set_category          (NautilusPropertyBrowser       *property_browser,
								 const char                    *new_category);
static void     nautilus_property_browser_set_dragged_file      (NautilusPropertyBrowser       *property_browser,
								 const char                    *dragged_file_name);
static void     nautilus_property_browser_set_drag_type         (NautilusPropertyBrowser       *property_browser,
								 const char                    *new_drag_type);
static void     add_new_button_callback                         (GtkWidget                     *widget,
								 NautilusPropertyBrowser       *property_browser);
static void     cancel_remove_mode                              (NautilusPropertyBrowser       *property_browser);
static void     done_button_callback                            (GtkWidget                     *widget,
								 GtkWidget                     *property_browser);
static void     remove_button_callback                          (GtkWidget                     *widget,
								 NautilusPropertyBrowser       *property_browser);
static gboolean nautilus_property_browser_delete_event_callback (GtkWidget                     *widget,
								 GdkEvent                      *event,
								 gpointer                       user_data);
static void     nautilus_property_browser_hide_callback         (GtkWidget                     *widget,
								 gpointer                       user_data);
static void     nautilus_property_browser_drag_end              (GtkWidget                     *widget,
								 GdkDragContext                *context);
static void     nautilus_property_browser_drag_data_get         (GtkWidget                     *widget,
								 GdkDragContext                *context,
								 GtkSelectionData              *selection_data,
								 guint                          info,
								 guint32                        time);
static void     nautilus_property_browser_theme_changed         (gpointer                       user_data);
static void     emit_emblems_changed_signal                     (void);

/* misc utilities */
static char *   strip_extension                                 (const char                    *string_to_strip);
static void     element_clicked_callback                        (GtkWidget                     *image_table,
								 GtkWidget                     *child,
								 const EelImageTableEvent *event,
								 gpointer                       callback_data);


#define BROWSER_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"

#define THEME_SELECT_COLOR "rgb:FFFF/9999/9999"

#define BROWSER_CATEGORIES_FILE_NAME "browser.xml"

#define PROPERTY_BROWSER_WIDTH 540
#define PROPERTY_BROWSER_HEIGHT 340
#define MAX_EMBLEM_HEIGHT 52
#define STANDARD_BUTTON_IMAGE_HEIGHT 42

#define MAX_ICON_WIDTH 63
#define MAX_ICON_HEIGHT 63
#define COLOR_SQUARE_SIZE 48

#define LABELED_IMAGE_SPACING 2
#define IMAGE_TABLE_X_SPACING 6
#define IMAGE_TABLE_Y_SPACING 4

#define ERASE_OBJECT_NAME "erase.png"

enum {
	PROPERTY_TYPE,
};

static GtkTargetEntry drag_types[] = {
	{ "text/uri-list",  0, PROPERTY_TYPE }
};

static NautilusPropertyBrowser *main_browser = NULL;

EEL_DEFINE_CLASS_BOILERPLATE (NautilusPropertyBrowser,
				   nautilus_property_browser,
				   GTK_TYPE_WINDOW)

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
 	NautilusPropertyBrowser *property_browser;
 	GtkWidget* widget, *temp_box, *temp_hbox, *temp_frame;
	GtkWidget* temp_button;
	GtkWidget *viewport;
	char *temp_str;
	
	property_browser = NAUTILUS_PROPERTY_BROWSER (object);
	widget = GTK_WIDGET (object);

	property_browser->details = g_new0 (NautilusPropertyBrowserDetails, 1);

	property_browser->details->category = g_strdup ("patterns");
	property_browser->details->category_type = NAUTILUS_PROPERTY_PATTERN;
		
	/* load the chit frame */
	temp_str = nautilus_pixmap_file ("chit_frame.png");
	property_browser->details->property_chit = gdk_pixbuf_new_from_file (temp_str);
	g_free (temp_str);
	
	/* set the initial size of the property browser */
	gtk_widget_set_usize (widget, PROPERTY_BROWSER_WIDTH, PROPERTY_BROWSER_HEIGHT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);				

	/* set the title and standard close accelerator */
	gtk_window_set_title (GTK_WINDOW (widget), _("Backgrounds and Emblems"));
	gtk_window_set_wmclass (GTK_WINDOW (widget), "property_browser", "Nautilus");
	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (widget));
		
	/* create the container box */  
  	property_browser->details->container = GTK_HBOX (gtk_hbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->container), 0);
	gtk_widget_show (GTK_WIDGET (property_browser->details->container));
	gtk_container_add (GTK_CONTAINER (property_browser),
			   GTK_WIDGET (property_browser->details->container));	

	/* make the category container */
	property_browser->details->category_container = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->category_container), 0);
 	property_browser->details->category_position = -1;	
 	
 	viewport = eel_viewport_new (NULL, NULL);	
	eel_viewport_set_never_smooth (EEL_VIEWPORT (viewport), FALSE);
	gtk_widget_show (viewport);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);

	gtk_box_pack_start (GTK_BOX (property_browser->details->container),
			    property_browser->details->category_container, FALSE, FALSE, 0);
	gtk_widget_show (property_browser->details->category_container);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->category_container),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* allocate a table to hold the category selector */
  	property_browser->details->category_box = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width ( GTK_CONTAINER (property_browser->details->category_box), GNOME_PAD_SMALL);
	gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->category_box); 
	gtk_container_add (GTK_CONTAINER (property_browser->details->category_container), viewport);
	gtk_widget_show (GTK_WIDGET (property_browser->details->category_box));

	/* make the content container vbox */
  	property_browser->details->content_container = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (property_browser->details->content_container);
	gtk_box_pack_start (GTK_BOX (property_browser->details->container),
			    property_browser->details->content_container,
			    TRUE, TRUE, 0);
	
  	/* create the title box */
  	property_browser->details->title_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->title_box), 0);				
 	
  	gtk_widget_show(property_browser->details->title_box);
	gtk_box_pack_start (GTK_BOX(property_browser->details->content_container),
			    property_browser->details->title_box,
			    FALSE, FALSE, 0);
  	
  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_NONE);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(property_browser->details->title_box), temp_frame);
  	
  	temp_hbox = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(temp_hbox);
 	gtk_container_set_border_width (GTK_CONTAINER (temp_hbox), GNOME_PAD_SMALL);				
 
  	gtk_container_add(GTK_CONTAINER(temp_frame), temp_hbox);
 	
	/* add the title label */
	property_browser->details->title_label = eel_label_new ("");
	eel_label_make_larger (EEL_LABEL (property_browser->details->title_label), 4);
	eel_label_make_bold   (EEL_LABEL (property_browser->details->title_label));
 	
	gtk_widget_show(property_browser->details->title_label);
	gtk_box_pack_start (GTK_BOX(temp_hbox), property_browser->details->title_label, FALSE, FALSE, 8);
 
 	/* add the help label */
	property_browser->details->help_label = eel_label_new  ("");
	gtk_widget_show(property_browser->details->help_label);
	eel_label_make_smaller (EEL_LABEL (property_browser->details->help_label), 2);
	gtk_box_pack_end (GTK_BOX(temp_hbox), property_browser->details->help_label, FALSE, FALSE, 8);
 	 	
  	/* add the bottom box to hold the command buttons */
  	temp_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (temp_box), 0);				
  	gtk_widget_show(temp_box);

  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_NONE);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(temp_box), temp_frame);

  	property_browser->details->bottom_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (property_browser->details->bottom_box);
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->bottom_box), GNOME_PAD_SMALL);
	
	gtk_box_pack_end (GTK_BOX(property_browser->details->content_container), temp_box, FALSE, FALSE, 0);
  	gtk_container_add (GTK_CONTAINER (temp_frame), property_browser->details->bottom_box);
  	
  	/* create the "done" button */
 	temp_button = gtk_button_new_with_label (_("Done"));
	eel_gtk_button_set_padding (GTK_BUTTON (temp_button), GNOME_PAD_SMALL);
	gtk_widget_show(temp_button);
	gtk_box_pack_end (GTK_BOX(property_browser->details->bottom_box), temp_button, FALSE, FALSE, GNOME_PAD_SMALL);  
 	gtk_signal_connect(GTK_OBJECT (temp_button), "clicked", GTK_SIGNAL_FUNC (done_button_callback), property_browser);
  	
  	/* create the "add new" button */
  	property_browser->details->add_button = gtk_button_new ();
	gtk_widget_show(property_browser->details->add_button);
	
	property_browser->details->add_button_label = gtk_label_new (_("Add new..."));
	gtk_widget_show(property_browser->details->add_button_label);
	gtk_container_add (GTK_CONTAINER(property_browser->details->add_button),
			   property_browser->details->add_button_label);
	eel_gtk_button_set_padding (GTK_BUTTON (property_browser->details->add_button), GNOME_PAD_SMALL);
	gtk_box_pack_end (GTK_BOX(property_browser->details->bottom_box),
			  property_browser->details->add_button, FALSE, FALSE, GNOME_PAD_SMALL);
 	  
 	gtk_signal_connect(GTK_OBJECT (property_browser->details->add_button), "clicked",
			   add_new_button_callback, property_browser);
	
	/* now create the "remove" button */
  	property_browser->details->remove_button = gtk_button_new();
	gtk_widget_show(property_browser->details->remove_button);
	
	/* FIXME: Using spaces to add padding is not good design. */
	property_browser->details->remove_button_label = gtk_label_new (_("Remove..."));	
	gtk_widget_show(property_browser->details->remove_button_label);
	gtk_container_add (GTK_CONTAINER(property_browser->details->remove_button),
			   property_browser->details->remove_button_label);
	eel_gtk_button_set_padding (GTK_BUTTON (property_browser->details->remove_button), GNOME_PAD_SMALL);
	gtk_box_pack_end (GTK_BOX (property_browser->details->bottom_box),
			  property_browser->details->remove_button,
			  FALSE,
			  FALSE,
			  GNOME_PAD_SMALL);
	
 	gtk_signal_connect (GTK_OBJECT (property_browser->details->remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (remove_button_callback),
			    property_browser);

	/* now create the actual content, with the category pane and the content frame */	
	
	/* the actual contents are created when necessary */	
  	property_browser->details->content_frame = NULL;

	/* add callback for preference changes */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_CAN_ADD_CONTENT, 
				      (EelPreferencesCallback) nautilus_property_browser_preferences_changed, 
				      property_browser);
	
	/* add a callback for when the theme changes */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, 
				      nautilus_property_browser_theme_changed,
				      property_browser);	
	
	gtk_signal_connect (GTK_OBJECT (property_browser), "delete_event",
                    	    GTK_SIGNAL_FUNC (nautilus_property_browser_delete_event_callback),
                    	    NULL);

	gtk_signal_connect (GTK_OBJECT (property_browser), "hide",
                    	    nautilus_property_browser_hide_callback,
                    	    NULL);

	/* initially, display the top level */
	nautilus_property_browser_set_path(property_browser, BROWSER_CATEGORIES_FILE_NAME);

	/* Register that things may be dragged from this window */
	nautilus_drag_window_register (GTK_WINDOW (property_browser));
}

static void
nautilus_property_browser_destroy (GtkObject *object)
{
	NautilusPropertyBrowser *property_browser;

	
	property_browser = NAUTILUS_PROPERTY_BROWSER (object);

	eel_nullify_cancel (&property_browser->details->dialog);
	
	g_free (property_browser->details->path);
	g_free (property_browser->details->category);
	g_free (property_browser->details->dragged_file);
	g_free (property_browser->details->drag_type);

	eel_g_list_free_deep (property_browser->details->keywords);
		
	if (property_browser->details->property_chit) {
		gdk_pixbuf_unref (property_browser->details->property_chit);
	}
	
	g_free (property_browser->details);
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
					 (EelPreferencesCallback) nautilus_property_browser_preferences_changed, 
					 NULL);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					 nautilus_property_browser_theme_changed,
					 property_browser);
	if (object == GTK_OBJECT (main_browser))
		main_browser = NULL;
		
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));

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
		eel_gtk_window_present (GTK_WINDOW (main_browser));
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

static void
nautilus_property_browser_hide_callback (GtkWidget *widget,
					 gpointer   user_data)
{
	cancel_remove_mode (NAUTILUS_PROPERTY_BROWSER (widget));
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
	gboolean is_reset;
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(widget);
	
	g_return_if_fail (widget != NULL);
	g_return_if_fail (context != NULL);

	switch (info) {
	case PROPERTY_TYPE:
		/* formulate the drag data based on the drag type.  Eventually, we will
		   probably select the behavior from properties in the category xml definition,
		   but for now we hardwire it to the drag_type */
		
		is_reset = FALSE;
		if (!strcmp(property_browser->details->drag_type, "property/keyword")) {
			char* keyword_str = strip_extension(property_browser->details->dragged_file);
		        gtk_selection_data_set(selection_data, selection_data->target, 8, keyword_str, strlen(keyword_str));
			g_free(keyword_str);
			return;	
		}
		else if (!strcmp(property_browser->details->drag_type, "application/x-color")) {
		        GdkColor color;
			guint16 colorArray[4];
			
			/* handle the "reset" case as an image */
			if (eel_strcmp (property_browser->details->dragged_file, RESET_IMAGE_NAME) != 0) {
				gdk_color_parse(property_browser->details->dragged_file, &color);
				colorArray[0] = color.red;
				colorArray[1] = color.green;
				colorArray[2] = color.blue;
				colorArray[3] = 0xffff;
				
				gtk_selection_data_set(selection_data,
				selection_data->target, 16, (const char *) &colorArray[0], 8);
				return;	
			} else {
				is_reset = TRUE;
			}

		}
		
		image_file_name = g_strdup_printf ("%s/%s/%s",
						   NAUTILUS_DATADIR,
						   is_reset ? "patterns" : property_browser->details->category,
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
ensure_uri_is_image (const char *uri)
{	
	gboolean is_image;
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info
		(uri, file_info,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		 | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
        is_image = eel_istr_has_prefix (file_info->mime_type, "image/")
		&& eel_strcasecmp (file_info->mime_type, "image/svg") != 0;
	gnome_vfs_file_info_unref (file_info);
	return is_image;
}

/* create the appropriate pixbuf for the passed in file */

static GdkPixbuf *
make_drag_image (NautilusPropertyBrowser *property_browser, const char* file_name)
{
	GdkPixbuf *pixbuf, *orig_pixbuf;
	char *image_file_name;
	gboolean is_reset;
	
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
	
	is_reset = eel_strcmp (file_name, RESET_IMAGE_NAME) == 0;
	
	if (!strcmp(property_browser->details->category, "patterns")) {
		pixbuf = nautilus_customization_make_pattern_chit (orig_pixbuf, property_browser->details->property_chit, TRUE, is_reset);
	} else {
		pixbuf = eel_gdk_pixbuf_scale_down_to_fit (orig_pixbuf, MAX_ICON_WIDTH, MAX_ICON_HEIGHT);
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
	
	return nautilus_customization_make_pattern_chit (color_square, 
							    property_browser->details->property_chit,
							    trim_edges, FALSE);	
}

/* this callback handles button presses on the category widget. It maintains the active state */

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

	/* exit remove mode when the user switches categories, since there might be nothing to remove
	   in the new category */
	property_browser->details->remove_mode = FALSE;
		
	save_flag = property_browser->details->toggle_button_flag;
	property_browser->details->toggle_button_flag = TRUE;	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (property_browser->details->selected_button), FALSE);
	property_browser->details->toggle_button_flag = save_flag;	
	
	nautilus_property_browser_set_category (property_browser, category_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	property_browser->details->selected_button = widget;
}

static xmlDocPtr
read_browser_xml (NautilusPropertyBrowser *property_browser)
{
	char *path;
	xmlDocPtr document;

	path = nautilus_get_data_file_path (property_browser->details->path);
	if (path == NULL) {
		return NULL;
	}
	document = xmlParseFile (path);
	g_free (path);
	return document;
}

static void
write_browser_xml (NautilusPropertyBrowser *property_browser,
		   xmlDocPtr document)
{
	char *user_directory, *path;

	user_directory = nautilus_get_user_directory ();	
	path = nautilus_make_path (user_directory, property_browser->details->path);
	g_free (user_directory);
	xmlSaveFile (path, document);
	g_free (path);
}

static xmlNodePtr
get_color_category (xmlDocPtr document)
{
	return eel_xml_get_root_child_by_name_and_property (document, "category", "name", "colors");
}

/* routines to remove specific category types.  First, handle colors */
/* having trouble removing nodes, so instead I'll mark it invisible - eventually this needs to be fixed */

static void
remove_color (NautilusPropertyBrowser *property_browser, const char* color_value)
{
	/* load the local xml file to remove the color */
	xmlDocPtr document;
	xmlNodePtr cur_node, color_node;
	gboolean match;
	char *color_content;
	char *deleted_value;

	document = read_browser_xml (property_browser);
	if (document == NULL) {
		return;
	}

	/* find the colors category */
	cur_node = get_color_category (document);
	if (cur_node != NULL) {
		/* loop through the colors to find one that matches */
		for (color_node = eel_xml_get_children (cur_node);
		     color_node != NULL;
		     color_node = color_node->next) {
			color_content = xmlNodeGetContent(color_node);
			match = color_content != NULL
				&& strcmp (color_content, color_value) == 0;
			xmlFree (color_content);

			deleted_value = xmlGetProp (color_node, "deleted");
			xmlFree (deleted_value);
			
			if (match && deleted_value == NULL) {
				xmlSetProp(color_node, "deleted", "1");
				write_browser_xml (property_browser, document);
				break;
			}
		}
	}
	
	xmlFreeDoc (document);
}

/* remove the pattern matching the passed in name */

static void
remove_pattern(NautilusPropertyBrowser *property_browser, const char* pattern_name)
{
	char *pattern_path, *pattern_uri;
	char *user_directory;

	user_directory = nautilus_get_user_directory ();

	/* build the pathname of the pattern */
	pattern_path = g_strdup_printf ("%s/patterns/%s",
					   user_directory,
					   pattern_name);
	pattern_uri = gnome_vfs_get_uri_from_local_path (pattern_path);
	g_free (pattern_path);

	g_free (user_directory);	

	/* delete the pattern from the pattern directory */
	if (gnome_vfs_unlink (pattern_uri) != GNOME_VFS_OK) {
		char *message = g_strdup_printf (_("Sorry, but pattern %s couldn't be deleted."), pattern_name);
		eel_show_error_dialog (message, _("Couldn't delete pattern"), GTK_WINDOW (property_browser));
		g_free (message);
	}
	
	g_free (pattern_uri);
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
		eel_show_error_dialog (message, _("Couldn't delete pattern"), GTK_WINDOW (property_browser));
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
	switch (property_browser->details->category_type) {
	case NAUTILUS_PROPERTY_PATTERN:
		remove_pattern (property_browser, element_name);
		break;
	case NAUTILUS_PROPERTY_COLOR:
		remove_color (property_browser, element_name);
		break;
	case NAUTILUS_PROPERTY_EMBLEM:
		remove_emblem (property_browser, element_name);
		break;
	default:
		break;
	}
}

/* here's where we create the emblem dialog */
static GtkWidget*
nautilus_emblem_dialog_new (NautilusPropertyBrowser *property_browser)
{
	GtkWidget *widget, *entry;
	GtkWidget *dialog = gnome_dialog_new(_("Create a New Emblem:"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	GtkWidget *table = gtk_table_new(2, 2, FALSE);

	/* make the keyword label and field */	
	
	widget = gtk_label_new(_("Keyword:"));
	gtk_widget_show(widget);
	gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	
  	property_browser->details->keyword = gtk_entry_new_with_max_length (24);
	gtk_widget_show(property_browser->details->keyword);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->keyword, 1, 2, 0, 1, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

	/* default image is the generic emblem */
	g_free(property_browser->details->image_path);		
	property_browser->details->image_path = gnome_pixmap_file ("gnome-question.png"); 
	
	/* set up a gnome icon entry to pick the image file */
	widget = gtk_label_new(_("Image:"));
	gtk_widget_show(widget);
	gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 1, 2, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

	property_browser->details->file_entry = gnome_icon_entry_new (NULL, _("Select an image file for the new emblem:"));
	gnome_icon_entry_set_pixmap_subdir (GNOME_ICON_ENTRY(property_browser->details->file_entry),
						DATADIR "/pixmaps");
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY(property_browser->details->file_entry),
					property_browser->details->image_path);
	
	gtk_widget_show(property_browser->details->file_entry);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->file_entry, 1, 2, 1, 2, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	
	entry = gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY(property_browser->details->file_entry));
	gtk_entry_set_text(GTK_ENTRY(entry), property_browser->details->image_path);
	
	/* install the table in the dialog */	
	gtk_widget_show(table);	
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), table, TRUE, TRUE, GNOME_PAD);
	gnome_dialog_set_default(GNOME_DIALOG(dialog), GNOME_OK);
	gtk_window_set_wmclass(GTK_WINDOW(dialog), "emblem_dialog", "Nautilus");
	
	return dialog;
}

/* create the color selection dialog */

static GtkWidget*
nautilus_color_selection_dialog_new (NautilusPropertyBrowser *property_browser)
{
	GtkWidget *widget;
	GtkWidget *dialog = gnome_dialog_new(_("Create a New Color:"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	GtkWidget *table = gtk_table_new(2, 2, FALSE);

	/* make the name label and field */	
	
	widget = gtk_label_new(_("Color name:"));
	gtk_widget_show(widget);
	gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
	
  	property_browser->details->color_name = gtk_entry_new_with_max_length (24);
	gtk_widget_show(property_browser->details->color_name);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->color_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

	/* default image is the generic emblem */
	g_free(property_browser->details->image_path);
		
	widget = gtk_label_new(_("Color value:"));
	gtk_widget_show(widget);
	gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 1, 2, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
 
	/* set up a gnome file entry to pick the image file */
	property_browser->details->color_picker = gnome_color_picker_new ();
	gtk_widget_show (property_browser->details->color_picker);
	
	gtk_widget_show(property_browser->details->color_picker);
	gtk_table_attach(GTK_TABLE(table), property_browser->details->color_picker, 1, 2, 1, 2, GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
		
	/* install the table in the dialog */
	
	gtk_widget_show(table);	
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), table, TRUE, TRUE, GNOME_PAD);
	gnome_dialog_set_default(GNOME_DIALOG(dialog), GNOME_OK);
	
	return dialog;
}

/* add the newly selected file to the browser images */
static void
add_pattern_to_browser (const char *path_name, gpointer *data)
{
	char *directory_path, *source_file_name, *destination_name;
	char *path_uri, *basename;
	char *user_directory;	
	char *directory_uri;
	GnomeVFSResult result;
	
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER (data);

	/* make sure that it's a valid path */
	if (path_name == NULL || path_name[0] != '/') {
		char *message;
		if (path_name != NULL) {
			message = g_strdup_printf (_("Sorry, but \"%s\" is not a valid file name."), path_name);
		} else {
			message = g_strdup (_("Sorry, but you did not supply a valid file name."));
		}
		eel_show_error_dialog (message, _("Couldn't install pattern"), GTK_WINDOW (property_browser));
		g_free (message);
		return;
	}
	
	/* fetch the mime type and make sure that the file is an image */
	path_uri = gnome_vfs_get_uri_from_local_path (path_name);	

	/* don't allow the user to change the reset image */
	basename = eel_uri_get_basename (path_uri);
	if (basename && eel_strcmp (basename, RESET_IMAGE_NAME) == 0) {
		eel_show_error_dialog (_("Sorry, but you can't replace the reset image."), _("Not an Image"), NULL);
		g_free (path_uri);
		g_free (basename);
		return;
	}
		
	g_free (path_uri);	
	g_free (basename);
		
	user_directory = nautilus_get_user_directory ();
		
	/* copy the image file to the patterns directory */
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
		
	g_free (directory_path);
		
	result = eel_copy_uri_simple (path_name, destination_name);		
	if (result != GNOME_VFS_OK) {
		char *message = g_strdup_printf (_("Sorry, but the pattern %s couldn't be installed."), path_name);
		eel_show_error_dialog (message, _("Couldn't install pattern"), GTK_WINDOW (property_browser));
		g_free (message);
	}
		
	g_free (destination_name);
	
	/* update the property browser's contents to show the new one */
	nautilus_property_browser_update_contents (property_browser);
}

/* here's where we initiate adding a new pattern by putting up a file selector */

static void
add_new_pattern (NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show(property_browser->details->dialog);
		if (property_browser->details->dialog->window) {
			gdk_window_raise(property_browser->details->dialog->window);
		}
	} else {
		property_browser->details->dialog = 
			eel_gnome_icon_selector_new (_("Select an image file to add as a pattern:"),
				DATADIR "/pixmaps/tiles/",
				GTK_WINDOW (property_browser),
				(EelIconSelectionFunction) add_pattern_to_browser,
				property_browser);						   

		eel_nullify_when_destroyed (&property_browser->details->dialog);		
	}
}

/* here's where we add the passed in color to the file that defines the colors */

static void
add_color_to_file (NautilusPropertyBrowser *property_browser, const char *color_spec, const char *color_name)
{
	xmlNodePtr cur_node, new_color_node;
	xmlDocPtr document;

	document = read_browser_xml (property_browser);
	if (document == NULL) {
		return;
	}

	/* find the colors category */
	cur_node = get_color_category (document);
	if (cur_node != NULL) {
		/* add a new color node */
		new_color_node = xmlNewChild (cur_node, NULL, "color", NULL);
		xmlNodeSetContent (new_color_node, color_spec);
		xmlSetProp (new_color_node, "local", "1");
		xmlSetProp (new_color_node, "name", color_name);
		
		write_browser_xml (property_browser, document);
	}
	
	xmlFreeDoc (document);
}

/* handle the OK button being pushed on the color selection dialog */
static void
add_color_to_browser (GtkWidget *widget, int which_button, gpointer *data)
{
	char *color_spec;
	char *color_name, *stripped_color_name;
	
	gdouble color[4];
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER (data);

	if (which_button == GNOME_OK) {	
		gnome_color_picker_get_d (GNOME_COLOR_PICKER (property_browser->details->color_picker), &color[0], &color[1], &color[2], &color[3]);		
		color_spec = g_strdup_printf
			("rgb:%04hX/%04hX/%04hX",
		 	(gushort) (color[0] * 65535.0 + 0.5),
		 	(gushort) (color[1] * 65535.0 + 0.5),
		 	(gushort) (color[2] * 65535.0 + 0.5));

		color_name = gtk_entry_get_text (GTK_ENTRY (property_browser->details->color_name));
		stripped_color_name = g_strstrip (g_strdup (color_name));
		if (strlen (stripped_color_name) == 0) {
			eel_show_error_dialog (_("Sorry, but you must specify a non-blank name for the new color."), 
						    _("Couldn't install color"), GTK_WINDOW (property_browser));
		
		} else {
			add_color_to_file (property_browser, color_spec, stripped_color_name);
			nautilus_property_browser_update_contents(property_browser);
		}
		g_free (stripped_color_name);
		g_free(color_spec);	
	} 
	
	gtk_widget_destroy(property_browser->details->dialog);
	property_browser->details->dialog = NULL;
}

/* create the color selection dialog, pre-set with the color that was just selected */
static void
show_color_selection_window (GtkWidget *widget, gpointer *data)
{
	gdouble color[4];
	NautilusPropertyBrowser *property_browser = NAUTILUS_PROPERTY_BROWSER(data);

	gtk_color_selection_get_color (GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (property_browser->details->dialog)->colorsel), color);
	gtk_widget_destroy (property_browser->details->dialog);

	/* allocate a new color selection dialog */
	property_browser->details->dialog = nautilus_color_selection_dialog_new (property_browser);		

	/* set the color to the one picked by the selector */
	gnome_color_picker_set_d (GNOME_COLOR_PICKER (property_browser->details->color_picker), color[0], color[1], color[2], 1.0);
	
	/* connect the signals to the new dialog */
	
	eel_nullify_when_destroyed (&property_browser->details->dialog);

	gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				 "clicked",
				 (GtkSignalFunc) add_color_to_browser, property_browser);
	gtk_window_set_position (GTK_WINDOW (property_browser->details->dialog), GTK_WIN_POS_MOUSE);
	gtk_widget_show (GTK_WIDGET(property_browser->details->dialog));

}


/* here's the routine to add a new color, by putting up a color selector */

static void
add_new_color (NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show(property_browser->details->dialog);
		if (property_browser->details->dialog->window)
			gdk_window_raise(property_browser->details->dialog->window);

	} else {
		GtkColorSelectionDialog *color_dialog;

		property_browser->details->dialog = gtk_color_selection_dialog_new (_("Select a color to add:"));
		color_dialog = GTK_COLOR_SELECTION_DIALOG (property_browser->details->dialog);
		
		eel_nullify_when_destroyed (&property_browser->details->dialog);

		gtk_signal_connect (GTK_OBJECT (color_dialog->ok_button),
				    "clicked",
				    (GtkSignalFunc) show_color_selection_window, property_browser);
		gtk_signal_connect_object (GTK_OBJECT (color_dialog->cancel_button),
					   "clicked",
					   (GtkSignalFunc) gtk_widget_destroy,
					   GTK_OBJECT (color_dialog));
		gtk_widget_hide(color_dialog->help_button);

		gtk_window_set_position (GTK_WINDOW (color_dialog), GTK_WIN_POS_MOUSE);
		gtk_widget_show (GTK_WIDGET(color_dialog));
	}
}

/* utility to make sure the passed-in keyword only contains alphanumeric characters */
static gboolean
emblem_keyword_valid (const char *keyword)
{
	int index, keyword_length;

	keyword_length = strlen (keyword);
	for (index = 0; index < keyword_length; index++) {
		if (!isalnum ((guchar) keyword[index])
		    && !isspace ((guchar) keyword[index])) {
			return FALSE;
		}
	}
	
	return TRUE;
}


/* check for reserved keywords */
static gboolean
is_reserved_keyword (NautilusPropertyBrowser *property_browser, const char *keyword)
{	
	/* check intrinsic emblems */
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_TRASH) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_CANT_READ) == 0) {
		return TRUE;
	}
	if (eel_strcasecmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE) == 0) {
		return TRUE;
	}
	
	/* see if the keyword already exists */
	return g_list_find_custom (property_browser->details->keywords,
				   (char *) keyword,
				   (GCompareFunc) eel_strcasecmp) != NULL;				
}

/* here's where we handle clicks in the emblem dialog buttons */
static void
emblem_dialog_clicked (GtkWidget *dialog, int which_button, NautilusPropertyBrowser *property_browser)
{
	char *directory_uri, *error_string;
	GnomeVFSResult result;
	
	if (which_button == GNOME_OK) {
		char *destination_name, *extension;
		char *new_keyword, *stripped_keyword;
		char *emblem_path, *emblem_uri;
		char *user_directory;	
		char *directory_path;

		/* update the image path from the file entry */
		if (property_browser->details->file_entry) {
			emblem_path = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (property_browser->details->file_entry));
			if (emblem_path) {
				emblem_uri = gnome_vfs_get_uri_from_local_path (emblem_path);
				if (ensure_uri_is_image (emblem_uri)) {
					g_free (property_browser->details->image_path);
					property_browser->details->image_path = emblem_path;				
				} else {
					char *message = g_strdup_printf
						(_("Sorry, but '%s' is not a usable image file!"), emblem_path);
					eel_show_error_dialog (message, _("Not an Image"), GTK_WINDOW (property_browser));
					g_free (message);
					g_free (emblem_path);
					return;
				}
				g_free (emblem_uri);
			}
		}
		
		new_keyword = gtk_entry_get_text(GTK_ENTRY(property_browser->details->keyword));		
		if (new_keyword == NULL) {
			stripped_keyword = NULL;
		} else {
			stripped_keyword = g_strstrip (g_strdup (new_keyword));
		}
		
		if (stripped_keyword == NULL || strlen (stripped_keyword) == 0) {
			eel_show_error_dialog (_("Sorry, but you must specify a non-blank keyword for the new emblem."), 
						    _("Couldn't install emblem"), GTK_WINDOW (property_browser));
		} else if (!emblem_keyword_valid (stripped_keyword)) {
			eel_show_error_dialog (_("Sorry, but emblem keywords can only contain letters, spaces and numbers."), 
						    _("Couldn't install emblem"), GTK_WINDOW (property_browser));
		} else if (is_reserved_keyword (property_browser, stripped_keyword)) {
			error_string = g_strdup_printf (_("Sorry, but \"%s\" is an existing keyword.  Please choose a different name for it."), stripped_keyword);
			eel_show_error_dialog (error_string, 
						    _("Couldn't install emblem"), GTK_WINDOW (property_browser));
			g_free (error_string);
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
			destination_name = g_strdup_printf("%s/%s.%s", directory_path, stripped_keyword, extension + 1);
			g_free(directory_path);
				
			/* perform the actual copy */
			result = eel_copy_uri_simple (property_browser->details->image_path, destination_name);		
		
			if (result != GNOME_VFS_OK) {
				char *message = g_strdup_printf (_("Sorry, but the image at %s couldn't be installed as an emblem."), property_browser->details->image_path);
				eel_show_error_dialog (message, _("Couldn't install emblem"), GTK_WINDOW (property_browser));
				g_free (message);
			} else {
				emit_emblems_changed_signal ();	
			}
			
			g_free(destination_name);
				
			nautilus_property_browser_update_contents(property_browser);
		}
		g_free (stripped_keyword);
	}
	
	gtk_widget_destroy(dialog);
	
	property_browser->details->keyword = NULL;
	property_browser->details->emblem_image = NULL;
	property_browser->details->file_entry = NULL;
}

/* here's the routine to add a new emblem, by putting up an emblem dialog */

static void
add_new_emblem (NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->dialog) {
		gtk_widget_show (property_browser->details->dialog);
		if (property_browser->details->dialog->window) {
			gdk_window_raise (property_browser->details->dialog->window);
		}
	} else {
		property_browser->details->dialog = nautilus_emblem_dialog_new (property_browser);		

		eel_nullify_when_destroyed (&property_browser->details->dialog);

		gtk_signal_connect (GTK_OBJECT (property_browser->details->dialog),
				    "clicked",
				    (GtkSignalFunc) emblem_dialog_clicked, property_browser);
		gtk_window_set_position (GTK_WINDOW (property_browser->details->dialog), GTK_WIN_POS_MOUSE);
		gtk_widget_show (GTK_WIDGET(property_browser->details->dialog));
	}
}

/* cancelremove mode */
static void
cancel_remove_mode (NautilusPropertyBrowser *property_browser)
{
	if (property_browser->details->remove_mode) {
		property_browser->details->remove_mode = FALSE;
		nautilus_property_browser_update_contents(property_browser);
		gtk_widget_show (property_browser->details->help_label);
	}
}

/* handle the add_new button */

static void
add_new_button_callback(GtkWidget *widget, NautilusPropertyBrowser *property_browser)
{
	/* handle remove mode, where we act as a cancel button */
	if (property_browser->details->remove_mode) {
		cancel_remove_mode (property_browser);
		return;
	}

	switch (property_browser->details->category_type) {
		case NAUTILUS_PROPERTY_PATTERN:
			add_new_pattern (property_browser);
			break;
		case NAUTILUS_PROPERTY_COLOR:
			add_new_color (property_browser);
			break;
		case NAUTILUS_PROPERTY_EMBLEM:
			add_new_emblem (property_browser);
			break;
		default:
			break;
	}	
}

/* handle the "done" button */
static void
done_button_callback (GtkWidget *widget, GtkWidget *property_browser)
{
	cancel_remove_mode (NAUTILUS_PROPERTY_BROWSER (property_browser));
	gtk_widget_hide (property_browser);
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
element_clicked_callback (GtkWidget *image_table,
			  GtkWidget *child,
			  const EelImageTableEvent *event,
			  gpointer callback_data)
{
	NautilusPropertyBrowser *property_browser;
	GtkTargetList *target_list;	
	GdkDragContext *context;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap_for_dragged_file;
	GdkBitmap *mask_for_dragged_file;
	int x_delta, y_delta;
	const char *element_name;
	EelArtIPoint scroll_offset;

	g_return_if_fail (EEL_IS_IMAGE_TABLE (image_table));
	g_return_if_fail (EEL_IS_LABELED_IMAGE (child));
	g_return_if_fail (event != NULL);
	g_return_if_fail (NAUTILUS_IS_PROPERTY_BROWSER (callback_data));
	g_return_if_fail (gtk_object_get_data (GTK_OBJECT (child), "property-name") != NULL);

	element_name = gtk_object_get_data (GTK_OBJECT (child), "property-name");
	property_browser = NAUTILUS_PROPERTY_BROWSER (callback_data);

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
	
	/* treat the reset property in the colors section specially */	
	if (eel_strcmp (element_name, RESET_IMAGE_NAME) == 0) {
		drag_types[0].target = "x-special/gnome-reset-background";	
	}
	
	target_list = gtk_target_list_new (drag_types, EEL_N_ELEMENTS (drag_types));	
	nautilus_property_browser_set_dragged_file(property_browser, element_name);
	
	context = gtk_drag_begin (GTK_WIDGET (property_browser),
				  target_list,
				  GDK_ACTION_MOVE | GDK_ACTION_COPY,
				  event->button,
				  NULL);

	/* compute the offsets for dragging */
	scroll_offset = eel_viewport_get_scroll_offset (EEL_VIEWPORT (image_table->parent));

	if (strcmp(drag_types[0].target, "application/x-color")) {
		/*it's not a color, so, for now, it must be an image */
		/* fiddle with the category to handle the "reset" case properly */
		char * save_category = property_browser->details->category;
		if (eel_strcmp (property_browser->details->category, "colors") == 0) {
			property_browser->details->category = "patterns";
		}
		pixbuf = make_drag_image (property_browser, element_name);
		property_browser->details->category = save_category;
		x_delta = gdk_pixbuf_get_width (pixbuf) / 2;
		y_delta = gdk_pixbuf_get_height (pixbuf) / 2;
	} else {
		pixbuf = make_color_drag_image (property_browser, element_name, TRUE);
		x_delta = event->x - child->allocation.x - scroll_offset.x;
		y_delta = event->y - child->allocation.y - scroll_offset.y;
	}

        /* set the pixmap and mask for dragging */       
	if (pixbuf != NULL) {
		gdk_pixbuf_render_pixmap_and_mask
			(pixbuf,
			 &pixmap_for_dragged_file,
			 &mask_for_dragged_file,
			 EEL_STANDARD_ALPHA_THRESHHOLD);

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
	nautilus_property_browser_update_contents (property_browser);
}

static void
labeled_image_configure (EelLabeledImage *labeled_image)
{
	g_return_if_fail (EEL_IS_LABELED_IMAGE (labeled_image));

	eel_labeled_image_set_background_mode (labeled_image,
						    EEL_SMOOTH_BACKGROUND_SOLID_COLOR);
	eel_labeled_image_set_solid_background_color (labeled_image,
							   EEL_RGB_COLOR_WHITE);		
	eel_labeled_image_set_spacing (labeled_image, LABELED_IMAGE_SPACING);
}

/* Make a color tile for a property */
static GtkWidget *
labeled_image_new (const char *text,
		   GdkPixbuf *pixbuf,
		   const char *property_name,
		   guint num_smaller)
{
	GtkWidget *labeled_image;
	
	labeled_image = eel_labeled_image_new (text, pixbuf);
	labeled_image_configure (EEL_LABELED_IMAGE (labeled_image));
	if (num_smaller > 0) {
		eel_labeled_image_make_smaller (EEL_LABELED_IMAGE (labeled_image),
						     num_smaller);
	}

	if (property_name != NULL) {
		gtk_object_set_data_full (GTK_OBJECT (labeled_image),
					  "property-name",
					  g_strdup (property_name),
					  (GtkDestroyNotify) g_free);
	}

	return labeled_image;
}

static void
make_properties_from_directories (NautilusPropertyBrowser *property_browser)
{
	NautilusCustomizationData *customization_data;
	char *object_name;
	char *object_label;
	GdkPixbuf *object_pixbuf;
	EelImageTable *image_table;
	GtkWidget *reset_object = NULL;
	GtkWidget *erase_object = NULL;

	g_return_if_fail (NAUTILUS_IS_PROPERTY_BROWSER (property_browser));
	g_return_if_fail (EEL_IS_IMAGE_TABLE (property_browser->details->content_table));
	
	image_table = EEL_IMAGE_TABLE (property_browser->details->content_table);
	
	if (property_browser->details->category_type == NAUTILUS_PROPERTY_EMBLEM) {
		eel_g_list_free_deep (property_browser->details->keywords);	
		property_browser->details->keywords = NULL;
	}
	
	customization_data = nautilus_customization_data_new (property_browser->details->category,
							      !property_browser->details->remove_mode,
							      FALSE,
							      MAX_ICON_WIDTH,
							      MAX_ICON_HEIGHT);
	if (customization_data == NULL) {
		return;
	}

	/* interate through the set of objects and display each */
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &object_name,
									 &object_pixbuf,
									 &object_label) == GNOME_VFS_OK) {
		GtkWidget *property_image;

		property_image = labeled_image_new (object_label, object_pixbuf, object_name, 2);
		
		if (property_browser->details->category_type == NAUTILUS_PROPERTY_EMBLEM) {		
			char *keyword;
			char *extension;
			
			keyword = g_strdup (object_name);
			extension = strchr (keyword, '.');

			eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (property_image), MAX_EMBLEM_HEIGHT);

			if (extension) {
				*extension = '\0';
			}

			property_browser->details->keywords = g_list_prepend (property_browser->details->keywords,
									      keyword);
		}
		
		gtk_container_add (GTK_CONTAINER (image_table), property_image);
		gtk_widget_show (property_image);

		/* Keep track of ERASE objects to place them prominently later */
		if (property_browser->details->category_type == NAUTILUS_PROPERTY_EMBLEM
		    && eel_str_is_equal (object_name, ERASE_OBJECT_NAME)) {
			g_assert (erase_object == NULL);
			erase_object = property_image;
		/* Keep track of RESET objects to place them prominently later */
		} else if (property_browser->details->category_type == NAUTILUS_PROPERTY_PATTERN
			   && eel_str_is_equal (object_name, RESET_IMAGE_NAME)) {
			g_assert (reset_object == NULL);
			reset_object = property_image;
		}
		
		gtk_widget_show (property_image);

		g_free (object_name);
		g_free (object_label);
		gdk_pixbuf_unref (object_pixbuf);
	}

	/*
	 * We place ERASE objects (for emblems) at the end with a blank in between.
	 */
	if (erase_object != NULL) {
		GtkWidget *blank;
		guint num_images;

		g_assert (EEL_IS_LABELED_IMAGE (erase_object));

		blank = eel_image_table_add_empty_image (image_table);
		labeled_image_configure (EEL_LABELED_IMAGE (blank));
		
		num_images = eel_wrap_table_get_num_children (EEL_WRAP_TABLE (image_table));
		g_assert (num_images > 0);
		eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
						   blank,
						   num_images - 1);
		
		gtk_widget_show (blank);
		eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
						   erase_object,
						   -1);
	}

	/*
	 * We place RESET objects (for colors and patterns) at the beginning.
	 */
	if (reset_object != NULL) {
		g_assert (EEL_IS_LABELED_IMAGE (reset_object));
		eel_wrap_table_reorder_child (EEL_WRAP_TABLE (image_table),
						   reset_object,
						   0);
	}

	property_browser->details->has_local = nautilus_customization_data_private_data_was_displayed (customization_data);	
	nautilus_customization_data_destroy (customization_data);
}

/* utility routine to add a reset property in the first position */
static void
add_reset_property (NautilusPropertyBrowser *property_browser)
{
	char *reset_image_file_name;
	GtkWidget *reset_image;
	GdkPixbuf *reset_pixbuf, *reset_chit;

	reset_image_file_name = g_strdup_printf ("%s/%s/%s", NAUTILUS_DATADIR, "patterns", RESET_IMAGE_NAME);
	reset_pixbuf = gdk_pixbuf_new_from_file (reset_image_file_name);
	reset_chit = nautilus_customization_make_pattern_chit (reset_pixbuf, property_browser->details->property_chit, FALSE, TRUE);
	
	g_free (reset_image_file_name);

	reset_image = labeled_image_new (NULL, reset_chit, RESET_IMAGE_NAME, 0);
	gtk_container_add (GTK_CONTAINER (property_browser->details->content_table), reset_image);
	eel_wrap_table_reorder_child (EEL_WRAP_TABLE (property_browser->details->content_table),
					   reset_image,
					   0);
	gtk_widget_show (reset_image);
}
	
/* generate properties from the children of the passed in node */
/* for now, we just handle color nodes */

static void
make_properties_from_xml_node (NautilusPropertyBrowser *property_browser,
			       xmlNodePtr node)
{
	xmlNodePtr child_node;
	GdkPixbuf *pixbuf;
	GtkWidget *new_property;
	char *deleted, *local, *color, *name;
	
	gboolean local_only = property_browser->details->remove_mode;
	
	/* add a reset property in the first slot */
	if (!property_browser->details->remove_mode) {
		add_reset_property (property_browser);
	}
	
	property_browser->details->has_local = FALSE;
	
	for (child_node = eel_xml_get_children (node);
	     child_node != NULL;
	     child_node = child_node->next) {
		deleted = xmlGetProp (child_node, "deleted");
		local = xmlGetProp (child_node, "local");
		
		if (deleted == NULL && (!local_only || local != NULL)) {
			if (local != NULL) {
				property_browser->details->has_local = TRUE;
			}
			
			color = xmlNodeGetContent (child_node);
			name = eel_xml_get_property_translated (child_node, "name");
			
			/* make the image from the color spec */
			pixbuf = make_color_drag_image (property_browser, color, FALSE);			

			/* make the tile from the pixmap and name */
			new_property = labeled_image_new (name, pixbuf, color, 2);

			gtk_container_add (GTK_CONTAINER (property_browser->details->content_table), new_property);
			gtk_widget_show (new_property);

			gdk_pixbuf_unref (pixbuf);
			xmlFree (color);
			xmlFree (name);
		}

		xmlFree (local);
		xmlFree (deleted);
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
	eel_label_set_text (EEL_LABEL (property_browser->details->help_label), description);
	
	/* case out on the mode */
	if (strcmp(mode, "directory") == 0)
		make_properties_from_directories (property_browser);
	else if (strcmp(mode, "inline") == 0)
		make_properties_from_xml_node (property_browser, node);

}

/* Create a category button */
static GtkWidget *
property_browser_category_button_new (const char *display_name,
				      const char *image)
{
	GtkWidget *button;
	char *file_name;

	g_return_val_if_fail (display_name != NULL, NULL);
	g_return_val_if_fail (image != NULL, NULL);

	file_name = nautilus_pixmap_file (image); 
	g_return_val_if_fail (file_name != NULL, NULL);

	button = eel_labeled_image_toggle_button_new_from_file_name (display_name,
									  file_name);

	/* We want the label to never be smooth */
	eel_labeled_image_set_label_never_smooth (EEL_LABELED_IMAGE (GTK_BIN (button)->child), TRUE);

	/* We also want all of the buttons to be the same height */
	eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (GTK_BIN (button)->child), STANDARD_BUTTON_IMAGE_HEIGHT);

	g_free (file_name);

	return button;
}

/* this is a utility routine to generate a category link widget and install it in the browser */
static void
make_category_link (NautilusPropertyBrowser *property_browser,
		    const char *name,
		    const char *display_name,
		    const char *image)
{
	GtkWidget *button;

	g_return_if_fail (NAUTILUS_IS_PROPERTY_BROWSER (property_browser));
	g_return_if_fail (name != NULL);
	g_return_if_fail (display_name != NULL);
	g_return_if_fail (image != NULL);

	button = property_browser_category_button_new (display_name, image);
	gtk_widget_show (button);
	
	/* if the button represents the current category, highlight it */	
	if (property_browser->details->category && !strcmp(property_browser->details->category, name)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		property_browser->details->selected_button = button;		
	}

	/* Place it in the category box */
	gtk_box_pack_start (GTK_BOX (property_browser->details->category_box),
			    button, FALSE, FALSE, 8);
	
	property_browser->details->category_position += 1;
	
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
}

/* update_contents populates the property browser with information specified by the path and other state variables */
void
nautilus_property_browser_update_contents (NautilusPropertyBrowser *property_browser)
{
	xmlNodePtr cur_node;
 	xmlDocPtr document;
 	EelBackground *background;
	GtkWidget *viewport;
	gboolean show_buttons, got_categories;
	char *name, *image, *type, *description, *display_name, *path, *mode;
	const char *text;

	/* load the xml document corresponding to the path and selection */
	document = read_browser_xml (property_browser);
	if (document == NULL) {
		return;
	}
		
	/* remove the existing content box, if any, and allocate a new one */
	if (property_browser->details->content_frame) {
		gtk_widget_destroy(property_browser->details->content_frame);
	}
	
	/* allocate a new container, with a scrollwindow and viewport */
	property_browser->details->content_frame = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (property_browser->details->content_frame), 0);				
 	viewport = eel_viewport_new (NULL, NULL);
	eel_viewport_set_never_smooth (EEL_VIEWPORT (viewport), FALSE);
	gtk_widget_show(viewport);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_IN);
	background = eel_get_widget_background (viewport);
	eel_background_set_color (background, BROWSER_BACKGROUND_COLOR);	
	gtk_container_add (GTK_CONTAINER (property_browser->details->content_container), property_browser->details->content_frame);
	gtk_widget_show (property_browser->details->content_frame);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (property_browser->details->content_frame), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	/* allocate a table to hold the content widgets */
  	property_browser->details->content_table = eel_image_table_new (TRUE);
	eel_wrap_table_set_x_spacing (EEL_WRAP_TABLE (property_browser->details->content_table),
					   IMAGE_TABLE_X_SPACING);
	eel_wrap_table_set_y_spacing (EEL_WRAP_TABLE (property_browser->details->content_table),
					   IMAGE_TABLE_Y_SPACING);
	
	gtk_signal_connect (GTK_OBJECT (property_browser->details->content_table),
			    "child_pressed", 
			    GTK_SIGNAL_FUNC (element_clicked_callback),
			    property_browser);

	gtk_container_add(GTK_CONTAINER(viewport), property_browser->details->content_table); 
	gtk_container_add (GTK_CONTAINER (property_browser->details->content_frame), viewport);
	gtk_widget_show (GTK_WIDGET (property_browser->details->content_table));
	
	/* iterate through the xml file to generate the widgets */
	got_categories = property_browser->details->category_position >= 0;
	if (!got_categories) {
		property_browser->details->category_position = 0;
	}
	
	for (cur_node = eel_xml_get_children (xmlDocGetRootElement (document));
	     cur_node != NULL;
	     cur_node = cur_node->next) {
		if (strcmp (cur_node->name, "category") == 0) {
			name = xmlGetProp (cur_node, "name");
			
			if (property_browser->details->category != NULL
			    && strcmp (property_browser->details->category, name) == 0) {
				path = xmlGetProp (cur_node, "path");
				mode = xmlGetProp (cur_node, "mode");
				description = eel_xml_get_property_translated (cur_node, "description");
				type = xmlGetProp (cur_node, "type");
				
				make_category (property_browser,
					       path,
					       mode,
					       cur_node,
					       description);
				nautilus_property_browser_set_drag_type (property_browser, type);
				
				xmlFree (path);
				xmlFree (mode);
				xmlFree (description);
				xmlFree (type);
			}
			
			if (!got_categories) {
				display_name = eel_xml_get_property_translated (cur_node, "display_name");
				image = xmlGetProp (cur_node, "image");

				make_category_link (property_browser,
						    name,
						    display_name,
						    image);
				
				xmlFree (display_name);
				xmlFree (image);
			}
			
			xmlFree (name);
		}
	}
	
	/* release the  xml document and we're done */
	xmlFreeDoc (document);

	/* update the title and button */

	show_buttons = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_CAN_ADD_CONTENT);

	if (property_browser->details->category == NULL) {
		eel_label_set_text(EEL_LABEL (property_browser->details->title_label), _("Select A Category:"));
		gtk_widget_hide(property_browser->details->add_button);
		gtk_widget_hide(property_browser->details->remove_button);
	
	} else {
		char *label_text;
				
		if (property_browser->details->remove_mode) {
			text = _("Cancel Remove");
		} else {
			/* FIXME: Using spaces to add padding is not good design. */
			switch (property_browser->details->category_type) {
			case NAUTILUS_PROPERTY_PATTERN:
				text = _("Add a New Pattern...");
				break;
			case NAUTILUS_PROPERTY_COLOR:
				text = _("Add a New Color...");
				break;
			case NAUTILUS_PROPERTY_EMBLEM:
				text = _("Add a New Emblem...");
				break;
			default:
				text = NULL;
				break;
			}		
		}
		
		/* enable the "add new" button and update it's name */		
		
		if (text != NULL) {
			gtk_label_set (GTK_LABEL(property_browser->details->add_button_label), text);
		}
		if (show_buttons) {
			gtk_widget_show (property_browser->details->add_button);
		} else {
			gtk_widget_hide (property_browser->details->add_button);
		}
			
		if (property_browser->details->remove_mode) {

			switch (property_browser->details->category_type) {
			case NAUTILUS_PROPERTY_PATTERN:
				label_text = g_strdup (_("Click on a pattern to remove it"));
				break;
			case NAUTILUS_PROPERTY_COLOR:
				label_text = g_strdup (_("Click on a color to remove it"));
				break;
			case NAUTILUS_PROPERTY_EMBLEM:
				label_text = g_strdup (_("Click on an emblem to remove it"));
				break;
			default:
				label_text = NULL;
				break;
			}
		} else {	
			switch (property_browser->details->category_type) {
			case NAUTILUS_PROPERTY_PATTERN:
				label_text = g_strdup (_("Patterns:"));
				break;
			case NAUTILUS_PROPERTY_COLOR:
				label_text = g_strdup (_("Colors:"));
				break;
			case NAUTILUS_PROPERTY_EMBLEM:
				label_text = g_strdup (_("Emblems:"));
				break;
			default:
				label_text = NULL;
				break;
			}
		}
		
		if (label_text) {
			eel_label_set_text (EEL_LABEL (property_browser->details->title_label), label_text);
		}
		g_free(label_text);

		/* enable the remove button (if necessary) and update its name */
		
		/* case out instead of substituting to provide flexibilty for other languages */
		/* FIXME: Using spaces to add padding is not good design. */
		switch (property_browser->details->category_type) {
		case NAUTILUS_PROPERTY_PATTERN:
			text = _("Remove a Pattern...");
			break;
		case NAUTILUS_PROPERTY_COLOR:
			text = _("Remove a Color...");
			break;
		case NAUTILUS_PROPERTY_EMBLEM:
			text = _("Remove an Emblem...");
			break;
		default:
			text = NULL;
			break;
		}
		
		if (!show_buttons
		    || property_browser->details->remove_mode
		    || !property_browser->details->has_local)
			gtk_widget_hide(property_browser->details->remove_button);
		else
			gtk_widget_show(property_browser->details->remove_button);
		if (text != NULL) {
			gtk_label_set (GTK_LABEL(property_browser->details->remove_button_label), text);
		}
	}
}

/* set the category and regenerate contents as necessary */

static void
nautilus_property_browser_set_category (NautilusPropertyBrowser *property_browser,
					const char *new_category)
{       
	/* there's nothing to do if the category is the same as the current one */ 
	if (eel_strcmp (property_browser->details->category, new_category) == 0) {
		return;
	}
	
	g_free (property_browser->details->category);
	property_browser->details->category = g_strdup (new_category);
	
	/* set up the property type enum */
	if (eel_strcmp (new_category, "patterns") == 0) {
		property_browser->details->category_type = NAUTILUS_PROPERTY_PATTERN;
	} else if (eel_strcmp (new_category, "colors") == 0) {	
		property_browser->details->category_type = NAUTILUS_PROPERTY_COLOR;
	} else if (eel_strcmp (new_category, "emblems") == 0) {	
		property_browser->details->category_type = NAUTILUS_PROPERTY_EMBLEM;
	} else {
		property_browser->details->category_type = NAUTILUS_PROPERTY_NONE;
	}
	
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
	if (eel_strcmp (property_browser->details->path, new_path) == 0) {
		return;
	}
	
	g_free (property_browser->details->path);
	property_browser->details->path = g_strdup (new_path);
	
	/* populate the per-uri box with the info */
	nautilus_property_browser_update_contents (property_browser);  	
}

static void
emit_emblems_changed_signal (void)
{
	gtk_signal_emit_by_name (nautilus_signaller_get_current (), "emblems_changed");
}
