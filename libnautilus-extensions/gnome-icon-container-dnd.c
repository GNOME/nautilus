/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-dnd.c - Drag & drop handling for the icon container
   widget.

   Copyright (C) 1999, 2000 Free Software Foundation

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

#include <glib.h>
#include <gtk/gtk.h>

#include "gnome-icon-container-private.h"

#include "gnome-icon-container-dnd.h"


struct _DndSelectionItem {
	gchar *uri;
	gint x, y;
};
typedef struct _DndSelectionItem DndSelectionItem;

static GtkTargetEntry drag_types [] = {
	{ GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST },
	{ GNOME_ICON_CONTAINER_DND_URI_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_URI_LIST },
	{ GNOME_ICON_CONTAINER_DND_URL_TYPE, 0, GNOME_ICON_CONTAINER_DND_URL }
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetEntry drop_types [] = {
	{ GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST },
	{ GNOME_ICON_CONTAINER_DND_URI_LIST_TYPE, 0, GNOME_ICON_CONTAINER_DND_URI_LIST },
	{ GNOME_ICON_CONTAINER_DND_URL_TYPE, 0, GNOME_ICON_CONTAINER_DND_URL }
};
static const int num_drop_types = sizeof (drop_types) / sizeof (drop_types[0]);

static GnomeCanvasItem *
create_selection_shadow (GnomeIconContainer *container,
			 GList *list)
{
	GnomeCanvasGroup *group;
	GnomeCanvas *canvas;
	GdkBitmap *stipple;
	gint max_x, max_y;
	gint min_x, min_y;
	gint icon_width, icon_height;
	gint cell_width, cell_height;
	GList *p;

	if (list == NULL)
		return NULL;

	stipple = container->priv->dnd_info->stipple;
	g_return_val_if_fail (stipple != NULL, NULL);

	icon_width = GNOME_ICON_CONTAINER_ICON_WIDTH (container);
	icon_height = GNOME_ICON_CONTAINER_ICON_HEIGHT (container);
	cell_width = GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	cell_height = GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

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
		gint x1, y1;
		gint x2, y2;

		item = p->data;

		x1 = item->x;
		y1 = item->y;
		x2 = item->x + icon_width;
		y2 = item->y + icon_height;

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

	new = g_new (DndSelectionItem, 1);
	new->uri = NULL;
	new->x = 0;
	new->y = 0;
	
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
		dnd_selection_item_destroy ((DndSelectionItem *) p->data);

	g_list_free (list);
}


/* Source-side handling of the drag.  */

/* Encode a "special/x-gnome-icon-list" selection.  */
static void
set_gnome_icon_list_selection (GnomeIconContainer *container,
			       GtkSelectionData *selection_data)
{
	GnomeIconContainerPrivate *priv;
	GList *p;
	GString *data;
	gdouble x_offset, y_offset;

	priv = container->priv;
	if (priv->icons == NULL) {
		/* FIXME?  Actually this probably shouldn't happen.  */
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, NULL, 0);
		return;
	}

	x_offset = container->priv->dnd_info->start_x;
	y_offset = container->priv->dnd_info->start_y;

	data = g_string_new (NULL);
	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		gchar *s;
		gint x, y;

		icon = p->data;
		if (! icon->is_selected)
			continue;

		x = (gint) (icon->x - x_offset);
		y = (gint) (icon->y - y_offset);

		x += (GNOME_ICON_CONTAINER_ICON_XOFFSET (container)
		      - GNOME_ICON_CONTAINER_ICON_WIDTH (container) / 2);
		y += (GNOME_ICON_CONTAINER_ICON_YOFFSET (container)
		      - GNOME_ICON_CONTAINER_ICON_HEIGHT (container) / 2);

		if (priv->base_uri != NULL)
			s = g_strdup_printf ("%s%s\r%d:%d\r\n",
					     priv->base_uri, icon->text,
					     x, y);
		else
			s = g_strdup_printf ("%s\r%d:%d\r\n",
					     icon->text, x, y);

		g_string_append (data, s);
		g_free (s);
	}

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, (guchar *) data->str, data->len);

	g_string_free (data, TRUE);
}

