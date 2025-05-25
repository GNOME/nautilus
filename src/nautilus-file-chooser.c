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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gtk/nautilusgtkplacessidebarprivate.h"

#include "nautilus-directory.h"
#include "nautilus-enum-types.h"
#include "nautilus-file.h"
#include "nautilus-filename-utilities.h"
#include "nautilus-filename-validator.h"
#include "nautilus-global-preferences.h"
#include "nautilus-scheme.h"
#include "nautilus-shortcut-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-view-item-filter.h"
#include "nautilus-window-slot.h"

#define FILTER_WIDTH_CHARS 12

struct _NautilusFileChooser
{
    AdwWindow parent_instance;

    NautilusMode mode;
    char *accept_label;
    char *suggested_name;
    gboolean flag_initial_focus_done;

    GtkWidget *split_view;
    GtkWidget *places_sidebar;
    NautilusToolbar *toolbar;
    AdwBin *slot_container;
    NautilusWindowSlot *slot;
    GtkDropDown *filters_dropdown;
    GtkWidget *choices_menu_button;
    GtkWidget *read_only_checkbox;
    GtkWidget *accept_button;
    GtkWidget *filename_widget;
    GtkWidget *filename_button_container;
    GtkWidget *filename_undo_button;
    GtkWidget *filename_entry;
    GtkWidget *new_folder_button;
    GtkWidget *title_widget;

    NautilusFilenameValidator *validator;
    AdwBreakpoint *breakpoint;
};

G_DEFINE_FINAL_TYPE (NautilusFileChooser, nautilus_file_chooser, ADW_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_MODE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum
{
    SIGNAL_ACCEPTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void open_filename_entry (NautilusFileChooser *self);

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
        case NAUTILUS_MODE_SAVE_FILE:
        {
            gboolean is_folder = nautilus_file_opens_in_view (NAUTILUS_FILE (files->data));

            return (exactly_one_item && !is_folder);
        }

        case NAUTILUS_MODE_OPEN_FOLDER:
        {
            gboolean is_folder = nautilus_file_opens_in_view (NAUTILUS_FILE (files->data));

            return (exactly_one_item && is_folder);
        }

        case NAUTILUS_MODE_SAVE_FILES:
        {
            return FALSE;
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
        case NAUTILUS_MODE_OPEN_FOLDERS:
        case NAUTILUS_MODE_SAVE_FILE:
        case NAUTILUS_MODE_SAVE_FILES:
        {
            g_autofree char *scheme = g_file_get_uri_scheme (location);

            return !nautilus_scheme_is_internal (scheme);
        }

        case NAUTILUS_MODE_OPEN_FILE:
        case NAUTILUS_MODE_OPEN_FILES:
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
nautilus_file_chooser_can_accept (NautilusFileChooser *self,
                                  GList               *files,
                                  GFile               *location,
                                  gboolean             filename_passed)
{
    if (self->mode == NAUTILUS_MODE_SAVE_FILE)
    {
        return (filename_passed &&
                mode_can_accept_current_directory (self->mode, location));
    }
    else
    {
        return (mode_can_accept_files (self->mode, files) ||
                mode_can_accept_current_directory (self->mode, location));
    }
}

static void
emit_accepted (NautilusFileChooser *self,
               GList               *file_locations)
{
    g_signal_emit (self, signals[SIGNAL_ACCEPTED], 0,
                   file_locations,
                   GTK_FILE_FILTER (gtk_drop_down_get_selected_item (self->filters_dropdown)));
}

static void
on_overwrite_confirm_response (AdwAlertDialog      *dialog,
                               GAsyncResult        *result,
                               NautilusFileChooser *self)
{
    const char *response = adw_alert_dialog_choose_finish (dialog, result);

    if (g_strcmp0 (response, "replace") == 0)
    {
        GFile *parent_location = nautilus_window_slot_get_location (self->slot);
        g_autofree char *new_filename = nautilus_filename_validator_get_new_name (self->validator);
        g_autoptr (GFile) new_file_location = g_file_get_child (parent_location, new_filename);

        emit_accepted (self, &(GList){ .data = new_file_location });
    }
    else
    {
        /* The user probably wants to rename the file name, so focus entry */
        open_filename_entry (self);
    }
}

static void
ask_confirm_overwrite (NautilusFileChooser *self)
{
    AdwDialog *dialog = adw_alert_dialog_new (_("Replace When Saving?"), NULL);
    g_autofree char *filename = nautilus_filename_validator_get_new_name (self->validator);
    g_autoptr (NautilusFile) directory_as_file = nautilus_file_get (nautilus_window_slot_get_location (self->slot));
    const char *directory_name = nautilus_file_get_display_name (directory_as_file);

    adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                  _("“%s” already exists in “%s”. Saving will replace or overwrite its contents."),
                                  filename, directory_name);

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "replace", _("_Replace"),
                                    NULL);

    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                              "replace",
                                              ADW_RESPONSE_DESTRUCTIVE);

    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

    adw_alert_dialog_choose (ADW_ALERT_DIALOG (dialog), GTK_WIDGET (self),
                             NULL, (GAsyncReadyCallback) on_overwrite_confirm_response, self);
}

