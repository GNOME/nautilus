/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-dnd.c - Drag & drop handling for the icon container
   widget.

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gnome-icon-container-dnd.h"
#include "nautilus-icons-view-icon-item.h"

#include "gnome-icon-container-private.h"
#include "nautilus-background.h"
#include "nautilus-gtk-extensions.h"

#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtksignal.h>


typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} DndSelectionItem;

static GtkTargetEntry drag_types [] = {
	{ GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST },
	{ GNOME_ICON_CONTAINER_DND_URI_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_URI_LIST },
	{ GNOME_ICON_CONTAINER_DND_URL_TYPE, 0, GNOME_ICON_CONTAINER_DND_URL }
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetEntry drop_types [] = {
	{ GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST },
	{ GNOME_ICON_CONTAINER_DND_URI_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_URI_LIST },
	{ GNOME_ICON_CONTAINER_DND_URL_TYPE, 0, GNOME_ICON_CONTAINER_DND_URL },
	{ GNOME_ICON_CONTAINER_DND_COLOR_TYPE, 0, GNOME_ICON_CONTAINER_DND_COLOR }
};
static const int num_drop_types = sizeof (drop_types) / sizeof (drop_types[0]);

static GnomeCanvasItem *
create_selection_shadow (GnomeIconContainer *container,
			 GList *list)
{
	GnomeCanvasGroup *group;
	GnomeCanvas *canvas;
	GdkBitmap *stipple;
	int max_x, max_y;
	int min_x, min_y;
	GList *p;

	if (list == NULL)
	    return NULL;

	stipple = container->details->dnd_info->stipple;
	g_return_val_if_fail (stipple != NULL, NULL);

	canvas = GNOME_CANVAS (container);

	/* Creating a big set of rectangles in the canvas can be expensive, so
           we try to be smart and only create the maximum number of rectangles
           that we will need, in the vertical/horizontal directions.  */

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
	
	for (p = list; p != NULL; p = p->next) {
		DndSelectionItem *item;
		int x1, y1;
		int x2, y2;

		item = p->data;

		if (!item->got_icon_position)
			continue;

		x1 = item->icon_x;
		y1 = item->icon_y;
		x2 = x1 + item->icon_width;
		y2 = y1 + item->icon_height;
			
		if (x2 >= min_x && x1 <= max_x && y2 >= min_y && y1 <= max_y) {
			GnomeCanvasItem *rect;

			rect = gnome_canvas_item_new
				(group,
				 gnome_canvas_rect_get_type (),
				 "x1", (double) x1, "y1", (double) y1,
				 "x2", (double) x2, "y2", (double) y2,
				 "outline_color", "black",
				 "outline_stipple", stipple,
				 "width_pixels", 1,
				 NULL);
		}
	}

	return GNOME_CANVAS_ITEM (group);
}

/* This is a workaround for a gnome-canvas bug: with the current (1.0.18)
   gnome-libs, setting the x/y values for an existing group fails at updating
   the bounds of the group.  So, instead of setting the x/y values to the
   current position at initialization time, we set them to (0,0) and then use a
   simple affine transform.  */
static void
set_shadow_position (GnomeCanvasItem *shadow,
		     gdouble x, gdouble y)
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
set_gnome_icon_list_selection (GnomeIconContainer *container,
			       GtkSelectionData *selection_data)
{
	GnomeIconContainerDetails *details;
	GnomeCanvas* canvas = GNOME_CANVAS(container);
	GList *p;
	GString *data;

	details = container->details;

	data = g_string_new (NULL);
	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		gint center_offset;
		char *uri;
		char *s;
		GdkPixbuf *pixbuf;
                int icon_x, icon_y;

		icon = p->data;
		if (!icon->is_selected)
			continue;

		
		center_offset = nautilus_icons_view_icon_item_center_offset(icon->item);
		
		/* Corner of the icon relative to the cursor. */
		icon_x = icon->x - details->dnd_info->start_x + floor(center_offset / canvas->pixels_per_unit);
		icon_y = icon->y - details->dnd_info->start_y;

		uri = nautilus_icons_controller_get_icon_uri (details->controller, icon->data);
		pixbuf = nautilus_icons_controller_get_icon_image(details->controller, icon->data);
                
                s = g_strdup_printf ("%s\r%d:%d:%d:%d\r\n",
				     uri, icon_x, icon_y, pixbuf->art_pixbuf->width, pixbuf->art_pixbuf->height);
		g_free (uri);
                gdk_pixbuf_unref(pixbuf);
                
		g_string_append (data, s);
		g_free (s);
	}

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, data->str, data->len);

	g_string_free (data, TRUE);
}

