/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-dnd.c - Drag & drop handling for the icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@eazel.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-dnd.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-background.h"
#include "nautilus-graphic-effects.h"

#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "nautilus-icon-private.h"

static int      nautilus_icon_drag_key_callback      (GtkWidget             *widget,
						      GdkEventKey           *event,
						      gpointer               data);
static gboolean drag_drop_callback                   (GtkWidget             *widget,
						      GdkDragContext        *context,
						      int                    x,
						      int                    y,
						      guint32                time,
						      gpointer               data);
static void     nautilus_icon_dnd_update_drop_target (NautilusIconContainer *container,
						      GdkDragContext        *context,
						      int                    x,
						      int                    y);
static gboolean drag_motion_callback                 (GtkWidget             *widget,
						      GdkDragContext        *context,
						      int                    x,
						      int                    y,
						      guint32                time);


typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} DndSelectionItem;

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL }
};

static GtkTargetEntry drop_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL },
	{ NAUTILUS_ICON_DND_COLOR_TYPE, 0, NAUTILUS_ICON_DND_COLOR },
	{ NAUTILUS_ICON_DND_BGIMAGE_TYPE, 0, NAUTILUS_ICON_DND_BGIMAGE }
};

static GnomeCanvasItem *
create_selection_shadow (NautilusIconContainer *container,
			 GList *list)
{
	GnomeCanvasGroup *group;
	GnomeCanvas *canvas;
	GdkBitmap *stipple;
	int max_x, max_y;
	int min_x, min_y;
	GList *p;
	double pixels_per_unit;

	if (list == NULL) {
		return NULL;
	}

	/* if we're only dragging a single item, don't worry about the shadow */
	if (list->next == NULL) {
		return NULL;
	}
		
	stipple = container->details->dnd_info->stipple;
	g_return_val_if_fail (stipple != NULL, NULL);

	canvas = GNOME_CANVAS (container);

	/* Creating a big set of rectangles in the canvas can be expensive, so
           we try to be smart and only create the maximum number of rectangles
           that we will need, in the vertical/horizontal directions.  */

	/* FIXME bugzilla.eazel.com 624: 
	 * Does this work properly if the window is scrolled? 
	 */
	max_x = GTK_WIDGET (container)->allocation.width;
	min_x = -max_x;

	max_y = GTK_WIDGET (container)->allocation.height;
	min_y = -max_y;

	/* Create a group, so that it's easier to move all the items around at
           once.  */
	group = GNOME_CANVAS_GROUP
		(gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
					gnome_canvas_group_get_type (),
					NULL));
	
	pixels_per_unit = canvas->pixels_per_unit;
	for (p = list; p != NULL; p = p->next) {
		DndSelectionItem *item;
		int x1, y1, x2, y2;

		item = p->data;

		if (!item->got_icon_position) {
			continue;
		}

		x1 = item->icon_x;
		y1 = item->icon_y;
		x2 = x1 + item->icon_width;
		y2 = y1 + item->icon_height;
			
		if (x2 >= min_x && x1 <= max_x && y2 >= min_y && y1 <= max_y)
			gnome_canvas_item_new
				(group,
				 gnome_canvas_rect_get_type (),
				 "x1", (double) x1 / pixels_per_unit,
				 "y1", (double) y1 / pixels_per_unit,
				 "x2", (double) x2 / pixels_per_unit,
				 "y2", (double) y2 / pixels_per_unit,
				 "outline_color", "black",
				 "outline_stipple", stipple,
				 "width_pixels", 1,
				 NULL);
	}

	return GNOME_CANVAS_ITEM (group);
}

/* Set the affine instead of the x and y position.
 * Simple, and setting x and y was broken at one point.
 */
static void
set_shadow_position (GnomeCanvasItem *shadow,
		     double x, double y)
{
	double affine[6];

	affine[0] = 1.0;
	affine[1] = 0.0;
	affine[2] = 0.0;
	affine[3] = 1.0;
	affine[4] = x;
	affine[5] = y;

	gnome_canvas_item_affine_absolute (shadow, affine);
}



/* Functions to deal with DndSelectionItems.  */

static DndSelectionItem *
dnd_selection_item_new (void)
{
	DndSelectionItem *new;

	new = g_new0 (DndSelectionItem, 1);
	
	return new;
}

static void
dnd_selection_item_destroy (DndSelectionItem *item)
{
	g_free (item->uri);
	g_free (item);
}

static void
destroy_selection_list (GList *list)
{
	GList *p;

	if (list == NULL)
		return;

	for (p = list; p != NULL; p = p->next)
		dnd_selection_item_destroy (p->data);

	g_list_free (list);
}

