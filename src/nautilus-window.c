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

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-location-entry.h"
#include "nautilus-mime-actions.h"
#include "nautilus-notebook.h"
#include "nautilus-pathbar.h"
#include "nautilus-properties-window.h"
#include "nautilus-toolbar.h"
#include "nautilus-window-slot.h"
#include "nautilus-list-view.h"
#include "nautilus-other-locations-window-slot.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "nautilus-dnd.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-profile.h"
#include "nautilus-clipboard.h"
#include "nautilus-signaller.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include "nautilus-debug.h"

#include <math.h>
#include <sys/time.h>

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static int mouse_forward_button = 9;
static int mouse_back_button = 8;

static void mouse_back_button_changed (gpointer callback_data);
static void mouse_forward_button_changed (gpointer callback_data);
static void use_extra_mouse_buttons_changed (gpointer callback_data);
static void nautilus_window_initialize_actions (NautilusWindow *window);
static GtkWidget *nautilus_window_ensure_location_entry (NautilusWindow *window);
static void close_slot (NautilusWindow     *window,
                        NautilusWindowSlot *slot,
                        gboolean            remove_from_notebook);
static void free_restore_tab_data (gpointer data,
                                   gpointer user_data);

/* Sanity check: highest mouse button value I could find was 14. 5 is our
 * lower threshold (well-documented to be the one of the button events for the
 * scrollwheel), so it's hardcoded in the functions below. However, if you have
 * a button that registers higher and want to map it, file a bug and
 * we'll move the bar. Makes you wonder why the X guys don't have
 * defined values for these like the XKB stuff, huh?
 */
#define UPPER_MOUSE_LIMIT 14

#define NOTIFICATION_TIMEOUT 6 /*s */

typedef struct
{
    GtkWidget *notebook;

    /* available slots, and active slot.
     * Both of them may never be NULL.
     */
    GList *slots;
    NautilusWindowSlot *active_slot;

    GtkWidget *content_paned;

    /* Side Pane */
    int side_pane_width;
    GtkWidget *sidebar;            /* container for the GtkPlacesSidebar */
    GtkWidget *places_sidebar;     /* the actual GtkPlacesSidebar */
    GVolume *selected_volume;     /* the selected volume in the sidebar popup callback */
    GFile *selected_file;     /* the selected file in the sidebar popup callback */

    /* Main view */
    GtkWidget *main_view;

    /* Notifications */
    GtkWidget *notification_delete;
    GtkWidget *notification_delete_label;
    GtkWidget *notification_delete_close;
    GtkWidget *notification_delete_undo;
    guint notification_delete_timeout_id;
    GtkWidget *notification_operation;
    GtkWidget *notification_operation_label;
    GtkWidget *notification_operation_close;
    GtkWidget *notification_operation_open;
    guint notification_operation_timeout_id;
    GFile *folder_to_open;

    /* Toolbar */
    GtkWidget *toolbar;
    gboolean temporary_navigation_bar;

    /* focus widget before the location bar has been shown temporarily */
    GtkWidget *last_focus_widget;

    gboolean disable_chrome;

    guint sidebar_width_handler_id;
    guint bookmarks_id;

    GQueue *tab_data_queue;

    GtkPadController *pad_controller;
} NautilusWindowPrivate;

enum
{
    PROP_DISABLE_CHROME = 1,
    NUM_PROPERTIES,
};

enum
{
    SLOT_ADDED,
    SLOT_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (NautilusWindow, nautilus_window, GTK_TYPE_APPLICATION_WINDOW);

static const struct
{
    unsigned int keyval;
    const char *action;
} extra_window_keybindings [] =
{
    /* Window actions */
    { GDK_KEY_AddFavorite, "bookmark-current-location" },
    { GDK_KEY_Favorites, "bookmarks" },
    { GDK_KEY_Go, "enter-location" },
    { GDK_KEY_HomePage, "go-home" },
    { GDK_KEY_OpenURL, "enter-location" },
    { GDK_KEY_Refresh, "reload" },
    { GDK_KEY_Reload, "reload" },
    { GDK_KEY_Search, "search" },
    { GDK_KEY_Start, "go-home" },
    { GDK_KEY_Stop, "stop" },
    { GDK_KEY_Back, "back" },
    { GDK_KEY_Forward, "forward" },
};

static const GtkPadActionEntry pad_actions[] = {
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

    nautilus_window_open_location_full (window, home, nautilus_event_get_window_open_flags (), NULL, NULL);

    g_object_unref (home);
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
                                                nautilus_event_get_window_open_flags (),
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
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data),
                                     TRUE, 0, nautilus_event_get_window_open_flags ());
}

static void
action_forward (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data),
                                     FALSE, 0, nautilus_event_get_window_open_flags ());
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
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_prev_page (NAUTILUS_NOTEBOOK (priv->notebook));
}

static void
action_tab_next (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_next_page (NAUTILUS_NOTEBOOK (priv->notebook));
}

static void
action_tab_move_left (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (priv->notebook), -1);
}

static void
action_tab_move_right (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (priv->notebook), 1);
}

static void
action_go_to_tab (GSimpleAction *action,
                  GVariant      *value,
                  gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusWindowPrivate *priv;
    GtkNotebook *notebook;
    gint16 num;

    priv = nautilus_window_get_instance_private (window);
    notebook = GTK_NOTEBOOK (priv->notebook);

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

    nautilus_file_undo_manager_redo (GTK_WINDOW (window));
}

static void
action_undo (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusWindow *window = user_data;

    nautilus_file_undo_manager_undo (GTK_WINDOW (window));
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
on_location_changed (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (priv->places_sidebar),
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
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);
    widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), page_num);
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
nautilus_window_create_slot (NautilusWindow *window,
                             GFile          *location)
{
    return NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->create_slot (window, location);
}

static NautilusWindowSlot *
nautilus_window_create_and_init_slot (NautilusWindow          *window,
                                      GFile                   *location,
                                      NautilusWindowOpenFlags  flags)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_create_slot (window, location);
    nautilus_window_initialize_slot (window, slot, flags);

    return slot;
}

static NautilusWindowSlot *
real_create_slot (NautilusWindow *window,
                  GFile          *location)
{
    NautilusFile *file = NULL;
    NautilusWindowSlot *slot;

    if (location)
    {
        file = nautilus_file_get (location);
    }
    /* If not file, assume we open the home directory. We will switch eventually
     * to a different location if not.
     */
    if (file && nautilus_file_is_other_locations (file))
    {
        slot = NAUTILUS_WINDOW_SLOT (nautilus_other_locations_window_slot_new (window));
    }
    else
    {
        slot = nautilus_window_slot_new (window);
    }

    nautilus_file_unref (file);

    return slot;
}

static NautilusWindowSlot *
replace_active_slot (NautilusWindow          *window,
                     GFile                   *location,
                     NautilusWindowOpenFlags  flags)
{
    NautilusWindowSlot *new_slot;
    NautilusWindowSlot *active_slot;

    new_slot = nautilus_window_create_and_init_slot (window, location, flags);
    active_slot = nautilus_window_get_active_slot (window);
    if (active_slot)
    {
        close_slot (window, active_slot, TRUE);
    }

    return new_slot;
}

void
nautilus_window_initialize_slot (NautilusWindow          *window,
                                 NautilusWindowSlot      *slot,
                                 NautilusWindowOpenFlags  flags)
{
    NautilusWindowPrivate *priv;

    g_assert (NAUTILUS_IS_WINDOW (window));
    g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

    priv = nautilus_window_get_instance_private (window);

    connect_slot (window, slot);

    g_signal_handlers_block_by_func (priv->notebook,
                                     G_CALLBACK (notebook_switch_page_cb),
                                     window);
    nautilus_notebook_add_tab (NAUTILUS_NOTEBOOK (priv->notebook),
                               slot,
                               (flags & NAUTILUS_WINDOW_OPEN_SLOT_APPEND) != 0 ?
                               -1 :
                               gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) + 1,
                               FALSE);
    g_signal_handlers_unblock_by_func (priv->notebook,
                                       G_CALLBACK (notebook_switch_page_cb),
                                       window);

    priv->slots = g_list_append (priv->slots, slot);
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);
}

