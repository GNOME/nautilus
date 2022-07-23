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
#include "nautilus-file-undo-manager.h"
#include "nautilus-global-preferences.h"
#include "nautilus-history-controls.h"
#include "nautilus-location-entry.h"
#include "nautilus-pathbar.h"
#include "nautilus-progress-indicator.h"
#include "nautilus-view-controls.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window.h"

struct _NautilusToolbar
{
    AdwBin parent_instance;

    NautilusWindow *window;

    GtkWidget *path_bar_container;
    GtkWidget *location_entry_container;
    GtkWidget *search_container;
    GtkWidget *toolbar_switcher;
    GtkWidget *path_bar;
    GtkWidget *location_entry;

    gboolean show_location_entry;
    gboolean location_entry_should_auto_hide;

    GtkWidget *app_button;
    GMenuModel *undo_redo_section;

    GtkWidget *sidebar_button;
    gboolean show_sidebar_button;
    gboolean sidebar_button_active;

    gboolean show_toolbar_children;

    GtkWidget *search_button;

    GtkWidget *location_entry_close_button;

    /* active slot & bindings */
    NautilusWindowSlot *window_slot;
    GBinding *search_binding;
};

enum
{
    PROP_WINDOW = 1,
    PROP_SHOW_LOCATION_ENTRY,
    PROP_WINDOW_SLOT,
    PROP_SEARCHING,
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
        nautilus_window_slot_get_searching (self->window_slot))
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
}

static void
update_action (NautilusToolbar *self,
               const char      *action_name,
               gboolean         enabled)
{
    GAction *action;

    /* Activate/deactivate */
    action = g_action_map_lookup_action (G_ACTION_MAP (self->window), action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
undo_manager_changed (NautilusToolbar *self)
{
    NautilusFileUndoInfo *info;
    NautilusFileUndoManagerState undo_state;
    gboolean undo_active;
    gboolean redo_active;
    g_autofree gchar *undo_label = NULL;
    g_autofree gchar *redo_label = NULL;
    g_autofree gchar *undo_description = NULL;
    g_autofree gchar *redo_description = NULL;
    gboolean is_undo;
    g_autoptr (GMenu) updated_section = g_menu_new ();
    g_autoptr (GMenuItem) menu_item = NULL;

    /* Look up the last action from the undo manager, and get the text that
     * describes it, e.g. "Undo Create Folder"/"Redo Create Folder"
     */
    info = nautilus_file_undo_manager_get_action ();
    undo_state = nautilus_file_undo_manager_get_state ();
    undo_active = redo_active = FALSE;
    if (info != NULL && undo_state > NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE)
    {
        is_undo = undo_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO;

        /* The last action can either be undone/redone. Activate the corresponding
         * menu item and deactivate the other
         */
        undo_active = is_undo;
        redo_active = !is_undo;
        nautilus_file_undo_info_get_strings (info, &undo_label, &undo_description,
                                             &redo_label, &redo_description);
    }

    /* Set the label of the undo and redo menu items, and activate them appropriately
     */
    if (!undo_active || undo_label == NULL)
    {
        g_free (undo_label);
        undo_label = g_strdup (_("_Undo"));
    }
    g_set_object (&menu_item, g_menu_item_new (undo_label, "win.undo"));
    g_menu_append_item (updated_section, menu_item);
    update_action (self, "undo", undo_active);

    if (!redo_active || redo_label == NULL)
    {
        g_free (redo_label);
        redo_label = g_strdup (_("_Redo"));
    }
    g_set_object (&menu_item, g_menu_item_new (redo_label, "win.redo"));
    g_menu_append_item (updated_section, menu_item);
    update_action (self, "redo", redo_active);

    nautilus_gmenu_set_from_model (G_MENU (self->undo_redo_section),
                                   G_MENU_MODEL (updated_section));
}

static void
on_location_entry_close (GtkWidget       *close_button,
                         NautilusToolbar *self)
{
    nautilus_toolbar_set_show_location_entry (self, FALSE);
}

static void
on_location_entry_focus_changed (GObject    *object,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
    NautilusToolbar *toolbar;

    toolbar = NAUTILUS_TOOLBAR (user_data);

    if (gtk_widget_has_focus (GTK_WIDGET (object)))
    {
        toolbar->location_entry_should_auto_hide = TRUE;
    }
    else if (toolbar->location_entry_should_auto_hide)
    {
        nautilus_toolbar_set_show_location_entry (toolbar, FALSE);
    }
}

static void
nautilus_toolbar_constructed (GObject *object)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    self->path_bar = GTK_WIDGET (g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL));
    gtk_box_append (GTK_BOX (self->path_bar_container),
                    self->path_bar);

    self->location_entry = nautilus_location_entry_new ();
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry);
    self->location_entry_close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry_close_button);
    g_signal_connect (self->location_entry_close_button, "clicked",
                      G_CALLBACK (on_location_entry_close), self);
    g_signal_connect (self->location_entry, "notify::has-focus",
                      G_CALLBACK (on_location_entry_focus_changed), self);

    /* Setting a max width on one entry to effectively set a max expansion for
     * the whole title widget. */
    gtk_editable_set_max_width_chars (GTK_EDITABLE (self->location_entry), 88);

    gtk_widget_show (GTK_WIDGET (self));
    toolbar_update_appearance (self);
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
    g_type_ensure (NAUTILUS_TYPE_HISTORY_CONTROLS);
    g_type_ensure (NAUTILUS_TYPE_PROGRESS_INDICATOR);
    g_type_ensure (NAUTILUS_TYPE_VIEW_CONTROLS);

    gtk_widget_init_template (GTK_WIDGET (self));
}

