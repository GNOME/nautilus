/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999, 2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <config.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkviewport.h>
#include "gtkscrollframe.h"


/* scrolled window policy and size requisition handling:
 *
 * gtk size requisition works as follows:
 *   a widget upon size-request reports the width and height that it finds
 *   to be best suited to display its contents, including children.
 *   the width and/or height reported from a widget upon size requisition
 *   may be overidden by the user by specifying a width and/or height
 *   other than 0 through gtk_widget_set_usize().
 *
 * a scrolled window needs (for imlementing all three policy types) to
 * request its width and height based on two different rationales.
 * 1)   the user wants the scrolled window to just fit into the space
 *      that it gets allocated for a specifc dimension.
 * 1.1) this does not apply if the user specified a concrete value
 *      value for that specific dimension by either specifying usize for the
 *      scrolled window or for its child.
 * 2)   the user wants the scrolled window to take as much space up as
 *      is desired by the child for a specifc dimension (i.e. POLICY_NEVER).
 *
 * also, kinda obvious:
 * 3)   a user would certainly not have choosen a scrolled window as a container
 *      for the child, if the resulting allocation takes up more space than the
 *      child would have allocated without the scrolled window.
 *
 * conclusions:
 * A) from 1) follows: the scrolled window shouldn't request more space for a
 *    specifc dimension than is required at minimum.
 * B) from 1.1) follows: the requisition may be overidden by usize of the scrolled
 *    window (done automatically) or by usize of the child (needs to be checked).
 * C) from 2) follows: for POLICY_NEVER, the scrolled window simply reports the
 *    child's dimension.
 * D) from 3) follows: the scrolled window child's minimum width and minimum height
 *    under A) at least correspond to the space taken up by its scrollbars.
 */

/* Object argument IDs */
enum {
	ARG_0,
	ARG_HADJUSTMENT,
	ARG_VADJUSTMENT,
	ARG_HSCROLLBAR_POLICY,
	ARG_VSCROLLBAR_POLICY,
	ARG_FRAME_PLACEMENT,
	ARG_SHADOW_TYPE,
	ARG_SCROLLBAR_SPACING
};

/* Private part of the GtkScrollFrame structure */
typedef struct {
	/* Horizontal and vertical scrollbars */
	GtkWidget *hsb;
	GtkWidget *vsb;

	/* Space between scrollbars and frame */
	guint sb_spacing;

	/* Allocation for frame */
	guint frame_x;
	guint frame_y;
	guint frame_w;
	guint frame_h;

	/* Scrollbar policy */
	guint hsb_policy : 2;
	guint vsb_policy : 2;

	/* Whether scrollbars are visible */
	guint hsb_visible : 1;
	guint vsb_visible : 1;

	/* Placement of frame wrt scrollbars */
	guint frame_placement : 2;

	/* Shadow type for frame */
	guint shadow_type : 3;
} ScrollFramePrivate;


static void gtk_scroll_frame_class_init (GtkScrollFrameClass *class);
static void gtk_scroll_frame_init (GtkScrollFrame *sf);
static void gtk_scroll_frame_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gtk_scroll_frame_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gtk_scroll_frame_destroy (GtkObject *object);
static void gtk_scroll_frame_finalize (GtkObject *object);

static void gtk_scroll_frame_map (GtkWidget *widget);
static void gtk_scroll_frame_unmap (GtkWidget *widget);
static void gtk_scroll_frame_draw (GtkWidget *widget, GdkRectangle *area);
static void gtk_scroll_frame_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_scroll_frame_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_scroll_frame_expose (GtkWidget *widget, GdkEventExpose *event);

static void gtk_scroll_frame_add (GtkContainer *container, GtkWidget *widget);
static void gtk_scroll_frame_remove (GtkContainer *container, GtkWidget *widget);
static void gtk_scroll_frame_forall (GtkContainer *container, gboolean include_internals,
				     GtkCallback callback, gpointer callback_data);

static GtkBinClass *parent_class;


/**
 * gtk_scroll_frame_get_type:
 * @void:
 *
 * Registers the &GtkScrollFrame class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GtkScrollFrame class.
 **/
