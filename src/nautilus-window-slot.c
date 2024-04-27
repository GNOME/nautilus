/*
 *  nautilus-window-slot.c: Nautilus window slot
 *
 *  Copyright (C) 2008 Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Christian Neumair <cneumair@gnome.org>
 */
#define G_LOG_DOMAIN "nautilus-window"

#include "config.h"

#include "nautilus-window-slot.h"

#include "nautilus-application.h"
#include "nautilus-bookmark.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-fd-holder.h"
#include "nautilus-files-view.h"
#include "nautilus-location-banner.h"
#include "nautilus-mime-actions.h"
#include "nautilus-query-editor.h"
#include "nautilus-scheme.h"
#include "nautilus-toolbar.h"
#include "nautilus-view.h"
#include "nautilus-x-content-bar.h"

#include <glib/gi18n.h>
#include <adwaita.h>

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-module.h"
#include "nautilus-monitor.h"
#include "nautilus-ui-utilities.h"

enum
{
    PROP_ACTIVE = 1,
    PROP_ICON_NAME,
    PROP_TOOLBAR_MENU_SECTIONS,
    PROP_EXTENSIONS_BACKGROUND_MENU,
    PROP_TEMPLATES_MENU,
    PROP_LOADING,
    PROP_SEARCH_VISIBLE,
    PROP_SEARCH_GLOBAL,
    PROP_SELECTION,
    PROP_LOCATION,
    PROP_TOOLTIP,
    PROP_ALLOW_STOP,
    PROP_TITLE,
    NUM_PROPERTIES
};

struct _NautilusWindowSlot
{
    AdwBin parent_instance;

    gboolean active : 1;
    guint loading : 1;

    GtkWidget *stack;
    GtkWidget *vbox;
    AdwStatusPage *global_search_page;

    GtkWidget *extra_location_widgets;

    /* Slot actions */
    GActionGroup *slot_action_group;

    /* Current location. */
    GFile *location;
    gchar *title;
    NautilusFdHolder *fd_holder;

    /* Viewed file */
    NautilusView *content_view;
    NautilusView *new_content_view;
    NautilusFile *viewed_file;
    gboolean viewed_file_seen;
    gboolean viewed_file_in_trash;

    /* Information about bookmarks and history list */
    NautilusBookmark *current_location_bookmark;
    NautilusBookmark *last_location_bookmark;
    GList *back_list;
    GList *forward_list;

    /* Alt+Up reverse stack for Alt+Down. */
    GList /*<owned GFile*>*/ *down_list;

    /* Query editor */
    NautilusQueryEditor *query_editor;
    NautilusQuery *pending_search_query;

    /* Banner */
    AdwBanner *banner;

    GtkLabel *search_info_label;
    GtkRevealer *search_info_label_revealer;

    /* Load state */
    GCancellable *find_mount_cancellable;
    /* It could be either the view is loading the files or the search didn't
     * finish. Used for showing a spinner to provide feedback to the user. */
    gboolean allow_stop;
    gboolean needs_reload;

    /* New location. */
    GFile *pending_location;
    NautilusLocationChangeType location_change_type;
    guint location_change_distance;
    GList *pending_selection;
    NautilusFile *pending_file_to_activate;
    NautilusFile *determine_view_file;
    GCancellable *mount_cancellable;
    GError *mount_error;
    gboolean tried_mount;
    gint view_mode_before_network;

    /* Menus */
    GMenuModel *extensions_background_menu;
    GMenuModel *templates_menu;

    /* View bindings */
    GBinding *searching_binding;
    GBinding *selection_binding;
    GBinding *extensions_background_menu_binding;
    GBinding *templates_menu_binding;
    GList *selection;
};

G_DEFINE_TYPE (NautilusWindowSlot, nautilus_window_slot, ADW_TYPE_BIN);

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void nautilus_window_slot_force_reload (NautilusWindowSlot *self);
static void change_view (NautilusWindowSlot *self);
static void nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *self);
static void nautilus_window_slot_disconnect_content_view (NautilusWindowSlot *self);
static gboolean nautilus_window_slot_content_view_matches (NautilusWindowSlot *self,
                                                           guint               id);
static void nautilus_window_slot_set_loading (NautilusWindowSlot *self,
                                              gboolean            loading);
char *nautilus_window_slot_get_location_uri (NautilusWindowSlot *self);
static void nautilus_window_slot_set_search_visible (NautilusWindowSlot *self,
                                                     gboolean            visible);
static void nautilus_window_slot_set_location (NautilusWindowSlot *self,
                                               GFile              *location);
static void nautilus_window_slot_set_viewed_file (NautilusWindowSlot *self,
                                                  NautilusFile       *file);
static void update_search_information (NautilusWindowSlot *self);

void
free_navigation_state (gpointer data)
{
    NautilusNavigationState *navigation_state = data;

    g_list_free_full (navigation_state->back_list, g_object_unref);
    g_list_free_full (navigation_state->forward_list, g_object_unref);
    g_clear_object (&navigation_state->current_location_bookmark);
    g_clear_object (&navigation_state->current_search_query);

    g_free (navigation_state);
}

void
nautilus_window_slot_restore_navigation_state (NautilusWindowSlot      *self,
                                               NautilusNavigationState *data)
{
    self->back_list = g_steal_pointer (&data->back_list);

    self->forward_list = g_steal_pointer (&data->forward_list);

    g_set_object (&self->current_location_bookmark, data->current_location_bookmark);

    self->location_change_type = NAUTILUS_LOCATION_CHANGE_RELOAD;

    g_set_object (&self->pending_search_query, data->current_search_query);
}

NautilusNavigationState *
nautilus_window_slot_get_navigation_state (NautilusWindowSlot *self)
{
    NautilusNavigationState *data;
    GList *back_list;
    GList *forward_list;
    NautilusView *view = nautilus_window_slot_get_current_view (self);

    if (self->location == NULL)
    {
        return NULL;
    }

    back_list = g_list_copy_deep (self->back_list,
                                  (GCopyFunc) g_object_ref,
                                  NULL);
    forward_list = g_list_copy_deep (self->forward_list,
                                     (GCopyFunc) g_object_ref,
                                     NULL);

    /* This data is used to restore a tab after it was closed.
     * In order to do that we need to keep the history, what was
     * the view mode before search and a reference to the file.
     * A GFile isn't enough, as the NautilusFile also keeps a
     * reference to the search directory */
    data = g_new0 (NautilusNavigationState, 1);
    data->back_list = back_list;
    data->forward_list = forward_list;
    g_set_object (&data->current_location_bookmark, self->current_location_bookmark);
    g_set_object (&data->current_search_query, nautilus_view_get_search_query (view));

    return data;
}

static NautilusView *
nautilus_window_slot_get_view_for_location (NautilusWindowSlot *self,
                                            GFile              *location)
{
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    NautilusView *view = NULL;
    guint view_id = NAUTILUS_VIEW_INVALID_ID;

    if (self->content_view != NULL)
    {
        /* If there is already a view, just use the view mode that it's currently using */
        view_id = nautilus_view_get_view_id (self->content_view);
        if (view_id == NAUTILUS_VIEW_NETWORK_ID)
        {
            view_id = self->view_mode_before_network;
        }
    }

    if (nautilus_file_is_network_view (file))
    {
        self->view_mode_before_network = view_id;
        view_id = NAUTILUS_VIEW_NETWORK_ID;
    }

    /* If there is not previous view in this slot, use the default view mode
     * from preferences */
    if (view_id == NAUTILUS_VIEW_INVALID_ID)
    {
        view_id = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER);
    }
    if (view_id == NAUTILUS_VIEW_INVALID_ID)
    {
        g_warning ("Invalid value stored for 'default-folder-viewer' key for "
                   "the 'org.gnome.nautilus.preferences' schemas. Installed "
                   "schemas may be outdated. Falling back to 'list-view'.");
        view_id = NAUTILUS_VIEW_LIST_ID;
    }

    /* Try to reuse the current view */
    if (nautilus_window_slot_content_view_matches (self, view_id))
    {
        view = self->content_view;
    }
    else
    {
        view = NAUTILUS_VIEW (nautilus_files_view_new (view_id, self));
    }

    return view;
}

static gboolean
nautilus_window_slot_content_view_matches (NautilusWindowSlot *self,
                                           guint               id)
{
    if (self->content_view == NULL)
    {
        return FALSE;
    }

    if (id != NAUTILUS_VIEW_INVALID_ID)
    {
        return nautilus_view_get_view_id (self->content_view) == id;
    }
    else
    {
        return FALSE;
    }
}

static void
update_search_visible (NautilusWindowSlot *self)
{
    NautilusView *view = nautilus_window_slot_get_current_view (self);

    /* If we changed location just to another search location, for example,
     * when changing the query, just keep the search visible.
     * Make sure the search is visible though, since we could be returning
     * from a previous search location when using the history */
    if (nautilus_view_is_searching (view))
    {
        nautilus_window_slot_set_search_visible (self, TRUE);
        return;
    }

    if (self->pending_search_query)
    {
        nautilus_window_slot_search (self, g_object_ref (self->pending_search_query));
    }
}

static void
nautilus_window_slot_sync_actions (NautilusWindowSlot *self)
{
    NautilusView *view;
    gboolean is_network_view;
    GAction *action;
    GVariant *variant;

    if (!nautilus_window_slot_get_active (self))
    {
        return;
    }

    if (self->content_view == NULL || self->new_content_view != NULL)
    {
        return;
    }

    /* Check if we need to close the search or show search after changing the location.
     * Needs to be done after the change has been done, if not, a loop happens,
     * because setting the search enabled or not actually opens a location */
    update_search_visible (self);

    /* Files view mode */
    view = nautilus_window_slot_get_current_view (self);
    is_network_view = nautilus_view_get_view_id (view) == NAUTILUS_VIEW_NETWORK_ID;
    action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group), "files-view-mode");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_network_view);
    if (g_action_get_enabled (action))
    {
        variant = g_variant_new_uint32 (nautilus_view_get_view_id (view));
        g_action_change_state (action, variant);
    }
    action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group), "files-view-mode-toggle");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !is_network_view);
}

static void
query_editor_cancel_callback (NautilusQueryEditor *editor,
                              NautilusWindowSlot  *self)
{
    nautilus_window_slot_set_search_visible (self, FALSE);
}

static void
query_editor_activated_callback (NautilusQueryEditor *editor,
                                 NautilusWindowSlot  *self)
{
    if (self->content_view != NULL)
    {
        if (NAUTILUS_IS_FILES_VIEW (self->content_view))
        {
            nautilus_files_view_activate_selection (NAUTILUS_FILES_VIEW (self->content_view), 0);
        }
    }
}

static void
query_editor_focus_view_callback (NautilusQueryEditor *editor,
                                  NautilusWindowSlot  *self)
{
    if (self->content_view != NULL)
    {
        gtk_widget_grab_focus (GTK_WIDGET (self->content_view));
    }
}