/* Source-side handling of the drag.  */

/* Encode a "special/x-gnome-icon-list" selection.
   Along with the URIs of the dragged files, this encodes
   the location and size of each icon relative to the cursor.
*/
static void
set_gnome_icon_list_selection (NautilusIconContainer *container,
			       GtkSelectionData *selection_data)
{
	NautilusIconContainerDetails *details;
	GList *p;
	GString *data;

	details = container->details;

	data = g_string_new (NULL);
	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;
		ArtDRect world_rect;
		ArtIRect window_rect;
		char *uri;
		char *s;

		icon = p->data;
		if (!icon->is_selected) {
			continue;
		}

		nautilus_icon_canvas_item_get_icon_rectangle
			(icon->item, &world_rect);
		nautilus_gnome_canvas_world_to_window_rectangle
			(GNOME_CANVAS (container), &world_rect, &window_rect);

		uri = nautilus_icon_container_get_icon_uri (container, icon);

		if (uri == NULL) {
			g_warning ("no URI for one of the dragged items");
		} else {
			s = g_strdup_printf ("%s\r%d:%d:%hu:%hu\r\n",
					     uri,
					     (int) (window_rect.x0 - details->dnd_info->start_x),
					     (int) (window_rect.y0 - details->dnd_info->start_y),
					     window_rect.x1 - window_rect.x0,
					     window_rect.y1 - window_rect.y0);
			
			g_free (uri);
			
			g_string_append (data, s);
			g_free (s);
		}
	}

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, data->str, data->len);

	g_string_free (data, TRUE);
}

/* Encode a "text/uri-list" selection.  */
static void
set_uri_list_selection (NautilusIconContainer *container,
			GtkSelectionData *selection_data)
{
	NautilusIconContainerDetails *details;
	GList *p;
	char *uri;
	GString *data;

	details = container->details;

	data = g_string_new (NULL);

	for (p = details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;

		icon = p->data;
		if (!icon->is_selected)
			continue;

		uri = nautilus_icon_container_get_icon_uri (container, icon);
		g_string_append (data, uri);
		g_free (uri);

		g_string_append (data, "\r\n");
	}

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, data->str, data->len);

	g_string_free (data, TRUE);
}

static void
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint32 time,
			gpointer data)
{
	NautilusIconContainer *container;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (widget));
	g_return_if_fail (context != NULL);

	container = NAUTILUS_ICON_CONTAINER (widget);

	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		set_gnome_icon_list_selection (container, selection_data);
		break;
	case NAUTILUS_ICON_DND_URI_LIST:
		set_uri_list_selection (container, selection_data);
		break;
	default:
		g_assert_not_reached ();
	}
}

/* Target-side handling of the drag.  */

static void
get_gnome_icon_list_selection (NautilusIconContainer *container,
			       GtkSelectionData *data)
{
	NautilusIconDndInfo *dnd_info;
	const guchar *p, *oldp;
	int size;

	dnd_info = container->details->dnd_info;

	oldp = data->data;
	size = data->length;

	while (size > 0) {
		DndSelectionItem *item;
		guint len;

		/* The list is in the form:

		   name\rx:y:width:height\r\n

		   The geometry information after the first \r is optional.  */

		/* 1: Decode name. */

		p = memchr (oldp, '\r', size);
		if (p == NULL) {
			break;
		}

		item = dnd_selection_item_new ();

		len = p - oldp;

		item->uri = g_malloc (len + 1);
		memcpy (item->uri, oldp, len);
		item->uri[len] = 0;

		p++;
		if (*p == '\n' || *p == '\0') {
			dnd_info->selection_list
				= g_list_prepend (dnd_info->selection_list,
						  item);
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

		dnd_info->selection_list
			= g_list_prepend (dnd_info->selection_list, item);

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
}

static void
nautilus_icon_container_position_shadow (NautilusIconContainer *container,
					 int x, int y)
{
	GnomeCanvasItem *shadow;
	double world_x, world_y;

	shadow = container->details->dnd_info->shadow;
	if (shadow == NULL) {
		return;
	}

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);
	set_shadow_position (shadow, world_x, world_y);
	gnome_canvas_item_show (shadow);
}

static void
nautilus_icon_container_dropped_icon_feedback (GtkWidget *widget,
					       GtkSelectionData *data,
					       int x, int y)
{
	NautilusIconContainer *container;
	NautilusIconDndInfo *dnd_info;

	container = NAUTILUS_ICON_CONTAINER (widget);
	dnd_info = container->details->dnd_info;

	/* Delete old selection list if any. */
	if (dnd_info->selection_list != NULL) {
		destroy_selection_list (dnd_info->selection_list);
		dnd_info->selection_list = NULL;
	}

	/* Delete old shadow if any. */
	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
	}

	/* Build the selection list and the shadow. */
	get_gnome_icon_list_selection (container, data);
	dnd_info->shadow = create_selection_shadow (container, dnd_info->selection_list);
	nautilus_icon_container_position_shadow (container, x, y);
}

