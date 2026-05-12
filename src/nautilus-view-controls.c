/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-view-controls.h"

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window-slot.h"
#include <glib/gi18n.h>

struct _NautilusViewControls
{
    AdwBin parent_instance;

    GtkWidget *view_split_button;
    GMenuModel *view_menu;
    GMenuModel *sort_section;

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
update_sort_section (NautilusViewControls *self,
                     NautilusFile         *file)
{
    const char *action;

    /* When not in the special location, set an inexistant action to hide the
     * menu item. This works under the assumptiont that the menu item has its
     * "hidden-when" attribute set to "action-disabled", and that an inexistant
     * action is treated as a disabled action. */
    action = nautilus_file_is_in_trash (file) ? "view.sort" : "doesnt-exist";
    nautilus_menu_item_change_attribute (self->sort_section,
                                         "last_trashed",
                                         G_MENU_ATTRIBUTE_ACTION, action);

    action = nautilus_file_is_in_recent (file) ? "view.sort" : "doesnt-exist";
    nautilus_menu_item_change_attribute (self->sort_section,
                                         "recency",
                                         G_MENU_ATTRIBUTE_ACTION, action);

    action = nautilus_file_is_in_search (file) ? "view.sort" : "doesnt-exist";
    nautilus_menu_item_change_attribute (self->sort_section,
                                         "relevance",
                                         G_MENU_ATTRIBUTE_ACTION, action);
}

static void
adjust_to_location (NautilusViewControls *self,
                    GParamSpec           *param,
                    NautilusWindowSlot   *slot)
{
    GFile *location = nautilus_window_slot_get_location (slot);
    gboolean has_menu = location != NULL &&
                        !nautilus_is_root_for_scheme (location, SCHEME_NETWORK_VIEW);

    gtk_widget_set_sensitive (self->view_split_button, has_menu);

    if (!has_menu)
    {
        return;
    }

    g_autoptr (NautilusFile) file = nautilus_file_get (location);

    update_sort_section (self, file);
}

static void
on_tooltip_changed (NautilusViewControls *self,
                    GParamSpec           *param,
                    NautilusWindowSlot   *slot)
{
    const gchar *description;
    const gchar *tooltip = nautilus_window_slot_get_tooltip_with_description (slot, &description);

    if (tooltip == NULL)
    {
        return;
    }
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->view_split_button),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    tooltip,
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                    description,
                                    -1);
}


static void
disconnect_toolbar_menu_sections_change_handler (NautilusViewControls *self)
{
    if (self->window_slot == NULL)
    {
        return;
    }

    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (adjust_to_location),
                                          self);
    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (on_tooltip_changed),
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
        adjust_to_location (self, NULL, self->window_slot);
        g_signal_connect_swapped (self->window_slot, "notify::location",
                                  G_CALLBACK (adjust_to_location), self);
        g_signal_connect_swapped (self->window_slot, "notify::tooltip",
                                  G_CALLBACK (on_tooltip_changed), self);
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
nautilus_view_controls_dispose (GObject *obj)
{
    NautilusViewControls *self = NAUTILUS_VIEW_CONTROLS (obj);

    disconnect_toolbar_menu_sections_change_handler (self);
    adw_bin_set_child (ADW_BIN (self), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_VIEW_CONTROLS);

    G_OBJECT_CLASS (nautilus_view_controls_parent_class)->dispose (obj);
}

static void
nautilus_view_controls_finalize (GObject *obj)
{
    NautilusViewControls *self = NAUTILUS_VIEW_CONTROLS (obj);

    g_clear_object (&self->sort_section);
    g_clear_object (&self->window_slot);

    G_OBJECT_CLASS (nautilus_view_controls_parent_class)->finalize (obj);
}

static void
nautilus_view_controls_class_init (NautilusViewControlsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_view_controls_dispose;
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
    gtk_widget_class_bind_template_child (widget_class, NautilusViewControls, sort_section);
}

static void
nautilus_view_controls_init (NautilusViewControls *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
