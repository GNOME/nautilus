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
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-background.h"
#include "nautilus-graphic-effects.h"
#include "nautilus-stock-dialogs.h"

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
	{ NAUTILUS_ICON_DND_BGIMAGE_TYPE, 0, NAUTILUS_ICON_DND_BGIMAGE },
	{ NAUTILUS_ICON_DND_KEYWORD_TYPE, 0, NAUTILUS_ICON_DND_KEYWORD }
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
		
	stipple = container->details->dnd_info->drag_info.stipple;
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
		DragSelectionItem *item;
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


/* Source-side handling of the drag. */

/* iteration glue struct */
typedef struct {
	gpointer iterator_context;
	NautilusDragEachSelectedItemDataGet iteratee;
	gpointer iteratee_data;
} IconGetDataBinderContext;

static gboolean
icon_get_data_binder (NautilusIcon *icon, gpointer data)
{
	IconGetDataBinderContext *context;
	ArtDRect world_rect;
	ArtIRect window_rect;
	char *uri;
	NautilusIconContainer *container;

	context = (IconGetDataBinderContext *)data;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (context->iterator_context));

	container = NAUTILUS_ICON_CONTAINER (context->iterator_context);

	nautilus_icon_canvas_item_get_icon_rectangle
		(icon->item, &world_rect);
	nautilus_gnome_canvas_world_to_window_rectangle
		(GNOME_CANVAS (container), &world_rect, &window_rect);

	uri = nautilus_icon_container_get_icon_uri (container, icon);
	if (uri == NULL) {
		g_warning ("no URI for one of the iterated icons");
		return TRUE;
	}

	/* pass the uri, mouse-relative x/y and icon width/height */
	context->iteratee (uri, 
			   (int) (window_rect.x0 - container->details->dnd_info->drag_info.start_x),
			   (int) (window_rect.y0 - container->details->dnd_info->drag_info.start_y),
			   window_rect.x1 - window_rect.x0,
			   window_rect.y1 - window_rect.y0,
			   context->iteratee_data);

	g_free (uri);

	return TRUE;
}

/* Iterate over each selected icon in a NautilusIconContainer,
 * calling each_function on each.
 */
static void
nautilus_icon_container_each_selected_icon (NautilusIconContainer *container,
	gboolean (*each_function) (NautilusIcon *, gpointer), gpointer data)
{
	GList *p;
	NautilusIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (!icon->is_selected) {
			continue;
		}
		if (!each_function (icon, data)) {
			return;
		}
	}
}

/* Adaptor function used with nautilus_icon_container_each_selected_icon
 * to help iterate over all selected items, passing uris, x,y,w and h
 * values to the iteratee
 */
static void
each_icon_get_data_binder (NautilusDragEachSelectedItemDataGet iteratee, 
	gpointer iterator_context, gpointer data)
{
	IconGetDataBinderContext context;
	NautilusIconContainer *container;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (iterator_context));
	container = NAUTILUS_ICON_CONTAINER (iterator_context);

	context.iterator_context = iterator_context;
	context.iteratee = iteratee;
	context.iteratee_data = data;
	nautilus_icon_container_each_selected_icon (container, icon_get_data_binder, &context);
}

/* Called when the data for drag&drop is needed */
static void
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint32 time,
			gpointer data)
{
	g_assert (widget != NULL);
	g_assert (NAUTILUS_IS_ICON_CONTAINER (widget));
	g_return_if_fail (context != NULL);

	/* Call common function from nautilus-drag that set's up
	 * the selection data in the right format. Pass it means to
	 * iterate all the selected icons.
	 */
	nautilus_drag_drag_data_get (widget, context, selection_data,
		info, time, widget, each_icon_get_data_binder);
}



