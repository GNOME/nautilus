/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Author: Dave Camp <dave@ximian.com>
 */

/* nautilus-tree-view-drag-dest.c: Handles drag and drop for treeviews which 
 *                                 contain a hierarchy of files
 */

#include <config.h>
#include "nautilus-tree-view-drag-dest.h"

#include <eel/eel-gtk-macros.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "nautilus-file-dnd.h"
#include "nautilus-icon-dnd.h"
#include "nautilus-link.h"
#include "nautilus-marshal.h"

#define AUTO_SCROLL_MARGIN 20

struct _NautilusTreeViewDragDestDetails {
	GtkTreeView *tree_view;

	gboolean drop_occurred;

	gboolean have_drag_data;
	guint drag_type;
	GtkSelectionData *drag_data;
	GList *drag_list;

	guint highlight_id;
	guint scroll_id;
};

enum {
	GET_ROOT_URI,
	GET_FILE_FOR_PATH,
	MOVE_COPY_ITEMS,
	LAST_SIGNAL
};

static void nautilus_tree_view_drag_dest_instance_init (NautilusTreeViewDragDest      *dest);
static void nautilus_tree_view_drag_dest_class_init    (NautilusTreeViewDragDestClass *class);

static guint signals[LAST_SIGNAL];

GNOME_CLASS_BOILERPLATE (NautilusTreeViewDragDest,
			 nautilus_tree_view_drag_dest,
			 GObject, G_TYPE_OBJECT);

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_URL_TYPE, 0, NAUTILUS_ICON_DND_URL }
	/* FIXME: Should handle emblems once the list view supports them */
};


static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
	GdkRectangle visible_rect;
	GtkAdjustment *vadjustment;
	GdkWindow *window;
	int y;
	int offset;
	float value;
	
	window = gtk_tree_view_get_bin_window (tree_view);
	vadjustment = gtk_tree_view_get_vadjustment (tree_view);
	
	gdk_window_get_pointer (window, NULL, &y, NULL);
	
	y += vadjustment->value;

	gtk_tree_view_get_visible_rect (tree_view, &visible_rect);
	
	offset = y - (visible_rect.y + 2 * AUTO_SCROLL_MARGIN);
	if (offset > 0) {
		offset = y - (visible_rect.y + visible_rect.height - 2 * AUTO_SCROLL_MARGIN);
		if (offset < 0) {
			return;
		}
	}

	value = CLAMP (vadjustment->value + offset, 0.0,
		       vadjustment->upper - vadjustment->page_size);
	gtk_adjustment_set_value (vadjustment, value);
}

static int
scroll_timeout (gpointer data)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (data);
	
	gtk_tree_view_vertical_autoscroll (tree_view);

	return TRUE;
}

static void
remove_scroll_timeout (NautilusTreeViewDragDest *dest)
{
	if (dest->details->scroll_id) {
		gtk_timeout_remove (dest->details->scroll_id);
		dest->details->scroll_id = 0;
	}
}

static gboolean
highlight_expose (GtkWidget *widget,
		  GdkEventExpose *event,
		  gpointer data)
{
	GdkWindow *bin_window;
	int width;
	int height;

	if (GTK_WIDGET_DRAWABLE (widget)) {
		bin_window = 
			gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
		
		gdk_drawable_get_size (bin_window, &width, &height);
		
		gtk_paint_focus (widget->style,
				 bin_window,
				 GTK_WIDGET_STATE (widget),
				 NULL,
				 widget,
				 "treeview-drop-indicator",
				 0, 0, width, height);
	}
	
	return FALSE;
}

static void
set_widget_highlight (NautilusTreeViewDragDest *dest, gboolean highlight)
{
	if (!highlight && dest->details->highlight_id) {
		g_signal_handler_disconnect (dest->details->tree_view,
					     dest->details->highlight_id);
		dest->details->highlight_id = 0;
	}
	
	if (highlight && !dest->details->highlight_id) {
		dest->details->highlight_id = 
			g_signal_connect_object (dest->details->tree_view,
						 "expose_event",
						 G_CALLBACK (highlight_expose),
						 dest,
						 G_CONNECT_AFTER);
	}
	gtk_widget_queue_draw (GTK_WIDGET (dest->details->tree_view));
}