static void
query_editor_changed_callback (NautilusQueryEditor *editor,
                               NautilusQuery       *query,
                               gboolean             reload,
                               NautilusWindowSlot  *self)
{
    NautilusView *view;

    view = nautilus_window_slot_get_current_view (self);

    /* Setting search query may cause the view to load a new location. */
    nautilus_view_set_search_query (view, query);
    nautilus_window_slot_set_location (self, nautilus_view_get_location (view));
}

static void
hide_query_editor (NautilusWindowSlot *self)
{
    NautilusView *view;

    view = nautilus_window_slot_get_current_view (self);

    g_signal_handlers_disconnect_by_data (self->query_editor, self);

    nautilus_query_editor_set_query (self->query_editor, NULL);

    if (nautilus_view_is_searching (view))
    {
        g_autolist (NautilusFile) selection = NULL;

        /* Save current selection to restore it after leaving search results.
         * This allows finding a file from current folder using search, then
         * press [Esc], and have the selected search result still selected and
         * revealed in the unfiltered folder view. */
        selection = nautilus_view_get_selection (view);

        /* Now that we have saved the search, clear the view's query. The view
         * will immediately clear its model model and load the previous location
         */
        nautilus_view_set_search_query (view, NULL);

        /* The view location has changed, update the slot location. */
        nautilus_window_slot_set_location (self, nautilus_view_get_location (view));
        nautilus_view_set_selection (view, selection);
    }

    if (nautilus_window_slot_get_active (self))
    {
        gtk_widget_grab_focus (GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
    }
}

static GFile *
nautilus_window_slot_get_current_location (NautilusWindowSlot *self)
{
    if (self->pending_location != NULL)
    {
        return self->pending_location;
    }

    return self->location;
}

static void
show_query_editor (NautilusWindowSlot *self)
{
    NautilusView *view;

    view = nautilus_window_slot_get_current_view (self);
    if (view == NULL)
    {
        return;
    }

    if (nautilus_view_is_searching (view))
    {
        NautilusQuery *query;

        query = nautilus_view_get_search_query (view);

        if (query != NULL)
        {
            nautilus_query_editor_set_query (self->query_editor, query);
        }
    }

    gtk_widget_grab_focus (GTK_WIDGET (self->query_editor));

    g_signal_connect (self->query_editor, "changed",
                      G_CALLBACK (query_editor_changed_callback), self);
    g_signal_connect (self->query_editor, "cancel",
                      G_CALLBACK (query_editor_cancel_callback), self);
    g_signal_connect (self->query_editor, "activated",
                      G_CALLBACK (query_editor_activated_callback), self);
    g_signal_connect (self->query_editor, "focus-view",
                      G_CALLBACK (query_editor_focus_view_callback), self);
}

static void
nautilus_window_slot_set_search_visible (NautilusWindowSlot *self,
                                         gboolean            visible)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group),
                                         "search-visible");
    g_action_change_state (action, g_variant_new_boolean (visible));
}

gboolean
nautilus_window_slot_get_search_visible (NautilusWindowSlot *self)
{
    GAction *action;
    GVariant *state;
    gboolean search_visible;

    action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group),
                                         "search-visible");
    state = g_action_get_state (action);
    search_visible = g_variant_get_boolean (state);

    g_variant_unref (state);

    return search_visible;
}

static void
nautilus_window_slot_set_search_global (NautilusWindowSlot *self,
                                        gboolean            visible)
{
    GAction *action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group),
                                                  "search-global");
    g_action_change_state (action, g_variant_new_boolean (visible));
}

gboolean
nautilus_window_slot_get_search_global (NautilusWindowSlot *self)
{
    GAction *action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group),
                                                  "search-global");
    g_autoptr (GVariant) state = g_action_get_state (action);

    return g_variant_get_boolean (state);
}

void
nautilus_window_slot_search (NautilusWindowSlot *self,
                             NautilusQuery      *query)
{
    NautilusView *view;

    g_clear_object (&self->pending_search_query);

    view = nautilus_window_slot_get_current_view (self);
    /* We could call this when the location is still being checked in the
     * window slot. For that, save the search we want to do for once we have
     * a view set up */
    if (view)
    {
        nautilus_window_slot_set_search_visible (self, TRUE);
        nautilus_query_editor_set_query (self->query_editor, query);
        nautilus_view_set_search_query (view, query);
    }
    else
    {
        self->pending_search_query = g_object_ref (query);
    }
}

gboolean
nautilus_window_slot_handle_event (NautilusWindowSlot    *self,
                                   GtkEventControllerKey *controller,
                                   guint                  keyval,
                                   GdkModifierType        state)
{
    gboolean retval;
    GAction *action;

    retval = FALSE;
    action = g_action_map_lookup_action (G_ACTION_MAP (self->slot_action_group),
                                         "search-visible");

    if (keyval == GDK_KEY_Escape ||
        keyval == GDK_KEY_BackSpace)
    {
        g_autoptr (GVariant) action_state = NULL;

        action_state = g_action_get_state (action);

        if (!g_variant_get_boolean (action_state))
        {
            return GDK_EVENT_PROPAGATE;
        }
        else if (keyval == GDK_KEY_Escape)
        {
            nautilus_window_slot_set_search_visible (self, FALSE);
            return GDK_EVENT_STOP;
        }
    }

    /* If the action is not enabled, don't try to handle search */
    if (g_action_get_enabled (action))
    {
        retval = nautilus_query_editor_handle_event (self->query_editor,
                                                     controller,
                                                     keyval,
                                                     state);
    }

    if (retval)
    {
        nautilus_window_slot_set_search_visible (self, TRUE);
    }

    return retval;
}

static void
nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *self)
{
    GtkWidget *widget;
    while ((widget = gtk_widget_get_first_child (self->extra_location_widgets)) != NULL)
    {
        gtk_box_remove (GTK_BOX (self->extra_location_widgets), widget);
    }
}

static void
nautilus_window_slot_add_extra_location_widget (NautilusWindowSlot *self,
                                                GtkWidget          *widget)
{
    gtk_box_append (GTK_BOX (self->extra_location_widgets), widget);
}

static void
nautilus_window_slot_set_selection (NautilusWindowSlot *self,
                                    GList              *selection)
{
    nautilus_file_list_free (self->selection);

    self->selection = selection;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTION]);
}

static void
real_set_extensions_background_menu (NautilusWindowSlot *self,
                                     GMenuModel         *menu)
{
    g_set_object (&self->extensions_background_menu, menu);
}

static void
real_set_templates_menu (NautilusWindowSlot *self,
                         GMenuModel         *menu)
{
    g_set_object (&self->templates_menu, menu);
}

static void
nautilus_window_slot_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (object);

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            nautilus_window_slot_set_location (self, g_value_get_object (value));
        }
        break;

        case PROP_SEARCH_VISIBLE:
        {
            nautilus_window_slot_set_search_visible (self, g_value_get_boolean (value));
        }
        break;

        case PROP_EXTENSIONS_BACKGROUND_MENU:
        {
            real_set_extensions_background_menu (self, g_value_get_object (value));
        }
        break;

        case PROP_TEMPLATES_MENU:
        {
            real_set_templates_menu (self, g_value_get_object (value));
        }
        break;

        case PROP_SELECTION:
        {
            nautilus_window_slot_set_selection (self, g_value_get_pointer (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static GMenuModel *
real_get_extensions_background_menu (NautilusWindowSlot *self)
{
    return self->extensions_background_menu;
}

GMenuModel *
nautilus_window_slot_get_extensions_background_menu (NautilusWindowSlot *self)
{
    GMenuModel *menu = NULL;

    g_object_get (self, "extensions-background-menu", &menu, NULL);

    return menu;
}

static GMenuModel *
real_get_templates_menu (NautilusWindowSlot *self)
{
    return self->templates_menu;
}

GMenuModel *
nautilus_window_slot_get_templates_menu (NautilusWindowSlot *self)
{
    GMenuModel *menu = NULL;

    g_object_get (self, "templates-menu", &menu, NULL);

    return menu;
}

static void
nautilus_window_slot_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (object);
    switch (property_id)
    {
        case PROP_ACTIVE:
        {
            g_value_set_boolean (value, nautilus_window_slot_get_active (self));
        }
        break;

        case PROP_ICON_NAME:
        {
            g_value_set_static_string (value, nautilus_window_slot_get_icon_name (self));
        }
        break;

        case PROP_TOOLBAR_MENU_SECTIONS:
        {
            g_value_set_object (value, nautilus_window_slot_get_toolbar_menu_sections (self));
        }
        break;

        case PROP_EXTENSIONS_BACKGROUND_MENU:
        {
            g_value_set_object (value, real_get_extensions_background_menu (self));
        }
        break;

        case PROP_TEMPLATES_MENU:
        {
            g_value_set_object (value, real_get_templates_menu (self));
        }
        break;

        case PROP_LOADING:
        {
            g_value_set_boolean (value, nautilus_window_slot_get_loading (self));
        }
        break;

        case PROP_LOCATION:
        {
            g_value_set_object (value, nautilus_window_slot_get_current_location (self));
        }
        break;

        case PROP_SEARCH_VISIBLE:
        {
            g_value_set_boolean (value, nautilus_window_slot_get_search_visible (self));
        }
        break;

        case PROP_SEARCH_GLOBAL:
        {
            g_value_set_boolean (value, nautilus_window_slot_get_search_global (self));
        }
        break;

        case PROP_TOOLTIP:
        {
            g_value_set_static_string (value, nautilus_window_slot_get_tooltip (self));
        }
        break;

        case PROP_ALLOW_STOP:
        {
            g_value_set_boolean (value, nautilus_window_slot_get_allow_stop (self));
        }
        break;

        case PROP_TITLE:
        {
            g_value_set_string (value, nautilus_window_slot_get_title (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

GList *
nautilus_window_slot_get_selection (NautilusWindowSlot *self)
{
    return self->selection;
}

static void
nautilus_window_slot_constructed (GObject *object)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (object);
    GtkWidget *extras_vbox;

    G_OBJECT_CLASS (nautilus_window_slot_parent_class)->constructed (object);

    self->stack = gtk_stack_new ();
    adw_bin_set_child (ADW_BIN (self), self->stack);

    self->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_stack_add_child (GTK_STACK (self->stack), self->vbox);

    extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    self->extra_location_widgets = extras_vbox;
    gtk_box_append (GTK_BOX (self->vbox), extras_vbox);

    self->query_editor = NAUTILUS_QUERY_EDITOR (nautilus_query_editor_new ());
    /* We want to keep alive the query editor betwen additions and removals on the
     * UI, specifically when the toolbar adds or removes it */
    g_object_ref_sink (self->query_editor);

    self->banner = ADW_BANNER (adw_banner_new (""));
    gtk_box_append (GTK_BOX (self->vbox),
                    GTK_WIDGET (self->banner));

    self->search_info_label = GTK_LABEL (gtk_label_new (NULL));
    self->search_info_label_revealer = GTK_REVEALER (gtk_revealer_new ());

    gtk_revealer_set_child (GTK_REVEALER (self->search_info_label_revealer),
                            GTK_WIDGET (self->search_info_label));
    gtk_box_append (GTK_BOX (self->vbox),
                    GTK_WIDGET (self->search_info_label_revealer));

    gtk_widget_add_css_class (GTK_WIDGET (self->search_info_label), "search-information");

    self->global_search_page = ADW_STATUS_PAGE (adw_status_page_new ());
    adw_status_page_set_icon_name (self->global_search_page, "edit-find-symbolic");
    adw_status_page_set_title (self->global_search_page, _("Search Everywhere"));
    adw_status_page_set_description (self->global_search_page, _("Find files and folders in all search locations"));
    gtk_stack_add_child (GTK_STACK (self->stack), GTK_WIDGET (self->global_search_page));

    GtkWidget *button = gtk_button_new_with_label (_("Search Settings"));
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.search-settings");
    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "pill");
    adw_status_page_set_child (self->global_search_page, button);

    g_object_bind_property (self, "location",
                            self->query_editor, "location",
                            G_BINDING_DEFAULT);

    self->title = g_strdup (_("Loading…"));
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

static void
action_focus_search (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (user_data);
    NautilusQueryEditor *editor = nautilus_window_slot_get_query_editor (self);

    nautilus_window_slot_set_search_visible (self, TRUE);
    nautilus_query_editor_select_all_text (editor);
}

static void
action_search_visible (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (user_data);
    g_autoptr (GVariant) current_state = g_action_get_state (G_ACTION (action));
    gboolean search_visible = g_variant_get_boolean (state);

    if (g_variant_get_boolean (current_state) != search_visible)
    {
        g_simple_action_set_state (action, state);

        if (search_visible)
        {
            show_query_editor (self);
        }
        else
        {
            hide_query_editor (self);
        }

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_VISIBLE]);

        update_search_information (self);
    }

    /* Update global_search_page visibility even if the state of search-visible
     * hasn't changed. This is because we may have just been notified through
     * property binding that the view has started showing search results. */
    if (nautilus_window_slot_get_search_global (self))
    {
        if (search_visible)
        {
            NautilusView *view = nautilus_window_slot_get_current_view (self);
            if (view != NULL && nautilus_view_is_searching (view))
            {
                gtk_stack_set_visible_child (GTK_STACK (self->stack), self->vbox);
            }
        }
        else
        {
            /* The Escape key has been used. */
            nautilus_window_slot_set_search_global (self, FALSE);
        }
    }
}

static void
action_search_global (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (user_data);
    g_autoptr (GVariant) current_state = g_action_get_state (G_ACTION (action));
    gboolean search_global = g_variant_get_boolean (state);

    if (g_variant_get_boolean (current_state) != search_global)
    {
        g_simple_action_set_state (action, state);

        if (search_global)
        {
            NautilusQuery *query = nautilus_view_get_search_query (self->content_view);
            if (query != NULL)
            {
                nautilus_query_set_location (query, NULL);
                nautilus_view_set_search_query (self->content_view, query);
            }

            nautilus_query_editor_set_location (self->query_editor, NULL);
            gtk_stack_set_visible_child (GTK_STACK (self->stack), GTK_WIDGET (self->global_search_page));
        }
        else
        {
            nautilus_query_editor_set_location (self->query_editor, nautilus_window_slot_get_current_location (self));
            gtk_stack_set_visible_child (GTK_STACK (self->stack), self->vbox);
        }

        nautilus_window_slot_set_search_visible (self, search_global);
        gtk_widget_set_visible (GTK_WIDGET (self->banner), !search_global);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_GLOBAL]);
    }
}

static void
change_files_view_mode (NautilusWindowSlot *self,
                        guint               view_id)
{
    if (!nautilus_window_slot_content_view_matches (self, view_id))
    {
        NautilusView *view = nautilus_window_slot_get_current_view (self);

        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));
        nautilus_files_view_change (NAUTILUS_FILES_VIEW (view), view_id);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOOLTIP]);
    }

    g_settings_set_enum (nautilus_preferences, NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER, view_id);
}