GtkType
gtk_scroll_frame_get_type (void)
{
	static GtkType scroll_frame_type = 0;

	if (!scroll_frame_type) {
		static const GtkTypeInfo scroll_frame_info = {
			"GtkScrollFrame",
			sizeof (GtkScrollFrame),
			sizeof (GtkScrollFrameClass),
			(GtkClassInitFunc) gtk_scroll_frame_class_init,
			(GtkObjectInitFunc) gtk_scroll_frame_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		scroll_frame_type = gtk_type_unique (GTK_TYPE_BIN, &scroll_frame_info);
	}

	return scroll_frame_type;
}

/* Class initialization function for the scroll frame widget */
static void
gtk_scroll_frame_class_init (GtkScrollFrameClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_BIN);

	gtk_object_add_arg_type ("GtkScrollFrame::hadjustment",
				 GTK_TYPE_ADJUSTMENT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
				 ARG_HADJUSTMENT);
	gtk_object_add_arg_type ("GtkScrollFrame::vadjustment",
				 GTK_TYPE_ADJUSTMENT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
				 ARG_VADJUSTMENT);
	gtk_object_add_arg_type ("GtkScrollFrame::hscrollbar_policy",
				 GTK_TYPE_POLICY_TYPE,
				 GTK_ARG_READWRITE,
				 ARG_HSCROLLBAR_POLICY);
	gtk_object_add_arg_type ("GtkScrollFrame::vscrollbar_policy",
				 GTK_TYPE_POLICY_TYPE,
				 GTK_ARG_READWRITE,
				 ARG_VSCROLLBAR_POLICY);
	gtk_object_add_arg_type ("GtkScrollFrame::frame_placement",
				 GTK_TYPE_CORNER_TYPE,
				 GTK_ARG_READWRITE,
				 ARG_FRAME_PLACEMENT);
	gtk_object_add_arg_type ("GtkScrollFrame::shadow_type",
				 GTK_TYPE_SHADOW_TYPE,
				 GTK_ARG_READWRITE,
				 ARG_SHADOW_TYPE);
	gtk_object_add_arg_type ("GtkScrollFrame::scrollbar_spacing",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SCROLLBAR_SPACING);

	object_class->set_arg = gtk_scroll_frame_set_arg;
	object_class->get_arg = gtk_scroll_frame_get_arg;
	object_class->destroy = gtk_scroll_frame_destroy;
	object_class->finalize = gtk_scroll_frame_finalize;

	widget_class->map = gtk_scroll_frame_map;
	widget_class->unmap = gtk_scroll_frame_unmap;
	widget_class->draw = gtk_scroll_frame_draw;
	widget_class->size_request = gtk_scroll_frame_size_request;
	widget_class->size_allocate = gtk_scroll_frame_size_allocate;
	widget_class->expose_event = gtk_scroll_frame_expose;

	container_class->add = gtk_scroll_frame_add;
	container_class->remove = gtk_scroll_frame_remove;
	container_class->forall = gtk_scroll_frame_forall;
}

/* Object initialization function for the scroll frame widget */
static void
gtk_scroll_frame_init (GtkScrollFrame *sf)
{
	ScrollFramePrivate *priv;

	priv = g_new0 (ScrollFramePrivate, 1);
	sf->priv = priv;

	GTK_WIDGET_SET_FLAGS (sf, GTK_NO_WINDOW);

	gtk_container_set_resize_mode (GTK_CONTAINER (sf), GTK_RESIZE_QUEUE);

	priv->sb_spacing = 3;
	priv->hsb_policy = GTK_POLICY_ALWAYS;
	priv->vsb_policy = GTK_POLICY_ALWAYS;
	priv->frame_placement = GTK_CORNER_TOP_LEFT;
	priv->shadow_type = GTK_SHADOW_NONE;
}

