/* nautilus-drag.h - Common Drag & drop handling code shared by the icon container
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

#ifndef NAUTILUS_DRAG_H
#define NAUTILUS_DRAG_H

#include <gtk/gtkdnd.h>

typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} DragSelectionItem;

/* Standard Drag & Drop types. */
typedef enum {
	NAUTILUS_ICON_DND_GNOME_ICON_LIST,
	NAUTILUS_ICON_DND_URI_LIST,
	NAUTILUS_ICON_DND_URL,
	NAUTILUS_ICON_DND_COLOR,
	NAUTILUS_ICON_DND_BGIMAGE,
	NAUTILUS_ICON_DND_KEYWORD
} NautilusIconDndTargetType;

/* Drag & Drop target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE "special/x-gnome-icon-list"
#define NAUTILUS_ICON_DND_URI_LIST_TYPE        "text/uri-list"
#define NAUTILUS_ICON_DND_URL_TYPE	       "_NETSCAPE_URL"
#define NAUTILUS_ICON_DND_COLOR_TYPE           "application/x-color"
#define NAUTILUS_ICON_DND_BGIMAGE_TYPE         "property/bgimage"
#define NAUTILUS_ICON_DND_KEYWORD_TYPE         "property/keyword"


DragSelectionItem *nautilus_drag_selection_item_new (void);
void nautilus_drag_destroy_selection_list (GList *list);
GList *nautilus_drag_build_selection_list (GtkSelectionData *data);

#endif
