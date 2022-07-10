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

#include "nautilus-window.h"

#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <sys/time.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include "nautilus-debug.h"

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
#include "nautilus-location-entry.h"
#include "nautilus-metadata.h"
#include "nautilus-mime-actions.h"
#include "nautilus-module.h"
#include "nautilus-notebook.h"
#include "nautilus-pathbar.h"
#include "nautilus-profile.h"
#include "nautilus-signaller.h"
#include "nautilus-toolbar.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window-slot.h"

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static int mouse_forward_button = 9;
static int mouse_back_button = 8;

static void mouse_back_button_changed (gpointer callback_data);
static void mouse_forward_button_changed (gpointer callback_data);
static void use_extra_mouse_buttons_changed (gpointer callback_data);
static void nautilus_window_initialize_actions (NautilusWindow *window);
static GtkWidget *nautilus_window_ensure_location_entry (NautilusWindow *window);
static void nautilus_window_back_or_forward (NautilusWindow *window,
                                             gboolean        back,
                                             guint           distance);

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

    GtkWidget *notebook;

    /* available slots, and active slot.
     * Both of them may never be NULL.
     */
    GList *slots;
    NautilusWindowSlot *active_slot; /* weak reference */

    GtkWidget *content_flap;

    /* Side Pane */
    GtkWidget *places_sidebar;     /* the actual GtkPlacesSidebar */
    GVolume *selected_volume;     /* the selected volume in the sidebar popup callback */
    GFile *selected_file;     /* the selected file in the sidebar popup callback */

    /* Notifications */
    AdwToastOverlay *toast_overlay;

    /* Toolbar */
    GtkWidget *toolbar;
    gboolean temporary_navigation_bar;

    /* focus widget before the location bar has been shown temporarily */
    GtkWidget *last_focus_widget;

    /* Handle when exported */
    gchar *export_handle;

    guint sidebar_width_handler_id;
    gulong bookmarks_id;

    GtkWidget *tab_menu;

    GQueue *tab_data_queue;
};

enum
{
    SLOT_ADDED,
    SLOT_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NautilusWindow, nautilus_window, ADW_TYPE_APPLICATION_WINDOW);

static const GtkPadActionEntry pad_actions[] =
{
    { GTK_PAD_ACTION_BUTTON, 0, -1, N_("Parent folder"), "up" },
    { GTK_PAD_ACTION_BUTTON, 1, -1, N_("Home"), "go-home" },
    { GTK_PAD_ACTION_BUTTON, 2, -1, N_("New tab"), "new-tab" },
    { GTK_PAD_ACTION_BUTTON, 3, -1, N_("Close current view"), "close-current-view" },
    { GTK_PAD_ACTION_BUTTON, 4, -1, N_("Back"), "back" },
    { GTK_PAD_ACTION_BUTTON, 5, -1, N_("Forward"), "forward" },
};

static void
action_close_current_view (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusWindow *window;
    NautilusWindowSlot *slot;

    window = NAUTILUS_WINDOW (user_data);
    slot = nautilus_window_get_active_slot (window);

    nautilus_window_slot_close (window, slot);
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
    starred = g_file_new_for_uri ("starred:///");

    nautilus_window_open_location_full (window, starred, 0, NULL, NULL);
}

static void
action_reload (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (user_data));
    nautilus_window_slot_queue_reload (slot);
}

static void
action_stop (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusWindow *window;
    NautilusWindowSlot *slot;

    window = NAUTILUS_WINDOW (user_data);
    slot = nautilus_window_get_active_slot (window);

    nautilus_window_slot_stop_loading (slot);
}

static void
action_up (GSimpleAction *action,
           GVariant      *state,
           gpointer       user_data)
{
    NautilusWindowSlot *slot;
    GFile *parent, *location;

    slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (user_data));
    location = nautilus_window_slot_get_location (slot);

    if (location != NULL)
    {
        parent = g_file_get_parent (location);
        if (parent != NULL)
        {
            nautilus_window_open_location_full (NAUTILUS_WINDOW (user_data),
                                                parent,
                                                0,
                                                NULL, NULL);
        }

        g_clear_object (&parent);
    }
}

static void
action_back (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data), TRUE, 0);
}

static void
action_forward (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data), FALSE, 0);
}

static void
action_back_n (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data),
                                     TRUE,
                                     g_variant_get_uint32 (parameter));
}

static void
action_forward_n (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data),
                                     FALSE,
                                     g_variant_get_uint32 (parameter));
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
    NautilusWindow *window = user_data;

    nautilus_window_ensure_location_entry (window);
}

static void
action_tab_previous (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_notebook_prev_page (GTK_NOTEBOOK (window->notebook));
}

static void
action_tab_next (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_notebook_next_page (GTK_NOTEBOOK (window->notebook));
}

