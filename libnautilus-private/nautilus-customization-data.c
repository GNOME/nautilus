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
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-customization-data.c - functions to collect and load customization
   names and imges */

#include <config.h>
#include <ctype.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>
#include <libgnome/gnome-defs.h>
#include <stdlib.h>

#include "nautilus-customization-data.h"
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include "nautilus-file-utilities.h"
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-xml-extensions.h>
#include <eel/eel-string.h>

typedef enum {
	READ_PUBLIC_CUSTOMIZATIONS,
	READ_PRIVATE_CUSTOMIZATIONS
} CustomizationReadingMode;

struct NautilusCustomizationData {
	char *customization_name;
	CustomizationReadingMode reading_mode;

	GList *public_file_list;	
	GList *private_file_list;
	GList *current_file_list;

	GHashTable *name_map_hash;
	
	GdkPixbuf *pattern_frame;

	gboolean private_data_was_displayed;
	gboolean data_is_for_a_menu;
	int maximum_icon_height;
	int maximum_icon_width;
};


/* The Property here should be one of "emblems", "colors" or "patterns" */
static char *            get_global_customization_uri        (const char *customization_name);
static char *            get_private_customization_uri       (const char *customization_name);
static char *            get_file_path_for_mode              (const NautilusCustomizationData *data,
							      const char *file_name);
static char*             format_name_for_display             (NautilusCustomizationData *data, const char *name);
static char*             strip_extension                     (const char* string_to_strip);
static void		 load_name_map_hash_table	     (NautilusCustomizationData *data);

NautilusCustomizationData* 
nautilus_customization_data_new (const char *customization_name,
				 gboolean show_public_customizations,
				 gboolean data_is_for_a_menu,
				 int maximum_icon_height,
				 int maximum_icon_width)
{
	NautilusCustomizationData *data;
	char *public_directory_uri, *private_directory_uri;
	char *temp_str;
	GnomeVFSResult public_result, private_result;

	data = g_new0 (NautilusCustomizationData, 1);

	public_result = GNOME_VFS_OK;

	if (show_public_customizations) {
		public_directory_uri = get_global_customization_uri (customization_name);
		
		
		public_result = gnome_vfs_directory_list_load (&data->public_file_list,
							       public_directory_uri,
							       GNOME_VFS_FILE_INFO_GET_MIME_TYPE
							       | GNOME_VFS_FILE_INFO_FOLLOW_LINKS, NULL);
		g_free (public_directory_uri);
	}

	private_directory_uri = get_private_customization_uri (customization_name);
	private_result = gnome_vfs_directory_list_load (&data->private_file_list,
							private_directory_uri,
							GNOME_VFS_FILE_INFO_GET_MIME_TYPE
							| GNOME_VFS_FILE_INFO_FOLLOW_LINKS, NULL);
	g_free (private_directory_uri);
	if (public_result != GNOME_VFS_OK && 
	    private_result != GNOME_VFS_OK) {
		g_warning ("Couldn't read any of the emblem directories\n");
		g_free (data);
		return NULL;
	}
	if (private_result == GNOME_VFS_OK) {
		data->reading_mode = READ_PRIVATE_CUSTOMIZATIONS;
		data->current_file_list = data->private_file_list;
	}
	if (show_public_customizations && public_result == GNOME_VFS_OK) {
		data->reading_mode = READ_PUBLIC_CUSTOMIZATIONS;	
		data->current_file_list = data->public_file_list;
	}

	/* load the frame if necessary */
	if (!strcmp(customization_name, "patterns")) {
		temp_str = nautilus_pixmap_file ("chit_frame.png");
		data->pattern_frame = gdk_pixbuf_new_from_file (temp_str);
		g_free (temp_str);
	} else {
		data->pattern_frame = NULL;
	}

	
	data->private_data_was_displayed = FALSE;
	data->data_is_for_a_menu = data_is_for_a_menu;
	data->customization_name = g_strdup (customization_name);

	data->maximum_icon_height = maximum_icon_height;
	data->maximum_icon_width = maximum_icon_width;

	load_name_map_hash_table (data);
	
	return data;
}

