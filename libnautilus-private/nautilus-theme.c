/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-theme.c: theme framework with xml-based theme definition files
  
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-theme.h"

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include <eel/eel-debug.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string-list.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <librsvg/rsvg.h>

/* static globals to hold the last accessed theme files */
static char	 *last_theme_name = NULL;
static xmlDocPtr last_theme_document = NULL;

static char *theme_from_preferences = NULL;

#define THEME_PREVIEW_ICON_WIDTH 70
#define THEME_PREVIEW_ICON_HEIGHT 48

static void
theme_changed_callback (gpointer callback_data)
{
	g_free (theme_from_preferences);
	theme_from_preferences = eel_preferences_get (NAUTILUS_PREFERENCES_THEME);
}

/* return the current theme by asking the preferences machinery */
char *
nautilus_theme_get_theme (void)
{
	static gboolean theme_changed_callback_installed = FALSE;

	/* Add the callback once for the life of our process */
	if (!theme_changed_callback_installed) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
						   theme_changed_callback,
						   NULL);
		theme_changed_callback_installed = TRUE;
		
		/* Peek for the first time */
		theme_changed_callback (NULL);
	}
	
	return g_strdup (theme_from_preferences);
}

/* load and parse a theme file */
static xmlDocPtr
load_theme_document (const char *theme_name)
{
	xmlDocPtr theme_document;
	char *theme_path, *temp_str;
	char *user_themes_directory;
	
	temp_str = g_strdup_printf("%s/%s.xml", theme_name, theme_name);
	theme_path = nautilus_pixmap_file (temp_str);
	g_free(temp_str);

	/* if we can't find the theme document in the global area, try in the user's home */
	if (theme_path == NULL) {
		user_themes_directory = nautilus_theme_get_user_themes_directory ();
		temp_str = g_strdup_printf("%s/%s.xml", theme_name, theme_name);
		theme_path = g_build_filename (user_themes_directory, temp_str, NULL);
		
		g_free (user_themes_directory);
		g_free (temp_str);
	
		if (!g_file_test (theme_path, G_FILE_TEST_EXISTS)) {
			g_free (theme_path);
			theme_path = NULL;
		}
	}
	
	/* if the file cannot be found, return NULL for no document */
	if (theme_path == NULL) {
		return NULL;
	}
	
	/* load and parse the theme file */
	theme_document = xmlParseFile (theme_path);
	g_free (theme_path);

	return theme_document;
}

static void
free_last_theme (void)
{
	if (last_theme_document != NULL) {
		xmlFreeDoc (last_theme_document);
	}
	g_free (last_theme_name);
}

/* Fetch data from the specified theme.  Cache the last theme file as a parsed xml document
 */
char *
nautilus_theme_get_theme_data_from_theme (const char *resource_name, const char *property_name, const char *theme_name)
{
	char *temp_str;
	char *theme_data;
	xmlDocPtr theme_document;
	xmlNodePtr resource_node;
	static gboolean did_set_up_free_last_theme = FALSE;
	
	/* fetch the current theme name */
	theme_data = NULL;
	
	if (eel_strcmp (theme_name, last_theme_name) == 0) {
		theme_document = last_theme_document;
	} else {
		/* release the old saved data, since the theme changed */
		if (!did_set_up_free_last_theme) {
			eel_debug_call_at_shutdown (free_last_theme);
			did_set_up_free_last_theme = TRUE;
		}
		free_last_theme ();
		
		last_theme_name = g_strdup (theme_name);
		last_theme_document = load_theme_document (theme_name);
		theme_document = last_theme_document;
	}
	
	if (theme_document != NULL) {
		/* fetch the resource node */			
		resource_node = eel_xml_get_child_by_name (xmlDocGetRootElement (theme_document), resource_name);
		if (resource_node != NULL) {
			temp_str = xmlGetProp (resource_node, property_name);
			if (temp_str != NULL) {
				theme_data = g_strdup (temp_str);
				xmlFree (temp_str);
			}
		}
	}
	
	return theme_data;
}

/* Fetch data from the current theme.
 */
char *
nautilus_theme_get_theme_data (const char *resource_name, const char *property_name)
{
	char *result;
	char *theme_name;
	theme_name = nautilus_theme_get_theme ();
	result = nautilus_theme_get_theme_data_from_theme (resource_name, property_name, theme_name);
	g_free (theme_name);
	return result;
}

/* utility routine to return the full path to a themed image that
   searches the local themes if it can't find it in the shared space */
static char *
nautilus_pixmap_file_may_be_local (const char *themed_image)
{
	char *image_path, *user_themes_directory;
	
	image_path = nautilus_pixmap_file (themed_image);
	if (image_path == NULL) {
		user_themes_directory = nautilus_theme_get_user_themes_directory ();
		
		image_path = g_build_filename (user_themes_directory, themed_image, NULL);
		if (!g_file_test (image_path, G_FILE_TEST_EXISTS)) {
			g_free (image_path);
			image_path = NULL;
		}
		
		g_free (user_themes_directory);
	}
	return image_path;
}

/* given a theme, fetch the full path name of an image with the passed-in name  */
/* return NULL if there isn't a corresponding image.  Optionally, add a .png suffix if we */
/* cant otherwise find one. 								  */

char *
nautilus_theme_get_image_path_from_theme (const char *image_name, const char* theme_name)
{
	char *image_path, *png_string, *temp_str;
	
	temp_str = g_strdup_printf ("%s/%s", theme_name, image_name);
	image_path = nautilus_pixmap_file_may_be_local (temp_str);
	
	/* see if a theme-specific image exists; if so, return it */
	if (image_path != NULL) {
		g_free (temp_str);	
		return image_path;
	}
	
	/* try if with a .png extension if it doesn't already have one */
	if (!eel_istr_has_suffix (image_name, ".png")) {
		png_string = g_strconcat (temp_str, ".png", NULL);
		image_path = nautilus_pixmap_file_may_be_local (png_string);
		g_free (png_string);
		
		if (image_path) {
			g_free (temp_str);	
			return image_path;
		}
	}
	g_free (temp_str);


	/* we couldn't find a theme specific one, so look for a general image */
	image_path = nautilus_pixmap_file (image_name);
	if (image_path != NULL) {
		return image_path;
	}
	
	/* if it doesn't have a .png extension, try it with that */
	if (!eel_istr_has_suffix (image_name, ".png")) {
		png_string = g_strconcat (image_name, ".png", NULL);
		image_path = nautilus_pixmap_file (png_string);
		g_free (png_string);
		
		if (image_path) {
			return image_path;
		}
	}
		
	/* we couldn't find anything, so return NULL */
	g_free (image_path);
	return NULL;
}

/* commonly used cover to get_image_path_from_theme to return an image path using the current theme */
char *
nautilus_theme_get_image_path (const char *image_name)
{
	char *theme_name, *image_path;
	
	theme_name = nautilus_theme_get_theme ();
	image_path = nautilus_theme_get_image_path_from_theme (image_name, theme_name);	
	g_free (theme_name);
	
	return image_path;
}

char *
nautilus_theme_get_user_themes_directory (void)
{
	char *user_directory;
	char *user_themes_directory;

	user_directory = nautilus_get_user_directory ();
	user_themes_directory = g_build_filename (user_directory, "themes", NULL);
	g_free (user_directory);

	return user_themes_directory;
}