/* Encode a "text/uri-list" selection.  */
static void
set_uri_list_selection (GnomeIconContainer *container,
			GtkSelectionData *selection_data)
{
	GnomeIconContainerDetails *details;
	GList *p;
	char *uri;
	GString *data;

	details = container->details;

	data = g_string_new (NULL);

	for (p = details->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (!icon->is_selected)
			continue;

		uri = nautilus_icons_controller_get_icon_uri (details->controller, icon->data);
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
drag_data_get_cb (GtkWidget *widget,
		  GdkDragContext *context,
		  GtkSelectionData *selection_data,
		  guint info,
		  guint32 time,
		  gpointer data)
{
	GnomeIconContainer *container;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (widget));
	g_return_if_fail (context != NULL);

	container = GNOME_ICON_CONTAINER (widget);

	switch (info) {
	case GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST:
		set_gnome_icon_list_selection (container, selection_data);
		break;
	case GNOME_ICON_CONTAINER_DND_URI_LIST:
		set_uri_list_selection (container, selection_data);
		break;
	default:
		g_assert_not_reached ();
	}
}


/* Target-side handling of the drag.  */

static void
get_gnome_icon_list_selection (GnomeIconContainer *container,
			       GtkSelectionData *data)
{
	GnomeIconContainerDndInfo *dnd_info;
	const guchar *p, *oldp;
	gint size;

	dnd_info = container->details->dnd_info;

	oldp = data->data;
	size = data->length;

	while (1) {
		DndSelectionItem *item;
		guint len;

		/* The list is in the form:

		   name\rx:y:width:height\r\n

		   The geometry information after the first \r is optional.  */

		/* 1: Decode name. */

		p = memchr (oldp, '\r', size);
		if (p == NULL)
			break;

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
		if (!item->got_icon_position)
			g_warning ("Invalid special/x-gnome-icon-list data received: "
				   "invalid icon position specification.");

		dnd_info->selection_list
			= g_list_prepend (dnd_info->selection_list, item);

		p = memchr (p, '\r', size);
		if (p == NULL || p[1] != '\n') {
			g_warning ("Invalid special/x-gnome-icon-list data received: "
				   "missing newline character.");
			if (p == NULL)
				break;
		} else {
			p += 2;
		}

		size -= p - oldp;
		oldp = p;
	}
}

static void
gnome_icon_container_position_shadow (GnomeIconContainer *container,
				      int x, int y)
{
	GnomeCanvasItem *shadow;
	double world_x, world_y;

	shadow = container->details->dnd_info->shadow;
	if (shadow == NULL)
		return;

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);
	gnome_canvas_item_show (shadow);
	set_shadow_position (shadow, world_x, world_y);
}

