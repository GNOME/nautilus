/* nautilus-drag.c - Common Drag & drop handling code shared by the icon container
   and the list view.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
   	    Ettore Perazzoli <ettore@gnu.org>
*/

#include <config.h>
#include "nautilus-drag.h"

#include <string.h>
#include <stdio.h>

#include "nautilus-glib-extensions.h"

/* Functions to deal with DragSelectionItems.  */

DragSelectionItem *
nautilus_drag_selection_item_new (void)
{
	return g_new0 (DragSelectionItem, 1);
}

static void
drag_selection_item_destroy (DragSelectionItem *item)
{
	g_free (item->uri);
	g_free (item);
}

void
nautilus_drag_destroy_selection_list (GList *list)
{
	GList *p;

	if (list == NULL)
		return;

	for (p = list; p != NULL; p = p->next)
		drag_selection_item_destroy (p->data);

	g_list_free (list);
}

GList *
nautilus_drag_build_selection_list (GtkSelectionData *data)
{
	GList *result;
	const guchar *p, *oldp;
	int size;

	result = NULL;
	oldp = data->data;
	size = data->length;

	while (size > 0) {
		DragSelectionItem *item;
		guint len;

		/* The list is in the form:

		   name\rx:y:width:height\r\n

		   The geometry information after the first \r is optional.  */

		/* 1: Decode name. */

		p = memchr (oldp, '\r', size);
		if (p == NULL) {
			break;
		}

		item = nautilus_drag_selection_item_new ();

		len = p - oldp;

		item->uri = g_malloc (len + 1);
		memcpy (item->uri, oldp, len);
		item->uri[len] = 0;

		p++;
		if (*p == '\n' || *p == '\0') {
			result = g_list_prepend (result, item);
			if (p == 0) {
				g_warning ("Invalid special/x-gnome-icon-list data received: "
					   "missing newline character.");
				break;
			} else {
				oldp = p + 1;
				continue;
			}
		}

		size -= p - oldp;
		oldp = p;

		/* 2: Decode geometry information.  */

		item->got_icon_position = sscanf (p, "%d:%d:%d:%d%*s",
						  &item->icon_x, &item->icon_y,
						  &item->icon_width, &item->icon_height) == 4;
		if (!item->got_icon_position) {
			g_warning ("Invalid special/x-gnome-icon-list data received: "
				   "invalid icon position specification.");
		}

		result = g_list_prepend (result, item);

		p = memchr (p, '\r', size);
		if (p == NULL || p[1] != '\n') {
			g_warning ("Invalid special/x-gnome-icon-list data received: "
				   "missing newline character.");
			if (p == NULL) {
				break;
			}
		} else {
			p += 2;
		}

		size -= p - oldp;
		oldp = p;
	}

	return result;
}

