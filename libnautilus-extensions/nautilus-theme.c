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
#include "nautilus-preferences.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <librsvg/rsvg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* static globals to hold the last accessed and default theme files */
static char	 *last_theme_name = NULL;
static xmlDocPtr last_theme_document = NULL;
static xmlDocPtr default_theme_document = NULL;

/* return the current theme by asking the preferences machinery */
char *
nautilus_theme_get_theme (void)
{
	return nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME);
}

/* set the current theme */
void
nautilus_theme_set_theme (const char *new_theme)
{
	char *old_theme;
	
	old_theme = nautilus_theme_get_theme ();
	if (nautilus_strcmp (old_theme, new_theme)) {
		nautilus_preferences_set (NAUTILUS_PREFERENCES_THEME, new_theme);
	}
	g_free (old_theme);
}

/* load and parse a theme file */
static xmlDocPtr
load_theme_document (const char *theme_name)
{
	xmlDocPtr theme_document;
	char *theme_path, *temp_str;
	char *user_directory, *themes_directory;
	
	/* formulate the theme path name */
	if (strcmp(theme_name, "default") == 0) {
		theme_path = nautilus_pixmap_file ("default.xml");
	} else {
		temp_str = g_strdup_printf("%s/%s.xml", theme_name, theme_name);
		theme_path = nautilus_pixmap_file (temp_str);
		g_free(temp_str);
	}

	/* if we can't find the theme document in the global area, try in the user's home */
	if (theme_path == NULL) {
		user_directory = nautilus_get_user_directory ();
		themes_directory = nautilus_make_path (user_directory, "themes");
		temp_str = g_strdup_printf("%s/%s.xml", theme_name, theme_name);
		theme_path = nautilus_make_path (themes_directory, temp_str);
		
		g_free (user_directory);
		g_free (themes_directory);
		g_free (temp_str);
	
		if (!g_file_exists (theme_path)) {
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

static void
free_default_theme (void)
{
	xmlFreeDoc (default_theme_document);
}

/* Fetch data from the specified theme.  Cache the last theme file as a parsed xml document
 */
char *
nautilus_theme_get_theme_data_from_theme (const char *resource_name, const char *property_name, const char* theme_name)
{
	char *temp_str;
	char *theme_data;
	xmlDocPtr theme_document;
	xmlNodePtr resource_node;
	static gboolean did_set_up_free_last_theme = FALSE;
	
	/* fetch the current theme name */
	theme_data = NULL;
	
	if (nautilus_strcmp (theme_name, last_theme_name) == 0) {
		theme_document = last_theme_document;
	} else {
		/* release the old saved data, since the theme changed */
		if (!did_set_up_free_last_theme) {
			g_atexit (free_last_theme);
			did_set_up_free_last_theme = TRUE;
		}
		free_last_theme ();
		
		last_theme_name = g_strdup (theme_name);
		last_theme_document = load_theme_document (theme_name);
		theme_document = last_theme_document;
	}
	
	if (theme_document != NULL) {
		/* fetch the resource node */			
		resource_node = nautilus_xml_get_child_by_name (xmlDocGetRootElement (theme_document), resource_name);
		if (resource_node) {		
			temp_str = xmlGetProp(resource_node, property_name);
			if (temp_str) {
				theme_data = g_strdup (temp_str);
				xmlFree (temp_str);
			}
		}
	}
	
	/* if we couldn't find anything in the current theme, try the default theme */
	if (theme_data == NULL) {
		if (default_theme_document == NULL) {
			default_theme_document = load_theme_document ("default");
			g_atexit (free_default_theme);
		}

		resource_node = nautilus_xml_get_child_by_name (xmlDocGetRootElement (default_theme_document), resource_name);
		if (resource_node) {		
			temp_str = xmlGetProp (resource_node, property_name);
			if (temp_str) {
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
	theme_name = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME);
	result = nautilus_theme_get_theme_data_from_theme (resource_name, property_name, theme_name);
	g_free (theme_name);
	return result;
}

/* utility routine to return the full path to a themed image that
   searches the local themes if it can't find it in the shared space */
static char *
nautilus_pixmap_file_may_be_local (const char *themed_image)
{
	char *image_path, *user_directory, *themes_directory;
	
	image_path = nautilus_pixmap_file (themed_image);
	if (image_path == NULL) {
		user_directory = nautilus_get_user_directory ();
		themes_directory = nautilus_make_path (user_directory, "themes");
		
		image_path = nautilus_make_path (themes_directory, themed_image);
		if (!g_file_exists (image_path)) {
			g_free (image_path);
			image_path = NULL;
		}
		
		g_free (user_directory);
		g_free (themes_directory);
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
	
	if (nautilus_strcmp (theme_name, "default") != 0) {
		temp_str = g_strdup_printf ("%s/%s", theme_name, image_name);
		image_path = nautilus_pixmap_file_may_be_local (temp_str);
		
		/* see if a theme-specific image exists; if so, return it */
		if (image_path) {
			g_free (temp_str);	
			return image_path;
		}
		
		/* try if with a .png extension if it doesn't already have one */
		if (!nautilus_istr_has_suffix (image_name, ".png")) {
			png_string = g_strconcat (temp_str, ".png", NULL);
			image_path = nautilus_pixmap_file_may_be_local (png_string);
			g_free (png_string);
			
			if (image_path) {
				g_free (temp_str);	
				return image_path;
			}
		}		
		g_free (temp_str);
	}
	
	/* we couldn't find a theme specific one, so look for a general image */
	image_path = nautilus_pixmap_file (image_name);
	
	if (image_path) {
		return image_path;
	}
	
	/* if it doesn't have a .png extension, try it with that */
	if (!nautilus_istr_has_suffix (image_name, ".png")) {
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
	
	theme_name = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME);	
	image_path = nautilus_theme_get_image_path_from_theme (image_name, theme_name);	
	g_free (theme_name);
	
	return image_path;
}

/* create a pixbuf that represents the passed in theme name */

GdkPixbuf *
nautilus_theme_make_selector (const char *theme_name)
{
	char *pixbuf_file, *theme_preview_name;
	char *user_directory, *directory_uri;
	GdkPixbuf *pixbuf;
	
	/* first, see if we can find an explicit preview */
	if (!nautilus_strcmp (theme_name, "default")) {
		theme_preview_name = g_strdup ("theme_preview.png");
	} else {
		theme_preview_name = g_strdup_printf ("%s/%s", theme_name, "theme_preview.png");
	}
	
	pixbuf_file = nautilus_pixmap_file (theme_preview_name);
	if (pixbuf_file != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (pixbuf_file);
		g_free (pixbuf_file);
		return pixbuf;
	} else {
		/* try the user directory */
		user_directory = nautilus_get_user_directory ();
		directory_uri = nautilus_make_path (user_directory, "themes");
		pixbuf_file = nautilus_make_path (directory_uri, theme_preview_name);
		
		g_free (user_directory);
		g_free (directory_uri);
		
		if (g_file_exists (pixbuf_file)) {
			pixbuf = gdk_pixbuf_new_from_file (pixbuf_file);
			g_free (pixbuf_file);
			return pixbuf;
		}  else {
			g_free (pixbuf_file);
		}
	}
	
	/* couldn't find a custom one, so try for a directory */	
	g_free (theme_preview_name);
	theme_preview_name = g_strdup_printf ("%s/%s", theme_name, "i-directory.png");
	pixbuf_file = nautilus_pixmap_file (theme_preview_name);
	g_free (theme_preview_name);
	
	if (pixbuf_file == NULL) {
		theme_preview_name = g_strdup_printf ("%s/%s", theme_name, "i-directory.svg");
		pixbuf_file = nautilus_pixmap_file (theme_preview_name);
		g_free (theme_preview_name);
	}
	
	/* try the user directory if necessary */
	if (pixbuf_file == NULL) {
		user_directory = nautilus_get_user_directory ();
		directory_uri = nautilus_make_path (user_directory, "themes");
		theme_preview_name = g_strdup_printf ("%s/i-directory.png", theme_name);
		pixbuf_file = nautilus_make_path (directory_uri, theme_preview_name);
		g_free (theme_preview_name);
		
		if (!g_file_exists (pixbuf_file)) {
			g_free (pixbuf_file);
			theme_preview_name = g_strdup_printf ("%s/i-directory.svg", theme_name);
			pixbuf_file = nautilus_make_path (directory_uri, theme_preview_name);
			g_free (theme_preview_name);
		
			if (!g_file_exists (pixbuf_file)) {
				g_free (pixbuf_file);
				pixbuf_file = NULL;
			}
		}
		
		g_free (user_directory);
		g_free (directory_uri);		
	}
	
	/* if we can't find anything, return NULL */
	if (pixbuf_file == NULL) {
		return NULL;
	}
	
	pixbuf = NULL;
	
	/* load the icon that we found and return it */
	if (nautilus_istr_has_suffix (pixbuf_file, ".svg")) {
		FILE *f = fopen (pixbuf_file, "rb");
		if (f != NULL) {
			pixbuf = rsvg_render_file (f, 1.0);
			fclose (f);
		}
	
	} else {
		pixbuf = gdk_pixbuf_new_from_file (pixbuf_file);
	}
	
	g_free (pixbuf_file);
	return pixbuf;
}