static void
action_files_view_mode_toggle (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       user_data)
{
    NautilusWindowSlot *self;
    guint current_view_id;

    self = NAUTILUS_WINDOW_SLOT (user_data);
    if (!NAUTILUS_IS_FILES_VIEW (self->content_view))
    {
        return;
    }

    current_view_id = nautilus_view_get_view_id (self->content_view);
    if (current_view_id == NAUTILUS_VIEW_LIST_ID)
    {
        change_files_view_mode (self, NAUTILUS_VIEW_GRID_ID);
    }
    else
    {
        change_files_view_mode (self, NAUTILUS_VIEW_LIST_ID);
    }
}

static void
action_files_view_mode (GSimpleAction *action,
                        GVariant      *value,
                        gpointer       user_data)
{
    NautilusWindowSlot *self;
    guint view_id;

    view_id = g_variant_get_uint32 (value);
    self = NAUTILUS_WINDOW_SLOT (user_data);

    if (!NAUTILUS_IS_FILES_VIEW (nautilus_window_slot_get_current_view (self)))
    {
        return;
    }

    change_files_view_mode (self, view_id);

    g_simple_action_set_state (action, value);
}

static void
action_reload (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (user_data);

    nautilus_window_slot_queue_reload (self);
}

static void
action_stop (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusWindowSlot *self = NAUTILUS_WINDOW_SLOT (user_data);

    nautilus_window_slot_stop_loading (self);
}

const GActionEntry slot_entries[] =
{
    {
        .name = "files-view-mode", .parameter_type = "u",
        .state = "uint32 " G_STRINGIFY (NAUTILUS_VIEW_INVALID_ID),
        .change_state = action_files_view_mode
    },
    { .name = "files-view-mode-toggle", .activate = action_files_view_mode_toggle },
    { .name = "search-visible", .state = "false", .change_state = action_search_visible },
    { .name = "search-global", .state = "false", .change_state = action_search_global },
    { .name = "focus-search", .activate = action_focus_search },
    { .name = "reload", .activate = action_reload },
    { .name = "stop", .activate = action_stop },
};

static void
update_search_information (NautilusWindowSlot *self)
{
    GFile *location;

    if (!nautilus_window_slot_get_search_visible (self))
    {
        gtk_revealer_set_reveal_child (self->search_info_label_revealer, FALSE);

        return;
    }

    location = nautilus_window_slot_get_current_location (self);

    if (location)
    {
        g_autoptr (NautilusFile) file = NULL;
        gchar *label;

        file = nautilus_file_get (location);
        label = NULL;

        if (nautilus_is_root_for_scheme (location, SCHEME_NETWORK_VIEW) ||
            nautilus_is_root_for_scheme (location, SCHEME_NETWORK))
        {
            label = _("Searching network locations only");
        }
        else if (nautilus_file_is_remote (file) &&
                 location_settings_search_get_recursive_for_location (location) == NAUTILUS_QUERY_RECURSIVE_NEVER)
        {
            label = _("Remote location — only searching the current folder");
        }
        else if (location_settings_search_get_recursive_for_location (location) == NAUTILUS_QUERY_RECURSIVE_NEVER)
        {
            label = _("Only searching the current folder");
        }

        gtk_label_set_label (self->search_info_label, label);
        gtk_revealer_set_reveal_child (self->search_info_label_revealer,
                                       label != NULL);
    }
}

static void
recursive_search_preferences_changed (GSettings *settings,
                                      gchar     *key,
                                      gpointer   callback_data)
{
    NautilusWindowSlot *self;

    self = callback_data;

    update_search_information (self);
}

static void
nautilus_window_slot_init (NautilusWindowSlot *self)
{
    GApplication *app;
    const gchar *search_visible_accels[] =
    {
        "<control>f",
        "Search",
        NULL
    };

    app = g_application_get_default ();

    g_signal_connect_object (nautilus_preferences,
                             "changed::recursive-search",
                             G_CALLBACK (recursive_search_preferences_changed),
                             self, 0);

    self->slot_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (G_ACTION_MAP (self->slot_action_group),
                                     slot_entries,
                                     G_N_ELEMENTS (slot_entries),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "slot",
                                    G_ACTION_GROUP (self->slot_action_group));

#define ACCELS(...) ((const char *[]) { __VA_ARGS__, NULL })

    nautilus_application_set_accelerator (app,
                                          "slot.files-view-mode(uint32 " G_STRINGIFY (NAUTILUS_VIEW_LIST_ID) ")",
                                          "<control>1");
    nautilus_application_set_accelerator (app,
                                          "slot.files-view-mode(uint32 " G_STRINGIFY (NAUTILUS_VIEW_GRID_ID) ")",
                                          "<control>2");
    nautilus_application_set_accelerators (app, "slot.focus-search", search_visible_accels);
    nautilus_application_set_accelerator (app, "slot.search-global", "<control><shift>f");
    nautilus_application_set_accelerators (app, "slot.reload", ACCELS ("F5", "<ctrl>r", "Refresh", "Reload"));
    nautilus_application_set_accelerator (app, "slot.stop", "Stop");

#undef ACCELS

    self->fd_holder = nautilus_fd_holder_new ();
    self->view_mode_before_network = NAUTILUS_VIEW_INVALID_ID;
}

static void begin_location_change (NautilusWindowSlot        *slot,
                                   GFile                     *location,
                                   GFile                     *previous_location,
                                   GList                     *new_selection,
                                   NautilusLocationChangeType type,
                                   guint                      distance);
static void free_location_change (NautilusWindowSlot *self);
static void end_location_change (NautilusWindowSlot *self);
static void got_file_info_for_view_selection_callback (NautilusFile *file,
                                                       gpointer      callback_data);
static gboolean setup_view (NautilusWindowSlot *self,
                            NautilusView       *view);
static void load_new_location (NautilusWindowSlot *slot,
                               GFile              *location,
                               GList              *selection,
                               NautilusFile       *file_to_activate,
                               gboolean            tell_current_content_view,
                               gboolean            tell_new_content_view);

void
nautilus_window_slot_open_location_full (NautilusWindowSlot *self,
                                         GFile              *location,
                                         NautilusOpenFlags   flags,
                                         GList              *new_selection)
{
    GFile *old_location = nautilus_window_slot_get_location (self);

    nautilus_window_slot_set_search_global (self, FALSE);

    if (old_location != NULL && g_file_equal (old_location, location))
    {
        if (self->content_view != NULL && new_selection != NULL)
        {
            nautilus_view_set_selection (self->content_view, new_selection);
        }

        return;
    }

    begin_location_change (self, location, old_location, new_selection,
                           NAUTILUS_LOCATION_CHANGE_STANDARD, 0);
}