GnomeVFSResult
nautilus_customization_data_get_next_element_for_display (NautilusCustomizationData *data,
							  char **emblem_name,
							  GdkPixbuf **pixbuf_out,
							  char **label_out)
{
	GnomeVFSFileInfo *current_file_info;

	char *image_file_name, *filtered_name;
	GdkPixbuf *pixbuf;
	GdkPixbuf *orig_pixbuf;
	gboolean is_reset_image;
	
	g_return_val_if_fail (data != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (emblem_name != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (pixbuf_out != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (label_out != NULL, GNOME_VFS_ERROR_BAD_PARAMETERS);
	
	if (data->current_file_list == NULL) {
		if (data->reading_mode == READ_PUBLIC_CUSTOMIZATIONS) {
			if (data->private_file_list == NULL) {
				return GNOME_VFS_ERROR_EOF;
			}
			data->reading_mode = READ_PRIVATE_CUSTOMIZATIONS;
			data->current_file_list = data->private_file_list;
			return nautilus_customization_data_get_next_element_for_display (data,
											 emblem_name,
											 pixbuf_out,
											 label_out);
		}
		else {
			return GNOME_VFS_ERROR_EOF;
		}
	}
	
	
	current_file_info = data->current_file_list->data;
	data->current_file_list = data->current_file_list->next;

	g_assert (current_file_info != NULL);

	if (!eel_istr_has_prefix (current_file_info->mime_type, "image/")
	    || eel_istr_has_prefix (current_file_info->name, ".")) {
		return nautilus_customization_data_get_next_element_for_display (data,
										 emblem_name,
										 pixbuf_out,
										 label_out);
	}

	image_file_name = get_file_path_for_mode (data,
						  current_file_info->name);
	orig_pixbuf = gdk_pixbuf_new_from_file (image_file_name);	
	g_free (image_file_name);

	is_reset_image = eel_strcmp(current_file_info->name, RESET_IMAGE_NAME) == 0;

	*emblem_name = g_strdup (current_file_info->name);
	
	if (!strcmp(data->customization_name, "patterns")) {
		pixbuf = nautilus_customization_make_pattern_chit (orig_pixbuf, data->pattern_frame, FALSE, is_reset_image);
	} else {
		pixbuf = eel_gdk_pixbuf_scale_down_to_fit (orig_pixbuf, 
								data->maximum_icon_width, 
								data->maximum_icon_height);
		gdk_pixbuf_unref (orig_pixbuf);
	}
	
	*pixbuf_out = pixbuf;
	
	filtered_name = format_name_for_display (data, current_file_info->name);
	/* If the data is for a menu,
	   we want to truncate it and not use the nautilus
	   label because anti-aliased text doesn't look right
	   in menus */
	if (data->data_is_for_a_menu) {
		*label_out = eel_truncate_text_for_menu_item (filtered_name);
	}
	else {
		*label_out = g_strdup (filtered_name);
	}
	
	g_free (filtered_name);

	if (data->reading_mode == READ_PRIVATE_CUSTOMIZATIONS) {
		data->private_data_was_displayed = TRUE;
	}
	return GNOME_VFS_OK;
}

gboolean                   
nautilus_customization_data_private_data_was_displayed (NautilusCustomizationData *data)
{
	return data->private_data_was_displayed;
}

void                       
nautilus_customization_data_destroy (NautilusCustomizationData *data)
{
	g_assert (data->public_file_list != NULL ||
		  data->private_file_list != NULL);

	if (data->pattern_frame != NULL) {
		gdk_pixbuf_unref (data->pattern_frame);
	}

	gnome_vfs_file_info_list_free (data->public_file_list);
	gnome_vfs_file_info_list_free (data->private_file_list);

	if (data->name_map_hash != NULL) {
		eel_g_hash_table_destroy_deep (data->name_map_hash);	
	}
	
	g_free (data->customization_name);
	g_free (data);
}


/* get_global_customization_directory
   Get the path where a property's pixmaps are stored 
   @customization_name : the name of the customization to get.
   Should be one of "emblems", "colors", or "paterns" 

   Return value: The directory name where the customization's 
   public pixmaps are stored */
static char *                  
get_global_customization_uri (const char *customization_name)
{
	char *directory_path, *directory_uri;
	
	directory_path = nautilus_make_path (NAUTILUS_DATADIR,
					     customization_name);
	directory_uri = gnome_vfs_get_uri_from_local_path (directory_path);
	
	g_free (directory_path);

	return directory_uri;
	
}


/* get_private_customization_directory
   Get the path where a customization's pixmaps are stored 
   @customization_name : the name of the customization to get.
   Should be one of "emblems", "colors", or "patterns" 

   Return value: The directory name where the customization's 
   user-specific pixmaps are stored */
static char *                  
get_private_customization_uri (const char *customization_name)
{
	char *user_directory;
	char *directory_path, *directory_uri;

	user_directory = nautilus_get_user_directory ();
	directory_path = nautilus_make_path (user_directory,
					     customization_name);
	g_free (user_directory);
	directory_uri = gnome_vfs_get_uri_from_local_path (directory_path);
	g_free (directory_path);
	
	return directory_uri;
}


static char *            
get_file_path_for_mode (const NautilusCustomizationData *data,
			const char *file_name)
{
	char *directory_uri, *uri, *directory_name;
	if (data->reading_mode == READ_PUBLIC_CUSTOMIZATIONS) {
		directory_uri = get_global_customization_uri (data->customization_name);
	}
	else {
		directory_uri = get_private_customization_uri (data->customization_name);
	}
	
	uri = nautilus_make_path (directory_uri, file_name);
	g_free (directory_uri);
	directory_name = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);

	return directory_name;
}

/* utility to composite localizable text onto the reset pixbuf */
static void
add_reset_text (GdkPixbuf *pixbuf)
{
	char *reset_text;
	EelScalableFont *font;
	EelDimensions title_dimensions;
	int width, height;
	int font_size, text_len;
	int h_offset, v_offset;
	
	font = eel_scalable_font_get_default_font ();
	reset_text = _("reset");
	text_len = strlen (reset_text);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	
	font_size = eel_scalable_font_largest_fitting_font_size (font, reset_text, width - 12, 12, 36);
	title_dimensions = eel_scalable_font_measure_text (font, font_size, reset_text, text_len);
	
	/* compute text position, correcting for the imbalanced shadow, etc. */
	h_offset = ((width - title_dimensions.width) / 2) - 2;
	v_offset = (((height - 8)/ 2) - title_dimensions.height) / 2;
	
	eel_scalable_font_draw_text (font, pixbuf, 
					  h_offset, v_offset,
					  eel_gdk_pixbuf_whole_pixbuf,
					  font_size,
					  reset_text, text_len,
					  EEL_RGBA_COLOR_OPAQUE_WHITE,
					  EEL_OPACITY_FULLY_OPAQUE);
	
	gtk_object_unref (GTK_OBJECT (font));
}

/* utility to make an attractive pattern image by compositing with a frame */
GdkPixbuf*
nautilus_customization_make_pattern_chit (GdkPixbuf *pattern_tile, GdkPixbuf *frame, gboolean dragging, gboolean is_reset)
{
	GdkPixbuf *pixbuf, *temp_pixbuf;
	int frame_width, frame_height;
	
	
	frame_width = gdk_pixbuf_get_width (frame);
	frame_height = gdk_pixbuf_get_height (frame);
	
	/* scale the pattern tile to the proper size */
	pixbuf = gdk_pixbuf_scale_simple (pattern_tile, frame_width, frame_height, GDK_INTERP_BILINEAR);
			
	/* composite the mask on top of it */
	gdk_pixbuf_composite (frame, pixbuf, 0, 0, frame_width, frame_height,
			      0.0, 0.0, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
	
	/* if we're dragging, get rid of the light-colored halo */
	if (dragging) {
		temp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, frame_width - 6, frame_height - 6);
		gdk_pixbuf_copy_area (pixbuf, 2, 2, frame_width - 6, frame_height - 6, temp_pixbuf, 0, 0);
		gdk_pixbuf_unref (pixbuf);
		pixbuf = temp_pixbuf;
	}
			      
	gdk_pixbuf_unref (pattern_tile);

	if (is_reset) {
		add_reset_text (pixbuf);
	}

	return pixbuf;
}


/* utility to format the passed-in name for display by stripping the extension, mapping underscore
   and capitalizing as necessary */

static char*
format_name_for_display (NautilusCustomizationData *data, const char* name)
{
	char *formatted_str, *mapped_name;

	/* don't display a name for the "reset" property, since its name is
	   contained in its image and also to help distinguish it */  
	if (!eel_strcmp(name, RESET_IMAGE_NAME)) {
		return g_strdup("");
	}

	/* map file names to display names using the mappings defined in the hash table */
	
	formatted_str = strip_extension (name);
	if (data->name_map_hash != NULL) {
		mapped_name = g_hash_table_lookup (data->name_map_hash, formatted_str);
		if (mapped_name) {
			g_free (formatted_str);
			formatted_str = g_strdup (mapped_name);
		}	
	}
			
	return formatted_str;	
}

/* utility routine to allocate a hash table and load it with the appropriate 
 * name mapping data from the browser xml file 
 */
static void
load_name_map_hash_table (NautilusCustomizationData *data)
{
	char *xml_path;
	char *filename, *display_name;

	xmlDocPtr browser_data;
	xmlNodePtr category_node, current_node;
	
	/* allocate the hash table */
	data->name_map_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	/* build the path name to the browser.xml file and load it */
	xml_path = nautilus_make_path (NAUTILUS_DATADIR, "browser.xml");
	if (xml_path) {
		browser_data = xmlParseFile (xml_path);
		g_free (xml_path);

		if (browser_data) {
			/* get the category node */
			category_node = eel_xml_get_root_child_by_name_and_property (browser_data, "category", "name", data->customization_name);
			current_node = category_node->childs;	
			
			/* loop through the entries, adding a mapping to the hash table */
			while (current_node != NULL) {
				display_name = eel_xml_get_property_translated (current_node, "display_name");
				filename = xmlGetProp (current_node, "filename");
				if (display_name && filename) {
					g_hash_table_insert (data->name_map_hash, g_strdup (filename), g_strdup (display_name));
				}
				xmlFree (filename);		
				xmlFree (display_name);
				current_node = current_node->next;
			}
			
			/* close the xml file */
			xmlFreeDoc (browser_data);
		}		
	}	
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
