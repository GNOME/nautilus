/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container.c - Icon container widget.

   Copyright (C) 1999 Free Software Foundation

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

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "gnome-icon-container-private.h"
#include "gnome-icon-container-dnd.h"


static GnomeCanvasClass *parent_class;

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10

/* Timeout for making the icon currently selected for keyboard operation
   visible.  FIXME: This *must* be higher than the double-click time in GDK,
   but there is no way to access its value from outside.  */
#define KBD_ICON_VISIBILITY_TIMEOUT 300

/* Timeout for selecting an icon in "browser mode" (i.e. by just placing the
   pointer over the icon, without pressing any button).  */
#define BROWSER_MODE_SELECTION_TIMEOUT 800


/* WARNING: Keep this in sync with the `GnomeIconContainerIconMode' enum in
   `gnome-icon-container.h'.  */
GnomeIconContainerIconModeInfo gnome_icon_container_icon_mode_info[] = {
	{ 48, 48, 80, 80, 4, 44, 28 }, /* GNOME_ICON_CONTAINER_NORMAL_ICONS */
	{ 24, 24, 100, 40, 4, 16, 16 } /* GNOME_ICON_CONTAINER_SMALL_ICONS */
};

#define NUM_ICON_MODES (sizeof (gnome_icon_container_icon_mode_info) \
			/ sizeof (*gnome_icon_container_icon_mode_info))


/* The GnomeIconContainer signals.  */
enum _GnomeIconContainerSignalNumber {
	SELECTION_CHANGED,
	BUTTON_PRESS,
	ACTIVATE,
	CONTEXT_CLICK,
	LAST_SIGNAL
};
typedef enum _GnomeIconContainerSignalNumber GnomeIconContainerSignalNumber;
static guint signals[LAST_SIGNAL] = { 0 };

/* Bitmap for stippled selection rectangles.  */
static GdkBitmap *stipple;
static char stipple_bits[] = { 0x02, 0x01 };


/* Functions dealing with GnomeIconContainerIcons.  */

static void
icon_destroy (GnomeIconContainerIcon *icon)
{
	gtk_object_destroy (GTK_OBJECT (icon->item));
}

static void
icon_configure (GnomeIconContainerIcon *icon,
		GnomeIconContainer *container)
{
	switch (container->priv->icon_mode) {
	case GNOME_ICON_CONTAINER_NORMAL_ICONS:
		gnome_icon_text_item_configure
			(icon->text_item,
			 GNOME_ICON_CONTAINER_CELL_SPACING (container),
			 GNOME_ICON_CONTAINER_ICON_HEIGHT (container),
			 (GNOME_ICON_CONTAINER_CELL_WIDTH (container)
			  - 2 * GNOME_ICON_CONTAINER_CELL_SPACING (container)),
			 NULL,
			 icon->text,
			 TRUE,
			 TRUE);
		break;
	case GNOME_ICON_CONTAINER_SMALL_ICONS:
		gnome_icon_text_item_configure
			(icon->text_item,
			 (GNOME_ICON_CONTAINER_ICON_WIDTH (container)
			  + GNOME_ICON_CONTAINER_CELL_SPACING (container)),
			 GNOME_ICON_CONTAINER_CELL_HEIGHT (container) / 2,
			 (GNOME_ICON_CONTAINER_CELL_WIDTH (container)
			  - 2 * GNOME_ICON_CONTAINER_CELL_SPACING (container)
			  - GNOME_ICON_CONTAINER_ICON_WIDTH (container)),
			 NULL,
			 icon->text,
			 TRUE,
			 TRUE);
		break;
	default:
		g_warning ("Unknown icon mode %d.", container->priv->icon_mode);
	}

	gnome_canvas_item_set
		(GNOME_CANVAS_ITEM (icon->image_item),
		 "width", (gdouble) GNOME_ICON_CONTAINER_ICON_WIDTH (container),
		 "height", (gdouble) GNOME_ICON_CONTAINER_ICON_HEIGHT (container),
		 NULL);
}

static GnomeIconContainerIcon *
icon_new (GnomeIconContainer *container,
	  const gchar *text,
	  gpointer data)
{
	GnomeCanvas *canvas;
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIcon *new;

	canvas = GNOME_CANVAS (container);
	priv = container->priv;

	new = g_new (GnomeIconContainerIcon, 1);

	new->is_selected = FALSE;
	new->is_current = FALSE;
	new->layout_done = TRUE;
	new->was_selected_before_rubberband = FALSE;

	new->data = data;
	new->text = g_strdup (text); /* FIXME */

	new->item = GNOME_CANVAS_GROUP (gnome_canvas_item_new
					(GNOME_CANVAS_GROUP (canvas->root),
					 gnome_canvas_group_get_type (),
					 NULL));

	new->image_item = NULL;

	new->text_item
		= GNOME_ICON_TEXT_ITEM (gnome_canvas_item_new
					(new->item,
					 gnome_icon_text_item_get_type (),
					 NULL));

	new->width = GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	new->height = GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	return new;
}

static GnomeIconContainerIcon *
icon_new_pixbuf (GnomeIconContainer *container,
		GdkPixbuf *image,
		const gchar *text,
		gpointer data)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIcon *new;

	priv = container->priv;

	new = icon_new (container, text, data);

	new->image_item
		= gnome_canvas_item_new (new->item,
					 gnome_canvas_pixbuf_get_type (),
					 "pixbuf", image,
					 "x", (gdouble) 0,
					 "y", (gdouble) 0,
					 NULL);

	icon_configure (new, container);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (new->item),
			       "x", (gdouble) 0,
			       "y", (gdouble) 0,
			       NULL);

	return new;
}

static void
icon_position (GnomeIconContainerIcon *icon,
	       GnomeIconContainer *container,
	       gdouble x, gdouble y)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	icon->x = x;
	icon->y = y;

	/* ???  Canvas bug ???  It should be enough to do this once in
           `icon-configure()', but it does not work.  */

	switch (container->priv->icon_mode) {
	case GNOME_ICON_CONTAINER_NORMAL_ICONS:
		gnome_icon_text_item_setxy
			(icon->text_item,
			 GNOME_ICON_CONTAINER_CELL_SPACING (container),
			 (GNOME_ICON_CONTAINER_ICON_HEIGHT (container)
			  + GNOME_ICON_CONTAINER_CELL_SPACING (container) + 2));
		break;
	case GNOME_ICON_CONTAINER_SMALL_ICONS:
		gnome_icon_text_item_setxy
			(icon->text_item,
			 (GNOME_ICON_CONTAINER_ICON_WIDTH (container)
			  + GNOME_ICON_CONTAINER_CELL_SPACING (container)),
			 GNOME_ICON_CONTAINER_CELL_SPACING (container));
		break;
	default:
		g_warning ("Unknown icon mode %d.", container->priv->icon_mode);
	}
	
	gnome_canvas_item_set
		(icon->image_item,
		 "x", (gdouble) GNOME_ICON_CONTAINER_ICON_XOFFSET (container),
		 "y", (gdouble) GNOME_ICON_CONTAINER_ICON_YOFFSET (container),
		 NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (icon->item),
			       "x", (gdouble) icon->x,
			       "y", (gdouble) icon->y,
			       NULL);
}

static void
icon_raise (GnomeIconContainerIcon *icon)
{
	gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_show (GnomeIconContainerIcon *icon)
{
	gnome_canvas_item_show (GNOME_CANVAS_ITEM (icon->item));
}

static void
icon_select (GnomeIconContainerIcon *icon,
	     gboolean sel)
{
	gboolean was_selected;

	/* FIXME: We want the icon image to appear as selected too.  Maybe
           this can be done with a new custom CanvasImage-like item providing
           this functionality?  */

	was_selected = icon->is_selected;
	icon->is_selected = sel;

	gnome_icon_text_item_select (icon->text_item, sel);
}

static gboolean
icon_toggle_selection (GnomeIconContainerIcon *icon)
{
	if (icon->is_selected) {
		icon_select (icon, FALSE);
		return TRUE;
	} else {
		icon_select (icon, TRUE);
		return FALSE;
	}
}

static gboolean
icon_is_in_region (GnomeIconContainerIcon *icon,
		   gint x1, gint y1,
		   gint x2, gint y2)
{
	gint icon_x2, icon_y2;

	icon_x2 = icon->x + icon->width;
	icon_y2 = icon->y + icon->height;

	if (x1 == x2 && y1 == y2)
		return FALSE;

	if (x1 < icon_x2 && x2 >= icon->x && y1 < icon_y2 && y2 >= icon->y)
		return TRUE;
	else
		return FALSE;
}

static void
icon_get_text_bounding_box (GnomeIconContainerIcon *icon,
			    guint *x1_return, guint *y1_return,
			    guint *x2_return, guint *y2_return)
{
	double x1, y1, x2, y2;

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->text_item),
				      &x1, &y1, &x2, &y2);

	*x1_return = icon->x + x1;
	*y1_return = icon->y + y1;
	*x2_return = icon->x + x2;
	*y2_return = icon->y + y2;
}

