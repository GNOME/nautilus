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

    GtkWidget *sidebar_button;
    gboolean show_sidebar_button;
    gboolean sidebar_button_active;

    gboolean show_toolbar_children;

    GtkWidget *location_entry_close_button;

    /* active slot & bindings */
    NautilusWindowSlot *window_slot;
};

enum
{
    PROP_0,
    PROP_SHOW_LOCATION_ENTRY,
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
}

static void
update_action (NautilusToolbar *self,
               const char      *action_name,
               gboolean         enabled)
{
    GtkWidget *window;
    GAction *action;

    window = gtk_widget_get_ancestor (GTK_WIDGET (self), NAUTILUS_TYPE_WINDOW);

    /* Activate/deactivate */
    action = g_action_map_lookup_action (G_ACTION_MAP (window), action_name);
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
    g_autoptr (GMenuItem) undo_menu_item = NULL;
    g_autoptr (GMenuItem) redo_menu_item = NULL;
    NautilusWindow *window;

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
    undo_menu_item = g_menu_item_new (undo_label, "win.undo");
    g_menu_append_item (updated_section, undo_menu_item);
    update_action (self, "undo", undo_active);

    if (!redo_active || redo_label == NULL)
    {
        g_free (redo_label);
        redo_label = g_strdup (_("_Redo"));
    }
    redo_menu_item = g_menu_item_new (redo_label, "win.redo");
    g_menu_append_item (updated_section, redo_menu_item);
    update_action (self, "redo", redo_active);

    if (self->window_slot != NULL)
    {
        window = nautilus_window_slot_get_window (self->window_slot);

        nautilus_gmenu_set_from_model (G_MENU (nautilus_window_get_undo_redo_section (window)),
                                       G_MENU_MODEL (updated_section));
    }
}

static void
on_location_entry_close (GtkWidget       *close_button,
                         NautilusToolbar *self)
{
    nautilus_toolbar_set_show_location_entry (self, FALSE);
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

    nautilus_toolbar_set_show_location_entry (toolbar, FALSE);
}

static void
nautilus_toolbar_constructed (GObject *object)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);
    GtkEventController *controller;

    self->path_bar = GTK_WIDGET (g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL));
    gtk_box_append (GTK_BOX (self->path_bar_container),
                    self->path_bar);

    self->location_entry = nautilus_location_entry_new ();
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry);
    self->location_entry_close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
    gtk_widget_set_tooltip_text (self->location_entry_close_button, _("Cancel"));
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry_close_button);
    g_signal_connect (self->location_entry_close_button, "clicked",
                      G_CALLBACK (on_location_entry_close), self);

    controller = gtk_event_controller_focus_new ();
    gtk_widget_add_controller (self->location_entry, controller);
    g_signal_connect (controller, "leave",
                      G_CALLBACK (on_location_entry_focus_leave), self);

    /* Setting a max width on one entry to effectively set a max expansion for
     * the whole title widget. */
    gtk_editable_set_max_width_chars (GTK_EDITABLE (self->location_entry), 88);

    toolbar_update_appearance (self);
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
    g_type_ensure (NAUTILUS_TYPE_HISTORY_CONTROLS);
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

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, toolbar_switcher);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, path_bar_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, location_entry_container);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TOOLBAR);
}

GtkWidget *
nautilus_toolbar_new (void)
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

        g_signal_connect_swapped (self->window_slot, "notify::extensions-background-menu",
                                  G_CALLBACK (slot_on_extensions_background_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::templates-menu",
                                  G_CALLBACK (slot_on_templates_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::search-visible",
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