/* Set_arg handler for the scroll frame widget */
static void
gtk_scroll_frame_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	sf = GTK_SCROLL_FRAME (object);
	priv = sf->priv;

	switch (arg_id) {
	case ARG_HADJUSTMENT:
		gtk_scroll_frame_set_hadjustment (sf, GTK_VALUE_POINTER (*arg));
		break;

	case ARG_VADJUSTMENT:
		gtk_scroll_frame_set_vadjustment (sf, GTK_VALUE_POINTER (*arg));
		break;

	case ARG_HSCROLLBAR_POLICY:
		gtk_scroll_frame_set_policy (sf, GTK_VALUE_ENUM (*arg), priv->vsb_policy);
		break;

	case ARG_VSCROLLBAR_POLICY:
		gtk_scroll_frame_set_policy (sf, priv->hsb_policy, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_FRAME_PLACEMENT:
		gtk_scroll_frame_set_placement (sf, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_SHADOW_TYPE:
		gtk_scroll_frame_set_shadow_type (sf, GTK_VALUE_ENUM (*arg));
		break;

	case ARG_SCROLLBAR_SPACING:
		gtk_scroll_frame_set_scrollbar_spacing (sf, GTK_VALUE_UINT (*arg));
		break;

	default:
		break;
	}
}

/* Get_arg handler for the scroll frame widget */
static void
gtk_scroll_frame_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	sf = GTK_SCROLL_FRAME (object);
	priv = sf->priv;

	switch (arg_id) {
	case ARG_HADJUSTMENT:
		GTK_VALUE_POINTER (*arg) = gtk_scroll_frame_get_hadjustment (sf);
		break;

	case ARG_VADJUSTMENT:
		GTK_VALUE_POINTER (*arg) = gtk_scroll_frame_get_vadjustment (sf);
		break;

	case ARG_HSCROLLBAR_POLICY:
		GTK_VALUE_ENUM (*arg) = priv->hsb_policy;
		break;

	case ARG_VSCROLLBAR_POLICY:
		GTK_VALUE_ENUM (*arg) = priv->vsb_policy;
		break;

	case ARG_FRAME_PLACEMENT:
		GTK_VALUE_ENUM (*arg) = priv->frame_placement;
		break;

	case ARG_SHADOW_TYPE:
		GTK_VALUE_ENUM (*arg) = priv->shadow_type;
		break;

	case ARG_SCROLLBAR_SPACING:
		GTK_VALUE_UINT (*arg) = priv->sb_spacing;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/* Destroy handler for the scroll frame widget */
static void
gtk_scroll_frame_destroy (GtkObject *object)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (object));

	sf = GTK_SCROLL_FRAME (object);
	priv = sf->priv;

	gtk_widget_unparent (priv->hsb);
	gtk_widget_unparent (priv->vsb);
	gtk_widget_destroy (priv->hsb);
	gtk_widget_destroy (priv->vsb);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Finalize handler for the scroll frame widget */
static void
gtk_scroll_frame_finalize (GtkObject *object)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	sf = GTK_SCROLL_FRAME (object);
	priv = sf->priv;

	gtk_widget_unref (priv->hsb);
	gtk_widget_unref (priv->vsb);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Map handler for the scroll frame widget */
static void
gtk_scroll_frame_map (GtkWidget *widget)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (widget));

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;

	/* chain parent class handler to map self and child */
	if (GTK_WIDGET_CLASS (parent_class)->map)
		(* GTK_WIDGET_CLASS (parent_class)->map) (widget);

	if (GTK_WIDGET_VISIBLE (priv->hsb) && !GTK_WIDGET_MAPPED (priv->hsb))
		gtk_widget_map (priv->hsb);

	if (GTK_WIDGET_VISIBLE (priv->vsb) && !GTK_WIDGET_MAPPED (priv->vsb))
		gtk_widget_map (priv->vsb);
}

/* Unmap handler for the scroll frame widget */
static void
gtk_scroll_frame_unmap (GtkWidget *widget)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (widget));

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;

	/* chain parent class handler to unmap self and child */
	if (GTK_WIDGET_CLASS (parent_class)->unmap)
		(* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);

	if (GTK_WIDGET_MAPPED (priv->hsb))
		gtk_widget_unmap (priv->hsb);

	if (GTK_WIDGET_MAPPED (priv->vsb))
		gtk_widget_unmap (priv->vsb);
}

/* Draws the shadow of a scroll frame widget */
static void
draw_shadow (GtkScrollFrame *sf, GdkRectangle *area)
{
	ScrollFramePrivate *priv;

	g_assert (area != NULL);

	priv = sf->priv;

	gtk_paint_shadow (GTK_WIDGET (sf)->style,
			  GTK_WIDGET (sf)->window,
			  GTK_STATE_NORMAL, priv->shadow_type,
			  area, GTK_WIDGET (sf),
			  "scroll_frame",
			  priv->frame_x, priv->frame_y,
			  priv->frame_w, priv->frame_h);
}

/* Draw handler for the scroll frame widget */
static void
gtk_scroll_frame_draw (GtkWidget *widget, GdkRectangle *area)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;
	GtkBin *bin;
	GdkRectangle child_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (widget));
	g_return_if_fail (area != NULL);

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;
	bin = GTK_BIN (widget);

	if (GTK_WIDGET_DRAWABLE (widget))
		draw_shadow (sf, area);

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)
	    && gtk_widget_intersect (bin->child, area, &child_area))
		gtk_widget_draw (bin->child, &child_area);

	if (GTK_WIDGET_VISIBLE (priv->hsb)
	    && gtk_widget_intersect (priv->hsb, area, &child_area))
		gtk_widget_draw (priv->hsb, &child_area);

	if (GTK_WIDGET_VISIBLE (priv->vsb)
	    && gtk_widget_intersect (priv->vsb, area, &child_area))
		gtk_widget_draw (priv->vsb, &child_area);
}

/* Forall handler for the scroll frame widget */
static void
gtk_scroll_frame_forall (GtkContainer *container, gboolean include_internals,
			 GtkCallback callback, gpointer callback_data)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_return_if_fail (container != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (container));
	g_return_if_fail (callback != NULL);

	sf = GTK_SCROLL_FRAME (container);
	priv = sf->priv;

	if (GTK_CONTAINER_CLASS (parent_class)->forall)
		(* GTK_CONTAINER_CLASS (parent_class)->forall) (
			container, include_internals,
			callback, callback_data);

	if (include_internals) {
		if (priv->vsb)
			(* callback) (priv->vsb, callback_data);

		if (priv->hsb)
			(* callback) (priv->hsb, callback_data);
	}
}

