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
#include <gtk/gtksignal.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-gnome-extensions.h"
#include "nautilus-background.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include "nautilus-icon-private.h"

typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} DndSelectionItem;

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL }
};

static GtkTargetEntry drop_types [] = {
	{ NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL },
	{ NAUTILUS_ICON_DND_COLOR_TYPE, 0, NAUTILUS_ICON_DND_COLOR }
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

	if (list == NULL)
	    return NULL;

	/* if we're only dragging a single item, don't worry about the shadow */
	if (list->next == NULL)
		return NULL;
		
	stipple = container->details->dnd_info->stipple;
	g_return_val_if_fail (stipple != NULL, NULL);

	canvas = GNOME_CANVAS (container);

	/* Creating a big set of rectangles in the canvas can be expensive, so
           we try to be smart and only create the maximum number of rectangles
           that we will need, in the vertical/horizontal directions.  */

	/* FIXME: Does this work properly if the window is scrolled? */
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

		if (!item->got_icon_position)
			continue;

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

/* Encode a "special/x-nautilus-icon-list" selection.
   Along with the URIs of the dragged files, this encodes
   the location and size of each icon relative to the cursor.
*/
static void
set_nautilus_icon_list_selection (NautilusIconContainer *container,
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
	case NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST:
		set_nautilus_icon_list_selection (container, selection_data);
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
get_nautilus_icon_list_selection (NautilusIconContainer *container,
				  GtkSelectionData *data)
{
	NautilusIconDndInfo *dnd_info;
	const guchar *p, *oldp;
	int size;

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
				g_warning ("Invalid special/x-nautilus-icon-list data received: "
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
			g_warning ("Invalid special/x-nautilus-icon-list data received: "
				   "invalid icon position specification.");

		dnd_info->selection_list
			= g_list_prepend (dnd_info->selection_list, item);

		p = memchr (p, '\r', size);
		if (p == NULL || p[1] != '\n') {
			g_warning ("Invalid special/x-nautilus-icon-list data received: "
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
nautilus_icon_container_position_shadow (NautilusIconContainer *container,
					 int x, int y)
{
	GnomeCanvasItem *shadow;
	double world_x, world_y;

	shadow = container->details->dnd_info->shadow;
	if (shadow == NULL)
		return;

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
	if (dnd_info->shadow != NULL)
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));

	/* Build the selection list and the shadow. */
	get_nautilus_icon_list_selection (container, data);
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
	case NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST:
		nautilus_icon_container_dropped_icon_feedback (widget, data, x, y);
		break;
	case NAUTILUS_ICON_DND_COLOR:
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

	if (!dnd_info->got_data_type)
		gtk_drag_get_data (GTK_WIDGET (container), context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);
}

static gboolean
drag_motion_callback (GtkWidget *widget,
		      GdkDragContext *context,
		      int x, int y,
		      guint32 time)
{
	nautilus_icon_container_ensure_drag_data (NAUTILUS_ICON_CONTAINER (widget), context, time);
	nautilus_icon_container_position_shadow (NAUTILUS_ICON_CONTAINER (widget), x, y);

	gdk_drag_status (context, context->suggested_action, time);

	return TRUE;
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

/* Utility routine to extract the directory from an item_uri
   (which may have geometry info attached).
*/

static void
nautilus_icon_container_receive_dropped_icons (NautilusIconContainer *container,
					       GdkDragContext *context,
					       int x, int y)
{
	NautilusIconDndInfo *dnd_info;
	GList *p;

	dnd_info = container->details->dnd_info;
	if (dnd_info->selection_list == NULL)
		return;

	/* Move files. */
	if (context->action != GDK_ACTION_MOVE) {
		/* FIXME: We want to copy files here, I think. */
		g_warning ("non-move action not implemented yet");
	} else {
		GList *icons_to_select;
		
		icons_to_select = NULL;
		for (p = dnd_info->selection_list; p != NULL; p = p->next) {
			DndSelectionItem *item;
			NautilusIcon *icon;

			item = p->data;
			icon = nautilus_icon_container_get_icon_by_uri
				(container, item->uri);
			
			if (icon == NULL) {
				/* FIXME: Do we ever get a MOVE between windows?
				 * If so, we need to move files here.
				 */
				g_warning ("drag between containers not implemented yet");
				continue;
			}
			
			if (item->got_icon_position) {
				double world_x, world_y;

				gnome_canvas_window_to_world (GNOME_CANVAS (container),
							      x + item->icon_x,
							      y + item->icon_y,
							      &world_x, &world_y);
				nautilus_icon_container_move_icon
					(container, icon,
					 world_x, world_y,
					 icon->scale_x, icon->scale_y,
					 TRUE);   

			}
			
			icons_to_select = g_list_prepend (icons_to_select, icon);
		}
		
		nautilus_icon_container_select_list_unselect_others (container, icons_to_select);

		g_list_free (icons_to_select);
	}
	
	destroy_selection_list (dnd_info->selection_list);
	dnd_info->selection_list = NULL;
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
	case NAUTILUS_ICON_DND_NAUTILUS_ICON_LIST:
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
	default:
		gtk_drag_finish (context, FALSE, FALSE, time);
	}

	nautilus_icon_container_free_drag_data (NAUTILUS_ICON_CONTAINER (widget));

	return FALSE;
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


	/* Set up the widget as a drag destination.  */
	/* (But not a source, as drags starting from this widget will be
           implemented by dealing with events manually.)  */

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

/* this routine takes the source pixbuf and returns a new one that's semi-transparent, by
   clearing every other pixel's alpha value in a checkerboard grip.  We have to do the
   checkerboard instead of reducing the alpha since it will be turned into an alpha-less
   gdkpixmap and mask for the actual dragging */

/* FIXME: this should probably be in a graphics effects library instead of here */

static GdkPixbuf * 
make_semi_transparent(GdkPixbuf *source_pixbuf)
{
	gint i, j, temp_alpha;
	gint width, height, has_alpha, src_rowstride, dst_rowstride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	guchar alpha_value;
	GdkPixbuf *dest_pixbuf;
	guchar start_alpha_value;
	
	has_alpha = gdk_pixbuf_get_has_alpha (source_pixbuf);
	width = gdk_pixbuf_get_width (source_pixbuf);
	height = gdk_pixbuf_get_height (source_pixbuf);
	src_rowstride = gdk_pixbuf_get_rowstride (source_pixbuf);
	
	/* allocate the destination pixbuf to be a clone of the source */

	dest_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_format (source_pixbuf),
				      TRUE,
				      gdk_pixbuf_get_bits_per_sample (source_pixbuf),
				      width,
				      height);
	dst_rowstride = gdk_pixbuf_get_rowstride (dest_pixbuf);
	
	/* set up pointers to the actual pixels */
	target_pixels = gdk_pixbuf_get_pixels (dest_pixbuf);
	original_pixels = gdk_pixbuf_get_pixels (source_pixbuf);

	/* loop through the pixels to do the actual work, copying from the source to the destination */
	
	start_alpha_value = ~0;
	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_rowstride;
		pixsrc = original_pixels + i * src_rowstride;
		alpha_value = start_alpha_value;
		for (j = 0; j < width; j++) {
			*pixdest++ = *pixsrc++; /* red */
			*pixdest++ = *pixsrc++; /* green */
			*pixdest++ = *pixsrc++; /* blue */
			
			if (has_alpha) {
				temp_alpha = *pixsrc++;
			} else {
				temp_alpha = ~0;
			}
			*pixdest++ = temp_alpha & alpha_value;
			
			alpha_value = ~alpha_value;
		}
		
		start_alpha_value = ~start_alpha_value;
	}
	
	return dest_pixbuf;
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
	  
        /* create a pixmap and mask to drag with */
        pixbuf = nautilus_icon_canvas_item_get_image (container->details->drag_icon->item, NULL);
        
	/* unfortunately, X is very slow when using a stippled mask, so only use the stipple
	   for relatively small pixbufs.  Eventually, we may have to remove this entirely
	   for UI consistency reasons */
	
	if ((gdk_pixbuf_get_width(pixbuf) * gdk_pixbuf_get_height(pixbuf)) < 4096) {
		transparent_pixbuf = make_semi_transparent (pixbuf);
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