static void
icon_get_bounding_box (GnomeIconContainerIcon *icon,
		       guint *x1_return, guint *y1_return,
		       guint *x2_return, guint *y2_return)
{
	double x1, y1, x2, y2;

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (icon->item),
				      &x1, &y1, &x2, &y2);

	*x1_return = x1;
	*y1_return = y1;
	*x2_return = x2;
	*y2_return = y2;
}


/* Functions for dealing with IconGrids.  */

static GnomeIconContainerIconGrid *
icon_grid_new (void)
{
	GnomeIconContainerIconGrid *new;

	new = g_new (GnomeIconContainerIconGrid, 1);

	new->width = new->height = 0;
	new->visible_width = 0;
	new->alloc_width = new->alloc_height = 0;

	new->elems = NULL;

	new->first_free_x = -1;
	new->first_free_y = -1;

	return new;
}

static void
icon_grid_clear (GnomeIconContainerIconGrid *grid)
{
	GList **p;
	guint i, j;

	p = grid->elems;
	for (j = 0; j < grid->height; j++) {
		for (i = 0; i < grid->width; i++) {
			if (p[i] != NULL) {
				g_list_free (p[i]);
				p[i] = NULL;
			}
		}

		p += grid->alloc_width;
	}

	grid->first_free_x = 0;
	grid->first_free_y = 0;
}

static void
icon_grid_destroy (GnomeIconContainerIconGrid *grid)
{
	icon_grid_clear (grid);
	g_free (grid->elems);
	g_free (grid);
}

inline static GList **
icon_grid_get_element_ptr (GnomeIconContainerIconGrid *grid,
			   guint x, guint y)
{
	return &grid->elems[y * grid->alloc_width + x];
}

inline static GList *
icon_grid_get_element (GnomeIconContainerIconGrid *grid,
		       guint x, guint y)
{
	return *icon_grid_get_element_ptr (grid, x, y);
}

/* This is admittedly a bit lame.

   Instead of re-allocating the grid from scratch and copying the values, we
   should just link grid chunks horizontally and vertically in lists;
   i.e. use a hybrid list/array representation.  */
static void
icon_grid_resize_allocation (GnomeIconContainerIconGrid *grid,
			     guint new_alloc_width,
			     guint new_alloc_height)
{
	GList **new_elems;
	guint i, j;
	guint new_alloc_size;

	if (new_alloc_width == 0 || new_alloc_height == 0) {
		g_free (grid->elems);
		grid->elems = NULL;
		grid->width = grid->height = 0;
		grid->alloc_width = new_alloc_width;
		grid->alloc_height = new_alloc_height;
		return;
	}

	new_alloc_size = new_alloc_width * new_alloc_height;
	new_elems = g_new (GList *, new_alloc_size);

	if (grid->elems == NULL || grid->width == 0 || grid->height == 0) {
		memset (new_elems, 0, sizeof (*new_elems) * new_alloc_size);
	} else {
		GList **sp, **dp;
		guint copy_width, copy_height;

		/* Copy existing elements into the new array.  */

		sp = grid->elems;
		dp = new_elems;
		copy_width = MIN (grid->width, new_alloc_width);
		copy_height = MIN (grid->height, new_alloc_height);
		
		for (i = 0; i < copy_height; i++) {
			for (j = 0; j < copy_width; j++)
				dp[j] = sp[j];

			for (j = copy_width; j < new_alloc_width; j++)
				dp[j] = NULL;

			for (j = copy_width; j < grid->width; j++)
				g_list_free (sp[j]);

			sp += grid->alloc_width;
			dp += new_alloc_width;
		}

		/* If there are other lines left, zero them as well.  */

		if (i < new_alloc_height) {
			guint elems_left;

			elems_left = new_alloc_size - (dp - new_elems);
			memset (dp, 0, sizeof (*new_elems) * elems_left);
		}
	}

	g_free (grid->elems);
	grid->elems = new_elems;

	grid->alloc_width = new_alloc_width;
	grid->alloc_height = new_alloc_height;
}

static void
icon_grid_update_first_free_forward (GnomeIconContainerIconGrid *grid)
{
	GList **p;
	guint start_x, start_y;
	guint x, y;

	if (grid->first_free_x == -1) {
		start_x = start_y = 0;
		p = grid->elems;
	} else {
		start_x = grid->first_free_x;
		start_y = grid->first_free_y;
		p = icon_grid_get_element_ptr (grid, start_x, start_y);
	}

	x = start_x;
	y = start_y;
	while (y < grid->height) {
		if (*p == NULL) {
			grid->first_free_x = x;
			grid->first_free_y = y;
			return;
		}

		x++, p++;

		if (x >= grid->visible_width) {
			x = 0;
			y++;
			p += grid->alloc_width - grid->visible_width;
		}
	}

	/* No free cell found.  */

	grid->first_free_x = -1;
	grid->first_free_y = -1;
}

static void
icon_grid_set_visible_width (GnomeIconContainerIconGrid *grid,
			     guint visible_width)
{
	if (visible_width > grid->visible_width
	    && grid->height > 0
	    && grid->first_free_x == -1) {
		grid->first_free_x = visible_width;
		grid->first_free_y = 0;
	} else if (grid->first_free_x >= visible_width) {
		if (grid->first_free_y == grid->height - 1) {
			grid->first_free_x = -1;
			grid->first_free_y = -1;
		} else {
			grid->first_free_x = 0;
			grid->first_free_y++;
			icon_grid_update_first_free_forward (grid);
		}
	}

	grid->visible_width = visible_width;
}

static void
icon_grid_resize (GnomeIconContainerIconGrid *grid,
		  guint width, guint height)
{
	guint new_alloc_width, new_alloc_height;

	if (width > grid->alloc_width || height > grid->alloc_height) {
		if (grid->alloc_width > 0)
			new_alloc_width = grid->alloc_width;
		else
			new_alloc_width = INITIAL_GRID_WIDTH;
		while (new_alloc_width < width)
			new_alloc_width *= 2;

		if (grid->alloc_height > 0)
			new_alloc_height = grid->alloc_height;
		else
			new_alloc_height = INITIAL_GRID_HEIGHT;
		while (new_alloc_height < height)
			new_alloc_height *= 2;

		icon_grid_resize_allocation (grid, new_alloc_width,
					     new_alloc_height);
	}

	grid->width = width;
	grid->height = height;

	if (grid->visible_width != grid->width)
		icon_grid_set_visible_width (grid, grid->width);
}

static void
icon_grid_maybe_resize (GnomeIconContainerIconGrid *grid,
			guint x, guint y)
{
	guint new_width, new_height;

	if (x < grid->width && y < grid->height)
		return;

	if (x >= grid->width)
		new_width = x + 1;
	else
		new_width = grid->width;

	if (y >= grid->height)
		new_height = y + 1;
	else
		new_height = grid->height;

	icon_grid_resize (grid, new_width, new_height);
}

static void
icon_grid_add (GnomeIconContainerIconGrid *grid,
	       GnomeIconContainerIcon *icon,
	       guint x, guint y)
{
	GList **elem_ptr;

	icon_grid_maybe_resize (grid, x, y);

	elem_ptr = icon_grid_get_element_ptr (grid, x, y);
	*elem_ptr = g_list_prepend (*elem_ptr, icon);

	if (x == grid->first_free_x && y == grid->first_free_y)
		icon_grid_update_first_free_forward (grid);
}

static void
icon_grid_remove (GnomeIconContainerIconGrid *grid,
		  GnomeIconContainerIcon *icon,
		  guint x, guint y)
{
	GList **elem_ptr;

	elem_ptr = icon_grid_get_element_ptr (grid, x, y);

	g_return_if_fail (*elem_ptr != NULL);

	*elem_ptr = g_list_remove (*elem_ptr, icon);

	if (*elem_ptr == NULL) {
		if ((grid->first_free_x == -1 && grid->first_free_y == -1)
		    || grid->first_free_y > y
		    || (grid->first_free_y == y && grid->first_free_x > x)) {
			grid->first_free_x = x;
			grid->first_free_y = y;
		}
	}
}

static void
icon_grid_add_auto (GnomeIconContainerIconGrid *grid,
		    GnomeIconContainerIcon *icon,
		    guint *x_return, guint *y_return)
{
	GList **empty_elem_ptr;

	if (grid->first_free_x < 0 || grid->first_free_y < 0
	    || grid->height == 0 || grid->width == 0) {
		/* No empty element: add a row.  */
		icon_grid_resize (grid, MAX (grid->width, 1), grid->height + 1);
		grid->first_free_x = 0;
		grid->first_free_y = grid->height - 1;
	}

	empty_elem_ptr = icon_grid_get_element_ptr (grid,
						    grid->first_free_x,
						    grid->first_free_y);

	*empty_elem_ptr = g_list_prepend (*empty_elem_ptr, icon);

	if (x_return != NULL)
		*x_return = grid->first_free_x;
	if (y_return != NULL)
		*y_return = grid->first_free_y;

	icon_grid_update_first_free_forward (grid);
}

