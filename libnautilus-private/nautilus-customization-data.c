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
#include "nautilus-customization-data.h"

#include "nautilus-file-utilities.h"
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-pango-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-util.h>
#include <libxml/parser.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	READ_PUBLIC_CUSTOMIZATIONS,
	READ_PRIVATE_CUSTOMIZATIONS
} CustomizationReadingMode;

struct NautilusCustomizationData {
	char *customization_name;

	GList *public_file_list;	
	GList *private_file_list;
	GList *current_file_list;

	GHashTable *name_map_hash;
	
	GdkPixbuf *pattern_frame;

	int maximum_icon_height;
	int maximum_icon_width;

	guint private_data_was_displayed : 1;
	guint reading_mode : 2; /* enough bits for CustomizationReadingMode */
};


/* The Property here should be one of "emblems", "colors" or "patterns" */
static char *            get_global_customization_path       (const char *customization_name);
static char *            get_private_customization_path      (const char *customization_name);
static char *            get_file_path_for_mode              (const NautilusCustomizationData *data,
							      const char *file_name);
static char*             format_name_for_display             (NautilusCustomizationData *data, const char *name);
static char*             strip_extension                     (const char* string_to_strip);
static void		 load_name_map_hash_table	     (NautilusCustomizationData *data);


static gboolean
read_all_children (char *filename,
		   const char *attributes,
		   GList **list_out)
{
	GFileEnumerator *enumerator;
	GList *list;
	GFile *file;
	GFileInfo *info;

	file = g_file_new_for_path (filename);

	enumerator = g_file_enumerate_children (file, attributes, 0, NULL, NULL);
	if (enumerator == NULL) {
		return FALSE;
	}

	list = NULL;
	do {
		info = g_file_enumerator_next_file (enumerator, NULL, NULL);
		if (info) {
			list = g_list_prepend (list, info);
		}
	} while (info != NULL);

	g_object_unref (enumerator);
	g_object_unref (file);

	*list_out = g_list_reverse (list);
	return TRUE;
}


NautilusCustomizationData* 
nautilus_customization_data_new (const char *customization_name,
				 gboolean show_public_customizations,
				 int maximum_icon_height,
				 int maximum_icon_width)
{
	NautilusCustomizationData *data;
	char *public_directory_path, *private_directory_path;
	char *temp_str;
	gboolean public_result, private_result;

	data = g_new0 (NautilusCustomizationData, 1);

	public_result = TRUE;

	if (show_public_customizations) {
		public_directory_path = get_global_customization_path (customization_name);
		
		public_result  = read_all_children (public_directory_path,
						    G_FILE_ATTRIBUTE_STANDARD_NAME ","
						    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
						    &data->public_file_list);
		g_free (public_directory_path);
	}

	private_directory_path = get_private_customization_path (customization_name);
	private_result = read_all_children (private_directory_path,
					    G_FILE_ATTRIBUTE_STANDARD_NAME ","
					    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					    &data->private_file_list);
	g_free (private_directory_path);
	if (!public_result && !private_result) {
		g_warning ("Couldn't read any of the emblem directories\n");
		g_free (data);
		return NULL;
	}
	if (private_result) {
		data->reading_mode = READ_PRIVATE_CUSTOMIZATIONS;
		data->current_file_list = data->private_file_list;
	}
	if (show_public_customizations && public_result) {
		data->reading_mode = READ_PUBLIC_CUSTOMIZATIONS;	
		data->current_file_list = data->public_file_list;
	}

	data->pattern_frame = NULL;

	/* load the frame if necessary */
	if (strcmp (customization_name, "patterns") == 0) {
		temp_str = nautilus_pixmap_file ("chit_frame.png");
		if (temp_str != NULL) {
			data->pattern_frame = gdk_pixbuf_new_from_file (temp_str, NULL);
		}
		g_free (temp_str);
	}

	data->private_data_was_displayed = FALSE;
	data->customization_name = g_strdup (customization_name);

	data->maximum_icon_height = maximum_icon_height;
	data->maximum_icon_width = maximum_icon_width;

	load_name_map_hash_table (data);
	
	return data;
}