/* Size_request handler for the scroll frame widget */
static void
gtk_scroll_frame_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;
	GtkBin *bin;
	gint extra_width;
	gint extra_height;
	GtkRequisition hsb_requisition;
	GtkRequisition vsb_requisition;
	GtkRequisition child_requisition;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (widget));
	g_return_if_fail (requisition != NULL);

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;
	bin = GTK_BIN (widget);

	extra_width = 0;
	extra_height = 0;

	requisition->width = GTK_CONTAINER (widget)->border_width * 2;
	requisition->height = GTK_CONTAINER (widget)->border_width * 2;

	if (priv->shadow_type != GTK_SHADOW_NONE) {
		requisition->width += 2 * widget->style->klass->xthickness;
		requisition->height += 2 * widget->style->klass->ythickness;
	}

	gtk_widget_size_request (priv->hsb, &hsb_requisition);
	gtk_widget_size_request (priv->vsb, &vsb_requisition);

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
		static guint quark_aux_info;

		if (!quark_aux_info)
			quark_aux_info = g_quark_from_static_string ("gtk-aux-info");

		gtk_widget_size_request (bin->child, &child_requisition);

		if (priv->hsb_policy == GTK_POLICY_NEVER)
			requisition->width += child_requisition.width;
		else {
			GtkWidgetAuxInfo *aux_info;

			aux_info = gtk_object_get_data_by_id (GTK_OBJECT (bin->child),
							      quark_aux_info);
			if (aux_info && aux_info->width > 0) {
				requisition->width += aux_info->width;
				extra_width = -1;
			} else
				requisition->width += vsb_requisition.width;
		}

		if (priv->vsb_policy == GTK_POLICY_NEVER)
			requisition->height += child_requisition.height;
		else {
			GtkWidgetAuxInfo *aux_info;

			aux_info = gtk_object_get_data_by_id (GTK_OBJECT (bin->child),
							      quark_aux_info);
			if (aux_info && aux_info->height > 0) {
				requisition->height += aux_info->height;
				extra_height = -1;
			} else
				requisition->height += hsb_requisition.height;
		}
	}

	if (priv->hsb_policy == GTK_POLICY_AUTOMATIC || GTK_WIDGET_VISIBLE (priv->hsb)) {
		requisition->width = MAX (requisition->width, hsb_requisition.width);
		if (!extra_height || GTK_WIDGET_VISIBLE (priv->hsb))
			extra_height = priv->sb_spacing + hsb_requisition.height;
	}

	if (priv->vsb_policy == GTK_POLICY_AUTOMATIC || GTK_WIDGET_VISIBLE (priv->vsb)) {
		requisition->height = MAX (requisition->height, vsb_requisition.height);
		if (!extra_width || GTK_WIDGET_VISIBLE (priv->vsb))
			extra_width = priv->sb_spacing + vsb_requisition.width;
	}

	requisition->width += MAX (0, extra_width);
	requisition->height += MAX (0, extra_height);
}

/* Computes the relative allocation for the scroll frame widget */
static void
compute_relative_allocation (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_assert (widget != NULL);
	g_assert (GTK_IS_SCROLL_FRAME (widget));
	g_assert (allocation != NULL);

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;

	allocation->x = GTK_CONTAINER (widget)->border_width;
	allocation->y = GTK_CONTAINER (widget)->border_width;
	allocation->width = MAX (1, (gint) widget->allocation.width - allocation->x * 2);
	allocation->height = MAX (1, (gint) widget->allocation.height - allocation->y * 2);

	if (priv->vsb_visible) {
		GtkRequisition vsb_requisition;
		gint possible_new_width;

		gtk_widget_get_child_requisition (priv->vsb, &vsb_requisition);

		if (priv->frame_placement == GTK_CORNER_TOP_RIGHT
		    || priv->frame_placement == GTK_CORNER_BOTTOM_RIGHT)
			allocation->x += vsb_requisition.width + priv->sb_spacing;

		possible_new_width = ((gint) allocation->width
				      - ((gint) vsb_requisition.width + priv->sb_spacing));
		allocation->width = MAX (0, possible_new_width);
	}

	if (priv->hsb_visible) {
		GtkRequisition hsb_requisition;
		gint possible_new_height;

		gtk_widget_get_child_requisition (priv->hsb, &hsb_requisition);

		if (priv->frame_placement == GTK_CORNER_BOTTOM_LEFT
		    || priv->frame_placement == GTK_CORNER_BOTTOM_RIGHT)
			allocation->y += hsb_requisition.height + priv->sb_spacing;

		possible_new_height = 
			(   ((gint)allocation->height)
			    - ((gint)hsb_requisition.height) + ((gint)priv->sb_spacing));
		allocation->height = MAX (0, possible_new_height);
	}
}