void
nautilus_window_open_location_full (NautilusWindow          *window,
                                    GFile                   *location,
                                    NautilusWindowOpenFlags  flags,
                                    GList                   *selection,
                                    NautilusWindowSlot      *target_slot)
{
    NautilusWindowSlot *active_slot;
    gboolean new_tab_at_end;

    /* The location owner can be one of the slots requesting to handle an
     * unhandled location. But this slot can be destroyed when switching to
     * a new slot. So keep the location alive.
     */
    g_object_ref (location);

    /* Assert that we are not managing new windows */
    g_assert (!(flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW));
    /* if the flags say we want a new tab, open a slot in the current window */
    if ((flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB) != 0)
    {
        new_tab_at_end = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_NEW_TAB_POSITION) == NAUTILUS_NEW_TAB_POSITION_END;
        if (new_tab_at_end)
        {
            flags |= NAUTILUS_WINDOW_OPEN_SLOT_APPEND;
        }
    }

    active_slot = nautilus_window_get_active_slot (window);
    if (!target_slot)
    {
        target_slot = active_slot;
    }

    if (target_slot == NULL || (flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB) != 0)
    {
        target_slot = nautilus_window_create_and_init_slot (window, location, flags);
    }
    else if (!nautilus_window_slot_handles_location (target_slot, location))
    {
        target_slot = replace_active_slot (window, location, flags);
    }

    /* Make the opened location the one active if we weren't ask for the
     * oposite, since it's the most usual use case */
    if (!(flags & NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE))
    {
        gtk_window_present (GTK_WINDOW (window));
        nautilus_window_set_active_slot (window, target_slot);
    }

    nautilus_window_slot_open_location_full (target_slot, location, flags, selection);

    g_object_unref (location);
}

static void
unset_focus_widget (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);
    if (priv->last_focus_widget != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (priv->last_focus_widget),
                                      (gpointer *) &priv->last_focus_widget);
        priv->last_focus_widget = NULL;
    }
}

static void
remember_focus_widget (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    GtkWidget *focus_widget;

    priv = nautilus_window_get_instance_private (window);
    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
    if (focus_widget != NULL)
    {
        unset_focus_widget (window);

        priv->last_focus_widget = focus_widget;
        g_object_add_weak_pointer (G_OBJECT (focus_widget),
                                   (gpointer *) &(priv->last_focus_widget));
    }
}

static void
nautilus_window_grab_focus (GtkWidget *widget)
{
    NautilusWindowSlot *slot;

    slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (widget));

    GTK_WIDGET_CLASS (nautilus_window_parent_class)->grab_focus (widget);

    if (slot)
    {
        gtk_widget_grab_focus (GTK_WIDGET (slot));
    }
}

static void
restore_focus_widget (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (priv->last_focus_widget != NULL)
    {
        gtk_widget_grab_focus (priv->last_focus_widget);
        unset_focus_widget (window);
    }
}

static void
location_entry_cancel_callback (GtkWidget      *widget,
                                NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (priv->toolbar), FALSE);

    restore_focus_widget (window);
}

static void
location_entry_location_changed_callback (GtkWidget      *widget,
                                          GFile          *location,
                                          NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (priv->toolbar), FALSE);

    restore_focus_widget (window);

    nautilus_window_open_location_full (window, location, 0, NULL, NULL);
}

static void
close_slot (NautilusWindow     *window,
            NautilusWindowSlot *slot,
            gboolean            remove_from_notebook)
{
    NautilusWindowPrivate *priv;
    int page_num;
    GtkNotebook *notebook;

    g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

    priv = nautilus_window_get_instance_private (window);

    DEBUG ("Closing slot %p", slot);

    disconnect_slot (window, slot);

    priv->slots = g_list_remove (priv->slots, slot);

    g_signal_emit (window, signals[SLOT_REMOVED], 0, slot);

    notebook = GTK_NOTEBOOK (priv->notebook);

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
    NautilusWindowOpenFlags flags;
    GFile *location;
    char *scheme;

    current_slot = nautilus_window_get_active_slot (window);
    location = nautilus_window_slot_get_location (current_slot);

    if (location != NULL)
    {
        flags = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_NEW_TAB_POSITION);

        scheme = g_file_get_uri_scheme (location);
        if (strcmp (scheme, "x-nautilus-search") == 0)
        {
            location = g_file_new_for_path (g_get_home_dir ());
        }
        else
        {
            g_object_ref (location);
        }

        g_free (scheme);

        flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
        nautilus_window_open_location_full (window, location, flags, NULL, NULL);
        g_object_unref (location);
    }
}

static void
update_cursor (NautilusWindow *window)
{
    NautilusWindowSlot *slot;
    GdkCursor *cursor;

    slot = nautilus_window_get_active_slot (window);

    if (slot != NULL &&
        nautilus_window_slot_get_allow_stop (slot))
    {
        GdkDisplay *display;

        display = gtk_widget_get_display (GTK_WIDGET (window));
        cursor = gdk_cursor_new_from_name (display, "progress");
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
        g_object_unref (cursor);
    }
    else
    {
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
    }
}

void
nautilus_window_hide_view_menu (NautilusWindow *window)
{
    GAction *menu_action;

    menu_action = g_action_map_lookup_action (G_ACTION_MAP (window), "view-menu");
    g_action_change_state (menu_action, g_variant_new_boolean (FALSE));
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
    NautilusWindowPrivate *priv;

    stop_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                              "stop");
    reload_action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                "reload");
    allow_stop = g_action_get_enabled (stop_action);

    slot_allow_stop = nautilus_window_slot_get_allow_stop (slot);
    slot_is_active = (slot == nautilus_window_get_active_slot (window));

    priv = nautilus_window_get_instance_private (window);

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
        if (nautilus_notebook_contains_slot (NAUTILUS_NOTEBOOK (priv->notebook), slot))
        {
            nautilus_notebook_sync_loading (NAUTILUS_NOTEBOOK (priv->notebook), slot);
        }
    }
}

GtkWidget *
nautilus_window_get_notebook (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    return priv->notebook;
}

static gboolean
save_sidebar_width_cb (gpointer user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    priv->sidebar_width_handler_id = 0;

    DEBUG ("Saving sidebar width: %d", priv->side_pane_width);

    g_settings_set_int (nautilus_window_state,
                        NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH,
                        priv->side_pane_width);

    return FALSE;
}

/* side pane helpers */
static void
side_pane_size_allocate_callback (GtkWidget     *widget,
                                  GtkAllocation *allocation,
                                  gpointer       user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (priv->sidebar_width_handler_id != 0)
    {
        g_source_remove (priv->sidebar_width_handler_id);
        priv->sidebar_width_handler_id = 0;
    }

    if (allocation->width != priv->side_pane_width &&
        allocation->width > 1)
    {
        priv->side_pane_width = allocation->width;

        priv->sidebar_width_handler_id =
            g_idle_add (save_sidebar_width_cb, window);
    }
}

static void
setup_side_pane_width (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    g_return_if_fail (priv->sidebar != NULL);

    priv->side_pane_width =
        g_settings_get_int (nautilus_window_state,
                            NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH);

    gtk_paned_set_position (GTK_PANED (priv->content_paned),
                            priv->side_pane_width);
}

/* Callback used when the places sidebar changes location; we need to change the displayed folder */
static void
open_location_cb (NautilusWindow     *window,
                  GFile              *location,
                  GtkPlacesOpenFlags  open_flags)
{
    NautilusWindowOpenFlags flags;

    switch (open_flags)
    {
        case GTK_PLACES_OPEN_NEW_TAB:
        {
            flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB |
                    NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE;
        }
        break;

        case GTK_PLACES_OPEN_NEW_WINDOW:
        {
            flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
        }
        break;

        case GTK_PLACES_OPEN_NORMAL: /* fall-through */
        default:
        {
            flags = 0;
        }
        break;
    }

    /* FIXME: We shouldn't need to provide the window, but seems gtk_application_get_active_window
     * is not working properly in GtkApplication, so we cannot rely on that...
     */
    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             location, flags, NULL, window, NULL);
}

