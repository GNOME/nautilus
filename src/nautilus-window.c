/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           John Sullivan <sullivan@eazel.com>
 *           Alexander Larsson <alexl@redhat.com>
 */

/* nautilus-window.c: Implementation of the main window object */
#define G_LOG_DOMAIN "nautilus-window"

#include "nautilus-window.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <sys/time.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#include "gtk/nautilusgtkplacessidebarprivate.h"

#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-clipboard.h"
#include "nautilus-dnd.h"
#include "nautilus-enums.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-network-address-bar.h"
#include "nautilus-mime-actions.h"
#include "nautilus-module.h"
#include "nautilus-pathbar.h"
#include "nautilus-progress-indicator.h"
#include "nautilus-scheme.h"
#include "nautilus-shortcut-manager.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window-slot.h"

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static guint mouse_forward_button = 9;
static guint mouse_back_button = 8;

static void mouse_back_button_changed (gpointer callback_data);
static void mouse_forward_button_changed (gpointer callback_data);
static void use_extra_mouse_buttons_changed (gpointer callback_data);
static void nautilus_window_initialize_actions (NautilusWindow *window);
static void nautilus_window_back_or_forward (NautilusWindow *window,
                                             gboolean        back,
                                             guint           distance);
static void nautilus_window_sync_location_widgets (NautilusWindow *window);
static void update_cursor (NautilusWindow *window);

/* Sanity check: highest mouse button value I could find was 14. 5 is our
 * lower threshold (well-documented to be the one of the button events for the
 * scrollwheel), so it's hardcoded in the functions below. However, if you have
 * a button that registers higher and want to map it, file a bug and
 * we'll move the bar. Makes you wonder why the X guys don't have
 * defined values for these like the XKB stuff, huh?
 */
#define UPPER_MOUSE_LIMIT 14

struct _NautilusWindow
{
    AdwApplicationWindow parent_instance;

    GMenuModel *undo_redo_section;

    AdwTabView *tab_view;
    AdwTabBar *tab_bar;
    AdwTabPage *menu_page;

    GList *slots;
    NautilusWindowSlot *active_slot; /* weak reference */

    GtkWidget *split_view;

    /* Side Pane */
    GtkWidget *places_sidebar;     /* the actual GtkPlacesSidebar */
    GVolume *selected_volume;     /* the selected volume in the sidebar popup callback */
    GFile *selected_file;     /* the selected file in the sidebar popup callback */

    /* Notifications */
    AdwToastOverlay *toast_overlay;
    AdwToast *last_undo_toast;

    /* Toolbar */
    GtkWidget *toolbar;
    gboolean temporary_navigation_bar;

    GtkWidget *network_address_bar;

    guint sidebar_width_handler_id;
    gulong bookmarks_id;
    gulong starred_id;

    GQueue *tab_data_queue;

    /* Pad controller which holds a reference to the window. Kept around to
     * break reference-counting cycles during finalization. */
    GtkPadController *pad_controller;
};

enum
{
    SLOT_ADDED,
    SLOT_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NautilusWindow, nautilus_window, ADW_TYPE_APPLICATION_WINDOW);

enum
{
    PROP_0,
    PROP_ACTIVE_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static const GtkPadActionEntry pad_actions[] =
{
    { GTK_PAD_ACTION_BUTTON, 0, -1, N_("Home"), "go-home" },
    { GTK_PAD_ACTION_BUTTON, 1, -1, N_("New tab"), "new-tab" },
    { GTK_PAD_ACTION_BUTTON, 2, -1, N_("Close current view"), "close-current-view" },
    /* Button number sequence continues in window-slot.c */
};

static AdwTabPage *
get_current_page (NautilusWindow *window)
{
    if (window->menu_page != NULL)
    {
        return window->menu_page;
    }

    return adw_tab_view_get_selected_page (window->tab_view);
}

static void
action_close_current_view (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusWindow *window = user_data;
    AdwTabPage *page = get_current_page (window);

    if (adw_tab_view_get_n_pages (window->tab_view) <= 1)
    {
        nautilus_window_close (window);
        return;
    }

    adw_tab_view_close_page (window->tab_view, page);
}

static void
action_close_other_tabs (GSimpleAction *action,
                         GVariant      *parameters,
                         gpointer       user_data)
{
    NautilusWindow *window = user_data;
    AdwTabPage *page = get_current_page (window);

    adw_tab_view_close_other_pages (window->tab_view, page);
}

static void
action_go_home (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    NautilusWindow *window;
    GFile *home;

    window = NAUTILUS_WINDOW (user_data);
    home = g_file_new_for_path (g_get_home_dir ());

    nautilus_window_open_location_full (window, home, 0, NULL, NULL);

    g_object_unref (home);
}

static void
action_go_starred (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusWindow *window;
    g_autoptr (GFile) starred = NULL;

    window = NAUTILUS_WINDOW (user_data);
    starred = g_file_new_for_uri (SCHEME_STARRED ":///");

    nautilus_window_open_location_full (window, starred, 0, NULL, NULL);
}

static void
action_bookmark_current_location (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data)
{
    NautilusWindow *window = user_data;
    NautilusApplication *app = NAUTILUS_APPLICATION (g_application_get_default ());
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (window);
    nautilus_bookmark_list_append (nautilus_application_get_bookmarks (app),
                                   nautilus_window_slot_get_bookmark (slot));
}

static void
star_or_unstar_current_location (NautilusWindow *window)
{
    NautilusWindowSlot *slot = nautilus_window_get_active_slot (window);
    GFile *location = nautilus_window_slot_get_location (slot);
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    g_autofree gchar *uri = nautilus_file_get_uri (file);

    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    if (nautilus_tag_manager_file_is_starred (tag_manager, uri))
    {
        nautilus_tag_manager_unstar_files (tag_manager, G_OBJECT (window),
                                           &(GList){ .data = file }, NULL, NULL);
    }
    else
    {
        nautilus_tag_manager_star_files (tag_manager, G_OBJECT (window),
                                         &(GList){ .data = file }, NULL, NULL);
    }
}

static void
action_star_current_location (GSimpleAction *action,
                              GVariant      *state,
                              gpointer       user_data)
{
    NautilusWindow *window = user_data;
    star_or_unstar_current_location (window);
}

static void
action_unstar_current_location (GSimpleAction *action,
                                GVariant      *state,
                                gpointer       user_data)
{
    NautilusWindow *window = user_data;
    star_or_unstar_current_location (window);
}

static void
action_new_tab (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    nautilus_window_new_tab (NAUTILUS_WINDOW (user_data));
}

static void
action_enter_location (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindow *self = user_data;

    nautilus_toolbar_open_location_entry (NAUTILUS_TOOLBAR (self->toolbar), NULL);
}

static void
action_tab_move_left (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusWindow *window = user_data;
    AdwTabPage *page = get_current_page (window);

    adw_tab_view_reorder_backward (window->tab_view, page);
}

static void
action_tab_move_right (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindow *window = user_data;
    AdwTabPage *page = get_current_page (window);

    adw_tab_view_reorder_forward (window->tab_view, page);
}

static void
action_go_to_tab (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    gint16 num;

    num = g_variant_get_int32 (value);
    if (num < adw_tab_view_get_n_pages (window->tab_view))
    {
        AdwTabPage *page = adw_tab_view_get_nth_page (window->tab_view, num);

        adw_tab_view_set_selected_page (window->tab_view, page);
    }
}

static void
action_prompt_for_location_root (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    NautilusWindow *self = NAUTILUS_WINDOW (user_data);

    nautilus_toolbar_open_location_entry (NAUTILUS_TOOLBAR (self->toolbar), "/");
}

static void
action_prompt_for_location_home (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    NautilusWindow *self = NAUTILUS_WINDOW (user_data);

    nautilus_toolbar_open_location_entry (NAUTILUS_TOOLBAR (self->toolbar), "~");
}

static void
action_redo (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_file_undo_manager_redo (GTK_WINDOW (window), NULL);
}

static void
action_undo (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_file_undo_manager_undo (GTK_WINDOW (window), NULL);
}

static void
action_show_current_location_menu (GSimpleAction *action,
                                   GVariant      *state,
                                   gpointer       user_data)
{
    NautilusWindow *window = user_data;
    GtkWidget *path_bar;

    path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (window->toolbar));