static gint
icon_grid_cell_compare_by_x (gconstpointer ap,
			     gconstpointer bp)
{
	GnomeIconContainerIcon *a, *b;

	a = (GnomeIconContainerIcon *) ap;
	b = (GnomeIconContainerIcon *) bp;

	return (gint) a->x - b->x;
}


static void
world_to_grid (GnomeIconContainer *container,
	       gint world_x, gint world_y,
	       guint *grid_x_return, guint *grid_y_return)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	if (grid_x_return != NULL) {
		if (world_x < 0)
			*grid_x_return = 0;
		else
			*grid_x_return = world_x / GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	}

	if (grid_y_return != NULL) {
		if (world_y < 0)
			*grid_y_return = 0;
		else
			*grid_y_return = world_y / GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
grid_to_world (GnomeIconContainer *container,
	       guint grid_x, guint grid_y,
	       gint *world_x_return, gint *world_y_return)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	if (world_x_return != NULL)
		*world_x_return
			= grid_x * GNOME_ICON_CONTAINER_CELL_WIDTH (container);

	if (world_y_return != NULL)
		*world_y_return
			= grid_y * GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
}


/* Utility functions for GnomeIconContainer.  */

static void
scroll (GnomeIconContainer *container,
	gint delta_x, gint delta_y)
{
	GnomeIconContainerPrivate *priv;
	GtkAdjustment *hadj, *vadj;
	GtkAllocation *allocation;
	gfloat vnew, hnew;
	gfloat hmax, vmax;

	priv = container->priv;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	allocation = &GTK_WIDGET (container)->allocation;

	if (container->priv->width > allocation->width)
		hmax = (gfloat) (container->priv->width - allocation->width);
	else
		hmax = 0.0;

	if (container->priv->height > allocation->height)
		vmax = (gfloat) (container->priv->height - allocation->height);
	else
		vmax = 0.0;

	hnew = CLAMP (hadj->value + (gfloat) delta_x, 0.0, hmax);
	vnew = CLAMP (vadj->value + (gfloat) delta_y, 0.0, vmax);

	if (hnew != hadj->value) {
		hadj->value = hnew;
		gtk_signal_emit_by_name (GTK_OBJECT (hadj), "value_changed");
	}
	if (vnew != vadj->value) {
		vadj->value = vnew;
		gtk_signal_emit_by_name (GTK_OBJECT (vadj), "value_changed");
	}
}

static void
make_icon_visible (GnomeIconContainer *container,
		   GnomeIconContainerIcon *icon)
{
	GnomeIconContainerPrivate *priv;
	GtkAllocation *allocation;
	GtkAdjustment *hadj, *vadj;
	gint x1, y1, x2, y2;

	priv = container->priv;
	allocation = &GTK_WIDGET (container)->allocation;

	if (priv->height < allocation->height
	    && priv->width < allocation->width)
		return;

	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	icon_get_bounding_box (icon, &x1, &y1, &x2, &y2);

	if (y1 < vadj->value)
		gtk_adjustment_set_value (vadj, y1);
	else if (y2 > vadj->value + allocation->height)
		gtk_adjustment_set_value (vadj, y2 - allocation->height);

	if (x1 < hadj->value)
		gtk_adjustment_set_value (hadj, x1);
	else if (x2 > hadj->value + allocation->width)
		gtk_adjustment_set_value (hadj, x2 - allocation->width);
}

static gint
kbd_icon_visibility_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);

	if (container->priv->kbd_current != NULL)
		make_icon_visible (container, container->priv->kbd_current);
	container->priv->kbd_icon_visibility_timer_tag = -1;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
unschedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	if (priv->kbd_icon_visibility_timer_tag != -1)
		gtk_timeout_remove (priv->kbd_icon_visibility_timer_tag);
}

static void
schedule_kbd_icon_visibility (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	unschedule_kbd_icon_visibility (container);

	priv->kbd_icon_visibility_timer_tag
		= gtk_timeout_add (KBD_ICON_VISIBILITY_TIMEOUT,
				   kbd_icon_visibility_timeout_cb,
				   container);
}

static void
prepare_for_layout (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GList *p;

	priv = container->priv;

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		icon->layout_done = FALSE;
	}
}

/* Find the "first" icon (in left-to-right, top-to-bottom order) in
   `container'.  */
static GnomeIconContainerIcon *
find_first (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *first;
	GList **p;
	guint i, j;

	priv = container->priv;
	grid = priv->grid;

	if (grid->width == 0 || grid->height == 0)
		return NULL;

	first = NULL;
	p = grid->elems;
	for (i = 0; i < grid->height; i++) {
		for (j = 0; j < grid->width; j++) {
			GList *q;

			for (q = p[j]; q != NULL; q = q->next) {
				GnomeIconContainerIcon *icon;

				icon = q->data;
				if (first == NULL
				    || icon->y < first->y
				    || (icon->y == first->y
					&& icon->x < first->x))
					first = icon;
			}
		}

		p += grid->alloc_width;
	}

	return first;
}

static GnomeIconContainerIcon *
find_last (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *last;
	GList **p;
	gint i, j;

	priv = container->priv;
	grid = priv->grid;

	last = NULL;

	if (grid->height == 0 || grid->width == 0)
		return NULL;

	p = icon_grid_get_element_ptr (grid, 0, grid->height - 1);

	for (i = grid->height - 1; i >= 0; i--) {
		for (j = grid->width - 1; j >= 0; j--) {
			GList *q;

			for (q = p[j]; q != NULL; q = q->next) {
				GnomeIconContainerIcon *icon;

				icon = q->data;
				if (last == NULL
				    || icon->y > last->y
				    || (icon->y == last->y
					&& icon->x > last->x))
					last = icon;
			}
		}

		p -= grid->alloc_width;
	}

	return last;
}

/* Set `icon' as the icon currently selected for keyboard operations.  */
static void
set_kbd_current (GnomeIconContainer *container,
		 GnomeIconContainerIcon *icon,
		 gboolean schedule_visibility)
{
	GnomeIconContainerPrivate *priv;
	gint x1, y1, x2, y2;

	priv = container->priv;

	priv->kbd_current = icon;

	icon_get_text_bounding_box (icon, &x1, &y1, &x2, &y2);

	gnome_canvas_item_set (priv->kbd_navigation_rectangle,
			       "x1", (gdouble) x1 - 1,
			       "y1", (gdouble) y1 - 1,
			       "x2", (gdouble) x2,
			       "y2", (gdouble) y2,
			       NULL);
	gnome_canvas_item_show (priv->kbd_navigation_rectangle);

	icon_raise (icon);
	gnome_canvas_item_raise_to_top (priv->kbd_navigation_rectangle);

	if (schedule_visibility)
		schedule_kbd_icon_visibility (container);
	else
		unschedule_kbd_icon_visibility (container);
}

static void
unset_kbd_current (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	priv->kbd_current = NULL;
	gnome_canvas_item_hide (priv->kbd_navigation_rectangle);

	unschedule_kbd_icon_visibility (container);
}


/* Idle operation handler.  */

static void
set_scroll_region (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GtkAllocation *allocation;
	GtkAdjustment *vadj, *hadj;
	gdouble x1, y1, x2, y2;
	guint scroll_width, scroll_height;

	priv = container->priv;
	grid = priv->grid;
	allocation = &(GTK_WIDGET (container)->allocation);
	hadj = GTK_LAYOUT (container)->hadjustment;
	vadj = GTK_LAYOUT (container)->vadjustment;

	/* FIXME: We can do this more efficiently.  */
	gnome_canvas_item_get_bounds (GNOME_CANVAS (container)->root,
				      &x1, &y1, &x2, &y2);

	priv->width = x2 + GNOME_ICON_CONTAINER_CELL_SPACING (container);
	priv->height = y2 + GNOME_ICON_CONTAINER_CELL_SPACING (container);

	scroll_width = MAX (priv->width, allocation->width);
	scroll_height = MAX (priv->height, allocation->height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (container),
					0.0, 0.0,
					(gdouble) scroll_width,
					(gdouble) scroll_height);

	if (priv->width <= allocation->width)
		gtk_adjustment_set_value (hadj, 0.0);
	if (priv->height <= allocation->height)
		gtk_adjustment_set_value (vadj, 0.0);
}

static gint
idle_handler (gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerPrivate *priv;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);
	priv = container->priv;

	set_scroll_region (container);

	if (priv->icons != NULL && priv->kbd_current == NULL)
		set_kbd_current (container, find_first (container), FALSE);

	container->priv->idle_id = 0;

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
add_idle (GnomeIconContainer *container)
{
	if (container->priv->idle_id != 0)
		return;

	container->priv->idle_id = gtk_idle_add (idle_handler, container);
}


/* Container-level icon handling functions.  */

/* Select an icon.  Return TRUE if selection has changed.  */
static gboolean
select_icon (GnomeIconContainer *container,
	     GnomeIconContainerIcon *icon,
	     gboolean sel)
{
	GnomeIconContainerPrivate *priv;
	gboolean was_selected;

	priv = container->priv;

	was_selected = icon->is_selected;
	icon_select (icon, sel);

	if ((! was_selected && sel) || (was_selected && ! sel))
		return TRUE;
	else
		return FALSE;
}