static void
notify_unmount_done (GMountOperation *op,
                     const gchar     *message)
{
    NautilusApplication *application;
    gchar *notification_id;

    application = nautilus_application_get_default ();
    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_withdraw_notification (application, notification_id);

    if (message != NULL)
    {
        GNotification *unplug;
        GIcon *icon;
        gchar **strings;

        strings = g_strsplit (message, "\n", 0);
        icon = g_themed_icon_new ("media-removable-symbolic");
        unplug = g_notification_new (strings[0]);
        g_notification_set_body (unplug, strings[1]);
        g_notification_set_icon (unplug, icon);

        nautilus_application_send_notification (application, notification_id, unplug);
        g_object_unref (unplug);
        g_object_unref (icon);
        g_strfreev (strings);
    }

    g_free (notification_id);
}

static void
notify_unmount_show (GMountOperation *op,
                     const gchar     *message)
{
    NautilusApplication *application;
    GNotification *unmount;
    gchar *notification_id;
    GIcon *icon;
    gchar **strings;

    application = nautilus_application_get_default ();
    strings = g_strsplit (message, "\n", 0);
    icon = g_themed_icon_new ("media-removable");

    unmount = g_notification_new (strings[0]);
    g_notification_set_body (unmount, strings[1]);
    g_notification_set_icon (unmount, icon);
    g_notification_set_priority (unmount, G_NOTIFICATION_PRIORITY_URGENT);

    notification_id = g_strdup_printf ("nautilus-mount-operation-%p", op);
    nautilus_application_send_notification (application, notification_id, unmount);
    g_object_unref (unmount);
    g_object_unref (icon);
    g_strfreev (strings);
    g_free (notification_id);
}

static void
show_unmount_progress_cb (GMountOperation *op,
                          const gchar     *message,
                          gint64           time_left,
                          gint64           bytes_left,
                          gpointer         user_data)
{
    if (bytes_left == 0)
    {
        notify_unmount_done (op, message);
    }
    else
    {
        notify_unmount_show (op, message);
    }
}

static void
show_unmount_progress_aborted_cb (GMountOperation *op,
                                  gpointer         user_data)
{
    notify_unmount_done (op, NULL);
}

static void
places_sidebar_unmount_operation_cb (NautilusWindow  *window,
                                     GMountOperation *mount_operation)
{
    g_signal_connect (mount_operation, "show-unmount-progress",
                      G_CALLBACK (show_unmount_progress_cb), NULL);
    g_signal_connect (mount_operation, "aborted",
                      G_CALLBACK (show_unmount_progress_aborted_cb), NULL);
}

/* Callback used when the places sidebar needs us to present an error message */
static void
places_sidebar_show_error_message_cb (GtkPlacesSidebar *sidebar,
                                      const char       *primary,
                                      const char       *secondary,
                                      gpointer          user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);

    show_error_dialog (primary, secondary, GTK_WINDOW (window));
}

static void
places_sidebar_show_other_locations_with_flags (NautilusWindow     *window,
                                                GtkPlacesOpenFlags  open_flags)
{
    GFile *location;

    location = g_file_new_for_uri ("other-locations:///");

    open_location_cb (window, location, open_flags);

    g_object_unref (location);
}

static GList *
build_selection_list_from_gfile_list (GList *gfile_list)
{
    GList *result;
    GList *l;

    result = NULL;
    for (l = gfile_list; l; l = l->next)
    {
        GFile *file;
        NautilusDragSelectionItem *item;

        file = l->data;

        item = nautilus_drag_selection_item_new ();
        item->uri = g_file_get_uri (file);
        item->file = nautilus_file_get_existing (file);
        item->got_icon_position = FALSE;
        result = g_list_prepend (result, item);
    }

    return g_list_reverse (result);
}

void
nautilus_window_start_dnd (NautilusWindow *window,
                           GdkDragContext *context)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    gtk_places_sidebar_set_drop_targets_visible (GTK_PLACES_SIDEBAR (priv->places_sidebar),
                                                 TRUE,
                                                 context);
}

void
nautilus_window_end_dnd (NautilusWindow *window,
                         GdkDragContext *context)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    gtk_places_sidebar_set_drop_targets_visible (GTK_PLACES_SIDEBAR (priv->places_sidebar),
                                                 FALSE,
                                                 context);
}

/* Callback used when the places sidebar needs to know the drag action to suggest */
static GdkDragAction
places_sidebar_drag_action_requested_cb (GtkPlacesSidebar *sidebar,
                                         GdkDragContext   *context,
                                         GFile            *dest_file,
                                         GList            *source_file_list,
                                         gpointer          user_data)
{
    GList *items;
    char *uri;
    int action = 0;
    NautilusDragInfo *info;
    guint32 source_actions;

    info = nautilus_drag_get_source_data (context);
    if (info != NULL)
    {
        items = info->selection_cache;
        source_actions = info->source_actions;
    }
    else
    {
        items = build_selection_list_from_gfile_list (source_file_list);
        source_actions = 0;
    }
    uri = g_file_get_uri (dest_file);

    if (g_list_length (items) < 1)
    {
        goto out;
    }

    nautilus_drag_default_drop_action_for_icons (context, uri, items, source_actions, &action);

out:
    if (info == NULL)
    {
        nautilus_drag_destroy_selection_list (items);
    }

    g_free (uri);

    return action;
}

/* Callback used when the places sidebar needs us to pop up a menu with possible drag actions */
static GdkDragAction
places_sidebar_drag_action_ask_cb (GtkPlacesSidebar *sidebar,
                                   GdkDragAction     actions,
                                   gpointer          user_data)
{
    return nautilus_drag_drop_action_ask (GTK_WIDGET (sidebar), actions);
}

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
places_sidebar_drag_perform_drop_cb (GtkPlacesSidebar *sidebar,
                                     GFile            *dest_file,
                                     GList            *source_file_list,
                                     GdkDragAction     action,
                                     gpointer          user_data)
{
    char *dest_uri;
    GList *source_uri_list;

    dest_uri = g_file_get_uri (dest_file);
    source_uri_list = build_uri_list_from_gfile_list (source_file_list);

    nautilus_file_operations_copy_move (source_uri_list, NULL, dest_uri, action, GTK_WIDGET (sidebar), NULL, NULL);

    g_free (dest_uri);
    g_list_free_full (source_uri_list, g_free);
}

/* Callback used in the "empty trash" menu item from the places sidebar */
static void
action_empty_trash (GSimpleAction *action,
                    GVariant      *variant,
                    gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);

    nautilus_file_operations_empty_trash (GTK_WIDGET (window));
}

/* Callback used for the "properties" menu item from the places sidebar */
static void
action_properties (GSimpleAction *action,
                   GVariant      *variant,
                   gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusWindowPrivate *priv;
    GList *list;
    NautilusFile *file;

    priv = nautilus_window_get_instance_private (window);
    file = nautilus_file_get (priv->selected_file);

    list = g_list_append (NULL, file);
    nautilus_properties_window_present (list, GTK_WIDGET (window), NULL);
    nautilus_file_list_free (list);

    g_clear_object (&priv->selected_file);
}

static gboolean
check_have_gnome_disks (void)
{
    gchar *disks_path;
    gboolean res;

    disks_path = g_find_program_in_path ("gnome-disks");
    res = (disks_path != NULL);
    g_free (disks_path);

    return res;
}

static gboolean
should_show_format_command (GVolume *volume)
{
    gchar *unix_device_id;
    gboolean show_format;

    unix_device_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    show_format = (unix_device_id != NULL) && check_have_gnome_disks ();
    g_free (unix_device_id);

    return show_format;
}

static void
action_restore_tab (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    NautilusWindowPrivate *priv;
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusWindowOpenFlags flags;
    g_autoptr (GFile) location = NULL;
    NautilusWindowSlot *slot;
    RestoreTabData *data;

    priv = nautilus_window_get_instance_private (window);

    if (g_queue_get_length (priv->tab_data_queue) == 0)
    {
        return;
    }

    flags = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_NEW_TAB_POSITION);

    flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
    flags |= NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE;

    data = g_queue_pop_head (priv->tab_data_queue);

    location = nautilus_file_get_location (data->file);

    slot = nautilus_window_create_and_init_slot (window, location, flags);

    nautilus_window_slot_open_location_full (slot, location, flags, NULL);
    nautilus_window_slot_restore_from_data (slot, data);

    free_restore_tab_data (data, NULL);
}

