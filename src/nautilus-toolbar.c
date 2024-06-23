/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "nautilus-toolbar.h"

#include <glib/gi18n.h>
#include <math.h>

#include "nautilus-application.h"
#include "nautilus-bookmark.h"
#include "nautilus-global-preferences.h"
#include "nautilus-history-controls.h"
#include "nautilus-location-entry.h"
#include "nautilus-pathbar.h"
#include "nautilus-view-controls.h"
#include "nautilus-ui-utilities.h"

struct _NautilusToolbar
{
    AdwBin parent_instance;

    AdwHeaderBar *header_bar;
    GtkWidget *history_controls_stack;
    GtkWidget *history_controls;
    GtkWidget *history_controls_placeholder;
    GtkWidget *location_entry_container;
    GtkWidget *search_container;
    GtkWidget *toolbar_switcher;
    GtkWidget *path_bar;
    GtkWidget *location_entry;
    GtkWidget *search_button_stack;
    GtkWidget *search_button;
    GtkWidget *search_button_placeholder;
    GtkWidget *new_folder_button;

    gboolean show_location_entry;
    GtkWidget *focus_before_location_entry;

    GtkWidget *sidebar_button;
    gboolean show_sidebar_button;
    gboolean sidebar_button_active;

    gboolean show_toolbar_children;

    /* active slot & bindings */
    NautilusWindowSlot *window_slot;
};

enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    PROP_SHOW_SIDEBAR_BUTTON,
    PROP_SIDEBAR_BUTTON_ACTIVE,
    PROP_SHOW_TOOLBAR_CHILDREN,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusToolbar, nautilus_toolbar, ADW_TYPE_BIN);

static void nautilus_toolbar_set_window_slot_real (NautilusToolbar    *self,
                                                   NautilusWindowSlot *slot);
static void
toolbar_update_appearance (NautilusToolbar *self)
{
    gboolean show_location_entry;

    show_location_entry = self->show_location_entry ||
                          g_settings_get_boolean (nautilus_preferences,
                                                  NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

    if (self->window_slot != NULL &&
        nautilus_window_slot_get_search_visible (self->window_slot))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "search");
    }
    else if (show_location_entry)
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "location");
    }
    else
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "pathbar");
    }

    /* Adjust to global search mode. */
    gboolean search_global = (self->window_slot != NULL &&
                              nautilus_window_slot_get_search_global (self->window_slot));
    gtk_stack_set_visible_child (GTK_STACK (self->search_button_stack),
                                 search_global ? self->search_button_placeholder : self->search_button);
    gtk_stack_set_visible_child (GTK_STACK (self->history_controls_stack),
                                 search_global ? self->history_controls_placeholder : self->history_controls);

    if (self->window_slot != NULL)
    {
        NautilusMode mode = nautilus_window_slot_get_mode (self->window_slot);
        gboolean show_title_buttons = (mode != NAUTILUS_MODE_SAVE_FILE);

        adw_header_bar_set_show_start_title_buttons (self->header_bar, show_title_buttons);
        adw_header_bar_set_show_end_title_buttons (self->header_bar, show_title_buttons);

        gtk_widget_set_visible (self->new_folder_button, (mode == NAUTILUS_MODE_SAVE_FILE ||
                                                          mode == NAUTILUS_MODE_SAVE_FILES));
    }
}

static void
nautilus_toolbar_open_location_entry (NautilusToolbar *self,
                                      const char      *special_text)
{
    if (self->show_location_entry)
    {
        return;
    }

    /* Remember focus widget. */
    GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
    GtkWidget *focus_widget = (root != NULL) ? gtk_root_get_focus (root) : NULL;

    g_set_weak_pointer (&self->focus_before_location_entry, focus_widget);

    self->show_location_entry = TRUE;
    toolbar_update_appearance (self);

    gtk_widget_grab_focus (self->location_entry);

    if (special_text != NULL)
    {
        nautilus_location_entry_set_special_text (NAUTILUS_LOCATION_ENTRY (self->location_entry),
                                                  special_text);
        gtk_editable_set_position (GTK_EDITABLE (self->location_entry), -1);
    }
}