    nautilus_path_bar_show_current_location_menu (NAUTILUS_PATH_BAR (path_bar));
}

static void
action_open_location (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    g_autoptr (GFile) folder_to_open = NULL;

    folder_to_open = g_file_new_for_uri (g_variant_get_string (state, NULL));

    nautilus_window_open_location_full (window, folder_to_open, 0, NULL, NULL);
}

static void
on_location_changed (NautilusWindow *window)
{
    nautilus_window_sync_location_widgets (window);
    nautilus_gtk_places_sidebar_set_location (NAUTILUS_GTK_PLACES_SIDEBAR (window->places_sidebar),
                                              nautilus_window_slot_get_location (nautilus_window_get_active_slot (window)));
}

static void
on_slot_location_changed (NautilusWindowSlot *slot,
                          GParamSpec         *pspec,
                          NautilusWindow     *window)
{
    if (nautilus_window_get_active_slot (window) == slot)
    {
        on_location_changed (window);
    }
}

static void
on_slot_search_global_changed (NautilusWindowSlot *slot,
                               GParamSpec         *pspec,
                               NautilusWindow     *self)
{
    NautilusGtkPlacesSidebar *sidebar = NAUTILUS_GTK_PLACES_SIDEBAR (self->places_sidebar);

    if (nautilus_window_get_active_slot (self) != slot)
    {
        return;
    }

    if (nautilus_window_slot_get_search_global (slot))
    {
        nautilus_gtk_places_sidebar_set_location (sidebar, NULL);
    }
    else
    {
        nautilus_gtk_places_sidebar_set_location (sidebar, nautilus_window_slot_get_location (slot));
    }
}

static void
tab_view_setup_menu_cb (AdwTabView     *tab_view,
                        AdwTabPage     *page,
                        NautilusWindow *window)
{
    GAction *move_tab_left_action;
    GAction *move_tab_right_action;
    GAction *restore_tab_action;
    int position, n_pages;
    gboolean menu_is_closed = (page == NULL);

    window->menu_page = page;

    if (!menu_is_closed)
    {
        position = adw_tab_view_get_page_position (tab_view, page);
        n_pages = adw_tab_view_get_n_pages (tab_view);
    }

    move_tab_left_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                       "tab-move-left");
    move_tab_right_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                        "tab-move-right");
    restore_tab_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                     "restore-tab");

    /* Re-enable all of the actions if the menu is closed */
    g_simple_action_set_enabled (G_SIMPLE_ACTION (move_tab_left_action),
                                 menu_is_closed || position > 0);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (move_tab_right_action),
                                 menu_is_closed || position < n_pages - 1);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (restore_tab_action),
                                 menu_is_closed || g_queue_get_length (window->tab_data_queue) > 0);
}

static void
tab_view_notify_selected_page_cb (AdwTabView     *tab_view,
                                  GParamSpec     *pspec,
                                  NautilusWindow *window)
{
    AdwTabPage *page;
    NautilusWindowSlot *slot;
    GtkWidget *widget;

    page = adw_tab_view_get_selected_page (tab_view);
    widget = adw_tab_page_get_child (page);

    g_assert (widget != NULL);

    /* find slot corresponding to the target page */
    slot = NAUTILUS_WINDOW_SLOT (widget);
    g_assert (slot != NULL);

    nautilus_window_set_active_slot (window, slot);
}

static void
connect_slot (NautilusWindow     *window,
              NautilusWindowSlot *slot)
{
    g_signal_connect_swapped (slot, "notify::allow-stop",
                              G_CALLBACK (update_cursor), window);
    g_signal_connect (slot, "notify::location",
                      G_CALLBACK (on_slot_location_changed), window);
    g_signal_connect (slot, "notify::search-global",
                      G_CALLBACK (on_slot_search_global_changed), window);
}

static void
disconnect_slot (NautilusWindow     *window,
                 NautilusWindowSlot *slot)
{
    g_signal_handlers_disconnect_by_data (slot, window);
}

static NautilusWindowSlot *
nautilus_window_create_and_init_slot (NautilusWindow    *window,
                                      NautilusOpenFlags  flags)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_slot_new ();
    nautilus_window_initialize_slot (window, slot, flags);

    return slot;
}

static void
on_update_page_tooltip (NautilusWindowSlot *slot,
                        GParamSpec         *pspec,
                        AdwTabPage         *page)
{
    GFile *location = nautilus_window_slot_get_location (slot);
    g_autofree char *escaped_name = NULL;

    if (location == NULL)
    {
        return;
    }

    /* Set the tooltip on the label's parent (the tab label hbox),
     * so it covers all of the tab label.
     */

    if (g_file_has_uri_scheme (location, SCHEME_SEARCH))
    {
        escaped_name = g_markup_escape_text (nautilus_window_slot_get_title (slot), -1);
    }
    else
    {
        g_autofree gchar *location_name = g_file_get_parse_name (location);
        escaped_name = g_markup_escape_text (location_name, -1);
    }

    adw_tab_page_set_tooltip (page, escaped_name);
}

void
nautilus_window_initialize_slot (NautilusWindow     *window,
                                 NautilusWindowSlot *slot,
                                 NautilusOpenFlags   flags)
{
    AdwTabPage *page, *current;

    g_assert (NAUTILUS_IS_WINDOW (window));
    g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

    connect_slot (window, slot);

    current = get_current_page (window);
    page = adw_tab_view_add_page (window->tab_view, GTK_WIDGET (slot), current);

    g_object_bind_property (slot, "allow-stop",
                            page, "loading",
                            G_BINDING_SYNC_CREATE);
    g_object_bind_property (slot, "title",
                            page, "title",
                            G_BINDING_SYNC_CREATE);
    g_signal_connect (slot, "notify::title", G_CALLBACK (on_update_page_tooltip), page);
}

void
nautilus_window_open_location_full (NautilusWindow     *window,
                                    GFile              *location,
                                    NautilusOpenFlags   flags,
                                    GList              *selection,
                                    NautilusWindowSlot *target_slot)
{
    NautilusWindowSlot *active_slot;

    /* Assert that we are not managing new windows */
    g_assert (!(flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW));

    active_slot = nautilus_window_get_active_slot (window);
    if (!target_slot)
    {
        target_slot = active_slot;
    }

    if (target_slot == NULL || (flags & NAUTILUS_OPEN_FLAG_NEW_TAB) != 0)
    {
        target_slot = nautilus_window_create_and_init_slot (window, flags);
    }

    /* Make the opened location the one active if we weren't ask for the
     * opposite, since it's the most usual use case */
    if (!(flags & NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE))
    {
        gtk_window_present (GTK_WINDOW (window));
        nautilus_window_set_active_slot (window, target_slot);
    }

    nautilus_window_slot_open_location_full (target_slot, location, flags, selection);
}

static gboolean
nautilus_window_grab_focus (GtkWidget *widget)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (widget));

    if (slot != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (slot));
    }

    return GTK_WIDGET_CLASS (nautilus_window_parent_class)->grab_focus (widget);
}

