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

/* This is the implementation of the theme selector window, which
 * displays the available user interface themes and allows the user
 * to pick one.
 */

#include <config.h>
#include <math.h>
#include <ctype.h>

#include "nautilus-theme-selector.h"

#include <parser.h>
#include <xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>

#include <libnautilus-extensions/nautilus-background.h>
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
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>

struct NautilusThemeSelectorDetails {
	GtkWidget *container;
	GtkWidget *content_frame;
	GtkWidget *content_list;
		
	GtkWidget *title_box;
	GtkWidget *title_label;
	GtkWidget *help_label;
	
	GtkWidget *theme_list;
	GtkWidget *bottom_box;
	
	GtkWidget *add_button;
	GtkWidget *add_button_label;	
	GtkWidget *remove_button;
	GtkWidget *remove_button_label;

	GdkColor main_row_color;
	GdkColor alt_row_color;
	
	GtkWidget *dialog;
	int selected_row;
	gboolean handling_theme_change;
};

static void  nautilus_theme_selector_initialize_class (GtkObjectClass          *object_klass);
static void  nautilus_theme_selector_initialize       (GtkObject               *object);
static void  nautilus_theme_selector_destroy          (GtkObject               *object);

static void  add_new_button_callback                  (GtkWidget               *widget,
						       NautilusThemeSelector *theme_selector);
static void  remove_button_callback                   (GtkWidget               *widget,
						       NautilusThemeSelector *theme_selector);
static gboolean nautilus_theme_selector_delete_event_callback (GtkWidget *widget,
							GdkEvent  *event,
							gpointer   user_data);

static void  nautilus_theme_selector_theme_changed	(gpointer user_data);
static void  populate_list_with_themes 			(NautilusThemeSelector *theme_selector);

static void  theme_select_row_callback (GtkCList * clist, int row, int column, GdkEventButton *event, NautilusThemeSelector *theme_selector); 

#define THEME_SELECTOR_WIDTH  460
#define THEME_SELECTOR_HEIGHT 264

#define SELECTOR_BACKGROUND_COLOR "rgb:FFFF/FFFF/FFFF"

static NautilusThemeSelector *main_theme_selector = NULL;

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusThemeSelector, nautilus_theme_selector, GTK_TYPE_WINDOW)

/* initializing the class object by installing the operations we override */
static void
nautilus_theme_selector_initialize_class (GtkObjectClass *object_klass)
{
	NautilusThemeSelectorClass *klass;
	/* GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (object_klass); */

	klass = NAUTILUS_THEME_SELECTOR_CLASS (object_klass);
	object_klass->destroy = nautilus_theme_selector_destroy;
}


/* initialize the instance's fields, create the necessary subviews, etc. */

