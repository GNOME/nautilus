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
#include "nautilus-file.h"

/* Item of the drag selection list */
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

/* DnD-related information. */
typedef struct {
	GtkTargetList *target_list;

	/* Stuff saved at "receive data" time needed later in the drag. */
	gboolean got_drop_data_type;
	NautilusIconDndTargetType data_type;
	GtkSelectionData *selection_data;

	/* Start of the drag, in world coordinates. */
	gdouble start_x, start_y;

	/* List of DndSelectionItems, representing items being dragged, or NULL
	 * if data about them has not been received from the source yet.
	 */
	GList *selection_list;

	/* Stipple for drawing icon shadows during DnD.  */
	GdkBitmap *stipple;

} NautilusDragInfo;


/* Drag & Drop target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE "x-special/gnome-icon-list"
#define NAUTILUS_ICON_DND_URI_LIST_TYPE        "text/uri-list"
#define NAUTILUS_ICON_DND_URL_TYPE	       "_NETSCAPE_URL"
#define NAUTILUS_ICON_DND_COLOR_TYPE           "application/x-color"
#define NAUTILUS_ICON_DND_BGIMAGE_TYPE         "property/bgimage"
#define NAUTILUS_ICON_DND_KEYWORD_TYPE         "property/keyword"

typedef void 		(* NautilusDragEachSelectedItemDataGet)		(const char *url, 
									 int x, int y, int w, int h, 
									 gpointer data);
typedef void 		(* NautilusDragEachSelectedItemIterator)	(NautilusDragEachSelectedItemDataGet iteratee, 
									 gpointer iterator_context, 
									 gpointer data);

void 			nautilus_drag_init 				(NautilusDragInfo *drag_info,
								 	 const GtkTargetEntry *drag_types, 
								 	 int drag_type_count, 
								 	 GdkBitmap *stipple);

void			nautilus_drag_finalize 				(NautilusDragInfo *drag_info);


DragSelectionItem 	*nautilus_drag_selection_item_new 		(void);
void 			nautilus_drag_destroy_selection_list 		(GList *selection_list);
GList 			*nautilus_drag_build_selection_list 		(GtkSelectionData *data);
gboolean		nautilus_drag_items_local	 		(const char *target_uri,
								 	 const GList *selection_list);

gboolean		nautilus_drag_can_accept_item 			(NautilusFile *drop_target_item,
			       					 	 const char *item_uri);
gboolean		nautilus_drag_can_accept_items 			(NautilusFile *drop_target_item,
								 	 const GList *items);
void			nautilus_drag_default_drop_action_for_icons	(GdkDragContext *context,
								 	 const char *target_uri,
								 	 const GList *items,
								 	 int *default_action,
								 	 int *non_default_action);

gboolean 		nautilus_drag_drag_data_get 			(GtkWidget *widget,
								 	 GdkDragContext *context,
									 GtkSelectionData *selection_data,
									 guint info,
									 guint32 time,
									 gpointer container_context,
									 NautilusDragEachSelectedItemIterator each_selected_item_iterator);
int 			nautilus_drag_modifier_based_action 		(int default_action, 
									 int non_default_action);

GdkDragAction		nautilus_drag_drop_action_ask			(GdkDragAction possible_actions);

#endif
