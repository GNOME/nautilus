/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-history-controls.h"

#include "nautilus-bookmark.h"
#include "nautilus-window.h"

struct _NautilusHistoryControls
{
    AdwBin parent_instance;

    GtkWidget *back_button;
    GtkWidget *back_menu;

    GtkWidget *forward_button;
    GtkWidget *forward_menu;

    NautilusWindowSlot *window_slot;
};

G_DEFINE_FINAL_TYPE (NautilusHistoryControls, nautilus_history_controls, ADW_TYPE_BIN)

enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
fill_menu (NautilusHistoryControls *self,
           GMenu                   *menu,
           gboolean                 back)
{
    guint index;
    GList *list;
    const gchar *name;

    list = back ? nautilus_window_slot_get_back_history (self->window_slot) :
                  nautilus_window_slot_get_forward_history (self->window_slot);

    index = 0;
    while (list != NULL)
    {
        g_autoptr (GMenuItem) item = NULL;

        name = nautilus_bookmark_get_name (NAUTILUS_BOOKMARK (list->data));
        item = g_menu_item_new (name, NULL);
        g_menu_item_set_action_and_target (item,
                                           back ? "win.back-n" : "win.forward-n",
                                           "u", index);
        g_menu_append_item (menu, item);

        list = g_list_next (list);
        ++index;
    }
}

static void
show_menu (NautilusHistoryControls *self,
           GtkWidget               *widget)
{
    g_autoptr (GMenu) menu = NULL;
    NautilusNavigationDirection direction;
    GtkPopoverMenu *popover;

    menu = g_menu_new ();

    direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                     "nav-direction"));

    switch (direction)
    {
        case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
        {
            fill_menu (self, menu, FALSE);
            popover = GTK_POPOVER_MENU (self->forward_menu);
        }
        break;

        case NAUTILUS_NAVIGATION_DIRECTION_BACK:
        {
            fill_menu (self, menu, TRUE);
            popover = GTK_POPOVER_MENU (self->back_menu);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    gtk_popover_menu_set_menu_model (popover, G_MENU_MODEL (menu));
    gtk_popover_popup (GTK_POPOVER (popover));
}

static void
navigation_button_press_cb (GtkGestureClick *gesture,
                            gint             n_press,
                            gdouble          x,
                            gdouble          y,
                            gpointer         user_data)
{
    NautilusHistoryControls *self;
    NautilusWindow *window;
    GtkWidget *widget;
    guint button;

    self = NAUTILUS_HISTORY_CONTROLS (user_data);
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    window = NAUTILUS_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

    if (button == GDK_BUTTON_PRIMARY)
    {
        /* Don't do anything, primary click is handled through activate */
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
        return;
    }
    else if (button == GDK_BUTTON_MIDDLE)
    {
        NautilusNavigationDirection direction;

        direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                         "nav-direction"));

        nautilus_window_back_or_forward_in_new_tab (window, direction);
    }
    else if (button == GDK_BUTTON_SECONDARY)
    {
        show_menu (self, widget);
    }
}

static void
back_button_longpress_cb (GtkGestureLongPress *gesture,
                          double               x,
                          double               y,
                          gpointer             user_data)
{
    NautilusHistoryControls *self = user_data;

    show_menu (self, self->back_button);
}

static void
forward_button_longpress_cb (GtkGestureLongPress *gesture,
                             double               x,
                             double               y,
                             gpointer             user_data)
{
    NautilusHistoryControls *self = user_data;

    show_menu (self, self->forward_button);
}


static void
nautilus_history_controls_contructed (GObject *object)
{
    NautilusHistoryControls *self;
    GtkEventController *controller;

    self = NAUTILUS_HISTORY_CONTROLS (object);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (self->back_button, controller);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (back_button_longpress_cb), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (self->forward_button, controller);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (forward_button_longpress_cb), self);

    g_object_set_data (G_OBJECT (self->back_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_BACK));
    g_object_set_data (G_OBJECT (self->forward_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_FORWARD));

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (self->back_button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (self->forward_button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);
}

static void
nautilus_history_controls_dispose (GObject *object)
{
    NautilusHistoryControls *self;

    self = NAUTILUS_HISTORY_CONTROLS (object);

    g_clear_pointer (&self->back_menu, gtk_widget_unparent);
    g_clear_pointer (&self->forward_menu, gtk_widget_unparent);

    G_OBJECT_CLASS (nautilus_history_controls_parent_class)->dispose (object);
}

static void
nautilus_history_controls_set_window_slot (NautilusHistoryControls *self,
                                           NautilusWindowSlot      *window_slot)
{
    g_return_if_fail (NAUTILUS_IS_HISTORY_CONTROLS (self));
    g_return_if_fail (window_slot == NULL || NAUTILUS_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    self->window_slot = window_slot;

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

static void
nautilus_history_controls_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    NautilusHistoryControls *self = NAUTILUS_HISTORY_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_value_set_object (value, G_OBJECT (self->window_slot));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_history_controls_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    NautilusHistoryControls *self = NAUTILUS_HISTORY_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            nautilus_history_controls_set_window_slot (self, NAUTILUS_WINDOW_SLOT (g_value_get_object (value)));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_history_controls_class_init (NautilusHistoryControlsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = nautilus_history_controls_contructed;
    object_class->dispose = nautilus_history_controls_dispose;
    object_class->set_property = nautilus_history_controls_set_property;
    object_class->get_property = nautilus_history_controls_get_property;

    properties[PROP_WINDOW_SLOT] = g_param_spec_object ("window-slot",
                                                        NULL, NULL,
                                                        NAUTILUS_TYPE_WINDOW_SLOT,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-history-controls.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusHistoryControls, back_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusHistoryControls, back_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusHistoryControls, forward_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusHistoryControls, forward_menu);
}

static void
nautilus_history_controls_init (NautilusHistoryControls *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_widget_set_parent (self->back_menu, self->back_button);
    g_signal_connect (self->back_menu, "destroy", G_CALLBACK (gtk_widget_unparent), NULL);
    gtk_widget_set_parent (self->forward_menu, self->forward_button);
    g_signal_connect (self->forward_menu, "destroy", G_CALLBACK (gtk_widget_unparent), NULL);
}