static void
action_tab_move_left (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_notebook_reorder_current_child_relative (GTK_NOTEBOOK (window->notebook), -1);
}

static void
action_tab_move_right (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_notebook_reorder_current_child_relative (GTK_NOTEBOOK (window->notebook), 1);
}

static void
action_go_to_tab (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    GtkNotebook *notebook;
    gint16 num;

    notebook = GTK_NOTEBOOK (window->notebook);

    num = g_variant_get_int32 (value);
    if (num < gtk_notebook_get_n_pages (notebook))
    {
        gtk_notebook_set_current_page (notebook, num);
    }
}

static void
action_prompt_for_location_root (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    NautilusWindow *window = user_data;
    GFile *location;
    GtkWidget *entry;

    location = g_file_new_for_path ("/");
    entry = nautilus_window_ensure_location_entry (window);
    nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (entry), location);

    g_object_unref (location);
}

static void
action_prompt_for_location_home (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    GtkWidget *entry;

    entry = nautilus_window_ensure_location_entry (NAUTILUS_WINDOW (user_data));
    nautilus_location_entry_set_special_text (NAUTILUS_LOCATION_ENTRY (entry),
                                              "~");
    gtk_editable_set_position (GTK_EDITABLE (entry), -1);
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
action_toggle_state_view_button (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    GVariant *current_state;

    current_state = g_action_get_state (G_ACTION (action));
    g_action_change_state (G_ACTION (action),
                           g_variant_new_boolean (!g_variant_get_boolean (current_state)));
    g_variant_unref (current_state);
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
notebook_switch_page_cb (GtkNotebook    *notebook,
                         GtkWidget      *page,
                         unsigned int    page_num,
                         NautilusWindow *window)
{
    NautilusWindowSlot *slot;
    GtkWidget *widget;

    widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), page_num);
    g_assert (widget != NULL);

    /* find slot corresponding to the target page */
    slot = NAUTILUS_WINDOW_SLOT (widget);
    g_assert (slot != NULL);

    nautilus_window_set_active_slot (nautilus_window_slot_get_window (slot),
                                     slot);
}

static void
connect_slot (NautilusWindow     *window,
              NautilusWindowSlot *slot)
{
    g_signal_connect (slot, "notify::location",
                      G_CALLBACK (on_slot_location_changed), window);
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

    slot = nautilus_window_slot_new (window);
    nautilus_window_initialize_slot (window, slot, flags);

    return slot;
}

void
nautilus_window_initialize_slot (NautilusWindow     *window,
                                 NautilusWindowSlot *slot,
                                 NautilusOpenFlags   flags)
{
    g_assert (NAUTILUS_IS_WINDOW (window));
    g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

    connect_slot (window, slot);

    g_signal_handlers_block_by_func (window->notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     window);
    nautilus_notebook_add_tab (GTK_NOTEBOOK (window->notebook),
                               slot,
                               -1,
                               FALSE);
    g_signal_handlers_unblock_by_func (window->notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       window);

    window->slots = g_list_append (window->slots, slot);
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);
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
     * oposite, since it's the most usual use case */
    if (!(flags & NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE))
    {
        gtk_window_present (GTK_WINDOW (window));
        nautilus_window_set_active_slot (window, target_slot);
    }

    nautilus_window_slot_open_location_full (target_slot, location, flags, selection);
}

static void
unset_focus_widget (NautilusWindow *window)
{
    if (window->last_focus_widget != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (window->last_focus_widget),
                                      (gpointer *) &window->last_focus_widget);
        window->last_focus_widget = NULL;
    }
}

static void
remember_focus_widget (NautilusWindow *window)
{
    GtkWidget *focus_widget;

    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
    if (focus_widget != NULL)
    {
        unset_focus_widget (window);

        window->last_focus_widget = focus_widget;
        g_object_add_weak_pointer (G_OBJECT (focus_widget),
                                   (gpointer *) &(window->last_focus_widget));
    }
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
restore_focus_widget (NautilusWindow *window)
{
    if (window->last_focus_widget != NULL)
    {
        gtk_widget_grab_focus (window->last_focus_widget);
        unset_focus_widget (window);
    }
}

static void
location_entry_cancel_callback (GtkWidget      *widget,
                                NautilusWindow *window)
{
    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->toolbar), FALSE);

    restore_focus_widget (window);
}

static void
location_entry_location_changed_callback (GtkWidget      *widget,
                                          GFile          *location,
                                          NautilusWindow *window)
{
    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->toolbar), FALSE);

    restore_focus_widget (window);

    nautilus_window_open_location_full (window, location, 0, NULL, NULL);
}