static void
nautilus_theme_selector_initialize (GtkObject *object)
{
	NautilusBackground *background;
 	NautilusThemeSelector *theme_selector;
 	GtkWidget* widget, *temp_box, *temp_hbox, *temp_frame;
	GtkWidget *scrollwindow;
	GdkFont *font;
	
	theme_selector = NAUTILUS_THEME_SELECTOR (object);
	widget = GTK_WIDGET (object);

	theme_selector->details = g_new0 (NautilusThemeSelectorDetails, 1);
	
	/* set the initial size of the window */
	gtk_widget_set_usize (widget, THEME_SELECTOR_WIDTH, THEME_SELECTOR_HEIGHT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);				

	/* set the title */
	gtk_window_set_title (GTK_WINDOW(widget), _("Nautilus Theme Selector"));
	
	/* set up the background */	
	background = nautilus_get_widget_background (GTK_WIDGET (theme_selector));
	nautilus_background_set_color (background, SELECTOR_BACKGROUND_COLOR);	
	
	/* create the container box */  
  	theme_selector->details->container =  gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (theme_selector->details->container), 0);				
	gtk_widget_show (GTK_WIDGET (theme_selector->details->container));
	gtk_container_add (GTK_CONTAINER (theme_selector),
			   GTK_WIDGET (theme_selector->details->container));	
	
  	/* create the title box */
  	
  	theme_selector->details->title_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (theme_selector->details->title_box), 0);				
 
  	gtk_widget_show(theme_selector->details->title_box);
	gtk_box_pack_start (GTK_BOX(theme_selector->details->container), theme_selector->details->title_box, FALSE, FALSE, 0);
  
  	
  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_OUT);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(theme_selector->details->title_box), temp_frame);
  	
  	temp_hbox = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(temp_hbox);
  	gtk_container_add(GTK_CONTAINER(temp_frame), temp_hbox);
 	
	/* add the title label */
	theme_selector->details->title_label = gtk_label_new  (_("Nautilus Theme:"));

        font = nautilus_font_factory_get_font_from_preferences (18);
	nautilus_gtk_widget_set_font(theme_selector->details->title_label, font);
        gdk_font_unref (font);

  	gtk_widget_show(theme_selector->details->title_label);
	gtk_box_pack_start (GTK_BOX(temp_hbox), theme_selector->details->title_label, FALSE, FALSE, 8);
 
 	/* add the help label */
	theme_selector->details->help_label = gtk_label_new  (_("Click on a theme to change the\nappearance of Nautilus."));
  	gtk_widget_show(theme_selector->details->help_label);
	gtk_box_pack_end (GTK_BOX(temp_hbox), theme_selector->details->help_label, FALSE, FALSE, 8);
 
 	/* add the main part of the content, which is a list view, embedded in a scrollwindow */
	
	theme_selector->details->theme_list = gtk_clist_new (3);
	gtk_clist_set_row_height   (GTK_CLIST (theme_selector->details->theme_list), 48);
	gtk_clist_set_column_width (GTK_CLIST(theme_selector->details->theme_list), 0, 72);
	gtk_clist_set_column_width (GTK_CLIST(theme_selector->details->theme_list), 1, 80);
	gtk_clist_set_column_width (GTK_CLIST(theme_selector->details->theme_list), 2, 180);
	
	gtk_clist_set_shadow_type  (GTK_CLIST (theme_selector->details->theme_list), GTK_SHADOW_NONE);
			
	scrollwindow = gtk_scrolled_window_new (NULL, gtk_clist_get_vadjustment (GTK_CLIST (theme_selector->details->theme_list)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrollwindow), theme_selector->details->theme_list);	
	gtk_clist_set_selection_mode (GTK_CLIST (theme_selector->details->theme_list), GTK_SELECTION_BROWSE);

	gtk_box_pack_start (GTK_BOX (theme_selector->details->container), scrollwindow, TRUE, TRUE, 0);	
	gtk_widget_show (theme_selector->details->theme_list);
	gtk_widget_show (scrollwindow);

	populate_list_with_themes (theme_selector);
	
	/* connect a signal to let us know when the column titles are clicked */
	gtk_signal_connect(GTK_OBJECT(theme_selector->details->theme_list), "select_row",
				GTK_SIGNAL_FUNC(theme_select_row_callback), theme_selector);

  	/* add the bottom box to hold the command buttons */
  	temp_box = gtk_event_box_new();
	gtk_container_set_border_width (GTK_CONTAINER (temp_box), 0);				
  	gtk_widget_show(temp_box);

  	temp_frame = gtk_frame_new(NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME(temp_frame), GTK_SHADOW_IN);
  	gtk_widget_show(temp_frame);
  	gtk_container_add(GTK_CONTAINER(temp_box), temp_frame);

  	theme_selector->details->bottom_box = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(theme_selector->details->bottom_box);
	gtk_box_pack_end (GTK_BOX(theme_selector->details->container), temp_box, FALSE, FALSE, 0);
  	gtk_container_add (GTK_CONTAINER (temp_frame), theme_selector->details->bottom_box);
  	
  	/* create the "add new" button */
  	theme_selector->details->add_button = gtk_button_new ();
	gtk_widget_show(theme_selector->details->add_button);
	
	theme_selector->details->add_button_label = gtk_label_new (_("Add new theme"));
	gtk_widget_show(theme_selector->details->add_button_label);
	gtk_container_add (GTK_CONTAINER(theme_selector->details->add_button), theme_selector->details->add_button_label);
	gtk_box_pack_end (GTK_BOX(theme_selector->details->bottom_box), theme_selector->details->add_button, FALSE, FALSE, 4);
 	  
 	gtk_signal_connect(GTK_OBJECT (theme_selector->details->add_button), "clicked", GTK_SIGNAL_FUNC (add_new_button_callback), theme_selector);
	
	/* now create the "remove" button */
  	theme_selector->details->remove_button = gtk_button_new();
	
	theme_selector->details->remove_button_label = gtk_label_new (_("Remove theme"));
	gtk_widget_show(theme_selector->details->remove_button_label);
	gtk_container_add (GTK_CONTAINER(theme_selector->details->remove_button), theme_selector->details->remove_button_label);
	gtk_box_pack_end (GTK_BOX (theme_selector->details->bottom_box),
			  theme_selector->details->remove_button,
			  FALSE,
			  FALSE,
			  4);
	
 	gtk_signal_connect (GTK_OBJECT (theme_selector->details->remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (remove_button_callback),
			    theme_selector);

	/* now create the actual content, with the category pane and the content frame */	
	
	/* the actual contents are created when necessary */	
  	theme_selector->details->content_frame = NULL;
	
	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, 
					   nautilus_theme_selector_theme_changed,
					   theme_selector);	
	
	gtk_signal_connect (GTK_OBJECT (theme_selector), "delete_event",
                    	    GTK_SIGNAL_FUNC (nautilus_theme_selector_delete_event_callback),
                    	    NULL);
}