/* Size_allocate handler for the scroll frame widget */
static void
gtk_scroll_frame_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;
	GtkBin *bin;
	GtkAllocation relative_allocation;
	GtkAllocation child_allocation;
	gint xthickness, ythickness;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (widget));
	g_return_if_fail (allocation != NULL);

	sf = GTK_SCROLL_FRAME (widget);
	priv = sf->priv;
	bin = GTK_BIN (widget);

	widget->allocation = *allocation;

	if (priv->hsb_policy == GTK_POLICY_ALWAYS)
		priv->hsb_visible = TRUE;
	else if (priv->hsb_policy == GTK_POLICY_NEVER)
		priv->hsb_visible = FALSE;

	if (priv->vsb_policy == GTK_POLICY_ALWAYS)
		priv->vsb_visible = TRUE;
	else if (priv->vsb_policy == GTK_POLICY_NEVER)
		priv->vsb_visible = FALSE;

	if (priv->shadow_type == GTK_SHADOW_NONE) {
		xthickness = 0;
		ythickness = 0;
	} else {
		xthickness = widget->style->klass->xthickness;
		ythickness = widget->style->klass->ythickness;
	}

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
		gboolean previous_hvis;
		gboolean previous_vvis;
		guint count = 0;

		do {
			gint16 possible_new_size;

			compute_relative_allocation (widget, &relative_allocation);

			priv->frame_x = relative_allocation.x + allocation->x;
			priv->frame_y = relative_allocation.y + allocation->y;
			priv->frame_w = relative_allocation.width;
			priv->frame_h = relative_allocation.height;

			child_allocation.x = priv->frame_x + xthickness;
			child_allocation.y = priv->frame_y + ythickness;
			possible_new_size = priv->frame_w - 2 * xthickness;
			child_allocation.width = MAX(1, possible_new_size);
			possible_new_size = priv->frame_h - 2 * ythickness;
			child_allocation.height = MAX(1, possible_new_size);

			previous_hvis = priv->hsb_visible;
			previous_vvis = priv->vsb_visible;

			gtk_widget_size_allocate (bin->child, &child_allocation);

			/* If, after the first iteration, the hscrollbar and the
			 * vscrollbar flip visiblity, then we need both.
			 */
			if (count
			    && previous_hvis != priv->hsb_visible
			    && previous_vvis != priv->vsb_visible) {
				priv->hsb_visible = TRUE;
				priv->vsb_visible = TRUE;

				/* a new resize is already queued at this point,
				 * so we will immediatedly get reinvoked
				 */
				return;
			}

			count++;
		} while (previous_hvis != priv->hsb_visible
			 || previous_vvis != priv->vsb_visible);
	} else
		compute_relative_allocation (widget, &relative_allocation);

	if (priv->hsb_visible) {
		GtkRequisition hscrollbar_requisition;

		gtk_widget_get_child_requisition (priv->hsb, &hscrollbar_requisition);

		if (!GTK_WIDGET_VISIBLE (priv->hsb))
			gtk_widget_show (priv->hsb);

		child_allocation.x = relative_allocation.x;
		if (priv->frame_placement == GTK_CORNER_TOP_LEFT
		    || priv->frame_placement == GTK_CORNER_TOP_RIGHT)
			child_allocation.y = (relative_allocation.y
#if 0
					      + relative_allocation.height
					      + priv->sb_spacing);
#else
		+ relative_allocation.height);
#endif
		else
			child_allocation.y = GTK_CONTAINER (sf)->border_width;

		child_allocation.width = relative_allocation.width;
		child_allocation.height = hscrollbar_requisition.height;
		child_allocation.x += allocation->x;
		child_allocation.y += allocation->y;

		gtk_widget_size_allocate (priv->hsb, &child_allocation);
	} else if (GTK_WIDGET_VISIBLE (priv->hsb))
		gtk_widget_hide (priv->hsb);

	if (priv->vsb_visible) {
		GtkRequisition vscrollbar_requisition;

		if (!GTK_WIDGET_VISIBLE (priv->vsb))
			gtk_widget_show (priv->vsb);

		gtk_widget_get_child_requisition (priv->vsb, &vscrollbar_requisition);

		if (priv->frame_placement == GTK_CORNER_TOP_LEFT
		    || priv->frame_placement == GTK_CORNER_BOTTOM_LEFT)
			child_allocation.x = (relative_allocation.x
					      + relative_allocation.width
					      + priv->sb_spacing);
		else
			child_allocation.x = GTK_CONTAINER (sf)->border_width;

		child_allocation.y = relative_allocation.y;
		child_allocation.width = vscrollbar_requisition.width;
		child_allocation.height = relative_allocation.height;
		child_allocation.x += allocation->x;
		child_allocation.y += allocation->y;

		gtk_widget_size_allocate (priv->vsb, &child_allocation);
	} else if (GTK_WIDGET_VISIBLE (priv->vsb))
		gtk_widget_hide (priv->vsb);
}