static void
close_slot (NautilusWindow     *window,
            NautilusWindowSlot *slot,
            gboolean            remove_from_notebook)
{
    int page_num;
    GtkNotebook *notebook;

    g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));


    DEBUG ("Closing slot %p", slot);

    disconnect_slot (window, slot);

    window->slots = g_list_remove (window->slots, slot);

    g_signal_emit (window, signals[SLOT_REMOVED], 0, slot);

    notebook = GTK_NOTEBOOK (window->notebook);

    if (remove_from_notebook)
    {
        page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (slot));
        g_assert (page_num >= 0);

        /* this will call gtk_widget_destroy on the slot */
        gtk_notebook_remove_page (notebook, page_num);
    }
}

void
nautilus_window_new_tab (NautilusWindow *window)
{
    NautilusWindowSlot *current_slot;
    GFile *location;
    g_autofree gchar *uri = NULL;

    current_slot = nautilus_window_get_active_slot (window);
    location = nautilus_window_slot_get_location (current_slot);

    if (location != NULL)
    {
        uri = g_file_get_uri (location);
        if (eel_uri_is_search (uri))
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
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (window);

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

void
nautilus_window_reset_menus (NautilusWindow *window)
{
    nautilus_window_sync_allow_stop (window, nautilus_window_get_active_slot (window));
}

void
nautilus_window_sync_allow_stop (NautilusWindow     *window,
                                 NautilusWindowSlot *slot)
{
    GAction *stop_action;
    GAction *reload_action;
    gboolean allow_stop, slot_is_active, slot_allow_stop;

    stop_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                              "stop");
    reload_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                "reload");
    allow_stop = g_action_get_enabled (stop_action);

    slot_allow_stop = nautilus_window_slot_get_allow_stop (slot);
    slot_is_active = (slot == nautilus_window_get_active_slot (window));


    if (!slot_is_active ||
        allow_stop != slot_allow_stop)
    {
        if (slot_is_active)
        {
            g_simple_action_set_enabled (G_SIMPLE_ACTION (stop_action), slot_allow_stop);
            g_simple_action_set_enabled (G_SIMPLE_ACTION (reload_action), !slot_allow_stop);
        }
        if (gtk_widget_get_realized (GTK_WIDGET (window)))
        {
            update_cursor (window);
        }

        /* Avoid updating the notebook if we are calling on dispose or
         * on removal of a notebook tab */
        if (nautilus_notebook_contains_slot (GTK_NOTEBOOK (window->notebook), slot))
        {
            nautilus_notebook_sync_loading (GTK_NOTEBOOK (window->notebook), slot);
        }
    }
}

GtkWidget *
nautilus_window_get_notebook (NautilusWindow *window)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

    return window->notebook;
}

/* Callback used when the places sidebar changes location; we need to change the displayed folder */
static void
open_location_cb (NautilusWindow             *window,
                  GFile                      *location,
                  NautilusGtkPlacesOpenFlags  open_flags)
{
    NautilusOpenFlags flags;
    NautilusApplication *application;

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
places_sidebar_show_other_locations_with_flags (NautilusWindow             *window,
                                                NautilusGtkPlacesOpenFlags  open_flags)
{
    GFile *location;

    location = g_file_new_for_uri ("other-locations:///");

    open_location_cb (window, location, open_flags);

    g_object_unref (location);
}

static void
places_sidebar_show_starred_location (NautilusWindow             *window,
                                      NautilusGtkPlacesOpenFlags  open_flags)
{
    GFile *location;

    location = g_file_new_for_uri ("starred:///");

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
build_uri_list_from_gfile_list (GList *file_list)
{
    GList *result;
    GList *l;

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
                                     GList                    *source_file_list,
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

    location = nautilus_file_get_location (data->file);

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

    revealed = adw_flap_get_reveal_flap (ADW_FLAP (window->content_flap));
    adw_flap_set_reveal_flap (ADW_FLAP (window->content_flap), !revealed);
}


static guint
get_window_xid (NautilusWindow *window)
{
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkSurface *gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));
        return (guint) gdk_x11_surface_get_xid (gdk_surface);
    }