static void
toggle_icon (GnomeIconContainer *container,
	     GnomeIconContainerIcon *icon)
{
	icon_toggle_selection (icon);
}

static gboolean
unselect_all_but_one (GnomeIconContainer *container,
		      GnomeIconContainerIcon *icon_to_avoid)
{
	GnomeIconContainerPrivate *priv;
	GList *p;
	gboolean selection_changed;

	priv = container->priv;
	selection_changed = FALSE;

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (icon != icon_to_avoid && icon->is_selected) {
			icon_select (icon, FALSE);
			selection_changed = TRUE;
		}
	}

	return selection_changed;
}

static gboolean
unselect_all (GnomeIconContainer *container)
{
	return unselect_all_but_one (container, NULL);
}

/* FIXME: This could be optimized a bit.  */
static void
move_icon (GnomeIconContainer *container,
	   GnomeIconContainerIcon *icon,
	   gint x, gint y)
{
	GnomeIconContainerPrivate *priv;
	gint old_x, old_y;
	guint old_grid_x, old_grid_y;
	gint old_x_offset, old_y_offset;
	guint new_grid_x, new_grid_y;
	gint new_x_offset, new_y_offset;

	priv = container->priv;

	old_x = icon->x;
	old_y = icon->y;

	world_to_grid (container, old_x, old_y, &old_grid_x, &old_grid_y);
	old_x_offset = old_x % GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	old_y_offset = old_y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	world_to_grid (container, x, y, &new_grid_x, &new_grid_y);
	new_x_offset = x % GNOME_ICON_CONTAINER_CELL_WIDTH (container);
	new_y_offset = y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

	icon_grid_remove (priv->grid, icon, old_grid_x, old_grid_y);
	if (old_x_offset > 0)
		icon_grid_remove (priv->grid, icon,
				  old_grid_x + 1, old_grid_y);
	if (old_y_offset > 0)
		icon_grid_remove (priv->grid, icon,
				  old_grid_x, old_grid_y + 1);
	if (old_x_offset > 0 && old_y_offset > 0)
		icon_grid_remove (priv->grid, icon,
				  old_grid_x + 1, old_grid_y + 1);

	icon_grid_add (priv->grid, icon, new_grid_x, new_grid_y);
	if (new_x_offset > 0)
		icon_grid_add (priv->grid, icon, new_grid_x + 1, new_grid_y);
	if (new_y_offset > 0)
		icon_grid_add (priv->grid, icon, new_grid_x, new_grid_y + 1);
	if (new_x_offset > 0 && new_y_offset > 0)
		icon_grid_add (priv->grid, icon, new_grid_x + 1, new_grid_y + 1);

	icon_position (icon, container, x, y);
}

static void
change_icon_mode (GnomeIconContainer *container,
		  GnomeIconContainerIconMode mode)
{
	GnomeIconContainerIconModeInfo *old_mode_info;
	GnomeIconContainerIconModeInfo *new_mode_info;
	GnomeIconContainerPrivate *priv;
	GList *p;
	gdouble x_factor, y_factor;
	
	priv = container->priv;
	if (mode == priv->icon_mode)
		return;

	old_mode_info = gnome_icon_container_icon_mode_info + priv->icon_mode;
	new_mode_info = gnome_icon_container_icon_mode_info + mode;

	priv->icon_mode = mode;

	x_factor = ((gdouble) new_mode_info->cell_width
		    / (gdouble) old_mode_info->cell_width);
	y_factor = ((gdouble) new_mode_info->cell_height
		    / (gdouble) old_mode_info->cell_height);

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;

		icon_configure (icon, container);
		icon_position (icon, container,
			       icon->x * x_factor, icon->y * y_factor);
	}

	add_idle (container);

	if (priv->kbd_current != NULL)
		set_kbd_current (container, priv->kbd_current, TRUE);
}


/* Implementation of rubberband selection.  */

static gboolean
rubberband_select_in_cell (GList *cell,
			   gdouble curr_x1, gdouble curr_y1,
			   gdouble curr_x2, gdouble curr_y2,
			   gdouble prev_x1, gdouble prev_y1,
			   gdouble prev_x2, gdouble prev_y2)
{
	GList *p;
	gboolean selection_changed;

	selection_changed = FALSE;

	for (p = cell; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;
		gboolean in_curr_region;
		gboolean in_prev_region;

		icon = p->data;

		in_curr_region = icon_is_in_region (icon,
						    curr_x1, curr_y1,
						    curr_x2, curr_y2);

		in_prev_region = icon_is_in_region (icon,
						    prev_x1, prev_y1,
						    prev_x2, prev_y2);

		if (in_curr_region && ! in_prev_region) {
			if (icon->was_selected_before_rubberband) {
				if (icon->is_selected) {
					icon_select (icon, FALSE);
					selection_changed = TRUE;
				}
			} else {
				if (! icon->is_selected) {
					icon_select (icon, TRUE);
					selection_changed = TRUE;
				}
			}
		} else if (in_prev_region && ! in_curr_region) {
			if (icon->was_selected_before_rubberband) {
				if (! icon->is_selected) {
					icon_select (icon, TRUE);
					selection_changed = TRUE;
				}
			} else {
				if (icon->is_selected) {
					icon_select (icon, FALSE);
					selection_changed = TRUE;
				}
			}
		}
	}

	return selection_changed;
}

static void
rubberband_select (GnomeIconContainer *container,
		   gdouble curr_x1, gdouble curr_y1,
		   gdouble curr_x2, gdouble curr_y2,
		   gdouble prev_x1, gdouble prev_y1,
		   gdouble prev_x2, gdouble prev_y2)
{
	GList **p;
	GnomeIconContainerIconGrid *grid;
	guint curr_grid_x1, curr_grid_y1;
	guint curr_grid_x2, curr_grid_y2;
	guint prev_grid_x1, prev_grid_y1;
	guint prev_grid_x2, prev_grid_y2;
	guint grid_x1, grid_y1;
	guint grid_x2, grid_y2;
	guint i, j;
	gboolean selection_changed;

	grid = container->priv->grid;

	world_to_grid (container, curr_x1, curr_y1, &curr_grid_x1, &curr_grid_y1);
	world_to_grid (container, curr_x2, curr_y2, &curr_grid_x2, &curr_grid_y2);
	world_to_grid (container, prev_x1, prev_y1, &prev_grid_x1, &prev_grid_y1);
	world_to_grid (container, prev_x2, prev_y2, &prev_grid_x2, &prev_grid_y2);

	grid_x1 = MIN (curr_grid_x1, prev_grid_x1);
	grid_x2 = MAX (curr_grid_x2, prev_grid_x2);
	grid_y1 = MIN (curr_grid_y1, prev_grid_y1);
	grid_y2 = MAX (curr_grid_y2, prev_grid_y2);

	selection_changed = FALSE;

	p = icon_grid_get_element_ptr (grid, grid_x1, grid_y1);
	for (i = 0; i <= grid_y2 - grid_y1; i++) {
		for (j = 0; j <= grid_x2 - grid_x1; j++) {
			if (rubberband_select_in_cell (p[j],
						       curr_x1, curr_y1,
						       curr_x2, curr_y2,
						       prev_x1, prev_y1,
						       prev_x2, prev_y2))
				selection_changed = TRUE;
		}

		p += grid->alloc_width;
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

static gint
rubberband_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;
	GtkWidget *widget;
	GnomeIconContainerRubberbandInfo *rinfo;
	gint x, y;
	gdouble x1, y1, x2, y2;
	gdouble world_x, world_y;
	gint x_scroll, y_scroll;

	GDK_THREADS_ENTER ();

	widget = GTK_WIDGET (data);
	container = GNOME_ICON_CONTAINER (data);
	rinfo = &container->priv->rubberband_info;

	gdk_window_get_pointer (widget->window, &x, &y, NULL);

	if (x < 0) {
		x_scroll = x;
		x = 0;
	} else if (x >= widget->allocation.width) {
		x_scroll = x - widget->allocation.width + 1;
		x = widget->allocation.width - 1;
	} else {
		x_scroll = 0;
	}

	if (y < 0) {
		y_scroll = y;
		y = 0;
	} else if (y >= widget->allocation.height) {
		y_scroll = y - widget->allocation.height + 1;
		y = widget->allocation.height - 1;
	} else {
		y_scroll = 0;
	}

	if (y_scroll == 0 && x_scroll == 0
	    && rinfo->prev_x == x && rinfo->prev_y == y) {
		GDK_THREADS_LEAVE ();
		return TRUE;
	}

	scroll (container, x_scroll, y_scroll);

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      x, y, &world_x, &world_y);

	if (world_x < rinfo->start_x) {
		x1 = world_x;
		x2 = rinfo->start_x;
	} else {
		x1 = rinfo->start_x;
		x2 = world_x;
	}

	if (world_y < rinfo->start_y) {
		y1 = world_y;
		y2 = rinfo->start_y;
	} else {
		y1 = rinfo->start_y;
		y2 = world_y;
	}

	gnome_canvas_item_set (rinfo->selection_rectangle,
			       "x1", (gdouble) x1,
			       "y1", (gdouble) y1,
			       "x2", (gdouble) x2,
			       "y2", (gdouble) y2,
			       NULL);

	rubberband_select (container,
			   x1, y1, x2, y2,
			   rinfo->prev_x1, rinfo->prev_y1,
			   rinfo->prev_x2, rinfo->prev_y2);

	rinfo->prev_x = x;
	rinfo->prev_y = y;
	rinfo->prev_x1 = x1;
	rinfo->prev_y1 = y1;
	rinfo->prev_x2 = x2;
	rinfo->prev_y2 = y2;	

	GDK_THREADS_LEAVE ();

	return TRUE;
}