static void
set_drag_dest_row (NautilusTreeViewDragDest *dest,
		   GtkTreePath *path)
{
	if (path) {
		set_widget_highlight (dest, FALSE);
		gtk_tree_view_set_drag_dest_row
			(dest->details->tree_view,
			 path,
			 GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	} else {
		set_widget_highlight (dest, TRUE);
		gtk_tree_view_set_drag_dest_row (dest->details->tree_view, 
						 NULL, 
						 0);
	}
}

static void
clear_drag_dest_row (NautilusTreeViewDragDest *dest)
{
	gtk_tree_view_set_drag_dest_row (dest->details->tree_view, NULL, 0);
	set_widget_highlight (dest, FALSE);
}

static void
get_drag_data (NautilusTreeViewDragDest *dest,
	       GdkDragContext *context, 
	       guint32 time)
{
	GdkAtom target;
	
	target = gtk_drag_dest_find_target (GTK_WIDGET (dest->details->tree_view), 
					    context, 
					    NULL);

	gtk_drag_get_data (GTK_WIDGET (dest->details->tree_view),
			   context, target, time);
}

static void
free_drag_data (NautilusTreeViewDragDest *dest)
{
	dest->details->have_drag_data = FALSE;

	if (dest->details->drag_data) {
		gtk_selection_data_free (dest->details->drag_data);
		dest->details->drag_data = NULL;
	}

	if (dest->details->drag_list) {
		nautilus_drag_destroy_selection_list (dest->details->drag_list);
		dest->details->drag_list = NULL;
	}
}

static char *
get_root_uri (NautilusTreeViewDragDest *dest)
{
	char *uri;
	
	g_signal_emit (dest, signals[GET_ROOT_URI], 0, &uri);
	
	return uri;
}

static NautilusFile *
file_for_path (NautilusTreeViewDragDest *dest, GtkTreePath *path)
{
	NautilusFile *file;
	char *uri;
	
	if (path) {
		g_signal_emit (dest, signals[GET_FILE_FOR_PATH], 0, path, &file);
	} else {
		uri = get_root_uri (dest);
		
		file = nautilus_file_get (uri);
		
		g_free (uri);
	}
	
	return file;
}

static GtkTreePath *
get_drop_path (NautilusTreeViewDragDest *dest,
	       GtkTreePath *path)
{
	NautilusFile *file;
	GtkTreePath *ret;
	
	if (!path) {
		return NULL;
	}

	file = file_for_path (dest, path);
	
	ret = NULL;

	if (!file || !nautilus_drag_can_accept_items (file, dest->details->drag_list)){
		if (gtk_tree_path_get_depth (path) == 1) {
			ret = NULL;
		} else {
			ret = gtk_tree_path_copy (path);
			gtk_tree_path_up (ret);
		}
	} else {
		ret = gtk_tree_path_copy (path);
	}

	nautilus_file_unref (file);
	
	return ret;
}

static char *
get_drop_target (NautilusTreeViewDragDest *dest, 
		 GtkTreePath *path)
{
	NautilusFile *file;
	char *target;

	file = file_for_path (dest, path);
	target = nautilus_file_get_drop_target_uri (file);
	nautilus_file_unref (file);
	
	return target;
}

static guint
get_drop_action (NautilusTreeViewDragDest *dest, 
		 GdkDragContext *context,
		 GtkTreePath *path)
{
	char *drop_target;
	guint action;
	
	if (!dest->details->have_drag_data || !dest->details->drag_list) {
		return 0;
	}

	switch (dest->details->drag_type) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST :
		drop_target = get_drop_target (dest, path);
		
		if (!drop_target) {
			return 0;
		}

		nautilus_drag_default_drop_action_for_icons
			(context,
			 drop_target,
			 dest->details->drag_list,
			 &action);

		g_free (drop_target);
		
		return action;
	case NAUTILUS_ICON_DND_URI_LIST :
	case NAUTILUS_ICON_DND_URL :
		return context->suggested_action;
	}

	return 0;
}

