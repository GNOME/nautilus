/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark-parsing.c - routines to parse bookmarks from XML data.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-bookmark-parsing.h"

#include <libnautilus-extensions/nautilus-icon-factory.h>

#include <xmlmemory.h>

NautilusBookmark *
nautilus_bookmark_new_from_node (xmlNodePtr node)
{
	xmlChar *xml_name;
	xmlChar *xml_uri;
	xmlChar *xml_icon_uri;
	xmlChar *xml_icon_name;
	NautilusScalableIcon *icon;
	NautilusBookmark *new_bookmark;

	/* Maybe should only accept bookmarks with both a name and uri? */
	xml_name = xmlGetProp (node, "name");
	xml_uri = xmlGetProp (node, "uri");
	xml_icon_uri = xmlGetProp (node, "icon_uri");
	xml_icon_name = xmlGetProp (node, "icon_name");

	if (xml_icon_uri == NULL && xml_icon_name == NULL) {
		icon = NULL;
	} else {
		icon = nautilus_scalable_icon_new_from_text_pieces
			(xml_icon_uri, xml_icon_name, NULL, NULL, FALSE);
	}
	new_bookmark = nautilus_bookmark_new_with_icon (xml_uri, xml_name, icon);

	xmlFree (xml_name);
	xmlFree (xml_uri);
	xmlFree (xml_icon_uri);
	xmlFree (xml_icon_name);

	return new_bookmark;
}