static void
nautilus_theme_selector_destroy (GtkObject *object)
{
	NautilusThemeSelector *theme_selector;

	theme_selector = NAUTILUS_THEME_SELECTOR (object);
		
	g_free (theme_selector->details);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_theme_selector_theme_changed,
					      theme_selector);

	if (object == GTK_OBJECT (main_theme_selector))
		main_theme_selector = NULL;
		
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

/* create a new instance */
NautilusThemeSelector *
nautilus_theme_selector_new (void)
{
	NautilusThemeSelector *browser = NAUTILUS_THEME_SELECTOR
		(gtk_type_new (nautilus_theme_selector_get_type ()));
	
	gtk_container_set_border_width (GTK_CONTAINER (browser), 0);
  	gtk_window_set_policy (GTK_WINDOW(browser), TRUE, TRUE, FALSE);
  	gtk_widget_show (GTK_WIDGET(browser));
	
	return browser;
}

/* show the main property browser */

void
nautilus_theme_selector_show (void)
{
	if (main_theme_selector == NULL) {
		main_theme_selector = nautilus_theme_selector_new ();
	} else {
		nautilus_gtk_window_present (GTK_WINDOW (main_theme_selector));
	}	
	gtk_clist_moveto (GTK_CLIST(main_theme_selector->details->theme_list), main_theme_selector ->details->selected_row, 0, 0.0, 0.0);		
}

static gboolean
nautilus_theme_selector_delete_event_callback (GtkWidget *widget,
					   GdkEvent  *event,
					   gpointer   user_data)
{
	/* Hide but don't destroy */
	gtk_widget_hide(widget);
	return TRUE;
}


/* handle the add_new button */

static void
add_new_button_callback(GtkWidget *widget, NautilusThemeSelector *theme_selector)
{
}

/* handle the "remove" button */
static void
remove_button_callback(GtkWidget *widget, NautilusThemeSelector *theme_selector)
{
}

/* utility routine to highlight the row that contains the passed in name */

static void
nautilus_theme_selector_highlight_by_name (NautilusThemeSelector *theme_selector, const char *theme_name)
{
	int index;
	char *row_theme;
	GtkCList *list;
	
	list = GTK_CLIST (theme_selector->details->theme_list);
	for (index = 0; index < list->rows; index++) {
		row_theme = gtk_clist_get_row_data (list, index);	
		if (!nautilus_strcmp (row_theme, theme_name)) {			
			gtk_clist_select_row (list, index, 0);
			theme_selector->details->selected_row = index ;
			return;
		}
	}	
}

/* handle theme changes by updating the browser contents */

