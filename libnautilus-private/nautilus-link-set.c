/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.c: xml-based link file sets.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
#include "nautilus-link-set.h"

#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtkwindow.h>

#include "nautilus-file.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"

/* routine to create a new .link file in the specified directory */
static gboolean
create_new_link (const char *directory_path, const char *name, const char *image, const char *uri)
{
	xmlDocPtr output_document;
	xmlNodePtr root_node;
	char *file_name;
	int result;
	
	/* create a new xml document */
	output_document = xmlNewDoc ("1.0");
	
	/* add the root node to the output document */
	root_node = xmlNewDocNode (output_document, NULL, "NAUTILUS_OBJECT", NULL);
	xmlDocSetRootElement (output_document, root_node);
	
	xmlSetProp (root_node, "CUSTOM_ICON", image);
	xmlSetProp (root_node, "LINK", uri);
	
	/* all done, so save the xml document as a link file */
	file_name = g_strdup_printf ("%s/%s.link", directory_path, name);
	result = xmlSaveFile (file_name, output_document);
	g_free (file_name);
	
	xmlFreeDoc (output_document);

	return result > 0;
}

/* utility to return link set path */

static xmlDocPtr
get_link_set_document(const char *link_set_name)
{
	char *link_set_path;
	xmlDocPtr document;
	
	link_set_path = g_strdup_printf ("%s/share/nautilus/linksets/%s.xml",
					 NAUTILUS_PREFIX,
					 link_set_name);
	document = xmlParseFile (link_set_path);
	g_free (link_set_path);
	
	return document;
}

/* utility to expand the uri to deal with the home directory, etc. */
static char *
expand_uri(const char *uri)
{
	char *full_uri, *home_in_uri_format;
				
	if (uri[0] == '~') {
		home_in_uri_format = gnome_vfs_escape_string (g_get_home_dir (), GNOME_VFS_URI_UNSAFE_PATH);
		full_uri = g_strconcat ("file://", home_in_uri_format, uri + 1, NULL);
		g_free (home_in_uri_format);
		return full_uri;
	}
	return g_strdup(uri);
}

/* install a link set into the specified directory */

gboolean
nautilus_link_set_install (const char *directory_path, const char *link_set_name)
{
	xmlDocPtr document;
	xmlNodePtr node;
	char *link_name, *image_name, *uri, *full_uri;	

	/* load and parse the link set document */
	document = get_link_set_document(link_set_name);

	if (document == NULL) {
		return FALSE;
	}
	
	/* loop through the entries, generating .link files, or incrementing the
	   reference count if it's already there */
	
	for (node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = xmlGetProp (node, "name");
			image_name = xmlGetProp (node, "image");
			uri = xmlGetProp (node, "uri");

			/* Expand special URIs */
			full_uri = expand_uri(uri);
			
			/* create the link file */
			if (!create_new_link (directory_path, link_name, image_name, full_uri)) {
				g_free (full_uri);
				xmlFreeDoc (document);
				return FALSE;
			}
			
			g_free (full_uri);
		}
	}
	
	xmlFreeDoc (document);

	/* all done so return TRUE */
	return TRUE;
}

/* remove a link set from the specified directory */
void
nautilus_link_set_remove (const char *directory_path, const char *link_set_name)
{
	xmlDocPtr document;
	xmlNodePtr node;
	char *link_name, *file_name;	
	
	document = get_link_set_document(link_set_name);

	if (document == NULL)
		return;
 
	/* loop through the entries in the xml file, formulating the names of the link files and
	   deleting them or decrementing their reference count */
	for (node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = xmlGetProp (node, "name");
			/* formulate the link file path name */
			file_name = g_strdup_printf ("%s/%s.link", directory_path, link_name);		
			/* delete the file */
			unlink(file_name);
			g_free(link_name);
		}   
	}
	
	xmlFreeDoc (document);
}

/* utility to make a link set checkbox that installs and removes it's corresponding
   link set when toggled */

static void
make_link_set_check_box(const char *directory_path, GtkWidget *checkbox_table, int index, char *name)
{
	/* not yet implemented, coming soon */
	g_message("make check box for directory %s, name %s", directory_path, name);
}

/* utility routine t o return a list of link set names by iterating the link set directory */

static GList *
get_link_set_names()
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
	char *link_set_uri, *link_set_name, *dot_pos;
	GList *link_set_list;
	
	link_set_list = NULL;
	
	/* get a uri to the link set directory */
	link_set_uri = g_strdup_printf ("file://%s/share/nautilus/linksets",
					 NAUTILUS_PREFIX);
	
	/* get the directory info */

	result = gnome_vfs_directory_list_load (&list, link_set_uri, GNOME_VFS_FILE_INFO_GETMIMETYPE, NULL, NULL);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	/* build the list by iterating through the directory info */	
	for (current_file_info = gnome_vfs_directory_list_first(list); current_file_info != NULL; 
	    current_file_info = gnome_vfs_directory_list_next(list)) {
		link_set_name = g_strdup(current_file_info->name);
			
		/* strip file type suffix */
		dot_pos = strrchr(link_set_name, '.');
		if (dot_pos)
			*dot_pos = '\0';
			
		link_set_list = g_list_prepend(link_set_list, link_set_name);
	}

	gnome_vfs_directory_list_destroy(list);	
	g_free(link_set_uri);

	return link_set_list;	
}

/* create a window used to configure the link sets in the passed in directory */
GtkWindow *
nautilus_link_set_configure_window(const char *directory_path)
{
	char *title;
	int link_set_count, index;
	GtkWindow *window;
	GtkWidget *checkbox_table, *scrolled_window;
	GList *link_set_names, *current_link_set;

	/* Create the window. */
	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (window, FALSE, FALSE, FALSE);

	/* set the window title */
	title = g_strdup_printf (_("Linksets for %s"), directory_path);
  	gtk_window_set_title (window, title);
	g_free(title);
	
	/* fetch the link set names */
	link_set_names = get_link_set_names();	
	link_set_count = g_list_length(link_set_names);
	
	/* allocate a table to hold the link set checkboxes */
	checkbox_table = gtk_table_new ((link_set_count + 1) / 2, 2, TRUE);
	gtk_widget_show (checkbox_table);
	gtk_container_set_border_width (GTK_CONTAINER (checkbox_table), GNOME_PAD);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), 
					       checkbox_table);
	gtk_container_add(GTK_CONTAINER(window), scrolled_window);
	gtk_widget_show (scrolled_window);
	
	/* iterate the directory to add a checkbox for each linkset found */
	index = 0;
	for (current_link_set = link_set_names; current_link_set != NULL; 
		current_link_set = current_link_set->next) {
			make_link_set_check_box(directory_path, checkbox_table, 
						index++, (char*) current_link_set->data);	
	}
	
	/* clean up and we're done */
	
	nautilus_g_list_free_deep (link_set_names);
	return window;	
}