static void
action_format (GSimpleAction *action,
               GVariant      *variant,
               gpointer       user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusWindowPrivate *priv;
    GAppInfo *app_info;
    gchar *cmdline, *device_identifier, *xid_string;
    gint xid;

    priv = nautilus_window_get_instance_private (window);
    device_identifier = g_volume_get_identifier (priv->selected_volume,
                                                 G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    xid = (gint) gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (window)));
    xid_string = g_strdup_printf ("%d", xid);

    cmdline = g_strconcat ("gnome-disks ",
                           "--block-device ", device_identifier, " ",
                           "--format-device ",
                           "--xid ", xid_string,
                           NULL);
    app_info = g_app_info_create_from_commandline (cmdline, NULL, 0, NULL);
    g_app_info_launch (app_info, NULL, NULL, NULL);

    g_free (cmdline);
    g_free (device_identifier);
    g_free (xid_string);
    g_clear_object (&app_info);
    g_clear_object (&priv->selected_volume);
}

static void
add_menu_separator (GtkWidget *menu)
{
    GtkWidget *separator;

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add (GTK_CONTAINER (menu), separator);
    gtk_widget_show (separator);
}

static void
places_sidebar_populate_popup_cb (GtkPlacesSidebar *sidebar,
                                  GtkWidget        *menu,
                                  GFile            *selected_file,
                                  GVolume          *selected_volume,
                                  gpointer          user_data)
{
    NautilusWindow *window = NAUTILUS_WINDOW (user_data);
    NautilusWindowPrivate *priv;
    GFile *trash;
    GtkWidget *menu_item;
    GAction *action;

    priv = nautilus_window_get_instance_private (window);

    g_clear_object (&priv->selected_file);
    g_clear_object (&priv->selected_volume);

    if (selected_file)
    {
        trash = g_file_new_for_uri ("trash:///");
        if (g_file_equal (trash, selected_file))
        {
            add_menu_separator (menu);

            menu_item = gtk_model_button_new ();
            gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                            "win.empty-trash");
            g_object_set (menu_item, "text", _("Empty _Trash"), NULL);
            gtk_container_add (GTK_CONTAINER (menu), menu_item);
            gtk_widget_show (menu_item);

            action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                 "empty-trash");
            g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                         !nautilus_trash_monitor_is_empty ());
        }
        g_object_unref (trash);

        if (g_file_is_native (selected_file))
        {
            priv->selected_file = g_object_ref (selected_file);
            add_menu_separator (menu);

            menu_item = gtk_model_button_new ();
            gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                            "win.properties");
            g_object_set (menu_item, "text", _("_Properties"), NULL);
            gtk_container_add (GTK_CONTAINER (menu), menu_item);
            gtk_widget_show (menu_item);
        }
    }
    if (selected_volume)
    {
        if (should_show_format_command (selected_volume))
        {
            menu_item = gtk_model_button_new ();
            gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                            "win.format");
            g_object_set (menu_item, "text", _("_Format…"), NULL);
            if (selected_volume != NULL && G_IS_VOLUME (selected_volume))
            {
                priv->selected_volume = g_object_ref (selected_volume);
            }
            gtk_container_add (GTK_CONTAINER (menu), menu_item);
            gtk_widget_show (menu_item);

            action = g_action_map_lookup_action (G_ACTION_MAP (window),
                                                 "format");
            g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                         selected_volume != NULL &&
                                         G_IS_VOLUME (selected_volume));
        }
    }
}

static void
nautilus_window_set_up_sidebar (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    setup_side_pane_width (window);
    g_signal_connect (priv->sidebar,
                      "size-allocate",
                      G_CALLBACK (side_pane_size_allocate_callback),
                      window);

    gtk_places_sidebar_set_open_flags (GTK_PLACES_SIDEBAR (priv->places_sidebar),
                                       (GTK_PLACES_OPEN_NORMAL
                                        | GTK_PLACES_OPEN_NEW_TAB
                                        | GTK_PLACES_OPEN_NEW_WINDOW));

    g_signal_connect_swapped (priv->places_sidebar, "open-location",
                              G_CALLBACK (open_location_cb), window);
    g_signal_connect (priv->places_sidebar, "show-error-message",
                      G_CALLBACK (places_sidebar_show_error_message_cb), window);
    g_signal_connect (priv->places_sidebar, "drag-action-requested",
                      G_CALLBACK (places_sidebar_drag_action_requested_cb), window);
    g_signal_connect (priv->places_sidebar, "drag-action-ask",
                      G_CALLBACK (places_sidebar_drag_action_ask_cb), window);
    g_signal_connect (priv->places_sidebar, "drag-perform-drop",
                      G_CALLBACK (places_sidebar_drag_perform_drop_cb), window);
    g_signal_connect (priv->places_sidebar, "populate-popup",
                      G_CALLBACK (places_sidebar_populate_popup_cb), window);
    g_signal_connect (priv->places_sidebar, "unmount",
                      G_CALLBACK (places_sidebar_unmount_operation_cb), window);
}

void
nautilus_window_hide_sidebar (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    DEBUG ("Called hide_sidebar()");

    priv = nautilus_window_get_instance_private (window);

    gtk_widget_hide (priv->sidebar);
}

void
nautilus_window_show_sidebar (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    DEBUG ("Called show_sidebar()");

    priv = nautilus_window_get_instance_private (window);

    if (priv->disable_chrome)
    {
        return;
    }

    gtk_widget_show (priv->sidebar);
    setup_side_pane_width (window);
}

static inline NautilusWindowSlot *
get_first_inactive_slot (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    GList *l;
    NautilusWindowSlot *slot;

    priv = nautilus_window_get_instance_private (window);

    for (l = priv->slots; l != NULL; l = l->next)
    {
        slot = NAUTILUS_WINDOW_SLOT (l->data);
        if (slot != priv->active_slot)
        {
            return slot;
        }
    }

    return NULL;
}

void
nautilus_window_slot_close (NautilusWindow     *window,
                            NautilusWindowSlot *slot)
{
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *next_slot;
    RestoreTabData *data;

    DEBUG ("Requesting to remove slot %p from window %p", slot, window);
    if (window == NULL)
    {
        return;
    }

    priv = nautilus_window_get_instance_private (window);

    if (priv->active_slot == slot)
    {
        next_slot = get_first_inactive_slot (window);
        nautilus_window_set_active_slot (window, next_slot);
    }

    data = nautilus_window_slot_get_restore_tab_data (slot);
    if (data != NULL)
    {
        g_queue_push_head (priv->tab_data_queue, data);
    }

    close_slot (window, slot, TRUE);

    /* If that was the last slot in the window, close the window. */
    if (priv->slots == NULL)
    {
        DEBUG ("Last slot removed, closing the window");
        nautilus_window_close (window);
    }
}

static void
nautilus_window_sync_bookmarks (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    gboolean can_bookmark = FALSE;
    NautilusWindowSlot *slot;
    NautilusBookmarkList *bookmarks;
    GAction *action;
    GFile *location;

    priv = nautilus_window_get_instance_private (window);
    slot = priv->active_slot;
    location = nautilus_window_slot_get_location (slot);

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
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot;
    GFile *location;
    GAction *action;
    gboolean enabled;

    priv = nautilus_window_get_instance_private (window);
    slot = priv->active_slot;
    location = nautilus_window_slot_get_location (slot);

    /* Change the location bar and path bar to match the current location. */
    if (location != NULL)
    {
        GtkWidget *location_entry;
        GtkWidget *path_bar;

        location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (priv->toolbar));
        nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (location_entry), location);

        path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (priv->toolbar));
        nautilus_path_bar_set_path (NAUTILUS_PATH_BAR (path_bar), location);
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (window), "back");
    enabled = nautilus_window_slot_get_back_history (slot) != NULL;
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

    action = g_action_map_lookup_action (G_ACTION_MAP (window), "forward");
    enabled = nautilus_window_slot_get_forward_history (slot) != NULL;
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

    nautilus_window_sync_bookmarks (window);
}