/* Target-side handling of the drag.  */


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

	/* Delete old selection list. */
	nautilus_drag_destroy_selection_list (dnd_info->drag_info.selection_list);
	dnd_info->drag_info.selection_list = NULL;

	/* Delete old shadow if any. */
	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
	}

	/* Build the selection list and the shadow. */
	dnd_info->drag_info.selection_list = nautilus_drag_build_selection_list (data);
	dnd_info->shadow = create_selection_shadow (container, dnd_info->drag_info.selection_list);
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

	dnd_info->drag_info.got_drop_data_type = TRUE;
	dnd_info->drag_info.data_type = info;

	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		nautilus_icon_container_dropped_icon_feedback (widget, data, x, y);
		break;
	case NAUTILUS_ICON_DND_COLOR:
	case NAUTILUS_ICON_DND_BGIMAGE:	
	case NAUTILUS_ICON_DND_KEYWORD:	
		/* Save the data so we can do the actual work on drop. */
		g_assert (dnd_info->drag_info.selection_data == NULL);
		dnd_info->drag_info.selection_data = nautilus_gtk_selection_data_copy_deep (data);
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

	if (!dnd_info->drag_info.got_drop_data_type) {
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

	nautilus_drag_destroy_selection_list (dnd_info->drag_info.selection_list);
	dnd_info->drag_info.selection_list = NULL;
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
nautilus_icon_container_selection_items_local (const NautilusIconContainer *container,
					       const GList *items)
{
	char *container_uri_string;
	gboolean result;

	/* must have at least one item */
	g_assert (items);

	result = FALSE;

	/* get the URI associated with the container */
	container_uri_string = get_container_uri (container);
	result = nautilus_drag_items_local (container_uri_string, items);
	g_free (container_uri_string);
	
	return result;
}

/* handle dropped tile images */
static void
receive_dropped_tile_image (NautilusIconContainer *container, gpointer data)
{
	g_assert(data != NULL);
	nautilus_background_set_tile_image_uri
		(nautilus_get_widget_background (GTK_WIDGET (container)), data);
}

/* handle dropped keywords */
static void
receive_dropped_keyword (NautilusIconContainer *container, char* keyword, int x, int y)
{
	GList *keywords, *word;
	char *uri;
	double world_x, world_y;

	NautilusIcon *drop_target_icon;
	NautilusFile *file;
	
	g_assert (keyword != NULL);

	/* find the item we hit with our drop, if any */
  	gnome_canvas_window_to_world (GNOME_CANVAS (container), x, y, &world_x, &world_y);
	drop_target_icon = nautilus_icon_container_item_at (container, world_x, world_y);
	if (drop_target_icon == NULL) {
		return;
	}

	/* FIXME: This does not belong in the icon code.
	 * It has to be in the file manager.
	 * The icon code has no right to deal with the file directly.
	 * But luckily there's no issue of not getting a file object,
	 * so we don't have to worry about async. issues here.
	 */
	uri = nautilus_icon_container_get_icon_uri (container, drop_target_icon);
	file = nautilus_file_get (uri);
	g_free (uri);
	
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, keyword, (GCompareFunc) strcmp);
	if (word == NULL) {
		keywords = g_list_append (keywords, g_strdup (keyword));
	} else {
		keywords = g_list_remove_link (keywords, word);
		g_free (word->data);
		g_list_free (word);
	}

	nautilus_file_set_keywords (file, keywords);
	nautilus_file_unref (file);
	nautilus_icon_container_update_icon (container, drop_target_icon);

}

static gboolean
confirm_switch_to_manual_layout (NautilusIconContainer *container)
{
	const char *message;

	/* FIXME bugzilla.eazel.com 915: Use of the word "directory"
	 * makes this FMIconView specific. Move these messages into
	 * FMIconView so NautilusIconContainer can be used for things
	 * that are not directories?
	 */
	if (nautilus_icon_container_has_stored_icon_positions (container)) {
		if (nautilus_g_list_exactly_one_item (container->details->dnd_info->drag_info.selection_list)) {
			message = _("This directory uses automatic layout. Do you want to switch to manual layout and leave this item where you dropped it? This will clobber the stored manual layout.");
		} else {
			message = _("This directory uses automatic layout. Do you want to switch to manual layout and leave these items where you dropped them? This will clobber the stored manual layout.");
		}
	} else {
		if (nautilus_g_list_exactly_one_item (container->details->dnd_info->drag_info.selection_list)) {
			message = _("This directory uses automatic layout. Do you want to switch to manual layout and leave this item where you dropped it?");
		} else {
			message = _("This directory uses automatic layout. Do you want to switch to manual layout and leave these items where you dropped them?");
		}
	}

	return nautilus_simple_dialog
		(GTK_WIDGET (container), message,
		 _("Switch to Manual Layout?"),
		 _("Switch"), GNOME_STOCK_BUTTON_CANCEL, NULL) == 0;
}

static void
handle_local_move (NautilusIconContainer *container,
		   double world_x, double world_y)
{
	GList *moved_icons, *p;
	DragSelectionItem *item;
	NautilusIcon *icon;

	if (container->details->auto_layout) {
		if (!confirm_switch_to_manual_layout (container)) {
			return;
		}
		nautilus_icon_container_freeze_icon_positions (container);
	}

	/* Move and select the icons. */
	moved_icons = NULL;
	for (p = container->details->dnd_info->drag_info.selection_list; p != NULL; p = p->next) {
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
		moved_icons = g_list_prepend (moved_icons, icon);
	}		
	nautilus_icon_container_select_list_unselect_others
		(container, moved_icons);
	g_list_free (moved_icons);
}