static void
start_rubberbanding (GnomeIconContainer *container,
		     GdkEventButton *event)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerRubberbandInfo *rinfo;
	GList *p;

	priv = container->priv;
	rinfo = &priv->rubberband_info;

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      event->x, event->y,
				      &rinfo->start_x, &rinfo->start_y);

	rinfo->selection_rectangle
		= gnome_canvas_item_new (gnome_canvas_root
					 (GNOME_CANVAS (container)),
					 gnome_canvas_rect_get_type (),
					 "x1", rinfo->start_x,
					 "y1", rinfo->start_y,
					 "x2", rinfo->start_x,
					 "y2", rinfo->start_y,
					 "outline_color", "black",
					 "outline_stipple", stipple,
					 "width_pixels", 2,
					 NULL);

	rinfo->prev_x = rinfo->prev_x1 = rinfo->prev_x2 = event->x;
	rinfo->prev_y = rinfo->prev_y1 = rinfo->prev_y2 = event->y;

	rinfo->active = TRUE;

	rinfo->timer_tag = gtk_timeout_add (RUBBERBAND_TIMEOUT_INTERVAL,
					    rubberband_timeout_cb,
					    container);

	gnome_canvas_item_grab (rinfo->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (GnomeIconContainer *container,
		    GdkEventButton *event)
{
	GnomeIconContainerRubberbandInfo *rinfo;

	rinfo = &container->priv->rubberband_info;

	gtk_timeout_remove (rinfo->timer_tag);
	rinfo->active = FALSE;

	gnome_canvas_item_ungrab (rinfo->selection_rectangle, event->time);
	gtk_object_destroy (GTK_OBJECT (rinfo->selection_rectangle));
}


/* Keyboard navigation.  */

static void
kbd_move_to (GnomeIconContainer *container,
	     GnomeIconContainerIcon *icon,
	     GdkEventKey *event)
{
	if (! (event->state & GDK_CONTROL_MASK)) {
		gboolean selection_changed;

		selection_changed = unselect_all (container);
		selection_changed |= select_icon (container, icon, TRUE);

		if (selection_changed)
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}

	set_kbd_current (container, icon, FALSE);
	make_icon_visible (container, icon);
}

static void
kbd_home (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerIcon *first;

	first = find_first (container);
	if (first != NULL)
		kbd_move_to (container, first, event);
}

static void
kbd_end (GnomeIconContainer *container,
	 GdkEventKey *event)
{
	GnomeIconContainerIcon *last;

	last = find_last (container);
	if (last != NULL)
		kbd_move_to (container, last, event);
}

static void
kbd_left (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	gint x, y;
	gint max_x;

	priv = container->priv;
	grid = priv->grid;

	if (priv->kbd_current == NULL)
		return;

	world_to_grid (container, priv->kbd_current->x, priv->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, 0, grid_y);
	nearmost = NULL;

	max_x = priv->kbd_current->x;

	while (1) {
		while (1) {
			GList *p;

			for (p = e[grid_x]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;
				if (icon == priv->kbd_current
				    || icon->x < x
				    || icon->y < y)
					continue;

				if (icon->x <= max_x
				    && (nearmost == NULL
					|| icon->x > nearmost->x))
					nearmost = icon;
			}
 
			if (nearmost != NULL) {
				kbd_move_to (container, nearmost, event);
				return;
			}

			if (grid_x == 0)
				break;

			grid_x--;
			x -= GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		if (grid_y == 0)
			break;

		grid_x = grid->width - 1;
		max_x = G_MAXINT;
		grid_to_world (container, grid_x, 0, &x, NULL);

		e -= grid->alloc_width;
		grid_y--;
		y -= GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
kbd_up (GnomeIconContainer *container,
	GdkEventKey *event)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	gint x, y;

	priv = container->priv;
	grid = priv->grid;

	if (priv->kbd_current == NULL)
		return;

	world_to_grid (container, priv->kbd_current->x, priv->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, grid_x, grid_y);
	nearmost = NULL;

	while (1) {
		GList *p;

		p = *e;

		for (; p != NULL; p = p->next) {
			GnomeIconContainerIcon *icon;

			icon = p->data;
			if (icon == priv->kbd_current
			    || icon->x < x
			    || icon->y < y)
				continue;

			if (icon->y <= priv->kbd_current->y
			    && (nearmost == NULL || icon->y > nearmost->y))
				nearmost = icon;
		}

		if (nearmost != NULL)
			break;

		if (grid_y == 0)
			break;

		e -= grid->alloc_width;
		grid_y--;
		y -= GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}

	if (nearmost != NULL)
		kbd_move_to (container, nearmost, event);
}

static void
kbd_right (GnomeIconContainer *container,
	   GdkEventKey *event)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	gint x, y;
	gint min_x;

	priv = container->priv;
	grid = priv->grid;

	if (priv->kbd_current == NULL)
		return;

	world_to_grid (container, priv->kbd_current->x, priv->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, 0, grid_y);
	nearmost = NULL;

	min_x = priv->kbd_current->x;

	while (grid_y < grid->height) {
		while (grid_x < grid->width) {
			GList *p;

			for (p = e[grid_x]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;
				if (icon == priv->kbd_current
				    || icon->x < x
				    || icon->y < y)
					continue;

				if (icon->x >= min_x
				    && (nearmost == NULL
					|| icon->x < nearmost->x))
					nearmost = icon;
			}
 
			if (nearmost != NULL) {
				kbd_move_to (container, nearmost, event);
				return;
			}

			grid_x++;
			x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		grid_x = 0;
		min_x = 0;
		x = 0;

		e += grid->alloc_width;
		grid_y++;
		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}
}

static void
kbd_down (GnomeIconContainer *container,
	  GdkEventKey *event)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIcon *nearmost;
	GList **e;
	guint grid_x, grid_y;
	gint x, y;

	priv = container->priv;
	grid = priv->grid;

	if (priv->kbd_current == NULL)
		return;

	world_to_grid (container, priv->kbd_current->x, priv->kbd_current->y,
		       &grid_x, &grid_y);
	grid_to_world (container, grid_x, grid_y, &x, &y);

	e = icon_grid_get_element_ptr (grid, grid_x, grid_y);
	nearmost = NULL;

	while (grid_y < grid->height) {
		GList *p;

		p = *e;

		for (; p != NULL; p = p->next) {
			GnomeIconContainerIcon *icon;

			icon = p->data;
			if (icon == priv->kbd_current
			    || icon->x < x
			    || icon->y < y)
				continue;

			if (icon->y >= priv->kbd_current->y
			    && (nearmost == NULL || icon->y < nearmost->y))
				nearmost = icon;
		}

		if (nearmost != NULL)
			break;

		e += grid->alloc_width;
		grid_y++;
		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
	}

	if (nearmost != NULL)
		kbd_move_to (container, nearmost, event);
}

static void
kbd_space (GnomeIconContainer *container,
	   GdkEventKey *event)
{
	if (container->priv->kbd_current != NULL) {
		if (select_icon (container, container->priv->kbd_current, TRUE))
			gtk_signal_emit (GTK_OBJECT (container),
					 signals[SELECTION_CHANGED]);
	}
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	GnomeIconContainer *container;

	container = GNOME_ICON_CONTAINER (object);

	icon_grid_destroy (container->priv->grid);

	gnome_icon_container_dnd_fini (container);

	g_free (container->priv->base_uri);

	g_hash_table_destroy (container->priv->canvas_item_to_icon);

	g_free (container->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static void
size_request (GtkWidget *widget,
	      GtkRequisition *requisition)
{
	requisition->width = 1;
	requisition->height = 1;
}

static void
size_allocate (GtkWidget *widget,
	       GtkAllocation *allocation)
{
	GnomeIconContainer *container;
	GnomeIconContainerIconGrid *grid;
	guint visible_width, visible_height;

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate)
			(widget, allocation);

	container = GNOME_ICON_CONTAINER (widget);
	grid = container->priv->grid;

	world_to_grid (container,
		       allocation->width, 0,
		       &visible_width, &visible_height);

	if (visible_width == 0)
		visible_width = 1;

	if (visible_width > grid->width || visible_height > grid->height)
		icon_grid_resize (grid,
				  MAX (visible_width, grid->width),
				  MAX (visible_height, grid->height));

	icon_grid_resize(grid, visible_width, visible_height);
	/*	icon_grid_set_visible_width (grid, visible_width); */

	set_scroll_region (container);
}

static void
realize (GtkWidget *widget)
{
	GtkStyle *style;

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	style = gtk_style_copy (gtk_widget_get_style (widget));
	style->bg[GTK_STATE_NORMAL] = style->base[GTK_STATE_NORMAL];
	gtk_widget_set_style (widget, style);

	gdk_window_set_background (GTK_LAYOUT (widget)->bin_window,
				   & widget->style->bg [GTK_STATE_NORMAL]);
}

static gboolean
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	gboolean return_value;
	GnomeIconContainer *container;

	/* Invoke the canvas event handler and see if an item picks up the
           event.  */
	if ((* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event))
		return TRUE;

	container = GNOME_ICON_CONTAINER (widget);

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		if (! (event->state & GDK_CONTROL_MASK)) {
			gboolean selection_changed;

			selection_changed = unselect_all (container);
			if (selection_changed)
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[SELECTION_CHANGED]);
		}

		start_rubberbanding (container, event);
		return TRUE;
	}

	gtk_signal_emit (GTK_OBJECT (widget), signals[BUTTON_PRESS], event,
			 &return_value);

	return return_value;
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	GnomeIconContainer *container;
	GnomeIconContainerPrivate *priv;

	container = GNOME_ICON_CONTAINER (widget);
	priv = container->priv;

	if (event->button == 1 && priv->rubberband_info.active) {
		stop_rubberbanding (container, event);
		return TRUE;
	}

	if (event->button == priv->drag_button) {
		priv->drag_button = 0;
	        if (! priv->doing_drag
		    && ! (event->state & GDK_CONTROL_MASK)) {
			gboolean selection_changed;

			selection_changed
				= unselect_all_but_one (container,
							priv->drag_icon);

			if (selection_changed)
				gtk_signal_emit (GTK_OBJECT (container),
						 signals[SELECTION_CHANGED]);
		}

		if (priv->drag_icon != NULL) {
			set_kbd_current (container, priv->drag_icon, TRUE);
			priv->drag_icon = NULL;
		}

		if (priv->doing_drag)
			gnome_icon_container_dnd_end_drag (container);

		priv->doing_drag = FALSE;
		return TRUE;
	}

	if (GTK_WIDGET_CLASS (parent_class)->button_release_event != NULL)
		return GTK_WIDGET_CLASS (parent_class)->button_release_event
			(widget, event);

	return FALSE;
}