static GtkWidget *
nautilus_window_ensure_location_entry (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    GtkWidget *location_entry;

    priv = nautilus_window_get_instance_private (window);

    remember_focus_widget (window);

    nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (priv->toolbar), TRUE);

    location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (priv->toolbar));
    gtk_widget_grab_focus (location_entry);

    return location_entry;
}

static void
remove_notifications (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    GtkRevealerTransitionType transition_type;

    priv = nautilus_window_get_instance_private (window);
    /* Hide it inmediatily so we can animate the new notification. */
    transition_type = gtk_revealer_get_transition_type (GTK_REVEALER (priv->notification_delete));
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->notification_delete),
                                      GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_delete),
                                   FALSE);
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->notification_delete),
                                      transition_type);
    if (priv->notification_delete_timeout_id != 0)
    {
        g_source_remove (priv->notification_delete_timeout_id);
        priv->notification_delete_timeout_id = 0;
    }

    transition_type = gtk_revealer_get_transition_type (GTK_REVEALER (priv->notification_operation));
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->notification_operation),
                                      GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_operation),
                                   FALSE);
    gtk_revealer_set_transition_type (GTK_REVEALER (priv->notification_operation),
                                      transition_type);
    if (priv->notification_operation_timeout_id != 0)
    {
        g_source_remove (priv->notification_operation_timeout_id);
        priv->notification_operation_timeout_id = 0;
    }
}

static void
hide_notification_delete (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (priv->notification_delete_timeout_id != 0)
    {
        g_source_remove (priv->notification_delete_timeout_id);
        priv->notification_delete_timeout_id = 0;
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_delete), FALSE);
}

static void
nautilus_window_on_notification_delete_undo_clicked (GtkWidget      *notification,
                                                     NautilusWindow *window)
{
    hide_notification_delete (window);

    nautilus_file_undo_manager_undo (GTK_WINDOW (window));
}

static void
nautilus_window_on_notification_delete_close_clicked (GtkWidget      *notification,
                                                      NautilusWindow *window)
{
    hide_notification_delete (window);
}

static gboolean
nautilus_window_on_notification_delete_timeout (NautilusWindow *window)
{
    hide_notification_delete (window);

    return FALSE;
}

static char *
nautilus_window_notification_delete_get_label (NautilusFileUndoInfo *undo_info,
                                               GList                *files)
{
    gchar *file_label;
    gchar *label;
    gint length;

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

static void
nautilus_window_on_undo_changed (NautilusFileUndoManager *manager,
                                 NautilusWindow          *window)
{
    NautilusWindowPrivate *priv;
    NautilusFileUndoInfo *undo_info;
    NautilusFileUndoManagerState state;
    gchar *label;
    GList *files;

    priv = nautilus_window_get_instance_private (window);
    undo_info = nautilus_file_undo_manager_get_action ();
    state = nautilus_file_undo_manager_get_state ();

    if (undo_info != NULL &&
        state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO &&
        nautilus_file_undo_info_get_op_type (undo_info) == NAUTILUS_FILE_UNDO_OP_MOVE_TO_TRASH &&
        !priv->disable_chrome)
    {
        files = nautilus_file_undo_info_trash_get_files (NAUTILUS_FILE_UNDO_INFO_TRASH (undo_info));

        /* Don't pop up a notification if user canceled the operation or the focus
         * is not in the this window. This is an easy way to know from which window
         * was the delete operation made */
        if (g_list_length (files) > 0 && gtk_window_has_toplevel_focus (GTK_WINDOW (window)))
        {
            label = nautilus_window_notification_delete_get_label (undo_info, files);
            gtk_label_set_markup (GTK_LABEL (priv->notification_delete_label), label);
            gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_delete), TRUE);
            priv->notification_delete_timeout_id = g_timeout_add_seconds (NOTIFICATION_TIMEOUT,
                                                                                  (GSourceFunc) nautilus_window_on_notification_delete_timeout,
                                                                                  window);
            g_free (label);
        }
        g_list_free (files);
    }
    else
    {
        hide_notification_delete (window);
    }
}

static void
hide_notification_operation (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (priv->notification_operation_timeout_id != 0)
    {
        g_source_remove (priv->notification_operation_timeout_id);
        priv->notification_operation_timeout_id = 0;
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_operation), FALSE);
    g_clear_object (&priv->folder_to_open);
}

static void
on_notification_operation_open_clicked (GtkWidget      *notification,
                                        NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_window_open_location_full (window, priv->folder_to_open,
                                        0, NULL, NULL);
    hide_notification_operation (window);
}

static void
on_notification_operation_close_clicked (GtkWidget      *notification,
                                         NautilusWindow *window)
{
    hide_notification_operation (window);
}

static gboolean
on_notification_operation_timeout (NautilusWindow *window)
{
    hide_notification_operation (window);

    return FALSE;
}

void
nautilus_window_show_operation_notification (NautilusWindow *window,
                                             gchar          *main_label,
                                             GFile          *folder_to_open)
{
    NautilusWindowPrivate *priv;
    gchar *button_label;
    gchar *folder_name;
    NautilusFile *folder;
    GFile *current_location;

    priv = nautilus_window_get_instance_private (window);
    current_location = nautilus_window_slot_get_location (priv->active_slot);
    if (gtk_window_has_toplevel_focus (GTK_WINDOW (window)) &&
        !priv->disable_chrome)
    {
        remove_notifications (window);
        gtk_label_set_text (GTK_LABEL (priv->notification_operation_label),
                            main_label);

        if (g_file_equal (folder_to_open, current_location))
        {
            gtk_widget_hide (priv->notification_operation_open);
        }
        else
        {
            gtk_widget_show (priv->notification_operation_open);
            priv->folder_to_open = g_object_ref (folder_to_open);
            folder = nautilus_file_get (folder_to_open);
            folder_name = nautilus_file_get_display_name (folder);
            button_label = g_strdup_printf (_("Open %s"), folder_name);
            gtk_button_set_label (GTK_BUTTON (priv->notification_operation_open),
                                  button_label);
            nautilus_file_unref (folder);
            g_free (folder_name);
            g_free (button_label);
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->notification_operation), TRUE);
        priv->notification_operation_timeout_id = g_timeout_add_seconds (NOTIFICATION_TIMEOUT,
                                                                                 (GSourceFunc) on_notification_operation_timeout,
                                                                                 window);
    }
}

static void
path_bar_location_changed_callback (GtkWidget      *widget,
                                    GFile          *location,
                                    NautilusWindow *window)
{
    nautilus_window_open_location_full (window, location, 0, NULL, NULL);
}

static void
notebook_popup_menu_new_tab_cb (GtkMenuItem *menuitem,
                                gpointer     user_data)
{
    NautilusWindow *window = user_data;

    nautilus_window_new_tab (window);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
                                  gpointer     user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (priv->notebook), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
                                   gpointer     user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (priv->notebook), 1);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
                              gpointer     user_data)
{
    NautilusWindow *window = user_data;
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot;

    priv = nautilus_window_get_instance_private (window);
    slot = priv->active_slot;
    nautilus_window_slot_close (window, slot);
}