static void
handle_nonlocal_move (NautilusIconContainer *container,
		      GdkDragContext *context,
		      int x, int y,
		      NautilusIcon *drop_target_icon)
{
	GList *source_uris, *p;
	char *target_uri;
	GdkPoint *source_item_locations;
	int i;

	if (container->details->dnd_info->drag_info.selection_list == NULL) {
		return;
	}
	
	source_uris = NULL;
	for (p = container->details->dnd_info->drag_info.selection_list; p != NULL; p = p->next) {
		/* do a shallow copy of all the uri strings of the copied files */
		source_uris = g_list_prepend (source_uris, ((DragSelectionItem *)p->data)->uri);
	}
	source_uris = g_list_reverse (source_uris);
	
	source_item_locations = NULL;
	if (drop_target_icon != NULL) {
		/* Drop onto a container. Pass along the item points to allow placing
		 * the items in their same relative positions in the new container.
		 */
		source_item_locations = g_new (GdkPoint, g_list_length (source_uris));
		for (i = 0, p = container->details->dnd_info->drag_info.selection_list;
		     p != NULL; i++, p = p->next) {
			/* FIXME bugzilla.eazel.com 626:
			 * subtract the original click coordinates from each point here
			 */
			source_item_locations[i].x = ((DragSelectionItem *)p->data)->icon_x;
			source_item_locations[i].y = ((DragSelectionItem *)p->data)->icon_y;
		}
	}
	
	/* get the URI of either the item or the container we hit */
	if (drop_target_icon != NULL) {
		target_uri = nautilus_icon_container_get_icon_uri
			(container, drop_target_icon);
	} else {
		target_uri = get_container_uri (container);
	}
	
	/* start the copy */
	gtk_signal_emit_by_name (GTK_OBJECT (container), "move_copy_items",
				 source_uris,
				 source_item_locations,
				 target_uri,
				 context->action,
				 x, y);
	g_list_free (source_uris);
	g_free (source_item_locations);
	g_free (target_uri);
}

static void
nautilus_icon_container_receive_dropped_icons (NautilusIconContainer *container,
					       GdkDragContext *context,
					       int x, int y)
{
	NautilusIcon *drop_target_icon;
	gboolean local_move_only;
	double world_x, world_y;
	
	if (container->details->dnd_info->drag_info.selection_list == NULL) {
		return;
	}

  	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	/* FIXME: These "can_accept_items" tests need to be done by
	 * the icon view, not here. This file is not supposed to know
	 * that the target is a file.
	 */

	/* Find the item we hit with our drop, if any */
	drop_target_icon = nautilus_icon_container_item_at (container, world_x, world_y);
	if (drop_target_icon != NULL 
		&& !nautilus_drag_can_accept_items 
			(nautilus_file_get (
				nautilus_icon_container_get_icon_uri 
					(container, drop_target_icon)), 
			container->details->dnd_info->drag_info.selection_list)) {
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
			(container, container->details->dnd_info->drag_info.selection_list);
	}

	if (local_move_only) {
		handle_local_move (container, world_x, world_y);
	} else {
		handle_nonlocal_move (container, context, x, y, drop_target_icon);
	}

	nautilus_drag_destroy_selection_list (container->details->dnd_info->drag_info.selection_list);
	container->details->dnd_info->drag_info.selection_list = NULL;
}

static void
set_drop_target (NautilusIconContainer *container,
		 NautilusIcon *icon)
{
	NautilusIcon *old_icon;

	/* Check if current drop target changed, update icon drop
	 * higlight if needed.
	 */
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
	if ((container->details->dnd_info->drag_info.selection_list == NULL) 
	   && (container->details->dnd_info->drag_info.data_type != NAUTILUS_ICON_DND_KEYWORD)) {
		return;
	}

  	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	/* Find the item we hit with our drop, if any. */
	icon = nautilus_icon_container_item_at (container, world_x, world_y);

	/* FIXME: These "can_accept_items" tests need to be done by
	 * the icon view, not here. This file is not supposed to know
	 * that the target is a file.
	 */

	/* Find if target icon accepts our drop. */
	if (icon != NULL 
		&& (container->details->dnd_info->drag_info.data_type != NAUTILUS_ICON_DND_KEYWORD) 
		&& !nautilus_drag_can_accept_items 
			(nautilus_file_get (
				nautilus_icon_container_get_icon_uri (container, icon)), 
			container->details->dnd_info->drag_info.selection_list)) {
		icon = NULL;
	}

	set_drop_target (container, icon);
}

