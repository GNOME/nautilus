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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <parser.h>
#include <xmlmemory.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <librsvg/rsvg.h>

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"

#include "nautilus-theme.h"

/* static globals to hold the last accessed theme file */
static char	 *last_theme_name = NULL;
static xmlDocPtr last_theme_document = NULL;

/* return the current theme by asking the preferences machinery */
char *
nautilus_theme_get_theme(void)
{
	return nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME, "default");
}

/* set the current theme */
void
nautilus_theme_set_theme(const char *new_theme)
{
	char *old_theme;
	
	old_theme = nautilus_theme_get_theme();
	if (nautilus_strcmp (old_theme, new_theme)) {
		nautilus_preferences_set (NAUTILUS_PREFERENCES_THEME, new_theme);
	}
	g_free (old_theme);
}

/* load and parse a theme file */
static xmlDocPtr
load_theme_document (const char * theme_name)
{
	xmlDocPtr theme_document;
	char *theme_path, *temp_str;
	
	/* formulate the theme path name */
	if (strcmp(theme_name, "default") == 0) {
		theme_path = nautilus_pixmap_file ("default.xml");
	} else {
		temp_str = g_strdup_printf("%s/%s.xml", theme_name, theme_name);
		theme_path = nautilus_pixmap_file (temp_str);
		g_free(temp_str);
	}
	
	/* load and parse the theme file */
	theme_document = xmlParseFile(theme_path);
	g_free(theme_path);

	return theme_document;
}

/* fetch data from the current theme.  Cache the last theme file as a parsed xml document */
char *
nautilus_theme_get_theme_data(const char *resource_name, const char *property_name)
{
	char *theme_name, *temp_str;
	char *theme_data;
	
	xmlDocPtr theme_document;
	xmlNodePtr resource_node;
	
	/* fetch the current theme name */
	theme_data = NULL;
	theme_name = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME, "default");
	if (nautilus_strcmp (theme_name, last_theme_name) == 0) {
		theme_document = last_theme_document;
	} else {
		/* release the old saved data, since the theme changed */
		if (last_theme_document)
			xmlFreeDoc (last_theme_document);
		g_free (last_theme_name);
		
		last_theme_name = g_strdup (theme_name);
		last_theme_document = load_theme_document (theme_name);
		theme_document = last_theme_document;
	}			
		
	if (theme_document != NULL) {
		/* fetch the resource node */
				
		resource_node = nautilus_xml_get_child_by_name(xmlDocGetRootElement (theme_document), resource_name);
		if (resource_node) {		
			temp_str = xmlGetProp(resource_node, property_name);
			if (temp_str)
				theme_data = g_strdup (temp_str);
		}
	}
	
	g_free(theme_name); 

	return theme_data;
}

/* given the current theme, fetch the full path name of an image with the passed-in name */
/* return NULL if there isn't a corresponding image */
char *
nautilus_theme_get_image_path (const char *image_name)
{
	char *theme_name, *image_path, *temp_str;
	
	theme_name = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME, "default");
	
	if (nautilus_strcmp (theme_name, "default") != 0) {
		temp_str = g_strdup_printf ("%s/%s", theme_name, image_name);
		image_path = nautilus_pixmap_file (temp_str);
	
		g_free (theme_name);
		g_free (temp_str);
	
		/* see if a theme-specific image exists; if so, return it */
		if (image_path && g_file_exists (image_path))
			return image_path;
		
		g_free (image_path);
	}
	
	/* we couldn't find a theme specific one, so look for a general image */
	image_path = nautilus_pixmap_file (image_name);
	
	if (image_path && g_file_exists (image_path))
		return image_path;
		
	/* we couldn't find anything, so return NULL */
	g_free (image_path);
	return NULL;
}

/* create a pixbuf that represents the passed in theme name */

GdkPixbuf *
nautilus_theme_make_selector (const char *theme_name)
{
	char *pixbuf_file, *temp_str ;
	GdkPixbuf *pixbuf;
	
	/* first, see if we can find an explicit preview */
	temp_str = g_strdup_printf ("%s/%s", theme_name, "theme_preview.png");
	pixbuf_file = nautilus_pixmap_file(temp_str);
	g_free (temp_str);
	if (pixbuf_file != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (pixbuf_file);
		g_free (pixbuf_file);
		return pixbuf;
	}
	
	/* now look for a directory */	
	temp_str = g_strdup_printf ("%s/%s", theme_name, "i-directory.png");
	pixbuf_file = nautilus_pixmap_file(temp_str);
	g_free (temp_str);
	
	if (pixbuf_file == NULL) {
		temp_str = g_strdup_printf ("%s/%s", theme_name, "i-directory.svg");
		pixbuf_file = nautilus_pixmap_file(temp_str);
		g_free (temp_str);
		if (pixbuf_file == NULL) {
			pixbuf_file = nautilus_pixmap_file ("i-directory.png");
		}	
	}
	
	/* if we can't find anything, return NULL */
	if (pixbuf_file == NULL) {
		return NULL;
	}
	
	/* load the icon that we found and return it */
	if (nautilus_istr_has_suffix(pixbuf_file, ".svg")) {
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