static void
notebook_popup_menu_show (NautilusWindow *window,
                          GdkEventButton *event)
{
    NautilusWindowPrivate *priv;
    GtkWidget *popup;
    GtkWidget *item;
    gboolean can_move_left, can_move_right;
    NautilusNotebook *notebook;

    priv = nautilus_window_get_instance_private (window);
    notebook = NAUTILUS_NOTEBOOK (priv->notebook);

    can_move_left = nautilus_notebook_can_reorder_current_child_relative (notebook, -1);
    can_move_right = nautilus_notebook_can_reorder_current_child_relative (notebook, 1);

    popup = gtk_menu_new ();

    item = gtk_menu_item_new_with_mnemonic (_("_New Tab"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_new_tab_cb),
                      window);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           gtk_separator_menu_item_new ());

    item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Left"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_left_cb),
                      window);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_left);

    item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Right"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_move_right_cb),
                      window);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);
    gtk_widget_set_sensitive (item, can_move_right);

    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           gtk_separator_menu_item_new ());

    item = gtk_menu_item_new_with_mnemonic (_("_Close Tab"));
    g_signal_connect (item, "activate",
                      G_CALLBACK (notebook_popup_menu_close_cb), window);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup),
                           item);

    gtk_widget_show_all (popup);

    gtk_menu_popup_at_pointer (GTK_MENU (popup),
                               (GdkEvent*) event);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (NautilusNotebook   *notebook,
                              NautilusWindowSlot *slot,
                              NautilusWindow     *window)
{
    nautilus_window_slot_close (window, slot);
}

static gboolean
notebook_button_press_cb (GtkWidget      *widget,
                          GdkEventButton *event,
                          gpointer        user_data)
{
    NautilusWindow *window = user_data;

    if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
    {
        notebook_popup_menu_show (window, event);
        return TRUE;
    }

    return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        gpointer   user_data)
{
    NautilusWindow *window = user_data;
    notebook_popup_menu_show (window, NULL);
    return TRUE;
}

GtkWidget *
nautilus_window_get_toolbar (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    return priv->toolbar;
}

static void
setup_toolbar (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;
    GtkWidget *path_bar;
    GtkWidget *location_entry;

    priv = nautilus_window_get_instance_private (window);

    g_object_set (priv->toolbar, "window", window, NULL);
    g_object_bind_property (window, "disable-chrome",
                            priv->toolbar, "visible",
                            G_BINDING_INVERT_BOOLEAN);

    /* connect to the pathbar signals */
    path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (priv->toolbar));

    g_signal_connect_object (path_bar, "path-clicked",
                             G_CALLBACK (path_bar_location_changed_callback), window, 0);
    g_signal_connect_swapped (path_bar, "open-location",
                              G_CALLBACK (open_location_cb), window);

    /* connect to the location entry signals */
    location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (priv->toolbar));

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
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page), *next_slot;
    gboolean dnd_slot;

    priv = nautilus_window_get_instance_private (window);
    dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
    if (!dnd_slot)
    {
        return;
    }

    if (priv->active_slot == slot)
    {
        next_slot = get_first_inactive_slot (window);
        nautilus_window_set_active_slot (window, next_slot);
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
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page);
    NautilusWindowSlot *dummy_slot;
    gboolean dnd_slot;

    priv = nautilus_window_get_instance_private (window);
    dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
    if (!dnd_slot)
    {
        return;
    }

    g_object_set_data (G_OBJECT (page), "dnd-window-slot",
                       GINT_TO_POINTER (FALSE));

    nautilus_window_slot_set_window (slot, window);
    priv->slots = g_list_append (priv->slots, slot);
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);

    nautilus_window_set_active_slot (window, slot);

    dummy_slot = g_list_nth_data (priv->slots, 0);
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
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot;

    if (!NAUTILUS_IS_WINDOW_SLOT (page))
    {
        return NULL;
    }

    app = NAUTILUS_APPLICATION (g_application_get_default ());
    new_window = nautilus_application_create_window
                     (app, gtk_widget_get_screen (GTK_WIDGET (notebook)));
    priv = nautilus_window_get_instance_private (new_window);

    slot = NAUTILUS_WINDOW_SLOT (page);
    g_object_set_data (G_OBJECT (slot), "dnd-window-slot",
                       GINT_TO_POINTER (TRUE));

    gtk_window_set_position (GTK_WINDOW (new_window), GTK_WIN_POS_MOUSE);

    return GTK_NOTEBOOK (priv->notebook);
}

static void
setup_notebook (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    g_signal_connect (priv->notebook, "tab-close-request",
                      G_CALLBACK (notebook_tab_close_requested),
                      window);
    g_signal_connect (priv->notebook, "popup-menu",
                      G_CALLBACK (notebook_popup_menu_cb),
                      window);
    g_signal_connect (priv->notebook, "switch-page",
                      G_CALLBACK (notebook_switch_page_cb),
                      window);
    g_signal_connect (priv->notebook, "create-window",
                      G_CALLBACK (notebook_create_window_cb),
                      window);
    g_signal_connect (priv->notebook, "page-added",
                      G_CALLBACK (notebook_page_added_cb),
                      window);
    g_signal_connect (priv->notebook, "page-removed",
                      G_CALLBACK (notebook_page_removed_cb),
                      window);
    g_signal_connect_after (priv->notebook, "button-press-event",
                            G_CALLBACK (notebook_button_press_cb),
                            window);
}

const GActionEntry win_entries[] =
{
    { "back", action_back },
    { "forward", action_forward },
    { "up", action_up },
    { "view-menu", action_toggle_state_view_button, NULL, "false", NULL },
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
    { "tab-previous", action_tab_previous },
    { "tab-next", action_tab_next },
    { "tab-move-left", action_tab_move_left },
    { "tab-move-right", action_tab_move_right },
    { "prompt-root-location", action_prompt_for_location_root },
    { "prompt-home-location", action_prompt_for_location_home },
    { "go-to-tab", NULL, "i", "0", action_go_to_tab },
    { "empty-trash", action_empty_trash },
    { "properties", action_properties },
    { "format", action_format },
    { "restore-tab", action_restore_tab },
};

static void
nautilus_window_initialize_actions (NautilusWindow *window)
{
    GApplication *app;
    GAction *action;
    GVariant *state;
    gchar detailed_action[80];
    gchar accel[80];
    gint i;
    const gchar *reload_accels[] =
    {
        "F5",
        "<ctrl>r",
        NULL
    };
    const gchar *prompt_home_location_accels[] =
    {
        "asciitilde",
        "dead_tilde",
        NULL
    };

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     win_entries, G_N_ELEMENTS (win_entries),
                                     window);

    app = g_application_get_default ();
    nautilus_application_set_accelerator (app, "win.back", "<alt>Left");
    nautilus_application_set_accelerator (app, "win.forward", "<alt>Right");
    nautilus_application_set_accelerator (app, "win.enter-location", "<control>l");
    nautilus_application_set_accelerator (app, "win.new-tab", "<control>t");
    nautilus_application_set_accelerator (app, "win.close-current-view", "<control>w");

    /* Special case reload, since users are used to use two shortcuts instead of one */
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.reload", reload_accels);

    nautilus_application_set_accelerator (app, "win.undo", "<control>z");
    nautilus_application_set_accelerator (app, "win.redo", "<shift><control>z");
    /* Only accesible by shorcuts */
    nautilus_application_set_accelerator (app, "win.bookmark-current-location", "<control>d");
    nautilus_application_set_accelerator (app, "win.up", "<alt>Up");
    nautilus_application_set_accelerator (app, "win.go-home", "<alt>Home");
    nautilus_application_set_accelerator (app, "win.tab-previous", "<control>Page_Up");
    nautilus_application_set_accelerator (app, "win.tab-next", "<control>Page_Down");
    nautilus_application_set_accelerator (app, "win.tab-move-left", "<shift><control>Page_Up");
    nautilus_application_set_accelerator (app, "win.tab-move-right", "<shift><control>Page_Down");
    nautilus_application_set_accelerator (app, "win.prompt-root-location", "slash");
    /* Support keyboard layouts which have a dead tilde key but not a tilde key. */
    nautilus_application_set_accelerators (app, "win.prompt-home-location", prompt_home_location_accels);
    nautilus_application_set_accelerator (app, "win.view-menu", "F10");
    nautilus_application_set_accelerator (app, "win.restore-tab", "<shift><control>t");

    /* Alt+N for the first 9 tabs */
    for (i = 0; i < 9; ++i)
    {
        g_snprintf (detailed_action, sizeof (detailed_action), "win.go-to-tab(%i)", i);
        g_snprintf (accel, sizeof (accel), "<alt>%i", i + 1);
        nautilus_application_set_accelerator (app, detailed_action, accel);
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (app), "show-hide-sidebar");
    state = g_action_get_state (action);
    if (g_variant_get_boolean (state))
    {
        nautilus_window_show_sidebar (window);
    }

    g_variant_unref (state);
}