#endif
    return 0;
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

    DEBUG ("Requesting to remove slot %p from window %p", slot, window);
    if (window == NULL || slot == NULL)
    {
        return;
    }

    data = nautilus_window_slot_get_navigation_state (slot);
    if (data != NULL)
    {
        g_queue_push_head (window->tab_data_queue, data);
    }

    close_slot (window, slot, TRUE);

    /* If that was the last slot in the window, close the window. */
    if (window->slots == NULL)
    {
        DEBUG ("Last slot removed, closing the window");
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

void
nautilus_window_sync_location_widgets (NautilusWindow *window)
{
    NautilusWindowSlot *slot;
    GFile *location;
    GAction *action;
    gboolean enabled;

    slot = window->active_slot;
    /* This function is called by the active slot. */
    g_assert (slot != NULL);

    location = nautilus_window_slot_get_location (slot);

    /* Change the location bar and path bar to match the current location. */
    if (location != NULL)
    {
        GtkWidget *location_entry;
        GtkWidget *path_bar;

        location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (window->toolbar));
        nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (location_entry), location);

        path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (window->toolbar));
        nautilus_path_bar_set_path (NAUTILUS_PATH_BAR (path_bar), location);
    }

    enabled = nautilus_window_slot_get_back_history (slot) != NULL;
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "back");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "back-n");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

    enabled = nautilus_window_slot_get_forward_history (slot) != NULL;
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "forward");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
    action = g_action_map_lookup_action (G_ACTION_MAP (window), "forward-n");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

    nautilus_window_sync_bookmarks (window);
}

static GtkWidget *
nautilus_window_ensure_location_entry (NautilusWindow *window)
{
    GtkWidget *location_entry;

    remember_focus_widget (window);

    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->toolbar), TRUE);

    location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (window->toolbar));
    gtk_widget_grab_focus (location_entry);

    return location_entry;
}

static gchar *
toast_undo_deleted_get_label (NautilusFileUndoInfo *undo_info)
{
    GList *files;
    gchar *file_label;
    gchar *label;
    gint length;

    files = nautilus_file_undo_info_trash_get_files (NAUTILUS_FILE_UNDO_INFO_TRASH (undo_info));
    length = g_list_length (files);
    if (length == 1)
    {
        file_label = g_file_get_basename (files->data);
        /* Translators: only one item has been deleted and %s is its name. */
        label = g_markup_printf_escaped (_("“%s” deleted"), file_label);
        g_free (file_label);
    }
    else
    {
        /* Translators: one or more items might have been deleted, and %d
         * is the count. */
        label = g_markup_printf_escaped (ngettext ("%d file deleted", "%d files deleted", length), length);
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
        g_autofree gchar *file_label = NULL;

        file_label = nautilus_file_get_display_name (files->data);
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
nautilus_window_on_undo_changed (NautilusFileUndoManager *manager,
                                 NautilusWindow          *window)
{
    NautilusFileUndoInfo *undo_info;
    NautilusFileUndoManagerState state;
    AdwToast *toast;

    undo_info = nautilus_file_undo_manager_get_action ();
    state = nautilus_file_undo_manager_get_state ();

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
            if (eel_uri_is_starred (g_file_get_uri (location)) &&
                gtk_window_is_active (GTK_WINDOW (window)) &&
                !nautilus_file_undo_info_starred_is_starred (NAUTILUS_FILE_UNDO_INFO_STARRED (undo_info)))
            {
                popup_toast = TRUE;
                label = toast_undo_unstar_get_label (undo_info);
            }
        }

        if (popup_toast)
        {
            toast = adw_toast_new (label);
            adw_toast_set_button_label (toast, _("Undo"));
            adw_toast_set_action_name (toast, "win.undo");
            adw_toast_set_priority (toast, ADW_TOAST_PRIORITY_HIGH);
            adw_toast_overlay_add_toast (window->toast_overlay, toast);
        }
    }
}

void
nautilus_window_show_operation_notification (NautilusWindow *window,
                                             gchar          *main_label,
                                             GFile          *folder_to_open)
{
    gchar *button_label;
    gchar *folder_name;
    NautilusFile *folder;
    GVariant *target;
    GFile *current_location;
    AdwToast *toast;

    if (window->active_slot == NULL)
    {
        return;
    }

    toast = adw_toast_new (main_label);
    adw_toast_set_priority (toast, ADW_TOAST_PRIORITY_HIGH);

    current_location = nautilus_window_slot_get_location (window->active_slot);
    if (gtk_window_is_active (GTK_WINDOW (window)))
    {
        if (!g_file_equal (folder_to_open, current_location))
        {
            target = g_variant_new_take_string (g_file_get_uri (folder_to_open));
            folder = nautilus_file_get (folder_to_open);
            folder_name = nautilus_file_get_display_name (folder);
            button_label = g_strdup_printf (_("Open %s"), folder_name);
            adw_toast_set_button_label (toast, button_label);
            adw_toast_set_action_name (toast, "win.open-location");
            adw_toast_set_action_target_value (toast, target);
            nautilus_file_unref (folder);
            g_free (folder_name);
            g_free (button_label);
        }

        adw_toast_overlay_add_toast (window->toast_overlay, toast);
    }
}