static void
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *data,
			     guint info,
			     guint32 time,
			     gpointer user_data)
{
    	NautilusIconDndInfo *dnd_info;

	dnd_info = NAUTILUS_ICON_CONTAINER (widget)->details->dnd_info;

	dnd_info->got_data_type = TRUE;
	dnd_info->data_type = info;

	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		nautilus_icon_container_dropped_icon_feedback (widget, data, x, y);
		break;
	case NAUTILUS_ICON_DND_COLOR:
	case NAUTILUS_ICON_DND_BGIMAGE:	
		/* Save the data so we can do the actual work on drop. */

		dnd_info->selection_data = nautilus_gtk_selection_data_copy_deep (data);
		break;
	default:
		break;
	}
}

static void
nautilus_icon_container_ensure_drag_data (NautilusIconContainer *container,
					  GdkDragContext *context,
					  guint32 time)
{
	NautilusIconDndInfo *dnd_info;

	dnd_info = container->details->dnd_info;

	if (!dnd_info->got_data_type) {
		gtk_drag_get_data (GTK_WIDGET (container), context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);
	}
}

static void
drag_end_callback (GtkWidget *widget,
		   GdkDragContext *context,
		   gpointer data)
{
	NautilusIconContainer *container;
	NautilusIconDndInfo *dnd_info;

	container = NAUTILUS_ICON_CONTAINER (widget);
	dnd_info = container->details->dnd_info;

	destroy_selection_list (dnd_info->selection_list);
	dnd_info->selection_list = NULL;
}

static NautilusIcon *
nautilus_icon_container_item_at (NautilusIconContainer *container,
				 int x, int y)
{
	GList *p;
	ArtDRect point;

	/* hit test a single pixel rectangle */
	point.x0 = x;
	point.y0 = y;
	point.x1 = x + 1;
	point.y1 = y + 1;

	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;
		icon = p->data;
		if (nautilus_icon_canvas_item_hit_test_rectangle
			(icon->item, &point)) {
			return icon;
		}
	}

	return NULL;
}

static char *
get_container_uri (const NautilusIconContainer *container)
{
	char *uri;

	/* get the URI associated with the container */
	uri = NULL;
	gtk_signal_emit_by_name (GTK_OBJECT (container),
			 "get_container_uri",
			 &uri);
	return uri;
}

static gboolean
uri_is_parent (const GnomeVFSURI *parent, const GnomeVFSURI *item)
{
	/* FIXME bugzilla.eazel.com 625:
	 * consider making this a gnome-vfs call
	 */

	gboolean result;
	GnomeVFSURI *item_parent_uri;

	item_parent_uri = gnome_vfs_uri_get_parent (item);

	if (item_parent_uri == NULL) 
		return FALSE;

	result = gnome_vfs_uri_equal (item_parent_uri, parent);	
	gnome_vfs_uri_unref (item_parent_uri);

	return result;
}
 
static gboolean
nautilus_icon_container_selection_items_local (const NautilusIconContainer *container,
					       const GList *items)
{
	/* check if the first item on the list has the container as a parent
	 * we should really test each item but that would be slow for large selections
	 * and currently dropped items can only be from the same container
	 */
	char *container_uri_string;
	GnomeVFSURI *container_uri;
	GnomeVFSURI *item_uri;
	gboolean result;

	/* must have at least one item */
	g_assert (items);

	result = FALSE;

	/* get the URI associated with the container */
	container_uri_string = get_container_uri (container);
	g_assert (container_uri_string);
	container_uri = gnome_vfs_uri_new (container_uri_string);

	/* get the parent URI of the first item in the selection */
	item_uri = gnome_vfs_uri_new (((DndSelectionItem *)items->data)->uri);
	result = uri_is_parent (container_uri, item_uri);
	
	gnome_vfs_uri_unref (item_uri);
	gnome_vfs_uri_unref (container_uri);
	g_free (container_uri_string);
	
	return result;
}

static gboolean
nautilus_icon_canvas_item_can_accept_item (NautilusIconContainer *container,
					   const NautilusIcon *drop_target_item,
					   const char *item_uri)
{
	gboolean result;

	gtk_signal_emit_by_name (GTK_OBJECT (container),
			 "can_accept_item",
			 drop_target_item->data,
			 item_uri,
			 &result);
	return result;
}
					       