static void
nautilus_window_constructed (GObject *self)
{
    NautilusWindow *window;
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *slot;
    NautilusApplication *application;

    window = NAUTILUS_WINDOW (self);
    priv = nautilus_window_get_instance_private (window);

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


    g_signal_connect_after (nautilus_file_undo_manager_get (), "undo-changed",
                            G_CALLBACK (nautilus_window_on_undo_changed), self);

    /* Is required that the UI is constructed before initializating the actions, since
     * some actions trigger UI widgets to show/hide. */
    nautilus_window_initialize_actions (window);

    slot = nautilus_window_create_and_init_slot (window, NULL, 0);
    nautilus_window_set_active_slot (window, slot);

    priv->bookmarks_id =
        g_signal_connect_swapped (nautilus_application_get_bookmarks (application), "changed",
                                  G_CALLBACK (nautilus_window_sync_bookmarks), window);

    nautilus_toolbar_on_window_constructed (NAUTILUS_TOOLBAR (priv->toolbar));

    nautilus_profile_end (NULL);
}

static void
nautilus_window_set_property (GObject      *object,
                              guint         arg_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    NautilusWindow *window;
    window = NAUTILUS_WINDOW (object);
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    switch (arg_id)
    {
        case PROP_DISABLE_CHROME:
        {
            priv->disable_chrome = g_value_get_boolean (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
        }
        break;
    }
}

static void
nautilus_window_get_property (GObject    *object,
                              guint       arg_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    NautilusWindow *window;
    window = NAUTILUS_WINDOW (object);
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    switch (arg_id)
    {
        case PROP_DISABLE_CHROME:
        {
            g_value_set_boolean (value, priv->disable_chrome);
        }
        break;
    }
}

static gint
sort_slots_active_last (NautilusWindowSlot *a,
                        NautilusWindowSlot *b,
                        NautilusWindow     *window)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (priv->active_slot == a)
    {
        return 1;
    }
    if (priv->active_slot == b)
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
nautilus_window_destroy (GtkWidget *object)
{
    NautilusWindow *window;
    NautilusWindowPrivate *priv;
    NautilusApplication *application;
    GList *slots_copy;

    window = NAUTILUS_WINDOW (object);
    priv = nautilus_window_get_instance_private (window);

    DEBUG ("Destroying window");

    /* close all slots safely */
    slots_copy = g_list_copy (priv->slots);
    if (priv->active_slot != NULL)
    {
        /* Make sure active slot is last one to be closed, to avoid default activation
         * of others slots when closing the active one, see bug #741952  */
        slots_copy = g_list_sort_with_data (slots_copy, (GCompareDataFunc) sort_slots_active_last, window);
    }
    g_list_foreach (slots_copy, (GFunc) destroy_slots_foreach, window);
    g_list_free (slots_copy);

    /* the slots list should now be empty */
    g_assert (priv->slots == NULL);

    priv->active_slot = NULL;

    if (priv->bookmarks_id != 0)
    {
        application = NAUTILUS_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
        g_signal_handler_disconnect (nautilus_application_get_bookmarks (application),
                                     priv->bookmarks_id);
        priv->bookmarks_id = 0;
    }

    GTK_WIDGET_CLASS (nautilus_window_parent_class)->destroy (object);
}

static void
free_restore_tab_data (gpointer data,
                       gpointer user_data)
{
    RestoreTabData *tab_data = data;

    g_list_free_full (tab_data->back_list, g_object_unref);
    g_list_free_full (tab_data->forward_list, g_object_unref);
    nautilus_file_unref (tab_data->file);

    g_free (tab_data);
}

static void
nautilus_window_finalize (GObject *object)
{
    NautilusWindow *window;
    NautilusWindowPrivate *priv;

    window = NAUTILUS_WINDOW (object);
    priv = nautilus_window_get_instance_private (window);

    if (priv->sidebar_width_handler_id != 0)
    {
        g_source_remove (priv->sidebar_width_handler_id);
        priv->sidebar_width_handler_id = 0;
    }

    if (priv->notification_delete_timeout_id != 0)
    {
        g_source_remove (priv->notification_delete_timeout_id);
        priv->notification_delete_timeout_id = 0;
    }

    if (priv->notification_operation_timeout_id != 0)
    {
        g_source_remove (priv->notification_operation_timeout_id);
        priv->notification_operation_timeout_id = 0;
    }

    g_clear_object (&priv->selected_file);
    g_clear_object (&priv->selected_volume);

    g_signal_handlers_disconnect_by_func (nautilus_file_undo_manager_get (),
                                          G_CALLBACK (nautilus_window_on_undo_changed),
                                          window);

    g_queue_foreach (priv->tab_data_queue, (GFunc) free_restore_tab_data, NULL);
    g_queue_free (priv->tab_data_queue);

    g_object_unref (priv->pad_controller);

    /* nautilus_window_close() should have run */
    g_assert (priv->slots == NULL);

    G_OBJECT_CLASS (nautilus_window_parent_class)->finalize (object);
}

static void
nautilus_window_save_geometry (NautilusWindow *window)
{
    char *geometry_string;
    gboolean is_maximized;

    g_assert (NAUTILUS_IS_WINDOW (window));

    if (gtk_widget_get_window (GTK_WIDGET (window)))
    {
        geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
        is_maximized = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)))
                       & GDK_WINDOW_STATE_MAXIMIZED;

        if (!is_maximized)
        {
            g_settings_set_string
                (nautilus_window_state, NAUTILUS_WINDOW_STATE_GEOMETRY,
                geometry_string);
        }
        g_free (geometry_string);

        g_settings_set_boolean
            (nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
            is_maximized);
    }
}

void
nautilus_window_close (NautilusWindow *window)
{
    NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->close (window);
}

void
nautilus_window_set_active_slot (NautilusWindow     *window,
                                 NautilusWindowSlot *new_slot)
{
    NautilusWindowPrivate *priv;
    NautilusWindowSlot *old_slot;

    g_assert (NAUTILUS_IS_WINDOW (window));

    priv = nautilus_window_get_instance_private (window);

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
    }

    priv->active_slot = new_slot;

    /* make new slot active, if it exists */
    if (new_slot)
    {
        nautilus_toolbar_set_active_slot (NAUTILUS_TOOLBAR (priv->toolbar), new_slot);

        /* inform slot & view */
        nautilus_window_slot_set_active (new_slot, TRUE);

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
nautilus_window_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
    NautilusWindow *window;
    NautilusWindowPrivate *priv;
    GtkWidget *focus_widget;
    int i;

    window = NAUTILUS_WINDOW (widget);
    priv = nautilus_window_get_instance_private (window);

    focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
    if (focus_widget != NULL && GTK_IS_EDITABLE (focus_widget))
    {
        /* if we have input focus on a GtkEditable (e.g. a GtkEntry), forward
         * the event to it before activating accelerator bindings too.
         */
        if (gtk_window_propagate_key_event (GTK_WINDOW (window), event))
        {
            return TRUE;
        }
    }

    for (i = 0; i < G_N_ELEMENTS (extra_window_keybindings); i++)
    {
        if (extra_window_keybindings[i].keyval == event->keyval)
        {
            GAction *action;

            action = g_action_map_lookup_action (G_ACTION_MAP (window), extra_window_keybindings[i].action);

            g_assert (action != NULL);
            if (g_action_get_enabled (action))
            {
                g_action_activate (action, NULL);
                return TRUE;
            }

            break;
        }
    }

    if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->key_press_event (widget, event))
    {
        return TRUE;
    }

    if (nautilus_window_slot_handle_event (priv->active_slot, event))
    {
        return TRUE;
    }

    return FALSE;
}

void
nautilus_window_sync_title (NautilusWindow     *window,
                            NautilusWindowSlot *slot)
{
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    if (NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title != NULL)
    {
        NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title (window, slot);

        return;
    }

    if (slot == nautilus_window_get_active_slot (window))
    {
        gtk_window_set_title (GTK_WINDOW (window), nautilus_window_slot_get_title (slot));
    }

    nautilus_notebook_sync_tab_label (NAUTILUS_NOTEBOOK (priv->notebook), slot);
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
    NautilusWindowPrivate *priv;

    g_assert (NAUTILUS_IS_WINDOW (window));

    priv = nautilus_window_get_instance_private (window);

    return priv->active_slot;
}