static void
on_path_bar_open_location (NautilusWindow    *window,
                           GFile             *location,
                           NautilusOpenFlags  open_flags)
{
    if (open_flags & NAUTILUS_OPEN_FLAG_NEW_WINDOW)
    {
        nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                 location, NAUTILUS_OPEN_FLAG_NEW_WINDOW, NULL, NULL, NULL);
    }
    else
    {
        nautilus_window_open_location_full (window, location, open_flags, NULL, NULL);
    }
}

static void
notebook_popup_menu_show (NautilusWindow *window,
                          GtkWidget      *tab)
{
    GtkPopover *popover = GTK_POPOVER (window->tab_menu);
    GtkAllocation allocation;
    gdouble x, y;

    gtk_widget_get_allocation (tab, &allocation);
    gtk_widget_translate_coordinates (tab, GTK_WIDGET (window),
                                      allocation.x, allocation.y, &x, &y);
    allocation.x = x;
    allocation.y = y;
    gtk_popover_set_pointing_to (popover, (GdkRectangle *) &allocation);
    gtk_popover_popup (popover);
}

static void
notebook_button_press_cb (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    NautilusWindow *window;
    GtkNotebook *notebook;
    gint tab_clicked;
    GtkWidget *tab_widget;
    guint button;
    GdkModifierType state;

    if (n_press != 1)
    {
        return;
    }

    window = NAUTILUS_WINDOW (user_data);
    notebook = GTK_NOTEBOOK (window->notebook);

    tab_widget = nautilus_notebook_get_tab_clicked (notebook, x, y, &tab_clicked);
    if (tab_widget == NULL)
    {
        return;
    }

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

    if (button == GDK_BUTTON_SECONDARY &&
        (state & gtk_accelerator_get_default_mod_mask ()) == 0)
    {
        /* switch to the page before opening the menu */
        gtk_notebook_set_current_page (notebook, tab_clicked);
        notebook_popup_menu_show (window, tab_widget);
    }
    else if (button == GDK_BUTTON_MIDDLE)
    {
        GtkWidget *slot;

        slot = gtk_notebook_get_nth_page (notebook, tab_clicked);
        nautilus_window_slot_close (window, NAUTILUS_WINDOW_SLOT (slot));
    }
}

GtkWidget *
nautilus_window_get_toolbar (NautilusWindow *window)
{
    g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

    return window->toolbar;
}

static void
setup_toolbar (NautilusWindow *window)
{
    GtkWidget *path_bar;
    GtkWidget *location_entry;


    g_object_set (window->toolbar, "window", window, NULL);

    /* connect to the pathbar signals */
    path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (window->toolbar));

    g_signal_connect_swapped (path_bar, "open-location",
                              G_CALLBACK (on_path_bar_open_location), window);

    /* connect to the location entry signals */
    location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (window->toolbar));

    g_signal_connect_object (location_entry, "location-changed",
                             G_CALLBACK (location_entry_location_changed_callback), window, 0);
    g_signal_connect_object (location_entry, "cancel",
                             G_CALLBACK (location_entry_cancel_callback), window, 0);
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
                          GtkWidget   *page,
                          guint        page_num,
                          gpointer     user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page);
    gboolean dnd_slot;

    dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
    if (!dnd_slot)
    {
        return;
    }

    close_slot (window, slot, FALSE);
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
                        GtkWidget   *page,
                        guint        page_num,
                        gpointer     user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page);
    NautilusWindowSlot *dummy_slot;
    gboolean dnd_slot;

    dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
    if (!dnd_slot)
    {
        return;
    }

    g_object_set_data (G_OBJECT (page), "dnd-window-slot",
                       GINT_TO_POINTER (FALSE));

    nautilus_window_slot_set_window (slot, window);
    window->slots = g_list_append (window->slots, slot);
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);

    nautilus_window_set_active_slot (window, slot);

    dummy_slot = g_list_nth_data (window->slots, 0);
    if (dummy_slot != NULL)
    {
        close_slot (window, dummy_slot, TRUE);
    }

    gtk_widget_show (GTK_WIDGET (window));
}

static GtkNotebook *
notebook_create_window_cb (GtkNotebook *notebook,
                           GtkWidget   *page,
                           gint         x,
                           gint         y,
                           gpointer     user_data)
{
    NautilusApplication *app;
    NautilusWindow *new_window;
    NautilusWindowSlot *slot;

    if (!NAUTILUS_IS_WINDOW_SLOT (page))
    {
        return NULL;
    }

    app = NAUTILUS_APPLICATION (g_application_get_default ());
    new_window = nautilus_application_create_window (app);
    gtk_window_set_display (GTK_WINDOW (new_window),
                            gtk_widget_get_display (GTK_WIDGET (notebook)));

    slot = NAUTILUS_WINDOW_SLOT (page);
    g_object_set_data (G_OBJECT (slot), "dnd-window-slot",
                       GINT_TO_POINTER (TRUE));

    return GTK_NOTEBOOK (new_window->notebook);
}

