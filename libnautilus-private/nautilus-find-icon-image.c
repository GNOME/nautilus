/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-find-icon-image.c: Functions that locate icon image files,
                               used internally by the icon factory.
 
   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 1999, 2000 Eazel, Inc.
   Copyright (C) 2001 Free Software Foundation, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-find-icon-image.h"

#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libgnome/gnome-util.h>
#include <libxml/parser.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <string.h>

/* List of suffixes to search when looking for an icon file. */
static const char *icon_file_name_suffixes[] =
{
	".svg",
	".png",
	".jpg",
};

static char *
make_full_icon_path (const char *path,
		     const char *suffix,
		     gboolean theme_is_in_user_directory,
		     gboolean document_type_icon)
{
	char *partial_path, *full_path;
	char *user_directory, *themes_directory;
	
	partial_path = g_strconcat (path, suffix, NULL);

	if (path[0] == '/' && g_file_test (partial_path, G_FILE_TEST_EXISTS)) {
		return partial_path;
	}

	/* Build a path for this icon, depending on the theme_is_in_user_directory boolean. */
	if (theme_is_in_user_directory) {
		user_directory = nautilus_get_user_directory ();
		themes_directory = g_build_filename (user_directory, "themes", NULL);
		full_path = g_build_filename (themes_directory, partial_path, NULL);
		g_free (user_directory);
		g_free (themes_directory);

		if (!g_file_test (full_path, G_FILE_TEST_EXISTS)) {
			g_free (full_path);
			full_path = NULL;
		}
	} else if (document_type_icon) {
		full_path = g_build_filename (DATADIR "/pixmaps/document-icons/", partial_path, NULL);
		if (!g_file_test (full_path, G_FILE_TEST_EXISTS)) {
			g_free (full_path);
			full_path = NULL;
		}
	} else {
		full_path = nautilus_pixmap_file (partial_path);
	}
	
	if (full_path == NULL) {
		full_path = gnome_vfs_icon_path_from_filename (partial_path);
	}



	g_free (partial_path);
	return full_path;
}

/* utility routine to parse the attach points string to set up the array in icon_info */
static void
parse_attach_points (NautilusEmblemAttachPoints *attach_points,
		     const char *attach_point_string)
{
	char **point_array;
	char c;
	int i, x_offset, y_offset;

	attach_points->num_points = 0;
	if (attach_point_string == NULL) {
		return;
	}
			
	/* Split the attach point string into a string array, then process
	 * each point with sscanf in a loop.
	 */
	point_array = g_strsplit (attach_point_string, "|", MAX_ATTACH_POINTS); 
	
	for (i = 0; point_array[i] != NULL; i++) {
		if (sscanf (point_array[i], " %d , %d %c", &x_offset, &y_offset, &c) == 2) {
			attach_points->points[attach_points->num_points].x = x_offset;
			attach_points->points[attach_points->num_points].y = y_offset;
			attach_points->num_points++;
		} else {
			g_warning ("bad attach point specification: %s", point_array[i]);
		}
	}

	g_strfreev (point_array);
}

static void
read_details (const char *path,
	      guint icon_size,
	      NautilusIconDetails *details)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *size_as_string, *property;
	ArtIRect parsed_rect;
	char c;

	memset (&details->text_rect, 0, sizeof (details->text_rect));
	
	if (path == NULL) {
		return;
	}

	doc = xmlParseFile (path);

	if (icon_size == 0) {
		size_as_string = g_strdup ("*");
	} else {
		size_as_string = g_strdup_printf ("%u", icon_size);
	}
	node = eel_xml_get_root_child_by_name_and_property
		(doc, "icon", "size", size_as_string);
	g_free (size_as_string);
	
	property = xmlGetProp (node, "embedded_text_rectangle");
	
	if (property != NULL) {
		if (sscanf (property,
			    " %d , %d , %d , %d %c",
			    &parsed_rect.x0,
			    &parsed_rect.y0,
			    &parsed_rect.x1,
			    &parsed_rect.y1,
			    &c) == 4) {
			details->text_rect = parsed_rect;
		}
		xmlFree (property);
	}
	
	property = xmlGetProp (node, "attach_points");
	
	parse_attach_points (&details->attach_points, property);	
	xmlFree (property);
	
	xmlFreeDoc (doc);
}

/* Pick a particular icon to use, trying all the various suffixes.
 * Return the path of the icon or NULL if no icon is found.
 */