static void
nautilus_toolbar_close_location_entry (NautilusToolbar *self)
{
    if (!self->show_location_entry)
    {
        return;
    }

    self->show_location_entry = FALSE;
    toolbar_update_appearance (self);

    if (self->focus_before_location_entry != NULL)
    {
        /* Restore focus widget. */
        gtk_widget_grab_focus (self->focus_before_location_entry);
        g_clear_weak_pointer (&self->focus_before_location_entry);
    }
}

static void
on_path_bar_open_location (NautilusPathBar   *path_bar,
                           GFile             *location,
                           NautilusOpenFlags  open_flags,
                           gpointer           user_data)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (user_data);

    if (open_flags & (NAUTILUS_OPEN_FLAG_NEW_WINDOW | NAUTILUS_OPEN_FLAG_NEW_TAB))
    {
        nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                 location, open_flags, NULL, NULL, NULL);
    }
    else
    {
        nautilus_window_slot_open_location_full (self->window_slot, location, open_flags, NULL);
    }
}

static void
on_location_entry_focus_leave (GtkEventControllerFocus *controller,
                               gpointer                 user_data)
{
    NautilusToolbar *toolbar;
    GtkWidget *focus_widget;

    toolbar = NAUTILUS_TOOLBAR (user_data);

    /* The location entry is a transient: it should hide when it loses focus.
     *
     * However, if we lose focus because the window itself lost focus, then the
     * location entry should persist, because this may happen due to the user
     * switching keyboard layout/input method; or they may want to copy/drop
     * an path from another window/app. We detect this case by looking at the
     * focus widget of the window (GtkRoot).
     */

    focus_widget = gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (toolbar)));
    if (focus_widget != NULL &&
        gtk_widget_is_ancestor (focus_widget, GTK_WIDGET (toolbar->location_entry)))
    {
        return;
    }

    nautilus_toolbar_close_location_entry (toolbar);
}

static void
on_location_entry_location_changed (NautilusLocationEntry *entry,
                                    GFile                 *location,
                                    gpointer               user_data)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (user_data);

    nautilus_toolbar_close_location_entry (self);
    nautilus_window_slot_open_location_full (self->window_slot, location, 0, NULL);
}

static void
action_edit_location (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (widget);

    nautilus_toolbar_open_location_entry (self, NULL);
}

static void
action_prompt_root_location (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (widget);

    nautilus_toolbar_open_location_entry (self, "/");
}

static void
action_prompt_home_location (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (widget);

    nautilus_toolbar_open_location_entry (self, "~");
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
    g_type_ensure (NAUTILUS_TYPE_HISTORY_CONTROLS);
    g_type_ensure (NAUTILUS_TYPE_VIEW_CONTROLS);
    g_type_ensure (NAUTILUS_TYPE_PATH_BAR);
    g_type_ensure (NAUTILUS_TYPE_LOCATION_ENTRY);

    gtk_widget_init_template (GTK_WIDGET (self));

    GtkShortcutController *shortcuts = GTK_SHORTCUT_CONTROLLER (gtk_shortcut_controller_new ());

    gtk_shortcut_controller_set_scope (shortcuts, GTK_SHORTCUT_SCOPE_MANAGED);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (shortcuts));

#define ADD_SHORTCUT_FOR_ACTION(controller, action, trigger) \
        (gtk_shortcut_controller_add_shortcut ((controller), \
                                               gtk_shortcut_new (gtk_shortcut_trigger_parse_string ((trigger)), \
                                                                 gtk_named_action_new ((action)))))

    ADD_SHORTCUT_FOR_ACTION (shortcuts, "toolbar.edit-location", "<control>l|Go|OpenURL");
    ADD_SHORTCUT_FOR_ACTION (shortcuts, "toolbar.prompt-root-location", "slash|KP_Divide");
    /* Support keyboard layouts which have a dead tilde key but not a tilde key. */
    ADD_SHORTCUT_FOR_ACTION (shortcuts, "toolbar.prompt-home-location", "asciitilde|dead_tilde");