/* Expose handler for the scroll frame widget */
static gint
gtk_scroll_frame_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkScrollFrame *sf;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SCROLL_FRAME (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	sf = GTK_SCROLL_FRAME (widget);

	if (GTK_WIDGET_DRAWABLE (widget))
		draw_shadow (sf, &event->area);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

	return FALSE;
}

/* Add handler for the scroll frame widget */
static void
gtk_scroll_frame_add (GtkContainer *container, GtkWidget *child)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;
	GtkBin *bin;

	sf = GTK_SCROLL_FRAME (container);
	priv = sf->priv;
	bin = GTK_BIN (container);
	g_return_if_fail (bin->child == NULL);

	bin->child = child;
	gtk_widget_set_parent (child, GTK_WIDGET (bin));

	/* this is a temporary message */
	if (!gtk_widget_set_scroll_adjustments (child,
						gtk_range_get_adjustment (GTK_RANGE (priv->hsb)),
						gtk_range_get_adjustment (GTK_RANGE (priv->vsb))))
		g_warning ("gtk_scroll_frame_add(): cannot add non scrollable widget "
			   "use gtk_scroll_frame_add_with_viewport() instead");

	if (GTK_WIDGET_REALIZED (child->parent))
		gtk_widget_realize (child);

	if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (child->parent))
			gtk_widget_map (child);

		gtk_widget_queue_resize (child);
	}
}

/* Remove method for the scroll frame widget */
static void
gtk_scroll_frame_remove (GtkContainer *container, GtkWidget *child)
{
	g_return_if_fail (container != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (container));
	g_return_if_fail (child != NULL);
	g_return_if_fail (GTK_BIN (container)->child == child);

	gtk_widget_set_scroll_adjustments (child, NULL, NULL);

	/* chain parent class handler to remove child */
	if (GTK_CONTAINER_CLASS (parent_class)->remove)
		(* GTK_CONTAINER_CLASS (parent_class)->remove) (container, child);
}

/**
 * gtk_scroll_frame_new:
 * @hadj: If non-NULL, the adjustment to use for horizontal scrolling.
 * @vadj: If non-NULL, the adjustment to use for vertical scrolling.
 *
 * Creates a new scroll frame widget.
 *
 * Return value: The newly-created scroll frame widget.
 **/
GtkWidget *
gtk_scroll_frame_new (GtkAdjustment *hadj, GtkAdjustment *vadj)
{
	if (hadj)
		g_return_val_if_fail (GTK_IS_ADJUSTMENT (hadj), NULL);

	if (vadj)
		g_return_val_if_fail (GTK_IS_ADJUSTMENT (vadj), NULL);

	return gtk_widget_new (GTK_TYPE_SCROLL_FRAME,
			       "hadjustment", hadj,
			       "vadjustment", vadj,
			       NULL);
}

/* Callback used when one of the scroll frame widget's adjustments changes */
static void
adjustment_changed (GtkAdjustment *adj, gpointer data)
{
	GtkScrollFrame *sf;
	ScrollFramePrivate *priv;

	g_return_if_fail (adj != NULL);
	g_return_if_fail (GTK_IS_ADJUSTMENT (adj));
	g_return_if_fail (data != NULL);

	sf = GTK_SCROLL_FRAME (data);
	priv = sf->priv;

	if (adj == gtk_range_get_adjustment (GTK_RANGE (priv->hsb))) {
		if (priv->hsb_policy == GTK_POLICY_AUTOMATIC) {
			gboolean visible;

			visible = priv->hsb_visible;
			priv->hsb_visible = (adj->upper - adj->lower > adj->page_size);
			if (priv->hsb_visible != visible)
				gtk_widget_queue_resize (GTK_WIDGET (sf));
		}
	} else if (adj == gtk_range_get_adjustment (GTK_RANGE (priv->vsb))) {
		if (priv->vsb_policy == GTK_POLICY_AUTOMATIC) {
			gboolean visible;

			visible = priv->vsb_visible;
			priv->vsb_visible = (adj->upper - adj->lower > adj->page_size);
			if (priv->vsb_visible != visible)
				gtk_widget_queue_resize (GTK_WIDGET (sf));
		}
	}
}

