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
            Darin Adler <darin@bentspoon.com>,
	    Andy Hertzfeld <andy@eazel.com>
	    Pavel Cisler <pavel@eazel.com>
*/


#include <config.h>
#include <math.h>
#include "nautilus-icon-dnd.h"

#include "nautilus-file-dnd.h"
#include "nautilus-icon-private.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include <eel/eel-background.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <eel/eel-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <stdio.h>
#include <string.h>

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL },
	{ NAUTILUS_ICON_DND_TEXT_TYPE, 0, NAUTILUS_ICON_DND_TEXT }
};

static GtkTargetEntry drop_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL },
	{ NAUTILUS_ICON_DND_COLOR_TYPE, 0, NAUTILUS_ICON_DND_COLOR },
	{ NAUTILUS_ICON_DND_BGIMAGE_TYPE, 0, NAUTILUS_ICON_DND_BGIMAGE },
	{ NAUTILUS_ICON_DND_KEYWORD_TYPE, 0, NAUTILUS_ICON_DND_KEYWORD },
	{ NAUTILUS_ICON_DND_RESET_BACKGROUND_TYPE,  0, NAUTILUS_ICON_DND_RESET_BACKGROUND },
	/* Must be last: */
	{ NAUTILUS_ICON_DND_ROOTWINDOW_DROP_TYPE,  0, NAUTILUS_ICON_DND_ROOTWINDOW_DROP }
};

static GtkTargetList *drop_types_list = NULL;
static GtkTargetList *drop_types_list_root = NULL;

static EelCanvasItem *
create_selection_shadow (NautilusIconContainer *container,
			 GList *list)
{
	EelCanvasGroup *group;
	EelCanvas *canvas;
	GdkBitmap *stipple;
	int max_x, max_y;
	int min_x, min_y;
	GList *p;

	if (list == NULL) {
		return NULL;
	}

	/* if we're only dragging a single item, don't worry about the shadow */
	if (list->next == NULL) {
		return NULL;
	}
		
	stipple = container->details->dnd_info->stipple;
	g_return_val_if_fail (stipple != NULL, NULL);

	canvas = EEL_CANVAS (container);

	/* Creating a big set of rectangles in the canvas can be expensive, so
           we try to be smart and only create the maximum number of rectangles
           that we will need, in the vertical/horizontal directions.  */

	max_x = GTK_WIDGET (container)->allocation.width;
	min_x = -max_x;

	max_y = GTK_WIDGET (container)->allocation.height;
	min_y = -max_y;

	/* Create a group, so that it's easier to move all the items around at
           once.  */
	group = EEL_CANVAS_GROUP
		(eel_canvas_item_new (EEL_CANVAS_GROUP (canvas->root),
					eel_canvas_group_get_type (),
					NULL));
	
	for (p = list; p != NULL; p = p->next) {
		NautilusDragSelectionItem *item;
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
			eel_canvas_item_new
				(group,
				 eel_canvas_rect_get_type (),
				 "x1", (double) x1,
				 "y1", (double) y1,
				 "x2", (double) x2,
				 "y2", (double) y2,
				 "outline_color", "black",
				 "outline_stipple", stipple,
				 "width_pixels", 1,
				 NULL);
	}

	return EEL_CANVAS_ITEM (group);
}

/* Set the affine instead of the x and y position.
 * Simple, and setting x and y was broken at one point.
 */
static void
set_shadow_position (EelCanvasItem *shadow,
		     double x, double y)
{
	eel_canvas_item_set (shadow,
			     "x", x, "y", y,
			     NULL);
}


/* Source-side handling of the drag. */

/* iteration glue struct */
typedef struct {
	gpointer iterator_context;
	NautilusDragEachSelectedItemDataGet iteratee;
	gpointer iteratee_data;
} IconGetDataBinderContext;

static void
canvas_rect_world_to_widget (EelCanvas *canvas,
			     ArtDRect *world_rect,
			     ArtIRect *widget_rect)
{
	ArtDRect window_rect;
	
	eel_canvas_world_to_window (canvas,
				    world_rect->x0, world_rect->y0,
				    &window_rect.x0, &window_rect.y0);
	eel_canvas_world_to_window (canvas,
				    world_rect->x1, world_rect->y1,
				    &window_rect.x1, &window_rect.y1);
	widget_rect->x0 = (int) window_rect.x0 - gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (canvas)));
	widget_rect->y0 = (int) window_rect.y0 - gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (canvas)));
	widget_rect->x1 = (int) window_rect.x1 - gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (canvas)));
	widget_rect->y1 = (int) window_rect.y1 - gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (canvas)));
}

static void
canvas_widget_to_world (EelCanvas *canvas,
			double widget_x, double widget_y,
			double *world_x, double *world_y)
{
	eel_canvas_window_to_world (canvas,
				    widget_x + gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (canvas))),
				    widget_y + gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (canvas))),
				    world_x, world_y);
}

