/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

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

#ifndef _GNOME_ICON_CONTAINER_PRIVATE_H
#define _GNOME_ICON_CONTAINER_PRIVATE_H

#include "gnome-icon-container.h"
#include "gnome-icon-container-dnd.h"

/* An Icon.  */

struct _GnomeIconContainerIcon {
	/* Group containing the text and the image.  */
	GnomeCanvasGroup *item;	/* FIXME wrong name. */

	/* The image for the icon.  Using a generic item makes it
           possible for us to use any fancy canvas element.  */
	GnomeCanvasItem *image_item;

	/* The text for the icon.  */
	GnomeIconTextItem *text_item;

	/* Text for the icon.  */
	gchar *text;

	/* X/Y coordinates and size.  We could use the GnomeCanvasItem
           functions, but this is a lot faster.  */
	gdouble x, y;
	guint width, height;	/* FIXME we could actually do without this if
                                   we assume the size is always given by
                                   GnomeIconContainer.cell_width*/

	/* Whether this item is selected (i.e. highlighted) for operation.  */
	gboolean is_selected : 1;

	/* Whether this item is selected for keyboard navigation.  */
	gboolean is_current : 1;

	/* Whether this item has been repositioned during layout already.  */
	gboolean layout_done : 1;

	/* Whether this item was selected before rubberbanding.  */
	gboolean was_selected_before_rubberband : 1;

	gpointer data;
};
typedef struct _GnomeIconContainerIcon GnomeIconContainerIcon;


#define INITIAL_GRID_WIDTH 64
#define INITIAL_GRID_HEIGHT 64
struct _GnomeIconContainerIconGrid {
	/* Size of the grid.  */
	guint width, height;

	/* This is the width that we can actually use for finding an empty
           position.  */
	guint visible_width;

	/* Array of grid elements.  */
	GList **elems;

	/* Size of the allocated array.  */
	guint alloc_width, alloc_height;

	/* Position of the first free cell (used to speed up progressive
	   updates).  If negative, there is no free cell.  */
	gint first_free_x, first_free_y;
};
typedef struct _GnomeIconContainerIconGrid GnomeIconContainerIconGrid;


/* Private GnomeIconContainer members.  */

struct _GnomeIconContainerRubberbandInfo {
	gboolean active : 1;

	gdouble start_x, start_y;

	GnomeCanvasItem *selection_rectangle;
	guint timer_tag;

	guint prev_x, prev_y;
	guint prev_x1, prev_y1;
	guint prev_x2, prev_y2;
};
typedef struct _GnomeIconContainerRubberbandInfo GnomeIconContainerRubberbandInfo;

struct _GnomeIconContainerPrivate {
	/* Base URI for Drag & Drop.  */
	gchar *base_uri;

	/* Browser mode setting.  */
	gboolean browser_mode : 1;
	/* single-click mode setting */
	gboolean single_click_mode : 1;
	
	/* Current icon mode (index into `icon_mode_info[]' -- see
           `gnome-icon-container.c').  */
	GnomeIconContainerIconMode icon_mode;

	/* Size of the container.  */
	guint width, height;

	/* List of icons.  */
	GList *icons;

	/* Total number of icons.  */
	guint num_icons;

	/* The grid.  */
	GnomeIconContainerIconGrid *grid;

	/* FIXME: This is *ugly*, but more efficient (both memory- and
           speed-wise) than using gtk_object_{set,get}_data() for all the
           icon items.  */
	GHashTable *canvas_item_to_icon;

	/* Rectangle that shows that a certain icon is selected.  */
	GnomeCanvasItem *kbd_navigation_rectangle;

	/* Current icon for keyboard navigation.  */
	GnomeIconContainerIcon *kbd_current;

	/* Rubberbanding status.  */
	GnomeIconContainerRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
           period of time.  (The timeout is needed to make sure
           double-clicking still works.)  */
	gint kbd_icon_visibility_timer_tag;

	/* Position of the pointer during the last click.  */
	gint drag_x, drag_y;

	/* Button currently pressed, possibly for dragging.  */
	guint drag_button;

	/* Icon on which the click happened.  */
	GnomeIconContainerIcon *drag_icon;

	/* Whether we are actually performing a dragging action.  */
	gboolean doing_drag;

	/* Drag offset.  */
	gint drag_x_offset, drag_y_offset;

	/* Idle ID.  */
	guint idle_id;

	/* Timeout for selection in browser mode.  */
	gint browser_mode_selection_timer_tag;

	/* Icon to be selected at timeout in browser mode.  */
	GnomeIconContainerIcon *browser_mode_selection_icon;

	/* DnD info.  */
	GnomeIconContainerDndInfo *dnd_info;
};


/* Definition of the available icon container modes.  */
struct _GnomeIconContainerIconModeInfo {
	guint icon_width;
	guint icon_height;

	guint cell_width;
	guint cell_height;

	guint cell_spacing;

	guint icon_xoffset;
	guint icon_yoffset;
};
typedef struct _GnomeIconContainerIconModeInfo GnomeIconContainerIconModeInfo;

extern GnomeIconContainerIconModeInfo gnome_icon_container_icon_mode_info[];

#define GNOME_ICON_CONTAINER_ICON_WIDTH(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].icon_width

#define GNOME_ICON_CONTAINER_ICON_HEIGHT(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].icon_height

#define GNOME_ICON_CONTAINER_CELL_WIDTH(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].cell_width

#define GNOME_ICON_CONTAINER_CELL_HEIGHT(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].cell_height

#define GNOME_ICON_CONTAINER_CELL_SPACING(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].cell_spacing

#define GNOME_ICON_CONTAINER_ICON_XOFFSET(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].icon_xoffset

#define GNOME_ICON_CONTAINER_ICON_YOFFSET(container) \
	gnome_icon_container_icon_mode_info[container->priv->icon_mode].icon_yoffset

#endif /*  _GNOME_ICON_CONTAINER_PRIVATE_H */