static void
gnome_icon_container_dropped_icon_feedback (GtkWidget *widget,
					    GtkSelectionData *data,
					    int x, int y)
{
	GnomeIconContainer *container;
	GnomeIconContainerDndInfo *dnd_info;
	GnomeCanvasItem *shadow;
	double world_x, world_y;

	container = GNOME_ICON_CONTAINER (widget);
	dnd_info = container->details->dnd_info;

	/* delete old selection list if any */

	if (dnd_info->selection_list != NULL) {
		destroy_selection_list (dnd_info->selection_list);
		dnd_info->selection_list = NULL;
	}

	/* build the selection list from the drag data */

	get_gnome_icon_list_selection (container, data);		

	shadow = create_selection_shadow (container, dnd_info->selection_list);

	gnome_canvas_item_set (shadow, "x", (gdouble) 0, "y", (gdouble) 0,
			       NULL);

	gnome_canvas_window_to_world (GNOME_CANVAS (widget),
				      x, y, &world_x, &world_y);
	set_shadow_position (shadow, world_x, world_y);

	gnome_canvas_item_show (shadow);

	if (dnd_info->shadow != NULL)
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
	dnd_info->shadow = shadow;
}

static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *data,
		       guint info,
		       guint32 time,
		       gpointer user_data)
{
    	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = GNOME_ICON_CONTAINER (widget)->details->dnd_info;

	dnd_info->got_data_type = TRUE;
	dnd_info->data_type = info;

	switch (info) {
	case GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST:
		gnome_icon_container_dropped_icon_feedback (widget, data, x, y);
		break;
	case GNOME_ICON_CONTAINER_DND_COLOR:
		/* Save the data so we can do the actual work on drop. */
		dnd_info->selection_data = nautilus_gtk_selection_data_copy_deep (data);
		break;
	default:
		g_warning ("drag_data_received_cb received data but could not handle it");
	}
}

static void
gnome_icon_container_ensure_drag_data (GnomeIconContainer *container,
				       GdkDragContext *context,
				       guint32 time)
{
	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = container->details->dnd_info;

	if (!dnd_info->got_data_type)
		gtk_drag_get_data (GTK_WIDGET (container), context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);
}

static gboolean
drag_motion_cb (GtkWidget *widget,
		GdkDragContext *context,
		int x, int y,
		guint32 time)
{
	gnome_icon_container_ensure_drag_data (GNOME_ICON_CONTAINER (widget), context, time);
	gnome_icon_container_position_shadow (GNOME_ICON_CONTAINER (widget), x, y);

	gdk_drag_status (context, context->suggested_action, time);

	return TRUE;
}

static void
drag_end_cb (GtkWidget *widget,
	     GdkDragContext *context,
	     gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerDndInfo *dnd_info;

	container = GNOME_ICON_CONTAINER (widget);
	dnd_info = container->details->dnd_info;

	destroy_selection_list (dnd_info->selection_list);
	dnd_info->selection_list = NULL;
}

/* Utility routine to extract the directory from an item_uri
   (which may have geometry info attached).
*/

static void
gnome_icon_container_receive_dropped_icons (GnomeIconContainer *container,
					    GdkDragContext *context,
					    int x, int y)
{
	GnomeIconContainerDndInfo *dnd_info;
	GList *p;

	dnd_info = container->details->dnd_info;
	if (dnd_info->selection_list == NULL)
		return;

	/* Move files in same window.

	   FIXME: This won't work between windows for two reasons.

	   First, we have to figure out whether we need to actually
	   move or copy some files. For directory views this is based
	   on the URIs, but that should be done by the controller, not
	   here in the view.
	   
	   Second, the start_x and start_y values are only good for
	   this window. To make dragging between multiple windows work,
	   we need to communicate the offset between the mouse and the
	   corner of the selection along with the other selection info
	   so we can position the icons correctly in the window they
	   are dropped in. The geometry that's currently included along
	   with the selection is not sufficient.
	*/
	if (context->action == GDK_ACTION_MOVE) {
		double world_x, world_y;
		GList *icons_to_select;

		gnome_canvas_window_to_world (GNOME_CANVAS (container),
					      x, y, &world_x, &world_y);

		icons_to_select = NULL;
		for (p = dnd_info->selection_list; p != NULL; p = p->next) {
			DndSelectionItem *item;
			GnomeIconContainerIcon *icon;

			item = p->data;
			icon = gnome_icon_container_get_icon_by_uri
				(container, item->uri);

			if (icon == NULL) {
				g_warning ("drag between containers not implemented yet");
				continue;
			}

			if (item->got_icon_position) {
				int icon_x, icon_y;
                                double scale = icon->item->canvas->pixels_per_unit;
                                int center_offset = nautilus_icons_view_icon_item_center_offset(icon->item);

				icon_x = (int) world_x + item->icon_x - (center_offset / scale);
				icon_y = (int) world_y + item->icon_y;
				
				gnome_icon_container_move_icon
					(container, icon, icon_x, icon_y, TRUE);
			}

			icons_to_select = g_list_prepend (icons_to_select, icon);
		}

		gnome_icon_container_select_list_unselect_others (container, icons_to_select);

		g_list_free (icons_to_select);
	}
	
	destroy_selection_list (dnd_info->selection_list);
	dnd_info->selection_list = NULL;
}