static gboolean
icon_get_data_binder (NautilusIcon *icon, gpointer data)
{
	IconGetDataBinderContext *context;
	ArtDRect world_rect;
	ArtIRect widget_rect;
	char *uri;
	NautilusIconContainer *container;

	context = (IconGetDataBinderContext *)data;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (context->iterator_context));

	container = NAUTILUS_ICON_CONTAINER (context->iterator_context);

	world_rect = nautilus_icon_canvas_item_get_icon_rectangle (icon->item);

	canvas_rect_world_to_widget (EEL_CANVAS (container), &world_rect, &widget_rect);

	uri = nautilus_icon_container_get_icon_uri (container, icon);
	if (uri == NULL) {
		g_warning ("no URI for one of the iterated icons");
		return TRUE;
	}

	widget_rect = eel_art_irect_offset_by (widget_rect, 
		- container->details->dnd_info->drag_info.start_x,
		- container->details->dnd_info->drag_info.start_y);

	widget_rect = eel_art_irect_scale_by (widget_rect, 
		1 / EEL_CANVAS (container)->pixels_per_unit);
	
	/* pass the uri, mouse-relative x/y and icon width/height */
	context->iteratee (uri, 
			   (int) widget_rect.x0,
			   (int) widget_rect.y0,
			   widget_rect.x1 - widget_rect.x0,
			   widget_rect.y1 - widget_rect.y0,
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
 * to help iterate over all selected items, passing uris, x, y, w and h
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
	EelCanvasItem *shadow;
	double world_x, world_y;

	shadow = container->details->dnd_info->shadow;
	if (shadow == NULL) {
		return;
	}

	canvas_widget_to_world (EEL_CANVAS (container), x, y,
				&world_x, &world_y);

	set_shadow_position (shadow, world_x, world_y);
	eel_canvas_item_show (shadow);
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
		/* FIXME bugzilla.gnome.org 42484: 
		 * Is a destroy really sufficient here? Who does the unref? */
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
	}

	/* Build the selection list and the shadow. */
	dnd_info->drag_info.selection_list = nautilus_drag_build_selection_list (data);
	dnd_info->shadow = create_selection_shadow (container, dnd_info->drag_info.selection_list);
	nautilus_icon_container_position_shadow (container, x, y);
}

/* FIXME bugzilla.gnome.org 47445: Needs to become a shared function */
static void
get_data_on_first_target_we_support (GtkWidget *widget, GdkDragContext *context, guint32 time)
{
	GList *target;
	GtkTargetList *list;
	
	if (drop_types_list == NULL)
		drop_types_list = gtk_target_list_new (drop_types,
						       G_N_ELEMENTS (drop_types) - 1);
	if (drop_types_list_root == NULL)
		drop_types_list_root = gtk_target_list_new (drop_types,
							    G_N_ELEMENTS (drop_types));

	if (nautilus_icon_container_get_is_desktop (NAUTILUS_ICON_CONTAINER (widget))) {
		list = drop_types_list_root;
	} else {
		list = drop_types_list;
	}
	
	for (target = context->targets; target != NULL; target = target->next) {
		guint info;
		GdkAtom target_atom = GDK_POINTER_TO_ATOM (target->data);
		NautilusDragInfo *drag_info;

		drag_info = &(NAUTILUS_ICON_CONTAINER (widget)->details->dnd_info->drag_info);

		if (gtk_target_list_find (list, 
					  target_atom,
					  &info)) {
			/* Don't get_data for rootwindow drops unless it's the actual drop */
			if (info == NAUTILUS_ICON_DND_ROOTWINDOW_DROP &&
			    !drag_info->drop_occured) {
				/* We can't call get_data here, because that would
				   make the source execute the rootwin action */
				drag_info->got_drop_data_type = TRUE;
				drag_info->data_type = NAUTILUS_ICON_DND_ROOTWINDOW_DROP;
			} else {
				gtk_drag_get_data (GTK_WIDGET (widget), context,
						   target_atom,
						   time);
			}
			break;
		}
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
		get_data_on_first_target_we_support (GTK_WIDGET (container), context, time);
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
	int size;
	ArtDRect point;
	ArtIRect canvas_point;

	/* build the hit-test rectangle. Base the size on the scale factor to ensure that it is
	 * non-empty even at the smallest scale factor
	 */
	
	size = MAX (1, 1 + (1 / EEL_CANVAS (container)->pixels_per_unit));
	point.x0 = x;
	point.y0 = y;
	point.x1 = x + size;
	point.y1 = y + size;

	for (p = container->details->icons; p != NULL; p = p->next) {
		NautilusIcon *icon;
		icon = p->data;
		
		eel_canvas_w2c (EEL_CANVAS (container),
				point.x0,
				point.y0,
				&canvas_point.x0,
				&canvas_point.y0);
		eel_canvas_w2c (EEL_CANVAS (container),
				point.x1,
				point.y1,
				&canvas_point.x1,
				&canvas_point.y1);
		if (nautilus_icon_canvas_item_hit_test_rectangle (icon->item, canvas_point)) {
			return icon;
		}
	}
	
	return NULL;
}

static char *
get_container_uri (NautilusIconContainer *container)
{
	char *uri;

	/* get the URI associated with the container */
	uri = NULL;
	g_signal_emit_by_name (container, "get_container_uri", &uri);
	return uri;
}

static gboolean
nautilus_icon_container_selection_items_local (NautilusIconContainer *container,
					       GList *items)
{
	char *container_uri_string;
	gboolean result;

	/* must have at least one item */
	g_assert (items);

	result = FALSE;

	/* get the URI associated with the container */
	container_uri_string = get_container_uri (container);
	
	if (eel_uri_is_trash (container_uri_string)) {
		/* Special-case "trash:" because the nautilus_drag_items_local
		 * would not work for it.
		 */
		result = nautilus_drag_items_in_trash (items);
	} else if (eel_uri_is_desktop (container_uri_string)) {
		result = nautilus_drag_items_on_desktop (items);
	} else {
		result = nautilus_drag_items_local (container_uri_string, items);
	}
	g_free (container_uri_string);
	
	return result;
}

static GdkDragAction 
get_background_drag_action (NautilusIconContainer *container, 
			    GdkDragAction action)
{
	/* FIXME: This function is very FMDirectoryView specific, and
	 * should be moved out of nautilus-icon-dnd.c */
	GdkDragAction valid_actions;

	if (action == GDK_ACTION_ASK) {
		valid_actions = NAUTILUS_DND_ACTION_SET_AS_BACKGROUND;
		if (g_object_get_data (G_OBJECT (eel_get_widget_background (GTK_WIDGET (container))), "is_desktop") == 0) {
			valid_actions |= NAUTILUS_DND_ACTION_SET_AS_GLOBAL_BACKGROUND;
		}

		action = nautilus_drag_drop_background_ask 
			(GTK_WIDGET (container), valid_actions);
	}

	return action;
}

static void
receive_dropped_color (NautilusIconContainer *container,
		       int x, int y,
		       GdkDragAction action,
		       GtkSelectionData *data)
{
	action = get_background_drag_action (container, action);
	
	if (action > 0) {
		eel_background_receive_dropped_color
			(eel_get_widget_background (GTK_WIDGET (container)),
			 GTK_WIDGET (container), 
			 action, x, y, data);
	}
}

/* handle dropped tile images */
static void
receive_dropped_tile_image (NautilusIconContainer *container, GdkDragAction action, GtkSelectionData *data)
{
	g_assert (data != NULL);

	action = get_background_drag_action (container, action);

	if (action > 0) {
		eel_background_receive_dropped_background_image
			(eel_get_widget_background (GTK_WIDGET (container)), 
			 action, 
			 data->data);
	}
}

/* handle dropped keywords */
static void
receive_dropped_keyword (NautilusIconContainer *container, const char *keyword, int x, int y)
{
	char *uri;
	double world_x, world_y;

	NautilusIcon *drop_target_icon;
	NautilusFile *file;
	
	g_assert (keyword != NULL);

	/* find the item we hit with our drop, if any */
	canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);
	drop_target_icon = nautilus_icon_container_item_at (container, world_x, world_y);
	if (drop_target_icon == NULL) {
		return;
	}

	/* FIXME bugzilla.gnome.org 42485: 
	 * This does not belong in the icon code.
	 * It has to be in the file manager.
	 * The icon code has no right to deal with the file directly.
	 * But luckily there's no issue of not getting a file object,
	 * so we don't have to worry about async. issues here.
	 */
	uri = nautilus_icon_container_get_icon_uri (container, drop_target_icon);
	file = nautilus_file_get (uri);
	g_free (uri);
	
	nautilus_drag_file_receive_dropped_keyword (file, keyword);

	nautilus_file_unref (file);
	nautilus_icon_container_update_icon (container, drop_target_icon);
}