static gboolean
drag_motion_callback (GtkWidget *widget,
		      GdkDragContext *context,
		      int x,
		      int y,
		      guint32 time,
		      gpointer data)
{
	NautilusTreeViewDragDest *dest;
	GtkTreePath *path;
	GtkTreePath *drop_path;
	GtkTreeViewDropPosition pos;
	guint action;

	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

	gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
					   x, y, &path, &pos);
	

	if (!dest->details->have_drag_data) {
		get_drag_data (dest, context, time);
	}
	drop_path = get_drop_path (dest, path);
	
	action = get_drop_action (dest, context, drop_path);
	
	if (action) {
		set_drag_dest_row (dest, drop_path);
	} else {
		clear_drag_dest_row (dest);
	}
	
	if (path) {
		gtk_tree_path_free (path);
	}
	
	if (drop_path) {
		gtk_tree_path_free (drop_path);
	}
	
	if (dest->details->scroll_id == 0) {
		dest->details->scroll_id = 
			gtk_timeout_add (150, 
					 scroll_timeout, 
					 dest->details->tree_view);
	}

	gdk_drag_status (context, action, time);

	return TRUE;
}

static void
drag_leave_callback (GtkWidget *widget,
		     GdkDragContext *context,
		     guint32 time,
		     gpointer data)
{
	NautilusTreeViewDragDest *dest;

	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

	clear_drag_dest_row (dest);

	free_drag_data (dest);

	remove_scroll_timeout (dest);
}

static void
receive_uris (NautilusTreeViewDragDest *dest,
	      GdkDragContext *context,
	      GList *source_uris,
	      int x, int y)
{
	char *drop_target;
	GtkTreePath *path;
	GtkTreePath *drop_path;
	GtkTreeViewDropPosition pos;
	GdkDragAction action;

	gtk_tree_view_get_dest_row_at_pos (dest->details->tree_view, x, y, 
					   &path, &pos);

	drop_path = get_drop_path (dest, path);

	drop_target = get_drop_target (dest, drop_path);

	if (context->action == GDK_ACTION_ASK) {
		if (nautilus_drag_selection_includes_special_link (dest->details->drag_list)) {
			/* We only want to move the trash */
			action = GDK_ACTION_MOVE;
		} else {
			action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
		}
		context->action = nautilus_drag_drop_action_ask (action);
	}

	/* We only want to copy external uris */
	if (dest->details->drag_type == NAUTILUS_ICON_DND_URI_LIST) {
		action = GDK_ACTION_COPY;
	}

	if (context->action > 0) {
		g_signal_emit (dest, signals[MOVE_COPY_ITEMS], 0,
			       source_uris, 
			       drop_target,
			       context->action,
			       x, y);
	}

	if (path) {
		gtk_tree_path_free (path);
	}

	if (drop_path) {
		gtk_tree_path_free (drop_path);
	}

	g_free (drop_target);
}

static void
receive_dropped_icons (NautilusTreeViewDragDest *dest,
		       GdkDragContext *context,
		       int x, int y)
{
	GList *source_uris;
	GList *l;

	/* FIXME: ignore local only moves */

	if (!dest->details->drag_list) {
		return;
	}
	
	source_uris = NULL;
	for (l = dest->details->drag_list; l != NULL; l = l->next) {
		source_uris = g_list_prepend (source_uris,
					      ((NautilusDragSelectionItem *)l->data)->uri);
	}

	source_uris = g_list_reverse (source_uris);

	receive_uris (dest, context, source_uris, x, y);
	
	g_list_free (source_uris);
}

static void
receive_dropped_uri_list (NautilusTreeViewDragDest *dest,
			  GdkDragContext *context,
			  int x, int y)
{
	GList *source_uris;
	
	if (!dest->details->drag_data) {
		return;
	}

	source_uris = nautilus_icon_dnd_uri_list_extract_uris ((char*)dest->details->drag_data->data);
	
	receive_uris (dest, context, source_uris, x, y);
	
	nautilus_icon_dnd_uri_list_free_strings (source_uris);
}

static gboolean
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint32 time,
			     gpointer data)
{
	NautilusTreeViewDragDest *dest;
	gboolean success;
	
	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

	if (!dest->details->have_drag_data) {
		dest->details->have_drag_data = TRUE;
		dest->details->drag_type = info;
		dest->details->drag_data = 
			gtk_selection_data_copy (selection_data);
		if (info == NAUTILUS_ICON_DND_GNOME_ICON_LIST) {
			dest->details->drag_list = 
				nautilus_drag_build_selection_list (selection_data);
		}
	}

	if (dest->details->drop_occurred) {
		success = FALSE;
		switch (info) {
		case NAUTILUS_ICON_DND_GNOME_ICON_LIST :
			receive_dropped_icons (dest, context, x, y);
			success = TRUE;
			break;
		case NAUTILUS_ICON_DND_URI_LIST :
		case NAUTILUS_ICON_DND_URL :
			receive_dropped_uri_list (dest, context, x, y);
			success = TRUE;
			break;
		}

		dest->details->drop_occurred = FALSE;
		free_drag_data (dest);
		gtk_drag_finish (context, success, FALSE, time);
	}

	/* appease GtkTreeView by preventing its drag_data_receive
	 * from being called */
	g_signal_stop_emission_by_name (dest->details->tree_view,
					"drag_data_received");

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
	NautilusTreeViewDragDest *dest;

	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

	dest->details->drop_occurred = TRUE;

	get_drag_data (dest, context, time);
	remove_scroll_timeout (dest);
	clear_drag_dest_row (dest);
	
	return TRUE;
}

