/*
 * Copyright (C) 2022 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-name-cell.h"
#include "nautilus-view-cell.h"

#include "nautilus-list-base.h"
#include "nautilus-view-item.h"

/**
 * NautilusViewCell:
 *
 * Abstract class of widgets tailored to be set as #GtkListItem:child in a view
 * which subclasses #NautilusListBase.
 *
 * Subclass constructors should take a pointer to the #NautilusListBase view.
 *
 * The view is responsible for setting #NautilusViewCell:item. This is done
 * using nautilus_view_cell_bind_listitem.
 */

typedef struct _NautilusViewCellPrivate NautilusViewCellPrivate;
struct _NautilusViewCellPrivate
{
    GtkWidget parent_instance;

    NautilusListBase *view; /* Unowned */
    NautilusViewItem *item; /* Owned reference */

    guint icon_size;
    guint position;

    gboolean setup_called_once;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusViewCell, nautilus_view_cell, GTK_TYPE_WIDGET)

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

enum
{
    PROP_0,
    PROP_VIEW,
    PROP_ITEM,
    PROP_ICON_SIZE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
nautilus_view_cell_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_VIEW:
        {
            g_value_set_object (value, priv->view);
        }
        break;

        case PROP_ITEM:
        {
            g_value_set_object (value, priv->item);
        }
        break;

        case PROP_ICON_SIZE:
        {
            g_value_set_uint (value, priv->icon_size);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_cell_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_VIEW:
        {
            g_set_weak_pointer (&priv->view, g_value_get_object (value));
        }
        break;

        case PROP_ITEM:
        {
            g_set_object (&priv->item, g_value_get_object (value));
        }
        break;

        case PROP_ICON_SIZE:
        {
            priv->icon_size = g_value_get_uint (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_cell_init (NautilusViewCell *self)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-view-cell");

    priv->position = GTK_INVALID_LIST_POSITION;
}

static void
nautilus_view_cell_dispose (GObject *object)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    g_clear_object (&priv->item);
    g_clear_weak_pointer (&priv->view);

    G_OBJECT_CLASS (nautilus_view_cell_parent_class)->dispose (object);
}

static void
nautilus_view_cell_class_init (NautilusViewCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_view_cell_dispose;
    object_class->get_property = nautilus_view_cell_get_property;
    object_class->set_property = nautilus_view_cell_set_property;

    properties[PROP_VIEW] = g_param_spec_object ("view",
                                                 "", "",
                                                 NAUTILUS_TYPE_LIST_BASE,
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ITEM] = g_param_spec_object ("item",
                                                 "", "",
                                                 NAUTILUS_TYPE_VIEW_ITEM,
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ICON_SIZE] = g_param_spec_uint ("icon-size",
                                                    "", "",
                                                    NAUTILUS_LIST_ICON_SIZE_SMALL,
                                                    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE,
                                                    NAUTILUS_GRID_ICON_SIZE_LARGE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

gboolean
nautilus_view_cell_setup (NautilusViewCell *self,
                          GCallback         on_item_click_pressed,
                          GCallback         on_item_click_stopped,
                          GCallback         on_item_click_released,
                          GCallback         on_item_longpress_pressed,
                          GCallback         on_item_drag_prepare,
                          GCallback         on_item_drag_enter,
                          GCallback         on_item_drag_value_notify,
                          GCallback         on_item_drag_leave,
                          GCallback         on_item_drag_motion,
                          GCallback         on_item_drop,
                          GCallback         on_item_drag_hover_enter,
                          GCallback         on_item_drag_hover_leave,
                          GCallback         on_item_drag_hover_motion)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    if (priv->setup_called_once)
    {
        return FALSE;
    }

    GtkEventController *controller;
    GtkDropTarget *drop_target;
    GtkWidget *hover_target = NAUTILUS_IS_NAME_CELL (self)
                              ? nautilus_name_cell_get_content (NAUTILUS_NAME_CELL (self))
                              : GTK_WIDGET (self);

    priv->setup_called_once = TRUE;

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed", on_item_click_pressed, self);
    g_signal_connect (controller, "stopped", on_item_click_stopped, self);
    g_signal_connect (controller, "released", on_item_click_released, self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed", on_item_longpress_pressed, self);

    controller = GTK_EVENT_CONTROLLER (gtk_drag_source_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    g_signal_connect (controller, "prepare", on_item_drag_prepare, self);

    /* TODO: Implement GDK_ACTION_ASK */
    drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);
    gtk_drop_target_set_preload (drop_target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (drop_target, (GType[3]) { GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST, G_TYPE_STRING }, 3);
    g_signal_connect (drop_target, "enter", on_item_drag_enter, self);
    g_signal_connect (drop_target, "notify::value", on_item_drag_value_notify, self);
    g_signal_connect (drop_target, "leave", on_item_drag_leave, self);
    g_signal_connect (drop_target, "motion", on_item_drag_motion, self);
    g_signal_connect (drop_target, "drop", on_item_drop, self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));

    controller = gtk_drop_controller_motion_new ();
    gtk_widget_add_controller (hover_target, controller);
    g_signal_connect (controller, "enter", on_item_drag_hover_enter, self);
    g_signal_connect (controller, "leave", on_item_drag_hover_leave, self);
    g_signal_connect (controller, "motion", on_item_drag_hover_motion, self);

    return TRUE;
}

static void
nautilus_view_cell_set_position (NautilusViewCell *self,
                                 guint             position)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    priv->position = position;
}

guint
nautilus_view_cell_get_position (NautilusViewCell *self)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    return priv->position;
}

NautilusListBase *
nautilus_view_cell_get_view (NautilusViewCell *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_CELL (self), NULL);

    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    return priv->view;
}

void
nautilus_view_cell_set_item (NautilusViewCell *self,
                             NautilusViewItem *item)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_CELL (self));
    g_return_if_fail (item == NULL || NAUTILUS_IS_VIEW_ITEM (item));

    g_object_set (self, "item", item, NULL);
}

NautilusViewItem *
nautilus_view_cell_get_item (NautilusViewCell *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_CELL (self), NULL);

    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    /* Return full reference for consistency with gtk_tree_list_row_get_item() */
    return priv->item != NULL ? g_object_ref (priv->item) : NULL;
}

void
nautilus_view_cell_bind_listitem (NautilusViewCell *self,
                                  GtkListItem      *listitem)
{
    nautilus_view_cell_set_item (self, get_view_item (listitem));
    nautilus_view_cell_set_position (self, gtk_list_item_get_position (listitem));
}

void
nautilus_view_cell_unbind_listitem (NautilusViewCell *self)
{
    nautilus_view_cell_set_item (self, NULL);
    nautilus_view_cell_set_position (self, GTK_INVALID_LIST_POSITION);
}