static void
remove_slot_from_window (NautilusWindowSlot *slot,
                         NautilusWindow     *window)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));
    g_return_if_fail (NAUTILUS_WINDOW (window));

    g_debug ("Removing slot %p", slot);

    disconnect_slot (window, slot);
    window->slots = g_list_remove (window->slots, slot);
    g_signal_emit (window, signals[SLOT_REMOVED], 0, slot);
}

void
nautilus_window_new_tab (NautilusWindow *window)
{
    NautilusWindowSlot *current_slot;
    AdwTabPage *page;
    GFile *location;

    page = get_current_page (window);
    current_slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));
    location = nautilus_window_slot_get_location (current_slot);

    if (location != NULL)
    {
        if (g_file_has_uri_scheme (location, SCHEME_SEARCH))
        {
            location = g_file_new_for_path (g_get_home_dir ());
        }
        else
        {
            g_object_ref (location);
        }

        nautilus_window_open_location_full (window, location,
                                            NAUTILUS_OPEN_FLAG_NEW_TAB,
                                            NULL, NULL);
        g_object_unref (location);
    }
}

static void
update_cursor (NautilusWindow *window)
{
    NautilusWindowSlot *slot = nautilus_window_get_active_slot (window);

    if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    {
        return;
    }

    if (slot != NULL &&
        nautilus_window_slot_get_allow_stop (slot))
    {
        gtk_widget_set_cursor_from_name (GTK_WIDGET (window), "progress");
    }
    else
    {
        gtk_widget_set_cursor (GTK_WIDGET (window), NULL);
    }
}

/* Callback used when the places sidebar changes location; we need to change the displayed folder */
static void
open_location_cb (NautilusWindow             *window,
                  GFile                      *location,
                  NautilusGtkPlacesOpenFlags  open_flags)
{
    NautilusOpenFlags flags;
    NautilusApplication *application;
    AdwOverlaySplitView *split_view = ADW_OVERLAY_SPLIT_VIEW (window->split_view);

    switch (open_flags)
    {
        case NAUTILUS_GTK_PLACES_OPEN_NEW_TAB:
        {
            flags = NAUTILUS_OPEN_FLAG_NEW_TAB |
                    NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
        }
        break;

        case NAUTILUS_GTK_PLACES_OPEN_NEW_WINDOW:
        {
            flags = NAUTILUS_OPEN_FLAG_NEW_WINDOW;
        }
        break;

        case NAUTILUS_GTK_PLACES_OPEN_NORMAL: /* fall-through */
        default:
        {
            flags = 0;
        }
        break;
    }

    application = NAUTILUS_APPLICATION (g_application_get_default ());
    /* FIXME: We shouldn't need to provide the window, but seems gtk_application_get_active_window
     * is not working properly in GtkApplication, so we cannot rely on that...
     */
    nautilus_application_open_location_full (application, location, flags,
                                             NULL, window, NULL);

    if (adw_overlay_split_view_get_collapsed (split_view) &&
        open_flags == NAUTILUS_GTK_PLACES_OPEN_NORMAL)
    {
        adw_overlay_split_view_set_show_sidebar (split_view, FALSE);
    }
}

/* Callback used when the places sidebar needs us to present an error message */
static void
places_sidebar_show_error_message_cb (NautilusGtkPlacesSidebar *sidebar,
                                      const char               *primary,
                                      const char               *secondary,
                                      gpointer                  user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);

    show_dialog (primary, secondary, GTK_WINDOW (window), GTK_MESSAGE_ERROR);
}

static void
places_sidebar_show_starred_location (NautilusWindow             *window,
                                      NautilusGtkPlacesOpenFlags  open_flags)
{
    GFile *location;

    location = g_file_new_for_uri (SCHEME_STARRED ":///");

    open_location_cb (window, location, open_flags);

    g_object_unref (location);
}

/* Callback used when the places sidebar needs to know the drag action to suggest */
static GdkDragAction
places_sidebar_drag_action_requested_cb (NautilusGtkPlacesSidebar *sidebar,
                                         NautilusFile             *dest_file,
                                         GList                    *source_file_list)
{
    return nautilus_dnd_get_preferred_action (dest_file, source_file_list->data);
}
#if 0 && NAUTILUS_DND_NEEDS_GTK4_REIMPLEMENTATION
/* Callback used when the places sidebar needs us to pop up a menu with possible drag actions */
static GdkDragAction
places_sidebar_drag_action_ask_cb (NautilusGtkPlacesSidebar *sidebar,
                                   GdkDragAction             actions,
                                   gpointer                  user_data)
{
    return nautilus_drag_drop_action_ask (GTK_WIDGET (sidebar), actions);
}
#endif
static GList *
build_uri_list_from_gfile_list (GSList *file_list)
{
    GList *result;
    GSList *l;

    result = NULL;

    for (l = file_list; l; l = l->next)
    {
        GFile *file = l->data;
        char *uri;

        uri = g_file_get_uri (file);
        result = g_list_prepend (result, uri);
    }

    return g_list_reverse (result);
}

/* Callback used when the places sidebar has URIs dropped into it.  We do a normal file operation for them. */
static void
places_sidebar_drag_perform_drop_cb (NautilusGtkPlacesSidebar *sidebar,
                                     GFile                    *dest_file,
                                     GSList                   *source_file_list,
                                     GdkDragAction             action,
                                     gpointer                  user_data)
{
    char *dest_uri;
    GList *source_uri_list;

    dest_uri = g_file_get_uri (dest_file);
    source_uri_list = build_uri_list_from_gfile_list (source_file_list);

    nautilus_file_operations_copy_move (source_uri_list, dest_uri, action, GTK_WIDGET (sidebar), NULL, NULL, NULL);

    g_free (dest_uri);
    g_list_free_full (source_uri_list, g_free);
}

static void
action_restore_tab (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusOpenFlags flags;
    g_autoptr (GFile) location = NULL;
    NautilusWindowSlot *slot;
    NautilusNavigationState *data;

    if (g_queue_get_length (window->tab_data_queue) == 0)
    {
        return;
    }

    flags = NAUTILUS_OPEN_FLAG_NEW_TAB | NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;

    data = g_queue_pop_head (window->tab_data_queue);

    location = nautilus_bookmark_get_location (data->current_location_bookmark);

    slot = nautilus_window_create_and_init_slot (window, flags);

    nautilus_window_slot_open_location_full (slot, location, flags, NULL);
    nautilus_window_slot_restore_navigation_state (slot, data);

    free_navigation_state (data);
}

static void
action_toggle_sidebar (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    gboolean revealed;

    revealed = adw_overlay_split_view_get_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->split_view));
    adw_overlay_split_view_set_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (window->split_view), !revealed);
}

static void
nautilus_window_set_up_sidebar (NautilusWindow *window)
{
    nautilus_gtk_places_sidebar_set_open_flags (NAUTILUS_GTK_PLACES_SIDEBAR (window->places_sidebar),
                                                (NAUTILUS_GTK_PLACES_OPEN_NORMAL
                                                 | NAUTILUS_GTK_PLACES_OPEN_NEW_TAB
                                                 | NAUTILUS_GTK_PLACES_OPEN_NEW_WINDOW));

    g_signal_connect_swapped (window->places_sidebar, "open-location",
                              G_CALLBACK (open_location_cb), window);
    g_signal_connect (window->places_sidebar, "show-error-message",
                      G_CALLBACK (places_sidebar_show_error_message_cb), window);

    g_signal_connect (window->places_sidebar, "drag-action-requested",
                      G_CALLBACK (places_sidebar_drag_action_requested_cb), window);
#if 0 && NAUTILUS_DND_NEEDS_GTK4_REIMPLEMENTATION
    g_signal_connect (window->places_sidebar, "drag-action-ask",
                      G_CALLBACK (places_sidebar_drag_action_ask_cb), window);
  #endif
    g_signal_connect (window->places_sidebar, "drag-perform-drop",
                      G_CALLBACK (places_sidebar_drag_perform_drop_cb), window);
}