static void
tree_view_weak_notify (gpointer user_data,
		       GObject *object)
{
	NautilusTreeViewDragDest *dest;

	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (user_data);
	
	remove_scroll_timeout (dest);

	dest->details->tree_view = NULL;
}

static void
nautilus_tree_view_drag_dest_dispose (GObject *object)
{
	NautilusTreeViewDragDest *dest;
	
	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (object);

	if (dest->details->tree_view) {
		g_object_weak_unref (G_OBJECT (dest->details->tree_view),
				     tree_view_weak_notify,
				     dest);
	}
	
	remove_scroll_timeout (dest);

	EEL_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
nautilus_tree_view_drag_dest_finalize (GObject *object)
{
	NautilusTreeViewDragDest *dest;
	
	dest = NAUTILUS_TREE_VIEW_DRAG_DEST (object);

	free_drag_data (dest);

	g_free (dest->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_tree_view_drag_dest_instance_init (NautilusTreeViewDragDest *dest)
{
	dest->details = g_new0 (NautilusTreeViewDragDestDetails, 1);
}

static void
nautilus_tree_view_drag_dest_class_init (NautilusTreeViewDragDestClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	
	gobject_class->dispose = nautilus_tree_view_drag_dest_dispose;
	gobject_class->finalize = nautilus_tree_view_drag_dest_finalize;

	signals[GET_ROOT_URI] = 
		g_signal_new ("get_root_uri",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
					       get_root_uri),
			      NULL, NULL,
			      nautilus_marshal_STRING__VOID,
			      G_TYPE_STRING, 0);
	signals[GET_FILE_FOR_PATH] = 
		g_signal_new ("get_file_for_path",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
					       get_file_for_path),
			      NULL, NULL,
			      nautilus_marshal_OBJECT__BOXED,
			      NAUTILUS_TYPE_FILE, 1,
			      GTK_TYPE_TREE_PATH);
	signals[MOVE_COPY_ITEMS] =
		g_signal_new ("move_copy_items",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
					       move_copy_items),
			      NULL, NULL,
			      
			      nautilus_marshal_VOID__POINTER_STRING_UINT_INT_INT,
			      G_TYPE_NONE, 5,
			      G_TYPE_POINTER,
			      G_TYPE_STRING,
			      G_TYPE_UINT,
			      G_TYPE_INT,
			      G_TYPE_INT);
}



NautilusTreeViewDragDest *
nautilus_tree_view_drag_dest_new (GtkTreeView *tree_view)
{
	NautilusTreeViewDragDest *dest;
	
	dest = g_object_new (NAUTILUS_TYPE_TREE_VIEW_DRAG_DEST, NULL);

	dest->details->tree_view = tree_view;
	g_object_weak_ref (G_OBJECT (dest->details->tree_view),
			   tree_view_weak_notify, dest);
	
	gtk_drag_dest_set (GTK_WIDGET (tree_view),
			   0, drag_types, G_N_ELEMENTS (drag_types),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK);	
	
	g_signal_connect_object (tree_view,
				 "drag_motion",
				 G_CALLBACK (drag_motion_callback),
				 dest, 0);
	g_signal_connect_object (tree_view,
				 "drag_leave",
				 G_CALLBACK (drag_leave_callback),
				 dest, 0);
	g_signal_connect_object (tree_view,
				 "drag_drop",
				 G_CALLBACK (drag_drop_callback),
				 dest, 0);
	g_signal_connect_object (tree_view, 
				 "drag_data_received",
				 G_CALLBACK (drag_data_received_callback),
				 dest, 0);
	
	return dest;
}
