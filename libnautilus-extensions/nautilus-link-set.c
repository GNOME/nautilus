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

#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktogglebutton.h>

#include "nautilus-file.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"
#include "nautilus-file-utilities.h"

/* utility routine to build the link set path name */

static char *
link_set_path_name (const char *directory_path, const char *name)
{
	const char *path_start;

	/* FIXME: This hack is unacceptable. Either it's a URI and the
	 * file:// must be removed with the function that does that,
	 * or it's a path, and there's no reason to remove the prefix.
	 */
	if (nautilus_str_has_prefix(directory_path, "file://"))
		path_start = directory_path + 7;
	else
		path_start = directory_path;
	return g_strdup_printf ("%s/%s", path_start, name);
}

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
	root_node = xmlNewDocNode (output_document, NULL, "NAUTILUS_OBJECT", NULL);
	xmlDocSetRootElement (output_document, root_node);

	/* Add mime magic string so that the mime sniffer can recognize us.
	 * Note: The value of the tag has no meaning.  */
	xmlSetProp (root_node, "NAUTILUS_LINK", "Nautilus Link");

	/* Add link and custom icon tags */
	xmlSetProp (root_node, "CUSTOM_ICON", image);
	xmlSetProp (root_node, "LINK", uri);
	
	/* all done, so save the xml document as a link file */
	file_name = link_set_path_name (directory_path, name);
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

/* utility to expand the uri to deal with the home directory, etc. */
static char *
expand_uri (const char *uri)
{
	/* FIXME: This turns "~x" into "HOME", which is bad. */
	if (uri[0] == '~') {
		return nautilus_get_uri_from_local_path (g_get_home_dir ());
	}
	return g_strdup (uri);
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
		g_warning("couldnt load %s", link_set_name);
		return FALSE;
	}
	
	/* loop through the entries, generating link files */
	
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

/* test to see if a link set is installed.  Return TRUE if all of the members are installed, false otherwise */

gboolean
nautilus_link_set_is_installed (const char *directory_path, const char *link_set_name)
{
	xmlDocPtr document;
	xmlNodePtr node;
	char *link_name, *file_name;	

	/* load and parse the link set document */
	document = get_link_set_document(link_set_name);
	
	if (document == NULL) {
		g_warning("couldnt load %s", link_set_name);
		return FALSE;
	}
	
	/* loop through the entries, testing to see if any are present */
	for (node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = xmlGetProp (node, "name");
			file_name = link_set_path_name (directory_path, link_name);
			if (!g_file_exists(file_name)) {
				g_free(file_name);
				return FALSE;
			}
			g_free(file_name);
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
		g_message("couldnt load %s", link_set_name);
		return;
	}
	
	/* loop through the entries in the xml file, formulating the names of the link files and
	   deleting them or decrementing their reference count */
	for (node = nautilus_xml_get_children (xmlDocGetRootElement (document));
	     node != NULL; node = node->next) {
		if (strcmp (node->name, "link") == 0) {
			link_name = xmlGetProp (node, "name");
			/* formulate the link file path name */
			file_name = link_set_path_name (directory_path, link_name); 
			/* delete the file */
			unlink(file_name);
			g_free(link_name);
		}   
	}
	
	xmlFreeDoc (document);
}
