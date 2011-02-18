/*
 * gedit-overlay-child.c
 * This file is part of gedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gedit-overlay-child.h"

struct _GeditOverlayChildPrivate
{
	GtkWidget                *widget;
	GeditOverlayChildPosition position;
	guint                     offset;
	gboolean                  fixed;
};

enum
{
	PROP_0,
	PROP_WIDGET,
	PROP_POSITION,
	PROP_OFFSET,
	PROP_FIXED
};

G_DEFINE_TYPE (GeditOverlayChild, gedit_overlay_child, GTK_TYPE_BIN)

static void
gedit_overlay_child_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
			g_value_set_object (value, child->priv->widget);
			break;
		case PROP_POSITION:
			g_value_set_uint (value, child->priv->position);
			break;
		case PROP_OFFSET:
			g_value_set_uint (value, child->priv->offset);
			break;
		case PROP_FIXED:
			g_value_set_boolean (value, child->priv->fixed);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_child_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
			gtk_container_add (GTK_CONTAINER (child),
			                   g_value_get_object (value));
			break;
		case PROP_POSITION:
			child->priv->position = g_value_get_uint (value);
			break;
		case PROP_OFFSET:
			child->priv->offset = g_value_get_uint (value);
			break;
		case PROP_FIXED:
			child->priv->fixed = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_child_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	GdkWindow *parent_window;
	GdkWindow *window;
	GtkStyleContext *context;

	gtk_widget_set_realized (widget, TRUE);

	parent_window = gtk_widget_get_parent_window (widget);
	context = gtk_widget_get_style_context (widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = GDK_EXPOSURE_MASK;

	window = gdk_window_new (parent_window, &attributes, 0);
	gdk_window_set_user_data (window, widget);
	gtk_widget_set_window (widget, window);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
	gtk_style_context_set_background (context, window);
}

static void
gedit_overlay_child_get_preferred_width (GtkWidget *widget,
                                         gint      *minimum,
                                         gint      *natural)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (widget);
        gint child_min = 0, child_nat = 0;

	if (child->priv->widget != NULL)
	{

		gtk_widget_get_preferred_width (child->priv->widget,
		                                &child_min, &child_nat);
	}

	*minimum = child_min;
        *natural = child_nat;
}

static void
gedit_overlay_child_get_preferred_height (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (widget);
        gint child_min = 0, child_nat = 0;

	if (child->priv->widget != NULL)
        {

		gtk_widget_get_preferred_height (child->priv->widget,
		                                 &child_min, &child_nat);
	}

	*minimum = child_min;
        *natural = child_nat;
}

static void
gedit_overlay_child_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (widget);
        GtkAllocation tmp;

        tmp.width = allocation->width;
        tmp.height = allocation->height;
        tmp.x = 0;
        tmp.y = 0;

	GTK_WIDGET_CLASS (gedit_overlay_child_parent_class)->size_allocate (widget, allocation);

	if (child->priv->widget != NULL)
	{
		gtk_widget_size_allocate (child->priv->widget, &tmp);
	}
}

static void
gedit_overlay_child_add (GtkContainer *container,
                         GtkWidget    *widget)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (container);

	child->priv->widget = widget;

	GTK_CONTAINER_CLASS (gedit_overlay_child_parent_class)->add (container, widget);
}

static void
gedit_overlay_child_remove (GtkContainer *container,
                            GtkWidget    *widget)
{
	GeditOverlayChild *child = GEDIT_OVERLAY_CHILD (container);

	child->priv->widget = NULL;

	GTK_CONTAINER_CLASS (gedit_overlay_child_parent_class)->remove (container, widget);
}

static void
gedit_overlay_child_class_init (GeditOverlayChildClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	
	object_class->get_property = gedit_overlay_child_get_property;
	object_class->set_property = gedit_overlay_child_set_property;

	widget_class->realize = gedit_overlay_child_realize;
	widget_class->get_preferred_width = gedit_overlay_child_get_preferred_width;
	widget_class->get_preferred_height = gedit_overlay_child_get_preferred_height;
	widget_class->size_allocate = gedit_overlay_child_size_allocate;

	container_class->add = gedit_overlay_child_add;
	container_class->remove = gedit_overlay_child_remove;

	g_object_class_install_property (object_class, PROP_WIDGET,
	                                 g_param_spec_object ("widget",
	                                                      "Widget",
	                                                      "The Widget",
	                                                      GTK_TYPE_WIDGET,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_POSITION,
	                                 g_param_spec_uint ("position",
	                                                    "Position",
	                                                    "The Widget Position",
	                                                    1, GEDIT_OVERLAY_CHILD_POSITION_STATIC,
	                                                    GEDIT_OVERLAY_CHILD_POSITION_STATIC,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT |
	                                                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_OFFSET,
	                                 g_param_spec_uint ("offset",
	                                                    "Offset",
	                                                    "The Widget Offset",
	                                                    0,
	                                                    G_MAXUINT,
	                                                    0,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT |
	                                                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_FIXED,
	                                 g_param_spec_boolean ("fixed",
	                                                       "Fixed",
	                                                       "Wether the Widget is in a fixed position",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE |
	                                                       G_PARAM_CONSTRUCT |
	                                                       G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (object_class, sizeof (GeditOverlayChildPrivate));
}

static void
gedit_overlay_child_init (GeditOverlayChild *child)
{
	child->priv = G_TYPE_INSTANCE_GET_PRIVATE (child,
	                                           GEDIT_TYPE_OVERLAY_CHILD,
	                                           GeditOverlayChildPrivate);

	gtk_widget_set_has_window (GTK_WIDGET (child), TRUE);
}

/**
 * gedit_overlay_child_new:
 * @widget: a #GtkWidget
 *
 * Creates a new #GeditOverlayChild object
 *
 * Returns: a new #GeditOverlayChild object
 */