/**
 * gtk_scroll_frame_set_hadjustment:
 * @sf: A scroll frame widget.
 * @adj: An adjustment.
 *
 * Sets the adjustment to be used for horizontal scrolling in a scroll frame
 * widget.
 **/
void
gtk_scroll_frame_set_hadjustment (GtkScrollFrame *sf, GtkAdjustment *adj)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));

	priv = sf->priv;

	if (adj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (adj));
	else
		adj = GTK_ADJUSTMENT (gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL));

	if (!priv->hsb) {
		gtk_widget_push_composite_child ();
		priv->hsb = gtk_hscrollbar_new (adj);
		gtk_widget_set_composite_name (priv->hsb, "hscrollbar");
		gtk_widget_pop_composite_child ();

		gtk_widget_set_parent (priv->hsb, GTK_WIDGET (sf));
		gtk_widget_ref (priv->hsb);
		gtk_widget_show (priv->hsb);
	} else {
		GtkAdjustment *old_adj;

		old_adj = gtk_range_get_adjustment (GTK_RANGE (priv->hsb));
		if (old_adj == adj)
			return;

		gtk_signal_disconnect_by_func (GTK_OBJECT (old_adj),
					       GTK_SIGNAL_FUNC (adjustment_changed),
					       sf);
		gtk_range_set_adjustment (GTK_RANGE (priv->hsb), adj);
	}

	adj = gtk_range_get_adjustment (GTK_RANGE (priv->hsb));
	gtk_signal_connect (GTK_OBJECT (adj),
			    "changed",
			    GTK_SIGNAL_FUNC (adjustment_changed),
			    sf);
	adjustment_changed (adj, sf);

	if (GTK_BIN (sf)->child)
		gtk_widget_set_scroll_adjustments (
			GTK_BIN (sf)->child,
			gtk_range_get_adjustment (GTK_RANGE (priv->hsb)),
			gtk_range_get_adjustment (GTK_RANGE (priv->vsb)));
}

/**
 * gtk_scroll_frame_set_vadjustment:
 * @sf: A scroll frame widget.
 * @adj: An adjustment.
 *
 * Sets the adjustment to be used for vertical scrolling in a scroll frame
 * widget.
 **/
void
gtk_scroll_frame_set_vadjustment (GtkScrollFrame *sf, GtkAdjustment *adj)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));

	priv = sf->priv;

	if (adj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (adj));
	else
		adj = GTK_ADJUSTMENT (gtk_object_new (GTK_TYPE_ADJUSTMENT, NULL));

	if (!priv->vsb) {
		gtk_widget_push_composite_child ();
		priv->vsb = gtk_vscrollbar_new (adj);
		gtk_widget_set_composite_name (priv->vsb, "vscrollbar");
		gtk_widget_pop_composite_child ();

		gtk_widget_set_parent (priv->vsb, GTK_WIDGET (sf));
		gtk_widget_ref (priv->vsb);
		gtk_widget_show (priv->vsb);
	} else {
		GtkAdjustment *old_adj;

		old_adj = gtk_range_get_adjustment (GTK_RANGE (priv->vsb));
		if (old_adj == adj)
			return;

		gtk_signal_disconnect_by_func (GTK_OBJECT (old_adj),
					       GTK_SIGNAL_FUNC (adjustment_changed),
					       sf);
		gtk_range_set_adjustment (GTK_RANGE (priv->vsb), adj);
	}

	adj = gtk_range_get_adjustment (GTK_RANGE (priv->vsb));
	gtk_signal_connect (GTK_OBJECT (adj),
			    "changed",
			    GTK_SIGNAL_FUNC (adjustment_changed),
			    sf);
	adjustment_changed (adj, sf);

	if (GTK_BIN (sf)->child)
		gtk_widget_set_scroll_adjustments (
			GTK_BIN (sf)->child,
			gtk_range_get_adjustment (GTK_RANGE (priv->hsb)),
			gtk_range_get_adjustment (GTK_RANGE (priv->vsb)));
}

/**
 * gtk_scroll_frame_get_hadjustment:
 * @sf: A scroll frame widget.
 *
 * Queries the horizontal adjustment of a scroll frame widget.
 *
 * Return value: The horizontal adjustment of the scroll frame, or NULL if none.
 **/
GtkAdjustment *
gtk_scroll_frame_get_hadjustment (GtkScrollFrame *sf)
{
	ScrollFramePrivate *priv;

	g_return_val_if_fail (sf != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SCROLL_FRAME (sf), NULL);

	priv = sf->priv;

	return priv->hsb ? gtk_range_get_adjustment (GTK_RANGE (priv->hsb)) : NULL;
}