void
nautilus_window_slot_close (NautilusWindow     *window,
                            NautilusWindowSlot *slot)
{
    NautilusNavigationState *data;
    AdwTabPage *page;

    g_debug ("Requesting to remove slot %p from window %p", slot, window);
    if (window == NULL || slot == NULL)
    {
        return;
    }

    data = nautilus_window_slot_get_navigation_state (slot);
    if (data != NULL)
    {
        g_queue_push_head (window->tab_data_queue, data);
    }

    remove_slot_from_window (slot, window);

    page = adw_tab_view_get_page (window->tab_view, GTK_WIDGET (slot));
    /* this will destroy the slot */
    adw_tab_view_close_page (window->tab_view, page);

    /* If that was the last slot in the window, close the window. */
    if (window->slots == NULL)
    {
        g_debug ("Last slot removed, closing the window");
        nautilus_window_close (window);
    }
}

static void
nautilus_window_sync_bookmarks (NautilusWindow *window)
{
    gboolean can_bookmark = FALSE;
    NautilusWindowSlot *slot;
    NautilusBookmarkList *bookmarks;
    GAction *action;
    GFile *location;

    slot = window->active_slot;
    location = slot != NULL ? nautilus_window_slot_get_location (slot) : NULL;

    if (location != NULL)
    {
        bookmarks = nautilus_application_get_bookmarks
                        (NAUTILUS_APPLICATION (gtk_window_get_application (GTK_WINDOW (window))));
        can_bookmark = nautilus_bookmark_list_can_bookmark_location (bookmarks, location);
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (window), "bookmark-current-location");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_bookmark);
}

static void
nautilus_window_sync_starred (NautilusWindow *window)
{
    NautilusWindowSlot *slot = nautilus_window_get_active_slot (window);
    GFile *location = slot != NULL ? nautilus_window_slot_get_location (slot) : NULL;

    gboolean can_star_location = FALSE;
    gboolean is_starred = FALSE;

    if (location != NULL)
    {
        g_autofree gchar *uri = g_file_get_uri (location);
        if (uri)
        {
            NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
            can_star_location = nautilus_tag_manager_can_star_location (tag_manager, location);
            is_starred = nautilus_tag_manager_file_is_starred (tag_manager, uri);
        }
    }

    GAction *star_action = g_action_map_lookup_action (G_ACTION_MAP (window), "star-current-location");
    GAction *unstar_action = g_action_map_lookup_action (G_ACTION_MAP (window), "unstar-current-location");

    if (can_star_location)
    {
        g_simple_action_set_enabled (G_SIMPLE_ACTION (star_action), !is_starred);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (unstar_action), is_starred);
    }
    else
    {
        g_simple_action_set_enabled (G_SIMPLE_ACTION (star_action), FALSE);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (unstar_action), FALSE);
    }
}

static void
nautilus_window_sync_location_widgets (NautilusWindow *window)
{
    NautilusWindowSlot *slot = window->active_slot;
    GFile *location;

    /* This function can only be called when there is a slot. */
    g_assert (slot != NULL);

    location = nautilus_window_slot_get_location (slot);

    if (location != NULL)
    {
        gtk_widget_set_visible (window->network_address_bar,
                                g_file_has_uri_scheme (location, SCHEME_NETWORK_VIEW));
    }

    nautilus_window_sync_bookmarks (window);
    nautilus_window_sync_starred (window);
}

static gchar *
toast_undo_deleted_get_label (NautilusFileUndoInfo *undo_info)
{
    g_autoptr (GList) files = nautilus_file_undo_info_trash_get_files (NAUTILUS_FILE_UNDO_INFO_TRASH (undo_info));
    gchar *file_label;
    gchar *label;
    gint length;

    length = g_list_length (files);
    if (length == 1)
    {
        file_label = g_file_get_basename (files->data);
        /* Translators: only one item has been moved to trash and %s is its name. */
        label = g_markup_printf_escaped (_("“%s” moved to trash"), file_label);
        g_free (file_label);
    }
    else
    {
        /* Translators: one or more items might have been moved to trash, and %d
         * is the count. */
        label = g_markup_printf_escaped (ngettext ("%d file moved to trash", "%d files moved to trash", length), length);
    }

    return label;
}

static gchar *
toast_undo_unstar_get_label (NautilusFileUndoInfo *undo_info)
{
    GList *files;
    gchar *label;
    gint length;

    files = nautilus_file_undo_info_starred_get_files (NAUTILUS_FILE_UNDO_INFO_STARRED (undo_info));
    length = g_list_length (files);
    if (length == 1)
    {
        const char *file_label = nautilus_file_get_display_name (files->data);
        /* Translators: one item has been unstarred and %s is its name. */
        label = g_markup_printf_escaped (_("“%s” unstarred"), file_label);
    }
    else
    {
        /* Translators: one or more items have been unstarred, and %d
         * is the count. */
        label = g_markup_printf_escaped (ngettext ("%d file unstarred", "%d files unstarred", length), length);
    }

    return label;
}

static void
update_undo_redo_menu_items (NautilusWindow               *window,
                             NautilusFileUndoInfo         *info,
                             NautilusFileUndoManagerState  undo_state)
{
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
    GAction *action;

    /* Look at the last action from the undo manager, and get the text that
     * describes it, e.g. "Undo Create Folder"/"Redo Create Folder"
     */
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
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "undo");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), undo_active);

    if (!redo_active || redo_label == NULL)
    {
        g_free (redo_label);
        redo_label = g_strdup (_("_Redo"));
    }
    redo_menu_item = g_menu_item_new (redo_label, "win.redo");
    g_menu_append_item (updated_section, redo_menu_item);
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "redo");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), redo_active);

    nautilus_gmenu_set_from_model (G_MENU (window->undo_redo_section),
                                   G_MENU_MODEL (updated_section));
}

static void
nautilus_window_on_undo_changed (NautilusFileUndoManager *manager,
                                 NautilusWindow          *window)
{
    NautilusFileUndoInfo *undo_info;
    NautilusFileUndoManagerState state;
    AdwToast *toast;

    undo_info = nautilus_file_undo_manager_get_action ();
    state = nautilus_file_undo_manager_get_state ();

    update_undo_redo_menu_items (window, undo_info, state);

    if (undo_info != NULL &&
        state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO)
    {
        gboolean popup_toast = FALSE;
        g_autofree gchar *label = NULL;

        if (nautilus_file_undo_info_get_op_type (undo_info) == NAUTILUS_FILE_UNDO_OP_MOVE_TO_TRASH)
        {
            g_autoptr (GList) files = NULL;

            files = nautilus_file_undo_info_trash_get_files (NAUTILUS_FILE_UNDO_INFO_TRASH (undo_info));

            /* Don't pop up a notification if user canceled the operation or the focus
             * is not in the this window. This is an easy way to know from which window
             * was the delete operation made */
            if (files != NULL && gtk_window_is_active (GTK_WINDOW (window)))
            {
                popup_toast = TRUE;
                label = toast_undo_deleted_get_label (undo_info);
            }
        }
        else if (nautilus_file_undo_info_get_op_type (undo_info) == NAUTILUS_FILE_UNDO_OP_STARRED)
        {
            NautilusWindowSlot *active_slot;
            GFile *location;

            active_slot = nautilus_window_get_active_slot (window);
            location = nautilus_window_slot_get_location (active_slot);
            /* Don't pop up a notification if the focus is not in the this
             * window. This is an easy way to know from which window was the
             * unstart operation made */
            if (g_file_has_uri_scheme (location, SCHEME_STARRED) &&
                gtk_window_is_active (GTK_WINDOW (window)) &&
                !nautilus_file_undo_info_starred_is_starred (NAUTILUS_FILE_UNDO_INFO_STARRED (undo_info)))
            {
                popup_toast = TRUE;
                label = toast_undo_unstar_get_label (undo_info);
            }
        }

        if (window->last_undo_toast != NULL)
        {
            adw_toast_dismiss (window->last_undo_toast);
            g_clear_weak_pointer (&window->last_undo_toast);
        }

        if (popup_toast)
        {
            toast = adw_toast_new (label);
            adw_toast_set_button_label (toast, _("Undo"));
            adw_toast_set_action_name (toast, "win.undo");
            adw_toast_set_priority (toast, ADW_TOAST_PRIORITY_HIGH);
            g_set_weak_pointer (&window->last_undo_toast, toast);
            adw_toast_overlay_add_toast (window->toast_overlay, toast);
        }
    }
}

