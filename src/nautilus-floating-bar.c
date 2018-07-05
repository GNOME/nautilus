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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include <string.h>

#include "nautilus-floating-bar.h"

#define HOVER_HIDE_TIMEOUT_INTERVAL 100

struct _NautilusFloatingBar
{
    GtkBox parent;

    gchar *primary_label;
    gchar *details_label;

    GtkWidget *primary_label_widget;
    GtkWidget *details_label_widget;
    GtkWidget *spinner;
    gboolean show_spinner;
    gboolean is_interactive;
    guint hover_timeout_id;
};

enum
{
    PROP_PRIMARY_LABEL = 1,
    PROP_DETAILS_LABEL,
    PROP_SHOW_SPINNER,
    NUM_PROPERTIES
};

enum
{
    ACTION,
    NUM_SIGNALS
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GTK_TYPE_BOX);

static void
action_button_clicked_cb (GtkButton           *button,
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

    nautilus_floating_bar_remove_hover_timeout (self);
    g_free (self->primary_label);
    g_free (self->details_label);

    G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->finalize (obj);
}

static void
nautilus_floating_bar_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

    switch (property_id)
    {
        case PROP_PRIMARY_LABEL:
        {
            g_value_set_string (value, self->primary_label);
        }
        break;

        case PROP_DETAILS_LABEL:
        {
            g_value_set_string (value, self->details_label);
        }
        break;

        case PROP_SHOW_SPINNER:
        {
            g_value_set_boolean (value, self->show_spinner);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_floating_bar_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

    switch (property_id)
    {
        case PROP_PRIMARY_LABEL:
        {
            nautilus_floating_bar_set_primary_label (self, g_value_get_string (value));
        }
        break;

        case PROP_DETAILS_LABEL:
        {
            nautilus_floating_bar_set_details_label (self, g_value_get_string (value));
        }
        break;

        case PROP_SHOW_SPINNER:
        {
            nautilus_floating_bar_set_show_spinner (self, g_value_get_boolean (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
update_labels (NautilusFloatingBar *self)
{
    gboolean primary_visible, details_visible;

    primary_visible = (self->primary_label != NULL) &&
                      (strlen (self->primary_label) > 0);
    details_visible = (self->details_label != NULL) &&
                      (strlen (self->details_label) > 0);

    gtk_label_set_text (GTK_LABEL (self->primary_label_widget),
                        self->primary_label);
    gtk_widget_set_visible (self->primary_label_widget, primary_visible);

    gtk_label_set_text (GTK_LABEL (self->details_label_widget),
                        self->details_label);
    gtk_widget_set_visible (self->details_label_widget, details_visible);
}

void
nautilus_floating_bar_remove_hover_timeout (NautilusFloatingBar *self)
{
    if (self->hover_timeout_id != 0)
    {
        g_source_remove (self->hover_timeout_id);
        self->hover_timeout_id = 0;
    }
}

typedef struct
{
    GtkWidget *overlay;
    GtkWidget *floating_bar;
    GdkDevice *device;
    gint y_down_limit;
    gint y_upper_limit;
} CheckPointerData;

static void
check_pointer_data_free (gpointer data)
{
    g_slice_free (CheckPointerData, data);
}

static gboolean
check_pointer_timeout (gpointer user_data)
{
    CheckPointerData *data = user_data;
    gint pointer_y = -1;

    gdk_window_get_device_position (gtk_widget_get_window (data->overlay), data->device,
                                    NULL, &pointer_y, NULL);

    if (pointer_y == -1 || pointer_y < data->y_down_limit || pointer_y > data->y_upper_limit)
    {
        gtk_widget_show (data->floating_bar);
        NAUTILUS_FLOATING_BAR (data->floating_bar)->hover_timeout_id = 0;

        return G_SOURCE_REMOVE;
    }
    else
    {
        gtk_widget_hide (data->floating_bar);
    }

    return G_SOURCE_CONTINUE;
}

static gboolean
overlay_event_cb (GtkWidget *parent,
                  GdkEvent  *event,
                  gpointer   user_data)
{
    NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (user_data);
    GtkWidget *widget = user_data;
    CheckPointerData *data;
    gint y_pos;

    if (gdk_event_get_event_type (event) != GDK_ENTER_NOTIFY)
    {
        return GDK_EVENT_PROPAGATE;
    }

    if (self->hover_timeout_id != 0)
    {
        g_source_remove (self->hover_timeout_id);
    }

    if (gdk_event_get_window (event) != gtk_widget_get_window (widget))
    {
        return GDK_EVENT_PROPAGATE;
    }

    if (self->is_interactive)
    {
        return GDK_EVENT_PROPAGATE;
    }

    gdk_window_get_position (gtk_widget_get_window (widget), NULL, &y_pos);

    data = g_slice_new (CheckPointerData);
    data->overlay = parent;
    data->floating_bar = widget;
    data->device = gdk_event_get_device (event);
    data->y_down_limit = y_pos;
    data->y_upper_limit = y_pos + gtk_widget_get_allocated_height (widget);

    self->hover_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, HOVER_HIDE_TIMEOUT_INTERVAL,
                                                 check_pointer_timeout, data,
                                                 check_pointer_data_free);

    g_source_set_name_by_id (self->hover_timeout_id, "[nautilus-floating-bar] overlay_event_cb");

    return GDK_EVENT_STOP;
}

static void
nautilus_floating_bar_parent_set (GtkWidget *widget,
                                  GtkWidget *old_parent)
{
    GtkWidget *parent;

    parent = gtk_widget_get_parent (widget);

    if (old_parent != NULL)
    {
        g_signal_handlers_disconnect_by_func (old_parent,
                                              overlay_event_cb, widget);
    }

    if (parent != NULL)
    {
        g_signal_connect (parent, "event",
                          G_CALLBACK (overlay_event_cb), widget);
    }
}

static void
measure (GtkWidget      *widget,
         GtkOrientation  orientation,
         int             for_size,
         int            *minimum,
         int            *natural,
         int            *minimum_baseline,
         int            *natural_baseline)
{
    GtkStyleContext *context;
    GtkBorder border;
    GtkBorder padding;
    GtkWidgetClass *widget_class;

    context = gtk_widget_get_style_context (widget);
    widget_class = GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class);

    gtk_style_context_get_border (context, &border);
    gtk_style_context_get_padding (context, &padding);

    widget_class->measure (widget, orientation, for_size,
                           minimum, natural,
                           minimum_baseline, natural_baseline);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        border.bottom = 0;
        border.top = 0;

        padding.bottom = 0;
        padding.top = 0;
    }
    else
    {
        border.left = 0;
        border.right = 0;

        padding.left = 0;
        padding.right = 0;
    }

    if (minimum != NULL)
    {
        *minimum += border.bottom + border.left + border.right + border.top;
        *minimum += padding.bottom + padding.left + padding.right + padding.top;
    }
    if (natural != NULL)
    {
        *natural += border.bottom + border.left + border.right + border.top;
        *natural += padding.bottom + padding.left + padding.right + padding.top;
    }
}

static void
nautilus_floating_bar_constructed (GObject *obj)
{
    NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);
    GtkWidget *w, *box, *labels_box;

    G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->constructed (obj);

    box = GTK_WIDGET (obj);

    w = gtk_spinner_new ();
    gtk_box_pack_start (GTK_BOX (box), w);
    gtk_widget_set_visible (w, self->show_spinner);
    gtk_spinner_start (GTK_SPINNER (w));
    self->spinner = w;

    gtk_widget_set_size_request (w, 16, 16);
    gtk_widget_set_margin_start (w, 8);

    labels_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_hexpand (labels_box, TRUE);
    gtk_box_pack_start (GTK_BOX (box), labels_box);
    g_object_set (labels_box,
                  "margin-top", 2,
                  "margin-bottom", 2,
                  "margin-start", 12,
                  "margin-end", 12,
                  NULL);
    gtk_widget_show (labels_box);

    w = gtk_label_new (NULL);
    gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
    gtk_container_add (GTK_CONTAINER (labels_box), w);
    self->primary_label_widget = w;
    gtk_widget_show (w);

    w = gtk_label_new (NULL);
    gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
    gtk_container_add (GTK_CONTAINER (labels_box), w);
    self->details_label_widget = w;
    gtk_widget_show (w);
}

static void
nautilus_floating_bar_init (NautilusFloatingBar *self)
{
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_add_class (context, "floating-bar");
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

    wclass->measure = measure;
    wclass->parent_set = nautilus_floating_bar_parent_set;

    properties[PROP_PRIMARY_LABEL] =
        g_param_spec_string ("primary-label",
                             "Bar's primary label",
                             "Primary label displayed by the bar",
                             NULL,
                             G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DETAILS_LABEL] =
        g_param_spec_string ("details-label",
                             "Bar's details label",
                             "Details label displayed by the bar",
                             NULL,
                             G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SHOW_SPINNER] =
        g_param_spec_boolean ("show-spinner",
                              "Show spinner",
                              "Whether a spinner should be shown in the floating bar",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    signals[ACTION] =
        g_signal_new ("action",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1,
                      G_TYPE_INT);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_floating_bar_set_primary_label (NautilusFloatingBar *self,
                                         const gchar         *label)
{
    if (g_strcmp0 (self->primary_label, label) != 0)
    {
        g_free (self->primary_label);
        self->primary_label = g_strdup (label);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_LABEL]);

        update_labels (self);
    }
}

void
nautilus_floating_bar_set_details_label (NautilusFloatingBar *self,
                                         const gchar         *label)
{
    if (g_strcmp0 (self->details_label, label) != 0)
    {
        g_free (self->details_label);
        self->details_label = g_strdup (label);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DETAILS_LABEL]);

        update_labels (self);
    }
}