#undef ADD_SHORTCUT_FOR_ACTION

    /* Setup path bar */
    g_signal_connect_object (self->path_bar, "open-location",
                             G_CALLBACK (on_path_bar_open_location), self,
                             G_CONNECT_DEFAULT);

    /* Setup location entry */
    GtkEventController *controller = gtk_event_controller_focus_new ();

    gtk_widget_add_controller (self->location_entry, controller);
    g_signal_connect (controller, "leave",
                      G_CALLBACK (on_location_entry_focus_leave), self);

    /* Setting a max width on one entry to effectively set a max expansion for
     * the whole title widget. */
    gtk_editable_set_max_width_chars (GTK_EDITABLE (self->location_entry), 88);
    g_signal_connect_object (self->location_entry, "location-changed",
                             G_CALLBACK (on_location_entry_location_changed), self,
                             G_CONNECT_DEFAULT);
    g_signal_connect_object (self->location_entry, "cancel",
                             G_CALLBACK (nautilus_toolbar_close_location_entry), self,
                             G_CONNECT_SWAPPED);

    toolbar_update_appearance (self);
}

static void
nautilus_toolbar_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    switch (property_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_value_set_object (value, self->window_slot);
        }
        break;

        case PROP_SHOW_SIDEBAR_BUTTON:
        {
            g_value_set_boolean (value, self->show_sidebar_button);
        }
        break;

        case PROP_SIDEBAR_BUTTON_ACTIVE:
        {
            g_value_set_boolean (value, self->sidebar_button_active);
        }
        break;

        case PROP_SHOW_TOOLBAR_CHILDREN:
        {
            g_value_set_boolean (value, self->show_toolbar_children);
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
on_window_slot_destroyed (gpointer  data,
                          GObject  *where_the_object_was)
{
    NautilusToolbar *self;

    self = NAUTILUS_TOOLBAR (data);

    nautilus_toolbar_set_window_slot_real (self, NULL);
}

static void
nautilus_toolbar_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    switch (property_id)
    {
        case PROP_WINDOW_SLOT:
        {
            nautilus_toolbar_set_window_slot (self, g_value_get_object (value));
        }
        break;

        case PROP_SHOW_SIDEBAR_BUTTON:
        {
            self->show_sidebar_button = g_value_get_boolean (value);
        }
        break;

        case PROP_SIDEBAR_BUTTON_ACTIVE:
        {
            self->sidebar_button_active = g_value_get_boolean (value);
        }
        break;

        case PROP_SHOW_TOOLBAR_CHILDREN:
        {
            self->show_toolbar_children = g_value_get_boolean (value);
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
nautilus_toolbar_dispose (GObject *object)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    adw_bin_set_child (ADW_BIN (object), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_TOOLBAR);

    G_OBJECT_CLASS (nautilus_toolbar_parent_class)->dispose (object);
}

static void
nautilus_toolbar_finalize (GObject *obj)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

    g_clear_weak_pointer (&self->focus_before_location_entry);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          toolbar_update_appearance, self);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
        self->window_slot = NULL;
    }

    G_OBJECT_CLASS (nautilus_toolbar_parent_class)->finalize (obj);
}

static void
nautilus_toolbar_class_init (NautilusToolbarClass *klass)
{
    GObjectClass *oclass;
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);
    oclass->get_property = nautilus_toolbar_get_property;
    oclass->set_property = nautilus_toolbar_set_property;
    oclass->dispose = nautilus_toolbar_dispose;
    oclass->finalize = nautilus_toolbar_finalize;

    properties [PROP_WINDOW_SLOT] =
        g_param_spec_object ("window-slot",
                             "Window slot currently active",
                             "Window slot currently acive",
                             NAUTILUS_TYPE_WINDOW_SLOT,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));

    properties[PROP_SHOW_SIDEBAR_BUTTON] =
        g_param_spec_boolean ("show-sidebar-button", NULL, NULL, FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SIDEBAR_BUTTON_ACTIVE] =
        g_param_spec_boolean ("sidebar-button-active", NULL, NULL, FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SHOW_TOOLBAR_CHILDREN] =
        g_param_spec_boolean ("show-toolbar-children", NULL, NULL, TRUE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-toolbar.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, header_bar);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, history_controls_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, history_controls);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, history_controls_placeholder);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, toolbar_switcher);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, path_bar);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, location_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_button_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_button_placeholder);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, new_folder_button);

    gtk_widget_class_bind_template_callback (widget_class, nautilus_toolbar_close_location_entry);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOOLBAR);

    gtk_widget_class_install_action (widget_class, "toolbar.edit-location", NULL, action_edit_location);
    gtk_widget_class_install_action (widget_class, "toolbar.prompt-root-location", NULL, action_prompt_root_location);
    gtk_widget_class_install_action (widget_class, "toolbar.prompt-home-location", NULL, action_prompt_home_location);
}