static void
gnome_icon_container_free_drag_data (GnomeIconContainer *container)
{
	GnomeIconContainerDndInfo *dnd_info;
	
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

static gboolean
drag_drop_cb (GtkWidget *widget,
	      GdkDragContext *context,
	      gint x,
	      gint y,
	      guint32 time,
	      gpointer data)
{
	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = GNOME_ICON_CONTAINER (widget)->details->dnd_info;

	gnome_icon_container_ensure_drag_data (GNOME_ICON_CONTAINER (widget), context, time);

	g_assert (dnd_info->got_data_type);
	switch (dnd_info->data_type) {
	case GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST:
		gnome_icon_container_receive_dropped_icons
			(GNOME_ICON_CONTAINER (widget),
			 context, x, y);
		break;
	case GNOME_ICON_CONTAINER_DND_COLOR:
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (widget),
			 widget, x, y, dnd_info->selection_data);
		gtk_drag_finish (context, TRUE, FALSE, time);
		break;
	default:
		g_warning ("drag_drop_cb received data but could not handle it");
	}

	gnome_icon_container_free_drag_data (GNOME_ICON_CONTAINER (widget));

	return FALSE;
}

static void
drag_leave_cb (GtkWidget *widget,
	       GdkDragContext *context,
	       guint32 time,
	       gpointer data)
{
	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = GNOME_ICON_CONTAINER (widget)->details->dnd_info;

	if (dnd_info->shadow != NULL)
		gnome_canvas_item_hide (dnd_info->shadow);
}

void
gnome_icon_container_dnd_init (GnomeIconContainer *container,
			       GdkBitmap *stipple)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = g_new0 (GnomeIconContainerDndInfo, 1);

	dnd_info->target_list = gtk_target_list_new (drag_types,
						     num_drag_types);

	dnd_info->stipple = gdk_bitmap_ref (stipple);


	/* Set up the widget as a drag destination.  */
	/* (But not a source, as drags starting from this widget will be
           implemented by dealing with events manually.)  */

	gtk_drag_dest_set  (GTK_WIDGET (container),
			    0,
			    drop_types, num_drop_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE);

	/* Messages for outgoing drag. */

	gtk_signal_connect (GTK_OBJECT (container), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_end",
			    GTK_SIGNAL_FUNC (drag_end_cb), NULL);

	/* Messages for incoming drag. */

	gtk_signal_connect (GTK_OBJECT (container), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_motion",
			    GTK_SIGNAL_FUNC (drag_motion_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_drop",
			    GTK_SIGNAL_FUNC (drag_drop_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_leave",
			    GTK_SIGNAL_FUNC (drag_leave_cb), NULL);

	container->details->dnd_info = dnd_info;
}

void
gnome_icon_container_dnd_fini (GnomeIconContainer *container)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);

	gtk_target_list_unref (dnd_info->target_list);
	destroy_selection_list (dnd_info->selection_list);

	if (dnd_info->shadow != NULL)
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));

	gdk_bitmap_unref (dnd_info->stipple);

	g_free (dnd_info);
}