void
nautilus_window_show_operation_notification (NautilusWindow *window,
                                             gchar          *main_label,
                                             GFile          *folder_to_open,
                                             gboolean        was_quick)
{
    gboolean is_current_location;
    AdwToast *toast;

    if (window->active_slot == NULL || !gtk_window_is_active (GTK_WINDOW (window)))
    {
        return;
    }

    is_current_location = g_file_equal (folder_to_open,
                                        nautilus_window_slot_get_location (window->active_slot));

    if (is_current_location && was_quick)
    {
        return;
    }

    toast = adw_toast_new (main_label);
    adw_toast_set_priority (toast, ADW_TOAST_PRIORITY_HIGH);
    adw_toast_set_use_markup (toast, FALSE);

    if (!is_current_location)
    {
        g_autoptr (NautilusFile) folder = NULL;
        g_autofree gchar *button_label = NULL;
        GVariant *target;

        target = g_variant_new_take_string (g_file_get_uri (folder_to_open));
        folder = nautilus_file_get (folder_to_open);
        button_label = g_strdup_printf (_("Open %s"),
                                        nautilus_file_get_display_name (folder));
        adw_toast_set_button_label (toast, button_label);
        adw_toast_set_action_name (toast, "win.open-location");
        adw_toast_set_action_target_value (toast, target);
    }

    adw_toast_overlay_add_toast (window->toast_overlay, toast);
}

static gboolean
tab_view_close_page_cb (AdwTabView     *view,
                        AdwTabPage     *page,
                        NautilusWindow *window)
{
    NautilusWindowSlot *slot;

    slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));

    nautilus_window_slot_close (window, slot);

    return GDK_EVENT_PROPAGATE;
}

static void
tab_view_page_detached_cb (AdwTabView     *tab_view,
                           AdwTabPage     *page,
                           gint            position,
                           NautilusWindow *window)
{
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));

    /* If the tab has been moved to another window, we need to remove the slot
     * from the current window here. Otherwise, if the tab has been closed, then
     * we have*/
    if (g_list_find (window->slots, slot))
    {
        remove_slot_from_window (slot, window);
    }
}

static void
tab_view_page_attached_cb (AdwTabView     *tab_view,
                           AdwTabPage     *page,
                           gint            position,
                           NautilusWindow *window)
{
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));

    window->slots = g_list_append (window->slots, slot);
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);
}

static AdwTabView *
tab_view_create_window_cb (AdwTabView     *tab_view,
                           NautilusWindow *window)
{
    NautilusApplication *app;
    NautilusWindow *new_window;

    app = NAUTILUS_APPLICATION (g_application_get_default ());
    new_window = nautilus_application_create_window (app);
    gtk_window_set_display (GTK_WINDOW (new_window),
                            gtk_widget_get_display (GTK_WIDGET (tab_view)));

    gtk_window_present (GTK_WINDOW (new_window));

    return new_window->tab_view;
}

static void
action_tab_move_new_window (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
    NautilusWindow *window = user_data;
    AdwTabPage *page = get_current_page (window);
    AdwTabView *new_view = tab_view_create_window_cb (window->tab_view, window);

    adw_tab_view_transfer_page (window->tab_view, page, new_view, 0);
}

static void
setup_tab_view (NautilusWindow *window)
{
    g_signal_connect (window->tab_view, "close-page",
                      G_CALLBACK (tab_view_close_page_cb),
                      window);
    g_signal_connect (window->tab_view, "setup-menu",
                      G_CALLBACK (tab_view_setup_menu_cb),
                      window);
    g_signal_connect (window->tab_view, "notify::selected-page",
                      G_CALLBACK (tab_view_notify_selected_page_cb),
                      window);
    g_signal_connect (window->tab_view, "create-window",
                      G_CALLBACK (tab_view_create_window_cb),
                      window);
    g_signal_connect (window->tab_view, "page-attached",
                      G_CALLBACK (tab_view_page_attached_cb),
                      window);
    g_signal_connect (window->tab_view, "page-detached",
                      G_CALLBACK (tab_view_page_detached_cb),
                      window);
}

static GdkDragAction
extra_drag_value_cb (AdwTabBar    *self,
                     AdwTabPage   *page,
                     const GValue *value,
                     gpointer      user_data)
{
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));
    g_autoptr (NautilusFile) file = nautilus_file_get (nautilus_window_slot_get_location (slot));
    GdkDragAction action = 0;

    if (value != NULL)
    {
        if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
        {
            GSList *file_list = g_value_get_boxed (value);
            if (file_list != NULL)
            {
                action = nautilus_dnd_get_preferred_action (file, G_FILE (file_list->data));
            }
        }
        else if (G_VALUE_HOLDS (value, G_TYPE_STRING) || G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
        {
            action = GDK_ACTION_COPY;
        }
    }

    return action;
}

static gboolean
extra_drag_drop_cb (AdwTabBar    *self,
                    AdwTabPage   *page,
                    const GValue *value,
                    gpointer      user_data)
{
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (adw_tab_page_get_child (page));
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (nautilus_window_slot_get_current_view (slot));
    GFile *target_location = nautilus_window_slot_get_location (slot);
    GdkDragAction action = adw_tab_bar_get_extra_drag_preferred_action (self);

    return nautilus_dnd_perform_drop (view, value, action, target_location);
}

const GActionEntry win_entries[] =
{
    { .name = "current-location-menu", .activate = action_show_current_location_menu },
    { .name = "open-location", .activate = action_open_location, .parameter_type = "s" },
    { .name = "new-tab", .activate = action_new_tab },
    { .name = "enter-location", .activate = action_enter_location },
    { .name = "bookmark-current-location", .activate = action_bookmark_current_location },
    { .name = "star-current-location", .activate = action_star_current_location },
    { .name = "unstar-current-location", .activate = action_unstar_current_location },
    { .name = "undo", .activate = action_undo },
    { .name = "redo", .activate = action_redo },
    /* Only accessible by shorcuts */
    { .name = "close-current-view", .activate = action_close_current_view },
    { .name = "close-other-tabs", .activate = action_close_other_tabs },
    { .name = "go-home", .activate = action_go_home },
    { .name = "go-starred", .activate = action_go_starred },
    { .name = "tab-move-left", .activate = action_tab_move_left },
    { .name = "tab-move-right", .activate = action_tab_move_right },
    { .name = "tab-move-new-window", .activate = action_tab_move_new_window },
    { .name = "prompt-root-location", .activate = action_prompt_for_location_root },
    { .name = "prompt-home-location", .activate = action_prompt_for_location_home },
    { .name = "go-to-tab", .parameter_type = "i", .state = "0", .change_state = action_go_to_tab },
    { .name = "restore-tab", .activate = action_restore_tab },
    { .name = "toggle-sidebar", .activate = action_toggle_sidebar },
};

