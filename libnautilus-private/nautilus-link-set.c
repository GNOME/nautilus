/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-set.c: xml-based link file sets.
 
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

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>
#include <unistd.h>

/* routine to create a new link file in the specified directory */
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
	root_node = xmlNewDocNode (output_document, NULL, "nautilus_object", NULL);
	xmlDocSetRootElement (output_document, root_node);

	/* Add mime magic string so that the mime sniffer can recognize us.
	 * Note: The value of the tag has no meaning.  */
	xmlSetProp (root_node, "nautilus_link", "Nautilus Link");

	/* Add link and custom icon tags */
	xmlSetProp (root_node, "custom_icon", image);
	xmlSetProp (root_node, "link", uri);
	
	/* all done, so save the xml document as a link file */
	file_name = nautilus_make_path (directory_path, name);
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
	
	link_set_path = g_strdup_printf ("%s/linksets/%s.xml",
					 NAUTILUS_DATADIR,
					 link_set_name);
	document = xmlParseFile (link_set_path);
	g_free (link_set_path);
	
	return document;
}

/* install a link set into the specified directory */

gboolean
nautilus_link_set_install (const char *directory_path, const char *link_set_name)
{
	xmlDocPtr document;
	xmlNodePtr node;
	char *link_name, *image_name, *uri, *full_uri;
	gboolean created;

	/* load and parse the link set document */
	document = get_link_set_document (link_set_name);
	if (document == NULL) {
		g_warning ("could not load %s", link_set_name);
		return FALSE;
	}
	
	/* loop through the entries, generating link files */
	for (node = eel_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = eel_xml_get_property_translated (node, "name");
			image_name = xmlGetProp (node, "image");
			uri = xmlGetProp (node, "uri");

			/* Expand special URIs */
			full_uri = gnome_vfs_expand_initial_tilde (uri);
			
			/* create the link file */
			created = create_new_link (directory_path, link_name,
						   image_name, full_uri);
			
			xmlFree (link_name);
			xmlFree (image_name);
			xmlFree (uri);
			g_free (full_uri);

			if (!created) {
				xmlFreeDoc (document);
				return FALSE;
			}
		}
	}
	
	xmlFreeDoc (document);
	return TRUE;
}

/* test to see if a link set is installed.  Return TRUE if all of the members are installed, false otherwise */

gboolean
nautilus_link_set_is_installed (const char *directory_path, const char *link_set_name)
{
	xmlDocPtr document;
	xmlNodePtr node;
	char *link_name, *file_name;	

	/* load and parse the link set document */
	document = get_link_set_document (link_set_name);
	if (document == NULL) {
		g_warning ("could not load %s", link_set_name);
		return FALSE;
	}
	
	/* loop through the entries, testing to see if any are present */
	for (node = eel_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = eel_xml_get_property_translated (node, "name");
			file_name = nautilus_make_path (directory_path, link_name);
			xmlFree (link_name);

			if (!g_file_exists (file_name)) {
				g_free (file_name);
				xmlFreeDoc (document);
				return FALSE;
			}
			g_free (file_name);
		}
	}
	
	xmlFreeDoc (document);
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
	if (document == NULL) {	
		g_message ("could not load %s", link_set_name);
		return;
	}
	
	/* loop through the entries in the xml file, formulating the names of the link files and
	   deleting them or decrementing their reference count */
	for (node = eel_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			/* formulate the link file path name */
			link_name = eel_xml_get_property_translated (node, "name");
			file_name = nautilus_make_path (directory_path, link_name);
			xmlFree (link_name);
			
			/* delete the file */
			unlink (file_name);
			g_free (link_name);
		}   
	}
	
	xmlFreeDoc (document);
}