static gint
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *motion)
{
	GnomeIconContainer *container;
	GnomeIconContainerPrivate *priv;
	double world_x, world_y;

	container = GNOME_ICON_CONTAINER (widget);
	priv = container->priv;

	gnome_canvas_window_to_world (GNOME_CANVAS (container),
				      motion->x, motion->y,
				      &world_x, &world_y);

#define SNAP_RESISTANCE 2	/* GMC has this set to 3, but it's too much for
                                   my taste.  */
	if (priv->drag_button != 0
	    && abs (priv->drag_x - world_x) >= SNAP_RESISTANCE
	    && abs (priv->drag_y - world_y) >= SNAP_RESISTANCE) {
		priv->doing_drag = TRUE;

		/* KLUDGE ALERT: Poke the starting values into the motion
                   structure so that dragging behaves as expected.  */
		motion->x = priv->drag_x;
		motion->y = priv->drag_y;

		gnome_icon_container_dnd_begin_drag (container,
						     GDK_ACTION_MOVE,
						     priv->drag_button,
						     motion);
		return TRUE;
	}
#undef SNAP_RESISTANCE

	if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event != NULL)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event)
			(widget, motion);

	return FALSE;
}

static gint
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	GnomeIconContainer *container;

	if ((* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event))
		return TRUE;

	container = GNOME_ICON_CONTAINER (widget);

	switch (event->keyval) {
	case GDK_Home:
		kbd_home (container, event);
		break;
	case GDK_End:
		kbd_end (container, event);
		break;
	case GDK_Left:
		kbd_left (container, event);
		break;
	case GDK_Up:
		kbd_up (container, event);
		break;
	case GDK_Right:
		kbd_right (container, event);
		break;
	case GDK_Down:
		kbd_down (container, event);
		break;
	case GDK_space:
		kbd_space (container, event);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}


/* Initialization.  */

static void
class_init (GnomeIconContainerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	/* Derive from GnomeCanvas.  */

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	/* GnomeIconContainer class.  */

	class->button_press = NULL;

	/* GtkObject class.  */

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	/* Signals.  */

	signals[SELECTION_CHANGED]
		= gtk_signal_new ("selection_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     selection_changed),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	signals[BUTTON_PRESS]
		= gtk_signal_new ("button_press",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     button_press),
				  gtk_marshal_BOOL__POINTER,
				  GTK_TYPE_BOOL, 1,
				  GTK_TYPE_GDK_EVENT);
	signals[ACTIVATE]
		= gtk_signal_new ("activate",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     activate),
				  gtk_marshal_NONE__POINTER_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_STRING,
				  GTK_TYPE_POINTER);
	signals[CONTEXT_CLICK]
		= gtk_signal_new ("context_click",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (GnomeIconContainerClass,
						     activate),
				  gtk_marshal_NONE__POINTER_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_STRING,
				  GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	/* GtkWidget class.  */

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = size_request;
	widget_class->size_allocate = size_allocate;
	widget_class->realize = realize;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->key_press_event = key_press_event;

	/* Initialize the stipple bitmap.  */

	stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);
}

static void
init (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;

	priv = g_new (GnomeIconContainerPrivate, 1);

	priv->base_uri = NULL;

	priv->width = priv->height = 0;

	priv->icons = NULL;
	priv->num_icons = 0;

	priv->icon_mode = GNOME_ICON_CONTAINER_NORMAL_ICONS;

	priv->grid = icon_grid_new ();

	priv->canvas_item_to_icon = g_hash_table_new (g_direct_hash,
						      g_direct_equal);

	priv->kbd_navigation_rectangle 
		= gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (container)),
					 gnome_canvas_rect_get_type (),
					 "outline_color", "black",
					 "outline_stipple", stipple,
					 "width_pixels", 1,
					 NULL);
	gnome_canvas_item_hide (priv->kbd_navigation_rectangle);

	priv->kbd_current = NULL;
	priv->rubberband_info.active = FALSE;
	priv->kbd_icon_visibility_timer_tag = -1;
	priv->idle_id = 0;

	priv->drag_button = 0;
	priv->drag_icon = NULL;
	priv->drag_x = priv->drag_y = 0;
	priv->doing_drag = FALSE;

	priv->browser_mode = FALSE;
	priv->browser_mode_selection_timer_tag = -1;
	priv->browser_mode_selection_icon = NULL;

	container->priv = priv;

	/* Set up DnD.  */
	gnome_icon_container_dnd_init (container, stipple);

	/* Request update.  */
	add_idle (container);
}


/* GnomeIconContainerIcon event handling.  */

/* Selection in browser mode.  */
static gint
browser_select_timeout_cb (gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIcon *icon;
	gboolean selection_changed;

	GDK_THREADS_ENTER ();

	container = GNOME_ICON_CONTAINER (data);
	priv = container->priv;
	icon = priv->browser_mode_selection_icon;

	selection_changed = unselect_all (container);
	selection_changed |= select_icon (container, icon, TRUE);

	set_kbd_current (container, icon, FALSE);
	make_icon_visible (container, icon);

	/* FIXME: Am I allowed to do this between `GDK_THREADS_ENTER()' and
           `GDK_THREADS_LEAVE()'?  */
	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

/* Conceptually, pressing button 1 together with CTRL toggles selection of a
   single icon without affecting the other icons; without CTRL, it selects a
   single icon and un-selects all the other icons.  But in this latter case,
   the de-selection should only happen when the button is released if the
   icon is already selected, because the user might select multiple icons and
   drag all of them by doing a simple click-drag.  */
static gint
handle_icon_button_press (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventButton *event)
{
	GnomeIconContainerPrivate *priv;
	gdouble world_x, world_y;

	if (event->button != 1)
		return FALSE;

	priv = container->priv;

	if (event->state & GDK_CONTROL_MASK) {
		toggle_icon (container, icon);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	} else if (! icon->is_selected) {
		unselect_all (container);
		select_icon (container, icon, TRUE);
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
	}

	if (event->type == GDK_2BUTTON_PRESS) {
		/* Double clicking should *never* trigger a D&D action.
                 * We must clear this out before emitting the signal, because
		 * handling the activate signal might invalidate the drag_icon pointer.
                 */
		priv->drag_button = 0;
		priv->drag_icon = NULL;

		gtk_signal_emit (GTK_OBJECT (container),
				 signals[ACTIVATE],
				 icon->text, icon->data);

		return TRUE;
	}

	if (event->button == 3) {
		/* FIXME this means you cannot drag with right click.  Instead,
                   we should setup a timeout and emit this signal if the
                   timeout expires without movement.  */
		priv->drag_button = 0;
		priv->drag_icon = NULL;

		gtk_signal_emit (GTK_OBJECT (container),
				 signals[CONTEXT_CLICK],
				 icon->text, icon->data);

		return TRUE;
	}

	priv->drag_button = event->button;
	priv->drag_icon = icon;
	priv->drag_x = event->x;
	priv->drag_y = event->y;

	gnome_canvas_window_to_world (GNOME_CANVAS (container), event->x, event->y,
				      &world_x, &world_y);
	priv->drag_x_offset = (gint) world_x - icon->x;
	priv->drag_y_offset = (gint) world_y - icon->y;

	return TRUE;
}

static gint
handle_icon_enter_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;
	if (! priv->browser_mode)
		return FALSE;

	if (priv->browser_mode_selection_timer_tag != -1)
		gtk_timeout_remove (priv->browser_mode_selection_timer_tag);

	priv->browser_mode_selection_timer_tag
		= gtk_timeout_add (BROWSER_MODE_SELECTION_TIMEOUT,
				   browser_select_timeout_cb, container);

	priv->browser_mode_selection_icon = icon;

	return TRUE;
}

