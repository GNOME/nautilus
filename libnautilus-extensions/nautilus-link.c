/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.c: xml-based link files.
 
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

#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"

/* given a uri, returns TRUE if it's a link file */

gboolean
nautilus_link_is_link_file(const char *file_uri)
{
	if (file_uri == NULL)
		return FALSE;
		
	return nautilus_str_has_suffix(file_uri, LINK_SUFFIX);
}

/* returns additional text to display under the name, NULL if none */
char* nautilus_link_get_additional_text(const char *link_file_uri)
{
	xmlDoc *doc;
	char *extra_text = NULL;
	
	if (link_file_uri == NULL)
		return NULL;
	
	doc = xmlParseFile (link_file_uri + 7);
	if (doc) {
		extra_text = xmlGetProp (doc->root, NAUTILUS_METADATA_KEY_EXTRA_TEXT);
		xmlFreeDoc (doc);
	}  
	return extra_text;
}

/* returns the image associated with a link file */

char*
nautilus_link_get_image_uri(const char *link_file_uri)
{
	xmlDoc *doc;
	char *icon_str = NULL;
	
	if (link_file_uri == NULL)
		return NULL;
	
	doc = xmlParseFile (link_file_uri + 7);
	if (doc) {
		icon_str = xmlGetProp (doc->root, NAUTILUS_METADATA_KEY_CUSTOM_ICON);
		xmlFreeDoc (doc);
	}
	return icon_str;
}

/* returns the link uri associated with a link file */

char*		
nautilus_link_get_link_uri(const char *link_file_uri)
{
	xmlDoc *doc;
	char* result = NULL;
	
	if (link_file_uri == NULL)
		return NULL;
		
	doc = xmlParseFile (link_file_uri + 7);
	if (doc) {
		char* link_str = xmlGetProp (doc->root, "LINK");
		if (link_str) 
			result = link_str;
	
		xmlFreeDoc (doc);
	} 	
	
	if (result == NULL)
		result = g_strdup(link_file_uri);
	
	return result;
}

/* strips the suffix from the passed in string if it's a link file */
/* FIXME: don't do this at expert user levels */
char*
nautilus_link_get_display_name(char* link_file_name)
{
	if (link_file_name && nautilus_str_has_suffix(link_file_name, LINK_SUFFIX)) {
		char *suffix_pos = strstr(link_file_name, LINK_SUFFIX);
		if (suffix_pos)
			*suffix_pos = '\0';
	}
	
	return link_file_name;
}