static GFile *
get_file_chooser_activation_location (NautilusFile *file)
{
    g_autoptr (GFile) location = nautilus_file_get_location (file);
    const gchar *path = g_file_peek_path (location);

    if (path != NULL)
    {
        return g_steal_pointer (&location);
    }

    return nautilus_file_get_activation_location (file);
}

static void
on_accept_button_clicked (NautilusFileChooser *self)
{
    GList *selection = nautilus_window_slot_get_selection (self->slot);

    if (self->mode == NAUTILUS_MODE_SAVE_FILE)
    {
        if (nautilus_filename_validator_get_will_overwrite (self->validator))
        {
            ask_confirm_overwrite (self);
        }
        else
        {
            GFile *parent_location = nautilus_window_slot_get_location (self->slot);
            g_autofree char *name = nautilus_filename_validator_get_new_name (self->validator);
            g_autoptr (GFile) file_location = g_file_get_child (parent_location, name);

            emit_accepted (self, &(GList){ .data = file_location });
        }
    }
    else
    {
        if (mode_can_accept_files (self->mode, selection))
        {
            g_autolist (GFile) file_locations = g_list_copy_deep (selection,
                                                                  (GCopyFunc) get_file_chooser_activation_location,
                                                                  NULL);

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
on_validator_has_feedback_changed (NautilusFileChooser *self)
{
    gboolean has_feedback;

    g_object_get (self->validator,
                  "has-feedback", &has_feedback,
                  NULL);
    if (has_feedback)
    {
        gtk_widget_add_css_class (self->filename_entry, "warning");
    }
    else
    {
        gtk_widget_remove_css_class (self->filename_entry, "warning");
    }
}

static void
on_validator_will_overwrite_changed (NautilusFileChooser *self)
{
    if (nautilus_filename_validator_get_will_overwrite (self->validator))
    {
        gtk_widget_remove_css_class (self->accept_button, "suggested-action");
        gtk_widget_add_css_class (self->accept_button, "destructive-action");
        gtk_button_set_label (GTK_BUTTON (self->accept_button), _("_Replace"));
    }
    else
    {
        gtk_widget_remove_css_class (self->accept_button, "destructive-action");
        gtk_widget_add_css_class (self->accept_button, "suggested-action");
        gtk_button_set_label (GTK_BUTTON (self->accept_button), self->accept_label);
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
on_file_drop (GtkDropTarget *target,
              const GValue  *value,
              gdouble        x,
              gdouble        y,
              gpointer       user_data)
{
    GSList *locations = g_value_get_boxed (value);
    g_autolist (NautilusFile) selection = NULL;
    g_autoptr (GFile) location = NULL;
    NautilusFileChooser *self = user_data;

    for (GSList *l = locations; l != NULL; l = l->next)
    {
        selection = g_list_prepend (selection, nautilus_file_get (l->data));
    }

    selection = g_list_reverse (selection);

    if (nautilus_file_opens_in_view (selection->data) &&
        self->mode != NAUTILUS_MODE_OPEN_FOLDER &&
        self->mode != NAUTILUS_MODE_OPEN_FOLDERS)
    {
        /* If it's a folder go into the folder unless you want to open that folder */
        location = g_object_ref (locations->data);
    }
    else
    {
        location = g_file_get_parent (locations->data);
    }

    nautilus_window_slot_open_location_full (self->slot, location, 0, selection);
}

static void
on_slot_activate_files (NautilusFileChooser *self,
                        GList               *files)
{
    if (mode_can_accept_files (self->mode, files))
    {
        gtk_widget_activate (self->accept_button);
    }
}

static void
on_filename_entry_focus_leave (NautilusFileChooser *self)
{
    /* The filename entry is a transient: it should hide when it loses focus.
     *
     * However, if we lose focus because the window itself lost focus, then the
     * filename entry should persist, because this may happen due to the user
     * switching keyboard layout/input method; or they may want to copy/drop
     * an path from another window/app. We detect this case by looking at the
     * focus widget of the window (GtkRoot).
     */

    GtkWidget *focus_widget = gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (self)));
    if (focus_widget != NULL &&
        gtk_widget_is_ancestor (focus_widget, GTK_WIDGET (self->filename_entry)))
    {
        return;
    }

    gtk_stack_set_visible_child (GTK_STACK (self->filename_widget),
                                 self->filename_button_container);
}

static void
on_filename_entry_changed (NautilusFileChooser *self)
{
    const char *current_text = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
    gboolean is_not_suggested_text = (g_strcmp0 (self->suggested_name, current_text) != 0 &&
                                      self->suggested_name != NULL);

    gtk_widget_set_visible (self->filename_undo_button, is_not_suggested_text);

    nautilus_filename_validator_validate (self->validator);
}

static void
open_filename_entry (NautilusFileChooser *self)
{
    gtk_stack_set_visible_child (GTK_STACK (self->filename_widget),
                                 self->filename_entry);
    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->filename_entry));

    const char *filename = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
    int extension_offset = nautilus_filename_get_extension_char_offset (filename);
    gtk_editable_select_region (GTK_EDITABLE (self->filename_entry), 0, extension_offset);
}

static void
on_filename_undo_button_clicked (NautilusFileChooser *self)
{
    gtk_editable_set_text (GTK_EDITABLE (self->filename_entry), self->suggested_name);

    nautilus_window_slot_open_location_full (self->slot, nautilus_window_slot_get_location (self->slot), 0, NULL);
}

static void
on_slot_selection_notify (NautilusFileChooser *self)
{
    g_return_if_fail (self->mode == NAUTILUS_MODE_SAVE_FILE);

    GList *selection = nautilus_window_slot_get_selection (self->slot);

    if (mode_can_accept_files (self->mode, selection))
    {
        NautilusFile *file = NAUTILUS_FILE (selection->data);

        gtk_editable_set_text (GTK_EDITABLE (self->filename_entry),
                               nautilus_file_get_edit_name (file));
    }
}

static void
on_location_changed (NautilusFileChooser *self)
{
    if (self->mode != NAUTILUS_MODE_SAVE_FILE)
    {
        return;
    }

    g_autoptr (NautilusDirectory) directory = NULL;
    GFile *location = nautilus_window_slot_get_location (self->slot);
    g_autofree char *scheme = g_file_get_uri_scheme (location);

    if (nautilus_scheme_is_internal (scheme))
    {
        return;
    }

    directory = nautilus_directory_get (location);
    nautilus_filename_validator_set_containing_directory (self->validator, directory);
    nautilus_filename_validator_validate (self->validator);
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
on_click_gesture_pressed (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    NautilusFileChooser *self = user_data;
    guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (nautilus_global_preferences_get_use_extra_buttons () &&
        (button == nautilus_global_preferences_get_back_button ()))
    {
        nautilus_window_slot_back_or_forward (self->slot, TRUE, 0);
    }
    else if (nautilus_global_preferences_get_use_extra_buttons () &&
             (button == nautilus_global_preferences_get_forward_button ()))
    {
        nautilus_window_slot_back_or_forward (self->slot, FALSE, 0);
    }
}

static int
get_filter_width_chars (GtkListItem *listitem,
                        const char  *name)
{
    return MIN (g_utf8_strlen (name, -1), FILTER_WIDTH_CHARS);
}

static void
update_dropdown_checkmark (GtkDropDown *dropdown,
                           GParamSpec  *psepc,
                           GtkListItem *list_item)
{
    guint selected = gtk_drop_down_get_selected (dropdown);
    GtkWidget *cell = gtk_list_item_get_child (list_item);
    GtkWidget *check_mark = gtk_widget_get_last_child (cell);
    gdouble opacity = (selected == gtk_list_item_get_position (list_item)) ? 1 : 0;

    gtk_widget_set_opacity (check_mark, opacity);
}

static void
filters_dropdown_setup (GtkListItemFactory *factory,
                        GtkListItem        *list_item,
                        gpointer            user_data)
{
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *label = gtk_label_new (NULL);
    GtkWidget *icon;

    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_append (GTK_BOX (box), label);
    icon = g_object_new (GTK_TYPE_IMAGE,
                         "icon-name", "object-select-symbolic",
                         "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                         NULL);
    gtk_box_append (GTK_BOX (box), icon);
    gtk_list_item_set_child (list_item, box);

    g_assert (gtk_widget_get_first_child (box) == label &&
              gtk_widget_get_last_child (box) == icon);
}

static void
filters_dropdown_bind (GtkListItemFactory *factory,
                       GtkListItem        *list_item,
                       gpointer            user_data)
{
    NautilusFileChooser *self = user_data;
    GtkFileFilter *filter = gtk_list_item_get_item (list_item);
    GtkWidget *cell = gtk_list_item_get_child (list_item);
    GtkWidget *label = gtk_widget_get_first_child (cell);

    gtk_label_set_label (GTK_LABEL (label), gtk_file_filter_get_name (filter));

    g_signal_connect (self->filters_dropdown, "notify::selected", G_CALLBACK (update_dropdown_checkmark), list_item);
    update_dropdown_checkmark (self->filters_dropdown, NULL, list_item);
}

static void
filters_dropdown_unbind (GtkListItemFactory *factory,
                         GtkListItem        *list_item,
                         gpointer            user_data)
{
    NautilusFileChooser *self = user_data;

    g_signal_handlers_disconnect_by_func (self->filters_dropdown, update_dropdown_checkmark, list_item);
}

static gboolean
title_widget_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip,
                            gpointer    user_data)
{
    PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));

    if (pango_layout_is_ellipsized (layout))
    {
        const char *label = gtk_label_get_label (GTK_LABEL (widget));
        gtk_tooltip_set_text (tooltip, label);
        return TRUE;
    }

    return FALSE;
}