static gint
handle_icon_leave_notify (GnomeIconContainer *container,
			  GnomeIconContainerIcon *icon,
			  GdkEventMotion *motion)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;
	if (! priv->browser_mode)
		return FALSE;

	if (priv->browser_mode_selection_timer_tag != -1)
		gtk_timeout_remove (priv->browser_mode_selection_timer_tag);

	return TRUE;
}

static gint
item_event_cb (GnomeCanvasItem *item,
	       GdkEvent *event,
	       gpointer data)
{
	GnomeIconContainer *container;
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIcon *icon;

	container = GNOME_ICON_CONTAINER (data);
	priv = container->priv;

	icon = g_hash_table_lookup (priv->canvas_item_to_icon, item);
	g_return_val_if_fail (icon != NULL, FALSE);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
		return handle_icon_button_press (container, icon, &event->button);
	case GDK_ENTER_NOTIFY:
		return handle_icon_enter_notify (container, icon, &event->motion);
	case GDK_LEAVE_NOTIFY:
		return handle_icon_leave_notify (container, icon, &event->motion);
	default:
		return FALSE;
	}
}


GtkWidget *
gnome_icon_container_new (void)
{
	GtkWidget *new;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	new = gtk_type_new (gnome_icon_container_get_type ());

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	return new;
}


guint
gnome_icon_container_get_type (void)
{
	static guint type = 0;

	if (type == 0) {
		GtkTypeInfo type_info = {
			"GnomeIconContainer",
			sizeof (GnomeIconContainer),
			sizeof (GnomeIconContainerClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (gnome_canvas_get_type (), &type_info);
	}

	return type;
}

void
gnome_icon_container_clear (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GList *p;

	g_return_if_fail (container != NULL);

	priv = container->priv;

	for (p = priv->icons; p != NULL; p = p->next)
		icon_destroy (p->data);
	g_list_free (priv->icons);
	priv->icons = NULL;
	priv->num_icons = 0;

	icon_grid_clear (priv->grid);

	unset_kbd_current (container);
}


void
gnome_icon_container_set_icon_mode (GnomeIconContainer *container,
				    GnomeIconContainerIconMode mode)
{
	g_return_if_fail (container != NULL);

	if (mode >= NUM_ICON_MODES) {
		g_warning ("Unknown icon mode %d", mode);
		return;
	}

	change_icon_mode (container, mode);
}

GnomeIconContainerIconMode
gnome_icon_container_get_icon_mode (GnomeIconContainer *container)
{
	g_return_val_if_fail (container != NULL, GNOME_ICON_CONTAINER_NORMAL_ICONS);

	return container->priv->icon_mode;
}


static void
setup_icon_in_container (GnomeIconContainer *container,
			 GnomeIconContainerIcon *icon)
{
	GnomeIconContainerPrivate *priv;

	priv = container->priv;

	priv->icons = g_list_prepend (priv->icons, icon);
	priv->num_icons++;

	g_hash_table_insert (priv->canvas_item_to_icon, icon->item, icon);
	icon_show (icon);

	gtk_signal_connect (GTK_OBJECT (icon->item), "event",
			    GTK_SIGNAL_FUNC (item_event_cb), container);
}

void
gnome_icon_container_add_pixbuf (GnomeIconContainer *container,
				GdkPixbuf *image,
				const gchar *text,
				gint x, gint y,
				gpointer data)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIcon *new_icon;
	guint grid_x, grid_y;

	g_return_if_fail (container != NULL);
	g_return_if_fail (image != NULL);
	g_return_if_fail (text != NULL);

	priv = container->priv;

	new_icon = icon_new_pixbuf (container, image, text, data);
	icon_position (new_icon, container, x, y);

	world_to_grid (container, x, y, &grid_x, &grid_y);
	icon_grid_add (container->priv->grid, new_icon, grid_x, grid_y);

	if (x % GNOME_ICON_CONTAINER_CELL_WIDTH (container) > 0)
		icon_grid_add (priv->grid, new_icon, grid_x + 1, grid_y);
	if (y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container) > 0)
		icon_grid_add (priv->grid, new_icon, grid_x, grid_y + 1);
	if (x % GNOME_ICON_CONTAINER_CELL_WIDTH (container) > 0
	    && y % GNOME_ICON_CONTAINER_CELL_HEIGHT (container) > 0)
		icon_grid_add (priv->grid, new_icon, grid_x + 1, grid_y + 1);

	setup_icon_in_container (container, new_icon);

	add_idle (container);
}

/**
 * gnome_icon_container_add_pixbuf_auto:
 * @container: A GnomeIconContainer
 * @image: Image of the icon to add
 * @text: Caption
 * @data: Icon-specific data
 * 
 * Add @image with caption @text and data @data to @container, in the first
 * empty spot available.
 **/
void
gnome_icon_container_add_pixbuf_auto (GnomeIconContainer *container,
				     GdkPixbuf *image,
				     const gchar *text,
				     gpointer data)
{
	GnomeIconContainerIcon *new_icon;
	guint grid_x, grid_y;
	gint x, y;

	g_return_if_fail (container != NULL);
	g_return_if_fail (image != NULL);
	g_return_if_fail (text != NULL);

	new_icon = icon_new_pixbuf (container, image, text, data);

	icon_grid_add_auto (container->priv->grid, new_icon, &grid_x, &grid_y);

	grid_to_world (container, grid_x, grid_y, &x, &y);
	icon_position (new_icon, container, x, y);

	setup_icon_in_container (container, new_icon);

	add_idle (container);
}

/**
 * gnome_icon_container_add_pixbuf_with_layout:
 * @container: A GnomeIconContainer
 * @image: Image of the icon to add
 * @text: Caption
 * @data: Icon-specific data
 * @layout: Layout information
 * 
 * Add @image with the caption @text to @container using @layout, and attach
 * @data to it.
 * 
 * Return value: %FALSE if @text is not in @layout (and, consequently, the icon
 * has not been added); %TRUE otherwise.
 **/
