/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
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
 * This is the index title widget, which is the title part of the index panel
 *
 */

#include <config.h>
#include "nautilus-sidebar-title.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

static void     nautilus_sidebar_title_initialize_class   (NautilusSidebarTitleClass *klass);
static void     nautilus_sidebar_title_destroy            (GtkObject                 *object);
static void     nautilus_sidebar_title_initialize         (NautilusSidebarTitle      *pixmap);
static gboolean nautilus_sidebar_title_button_press_event (GtkWidget                 *widget,
							   GdkEventButton            *event);
static void     nautilus_sidebar_title_update_icon        (NautilusSidebarTitle      *sidebar_title);
static void     nautilus_sidebar_title_update_label       (NautilusSidebarTitle      *sidebar_title);
static void     nautilus_sidebar_title_update_info        (NautilusSidebarTitle      *sidebar_title);

struct NautilusSidebarTitleDetails {
	NautilusFile *file;
	guint file_changed_connection;
	char *requested_text;
	GtkWidget *icon;
	GtkWidget *title;
	GtkWidget *more_info;
	GtkWidget *emblem_box;
	GtkWidget *notes;
};

/* constants */

#define MAX_ICON_WIDTH 100
#define MAX_ICON_HEIGHT 120

/* button assignments */

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSidebarTitle, nautilus_sidebar_title, gtk_vbox_get_type ())

static void
nautilus_sidebar_title_initialize_class (NautilusSidebarTitleClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->destroy = nautilus_sidebar_title_destroy;
	widget_class->button_press_event = nautilus_sidebar_title_button_press_event;
}

static void
nautilus_sidebar_title_initialize (NautilusSidebarTitle *sidebar_title)
{ 
	sidebar_title->details = g_new0 (NautilusSidebarTitleDetails, 1);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       nautilus_sidebar_title_update_icon,
					       GTK_OBJECT (sidebar_title));
}

/* destroy by throwing away private storage */

static void
release_file (NautilusSidebarTitle *sidebar_title)
{
	if (sidebar_title->details->file_changed_connection != 0) {
		gtk_signal_disconnect (GTK_OBJECT (sidebar_title->details->file),
				       sidebar_title->details->file_changed_connection);
		sidebar_title->details->file_changed_connection = 0;
	}

	if (sidebar_title->details->file != NULL) {
		nautilus_file_monitor_remove (sidebar_title->details->file, sidebar_title);
		nautilus_file_unref (sidebar_title->details->file);
		sidebar_title->details->file = NULL;
	}
}

static void
nautilus_sidebar_title_destroy (GtkObject *object)
{
	NautilusSidebarTitle *sidebar_title;

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (object);

	release_file (sidebar_title);
	
	g_free (sidebar_title->details->requested_text);
	g_free (sidebar_title->details);
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* return a new index title object */

GtkWidget *
nautilus_sidebar_title_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_sidebar_title_get_type ()));
}

/* set up the icon image */
static void
nautilus_sidebar_title_update_icon (NautilusSidebarTitle *sidebar_title)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;

	/* NULL can happen because nautilus_file_get returns NULL for the root. */
	if (sidebar_title->details->file == NULL) {
		return;
	}
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (sidebar_title->details->file,
							    NAUTILUS_ICON_SIZE_STANDARD);

	/* make a pixmap and mask to pass to the widget */
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
	gdk_pixbuf_unref (pixbuf);
	
	/* if there's no pixmap so far, so allocate one */
	if (sidebar_title->details->icon != NULL) {
		gtk_pixmap_set (GTK_PIXMAP (sidebar_title->details->icon),
				pixmap, mask);
	} else {
		sidebar_title->details->icon = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));
		gtk_widget_show (sidebar_title->details->icon);
		gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->icon, 0, 0, 0);
		gtk_box_reorder_child (GTK_BOX (sidebar_title), sidebar_title->details->icon, 0);
	}
}

