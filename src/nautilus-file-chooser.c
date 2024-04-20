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

#include "nautilus-directory.h"
#include "nautilus-enum-types.h"
#include "nautilus-file.h"
#include "nautilus-scheme.h"
#include "nautilus-shortcut-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-view-item-filter.h"
#include "nautilus-window-slot.h"

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

    NautilusDirectory *current_directory;
};

G_DEFINE_FINAL_TYPE (NautilusFileChooser, nautilus_file_chooser, ADW_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_MODE,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum
{
    SIGNAL_ACCEPTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean
mode_can_accept_files (NautilusMode  mode,
                       GList        *files)
{
    if (files == NULL)
    {
        return FALSE;
    }

    gboolean exactly_one_item = (files->next == NULL);

    switch (mode)
    {
        case NAUTILUS_MODE_OPEN_FILE:
        {
            gboolean is_folder = nautilus_file_opens_in_view (NAUTILUS_FILE (files->data));

            return (exactly_one_item && !is_folder);
        }

        case NAUTILUS_MODE_OPEN_FOLDER:
        {
            gboolean is_folder = nautilus_file_opens_in_view (NAUTILUS_FILE (files->data));

            return (exactly_one_item && is_folder);
        }

        case NAUTILUS_MODE_OPEN_FILES:
        case NAUTILUS_MODE_OPEN_FOLDERS:
        {
            for (GList *l = files; l != NULL; l = l->next)
            {
                gboolean is_folder = nautilus_file_opens_in_view (NAUTILUS_FILE (l->data));

                if (mode != (is_folder ?
                             NAUTILUS_MODE_OPEN_FOLDERS : NAUTILUS_MODE_OPEN_FILES))
                {
                    return FALSE;
                }
            }

            return TRUE;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }

    return FALSE;
}

static gboolean
mode_can_accept_current_directory (NautilusMode  mode,
                                   GFile        *location)
{
    if (location == NULL)
    {
        return FALSE;
    }

    switch (mode)
    {
        case NAUTILUS_MODE_OPEN_FOLDER:
        {
            g_autofree char *scheme = g_file_get_uri_scheme (location);

            return !nautilus_scheme_is_internal (scheme);
        }

        case NAUTILUS_MODE_OPEN_FILE:
        case NAUTILUS_MODE_OPEN_FILES:
        case NAUTILUS_MODE_OPEN_FOLDERS:
        {
            return FALSE;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }

    return FALSE;
}

static gboolean
selector_can_accept (NautilusFileChooser *self,
                     GList               *files,
                     GFile               *location)
{
    return (mode_can_accept_files (self->mode, files) ||
            mode_can_accept_current_directory (self->mode, location));
}

static void
emit_accepted (NautilusFileChooser *self,
               GList               *file_locations)
{
    gboolean writable = !gtk_check_button_get_active (GTK_CHECK_BUTTON (self->read_only_checkbox));

    g_signal_emit (self, signals[SIGNAL_ACCEPTED], 0,
                   file_locations,
                   GTK_FILE_FILTER (gtk_drop_down_get_selected_item (self->filters_dropdown)),
                   writable);
}

static void
update_current_directory (NautilusFileChooser *self,
                          GFile               *location)
{
    g_return_if_fail (G_IS_FILE (location));

    g_autoptr (NautilusDirectory) directory = nautilus_directory_get (location);

    g_set_object (&self->current_directory, directory);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURRENT_DIRECTORY]);
}

static void
on_selector_accept_button_clicked (NautilusFileChooser *self)
{
    GList *selection = nautilus_window_slot_get_selection (self->slot);

    if (self->mode == NAUTILUS_MODE_SAVE_FILE)
    {
        /* TODO */
    }
    else
    {
        if (mode_can_accept_files (self->mode, selection))
        {
            g_autolist (GFile) file_locations = g_list_copy_deep (selection, (GCopyFunc) nautilus_file_get_activation_location, NULL);

            emit_accepted (self, file_locations);
        }
        else
        {
            GFile *location = nautilus_window_slot_get_location (self->slot);

            emit_accepted (self, &(GList){ .data = location });
        }
    }
}

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
on_slot_activate_files (NautilusFileChooser *self,
                        GList               *files)
{
    if (mode_can_accept_files (self->mode, files))
    {
        if (self->mode == NAUTILUS_MODE_SAVE_FILE)
        {
            /* TODO */
        }
        else
        {
            g_autolist (GFile) file_locations = g_list_copy_deep (files, (GCopyFunc) nautilus_file_get_activation_location, NULL);

            emit_accepted (self, file_locations);
        }
    }
}

static gboolean
on_key_pressed_bubble (GtkEventControllerKey *controller,
                       unsigned int           keyval,
                       unsigned int           keycode,
                       GdkModifierType        state,
                       gpointer               user_data)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (user_data);

    if (self->slot != NULL &&
        nautilus_window_slot_handle_event (self->slot, controller, keyval, state))
    {
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
nautilus_file_chooser_dispose (GObject *object)
{
    NautilusFileChooser *self = (NautilusFileChooser *) object;

    g_clear_object (&self->current_directory);

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
        case PROP_CURRENT_DIRECTORY:
        {
            g_value_set_object (value, self->current_directory);
        }
        break;

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
    g_signal_connect_swapped (self->slot, "activate-files",
                              G_CALLBACK (on_slot_activate_files), self);

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

    GtkEventController *controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_key_pressed_bubble), self);
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

    gtk_widget_class_bind_template_callback (widget_class, selector_can_accept);
    gtk_widget_class_bind_template_callback (widget_class, on_selector_accept_button_clicked);

    properties[PROP_CURRENT_DIRECTORY] =
        g_param_spec_object ("current-directory", NULL, NULL,
                             NAUTILUS_TYPE_DIRECTORY,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_MODE] =
        g_param_spec_enum ("mode", NULL, NULL,
                           NAUTILUS_TYPE_MODE, NAUTILUS_MODE_OPEN_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[SIGNAL_ACCEPTED] =
        g_signal_new ("accepted",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 3,
                      G_TYPE_POINTER, GTK_TYPE_FILE_FILTER, G_TYPE_BOOLEAN);
}

NautilusFileChooser *
nautilus_file_chooser_new (NautilusMode mode)
{
    g_assert (mode != NAUTILUS_MODE_BROWSE);

    return g_object_new (NAUTILUS_TYPE_FILE_CHOOSER,
                         "mode", mode,
                         NULL);
}

void
nautilus_file_chooser_set_accept_label (NautilusFileChooser *self,
                                        const char          *accept_label)
{
    gtk_button_set_label (GTK_BUTTON (self->selector_accept_button), accept_label);
}

void
nautilus_file_chooser_set_current_filter (NautilusFileChooser *self,
                                          guint                position)
{
    gtk_drop_down_set_selected (self->filters_dropdown, position);
}

void
nautilus_file_chooser_set_filters (NautilusFileChooser *self,
                                   GListModel          *filters)
{
    gboolean has_filters = (filters != NULL && g_list_model_get_n_items (filters) != 0);

    gtk_drop_down_set_model (self->filters_dropdown, filters);
    gtk_widget_set_visible (GTK_WIDGET (self->filters_dropdown), has_filters);
}

void
nautilus_file_chooser_set_starting_location (NautilusFileChooser *self,
                                             GFile               *starting_location)
{
    g_autoptr (GFile) location_to_open = NULL;

    if (starting_location != NULL)
    {
        update_current_directory (self, starting_location);
        location_to_open = g_object_ref (starting_location);
    }
    else
    {
        location_to_open = g_file_new_for_path (g_get_home_dir ());
    }

    nautilus_window_slot_open_location_full (self->slot, location_to_open, 0, NULL);
}