static void
nautilus_icon_container_free_drag_data (NautilusIconContainer *container)
{
	NautilusIconDndInfo *dnd_info;
	
	dnd_info = container->details->dnd_info;
	
	dnd_info->drag_info.got_drop_data_type = FALSE;
	
	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
		dnd_info->shadow = NULL;
	}

	if (dnd_info->drag_info.selection_data != NULL) {
		nautilus_gtk_selection_data_free_deep (dnd_info->drag_info.selection_data);
		dnd_info->drag_info.selection_data = NULL;
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
	
	nautilus_icon_container_free_drag_data(NAUTILUS_ICON_CONTAINER (widget));
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
	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));


	container->details->dnd_info = g_new0 (NautilusIconDndInfo, 1);
	nautilus_drag_init (&container->details->dnd_info->drag_info,
		drag_types, NAUTILUS_N_ELEMENTS (drag_types), stipple);

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

	saved_drag_source_info = NULL;

}

void
nautilus_icon_dnd_fini (NautilusIconContainer *container)
{
	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (container->details->dnd_info != NULL);

	if (container->details->dnd_info->shadow != NULL)
		gtk_object_destroy (GTK_OBJECT (container->details->dnd_info->shadow));

	nautilus_drag_finalize (&container->details->dnd_info->drag_info);
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
				      &dnd_info->drag_info.start_x, &dnd_info->drag_info.start_y);
	
	/* start the drag */
	context = gtk_drag_begin (GTK_WIDGET (container),
				  dnd_info->drag_info.target_list,
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
        pixbuf = nautilus_icon_canvas_item_get_image (container->details->drag_icon->item);
        
	/* unfortunately, X is very slow when using a stippled mask,
	   so only use the stipple for relatively small pixbufs. */
	/* FIXME bugzilla.eazel.com 914: Eventually, we may have to
	 * remove this entirely for UI consistency reasons.
	 */
	
	if (gdk_pixbuf_get_width(pixbuf) * gdk_pixbuf_get_height(pixbuf) < 4096) {
		transparent_pixbuf = nautilus_make_semi_transparent (pixbuf);
	} else {
		gdk_pixbuf_ref (pixbuf);
		transparent_pixbuf = pixbuf;
	}
		
	gdk_pixbuf_render_pixmap_and_mask (transparent_pixbuf,
					   &pixmap_for_dragged_file,
					   &mask_for_dragged_file,
					   2);

	gdk_pixbuf_unref (transparent_pixbuf);
	
        /* compute the image's offset */
	nautilus_icon_canvas_item_get_icon_rectangle
		(container->details->drag_icon->item, &world_rect);
	nautilus_gnome_canvas_world_to_window_rectangle
		(canvas, &world_rect, &window_rect);
        x_offset = dnd_info->drag_info.start_x - window_rect.x0;
        y_offset = dnd_info->drag_info.start_y - window_rect.y0;
        
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

	nautilus_icon_container_ensure_drag_data
		(NAUTILUS_ICON_CONTAINER (widget), context, time);

	g_assert (dnd_info->drag_info.got_drop_data_type);
	switch (dnd_info->drag_info.data_type) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		nautilus_icon_container_receive_dropped_icons
			(NAUTILUS_ICON_CONTAINER (widget),
			 context, x, y);
		gtk_drag_finish (context, TRUE, FALSE, time);
		break;
	case NAUTILUS_ICON_DND_COLOR:
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (widget),
			 widget, x, y, dnd_info->drag_info.selection_data);
		gtk_drag_finish (context, TRUE, FALSE, time);
		break;
	case NAUTILUS_ICON_DND_BGIMAGE:
		receive_dropped_tile_image
			(NAUTILUS_ICON_CONTAINER (widget),
			 dnd_info->drag_info.selection_data->data);
		gtk_drag_finish (context, FALSE, FALSE, time);
		break;
	case NAUTILUS_ICON_DND_KEYWORD:
		receive_dropped_keyword
			(NAUTILUS_ICON_CONTAINER (widget),
			 dnd_info->drag_info.selection_data->data, x, y);
		gtk_drag_finish (context, FALSE, FALSE, time);
		break;
	default:
		gtk_drag_finish (context, FALSE, FALSE, time);
	}

	nautilus_icon_container_free_drag_data (NAUTILUS_ICON_CONTAINER (widget));

	if (saved_drag_source_info != NULL) {
		gtk_signal_disconnect_by_func
			(GTK_OBJECT (saved_drag_source_info->ipc_widget),
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
	if (saved_drag_source_info == NULL) {
		return;
	}

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