static char *
get_themed_icon_file_path (const NautilusIconTheme *icon_theme,
			   const char *icon_name,
			   guint icon_size,
			   NautilusIconDetails *details,
			   NautilusIconDetails *scalable_details)
{
	guint i;
	gboolean include_size, in_user_directory, document_type_icon;
	char *themed_icon_name, *partial_path, *path, *xml_path;
	
	g_assert (icon_name != NULL);

	if (icon_name[0] == '/') {
		themed_icon_name = g_strdup (icon_name);
		in_user_directory = FALSE;
		document_type_icon = FALSE;
	} else if (icon_theme == NULL || icon_theme->name == NULL) {
		themed_icon_name = g_strdup (icon_name);
		in_user_directory = FALSE;
		document_type_icon = TRUE;
	} else {
		themed_icon_name = g_strconcat (icon_theme->name, "/", icon_name, NULL);
		in_user_directory = icon_theme->is_in_user_directory;
		document_type_icon = FALSE;
	}

	include_size = icon_size != NAUTILUS_ICON_SIZE_STANDARD;
	
	/* Try each suffix. */
	for (i = 0; i < G_N_ELEMENTS (icon_file_name_suffixes); i++) {
		if (include_size && g_ascii_strcasecmp (icon_file_name_suffixes[i], ".svg") != 0) {
			/* Build a path for this icon. */
			partial_path = g_strdup_printf ("%s-%u",
							themed_icon_name,
							icon_size);
		} else {
			partial_path = g_strdup (themed_icon_name);
		}
		

		path = make_full_icon_path (partial_path,
					    icon_file_name_suffixes[i],
					    in_user_directory,
					    document_type_icon);
		g_free (partial_path);

		/* Return the path if the file exists. */
		if (path != NULL) {
			break;
		}

		g_free (path);
		path = NULL;
	}

	/* Open the XML file to get the text rectangle and emblem attach points */
	if (path != NULL && details != NULL) {
		xml_path = make_full_icon_path (themed_icon_name,
						".xml",
						in_user_directory,
						document_type_icon);
		read_details (xml_path, icon_size, details);
		if (scalable_details) {
			read_details (xml_path, 0, scalable_details);
		}
		g_free (xml_path);
	}

	g_free (themed_icon_name);

	return path;
}

static gboolean
theme_has_icon (const NautilusIconTheme *theme,
		const char *name)
{
	char *path;

	if (theme == NULL || theme->name == NULL) {
		return FALSE;
	}

	path = get_themed_icon_file_path (theme, name,
					  NAUTILUS_ICON_SIZE_STANDARD,
					  NULL, NULL);
	g_free (path);

	return path != NULL;
}

/* Check and see if there is a theme icon to use. If there's a
 * fallback theme specified, try it, too. This decision is based on
 * whether there's a non-size-specific theme icon.
 */
static const NautilusIconTheme *
choose_theme (const NautilusIconThemeSpecifications *theme_specs,
	      const char *name)
{
	if (name[0] == '/') {
		return NULL;
	}

	if (theme_has_icon (&theme_specs->current, name)) {
		return &theme_specs->current;
	}

	if (theme_has_icon (&theme_specs->fallback, name)) {
		return &theme_specs->fallback;
	}

	return NULL;
}

/* Sick hack. If we still haven't found anything, and we're looking
 * for an emblem, check out the emblems area in the user's home
 * directory, since it might be an emblem that they've added there.
 */
static char *
get_user_emblem_path (const char *name, guint icon_size)
{
	char *path, *user_directory;
	guint i;

	if (icon_size != NAUTILUS_ICON_SIZE_STANDARD) {
		return FALSE;
	}

	if (!eel_str_has_prefix (name, NAUTILUS_EMBLEM_NAME_PREFIX)) {
		return FALSE;
	}

	user_directory = nautilus_get_user_directory ();
	path = NULL;
	for (i = 0; i < G_N_ELEMENTS (icon_file_name_suffixes); i++) {
		path = g_strdup_printf ("%s/emblems/%s%s", 
					user_directory,
					name + strlen (NAUTILUS_EMBLEM_NAME_PREFIX), 
					icon_file_name_suffixes[i]);
		if (g_file_test (path, G_FILE_TEST_EXISTS)) {
			break;
		}
		g_free (path);
		path = NULL;
	}
	g_free (user_directory);

	return path;
}

/* Choose the file name to load, taking into account theme
 * vs. non-theme icons. Also fill in info in the icon structure based
 * on what's found in the XML file.
 */
char *
nautilus_get_icon_file_name (const NautilusIconThemeSpecifications *theme_specs,
			     const char *name,
			     const char *modifier,
			     guint size,
			     NautilusIconDetails *details,
			     NautilusIconDetails *scalable_details)
{
	const NautilusIconTheme *theme;
	char *path;
	char *name_with_modifier;
	
	if (details != NULL) {
		memset (details, 0, sizeof (*details));
	}

	if (name == NULL) {
		return NULL;
	}

	theme = choose_theme (theme_specs, name);

	/* If there's a modifier, try the modified icon first. */
	if (modifier != NULL && modifier[0] != '\0') {
		name_with_modifier = g_strconcat (name, "-", modifier, NULL);
		path = get_themed_icon_file_path (theme, name_with_modifier, size, details, scalable_details);
		g_free (name_with_modifier);
		if (path != NULL) {
			return path;
		}
	}
	
	/* Check for a normal icon. */
	path = get_themed_icon_file_path (theme, name, size, details, scalable_details);
	if (path != NULL) {
		return path;
	}

	/* Check for a user emblem. */
	path = get_user_emblem_path (name, size);
	if (path != NULL) {
		return path;
	}

	return NULL;
}

char *
nautilus_remove_icon_file_name_suffix (const char *icon_name)
{
	guint i;
	const char *suffix;

	for (i = 0; i < G_N_ELEMENTS (icon_file_name_suffixes); i++) {
		suffix = icon_file_name_suffixes[i];
		if (eel_str_has_suffix (icon_name, suffix)) {
			return eel_str_strip_trailing_str (icon_name, suffix);
		}
	}
	return g_strdup (icon_name);
}