/* set up the filename label */
static void
nautilus_sidebar_title_update_label (NautilusSidebarTitle *sidebar_title)
{
	GdkFont *label_font;
	char *displayed_text;
		
	if (sidebar_title->details->requested_text == NULL) {
		/* Use empty string to replace previous contents. */
		displayed_text = g_strdup ("");
	} else {
		displayed_text = g_strdup (sidebar_title->details->requested_text);
	}
	
	/* split the filename into two lines if necessary */
	if (strlen(displayed_text) >= 16) {
		/* find an appropriate split point if we can */
		int index;
		int mid_point = strlen(displayed_text) >> 1;
		int quarter_point = mid_point >> 1;
		for (index = 0; index < quarter_point; index++) {
			int split_offset = 0;
			
			if (!isalnum(displayed_text[mid_point + index]))
				split_offset = mid_point + index;
			else if (!isalnum(displayed_text[mid_point - index]))
				split_offset = mid_point - index;
			
			if (split_offset != 0) {
				char *buffer = g_malloc(strlen(displayed_text) + 2);
				
				/* build the new string, with a blank inserted, also remembering them separately for measuring */
				memcpy(buffer, displayed_text, split_offset);
				buffer[split_offset] = '\n';
				strcpy(&buffer[split_offset + 1], &displayed_text[split_offset]);
				
				/* free up the old string and replace it with the new one with the return inserted */
				
				g_free (displayed_text);
				displayed_text = buffer;
				break;
			}
		}
	}
	
	if (sidebar_title->details->title != NULL) {
		gtk_label_set_text (GTK_LABEL (sidebar_title->details->title), displayed_text);
	} else {
		sidebar_title->details->title = GTK_WIDGET (gtk_label_new (displayed_text));
		gtk_label_set_line_wrap (GTK_LABEL (sidebar_title->details->title), TRUE);
		gtk_widget_show (sidebar_title->details->title);
		gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->title, 0, 0, 0);
		gtk_box_reorder_child (GTK_BOX (sidebar_title), sidebar_title->details->title, 1);
	}
	
	/* FIXME bugzilla.eazel.com 1103: 
	 * Make this use the font factory 
	 */
	label_font = nautilus_get_largest_fitting_font (displayed_text, GTK_WIDGET (sidebar_title)->allocation.width - 4,
				  "-*-helvetica-medium-r-normal-*-%d-*-*-*-*-*-*-*");
	
	nautilus_gtk_widget_set_font (sidebar_title->details->title, label_font);
	g_free (displayed_text);
}

/* add a pixbuf to the emblem box */
static void
nautilus_sidebar_title_add_pixbuf(NautilusSidebarTitle *sidebar_title, GdkPixbuf *pixbuf)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *pixmap_widget;
	
	if (sidebar_title->details->emblem_box == NULL) {
		/* alllocate a new emblem box */
		sidebar_title->details->emblem_box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(sidebar_title->details->emblem_box);
		gtk_box_pack_start(GTK_BOX (sidebar_title), sidebar_title->details->emblem_box, 0, 0, 0);
	}
        
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
	pixmap_widget = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));
	gtk_widget_show (pixmap_widget);
	gtk_container_add(GTK_CONTAINER(sidebar_title->details->emblem_box), pixmap_widget);	
}

/* set up more info about the file */