/* Encode a "text/uri-list" selection.  */
static void
set_uri_list_selection (GnomeIconContainer *container,
			GtkSelectionData *selection_data)
{
	GnomeIconContainerPrivate *priv;
	GList *p;
	GString *data;

	priv = container->priv;
	if (priv->icons == NULL) {
		/* FIXME?  Actually this probably shouldn't happen.  */
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, NULL, 0);
		return;
	}

	data = g_string_new (NULL);

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (! icon->is_selected)
			continue;

		/* This is lame code, I know.  */

		if (priv->base_uri != NULL)
			g_string_append (data, priv->base_uri);
		g_string_append (data, icon->text);
		g_string_append (data, "\r\n");
	}

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, (guchar *) data->str, data->len);

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

	dnd_info = container->priv->dnd_info;

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

		if (sscanf (p, "%d:%d", &item->x, &item->y) != 2)
			g_warning ("Invalid special/x-gnome-icon-list data received: "
				   "invalid geometry specification.");

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
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *data,
		       guint info,
		       guint32 time,
		       gpointer user_data)
{
	GnomeIconContainer *container;
	GnomeIconContainerDndInfo *dnd_info;
	GnomeCanvasItem *shadow;
	double world_x, world_y;

	container = GNOME_ICON_CONTAINER (widget);
	dnd_info = container->priv->dnd_info;
	g_return_if_fail (dnd_info->selection_list == NULL);

	switch (info) {
	case GNOME_ICON_CONTAINER_DND_GNOME_ICON_LIST:
		get_gnome_icon_list_selection (container, data);
		break;
	case GNOME_ICON_CONTAINER_DND_URI_LIST:
		puts ("Bad!  URI list!"); /* FIXME */
		return;
	}

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

static gboolean
drag_motion_cb (GtkWidget *widget,
		GdkDragContext *context,
		gint x,
		gint y,
		guint time)
{
	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = GNOME_ICON_CONTAINER (widget)->priv->dnd_info;
	if (dnd_info->selection_list == NULL)
		gtk_drag_get_data (widget, context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);

	if (dnd_info->shadow != NULL) {
		double world_x, world_y;

		gnome_canvas_window_to_world (GNOME_CANVAS (widget),
					      x, y, &world_x, &world_y);
		gnome_canvas_item_show (dnd_info->shadow);
		set_shadow_position (dnd_info->shadow, world_x, world_y);
	}

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
	dnd_info = container->priv->dnd_info;

	destroy_selection_list (dnd_info->selection_list);
	dnd_info->selection_list = NULL;
}

static gboolean
drag_drop_cb (GtkWidget *widget,
	      GdkDragContext *context,
	      gint x,
	      gint y,
	      guint time,
	      gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerDndInfo *dnd_info;
	GtkWidget *source_widget;

	container = GNOME_ICON_CONTAINER (widget);
	dnd_info = container->priv->dnd_info;
	source_widget = gtk_drag_get_source_widget (context);

	if (source_widget == widget && context->action == GDK_ACTION_MOVE) {
		double world_x, world_y;

		gnome_canvas_window_to_world (GNOME_CANVAS (container),
					      x, y, &world_x, &world_y);

		gnome_icon_container_xlate_selected (container,
						     world_x - dnd_info->start_x,
						     world_y - dnd_info->start_y,
						     TRUE);
	}

	return FALSE;
}

static void
drag_leave_cb (GtkWidget *widget,
	       GdkDragContext *context,
	       guint time,
	       gpointer data)
{
	GnomeIconContainerDndInfo *dnd_info;

	dnd_info = GNOME_ICON_CONTAINER (widget)->priv->dnd_info;

	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
		dnd_info->shadow = NULL;
	}

	if (dnd_info->selection_list != NULL) {
		destroy_selection_list (dnd_info->selection_list);
		dnd_info->selection_list = NULL;
	}
}


void
gnome_icon_container_dnd_init (GnomeIconContainer *container,
			       GdkBitmap *stipple)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = g_new (GnomeIconContainerDndInfo, 1);

	dnd_info->target_list = gtk_target_list_new (drag_types,
						     num_drag_types);

	dnd_info->start_x = 0;
	dnd_info->start_y = 0;
	dnd_info->selection_list = NULL;

	dnd_info->stipple = gdk_bitmap_ref (stipple);

	dnd_info->shadow = NULL;

	/* Set up the widget as a drag destination.  */
	/* (But not a source, as drags starting from this widget will be
           implemented by dealing with events manually.)  */

	gtk_drag_dest_set  (GTK_WIDGET (container),
			    0,
			    drop_types, num_drop_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE);

	gtk_signal_connect (GTK_OBJECT (container), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_motion",
			    GTK_SIGNAL_FUNC (drag_motion_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_end",
			    GTK_SIGNAL_FUNC (drag_end_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_drop",
			    GTK_SIGNAL_FUNC (drag_drop_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (container), "drag_leave",
			    GTK_SIGNAL_FUNC (drag_leave_cb), NULL);

	container->priv->dnd_info = dnd_info;
}

void
gnome_icon_container_dnd_fini (GnomeIconContainer *container)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = container->priv->dnd_info;
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
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));
	g_return_if_fail (event != NULL);

	dnd_info = container->priv->dnd_info;
	g_return_if_fail (dnd_info != NULL);

	/* Notice that the event is already in world coordinates, because of
           the way the canvas handles events!  */
	dnd_info->start_x = event->x;
	dnd_info->start_y = event->y;

	gtk_drag_begin (GTK_WIDGET (container),
			dnd_info->target_list,
			actions,
			button,
			(GdkEvent *) event);
}

void
gnome_icon_container_dnd_end_drag (GnomeIconContainer *container)
{
	GnomeIconContainerDndInfo *dnd_info;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	dnd_info = container->priv->dnd_info;
	g_return_if_fail (dnd_info != NULL);
}
