/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-parsing.c - routines to parse bookmarks from XML data.
 */

#include <config.h>
#include "nautilus-bookmark-parsing.h"

#include <eel/eel-xml-extensions.h>
#include <libxml/xmlmemory.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <stdlib.h>

NautilusBookmark *
nautilus_bookmark_new_from_node (xmlNodePtr node)
{
	xmlChar *name, *uri;
	xmlChar *icon_uri, *icon_mime_type, *icon_name;
	NautilusScalableIcon *icon;
	NautilusBookmark *new_bookmark;

	/* Maybe should only accept bookmarks with both a name and uri? */
	name = eel_xml_get_property_translated (node, "name");
	uri = xmlGetProp (node, "uri");
	icon_uri = xmlGetProp (node, "icon_uri");
	icon_mime_type = xmlGetProp (node, "icon_mime_type");
	icon_name = xmlGetProp (node, "icon_name");

	if (icon_uri == NULL && icon_name == NULL) {
		icon = NULL;
	} else {
		icon = nautilus_scalable_icon_new_from_text_pieces
			(icon_uri, icon_mime_type, icon_name, NULL, NULL);
	}
	new_bookmark = nautilus_bookmark_new_with_icon (uri, name, icon);
	if (icon != NULL) {
		nautilus_scalable_icon_unref (icon);
	}

	xmlFree (name);
	xmlFree (uri);
	xmlFree (icon_uri);
	xmlFree (icon_name);

	return new_bookmark;
}