static void
nautilus_file_chooser_dispose (GObject *object)
{
    NautilusFileChooser *self = (NautilusFileChooser *) object;

    if (self->slot != NULL)
    {
        g_assert (adw_bin_get_child (self->slot_container) == GTK_WIDGET (self->slot));

        nautilus_window_slot_set_active (self->slot, FALSE);
        /* Let bindings on AdwToolbarView:content react to the slot being unset
         * while the slot itself is still alive. */
        adw_bin_set_child (self->slot_container, NULL);
        g_clear_object (&self->slot);
    }

    G_OBJECT_CLASS (nautilus_file_chooser_parent_class)->dispose (object);
}

static void
nautilus_file_chooser_finalize (GObject *object)
{
    NautilusFileChooser *self = (NautilusFileChooser *) object;

    g_free (self->accept_label);
    g_free (self->suggested_name);

    G_OBJECT_CLASS (nautilus_file_chooser_parent_class)->finalize (object);
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

static gboolean
nautilus_file_chooser_grab_focus (GtkWidget *widget)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (widget);

    if (self->mode == NAUTILUS_MODE_SAVE_FILE && !self->flag_initial_focus_done)
    {
        self->flag_initial_focus_done = TRUE;
        open_filename_entry (self);
    }
    else if (self->slot != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (self->slot));
    }

    return GTK_WIDGET_CLASS (nautilus_file_chooser_parent_class)->grab_focus (widget);
}