/* handle dropped uri list */
static void
receive_dropped_uri_list (NautilusIconContainer *container, const char *uri_list, GdkDragAction action, int x, int y)
{	
	if (uri_list == NULL) {
		return;
	}
	
	g_signal_emit_by_name (container, "handle_uri_list",
				 uri_list,
				 action,
				 x, y);
}

static int
auto_scroll_timeout_callback (gpointer data)
{
	NautilusIconContainer *container;
	GtkWidget *widget;
	float x_scroll_delta, y_scroll_delta;
	GdkRectangle exposed_area;

	g_assert (NAUTILUS_IS_ICON_CONTAINER (data));
	widget = GTK_WIDGET (data);
	container = NAUTILUS_ICON_CONTAINER (widget);

	if (container->details->dnd_info->drag_info.waiting_to_autoscroll
	    && container->details->dnd_info->drag_info.start_auto_scroll_in > eel_get_system_time()) {
		/* not yet */
		return TRUE;
	}

	container->details->dnd_info->drag_info.waiting_to_autoscroll = FALSE;

	nautilus_drag_autoscroll_calculate_delta (widget, &x_scroll_delta, &y_scroll_delta);
	if (x_scroll_delta == 0 && y_scroll_delta == 0) {
		/* no work */
		return TRUE;
	}

	if (!nautilus_icon_container_scroll (container, (int)x_scroll_delta, (int)y_scroll_delta)) {
		/* the scroll value got pinned to a min or max adjustment value,
		 * we ended up not scrolling
		 */
		return TRUE;
	}

	/* update cached drag start offsets */
	container->details->dnd_info->drag_info.start_x -= x_scroll_delta;
	container->details->dnd_info->drag_info.start_y -= y_scroll_delta;

	/* Due to a glitch in GtkLayout, whe need to do an explicit draw of the exposed
	 * area. 
	 * Calculate the size of the area we need to draw
	 */
	exposed_area.x = widget->allocation.x;
	exposed_area.y = widget->allocation.y;
	exposed_area.width = widget->allocation.width;
	exposed_area.height = widget->allocation.height;

	if (x_scroll_delta > 0) {
		exposed_area.x = exposed_area.width - x_scroll_delta;
	} else if (x_scroll_delta < 0) {
		exposed_area.width = -x_scroll_delta;
	}

	if (y_scroll_delta > 0) {
		exposed_area.y = exposed_area.height - y_scroll_delta;
	} else if (y_scroll_delta < 0) {
		exposed_area.height = -y_scroll_delta;
	}

	/* offset it to 0, 0 */
	exposed_area.x -= widget->allocation.x;
	exposed_area.y -= widget->allocation.y;

	gtk_widget_queue_draw_area (widget,
				    exposed_area.x,
				    exposed_area.y,
				    exposed_area.width,
				    exposed_area.height);

	return TRUE;
}