/**
 * gtk_scroll_frame_get_vadjustment:
 * @sf: A scroll frame widget.
 *
 * Queries the vertical adjustment of a scroll frame widget.
 *
 * Return value: The vertical adjustment of the scroll frame, or NULL if none.
 **/
GtkAdjustment *
gtk_scroll_frame_get_vadjustment (GtkScrollFrame *sf)
{
	ScrollFramePrivate *priv;

	g_return_val_if_fail (sf != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SCROLL_FRAME (sf), NULL);

	priv = sf->priv;

	return priv->vsb ? gtk_range_get_adjustment (GTK_RANGE (priv->vsb)) : NULL;
}

/**
 * gtk_scroll_frame_set_policy:
 * @sf: A scroll frame widget.
 * @hsb_policy: Policy for the horizontal scrollbar.
 * @vsb_policy: Policy for the vertical scrollbar.
 *
 * Sets the scrollbar policies of a scroll frame widget.  These determine when
 * the scrollbars are to be shown or hidden.
 **/
void
gtk_scroll_frame_set_policy (GtkScrollFrame *sf,
			     GtkPolicyType hsb_policy,
			     GtkPolicyType vsb_policy)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));

	priv = sf->priv;

	if (priv->hsb_policy == hsb_policy && priv->vsb_policy == vsb_policy)
		return;

	priv->hsb_policy = hsb_policy;
	priv->vsb_policy = vsb_policy;

	gtk_widget_queue_resize (GTK_WIDGET (sf));
}

/**
 * gtk_scroll_frame_set_placement:
 * @sf: A scroll frame widget.
 * @frame_placement: Placement for the frame.
 *
 * Sets the placement of a scroll frame widget's frame with respect to its
 * scrollbars.
 **/
void
gtk_scroll_frame_set_placement (GtkScrollFrame *sf, GtkCornerType frame_placement)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));

	priv = sf->priv;

	if (priv->frame_placement == frame_placement)
		return;

	priv->frame_placement = frame_placement;
	gtk_widget_queue_resize (GTK_WIDGET (sf));
}

/**
 * gtk_scroll_frame_set_shadow_type:
 * @sf: A scroll frame widget.
 * @shadow_type: A shadow type.
 *
 * Sets the shadow type of a scroll frame widget.  You can use this when you
 * insert a child that does not paint a frame on its own.
 **/
void
gtk_scroll_frame_set_shadow_type (GtkScrollFrame *sf, GtkShadowType shadow_type)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));
	g_return_if_fail (shadow_type <= GTK_SHADOW_ETCHED_OUT);

	priv = sf->priv;

	if (priv->shadow_type == shadow_type)
		return;

	priv->shadow_type = shadow_type;
	gtk_widget_queue_resize (GTK_WIDGET (sf));
}

/**
 * gtk_scroll_frame_set_scrollbar_spacing:
 * @sf: A scroll frame widget.
 * @spacing: Desired spacing in pixels.
 *
 * Sets the spacing between the frame and the scrollbars of a scroll frame
 * widget.
 **/
void
gtk_scroll_frame_set_scrollbar_spacing (GtkScrollFrame *sf, guint spacing)
{
	ScrollFramePrivate *priv;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));

	priv = sf->priv;

	if (priv->sb_spacing == spacing)
		return;

	priv->sb_spacing = spacing;
	gtk_widget_queue_resize (GTK_WIDGET (sf));
}

/**
 * gtk_scroll_frame_add_with_viewport:
 * @sf: A scroll frame widget.
 * @child: A widget.
 *
 * Creates a &GtkViewport and puts the specified child inside it, thus allowing
 * the viewport to be scrolled by the scroll frame widget.  This is meant to be
 * used only when a child does not support the scrolling interface.
 **/
void
gtk_scroll_frame_add_with_viewport (GtkScrollFrame *sf, GtkWidget *child)
{
	ScrollFramePrivate *priv;
	GtkBin *bin;
	GtkWidget *viewport;

	g_return_if_fail (sf != NULL);
	g_return_if_fail (GTK_IS_SCROLL_FRAME (sf));
	g_return_if_fail (child != NULL);
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (child->parent == NULL);

	priv = sf->priv;
	bin = GTK_BIN (sf);

	if (bin->child != NULL) {
		g_return_if_fail (GTK_IS_VIEWPORT (bin->child));
		g_return_if_fail (GTK_BIN (bin->child)->child == NULL);

		viewport = bin->child;
	} else {
		viewport = gtk_viewport_new (gtk_scroll_frame_get_hadjustment (sf),
					     gtk_scroll_frame_get_vadjustment (sf));
		gtk_container_add (GTK_CONTAINER (sf), viewport);
	}

	gtk_widget_show (viewport);
	gtk_container_add (GTK_CONTAINER (viewport), child);
}