static void
setup_notebook (NautilusWindow *window)
{
    GtkEventController *controller;

    g_signal_connect (window->notebook, "switch-page",
                      G_CALLBACK (notebook_switch_page_cb),
                      window);
    g_signal_connect (window->notebook, "create-window",
                      G_CALLBACK (notebook_create_window_cb),
                      window);
    g_signal_connect (window->notebook, "page-added",
                      G_CALLBACK (notebook_page_added_cb),
                      window);
    g_signal_connect (window->notebook, "page-removed",
                      G_CALLBACK (notebook_page_removed_cb),
                      window);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (window->notebook), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (notebook_button_press_cb), window);
}

const GActionEntry win_entries[] =
{
    { "back", action_back },
    { "forward", action_forward },
    { "back-n", action_back_n, "u" },
    { "forward-n", action_forward_n, "u" },
    { "up", action_up },
    { "view-menu", action_toggle_state_view_button, NULL, "false", NULL },
    { "current-location-menu", action_show_current_location_menu },
    { "open-location", action_open_location, "s" },
    { "reload", action_reload },
    { "stop", action_stop },
    { "new-tab", action_new_tab },
    { "enter-location", action_enter_location },
    { "bookmark-current-location", action_bookmark_current_location },
    { "undo", action_undo },
    { "redo", action_redo },
    /* Only accesible by shorcuts */
    { "close-current-view", action_close_current_view },
    { "go-home", action_go_home },
    { "go-starred", action_go_starred },
    { "tab-previous", action_tab_previous },
    { "tab-next", action_tab_next },
    { "tab-move-left", action_tab_move_left },
    { "tab-move-right", action_tab_move_right },
    { "prompt-root-location", action_prompt_for_location_root },
    { "prompt-home-location", action_prompt_for_location_home },
    { "go-to-tab", NULL, "i", "0", action_go_to_tab },
    { "restore-tab", action_restore_tab },
    { "toggle-sidebar", action_toggle_sidebar },
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
    nautilus_application_set_accelerators (app, "win.back", ACCELS ("<alt>Left", "Back"));
    nautilus_application_set_accelerators (app, "win.forward", ACCELS ("<alt>Right", "Forward"));
    nautilus_application_set_accelerators (app, "win.enter-location", ACCELS ("<control>l", "Go", "OpenURL"));
    nautilus_application_set_accelerator (app, "win.new-tab", "<control>t");
    nautilus_application_set_accelerator (app, "win.close-current-view", "<control>w");

    /* Special case reload, since users are used to use two shortcuts instead of one */
    nautilus_application_set_accelerators (app, "win.reload", ACCELS ("F5", "<ctrl>r", "Refresh", "Reload"));
    nautilus_application_set_accelerator (app, "win.stop", "Stop");

    nautilus_application_set_accelerator (app, "win.undo", "<control>z");
    nautilus_application_set_accelerator (app, "win.redo", "<shift><control>z");
    /* Only accesible by shorcuts */
    nautilus_application_set_accelerators (app, "win.bookmark-current-location", ACCELS ("<control>d", "AddFavorite"));
    nautilus_application_set_accelerator (app, "win.up", "<alt>Up");
    nautilus_application_set_accelerators (app, "win.go-home", ACCELS ("<alt>Home", "HomePage", "Start"));
    nautilus_application_set_accelerator (app, "win.go-starred", "Favorites");
    nautilus_application_set_accelerator (app, "win.tab-previous", "<control>Page_Up");
    nautilus_application_set_accelerator (app, "win.tab-next", "<control>Page_Down");
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

#undef ACCELS

    action = g_action_map_lookup_action (G_ACTION_MAP (window), "toggle-sidebar");
    g_object_bind_property (window->content_flap, "folded",
                            action, "enabled", G_BINDING_SYNC_CREATE);
}


static void
nautilus_window_constructed (GObject *self)
{
    NautilusWindow *window;
    NautilusWindowSlot *slot;
    NautilusApplication *application;

    window = NAUTILUS_WINDOW (self);

    nautilus_profile_start (NULL);

    G_OBJECT_CLASS (nautilus_window_parent_class)->constructed (self);

    application = NAUTILUS_APPLICATION (g_application_get_default ());
    gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

    setup_toolbar (window);

    gtk_window_set_default_size (GTK_WINDOW (window),
                                 NAUTILUS_WINDOW_DEFAULT_WIDTH,
                                 NAUTILUS_WINDOW_DEFAULT_HEIGHT);

    setup_notebook (window);
    nautilus_window_set_up_sidebar (window);


    g_signal_connect_object (nautilus_file_undo_manager_get (), "undo-changed",
                             G_CALLBACK (nautilus_window_on_undo_changed), self,
                             G_CONNECT_AFTER);

    /* Is required that the UI is constructed before initializating the actions, since
     * some actions trigger UI widgets to show/hide. */
    nautilus_window_initialize_actions (window);

    slot = nautilus_window_create_and_init_slot (window, 0);
    nautilus_window_set_active_slot (window, slot);

    window->bookmarks_id =
        g_signal_connect_swapped (nautilus_application_get_bookmarks (application), "changed",
                                  G_CALLBACK (nautilus_window_sync_bookmarks), window);

    nautilus_toolbar_on_window_constructed (NAUTILUS_TOOLBAR (window->toolbar));

    nautilus_profile_end (NULL);
}