static GList *
check_select_old_location_containing_folder (GList                      *new_selection,
                                             NautilusLocationChangeType  type,
                                             GFile                      *location,
                                             GFile                      *previous_location)
{
    GFile *from_folder, *parent;

    /* If there is no new selection or if we are going back and the new location
     * is a (grand)parent of the old location then we automatically
     * select the folder the previous location was in */
    if ((new_selection == NULL || type == NAUTILUS_LOCATION_CHANGE_BACK) &&
        previous_location != NULL &&
        g_file_has_prefix (previous_location, location))
    {
        from_folder = g_object_ref (previous_location);
        parent = g_file_get_parent (from_folder);
        while (parent != NULL && !g_file_equal (parent, location))
        {
            g_object_unref (from_folder);
            from_folder = parent;
            parent = g_file_get_parent (from_folder);
        }

        if (parent != NULL)
        {
            g_clear_list (&new_selection, g_object_unref);
            new_selection = g_list_prepend (NULL, nautilus_file_get (from_folder));
            g_object_unref (parent);
        }

        g_object_unref (from_folder);
    }

    return new_selection;
}

static void
check_force_reload (GFile                      *location,
                    NautilusLocationChangeType  type)
{
    NautilusDirectory *directory;
    NautilusFile *file;
    gboolean force_reload;

    /* The code to force a reload is here because if we do it
     * after determining an initial view (in the components), then
     * we end up fetching things twice.
     */
    directory = nautilus_directory_get (location);
    file = nautilus_file_get (location);

    if (type == NAUTILUS_LOCATION_CHANGE_RELOAD)
    {
        force_reload = TRUE;
    }
    else
    {
        force_reload = !g_file_is_native (location) && !nautilus_file_is_starred_location (file);
    }

    /* We need to invalidate file attributes as well due to how mounting works
     * in the window slot and to avoid other caching issues.
     * Read handle_mount_if_needed for one example */
    if (force_reload)
    {
        nautilus_file_invalidate_all_attributes (file);
        nautilus_directory_force_reload (directory);
    }

    nautilus_directory_unref (directory);
    nautilus_file_unref (file);
}

static void
save_selection_for_history (NautilusWindowSlot *self)
{
    /* Set current_bookmark scroll pos */
    if (self->current_location_bookmark != NULL &&
        self->content_view != NULL &&
        NAUTILUS_IS_FILES_VIEW (self->content_view))
    {
        g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (self->content_view));
        g_autoptr (GStrvBuilder) selected_uris = g_strv_builder_new ();

        for (GList *l = selection; l != NULL; l = l->next)
        {
            NautilusFile *file = l->data;
            g_strv_builder_take (selected_uris, nautilus_file_get_uri (file));
        }

        nautilus_bookmark_take_selected_uris (self->current_location_bookmark,
                                              g_strv_builder_end (selected_uris));
    }
}

/*
 * begin_location_change
 *
 * Change a window slot's location.
 * @window: The NautilusWindow whose location should be changed.
 * @location: A url specifying the location to load
 * @previous_location: The url that was previously shown in the window that initialized the change, if any
 * @new_selection: The initial selection to present after loading the location
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 * @scroll_pos: The file to scroll to when the location is loaded.
 *
 * This is the core function for changing the location of a window. Every change to the
 * location begins here.
 */
static void
begin_location_change (NautilusWindowSlot         *self,
                       GFile                      *location,
                       GFile                      *previous_location,
                       GList                      *new_selection,
                       NautilusLocationChangeType  type,
                       guint                       distance)
{
    g_assert (self != NULL);
    g_assert (location != NULL);
    g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK
              || type == NAUTILUS_LOCATION_CHANGE_FORWARD
              || distance == 0);

    /* Avoid to update status from the current view in our async calls */
    nautilus_window_slot_disconnect_content_view (self);
    /* We are going to change the location, so make sure we stop any loading
     * or searching of the previous view, so we avoid to be slow */
    nautilus_window_slot_stop_loading (self);

    nautilus_window_slot_set_allow_stop (self, TRUE);

    g_assert (self->pending_location == NULL);

    self->pending_location = g_object_ref (location);
    self->location_change_type = type;
    self->location_change_distance = distance;
    self->tried_mount = FALSE;
    self->pending_selection =
        check_select_old_location_containing_folder (nautilus_file_list_copy (new_selection),
                                                     type,
                                                     location,
                                                     previous_location);
    /* Flush down list. Up/down actions should fetch the list beforehand. */
    g_clear_list (&self->down_list, g_object_unref);

    check_force_reload (location, type);

    save_selection_for_history (self);

    /* Get the info needed to make decisions about how to open the new location */
    self->determine_view_file = nautilus_file_get (location);
    g_assert (self->determine_view_file != NULL);

    nautilus_file_call_when_ready (self->determine_view_file,
                                   NAUTILUS_FILE_ATTRIBUTE_INFO |
                                   NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                   got_file_info_for_view_selection_callback,
                                   self);
}

static void
nautilus_window_slot_set_location (NautilusWindowSlot *self,
                                   GFile              *location)
{
    GFile *old_location;

    if (self->location &&
        g_file_equal (location, self->location))
    {
        /* The location name could be updated even if the location
         * wasn't changed. This is the case for a search.
         */
        nautilus_window_slot_update_title (self);
        return;
    }

    old_location = self->location;
    self->location = g_object_ref (location);

    nautilus_window_slot_update_title (self);

    if (old_location)
    {
        g_object_unref (old_location);
    }

    nautilus_fd_holder_set_location (self->fd_holder, self->location);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOCATION]);
}

static void
viewed_file_changed_callback (NautilusFile       *file,
                              NautilusWindowSlot *self)
{
    gboolean is_in_trash, was_in_trash;

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    g_assert (file == self->viewed_file);

    if (!nautilus_file_is_not_yet_confirmed (file))
    {
        self->viewed_file_seen = TRUE;
    }

    was_in_trash = self->viewed_file_in_trash;

    self->viewed_file_in_trash = is_in_trash = nautilus_file_is_in_trash (file);

    if (nautilus_file_is_gone (file) || (is_in_trash && !was_in_trash))
    {
        if (nautilus_file_is_gone (file))
        {
            nautilus_window_slot_set_viewed_file (self, NULL);
        }

        if (self->viewed_file_seen)
        {
            g_autoptr (GFile) location = nautilus_file_get_location (file);
            g_autoptr (GFile) go_to_file = NULL;

            /* If the current location disappears, the following code jumps to
             * the first existing parent except in the case when the file
             * resides on some of the user interesting mounts and the mount was
             * unmounted. In that case, it jumps to the home directory to avoid
             * jumping into the runtime dir.
             */

            if (!nautilus_file_has_been_unmounted (file))
            {
                /* Verify also the current location to prevent jumps to parent
                 * in case of autofs.
                 */
                go_to_file = nautilus_find_existing_uri_in_hierarchy (location);
            }

            if (go_to_file == NULL)
            {
                go_to_file = g_file_new_for_path (g_get_home_dir ());
            }

            if (g_file_equal (location, go_to_file))
            {
                /* Path gone by time out may have been remounted by
                 * `nautilus_find_existing_uri_in_hierarchy()`.
                 */
                nautilus_window_slot_force_reload (self);
            }
            else
            {
                nautilus_window_slot_open_location_full (self, go_to_file, 0, NULL);
            }
        }
    }
    else
    {
        g_autoptr (GFile) new_location = nautilus_file_get_location (file);
        nautilus_window_slot_set_location (self, new_location);
    }
}

static void
nautilus_window_slot_go_home (NautilusWindowSlot *self,
                              NautilusOpenFlags   flags)
{
    GFile *home;

    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (self));

    home = g_file_new_for_path (g_get_home_dir ());
    nautilus_window_slot_open_location_full (self, home, flags, NULL);
    g_object_unref (home);
}

static void
nautilus_window_slot_set_viewed_file (NautilusWindowSlot *self,
                                      NautilusFile       *file)
{
    NautilusFileAttributes attributes;

    if (self->viewed_file == file)
    {
        return;
    }

    nautilus_file_ref (file);

    if (self->viewed_file != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->viewed_file,
                                              G_CALLBACK (viewed_file_changed_callback),
                                              self);
        nautilus_file_monitor_remove (self->viewed_file,
                                      self);
    }

    if (file != NULL)
    {
        attributes = NAUTILUS_FILE_ATTRIBUTE_INFO;
        nautilus_file_monitor_add (file, self, attributes);

        g_signal_connect_object (file, "changed",
                                 G_CALLBACK (viewed_file_changed_callback), self, 0);
    }

    nautilus_file_unref (self->viewed_file);
    self->viewed_file = file;
}

typedef struct
{
    GCancellable *cancellable;
    NautilusWindowSlot *slot;
} MountNotMountedData;

static void
mount_not_mounted_callback (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    MountNotMountedData *data;
    NautilusWindowSlot *self;
    GError *error;
    GCancellable *cancellable;

    data = user_data;
    self = data->slot;
    cancellable = data->cancellable;
    g_free (data);

    if (g_cancellable_is_cancelled (cancellable))
    {
        /* Cancelled, don't call back */
        g_object_unref (cancellable);
        return;
    }

    self->mount_cancellable = NULL;

    self->determine_view_file = nautilus_file_get (self->pending_location);

    error = NULL;
    if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error))
    {
        self->mount_error = error;
        got_file_info_for_view_selection_callback (self->determine_view_file, self);
        self->mount_error = NULL;
        g_error_free (error);
    }
    else
    {
        nautilus_file_invalidate_all_attributes (self->determine_view_file);
        nautilus_file_call_when_ready (self->determine_view_file,
                                       NAUTILUS_FILE_ATTRIBUTE_INFO |
                                       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
                                       self);
    }

    g_object_unref (cancellable);
}