static gboolean
nautilus_icon_canvas_item_can_accept_items (NautilusIconContainer *container,
					    const NautilusIcon *drop_target_item,
					    const GList *items)
{
	int max;

	/* Iterate through selection checking if item will get accepted by the
	 * drop target. If more than 100 items selected, return an over-optimisic
	 * result
	 */
	for (max = 100; items != NULL && max >=0 ; items = items->next, max--) {
		if (!nautilus_icon_canvas_item_can_accept_item (container, drop_target_item, 
						((DndSelectionItem *)items->data)->uri)) {
			return FALSE;
		}
	}

	return TRUE;		
}

static void
receive_dropped_tile_image (NautilusIconContainer *container, gpointer data)
{
	g_assert(data != NULL);
	nautilus_background_set_tile_image_uri
		(nautilus_get_widget_background (GTK_WIDGET (container)), data);
}

static void
nautilus_icon_container_receive_dropped_icons (NautilusIconContainer *container,
					       GdkDragContext *context,
					       int x, int y)
{
	GList *p;
	NautilusIcon *drop_target_icon;
	gboolean local_move_only;
	DndSelectionItem *item;
	NautilusIcon *icon;
	GList *source_uris;
	char *target_uri;
	double world_x, world_y;
	GdkPoint *source_item_locations;
	int index;
	int count;
	
	if (container->details->dnd_info->selection_list == NULL) {
		return;
	}

  	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	/* find the item we hit with our drop, if any */
	drop_target_icon = nautilus_icon_container_item_at (container, world_x, world_y);
	if (drop_target_icon != NULL && !nautilus_icon_canvas_item_can_accept_items 
		(container, drop_target_icon, container->details->dnd_info->selection_list)) {
		/* the item we dropped our selection on cannot accept the items,
		 * do the same thing as if we just dropped the items on the canvas
		 */
		drop_target_icon = NULL;
	}

	local_move_only = FALSE;
	if (drop_target_icon == NULL && context->action == GDK_ACTION_MOVE) {
		/* we can just move the icon positions if the move ended up in
		 * the item's parent container
		 */
		local_move_only = nautilus_icon_container_selection_items_local
			(container, container->details->dnd_info->selection_list);
	}

	if (local_move_only) {
		GList *icons_to_select;

		icons_to_select = NULL;

		/* handle the simple case -- just change item locations */
		for (p = container->details->dnd_info->selection_list; p != NULL; p = p->next) {
			item = p->data;
			icon = nautilus_icon_container_get_icon_by_uri
				(container, item->uri);

			if (item->got_icon_position) {

				nautilus_icon_container_move_icon
					(container, icon,
					 world_x + item->icon_x, world_y + item->icon_y,
					 icon->scale_x, icon->scale_y,
					 TRUE);   

			}
			icons_to_select = g_list_prepend (icons_to_select, icon);
		}		
		if (icons_to_select != NULL) {
			nautilus_icon_container_select_list_unselect_others (container, 
									     icons_to_select);
			g_list_free (icons_to_select);
		}
	} else {		
		source_uris = NULL;
		target_uri = NULL;
		source_item_locations = NULL;

		/* get the URI of either the item or the container we hit */
		if (drop_target_icon != NULL) {
			target_uri = nautilus_icon_container_get_icon_uri
				(container, drop_target_icon);
		} else {
			target_uri = get_container_uri (container);

		}

		count = 0;
		for (p = container->details->dnd_info->selection_list; p != NULL; p = p->next) {
			/* do a shallow copy of all the uri strings of the copied files */
			source_uris = g_list_append (source_uris, ((DndSelectionItem *)p->data)->uri);
			/* count the number of items as we go */
			count++;
		}

		if (drop_target_icon != NULL) {
			/* drop onto a container, pass allong the item points to allow placing
			 * the items in their same relative positions in the new container
			 */
			source_item_locations = g_new (GdkPoint, count);
			for (index = 0, p = container->details->dnd_info->selection_list; p != NULL; 
			     index++, p = p->next) {
				/* FIXME bugzilla.eazel.com 626:
				 * subtract the original click coordinates from each point here
				 */
				source_item_locations[index].x = ((DndSelectionItem *)p->data)->icon_x;
				source_item_locations[index].y = ((DndSelectionItem *)p->data)->icon_y;
			}
		}
		
		if (source_uris != NULL) {
			/* start the copy */
			gtk_signal_emit_by_name (GTK_OBJECT (container), "move_copy_items",
						 source_uris,
						 source_item_locations,
						 target_uri,
						 context->action,
						 x, y);
			g_list_free (source_uris);
			g_free (source_item_locations);
		}
		g_free (target_uri);
	}

	destroy_selection_list (container->details->dnd_info->selection_list);
	container->details->dnd_info->selection_list = NULL;
}

