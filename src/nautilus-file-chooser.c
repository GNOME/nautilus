/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#define G_LOG_DOMAIN "nautilus-file-chooser"

#include "nautilus-file-chooser.h"

#include <config.h>
#include <gtk/gtk.h>

#include "gtk/nautilusgtkplacessidebarprivate.h"

#include "nautilus-enum-types.h"
#include "nautilus-shortcut-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-view-item-filter.h"

struct _NautilusFileChooser
{
    AdwWindow parent_instance;

    NautilusMode mode;
    GtkStack *stack;
    GtkWidget *selector_view;
    GtkWidget *places_sidebar;
    NautilusToolbar *toolbar;
    AdwToolbarView *slot_container;
    NautilusWindowSlot *slot;
    GtkDropDown *filters_dropdown;
    GtkWidget *choices_menu_button;
    GtkWidget *read_only_checkbox;
    GtkWidget *selector_accept_button;
};

G_DEFINE_FINAL_TYPE (NautilusFileChooser, nautilus_file_chooser, ADW_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_MODE,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
update_cursor (NautilusFileChooser *self)
{
    if (self->slot != NULL &&
        nautilus_window_slot_get_allow_stop (self->slot))
    {
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "progress");
    }
    else
    {
        gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
    }
}

static void
nautilus_file_chooser_dispose (GObject *object)
{
    NautilusFileChooser *self = (NautilusFileChooser *) object;

    if (self->slot != NULL)
    {
        g_assert (adw_toolbar_view_get_content (self->slot_container) == GTK_WIDGET (self->slot));

        nautilus_window_slot_set_active (self->slot, FALSE);
        /* Let bindings on AdwToolbarView:content react to the slot being unset
         * while the slot itself is still alive. */
        adw_toolbar_view_set_content (self->slot_container, NULL);
        g_clear_object (&self->slot);
    }

    G_OBJECT_CLASS (nautilus_file_chooser_parent_class)->dispose (object);
}

static void
nautilus_file_chooser_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (object);

    switch (prop_id)
    {
        case PROP_MODE:
        {
            g_value_set_enum (value, self->mode);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_file_chooser_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (object);

    switch (prop_id)
    {
        case PROP_MODE:
        {
            self->mode = g_value_get_enum (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_file_chooser_constructed (GObject *object)
{
    G_OBJECT_CLASS (nautilus_file_chooser_parent_class)->constructed (object);

    NautilusFileChooser *self = (NautilusFileChooser *) object;

    /* Setup slot.
     * We hold a reference to control its lifetime with relation to bindings. */
    self->slot = g_object_ref (nautilus_window_slot_new (self->mode));
    adw_toolbar_view_set_content (self->slot_container, GTK_WIDGET (self->slot));
    nautilus_window_slot_set_active (self->slot, TRUE);
    g_signal_connect_swapped (self->slot, "notify::allow-stop",
                              G_CALLBACK (update_cursor), self);

    g_autoptr (NautilusViewItemFilter) filter = nautilus_view_item_filter_new ();

    g_object_bind_property (self->filters_dropdown, "selected-item",
                            filter, "file-filter",
                            G_BINDING_SYNC_CREATE);
    nautilus_window_slot_set_filter (self->slot, GTK_FILTER (filter));

    gtk_widget_set_visible (self->choices_menu_button,
                            (self->mode != NAUTILUS_MODE_SAVE_FILE &&
                             self->mode != NAUTILUS_MODE_SAVE_FILES));
}

static void
nautilus_file_chooser_init (NautilusFileChooser *self)
{
    g_type_ensure (NAUTILUS_TYPE_TOOLBAR);
    g_type_ensure (NAUTILUS_TYPE_GTK_PLACES_SIDEBAR);
    g_type_ensure (NAUTILUS_TYPE_SHORTCUT_MANAGER);
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Setup sidebar */
    nautilus_gtk_places_sidebar_set_open_flags (NAUTILUS_GTK_PLACES_SIDEBAR (self->places_sidebar),
                                                NAUTILUS_OPEN_FLAG_NORMAL);
}

static void
nautilus_file_chooser_class_init (NautilusFileChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = nautilus_file_chooser_constructed;
    object_class->dispose = nautilus_file_chooser_dispose;
    object_class->get_property = nautilus_file_chooser_get_property;
    object_class->set_property = nautilus_file_chooser_set_property;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-file-chooser.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, selector_view);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, places_sidebar);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, toolbar);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, slot_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filters_dropdown);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, choices_menu_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, read_only_checkbox);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, selector_accept_button);

    properties[PROP_MODE] =
        g_param_spec_enum ("mode", NULL, NULL,
                           NAUTILUS_TYPE_MODE, NAUTILUS_MODE_OPEN_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

NautilusFileChooser *
nautilus_file_chooser_new (NautilusMode mode)
{
    g_assert (mode != NAUTILUS_MODE_BROWSE);

    return g_object_new (NAUTILUS_TYPE_FILE_CHOOSER,
                         "mode", mode,
                         NULL);
}