static void
nautilus_window_slot_display_view_selection_failure (GtkWindow    *window,
                                                     NautilusFile *file,
                                                     GFile        *location,
                                                     GError       *error)
{
    char *error_message;
    char *detail_message;
    char *scheme_string;
    char *file_path;

    /* Some sort of failure occurred. How 'bout we tell the user? */

    error_message = g_strdup (_("Oops! Something went wrong."));
    detail_message = NULL;
    if (error == NULL)
    {
        if (nautilus_file_is_directory (file))
        {
            detail_message = g_strdup (_("Unable to display the contents of this folder."));
        }
        else
        {
            detail_message = g_strdup (_("This location doesn’t appear to be a folder."));
        }
    }
    else if (error->domain == G_IO_ERROR)
    {
        switch (error->code)
        {
            case G_IO_ERROR_NOT_FOUND:
            {
                file_path = g_file_get_path (location);
                if (file_path != NULL)
                {
                    detail_message = g_strdup_printf (_("Unable to find “%s”. Please check the spelling and try again."),
                                                      file_path);
                }
                else
                {
                    detail_message = g_strdup (_("Unable to find the requested file. Please check the spelling and try again."));
                }
                g_free (file_path);
            }
            break;

            case G_IO_ERROR_NOT_SUPPORTED:
            {
                scheme_string = g_file_get_uri_scheme (location);
                if (scheme_string != NULL)
                {
                    detail_message = g_strdup_printf (_("“%s” locations are not supported."),
                                                      scheme_string);
                }
                else
                {
                    detail_message = g_strdup (_("Unable to handle this kind of location."));
                }
                g_free (scheme_string);
            }
            break;

            case G_IO_ERROR_NOT_MOUNTED:
            {
                detail_message = g_strdup (_("Unable to access the requested location."));
            }
            break;

            case G_IO_ERROR_PERMISSION_DENIED:
            {
                detail_message = g_strdup (_("Don’t have permission to access the requested location."));
            }
            break;

            case G_IO_ERROR_HOST_NOT_FOUND:
            {
                /* This case can be hit for user-typed strings like "foo" due to
                 * the code that guesses web addresses when there's no initial "/".
                 * But this case is also hit for legitimate web addresses when
                 * the proxy is set up wrong.
                 */
                detail_message = g_strdup (_("Unable to find the requested location. Please check the spelling or the network settings."));
            }
            break;

            case G_IO_ERROR_CONNECTION_REFUSED:
            {
                /* This case can be hit when server application is not installed
                 * or is inactive in the system user is trying to connect to.
                 */
                detail_message = g_strdup (_("The server has refused the connection. Typically this means that the firewall is blocking access or that the remote service is not running."));
            }
            break;

            case G_IO_ERROR_CANCELLED:
            case G_IO_ERROR_FAILED_HANDLED:
            {
                goto done;
            }

            default:
            {
            }
            break;
        }
    }

    if (detail_message == NULL)
    {
        detail_message = g_strdup_printf (_("Unhandled error message: %s"), error->message);
    }

    show_dialog (error_message, detail_message, window, GTK_MESSAGE_ERROR);

done:
    g_free (error_message);
    g_free (detail_message);
}

/* FIXME: This works in the folowwing way. begin_location_change tries to get the
 * information of the file directly.
 * If the nautilus file finds that there is an error trying to get its
 * information and the error match that the file is not mounted, it sets an
 * internal attribute with the error then we try to mount it here.
 *
 * However, files are cached, and if the file doesn't get finalized in a location
 * change, because needs to be in the navigation history or is a bookmark, and the
 * file is not the root of the mount point, which is tracked by a volume monitor,
 * and it gets unmounted aftwerwards, the file doesn't realize it's unmounted, and
 * therefore this trick to open an unmounted file will fail the next time the user
 * tries to open.
 * For that, we need to always invalidate the file attributes when a location is
 * changed, which is done in check_force_reload.
 * A better way would be to make sure any children of the mounted root gets
 * akwnoledge by it either by adding a reference to its parent volume monitor
 * or with another solution. */
static gboolean
handle_mount_if_needed (NautilusWindowSlot *self,
                        NautilusFile       *file)
{
    GMountOperation *mount_op;
    MountNotMountedData *data;
    GFile *location;
    GError *error = NULL;
    gboolean needs_mount_handling = FALSE;

    if (self->mount_error)
    {
        error = g_error_copy (self->mount_error);
    }
    else if (nautilus_file_get_file_info_error (file) != NULL)
    {
        error = g_error_copy (nautilus_file_get_file_info_error (file));
    }

    if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
        !self->tried_mount)
    {
        self->tried_mount = TRUE;

        mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
        g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
        location = nautilus_file_get_location (file);
        data = g_new0 (MountNotMountedData, 1);
        data->cancellable = g_cancellable_new ();
        data->slot = self;
        self->mount_cancellable = data->cancellable;
        g_file_mount_enclosing_volume (location, 0, mount_op, self->mount_cancellable,
                                       mount_not_mounted_callback, data);
        g_object_unref (location);
        g_object_unref (mount_op);

        needs_mount_handling = TRUE;
    }

    g_clear_error (&error);

    return needs_mount_handling;
}

static gboolean
handle_regular_file_if_needed (NautilusWindowSlot *self,
                               NautilusFile       *file)
{
    NautilusFile *parent_file;
    gboolean needs_regular_file_handling = FALSE;
    parent_file = nautilus_file_get_parent (file);
    if ((parent_file != NULL) &&
        nautilus_file_get_file_type (file) == G_FILE_TYPE_REGULAR)
    {
        g_clear_pointer (&self->pending_selection, nautilus_file_list_free);
        g_clear_object (&self->pending_location);
        g_clear_object (&self->pending_file_to_activate);

        self->pending_location = nautilus_file_get_parent_location (file);
        if (nautilus_mime_file_extracts (file))
        {
            self->pending_file_to_activate = nautilus_file_ref (file);
        }

        self->pending_selection = g_list_prepend (NULL, nautilus_file_ref (file));
        self->determine_view_file = nautilus_file_ref (parent_file);

        nautilus_file_invalidate_all_attributes (self->determine_view_file);
        nautilus_file_call_when_ready (self->determine_view_file,
                                       NAUTILUS_FILE_ATTRIBUTE_INFO |
                                       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
                                       self);

        needs_regular_file_handling = TRUE;
    }

    nautilus_file_unref (parent_file);

    return needs_regular_file_handling;
}

static void
got_file_info_for_view_selection_callback (NautilusFile *file,
                                           gpointer      callback_data)
{
    GError *error = NULL;
    NautilusWindowSlot *self;
    NautilusView *view;
    GFile *location;

    self = callback_data;

    g_assert (self->determine_view_file == file);
    self->determine_view_file = NULL;

    if (handle_mount_if_needed (self, file))
    {
        goto done;
    }

    if (handle_regular_file_if_needed (self, file))
    {
        goto done;
    }

    if (self->mount_error)
    {
        error = g_error_copy (self->mount_error);
    }
    else if (nautilus_file_get_file_info_error (file) != NULL)
    {
        error = g_error_copy (nautilus_file_get_file_info_error (file));
    }

    location = self->pending_location;

    /* desktop and other-locations GFile operations report G_IO_ERROR_NOT_SUPPORTED,
     * but it's not an actual error for Nautilus */
    if (!error || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
        view = nautilus_window_slot_get_view_for_location (self, location);
        setup_view (self, view);
    }
    else
    {
        GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

        nautilus_window_slot_display_view_selection_failure (window,
                                                             file,
                                                             location,
                                                             error);

        GFile *slot_location = nautilus_window_slot_get_location (self);

        if (slot_location == NULL)
        {
            /* This happens when a new slot cannot display its initial URI.
             * As usual, fallback to $HOME.
             *
             * But guard against the case that $HOME cannot be displayed either,
             * otherwise we would enter an infinite loop. */

            if (!nautilus_is_home_directory (location))
            {
                nautilus_window_slot_go_home (self, FALSE);
            }
            else
            {
                /* the last fallback is to go to a known place that can't be deleted! */
                g_autoptr (GFile) root = g_file_new_for_path ("/");
                nautilus_window_slot_open_location_full (self, root, 0, NULL);
            }
        }
        else
        {
            /* Clean up state of slot already showing a previous location */
            end_location_change (self);

            /* We disconnected this, so we need to re-connect it.
             * Although not conceptually correct, a force reload is a simple
             * way to do that. In the future we may want to have an error
             * status page instead and only reload on explicit request. */
            nautilus_window_slot_force_reload (self);

            /* Leave the location bar showing the bad location that the user
             * typed (or maybe achieved by dragging or something). Many times
             * the mistake will just be an easily-correctable typo.
             */
        }
    }

done:
    g_clear_error (&error);

    nautilus_file_unref (file);
}

/* Load a view into the window, either reusing the old one or creating
 * a new one. This happens when you want to load a new location, or just
 * switch to a different view.
 * If pending_location is set we're loading a new location and
 * pending_location/selection will be used. If not, we're just switching
 * view, and the current location will be used.
 */
static gboolean
setup_view (NautilusWindowSlot *self,
            NautilusView       *view)
{
    gboolean ret = TRUE;
    GFile *old_location;

    nautilus_window_slot_disconnect_content_view (self);

    self->new_content_view = view;

    nautilus_window_slot_connect_new_content_view (self);

    /* Forward search selection and state before loading the new model */
    old_location = self->content_view ? nautilus_view_get_location (self->content_view) : NULL;

    /* Actually load the pending location and selection: */
    if (self->pending_location != NULL)
    {
        load_new_location (self,
                           self->pending_location,
                           self->pending_selection,
                           self->pending_file_to_activate,
                           FALSE,
                           TRUE);

        nautilus_file_list_free (self->pending_selection);
        self->pending_selection = NULL;
    }
    else if (old_location != NULL)
    {
        g_autolist (NautilusFile) selection = NULL;

        selection = nautilus_view_get_selection (self->content_view);

        load_new_location (self,
                           old_location,
                           selection,
                           NULL,
                           FALSE,
                           TRUE);
    }
    else
    {
        ret = FALSE;
        goto out;
    }

    change_view (self);

out:
    return ret;
}

static void
load_new_location (NautilusWindowSlot *self,
                   GFile              *location,
                   GList              *selection,
                   NautilusFile       *file_to_activate,
                   gboolean            tell_current_content_view,
                   gboolean            tell_new_content_view)
{
    NautilusView *view = NULL;
    g_assert (self != NULL);
    g_assert (location != NULL);

    /* Note, these may recurse into report_load_underway */
    if (self->content_view != NULL && tell_current_content_view)
    {
        view = self->content_view;
        nautilus_view_set_location (self->content_view, location);
    }

    if (self->new_content_view != NULL && tell_new_content_view &&
        (!tell_current_content_view ||
         self->new_content_view != self->content_view))
    {
        view = self->new_content_view;
        nautilus_view_set_location (self->new_content_view, location);
    }
    if (view)
    {
        nautilus_view_set_selection (view, selection);
        if (file_to_activate != NULL)
        {
            g_autoptr (GAppInfo) app_info = NULL;
            const gchar *app_id;

            g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));
            app_info = nautilus_mime_get_default_application_for_file (file_to_activate);
            app_id = g_app_info_get_id (app_info);
            if (g_strcmp0 (app_id, NAUTILUS_DESKTOP_ID) == 0)
            {
                nautilus_files_view_activate_file (NAUTILUS_FILES_VIEW (view),
                                                   file_to_activate, 0);
            }
        }
    }
}

static void
end_location_change (NautilusWindowSlot *self)
{
    char *uri;
    uri = nautilus_window_slot_get_location_uri (self);
    if (uri)
    {
        g_debug ("Finished loading window for uri %s", uri);
        g_free (uri);
    }

    nautilus_window_slot_set_allow_stop (self, FALSE);

    free_location_change (self);
}