static void
set_drop_target (NautilusIconContainer *container,
		 NautilusIcon *icon)
{
	NautilusIcon *old_icon;

	/* Check if current drop target changed, update icon drop higlight if needed. */
	old_icon = container->details->drop_target;
	if (icon == old_icon) {
		return;
	}

	/* Remember the new drop target for the next round. */
	container->details->drop_target = icon;
	nautilus_icon_container_update_icon (container, old_icon);
	nautilus_icon_container_update_icon (container, icon);
}

static void
nautilus_icon_dnd_update_drop_target (NautilusIconContainer *container,
				      GdkDragContext *context,
				      int x, int y)
{
	NautilusIcon *icon;
	double world_x, world_y;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	if (container->details->dnd_info->selection_list == NULL) {
		return;
	}

  	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	/* Find the item we hit with our drop, if any. */
	icon = nautilus_icon_container_item_at (container, world_x, world_y);

	/* Find if target icon accepts our drop. */
	if (icon != NULL && !nautilus_icon_canvas_item_can_accept_items 
	    (container, icon, container->details->dnd_info->selection_list)) {
		icon = NULL;
	}

	set_drop_target (container, icon);
}

static void
nautilus_icon_container_free_drag_data (NautilusIconContainer *container)
{
	NautilusIconDndInfo *dnd_info;
	
	dnd_info = container->details->dnd_info;
	
	dnd_info->got_data_type = FALSE;
	
	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
		dnd_info->shadow = NULL;
	}

	if (dnd_info->selection_data != NULL) {
		nautilus_gtk_selection_data_free_deep (dnd_info->selection_data);
		dnd_info->selection_data = NULL;
	}
}

static void
drag_leave_callback (GtkWidget *widget,
		     GdkDragContext *context,
		     guint32 time,
		     gpointer data)
{
	NautilusIconDndInfo *dnd_info;

	dnd_info = NAUTILUS_ICON_CONTAINER (widget)->details->dnd_info;

	if (dnd_info->shadow != NULL)
		gnome_canvas_item_hide (dnd_info->shadow);
}

/* During drag&drop keep a saved pointer to the private drag context.
 * We also replace the severely broken gtk_drag_get_event_actions.
 * To do this we copy a lot of code from gtkdnd.c.
 * 
 * This is a hack-workaround to deal with the inability to override
 * drag action feedback in gtk and will be removed once the appropriate
 * interface gets added to gtkdnd to do this in a clean way.
 * For now we need to mirror the code here
 * to allow us to control the drop action based on the modifier
 * key state, drop target and other drag&drop state.
 * 
 * FIXME bugzilla.eazel.com 627:
 */

typedef struct GtkDragDestInfo GtkDragDestInfo;
typedef struct GtkDragStatus GtkDragStatus;

typedef struct GtkDragSourceInfo 
{
  GtkWidget         *widget;
  GtkTargetList     *target_list; /* Targets for drag data */
  GdkDragAction      possible_actions; /* Actions allowed by source */
  GdkDragContext    *context;	  /* drag context */
  GtkWidget         *icon_window; /* Window for drag */
  GtkWidget         *ipc_widget;  /* GtkInvisible for grab, message passing */
  GdkCursor         *cursor;	  /* Cursor for drag */
  gint hot_x, hot_y;		  /* Hot spot for drag */
  gint button;			  /* mouse button starting drag */

  gint	 	      status;	  /* drag status !!!  GtkDragStatus in real life*/
  GdkEvent          *last_event;  /* motion event waiting for response */

  gint               start_x, start_y; /* Initial position */
  gint               cur_x, cur_y;     /* Current Position */

  GList             *selections;  /* selections we've claimed */
  
  GtkDragDestInfo   *proxy_dest;  /* Set if this is a proxy drag */

  guint              drop_timeout;     /* Timeout for aborting drop */
  guint              destroy_icon : 1; /* If true, destroy icon_window
					*/
} GtkDragSourceInfo;
static GtkDragSourceInfo *saved_drag_source_info;