void
nautilus_toolbar_on_window_constructed (NautilusToolbar *self)
{
    /* undo_manager_changed manipulates the window actions, so set it up
     * after the window and it's actions have been constructed
     */
    g_signal_connect_object (nautilus_file_undo_manager_get (),
                             "undo-changed",
                             G_CALLBACK (undo_manager_changed),
                             self,
                             G_CONNECT_SWAPPED);

    undo_manager_changed (self);
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
        case PROP_SHOW_LOCATION_ENTRY:
        {
            g_value_set_boolean (value, self->show_location_entry);
        }
        break;

        case PROP_WINDOW_SLOT:
        {
            g_value_set_object (value, self->window_slot);
        }
        break;

        case PROP_SEARCHING:
        {
            g_value_set_boolean (value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button)));
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

    /* The window slot was finalized, and the binding has already been removed.
     * Null it here, so that dispose() does not trip over itself when removing it.
     */
    self->search_binding = NULL;

    nautilus_toolbar_set_window_slot_real (self, NULL);
}

static void
on_window_focus_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GtkWidget *widget;
    NautilusToolbar *toolbar;

    widget = GTK_WIDGET (object);
    toolbar = NAUTILUS_TOOLBAR (user_data);

    if (g_settings_get_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
    {
        return;
    }

    /* The working assumption being made here is, if the location entry is visible,
     * the user must have switched windows while having keyboard focus on the entry
     * (because otherwise it would be invisible),
     * so we focus the entry explicitly to reset the “should auto-hide” flag.
     */
    if (gtk_widget_has_focus (widget) && toolbar->show_location_entry)
    {
        gtk_widget_grab_focus (toolbar->location_entry);
    }
    /* The location entry in general is hidden when it loses focus,
     * but hiding it when switching windows could be undesirable, as the user
     * might want to copy a path from somewhere. This here prevents that from happening.
     */
    else
    {
        toolbar->location_entry_should_auto_hide = FALSE;
    }
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
        case PROP_WINDOW:
        {
            if (self->window != NULL)
            {
                g_signal_handlers_disconnect_by_func (self->window,
                                                      on_window_focus_changed, self);
            }
            self->window = g_value_get_object (value);
            if (self->window != NULL)
            {
                g_signal_connect (self->window, "notify::has-focus",
                                  G_CALLBACK (on_window_focus_changed), self);
            }
        }
        break;

        case PROP_SHOW_LOCATION_ENTRY:
        {
            nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
        }
        break;

        case PROP_WINDOW_SLOT:
        {
            nautilus_toolbar_set_window_slot (self, g_value_get_object (value));
        }
        break;

        case PROP_SEARCHING:
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button),
                                          g_value_get_boolean (value));
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
    NautilusToolbar *self;

    self = NAUTILUS_TOOLBAR (object);

    g_clear_pointer (&self->search_binding, g_binding_unbind);

    G_OBJECT_CLASS (nautilus_toolbar_parent_class)->dispose (object);
}