static void
nautilus_window_initialize_actions (NautilusWindow *window)
{
    GApplication *app;
    GAction *action;
    gchar detailed_action[80];
    gchar accel[80];
    gint i;

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     win_entries, G_N_ELEMENTS (win_entries),
                                     window);

#define ACCELS(...) ((const char *[]) { __VA_ARGS__, NULL })

    app = g_application_get_default ();
    nautilus_application_set_accelerators (app, "win.enter-location", ACCELS ("<control>l", "Go", "OpenURL"));
    nautilus_application_set_accelerator (app, "win.new-tab", "<control>t");
    nautilus_application_set_accelerator (app, "win.close-current-view", "<control>w");

    nautilus_application_set_accelerator (app, "win.undo", "<control>z");
    nautilus_application_set_accelerator (app, "win.redo", "<shift><control>z");
    /* Only accessible by shorcuts */
    nautilus_application_set_accelerators (app, "win.bookmark-current-location", ACCELS ("<control>d", "AddFavorite"));
    nautilus_application_set_accelerators (app, "win.go-home", ACCELS ("<alt>Home", "HomePage", "Start"));
    nautilus_application_set_accelerator (app, "win.go-starred", "Favorites");
    nautilus_application_set_accelerator (app, "win.tab-move-left", "<shift><control>Page_Up");
    nautilus_application_set_accelerator (app, "win.tab-move-right", "<shift><control>Page_Down");
    nautilus_application_set_accelerators (app, "win.prompt-root-location", ACCELS ("slash", "KP_Divide"));
    /* Support keyboard layouts which have a dead tilde key but not a tilde key. */
    nautilus_application_set_accelerators (app, "win.prompt-home-location", ACCELS ("asciitilde", "dead_tilde"));
    nautilus_application_set_accelerator (app, "win.current-location-menu", "F10");
    nautilus_application_set_accelerator (app, "win.restore-tab", "<shift><control>t");
    nautilus_application_set_accelerator (app, "win.toggle-sidebar", "F9");

    /* Alt+N for the first 9 tabs */
    for (i = 0; i < 9; ++i)
    {
        g_snprintf (detailed_action, sizeof (detailed_action), "win.go-to-tab(%i)", i);
        g_snprintf (accel, sizeof (accel), "<alt>%i", i + 1);
        nautilus_application_set_accelerator (app, detailed_action, accel);
    }

    nautilus_window_on_undo_changed (nautilus_file_undo_manager_get (), window);

#undef ACCELS

    action = g_action_map_lookup_action (G_ACTION_MAP (window), "toggle-sidebar");
    g_object_bind_property (window->split_view, "collapsed",
                            action, "enabled", G_BINDING_SYNC_CREATE);
}

static gboolean
is_layout_reversed (void)
{
    GtkSettings *settings = gtk_settings_get_default ();
    g_autofree char *layout = NULL;
    g_auto (GStrv) parts = NULL;

    if (settings != NULL)
    {
        g_object_get (settings, "gtk-decoration-layout", &layout, NULL);
    }

    if (layout != NULL)
    {
        parts = g_strsplit (layout, ":", 2);
    }

    /* Invalid layout, don't even try */
    if (parts == NULL || g_strv_length (parts) < 2)
    {
        return FALSE;
    }

    g_return_val_if_fail (parts[0] != NULL, FALSE);
    return (g_strrstr (parts[0], "close") != NULL);
}

static void
notify_decoration_layout_cb (NautilusWindow *self)
{
    gboolean inverted = is_layout_reversed ();

    if (ADW_IS_TAB_BAR (self->tab_bar))
    {
        adw_tab_bar_set_inverted (self->tab_bar, inverted);
    }
}

static void
nautilus_window_constructed (GObject *self)
{
    NautilusWindow *window;
    NautilusApplication *application;
    GtkSettings *settings;

    window = NAUTILUS_WINDOW (self);

    G_OBJECT_CLASS (nautilus_window_parent_class)->constructed (self);

    application = NAUTILUS_APPLICATION (g_application_get_default ());
    gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

    gtk_window_set_default_size (GTK_WINDOW (window),
                                 NAUTILUS_WINDOW_DEFAULT_WIDTH,
                                 NAUTILUS_WINDOW_DEFAULT_HEIGHT);

    setup_tab_view (window);

    /* Only allow tab DnD in Wayland.  We are using a hack in list-base to
     * get the preferred action which we can not replicate here because we
     * don't have access to the GtkDropTarget to check the actions on the GdkDrag.
     * See: https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982
     */
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        adw_tab_bar_setup_extra_drop_target (window->tab_bar,
                                             GDK_ACTION_COPY | GDK_ACTION_MOVE,
                                             (GType [3]) {GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST, G_TYPE_STRING}, 3);
        adw_tab_bar_set_extra_drag_preload (window->tab_bar, TRUE);
        g_signal_connect (window->tab_bar, "extra-drag-value", G_CALLBACK (extra_drag_value_cb), NULL);
        g_signal_connect (window->tab_bar, "extra-drag-drop", G_CALLBACK (extra_drag_drop_cb), NULL);
    }
#endif

    settings = gtk_settings_get_default ();
    g_signal_connect_object (settings, "notify::gtk-decoration-layout",
                             G_CALLBACK (notify_decoration_layout_cb), window,
                             G_CONNECT_SWAPPED);
    notify_decoration_layout_cb (window);

    nautilus_window_set_up_sidebar (window);


    g_signal_connect_object (nautilus_file_undo_manager_get (), "undo-changed",
                             G_CALLBACK (nautilus_window_on_undo_changed), self,
                             G_CONNECT_AFTER);

    /* Is required that the UI is constructed before initializating the actions, since
     * some actions trigger UI widgets to show/hide. */
    nautilus_window_initialize_actions (window);

    window->bookmarks_id = g_signal_connect_object (nautilus_application_get_bookmarks (application),
                                                    "changed",
                                                    G_CALLBACK (nautilus_window_sync_bookmarks),
                                                    window, G_CONNECT_SWAPPED);

    window->starred_id = g_signal_connect_object (nautilus_tag_manager_get (),
                                                  "starred-changed",
                                                  G_CALLBACK (nautilus_window_sync_starred),
                                                  window, G_CONNECT_SWAPPED);
}

static void
nautilus_window_dispose (GObject *object)
{
    NautilusWindow *window;
    GtkApplication *application;
    GList *slots_copy;

    window = NAUTILUS_WINDOW (object);
    application = gtk_window_get_application (GTK_WINDOW (window));

    g_debug ("Destroying window");

    /* close all slots safely */
    slots_copy = g_list_copy (window->slots);
    g_list_foreach (slots_copy, (GFunc) remove_slot_from_window, window);
    g_list_free (slots_copy);

    /* the slots list should now be empty */
    g_assert (window->slots == NULL);

    g_clear_weak_pointer (&window->active_slot);

    if (application != NULL)
    {
        g_clear_signal_handler (&window->bookmarks_id,
                                nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (application)));
        g_clear_signal_handler (&window->starred_id, nautilus_tag_manager_get ());
    }

    gtk_widget_dispose_template (GTK_WIDGET (window), NAUTILUS_TYPE_WINDOW);

    G_OBJECT_CLASS (nautilus_window_parent_class)->dispose (object);
}