static void
nautilus_file_chooser_constructed (GObject *object)
{
    G_OBJECT_CLASS (nautilus_file_chooser_parent_class)->constructed (object);

    NautilusFileChooser *self = (NautilusFileChooser *) object;

    /* Setup slot.
     * We hold a reference to control its lifetime with relation to bindings. */
    self->slot = g_object_ref (nautilus_window_slot_new (self->mode));
    g_signal_connect_swapped (self->slot, "notify::location", G_CALLBACK (on_location_changed), self);
    adw_bin_set_child (self->slot_container, GTK_WIDGET (self->slot));
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

    gtk_widget_set_visible (self->filename_widget,
                            (self->mode == NAUTILUS_MODE_SAVE_FILE));

    gtk_widget_set_visible (self->new_folder_button,
                            (self->mode == NAUTILUS_MODE_SAVE_FILE ||
                             self->mode == NAUTILUS_MODE_SAVE_FILES ||
                             self->mode == NAUTILUS_MODE_OPEN_FOLDER ||
                             self->mode == NAUTILUS_MODE_OPEN_FOLDERS));

    /* Add the setter here once the new folder property is set */
    adw_breakpoint_add_setters (self->breakpoint, G_OBJECT (self->toolbar),
                                "show-new-folder-button", FALSE, NULL);

    if (self->mode == NAUTILUS_MODE_SAVE_FILE)
    {
        g_signal_connect_object (self->slot, "notify::selection",
                                 G_CALLBACK (on_slot_selection_notify), self,
                                 G_CONNECT_SWAPPED);
    }

    if (g_strcmp0 (PROFILE, "") != 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
    }

    gtk_widget_add_css_class (self->title_widget, "windowtitle");
    gtk_widget_add_css_class (self->title_widget, "title");
    g_signal_connect (self->title_widget, "query-tooltip",
                      G_CALLBACK (title_widget_query_tooltip), self);

    int width, height;
    g_settings_get (nautilus_window_state, NAUTILUS_WINDOW_STATE_INITIAL_SIZE_FILE_CHOOSER,
                    "(ii)", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (self), width, height);
}