static void
free_location_change (NautilusWindowSlot *self)
{
    g_clear_object (&self->pending_location);
    g_clear_object (&self->pending_file_to_activate);
    nautilus_file_list_free (self->pending_selection);
    self->pending_selection = NULL;

    if (self->mount_cancellable != NULL)
    {
        g_cancellable_cancel (self->mount_cancellable);
        self->mount_cancellable = NULL;
    }

    if (self->determine_view_file != NULL)
    {
        nautilus_file_cancel_call_when_ready
            (self->determine_view_file,
            got_file_info_for_view_selection_callback, self);
        self->determine_view_file = NULL;
    }
}

void
nautilus_window_slot_back_or_forward (NautilusWindowSlot *self,
                                      gboolean            back,
                                      guint               distance)
{
    GList *list;
    guint len;
    NautilusBookmark *bookmark;
    g_autoptr (GFile) location = NULL;
    GFile *old_location;
    g_autolist (NautilusFile) selection = NULL;

    if (back)
    {
        /* While searching, maybe the user means to go "back" to no search. */
        NautilusView *view = nautilus_window_slot_get_current_view (self);
        if (nautilus_view_is_searching (view))
        {
            nautilus_window_slot_set_search_visible (self, FALSE);
            return;
        }
    }

    list = back ? self->back_list : self->forward_list;
    len = g_list_length (list);

    /* If we can't move in the direction at all, just return. */
    if (list == NULL)
    {
        return;
    }

    /* If the distance to move is off the end of the list, go to the end
     *  of the list. */
    if (distance >= len)
    {
        distance = len - 1;
    }

    bookmark = g_list_nth_data (list, distance);
    location = nautilus_bookmark_get_location (bookmark);
    old_location = nautilus_window_slot_get_location (self);

    GStrv selected_uris = nautilus_bookmark_get_selected_uris (bookmark);
    if (selected_uris != NULL)
    {
        for (int i = 0; selected_uris[i] != NULL; i++)
        {
            selection = g_list_prepend (selection, nautilus_file_get_by_uri (selected_uris[i]));
        }
        selection = g_list_reverse (selection);
    }

    begin_location_change (self,
                           location, old_location,
                           selection,
                           back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD,
                           distance);
}

/* reload the contents of the window */
static void
nautilus_window_slot_force_reload (NautilusWindowSlot *self)
{
    GFile *location;
    g_autolist (NautilusFile) selection = NULL;

    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    location = nautilus_window_slot_get_location (self);
    if (location == NULL)
    {
        return;
    }

    /* peek_slot_field (window, location) can be free'd during the processing
     * of begin_location_change, so make a copy
     */
    g_object_ref (location);

    if (self->content_view)
    {
        selection = nautilus_view_get_selection (self->content_view);
    }
    begin_location_change (self, location, location, selection, NAUTILUS_LOCATION_CHANGE_RELOAD, 0);
    g_object_unref (location);
}

void
nautilus_window_slot_queue_reload (NautilusWindowSlot *self)
{
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    if (nautilus_window_slot_get_location (self) == NULL)
    {
        return;
    }

    if (self->pending_location != NULL
        || self->content_view == NULL
        || nautilus_view_is_loading (self->content_view))
    {
        /* there is a reload in flight */
        self->needs_reload = TRUE;
        return;
    }

    nautilus_window_slot_force_reload (self);
}

static void
nautilus_window_slot_clear_forward_list (NautilusWindowSlot *self)
{
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    g_list_free_full (self->forward_list, g_object_unref);
    self->forward_list = NULL;
}

static void
nautilus_window_slot_clear_back_list (NautilusWindowSlot *self)
{
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    g_list_free_full (self->back_list, g_object_unref);
    self->back_list = NULL;
}

static void
nautilus_window_slot_update_bookmark (NautilusWindowSlot *self,
                                      NautilusFile       *file)
{
    gboolean recreate;
    GFile *new_location;
    new_location = nautilus_file_get_location (file);

    if (self->current_location_bookmark == NULL)
    {
        recreate = TRUE;
    }
    else
    {
        GFile *bookmark_location;
        bookmark_location = nautilus_bookmark_get_location (self->current_location_bookmark);
        recreate = !g_file_equal (bookmark_location, new_location);
        g_object_unref (bookmark_location);
    }

    if (recreate)
    {
        const char *display_name = nautilus_file_get_display_name (file);

        /* We've changed locations, must recreate bookmark for current location. */
        g_clear_object (&self->last_location_bookmark);
        self->last_location_bookmark = self->current_location_bookmark;

        self->current_location_bookmark = nautilus_bookmark_new (new_location, display_name);
    }

    g_object_unref (new_location);
}

static void
check_bookmark_location_matches (NautilusBookmark *bookmark,
                                 GFile            *location)
{
    GFile *bookmark_location;
    char *bookmark_uri, *uri;

    bookmark_location = nautilus_bookmark_get_location (bookmark);
    if (!g_file_equal (location, bookmark_location))
    {
        bookmark_uri = g_file_get_uri (bookmark_location);
        uri = g_file_get_uri (location);
        g_warning ("bookmark uri is %s, but expected %s", bookmark_uri, uri);
        g_free (uri);
        g_free (bookmark_uri);
    }
    g_object_unref (bookmark_location);
}

/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_slot (NautilusWindowSlot *self)
{
    GFile *slot_location = nautilus_window_slot_get_location (self);

    if (g_file_has_uri_scheme (slot_location, SCHEME_SEARCH))
    {
        /* In this case, it's expected not to match, because search is not
         * saved into the history stack. */
        return;
    }

    check_bookmark_location_matches (self->last_location_bookmark,
                                     slot_location);
}

static void
handle_go_direction (NautilusWindowSlot *self,
                     GFile              *location,
                     gboolean            forward)
{
    GList **list_ptr, **other_list_ptr;
    GList *list, *other_list, *link;
    NautilusBookmark *bookmark;
    list_ptr = (forward) ? (&self->forward_list) : (&self->back_list);
    other_list_ptr = (forward) ? (&self->back_list) : (&self->forward_list);
    list = *list_ptr;
    other_list = *other_list_ptr;

    /* Move items from the list to the other list. */
    g_assert (g_list_length (list) > self->location_change_distance);
    check_bookmark_location_matches (g_list_nth_data (list, self->location_change_distance),
                                     location);
    g_assert (nautilus_window_slot_get_location (self) != NULL);

    /* Move current location to list */
    check_last_bookmark_location_matches_slot (self);

    /* Use the first bookmark in the history list rather than creating a new one. */
    other_list = g_list_prepend (other_list, self->last_location_bookmark);
    g_object_ref (other_list->data);

    /* Move extra links from the list to the other list */
    for (guint i = 0; i < self->location_change_distance; ++i)
    {
        bookmark = NAUTILUS_BOOKMARK (list->data);
        list = g_list_remove (list, bookmark);
        other_list = g_list_prepend (other_list, bookmark);
    }

    /* One bookmark falls out of back/forward lists and becomes viewed location */
    link = list;
    list = g_list_remove_link (list, link);
    g_object_unref (link->data);
    g_list_free_1 (link);

    *list_ptr = list;
    *other_list_ptr = other_list;
}

static void
handle_go_elsewhere (NautilusWindowSlot *self,
                     GFile              *location)
{
    GFile *slot_location;
    /* Clobber the entire forward list, and move displayed location to back list */
    nautilus_window_slot_clear_forward_list (self);

    /* If we haven't updated the current bookmark, don't update history*/
    if (self->back_list != NULL &&
        self->back_list->data == self->last_location_bookmark)
    {
        return;
    }

    slot_location = nautilus_window_slot_get_location (self);

    if (slot_location != NULL)
    {
        /* If we're returning to the same uri somehow, don't put this uri on back list.
         * This also avoids a problem where set_displayed_location
         * didn't update last_location_bookmark since the uri didn't change.
         */
        if (!g_file_equal (slot_location, location) && self->last_location_bookmark != NULL)
        {
            /* Store bookmark for current location in back list, unless there is no current location */
            check_last_bookmark_location_matches_slot (self);
            /* Use the first bookmark in the history list rather than creating a new one. */
            self->back_list = g_list_prepend (self->back_list,
                                              self->last_location_bookmark);
            g_object_ref (self->back_list->data);
        }
    }
}

static void
update_history (NautilusWindowSlot         *self,
                NautilusLocationChangeType  type,
                GFile                      *new_location)
{
    switch (type)
    {
        case NAUTILUS_LOCATION_CHANGE_STANDARD:
        {
            handle_go_elsewhere (self, new_location);
            return;
        }

        case NAUTILUS_LOCATION_CHANGE_RELOAD:
        {
            /* for reload there is no work to do */
            return;
        }

        case NAUTILUS_LOCATION_CHANGE_BACK:
        {
            handle_go_direction (self, new_location, FALSE);
            return;
        }

        case NAUTILUS_LOCATION_CHANGE_FORWARD:
        {
            handle_go_direction (self, new_location, TRUE);
            return;
        }
    }
    g_return_if_fail (FALSE);
}

typedef struct
{
    NautilusWindowSlot *slot;
    GCancellable *cancellable;
    GMount *mount;
} FindMountData;

static void
nautilus_window_slot_show_x_content_bar (NautilusWindowSlot *self,
                                         GMount             *mount,
                                         const char * const *x_content_types)
{
    GtkWidget *bar;

    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    if (!should_handle_content_types (x_content_types))
    {
        return;
    }

    bar = nautilus_x_content_bar_new (mount, x_content_types);
    nautilus_window_slot_add_extra_location_widget (self, bar);
}

static void
found_content_type_cb (const char **x_content_types,
                       gpointer     user_data)
{
    NautilusWindowSlot *self;
    FindMountData *data = user_data;
    self = data->slot;
    if (g_cancellable_is_cancelled (data->cancellable))
    {
        goto out;
    }


    if (x_content_types != NULL && x_content_types[0] != NULL)
    {
        nautilus_window_slot_show_x_content_bar (self, data->mount, (const char * const *) x_content_types);
    }

    self->find_mount_cancellable = NULL;

out:
    g_object_unref (data->mount);
    g_object_unref (data->cancellable);
    g_free (data);
}

static void
found_mount_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    FindMountData *data = user_data;
    NautilusWindowSlot *self;
    GMount *mount;
    self = NAUTILUS_WINDOW_SLOT (data->slot);
    if (g_cancellable_is_cancelled (data->cancellable))
    {
        goto out;
    }

    mount = g_file_find_enclosing_mount_finish (G_FILE (source_object),
                                                res,
                                                NULL);
    if (mount != NULL)
    {
        data->mount = mount;
        nautilus_get_x_content_types_for_mount_async (mount,
                                                      found_content_type_cb,
                                                      data->cancellable,
                                                      data);
        return;
    }

    self->find_mount_cancellable = NULL;

out:
    g_object_unref (data->cancellable);
    g_free (data);
}