void
nautilus_floating_bar_set_labels (NautilusFloatingBar *self,
                                  const gchar         *primary_label,
                                  const gchar         *details_label)
{
    nautilus_floating_bar_set_primary_label (self, primary_label);
    nautilus_floating_bar_set_details_label (self, details_label);
}

void
nautilus_floating_bar_set_show_spinner (NautilusFloatingBar *self,
                                        gboolean             show_spinner)
{
    if (self->show_spinner != show_spinner)
    {
        self->show_spinner = show_spinner;
        gtk_widget_set_visible (self->spinner,
                                show_spinner);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINNER]);
    }
}

GtkWidget *
nautilus_floating_bar_new (const gchar *primary_label,
                           const gchar *details_label,
                           gboolean     show_spinner)
{
    return g_object_new (NAUTILUS_TYPE_FLOATING_BAR,
                         "primary-label", primary_label,
                         "details-label", details_label,
                         "show-spinner", show_spinner,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         "spacing", 8,
                         NULL);
}

void
nautilus_floating_bar_add_action (NautilusFloatingBar *self,
                                  const gchar         *icon_name,
                                  gint                 action_id)
{
    GtkWidget *button;
    GtkStyleContext *context;

    button = gtk_button_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
    context = gtk_widget_get_style_context (button);
    gtk_style_context_add_class (context, "circular");
    gtk_style_context_add_class (context, "flat");
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_box_pack_end (GTK_BOX (self), button);
    gtk_widget_show (button);

    g_object_set_data (G_OBJECT (button), "action-id",
                       GINT_TO_POINTER (action_id));

    g_signal_connect (button, "clicked",
                      G_CALLBACK (action_button_clicked_cb), self);

    self->is_interactive = TRUE;
}

void
nautilus_floating_bar_cleanup_actions (NautilusFloatingBar *self)
{
    GtkWidget *widget;
    GList *children, *l;
    gpointer data;

    children = gtk_container_get_children (GTK_CONTAINER (self));
    l = children;

    while (l != NULL)
    {
        widget = l->data;
        data = g_object_get_data (G_OBJECT (widget), "action-id");
        l = l->next;

        if (data != NULL)
        {
            /* destroy this */
            gtk_widget_destroy (widget);
        }
    }

    g_list_free (children);

    self->is_interactive = FALSE;
}
