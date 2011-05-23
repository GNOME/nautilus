/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
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
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-floating-bar.h"

struct _NautilusFloatingBarDetails {
	gchar *label;

	GtkWidget *label_widget;
	GtkWidget *spinner;
	gboolean show_spinner;

	guint escaping_distance;
	GtkAllocation initial_allocation;
	GdkRectangle escaping_area;
};

enum {
	PROP_LABEL = 1,
	PROP_SHOW_SPINNER,
	PROP_ESCAPING_DISTANCE,
	NUM_PROPERTIES
};

enum {
	ACTION,
	NUM_SIGNALS
};

#define NAUTILUS_FLOATING_BAR_ESCAPING_DISTANCE 20

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GEDIT_TYPE_OVERLAY_CHILD);

static void
action_button_clicked_cb (GtkButton *button,
			  NautilusFloatingBar *self)
{
	gint action_id;

	action_id = GPOINTER_TO_INT
		(g_object_get_data (G_OBJECT (button), "action-id"));
	
	g_signal_emit (self, signals[ACTION], 0, action_id);
}

static void
nautilus_floating_bar_finalize (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);

	g_free (self->priv->label);

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->finalize (obj);
}

static void
nautilus_floating_bar_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_LABEL:
		g_value_set_string (value, self->priv->label);
		break;
	case PROP_SHOW_SPINNER:
		g_value_set_boolean (value, self->priv->show_spinner);
		break;
	case PROP_ESCAPING_DISTANCE:
		g_value_set_uint (value, self->priv->escaping_distance);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_floating_bar_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_LABEL:
		nautilus_floating_bar_set_label (self, g_value_get_string (value));
		break;
	case PROP_SHOW_SPINNER:
		nautilus_floating_bar_set_show_spinner (self, g_value_get_boolean (value));
		break;
	case PROP_ESCAPING_DISTANCE:
		self->priv->escaping_distance = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
update_label (NautilusFloatingBar *self)
{
	gtk_label_set_text (GTK_LABEL (self->priv->label_widget), self->priv->label);
}

/* this is adapted from Epiphany:
 * lib/widgets/ephy-overlay-escaping-child.c
 *
 * License: LGPL v2.1+
 * Copyright © 2011 Igalia S.L.
 */

/* If the pointer leaves the window, restore the widget position */
static gboolean
parent_leave_notify_event (GtkWidget *widget,
                           GdkEventMotion *event,
                           GtkWidget *parent)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);
	NautilusFloatingBarDetails *priv = self->priv;
	GtkAllocation alloc;

	gtk_widget_get_allocation (widget, &alloc);
	alloc.y = priv->initial_allocation.y;
	gtk_widget_size_allocate (widget, &alloc);

	return FALSE;
}

/* this should be in Gdk...really */
static gboolean
is_point_in_rectangle (int point_x,
                       int point_y,
                       GdkRectangle rectangle)
{
	int rectangle_x_higher_bound = rectangle.x + rectangle.width;
	int rectangle_y_higher_bound = rectangle.y + rectangle.height;

	return point_x >= rectangle.x && point_x < rectangle_x_higher_bound
		&& point_y >= rectangle.y && point_y < rectangle_y_higher_bound;
}

/* Keep the widget-pointer distance at at least
 * EphyOverlayEscapingChildPrivate::escaping_distance by sliding the widget
 * away if needed.
 */
static gboolean
parent_motion_notify_event (GtkWidget *widget,
                            GdkEventMotion *event,
                            GtkWidget *parent)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);
	NautilusFloatingBarDetails *priv = self->priv;
	int distance_x, distance_y;
	GtkAllocation alloc;

	gtk_widget_get_allocation (widget, &alloc);

	if (is_point_in_rectangle (event->x, event->y, priv->escaping_area)) {
		gtk_widget_get_pointer (widget, &distance_x, &distance_y);
		alloc.y += priv->escaping_distance + distance_y;
	} else {
		/* Put the widget at its original position if we are out of the escaping
		 * zone. Do nothing if it is already there.
		 */
		if (alloc.y == priv->initial_allocation.y) {
			return FALSE;
		}

		alloc.y = priv->initial_allocation.y;
	}

	gtk_widget_size_allocate (widget, &alloc);

	return FALSE;
}