GtkWidget *
nautilus_toolbar_new (void)
{
    return g_object_new (NAUTILUS_TYPE_TOOLBAR,
                         NULL);
}

static void
box_remove_all_children (GtkBox *box)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (GTK_WIDGET (box))) != NULL)
    {
        gtk_box_remove (GTK_BOX (box), child);
    }
}

static void
on_slot_location_changed (NautilusToolbar *self)
{
    g_assert (self->window_slot != NULL);

    GFile *location = nautilus_window_slot_get_location (self->window_slot);

    if (location == NULL)
    {
        return;
    }

    nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (self->location_entry), location);
    nautilus_path_bar_set_path (NAUTILUS_PATH_BAR (self->path_bar), location);
}

static void
slot_on_extensions_background_menu_changed (NautilusToolbar    *self,
                                            GParamSpec         *param,
                                            NautilusWindowSlot *slot)
{
    g_autoptr (GMenuModel) menu = NULL;

    menu = nautilus_window_slot_get_extensions_background_menu (slot);
    nautilus_path_bar_set_extensions_background_menu (NAUTILUS_PATH_BAR (self->path_bar),
                                                      menu);
}

static void
slot_on_templates_menu_changed (NautilusToolbar    *self,
                                GParamSpec         *param,
                                NautilusWindowSlot *slot)
{
    g_autoptr (GMenuModel) menu = NULL;

    menu = nautilus_window_slot_get_templates_menu (slot);
    nautilus_path_bar_set_templates_menu (NAUTILUS_PATH_BAR (self->path_bar),
                                          menu);
}

void
nautilus_toolbar_show_current_location_menu (NautilusToolbar *self)
{
    nautilus_path_bar_show_current_location_menu (NAUTILUS_PATH_BAR (self->path_bar));
}

/* Called from on_window_slot_destroyed(), since bindings and signal handlers
 * are automatically removed once the slot goes away.
 */
static void
nautilus_toolbar_set_window_slot_real (NautilusToolbar    *self,
                                       NautilusWindowSlot *slot)
{
    self->window_slot = slot;

    if (self->window_slot != NULL)
    {
        g_object_weak_ref (G_OBJECT (self->window_slot),
                           on_window_slot_destroyed,
                           self);

        on_slot_location_changed (self);

        g_signal_connect_swapped (self->window_slot, "notify::location",
                                  G_CALLBACK (on_slot_location_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::extensions-background-menu",
                                  G_CALLBACK (slot_on_extensions_background_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::templates-menu",
                                  G_CALLBACK (slot_on_templates_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::search-visible",
                                  G_CALLBACK (toolbar_update_appearance), self);
        g_signal_connect_swapped (self->window_slot, "notify::search-global",
                                  G_CALLBACK (toolbar_update_appearance), self);
    }

    box_remove_all_children (GTK_BOX (self->search_container));

    if (self->window_slot != NULL)
    {
        gtk_box_append (GTK_BOX (self->search_container),
                        GTK_WIDGET (nautilus_window_slot_get_query_editor (self->window_slot)));
    }

    toolbar_update_appearance (self);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

void
nautilus_toolbar_set_window_slot (NautilusToolbar    *self,
                                  NautilusWindowSlot *window_slot)
{
    g_return_if_fail (NAUTILUS_IS_TOOLBAR (self));
    g_return_if_fail (window_slot == NULL || NAUTILUS_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
    }

    nautilus_toolbar_set_window_slot_real (self, window_slot);
}