static void
set_up_auto_scroll_if_needed (NautilusIconContainer *container)
{
	nautilus_drag_autoscroll_start (&container->details->dnd_info->drag_info,
					GTK_WIDGET (container),
					auto_scroll_timeout_callback,
					container);
}

static void
stop_auto_scroll (NautilusIconContainer *container)
{
	nautilus_drag_autoscroll_stop (&container->details->dnd_info->drag_info);
}

static gboolean
confirm_switch_to_manual_layout (NautilusIconContainer *container)
{
#if 0
	const char *message;
	const char *detail;
	GtkDialog *dialog;
	int response;

	/* FIXME bugzilla.gnome.org 40915: Use of the word "directory"
	 * makes this FMIconView specific. Move these messages into
	 * FMIconView so NautilusIconContainer can be used for things
	 * that are not directories?
	 */
	if (nautilus_icon_container_has_stored_icon_positions (container)) {
		if (eel_g_list_exactly_one_item (container->details->dnd_info->drag_info.selection_list)) {
			message = _("Do you want to switch to manual layout and leave this item where you dropped it? "
			"This will clobber the stored manual layout.");
			detail = _("This folder uses automatic layout.");
		} else {
			message = _("Do you want to switch to manual layout and leave these items where you dropped them? "
			"This will clobber the stored manual layout.");
			detail = _("This folder uses automatic layout.");
		}
	} else {
		if (eel_g_list_exactly_one_item (container->details->dnd_info->drag_info.selection_list)) {
			message = _("Do you want to switch to manual layout and leave this item where you dropped it?");
			detail = _("This folder uses automatic layout.");
		} else {
			message = _("Do you want to switch to manual layout and leave these items where you dropped them?");
			detail = _("This folder uses automatic layout.");

		}
	}

	dialog = eel_show_yes_no_dialog (message, detail, _("Switch to Manual Layout?"),
					 _("Switch"), GTK_STOCK_CANCEL,
					 GTK_WINDOW (gtk_widget_get_toplevel(GTK_WIDGET(container))));

	response = gtk_dialog_run (dialog);
	gtk_object_destroy (GTK_OBJECT (dialog));

	return response == GTK_RESPONSE_YES;
#else
	return FALSE;
#endif
}

static void
handle_local_move (NautilusIconContainer *container,
		   double world_x, double world_y)
{
	GList *moved_icons, *p;
	NautilusDragSelectionItem *item;
	NautilusIcon *icon;
	NautilusFile *file;
	char *screen_string;
	GdkScreen *screen;

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

		if (icon == NULL) {
			/* probably dragged from another screen.  Add it to
			 * this screen
			 */

			file = nautilus_file_get (item->uri);

			screen = gtk_widget_get_screen (GTK_WIDGET (container));
			screen_string = g_strdup_printf ("%d",
						gdk_screen_get_number (screen));
			nautilus_file_set_metadata (file,
					NAUTILUS_METADATA_KEY_SCREEN,
					NULL, screen_string);

			g_free (screen_string);

			nautilus_icon_container_add (container,
					NAUTILUS_ICON_CONTAINER_ICON_DATA (file));
			
			icon = nautilus_icon_container_get_icon_by_uri
				(container, item->uri);
		}

		if (item->got_icon_position) {
			nautilus_icon_container_move_icon
				(container, icon,
				 world_x + item->icon_x, world_y + item->icon_y,
				 icon->scale_x, icon->scale_y,
				 TRUE, TRUE, TRUE);
		}
		moved_icons = g_list_prepend (moved_icons, icon);
	}		
	nautilus_icon_container_select_list_unselect_others
		(container, moved_icons);
	/* Might have been moved in a way that requires adjusting scroll region. */
	nautilus_icon_container_update_scroll_region (container);
	g_list_free (moved_icons);
}

