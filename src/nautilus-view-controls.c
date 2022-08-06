/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-view-controls.h"

#include "nautilus-toolbar-menu-sections.h"
#include "nautilus-window.h"

struct _NautilusViewControls
{
    AdwBin parent_instance;

    GtkWidget *view_split_button;
    GMenuModel *view_menu;

    NautilusWindowSlot *window_slot;
};

G_DEFINE_FINAL_TYPE (NautilusViewControls, nautilus_view_controls, ADW_TYPE_BIN);


enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
on_slot_toolbar_menu_sections_changed (NautilusViewControls *self,
                                       GParamSpec           *param,
                                       NautilusWindowSlot   *slot)
{
    NautilusToolbarMenuSections *new_sections;
    g_autoptr (GMenuItem) zoom_item = NULL;
    g_autoptr (GMenuItem) sort_item = NULL;

    new_sections = nautilus_window_slot_get_toolbar_menu_sections (slot);

    gtk_widget_set_sensitive (self->view_split_button, (new_sections != NULL));
    if (new_sections == NULL)
    {
        return;
    }

    /* Let's assume that sort section is the first item
     * in view_menu, as per nautilus-toolbar.ui. */

    sort_item = g_menu_item_new_from_model (self->view_menu, 0);
    g_menu_remove (G_MENU (self->view_menu), 0);
    g_menu_item_set_section (sort_item, new_sections->sort_section);
    g_menu_insert_item (G_MENU (self->view_menu), 0, sort_item);
}

static void
disconnect_toolbar_menu_sections_change_handler (NautilusViewControls *self)
{
    if (self->window_slot == NULL)
    {
        return;
    }

    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (on_slot_toolbar_menu_sections_changed),
                                          self);
}


static void
nautilus_view_controls_set_window_slot (NautilusViewControls *self,
                                        NautilusWindowSlot   *window_slot)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_CONTROLS (self));
    g_return_if_fail (window_slot == NULL || NAUTILUS_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    disconnect_toolbar_menu_sections_change_handler (self);

    self->window_slot = window_slot;

    if (self->window_slot != NULL)
    {
        on_slot_toolbar_menu_sections_changed (self, NULL, self->window_slot);
        g_signal_connect_swapped (self->window_slot, "notify::toolbar-menu-sections",
                                  G_CALLBACK (on_slot_toolbar_menu_sections_changed), self);
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

static void
nautilus_view_controls_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusViewControls *self = NAUTILUS_VIEW_CONTROLS (object);

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
nautilus_view_controls_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    NautilusViewControls *self = NAUTILUS_VIEW_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            nautilus_view_controls_set_window_slot (self, NAUTILUS_WINDOW_SLOT (g_value_get_object (value)));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_controls_finalize (GObject *obj)
{
    NautilusViewControls *self = NAUTILUS_VIEW_CONTROLS (obj);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        self->window_slot = NULL;
    }

    G_OBJECT_CLASS (nautilus_view_controls_parent_class)->finalize (obj);
}

static void
nautilus_view_controls_class_init (NautilusViewControlsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_view_controls_finalize;
    object_class->get_property = nautilus_view_controls_get_property;
    object_class->set_property = nautilus_view_controls_set_property;

    properties[PROP_WINDOW_SLOT] = g_param_spec_object ("window-slot",
                                                        NULL, NULL,
                                                        NAUTILUS_TYPE_WINDOW_SLOT,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-view-controls.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusViewControls, view_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewControls, view_split_button);
}

static void
nautilus_view_controls_init (NautilusViewControls *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