static gint
sort_slots_active_last (NautilusWindowSlot *a,
                        NautilusWindowSlot *b,
                        NautilusWindow     *window)
{
    if (window->active_slot == a)
    {
        return 1;
    }
    if (window->active_slot == b)
    {
        return -1;
    }
    return 0;
}

static void
destroy_slots_foreach (gpointer data,
                       gpointer user_data)
{
    NautilusWindowSlot *slot = data;
    NautilusWindow *window = user_data;

    close_slot (window, slot, TRUE);
}

static void
nautilus_window_dispose (GObject *object)
{
    NautilusWindow *window;
    GtkApplication *application;
    GList *slots_copy;

    window = NAUTILUS_WINDOW (object);
    application = gtk_window_get_application (GTK_WINDOW (window));

    DEBUG ("Destroying window");

    g_clear_pointer (&window->tab_menu, gtk_widget_unparent);

    /* close all slots safely */
    slots_copy = g_list_copy (window->slots);
    if (window->active_slot != NULL)
    {
        /* Make sure active slot is last one to be closed, to avoid default activation
         * of others slots when closing the active one, see bug #741952  */
        slots_copy = g_list_sort_with_data (slots_copy, (GCompareDataFunc) sort_slots_active_last, window);
    }
    g_list_foreach (slots_copy, (GFunc) destroy_slots_foreach, window);
    g_list_free (slots_copy);

    /* the slots list should now be empty */
    g_assert (window->slots == NULL);

    g_clear_weak_pointer (&window->active_slot);

    if (application != NULL)
    {
        g_clear_signal_handler (&window->bookmarks_id,
                                nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (application)));
    }

    nautilus_window_unexport_handle (window);

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

    gtk_window_destroy (GTK_WINDOW (window));
}

void
nautilus_window_set_active_slot (NautilusWindow     *window,
                                 NautilusWindowSlot *new_slot)
{
    NautilusWindowSlot *old_slot;

    g_assert (NAUTILUS_IS_WINDOW (window));

    if (new_slot)
    {
        g_assert ((window == nautilus_window_slot_get_window (new_slot)));
    }

    old_slot = nautilus_window_get_active_slot (window);

    if (old_slot == new_slot)
    {
        return;
    }

    DEBUG ("Setting new slot %p as active, old slot inactive %p", new_slot, old_slot);

    /* make old slot inactive if it exists (may be NULL after init, for example) */
    if (old_slot != NULL)
    {
        /* inform slot & view */
        nautilus_window_slot_set_active (old_slot, FALSE);
        nautilus_toolbar_set_window_slot (NAUTILUS_TOOLBAR (window->toolbar), NULL);
    }

    g_set_weak_pointer (&window->active_slot, new_slot);

    /* make new slot active, if it exists */
    if (new_slot)
    {
        /* inform slot & view */
        nautilus_window_slot_set_active (new_slot, TRUE);
        nautilus_toolbar_set_window_slot (NAUTILUS_TOOLBAR (window->toolbar), new_slot);

        on_location_changed (window);
    }
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

void
nautilus_window_sync_title (NautilusWindow     *window,
                            NautilusWindowSlot *slot)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW (window));
    g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

    if (slot == nautilus_window_get_active_slot (window))
    {
        gtk_window_set_title (GTK_WINDOW (window), nautilus_window_slot_get_title (slot));
    }

    nautilus_notebook_sync_tab_label (GTK_NOTEBOOK (window->notebook), slot);
}

#ifdef GDK_WINDOWING_WAYLAND
typedef struct
{
    NautilusWindow *window;
    NautilusWindowHandleExported callback;
    gpointer user_data;
} WaylandWindowHandleExportedData;

static void
wayland_window_handle_exported (GdkToplevel *toplevel,
                                const char  *wayland_handle_str,
                                gpointer     user_data)
{
    WaylandWindowHandleExportedData *data = user_data;

    data->window->export_handle = g_strdup_printf ("wayland:%s", wayland_handle_str);
    data->callback (data->window, data->window->export_handle, 0, data->user_data);
}
#endif