static void
nautilus_window_finalize (GObject *object)
{
    NautilusWindow *window;

    window = NAUTILUS_WINDOW (object);

    if (window->sidebar_width_handler_id != 0)
    {
        g_source_remove (window->sidebar_width_handler_id);
        window->sidebar_width_handler_id = 0;
    }

    g_clear_object (&window->selected_file);
    g_clear_object (&window->selected_volume);

    g_signal_handlers_disconnect_by_func (nautilus_file_undo_manager_get (),
                                          G_CALLBACK (nautilus_window_on_undo_changed),
                                          window);

    g_queue_free_full (window->tab_data_queue, free_navigation_state);

    /* nautilus_window_close() should have run */
    g_assert (window->slots == NULL);

    G_OBJECT_CLASS (nautilus_window_parent_class)->finalize (object);
}

static void
nautilus_window_save_geometry (NautilusWindow *window)
{
    gint width;
    gint height;
    GVariant *initial_size;

    gtk_window_get_default_size (GTK_WINDOW (window), &width, &height);
    initial_size = g_variant_new_parsed ("(%i, %i)", width, height);

    g_settings_set_value (nautilus_window_state,
                          NAUTILUS_WINDOW_STATE_INITIAL_SIZE,
                          initial_size);
}

void
nautilus_window_close (NautilusWindow *window)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW (window));

    nautilus_window_save_geometry (window);
    nautilus_window_set_active_slot (window, NULL);

    /* The pad controller hold a reference to the window, creating a cycle.
     * Usually, reference cycles are resolved in dispose(), but GTK removes the
     * controllers in finalize(), so our only option is to manually remove it
     * here before starting the destruction of the window. */
    if (window->pad_controller != NULL)
    {
        gtk_widget_remove_controller (GTK_WIDGET (window),
                                      GTK_EVENT_CONTROLLER (window->pad_controller));
        g_clear_weak_pointer (&window->pad_controller);
    }

    gtk_window_destroy (GTK_WINDOW (window));
}

void
nautilus_window_set_active_slot (NautilusWindow     *window,
                                 NautilusWindowSlot *new_slot)
{
    NautilusWindowSlot *old_slot;

    g_assert (NAUTILUS_IS_WINDOW (window));

    if (new_slot != NULL)
    {
        g_assert (gtk_widget_is_ancestor (GTK_WIDGET (new_slot), GTK_WIDGET (window)));
    }

    old_slot = nautilus_window_get_active_slot (window);

    if (old_slot == new_slot)
    {
        return;
    }

    g_debug ("Setting new slot %p as active, old slot inactive %p", new_slot, old_slot);

    /* make old slot inactive if it exists (may be NULL after init, for example) */
    if (old_slot != NULL)
    {
        /* inform slot & view */
        nautilus_window_slot_set_active (old_slot, FALSE);
    }

    g_set_weak_pointer (&window->active_slot, new_slot);

    /* make new slot active, if it exists */
    if (new_slot)
    {
        AdwTabPage *page = adw_tab_view_get_page (window->tab_view,
                                                  GTK_WIDGET (new_slot));
        adw_tab_view_set_selected_page (window->tab_view, page);

        /* inform slot & view */
        nautilus_window_slot_set_active (new_slot, TRUE);

        on_location_changed (window);
        update_cursor (window);
    }

    g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_ACTIVE_SLOT]);
}

static void
nautilus_window_realize (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (nautilus_window_parent_class)->realize (widget);
    update_cursor (NAUTILUS_WINDOW (widget));
}