void
nautilus_icon_dnd_init (NautilusIconContainer *container,
			GdkBitmap *stipple)
{
	NautilusIconDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	dnd_info = g_new0 (NautilusIconDndInfo, 1);

	dnd_info->target_list = gtk_target_list_new (drag_types,
						     NAUTILUS_N_ELEMENTS (drag_types));

	dnd_info->stipple = gdk_bitmap_ref (stipple);

	/* Set up the widget as a drag destination.
	 * (But not a source, as drags starting from this widget will be
         * implemented by dealing with events manually.)
	 */
	gtk_drag_dest_set  (GTK_WIDGET (container),
			    0,
			    drop_types, NAUTILUS_N_ELEMENTS (drop_types),
			    GDK_ACTION_COPY | GDK_ACTION_MOVE);

	/* Messages for outgoing drag. */
	gtk_signal_connect (GTK_OBJECT (container), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_end",
			    GTK_SIGNAL_FUNC (drag_end_callback), NULL);

	/* Messages for incoming drag. */
	gtk_signal_connect (GTK_OBJECT (container), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_motion",
			    GTK_SIGNAL_FUNC (drag_motion_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_drop",
			    GTK_SIGNAL_FUNC (drag_drop_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_leave",
			    GTK_SIGNAL_FUNC (drag_leave_callback), NULL);

	container->details->dnd_info = dnd_info;
	saved_drag_source_info = NULL;

}

void
nautilus_icon_dnd_fini (NautilusIconContainer *container)
{
	NautilusIconDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);

	gtk_target_list_unref (dnd_info->target_list);
	destroy_selection_list (dnd_info->selection_list);

	if (dnd_info->shadow != NULL)
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));

	gdk_bitmap_unref (dnd_info->stipple);

	g_free (dnd_info);
}


static GtkDragSourceInfo *
nautilus_icon_dnd_get_drag_source_info (GtkWidget *widget, GdkDragContext *context)
{
	if (context == NULL)
		return NULL;

	return g_dataset_get_data (context, "gtk-info");
}

void
nautilus_icon_dnd_begin_drag (NautilusIconContainer *container,
			      GdkDragAction actions,
			      int button,
			      GdkEventMotion *event)
{
	NautilusIconDndInfo *dnd_info;
	GnomeCanvas *canvas;
	GdkDragContext *context;
	GdkPixbuf *pixbuf, *transparent_pixbuf;
	GdkPixmap *pixmap_for_dragged_file;
	GdkBitmap *mask_for_dragged_file;
	int x_offset, y_offset;
	ArtDRect world_rect;
	ArtIRect window_rect;
	
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (event != NULL);

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);
	
	/* Notice that the event is in world coordinates, because of
           the way the canvas handles events.
	*/
	canvas = GNOME_CANVAS (container);
	gnome_canvas_world_to_window (canvas,
				      event->x, event->y,
				      &dnd_info->start_x, &dnd_info->start_y);
	
	/* start the drag */
	context = gtk_drag_begin (GTK_WIDGET (container),
				  dnd_info->target_list,
				  actions,
				  button,
				  (GdkEvent *) event);

	/* set up state for overriding the broken gtk_drag_get_event_actions call */
	saved_drag_source_info = nautilus_icon_dnd_get_drag_source_info (GTK_WIDGET (container), context);
	g_assert (saved_drag_source_info != NULL);
	
	gtk_signal_connect (GTK_OBJECT (saved_drag_source_info ? saved_drag_source_info->ipc_widget : NULL), "key_press_event",
			    GTK_SIGNAL_FUNC (nautilus_icon_drag_key_callback), saved_drag_source_info);
	gtk_signal_connect (GTK_OBJECT (saved_drag_source_info ? saved_drag_source_info->ipc_widget : NULL), "key_release_event",
			    GTK_SIGNAL_FUNC (nautilus_icon_drag_key_callback), saved_drag_source_info);

        /* create a pixmap and mask to drag with */
        pixbuf = nautilus_icon_canvas_item_get_image (container->details->drag_icon->item, NULL);
        
	/* unfortunately, X is very slow when using a stippled mask, so only use the stipple
	   for relatively small pixbufs.  Eventually, we may have to remove this entirely
	   for UI consistency reasons */
	
	if (gdk_pixbuf_get_width(pixbuf) * gdk_pixbuf_get_height(pixbuf) < 4096) {
		transparent_pixbuf = nautilus_make_semi_transparent (pixbuf);
	} else {
		gdk_pixbuf_ref (pixbuf);
		transparent_pixbuf = pixbuf;
	}
		
	gdk_pixbuf_render_pixmap_and_mask (transparent_pixbuf,
					   &pixmap_for_dragged_file,
					   &mask_for_dragged_file,
					   128);

	gdk_pixbuf_unref (transparent_pixbuf);
	
        /* compute the image's offset */
	nautilus_icon_canvas_item_get_icon_rectangle
		(container->details->drag_icon->item, &world_rect);
	nautilus_gnome_canvas_world_to_window_rectangle
		(canvas, &world_rect, &window_rect);
        x_offset = dnd_info->start_x - window_rect.x0;
        y_offset = dnd_info->start_y - window_rect.y0;
        
        /* set the pixmap and mask for dragging */
        gtk_drag_set_icon_pixmap (context,
				  gtk_widget_get_colormap (GTK_WIDGET (container)),
				  pixmap_for_dragged_file,
				  mask_for_dragged_file,
				  x_offset, y_offset);
}