void
gnome_icon_container_dnd_begin_drag (GnomeIconContainer *container,
				     GdkDragAction actions,
				     gint button,
				     GdkEventMotion *event)
{
	GtkArg pixbuf_args[1];
	GnomeIconContainerDndInfo *dnd_info;
	GdkDragContext *context;
	GnomeCanvasItem *pixbuf_item;
	GnomeCanvas *canvas;
	GdkPixbuf *temp_pixbuf, *scaled_pixbuf;
	GdkPixmap *pixmap_for_dragged_file;
	GdkBitmap *mask_for_dragged_file;
	gint x_offset, y_offset, center_offset;
	
	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));
	g_return_if_fail (event != NULL);

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);
	
	/* Notice that the event is already in world coordinates, because of
           the way the canvas handles events! */
	dnd_info->start_x = event->x;
	dnd_info->start_y = event->y;
	
	/* start the drag */
	
	context = gtk_drag_begin (GTK_WIDGET (container),
				  dnd_info->target_list,
				  actions,
				  button,
				  (GdkEvent *) event);
	
	
	  
        /* create a pixmap and mask to drag with */
        pixbuf_item = GNOME_CANVAS_ITEM (container->details->drag_icon->item);
        pixbuf_args[0].name = "NautilusIconsViewIconItem::pixbuf";
        gtk_object_getv (GTK_OBJECT (pixbuf_item), 1, pixbuf_args);
        temp_pixbuf = (GdkPixbuf *) GTK_VALUE_OBJECT (pixbuf_args[0]);
        
        /* compute the image's offset */
	canvas = GNOME_CANVAS(container);

        x_offset = floor(event->x -  pixbuf_item->x1 + .5);
        y_offset = floor(event->y -  pixbuf_item->y1 + .5);
        
        center_offset = nautilus_icons_view_icon_item_center_offset(container->details->drag_icon->item);
        x_offset -= center_offset;
        
        /* if the scale factor isn't 1.0, we have to scale the pixmap */
	/* FIXME: eventually need to get the size, if any, from the metadata here */
	
	scaled_pixbuf = NULL;
	if (container->details->zoom_level != NAUTILUS_ZOOM_LEVEL_STANDARD)
	  {
	    gint old_width, old_height;
	    gint new_width, new_height;
	   
            x_offset = floor(event->x * canvas->pixels_per_unit - center_offset - pixbuf_item->x1 + .5);
            y_offset = floor(event->y * canvas->pixels_per_unit -  pixbuf_item->y1 + .5);
	    	    
	    old_width = gdk_pixbuf_get_width (temp_pixbuf);
	    old_height = gdk_pixbuf_get_height (temp_pixbuf);

	    new_width =  floor((old_width * canvas->pixels_per_unit) + .5);
	    new_height = floor((old_height * canvas->pixels_per_unit) + .5);
	    
	    scaled_pixbuf = gdk_pixbuf_scale_simple (temp_pixbuf, new_width, new_height, ART_FILTER_NEAREST);	
	    temp_pixbuf = scaled_pixbuf;
	  }

        gdk_pixbuf_render_pixmap_and_mask (temp_pixbuf, &pixmap_for_dragged_file, &mask_for_dragged_file, 128);
	if (scaled_pixbuf)
	  gdk_pixbuf_unref(scaled_pixbuf);
		
        /* set the pixmap and mask for dragging */
        gtk_drag_set_icon_pixmap (context, gtk_widget_get_colormap (GTK_WIDGET (container)),
				  pixmap_for_dragged_file, mask_for_dragged_file,
				  x_offset, y_offset);
}

void
gnome_icon_container_dnd_end_drag (GnomeIconContainer *container)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = container->details->dnd_info;
	g_return_if_fail (dnd_info != NULL);
}