static void
handle_nonlocal_move (NautilusIconContainer *container,
		      GdkDragContext *context,
		      int x, int y,
		      const char *target_uri,
		      gboolean icon_hit)
{
	GList *source_uris, *p;
	GArray *source_item_locations;
	gboolean free_target_uri;
	int index;

	if (container->details->dnd_info->drag_info.selection_list == NULL) {
		return;
	}

	source_uris = NULL;
	for (p = container->details->dnd_info->drag_info.selection_list; p != NULL; p = p->next) {
		/* do a shallow copy of all the uri strings of the copied files */
		source_uris = g_list_prepend (source_uris, ((NautilusDragSelectionItem *)p->data)->uri);
	}
	source_uris = g_list_reverse (source_uris);
	
	source_item_locations = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
	if (!icon_hit) {
		/* Drop onto a container. Pass along the item points to allow placing
		 * the items in their same relative positions in the new container.
		 */
		source_item_locations = g_array_set_size (source_item_locations,
			g_list_length (container->details->dnd_info->drag_info.selection_list));
			
		for (index = 0, p = container->details->dnd_info->drag_info.selection_list;
			p != NULL; index++, p = p->next) {
		     	g_array_index (source_item_locations, GdkPoint, index).x =
		     		((NautilusDragSelectionItem *)p->data)->icon_x;
		     	g_array_index (source_item_locations, GdkPoint, index).y =
				((NautilusDragSelectionItem *)p->data)->icon_y;
		}
	}

	free_target_uri = FALSE;
 	/* Rewrite internal desktop URIs to the normal target uri */
	if (eel_uri_is_desktop (target_uri)) {
		target_uri = nautilus_get_desktop_directory_uri ();
		free_target_uri = TRUE;
	}
	
	/* start the copy */
	g_signal_emit_by_name (container, "move_copy_items",
				 source_uris,
				 source_item_locations,
				 target_uri,
				 context->action,
				 x, y);

	if (free_target_uri) {
		g_free ((char *)target_uri);
	}

	g_list_free (source_uris);
	g_array_free (source_item_locations, TRUE);
}

static char *
nautilus_icon_container_find_drop_target (NautilusIconContainer *container,
					  GdkDragContext *context,
					  int x, int y,
					  gboolean *icon_hit)
{
	NautilusIcon *drop_target_icon;
	double world_x, world_y;
	NautilusFile *file;
	char *icon_uri;

	*icon_hit = FALSE;
	if (container->details->dnd_info->drag_info.selection_list == NULL) {
		return NULL;
	}

	canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);
	
	/* FIXME bugzilla.gnome.org 42485: 
	 * These "can_accept_items" tests need to be done by
	 * the icon view, not here. This file is not supposed to know
	 * that the target is a file.
	 */

	/* Find the item we hit with our drop, if any */	
	drop_target_icon = nautilus_icon_container_item_at (container, world_x, world_y);
	if (drop_target_icon != NULL) {
		icon_uri = nautilus_icon_container_get_icon_uri (container, drop_target_icon);
		if (icon_uri != NULL) {
			file = nautilus_file_get (icon_uri);

			if (!nautilus_drag_can_accept_items (file, 
					container->details->dnd_info->drag_info.selection_list)) {
			 	/* the item we dropped our selection on cannot accept the items,
			 	 * do the same thing as if we just dropped the items on the canvas
				 */
				drop_target_icon = NULL;
			}
			
			g_free (icon_uri);
			nautilus_file_unref (file);
		}
	}

	if (drop_target_icon == NULL) {
		*icon_hit = FALSE;
		return get_container_uri (container);
	}
	
	*icon_hit = TRUE;
	return nautilus_icon_container_get_icon_drop_target_uri (container, drop_target_icon);
}

static gboolean
selection_is_image_file (GList *selection_list)
{
	char *mime_type;
	NautilusDragSelectionItem *selected_item;
	gboolean result;

	/* Make sure only one item is selected */
	if (selection_list == NULL ||
	    selection_list->next != NULL) {
		return FALSE;
	}

	selected_item = selection_list->data;

	mime_type = gnome_vfs_get_mime_type (selected_item->uri);

	result = (g_ascii_strcasecmp (mime_type, "image/svg") != 0 &&
		  eel_istr_has_prefix (mime_type, "image/"));
	
	g_free (mime_type);
	
	return result;
}


static void
nautilus_icon_container_receive_dropped_icons (NautilusIconContainer *container,
					       GdkDragContext *context,
					       int x, int y)
{
	char *drop_target;
	gboolean local_move_only;
	double world_x, world_y;
	gboolean icon_hit;
	GdkDragAction action;
	NautilusDragSelectionItem *selected_item;

	drop_target = NULL;

	if (container->details->dnd_info->drag_info.selection_list == NULL) {
		return;
	}

	if (context->action == GDK_ACTION_ASK) {
		/* FIXME bugzilla.gnome.org 42485: This belongs in FMDirectoryView, not here. */
		/* Check for special case items in selection list */
		if (nautilus_drag_selection_includes_special_link (container->details->dnd_info->drag_info.selection_list)) {
			/* We only want to move the trash */
			action = GDK_ACTION_MOVE;
		} else {
			action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
			
			if (selection_is_image_file (container->details->dnd_info->drag_info.selection_list)) {
				action |= NAUTILUS_DND_ACTION_SET_AS_BACKGROUND;
			}
		}
		context->action = nautilus_drag_drop_action_ask
			(GTK_WIDGET (container), action);
	}
	
	if (context->action == NAUTILUS_DND_ACTION_SET_AS_BACKGROUND) {
		selected_item = container->details->dnd_info->drag_info.selection_list->data;
		eel_background_receive_dropped_background_image
			(eel_get_widget_background (GTK_WIDGET (container)),
			 context->action,
			 selected_item->uri);
		return;
	}
		
	if (context->action > 0) {
		eel_canvas_window_to_world (EEL_CANVAS (container),
					    x + gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (container))),
					    y + gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (container))),
					    &world_x, &world_y);

		drop_target = nautilus_icon_container_find_drop_target (container, 
			context, x, y, &icon_hit);

		local_move_only = FALSE;
		if (!icon_hit && context->action == GDK_ACTION_MOVE) {
			/* we can just move the icon positions if the move ended up in
			 * the item's parent container
			 */
			local_move_only = nautilus_icon_container_selection_items_local
				(container, container->details->dnd_info->drag_info.selection_list);
		}

		if (local_move_only) {
			handle_local_move (container, world_x, world_y);
		} else {
			handle_nonlocal_move (container, context, world_x, world_y, drop_target, icon_hit);
		}
	}

	g_free (drop_target);
	nautilus_drag_destroy_selection_list (container->details->dnd_info->drag_info.selection_list);
	container->details->dnd_info->drag_info.selection_list = NULL;
}