static gboolean
drag_motion_callback (GtkWidget *widget,
		      GdkDragContext *context,
		      int x, int y,
		      guint32 time)
{
	nautilus_icon_dnd_update_drop_action (widget);
	nautilus_icon_container_ensure_drag_data (NAUTILUS_ICON_CONTAINER (widget), context, time);
	nautilus_icon_container_position_shadow (NAUTILUS_ICON_CONTAINER (widget), x, y);
	nautilus_icon_dnd_update_drop_target (NAUTILUS_ICON_CONTAINER (widget), context, x, y);

	gdk_drag_status (context, context->suggested_action, time);

	return TRUE;
}

static gboolean
drag_drop_callback (GtkWidget *widget,
		    GdkDragContext *context,
		    int x,
		    int y,
		    guint32 time,
		    gpointer data)
{
	NautilusIconDndInfo *dnd_info;

	dnd_info = NAUTILUS_ICON_CONTAINER (widget)->details->dnd_info;

	nautilus_icon_container_ensure_drag_data (NAUTILUS_ICON_CONTAINER (widget), context, time);

	g_assert (dnd_info->got_data_type);
	switch (dnd_info->data_type) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		nautilus_icon_container_receive_dropped_icons
			(NAUTILUS_ICON_CONTAINER (widget),
			 context, x, y);
		gtk_drag_finish (context, TRUE, FALSE, time);
		break;
	case NAUTILUS_ICON_DND_COLOR:
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (widget),
			 widget, x, y, dnd_info->selection_data);
		gtk_drag_finish (context, TRUE, FALSE, time);
		break;
	case NAUTILUS_ICON_DND_BGIMAGE:
		receive_dropped_tile_image (NAUTILUS_ICON_CONTAINER (widget), dnd_info->selection_data->data);
		break;
	default:
		gtk_drag_finish (context, FALSE, FALSE, time);
	}

	nautilus_icon_container_free_drag_data (NAUTILUS_ICON_CONTAINER (widget));

	if (saved_drag_source_info != NULL) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (saved_drag_source_info->ipc_widget),
					GTK_SIGNAL_FUNC (nautilus_icon_drag_key_callback),
					saved_drag_source_info);
	}

	saved_drag_source_info = NULL;

	set_drop_target (NAUTILUS_ICON_CONTAINER (widget), NULL);

	return FALSE;
}

void
nautilus_icon_dnd_end_drag (NautilusIconContainer *container)
{
	NautilusIconDndInfo *dnd_info;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);

	/* Do nothing.
	 * Can that possibly be right?
	 */
}

static int
nautilus_icon_dnd_modifier_based_action ()
{
	GdkModifierType modifiers;
	gdk_window_get_pointer (NULL, NULL, NULL, &modifiers);

	if ((modifiers & GDK_CONTROL_MASK) != 0) {
		return GDK_ACTION_COPY;
#if 0
	/* FIXME bugzilla.eazel.com 628:
	 * don't know how to do links yet
	 */
	 } else if ((modifiers & GDK_MOD1_MASK) != 0) {
		return GDK_ACTION_LINK;
#endif
	}

	return GDK_ACTION_MOVE;
}

void
nautilus_icon_dnd_update_drop_action (GtkWidget *widget)
{
	if (saved_drag_source_info == NULL)
		return;

	saved_drag_source_info->possible_actions = nautilus_icon_dnd_modifier_based_action ();
}

/* Replacement for broken gtk_drag_get_event_actions */

static void
nautilus_icon_dnd_get_event_actions (GdkEvent *event, 
				    gint button, 
				    GdkDragAction actions,
				    GdkDragAction *suggested_action,
				    GdkDragAction *possible_actions)
{
	*suggested_action = nautilus_icon_dnd_modifier_based_action ();
	*possible_actions = *suggested_action;
}
		 
/* Copied from gtkdnd.c to work around broken gtk_drag_get_event_actions */

static guint32
nautilus_icon_dnd_get_event_time (GdkEvent *event)
{
  guint32 tm = GDK_CURRENT_TIME;
  
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	tm = event->motion.time; break;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
	tm = event->button.time; break;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	tm = event->key.time; break;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	tm = event->crossing.time; break;
      case GDK_PROPERTY_NOTIFY:
	tm = event->property.time; break;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
	tm = event->selection.time; break;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
	tm = event->proximity.time; break;
      default:			/* use current time */
	break;
      }
  
  return tm;
}