static void
nautilus_window_slot_update_for_new_location (NautilusWindowSlot *self)
{
    g_autoptr (GFile) new_location = g_steal_pointer (&self->pending_location);
    NautilusFile *file;

    file = nautilus_file_get (new_location);
    nautilus_window_slot_update_bookmark (self, file);

    update_history (self, self->location_change_type, new_location);

    /* Create a NautilusFile for this location, so we can catch it
     * if it goes away.
     */
    nautilus_window_slot_set_viewed_file (self, file);
    self->viewed_file_seen = !nautilus_file_is_not_yet_confirmed (file);
    self->viewed_file_in_trash = nautilus_file_is_in_trash (file);
    nautilus_file_unref (file);

    nautilus_window_slot_set_location (self, new_location);

    /* Sync the actions for this new location. */
    nautilus_window_slot_sync_actions (self);

    nautilus_location_banner_load (self->banner, new_location);
}

static gboolean
focus_is_on_popup (GtkRoot *window)
{
    GtkWidget *focus = gtk_root_get_focus (window);

    if (focus != NULL)
    {
        GtkNative *native = gtk_widget_get_native (focus);

        if (native != NULL)
        {
            return GDK_IS_POPUP (gtk_native_get_surface (native));
        }
    }

    return FALSE;
}

static void
view_started_loading (NautilusWindowSlot *self,
                      NautilusView       *view)
{
    if (view == self->content_view)
    {
        nautilus_window_slot_set_allow_stop (self, TRUE);
    }

    GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (self));

    if (!focus_is_on_popup (window))
    {
        /* This mysterious gtk_widget_grab_focus() call has been carried over
         * many refactorings, along the way having been wrapped in multiple ways
         * (nautilus_view_grab_focus(), nautilus_window_pane_grab_focus(),
         * nautilus_view_grab_focus()). It's been added in a series of bugfixes
         * related to extra pane.[0] If I had to guess, I'd say it was to deal
         * with side effects of the grandparent commit[1], which removed a much
         * older bugfix[2].
         *
         * Regardless of its original purpose, we still rely on this call to
         * prevent a "lost focus" situation. This is easily reproduced, in a
         * build where this call is removed, by refreshing a view where nothing
         * is selected. This "lost focus" state means, e.g., some keybindings,
         * such as Ctrl+A, won't work.
         *
         * [0] Commit c69f3a2ba2d0bd23de5a218b8ce13d256481213a
         * [1] Commit 4efd42312584b46f248e2839582a87776a7baebe
         * [2] Commit 4f7ae827f7e13cf06965cc72ca60cf7801906a33
         */
        gtk_widget_grab_focus (GTK_WIDGET (window));
    }

    nautilus_window_slot_set_loading (self, TRUE);
}

static void
view_ended_loading (NautilusWindowSlot *self,
                    NautilusView       *view)
{
    if (view == self->content_view)
    {
        end_location_change (self);
    }

    if (self->needs_reload)
    {
        nautilus_window_slot_queue_reload (self);
        self->needs_reload = FALSE;
    }

    nautilus_window_slot_set_allow_stop (self, FALSE);

    nautilus_window_slot_set_loading (self, FALSE);
}

static void
view_is_loading_changed_cb (GObject            *object,
                            GParamSpec         *pspec,
                            NautilusWindowSlot *self)
{
    NautilusView *view;

    view = NAUTILUS_VIEW (object);

    if (nautilus_view_is_loading (view))
    {
        view_started_loading (self, view);
    }
    else
    {
        view_ended_loading (self, view);
    }
}

static void
nautilus_window_slot_setup_extra_location_widgets (NautilusWindowSlot *self)
{
    GFile *location = nautilus_window_slot_get_current_location (self);
    FindMountData *data;

    if (location == NULL)
    {
        return;
    }

    /* need the mount to determine if we should put up the x-content cluebar */
    if (self->find_mount_cancellable != NULL)
    {
        g_cancellable_cancel (self->find_mount_cancellable);
        self->find_mount_cancellable = NULL;
    }

    data = g_new (FindMountData, 1);
    data->slot = self;
    data->cancellable = g_cancellable_new ();
    data->mount = NULL;

    self->find_mount_cancellable = data->cancellable;
    g_file_find_enclosing_mount_async (location,
                                       G_PRIORITY_DEFAULT,
                                       data->cancellable,
                                       found_mount_cb,
                                       data);
}

static void
nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *self)
{
    if (self->new_content_view)
    {
        g_signal_connect (self->new_content_view,
                          "notify::loading",
                          G_CALLBACK (view_is_loading_changed_cb),
                          self);
    }
}

static void
nautilus_window_slot_disconnect_content_view (NautilusWindowSlot *self)
{
    if (self->content_view)
    {
        /* disconnect old view */
        g_signal_handlers_disconnect_by_func (self->content_view,
                                              G_CALLBACK (view_is_loading_changed_cb),
                                              self);
    }
}

static void
nautilus_window_slot_switch_new_content_view (NautilusWindowSlot *self)
{
    GtkWidget *widget;
    gboolean reusing_view;
    reusing_view = self->new_content_view &&
                   gtk_widget_get_parent (GTK_WIDGET (self->new_content_view)) != NULL;
    /* We are either reusing the view, so new_content_view and content_view
     * are the same, or the new_content_view is invalid */
    if (self->new_content_view == NULL || reusing_view)
    {
        goto done;
    }

    if (self->content_view != NULL)
    {
        g_binding_unbind (self->searching_binding);
        g_binding_unbind (self->selection_binding);
        g_binding_unbind (self->extensions_background_menu_binding);
        g_binding_unbind (self->templates_menu_binding);
        widget = GTK_WIDGET (self->content_view);
        gtk_box_remove (GTK_BOX (self->vbox), widget);
        g_clear_object (&self->content_view);
    }

    if (self->new_content_view != NULL)
    {
        self->content_view = self->new_content_view;
        self->new_content_view = NULL;

        widget = GTK_WIDGET (self->content_view);
        gtk_box_append (GTK_BOX (self->vbox), widget);
        gtk_widget_set_vexpand (widget, TRUE);

        /* Note that this is not bidirectional and that we may also change
         * :search-visible alone, e.g. when clicking the search button. */
        self->searching_binding = g_object_bind_property (self->content_view, "searching",
                                                          self, "search-visible",
                                                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        self->selection_binding = g_object_bind_property (self->content_view, "selection",
                                                          self, "selection",
                                                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        self->extensions_background_menu_binding = g_object_bind_property (self->content_view, "extensions-background-menu",
                                                                           self, "extensions-background-menu",
                                                                           G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        self->templates_menu_binding = g_object_bind_property (self->content_view, "templates-menu",
                                                               self, "templates-menu",
                                                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOOLBAR_MENU_SECTIONS]);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXTENSIONS_BACKGROUND_MENU]);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEMPLATES_MENU]);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TOOLTIP]);
    }

done:
    /* Clean up, so we don't confuse having a new_content_view available or
     * just that we didn't care about it here */
    self->new_content_view = NULL;
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
change_view (NautilusWindowSlot *self)
{
    /* Switch to the new content view.
     * Destroy the extra location widgets first, since they might hold
     * a pointer to the old view, which will possibly be destroyed inside
     * nautilus_window_slot_switch_new_content_view().
     */
    nautilus_window_slot_remove_extra_location_widgets (self);
    nautilus_window_slot_switch_new_content_view (self);

    if (self->pending_location != NULL)
    {
        /* Tell the window we are finished. */
        nautilus_window_slot_update_for_new_location (self);
    }

    /* Now that we finished switching to the new location,
     * add back the extra location widgets.
     */
    nautilus_window_slot_setup_extra_location_widgets (self);
}

static void
nautilus_window_slot_dispose (GObject *object)
{
    NautilusWindowSlot *self;
    self = NAUTILUS_WINDOW_SLOT (object);

    g_signal_handlers_disconnect_by_data (nautilus_preferences, self);

    nautilus_window_slot_clear_forward_list (self);
    nautilus_window_slot_clear_back_list (self);
    g_clear_list (&self->down_list, g_object_unref);

    nautilus_window_slot_remove_extra_location_widgets (self);

    g_clear_pointer (&self->searching_binding, g_binding_unbind);
    g_clear_pointer (&self->selection_binding, g_binding_unbind);
    g_clear_pointer (&self->extensions_background_menu_binding, g_binding_unbind);
    g_clear_pointer (&self->templates_menu_binding, g_binding_unbind);

    g_clear_object (&self->templates_menu);
    g_clear_object (&self->extensions_background_menu);

    if (self->content_view)
    {
        gtk_box_remove (GTK_BOX (self->vbox), GTK_WIDGET (self->content_view));
        g_clear_object (&self->content_view);
    }

    if (self->new_content_view)
    {
        gtk_box_remove (GTK_BOX (self->vbox), GTK_WIDGET (self->new_content_view));
        g_clear_object (&self->new_content_view);
    }

    nautilus_window_slot_set_viewed_file (self, NULL);

    g_clear_object (&self->fd_holder);

    g_clear_object (&self->location);
    g_clear_object (&self->pending_file_to_activate);
    g_clear_pointer (&self->pending_selection, nautilus_file_list_free);
    g_clear_pointer (&self->selection, nautilus_file_list_free);

    g_clear_object (&self->current_location_bookmark);
    g_clear_object (&self->last_location_bookmark);
    g_clear_object (&self->slot_action_group);
    g_clear_object (&self->pending_search_query);

    g_clear_pointer (&self->find_mount_cancellable, g_cancellable_cancel);

    if (self->query_editor)
    {
        g_clear_object (&self->query_editor);
    }

    free_location_change (self);

    G_OBJECT_CLASS (nautilus_window_slot_parent_class)->dispose (object);
}

static void
nautilus_window_slot_finalize (GObject *object)
{
    NautilusWindowSlot *self;
    self = NAUTILUS_WINDOW_SLOT (object);
    g_clear_pointer (&self->title, g_free);

    G_OBJECT_CLASS (nautilus_window_slot_parent_class)->finalize (object);
}

static gboolean
nautilus_window_slot_grab_focus (GtkWidget *widget)
{
    NautilusWindowSlot *self;
    self = NAUTILUS_WINDOW_SLOT (widget);

    if (nautilus_window_slot_get_search_visible (self))
    {
        return gtk_widget_grab_focus (GTK_WIDGET (self->query_editor));
    }
    else if (self->content_view != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (self->content_view));
    }
    else if (self->new_content_view != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (self->new_content_view));
    }

    return GTK_WIDGET_CLASS (nautilus_window_slot_parent_class)->grab_focus (widget);
}