static void
nautilus_icon_container_get_drop_action (NautilusIconContainer *container,
					 GdkDragContext *context,
					 int x, int y,
					 int *action)
{
	char *drop_target;
	gboolean icon_hit;
	NautilusIcon *icon;
	double world_x, world_y;

	icon_hit = FALSE;
	if (!container->details->dnd_info->drag_info.got_drop_data_type) {
		/* drag_data_received_callback didn't get called yet */
		return;
	}

	/* find out if we're over an icon */
	canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);
	
	icon = nautilus_icon_container_item_at (container, world_x, world_y);

	*action = 0;

	/* case out on the type of object being dragged */
	switch (container->details->dnd_info->drag_info.data_type) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		if (container->details->dnd_info->drag_info.selection_list == NULL) {
			return;
		}
		drop_target = nautilus_icon_container_find_drop_target (container,
			context, x, y, &icon_hit);
		if (!drop_target) {
			return;
		}
		nautilus_drag_default_drop_action_for_icons (context, drop_target, 
			container->details->dnd_info->drag_info.selection_list, 
			action);
		g_free (drop_target);
		break;

	/* handle emblems by setting the action if we're over an object */
	case NAUTILUS_ICON_DND_KEYWORD:
		if (icon != NULL) {
			*action = context->suggested_action;
		}
		break;
	
	/* handle colors and backgrounds by setting the action if we're over the background */		
	case NAUTILUS_ICON_DND_COLOR:
	case NAUTILUS_ICON_DND_BGIMAGE:
	case NAUTILUS_ICON_DND_RESET_BACKGROUND:
		if (icon == NULL) {
			*action = context->suggested_action;
		}	
		break;
	
	case NAUTILUS_ICON_DND_URI_LIST:
	case NAUTILUS_ICON_DND_URL:
	case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
		*action = context->suggested_action;
		break;

	case NAUTILUS_ICON_DND_TEXT:
		break;
	}
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
	NautilusFile *file;
	double world_x, world_y;
	char *uri;
	
	g_assert (NAUTILUS_IS_ICON_CONTAINER (container));
	if ((container->details->dnd_info->drag_info.selection_list == NULL) 
	   && (container->details->dnd_info->drag_info.data_type != NAUTILUS_ICON_DND_KEYWORD)) {
		return;
	}

	canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);

	/* Find the item we hit with our drop, if any. */
	icon = nautilus_icon_container_item_at (container, world_x, world_y);

	/* FIXME bugzilla.gnome.org 42485: 
	 * These "can_accept_items" tests need to be done by
	 * the icon view, not here. This file is not supposed to know
	 * that the target is a file.
	 */

	/* Find if target icon accepts our drop. */
	if (icon != NULL && (container->details->dnd_info->drag_info.data_type != NAUTILUS_ICON_DND_KEYWORD)) {
		    uri = nautilus_icon_container_get_icon_uri (container, icon);
		    file = nautilus_file_get (uri);
		    g_free (uri);
		
		    if (!nautilus_drag_can_accept_items (file,
							 container->details->dnd_info->drag_info.selection_list)) {
			    icon = NULL;
		    }

		    nautilus_file_unref (file);
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
		gtk_selection_data_free (dnd_info->drag_info.selection_data);
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
		eel_canvas_item_hide (dnd_info->shadow);
	
	set_drop_target (NAUTILUS_ICON_CONTAINER (widget), NULL);
	stop_auto_scroll (NAUTILUS_ICON_CONTAINER (widget));
	nautilus_icon_container_free_drag_data(NAUTILUS_ICON_CONTAINER (widget));
}