/* Copied from gtkdnd.c to work around broken gtk_drag_get_event_actions */

enum {
  TARGET_MOTIF_SUCCESS = 0x40000000,
  TARGET_MOTIF_FAILURE,
  TARGET_DELETE
};

/* Copied from gtkdnd.c to work around broken gtk_drag_get_event_actions */

static void
nautilus_icon_dnd_source_check_selection (GtkDragSourceInfo *info, 
				 GdkAtom            selection,
				 guint32            time)
{
  GList *tmp_list;

  tmp_list = info->selections;
  while (tmp_list)
    {
      if (GPOINTER_TO_UINT (tmp_list->data) == selection)
	return;
      tmp_list = tmp_list->next;
    }

  gtk_selection_owner_set (info->ipc_widget, selection, time);
  info->selections = g_list_prepend (info->selections,
				     GUINT_TO_POINTER (selection));

  tmp_list = info->target_list->list;
  while (tmp_list)
    {
      GtkTargetPair *pair = tmp_list->data;

      gtk_selection_add_target (info->ipc_widget,
				selection,
				pair->target,
				pair->info);
      tmp_list = tmp_list->next;
    }
  
  if (info->context->protocol == GDK_DRAG_PROTO_MOTIF)
    {
      gtk_selection_add_target (info->ipc_widget,
				selection,
				gdk_atom_intern ("XmTRANSFER_SUCCESS", FALSE),
				TARGET_MOTIF_SUCCESS);
      gtk_selection_add_target (info->ipc_widget,
				selection,
				gdk_atom_intern ("XmTRANSFER_FAILURE", FALSE),
				TARGET_MOTIF_FAILURE);
    }

  gtk_selection_add_target (info->ipc_widget,
			    selection,
			    gdk_atom_intern ("DELETE", FALSE),
			    TARGET_DELETE);
}

/* Copied from gtkdnd.c to work around broken gtk_drag_get_event_actions */

static void
nautilus_icon_dnd_update (GtkDragSourceInfo *info,
		 gint               x_root,
		 gint               y_root,
		 GdkEvent          *event)
{
  GdkDragAction action;
  GdkDragAction possible_actions;
  GdkWindow *window = NULL;
  GdkWindow *dest_window;
  GdkDragProtocol protocol;
  GdkAtom selection;
  guint32 time = nautilus_icon_dnd_get_event_time (event);

  nautilus_icon_dnd_get_event_actions (event,
				      info->button, 
				      info->possible_actions,
				      &action, &possible_actions);
  info->cur_x = x_root;
  info->cur_y = y_root;

  if (info->icon_window)
    {
      gdk_window_raise (info->icon_window->window);
      gtk_widget_set_uposition (info->icon_window, 
				info->cur_x - info->hot_x, 
				info->cur_y - info->hot_y);
      window = info->icon_window->window;
    }
  
  gdk_drag_find_window (info->context,
			window, x_root, y_root,
			&dest_window, &protocol);

  if (gdk_drag_motion (info->context, dest_window, protocol,
		       x_root, y_root, action, 
		       possible_actions,
		       time))
    {
      if (info->last_event)
	gdk_event_free ((GdkEvent *)info->last_event);
      
      info->last_event = gdk_event_copy ((GdkEvent *)event);
    }

  if (dest_window)
    gdk_window_unref (dest_window);

  selection = gdk_drag_get_selection (info->context);

  if (selection)
    nautilus_icon_dnd_source_check_selection (info, selection, time);
}

/* Copied from gtkdnd.c to work around broken gtk_drag_get_event_actions */
static int 
nautilus_icon_drag_key_callback (GtkWidget *widget, GdkEventKey *event, 
		 		 gpointer data)
{
	GtkDragSourceInfo *info = (GtkDragSourceInfo *)data;
	GdkModifierType state;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);
	g_return_val_if_fail (data != NULL, TRUE);

	if (event->type == GDK_KEY_PRESS
		&& event->keyval == GDK_Escape) {
		return FALSE;
	}

	/* Now send a "motion" so that the modifier state is updated */

	/* The state is not yet updated in the event, so we need
	 * to query it here. We could use XGetModifierMapping, but
	 * that would be overkill.
	 */
	gdk_window_get_pointer (GDK_ROOT_PARENT(), NULL, NULL, &state);

	event->state = state;
	nautilus_icon_dnd_update (info, info->cur_x, info->cur_y, (GdkEvent *)event);

	return TRUE;
}