gboolean
gnome_icon_container_add_pixbuf_with_layout (GnomeIconContainer *container,
					    GdkPixbuf *image,
					    const gchar *text,
					    gpointer data,
					    const GnomeIconContainerLayout *layout)
{
	gint x, y;

	g_return_val_if_fail (container != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);
	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (layout != NULL, FALSE);

	if (gnome_icon_container_layout_get_position (layout, text, &x, &y)) {
		gnome_icon_container_add_pixbuf (container, image,
						text, x, y, data);
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * gnome_icon_container_relayout:
 * @container: An icon container.
 * 
 * Relayout the icons in @container according to the allocation we are given.
 * This is done by just collecting icons from top to bottom, from left to
 * right, and tiling them in the same direction.  The tiling is done in such a
 * way that no horizontal scrolling is needed to see all the icons.
 **/
void
gnome_icon_container_relayout (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *old_grid, *new_grid;
	GList **sp, **dp;
	guint i, j;
	guint dx, dy;
	guint sx, sy;
	guint cols;
	guint lines;

	g_return_if_fail (container != NULL);

	priv = container->priv;
	old_grid = priv->grid;

	g_return_if_fail (old_grid->visible_width > 0);

	prepare_for_layout (container);

	new_grid = icon_grid_new ();

	if (priv->num_icons % old_grid->visible_width != 0)
		icon_grid_resize (new_grid,
				  old_grid->visible_width,
				  (priv->num_icons
				   / old_grid->visible_width) + 1);
	else
		icon_grid_resize (new_grid,
				  old_grid->visible_width,
				  priv->num_icons / old_grid->visible_width);

	icon_grid_set_visible_width (new_grid, old_grid->visible_width);

	sp = old_grid->elems;
	dp = new_grid->elems;
	sx = sy = 0;
	dx = dy = 0;
	cols = lines = 0;
	for (i = 0; i < old_grid->height; i++) {
		for (j = 0; j < old_grid->width; j++) {
			GList *p;

			/* Make sure the icons are sorted by increasing X
			   position.  */
			sp[j] = g_list_sort (sp[j],
					     icon_grid_cell_compare_by_x);

			for (p = sp[j]; p != NULL; p = p->next) {
				GnomeIconContainerIcon *icon;

				icon = p->data;

				/* Make sure icons are not moved twice, and
				   ignore icons whose upper left corner is not
				   in this cell, unless the icon is partly
				   outside the container.  */
				if (icon->layout_done
				    || (icon->x >= 0 && icon->x < sx)
				    || (icon->y >= 0 && icon->y < sy))
					continue;

				dp[cols] = g_list_alloc ();
				dp[cols]->data = icon;

				icon_position (icon, container, dx, dy);

				icon->layout_done = TRUE;

				if (++cols == new_grid->visible_width) {
					cols = 0, lines++;
					dx = 0, dy += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
					dp += new_grid->alloc_width;
				} else {
					dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
				}
			}

			sx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		sx = 0, sy += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);

		sp += old_grid->alloc_width;
	}

	if (cols < new_grid->visible_width && lines < new_grid->height) {
		new_grid->first_free_x = cols;
		new_grid->first_free_y = lines;
	} else {
		new_grid->first_free_x = -1;
		new_grid->first_free_y = -1;
	}

	icon_grid_destroy (priv->grid);
	priv->grid = new_grid;

	if (priv->kbd_current != NULL)
		set_kbd_current (container, priv->kbd_current, FALSE);

	add_idle (container);
}


/**
 * gnome_icon_container_line_up:
 * @container: An icon container.
 * 
 * Line up icons in @container.
 **/
void
gnome_icon_container_line_up (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GnomeIconContainerIconGrid *new_grid;
	GList **p, **q;
	guint new_grid_width;
	guint i, j, k, m;
	gint x, y, dx;

	g_return_if_fail (container != NULL);

	priv = container->priv;
	grid = priv->grid;

	/* Mark all icons as "not moved yet".  */

	prepare_for_layout (container);

	/* Calculate the width for the resulting new grid.  This is the maximum
           width across all the lines.  */

	new_grid_width = 0;
	p = grid->elems;
	x = y = 0;
	for (i = 0; i < grid->height; i++) {
		guint line_width;

		line_width = grid->width;
		for (j = 0; j < grid->width; j++) {
			GList *e;
			guint count;

			count = 0;
			for (e = p[j]; e != NULL; e = e->next) {
				GnomeIconContainerIcon *icon;

				icon = e->data;
				if (icon->x >= x && icon->y >= y)
					count++;
			}

			if (count > 1)
				new_grid_width += count - 1;

			x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
		}

		new_grid_width = MAX (new_grid_width, line_width);
		p += grid->alloc_width;

		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
		x = 0;
	}

	/* Create the new grid.  */

	new_grid = icon_grid_new ();
	icon_grid_resize (new_grid, new_grid_width, grid->height);
        icon_grid_set_visible_width (new_grid, grid->visible_width);

	/* Allocate the icons in the new grid, one per cell.  */

	p = grid->elems;
	q = new_grid->elems;
	k = 0;
	x = y = dx = 0;
	for (i = 0; i < grid->height; i++) {
		m = 0;
		for (j = 0; j < grid->width; j++) {
			GList *e;
			guint count;

			/* Make sure the icons are sorted by increasing X
                           position.  */
			p[j] = g_list_sort
				(p[j], icon_grid_cell_compare_by_x);

			count = 0;
			for (e = p[j]; e != NULL; e = e->next) {
				GnomeIconContainerIcon *icon;

				icon = e->data;
					
				/* Make sure icons are not moved twice, and
				   ignore icons whose upper left corner is not
				   in this cell, unless the icon is partly
				   outside the container.  */
				if (icon->layout_done
				    || (icon->x >= 0 && icon->x < x)
				    || (icon->y >= 0 && icon->y < y))
					continue;

				icon_position (icon, container, dx, y);
				icon->layout_done = TRUE;

				q[k] = g_list_alloc ();
				q[k]->data = icon;
				dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);

				k++;

				if (count > 0)
					m++;

				count++;
			}

			if (count == 0) {
				if (m > 0) {
					m--;
				} else {
					k++;
					dx += GNOME_ICON_CONTAINER_CELL_WIDTH (container);
				}
			}
		}

		x += GNOME_ICON_CONTAINER_CELL_WIDTH (container);

		p += grid->alloc_width;

		q += new_grid->alloc_width;
		k = 0;

		y += GNOME_ICON_CONTAINER_CELL_HEIGHT (container);
		x = 0;

		dx = 0;
	}

	/* Done: use the new grid.  */

	icon_grid_destroy (priv->grid);
	priv->grid = new_grid;

	/* Update the keyboard selection indicator.  */
	if (priv->kbd_current != NULL)
		set_kbd_current (container, priv->kbd_current, FALSE);

	add_idle (container);
}


/**
 * gnome_icon_container_get_selection:
 * @container: An icon container.
 * 
 * Get a list of the icons currently selected in @container.
 * 
 * Return value: A GList of the programmer-specified data associated to each
 * selected icon, or NULL if no icon is selected.  The caller is expected to
 * free the list when it is not needed anymore.
 **/
GList *
gnome_icon_container_get_selection (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GList *list, *p;

	g_return_val_if_fail (container != NULL, FALSE);

	priv = container->priv;

	list = NULL;
	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (icon->is_selected)
			list = g_list_prepend (list, icon->data);
	}

	return list;
}

/**
 * gnome_icon_container_select_all:
 * @container: An icon container widget.
 * 
 * Select all the icons in @container at once.
 **/
void
gnome_icon_container_select_all (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	GnomeIconContainerIconGrid *grid;
	GList **p, *q;
	guint i, j;
	gboolean selection_changed;

	g_return_if_fail (container != NULL);

	priv = container->priv;
	grid = priv->grid;

	selection_changed = FALSE;
	p = grid->elems;
	for (i = 0; i < grid->height; i++) {
		for (j = 0; j < grid->width; j++) {
			for (q = p[j]; q != NULL; q =q->next) {
				if (select_icon (container, q->data, TRUE))
					selection_changed = TRUE;
			}
		}

		p += grid->alloc_width;
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_unselect_all:
 * @container: An icon container widget.
 * 
 * Deselect all the icons in @container.
 **/
void
gnome_icon_container_unselect_all (GnomeIconContainer *container)
{
	GnomeIconContainerPrivate *priv;
	gboolean selection_changed;
	GList *p;

	g_return_if_fail (container != NULL);

	priv = container->priv;

	selection_changed = FALSE;
	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (select_icon (container, icon, FALSE))
			selection_changed = TRUE;
	}

	if (selection_changed)
		gtk_signal_emit (GTK_OBJECT (container),
				 signals[SELECTION_CHANGED]);
}

/**
 * gnome_icon_container_set_base_uri:
 * @container: An icon container widget.
 * @base_uri: A base URI.
 * 
 * Set the base URI for drag & drop operations.
 **/
void
gnome_icon_container_set_base_uri (GnomeIconContainer *container,
				   const gchar *base_uri)
{
	GnomeIconContainerPrivate *priv;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	priv = container->priv;

	g_free (priv->base_uri);
	priv->base_uri = g_strdup (base_uri);
}

/**
 * gnome_icon_container_xlate_selected:
 * @container: An icon container widget.
 * @amount_x: Amount of translation on the X axis.
 * @amount_y: Amount of translation on the Y axis.
 * @raise: Whether icons should be raised during this operation.
 * 
 * Translate all the currently selected items in @container by @amount_x
 * horizontally and @amount_y vertically.  Positive values move to the
 * right/bottom, negative values to the left/top.
 **/
void
gnome_icon_container_xlate_selected (GnomeIconContainer *container,
				     gint amount_x,
				     gint amount_y,
				     gboolean raise)
{
	GnomeIconContainerPrivate *priv;
	GList *p;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GNOME_IS_ICON_CONTAINER (container));

	if (amount_x == 0 && amount_y == 0)
		return;

	priv = container->priv;

	for (p = priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		if (icon->is_selected) {
			move_icon (container, icon,
				   icon->x + amount_x, icon->y + amount_y);
			if (raise)
				icon_raise (icon);
		}
	}

	set_kbd_current (container, priv->kbd_current, TRUE);
}


GnomeIconContainerLayout *
gnome_icon_container_get_layout (GnomeIconContainer *container)
{
	GnomeIconContainerLayout *layout;
	GList *p;

	g_return_val_if_fail (container != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_ICON_CONTAINER (container), NULL);

	layout = gnome_icon_container_layout_new ();

	for (p = container->priv->icons; p != NULL; p = p->next) {
		GnomeIconContainerIcon *icon;

		icon = p->data;
		gnome_icon_container_layout_add (layout, icon->text,
						 icon->x, icon->y);
	}

	return layout;
}