void
nautilus_icon_dnd_begin_drag (NautilusIconContainer *container,
			      GdkDragAction actions,
			      int button,
			      GdkEventMotion *event)
{
	NautilusIconDndInfo *dnd_info;
	EelCanvas *canvas;
	GdkDragContext *context;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	int x_offset, y_offset;
	ArtDRect world_rect;
	ArtIRect widget_rect;
	
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
	g_return_if_fail (event != NULL);

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);
	
	/* Notice that the event is in bin_window coordinates, because of
           the way the canvas handles events.
	*/
	canvas = EEL_CANVAS (container);
	dnd_info->drag_info.start_x = event->x - gtk_adjustment_get_value (gtk_layout_get_hadjustment (GTK_LAYOUT (canvas)));
	dnd_info->drag_info.start_y = event->y - gtk_adjustment_get_value (gtk_layout_get_vadjustment (GTK_LAYOUT (canvas)));	

        /* create a pixmap and mask to drag with */
        pixmap = nautilus_icon_canvas_item_get_image (container->details->drag_icon->item, &mask);
    
    	/* we want to drag semi-transparent pixbufs, but X is too slow dealing with
	   stippled masks, so we had to remove the code; this comment is left as a memorial
	   to it, with the hope that we get it back someday as X Windows improves */
	
        /* compute the image's offset */
	world_rect = nautilus_icon_canvas_item_get_icon_rectangle
		(container->details->drag_icon->item);

	canvas_rect_world_to_widget (EEL_CANVAS (container),
				     &world_rect, &widget_rect);
	
        x_offset = dnd_info->drag_info.start_x - widget_rect.x0;
        y_offset = dnd_info->drag_info.start_y - widget_rect.y0;
        
	/* start the drag */
	context = gtk_drag_begin (GTK_WIDGET (container),
				  dnd_info->drag_info.target_list,
				  actions,
				  button,
				  (GdkEvent *) event);

	if (context) {
		/* set the icon for dragging */
		gtk_drag_set_icon_pixmap (context,
					  gtk_widget_get_colormap (GTK_WIDGET (container)),
					  pixmap, mask,
					  x_offset, y_offset);
	}
}

static gboolean
drag_motion_callback (GtkWidget *widget,
		      GdkDragContext *context,
		      int x, int y,
		      guint32 time)
{
	int action;
	
	nautilus_icon_container_ensure_drag_data (NAUTILUS_ICON_CONTAINER (widget), context, time);
	nautilus_icon_container_position_shadow (NAUTILUS_ICON_CONTAINER (widget), x, y);
	nautilus_icon_dnd_update_drop_target (NAUTILUS_ICON_CONTAINER (widget), context, x, y);
	set_up_auto_scroll_if_needed (NAUTILUS_ICON_CONTAINER (widget));
	/* Find out what the drop actions are based on our drag selection and
	 * the drop target.
	 */
	action = 0;
	nautilus_icon_container_get_drop_action (NAUTILUS_ICON_CONTAINER (widget), context, x, y,
						 &action);
	gdk_drag_status (context, action, time);

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

	/* tell the drag_data_received callback that
	   the drop occured and that it can actually
	   process the actions.
	   make sure it is going to be called at least once.
	*/
	dnd_info->drag_info.drop_occured = TRUE;

	get_data_on_first_target_we_support (widget, context, time);

	return FALSE;
}

void
nautilus_icon_dnd_end_drag (NautilusIconContainer *container)
{
	NautilusIconDndInfo *dnd_info;

	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));
		
	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);
	stop_auto_scroll (container);
	/* Do nothing.
	 * Can that possibly be right?
	 */
}

/**
 * implements the gnome 1.x gnome_vfs_uri_list_extract_uris(), which
 * returns a GList of char *uris.
 **/
GList *
nautilus_icon_dnd_uri_list_extract_uris (const char *uri_list)
{
	/* Note that this is mostly very stolen from old libgnome/gnome-mime.c */

	const gchar *p, *q;
	gchar *retval;
	GList *result = NULL;

	g_return_val_if_fail (uri_list != NULL, NULL);

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	 * 2396, or even check for allowed characters - we just ignore
	 * comments and trim whitespace off the ends.  We also
	 * allow LF delimination as well as the specified CRLF.
	 */
	while (p != NULL) {
		if (*p != '#') {
			while (g_ascii_isspace (*p))
				p++;

			q = p;
			while ((*q != '\0')
			       && (*q != '\n')
			       && (*q != '\r'))
				q++;

			if (q > p) {
				q--;
				while (q > p
				       && g_ascii_isspace (*q))
					q--;

				retval = g_malloc (q - p + 2);
				strncpy (retval, p, q - p + 1);
				retval[q - p + 1] = '\0';

				result = g_list_prepend (result, retval);
			}
		}
		p = strchr (p, '\n');
		if (p != NULL)
			p++;
	}

	return g_list_reverse (result);
}

/**
 * nautilus_icon_dnd_uri_list_free_strings:
 * @list: A GList returned by nautilus_icon_dnd_uri_list_extract_uris()
 *
 * Releases all of the resources allocated by @list.
 * 
 * stolen from gnome-mime.c
 */
void
nautilus_icon_dnd_uri_list_free_strings (GList *list)
{
	eel_g_list_free_deep (list);
}

