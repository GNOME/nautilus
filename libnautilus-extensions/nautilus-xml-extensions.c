/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-xml-extensions.c - functions that extend gnome-xml

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-xml-extensions.h"

#include <glib.h>
#include "nautilus-string.h"
#include <stdlib.h>
#include <xmlmemory.h>

xmlNodePtr
nautilus_xml_get_children (xmlNodePtr parent)
{
	if (parent == NULL) {
		return NULL;
	}
	return parent->childs;
}

xmlNodePtr
nautilus_xml_get_root_children (xmlDocPtr document)
{
	return nautilus_xml_get_children (xmlDocGetRootElement (document));
}

xmlNodePtr
nautilus_xml_get_child_by_name_and_property (xmlNodePtr parent,
					     const char *child_name,
					     const char *property_name,
					     const char *property_value)
{
	xmlNodePtr child;
	xmlChar *property;
	gboolean match;

	if (parent == NULL) {
		return NULL;
	}
	for (child = nautilus_xml_get_children (parent); child != NULL; child = child->next) {
		if (strcmp (child->name, child_name) == 0) {
			property = xmlGetProp (child, property_name);
			match = nautilus_strcmp (property, property_value) == 0;
			xmlFree (property);
			if (match) {
				return child;
			}
		}
	}
	return NULL;
}

xmlNodePtr
nautilus_xml_get_root_child_by_name_and_property (xmlDocPtr document,
						  const char *child_name,
						  const char *property_name,
						  const char *property_value)
{
	return nautilus_xml_get_child_by_name_and_property
		(xmlDocGetRootElement (document),
		 child_name,
		 property_name,
		 property_value);
}