static gboolean
nautilus_window_key_capture (GtkEventControllerKey *controller,
                             unsigned int           keyval,
                             unsigned int           keycode,
                             GdkModifierType        state,
                             gpointer               user_data)
{
    GtkWidget *widget;
    GtkWidget *focus_widget;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    focus_widget = gtk_window_get_focus (GTK_WINDOW (widget));
    if (focus_widget != NULL && GTK_IS_EDITABLE (focus_widget))
    {
        /* if we have input focus on a GtkEditable (e.g. a GtkEntry), forward
         * the event to it before activating accelerators. This allows, e.g.,
         * typing a tilde without activating the prompt-home-location action.
         */
        if (gtk_event_controller_key_forward (controller, focus_widget))
        {
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
nautilus_window_key_bubble (GtkEventControllerKey *controller,
                            unsigned int           keyval,
                            unsigned int           keycode,
                            GdkModifierType        state,
                            gpointer               user_data)
{
    GtkWidget *widget;
    NautilusWindow *window;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    window = NAUTILUS_WINDOW (widget);
    if (window->active_slot != NULL &&
        nautilus_window_slot_handle_event (window->active_slot, controller, keyval, state))
    {
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

/**
 * nautilus_window_show:
 * @widget: GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nautilus_window_show (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (nautilus_window_parent_class)->show (widget);
}

NautilusWindowSlot *
nautilus_window_get_active_slot (NautilusWindow *window)
{
    g_assert (NAUTILUS_IS_WINDOW (window));

    return window->active_slot;
}

GList *
nautilus_window_get_slots (NautilusWindow *window)
{
    g_assert (NAUTILUS_IS_WINDOW (window));

    return window->slots;
}

static void
on_is_maximized_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    gboolean is_maximized;

    is_maximized = gtk_window_is_maximized (GTK_WINDOW (object));

    g_settings_set_boolean (nautilus_window_state,
                            NAUTILUS_WINDOW_STATE_MAXIMIZED, is_maximized);
}

static gboolean
nautilus_window_close_request (GtkWindow *window)
{
    nautilus_window_close (NAUTILUS_WINDOW (window));
    return FALSE;
}

static void
nautilus_window_back_or_forward (NautilusWindow *window,
                                 gboolean        back,
                                 guint           distance)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (window);

    if (slot != NULL)
    {
        nautilus_window_slot_back_or_forward (slot, back, distance);
    }
}

void
nautilus_window_back_or_forward_in_new_tab (NautilusWindow              *window,
                                            NautilusNavigationDirection  direction)
{
    GFile *location;
    NautilusWindowSlot *window_slot;
    NautilusWindowSlot *new_slot;
    NautilusNavigationState *state;

    window_slot = nautilus_window_get_active_slot (window);
    new_slot = nautilus_window_slot_new ();
    state = nautilus_window_slot_get_navigation_state (window_slot);

    /* Manually fix up the back / forward lists and location.
     * This way we don't have to unnecessary load the current location
     * and then load back / forward */
    switch (direction)
    {
        case NAUTILUS_NAVIGATION_DIRECTION_BACK:
        {
            state->forward_list = g_list_prepend (state->forward_list, state->current_location_bookmark);
            state->current_location_bookmark = state->back_list->data;
            state->back_list = state->back_list->next;
            g_clear_object (&state->current_search_query);
        }
        break;

        case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
        {
            state->back_list = g_list_prepend (state->back_list, state->current_location_bookmark);
            state->current_location_bookmark = state->forward_list->data;
            state->forward_list = state->forward_list->next;
            g_clear_object (&state->current_search_query);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    location = nautilus_bookmark_get_location (state->current_location_bookmark);
    nautilus_window_initialize_slot (window, new_slot, NAUTILUS_OPEN_FLAG_NEW_TAB);
    nautilus_window_slot_open_location_full (new_slot, location, 0, NULL);
    nautilus_window_slot_restore_navigation_state (new_slot, state);

    free_navigation_state (state);
}

static void
on_click_gesture_pressed (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    GtkWidget *widget;
    NautilusWindow *window;
    guint button;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    window = NAUTILUS_WINDOW (widget);
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (mouse_extra_buttons && (button == mouse_back_button))
    {
        nautilus_window_back_or_forward (window, TRUE, 0);
    }
    else if (mouse_extra_buttons && (button == mouse_forward_button))
    {
        nautilus_window_back_or_forward (window, FALSE, 0);
    }
}

static void
mouse_back_button_changed (gpointer callback_data)
{
    int new_back_button;

    new_back_button = g_settings_get_uint (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON);

    /* Bounds checking */
    if (new_back_button < 6 || new_back_button > UPPER_MOUSE_LIMIT)
    {
        return;
    }

    mouse_back_button = new_back_button;
}

static void
mouse_forward_button_changed (gpointer callback_data)
{
    int new_forward_button;

    new_forward_button = g_settings_get_uint (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON);

    /* Bounds checking */
    if (new_forward_button < 6 || new_forward_button > UPPER_MOUSE_LIMIT)
    {
        return;
    }

    mouse_forward_button = new_forward_button;
}

static void
use_extra_mouse_buttons_changed (gpointer callback_data)
{
    mouse_extra_buttons = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS);
}

static void
nautilus_window_init (NautilusWindow *window)
{
    GtkWindowGroup *window_group;
    GtkEventController *controller;
    GtkPadController *pad_controller;

    g_type_ensure (NAUTILUS_TYPE_NETWORK_ADDRESS_BAR);
    g_type_ensure (NAUTILUS_TYPE_TOOLBAR);
    g_type_ensure (NAUTILUS_TYPE_GTK_PLACES_SIDEBAR);
    g_type_ensure (NAUTILUS_TYPE_PROGRESS_INDICATOR);
    g_type_ensure (NAUTILUS_TYPE_SHORTCUT_MANAGER);
    gtk_widget_init_template (GTK_WIDGET (window));

    g_signal_connect_object (window->places_sidebar,
                             "show-starred-location",
                             G_CALLBACK (places_sidebar_show_starred_location),
                             window,
                             G_CONNECT_SWAPPED);

    g_signal_connect (window, "notify::maximized",
                      G_CALLBACK (on_is_maximized_changed), NULL);

    window->slots = NULL;
    window->active_slot = NULL;

    gtk_widget_add_css_class (GTK_WIDGET (window), "nautilus-window");

    window_group = gtk_window_group_new ();
    gtk_window_group_add_window (window_group, GTK_WINDOW (window));
    g_object_unref (window_group);

    window->tab_data_queue = g_queue_new ();

    /* Attention: this creates a reference cycle: the pad controller owns a
     * reference to the window (as an action group) and the window (as a widget)
     * owns a reference to the pad controller. To break this, we must remove
     * the controller from the window before destroying the window. But we need
     * to know the controller is still alive before trying to remove it, so a
     * weak reference is added. */
    pad_controller = gtk_pad_controller_new (G_ACTION_GROUP (window), NULL);
    g_set_weak_pointer (&window->pad_controller, pad_controller);
    gtk_pad_controller_set_action_entries (window->pad_controller,
                                           pad_actions, G_N_ELEMENTS (pad_actions));
    gtk_widget_add_controller (GTK_WIDGET (window),
                               GTK_EVENT_CONTROLLER (window->pad_controller));

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (window), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_click_gesture_pressed), NULL);

    controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (window), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (nautilus_window_key_capture), NULL);

    controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (window), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (nautilus_window_key_bubble), NULL);
}

static void
nautilus_window_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    NautilusWindow *self = NAUTILUS_WINDOW (object);

    switch (prop_id)
    {
        case PROP_ACTIVE_SLOT:
        {
            g_value_set_object (value, G_OBJECT (nautilus_window_get_active_slot (self)));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_window_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    NautilusWindow *self = NAUTILUS_WINDOW (object);

    switch (prop_id)
    {
        case PROP_ACTIVE_SLOT:
        {
            nautilus_window_set_active_slot (self, NAUTILUS_WINDOW_SLOT (g_value_get_object (value)));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
    GObjectClass *oclass = G_OBJECT_CLASS (class);
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);
    GtkWindowClass *winclass = GTK_WINDOW_CLASS (class);

    oclass->dispose = nautilus_window_dispose;
    oclass->finalize = nautilus_window_finalize;
    oclass->constructed = nautilus_window_constructed;
    oclass->get_property = nautilus_window_get_property;
    oclass->set_property = nautilus_window_set_property;

    wclass->show = nautilus_window_show;
    wclass->realize = nautilus_window_realize;
    wclass->grab_focus = nautilus_window_grab_focus;

    winclass->close_request = nautilus_window_close_request;

    properties[PROP_ACTIVE_SLOT] =
        g_param_spec_object ("active-slot",
                             NULL, NULL,
                             NAUTILUS_TYPE_WINDOW_SLOT,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (wclass,
                                                 "/org/gnome/nautilus/ui/nautilus-window.ui");
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, undo_redo_section);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, toolbar);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, split_view);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, places_sidebar);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, toast_overlay);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, tab_view);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, tab_bar);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, network_address_bar);

    signals[SLOT_ADDED] =
        g_signal_new ("slot-added",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, NAUTILUS_TYPE_WINDOW_SLOT);
    signals[SLOT_REMOVED] =
        g_signal_new ("slot-removed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1, NAUTILUS_TYPE_WINDOW_SLOT);

    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON,
                              G_CALLBACK (mouse_back_button_changed),
                              NULL);

    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON,
                              G_CALLBACK (mouse_forward_button_changed),
                              NULL);

    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS,
                              G_CALLBACK (use_extra_mouse_buttons_changed),
                              NULL);
}

NautilusWindow *
nautilus_window_new (void)
{
    return g_object_new (NAUTILUS_TYPE_WINDOW,
                         "icon-name", APPLICATION_ID,
                         NULL);
}

void
nautilus_window_show_about_dialog (NautilusWindow *window)
{
    g_autofree gchar *module_names = nautilus_module_get_installed_module_names ();
    g_autofree gchar *debug_info = NULL;

    const gchar *designers[] =
    {
        _("The GNOME Project"),
        NULL
    };
    const gchar *developers[] =
    {
        _("The GNOME Project"),
        NULL
    };
    const gchar *documenters[] =
    {
        _("The GNOME Project"),
        "Sun Microsystems",
        NULL
    };

    if (module_names == NULL)
    {
        debug_info = g_strdup (_("No plugins currently installed."));
    }
    else
    {
        debug_info = g_strconcat (_("Currently installed plugins:"), "\n\n",
                                  module_names, "\n\n",
                                  _("For bug testing only, the following command can be used:"), "\n"
                                  "NAUTILUS_DISABLE_PLUGINS=TRUE nautilus", NULL);
    }

    adw_show_about_window (window ? GTK_WINDOW (window) : NULL,
                           "application-name", _("Files"),
                           "application-icon", APPLICATION_ID,
                           "developer-name", _("The GNOME Project"),
                           "version", VERSION,
                           "website", "https://apps.gnome.org/Nautilus/",
                           "issue-url", "https://gitlab.gnome.org/GNOME/nautilus/-/issues",
                           "support-url", "https://discourse.gnome.org/tag/nautilus",
                           "debug-info", debug_info,
                           "copyright", "© 1999 The Files Authors",
                           "license-type", GTK_LICENSE_GPL_3_0,
                           "designers", designers,
                           "developers", developers,
                           "documenters", documenters,
                           /* Translators should localize the following string
                            * which will be displayed at the bottom of the about
                            * box to give credit to the translator(s).
                            */
                           "translator-credits", _("translator-credits"),
                           NULL);
}

void
nautilus_window_search (NautilusWindow *window,
                        NautilusQuery  *query)
{
    NautilusWindowSlot *active_slot;

    active_slot = nautilus_window_get_active_slot (window);
    if (active_slot)
    {
        nautilus_window_slot_search (active_slot, query);
    }
    else
    {
        g_warning ("Trying search on a slot but no active slot present");
    }
}
