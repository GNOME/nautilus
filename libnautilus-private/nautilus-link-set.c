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
#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include "nautilus-file.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-widgets/nautilus-preferences.h"

#include "nautilus-link-set.h"

/* routine to create a new .link file in the specified directory */
static void
create_new_link(const char *directory_path, const char *name, const char *image, const char *uri)
{
	xmlDocPtr output_document;
	xmlNodePtr root_node;
	char *file_name;
	int result;
		
	/* create a new xml document */
	output_document =  xmlNewDoc ("1.0");
	
	/* add the root node to the output document */
	root_node = xmlNewDocNode (output_document, NULL, "NAUTILUS_OBJECT", NULL);
	xmlDocSetRootElement (output_document, root_node);
	
	xmlSetProp (root_node, "CUSTOM_ICON", image);
	xmlSetProp (root_node, "LINK", uri);
	
	/* all done , so save the xml document as a link file */
	file_name = g_strdup_printf("%s/%s.link", directory_path, name);
	result = xmlSaveFile(file_name, output_document);
	g_free(file_name);
	
	xmlFreeDoc(output_document);	
}

/* install a link set into the specified directory */

gboolean
nautilus_link_set_install (const char *directory_path, const char *link_set_name)
{
	NautilusFile *file;
	xmlDocPtr document;
	xmlNodePtr cur_node;
	char *temp_str, *link_set_path;
	char *link_name, *image_name, *uri_name;	

	file = nautilus_file_get(directory_path);
	if (file == NULL)
		return FALSE;

	/* make sure the target is a writable directory */

	if (!nautilus_file_is_directory(file) || !nautilus_file_can_write(file)) {
		nautilus_file_unref(file);
		return FALSE;
	}
	
	/* compose the path of the link set file */
	temp_str = g_strdup_printf ("nautilus/linksets/%s.xml", link_set_name);
	link_set_path = gnome_datadir_file (temp_str);

	/* load and parse the linkset xml file */
	document = xmlParseFile (link_set_path);
	g_free(temp_str);
	g_free(link_set_path);
	
	if (document == NULL)
		return FALSE;
	
	/* loop through the entries, generating .link files */
	for (cur_node = document->root->childs; cur_node != NULL; cur_node = cur_node->next) {
		if (strcmp(cur_node->name, "link") == 0) {
			link_name =  xmlGetProp (cur_node, "name");
			image_name =  xmlGetProp (cur_node, "image");
			uri_name =  xmlGetProp (cur_node, "uri");
			create_new_link(directory_path, link_name, image_name, uri_name);
		}
	}
	
	/* all done so return TRUE */
	xmlFreeDoc(document);
	return TRUE;

}

/* remove a link set from the specified directory */
void
nautilus_link_set_remove	(const char *directory_uri, const char *link_set_name)
{
}