static void
nautilus_theme_selector_theme_changed (gpointer user_data)
{
	char *current_theme;
	NautilusThemeSelector *theme_selector;
	
	theme_selector = NAUTILUS_THEME_SELECTOR (user_data);
	current_theme = nautilus_theme_get_theme();
	nautilus_theme_selector_highlight_by_name (theme_selector, current_theme);
	
	g_free (current_theme);
}

/* handle clicks on the theme selector by setting the theme */

static void 
theme_select_row_callback (GtkCList * clist, int row, int column, GdkEventButton *event, NautilusThemeSelector *theme_selector)
{
	char *theme_name;

	if (theme_selector->details->handling_theme_change)
		return;
		
	theme_selector->details->handling_theme_change = TRUE;	
	theme_name = gtk_clist_get_row_data (clist, row);	
	if (theme_name) {
		nautilus_theme_set_theme (theme_name);
	}
	theme_selector->details->handling_theme_change = FALSE;	
}

static gboolean
vfs_file_exists (const char *file_uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	
	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_uri, file_info, 0);
	gnome_vfs_file_info_unref (file_info);

	return result == GNOME_VFS_OK;
}

/* utility routine to test for the presence of an icon file */
static gboolean
has_image_file(const char *path_uri, const char *dir_name, const char *image_file)
{
	char* image_uri;
	gboolean exists;
	
	image_uri = g_strdup_printf("%s/%s/%s.png", path_uri, dir_name, image_file);
	exists = vfs_file_exists (image_uri);
	g_free (image_uri);
	if (exists)
		return TRUE;

	image_uri = g_strdup_printf("%s/%s/%s.svg", path_uri, dir_name, image_file);
	exists = vfs_file_exists (image_uri);
	g_free (image_uri);
	return exists;
}

/* derive the theme description from the theme name by reading its xml file */
static char*
make_theme_description (const char *theme_name)
{
	char *theme_file_name, *theme_path;
	char *description_result, *temp_str;
	xmlDocPtr theme_document;
	xmlNodePtr description_node;
	
	description_result = NULL;
	
	theme_file_name = g_strdup_printf ("%s.xml", theme_name);	
	if (!nautilus_strcmp (theme_name, "default")) {
		theme_path = nautilus_pixmap_file (theme_file_name);
	} else {
		temp_str = g_strdup_printf ("%s/%s", theme_name, theme_file_name);
		theme_path = nautilus_pixmap_file (temp_str);
		g_free (temp_str);
	}
	
	if (theme_path) {
		/* read the xml document */
		theme_document = xmlParseFile(theme_path);

		if (theme_document != NULL) {
			/* fetch the description mode, of any */		
			description_node = nautilus_xml_get_child_by_name(xmlDocGetRootElement (theme_document), "description");
			if (description_node) {		
				temp_str = xmlGetProp(description_node, "TEXT");
				if (temp_str)
					description_result = g_strdup (temp_str);
			}
		
			xmlFreeDoc (theme_document);
		}
			
		g_free (theme_path);
	}
	
	g_free (theme_file_name);
	if (description_result)
		return description_result;
	return g_strdup_printf (_("No information available for the  %s theme"), theme_name);
}


/*  set the font of a cell to a specific size from the font family specified by preferences */

static void
set_preferred_font_for_cell (NautilusThemeSelector *theme_selector, int theme_index, int column, int font_size)
{
	GtkStyle  *name_style;
	GdkFont   *name_font;

	name_font = nautilus_font_factory_get_font_from_preferences (font_size);
	name_style = gtk_clist_get_cell_style (GTK_CLIST(theme_selector->details->theme_list), theme_index, column);
	if (name_style == NULL)
		name_style = gtk_style_copy (gtk_widget_get_style (theme_selector->details->theme_list));
	else
		name_style = gtk_style_copy (name_style);	
	nautilus_gtk_style_set_font (name_style, name_font);
	gtk_clist_set_cell_style (GTK_CLIST(theme_selector->details->theme_list), theme_index, column, name_style);

	gtk_style_unref (name_style);
	gdk_font_unref (name_font);	
}


/* utility to add a theme folder to the list */

