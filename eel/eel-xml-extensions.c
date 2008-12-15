/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-xml-extensions.c - functions that extend gnome-xml

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
#include "eel-xml-extensions.h"

#include "eel-string.h"
#include "eel-i18n.h"
#include <glib.h>
#include <libxml/parser.h>
#include <stdlib.h>

xmlNodePtr
eel_xml_get_children (xmlNodePtr parent)
{
	if (parent == NULL) {
		return NULL;
	}
	return parent->children;
}

xmlNodePtr
eel_xml_get_root_children (xmlDocPtr document)
{
	return eel_xml_get_children (xmlDocGetRootElement (document));
}

xmlNodePtr
eel_xml_get_child_by_name_and_property (xmlNodePtr parent,
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
	for (child = eel_xml_get_children (parent); child != NULL; child = child->next) {
		if (strcmp (child->name, child_name) == 0) {
			property = xmlGetProp (child, property_name);
			match = eel_strcmp (property, property_value) == 0;
			xmlFree (property);
			if (match) {
				return child;
			}
		}
	}
	return NULL;
}

/* return a child of the passed-in node with a matching name */

xmlNodePtr
eel_xml_get_child_by_name (xmlNodePtr parent,
					     const char *child_name)
{
	xmlNodePtr child;

	if (parent == NULL) {
		return NULL;
	}
	for (child = eel_xml_get_children (parent); child != NULL; child = child->next) {
		if (strcmp (child->name, child_name) == 0) {
			return child;
		}
	}
	return NULL;
}


xmlNodePtr
eel_xml_get_root_child_by_name_and_property (xmlDocPtr document,
						  const char *child_name,
						  const char *property_name,
						  const char *property_value)
{
	return eel_xml_get_child_by_name_and_property
		(xmlDocGetRootElement (document),
		 child_name,
		 property_name,
		 property_value);
}

/**
 * eel_xml_get_property_for_children
 * 
 * Returns a list of the values for the specified property for all
 * children of the node that have the specified name.
 *
 * @parent:     xmlNodePtr representing the node in question.
 * @child_name: child element name to look for
 * @property:   name of propety to reutnr for matching children that have the property
 * 
 * Returns: A list of keywords.
 * 
 **/
GList *
eel_xml_get_property_for_children (xmlNodePtr parent,
					const char *child_name,
					const char *property_name)
{
	GList *properties;
	xmlNode *child;
	xmlChar *property;
	
	properties = NULL;
	
	for (child = eel_xml_get_children (parent);
	     child != NULL;
	     child = child->next) {
		if (strcmp (child->name, child_name) == 0) {
			property = xmlGetProp (child, property_name);
			if (property != NULL) {
				properties = g_list_prepend (properties,
							     g_strdup (property));
				xmlFree (property);
			}
		}
	}

	/* Reverse so you get them in the same order as the XML file. */
	return g_list_reverse (properties);
}

xmlChar *
eel_xml_get_property_translated (xmlNodePtr parent,
				      const char *property_name)
{
	xmlChar *property, *untranslated_property;
	char *untranslated_property_name;
	const char *translated_property;

	/* Try for the already-translated version. */
	property = xmlGetProp (parent, property_name);
	if (property != NULL) {
		return property;
	}

	/* Try for the untranslated version. */
	untranslated_property_name = g_strconcat ("_", property_name, NULL);
	untranslated_property = xmlGetProp (parent, untranslated_property_name);
	g_free (untranslated_property_name);
	if (untranslated_property == NULL) {
		return NULL;
	}

	/* Try to translate. */
	translated_property = gettext (untranslated_property);

	/* If not translation is found, return untranslated property as-is. */
	if (translated_property == (char *) untranslated_property) {
		return untranslated_property;
	}
	
	/* If a translation happened, make a copy to match the normal
	 * behavior of this function (returning a string you xmlFree).
	 */
	xmlFree (untranslated_property);
	return xmlStrdup (translated_property);
}