/* When the parent overlay is resized, the child relative position is modified.
 * So we update our initial_allocation to this new value and redefine our
 * escaping area.
 */
static void
parent_size_allocate (GtkWidget    *widget,
                      GdkRectangle *allocation,
                      GtkWidget      *parent)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);
	NautilusFloatingBarDetails *priv = self->priv;
	GtkAllocation initial_allocation;

	gtk_widget_get_allocation (widget, &initial_allocation);
	priv->escaping_area = priv->initial_allocation = initial_allocation;

	/* Define an escaping area around the widget.
	 * Current implementation only handle horizontal lowerside widgets
	 */
	priv->escaping_area.height += priv->escaping_distance;
	/* escape on both right and left */
	priv->escaping_area.width += 2 * priv->escaping_distance;
	priv->escaping_area.x -= priv->escaping_distance;
	priv->escaping_area.y -= priv->escaping_distance;
}

/* Install listeners on our overlay parents to locate the pointer
 * and our relative position.
 */
static void
nautilus_floating_bar_parent_set (GtkWidget *widget,
				  GtkWidget *previous_parent)
{
	GtkWidget *parent;

	if (previous_parent != NULL) {
		g_signal_handlers_disconnect_by_func (previous_parent,
						      G_CALLBACK (parent_motion_notify_event),
						      widget);
		g_signal_handlers_disconnect_by_func (previous_parent,
						      G_CALLBACK (parent_leave_notify_event),
						      widget);
		g_signal_handlers_disconnect_by_func (previous_parent,
						      G_CALLBACK (parent_size_allocate),
						      widget);
	}

	parent = gtk_widget_get_parent (widget);

	if (parent == NULL) {
		return;
	}

	g_signal_connect_swapped (parent,
				  "motion-notify-event",
				  G_CALLBACK (parent_motion_notify_event),
				  widget);
	g_signal_connect_swapped (parent,
				  "leave-notify-event",
				  G_CALLBACK (parent_leave_notify_event),
				  widget);
	g_signal_connect_swapped (parent,
				  "size-allocate",
				  G_CALLBACK (parent_size_allocate),
				  widget);
}

/* When the mouse is over us, translate the event coords and slide the widget
 * accordingly
 */
static gboolean
nautilus_floating_bar_motion_notify (GtkWidget *widget,
				     GdkEventMotion *event)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);
	NautilusFloatingBarDetails *priv = self->priv;

	event->x += priv->initial_allocation.x;
	event->y += priv->initial_allocation.y;
	return parent_motion_notify_event (widget, event, gtk_widget_get_parent (widget));
}

/* Make our event window propagate mouse motion events, so we can slide the widget,
 * when hovered.
 */
static void
nautilus_floating_bar_realize (GtkWidget *widget)
{
	GdkWindow *window;
	GdkEventMask events;

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->realize (widget);

	window = gtk_widget_get_window (widget);
	events = gdk_window_get_events (window);
	events |= GDK_POINTER_MOTION_MASK;

	gdk_window_set_events (window, events);
}

/* end of code adapted from Epiphany */

static void
nautilus_floating_bar_show (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->show (widget);

	if (self->priv->show_spinner) {
		gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
	}
}

static void
nautilus_floating_bar_hide (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->hide (widget);

	gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
}

static gboolean
nautilus_floating_bar_draw (GtkWidget *widget,
                          cairo_t *cr)
{
	  GtkStyleContext *context;

	  if (gtk_widget_get_has_window (widget) &&
	      !gtk_widget_get_app_paintable (widget)) {

		  context = gtk_widget_get_style_context (widget);

		  gtk_style_context_save (context);
		  gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));

		  gtk_render_background (context, cr, 0, 0,
					 gtk_widget_get_allocated_width (widget),
					 gtk_widget_get_allocated_height (widget));

		  gtk_render_frame (context, cr, 0, 0,
		  		    gtk_widget_get_allocated_width (widget),
		  		    gtk_widget_get_allocated_height (widget));

		  gtk_style_context_restore (context);
	  }

	  GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->draw (widget, cr);

	  return FALSE;
}