static void
nautilus_window_slot_class_init (NautilusWindowSlotClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    oclass->dispose = nautilus_window_slot_dispose;
    oclass->finalize = nautilus_window_slot_finalize;
    oclass->constructed = nautilus_window_slot_constructed;
    oclass->set_property = nautilus_window_slot_set_property;
    oclass->get_property = nautilus_window_slot_get_property;

    widget_class->grab_focus = nautilus_window_slot_grab_focus;

    properties[PROP_ACTIVE] =
        g_param_spec_boolean ("active",
                              "Whether the slot is active",
                              "Whether the slot is the active slot of the window",
                              FALSE,
                              G_PARAM_READABLE);

    properties[PROP_LOADING] =
        g_param_spec_boolean ("loading",
                              "Whether the slot loading",
                              "Whether the slot is loading a new location",
                              FALSE,
                              G_PARAM_READABLE);

    /**
     * NautilusWindowSlot:search-visible:
     *
     * Whether the search query editor is to be shown for this slot.
     * This doesn't necessarily mean the view is showing search results.
     */
    properties[PROP_SEARCH_VISIBLE] =
        g_param_spec_boolean ("search-visible", "", "",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    properties[PROP_SEARCH_GLOBAL] =
        g_param_spec_boolean ("search-global", NULL, NULL,
                              FALSE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SELECTION] =
        g_param_spec_pointer ("selection",
                              "Selection of the current view of the slot",
                              "The selection of the current view of the slot. Proxy property from the view",
                              G_PARAM_READWRITE);

    properties[PROP_ICON_NAME] =
        g_param_spec_string ("icon-name",
                             "Icon that represents the slot",
                             "The icon that represents the slot",
                             NULL,
                             G_PARAM_READABLE);

    properties[PROP_TOOLBAR_MENU_SECTIONS] =
        g_param_spec_pointer ("toolbar-menu-sections",
                              "Menu sections for the toolbar menu",
                              "The menu sections to add to the toolbar menu for this slot",
                              G_PARAM_READABLE);

    properties[PROP_EXTENSIONS_BACKGROUND_MENU] =
        g_param_spec_object ("extensions-background-menu",
                             "Background menu of extensions",
                             "Proxy property from the view for the background menu for extensions",
                             G_TYPE_MENU_MODEL,
                             G_PARAM_READWRITE);

    properties[PROP_TEMPLATES_MENU] =
        g_param_spec_object ("templates-menu",
                             "Templates menu",
                             "Proxy property from the view for the templates menu",
                             G_TYPE_MENU_MODEL,
                             G_PARAM_READWRITE);

    properties[PROP_LOCATION] =
        g_param_spec_object ("location",
                             "Current location visible on the slot",
                             "Either the location that is used currently, or the pending location. Clients will see the same value they set, and therefore it will be cosistent from clients point of view.",
                             G_TYPE_FILE,
                             G_PARAM_READWRITE);

    properties[PROP_TOOLTIP] =
        g_param_spec_string ("tooltip",
                             "Tooltip that represents the slot",
                             "The tooltip that represents the slot",
                             NULL,
                             G_PARAM_READWRITE);

    properties[PROP_ALLOW_STOP] =
        g_param_spec_boolean ("allow-stop", "", "",
                              FALSE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    properties[PROP_TITLE] =
        g_param_spec_string ("title", "", "",
                             NULL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GFile *
nautilus_window_slot_get_location (NautilusWindowSlot *self)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    return self->location;
}

GFile *
nautilus_window_slot_get_pending_location (NautilusWindowSlot *self)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    return self->pending_location;
}

const gchar *
nautilus_window_slot_get_title (NautilusWindowSlot *self)
{
    return self->title;
}

char *
nautilus_window_slot_get_location_uri (NautilusWindowSlot *self)
{
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    if (self->location)
    {
        return g_file_get_uri (self->location);
    }
    return NULL;
}

/* nautilus_window_slot_update_title:
 *
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NautilusWindowSlot in question.
 *
 */
void
nautilus_window_slot_update_title (NautilusWindowSlot *self)
{
    g_autofree char *title = nautilus_compute_title_for_location (self->location);

    if (g_strcmp0 (title, self->title) != 0)
    {
        g_free (self->title);
        self->title = g_steal_pointer (&title);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
    }
}

gboolean
nautilus_window_slot_get_allow_stop (NautilusWindowSlot *self)
{
    return self->allow_stop;
}

void
nautilus_window_slot_set_allow_stop (NautilusWindowSlot *self,
                                     gboolean            allow)
{
    g_assert (NAUTILUS_IS_WINDOW_SLOT (self));

    self->allow_stop = allow;

    GActionMap *action_map = G_ACTION_MAP (self->slot_action_group);
    GAction *stop_action = g_action_map_lookup_action (action_map, "stop");
    GAction *reload_action = g_action_map_lookup_action (action_map, "reload");

    g_simple_action_set_enabled (G_SIMPLE_ACTION (stop_action), self->allow_stop);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (reload_action), !self->allow_stop);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALLOW_STOP]);
}

void
nautilus_window_slot_stop_loading (NautilusWindowSlot *self)
{
    GFile *location;
    NautilusDirectory *directory;
    location = nautilus_window_slot_get_location (self);
    directory = nautilus_directory_get (self->location);

    if (NAUTILUS_IS_FILES_VIEW (self->content_view))
    {
        nautilus_files_view_stop_loading (NAUTILUS_FILES_VIEW (self->content_view));
    }

    nautilus_directory_unref (directory);

    if (self->pending_location != NULL &&
        location != NULL &&
        self->content_view != NULL &&
        NAUTILUS_IS_FILES_VIEW (self->content_view))
    {
        /* No need to tell the new view - either it is the
         * same as the old view, in which case it will already
         * be told, or it is the very pending change we wish
         * to cancel.
         */
        g_autolist (NautilusFile) selection = NULL;

        selection = nautilus_view_get_selection (self->content_view);
        load_new_location (self,
                           location,
                           selection,
                           NULL,
                           TRUE,
                           FALSE);
    }

    end_location_change (self);

    if (self->new_content_view)
    {
        g_object_unref (self->new_content_view);
        self->new_content_view = NULL;
    }
}

NautilusView *
nautilus_window_slot_get_current_view (NautilusWindowSlot *self)
{
    if (self->content_view != NULL)
    {
        return self->content_view;
    }
    else if (self->new_content_view)
    {
        return self->new_content_view;
    }

    return NULL;
}

NautilusBookmark *
nautilus_window_slot_get_bookmark (NautilusWindowSlot *self)
{
    return self->current_location_bookmark;
}

GList *
nautilus_window_slot_get_back_history (NautilusWindowSlot *self)
{
    return self->back_list;
}

GList *
nautilus_window_slot_get_forward_history (NautilusWindowSlot *self)
{
    return self->forward_list;
}

NautilusWindowSlot *
nautilus_window_slot_new (void)
{
    return g_object_new (NAUTILUS_TYPE_WINDOW_SLOT,
                         NULL);
}

const gchar *
nautilus_window_slot_get_icon_name (NautilusWindowSlot *self)
{
    guint current_view_id;
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    if (self->content_view == NULL)
    {
        return "";
    }

    current_view_id = nautilus_view_get_view_id (NAUTILUS_VIEW (self->content_view));
    switch (current_view_id)
    {
        case NAUTILUS_VIEW_LIST_ID:
        {
            return nautilus_view_get_icon_name (NAUTILUS_VIEW_GRID_ID);
        }
        break;

        case NAUTILUS_VIEW_GRID_ID:
        {
            return nautilus_view_get_icon_name (NAUTILUS_VIEW_LIST_ID);
        }
        break;

        case NAUTILUS_VIEW_NETWORK_ID:
        {
            return nautilus_view_get_icon_name (NAUTILUS_VIEW_NETWORK_ID);
        }
        break;

        default:
        {
            return NULL;
        }
    }
}

const gchar *
nautilus_window_slot_get_tooltip (NautilusWindowSlot *self)
{
    guint current_view_id;
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    if (self->content_view == NULL)
    {
        return NULL;
    }

    current_view_id = nautilus_view_get_view_id (NAUTILUS_VIEW (self->content_view));
    switch (current_view_id)
    {
        case NAUTILUS_VIEW_LIST_ID:
        {
            return nautilus_view_get_tooltip (NAUTILUS_VIEW_GRID_ID);
        }
        break;

        case NAUTILUS_VIEW_GRID_ID:
        {
            return nautilus_view_get_tooltip (NAUTILUS_VIEW_LIST_ID);
        }
        break;

        case NAUTILUS_VIEW_NETWORK_ID:
        {
            return nautilus_view_get_tooltip (NAUTILUS_VIEW_NETWORK_ID);
        }
        break;

        default:
        {
            return NULL;
        }
    }
}

NautilusToolbarMenuSections *
nautilus_window_slot_get_toolbar_menu_sections (NautilusWindowSlot *self)
{
    NautilusView *view;

    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    view = nautilus_window_slot_get_current_view (self);

    return view ? nautilus_view_get_toolbar_menu_sections (view) : NULL;
}

gboolean
nautilus_window_slot_get_active (NautilusWindowSlot *self)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), FALSE);

    return self->active;
}

void
nautilus_window_slot_set_active (NautilusWindowSlot *self,
                                 gboolean            active)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (self));

    if (self->active != active)
    {
        GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (self));

        self->active = active;

        if (active)
        {
            /* sync window to new slot */
            nautilus_window_slot_sync_actions (self);

            gtk_widget_insert_action_group (GTK_WIDGET (window), "slot", self->slot_action_group);
        }
        else
        {
            gtk_widget_insert_action_group (GTK_WIDGET (window), "slot", NULL);
        }

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
    }
}

static void
nautilus_window_slot_set_loading (NautilusWindowSlot *self,
                                  gboolean            loading)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (self));

    self->loading = loading;

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

gboolean
nautilus_window_slot_get_loading (NautilusWindowSlot *self)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), FALSE);

    return self->loading;
}

NautilusQueryEditor *
nautilus_window_slot_get_query_editor (NautilusWindowSlot *self)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (self), NULL);

    return self->query_editor;
}

void
nautilus_window_slot_go_up (NautilusWindowSlot *self)
{
    GFile *location = nautilus_window_slot_get_location (self);
    if (location == NULL)
    {
        return;
    }

    g_autoptr (GFile) parent = g_file_get_parent (location);
    if (parent != NULL)
    {
        /* Save the down list from getting flushed by begin_location_change ()*/
        g_autolist (GFile) down_list = g_steal_pointer (&self->down_list);

        nautilus_window_slot_open_location_full (self, parent, 0, NULL);

        down_list = g_list_prepend (down_list, g_object_ref (location));
        self->down_list = g_steal_pointer (&down_list);
    }
}

void
nautilus_window_slot_go_down (NautilusWindowSlot *self)
{
    g_autolist (GFile) down_list = g_steal_pointer (&self->down_list);
    if (down_list == NULL)
    {
        return;
    }

    GFile *child = G_FILE (down_list->data);

    nautilus_window_slot_open_location_full (self, child, 0, NULL);

    /* Undo ref'ing done in nautilus_window_slot_go_up() */
    g_object_unref (child);
    down_list = g_list_delete_link (down_list, down_list);
    self->down_list = g_steal_pointer (&down_list);
}