GeditOverlayChild *
gedit_overlay_child_new (GtkWidget *widget)
{
	return g_object_new (GEDIT_TYPE_OVERLAY_CHILD,
	                     "widget", widget,
	                     NULL);
}

/**
 * gedit_overlay_child_get_position:
 * @child: a #GeditOverlayChild
 *
 * Gets the position of the widget
 *
 * Returns: wether the child should be placed in a #GeditOverlay
 */
GeditOverlayChildPosition
gedit_overlay_child_get_position (GeditOverlayChild *child)
{
	g_return_val_if_fail (GEDIT_IS_OVERLAY_CHILD (child), GEDIT_OVERLAY_CHILD_POSITION_STATIC);

	return child->priv->position;
}

/**
 * gedit_overlay_child_set_position:
 * @child: a #GeditOverlayChild
 * @position: a #GeditOverlayChildPosition
 *
 * Sets the new position for @child
 */
void
gedit_overlay_child_set_position (GeditOverlayChild        *child,
                                  GeditOverlayChildPosition position)
{
	g_return_if_fail (GEDIT_IS_OVERLAY_CHILD (child));

	if (child->priv->position != position)
	{
		child->priv->position = position;

		g_object_notify (G_OBJECT (child), "position");
	}
}

/**
 * gedit_overlay_child_get_offset:
 * @child: a #GeditOverlayChild
 *
 * Gets the offset for @child. The offset is usually used by #GeditOverlay
 * to not place the widget directly in the border of the container
 *
 * Returns: the offset for @child
 */
guint
gedit_overlay_child_get_offset (GeditOverlayChild *child)
{
	g_return_val_if_fail (GEDIT_IS_OVERLAY_CHILD (child), 0);

	return child->priv->offset;
}

/**
 * gedit_overlay_child_set_offset:
 * @child: a #GeditOverlayChild
 * @offset: the offset for @child
 *
 * Sets the new offset for @child
 */
void
gedit_overlay_child_set_offset (GeditOverlayChild *child,
                                guint              offset)
{
	g_return_if_fail (GEDIT_IS_OVERLAY_CHILD (child));

	if (child->priv->offset != offset)
	{
		child->priv->offset = offset;

		g_object_notify (G_OBJECT (child), "offset");
	}
}

/**
 * gedit_overlay_child_get_fixed:
 * @child: a #GeditOverlayChild
 *
 * Gets wether @child is fixed in its position. If @child is not fixed the position
 * will change when for example you scroll the container.
 *
 * Returns: wether @child is fixed in its position
 */
gboolean
gedit_overlay_child_get_fixed (GeditOverlayChild *child)
{
	g_return_val_if_fail (GEDIT_IS_OVERLAY_CHILD (child), TRUE);

	return child->priv->fixed;
}

/**
 * gedit_overlay_child_set_fixed:
 * @child: a #GeditOverlayChild
 * @fixed: wether @child is in a fixed position
 *
 * Sets wether @child is in a fixed position
 */
void
gedit_overlay_child_set_fixed (GeditOverlayChild *child,
                               gboolean           fixed)
{
	g_return_if_fail (GEDIT_IS_OVERLAY_CHILD (child));

	fixed = (fixed != FALSE);

	if (child->priv->fixed != fixed)
	{
		child->priv->fixed = fixed;

		g_object_notify (G_OBJECT (child), "fixed");
	}
}

/* ex:set ts=8 noet: */