static void
nautilus_floating_bar_constructed (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);
	GtkWidget *w, *box;

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->constructed (obj);

	g_object_get (self,
		      "widget", &box,
		      NULL);
	gtk_widget_show (box);

	w = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_widget_set_visible (w, self->priv->show_spinner);
	self->priv->spinner = w;

	gtk_widget_set_size_request (w, 16, 16);
	gtk_widget_set_margin_left (w, 8);

	w = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	g_object_set (w,
		      "margin-top", 2,
		      "margin-bottom", 2,
		      "margin-left", 12,
		      "margin-right", 12,
		      NULL);
	self->priv->label_widget = w;
	gtk_widget_show (w);

	g_object_unref (box);
}

static void
nautilus_floating_bar_init (NautilusFloatingBar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_FLOATING_BAR,
						  NautilusFloatingBarDetails);
}

static void
nautilus_floating_bar_class_init (NautilusFloatingBarClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

	oclass->constructed = nautilus_floating_bar_constructed;
	oclass->set_property = nautilus_floating_bar_set_property;
	oclass->get_property = nautilus_floating_bar_get_property;
	oclass->finalize = nautilus_floating_bar_finalize;

	wclass->draw = nautilus_floating_bar_draw;
	wclass->show = nautilus_floating_bar_show;
	wclass->hide = nautilus_floating_bar_hide;
	wclass->parent_set = nautilus_floating_bar_parent_set;
	wclass->motion_notify_event = nautilus_floating_bar_motion_notify;
	wclass->realize = nautilus_floating_bar_realize;

	properties[PROP_LABEL] =
		g_param_spec_string ("label",
				     "Bar's label",
				     "Label displayed by the bar",
				     NULL,
				     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_SPINNER] =
		g_param_spec_boolean ("show-spinner",
				      "Show spinner",
				      "Whether a spinner should be shown in the floating bar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_ESCAPING_DISTANCE] =
		g_param_spec_uint ("escaping-distance",
				   "Escaping distance",
				   "Maximum distance between the mouse pointer and the widget",
				   0,
				   G_MAXUINT,
				   NAUTILUS_FLOATING_BAR_ESCAPING_DISTANCE,
				   G_PARAM_CONSTRUCT_ONLY |
				   G_PARAM_READWRITE |
				   G_PARAM_STATIC_STRINGS);

	signals[ACTION] =
		g_signal_new ("action",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (NautilusFloatingBarDetails));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_floating_bar_set_label (NautilusFloatingBar *self,
				 const gchar *label)
{
	if (g_strcmp0 (self->priv->label, label) != 0) {
		g_free (self->priv->label);
		self->priv->label = g_strdup (label);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);

		update_label (self);
	}
}

void
nautilus_floating_bar_set_show_spinner (NautilusFloatingBar *self,
					gboolean show_spinner)
{
	if (self->priv->show_spinner != show_spinner) {
		self->priv->show_spinner = show_spinner;
		gtk_widget_set_visible (self->priv->spinner,
					show_spinner);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINNER]);
	}
}

GtkWidget *
nautilus_floating_bar_new (const gchar *label,
			   gboolean show_spinner)
{
	return g_object_new (NAUTILUS_TYPE_FLOATING_BAR,
			     "widget", gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8),
			     "label", label,
			     "show-spinner", show_spinner,
			     NULL);
}

void
nautilus_floating_bar_add_action (NautilusFloatingBar *self,
				  const gchar *stock_id,
				  gint action_id)
{
	GtkWidget *w, *button, *box;

	g_object_get (self,
		      "widget", &box,
		      NULL);

	w = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
	gtk_widget_show (w);

	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button), w);
	gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	g_object_set_data (G_OBJECT (button), "action-id",
			   GINT_TO_POINTER (action_id));

	g_signal_connect (button, "clicked",
			  G_CALLBACK (action_button_clicked_cb), self);

	g_object_unref (box);
}

void
nautilus_floating_bar_cleanup_actions (NautilusFloatingBar *self)
{
	GtkWidget *box, *widget;
	GList *children, *l;
	gpointer data;

	g_object_get (self,
		      "widget", &box,
		      NULL);

	children = gtk_container_get_children (GTK_CONTAINER (box));
	l = children;

	while (l != NULL) {
		widget = l->data;
		data = g_object_get_data (G_OBJECT (widget), "action-id");
		l = l->next;

		if (data != NULL) {
			/* destroy this */
			gtk_widget_destroy (widget);
		}
	}

	g_object_unref (box);
	g_list_free (children);
}