/** this callback is called in 2 cases.
    It is called upon drag_motion events to get the actual data 
    In that case, it just makes sure it gets the data.
    It is called upon drop_drop events to execute the actual 
    actions on the received action. In that case, it actually first makes sure
    that we have got the data then processes it.
*/

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
    	NautilusDragInfo *drag_info;
	EelBackground *background;
	gboolean success;

	drag_info = &(NAUTILUS_ICON_CONTAINER (widget)->details->dnd_info->drag_info);

	drag_info->got_drop_data_type = TRUE;
	drag_info->data_type = info;

	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		nautilus_icon_container_dropped_icon_feedback (widget, data, x, y);
		break;
	case NAUTILUS_ICON_DND_COLOR:
	case NAUTILUS_ICON_DND_BGIMAGE:	
	case NAUTILUS_ICON_DND_KEYWORD:
	case NAUTILUS_ICON_DND_URI_LIST:
	case NAUTILUS_ICON_DND_RESET_BACKGROUND:
		/* Save the data so we can do the actual work on drop. */
		if (drag_info->selection_data != NULL) {
			gtk_selection_data_free (drag_info->selection_data);
		}
		drag_info->selection_data = gtk_selection_data_copy (data);
		break;

	/* Netscape keeps sending us the data, even though we accept the first drag */
	case NAUTILUS_ICON_DND_URL:
		if (drag_info->selection_data != NULL) {
			gtk_selection_data_free (drag_info->selection_data);
			drag_info->selection_data = gtk_selection_data_copy (data);
		}
		break;
	case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
		/* Do nothing, this won't even happen, since we don't want to call get_data twice */
		break;
	}

	/* this is the second use case of this callback.
	 * we have to do the actual work for the drop.
	 */
	if (drag_info->drop_occured) {

		success = FALSE;
		switch (info) {
		case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
			nautilus_icon_container_receive_dropped_icons
				(NAUTILUS_ICON_CONTAINER (widget),
				 context, x, y);
			break;
		case NAUTILUS_ICON_DND_COLOR:
			receive_dropped_color (NAUTILUS_ICON_CONTAINER (widget),
					       x, y,
					       context->action,
					       data);
			success = TRUE;
			break;
		case NAUTILUS_ICON_DND_BGIMAGE:
			receive_dropped_tile_image
				(NAUTILUS_ICON_CONTAINER (widget),
				 context->action,
				 data);
			break;
		case NAUTILUS_ICON_DND_KEYWORD:
			receive_dropped_keyword
				(NAUTILUS_ICON_CONTAINER (widget),
				 (char *) data->data, x, y);
			break;
		case NAUTILUS_ICON_DND_URI_LIST:
		case NAUTILUS_ICON_DND_URL:
			receive_dropped_uri_list
				(NAUTILUS_ICON_CONTAINER (widget),
				 (char *) data->data, context->action, x, y);
			success = TRUE;
			break;
		case NAUTILUS_ICON_DND_RESET_BACKGROUND:
			background = eel_get_widget_background (widget);
			if (background != NULL) {
				eel_background_reset (background);
			}
			gtk_drag_finish (context, FALSE, FALSE, time);			
			break;
		case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
			/* Do nothing, everything is done by the sender */
			break;
		}
		gtk_drag_finish (context, success, FALSE, time);
		
		nautilus_icon_container_free_drag_data (NAUTILUS_ICON_CONTAINER (widget));
		
		set_drop_target (NAUTILUS_ICON_CONTAINER (widget), NULL);

		/* reinitialise it for the next dnd */
		drag_info->drop_occured = FALSE;
	}

}

void
nautilus_icon_dnd_set_stipple (NautilusIconContainer *container,
			       GdkBitmap             *stipple)
{
	if (stipple != NULL) {
		g_object_ref (stipple);
	}
	
	if (container->details->dnd_info->stipple != NULL) {
		g_object_unref (container->details->dnd_info->stipple);
	}

	container->details->dnd_info->stipple = stipple;
}

void
nautilus_icon_dnd_init (NautilusIconContainer *container,
			GdkBitmap *stipple)
{
	int n_elements;
	
	g_return_if_fail (container != NULL);
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));


	container->details->dnd_info = g_new0 (NautilusIconDndInfo, 1);
	nautilus_drag_init (&container->details->dnd_info->drag_info,
		drag_types, G_N_ELEMENTS (drag_types));

	/* Set up the widget as a drag destination.
	 * (But not a source, as drags starting from this widget will be
         * implemented by dealing with events manually.)
	 */
	n_elements = G_N_ELEMENTS (drop_types);
	if (!nautilus_icon_container_get_is_desktop (container)) {
		/* Don't set up rootwindow drop */
		n_elements -= 1;
	}
	gtk_drag_dest_set (GTK_WIDGET (container),
			   0,
			   drop_types, n_elements,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

	/* Messages for outgoing drag. */
	g_signal_connect (container, "drag_data_get",
			  G_CALLBACK (drag_data_get_callback), NULL);
	g_signal_connect (container, "drag_end",
			  G_CALLBACK (drag_end_callback), NULL);
	
	/* Messages for incoming drag. */
	g_signal_connect (container, "drag_data_received",
			  G_CALLBACK (drag_data_received_callback), NULL);
	g_signal_connect (container, "drag_motion",
			  G_CALLBACK (drag_motion_callback), NULL);
	g_signal_connect (container, "drag_drop",
			  G_CALLBACK (drag_drop_callback), NULL);
	g_signal_connect (container, "drag_leave",
			  G_CALLBACK (drag_leave_callback), NULL);

	if (stipple != NULL) {
		container->details->dnd_info->stipple = g_object_ref (stipple);
	}
}

void
nautilus_icon_dnd_fini (NautilusIconContainer *container)
{
	g_return_if_fail (NAUTILUS_IS_ICON_CONTAINER (container));

	if (container->details->dnd_info != NULL) {
		stop_auto_scroll (container);

		if (container->details->dnd_info->stipple != NULL) {
			g_object_unref (container->details->dnd_info->stipple);
		}

		nautilus_drag_finalize (&container->details->dnd_info->drag_info);
		container->details->dnd_info = NULL;
	}
}