gboolean
nautilus_customization_data_get_next_element_for_display (NautilusCustomizationData *data,
							  char **emblem_name,
							  GdkPixbuf **pixbuf_out,
							  char **label_out)
{
	GFileInfo *current_file_info;
	char *image_file_name;
	GdkPixbuf *pixbuf;
	GdkPixbuf *orig_pixbuf;
	gboolean is_reset_image;
	
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (emblem_name != NULL, FALSE);
	g_return_val_if_fail (pixbuf_out != NULL, FALSE);
	g_return_val_if_fail (label_out != NULL, FALSE);
	
	if (data->current_file_list == NULL) {
		if (data->reading_mode == READ_PUBLIC_CUSTOMIZATIONS) {
			if (data->private_file_list == NULL) {
				return FALSE;
			}
			data->reading_mode = READ_PRIVATE_CUSTOMIZATIONS;
			data->current_file_list = data->private_file_list;
			return nautilus_customization_data_get_next_element_for_display (data,
											 emblem_name,
											 pixbuf_out,
											 label_out);
		}
		else {
			return FALSE;
		}
	}
	
	
	current_file_info = data->current_file_list->data;
	data->current_file_list = data->current_file_list->next;

	g_assert (current_file_info != NULL);

	if (!eel_istr_has_prefix (g_file_info_get_content_type (current_file_info), "image/")
	    || eel_istr_has_prefix (g_file_info_get_name (current_file_info), ".")) {
		return nautilus_customization_data_get_next_element_for_display (data,
										 emblem_name,
										 pixbuf_out,
										 label_out);
	}

	image_file_name = get_file_path_for_mode (data,
						  g_file_info_get_name (current_file_info));
	orig_pixbuf = gdk_pixbuf_new_from_file (image_file_name, NULL);

	if (orig_pixbuf == NULL) {
		orig_pixbuf = rsvg_pixbuf_from_file_at_max_size (image_file_name,
								 data->maximum_icon_width, 
								 data->maximum_icon_height,
								 NULL);
	}
	g_free (image_file_name);

	if (orig_pixbuf == NULL) {
		return nautilus_customization_data_get_next_element_for_display (data,
										 emblem_name,
										 pixbuf_out,
										 label_out);
	}

	is_reset_image = eel_strcmp(g_file_info_get_name (current_file_info), RESET_IMAGE_NAME) == 0;

	*emblem_name = g_strdup (g_file_info_get_name (current_file_info));

	if (strcmp (data->customization_name, "patterns") == 0 &&
	    data->pattern_frame != NULL) {
		pixbuf = nautilus_customization_make_pattern_chit (orig_pixbuf, data->pattern_frame, FALSE, is_reset_image);
	} else {
		pixbuf = eel_gdk_pixbuf_scale_down_to_fit (orig_pixbuf, 
							   data->maximum_icon_width, 
							   data->maximum_icon_height);
	}

	g_object_unref (orig_pixbuf);
	
	*pixbuf_out = pixbuf;
	
	*label_out = format_name_for_display (data, g_file_info_get_name (current_file_info));

	if (data->reading_mode == READ_PRIVATE_CUSTOMIZATIONS) {
		data->private_data_was_displayed = TRUE;
	}
	return TRUE;
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
		g_object_unref (data->pattern_frame);
	}

	eel_g_object_list_free (data->public_file_list);
	eel_g_object_list_free (data->private_file_list);

	if (data->name_map_hash != NULL) {
		g_hash_table_destroy (data->name_map_hash);	
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
get_global_customization_path (const char *customization_name)
{
	return g_build_filename (NAUTILUS_DATADIR,
				 customization_name,
				 NULL);
}


/* get_private_customization_directory
   Get the path where a customization's pixmaps are stored 
   @customization_name : the name of the customization to get.
   Should be one of "emblems", "colors", or "patterns" 

   Return value: The directory name where the customization's 
   user-specific pixmaps are stored */
static char *                  
get_private_customization_path (const char *customization_name)
{
	char *user_directory;
	char *directory_path;

	user_directory = nautilus_get_user_directory ();
	directory_path = g_build_filename (user_directory,
					   customization_name,
					   NULL);
	g_free (user_directory);
	
	return directory_path;
}


static char *            
get_file_path_for_mode (const NautilusCustomizationData *data,
			const char *file_name)
{
	char *directory_path, *file;
	if (data->reading_mode == READ_PUBLIC_CUSTOMIZATIONS) {
		directory_path = get_global_customization_path (data->customization_name);
	}
	else {
		directory_path = get_private_customization_path (data->customization_name);
	}
	
	file = g_build_filename (directory_path, file_name, NULL);
	g_free (directory_path);

	return file;
}


/* utility to make an attractive pattern image by compositing with a frame */
GdkPixbuf*
nautilus_customization_make_pattern_chit (GdkPixbuf *pattern_tile, GdkPixbuf *frame, gboolean dragging, gboolean is_reset)
{
	GdkPixbuf *pixbuf, *temp_pixbuf;
	int frame_width, frame_height;
	int pattern_width, pattern_height;

	g_assert (pattern_tile != NULL);
	g_assert (frame != NULL);

	frame_width = gdk_pixbuf_get_width (frame);
	frame_height = gdk_pixbuf_get_height (frame);
	pattern_width = gdk_pixbuf_get_width (pattern_tile);
	pattern_height = gdk_pixbuf_get_height (pattern_tile);

	pixbuf = gdk_pixbuf_copy (frame);

	/* scale the pattern tile to the proper size */
	gdk_pixbuf_scale (pattern_tile,
			  pixbuf,
			  2, 2, frame_width - 8, frame_height - 8,
			  0, 0,
			  (double)(frame_width - 8 + 1)/pattern_width,
			  (double)(frame_height - 8 + 1)/pattern_height,
			  GDK_INTERP_BILINEAR);
	
	/* if we're dragging, get rid of the light-colored halo */
	if (dragging) {
		temp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, frame_width - 8, frame_height - 8);
		gdk_pixbuf_copy_area (pixbuf, 2, 2, frame_width - 8, frame_height - 8, temp_pixbuf, 0, 0);
		g_object_unref (pixbuf);
		pixbuf = temp_pixbuf;
	}

	return pixbuf;
}


/* utility to format the passed-in name for display by stripping the extension, mapping underscore
   and capitalizing as necessary */

static char*
format_name_for_display (NautilusCustomizationData *data, const char* name)
{
	char *formatted_str, *mapped_name;

	if (!eel_strcmp(name, RESET_IMAGE_NAME)) {
		return g_strdup (_("Reset"));
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
	data->name_map_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	/* build the path name to the browser.xml file and load it */
	xml_path = g_build_filename (NAUTILUS_DATADIR, "browser.xml", NULL);
	if (xml_path) {
		browser_data = xmlParseFile (xml_path);
		g_free (xml_path);

		if (browser_data) {
			/* get the category node */
			category_node = eel_xml_get_root_child_by_name_and_property (browser_data, "category", "name", data->customization_name);
			current_node = category_node->children;	
			
			/* loop through the entries, adding a mapping to the hash table */
			while (current_node != NULL) {
				display_name = eel_xml_get_property_translated (current_node, "display_name");
				filename = xmlGetProp (current_node, "filename");
				if (display_name && filename) {
					g_hash_table_replace (data->name_map_hash, g_strdup (filename), g_strdup (display_name));
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