static void
nautilus_file_chooser_init (NautilusFileChooser *self)
{
    g_type_ensure (NAUTILUS_TYPE_FILENAME_VALIDATOR);
    g_type_ensure (NAUTILUS_TYPE_TOOLBAR);
    g_type_ensure (NAUTILUS_TYPE_GTK_PLACES_SIDEBAR);
    g_type_ensure (NAUTILUS_TYPE_SHORTCUT_MANAGER);
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Give the dialog its own window group. Otherwise all such dialogs would
     * belong to the same default window group, acting like a stack of modals.
     */
    g_autoptr (GtkWindowGroup) window_group = gtk_window_group_new ();
    gtk_window_group_add_window (window_group, GTK_WINDOW (self));

    /* Setup sidebar */
    nautilus_gtk_places_sidebar_set_open_flags (NAUTILUS_GTK_PLACES_SIDEBAR (self->places_sidebar),
                                                NAUTILUS_OPEN_FLAG_NORMAL);
    nautilus_gtk_places_sidebar_set_show_trash (NAUTILUS_GTK_PLACES_SIDEBAR (self->places_sidebar),
                                                FALSE);

    GtkEventController *controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_key_pressed_bubble), self);
    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_click_gesture_pressed), self);

    /* The factory is set in the ui, but we need to set the popup (list) factory
     * in code to make the checkmark appear correctly. */
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (filters_dropdown_setup), self);
    g_signal_connect (factory, "bind", G_CALLBACK (filters_dropdown_bind), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (filters_dropdown_unbind), self);
    gtk_drop_down_set_list_factory (self->filters_dropdown, factory);
}

static void
nautilus_file_chooser_class_init (NautilusFileChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = nautilus_file_chooser_constructed;
    object_class->dispose = nautilus_file_chooser_dispose;
    object_class->finalize = nautilus_file_chooser_finalize;
    object_class->get_property = nautilus_file_chooser_get_property;
    object_class->set_property = nautilus_file_chooser_set_property;

    widget_class->grab_focus = nautilus_file_chooser_grab_focus;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-file-chooser.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, split_view);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, places_sidebar);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, toolbar);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, slot_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filters_dropdown);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, choices_menu_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, accept_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filename_widget);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filename_button_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filename_undo_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, filename_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, new_folder_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, validator);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, title_widget);
    gtk_widget_class_bind_template_child (widget_class, NautilusFileChooser, breakpoint);

    gtk_widget_class_bind_template_callback (widget_class, nautilus_file_chooser_can_accept);
    gtk_widget_class_bind_template_callback (widget_class, on_accept_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, open_filename_entry);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_undo_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_entry_focus_leave);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_validate);
    gtk_widget_class_bind_template_callback (widget_class, on_validator_has_feedback_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_validator_will_overwrite_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_file_drop);
    gtk_widget_class_bind_template_callback (widget_class, get_filter_width_chars);

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
                      G_TYPE_NONE, 2,
                      G_TYPE_POINTER, GTK_TYPE_FILE_FILTER);
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
    g_set_str (&self->accept_label, accept_label);

    gtk_button_set_label (GTK_BUTTON (self->accept_button), accept_label);
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
        location_to_open = g_object_ref (starting_location);
    }
    else
    {
        location_to_open = g_file_new_for_path (g_get_home_dir ());
    }

    nautilus_window_slot_open_location_full (self->slot, location_to_open, 0, NULL);
}

void
nautilus_file_chooser_set_suggested_name (NautilusFileChooser *self,
                                          const char          *suggested_name)
{
    g_set_str (&self->suggested_name, suggested_name);

    if (suggested_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->filename_entry), suggested_name);
    }
}

void
nautilus_file_chooser_add_choices (NautilusFileChooser *self,
                                   GActionGroup        *action_group,
                                   GMenuModel          *menu)
{
    gboolean visible = g_menu_model_get_n_items (menu) > 0;

    gtk_widget_insert_action_group (GTK_WIDGET (self), "choices", action_group);
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->choices_menu_button), menu);

    gtk_widget_set_visible (self->choices_menu_button, visible);
}