static void
nautilus_toolbar_finalize (GObject *obj)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          toolbar_update_appearance, self);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
        self->window_slot = NULL;
    }

    g_signal_handlers_disconnect_by_func (self->window,
                                          on_window_focus_changed, self);

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
    oclass->constructed = nautilus_toolbar_constructed;

    properties[PROP_WINDOW] =
        g_param_spec_object ("window",
                             "The NautilusWindow",
                             "The NautilusWindow this toolbar is part of",
                             NAUTILUS_TYPE_WINDOW,
                             G_PARAM_WRITABLE |
                             G_PARAM_STATIC_STRINGS);
    properties[PROP_SHOW_LOCATION_ENTRY] =
        g_param_spec_boolean ("show-location-entry",
                              "Whether to show the location entry",
                              "Whether to show the location entry instead of the pathbar",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties [PROP_WINDOW_SLOT] =
        g_param_spec_object ("window-slot",
                             "Window slot currently active",
                             "Window slot currently acive",
                             NAUTILUS_TYPE_WINDOW_SLOT,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));

    properties [PROP_SEARCHING] =
        g_param_spec_boolean ("searching",
                              "Current view is searching",
                              "Whether the current view is searching or not",
                              FALSE,
                              G_PARAM_READWRITE);

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

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, app_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, undo_redo_section);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, toolbar_switcher);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, path_bar_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, location_entry_container);

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_button);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOOLBAR);
}

GtkWidget *
nautilus_toolbar_new ()
{
    return g_object_new (NAUTILUS_TYPE_TOOLBAR,
                         NULL);
}

GtkWidget *
nautilus_toolbar_get_path_bar (NautilusToolbar *self)
{
    return self->path_bar;
}

GtkWidget *
nautilus_toolbar_get_location_entry (NautilusToolbar *self)
{
    return self->location_entry;
}

void
nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
                                          gboolean         show_location_entry)
{
    if (show_location_entry != self->show_location_entry)
    {
        self->show_location_entry = show_location_entry;
        toolbar_update_appearance (self);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
    }
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

/* Called from on_window_slot_destroyed(), since bindings and signal handlers
 * are automatically removed once the slot goes away.
 */
static void
nautilus_toolbar_set_window_slot_real (NautilusToolbar    *self,
                                       NautilusWindowSlot *slot)
{
    g_autoptr (GList) children = NULL;

    self->window_slot = slot;

    if (self->window_slot != NULL)
    {
        g_object_weak_ref (G_OBJECT (self->window_slot),
                           on_window_slot_destroyed,
                           self);

        self->search_binding = g_object_bind_property (self->window_slot, "searching",
                                                       self, "searching",
                                                       G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

        g_signal_connect_swapped (self->window_slot, "notify::extensions-background-menu",
                                  G_CALLBACK (slot_on_extensions_background_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::templates-menu",
                                  G_CALLBACK (slot_on_templates_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::searching",
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
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCHING]);
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

    g_clear_pointer (&self->search_binding, g_binding_unbind);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
    }

    nautilus_toolbar_set_window_slot_real (self, window_slot);
}

gboolean
nautilus_toolbar_is_menu_visible (NautilusToolbar *self)
{
    GtkWidget *menu;

    g_return_val_if_fail (NAUTILUS_IS_TOOLBAR (self), FALSE);

    menu = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (self->app_button)));
    g_return_val_if_fail (menu != NULL, FALSE);

    return gtk_widget_is_visible (menu);
}