void
nautilus_sidebar_title_update_info (NautilusSidebarTitle *sidebar_title)
{
	NautilusFile *file;
	GList *emblem_icons, *current_emblem;
	char *notes_text;
	char *temp_string;
	char *info_string;
	GdkFont *font;

	/* NULL can happen because nautilus_file_get returns NULL for the root. */
	file = sidebar_title->details->file;
	if (file == NULL) {
		return;
	}
	
	info_string = nautilus_file_get_string_attribute(file, "type");
	if (info_string == NULL) {
		return;
	}

	/* combine the type and the size */
	temp_string = nautilus_file_get_string_attribute(file, "size");
	if (temp_string != NULL) {
		char *new_info_string = g_strconcat(info_string, ", ", temp_string, NULL);
		g_free (info_string);
		g_free (temp_string);
		info_string = new_info_string; 
	}
	
	/* append the date modified */
	temp_string = nautilus_file_get_string_attribute(file, "date_modified");
	if (temp_string != NULL) {
		char *new_info_string = g_strconcat(info_string, "\n", temp_string, NULL);
		g_free (info_string);
		g_free (temp_string);
		info_string = new_info_string;
	}
	
	/* set up the emblems if necessary.  First, deallocate any existing ones */
	if (sidebar_title->details->emblem_box) {
		gtk_widget_destroy(sidebar_title->details->emblem_box);
		sidebar_title->details->emblem_box = NULL;
	}
	
	/* fetch the emblem icons from metadata */
	emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file (file);
	if (emblem_icons) {
		GdkPixbuf *emblem_pixbuf;

		/* loop through the list of emblems, installing them in the box */
		for (current_emblem = emblem_icons; current_emblem != NULL; current_emblem = current_emblem->next) {
			emblem_pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
				(current_emblem->data,
				 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
				 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD);
			if (emblem_pixbuf != NULL) {
				nautilus_sidebar_title_add_pixbuf (sidebar_title, emblem_pixbuf);
				gdk_pixbuf_unref (emblem_pixbuf);
			}
		}
		
		nautilus_scalable_icon_list_free (emblem_icons);
	}
			
	/* set up the additional text info */
	
	if (sidebar_title->details->more_info)
		gtk_label_set_text(GTK_LABEL(sidebar_title->details->more_info), info_string);
	else {
		sidebar_title->details->more_info = GTK_WIDGET(gtk_label_new(info_string));
		gtk_widget_show (sidebar_title->details->more_info);
		gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->more_info, 0, 0, 0);
		gtk_box_reorder_child (GTK_BOX (sidebar_title), sidebar_title->details->more_info, 2);
	}   

        font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (sidebar_title->details->more_info, font);
        gdk_font_unref (font);
	
	g_free (info_string);
	
	/* see if there are any notes for this file. If so, display them */
	notes_text = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_NOTES, NULL);
	if (notes_text != NULL) {
		if (sidebar_title->details->notes != NULL)
			gtk_label_set_text(GTK_LABEL(sidebar_title->details->notes), notes_text);
		else  {  
			sidebar_title->details->notes = GTK_WIDGET(gtk_label_new(notes_text));
			gtk_label_set_line_wrap(GTK_LABEL(sidebar_title->details->notes), TRUE);
			gtk_widget_show (sidebar_title->details->notes);
			gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->notes, 0, 0, 0);
			gtk_box_reorder_child (GTK_BOX (sidebar_title), sidebar_title->details->notes, 3);
		}
		
		font = nautilus_font_factory_get_font_from_preferences (12);
		nautilus_gtk_widget_set_font (sidebar_title->details->notes, font);
		gdk_font_unref (font);
		
		g_free (notes_text);
	} else if (sidebar_title->details->notes != NULL) {
		gtk_label_set_text (GTK_LABEL (sidebar_title->details->notes), "");
	}
}

/* return the filename text */
char *
nautilus_sidebar_title_get_text(NautilusSidebarTitle *sidebar_title)
{
	return g_strdup (sidebar_title->details->requested_text);
}

/* set up the filename text */
void
nautilus_sidebar_title_set_text (NautilusSidebarTitle *sidebar_title, const char* new_text)
{
	g_free (sidebar_title->details->requested_text);
	sidebar_title->details->requested_text = g_strdup (new_text);

	/* Recompute the displayed text. */
	nautilus_sidebar_title_update_label (sidebar_title);
}

static void
update (NautilusSidebarTitle *sidebar_title)
{
	/* add the icon */
	nautilus_sidebar_title_update_icon (sidebar_title);

	/* add the name, in a variable-sized label */
	nautilus_sidebar_title_update_label (sidebar_title);

	/* add various info */
	nautilus_sidebar_title_update_info (sidebar_title);
}

void
nautilus_sidebar_title_set_uri (NautilusSidebarTitle *sidebar_title,
			      const char* new_uri,
			      const char* initial_text)
{
	GList *attributes;

	release_file (sidebar_title);

	sidebar_title->details->file = nautilus_file_get (new_uri);
	if (sidebar_title->details->file != NULL) {
		sidebar_title->details->file_changed_connection =
			gtk_signal_connect_object (GTK_OBJECT (sidebar_title->details->file),
						   "changed",
						   update,
						   GTK_OBJECT (sidebar_title));

		/* Monitor the things needed to get the right icon. */
		attributes = nautilus_icon_factory_get_required_file_attributes ();		
						   
		/* Also monitor a directory's item count so we can update when it is known. */
		if (nautilus_file_is_directory (sidebar_title->details->file)) {
			attributes = g_list_prepend (attributes,
						     NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
		}
		nautilus_file_monitor_add (sidebar_title->details->file, sidebar_title,
					   attributes, FALSE);
		g_list_free (attributes);
	}

	g_free (sidebar_title->details->requested_text);
	sidebar_title->details->requested_text = g_strdup (initial_text);

	update (sidebar_title);
}

static gboolean
nautilus_sidebar_title_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	/* FIXME: We must do something other than a g_message here.
	 * NautilusSidebarTitle *sidebar_title = NAUTILUS_SIDEBAR_TITLE (widget);  
	 */
	g_message ("button press");
	return TRUE;
}

gboolean
nautilus_sidebar_title_hit_test_icon (NautilusSidebarTitle *title, int x, int y)
{
	return nautilus_point_in_widget (title->details->icon, x, y);
}