GList *
nautilus_window_get_slots (NautilusWindow *window)
{
    NautilusWindowPrivate *priv;

    g_assert (NAUTILUS_IS_WINDOW (window));

    priv = nautilus_window_get_instance_private (window);

    return priv->slots;
}

static gboolean
nautilus_window_state_event (GtkWidget           *widget,
                             GdkEventWindowState *event)
{
    if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
    {
        g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
                                event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
    }

    if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->window_state_event != NULL)
    {
        return GTK_WIDGET_CLASS (nautilus_window_parent_class)->window_state_event (widget, event);
    }

    return FALSE;
}

static gboolean
nautilus_window_delete_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
    nautilus_window_close (NAUTILUS_WINDOW (widget));
    return FALSE;
}

static gboolean
nautilus_window_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event)
{
    NautilusWindow *window;
    gboolean handled;

    window = NAUTILUS_WINDOW (widget);

    if (mouse_extra_buttons && (event->button == mouse_back_button))
    {
        nautilus_window_back_or_forward (window, TRUE, 0, 0);
        handled = TRUE;
    }
    else if (mouse_extra_buttons && (event->button == mouse_forward_button))
    {
        nautilus_window_back_or_forward (window, FALSE, 0, 0);
        handled = TRUE;
    }
    else if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->button_press_event)
    {
        handled = GTK_WIDGET_CLASS (nautilus_window_parent_class)->button_press_event (widget, event);
    }
    else
    {
        handled = FALSE;
    }
    return handled;
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
    NautilusWindowPrivate *priv;

    priv = nautilus_window_get_instance_private (window);

    g_type_ensure (NAUTILUS_TYPE_TOOLBAR);
    g_type_ensure (NAUTILUS_TYPE_NOTEBOOK);
    gtk_widget_init_template (GTK_WIDGET (window));

    g_signal_connect_object (priv->notification_delete_close, "clicked",
                             G_CALLBACK (nautilus_window_on_notification_delete_close_clicked), window, 0);
    g_signal_connect_object (priv->notification_delete_undo, "clicked",
                             G_CALLBACK (nautilus_window_on_notification_delete_undo_clicked), window, 0);

    priv->slots = NULL;
    priv->active_slot = NULL;

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                 "nautilus-window");

    window_group = gtk_window_group_new ();
    gtk_window_group_add_window (window_group, GTK_WINDOW (window));
    g_object_unref (window_group);

    priv->tab_data_queue = g_queue_new();

    priv->pad_controller = gtk_pad_controller_new (GTK_WINDOW (window),
                                                   G_ACTION_GROUP (window),
                                                   NULL);
    gtk_pad_controller_set_action_entries (priv->pad_controller,
                                           pad_actions, G_N_ELEMENTS (pad_actions));
}

static void
real_window_close (NautilusWindow *window)
{
    g_return_if_fail (NAUTILUS_IS_WINDOW (window));

    nautilus_window_save_geometry (window);
    nautilus_window_set_active_slot (window, NULL);

    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
    GObjectClass *oclass = G_OBJECT_CLASS (class);
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);

    oclass->finalize = nautilus_window_finalize;
    oclass->constructed = nautilus_window_constructed;
    oclass->get_property = nautilus_window_get_property;
    oclass->set_property = nautilus_window_set_property;

    wclass->destroy = nautilus_window_destroy;
    wclass->show = nautilus_window_show;
    wclass->realize = nautilus_window_realize;
    wclass->key_press_event = nautilus_window_key_press_event;
    wclass->window_state_event = nautilus_window_state_event;
    wclass->button_press_event = nautilus_window_button_press_event;
    wclass->delete_event = nautilus_window_delete_event;
    wclass->grab_focus = nautilus_window_grab_focus;

    class->close = real_window_close;
    class->create_slot = real_create_slot;

    gtk_widget_class_set_template_from_resource (wclass,
                                                 "/org/gnome/nautilus/ui/nautilus-window.ui");
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, toolbar);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, content_paned);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, sidebar);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, places_sidebar);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, main_view);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notebook);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_delete);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_delete_label);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_delete_undo);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_delete_close);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_operation);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_operation_label);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_operation_open);
    gtk_widget_class_bind_template_child_private (wclass, NautilusWindow, notification_operation_close);

    gtk_widget_class_bind_template_callback (wclass, places_sidebar_show_other_locations_with_flags);

    properties[PROP_DISABLE_CHROME] =
        g_param_spec_boolean ("disable-chrome",
                              "Disable chrome",
                              "Disable window chrome, for the desktop",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                              G_PARAM_STATIC_STRINGS);
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

    gtk_widget_class_bind_template_callback (wclass, on_notification_operation_open_clicked);
    gtk_widget_class_bind_template_callback (wclass, on_notification_operation_close_clicked);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

NautilusWindow *
nautilus_window_new (GdkScreen *screen)
{
    return g_object_new (NAUTILUS_TYPE_WINDOW,
                         "screen", screen,
                         NULL);
}

NautilusWindowOpenFlags
nautilus_event_get_window_open_flags (void)
{
    NautilusWindowOpenFlags flags = 0;
    GdkEvent *event;

    event = gtk_get_current_event ();

    if (event == NULL)
    {
        return flags;
    }

    if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
        (event->button.button == 2))
    {
        flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
    }

    gdk_event_free (event);

    return flags;
}

void
nautilus_window_show_about_dialog (NautilusWindow *window)
{
    const gchar *artists[] =
    {
        "The GNOME Project",
        NULL
    };
    const gchar *authors[] =
    {
        "Alexander Larsson",
        "Ali Abdin",
        "Anders Carlsson",
        "Andrew Walton",
        "Andy Hertzfeld",
        "Arlo Rose",
        "Christian Neumair",
        "Cosimo Cecchi",
        "Darin Adler",
        "David Camp",
        "Eli Goldberg",
        "Elliot Lee",
        "Eskil Heyn Olsen",
        "Ettore Perazzoli",
        "Gene Z. Ragan",
        "George Lebl",
        "Ian McKellar",
        "J Shane Culpepper",
        "James Willcox",
        "Jan Arne Petersen",
        "John Harper",
        "John Sullivan",
        "Josh Barrow",
        "Maciej Stachowiak",
        "Mark McLoughlin",
        "Mathieu Lacage",
        "Mike Engber",
        "Mike Fleming",
        "Pavel Cisler",
        "Ramiro Estrugo",
        "Raph Levien",
        "Rebecca Schulman",
        "Robey Pointer",
        "Robin * Slomkowski",
        "Seth Nickell",
        "Susan Kare",
        "Tomas Bzatek",
        "William Jon McCann",
        NULL
    };
    const gchar *documenters[] =
    {
        "GNOME Documentation Team",
        "Sun Microsystems",
        NULL
    };

    gtk_show_about_dialog (window ? GTK_WINDOW (window) : NULL,
                           "program-name", _("Files"),
                           "version", VERSION,
                           "comments", _("Access and organize your files."),
                           "copyright", "Copyright © 1999–2016 The Files Authors",
                           "license-type", GTK_LICENSE_GPL_3_0,
                           "artists", artists,
                           "authors", authors,
                           "documenters", documenters,
                           /* Translators should localize the following string
                            * which will be displayed at the bottom of the about
                            * box to give credit to the translator(s).
                            */
                           "translator-credits", _("translator-credits"),
                           "logo-icon-name", "org.gnome.Nautilus",
                           NULL);
}

void
nautilus_window_search (NautilusWindow *window,
                        const gchar    *text)
{
    NautilusWindowSlot *active_slot;

    active_slot = nautilus_window_get_active_slot (window);
    if (active_slot)
    {
        nautilus_window_slot_search (active_slot, text);
    }
    else
    {
        g_warning ("Trying search on a slot but no active slot present");
    }
}