static void
add_theme (NautilusThemeSelector *theme_selector, const char *theme_path_uri, const char *theme_name, const char *current_theme, int theme_index)
{
	GtkWidget *pix_widget;
	GdkPixbuf *theme_pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	char      *clist_entry[3];
	
	/* generate a pixbuf to represent the theme */
	theme_pixbuf = nautilus_theme_make_selector (theme_name);
	theme_pixbuf = nautilus_gdk_pixbuf_scale_to_fit	 (theme_pixbuf, 70, 48);
	
	gdk_pixbuf_render_pixmap_and_mask (theme_pixbuf, &pixmap, &mask, 128);
	gdk_pixbuf_unref (theme_pixbuf);
	
	/* generate a pixwidget to hold it */
	
	pix_widget = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));
	gtk_widget_show (pix_widget);

	/* install it in the list view */	
	
	clist_entry[0] = NULL;
	clist_entry[1] = g_strdup (theme_name);
	clist_entry[2] = make_theme_description (theme_name);
	
	gtk_clist_append (GTK_CLIST(theme_selector->details->theme_list), clist_entry);
	
	/* set up the theme logo image */ 
	gtk_clist_set_pixmap (GTK_CLIST(theme_selector->details->theme_list), theme_index, 0, pixmap , mask);
	gtk_clist_set_row_data (GTK_CLIST(theme_selector->details->theme_list),
					 theme_index, g_strdup(theme_name ));
	
	/* set up the fonts for the theme name and description */
	
	set_preferred_font_for_cell (theme_selector, theme_index, 1, 18);
	set_preferred_font_for_cell (theme_selector, theme_index, 2, 10);
		
	gdk_pixmap_unref (pixmap);
	gdk_bitmap_unref (mask);
}
 
/* populate the list view with the available themes, glean by iterating  */
static void
populate_list_with_themes (NautilusThemeSelector *theme_selector)
{
	int index, selected_index;
	char *current_theme, *pixmap_directory, *directory_uri;
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
		
	/* get the current theme */
	current_theme = nautilus_theme_get_theme();

	/* allocate the colors for the rows */
	
	gdk_color_parse ("rgb:FF/FF/FF", &theme_selector->details->main_row_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (theme_selector->details->theme_list)),
				  &theme_selector->details->main_row_color, FALSE, TRUE);

	gdk_color_parse ("rgb:DD/DD/DD", &theme_selector->details->alt_row_color);	
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (theme_selector->details->theme_list)),
				  &theme_selector->details->alt_row_color, FALSE, TRUE);
	
	/* iterate the pixmap directory to find other installed themes */	
	pixmap_directory = nautilus_get_pixmap_directory ();
	directory_uri = nautilus_get_uri_from_local_path (pixmap_directory);

	index = 0;
	selected_index = -1;
	
	/* add a theme element for the default theme */
	add_theme (theme_selector, pixmap_directory, "default", current_theme, index++);

	/* get the uri for the images directory */
	g_free (pixmap_directory);
			
	result = gnome_vfs_directory_list_load (&list, directory_uri,
					       GNOME_VFS_FILE_INFO_DEFAULT, NULL);
	if (result != GNOME_VFS_OK) {
		g_free (directory_uri);
		g_free (current_theme);
		return;
	}

	/* interate through the directory for each file */
	current_file_info = gnome_vfs_directory_list_first(list);
	while (current_file_info != NULL) {
		if ((current_file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) && (current_file_info->name[0] != '.')) {
			if (has_image_file (directory_uri, current_file_info->name, "i-directory" )) {
				if (!nautilus_strcmp (current_theme, current_file_info->name)) {
					selected_index = index;
				}
				add_theme (theme_selector, directory_uri, current_file_info->name, current_theme, index);
				index += 1;
			}
		}
			
		current_file_info = gnome_vfs_directory_list_next (list);
	}

	/* select the appropriate row, and make sure it's visible on the screen */	
	if (selected_index >= 0) {
		theme_selector->details->selected_row = selected_index;
		gtk_clist_select_row (GTK_CLIST(theme_selector->details->theme_list), selected_index, 0);
		gtk_clist_moveto (GTK_CLIST(theme_selector->details->theme_list), selected_index, 0, 0.0, 0.0);		
	}
	
	g_free (directory_uri);
	g_free (current_theme);
	gnome_vfs_directory_list_destroy (list);	

}