gboolean
nautilus_window_export_handle (NautilusWindow               *window,
                               NautilusWindowHandleExported  callback,
                               gpointer                      user_data)
{
    guint xid = get_window_xid (window);

    if (window->export_handle != NULL)
    {
        callback (window, window->export_handle, xid, user_data);
        return TRUE;
    }

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        window->export_handle = g_strdup_printf ("x11:%x", xid);
        callback (window, window->export_handle, xid, user_data);

        return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkSurface *gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));
        WaylandWindowHandleExportedData *data;

        data = g_new0 (WaylandWindowHandleExportedData, 1);
        data->window = window;
        data->callback = callback;
        data->user_data = user_data;

        if (!gdk_wayland_toplevel_export_handle (GDK_WAYLAND_TOPLEVEL (gdk_surface),
                                                 wayland_window_handle_exported,
                                                 data,
                                                 g_free))
        {
            g_free (data);
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
#endif

    g_warning ("Couldn't export handle, unsupported windowing system");

    return FALSE;
}

void
nautilus_window_unexport_handle (NautilusWindow *window)
{
    if (window->export_handle == NULL)
    {
        return;
    }

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        GdkSurface *gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));
        if (GDK_IS_WAYLAND_TOPLEVEL (gdk_surface))
        {
            gdk_wayland_toplevel_unexport_handle (GDK_WAYLAND_TOPLEVEL (gdk_surface));
        }
    }
#endif

    g_clear_pointer (&window->export_handle, g_free);
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
    new_slot = nautilus_window_slot_new (window);
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
        }
        break;

        case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
        {
            state->back_list = g_list_prepend (state->back_list, state->current_location_bookmark);
            state->current_location_bookmark = state->forward_list->data;
            state->forward_list = state->forward_list->next;
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

    new_back_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON);

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

    new_forward_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON);

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
    GtkPadController *pad_controller;
    GtkEventController *controller;

    g_type_ensure (NAUTILUS_TYPE_TOOLBAR);
    g_type_ensure (NAUTILUS_TYPE_GTK_PLACES_SIDEBAR);
    gtk_widget_init_template (GTK_WIDGET (window));
    nautilus_notebook_setup (GTK_NOTEBOOK (window->notebook));

    g_signal_connect_object (window->places_sidebar,
                             "show-other-locations-with-flags",
                             G_CALLBACK (places_sidebar_show_other_locations_with_flags),
                             window,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (window->places_sidebar,
                             "show-starred-location",
                             G_CALLBACK (places_sidebar_show_starred_location),
                             window,
                             G_CONNECT_SWAPPED);

    gtk_widget_set_parent (window->tab_menu, GTK_WIDGET (window));

    g_signal_connect (window, "notify::is-maximized",
                      G_CALLBACK (on_is_maximized_changed), NULL);

    window->slots = NULL;
    window->active_slot = NULL;

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                 "nautilus-window");

    window_group = gtk_window_group_new ();
    gtk_window_group_add_window (window_group, GTK_WINDOW (window));
    g_object_unref (window_group);

    window->tab_data_queue = g_queue_new ();

    pad_controller = gtk_pad_controller_new (G_ACTION_GROUP (window), NULL);
    gtk_pad_controller_set_action_entries (pad_controller,
                                           pad_actions, G_N_ELEMENTS (pad_actions));
    gtk_widget_add_controller (GTK_WIDGET (window),
                               GTK_EVENT_CONTROLLER (pad_controller));

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
nautilus_window_class_init (NautilusWindowClass *class)
{
    GObjectClass *oclass = G_OBJECT_CLASS (class);
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);
    GtkWindowClass *winclass = GTK_WINDOW_CLASS (class);

    oclass->dispose = nautilus_window_dispose;
    oclass->finalize = nautilus_window_finalize;
    oclass->constructed = nautilus_window_constructed;

    wclass->show = nautilus_window_show;
    wclass->realize = nautilus_window_realize;
    wclass->grab_focus = nautilus_window_grab_focus;

    winclass->close_request = nautilus_window_close_request;

    gtk_widget_class_set_template_from_resource (wclass,
                                                 "/org/gnome/nautilus/ui/nautilus-window.ui");
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, toolbar);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, content_flap);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, places_sidebar);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, notebook);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, tab_menu);
    gtk_widget_class_bind_template_child (wclass, NautilusWindow, toast_overlay);

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
        "The GNOME Project",
        NULL
    };
    const gchar *developers[] =
    {
        "The contributors to the Nautilus project",
        NULL
    };
    const gchar *documenters[] =
    {
        "GNOME Documentation Team",
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
                           "website", "https://wiki.gnome.org/action/show/Apps/Files",
                           "issue-url", "https://gitlab.gnome.org/GNOME/nautilus/-/issues/new",
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
