/* nautilus-files-view.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundation
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ettore Perazzoli,
 *          John Sullivan <sullivan@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          Pavel Cisler <pavel@eazel.com>,
 *          David Emory Watson <dwatson@cs.ucr.edu>
 */
#define G_LOG_DOMAIN "nautilus-view"

#include "nautilus-files-view.h"

#include <eel/eel-stock-dialogs.h>
#include <gdesktop-enums.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gnome-autoar/gnome-autoar.h>
#include <libportal/portal.h>
#include <libportal-gtk4/portal-gtk4.h>
#include <math.h>
#include <nautilus-extension.h>
#include <string.h>

#include "nautilus-application.h"
#include "nautilus-app-chooser.h"
#include "nautilus-batch-rename-dialog.h"
#include "nautilus-batch-rename-utilities.h"
#include "nautilus-clipboard.h"
#include "nautilus-compress-dialog.h"
#include "nautilus-dbus-launcher.h"
#include "nautilus-directory.h"
#include "nautilus-dnd.h"
#include "nautilus-enums.h"
#include "nautilus-error-reporting.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-private.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-filename-utilities.h"
#include "nautilus-floating-bar.h"
#include "nautilus-global-preferences.h"
#include "nautilus-grid-view.h"
#include "nautilus-icon-info.h"
#include "nautilus-icon-names.h"
#include "nautilus-list-base.h"
#include "nautilus-list-view.h"
#include "nautilus-metadata.h"
#include "nautilus-mime-actions.h"
#include "nautilus-network-view.h"
#include "nautilus-module.h"
#include "nautilus-new-folder-dialog.h"
#include "nautilus-previewer.h"
#include "nautilus-program-choosing.h"
#include "nautilus-properties-window.h"
#include "nautilus-recent-servers.h"
#include "nautilus-rename-file-popover.h"
#include "nautilus-scheme.h"
#include "nautilus-search-directory.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-trash-monitor.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-view.h"
#include "nautilus-view-model.h"
#include "nautilus-window.h"
#include "nautilus-tracker-utilities.h"

/* Minimum starting update inverval */
#define UPDATE_INTERVAL_MIN 100
/* Maximum update interval */
#define UPDATE_INTERVAL_MAX 2000
/* Amount of miliseconds the update interval is increased */
#define UPDATE_INTERVAL_INC 250
/* Interval at which the update interval is increased */
#define UPDATE_INTERVAL_TIMEOUT_INTERVAL 250
/* Milliseconds that have to pass without a change to reset the update interval */
#define UPDATE_INTERVAL_RESET 1000

#define SILENT_WINDOW_OPEN_LIMIT 5

#define DUPLICATE_HORIZONTAL_ICON_OFFSET 70
#define DUPLICATE_VERTICAL_ICON_OFFSET   30

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30

#define SHORTCUTS_PATH "/nautilus/scripts-accels"

/* Delay to show the Loading... floating bar */
#define FLOATING_BAR_LOADING_DELAY 200 /* ms */

/* Delay to clear search results (avoid while flasing while typing) */
#define SEARCH_TRANSITION_TIMEOUT 200 /* ms */

#define MIN_COMMON_FILENAME_PREFIX_LENGTH 4

enum
{
    ADD_FILES,
    BEGIN_FILE_CHANGES,
    BEGIN_LOADING,
    CLEAR,
    END_FILE_CHANGES,
    END_LOADING,
    FILE_CHANGED,
    MOVE_COPY_ITEMS,
    REMOVE_FILES,
    SELECTION_CHANGED,
    TRASH,
    DELETE,
    LAST_SIGNAL
};

enum
{
    PROP_WINDOW_SLOT = 1,
    PROP_ICON,
    PROP_SEARCHING,
    PROP_LOADING,
    PROP_SELECTION,
    PROP_LOCATION,
    PROP_SEARCH_QUERY,
    PROP_EXTENSIONS_BACKGROUND_MENU,
    PROP_TEMPLATES_MENU,
    NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];

static char *scripts_directory_uri = NULL;
static int scripts_directory_uri_length;

static GHashTable *script_accels = NULL;

typedef struct
{
    /* Main components */
    GtkWidget *overlay;
    NautilusListBase *list_base;

    NautilusWindowSlot *slot;
    NautilusDirectory *directory;
    NautilusFile *directory_as_file;
    GFile *location;
    guint dir_merge_id;

    NautilusViewModel *model;

    NautilusQuery *search_query;
    GFile *location_before_search;
    NautilusDirectory *outgoing_search;

    GtkWidget *rename_file_popover;

    GList *scripts_directory_list;
    GList *templates_directory_list;
    gboolean scripts_menu_updated;
    gboolean templates_menu_updated;

    guint display_selection_idle_id;
    guint update_context_menus_timeout_id;
    guint update_status_idle_id;

    guint search_transition_timeout_id;
    gboolean begin_loading_delayed;

    guint display_pending_source_id;
    guint changes_timeout_id;

    guint update_interval;
    guint64 last_queued;

    gulong files_added_handler_id;
    gulong files_changed_handler_id;
    gulong load_error_handler_id;
    gulong done_loading_handler_id;
    gulong file_changed_handler_id;

    /* Containers with FileAndDirectory* elements */
    GList *new_added_files;
    GList *new_changed_files;

    GList *pending_selection;
    GHashTable *pending_reveal;

    /* whether we are in the active slot */
    gboolean active;

    /* loading indicates whether this view has begun loading a directory.
     * This flag should need not be set inside subclasses. NautilusFilesView automatically
     * sets 'loading' to TRUE before it begins loading a directory's contents and to FALSE
     * after it finishes loading the directory and its view.
     */
    gboolean loading;

    gboolean in_destruction;

    gboolean show_hidden_files;

    gboolean selection_was_removed;

    gboolean metadata_for_directory_as_file_pending;
    gboolean metadata_for_files_in_directory_pending;

    GList *subdirectory_list;
    GList *subdirectories_loading;

    GMenu *selection_menu_model;
    GMenu *background_menu_model;

    GtkWidget *selection_menu;
    GtkWidget *background_menu;

    GActionGroup *view_action_group;

    /* Empty states */
    GtkWidget *empty_view_page;

    /* Floating bar */
    guint floating_bar_set_status_timeout_id;
    guint floating_bar_loading_timeout_id;
    guint floating_bar_set_passthrough_timeout_id;
    GtkWidget *floating_bar;

    /* Toolbar menu */
    NautilusToolbarMenuSections *toolbar_menu_sections;

    /* Exposed menus, for the path bar etc. */
    GMenuModel *extensions_background_menu;
    GMenuModel *templates_menu;

    /* Non exported menu, only for caching */
    GMenuModel *scripts_menu;

    GCancellable *clipboard_cancellable;

    GCancellable *starred_cancellable;
} NautilusFilesViewPrivate;

/**
 * FileAndDirectory:
 * @file: A #NautilusFile
 * @directory: A #NautilusDirectory where @file is present.
 *
 * The #FileAndDirectory struct is used to relate files to the directories they
 * are displayed in. This is necessary because the same file can appear multiple
 * times in the same view, by expanding folders as a tree in a list of search
 * results. (Adapted from commit 671e4bdaa4d07b039015bedfcb5d42026e5d099e)
 */
typedef struct
{
    NautilusFile *file;
    NautilusDirectory *directory;
} FileAndDirectory;

typedef struct
{
    NautilusFilesView *view;
    GList *selection;
} CompressCallbackData;

typedef struct
{
    GList *added_files;
    NautilusFilesView *directory_view;
} CopyMoveDoneData;

/* forward declarations */

static gboolean display_selection_info_idle_callback (gpointer data);
static void     trash_or_delete_files (GtkWindow         *parent_window,
                                       const GList       *files,
                                       NautilusFilesView *view);
static void     load_directory (NautilusFilesView *view,
                                NautilusDirectory *directory);
static void on_clipboard_owner_changed (GdkClipboard *clipboard,
                                        gpointer      user_data);
static void     schedule_update_context_menus (NautilusFilesView *view);
static void     remove_update_context_menus_timeout_callback (NautilusFilesView *view);
static void     schedule_update_status (NautilusFilesView *view);
static void     remove_update_status_idle_callback (NautilusFilesView *view);
static void     reset_update_interval (NautilusFilesView *view);
static void     schedule_idle_display_of_pending_files (NautilusFilesView *view);
static void     unschedule_display_of_pending_files (NautilusFilesView *view);
static void     disconnect_directory_handlers (NautilusFilesView *view);
static void     metadata_for_directory_as_file_ready_callback (NautilusFile *file,
                                                               gpointer      callback_data);
static void     metadata_for_files_in_directory_ready_callback (NautilusDirectory *directory,
                                                                GList             *files,
                                                                gpointer           callback_data);
static void     nautilus_files_view_trash_state_changed_callback (NautilusTrashMonitor *trash,
                                                                  gboolean              state,
                                                                  gpointer              callback_data);
static void     update_templates_directory (NautilusFilesView *view);

static void     extract_files (NautilusFilesView *view,
                               GList             *files,
                               GFile             *destination_directory);
static void     extract_files_to_chosen_location (NautilusFilesView *view,
                                                  GList             *files);

static void     nautilus_files_view_check_empty_states (NautilusFilesView *view);

static gboolean nautilus_files_view_is_searching (NautilusView *view);

static void     nautilus_files_view_iface_init (NautilusViewInterface *view);

static gboolean nautilus_files_view_is_read_only (NautilusFilesView *view);
static void nautilus_files_view_set_selection (NautilusView *nautilus_files_view,
                                               GList        *selection);
static void copy_move_done_callback (GHashTable *debuting_files,
                                     gboolean    success,
                                     gpointer    data);
static CopyMoveDoneData * pre_copy_move (NautilusFilesView *directory_view);
static void search_transition_emit_delayed_signals_if_pending (NautilusFilesView *view);

static void     nautilus_files_view_display_selection_info (NautilusFilesView *view);
static char *   nautilus_files_view_get_uri (NautilusFilesView *view);
static gboolean nautilus_files_view_should_show_file (NautilusFilesView *view,
                                                      NautilusFile      *file);
static void nautilus_files_view_pop_up_selection_context_menu (NautilusFilesView *view,
                                                               graphene_point_t  *point);
static void nautilus_files_view_pop_up_background_context_menu (NautilusFilesView *view,
                                                                graphene_point_t  *point);

G_DEFINE_TYPE_WITH_CODE (NautilusFilesView,
                         nautilus_files_view,
                         ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW, nautilus_files_view_iface_init)
                         G_ADD_PRIVATE (NautilusFilesView));

static inline NautilusViewItem *
get_view_item (GListModel *model,
               guint       position)
{
    g_autoptr (GtkTreeListRow) row = g_list_model_get_item (model, position);

    g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (row), NULL);
    return NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
}

/*
 * Floating Bar code
 */
static void
remove_loading_floating_bar (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->floating_bar_loading_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_loading_timeout_id);
        priv->floating_bar_loading_timeout_id = 0;
    }

    gtk_widget_set_visible (priv->floating_bar, FALSE);
    nautilus_floating_bar_set_show_stop (NAUTILUS_FLOATING_BAR (priv->floating_bar), FALSE);
}

static void
real_setup_loading_floating_bar (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (priv->floating_bar),
                                             nautilus_view_is_searching (NAUTILUS_VIEW (view)) ? _("Searching…") : _("Loading…"));
    nautilus_floating_bar_set_details_label (NAUTILUS_FLOATING_BAR (priv->floating_bar), NULL);
    nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (priv->floating_bar), priv->loading);
    nautilus_floating_bar_set_show_stop (NAUTILUS_FLOATING_BAR (priv->floating_bar), priv->loading);

    gtk_widget_set_halign (priv->floating_bar, GTK_ALIGN_END);
    gtk_widget_set_visible (priv->floating_bar, TRUE);
}

static gboolean
setup_loading_floating_bar_timeout_cb (gpointer user_data)
{
    NautilusFilesView *view = user_data;
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    priv->floating_bar_loading_timeout_id = 0;
    real_setup_loading_floating_bar (view);

    return FALSE;
}

static void
setup_loading_floating_bar (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    /* setup loading overlay */
    if (priv->floating_bar_set_status_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_set_status_timeout_id);
        priv->floating_bar_set_status_timeout_id = 0;
    }

    if (priv->floating_bar_loading_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_loading_timeout_id);
        priv->floating_bar_loading_timeout_id = 0;
    }

    priv->floating_bar_loading_timeout_id =
        g_timeout_add (FLOATING_BAR_LOADING_DELAY, setup_loading_floating_bar_timeout_cb, view);
}

static void
floating_bar_stop_cb (NautilusFloatingBar *floating_bar,
                      NautilusFilesView   *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    remove_loading_floating_bar (view);
    nautilus_window_slot_stop_loading (priv->slot);
}

static void
real_floating_bar_set_short_status (NautilusFilesView *view,
                                    const gchar       *primary_status,
                                    const gchar       *detail_status)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->loading)
    {
        return;
    }

    nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (priv->floating_bar),
                                            FALSE);
    nautilus_floating_bar_set_show_stop (NAUTILUS_FLOATING_BAR (priv->floating_bar),
                                         FALSE);

    if (primary_status == NULL && detail_status == NULL)
    {
        gtk_widget_set_visible (priv->floating_bar, FALSE);
        nautilus_floating_bar_remove_hover_timeout (NAUTILUS_FLOATING_BAR (priv->floating_bar));
        return;
    }

    nautilus_floating_bar_set_labels (NAUTILUS_FLOATING_BAR (priv->floating_bar),
                                      primary_status,
                                      detail_status);

    gtk_widget_set_visible (priv->floating_bar, TRUE);
}

typedef struct
{
    gchar *primary_status;
    gchar *detail_status;
    NautilusFilesView *view;
} FloatingBarSetStatusData;

static void
floating_bar_set_status_data_free (gpointer data)
{
    FloatingBarSetStatusData *status_data = data;

    g_free (status_data->primary_status);
    g_free (status_data->detail_status);

    g_slice_free (FloatingBarSetStatusData, data);
}

static gboolean
floating_bar_set_status_timeout_cb (gpointer data)
{
    NautilusFilesViewPrivate *priv;

    FloatingBarSetStatusData *status_data = data;

    priv = nautilus_files_view_get_instance_private (status_data->view);

    priv->floating_bar_set_status_timeout_id = 0;
    real_floating_bar_set_short_status (status_data->view,
                                        status_data->primary_status,
                                        status_data->detail_status);

    return FALSE;
}

static gboolean
remove_floating_bar_passthrough (gpointer data)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (data));
    gtk_widget_set_can_target (priv->floating_bar, TRUE);
    priv->floating_bar_set_passthrough_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
set_floating_bar_status (NautilusFilesView *view,
                         const gchar       *primary_status,
                         const gchar       *detail_status)
{
    GtkSettings *settings;
    gint double_click_time;
    FloatingBarSetStatusData *status_data;
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->floating_bar_set_status_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_set_status_timeout_id);
        priv->floating_bar_set_status_timeout_id = 0;
    }

    settings = gtk_settings_get_for_display (gtk_widget_get_display (GTK_WIDGET (view)));
    g_object_get (settings,
                  "gtk-double-click-time", &double_click_time,
                  NULL);

    status_data = g_slice_new0 (FloatingBarSetStatusData);
    status_data->primary_status = g_strdup (primary_status);
    status_data->detail_status = g_strdup (detail_status);
    status_data->view = view;

    if (priv->floating_bar_set_passthrough_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_set_passthrough_timeout_id);
        priv->floating_bar_set_passthrough_timeout_id = 0;
    }
    /* Activate passthrough on the floating bar just long enough for a
     * potential double click to happen, so to not interfere with it */
    gtk_widget_set_can_target (priv->floating_bar, FALSE);
    priv->floating_bar_set_passthrough_timeout_id = g_timeout_add ((guint) double_click_time,
                                                                   remove_floating_bar_passthrough,
                                                                   view);

    /* waiting for half of the double-click-time before setting
     * the status seems to be a good approximation of not setting it
     * too often and not delaying the statusbar too much.
     */
    priv->floating_bar_set_status_timeout_id =
        g_timeout_add_full (G_PRIORITY_DEFAULT,
                            (guint) (double_click_time / 2),
                            floating_bar_set_status_timeout_cb,
                            status_data,
                            floating_bar_set_status_data_free);
}

/**
 * escape_underscores:
 * @to_escape: input string
 *
 * This is used to preserve underscore characters in strings, when they would
 * otherwise be used for mnemonics.
 *
 * Returns: A copy of @to_escape, with underscore characters duplicated.
 * If @to_escape doesn't contain underscores, returns a copy of it.
 * If @to_escape is %NULL, returns an empty string.
 */
static char *
escape_underscores (const char *to_escape)
{
    GString *string = g_string_new (to_escape);

    g_string_replace (string, "_", "__", 0);

    return g_string_free_and_steal (string);
}

static void
on_sort_action_state_changed (GActionGroup *action_group,
                              gchar        *action_name,
                              GVariant     *value,
                              gpointer      user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    const gchar *target_name;
    gboolean reversed;

    g_variant_get (value, "(&sb)", &target_name, &reversed);

    nautilus_file_set_metadata (priv->directory_as_file,
                                NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                NULL,
                                target_name);
    nautilus_file_set_boolean_metadata (priv->directory_as_file,
                                        NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                        reversed);
}

static const char *
get_directory_sort_by (NautilusFile *file,
                       gboolean     *reversed)
{
    NautilusFileSortType default_sort = nautilus_file_get_default_sort_type (file, reversed);

    if (default_sort == NAUTILUS_FILE_SORT_BY_RECENCY ||
        default_sort == NAUTILUS_FILE_SORT_BY_TRASHED_TIME ||
        default_sort == NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE)
    {
        /* These defaults are important. Ignore metadata. */
        return nautilus_file_sort_type_get_attribute (default_sort);
    }

    *reversed = nautilus_file_get_boolean_metadata (file,
                                                    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                                    *reversed);

    return nautilus_file_get_metadata (file,
                                       NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                       nautilus_file_sort_type_get_attribute (default_sort));
}

static void
update_sort_order_from_metadata_and_preferences (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    gboolean reversed;
    const char *sort_attribute = get_directory_sort_by (priv->directory_as_file, &reversed);

    g_signal_handlers_block_by_func (priv->view_action_group, on_sort_action_state_changed, self);

    g_action_group_change_action_state (priv->view_action_group,
                                        "sort",
                                        g_variant_new ("(sb)",
                                                       sort_attribute,
                                                       reversed));

    g_signal_handlers_unblock_by_func (priv->view_action_group, on_sort_action_state_changed, self);
}

static void
real_begin_loading (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    update_sort_order_from_metadata_and_preferences (self);

    /* We could have changed to the trash directory or to searching, and then
     * we need to update the menus */
    nautilus_files_view_update_context_menus (self);
    nautilus_files_view_update_toolbar_menus (self);

    nautilus_list_base_setup_directory (priv->list_base, priv->directory);
}

static void
update_cut_status_callback (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    NautilusFilesView *self;
    NautilusFilesViewPrivate *priv;
    const GValue *value;
    GList *cut_files = NULL;
    g_autoptr (GError) error = NULL;

    value = gdk_clipboard_read_value_finish (GDK_CLIPBOARD (source_object), res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The files view has already been disposed, bailout */
        return;
    }

    self = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (self);

    if (value != NULL &&
        G_VALUE_HOLDS (value, NAUTILUS_TYPE_CLIPBOARD))
    {
        NautilusClipboard *clip = g_value_get_boxed (value);
        if (clip != NULL && nautilus_clipboard_is_cut (clip))
        {
            cut_files = nautilus_clipboard_peek_files (clip);
        }
    }

    nautilus_view_model_set_cut_files (priv->model, cut_files);
}

static void
update_cut_status (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
    GdkContentFormats *formats = gdk_clipboard_get_formats (clipboard);

    if (gdk_content_formats_contain_gtype (formats, NAUTILUS_TYPE_CLIPBOARD))
    {
        gdk_clipboard_read_value_async (clipboard, NAUTILUS_TYPE_CLIPBOARD,
                                        G_PRIORITY_DEFAULT,
                                        priv->clipboard_cancellable,
                                        update_cut_status_callback,
                                        self);
    }
    else
    {
        nautilus_view_model_set_cut_files (priv->model, NULL);
    }
}

static void
real_end_loading (NautilusFilesView *self,
                  gboolean           all_files_seen)
{
    update_cut_status (self);
}

/**
 *
 * nautilus_files_view_get_backing_uri:
 *
 * Returns the URI for the target location of new directory, new file, new
 * link and paste operations.
 */

char *
nautilus_files_view_get_backing_uri (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (NautilusViewItem) item = nautilus_list_base_get_backing_item (priv->list_base);

    if (item != NULL)
    {
        return nautilus_file_get_uri (nautilus_view_item_get_file (item));
    }
    else if (priv->directory != NULL)
    {
        return nautilus_directory_get_uri (priv->directory);
    }

    return NULL;
}

/**
 * nautilus_files_view_select_all:
 *
 * select all the items in the view
 *
 **/
static void
nautilus_files_view_select_all (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    gtk_selection_model_select_all (GTK_SELECTION_MODEL (priv->model));
}

static void
nautilus_files_view_select_first (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    if (g_list_model_get_n_items (G_LIST_MODEL (priv->model)) > 0)
    {
        nautilus_list_base_set_cursor (priv->list_base, 0, TRUE, TRUE);
    }
}

static void
nautilus_files_view_call_set_selection (NautilusFilesView *self,
                                        GList             *selection)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    g_autoptr (GList) files_to_find = g_list_copy (selection);
    g_autoptr (GtkBitset) update_set = NULL;
    g_autoptr (GtkBitset) new_selection_set = NULL;
    g_autoptr (GtkBitset) old_selection_set = NULL;
    guint n_items;

    old_selection_set = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    /* We aren't allowed to modify the actual selection bitset */
    update_set = gtk_bitset_copy (old_selection_set);
    new_selection_set = gtk_bitset_new_empty ();

    /* Convert file list into set of model indices */
    n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->model));
    for (guint position = 0; position < n_items; position++)
    {
        g_autoptr (NautilusViewItem) item = get_view_item (G_LIST_MODEL (priv->model), position);

        GList *link = g_list_find (files_to_find, nautilus_view_item_get_file (item));
        if (link != NULL)
        {
            /* Found item to select */
            gtk_bitset_add (new_selection_set, position);

            /* Remove found file from the list of files yet to find. */
            files_to_find = g_list_delete_link (files_to_find, link);
            if (files_to_find == NULL)
            {
                /* We've matched everything. */
                break;
            }
        }
    }

    /* Set focus on the first selected row, and scroll it into view. */
    if (!gtk_bitset_is_empty (new_selection_set))
    {
        guint first_position = gtk_bitset_get_nth (new_selection_set, 0);

        /* We pass TRUE for the third parameter (`select`) here, which appears
         * to be redundact, but is necessary in order to fix the bug reported in
         * https://gitlab.gnome.org/GNOME/nautilus/-/issues/2294 . See also
         * GTK ticket: https://gitlab.gnome.org/GNOME/gtk/-/issues/5485 */
        nautilus_list_base_set_cursor (priv->list_base, first_position, TRUE, TRUE);
    }

    gtk_bitset_union (update_set, new_selection_set);
    gtk_selection_model_set_selection (GTK_SELECTION_MODEL (priv->model),
                                       new_selection_set,
                                       update_set);
}

static gboolean
is_ancestor_selected (GtkTreeListRow *row,
                      GtkBitset      *selection)
{
    g_autoptr (GtkTreeListRow) parent = gtk_tree_list_row_get_parent (row);
    GtkTreeListRow *grandparent;

    /* Walk up the tree looking for a selected ancestor. */
    while (parent != NULL)
    {
        guint parent_position = gtk_tree_list_row_get_position (parent);
        if (gtk_bitset_contains (selection, parent_position))
        {
            return TRUE;
        }

        grandparent = gtk_tree_list_row_get_parent (parent);
        g_object_unref (parent);
        parent = grandparent;
    }

    return FALSE;
}

static GList *
get_selection_internal (NautilusFilesView *self,
                        gboolean           for_file_transfer)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    g_autoptr (GtkBitset) selection = NULL;
    GtkBitsetIter iter;
    guint i;
    GList *selected_files = NULL;

    selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));

    for (gtk_bitset_iter_init_last (&iter, selection, &i);
         gtk_bitset_iter_is_valid (&iter);
         gtk_bitset_iter_previous (&iter, &i))
    {
        g_autoptr (GtkTreeListRow) row = NULL;
        g_autoptr (NautilusViewItem) item = NULL;
        NautilusFile *file;

        row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (priv->model), i));

        if (for_file_transfer && is_ancestor_selected (row, selection))
        {
            /* If an ancestor is already selected, don't include its descendants
             * in the selection of files to copy/move. */
            continue;
        }

        item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
        file = nautilus_view_item_get_file (item);

        selected_files = g_list_prepend (selected_files, g_object_ref (file));
    }

    return selected_files;
}

/* The difference from get_selection() is that any files in the selection that
 * also has a parent folder in the selection is not included */
static GList *
nautilus_files_view_get_selection_for_file_transfer (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    return get_selection_internal (NAUTILUS_FILES_VIEW (view), TRUE);
}

static void
nautilus_files_view_invert_selection (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (priv->model);
    g_autoptr (GtkBitset) selected = NULL;
    g_autoptr (GtkBitset) all = NULL;
    g_autoptr (GtkBitset) new_selected = NULL;

    selected = gtk_selection_model_get_selection (selection_model);

    /* We are going to flip the selection state of every item in the model. */
    all = gtk_bitset_new_range (0, g_list_model_get_n_items (G_LIST_MODEL (priv->model)));

    /* The new selection is all items minus the ones currently selected. */
    new_selected = gtk_bitset_copy (all);
    gtk_bitset_subtract (new_selected, selected);

    gtk_selection_model_set_selection (selection_model, new_selected, all);
}

/**
 * nautilus_files_view_get_toolbar_menu_sections:
 * @view: a #NautilusFilesView
 *
 * Retrieves the menu sections that should be added to the toolbar menu when
 * this view is active
 *
 * Returns: (transfer none): a #NautilusToolbarMenuSections with the details of
 * which menu sections should be added to the menu
 */
static NautilusToolbarMenuSections *
nautilus_files_view_get_toolbar_menu_sections (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    if (NAUTILUS_IS_NETWORK_VIEW (priv->list_base))
    {
        return NULL;
    }

    return priv->toolbar_menu_sections;
}

static GMenuModel *
nautilus_files_view_get_templates_menu (NautilusView *self)
{
    GMenuModel *menu;

    g_object_get (self, "templates-menu", &menu, NULL);

    return menu;
}

static GMenuModel *
nautilus_files_view_get_extensions_background_menu (NautilusView *self)
{
    GMenuModel *menu;

    g_object_get (self, "extensions-background-menu", &menu, NULL);

    return menu;
}

static GMenuModel *
real_get_extensions_background_menu (NautilusView *view)
{
    NautilusFilesViewPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    return priv->extensions_background_menu;
}

static GMenuModel *
real_get_templates_menu (NautilusView *view)
{
    NautilusFilesViewPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    return priv->templates_menu;
}

static void
nautilus_files_view_set_templates_menu (NautilusView *self,
                                        GMenuModel   *menu)
{
    g_object_set (self, "templates-menu", menu, NULL);
}

static void
nautilus_files_view_set_extensions_background_menu (NautilusView *self,
                                                    GMenuModel   *menu)
{
    g_object_set (self, "extensions-background-menu", menu, NULL);
}

static void
real_set_extensions_background_menu (NautilusView *view,
                                     GMenuModel   *menu)
{
    NautilusFilesViewPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    g_set_object (&priv->extensions_background_menu, menu);
}

static void
real_set_templates_menu (NautilusView *view,
                         GMenuModel   *menu)
{
    NautilusFilesViewPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    g_set_object (&priv->templates_menu, menu);
}

static gboolean
showing_trash_directory (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    return priv->directory_as_file != NULL &&
           nautilus_file_is_in_trash (priv->directory_as_file);
}

static gboolean
showing_recent_directory (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    return priv->directory_as_file != NULL &&
           nautilus_file_is_in_recent (priv->directory_as_file);
}

static gboolean
showing_starred_directory (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    return priv->directory_as_file != NULL &&
           nautilus_file_is_in_starred (priv->directory_as_file);
}

static gboolean
nautilus_files_view_supports_creating_files (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

    return !nautilus_files_view_is_read_only (view)
           && !showing_trash_directory (view)
           && !showing_recent_directory (view)
           && !showing_starred_directory (view);
}

static gboolean
nautilus_files_view_supports_extract_here (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

    return nautilus_files_view_supports_creating_files (view)
           && !nautilus_view_is_searching (NAUTILUS_VIEW (view));
}

static gboolean
nautilus_files_view_is_empty (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    return g_list_model_get_n_items (G_LIST_MODEL (priv->model)) == 0;
}

gboolean
nautilus_files_view_is_searching (NautilusView *view)
{
    NautilusFilesView *files_view;
    NautilusFilesViewPrivate *priv;

    files_view = NAUTILUS_FILES_VIEW (view);
    priv = nautilus_files_view_get_instance_private (files_view);

    if (!priv->directory)
    {
        return FALSE;
    }

    return NAUTILUS_IS_SEARCH_DIRECTORY (priv->directory);
}

static guint
nautilus_files_view_get_view_id (NautilusView *view)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (view);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    return nautilus_list_base_get_view_info (priv->list_base).view_id;
}

static GList *
nautilus_files_view_get_selection (NautilusView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    return get_selection_internal (NAUTILUS_FILES_VIEW (view), FALSE);
}

typedef struct
{
    NautilusFile *file;
    NautilusFilesView *directory_view;
} ScriptLaunchParameters;

typedef struct
{
    NautilusFile *file;
    NautilusFilesView *directory_view;
} CreateTemplateParameters;

static FileAndDirectory *
file_and_directory_new (NautilusFile      *file,
                        NautilusDirectory *directory)
{
    FileAndDirectory *fad;

    fad = g_new0 (FileAndDirectory, 1);
    fad->directory = nautilus_directory_ref (directory);
    fad->file = nautilus_file_ref (file);

    return fad;
}

static NautilusFile *
file_and_directory_get_file (FileAndDirectory *fad)
{
    g_return_val_if_fail (fad != NULL, NULL);

    return nautilus_file_ref (fad->file);
}

static void
file_and_directory_free (gpointer data)
{
    FileAndDirectory *fad = data;

    nautilus_directory_unref (fad->directory);
    nautilus_file_unref (fad->file);
    g_free (fad);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FileAndDirectory, file_and_directory_free)

static ScriptLaunchParameters *
script_launch_parameters_new (NautilusFile      *file,
                              NautilusFilesView *directory_view)
{
    ScriptLaunchParameters *result;

    result = g_new0 (ScriptLaunchParameters, 1);
    result->directory_view = directory_view;
    nautilus_file_ref (file);
    result->file = file;

    return result;
}

static void
script_launch_parameters_free (ScriptLaunchParameters *parameters)
{
    nautilus_file_unref (parameters->file);
    g_free (parameters);
}

static CreateTemplateParameters *
create_template_parameters_new (NautilusFile      *file,
                                NautilusFilesView *directory_view)
{
    CreateTemplateParameters *result;

    result = g_new0 (CreateTemplateParameters, 1);
    result->directory_view = directory_view;
    nautilus_file_ref (file);
    result->file = file;

    return result;
}

static void
create_templates_parameters_free (CreateTemplateParameters *parameters)
{
    nautilus_file_unref (parameters->file);
    g_free (parameters);
}

static NautilusWindow *
nautilus_files_view_get_window (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    return nautilus_window_slot_get_window (priv->slot);
}

/* Returns the GtkWindow that this directory view occupies, or NULL
 * if at the moment this directory view is not in a GtkWindow or the
 * GtkWindow cannot be determined. Primarily used for parenting dialogs.
 */
static GtkWindow *
nautilus_files_view_get_containing_window (NautilusFilesView *view)
{
    GtkWidget *window;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
    if (window == NULL)
    {
        return NULL;
    }

    return GTK_WINDOW (window);
}

static char *
get_view_directory (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    char *uri, *path;
    GFile *f;

    priv = nautilus_files_view_get_instance_private (view);

    uri = nautilus_directory_get_uri (priv->directory);
    f = g_file_new_for_uri (uri);
    path = g_file_get_path (f);
    g_object_unref (f);
    g_free (uri);

    return path;
}

typedef struct
{
    gchar *uri;
    gboolean is_update;
} PreviewExportData;

static void
preview_export_data_free (gpointer _data)
{
    PreviewExportData *data = _data;
    g_free (data->uri);
    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PreviewExportData, preview_export_data_free)

static void
on_window_handle_export (NautilusWindow *window,
                         const char     *handle,
                         guint           xid,
                         gpointer        user_data)
{
    g_autoptr (PreviewExportData) data = user_data;
    nautilus_previewer_call_show_file (data->uri, handle, xid, !data->is_update);
}

static void
nautilus_files_view_preview (NautilusFilesView *view,
                             PreviewExportData *data)
{
    if (!nautilus_window_export_handle (nautilus_files_view_get_window (view),
                                        on_window_handle_export,
                                        data))
    {
        /* Let's use a fallback, so at least a preview will be displayed */
        nautilus_previewer_call_show_file (data->uri, "x11:0", 0, !data->is_update);
    }
}

static void
nautilus_files_view_preview_update (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    GtkApplication *app;
    GtkWindow *window;
    g_autolist (NautilusFile) selection = NULL;
    PreviewExportData *data;

    if (!priv->active ||
        !nautilus_previewer_is_visible ())
    {
        return;
    }

    app = GTK_APPLICATION (g_application_get_default ());
    window = GTK_WINDOW (nautilus_files_view_get_window (view));
    if (window == NULL || window != gtk_application_get_active_window (app))
    {
        return;
    }

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    if (selection == NULL)
    {
        return;
    }

    data = g_new0 (PreviewExportData, 1);
    data->uri = nautilus_file_get_uri (selection->data);
    data->is_update = TRUE;

    nautilus_files_view_preview (view, data);
}

void
nautilus_files_view_preview_selection_event (NautilusFilesView *self,
                                             GtkDirectionType   direction)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    nautilus_list_base_preview_selection_event (priv->list_base, direction);
}

static void
nautilus_files_view_activate_files (NautilusFilesView *view,
                                    GList             *files,
                                    NautilusOpenFlags  flags,
                                    gboolean           confirm_multiple)
{
    NautilusFilesViewPrivate *priv;
    GList *files_to_extract;
    GList *files_to_activate;
    char *path;

    if (files == NULL)
    {
        return;
    }

    priv = nautilus_files_view_get_instance_private (view);

    files_to_extract = nautilus_file_list_filter (files,
                                                  &files_to_activate,
                                                  (NautilusFileFilterFunc) nautilus_mime_file_extracts,
                                                  NULL);

    if (nautilus_files_view_supports_extract_here (view))
    {
        g_autoptr (GFile) location = NULL;
        g_autoptr (GFile) parent = NULL;

        location = nautilus_file_get_location (NAUTILUS_FILE (g_list_first (files)->data));
        /* Get a parent from a random file. We assume all files has a common parent.
         * But don't assume the parent is the view location, since that's not the
         * case in list view when expand-folder setting is set
         */
        parent = g_file_get_parent (location);
        extract_files (view, files_to_extract, parent);
    }
    else
    {
        extract_files_to_chosen_location (view, files_to_extract);
    }

    path = get_view_directory (view);
    nautilus_mime_activate_files (nautilus_files_view_get_containing_window (view),
                                  priv->slot,
                                  files_to_activate,
                                  path,
                                  flags,
                                  confirm_multiple);

    g_free (path);
    g_list_free (files_to_extract);
    g_list_free (files_to_activate);
}

void
nautilus_files_view_activate_selection (NautilusFilesView *self,
                                        NautilusOpenFlags  flags)
{
    g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));

    nautilus_files_view_activate_files (self, selection, flags, TRUE);
}

void
nautilus_files_view_activate_file (NautilusFilesView *view,
                                   NautilusFile      *file,
                                   NautilusOpenFlags  flags)
{
    g_autoptr (GList) files = NULL;

    files = g_list_append (files, file);
    nautilus_files_view_activate_files (view, files, flags, FALSE);
}

static void
action_open_with_default_application (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);
    nautilus_files_view_activate_selection (view, 0);
}

static void
action_open_item_location (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autolist (NautilusFile) selection = NULL;
    NautilusFile *item;
    GFile *activation_location;
    NautilusFile *activation_file;
    NautilusFile *parent;
    g_autoptr (GFile) parent_location = NULL;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    if (!selection)
    {
        return;
    }

    item = NAUTILUS_FILE (selection->data);
    activation_location = nautilus_file_get_activation_location (item);
    activation_file = nautilus_file_get (activation_location);
    parent = nautilus_file_get_parent (activation_file);
    parent_location = nautilus_file_get_location (parent);

    if (nautilus_file_is_in_recent (item))
    {
        /* Selection logic will check against a NautilusFile of the
         * activation uri, not the recent:// one. Fixes bug 784516 */
        nautilus_file_unref (item);
        item = nautilus_file_ref (activation_file);
        selection->data = item;
    }

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             parent_location, 0, selection, NULL,
                                             priv->slot);

    nautilus_file_unref (parent);
    nautilus_file_unref (activation_file);
    g_object_unref (activation_location);
}

static void
action_open_item_new_tab (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;

    view = NAUTILUS_FILES_VIEW (user_data);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    nautilus_files_view_activate_files (view,
                                        selection,
                                        NAUTILUS_OPEN_FLAG_NEW_TAB |
                                        NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE,
                                        TRUE);
}

static void
sandboxed_choose_program_callback (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
    XdpPortal *portal = XDP_PORTAL (source_object);
    GList *files = user_data;

    if (xdp_portal_open_uri_finish (portal, res, NULL))
    {
        /* Extract URI from the opened file and add it to recents. */
        g_autofree gchar *uri = nautilus_file_get_uri (NAUTILUS_FILE (files->data));
        gtk_recent_manager_add_item (gtk_recent_manager_get_default (), uri);
    }

    /* Remove hanlded file from the list top of the list. */
    g_object_unref (files->data);
    files = g_list_delete_link (files, files);

    if (files != NULL)
    {
        /* Ask to choose app and open the next file. */
        g_autofree gchar *uri = nautilus_file_get_uri (NAUTILUS_FILE (files->data));

        xdp_portal_open_uri (portal,
                             g_object_get_data (G_OBJECT (portal), "nautilus-parent"),
                             uri,
                             XDP_OPEN_URI_FLAG_ASK,
                             NULL,
                             sandboxed_choose_program_callback,
                             g_steal_pointer (&files));
    }
}

static void
sandboxed_choose_program (NautilusFilesView *view,
                          GList             *files,
                          GtkWindow         *window)
{
    g_autoptr (XdpPortal) portal = xdp_portal_new ();
    XdpParent *parent = xdp_parent_new_gtk (window);
    g_autofree gchar *uri = nautilus_file_get_uri (NAUTILUS_FILE (files->data));

    g_object_set_data_full (G_OBJECT (portal), "nautilus-parent",
                            parent, (GDestroyNotify) xdp_parent_free);

    /* Ask to choose app and open the first file. */
    xdp_portal_open_uri (portal,
                         parent,
                         uri,
                         XDP_OPEN_URI_FLAG_ASK,
                         NULL,
                         sandboxed_choose_program_callback,
                         g_steal_pointer (&files));
}

static void
app_chooser_dialog_response_cb (GtkDialog *dialog,
                                gint       response_id,
                                gpointer   user_data)
{
    GtkWindow *parent_window;
    GList *files;
    GAppInfo *info;

    parent_window = user_data;
    files = g_object_get_data (G_OBJECT (dialog), "directory-view:files");

    if (response_id != GTK_RESPONSE_OK)
    {
        goto out;
    }

    info = nautilus_app_chooser_get_app_info (NAUTILUS_APP_CHOOSER (dialog));

    nautilus_launch_application (info, files, parent_window);

    g_object_unref (info);
out:
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
choose_program (NautilusFilesView *view,
                GList             *files)
{
    GtkWidget *dialog;
    GtkWindow *parent_window;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    parent_window = nautilus_files_view_get_containing_window (view);

    if (nautilus_application_is_sandboxed ())
    {
        sandboxed_choose_program (view, g_steal_pointer (&files), parent_window);
        return;
    }

    dialog = GTK_WIDGET (nautilus_app_chooser_new (files, parent_window));
    g_object_set_data_full (G_OBJECT (dialog),
                            "directory-view:files",
                            files,
                            (GDestroyNotify) nautilus_file_list_free);
    gtk_window_present (GTK_WINDOW (dialog));

    g_signal_connect_object (dialog, "response",
                             G_CALLBACK (app_chooser_dialog_response_cb),
                             parent_window, 0);
}

static void
open_with_other_program (NautilusFilesView *view)
{
    g_autolist (NautilusFile) selection = NULL;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    choose_program (view, g_steal_pointer (&selection));
}

static void
action_open_with_other_application (GSimpleAction *action,
                                    GVariant      *state,
                                    gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    open_with_other_program (NAUTILUS_FILES_VIEW (user_data));
}

static void
action_open_current_directory_with_other_application (GSimpleAction *action,
                                                      GVariant      *state,
                                                      gpointer       user_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    GList *files;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory_as_file != NULL)
    {
        files = g_list_append (NULL, nautilus_file_ref (priv->directory_as_file));
        choose_program (view, files);
    }
}

static void
trash_or_delete_selected_files (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    priv = nautilus_files_view_get_instance_private (view);

    /* This might be rapidly called multiple times for the same selection
     * when using keybindings. So we remember if the current selection
     * was already removed (but the view doesn't know about it yet).
     */
    if (!priv->selection_was_removed)
    {
        g_autolist (NautilusFile) selection = NULL;
        selection = nautilus_files_view_get_selection_for_file_transfer (view);
        trash_or_delete_files (nautilus_files_view_get_containing_window (view),
                               selection,
                               view);
        priv->selection_was_removed = TRUE;
    }
}

static void
action_move_to_trash (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    trash_or_delete_selected_files (NAUTILUS_FILES_VIEW (user_data));
}

static void
action_remove_from_recent (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    /* gvfs handles this with normal delete operations if the path starts
     * with "recent://" */
    trash_or_delete_selected_files (NAUTILUS_FILES_VIEW (user_data));
}

static void
delete_selected_files (NautilusFilesView *view)
{
    GList *selection;
    GList *node;
    GList *locations;

    selection = nautilus_files_view_get_selection_for_file_transfer (view);
    if (selection == NULL)
    {
        return;
    }

    locations = NULL;
    for (node = selection; node != NULL; node = node->next)
    {
        locations = g_list_prepend (locations,
                                    nautilus_file_get_location ((NautilusFile *) node->data));
    }
    locations = g_list_reverse (locations);

    nautilus_file_operations_delete_async (locations, nautilus_files_view_get_containing_window (view), NULL, NULL, NULL);

    g_list_free_full (locations, g_object_unref);
    nautilus_file_list_free (selection);
}

static void
action_delete (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
    delete_selected_files (NAUTILUS_FILES_VIEW (user_data));
}

static void
action_star (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    nautilus_tag_manager_star_files (nautilus_tag_manager_get (),
                                     G_OBJECT (view),
                                     selection,
                                     NULL,
                                     priv->starred_cancellable);
}

static void
action_unstar (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    nautilus_tag_manager_unstar_files (nautilus_tag_manager_get (),
                                       G_OBJECT (view),
                                       selection,
                                       NULL,
                                       priv->starred_cancellable);
}

static void
action_restore_from_trash (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusFilesView *view;
    GList *selection;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_files_view_get_selection_for_file_transfer (view);
    nautilus_restore_files_from_trash (selection,
                                       nautilus_files_view_get_containing_window (view));

    nautilus_file_list_free (selection);
}

static void
action_select_all (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusFilesView *view;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);

    nautilus_files_view_select_all (view);
}

static void
action_invert_selection (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    nautilus_files_view_invert_selection (user_data);
}

static void
action_preview_selection (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    g_autolist (NautilusFile) selection = NULL;
    PreviewExportData *data = g_new0 (PreviewExportData, 1);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    data->uri = nautilus_file_get_uri (selection->data);
    data->is_update = FALSE;

    nautilus_files_view_preview (view, data);
}

static void
action_popup_menu (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    gboolean no_selection = gtk_bitset_is_empty (selection);

    if (no_selection)
    {
        nautilus_files_view_pop_up_background_context_menu (view, &GRAPHENE_POINT_INIT (0, 0));
        return;
    }

    nautilus_files_view_pop_up_selection_context_menu (view, NULL);
}

static void
pattern_select_response_select (AdwWindow *dialog,
                                gpointer   user_data)
{
    NautilusFilesView *view = g_object_get_data (G_OBJECT (dialog), "view");
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    GtkWidget *entry = g_object_get_data (G_OBJECT (dialog), "entry");
    const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
    g_autolist (NautilusFile) selection = NULL;

    selection = nautilus_directory_match_pattern (priv->directory, text);

    for (GList *l = priv->subdirectory_list; l != NULL; l = l->next)
    {
        NautilusDirectory *subdirectory = l->data;
        selection = g_list_concat (selection, nautilus_directory_match_pattern (subdirectory, text));
    }

    nautilus_files_view_call_set_selection (view, selection);

    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
select_pattern (NautilusFilesView *view)
{
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *dialog;
    NautilusWindow *window;
    GtkWidget *example;
    GtkWidget *entry, *select_button;
    char *example_pattern;

    window = nautilus_files_view_get_window (view);
    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-files-view-select-items.ui");
    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "select_items_dialog"));

    example = GTK_WIDGET (gtk_builder_get_object (builder, "example"));
    example_pattern = g_strdup_printf ("%s<i>%s</i> ",
                                       _("Examples: "),
                                       "*.png, file\?\?.txt, pict*.\?\?\?");
    gtk_label_set_markup (GTK_LABEL (example), example_pattern);
    g_free (example_pattern);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));

    entry = GTK_WIDGET (gtk_builder_get_object (builder, "pattern_entry"));
    select_button = GTK_WIDGET (gtk_builder_get_object (builder, "select_button"));

    g_object_set_data (G_OBJECT (dialog), "entry", entry);
    g_object_set_data (G_OBJECT (dialog), "view", view);
    g_signal_connect_swapped (select_button, "clicked",
                              G_CALLBACK (pattern_select_response_select),
                              dialog);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
action_select_pattern (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    select_pattern (user_data);
}

typedef struct
{
    NautilusFilesView *directory_view;
    GHashTable *added_locations;
    GList *selection;
} NewFolderData;

typedef struct
{
    NautilusFilesView *directory_view;
    GHashTable *to_remove_locations;
    NautilusFile *new_folder;
} NewFolderSelectionData;

static void
track_newly_added_locations (NautilusFilesView *view,
                             GList             *new_files,
                             gpointer           user_data)
{
    GHashTable *added_locations;

    added_locations = user_data;

    while (new_files)
    {
        NautilusFile *new_file;

        new_file = NAUTILUS_FILE (new_files->data);

        g_hash_table_add (added_locations,
                          nautilus_file_get_location (new_file));

        new_files = new_files->next;
    }
}

static void
new_folder_done (GFile    *new_folder,
                 gboolean  success,
                 gpointer  user_data)
{
    NautilusFilesView *directory_view;
    NautilusFilesViewPrivate *priv;
    NautilusFile *file;
    NewFolderData *data;

    data = (NewFolderData *) user_data;

    directory_view = data->directory_view;
    priv = nautilus_files_view_get_instance_private (directory_view);

    if (directory_view == NULL)
    {
        goto fail;
    }

    g_signal_handlers_disconnect_by_func (directory_view,
                                          G_CALLBACK (track_newly_added_locations),
                                          data->added_locations);

    if (new_folder == NULL)
    {
        goto fail;
    }

    file = nautilus_file_get (new_folder);

    if (data->selection != NULL)
    {
        GList *uris, *l;
        char *target_uri;

        uris = NULL;
        for (l = data->selection; l != NULL; l = l->next)
        {
            uris = g_list_prepend (uris, nautilus_file_get_uri ((NautilusFile *) l->data));
        }
        uris = g_list_reverse (uris);

        target_uri = nautilus_file_get_uri (file);

        nautilus_files_view_move_copy_items (directory_view,
                                             uris,
                                             target_uri,
                                             GDK_ACTION_MOVE);
        g_list_free_full (uris, g_free);
        g_free (target_uri);
    }

    if (g_hash_table_contains (data->added_locations, new_folder))
    {
        /* The file was already added */
        nautilus_files_view_call_set_selection (directory_view, &(GList){ .data = file });
    }
    else
    {
        g_hash_table_insert (priv->pending_reveal,
                             file,
                             GUINT_TO_POINTER (TRUE));
    }

    nautilus_file_unref (file);

fail:
    g_hash_table_destroy (data->added_locations);

    if (data->directory_view != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
                                      (gpointer *) &data->directory_view);
    }

    nautilus_file_list_free (data->selection);
    g_free (data);
}


static NewFolderData *
new_folder_data_new (NautilusFilesView *directory_view,
                     gboolean           with_selection)
{
    NewFolderData *data;

    data = g_new (NewFolderData, 1);
    data->directory_view = directory_view;
    data->added_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                                   g_object_unref, NULL);
    if (with_selection)
    {
        data->selection = nautilus_files_view_get_selection_for_file_transfer (directory_view);
    }
    else
    {
        data->selection = NULL;
    }
    g_object_add_weak_pointer (G_OBJECT (data->directory_view),
                               (gpointer *) &data->directory_view);

    return data;
}

static void
rename_file_popover_callback (NautilusFile *target_file,
                              const char   *new_name,
                              gpointer      user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    /* Put it on the queue for reveal after the view acknowledges the change */
    g_hash_table_insert (priv->pending_reveal,
                         target_file,
                         GUINT_TO_POINTER (FALSE));

    nautilus_rename_file (target_file, new_name, NULL, NULL);
}

static gboolean
get_selected_rectangle (NautilusFilesView *self,
                        GdkRectangle      *rectangle)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    GtkWidget *item_ui = nautilus_list_base_get_selected_item_ui (priv->list_base);
    graphene_point_t view_point;

    if (item_ui == NULL)
    {
        return FALSE;
    }

    gtk_widget_get_allocation (item_ui, rectangle);
    if (!gtk_widget_compute_point (item_ui, GTK_WIDGET (self),
                                   &GRAPHENE_POINT_INIT (rectangle->x,
                                                         rectangle->y),
                                   &view_point))
    {
        return FALSE;
    }

    rectangle->x = view_point.x;
    rectangle->y = view_point.y;
    return TRUE;
}

static void
nautilus_files_view_rename_file_popover_new (NautilusFilesView *view,
                                             NautilusFile      *target_file)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    GdkRectangle pointing_to;

    if (!get_selected_rectangle (view, &pointing_to))
    {
        g_return_if_reached ();
    }

    nautilus_rename_file_popover_show_for_file (NAUTILUS_RENAME_FILE_POPOVER (priv->rename_file_popover),
                                                target_file,
                                                &pointing_to,
                                                rename_file_popover_callback,
                                                view);
}

static void
create_new_folder_callback (const char *folder_name,
                            gboolean    with_selection,
                            gpointer    user_data)
{
    NautilusFilesView *view;
    NewFolderData *data;
    g_autofree gchar *parent_uri = NULL;
    NautilusFile *parent;

    view = NAUTILUS_FILES_VIEW (user_data);

    data = new_folder_data_new (view, with_selection);

    g_signal_connect_data (view,
                           "add-files",
                           G_CALLBACK (track_newly_added_locations),
                           data->added_locations,
                           (GClosureNotify) NULL,
                           G_CONNECT_AFTER);

    parent_uri = nautilus_files_view_get_backing_uri (view);
    parent = nautilus_file_get_by_uri (parent_uri);
    nautilus_file_operations_new_folder (GTK_WIDGET (view),
                                         NULL,
                                         parent_uri, folder_name,
                                         new_folder_done, data);

    /* After the dialog is destroyed the focus, is probably in the menu item
     * that created the dialog, but we want the focus to be in the newly created
     * folder.
     */
    gtk_widget_grab_focus (GTK_WIDGET (view));

    g_object_unref (parent);
}

static void
nautilus_files_view_new_folder_dialog_new (NautilusFilesView *view,
                                           gboolean           with_selection)
{
    g_autoptr (NautilusDirectory) containing_directory = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *common_prefix = NULL;

    uri = nautilus_files_view_get_backing_uri (view);
    containing_directory = nautilus_directory_get_by_uri (uri);

    if (with_selection)
    {
        g_autolist (NautilusFile) selection = NULL;
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        common_prefix = nautilus_get_common_filename_prefix (selection, MIN_COMMON_FILENAME_PREFIX_LENGTH);
    }

    (void) nautilus_new_folder_dialog_new (nautilus_files_view_get_containing_window (view),
                                           containing_directory,
                                           with_selection,
                                           common_prefix,
                                           create_new_folder_callback,
                                           view);
}

typedef struct
{
    NautilusFilesView *view;
    GHashTable *added_locations;
} CompressData;

static void
compress_done (GFile    *new_file,
               gboolean  success,
               gpointer  user_data)
{
    CompressData *data;
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    NautilusFile *file;
    char *uri = NULL;

    data = user_data;
    view = data->view;

    if (view == NULL)
    {
        goto out;
    }

    priv = nautilus_files_view_get_instance_private (view);

    g_signal_handlers_disconnect_by_func (view,
                                          G_CALLBACK (track_newly_added_locations),
                                          data->added_locations);

    if (!success)
    {
        goto out;
    }

    file = nautilus_file_get (new_file);

    if (g_hash_table_contains (data->added_locations, new_file))
    {
        /* The file was already added */
        nautilus_files_view_call_set_selection (view, &(GList){ .data = file });
    }
    else
    {
        g_hash_table_insert (priv->pending_reveal,
                             file,
                             GUINT_TO_POINTER (TRUE));
    }

    uri = nautilus_file_get_uri (file);
    gtk_recent_manager_add_item (gtk_recent_manager_get_default (), uri);

    nautilus_file_unref (file);
out:
    g_hash_table_destroy (data->added_locations);

    if (data->view != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (data->view),
                                      (gpointer *) &data->view);
    }

    g_free (uri);
    g_free (data);
}

static void
create_archive_callback (const char *archive_name,
                         const char *passphrase,
                         gpointer    user_data)
{
    CompressCallbackData *callback_data = user_data;
    NautilusFilesView *view;
    GList *source_files = NULL;
    GList *l;
    CompressData *data;
    g_autoptr (GFile) output = NULL;
    g_autoptr (GFile) parent = NULL;
    NautilusCompressionFormat compression_format;
    AutoarFormat format;
    AutoarFilter filter;

    view = NAUTILUS_FILES_VIEW (callback_data->view);

    for (l = callback_data->selection; l != NULL; l = l->next)
    {
        source_files = g_list_prepend (source_files,
                                       nautilus_file_get_location (l->data));
    }
    source_files = g_list_reverse (source_files);

    /* Get a parent from a random file. We assume all files has a common parent.
     * But don't assume the parent is the view location, since that's not the
     * case in list view when expand-folder setting is set
     */
    parent = g_file_get_parent (G_FILE (g_list_first (source_files)->data));
    output = g_file_get_child (parent, archive_name);

    data = g_new (CompressData, 1);
    data->view = view;
    data->added_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                                   g_object_unref, NULL);
    g_object_add_weak_pointer (G_OBJECT (data->view),
                               (gpointer *) &data->view);

    g_signal_connect_data (view,
                           "add-files",
                           G_CALLBACK (track_newly_added_locations),
                           data->added_locations,
                           NULL,
                           G_CONNECT_AFTER);

    compression_format = g_settings_get_enum (nautilus_compression_preferences,
                                              NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT);

    switch (compression_format)
    {
        case NAUTILUS_COMPRESSION_ZIP:
        {
            format = AUTOAR_FORMAT_ZIP;
            filter = AUTOAR_FILTER_NONE;
        }
        break;

        case NAUTILUS_COMPRESSION_ENCRYPTED_ZIP:
        {
            format = AUTOAR_FORMAT_ZIP;
            filter = AUTOAR_FILTER_NONE;
        }
        break;

        case NAUTILUS_COMPRESSION_TAR_XZ:
        {
            format = AUTOAR_FORMAT_TAR;
            filter = AUTOAR_FILTER_XZ;
        }
        break;

        case NAUTILUS_COMPRESSION_7ZIP:
        {
            format = AUTOAR_FORMAT_7ZIP;
            filter = AUTOAR_FILTER_NONE;
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    nautilus_file_operations_compress (source_files, output,
                                       format,
                                       filter,
                                       passphrase,
                                       nautilus_files_view_get_containing_window (view),
                                       NULL,
                                       compress_done,
                                       data);

    g_list_free_full (source_files, g_object_unref);
}

static void
compress_callback_data_free (CompressCallbackData *data)
{
    nautilus_file_list_free (data->selection);
    g_free (data);
}

static void
nautilus_files_view_compress_dialog_new (NautilusFilesView *view)
{
    NautilusDirectory *containing_directory;
    g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    gboolean is_single_selection = selection != NULL && selection->next == NULL;
    g_autofree char *common_prefix = NULL;
    g_autofree char *uri = NULL;
    CompressCallbackData *data;
    NautilusCompressDialog *compress_dialog;

    uri = nautilus_files_view_get_backing_uri (view);
    containing_directory = nautilus_directory_get_by_uri (uri);

    if (is_single_selection)
    {
        const char *display_name = nautilus_file_get_display_name (selection->data);

        if (nautilus_file_is_directory (selection->data))
        {
            common_prefix = g_strdup (display_name);
        }
        else
        {
            common_prefix = nautilus_filename_strip_extension (display_name);
        }
    }
    else
    {
        common_prefix = nautilus_get_common_filename_prefix (selection,
                                                             MIN_COMMON_FILENAME_PREFIX_LENGTH);
    }

    data = g_new0 (CompressCallbackData, 1);
    data->view = view;
    data->selection = nautilus_files_view_get_selection_for_file_transfer (view);

    compress_dialog = nautilus_compress_dialog_new (nautilus_files_view_get_containing_window (view),
                                                    containing_directory,
                                                    common_prefix,
                                                    create_archive_callback,
                                                    data);
    g_object_weak_ref (G_OBJECT (compress_dialog),
                       (GWeakNotify) compress_callback_data_free,
                       data);
}

static void
nautilus_files_view_new_folder (NautilusFilesView *directory_view,
                                gboolean           with_selection)
{
    nautilus_files_view_new_folder_dialog_new (directory_view, with_selection);
}

static NewFolderData *
setup_new_folder_data (NautilusFilesView *directory_view)
{
    NewFolderData *data;

    data = new_folder_data_new (directory_view, FALSE);

    g_signal_connect_data (directory_view,
                           "add-files",
                           G_CALLBACK (track_newly_added_locations),
                           data->added_locations,
                           (GClosureNotify) NULL,
                           G_CONNECT_AFTER);

    return data;
}

void
nautilus_files_view_new_file_with_initial_contents (NautilusFilesView *view,
                                                    const char        *parent_uri,
                                                    const char        *filename,
                                                    const char        *initial_contents,
                                                    gsize              length)
{
    NewFolderData *data;

    g_assert (parent_uri != NULL);

    data = setup_new_folder_data (view);

    nautilus_file_operations_new_file (GTK_WIDGET (view),
                                       parent_uri, filename,
                                       initial_contents, length,
                                       new_folder_done, data);
}

static void
nautilus_files_view_new_file (NautilusFilesView *directory_view,
                              const char        *parent_uri,
                              NautilusFile      *source)
{
    NewFolderData *data;
    char *source_uri;
    char *container_uri;

    container_uri = NULL;
    if (parent_uri == NULL)
    {
        container_uri = nautilus_files_view_get_backing_uri (directory_view);
        g_assert (container_uri != NULL);
    }

    if (source == NULL)
    {
        nautilus_files_view_new_file_with_initial_contents (directory_view,
                                                            parent_uri != NULL ? parent_uri : container_uri,
                                                            NULL,
                                                            NULL,
                                                            0);
        g_free (container_uri);
        return;
    }

    data = setup_new_folder_data (directory_view);

    source_uri = nautilus_file_get_uri (source);

    nautilus_file_operations_new_file_from_template (GTK_WIDGET (directory_view),
                                                     parent_uri != NULL ? parent_uri : container_uri,
                                                     NULL,
                                                     source_uri,
                                                     new_folder_done, data);

    g_free (source_uri);
    g_free (container_uri);
}

static void
action_empty_trash (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    NautilusFilesView *view;
    GtkRoot *window;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    window = gtk_widget_get_root (GTK_WIDGET (view));

    nautilus_file_operations_empty_trash (GTK_WIDGET (window), TRUE, NULL);
}

static void
action_new_folder (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    nautilus_files_view_new_folder (NAUTILUS_FILES_VIEW (user_data), FALSE);
}

static void
action_new_folder_with_selection (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    nautilus_files_view_new_folder (NAUTILUS_FILES_VIEW (user_data), TRUE);
}

static void
real_open_console (NautilusFile      *file,
                   NautilusFilesView *view)
{
    GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (view));
    GVariant *parameters;
    g_autofree gchar *uri = NULL;

    uri = nautilus_file_get_uri (file);
    parameters = g_variant_new_parsed ("([%s], @a{sv} {})", uri);
    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                 NAUTILUS_DBUS_LAUNCHER_CONSOLE,
                                 "Open",
                                 parameters, GTK_WINDOW (window));
}

static void
action_open_console (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (user_data));
    gboolean is_single_selection = selection != NULL && selection->next == NULL;

    g_return_if_fail (is_single_selection);

    real_open_console (NAUTILUS_FILE (selection->data), NAUTILUS_FILES_VIEW (user_data));
}

static void
action_current_dir_open_console (GSimpleAction *action,
                                 GVariant      *state,
                                 gpointer       user_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);
    real_open_console (priv->directory_as_file, view);
}

static void
action_properties (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    g_autolist (NautilusFile) selection = NULL;
    GList *files;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    if (selection == NULL)
    {
        if (priv->directory_as_file != NULL)
        {
            files = g_list_append (NULL, nautilus_file_ref (priv->directory_as_file));

            nautilus_properties_window_present (files, GTK_WIDGET (view), NULL,
                                                NULL, NULL);

            nautilus_file_list_free (files);
        }
    }
    else
    {
        nautilus_properties_window_present (selection, GTK_WIDGET (view), NULL,
                                            NULL, NULL);
    }
}

static void
action_current_dir_properties (GSimpleAction *action,
                               GVariant      *state,
                               gpointer       user_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    GList *files;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory_as_file != NULL)
    {
        files = g_list_append (NULL, nautilus_file_ref (priv->directory_as_file));

        nautilus_properties_window_present (files, GTK_WIDGET (view), NULL,
                                            NULL, NULL);

        nautilus_file_list_free (files);
    }
}

static void
nautilus_files_view_set_show_hidden_files (NautilusFilesView *view,
                                           gboolean           show_hidden)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (show_hidden != priv->show_hidden_files)
    {
        priv->show_hidden_files = show_hidden;

        g_settings_set_boolean (gtk_filechooser_preferences,
                                NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                                show_hidden);

        if (priv->directory != NULL)
        {
            load_directory (view, priv->directory);
        }
    }
}

static void
action_show_hidden_files (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
    gboolean show_hidden;
    NautilusFilesView *view;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    show_hidden = g_variant_get_boolean (state);

    nautilus_files_view_set_show_hidden_files (view, show_hidden);

    g_simple_action_set_state (action, state);
}

static void
action_sort_order_changed (GSimpleAction *action,
                           GVariant      *value,
                           gpointer       user_data)
{
    g_autoptr (GVariant) old_value = g_action_get_state (G_ACTION (action));

    /* Don't resort if the action is in the same state as before */
    if (g_variant_equal (value, old_value))
    {
        return;
    }

    /* Actual changes happen through binding to NautilusListBase:sort-state. */
    g_simple_action_set_state (action, value);
}

static void
action_visible_columns (GSimpleAction *action,
                        GVariant      *state,
                        gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    g_return_if_fail (NAUTILUS_IS_LIST_VIEW (priv->list_base));

    nautilus_list_view_present_column_editor (NAUTILUS_LIST_VIEW (priv->list_base));
}

static void
update_zoom_actions_state (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    NautilusViewInfo view_info = nautilus_list_base_get_view_info (priv->list_base);
    gint zoom_level = nautilus_list_base_get_zoom_level (priv->list_base);
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "zoom-in");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 zoom_level < view_info.zoom_level_max);

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "zoom-out");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 zoom_level > view_info.zoom_level_min);

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "zoom-standard");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 zoom_level != view_info.zoom_level_standard);
}

static void
action_zoom_in (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    gint zoom_level = nautilus_list_base_get_zoom_level (priv->list_base);

    nautilus_list_base_set_zoom_level (priv->list_base, zoom_level + 1);
    update_zoom_actions_state (self);
}

static void
action_zoom_out (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    gint zoom_level = nautilus_list_base_get_zoom_level (priv->list_base);

    nautilus_list_base_set_zoom_level (priv->list_base, zoom_level - 1);
    update_zoom_actions_state (self);
}

static void
action_zoom_standard (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    NautilusViewInfo view_info = nautilus_list_base_get_view_info (priv->list_base);

    nautilus_list_base_set_zoom_level (priv->list_base, view_info.zoom_level_standard);
    update_zoom_actions_state (self);
}

static void
action_open_item_new_window (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       user_data)
{
    NautilusFilesView *view;
    GList *selection;

    view = NAUTILUS_FILES_VIEW (user_data);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    nautilus_files_view_activate_files (view,
                                        selection,
                                        NAUTILUS_OPEN_FLAG_NEW_WINDOW,
                                        TRUE);

    nautilus_file_list_free (selection);
}

typedef struct _PasteCallbackData
{
    NautilusFilesView *view;
    gboolean as_link;
    gchar *dest_uri;
} PasteCallbackData;

static void
paste_callback_data_free (PasteCallbackData *data)
{
    g_free (data->dest_uri);
    g_free (data);
}

static void
handle_clipboard_data (NautilusFilesView *view,
                       GList             *item_uris,
                       char              *destination_uri,
                       GdkDragAction      action)
{
    if (item_uris != NULL && destination_uri != NULL)
    {
        nautilus_files_view_move_copy_items (view, item_uris, destination_uri,
                                             action);

        /* If items are cut then remove from clipboard */
        if (action == GDK_ACTION_MOVE)
        {
            gdk_clipboard_set_content (gtk_widget_get_clipboard (GTK_WIDGET (view)),
                                       NULL);
        }
    }
}

static void
paste_value_received_callback (GObject      *source_object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
    GdkClipboard *clipboard;
    GdkDragAction action;
    const GValue *value;
    PasteCallbackData *data = user_data;
    g_autoptr (GError) error = NULL;

    clipboard = GDK_CLIPBOARD (source_object);

    value = gdk_clipboard_read_value_finish (clipboard, result, &error);
    if (error != NULL)
    {
        g_warning ("Failed to read clipboard: %s", error->message);
        return;
    }

    action = data->as_link ? GDK_ACTION_LINK : GDK_ACTION_COPY;

    if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_CLIPBOARD))
    {
        NautilusClipboard *clip = g_value_get_boxed (value);
        GList *item_uris = nautilus_clipboard_get_uri_list (clip);

        action = nautilus_clipboard_is_cut (clip) ? GDK_ACTION_MOVE : action;
        handle_clipboard_data (data->view, item_uris, data->dest_uri, action);

        g_list_free_full (item_uris, g_free);
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_list = g_value_get_boxed (value);
        GList *item_uris = NULL;

        for (GSList *l = source_list; l != NULL; l = l->next)
        {
            item_uris = g_list_prepend (item_uris, g_file_get_uri (l->data));
        }

        item_uris = g_list_reverse (item_uris);
        handle_clipboard_data (data->view, item_uris, data->dest_uri, action);
        g_list_free_full (item_uris, g_free);
    }
    else if (G_VALUE_HOLDS (value, G_TYPE_FILE))
    {
        GFile *location = g_value_get_object (value);
        GList *item_uris = g_list_append (NULL, g_file_get_uri (location));

        handle_clipboard_data (data->view, item_uris, data->dest_uri, action);
        g_list_free_full (item_uris, g_free);
    }

    paste_callback_data_free (data);
}

static void
paste_files (NautilusFilesView *view,
             gchar             *dest_uri,
             gboolean           as_link)
{
    PasteCallbackData *data;
    GdkClipboard *clipboard;
    GdkContentFormats *formats;
    gchar *real_dest_uri;
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
    formats = gdk_clipboard_get_formats (clipboard);

    real_dest_uri = dest_uri != NULL ? dest_uri : nautilus_files_view_get_backing_uri (view);

    if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_TEXTURE))
    {
        nautilus_file_operations_paste_image_from_clipboard (GTK_WIDGET (view),
                                                             NULL,
                                                             real_dest_uri,
                                                             copy_move_done_callback,
                                                             pre_copy_move (view));
        return;
    }

    data = g_new0 (PasteCallbackData, 1);
    data->dest_uri = real_dest_uri;
    data->as_link = as_link;
    data->view = view;

    if (gdk_content_formats_contain_gtype (formats, NAUTILUS_TYPE_CLIPBOARD))
    {
        gdk_clipboard_read_value_async (clipboard, NAUTILUS_TYPE_CLIPBOARD,
                                        G_PRIORITY_DEFAULT,
                                        priv->clipboard_cancellable,
                                        paste_value_received_callback, data);
    }
    else if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST))
    {
        gdk_clipboard_read_value_async (clipboard, GDK_TYPE_FILE_LIST,
                                        G_PRIORITY_DEFAULT,
                                        priv->clipboard_cancellable,
                                        paste_value_received_callback, data);
    }
    else if (gdk_content_formats_contain_gtype (formats, G_TYPE_FILE))
    {
        gdk_clipboard_read_value_async (clipboard, G_TYPE_FILE,
                                        G_PRIORITY_DEFAULT,
                                        priv->clipboard_cancellable,
                                        paste_value_received_callback, data);
    }
}

static void
action_paste_files (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);

    paste_files (view, NULL, FALSE);
}

static void
action_paste_files_accel (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);

    if (showing_starred_directory (view))
    {
        show_dialog (_("Could not paste files"),
                     _("Cannot paste files into Starred"),
                     nautilus_files_view_get_containing_window (view),
                     GTK_MESSAGE_ERROR);
    }
    else if (showing_recent_directory (view))
    {
        show_dialog (_("Could not paste files"),
                     _("Cannot paste files into Recent"),
                     nautilus_files_view_get_containing_window (view),
                     GTK_MESSAGE_ERROR);
    }
    else if (showing_trash_directory (view))
    {
        show_dialog (_("Could not paste files"),
                     _("Cannot paste files into Trash"),
                     nautilus_files_view_get_containing_window (view),
                     GTK_MESSAGE_ERROR);
    }
    else if (nautilus_files_view_is_read_only (view))
    {
        show_dialog (_("Could not paste files"),
                     _("Permissions do not allow pasting files in this directory"),
                     nautilus_files_view_get_containing_window (view),
                     GTK_MESSAGE_ERROR);
    }
    else
    {
        paste_files (view, NULL, FALSE);
    }
}

static void
action_create_links (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFilesView *view = user_data;
    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    paste_files (view, NULL, TRUE);
}

static void
show_hidden_files_changed_callback (gpointer callback_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    gboolean preference_value;

    view = NAUTILUS_FILES_VIEW (callback_data);
    priv = nautilus_files_view_get_instance_private (view);

    preference_value =
        g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

    nautilus_files_view_set_show_hidden_files (view, preference_value);

    if (priv->active)
    {
        schedule_update_context_menus (view);
    }
}

static gboolean
set_up_scripts_directory_global (void)
{
    g_autofree gchar *scripts_directory_path = NULL;
    g_autoptr (GFile) scripts_directory = NULL;
    g_autoptr (GError) error = NULL;

    if (scripts_directory_uri != NULL)
    {
        return TRUE;
    }

    scripts_directory_path = nautilus_get_scripts_directory_path ();
    scripts_directory = g_file_new_for_path (scripts_directory_path);

    g_file_make_directory_with_parents (scripts_directory, NULL, &error);

    if (error == NULL ||
        g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_file_set_attribute_uint32 (scripts_directory,
                                     G_FILE_ATTRIBUTE_UNIX_MODE,
                                     S_IRWXU,
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, NULL);

        scripts_directory_uri = g_file_get_uri (scripts_directory);
        scripts_directory_uri_length = strlen (scripts_directory_uri);
    }

    return scripts_directory_uri != NULL;
}

static void
scripts_added_or_changed_callback (NautilusDirectory *directory,
                                   GList             *files,
                                   gpointer           callback_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (callback_data);
    priv = nautilus_files_view_get_instance_private (view);

    priv->scripts_menu_updated = FALSE;
    if (priv->active)
    {
        schedule_update_context_menus (view);
    }
}

static void
templates_added_or_changed_callback (NautilusDirectory *directory,
                                     GList             *files,
                                     gpointer           callback_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (callback_data);
    priv = nautilus_files_view_get_instance_private (view);

    priv->templates_menu_updated = FALSE;
    if (priv->active)
    {
        schedule_update_context_menus (view);
    }
}

static void
add_directory_to_directory_list (NautilusFilesView  *view,
                                 NautilusDirectory  *directory,
                                 GList             **directory_list,
                                 GCallback           changed_callback)
{
    NautilusFileAttributes attributes;

    if (g_list_find (*directory_list, directory) == NULL)
    {
        nautilus_directory_ref (directory);

        attributes =
            NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
            NAUTILUS_FILE_ATTRIBUTE_INFO |
            NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

        nautilus_directory_file_monitor_add (directory, directory_list,
                                             FALSE, attributes,
                                             (NautilusDirectoryCallback) changed_callback, view);

        g_signal_connect_object (directory, "files-added",
                                 G_CALLBACK (changed_callback), view, 0);
        g_signal_connect_object (directory, "files-changed",
                                 G_CALLBACK (changed_callback), view, 0);

        *directory_list = g_list_append (*directory_list, directory);
    }
}

static void
remove_directory_from_directory_list (NautilusFilesView  *view,
                                      NautilusDirectory  *directory,
                                      GList             **directory_list,
                                      GCallback           changed_callback)
{
    *directory_list = g_list_remove (*directory_list, directory);

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (changed_callback),
                                          view);

    nautilus_directory_file_monitor_remove (directory, directory_list);

    nautilus_directory_unref (directory);
}


static void
add_directory_to_scripts_directory_list (NautilusFilesView *view,
                                         NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    add_directory_to_directory_list (view, directory,
                                     &priv->scripts_directory_list,
                                     G_CALLBACK (scripts_added_or_changed_callback));
}

static void
remove_directory_from_scripts_directory_list (NautilusFilesView *view,
                                              NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    remove_directory_from_directory_list (view, directory,
                                          &priv->scripts_directory_list,
                                          G_CALLBACK (scripts_added_or_changed_callback));
}

static void
add_directory_to_templates_directory_list (NautilusFilesView *view,
                                           NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    add_directory_to_directory_list (view, directory,
                                     &priv->templates_directory_list,
                                     G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (NautilusFilesView *view,
                                                NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    remove_directory_from_directory_list (view, directory,
                                          &priv->templates_directory_list,
                                          G_CALLBACK (templates_added_or_changed_callback));
}

static void
slot_active_changed (NautilusWindowSlot *slot,
                     GParamSpec         *pspec,
                     NautilusFilesView  *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->active == nautilus_window_slot_get_active (slot))
    {
        return;
    }

    priv->active = nautilus_window_slot_get_active (slot);

    if (priv->active)
    {
        /* Avoid updating the toolbar withouth making sure the toolbar
         * zoom slider has the correct adjustment that changes when the
         * view mode changes
         */
        nautilus_files_view_update_context_menus (view);
        nautilus_files_view_update_toolbar_menus (view);
        nautilus_files_view_preview_update (view);

        schedule_update_context_menus (view);

        gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                        "view",
                                        G_ACTION_GROUP (priv->view_action_group));
    }
    else
    {
        remove_update_context_menus_timeout_callback (view);
        gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                        "view",
                                        NULL);
    }
}

static gboolean
nautilus_files_view_grab_focus (GtkWidget *widget)
{
    /* focus the inner view if it exists */
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (widget);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    if (priv->list_base != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (priv->list_base));
    }

    return GTK_WIDGET_CLASS (nautilus_files_view_parent_class)->grab_focus (widget);
}

static void
nautilus_files_view_set_selection (NautilusView *nautilus_files_view,
                                   GList        *selection)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    GList *pending_selection;

    view = NAUTILUS_FILES_VIEW (nautilus_files_view);
    priv = nautilus_files_view_get_instance_private (view);

    if (!priv->loading)
    {
        /* If we aren't still loading, set the selection right now,
         * and reveal the new selection.
         */
        nautilus_files_view_call_set_selection (view, selection);
    }
    else
    {
        /* If we are still loading, set the list of pending URIs instead.
         * done_loading() will eventually select the pending URIs and reveal them.
         */
        pending_selection = g_list_copy_deep (selection,
                                              (GCopyFunc) g_object_ref, NULL);
        g_list_free_full (priv->pending_selection, g_object_unref);

        priv->pending_selection = pending_selection;
    }
}

static void
on_popup_background_context_menu (NautilusListBase *list_base,
                                  double            x,
                                  double            y,
                                  gpointer          user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    graphene_point_t view_point;

    if (!gtk_widget_compute_point (GTK_WIDGET (list_base), GTK_WIDGET (self),
                                   &GRAPHENE_POINT_INIT (x, y),
                                   &view_point))
    {
        g_return_if_reached ();
    }

    nautilus_files_view_pop_up_background_context_menu (self, &view_point);
}

static void
on_popup_selection_context_menu (NautilusListBase *list_base,
                                 double            x,
                                 double            y,
                                 GtkWidget        *target,
                                 gpointer          user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    graphene_point_t view_point;

    if (!gtk_widget_compute_point (GTK_WIDGET (target), GTK_WIDGET (self),
                                   &GRAPHENE_POINT_INIT (x, y),
                                   &view_point))
    {
        g_return_if_reached ();
    }

    nautilus_files_view_pop_up_selection_context_menu (self, &view_point);
}

static void
on_load_subdirectory (NautilusListBase *list_base,
                      NautilusViewItem *item,
                      gpointer          user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_for_file (nautilus_view_item_get_file (item));

    if (!nautilus_files_view_has_subdirectory (self, directory))
    {
        nautilus_files_view_add_subdirectory (self, directory);
    }
}

static void
on_unload_subdirectory (NautilusListBase *list_base,
                        NautilusViewItem *item,
                        gpointer          user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_for_file (nautilus_view_item_get_file (item));

    if (nautilus_files_view_has_subdirectory (self, directory))
    {
        nautilus_files_view_remove_subdirectory (self, directory);
    }
}

static void
connect_inner_view (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    /* Model must be set before the sort-state is bound, for a new sorter to be
     * set on the model. */
    nautilus_list_base_set_model (priv->list_base, priv->model);
    GAction *action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                                  "sort");
    g_object_bind_property (G_SIMPLE_ACTION (action), "state",
                            priv->list_base, "sort-state",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

    g_signal_connect_object (priv->list_base, "activate-selection",
                             G_CALLBACK (nautilus_files_view_activate_selection), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (priv->list_base, "perform-drop",
                             G_CALLBACK (nautilus_dnd_perform_drop), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (priv->list_base, "popup-background-context-menu",
                             G_CALLBACK (on_popup_background_context_menu), self,
                             G_CONNECT_DEFAULT);
    g_signal_connect_object (priv->list_base, "popup-selection-context-menu",
                             G_CALLBACK (on_popup_selection_context_menu), self,
                             G_CONNECT_DEFAULT);

    if (NAUTILUS_IS_LIST_VIEW (priv->list_base))
    {
        g_signal_connect_object (priv->list_base, "load-subdirectory",
                                 G_CALLBACK (on_load_subdirectory), self,
                                 G_CONNECT_DEFAULT);
        g_signal_connect_object (priv->list_base, "unload-subdirectory",
                                 G_CALLBACK (on_unload_subdirectory), self,
                                 G_CONNECT_DEFAULT);
    }

    nautilus_list_base_add_overlay (priv->list_base, priv->empty_view_page);
}

static void
nautilus_files_view_dispose (GObject *object)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    GdkClipboard *clipboard;
    GList *node, *next;

    view = NAUTILUS_FILES_VIEW (object);
    priv = nautilus_files_view_get_instance_private (view);

    priv->in_destruction = TRUE;
    nautilus_files_view_stop_loading (view);

    g_clear_pointer (&priv->selection_menu, gtk_widget_unparent);
    g_clear_pointer (&priv->background_menu, gtk_widget_unparent);
    g_clear_pointer (&priv->rename_file_popover, gtk_widget_unparent);

    if (priv->directory)
    {
        nautilus_directory_unref (priv->directory);
        priv->directory = NULL;
    }

    for (node = priv->scripts_directory_list; node != NULL; node = next)
    {
        next = node->next;
        remove_directory_from_scripts_directory_list (view, node->data);
    }

    for (node = priv->templates_directory_list; node != NULL; node = next)
    {
        next = node->next;
        remove_directory_from_templates_directory_list (view, node->data);
    }

    g_clear_pointer (&priv->subdirectories_loading, g_list_free);
    while (priv->subdirectory_list != NULL)
    {
        nautilus_files_view_remove_subdirectory (view,
                                                 priv->subdirectory_list->data);
    }

    remove_update_context_menus_timeout_callback (view);
    remove_update_status_idle_callback (view);

    g_clear_handle_id (&priv->search_transition_timeout_id, g_source_remove);

    if (priv->display_selection_idle_id != 0)
    {
        g_source_remove (priv->display_selection_idle_id);
        priv->display_selection_idle_id = 0;
    }

    if (priv->floating_bar_set_status_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_set_status_timeout_id);
        priv->floating_bar_set_status_timeout_id = 0;
    }

    if (priv->floating_bar_loading_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_loading_timeout_id);
        priv->floating_bar_loading_timeout_id = 0;
    }

    if (priv->floating_bar_set_passthrough_timeout_id != 0)
    {
        g_source_remove (priv->floating_bar_set_passthrough_timeout_id);
        priv->floating_bar_set_passthrough_timeout_id = 0;
    }

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          schedule_update_context_menus, view);
    g_signal_handlers_disconnect_by_func (gtk_filechooser_preferences,
                                          show_hidden_files_changed_callback, view);
    g_signal_handlers_disconnect_by_func (nautilus_window_state,
                                          nautilus_files_view_display_selection_info, view);
    g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
                                          schedule_update_context_menus, view);
    g_signal_handlers_disconnect_by_func (nautilus_trash_monitor_get (),
                                          nautilus_files_view_trash_state_changed_callback, view);

    clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
    g_signal_handlers_disconnect_by_func (clipboard, on_clipboard_owner_changed, view);
    g_cancellable_cancel (priv->clipboard_cancellable);

    nautilus_file_unref (priv->directory_as_file);
    priv->directory_as_file = NULL;

    g_clear_object (&priv->search_query);
    g_clear_object (&priv->location_before_search);
    g_clear_object (&priv->outgoing_search);
    g_clear_object (&priv->location);
    g_clear_object (&priv->model);

    adw_bin_set_child (ADW_BIN (view), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (view), NAUTILUS_TYPE_FILES_VIEW);

    G_OBJECT_CLASS (nautilus_files_view_parent_class)->dispose (object);
}

static void
nautilus_files_view_finalize (GObject *object)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (object);
    priv = nautilus_files_view_get_instance_private (view);

    g_clear_object (&priv->empty_view_page);
    g_clear_object (&priv->view_action_group);
    g_clear_object (&priv->background_menu_model);
    g_clear_object (&priv->selection_menu_model);
    g_clear_object (&priv->toolbar_menu_sections->sort_section);
    g_clear_object (&priv->extensions_background_menu);
    g_clear_object (&priv->templates_menu);
    /* We don't own the slot, so no unref */
    priv->slot = NULL;

    g_free (priv->toolbar_menu_sections);

    g_hash_table_destroy (priv->pending_reveal);

    g_clear_object (&priv->clipboard_cancellable);

    g_cancellable_cancel (priv->starred_cancellable);
    g_clear_object (&priv->starred_cancellable);

    G_OBJECT_CLASS (nautilus_files_view_parent_class)->finalize (object);
}

/**
 * nautilus_files_view_display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: NautilusFilesView for which to display selection info.
 *
 **/
static void
nautilus_files_view_display_selection_info (NautilusFilesView *view)
{
    g_autolist (NautilusFile) selection = NULL;
    goffset non_folder_size;
    gboolean non_folder_size_known;
    guint non_folder_count, folder_count, folder_item_count;
    gboolean folder_item_count_known;
    guint file_item_count;
    GList *p;
    const char *first_item_name = NULL;
    char *non_folder_count_str;
    char *non_folder_item_count_str;
    char *non_folder_counts_str;
    char *folder_count_str;
    char *folder_item_count_str;
    char *folder_counts_str;
    char *primary_status;
    char *detail_status;
    NautilusFile *file;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    if (priv->list_base != NULL && NAUTILUS_IS_NETWORK_VIEW (priv->list_base))
    {
        /* Selection info is not relevant on this view and visually clashes with
         * the action bar. */
        return;
    }

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    folder_item_count_known = TRUE;
    folder_count = 0;
    folder_item_count = 0;
    non_folder_count = 0;
    non_folder_size_known = FALSE;
    non_folder_size = 0;
    folder_count_str = NULL;
    folder_item_count_str = NULL;
    folder_counts_str = NULL;
    non_folder_count_str = NULL;
    non_folder_item_count_str = NULL;
    non_folder_counts_str = NULL;

    for (p = selection; p != NULL; p = p->next)
    {
        file = p->data;
        if (nautilus_file_is_directory (file))
        {
            folder_count++;
            if (nautilus_file_get_directory_item_count (file, &file_item_count, NULL))
            {
                folder_item_count += file_item_count;
            }
            else
            {
                folder_item_count_known = FALSE;
            }
        }
        else
        {
            non_folder_count++;
            if (!nautilus_file_can_get_size (file))
            {
                non_folder_size_known = TRUE;
                non_folder_size += nautilus_file_get_size (file);
            }
        }

        if (first_item_name == NULL)
        {
            first_item_name = nautilus_file_get_display_name (file);
        }
    }

    /* Break out cases for localization's sake. But note that there are still pieces
     * being assembled in a particular order, which may be a problem for some localizers.
     */

    if (folder_count != 0)
    {
        if (folder_count == 1 && non_folder_count == 0)
        {
            folder_count_str = g_strdup_printf (_("“%s” selected"), first_item_name);
        }
        else
        {
            folder_count_str = g_strdup_printf (ngettext ("%'d folder selected",
                                                          "%'d folders selected",
                                                          folder_count),
                                                folder_count);
        }

        if (folder_count == 1)
        {
            if (!folder_item_count_known)
            {
                folder_item_count_str = g_strdup ("");
            }
            else
            {
                folder_item_count_str = g_strdup_printf (ngettext ("(containing %'d item)",
                                                                   "(containing %'d items)",
                                                                   folder_item_count),
                                                         folder_item_count);
            }
        }
        else
        {
            if (!folder_item_count_known)
            {
                folder_item_count_str = g_strdup ("");
            }
            else
            {
                /* translators: this is preceded with a string of form 'N folders' (N more than 1) */
                folder_item_count_str = g_strdup_printf (ngettext ("(containing a total of %'d item)",
                                                                   "(containing a total of %'d items)",
                                                                   folder_item_count),
                                                         folder_item_count);
            }
        }
    }

    if (non_folder_count != 0)
    {
        if (folder_count == 0)
        {
            if (non_folder_count == 1)
            {
                non_folder_count_str = g_strdup_printf (_("“%s” selected"),
                                                        first_item_name);
            }
            else
            {
                non_folder_count_str = g_strdup_printf (ngettext ("%'d item selected",
                                                                  "%'d items selected",
                                                                  non_folder_count),
                                                        non_folder_count);
            }
        }
        else
        {
            /* Folders selected also, use "other" terminology */
            non_folder_count_str = g_strdup_printf (ngettext ("%'d other item selected",
                                                              "%'d other items selected",
                                                              non_folder_count),
                                                    non_folder_count);
        }

        if (non_folder_size_known)
        {
            char *size_string;

            size_string = g_format_size (non_folder_size);
            /* This is marked for translation in case a localiser
             * needs to use something other than parentheses. The
             * the message in parentheses is the size of the selected items.
             */
            non_folder_item_count_str = g_strdup_printf (_("(%s)"), size_string);
            g_free (size_string);
        }
        else
        {
            non_folder_item_count_str = g_strdup ("");
        }
    }

    if (folder_count == 0 && non_folder_count == 0)
    {
        primary_status = NULL;
        detail_status = NULL;
    }
    else if (folder_count == 0)
    {
        primary_status = g_strdup (non_folder_count_str);
        detail_status = g_strdup (non_folder_item_count_str);
    }
    else if (non_folder_count == 0)
    {
        primary_status = g_strdup (folder_count_str);
        detail_status = g_strdup (folder_item_count_str);
    }
    else
    {
        if (folder_item_count_known)
        {
            folder_counts_str = g_strconcat (folder_count_str, " ", folder_item_count_str, NULL);
        }
        else
        {
            folder_counts_str = g_strdup (folder_count_str);
        }

        if (non_folder_size_known)
        {
            non_folder_counts_str = g_strconcat (non_folder_count_str, " ", non_folder_item_count_str, NULL);
        }
        else
        {
            non_folder_counts_str = g_strdup (non_folder_count_str);
        }
        /* This is marked for translation in case a localizer
         * needs to change ", " to something else. The comma
         * is between the message about the number of folders
         * and the number of items in those folders and the
         * message about the number of other items and the
         * total size of those items.
         */
        primary_status = g_strdup_printf (_("%s, %s"),
                                          folder_counts_str,
                                          non_folder_counts_str);
        detail_status = NULL;
    }

    g_free (folder_count_str);
    g_free (folder_item_count_str);
    g_free (folder_counts_str);
    g_free (non_folder_count_str);
    g_free (non_folder_item_count_str);
    g_free (non_folder_counts_str);

    set_floating_bar_status (view, primary_status, detail_status);

    g_free (primary_status);
    g_free (detail_status);
}

static void
nautilus_files_view_send_selection_change (NautilusFilesView *view)
{
    g_signal_emit (view, signals[SELECTION_CHANGED], 0);
    g_object_notify (G_OBJECT (view), "selection");
}

static void
nautilus_files_view_set_location (NautilusView *view,
                                  GFile        *location)
{
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get (location);

    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        /* Special case.
         *
         * This may happen if switching view mode while searching. In that case,
         * we need to run the previous query again with the new view, because
         * `load_directory()` alone doesn't load the old results results for us.
         *
         * In this case we don't call `load_directory()` here because that's
         * going to be called internally by `nautilus_view_set_search_query()`
         */
        NautilusQuery *previous_query;

        previous_query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
        nautilus_view_set_search_query (view, previous_query);
    }
    else
    {
        /* Regular case */
        load_directory (NAUTILUS_FILES_VIEW (view), directory);

        /* Ensure we don't keep carrying old queries on. This must be called
         * after load_directory() is called for the new directory, otherwise it
         * could try to load the existing search query's base directory. */
        nautilus_view_set_search_query (view, NULL);
    }
}

static GtkWidget *
build_search_settings_button (NautilusFilesView *self)
{
    GtkWidget *button = gtk_button_new_with_mnemonic (_("Search _Settings"));

    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "pill");
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.search-settings");

    return button;
}

static GtkWidget *
build_search_everywhere_button (NautilusFilesView *self)
{
    GtkWidget *button = gtk_button_new_with_mnemonic (_("Search _Everywhere"));

    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "pill");
    gtk_widget_add_css_class (button, "suggested-action");
    gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "slot.search-global");

    return button;
}

static void
nautilus_files_view_check_empty_states (NautilusFilesView *view)
{
    NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->check_empty_states (view);
}

static void
real_check_empty_states (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    AdwStatusPage *status_page = ADW_STATUS_PAGE (priv->empty_view_page);

    if (!priv->loading &&
        nautilus_files_view_is_empty (view))
    {
        adw_status_page_set_child (status_page, NULL);

        if (NAUTILUS_IS_SEARCH_DIRECTORY (priv->directory))
        {
            NautilusSearchDirectory *search = NAUTILUS_SEARCH_DIRECTORY (priv->directory);
            NautilusQuery *query = nautilus_search_directory_get_query (search);
            gboolean global_search = nautilus_query_is_global (query);

            if (global_search)
            {
                adw_status_page_set_icon_name (status_page, "edit-find-symbolic");
                adw_status_page_set_description (status_page,
                                                 _("More locations can be added to search in the settings"));
                adw_status_page_set_child (status_page, build_search_settings_button (view));
            }
            else
            {
                g_autoptr (GFile) location = nautilus_query_get_location (query);
                g_autoptr (NautilusFile) file = nautilus_file_get (location);
                /* Translators: %s is the name of the search location formatted for display */
                g_autofree gchar *local_description = g_strdup_printf (_("No matches in “%s”"),
                                                                       nautilus_file_get_display_name (file));

                adw_status_page_set_icon_name (status_page, "nautilus-folder-search-symbolic");
                adw_status_page_set_description (status_page, local_description);
                adw_status_page_set_child (status_page, (build_search_everywhere_button (view)));
            }

            adw_status_page_set_title (status_page, _("No Results Found"));
        }
        else if (nautilus_is_root_for_scheme (priv->location, SCHEME_TRASH))
        {
            adw_status_page_set_icon_name (status_page, "user-trash-symbolic");
            adw_status_page_set_title (status_page, _("Trash is Empty"));
            adw_status_page_set_description (status_page, NULL);
        }
        else if (g_file_has_uri_scheme (priv->location, SCHEME_STARRED))
        {
            adw_status_page_set_icon_name (status_page, "starred-symbolic");
            adw_status_page_set_title (status_page, _("No Starred Files"));
            adw_status_page_set_description (status_page, NULL);
        }
        else if (g_file_has_uri_scheme (priv->location, SCHEME_RECENT))
        {
            adw_status_page_set_icon_name (status_page, "document-open-recent-symbolic");
            adw_status_page_set_title (status_page, _("No Recent Files"));
            adw_status_page_set_description (status_page, NULL);
        }
        else if (g_file_has_uri_scheme (priv->location, SCHEME_NETWORK_VIEW))
        {
            adw_status_page_set_icon_name (status_page, "network-computer-symbolic");
            adw_status_page_set_title (status_page, _("No Known Connections"));
            adw_status_page_set_description (status_page, _("Enter an address to connect to a network location."));
        }
        else
        {
            adw_status_page_set_icon_name (status_page, "folder-symbolic");
            adw_status_page_set_title (status_page, _("Folder is Empty"));
            adw_status_page_set_description (status_page, NULL);
        }

        gtk_widget_set_visible (priv->empty_view_page, TRUE);

        nautilus_files_view_display_selection_info (view);
    }
    else
    {
        gtk_widget_set_visible (priv->empty_view_page, FALSE);
    }
}

static void
done_loading (NautilusFilesView *view,
              gboolean           all_files_seen)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (!priv->loading)
    {
        return;
    }

    if (!priv->in_destruction)
    {
        g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
        gboolean no_selection = gtk_bitset_is_empty (selection);

        remove_loading_floating_bar (view);
        schedule_update_context_menus (view);
        schedule_update_status (view);
        nautilus_files_view_update_toolbar_menus (view);
        reset_update_interval (view);

        if (nautilus_view_is_searching (NAUTILUS_VIEW (view)) &&
            all_files_seen && no_selection && priv->pending_selection == NULL)
        {
            nautilus_files_view_select_first (view);
        }
        else if (priv->pending_selection != NULL && all_files_seen)
        {
            g_autolist (NautilusFile) pending_selection = NULL;
            pending_selection = g_steal_pointer (&priv->pending_selection);

            nautilus_files_view_call_set_selection (view, pending_selection);
        }

        g_clear_pointer (&priv->pending_selection, nautilus_file_list_free);

        nautilus_files_view_display_selection_info (view);
    }

    priv->loading = FALSE;
    g_clear_handle_id (&priv->search_transition_timeout_id, g_source_remove);
    g_signal_emit (view, signals[END_LOADING], 0, all_files_seen);
    g_object_notify (G_OBJECT (view), "loading");

    if (!priv->in_destruction)
    {
        nautilus_files_view_check_empty_states (view);
    }
}


typedef struct
{
    GHashTable *debuting_files;
    GList *added_files;
} DebutingFilesData;

static void
debuting_files_data_free (DebutingFilesData *data)
{
    g_hash_table_unref (data->debuting_files);
    nautilus_file_list_free (data->added_files);
    g_free (data);
}

/* This signal handler watch for the arrival of the icons created
 * as the result of a file operation. Once the last one is detected
 * it selects and reveals them all.
 */
static void
debuting_files_add_files_callback (NautilusFilesView *view,
                                   GList             *new_files,
                                   DebutingFilesData *data)
{
    GFile *location;
    GList *l;

    for (l = new_files; l != NULL; l = l->next)
    {
        location = nautilus_file_get_location (NAUTILUS_FILE (l->data));

        if (g_hash_table_remove (data->debuting_files, location))
        {
            nautilus_file_ref (NAUTILUS_FILE (l->data));
            data->added_files = g_list_prepend (data->added_files, NAUTILUS_FILE (l->data));
        }
        g_object_unref (location);
    }

    if (g_hash_table_size (data->debuting_files) == 0)
    {
        nautilus_files_view_call_set_selection (view, data->added_files);
        g_signal_handlers_disconnect_by_func (view,
                                              G_CALLBACK (debuting_files_add_files_callback),
                                              data);
    }
}

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
    g_assert (data != NULL);

    if (data->directory_view != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
                                      (gpointer *) &data->directory_view);
    }

    nautilus_file_list_free (data->added_files);
    g_free (data);
}

static void
pre_copy_move_add_files_callback (NautilusFilesView *view,
                                  GList             *new_files,
                                  CopyMoveDoneData  *data)
{
    GList *l;

    for (l = new_files; l != NULL; l = l->next)
    {
        nautilus_file_ref (NAUTILUS_FILE (l->data));
        data->added_files = g_list_prepend (data->added_files, l->data);
    }
}

/* This needs to be called prior to nautilus_file_operations_copy_move.
 * It hooks up a signal handler to catch any icons that get added before
 * the copy_done_callback is invoked. The return value should  be passed
 * as the data for uri_copy_move_done_callback.
 */
static CopyMoveDoneData *
pre_copy_move (NautilusFilesView *directory_view)
{
    CopyMoveDoneData *copy_move_done_data;

    copy_move_done_data = g_new0 (CopyMoveDoneData, 1);
    copy_move_done_data->directory_view = directory_view;

    g_object_add_weak_pointer (G_OBJECT (copy_move_done_data->directory_view),
                               (gpointer *) &copy_move_done_data->directory_view);

    /* We need to run after the default handler adds the folder we want to
     * operate on. The ADD_FILES signal is registered as G_SIGNAL_RUN_LAST, so we
     * must use connect_after.
     */
    g_signal_connect_after (directory_view, "add-files",
                            G_CALLBACK (pre_copy_move_add_files_callback), copy_move_done_data);

    return copy_move_done_data;
}

/* This function is used to pull out any debuting uris that were added
 * and (as a side effect) remove them from the debuting uri hash table.
 */
static gboolean
copy_move_done_partition_func (NautilusFile *file,
                               gpointer      callback_data)
{
    GFile *location;
    gboolean result;

    location = nautilus_file_get_location (file);
    result = g_hash_table_remove ((GHashTable *) callback_data, location);
    g_object_unref (location);

    return result;
}

static gboolean
remove_not_really_moved_files (gpointer key,
                               gpointer value,
                               gpointer callback_data)
{
    GList **added_files;
    GFile *loc;

    loc = key;

    if (GPOINTER_TO_INT (value))
    {
        return FALSE;
    }

    added_files = callback_data;
    *added_files = g_list_prepend (*added_files,
                                   nautilus_file_get (loc));
    return TRUE;
}

/* When this function is invoked, the file operation is over, but all
 * the icons may not have been added to the directory view yet, so
 * we can't select them yet.
 *
 * We're passed a hash table of the uri's to look out for, we hook
 * up a signal handler to await their arrival.
 */
static void
copy_move_done_callback (GHashTable *debuting_files,
                         gboolean    success,
                         gpointer    data)
{
    NautilusFilesView *directory_view;
    CopyMoveDoneData *copy_move_done_data;
    DebutingFilesData *debuting_files_data;
    GList *failed_files;

    copy_move_done_data = (CopyMoveDoneData *) data;
    directory_view = copy_move_done_data->directory_view;

    if (directory_view != NULL)
    {
        g_assert (NAUTILUS_IS_FILES_VIEW (directory_view));

        debuting_files_data = g_new (DebutingFilesData, 1);
        debuting_files_data->debuting_files = g_hash_table_ref (debuting_files);
        debuting_files_data->added_files = nautilus_file_list_filter (copy_move_done_data->added_files,
                                                                      &failed_files,
                                                                      copy_move_done_partition_func,
                                                                      debuting_files);
        nautilus_file_list_free (copy_move_done_data->added_files);
        copy_move_done_data->added_files = failed_files;

        /* We're passed the same data used by pre_copy_move_add_files_callback, so disconnecting
         * it will free data. We've already siphoned off the added_files we need, and stashed the
         * directory_view pointer.
         */
        g_signal_handlers_disconnect_by_func (directory_view,
                                              G_CALLBACK (pre_copy_move_add_files_callback),
                                              data);

        /* Any items in the debuting_files hash table that have
         * "FALSE" as their value aren't really being copied
         * or moved, so we can't wait for an add_files signal
         * to come in for those.
         */
        g_hash_table_foreach_remove (debuting_files,
                                     remove_not_really_moved_files,
                                     &debuting_files_data->added_files);

        if (g_hash_table_size (debuting_files) == 0)
        {
            /* on the off-chance that all the icons have already been added */
            if (debuting_files_data->added_files != NULL)
            {
                nautilus_files_view_call_set_selection (directory_view,
                                                        debuting_files_data->added_files);
            }
            debuting_files_data_free (debuting_files_data);
        }
        else
        {
            /* We need to run after the default handler adds the folder we want to
             * operate on. The ADD_FILES signal is registered as G_SIGNAL_RUN_LAST, so we
             * must use connect_after.
             */
            g_signal_connect_data (directory_view,
                                   "add-files",
                                   G_CALLBACK (debuting_files_add_files_callback),
                                   debuting_files_data,
                                   (GClosureNotify) debuting_files_data_free,
                                   G_CONNECT_AFTER);
        }
        /* Schedule menu update for undo items */
        schedule_update_context_menus (directory_view);
    }

    copy_move_done_data_free (copy_move_done_data);
}

static gboolean
view_file_still_belongs (NautilusFilesView *view,
                         FileAndDirectory  *fad)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory != fad->directory &&
        g_list_find (priv->subdirectory_list, fad->directory) == NULL)
    {
        return FALSE;
    }

    return nautilus_directory_contains_file (fad->directory, fad->file);
}

static gboolean
still_should_show_file (NautilusFilesView *view,
                        FileAndDirectory  *fad)
{
    return nautilus_files_view_should_show_file (view, fad->file) &&
           view_file_still_belongs (view, fad);
}

static void
real_end_file_changes (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    nautilus_view_model_sort (priv->model);

    /* Addition and removal of files modify the empty state */
    nautilus_files_view_check_empty_states (view);
    /* If the view is empty, zoom slider and sort menu are insensitive */
    nautilus_files_view_update_toolbar_menus (view);

    /* Reveal files that were pending to be revealed, only if all of them
     * were acknowledged by the view
     */
    if (g_hash_table_size (priv->pending_reveal) > 0)
    {
        GList *keys;
        GList *l;
        gboolean all_files_acknowledged = TRUE;

        keys = g_hash_table_get_keys (priv->pending_reveal);
        for (l = keys; l && all_files_acknowledged; l = l->next)
        {
            all_files_acknowledged = GPOINTER_TO_UINT (g_hash_table_lookup (priv->pending_reveal,
                                                                            l->data));
        }

        if (all_files_acknowledged)
        {
            nautilus_files_view_set_selection (NAUTILUS_VIEW (view), keys);
            g_hash_table_remove_all (priv->pending_reveal);
        }

        g_list_free (keys);
    }
}

static int
compare_pointers (gconstpointer pointer_1,
                  gconstpointer pointer_2)
{
    if (pointer_1 < pointer_2)
    {
        return -1;
    }
    else if (pointer_1 > pointer_2)
    {
        return +1;
    }

    return 0;
}

static gboolean
_g_lists_sort_and_check_for_intersection (GList **list_1,
                                          GList **list_2)
{
    GList *node_1;
    GList *node_2;
    int compare_result;

    *list_1 = g_list_sort (*list_1, compare_pointers);
    *list_2 = g_list_sort (*list_2, compare_pointers);

    node_1 = *list_1;
    node_2 = *list_2;

    while (node_1 != NULL && node_2 != NULL)
    {
        compare_result = compare_pointers (node_1->data, node_2->data);
        if (compare_result == 0)
        {
            return TRUE;
        }
        if (compare_result <= 0)
        {
            node_1 = node_1->next;
        }
        if (compare_result >= 0)
        {
            node_2 = node_2->next;
        }
    }

    return FALSE;
}

static void
real_add_files (NautilusFilesView *self,
                GList             *files)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    g_autolist (NautilusViewItem) items = NULL;

    items = g_list_copy_deep (files, (GCopyFunc) nautilus_view_item_new, NULL);
    nautilus_view_model_add_items (priv->model, items);
}

static void
real_remove_files (NautilusFilesView *self,
                   GList             *files,
                   NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    g_autoptr (GList) items = NULL;

    for (GList *l = files; l != NULL; l = l->next)
    {
        NautilusViewItem *item;

        item = nautilus_view_model_get_item_for_file (priv->model, l->data);
        if (item != NULL)
        {
            items = g_list_prepend (items, item);
        }
    }

    if (items != NULL)
    {
        nautilus_view_model_remove_items (priv->model, items, directory);
    }
}

static void
real_file_changed (NautilusFilesView *self,
                   NautilusFile      *file,
                   NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    g_autoptr (NautilusFile) directory_as_file = NULL;
    NautilusViewItem *item;

    directory_as_file = nautilus_directory_get_corresponding_file (directory);
    if (file == directory_as_file)
    {
        /* We don't care about changes to the current directory itself here, so
         * silently ignore it. This happens only with self-owned files.*/
        return;
    }

    item = nautilus_view_model_get_item_for_file (priv->model, file);
    if (item != NULL)
    {
        nautilus_view_item_file_changed (item);
    }
    else
    {
        /* When a file that was hidden is not hidden anymore (e.g. undoing the
         * rename operation which made it hidden), we get a change notification
         * for a file that's not in our model. Let's add it then. */
        real_add_files (self, &(GList){ .data = file });
    }
}

static void
process_pending_files (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    g_autolist (FileAndDirectory) files_added = NULL;
    g_autolist (FileAndDirectory) files_changed = NULL;
    FileAndDirectory *pending;
    GList *files;
    g_autoptr (GList) pending_additions = NULL;

    priv = nautilus_files_view_get_instance_private (view);
    files_added = g_steal_pointer (&priv->new_added_files);
    files_changed = g_steal_pointer (&priv->new_changed_files);


    if (files_added != NULL || files_changed != NULL)
    {
        g_autoptr (GHashTable) files_removed = g_hash_table_new (NULL, NULL);
        gboolean send_selection_change = FALSE;

        g_signal_emit (view, signals[BEGIN_FILE_CHANGES], 0);

        for (GList *node = files_added; node != NULL; node = node->next)
        {
            pending = node->data;
            if (nautilus_file_is_gone (pending->file))
            {
                if (g_getenv ("G_MESSAGES_DEBUG") == NULL)
                {
                    g_warning ("Attempted to add a non-existent file to the view.");
                }
                else
                {
                    g_autofree char *uri = nautilus_file_get_uri (pending->file);
                    g_warning ("Attempted to add non-existent file \"%s\" to the view.", uri);
                }

                continue;
            }
            if (!nautilus_files_view_should_show_file (view, pending->file))
            {
                continue;
            }
            pending_additions = g_list_prepend (pending_additions, pending->file);
            /* Acknowledge the files that were pending to be revealed */
            if (g_hash_table_contains (priv->pending_reveal, pending->file))
            {
                g_hash_table_insert (priv->pending_reveal,
                                     pending->file,
                                     GUINT_TO_POINTER (TRUE));
            }
        }
        pending_additions = g_list_reverse (pending_additions);

        if (files_added != NULL)
        {
            g_signal_emit (view,
                           signals[ADD_FILES], 0, pending_additions);
        }

        for (GList *node = files_changed; node != NULL; node = node->next)
        {
            gboolean should_show_file;
            pending = node->data;
            should_show_file = still_should_show_file (view, pending);
            if (should_show_file)
            {
                g_signal_emit (view,
                               signals[FILE_CHANGED], 0, pending->file, pending->directory);
            }
            else
            {
                files = g_hash_table_lookup (files_removed, pending->directory);
                g_hash_table_insert (files_removed,
                                     pending->directory,
                                     g_list_prepend (files, pending->file));
            }

            /* Acknowledge the files that were pending to be revealed */
            if (g_hash_table_contains (priv->pending_reveal, pending->file))
            {
                if (should_show_file)
                {
                    g_hash_table_insert (priv->pending_reveal,
                                         pending->file,
                                         GUINT_TO_POINTER (TRUE));
                }
                else
                {
                    g_hash_table_remove (priv->pending_reveal,
                                         pending->file);
                }
            }
        }

        if (files_removed != NULL)
        {
            GHashTableIter iter;
            gpointer directory;

            g_hash_table_iter_init (&iter, files_removed);
            while (g_hash_table_iter_next (&iter, &directory, (gpointer *) &files))
            {
                g_signal_emit (view, signals[REMOVE_FILES], 0, files, directory);
                g_list_free (files);
                g_hash_table_iter_steal (&iter);
            }
        }
        if (files_changed != NULL)
        {
            g_autolist (NautilusFile) selection = NULL;
            selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
            files = g_list_copy_deep (files_changed, (GCopyFunc) file_and_directory_get_file, NULL);
            send_selection_change = _g_lists_sort_and_check_for_intersection
                                        (&files, &selection);
            nautilus_file_list_free (files);
        }

        if (send_selection_change)
        {
            /* Send a selection change since some file names could
             * have changed.
             */
            nautilus_files_view_send_selection_change (view);
        }

        g_signal_emit (view, signals[END_FILE_CHANGES], 0);
    }
}

static void
display_pending_files (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (GtkBitset) selection = NULL;
    gboolean no_selection = FALSE;

    search_transition_emit_delayed_signals_if_pending (view);

    selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    no_selection = gtk_bitset_is_empty (selection);

    process_pending_files (view);

    if (no_selection &&
        !priv->pending_selection &&
        nautilus_view_is_searching (NAUTILUS_VIEW (view)))
    {
        nautilus_files_view_select_first (view);
    }

    if (priv->model != NULL
        && nautilus_directory_are_all_files_seen (priv->directory))
    {
        done_loading (view, TRUE);
    }
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (data);
    priv = nautilus_files_view_get_instance_private (view);

    g_object_ref (G_OBJECT (view));

    priv->display_selection_idle_id = 0;
    nautilus_files_view_display_selection_info (view);
    nautilus_files_view_send_selection_change (view);

    g_object_unref (G_OBJECT (view));

    return FALSE;
}

static void
remove_update_context_menus_timeout_callback (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->update_context_menus_timeout_id != 0)
    {
        g_source_remove (priv->update_context_menus_timeout_id);
        priv->update_context_menus_timeout_id = 0;
    }
}

static void
update_context_menus_if_pending (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    if (priv->update_context_menus_timeout_id != 0)
    {
        remove_update_context_menus_timeout_callback (view);
        nautilus_files_view_update_context_menus (view);
    }
}

static gboolean
update_context_menus_timeout_callback (gpointer data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (data);
    priv = nautilus_files_view_get_instance_private (view);

    g_object_ref (G_OBJECT (view));

    priv->update_context_menus_timeout_id = 0;
    nautilus_files_view_update_context_menus (view);

    g_object_unref (G_OBJECT (view));

    return FALSE;
}

static gboolean
display_pending_callback (gpointer data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (data);
    priv = nautilus_files_view_get_instance_private (view);

    g_object_ref (G_OBJECT (view));

    priv->display_pending_source_id = 0;

    display_pending_files (view);

    g_object_unref (G_OBJECT (view));

    return FALSE;
}

static void
schedule_idle_display_of_pending_files (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    /* Get rid of a pending source as it might be a timeout */
    unschedule_display_of_pending_files (view);

    /* We want higher priority than the idle that handles the relayout
     *  to avoid a resort on each add. But we still want to allow repaints
     *  and other hight prio events while we have pending files to show. */
    priv->display_pending_source_id =
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
                         display_pending_callback, view, NULL);
}

static void
schedule_timeout_display_of_pending_files (NautilusFilesView *view,
                                           guint              interval)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    /* No need to schedule an update if there's already one pending. */
    if (priv->display_pending_source_id != 0)
    {
        return;
    }

    priv->display_pending_source_id =
        g_timeout_add (interval, display_pending_callback, view);
}

static void
unschedule_display_of_pending_files (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    /* Get rid of source if it's active. */
    if (priv->display_pending_source_id != 0)
    {
        g_source_remove (priv->display_pending_source_id);
        priv->display_pending_source_id = 0;
    }
}

static void
queue_pending_files (NautilusFilesView  *view,
                     NautilusDirectory  *directory,
                     GList              *files,
                     GList             **pending_list)
{
    NautilusFilesViewPrivate *priv;
    GList *fad_list;

    priv = nautilus_files_view_get_instance_private (view);

    if (files == NULL)
    {
        return;
    }

    fad_list = g_list_copy_deep (files, (GCopyFunc) file_and_directory_new, directory);
    *pending_list = g_list_concat (fad_list, *pending_list);
    /* Generally we don't want to show the files while the directory is loading
     * the files themselves, so we avoid jumping and oddities. However, for
     * search it can be a long wait, and we actually want to show files as
     * they are getting found. So for search is fine if not all files are
     * seen */
    if ((!priv->loading && priv->subdirectories_loading == NULL) ||
        (nautilus_directory_are_all_files_seen (directory) ||
         nautilus_view_is_searching (NAUTILUS_VIEW (view))))
    {
        schedule_timeout_display_of_pending_files (view, priv->update_interval);
    }
}

static void
remove_changes_timeout_callback (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->changes_timeout_id != 0)
    {
        g_source_remove (priv->changes_timeout_id);
        priv->changes_timeout_id = 0;
    }
}

static void
reset_update_interval (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    priv->update_interval = UPDATE_INTERVAL_MIN;
    remove_changes_timeout_callback (view);
    /* Reschedule a pending timeout to idle */
    if (priv->display_pending_source_id != 0)
    {
        schedule_idle_display_of_pending_files (view);
    }
}

static gboolean
changes_timeout_callback (gpointer data)
{
    gint64 now;
    gint64 time_delta;
    gboolean ret;
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (data);
    priv = nautilus_files_view_get_instance_private (view);

    g_object_ref (G_OBJECT (view));

    now = g_get_monotonic_time ();
    time_delta = now - priv->last_queued;

    if (time_delta < UPDATE_INTERVAL_RESET * 1000)
    {
        if (priv->update_interval < UPDATE_INTERVAL_MAX &&
            priv->loading)
        {
            /* Increase */
            priv->update_interval += UPDATE_INTERVAL_INC;
        }
        ret = TRUE;
    }
    else
    {
        /* Reset */
        reset_update_interval (view);
        ret = FALSE;
    }

    g_object_unref (G_OBJECT (view));

    return ret;
}

static void
schedule_changes (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);
    /* Remember when the change was queued */
    priv->last_queued = g_get_monotonic_time ();

    /* No need to schedule if there are already changes pending or during loading */
    if (priv->changes_timeout_id != 0 ||
        priv->loading)
    {
        return;
    }

    priv->changes_timeout_id =
        g_timeout_add (UPDATE_INTERVAL_TIMEOUT_INTERVAL, changes_timeout_callback, view);
}

static void
files_added_callback (NautilusDirectory *directory,
                      GList             *files,
                      gpointer           callback_data)
{
    NautilusFilesViewPrivate *priv;
    NautilusFilesView *view;
    GtkWindow *window;
    char *uri;

    view = NAUTILUS_FILES_VIEW (callback_data);
    priv = nautilus_files_view_get_instance_private (view);

    window = nautilus_files_view_get_containing_window (view);
    uri = nautilus_files_view_get_uri (view);

    g_debug ("Files added in window %p: %s", window, uri ? uri : "(no directory)");
    nautilus_file_list_debug (files);

    g_free (uri);

    schedule_changes (view);

    queue_pending_files (view, directory, files, &priv->new_added_files);

    /* The number of items could have changed */
    schedule_update_status (view);
}

static void
files_changed_callback (NautilusDirectory *directory,
                        GList             *files,
                        gpointer           callback_data)
{
    NautilusFilesViewPrivate *priv;
    NautilusFilesView *view;
    GtkWindow *window;
    char *uri;

    view = NAUTILUS_FILES_VIEW (callback_data);
    priv = nautilus_files_view_get_instance_private (view);

    window = nautilus_files_view_get_containing_window (view);
    uri = nautilus_files_view_get_uri (view);
    g_debug ("Files changed in window (%p) %s", window, uri ? uri : "(no directory)");
    nautilus_file_list_debug (files);

    g_free (uri);

    schedule_changes (view);

    queue_pending_files (view, directory, files, &priv->new_changed_files);

    /* The free space or the number of items could have changed */
    schedule_update_status (view);

    /* A change in MIME type could affect the Open with menu, for
     * one thing, so we need to update menus when files change.
     */
    schedule_update_context_menus (view);
}

static void
done_loading_callback (NautilusDirectory *directory,
                       gpointer           callback_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (callback_data);

    /* Unschedule a pending update and schedule a new one with the minimal
     * update interval. This gives the view a short chance at gathering the
     * (cached) deep counts.
     */
    unschedule_display_of_pending_files (view);
    schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);

    remove_loading_floating_bar (view);
}

static void
load_error_callback (NautilusDirectory *directory,
                     GError            *error,
                     gpointer           callback_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (callback_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    /* FIXME: By doing a stop, we discard some pending files. Is
     * that OK?
     */
    nautilus_files_view_stop_loading (view);

    nautilus_report_error_loading_directory (priv->directory_as_file,
                                             error,
                                             nautilus_files_view_get_containing_window (view));
}

gboolean
nautilus_files_view_has_subdirectory (NautilusFilesView *view,
                                      NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;
    priv = nautilus_files_view_get_instance_private (view);

    return g_list_find (priv->subdirectory_list, directory) != NULL;
}

static void
subdirectory_done_loading (NautilusDirectory *directory,
                           gpointer           user_data)
{
    NautilusFilesView *view = user_data;
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (NautilusFile) file = nautilus_directory_get_corresponding_file (directory);
    NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

    priv->subdirectories_loading = g_list_remove (priv->subdirectories_loading, directory);

    if (item != NULL)
    {
        nautilus_view_item_set_loading (item, FALSE);
    }
}

void
nautilus_files_view_add_subdirectory (NautilusFilesView *view,
                                      NautilusDirectory *directory)
{
    NautilusFileAttributes attributes;
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (NautilusFile) file = nautilus_directory_get_corresponding_file (directory);
    NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

    g_return_if_fail (!g_list_find (priv->subdirectory_list, directory));

    nautilus_directory_ref (directory);

    attributes =
        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
        NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_MOUNT |
        NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

    nautilus_directory_file_monitor_add (directory,
                                         &priv->directory,
                                         priv->show_hidden_files,
                                         attributes,
                                         files_added_callback, view);

    g_signal_connect
        (directory, "files-added",
        G_CALLBACK (files_added_callback), view);
    g_signal_connect
        (directory, "files-changed",
        G_CALLBACK (files_changed_callback), view);
    g_signal_connect_object (directory, "done-loading",
                             G_CALLBACK (subdirectory_done_loading),
                             view, 0);

    priv->subdirectory_list = g_list_prepend (priv->subdirectory_list, directory);
    priv->subdirectories_loading = g_list_prepend (priv->subdirectories_loading, directory);

    if (item != NULL &&
        !nautilus_directory_are_all_files_seen (directory))
    {
        nautilus_view_item_set_loading (item, TRUE);
    }
}

void
nautilus_files_view_remove_subdirectory (NautilusFilesView *view,
                                         NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autoptr (NautilusFile) file = nautilus_directory_get_corresponding_file (directory);
    NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

    g_return_if_fail (g_list_find (priv->subdirectory_list, directory));

    priv->subdirectory_list = g_list_remove (priv->subdirectory_list, directory);
    priv->subdirectories_loading = g_list_remove (priv->subdirectories_loading, directory);

    if (item != NULL)
    {
        nautilus_view_item_set_loading (item, FALSE);

        /* The model holds a GListStore for every subdirectory. Empty it. */
        nautilus_view_model_clear_subdirectory (priv->model, item);
    }

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (files_added_callback),
                                          view);
    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (files_changed_callback),
                                          view);
    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (subdirectory_done_loading),
                                          view);

    nautilus_directory_file_monitor_remove (directory, &priv->directory);

    nautilus_directory_unref (directory);
}

/**
 * nautilus_files_view_get_loading:
 * @view: an #NautilusFilesView.
 *
 * Return value: #gboolean inicating whether @view is currently loaded.
 *
 **/
gboolean
nautilus_files_view_get_loading (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

    priv = nautilus_files_view_get_instance_private (view);

    return priv->loading;
}

/* home_dir_in_selection()
 *
 * Return TRUE if the home directory is in the selection.
 */

static gboolean
home_dir_in_selection (GList *selection)
{
    for (GList *node = selection; node != NULL; node = node->next)
    {
        if (nautilus_file_is_home (NAUTILUS_FILE (node->data)))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
trash_or_delete_done_cb (GHashTable        *debuting_uris,
                         gboolean           user_cancel,
                         NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);
    if (user_cancel)
    {
        priv->selection_was_removed = FALSE;
    }
}

static void
trash_or_delete_files (GtkWindow         *parent_window,
                       const GList       *files,
                       NautilusFilesView *view)
{
    GList *locations;
    const GList *node;

    locations = NULL;
    for (node = files; node != NULL; node = node->next)
    {
        locations = g_list_prepend (locations,
                                    nautilus_file_get_location ((NautilusFile *) node->data));
    }

    locations = g_list_reverse (locations);

    nautilus_file_operations_trash_or_delete_async (locations,
                                                    parent_window,
                                                    NULL,
                                                    (NautilusDeleteCallback) trash_or_delete_done_cb,
                                                    view);
    g_list_free_full (locations, g_object_unref);
}

static GdkTexture *
get_menu_icon_for_file (NautilusFile *file,
                        GtkWidget    *widget)
{
    int scale = gtk_widget_get_scale_factor (widget);

    return nautilus_file_get_icon_texture (file, 16, scale, 0);
}

static GList *
get_extension_selection_menu_items (NautilusFilesView *view)
{
    GList *items;
    GList *providers;
    GList *l;
    g_autolist (NautilusFile) selection = NULL;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
    items = NULL;

    for (l = providers; l != NULL; l = l->next)
    {
        NautilusMenuProvider *provider;
        GList *file_items;

        provider = NAUTILUS_MENU_PROVIDER (l->data);
        file_items = nautilus_menu_provider_get_file_items (provider,
                                                            selection);
        items = g_list_concat (items, file_items);
    }

    nautilus_module_extension_list_free (providers);

    return items;
}

static GList *
get_extension_background_menu_items (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    GList *items;
    GList *providers;
    GList *l;

    priv = nautilus_files_view_get_instance_private (view);
    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
    items = NULL;

    for (l = providers; l != NULL; l = l->next)
    {
        NautilusMenuProvider *provider;
        NautilusFileInfo *file_info;
        GList *file_items;

        provider = NAUTILUS_MENU_PROVIDER (l->data);
        file_info = NAUTILUS_FILE_INFO (priv->directory_as_file);
        file_items = nautilus_menu_provider_get_background_items (provider,
                                                                  file_info);
        items = g_list_concat (items, file_items);
    }

    nautilus_module_extension_list_free (providers);

    return items;
}

static void
extension_action_callback (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    NautilusMenuItem *item = user_data;
    nautilus_menu_item_activate (item);
}

static void
add_extension_action (NautilusFilesView *view,
                      NautilusMenuItem  *item,
                      const char        *action_name)
{
    NautilusFilesViewPrivate *priv;
    gboolean sensitive;
    GSimpleAction *action;

    priv = nautilus_files_view_get_instance_private (view);

    g_object_get (item,
                  "sensitive", &sensitive,
                  NULL);

    action = g_simple_action_new (action_name, NULL);
    g_signal_connect_data (action, "activate",
                           G_CALLBACK (extension_action_callback),
                           g_object_ref (item),
                           (GClosureNotify) g_object_unref, 0);

    g_action_map_add_action (G_ACTION_MAP (priv->view_action_group),
                             G_ACTION (action));
    g_simple_action_set_enabled (action, sensitive);

    g_object_unref (action);
}

static GMenuModel *
build_menu_for_extension_menu_items (NautilusFilesView *view,
                                     const gchar       *extension_prefix,
                                     GList             *menu_items)
{
    GList *l;
    GMenu *gmenu;
    gint idx = 0;

    gmenu = g_menu_new ();

    for (l = menu_items; l; l = l->next)
    {
        NautilusMenuItem *item;
        NautilusMenu *menu;
        GMenuItem *menu_item;
        char *name, *label;
        g_autofree gchar *escaped_name = NULL;
        char *extension_id, *detailed_action_name;

        item = NAUTILUS_MENU_ITEM (l->data);

        g_object_get (item,
                      "label", &label,
                      "menu", &menu,
                      "name", &name,
                      NULL);

        escaped_name = g_uri_escape_string (name, NULL, TRUE);
        extension_id = g_strdup_printf ("extension_%s_%d_%s",
                                        extension_prefix, idx, escaped_name);
        add_extension_action (view, item, extension_id);

        detailed_action_name = g_strconcat ("view.", extension_id, NULL);
        menu_item = g_menu_item_new (label, detailed_action_name);

        if (menu != NULL)
        {
            GList *children;
            g_autoptr (GMenuModel) children_menu = NULL;

            children = nautilus_menu_get_items (menu);
            children_menu = build_menu_for_extension_menu_items (view, extension_id, children);
            g_menu_item_set_submenu (menu_item, children_menu);

            nautilus_menu_item_list_free (children);
        }

        g_menu_append_item (gmenu, menu_item);
        idx++;

        g_free (extension_id);
        g_free (detailed_action_name);
        g_free (name);
        g_free (label);
        g_object_unref (menu_item);
    }

    return G_MENU_MODEL (gmenu);
}

static void
update_extensions_menus (NautilusFilesView *view,
                         GtkBuilder        *builder)
{
    GList *selection_items, *background_items;
    GObject *object;
    g_autoptr (GMenuModel) background_menu = NULL;
    g_autoptr (GMenuModel) selection_menu = NULL;

    selection_items = get_extension_selection_menu_items (view);
    if (selection_items != NULL)
    {
        selection_menu = build_menu_for_extension_menu_items (view, "extensions",
                                                              selection_items);

        object = gtk_builder_get_object (builder, "selection-extensions-section");
        nautilus_gmenu_set_from_model (G_MENU (object), selection_menu);

        nautilus_menu_item_list_free (selection_items);
    }

    background_items = get_extension_background_menu_items (view);
    if (background_items != NULL)
    {
        background_menu = build_menu_for_extension_menu_items (view, "extensions",
                                                               background_items);

        object = gtk_builder_get_object (builder, "background-extensions-section");
        nautilus_gmenu_set_from_model (G_MENU (object), background_menu);

        nautilus_menu_item_list_free (background_items);
    }

    nautilus_view_set_extensions_background_menu (NAUTILUS_VIEW (view), background_menu);
}

static char *
change_to_view_directory (NautilusFilesView *view)
{
    char *path;
    char *old_path;

    old_path = g_get_current_dir ();

    path = get_view_directory (view);

    /* FIXME: What to do about non-local directories? */
    if (path != NULL)
    {
        g_chdir (path);
    }

    g_free (path);

    return old_path;
}

static char **
get_file_names_as_parameter_array (GList             *selection,
                                   NautilusDirectory *directory)
{
    char **parameters;
    g_autoptr (GFile) directory_location = NULL;
    int i;

    if (directory == NULL)
    {
        return NULL;
    }

    parameters = g_new (char *, g_list_length (selection) + 1);

    directory_location = nautilus_directory_get_location (directory);

    i = 0;
    for (GList *node = selection; node != NULL; node = node->next, i++)
    {
        g_autoptr (GFile) file_location = NULL;
        NautilusFile *file = NAUTILUS_FILE (node->data);

        if (!nautilus_file_has_local_path (file))
        {
            parameters[i] = NULL;
            g_strfreev (parameters);
            return NULL;
        }

        file_location = nautilus_file_get_location (file);
        parameters[i] = g_file_get_relative_path (directory_location, file_location);
        if (parameters[i] == NULL)
        {
            parameters[i] = g_file_get_path (file_location);
        }
    }

    parameters[i] = NULL;
    return parameters;
}

static char *
get_file_paths_or_uris_as_newline_delimited_string (GList    *selection,
                                                    gboolean  get_paths)
{
    GString *expanding_string;

    expanding_string = g_string_new ("");
    for (GList *node = selection; node != NULL; node = node->next)
    {
        NautilusFile *file = NAUTILUS_FILE (node->data);
        g_autofree gchar *uri = NULL;

        uri = nautilus_file_get_uri (file);
        if (uri == NULL)
        {
            continue;
        }

        if (get_paths)
        {
            g_autofree gchar *path = NULL;

            if (!nautilus_file_has_local_path (file))
            {
                g_string_free (expanding_string, TRUE);
                return g_strdup ("");
            }

            path = g_filename_from_uri (uri, NULL, NULL);
            if (path != NULL)
            {
                g_string_append (expanding_string, path);
                g_string_append (expanding_string, "\n");
            }
        }
        else
        {
            g_string_append (expanding_string, uri);
            g_string_append (expanding_string, "\n");
        }
    }

    return g_string_free (expanding_string, FALSE);
}

static char *
get_file_paths_as_newline_delimited_string (GList *selection)
{
    return get_file_paths_or_uris_as_newline_delimited_string (selection, TRUE);
}

static char *
get_file_uris_as_newline_delimited_string (GList *selection)
{
    return get_file_paths_or_uris_as_newline_delimited_string (selection, FALSE);
}

/*
 * Set up some environment variables that scripts can use
 * to take advantage of the current Nautilus state.
 */
static void
set_script_environment_variables (NautilusFilesView *view,
                                  GList             *selected_files)
{
    g_autofree gchar *file_paths = NULL;
    g_autofree gchar *uris = NULL;
    g_autofree gchar *uri = NULL;
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    file_paths = get_file_paths_as_newline_delimited_string (selected_files);
    g_setenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);

    uris = get_file_uris_as_newline_delimited_string (selected_files);
    g_setenv ("NAUTILUS_SCRIPT_SELECTED_URIS", uris, TRUE);

    uri = nautilus_directory_get_uri (priv->directory);
    g_setenv ("NAUTILUS_SCRIPT_CURRENT_URI", uri, TRUE);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
    g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS");
    g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_URIS");
    g_unsetenv ("NAUTILUS_SCRIPT_CURRENT_URI");
}

static void
run_script (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
    ScriptLaunchParameters *launch_parameters;
    NautilusFilesViewPrivate *priv;
    g_autofree gchar *file_uri = NULL;
    g_autofree gchar *local_file_path = NULL;
    g_autofree gchar *quoted_path = NULL;
    g_autofree gchar *old_working_dir = NULL;
    g_autolist (NautilusFile) selection = NULL;
    g_auto (GStrv) parameters = NULL;
    GdkDisplay *display;

    launch_parameters = (ScriptLaunchParameters *) user_data;
    priv = nautilus_files_view_get_instance_private (launch_parameters->directory_view);

    file_uri = nautilus_file_get_uri (launch_parameters->file);
    local_file_path = g_filename_from_uri (file_uri, NULL, NULL);
    g_assert (local_file_path != NULL);
    quoted_path = g_shell_quote (local_file_path);

    old_working_dir = change_to_view_directory (launch_parameters->directory_view);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (launch_parameters->directory_view));
    set_script_environment_variables (launch_parameters->directory_view, selection);

    parameters = get_file_names_as_parameter_array (selection, priv->directory);

    display = gtk_widget_get_display (GTK_WIDGET (launch_parameters->directory_view));

    g_debug ("run_script, script_path=“%s” (omitting script parameters)", local_file_path);

    nautilus_launch_application_from_command_array (display, quoted_path, FALSE,
                                                    (const char * const *) parameters);

    unset_script_environment_variables ();
    g_chdir (old_working_dir);
}

static void
add_script_to_scripts_menus (NautilusFilesView *view,
                             NautilusFile      *file,
                             GMenu             *menu)
{
    NautilusFilesViewPrivate *priv;
    const gchar *name;
    g_autofree gchar *uri = NULL;
    g_autofree gchar *escaped_uri = NULL;
    GdkTexture *mimetype_icon;
    gchar *action_name, *detailed_action_name;
    ScriptLaunchParameters *launch_parameters;
    GAction *action;
    GMenuItem *menu_item;
    const gchar *shortcut;

    priv = nautilus_files_view_get_instance_private (view);
    launch_parameters = script_launch_parameters_new (file, view);

    name = nautilus_file_get_display_name (file);

    uri = nautilus_file_get_uri (file);
    escaped_uri = g_uri_escape_string (uri, NULL, TRUE);
    action_name = g_strconcat ("script_", escaped_uri, NULL);

    action = G_ACTION (g_simple_action_new (action_name, NULL));

    g_signal_connect_data (action, "activate",
                           G_CALLBACK (run_script),
                           launch_parameters,
                           (GClosureNotify) script_launch_parameters_free, 0);

    g_action_map_add_action (G_ACTION_MAP (priv->view_action_group), action);

    g_object_unref (action);

    detailed_action_name = g_strconcat ("view.", action_name, NULL);
    menu_item = g_menu_item_new (name, detailed_action_name);

    mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
    if (mimetype_icon != NULL)
    {
        g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
        g_object_unref (mimetype_icon);
    }

    g_menu_append_item (menu, menu_item);

    if ((shortcut = g_hash_table_lookup (script_accels, name)))
    {
        nautilus_application_set_accelerator (g_application_get_default (),
                                              detailed_action_name, shortcut);
    }

    g_free (action_name);
    g_free (detailed_action_name);
    g_object_unref (menu_item);
}

static gboolean
directory_belongs_in_scripts_menu (const char *uri)
{
    int num_levels;
    int i;

    if (!g_str_has_prefix (uri, scripts_directory_uri))
    {
        return FALSE;
    }

    num_levels = 0;
    for (i = scripts_directory_uri_length; uri[i] != '\0'; i++)
    {
        if (uri[i] == '/')
        {
            num_levels++;
        }
    }

    if (num_levels > MAX_MENU_LEVELS)
    {
        return FALSE;
    }

    return TRUE;
}

/* Expected format: accel script_name */
static void
nautilus_load_custom_accel_for_scripts (void)
{
    gchar *path, *contents;
    gchar **lines;
    GError *error = NULL;
    const int max_len = 100;

    path = g_build_filename (g_get_user_config_dir (), SHORTCUTS_PATH, NULL);

    if (g_file_get_contents (path, &contents, NULL, &error))
    {
        lines = g_strsplit (contents, "\n", -1);
        for (guint i = 0; lines[i] != NULL; i++)
        {
            g_auto (GStrv) result = g_strsplit (lines[i], " ", 2);

            if (result[0] == NULL || result[1] == NULL)
            {
                continue;
            }

            g_hash_table_insert (script_accels,
                                 g_strndup (result[1], max_len),
                                 g_strndup (result[0], max_len));
        }

        g_free (contents);
        g_strfreev (lines);
    }
    else
    {
        g_debug ("Unable to open '%s', error message: %s", path, error->message);
        g_clear_error (&error);
    }

    g_free (path);
}

static GMenu *
update_directory_in_scripts_menu (NautilusFilesView *view,
                                  NautilusDirectory *directory)
{
    GList *file_list, *filtered, *node;
    GMenu *menu, *children_menu;
    GMenuItem *menu_item;
    gboolean any_scripts;
    NautilusFile *file;
    NautilusDirectory *dir;
    char *uri;
    int num;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    if (script_accels == NULL)
    {
        script_accels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_free);
        nautilus_load_custom_accel_for_scripts ();
    }

    file_list = nautilus_directory_get_file_list (directory);
    filtered = nautilus_file_list_filter_hidden (file_list, FALSE);
    nautilus_file_list_free (file_list);
    menu = g_menu_new ();

    filtered = nautilus_file_list_sort_by_display_name (filtered);

    num = 0;
    any_scripts = FALSE;
    for (node = filtered; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++)
    {
        file = node->data;
        if (nautilus_file_is_directory (file))
        {
            uri = nautilus_file_get_uri (file);
            if (directory_belongs_in_scripts_menu (uri))
            {
                dir = nautilus_directory_get_by_uri (uri);
                add_directory_to_scripts_directory_list (view, dir);

                children_menu = update_directory_in_scripts_menu (view, dir);

                if (children_menu != NULL)
                {
                    const char *file_name = nautilus_file_get_display_name (file);
                    menu_item = g_menu_item_new_submenu (file_name,
                                                         G_MENU_MODEL (children_menu));
                    g_menu_append_item (menu, menu_item);
                    any_scripts = TRUE;
                    g_object_unref (menu_item);
                    g_object_unref (children_menu);
                }

                nautilus_directory_unref (dir);
            }
            g_free (uri);
        }
        else if (nautilus_file_is_launchable (file))
        {
            add_script_to_scripts_menus (view, file, menu);
            any_scripts = TRUE;
        }
    }

    nautilus_file_list_free (filtered);

    if (!any_scripts)
    {
        g_object_unref (menu);
        menu = NULL;
    }

    return menu;
}



static void
update_scripts_menu (NautilusFilesView *view,
                     GtkBuilder        *builder)
{
    NautilusFilesViewPrivate *priv;
    g_autolist (NautilusDirectory) sorted_copy = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (GMenu) submenu = NULL;

    priv = nautilus_files_view_get_instance_private (view);

    sorted_copy = nautilus_directory_list_sort_by_uri
                      (nautilus_directory_list_copy (priv->scripts_directory_list));

    for (GList *dir_l = sorted_copy; dir_l != NULL; dir_l = dir_l->next)
    {
        g_autofree char *uri = nautilus_directory_get_uri (dir_l->data);
        if (!directory_belongs_in_scripts_menu (uri))
        {
            remove_directory_from_scripts_directory_list (view, dir_l->data);
        }
    }

    directory = nautilus_directory_get_by_uri (scripts_directory_uri);
    submenu = update_directory_in_scripts_menu (view, directory);
    g_set_object (&priv->scripts_menu, G_MENU_MODEL (submenu));
}

static void
create_template (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
    CreateTemplateParameters *parameters;

    parameters = user_data;

    nautilus_files_view_new_file (parameters->directory_view, NULL, parameters->file);
}

static void
add_template_to_templates_menus (NautilusFilesView *view,
                                 NautilusFile      *file,
                                 GMenu             *menu)
{
    NautilusFilesViewPrivate *priv;
    char *uri;
    const char *name;
    g_autofree gchar *escaped_uri = NULL;
    GdkTexture *mimetype_icon;
    char *action_name, *detailed_action_name;
    CreateTemplateParameters *parameters;
    GAction *action;
    g_autofree char *label = NULL;
    GMenuItem *menu_item;

    priv = nautilus_files_view_get_instance_private (view);
    name = nautilus_file_get_display_name (file);
    uri = nautilus_file_get_uri (file);
    escaped_uri = g_uri_escape_string (uri, NULL, TRUE);
    action_name = g_strconcat ("template_", escaped_uri, NULL);
    action = G_ACTION (g_simple_action_new (action_name, NULL));
    parameters = create_template_parameters_new (file, view);

    g_signal_connect_data (action, "activate",
                           G_CALLBACK (create_template),
                           parameters,
                           (GClosureNotify) create_templates_parameters_free, 0);

    g_action_map_add_action (G_ACTION_MAP (priv->view_action_group), action);

    detailed_action_name = g_strconcat ("view.", action_name, NULL);
    label = escape_underscores (name);
    menu_item = g_menu_item_new (label, detailed_action_name);

    mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
    if (mimetype_icon != NULL)
    {
        g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
        g_object_unref (mimetype_icon);
    }

    g_menu_append_item (menu, menu_item);

    g_free (uri);
    g_free (action_name);
    g_free (detailed_action_name);
    g_object_unref (action);
    g_object_unref (menu_item);
}

static void
update_templates_directory (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    NautilusDirectory *templates_directory;
    GList *node, *next;
    char *templates_uri;

    priv = nautilus_files_view_get_instance_private (view);

    for (node = priv->templates_directory_list; node != NULL; node = next)
    {
        next = node->next;
        remove_directory_from_templates_directory_list (view, node->data);
    }

    if (nautilus_should_use_templates_directory ())
    {
        templates_uri = nautilus_get_templates_directory_uri ();
        templates_directory = nautilus_directory_get_by_uri (templates_uri);
        g_free (templates_uri);
        add_directory_to_templates_directory_list (view, templates_directory);
        nautilus_directory_unref (templates_directory);
    }
}

static gboolean
directory_belongs_in_templates_menu (const char *templates_directory_uri,
                                     const char *uri)
{
    int num_levels;
    int i;

    if (templates_directory_uri == NULL)
    {
        return FALSE;
    }

    if (!g_str_has_prefix (uri, templates_directory_uri))
    {
        return FALSE;
    }

    num_levels = 0;
    for (i = strlen (templates_directory_uri); uri[i] != '\0'; i++)
    {
        if (uri[i] == '/')
        {
            num_levels++;
        }
    }

    if (num_levels > MAX_MENU_LEVELS)
    {
        return FALSE;
    }

    return TRUE;
}

static gboolean
filter_templates_callback (NautilusFile *file,
                           gpointer      callback_data)
{
    gboolean show_hidden = GPOINTER_TO_INT (callback_data);

    if (nautilus_file_is_hidden_file (file))
    {
        if (!show_hidden)
        {
            return FALSE;
        }

        if (nautilus_file_is_directory (file))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static GList *
filter_templates (GList    *files,
                  gboolean  show_hidden)
{
    GList *filtered_files;
    GList *removed_files;

    filtered_files = nautilus_file_list_filter (files,
                                                &removed_files,
                                                filter_templates_callback,
                                                GINT_TO_POINTER (show_hidden));
    nautilus_file_list_free (removed_files);

    return filtered_files;
}

static GMenuModel *
update_directory_in_templates_menu (NautilusFilesView *view,
                                    NautilusDirectory *directory)
{
    NautilusFilesViewPrivate *priv;
    GList *file_list, *filtered, *node;
    GMenu *menu;
    GMenuItem *menu_item;
    gboolean any_templates;
    NautilusFile *file;
    NautilusDirectory *dir;
    char *uri;
    char *templates_directory_uri;
    int num;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    priv = nautilus_files_view_get_instance_private (view);

    file_list = nautilus_directory_get_file_list (directory);

    /*
     * The nautilus_file_list_filter_hidden() function isn't used here, because
     * we want to show hidden files, but not directories. This is a compromise
     * to allow creating hidden files but to prevent content from .git directory
     * for example. See https://gitlab.gnome.org/GNOME/nautilus/issues/1413.
     */
    filtered = filter_templates (file_list, priv->show_hidden_files);
    nautilus_file_list_free (file_list);
    templates_directory_uri = nautilus_get_templates_directory_uri ();
    menu = g_menu_new ();

    filtered = nautilus_file_list_sort_by_display_name (filtered);

    num = 0;
    any_templates = FALSE;
    for (node = filtered; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++)
    {
        file = node->data;
        if (nautilus_file_is_directory (file))
        {
            uri = nautilus_file_get_uri (file);
            if (directory_belongs_in_templates_menu (templates_directory_uri, uri))
            {
                g_autoptr (GMenuModel) children_menu = NULL;

                dir = nautilus_directory_get_by_uri (uri);
                add_directory_to_templates_directory_list (view, dir);

                children_menu = update_directory_in_templates_menu (view, dir);

                if (children_menu != NULL)
                {
                    const char *display_name = nautilus_file_get_display_name (file);
                    g_autofree char *label = NULL;

                    label = escape_underscores (display_name);
                    menu_item = g_menu_item_new_submenu (label, children_menu);
                    g_menu_append_item (menu, menu_item);
                    any_templates = TRUE;
                    g_object_unref (menu_item);
                }

                nautilus_directory_unref (dir);
            }
            g_free (uri);
        }
        else if (nautilus_file_can_read (file))
        {
            add_template_to_templates_menus (view, file, menu);
            any_templates = TRUE;
        }
    }

    nautilus_file_list_free (filtered);
    g_free (templates_directory_uri);

    if (!any_templates)
    {
        g_object_unref (menu);
        menu = NULL;
    }

    return G_MENU_MODEL (menu);
}



static void
update_templates_menu (NautilusFilesView *view,
                       GtkBuilder        *builder)
{
    NautilusFilesViewPrivate *priv;
    g_autolist (NautilusDirectory) sorted_copy = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (GMenuModel) submenu = NULL;
    g_autofree char *templates_directory_uri = NULL;

    priv = nautilus_files_view_get_instance_private (view);

    if (!nautilus_should_use_templates_directory ())
    {
        nautilus_view_set_templates_menu (NAUTILUS_VIEW (view), NULL);
        return;
    }

    templates_directory_uri = nautilus_get_templates_directory_uri ();
    sorted_copy = nautilus_directory_list_sort_by_uri
                      (nautilus_directory_list_copy (priv->templates_directory_list));

    for (GList *dir_l = sorted_copy; dir_l != NULL; dir_l = dir_l->next)
    {
        g_autofree char *uri = nautilus_directory_get_uri (dir_l->data);
        if (!directory_belongs_in_templates_menu (templates_directory_uri, uri))
        {
            remove_directory_from_templates_directory_list (view, dir_l->data);
        }
    }

    directory = nautilus_directory_get_by_uri (templates_directory_uri);
    submenu = update_directory_in_templates_menu (view, directory);

    nautilus_view_set_templates_menu (NAUTILUS_VIEW (view), submenu);
}


static void
action_open_scripts_folder (GSimpleAction *action,
                            GVariant      *state,
                            gpointer       user_data)
{
    static GFile *location = NULL;

    if (location == NULL)
    {
        location = g_file_new_for_uri (scripts_directory_uri);
    }

    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             location, 0, NULL, NULL, NULL);
}

static GFile *
get_dialog_initial_location (NautilusFilesView *view,
                             GList             *files)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    GFile *location;
    NautilusFile *file = NAUTILUS_FILE (files->data);

    /* The file dialog will not be able to display the search directory,
     * so we need to get the base directory of the search if we are, in fact,
     * in search.
     */
    if (nautilus_view_is_searching (NAUTILUS_VIEW (view)))
    {
        NautilusSearchDirectory *search = NAUTILUS_SEARCH_DIRECTORY (priv->directory);

        location = nautilus_query_get_location (nautilus_search_directory_get_query (search));
    }
    else if (showing_starred_directory (view))
    {
        location = nautilus_file_get_parent_location (file);
    }
    else if (showing_recent_directory (view))
    {
        g_autoptr (GFile) child = nautilus_file_get_activation_location (file);

        location = g_file_get_parent (child);
    }
    else if (showing_trash_directory (view))
    {
        g_autoptr (NautilusFile) child = nautilus_file_get_trash_original_file (file);

        location = nautilus_file_get_parent_location (child);
    }
    else
    {
        location = nautilus_directory_get_location (priv->directory);
        g_autofree gchar *path = g_file_get_path (location);

        if (path == NULL || *path == '\0')
        {
            /* Portals will not accept locations with no path, fall back to
             * null to use the default location. */
            g_clear_object (&location);
        }
    }

    return location;
}

typedef struct _CopyCallbackData
{
    NautilusFilesView *view;
    GList *selection;
    gboolean is_move;
} CopyCallbackData;

static void
copy_data_free (CopyCallbackData *data)
{
    nautilus_file_list_free (data->selection);
    g_free (data);
}

static void
on_destination_dialog_response (GtkFileDialog *dialog,
                                GAsyncResult  *result,
                                gpointer       user_data)
{
    CopyCallbackData *copy_data = user_data;
    g_autoptr (GFile) target_location = NULL;
    g_autoptr (GError) error = NULL;

    target_location = gtk_file_dialog_select_folder_finish (dialog, result, &error);

    if (target_location != NULL)
    {
        char *target_uri;
        GList *uris, *l;

        target_uri = g_file_get_uri (target_location);
        uris = NULL;
        for (l = copy_data->selection; l != NULL; l = l->next)
        {
            uris = g_list_prepend (uris,
                                   nautilus_file_get_uri ((NautilusFile *) l->data));
        }
        uris = g_list_reverse (uris);

        nautilus_files_view_move_copy_items (copy_data->view, uris, target_uri,
                                             copy_data->is_move ? GDK_ACTION_MOVE : GDK_ACTION_COPY);

        g_list_free_full (uris, g_free);
        g_free (target_uri);
    }
    else if (error != NULL &&
             !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
    {
        g_warning ("Error while choosing a destination folder: %s", error->message);
    }

    copy_data_free (copy_data);
}

static void
copy_or_move_selection (NautilusFilesView *view,
                        gboolean           is_move)
{
    g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
    g_autoptr (GFile) location = NULL;
    CopyCallbackData *copy_data;
    GList *selection;
    const gchar *title;

    if (is_move)
    {
        title = _("Select Move Destination");
    }
    else
    {
        title = _("Select Copy Destination");
    }

    selection = nautilus_files_view_get_selection_for_file_transfer (view);

    gtk_file_dialog_set_title (dialog, title);
    gtk_file_dialog_set_accept_label (dialog, _("_Select"));

    copy_data = g_new0 (CopyCallbackData, 1);
    copy_data->view = view;
    copy_data->selection = selection;
    copy_data->is_move = is_move;

    location = get_dialog_initial_location (view, selection);

    gtk_file_dialog_set_initial_folder (dialog, location);

    gtk_file_dialog_select_folder (dialog,
                                   GTK_WINDOW (nautilus_files_view_get_window (view)),
                                   NULL,
                                   (GAsyncReadyCallback) on_destination_dialog_response,
                                   copy_data);
}

static void
action_copy (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    NautilusFilesView *view;
    GdkClipboard *clipboard;
    GList *selection;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_files_view_get_selection_for_file_transfer (view);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
    nautilus_clipboard_prepare_for_files (clipboard, selection, FALSE);

    nautilus_file_list_free (selection);
}

static void
action_cut (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
    NautilusFilesView *view;
    GList *selection;
    GdkClipboard *clipboard;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_files_view_get_selection_for_file_transfer (view);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
    nautilus_clipboard_prepare_for_files (clipboard, selection, TRUE);

    nautilus_file_list_free (selection);
}

static void
action_copy_current_location (GSimpleAction *action,
                              GVariant      *state,
                              gpointer       user_data)
{
    NautilusFilesView *view;
    GdkClipboard *clipboard;
    GList *files;
    NautilusFilesViewPrivate *priv;

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory_as_file != NULL)
    {
        files = g_list_append (NULL, nautilus_file_ref (priv->directory_as_file));

        clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
        nautilus_clipboard_prepare_for_files (clipboard, files, FALSE);

        nautilus_file_list_free (files);
    }
}

static void
action_create_links_in_place (GSimpleAction *action,
                              GVariant      *state,
                              gpointer       user_data)
{
    NautilusFilesView *view;
    GList *selection;
    GList *item_uris;
    GList *l;
    char *destination_uri;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_files_view_get_selection_for_file_transfer (view);

    item_uris = NULL;
    for (l = selection; l != NULL; l = l->next)
    {
        item_uris = g_list_prepend (item_uris, nautilus_file_get_uri (l->data));
    }
    item_uris = g_list_reverse (item_uris);

    destination_uri = nautilus_files_view_get_backing_uri (view);

    nautilus_files_view_move_copy_items (view, item_uris, destination_uri,
                                         GDK_ACTION_LINK);

    g_list_free_full (item_uris, g_free);
    nautilus_file_list_free (selection);
}

static void
action_copy_to (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);
    copy_or_move_selection (view, FALSE);
}

static void
action_move_to (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);
    copy_or_move_selection (view, TRUE);
}

static void
action_paste_files_into (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;

    view = NAUTILUS_FILES_VIEW (user_data);
    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    if (selection != NULL)
    {
        paste_files (view, nautilus_file_get_activation_uri (selection->data), FALSE);
    }
}

static void
real_action_rename (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GtkWidget *dialog;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    if (selection != NULL)
    {
        /* If there is more than one file selected, invoke a batch renamer */
        if (selection->next != NULL)
        {
            NautilusWindow *window;

            window = nautilus_files_view_get_window (view);
            gtk_widget_set_cursor_from_name (GTK_WIDGET (window), "progress");

            dialog = nautilus_batch_rename_dialog_new (selection,
                                                       priv->directory,
                                                       window);

            gtk_window_present (GTK_WINDOW (dialog));
        }
        else
        {
            file = NAUTILUS_FILE (selection->data);

            nautilus_files_view_rename_file_popover_new (view, file);
        }
    }
}

static void
action_rename (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
    real_action_rename (NAUTILUS_FILES_VIEW (user_data));
}

typedef struct
{
    NautilusFilesView *view;
    GHashTable *added_locations;
} ExtractData;

static void
extract_done (GList    *outputs,
              gpointer  user_data)
{
    NautilusFilesViewPrivate *priv;
    ExtractData *data;
    GList *l;
    gboolean all_files_acknowledged;

    data = user_data;

    if (data->view == NULL)
    {
        goto out;
    }

    priv = nautilus_files_view_get_instance_private (data->view);

    g_signal_handlers_disconnect_by_func (data->view,
                                          G_CALLBACK (track_newly_added_locations),
                                          data->added_locations);

    if (outputs == NULL)
    {
        goto out;
    }

    all_files_acknowledged = TRUE;
    for (l = outputs; l && all_files_acknowledged; l = l->next)
    {
        all_files_acknowledged = g_hash_table_contains (data->added_locations,
                                                        l->data);
    }

    if (all_files_acknowledged)
    {
        GList *selection = NULL;

        for (l = outputs; l != NULL; l = l->next)
        {
            selection = g_list_prepend (selection,
                                        nautilus_file_get (l->data));
        }

        nautilus_files_view_set_selection (NAUTILUS_VIEW (data->view),
                                           selection);

        nautilus_file_list_free (selection);
    }
    else
    {
        for (l = outputs; l != NULL; l = l->next)
        {
            gboolean acknowledged;
            g_autoptr (NautilusFile) file = nautilus_file_get (l->data);

            acknowledged = g_hash_table_contains (data->added_locations,
                                                  l->data);

            g_hash_table_insert (priv->pending_reveal,
                                 file,
                                 GUINT_TO_POINTER (acknowledged));
        }
    }
out:
    g_hash_table_destroy (data->added_locations);

    if (data->view != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (data->view),
                                      (gpointer *) &data->view);
    }

    g_free (data);
}

static void
extract_files (NautilusFilesView *view,
               GList             *files,
               GFile             *destination_directory)
{
    GList *locations = NULL;
    GList *l;
    gboolean extracting_to_current_directory;

    if (files == NULL)
    {
        return;
    }

    for (l = files; l != NULL; l = l->next)
    {
        locations = g_list_prepend (locations,
                                    nautilus_file_get_location (l->data));
    }

    locations = g_list_reverse (locations);

    extracting_to_current_directory = g_file_equal (destination_directory,
                                                    nautilus_view_get_location (NAUTILUS_VIEW (view)));

    if (extracting_to_current_directory)
    {
        ExtractData *data;

        data = g_new (ExtractData, 1);
        data->view = view;
        data->added_locations = g_hash_table_new_full (g_file_hash,
                                                       (GEqualFunc) g_file_equal,
                                                       g_object_unref, NULL);


        g_object_add_weak_pointer (G_OBJECT (data->view),
                                   (gpointer *) &data->view);

        g_signal_connect_data (view,
                               "add-files",
                               G_CALLBACK (track_newly_added_locations),
                               data->added_locations,
                               NULL,
                               G_CONNECT_AFTER);

        nautilus_file_operations_extract_files (locations,
                                                destination_directory,
                                                nautilus_files_view_get_containing_window (view),
                                                NULL,
                                                extract_done,
                                                data);
    }
    else
    {
        nautilus_file_operations_extract_files (locations,
                                                destination_directory,
                                                nautilus_files_view_get_containing_window (view),
                                                NULL,
                                                NULL,
                                                NULL);
    }

    g_list_free_full (locations, g_object_unref);
}

typedef struct
{
    NautilusFilesView *view;
    GList *files;
} ExtractToData;

static void
on_extract_destination_dialog_response (GtkFileDialog *dialog,
                                        GAsyncResult  *result,
                                        gpointer       user_data)
{
    ExtractToData *data;
    g_autoptr (GFile) destination_directory = NULL;
    g_autoptr (GError) error = NULL;

    data = user_data;
    destination_directory = gtk_file_dialog_select_folder_finish (dialog, result, &error);

    if (destination_directory != NULL)
    {
        extract_files (data->view, data->files, destination_directory);
    }
    else if (error != NULL &&
             !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
    {
        g_warning ("Error while choosing a destination folder: %s", error->message);
    }

    nautilus_file_list_free (data->files);
    g_free (data);
}

static void
extract_files_to_chosen_location (NautilusFilesView *view,
                                  GList             *files)
{
    ExtractToData *data;
    g_autoptr (GtkFileDialog) dialog = NULL;
    g_autoptr (GFile) location = NULL;

    if (files == NULL)
    {
        return;
    }

    data = g_new (ExtractToData, 1);

    dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, _("Select Extract Destination"));
    gtk_file_dialog_set_accept_label (dialog, _("_Select"));

    location = get_dialog_initial_location (view, files);

    gtk_file_dialog_set_initial_folder (dialog, location);

    data->view = view;
    data->files = nautilus_file_list_copy (files);

    gtk_file_dialog_select_folder (dialog,
                                   GTK_WINDOW (nautilus_files_view_get_window (view)),
                                   NULL,
                                   (GAsyncReadyCallback) on_extract_destination_dialog_response,
                                   data);
}

static void
action_extract_here (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) parent = NULL;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    location = nautilus_file_get_location (NAUTILUS_FILE (g_list_first (selection)->data));
    /* Get a parent from a random file. We assume all files has a common parent.
     * But don't assume the parent is the view location, since that's not the
     * case in list view when expand-folder setting is set
     */
    parent = g_file_get_parent (location);

    extract_files (view, selection, parent);
}

static void
action_extract_to (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    extract_files_to_chosen_location (view, selection);
}

static void
action_compress (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
    NautilusFilesView *view = user_data;

    nautilus_files_view_compress_dialog_new (view);
}

static void
send_email_done (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
    GtkWindow *window = user_data;
    g_autoptr (GError) error = NULL;

    xdp_portal_compose_email_finish (XDP_PORTAL (source_object), res, &error);
    if (error != NULL)
    {
        show_dialog (_("Error sending email."),
                     error->message,
                     window,
                     GTK_MESSAGE_ERROR);
    }
}

static void
real_send_email (GStrv              attachments,
                 NautilusFilesView *view)
{
    /* Although the documentation says that addresses can be NULL, it takes
     * no action when addresses is NULL. Since we don't know the address,
     * provide an empty list */
    const char * const addresses[] = {NULL};
    g_autoptr (XdpPortal) portal = NULL;
    XdpParent *parent;
    GtkWidget *toplevel;

    portal = xdp_portal_new ();
    toplevel = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
    parent = xdp_parent_new_gtk (GTK_WINDOW (toplevel));
    xdp_portal_compose_email (portal, parent, addresses,
                              NULL, NULL, NULL, NULL, (const char * const *) attachments,
                              XDP_EMAIL_FLAG_NONE, NULL, send_email_done, toplevel);
}

static void
email_archive_ready (GFile    *new_file,
                     gboolean  success,
                     gpointer  user_data)
{
    g_autoptr (GStrvBuilder) strv_builder = NULL;
    g_auto (GStrv) attachments = NULL;
    NautilusFilesView *view = user_data;

    if (success)
    {
        strv_builder = g_strv_builder_new ();
        g_strv_builder_add (strv_builder, g_file_get_path (new_file));
        attachments = g_strv_builder_end (strv_builder);
        real_send_email (attachments, view);
    }
}

static void
action_send_email (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
    NautilusFilesView *view = user_data;
    g_autolist (NautilusFile) selection = NULL;
    g_auto (GStrv) attachments = NULL;
    g_autoptr (GStrvBuilder) strv_builder = NULL;
    gboolean has_directory = FALSE;

    strv_builder = g_strv_builder_new ();
    selection = nautilus_files_view_get_selection (NAUTILUS_VIEW (view));

    for (GList *l = selection; l != NULL; l = l->next)
    {
        if (nautilus_file_has_local_path (l->data))
        {
            g_autoptr (GFile) location = nautilus_file_get_location (l->data);
            g_strv_builder_add (strv_builder, g_file_get_path (location));
        }
        /* If there's a directory in the list, we can't attach a folder,
         * so to keep things simple let's archive the whole selection */
        if (nautilus_file_is_directory (l->data))
        {
            has_directory = TRUE;
            break;
        }
    }

    if (has_directory)
    {
        g_autolist (GFile) source_locations = NULL;
        g_autofree gchar *archive_directory_name = NULL;
        g_autoptr (GFile) archive_directory = NULL;
        g_autoptr (GFile) archive_location = NULL;

        for (GList *l = selection; l != NULL; l = l->next)
        {
            source_locations = g_list_prepend (source_locations,
                                               nautilus_file_get_location (l->data));
        }
        source_locations = g_list_reverse (source_locations);
        archive_directory_name = g_dir_make_tmp ("nautilus-sendto-XXXXXX", NULL);
        archive_directory = g_file_new_for_path (archive_directory_name);
        archive_location = g_file_get_child (archive_directory, "archive.zip");
        nautilus_file_operations_compress (source_locations, archive_location,
                                           AUTOAR_FORMAT_ZIP, AUTOAR_FILTER_NONE,
                                           NULL,
                                           nautilus_files_view_get_containing_window (view),
                                           NULL, email_archive_ready, view);
    }
    else
    {
        attachments = g_strv_builder_end (strv_builder);
        real_send_email (attachments, view);
    }
}

static gboolean
can_run_in_terminal (GList *selection)
{
    NautilusFile *file;

    if (g_list_length (selection) != 1)
    {
        return FALSE;
    }

    file = NAUTILUS_FILE (selection->data);

    if (nautilus_file_is_launchable (file) &&
        nautilus_file_contains_text (file))
    {
        g_autofree gchar *activation_uri = NULL;
        g_autofree gchar *executable_path = NULL;

        activation_uri = nautilus_file_get_activation_uri (file);
        executable_path = g_filename_from_uri (activation_uri, NULL, NULL);

        if (executable_path != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
action_run_in_terminal (GSimpleAction *action,
                        GVariant      *state,
                        gpointer       user_data)
{
    NautilusFilesView *view;
    g_autolist (NautilusFile) selection = NULL;
    g_autofree char *old_working_dir = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *executable_path = NULL;
    g_autofree char *quoted_path = NULL;
    GtkWindow *parent_window;
    GdkDisplay *display;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    if (!can_run_in_terminal (selection))
    {
        return;
    }

    old_working_dir = change_to_view_directory (view);

    uri = nautilus_file_get_activation_uri (NAUTILUS_FILE (selection->data));
    executable_path = g_filename_from_uri (uri, NULL, NULL);
    quoted_path = g_shell_quote (executable_path);

    parent_window = nautilus_files_view_get_containing_window (view);
    display = gtk_widget_get_display (GTK_WIDGET (parent_window));

    g_debug ("Launching in terminal %s", quoted_path);

    nautilus_launch_application_from_command (display, quoted_path, TRUE, NULL);

    g_chdir (old_working_dir);
}

static gboolean
can_set_wallpaper (GList *selection)
{
    NautilusFile *file;

    if (g_list_length (selection) != 1)
    {
        return FALSE;
    }

    file = NAUTILUS_FILE (selection->data);
    if (!nautilus_file_is_mime_type (file, "image/*"))
    {
        return FALSE;
    }

    /* FIXME: check file size? */

    return TRUE;
}

static void
set_wallpaper_with_portal_cb (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data)
{
    XdpPortal *portal = XDP_PORTAL (source);
    g_autoptr (GError) error = NULL;

    if (!xdp_portal_set_wallpaper_finish (portal, result, &error)
        && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        g_warning ("Failed to set wallpaper via portal: %s", error->message);
    }
}

static void
set_wallpaper_with_portal (NautilusFile *file,
                           gpointer      user_data)
{
    g_autoptr (XdpPortal) portal = NULL;
    g_autofree gchar *uri = NULL;
    XdpParent *parent = NULL;
    GtkWidget *toplevel;

    portal = xdp_portal_new ();
    toplevel = gtk_widget_get_ancestor (GTK_WIDGET (user_data), GTK_TYPE_WINDOW);
    parent = xdp_parent_new_gtk (GTK_WINDOW (toplevel));
    uri = nautilus_file_get_uri (file);

    xdp_portal_set_wallpaper (portal,
                              parent,
                              uri,
                              XDP_WALLPAPER_FLAG_BACKGROUND | XDP_WALLPAPER_FLAG_PREVIEW,
                              NULL,
                              set_wallpaper_with_portal_cb,
                              NULL);
    xdp_parent_free (parent);
}

static void
action_set_as_wallpaper (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
    g_autolist (NautilusFile) selection = NULL;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    selection = nautilus_view_get_selection (user_data);
    if (can_set_wallpaper (selection))
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (selection->data);

        set_wallpaper_with_portal (file, user_data);
    }
}

static void
file_mount_callback (NautilusFile *file,
                     GFile        *result_location,
                     GError       *error,
                     gpointer      callback_data)
{
    g_autoptr (NautilusFilesView) self = NAUTILUS_FILES_VIEW (callback_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

    nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_MOUNT);
    nautilus_view_item_set_loading (item, FALSE);

    if (error != NULL &&
        (error->domain != G_IO_ERROR ||
         (error->code != G_IO_ERROR_CANCELLED &&
          error->code != G_IO_ERROR_FAILED_HANDLED &&
          error->code != G_IO_ERROR_ALREADY_MOUNTED)))
    {
        /* Translators: %s is a file name formatted for display */
        g_autofree char *text = g_strdup_printf (_("Unable to access “%s”"),
                                                 nautilus_file_get_display_name (file));
        show_dialog (text,
                     error->message,
                     GTK_WINDOW (nautilus_files_view_get_window (self)),
                     GTK_MESSAGE_ERROR);
    }
}

static void
file_unmount_callback (NautilusFile *file,
                       GFile        *result_location,
                       GError       *error,
                       gpointer      callback_data)
{
    g_autoptr (NautilusFilesView) self = NAUTILUS_FILES_VIEW (callback_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

    nautilus_view_item_set_loading (item, FALSE);

    if (error != NULL &&
        (error->domain != G_IO_ERROR ||
         (error->code != G_IO_ERROR_CANCELLED &&
          error->code != G_IO_ERROR_FAILED_HANDLED)))
    {
        /* Translators: %s is a file name formatted for display */
        g_autofree char *text = g_strdup_printf (_("Unable to remove “%s”"),
                                                 nautilus_file_get_display_name (file));
        show_dialog (text,
                     error->message,
                     GTK_WINDOW (nautilus_files_view_get_window (self)),
                     GTK_MESSAGE_ERROR);
    }
}

static void
file_eject_callback (NautilusFile *file,
                     GFile        *result_location,
                     GError       *error,
                     gpointer      callback_data)
{
    g_autoptr (NautilusFilesView) self = NAUTILUS_FILES_VIEW (callback_data);

    if (error != NULL &&
        (error->domain != G_IO_ERROR ||
         (error->code != G_IO_ERROR_CANCELLED &&
          error->code != G_IO_ERROR_FAILED_HANDLED)))
    {
        /* Translators: %s is a file name formatted for display */
        g_autofree char *text = g_strdup_printf (_("Unable to eject “%s”"),
                                                 nautilus_file_get_display_name (file));
        show_dialog (text,
                     error->message,
                     GTK_WINDOW (nautilus_files_view_get_window (self)),
                     GTK_MESSAGE_ERROR);
    }
}

static void
file_stop_callback (NautilusFile *file,
                    GFile        *result_location,
                    GError       *error,
                    gpointer      callback_data)
{
    g_autoptr (NautilusFilesView) self = NAUTILUS_FILES_VIEW (callback_data);

    if (error != NULL &&
        (error->domain != G_IO_ERROR ||
         (error->code != G_IO_ERROR_CANCELLED &&
          error->code != G_IO_ERROR_FAILED_HANDLED)))
    {
        show_dialog (_("Unable to stop drive"),
                     error->message,
                     GTK_WINDOW (nautilus_files_view_get_window (self)),
                     GTK_MESSAGE_ERROR);
    }
}

static void
action_mount_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    NautilusFile *file;
    GList *selection, *l;
    GMountOperation *mount_op;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_can_mount (file))
        {
            NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

            nautilus_view_item_set_loading (item, TRUE);
            mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
            g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
            nautilus_file_mount (file, mount_op, NULL,
                                 file_mount_callback,
                                 g_object_ref (view));
            g_object_unref (mount_op);
        }
    }
    nautilus_file_list_free (selection);
}

static void
action_unmount_volume (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GList *l;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);
        if (nautilus_file_can_unmount (file))
        {
            GMountOperation *mount_op;
            NautilusViewItem *item = nautilus_view_model_get_item_for_file (priv->model, file);

            nautilus_view_item_set_loading (item, TRUE);
            mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
            nautilus_file_unmount (file, mount_op, NULL,
                                   file_unmount_callback, g_object_ref (view));
            g_object_unref (mount_op);
        }
    }
}

static void
action_eject_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_can_eject (file))
        {
            GMountOperation *mount_op;
            mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
            nautilus_file_eject (file, mount_op, NULL,
                                 file_eject_callback, g_object_ref (view));
            g_object_unref (mount_op);
        }
    }
}

static void
file_start_callback (NautilusFile *file,
                     GFile        *result_location,
                     GError       *error,
                     gpointer      callback_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (callback_data);

    if (error != NULL &&
        (error->domain != G_IO_ERROR ||
         (error->code != G_IO_ERROR_CANCELLED &&
          error->code != G_IO_ERROR_FAILED_HANDLED &&
          error->code != G_IO_ERROR_ALREADY_MOUNTED)))
    {
        const char *name = nautilus_file_get_display_name (file);
        /* Translators: %s is a file name formatted for display */
        g_autofree char *text = g_strdup_printf (_("Unable to start “%s”"), name);
        show_dialog (text,
                     error->message,
                     GTK_WINDOW (nautilus_files_view_get_window (view)),
                     GTK_MESSAGE_ERROR);
    }
}

static void
action_start_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    NautilusFilesView *view;
    GMountOperation *mount_op;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file))
        {
            mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
            nautilus_file_start (file, mount_op, NULL,
                                 file_start_callback, view);
            g_object_unref (mount_op);
        }
    }
}

static void
action_stop_volume (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_can_stop (file))
        {
            GMountOperation *mount_op;
            mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
            nautilus_file_stop (file, mount_op, NULL,
                                file_stop_callback, g_object_ref (view));
            g_object_unref (mount_op);
        }
    }
}

static void
action_detect_media (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
    NautilusFile *file;
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    NautilusView *view;

    view = NAUTILUS_VIEW (user_data);

    selection = nautilus_view_get_selection (view);
    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file))
        {
            nautilus_file_poll_for_media (file);
        }
    }
}

static void
action_copy_network_address (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));

    g_return_if_fail (selection != NULL && selection->next == NULL);

    g_autofree char *address = nautilus_file_get_activation_uri (NAUTILUS_FILE (selection->data));

    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), address);
}

static void
action_remove_recent_server (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));

    for (GList *l = selection; l != NULL; l = l->next)
    {
        g_autofree char *address = nautilus_file_get_activation_uri (NAUTILUS_FILE (selection->data));

        nautilus_remove_recent_server (address);
    }
}

const GActionEntry view_entries[] =
{
    /* Toolbar menu */
    { .name = "zoom-in", .activate = action_zoom_in },
    { .name = "zoom-out", .activate = action_zoom_out },
    { .name = "zoom-standard", .activate = action_zoom_standard },
    { .name = "sort", .parameter_type = "(sb)", .state = "('invalid',false)", .change_state = action_sort_order_changed },
    { .name = "show-hidden-files", .state = "true", .change_state = action_show_hidden_files },
    { .name = "visible-columns", .activate = action_visible_columns },
    /* Background menu */
    { .name = "empty-trash", .activate = action_empty_trash },
    { .name = "new-folder", .activate = action_new_folder },
    { .name = "select-all", .activate = action_select_all },
    { .name = "paste", .activate = action_paste_files },
    { .name = "copy-current-location", .activate = action_copy_current_location },
    { .name = "paste_accel", .activate = action_paste_files_accel },
    { .name = "create-link", .activate = action_create_links },
    { .name = "create-link-shortcut", .activate = action_create_links },
    /* Selection menu */
    { .name = "new-folder-with-selection", .activate = action_new_folder_with_selection },
    { .name = "open-scripts-folder", .activate = action_open_scripts_folder },
    { .name = "open-item-location", .activate = action_open_item_location },
    { .name = "open-with-default-application", .activate = action_open_with_default_application },
    { .name = "open-with-other-application", .activate = action_open_with_other_application },
    {
        .name = "open-current-directory-with-other-application",
        .activate = action_open_current_directory_with_other_application
    },
    { .name = "open-item-new-window", .activate = action_open_item_new_window },
    { .name = "open-item-new-tab", .activate = action_open_item_new_tab },
    { .name = "cut", .activate = action_cut},
    { .name = "copy", .activate = action_copy},
    { .name = "create-link-in-place", .activate = action_create_links_in_place },
    { .name = "create-link-in-place-shortcut", .activate = action_create_links_in_place },
    { .name = "move-to", .activate = action_move_to},
    { .name = "copy-to", .activate = action_copy_to},
    { .name = "move-to-trash", .activate = action_move_to_trash},
    { .name = "delete-from-trash", .activate = action_delete },
    { .name = "star", .activate = action_star},
    { .name = "unstar", .activate = action_unstar},
    /* We separate the shortcut and the menu item since we want the shortcut
     * to always be available, but we don't want the menu item shown if not
     * completely necesary. Since the visibility of the menu item is based on
     * the action enability, we need to split the actions for the menu and the
     * shortcut. */
    { .name = "delete-permanently-shortcut", .activate = action_delete },
    { .name = "delete-permanently-menu-item", .activate = action_delete },
    /* This is only shown when the setting to show always delete permanently
     * is set and when the common use cases for delete permanently which uses
     * Delete as a shortcut are not needed. For instance this will be only
     * present when the setting is true and when it can trash files */
    { .name = "permanent-delete-permanently-menu-item", .activate = action_delete },
    { .name = "remove-from-recent", .activate = action_remove_from_recent },
    { .name = "restore-from-trash", .activate = action_restore_from_trash},
    { .name = "paste-into", .activate = action_paste_files_into },
    { .name = "rename", .activate = action_rename},
    { .name = "extract-here", .activate = action_extract_here },
    { .name = "extract-to", .activate = action_extract_to },
    { .name = "compress", .activate = action_compress },
    { .name = "send-email", .activate = action_send_email },
    { .name = "console", .activate = action_open_console },
    { .name = "current-directory-console", .activate = action_current_dir_open_console },
    { .name = "properties", .activate = action_properties},
    { .name = "current-directory-properties", .activate = action_current_dir_properties},
    { .name = "run-in-terminal", .activate = action_run_in_terminal },
    { .name = "set-as-wallpaper", .activate = action_set_as_wallpaper },
    { .name = "mount-volume", .activate = action_mount_volume },
    { .name = "unmount-volume", .activate = action_unmount_volume },
    { .name = "eject-volume", .activate = action_eject_volume },
    { .name = "start-volume", .activate = action_start_volume },
    { .name = "stop-volume", .activate = action_stop_volume },
    { .name = "detect-media", .activate = action_detect_media },
    /* Only in Network View */
    { .name = "copy-network-address", .activate = action_copy_network_address },
    { .name = "remove-recent-server", .activate = action_remove_recent_server },
    /* Only accesible by shorcuts */
    { .name = "select-pattern", .activate = action_select_pattern },
    { .name = "invert-selection", .activate = action_invert_selection },
    { .name = "preview-selection", .activate = action_preview_selection },
    { .name = "popup-menu", .activate = action_popup_menu },
};

static gboolean
can_paste_into_file (NautilusFile *file)
{
    if (nautilus_file_is_directory (file) &&
        nautilus_file_can_write (file))
    {
        return TRUE;
    }
    if (nautilus_file_has_activation_uri (file))
    {
        GFile *location;
        NautilusFile *activation_file;
        gboolean res;

        location = nautilus_file_get_activation_location (file);
        activation_file = nautilus_file_get (location);
        g_object_unref (location);

        /* The target location might not have data for it read yet,
         *  and we can't want to do sync I/O, so treat the unknown
         *  case as can-write */
        res = (nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_UNKNOWN) ||
              (nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_DIRECTORY &&
               nautilus_file_can_write (activation_file));

        nautilus_file_unref (activation_file);

        return res;
    }

    return FALSE;
}

static void
update_actions_clipboard_contents_received (GObject      *source_object,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    NautilusClipboard *clip = NULL;
    gboolean can_link_from_copied_files;
    gboolean settings_show_create_link;
    gboolean is_read_only;
    gboolean selection_contains_recent;
    gboolean selection_contains_starred;
    GAction *action;
    const GValue *value;

    value = gdk_clipboard_read_value_finish (GDK_CLIPBOARD (source_object), res, NULL);
    if (value == NULL)
    {
        return;
    }

    view = NAUTILUS_FILES_VIEW (user_data);
    priv = nautilus_files_view_get_instance_private (view);

    if (G_VALUE_HOLDS (value, NAUTILUS_TYPE_CLIPBOARD))
    {
        clip = g_value_get_boxed (value);
    }

    if (priv->in_destruction ||
        !priv->active)
    {
        /* We've been destroyed or became inactive since call */
        return;
    }

    settings_show_create_link = g_settings_get_boolean (nautilus_preferences,
                                                        NAUTILUS_PREFERENCES_SHOW_CREATE_LINK);
    is_read_only = nautilus_files_view_is_read_only (view);
    selection_contains_recent = showing_recent_directory (view);
    selection_contains_starred = showing_starred_directory (view);
    can_link_from_copied_files = clip != NULL && !nautilus_clipboard_is_cut (clip) &&
                                 !selection_contains_recent && !selection_contains_starred &&
                                 !is_read_only;

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "create-link");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_link_from_copied_files &&
                                 settings_show_create_link);
    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "create-link-shortcut");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_link_from_copied_files &&
                                 !settings_show_create_link);
}

static void
update_actions_state_for_clipboard_targets (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    GdkClipboard *clipboard;
    GdkContentFormats *formats;
    gboolean is_data_copied;
    GAction *action;

    priv = nautilus_files_view_get_instance_private (view);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
    formats = gdk_clipboard_get_formats (clipboard);
    is_data_copied = gdk_content_formats_contain_gtype (formats, NAUTILUS_TYPE_CLIPBOARD) ||
                     gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST) ||
                     gdk_content_formats_contain_gtype (formats, G_TYPE_FILE) ||
                     gdk_content_formats_contain_gtype (formats, GDK_TYPE_TEXTURE);

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "paste");
    /* Take into account if the action was previously disabled for other reasons,
     * like the directory not being writabble */
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 is_data_copied && g_action_get_enabled (action));

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "paste-into");

    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 is_data_copied && g_action_get_enabled (action));

    action = g_action_map_lookup_action (G_ACTION_MAP (priv->view_action_group),
                                         "create-link");

    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 is_data_copied && g_action_get_enabled (action));

    if (gdk_content_formats_contain_gtype (formats, NAUTILUS_TYPE_CLIPBOARD))
    {
        gdk_clipboard_read_value_async (clipboard, NAUTILUS_TYPE_CLIPBOARD,
                                        G_PRIORITY_DEFAULT,
                                        priv->clipboard_cancellable,
                                        update_actions_clipboard_contents_received,
                                        view);
    }
}

static void
file_should_show_foreach (NautilusFile        *file,
                          gboolean            *show_mount,
                          gboolean            *show_unmount,
                          gboolean            *show_eject,
                          gboolean            *show_start,
                          gboolean            *show_stop,
                          gboolean            *show_poll,
                          GDriveStartStopType *start_stop_type)
{
    *show_mount = FALSE;
    *show_unmount = FALSE;
    *show_eject = FALSE;
    *show_start = FALSE;
    *show_stop = FALSE;
    *show_poll = FALSE;

    if (nautilus_file_can_eject (file))
    {
        *show_eject = TRUE;
    }

    if (nautilus_file_can_mount (file))
    {
        *show_mount = TRUE;
    }

    if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file))
    {
        *show_start = TRUE;
    }

    if (nautilus_file_can_stop (file))
    {
        *show_stop = TRUE;
    }

    /* Dot not show both Unmount and Eject/Safe Removal; too confusing to
     * have too many menu entries */
    if (nautilus_file_can_unmount (file) && !*show_eject && !*show_stop)
    {
        *show_unmount = TRUE;
    }

    if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file))
    {
        *show_poll = TRUE;
    }

    *start_stop_type = nautilus_file_get_start_stop_type (file);
}

static gboolean
can_restore_from_trash (GList *files)
{
    NautilusFile *original_file;
    NautilusFile *original_dir;
    GHashTable *original_dirs_hash;
    GList *original_dirs;
    gboolean can_restore;

    original_file = NULL;
    original_dir = NULL;
    original_dirs = NULL;
    original_dirs_hash = NULL;

    if (files != NULL)
    {
        if (g_list_length (files) == 1)
        {
            original_file = nautilus_file_get_trash_original_file (files->data);
        }
        else
        {
            original_dirs_hash = nautilus_trashed_files_get_original_directories (files, NULL);
            if (original_dirs_hash != NULL)
            {
                original_dirs = g_hash_table_get_keys (original_dirs_hash);
                if (g_list_length (original_dirs) == 1)
                {
                    original_dir = nautilus_file_ref (NAUTILUS_FILE (original_dirs->data));
                }
            }
        }
    }

    can_restore = original_file != NULL || original_dirs != NULL;

    nautilus_file_unref (original_file);
    nautilus_file_unref (original_dir);
    g_list_free (original_dirs);

    if (original_dirs_hash != NULL)
    {
        g_hash_table_destroy (original_dirs_hash);
    }
    return can_restore;
}

static void
on_clipboard_owner_changed (GdkClipboard *clipboard,
                            gpointer      user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);

    update_cut_status (self);

    /* We need to update paste and paste-like actions */
    nautilus_files_view_update_actions_state (self);
}

static gboolean
can_delete_all (GList *files)
{
    NautilusFile *file;
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        file = l->data;
        if (!nautilus_file_can_delete (file))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
can_trash_all (GList *files)
{
    NautilusFile *file;
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        file = l->data;
        if (!nautilus_file_can_trash (file))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
all_in_trash (GList *files)
{
    NautilusFile *file;
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        file = l->data;
        if (!nautilus_file_is_in_trash (file))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
can_extract_all (GList *files)
{
    NautilusFile *file;
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        file = l->data;
        if (!nautilus_file_is_archive (file))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
nautilus_handles_all_files_to_extract (GList *files)
{
    NautilusFile *file;
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        file = l->data;
        if (!nautilus_mime_file_extracts (file))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static void
real_update_actions_state (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    gint selection_count;
    gboolean is_network_view = NAUTILUS_IS_NETWORK_VIEW (priv->list_base);
    gboolean selection_contains_home_dir;
    gboolean selection_contains_recent;
    gboolean selection_contains_search;
    gboolean selection_contains_starred;
    gboolean selection_all_in_trash;
    gboolean can_create_files;
    gboolean can_delete_files;
    gboolean can_move_files;
    gboolean can_trash_files;
    gboolean can_copy_files;
    gboolean can_paste_files_into;
    gboolean can_extract_files;
    gboolean handles_all_files_to_extract;
    gboolean can_extract_here;
    gboolean item_opens_in_view;
    gboolean is_read_only;
    gboolean is_in_trash;
    GAction *action;
    GActionGroup *view_action_group;
    gboolean show_mount;
    gboolean show_unmount;
    gboolean show_eject;
    gboolean show_start;
    gboolean show_stop;
    gboolean show_detect_media;
    gboolean settings_show_delete_permanently;
    gboolean settings_show_create_link;
    GDriveStartStopType start_stop_type;
    g_autoptr (GFile) current_location = NULL;
    g_autofree gchar *current_uri = NULL;
    gboolean can_star_current_directory;
    gboolean show_star;
    gboolean show_unstar;
    g_autoptr (GAppInfo) app_info_mailto = NULL;

    view_action_group = priv->view_action_group;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    selection_count = g_list_length (selection);
    selection_contains_home_dir = home_dir_in_selection (selection);
    selection_contains_recent = showing_recent_directory (view);
    selection_contains_starred = showing_starred_directory (view);
    selection_contains_search = nautilus_view_is_searching (NAUTILUS_VIEW (view));
    selection_all_in_trash = all_in_trash (selection);

    is_read_only = nautilus_files_view_is_read_only (view);
    is_in_trash = showing_trash_directory (view);
    can_create_files = nautilus_files_view_supports_creating_files (view);
    can_delete_files =
        can_delete_all (selection) &&
        selection_count != 0 &&
        !selection_contains_home_dir;
    can_trash_files =
        can_trash_all (selection) &&
        selection_count != 0 &&
        !selection_contains_home_dir;
    can_copy_files = selection_count != 0;
    can_move_files = can_delete_files && !selection_contains_recent &&
                     !selection_contains_starred;
    can_paste_files_into = (selection_count == 1 &&
                            can_paste_into_file (NAUTILUS_FILE (selection->data)));
    can_extract_files = selection_count != 0 &&
                        can_extract_all (selection);
    can_extract_here = nautilus_files_view_supports_extract_here (view);
    handles_all_files_to_extract = nautilus_handles_all_files_to_extract (selection);
    settings_show_delete_permanently = g_settings_get_boolean (nautilus_preferences,
                                                               NAUTILUS_PREFERENCES_SHOW_DELETE_PERMANENTLY);
    settings_show_create_link = g_settings_get_boolean (nautilus_preferences,
                                                        NAUTILUS_PREFERENCES_SHOW_CREATE_LINK);

    app_info_mailto = g_app_info_get_default_for_uri_scheme ("mailto");

    /* Right click actions
     * Selection menu actions
     */
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "new-folder-with-selection");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_create_files && can_delete_files && (selection_count > 1) && !selection_contains_recent
                                 && !selection_contains_starred);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "rename");
    if (selection_count > 1)
    {
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     nautilus_file_can_rename_files (selection));
    }
    else
    {
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     selection_count == 1 &&
                                     nautilus_file_can_rename (selection->data));
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "extract-here");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_extract_files &&
                                 !handles_all_files_to_extract &&
                                 can_extract_here);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "extract-to");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_extract_files &&
                                 (!handles_all_files_to_extract ||
                                  can_extract_here));

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "compress");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_create_files && can_copy_files);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-item-location");

    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 selection_count == 1 &&
                                 (selection_contains_recent || selection_contains_search ||
                                  selection_contains_starred));

    item_opens_in_view = selection_count != 0;

    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (selection->data);

        if (!nautilus_file_opens_in_view (file))
        {
            item_opens_in_view = FALSE;
        }

        if (!item_opens_in_view)
        {
            break;
        }
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-with-default-application");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection_count != 0);

    /* Allow to select a different application to open the item */
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-with-other-application");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 selection_count > 0);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-item-new-tab");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), item_opens_in_view);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-item-new-window");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), item_opens_in_view);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "run-in-terminal");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_run_in_terminal (selection));
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "set-as-wallpaper");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_set_wallpaper (selection));
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "restore-from-trash");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_restore_from_trash (selection));

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "move-to-trash");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_trash_files);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "delete-from-trash");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_delete_files && selection_all_in_trash);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "delete-permanently-shortcut");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_delete_files);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "delete-permanently-menu-item");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_delete_files && !can_trash_files &&
                                 !selection_all_in_trash && !selection_contains_recent &&
                                 !selection_contains_starred);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "permanent-delete-permanently-menu-item");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_delete_files && can_trash_files &&
                                 settings_show_delete_permanently &&
                                 !selection_all_in_trash && !selection_contains_recent &&
                                 !selection_contains_starred);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "remove-from-recent");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 selection_contains_recent && selection_count > 0);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "cut");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_move_files && !selection_contains_recent &&
                                 !selection_contains_starred);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "copy");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_copy_files);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "create-link-in-place");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_copy_files &&
                                 can_create_files &&
                                 settings_show_create_link);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "create-link-in-place-shortcut");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_copy_files &&
                                 can_create_files &&
                                 !settings_show_create_link);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "send-email");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 app_info_mailto != NULL);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "copy-to");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_copy_files);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "move-to");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 can_move_files && !selection_contains_recent &&
                                 !selection_contains_starred);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "preview-selection");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), selection_count != 0);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "copy-current-location");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !selection_contains_recent &&
                                 !selection_contains_search &&
                                 !selection_contains_starred &&
                                 !is_network_view);

    /* Drive menu */
    show_mount = (selection != NULL);
    show_unmount = (selection != NULL);
    show_eject = (selection != NULL);
    show_start = (selection != NULL && selection_count == 1);
    show_stop = (selection != NULL && selection_count == 1);
    show_detect_media = (selection != NULL && selection_count == 1);
    for (l = selection; l != NULL && (show_mount || show_unmount
                                      || show_eject
                                      || show_start || show_stop
                                      || show_detect_media);
         l = l->next)
    {
        NautilusFile *file;
        gboolean show_mount_one;
        gboolean show_unmount_one;
        gboolean show_eject_one;
        gboolean show_start_one;
        gboolean show_stop_one;
        gboolean show_detect_media_one;

        file = NAUTILUS_FILE (l->data);
        file_should_show_foreach (file,
                                  &show_mount_one,
                                  &show_unmount_one,
                                  &show_eject_one,
                                  &show_start_one,
                                  &show_stop_one,
                                  &show_detect_media_one,
                                  &start_stop_type);

        show_mount &= show_mount_one;
        show_unmount &= show_unmount_one;
        show_eject &= show_eject_one;
        show_start &= show_start_one;
        show_stop &= show_stop_one;
        show_detect_media &= show_detect_media_one;
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "mount-volume");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_mount);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "unmount-volume");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_unmount);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "eject-volume");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_eject);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "start-volume");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_start);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "stop-volume");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_stop);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "detect-media");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 show_detect_media);

    /* Background menu actions */
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "open-current-directory-with-other-application");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !selection_contains_recent &&
                                 !selection_contains_search &&
                                 !selection_contains_starred &&
                                 !is_network_view);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "new-folder");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_create_files);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "empty-trash");

    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !nautilus_trash_monitor_is_empty () &&
                                 is_in_trash);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "paste");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !is_read_only &&
                                 !selection_contains_recent &&
                                 !is_in_trash &&
                                 !selection_contains_starred);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "paste-into");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !selection_contains_recent &&
                                 !is_in_trash &&
                                 !selection_contains_starred &&
                                 can_paste_files_into);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "console");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 selection_count == 1 && nautilus_file_is_directory (selection->data) &&
                                 nautilus_dbus_launcher_is_available (nautilus_dbus_launcher_get (),
                                                                      NAUTILUS_DBUS_LAUNCHER_CONSOLE));
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "current-directory-console");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 nautilus_dbus_launcher_is_available (nautilus_dbus_launcher_get (),
                                                                      NAUTILUS_DBUS_LAUNCHER_CONSOLE));
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "properties");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 (is_network_view ?
                                  (selection_count == 1 &&
                                   nautilus_file_can_unmount (selection->data)) :
                                  (selection_count != 0 ||
                                   (!selection_contains_recent &&
                                    !selection_contains_search &&
                                    !selection_contains_starred))));
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "current-directory-properties");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !selection_contains_recent &&
                                 !selection_contains_search &&
                                 !selection_contains_starred &&
                                 !is_network_view);

    /* Actions that are related to the clipboard need request, request the data
     * and update them once we have the data */
    update_actions_state_for_clipboard_targets (view);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "select-all");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !nautilus_files_view_is_empty (view) &&
                                 !priv->loading);

    /* Toolbar menu actions */
    g_action_group_change_action_state (view_action_group,
                                        "show-hidden-files",
                                        g_variant_new_boolean (priv->show_hidden_files));

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "visible-columns");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 NAUTILUS_IS_LIST_VIEW (priv->list_base));

    update_zoom_actions_state (view);

    current_location = nautilus_file_get_location (priv->directory_as_file);
    current_uri = g_file_get_uri (current_location);
    can_star_current_directory = nautilus_tag_manager_can_star_contents (nautilus_tag_manager_get (), current_location);

    show_star = (selection != NULL) &&
                (can_star_current_directory || selection_contains_starred);
    show_unstar = (selection != NULL) &&
                  (can_star_current_directory || selection_contains_starred);
    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file;
        g_autofree gchar *uri = NULL;

        file = NAUTILUS_FILE (l->data);
        uri = nautilus_file_get_uri (file);

        if (!show_star && !show_unstar)
        {
            break;
        }

        if (nautilus_tag_manager_file_is_starred (nautilus_tag_manager_get (), uri))
        {
            show_star = FALSE;
        }
        else
        {
            show_unstar = FALSE;
        }
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "star");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_star);

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "unstar");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_unstar && selection_contains_starred);

    /* Network view actions */
    gboolean can_remove_recent_server = is_network_view;

    for (l = selection; l != NULL && can_remove_recent_server; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        g_autoptr (GFile) location = nautilus_file_get_location (NAUTILUS_FILE (l->data));

        /* Only recent servers have x-network-view: scheme */
        if (!g_file_has_uri_scheme (location, SCHEME_NETWORK_VIEW) || nautilus_file_can_unmount (file))
        {
            can_remove_recent_server = FALSE;
        }
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "copy-network-address");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 (is_network_view &&
                                  selection_count == 1 &&
                                  nautilus_file_has_activation_uri (selection->data)));

    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                         "remove-recent-server");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_remove_recent_server);
}

/* Convenience function to be called when updating menus,
 * so children can subclass it and it will be called when
 * they chain up to the parent in update_context_menus
 * or update_toolbar_menus
 */
void
nautilus_files_view_update_actions_state (NautilusFilesView *view)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_actions_state (view);
}

static void
update_selection_menu (NautilusFilesView *view,
                       GtkBuilder        *builder)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    g_autolist (NautilusFile) selection = NULL;
    GList *l;
    gint selection_count;
    gboolean show_app;
    gboolean show_run;
    gboolean show_extract;
    gboolean item_opens_in_view;
    gchar *item_label;
    GAppInfo *app;
    g_autoptr (GIcon) app_icon = NULL;
    GMenuItem *menu_item;
    GObject *object;
    gboolean show_mount;
    gboolean show_unmount;
    gboolean show_eject;
    gboolean show_start;
    gboolean show_stop;
    gboolean show_detect_media;
    gboolean show_scripts = FALSE;
    gint i;
    GDriveStartStopType start_stop_type;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    selection_count = g_list_length (selection);

    show_mount = (selection != NULL);
    show_unmount = (selection != NULL);
    show_eject = (selection != NULL);
    show_start = (selection != NULL && selection_count == 1);
    show_stop = (selection != NULL && selection_count == 1);
    show_detect_media = (selection != NULL && selection_count == 1);
    start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
    item_label = g_strdup_printf (ngettext ("New Folder with Selection (%'d Item)",
                                            "New Folder with Selection (%'d Items)",
                                            selection_count),
                                  selection_count);
    menu_item = g_menu_item_new (item_label, "view.new-folder-with-selection");
    g_menu_item_set_attribute (menu_item, "hidden-when", "s", "action-disabled");
    object = gtk_builder_get_object (builder, "new-folder-with-selection-section");
    g_menu_append_item (G_MENU (object), menu_item);
    g_object_unref (menu_item);
    g_free (item_label);

    /* Open With <App> menu item */
    show_extract = show_app = show_run = item_opens_in_view = selection_count != 0;
    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file;

        file = NAUTILUS_FILE (l->data);

        if (show_extract && !nautilus_mime_file_extracts (file))
        {
            show_extract = FALSE;
        }

        if (show_app && !nautilus_mime_file_opens_in_external_app (file))
        {
            show_app = FALSE;
        }

        if (show_run && !nautilus_mime_file_launches (file))
        {
            show_run = FALSE;
        }

        if (item_opens_in_view && !nautilus_file_opens_in_view (file))
        {
            item_opens_in_view = FALSE;
        }

        if (!show_extract && !show_app && !show_run && !item_opens_in_view)
        {
            break;
        }
    }

    item_label = NULL;
    app = NULL;
    if (show_app)
    {
        app = nautilus_mime_get_default_application_for_files (selection);
    }

    if (app != NULL)
    {
        g_autofree char *escaped_app = escape_underscores (g_app_info_get_name (app));
        item_label = g_strdup_printf (_("Open With %s"), escaped_app);

        app_icon = g_app_info_get_icon (app);
        if (app_icon != NULL)
        {
            g_object_ref (app_icon);
        }
        g_object_unref (app);
    }
    else if (show_run)
    {
        item_label = g_strdup (_("Run"));
    }
    else if (show_extract)
    {
        item_label = nautilus_files_view_supports_extract_here (view) ?
                     g_strdup (_("Extract")) :
                     g_strdup (_("Extract to…"));
    }
    else
    {
        item_label = g_strdup (_("Open"));
    }

    /* The action already exists in the submenu if item opens in view */
    if (!item_opens_in_view)
    {
        menu_item = g_menu_item_new (item_label, "view.open-with-default-application");
        if (app_icon != NULL)
        {
            g_menu_item_set_icon (menu_item, app_icon);
        }

        object = gtk_builder_get_object (builder, "open-with-application-section");
        g_menu_prepend_item (G_MENU (object), menu_item);

        g_object_unref (menu_item);
    }
    else
    {
        object = gtk_builder_get_object (builder, "open-with-application-section");
        i = nautilus_g_menu_model_find_by_string (G_MENU_MODEL (object),
                                                  "nautilus-menu-item",
                                                  "open_with_in_main_menu");
        g_menu_remove (G_MENU (object), i);
    }

    /* The "Open" submenu should be hidden if the item doesn't open in the view. */
    object = gtk_builder_get_object (builder, "open-with-application-section");
    i = nautilus_g_menu_model_find_by_string (G_MENU_MODEL (object),
                                              "nautilus-menu-item",
                                              "open_in_view_submenu");
    nautilus_g_menu_replace_string_in_item (G_MENU (object), i,
                                            "hidden-when",
                                            (!item_opens_in_view) ? "action-missing" : NULL);

    g_free (item_label);

    /* Drives */
    for (l = selection; l != NULL && (show_mount || show_unmount
                                      || show_eject
                                      || show_start || show_stop
                                      || show_detect_media);
         l = l->next)
    {
        NautilusFile *file;
        gboolean show_mount_one;
        gboolean show_unmount_one;
        gboolean show_eject_one;
        gboolean show_start_one;
        gboolean show_stop_one;
        gboolean show_detect_media_one;

        file = NAUTILUS_FILE (l->data);
        file_should_show_foreach (file,
                                  &show_mount_one,
                                  &show_unmount_one,
                                  &show_eject_one,
                                  &show_start_one,
                                  &show_stop_one,
                                  &show_detect_media_one,
                                  &start_stop_type);

        show_mount &= show_mount_one;
        show_unmount &= show_unmount_one;
        show_eject &= show_eject_one;
        show_start &= show_start_one;
        show_stop &= show_stop_one;
        show_detect_media &= show_detect_media_one;
    }

    if (show_start)
    {
        switch (start_stop_type)
        {
            default:
            case G_DRIVE_START_STOP_TYPE_UNKNOWN:
            case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
            {
                item_label = _("_Start");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_NETWORK:
            {
                item_label = _("_Connect");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_MULTIDISK:
            {
                item_label = _("_Start Multi-disk Drive");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_PASSWORD:
            {
                item_label = _("U_nlock Drive");
            }
            break;
        }

        menu_item = g_menu_item_new (item_label, "view.start-volume");
        object = gtk_builder_get_object (builder, "drive-section");
        g_menu_append_item (G_MENU (object), menu_item);
        g_object_unref (menu_item);
    }

    if (show_stop)
    {
        switch (start_stop_type)
        {
            default:
            case G_DRIVE_START_STOP_TYPE_UNKNOWN:
            {
                item_label = _("Stop Drive");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
            {
                item_label = _("_Safely Remove Drive");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_NETWORK:
            {
                item_label = _("_Disconnect");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_MULTIDISK:
            {
                item_label = _("_Stop Multi-disk Drive");
            }
            break;

            case G_DRIVE_START_STOP_TYPE_PASSWORD:
            {
                item_label = _("_Lock Drive");
            }
            break;
        }

        menu_item = g_menu_item_new (item_label, "view.stop-volume");
        object = gtk_builder_get_object (builder, "drive-section");
        g_menu_append_item (G_MENU (object), menu_item);
        g_object_unref (menu_item);
    }

    if (!priv->scripts_menu_updated)
    {
        update_scripts_menu (view, builder);
        priv->scripts_menu_updated = TRUE;
    }

    if (priv->scripts_menu != NULL)
    {
        show_scripts = TRUE;
        object = gtk_builder_get_object (builder, "scripts-submenu-section");
        nautilus_gmenu_set_from_model (G_MENU (object), priv->scripts_menu);
    }

    object = gtk_builder_get_object (builder, "open-with-application-section");
    i = nautilus_g_menu_model_find_by_string (G_MENU_MODEL (object),
                                              "nautilus-menu-item",
                                              "scripts-submenu");
    nautilus_g_menu_replace_string_in_item (G_MENU (object), i,
                                            "hidden-when",
                                            (!show_scripts) ? "action-missing" : NULL);

    if (NAUTILUS_IS_NETWORK_VIEW (priv->list_base))
    {
        object = gtk_builder_get_object (builder, "move-copy-section");
        g_menu_remove_all (G_MENU (object));

        object = gtk_builder_get_object (builder, "file-actions-section");
        g_menu_remove_all (G_MENU (object));
    }
    else
    {
        object = gtk_builder_get_object (builder, "network-view-section");
        g_menu_remove_all (G_MENU (object));
    }
}

static void
update_background_menu (NautilusFilesView *view,
                        GtkBuilder        *builder)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    GObject *object;
    gboolean remove_submenu = TRUE;
    gint i;

    if (nautilus_files_view_supports_creating_files (view) &&
        !showing_recent_directory (view) &&
        !showing_starred_directory (view))
    {
        if (!priv->templates_menu_updated)
        {
            update_templates_menu (view, builder);
            priv->templates_menu_updated = TRUE;
        }

        object = gtk_builder_get_object (builder, "templates-submenu");
        nautilus_gmenu_set_from_model (G_MENU (object), priv->templates_menu);

        if (priv->templates_menu != NULL)
        {
            remove_submenu = FALSE;
        }
    }
    else
    {
        /* This is necessary because the pathbar menu relies on it being NULL
         * to hide the submenu. */
        nautilus_view_set_templates_menu (NAUTILUS_VIEW (view), NULL);

        /* And this is necessary to regenerate the templates menu when we go
         * back to a normal folder. */
        priv->templates_menu_updated = FALSE;
    }

    i = nautilus_g_menu_model_find_by_string (G_MENU_MODEL (priv->background_menu_model),
                                              "nautilus-menu-item",
                                              "templates-submenu");
    nautilus_g_menu_replace_string_in_item (priv->background_menu_model, i,
                                            "hidden-when",
                                            remove_submenu ? "action-missing" : NULL);
}

static void
real_update_context_menus (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    g_autoptr (GtkBuilder) builder = NULL;
    GObject *object;

    priv = nautilus_files_view_get_instance_private (view);
    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-files-view-context-menus.ui");

    g_clear_object (&priv->background_menu_model);
    g_clear_object (&priv->selection_menu_model);

    object = gtk_builder_get_object (builder, "background-menu");
    priv->background_menu_model = g_object_ref (G_MENU (object));

    object = gtk_builder_get_object (builder, "selection-menu");
    priv->selection_menu_model = g_object_ref (G_MENU (object));

    update_selection_menu (view, builder);
    update_background_menu (view, builder);
    update_extensions_menus (view, builder);

    nautilus_files_view_update_actions_state (view);
}

/* Convenience function to reset the context menus owned by the view and update
 * them with the current state.
 * Children can subclass it and add items on the menu after chaining up to the
 * parent, so menus are already reseted.
 * It will also update the actions state, which will also update children
 * actions state if the children subclass nautilus_files_view_update_actions_state
 */
void
nautilus_files_view_update_context_menus (NautilusFilesView *view)
{
    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_context_menus (view);
}

static void
nautilus_files_view_reset_view_menu (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);
    NautilusFile *file = priv->directory_as_file;
    GMenuModel *sort_section = priv->toolbar_menu_sections->sort_section;
    const gchar *action;
    gint i;

    /* When not in the special location, set an inexistant action to hide the
     * menu item. This works under the assumptiont that the menu item has its
     * "hidden-when" attribute set to "action-disabled", and that an inexistant
     * action is treated as a disabled action. */
    action = nautilus_file_is_in_trash (file) ? "view.sort" : "doesnt-exist";
    i = nautilus_g_menu_model_find_by_string (sort_section, "nautilus-menu-item", "last_trashed");
    nautilus_g_menu_replace_string_in_item (G_MENU (sort_section), i, "action", action);

    action = nautilus_file_is_in_recent (file) ? "view.sort" : "doesnt-exist";
    i = nautilus_g_menu_model_find_by_string (sort_section, "nautilus-menu-item", "recency");
    nautilus_g_menu_replace_string_in_item (G_MENU (sort_section), i, "action", action);

    action = nautilus_file_is_in_search (file) ? "view.sort" : "doesnt-exist";
    i = nautilus_g_menu_model_find_by_string (sort_section, "nautilus-menu-item", "relevance");
    nautilus_g_menu_replace_string_in_item (G_MENU (sort_section), i, "action", action);
}

/* Convenience function to reset the menus owned by the view but managed on
 * the toolbar, and update them with the current state.
 * It will also update the actions state, which will also update children
 * actions state if the children subclass nautilus_files_view_update_actions_state
 */
void
nautilus_files_view_update_toolbar_menus (NautilusFilesView *view)
{
    NautilusWindow *window;
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    /* Don't update after destroy (#349551),
     * or if we are not active.
     */
    if (priv->in_destruction ||
        !priv->active)
    {
        return;
    }
    window = nautilus_files_view_get_window (view);
    nautilus_window_reset_menus (window);

    nautilus_files_view_update_actions_state (view);
    nautilus_files_view_reset_view_menu (view);
}

static void
nautilus_files_view_pop_up_selection_context_menu (NautilusFilesView *view,
                                                   graphene_point_t  *point)
{
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    /* Make the context menu items not flash as they update to proper disabled,
     * etc. states by forcing menus to update now.
     */
    update_context_menus_if_pending (view);

    /* Destroy old popover and create a new one, to avoid duplicate submenu bugs
     * and showing old model temporarily. We don't do this when popover is
     * closed because it wouldn't activate the actions then. */
    g_clear_pointer (&priv->selection_menu, gtk_widget_unparent);
    priv->selection_menu = gtk_popover_menu_new_from_model (NULL);

    /* There's something related to NautilusFilesView that isn't grabbing the
     * focus back when the popover is closed. Let's force it as a workaround. */
    g_signal_connect_object (priv->selection_menu, "closed",
                             G_CALLBACK (gtk_widget_grab_focus), view,
                             G_CONNECT_SWAPPED);
    gtk_widget_set_parent (priv->selection_menu, GTK_WIDGET (view));
    gtk_popover_set_has_arrow (GTK_POPOVER (priv->selection_menu), FALSE);
    gtk_widget_set_halign (priv->selection_menu, GTK_ALIGN_START);

    gtk_popover_menu_set_menu_model (GTK_POPOVER_MENU (priv->selection_menu),
                                     G_MENU_MODEL (priv->selection_menu_model));

    if (point == NULL)
    {
        /* If triggered from the keyboard, popup at selection, not pointer */
        GdkRectangle rectangle;

        if (!get_selected_rectangle (view, &rectangle))
        {
            g_return_if_reached ();
        }
        gtk_popover_set_pointing_to (GTK_POPOVER (priv->selection_menu),
                                     &rectangle);
    }
    else
    {
        gtk_popover_set_pointing_to (GTK_POPOVER (priv->selection_menu),
                                     &(GdkRectangle){ point->x, point->y, 0, 0 });
    }
    gtk_popover_popup (GTK_POPOVER (priv->selection_menu));
}

static void
nautilus_files_view_pop_up_background_context_menu (NautilusFilesView *view,
                                                    graphene_point_t  *point)
{
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    /* Make the context menu items not flash as they update to proper disabled,
     * etc. states by forcing menus to update now.
     */
    update_context_menus_if_pending (view);

    /* Destroy old popover and create a new one, to avoid duplicate submenu bugs
     * and showing old model temporarily. We don't do this when popover is
     * closed because it wouldn't activate the actions then. */
    g_clear_pointer (&priv->background_menu, gtk_widget_unparent);
    priv->background_menu = gtk_popover_menu_new_from_model (NULL);

    /* There's something related to NautilusFilesView that isn't grabbing the
     * focus back when the popover is closed. Let's force it as a workaround. */
    g_signal_connect_object (priv->background_menu, "closed",
                             G_CALLBACK (gtk_widget_grab_focus), view,
                             G_CONNECT_SWAPPED);
    gtk_widget_set_parent (priv->background_menu, GTK_WIDGET (view));
    gtk_popover_set_has_arrow (GTK_POPOVER (priv->background_menu), FALSE);
    gtk_widget_set_halign (priv->background_menu, GTK_ALIGN_START);

    gtk_popover_menu_set_menu_model (GTK_POPOVER_MENU (priv->background_menu),
                                     G_MENU_MODEL (priv->background_menu_model));

    gtk_popover_set_pointing_to (GTK_POPOVER (priv->background_menu),
                                 &(GdkRectangle){ point->x, point->y, 0, 0 });
    gtk_popover_popup (GTK_POPOVER (priv->background_menu));
}

static void
schedule_update_context_menus (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    /* Don't schedule updates after destroy (#349551),
     * or if we are not active.
     */
    if (priv->in_destruction ||
        !priv->active)
    {
        return;
    }

    /* Schedule a menu update with the current update interval */
    if (priv->update_context_menus_timeout_id == 0)
    {
        priv->update_context_menus_timeout_id
            = g_timeout_add (priv->update_interval, update_context_menus_timeout_callback, view);
    }
}

static void
remove_update_status_idle_callback (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->update_status_idle_id != 0)
    {
        g_source_remove (priv->update_status_idle_id);
        priv->update_status_idle_id = 0;
    }
}

static gboolean
update_status_idle_callback (gpointer data)
{
    NautilusFilesViewPrivate *priv;
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (data);
    priv = nautilus_files_view_get_instance_private (view);
    nautilus_files_view_display_selection_info (view);
    priv->update_status_idle_id = 0;
    return FALSE;
}

static void
schedule_update_status (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    /* Make sure we haven't already destroyed it */
    if (priv->in_destruction)
    {
        return;
    }

    if (priv->loading)
    {
        /* Don't update status bar while loading the dir */
        return;
    }

    if (priv->update_status_idle_id == 0)
    {
        priv->update_status_idle_id =
            g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
                             update_status_idle_callback, view, NULL);
    }
}

static void
nautilus_files_view_notify_selection_changed (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    GtkWindow *window;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    window = nautilus_files_view_get_containing_window (view);
    g_debug ("Selection changed in window %p", window);

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_autolist (NautilusFile) selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        nautilus_file_list_debug (selection);
    }

    priv->selection_was_removed = FALSE;

    /* Schedule a display of the new selection. */
    if (priv->display_selection_idle_id == 0)
    {
        priv->display_selection_idle_id
            = g_idle_add (display_selection_info_idle_callback,
                          view);
    }

    nautilus_files_view_update_actions_state (view);
    schedule_update_context_menus (view);
}

static void
file_changed_callback (NautilusFile *file,
                       gpointer      callback_data)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (callback_data);

    schedule_changes (view);

    schedule_update_context_menus (view);
    schedule_update_status (view);
}

static void
real_clear (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    nautilus_view_model_remove_all_items (priv->model);
}

static void
emit_clear (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    if (priv->search_transition_timeout_id != 0)
    {
        /* Scheduled to be emitted later. */
        return;
    }

    g_signal_emit (self, signals[CLEAR], 0);
}

static void
emit_begin_loading (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    if (priv->search_transition_timeout_id != 0)
    {
        /* Mark it to be emitted later, as we haven't cleared old contents yet. */
        priv->begin_loading_delayed = TRUE;
        return;
    }
    priv->begin_loading_delayed = FALSE;

    /* Tell interested parties that we've begun loading this directory now.
     * Subclasses use this to know that the new metadata is now available.
     */
    g_signal_emit (self, signals[BEGIN_LOADING], 0);
}

static void
search_transition_emit_delayed_signals (gpointer user_data)
{
    NautilusFilesView *self = NAUTILUS_FILES_VIEW (user_data);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    priv->search_transition_timeout_id = 0;

    emit_clear (self);

    if (priv->begin_loading_delayed)
    {
        emit_begin_loading (self);
    }
}

static void
search_transition_emit_delayed_signals_if_pending (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    if (priv->search_transition_timeout_id != 0)
    {
        g_clear_handle_id (&priv->search_transition_timeout_id, g_source_remove);
        search_transition_emit_delayed_signals (self);
    }
}

static void
search_transition_schedule_delayed_signals (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    if (priv->search_transition_timeout_id == 0)
    {
        guint id = g_timeout_add_once (SEARCH_TRANSITION_TIMEOUT,
                                       search_transition_emit_delayed_signals,
                                       self);

        priv->search_transition_timeout_id = id;
    }
}

/**
 * load_directory:
 *
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: NautilusFilesView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 *
 **/
static void
load_directory (NautilusFilesView *view,
                NautilusDirectory *directory)
{
    NautilusFileAttributes attributes;
    NautilusFilesViewPrivate *priv;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    priv = nautilus_files_view_get_instance_private (view);

    nautilus_files_view_stop_loading (view);

    /* To make search feel fast and smooth as if it were filtering the current
     * view, avoid blanking the view temporarily in the following cases:
     * 1- Going from a search to a search
     * 2- Going from a location to local search
     */
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory) &&
        (NAUTILUS_IS_SEARCH_DIRECTORY (priv->directory) ||
         (priv->search_query != NULL && !nautilus_query_is_global (priv->search_query))))
    {
        search_transition_schedule_delayed_signals (view);
    }

    emit_clear (view);

    priv->loading = TRUE;

    setup_loading_floating_bar (view);

    /* Update menus when directory is empty, before going to new
     * location, so they won't have any false lingering knowledge
     * of old selection.
     */
    schedule_update_context_menus (view);

    g_clear_pointer (&priv->subdirectories_loading, g_list_free);
    while (priv->subdirectory_list != NULL)
    {
        nautilus_files_view_remove_subdirectory (view,
                                                 priv->subdirectory_list->data);
    }

    /* Avoid freeing it and won't be able to ref it */
    if (priv->directory != directory)
    {
        nautilus_directory_unref (priv->directory);
        priv->directory = nautilus_directory_ref (directory);
    }

    nautilus_file_unref (priv->directory_as_file);
    priv->directory_as_file = nautilus_directory_get_corresponding_file (directory);

    g_clear_object (&priv->location);
    priv->location = nautilus_directory_get_location (directory);

    g_object_notify (G_OBJECT (view), "location");
    g_object_notify (G_OBJECT (view), "loading");
    g_object_notify (G_OBJECT (view), "searching");

    /* FIXME bugzilla.gnome.org 45062: In theory, we also need to monitor metadata here (as
     * well as doing a call when ready), in case external forces
     * change the directory's file metadata.
     */
    attributes =
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_MOUNT |
        NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
    priv->metadata_for_directory_as_file_pending = TRUE;
    priv->metadata_for_files_in_directory_pending = TRUE;
    nautilus_file_call_when_ready
        (priv->directory_as_file,
        attributes,
        metadata_for_directory_as_file_ready_callback, view);
    nautilus_directory_call_when_ready
        (priv->directory,
        attributes,
        FALSE,
        metadata_for_files_in_directory_ready_callback, view);

    /* If capabilities change, then we need to update the menus
     * because of New Folder, and relative emblems.
     */
    attributes =
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
    nautilus_file_monitor_add (priv->directory_as_file,
                               &priv->directory_as_file,
                               attributes);

    priv->file_changed_handler_id = g_signal_connect
                                        (priv->directory_as_file, "changed",
                                        G_CALLBACK (file_changed_callback), view);
}

static void
finish_loading (NautilusFilesView *view)
{
    NautilusFileAttributes attributes;
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    emit_begin_loading (view);

    nautilus_files_view_check_empty_states (view);

    if (nautilus_directory_are_all_files_seen (priv->directory))
    {
        /* Unschedule a pending update and schedule a new one with the minimal
         * update interval. This gives the view a short chance at gathering the
         * (cached) deep counts.
         */
        unschedule_display_of_pending_files (view);
        schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
    }

    /* Start loading. */

    /* Connect handlers to learn about loading progress. */
    priv->done_loading_handler_id = g_signal_connect (priv->directory, "done-loading",
                                                      G_CALLBACK (done_loading_callback), view);
    priv->load_error_handler_id = g_signal_connect (priv->directory, "load-error",
                                                    G_CALLBACK (load_error_callback), view);

    /* Monitor the things needed to get the right icon. Also
     * monitor a directory's item count because the "size"
     * attribute is based on that, and the file's metadata
     * and possible custom name.
     */
    attributes =
        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
        NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
        NAUTILUS_FILE_ATTRIBUTE_INFO |
        NAUTILUS_FILE_ATTRIBUTE_MOUNT |
        NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

    priv->files_added_handler_id = g_signal_connect
                                       (priv->directory, "files-added",
                                       G_CALLBACK (files_added_callback), view);
    priv->files_changed_handler_id = g_signal_connect
                                         (priv->directory, "files-changed",
                                         G_CALLBACK (files_changed_callback), view);

    nautilus_directory_file_monitor_add (priv->directory,
                                         &priv->directory,
                                         priv->show_hidden_files,
                                         attributes,
                                         files_added_callback, view);

    /* If escaping search we can release the search directory now that the view
     * is now monitoring the base directory directly. */
    g_clear_object (&priv->outgoing_search);
}

static void
finish_loading_if_all_metadata_loaded (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (!priv->metadata_for_directory_as_file_pending &&
        !priv->metadata_for_files_in_directory_pending)
    {
        finish_loading (view);
    }
}

static void
metadata_for_directory_as_file_ready_callback (NautilusFile *file,
                                               gpointer      callback_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = callback_data;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));
    priv = nautilus_files_view_get_instance_private (view);
    g_assert (priv->directory_as_file == file);
    g_assert (priv->metadata_for_directory_as_file_pending);

    priv->metadata_for_directory_as_file_pending = FALSE;

    finish_loading_if_all_metadata_loaded (view);
}

static void
metadata_for_files_in_directory_ready_callback (NautilusDirectory *directory,
                                                GList             *files,
                                                gpointer           callback_data)
{
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;

    view = callback_data;

    g_assert (NAUTILUS_IS_FILES_VIEW (view));
    priv = nautilus_files_view_get_instance_private (view);
    g_assert (priv->directory == directory);
    g_assert (priv->metadata_for_files_in_directory_pending);

    priv->metadata_for_files_in_directory_pending = FALSE;

    finish_loading_if_all_metadata_loaded (view);
}

static void
disconnect_directory_handlers (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory == NULL)
    {
        return;
    }
    g_clear_signal_handler (&priv->files_added_handler_id, priv->directory);
    g_clear_signal_handler (&priv->files_changed_handler_id, priv->directory);
    g_clear_signal_handler (&priv->done_loading_handler_id, priv->directory);
    g_clear_signal_handler (&priv->load_error_handler_id, priv->directory);
    g_clear_signal_handler (&priv->file_changed_handler_id, priv->directory_as_file);
    nautilus_file_cancel_call_when_ready (priv->directory_as_file,
                                          metadata_for_directory_as_file_ready_callback,
                                          view);
    nautilus_directory_cancel_callback (priv->directory,
                                        metadata_for_files_in_directory_ready_callback,
                                        view);
    nautilus_directory_file_monitor_remove (priv->directory,
                                            &priv->directory);
    nautilus_file_monitor_remove (priv->directory_as_file,
                                  &priv->directory_as_file);
}

/**
 * nautilus_files_view_stop_loading:
 *
 * Stop the current ongoing process, such as switching to a new uri.
 * @view: NautilusFilesView in question.
 *
 **/
void
nautilus_files_view_stop_loading (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

    priv = nautilus_files_view_get_instance_private (view);

    unschedule_display_of_pending_files (view);
    reset_update_interval (view);

    /* Free extra undisplayed files */
    g_list_free_full (priv->new_added_files, file_and_directory_free);
    priv->new_added_files = NULL;

    g_list_free_full (priv->new_changed_files, file_and_directory_free);
    priv->new_changed_files = NULL;

    g_list_free_full (priv->pending_selection, g_object_unref);
    priv->pending_selection = NULL;

    done_loading (view, FALSE);

    disconnect_directory_handlers (view);
}

static gboolean
nautilus_files_view_is_editable (NautilusFilesView *self)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    if (priv->directory != NULL)
    {
        return nautilus_directory_is_editable (priv->directory);
    }

    return TRUE;
}

static gboolean
nautilus_files_view_is_read_only (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    if (!nautilus_files_view_is_editable (view))
    {
        return TRUE;
    }

    if (priv->directory_as_file != NULL)
    {
        return !nautilus_file_can_write (priv->directory_as_file);
    }
    return FALSE;
}

/**
 * nautilus_files_view_should_show_file
 *
 * Returns whether or not this file should be displayed based on
 * current filtering options.
 */
static gboolean
nautilus_files_view_should_show_file (NautilusFilesView *view,
                                      NautilusFile      *file)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (view);

    return nautilus_file_should_show (file,
                                      priv->show_hidden_files);
}

static char *
nautilus_files_view_get_uri (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    priv = nautilus_files_view_get_instance_private (view);

    if (priv->directory == NULL)
    {
        return NULL;
    }
    return nautilus_directory_get_uri (priv->directory);
}

void nautilus_file_view_save_image_from_texture (NautilusFilesView *view,
                                                 GdkTexture        *texture,
                                                 const char        *dest_uri,
                                                 const char        *base_name)
{
    nautilus_file_operations_save_image_from_texture (GTK_WIDGET (view), NULL,
                                                      dest_uri,
                                                      base_name,
                                                      texture,
                                                      copy_move_done_callback,
                                                      pre_copy_move (view));
}

void
nautilus_files_view_move_copy_items (NautilusFilesView *view,
                                     const GList       *item_uris,
                                     const char        *target_uri,
                                     int                copy_action)
{
    NautilusFile *target_file;

    target_file = nautilus_file_get_existing_by_uri (target_uri);
    if (copy_action == GDK_ACTION_COPY &&
        nautilus_is_file_roller_installed () &&
        target_file != NULL &&
        nautilus_file_is_archive (target_file))
    {
        char *command, *quoted_uri, *tmp;
        const GList *l;
        GdkDisplay *display;

        /* Handle dropping onto a file-roller archiver file, instead of starting a move/copy */

        nautilus_file_unref (target_file);

        quoted_uri = g_shell_quote (target_uri);
        command = g_strconcat ("file-roller -a ", quoted_uri, NULL);
        g_free (quoted_uri);

        for (l = item_uris; l != NULL; l = l->next)
        {
            quoted_uri = g_shell_quote ((char *) l->data);

            tmp = g_strconcat (command, " ", quoted_uri, NULL);
            g_free (command);
            command = tmp;

            g_free (quoted_uri);
        }

        display = gtk_widget_get_display (GTK_WIDGET (view));
        if (display == NULL)
        {
            display = gdk_display_get_default ();
        }

        nautilus_launch_application_from_command (display, command, FALSE, NULL);
        g_free (command);

        return;
    }
    else if (copy_action == GDK_ACTION_MOVE)
    {
        nautilus_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
                                                    item_uris);
    }
    nautilus_file_unref (target_file);

    nautilus_file_operations_copy_move
        (item_uris,
        target_uri, copy_action, GTK_WIDGET (view),
        NULL,
        copy_move_done_callback, pre_copy_move (view));
}

static void
nautilus_files_view_trash_state_changed_callback (NautilusTrashMonitor *trash_monitor,
                                                  gboolean              state,
                                                  gpointer              callback_data)
{
    NautilusFilesView *view;

    view = (NautilusFilesView *) callback_data;
    g_assert (NAUTILUS_IS_FILES_VIEW (view));

    schedule_update_context_menus (view);
}

static void
nautilus_files_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
    NautilusFilesView *view = NAUTILUS_FILES_VIEW (object);
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (view);

    switch (prop_id)
    {
        case PROP_LOADING:
        {
            g_value_set_boolean (value, nautilus_view_is_loading (NAUTILUS_VIEW (view)));
        }
        break;

        case PROP_SEARCHING:
        {
            g_value_set_boolean (value, nautilus_view_is_searching (NAUTILUS_VIEW (view)));
        }
        break;

        case PROP_LOCATION:
        {
            g_value_set_object (value, nautilus_view_get_location (NAUTILUS_VIEW (view)));
        }
        break;

        case PROP_SELECTION:
        {
            g_value_set_pointer (value, nautilus_view_get_selection (NAUTILUS_VIEW (view)));
        }
        break;

        case PROP_SEARCH_QUERY:
        {
            g_value_set_object (value, priv->search_query);
        }
        break;

        case PROP_EXTENSIONS_BACKGROUND_MENU:
        {
            g_value_set_object (value,
                                real_get_extensions_background_menu (NAUTILUS_VIEW (view)));
        }
        break;

        case PROP_TEMPLATES_MENU:
        {
            g_value_set_object (value,
                                real_get_templates_menu (NAUTILUS_VIEW (view)));
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }
}

static void
nautilus_files_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    NautilusFilesView *directory_view;
    NautilusFilesViewPrivate *priv;
    NautilusWindowSlot *slot;

    directory_view = NAUTILUS_FILES_VIEW (object);
    priv = nautilus_files_view_get_instance_private (directory_view);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_assert (priv->slot == NULL);

            slot = NAUTILUS_WINDOW_SLOT (g_value_get_object (value));
            priv->slot = slot;

            g_signal_connect_object (priv->slot,
                                     "notify::active", G_CALLBACK (slot_active_changed),
                                     directory_view, 0);
        }
        break;

        case PROP_LOCATION:
        {
            nautilus_view_set_location (NAUTILUS_VIEW (directory_view), g_value_get_object (value));
        }
        break;

        case PROP_SEARCH_QUERY:
        {
            nautilus_view_set_search_query (NAUTILUS_VIEW (directory_view), g_value_get_object (value));
        }
        break;

        case PROP_SELECTION:
        {
            nautilus_view_set_selection (NAUTILUS_VIEW (directory_view), g_value_get_pointer (value));
        }
        break;

        case PROP_EXTENSIONS_BACKGROUND_MENU:
        {
            real_set_extensions_background_menu (NAUTILUS_VIEW (directory_view),
                                                 g_value_get_object (value));
        }
        break;

        case PROP_TEMPLATES_MENU:
        {
            real_set_templates_menu (NAUTILUS_VIEW (directory_view),
                                     g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
on_parent_changed (GObject    *object,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
    GtkWidget *widget;
    NautilusWindow *window;
    NautilusFilesView *view;
    NautilusFilesViewPrivate *priv;
    GtkWidget *parent;

    widget = GTK_WIDGET (object);
    view = NAUTILUS_FILES_VIEW (object);
    priv = nautilus_files_view_get_instance_private (view);

    parent = gtk_widget_get_parent (widget);
    window = nautilus_files_view_get_window (view);

    if (parent != NULL)
    {
        if (priv->slot == nautilus_window_get_active_slot (window))
        {
            priv->active = TRUE;
            gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                            "view",
                                            G_ACTION_GROUP (priv->view_action_group));
        }
    }
    else
    {
        remove_update_context_menus_timeout_callback (view);
        /* Only remove the action group if this is still the active view.
         * Otherwise we might be removing an action group set by a different
         * view i.e. if slot_active_changed() is called before this one.
         */
        if (priv->active)
        {
            gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                            "view",
                                            NULL);
        }
    }
}

static NautilusQuery *
nautilus_files_view_get_search_query (NautilusView *view)
{
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (NAUTILUS_FILES_VIEW (view));

    return priv->search_query;
}

static void
nautilus_files_view_set_search_query (NautilusView  *view,
                                      NautilusQuery *query)
{
    NautilusFilesView *files_view = NAUTILUS_FILES_VIEW (view);
    g_autoptr (GFile) location = NULL;
    NautilusFilesViewPrivate *priv;

    priv = nautilus_files_view_get_instance_private (files_view);

    if (g_set_object (&priv->search_query, query))
    {
        g_object_notify (G_OBJECT (view), "search-query");
    }

    if (!nautilus_query_is_empty (query))
    {
        if (nautilus_view_is_searching (view))
        {
            /*
             * Reuse the search directory and reload it.
             */
            nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (priv->directory), query);
            load_directory (files_view, priv->directory);
        }
        else
        {
            NautilusDirectory *directory;
            gchar *uri;

            uri = nautilus_search_directory_generate_new_uri ();
            location = g_file_new_for_uri (uri);

            directory = nautilus_directory_get (location);
            g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));
            nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory), query);

            g_set_object (&priv->location_before_search, priv->location);
            if (priv->location_before_search == NULL)
            {
                /* This may happen if switching view mode while searching, as
                 * the new view doesn't have a location. In such cases, we can
                 * assume the location before search from the query, if not NULL.
                 */
                g_autoptr (GFile) queried_location = nautilus_query_get_location (query);
                g_set_object (&priv->location_before_search, queried_location);
            }

            load_directory (files_view, directory);

            g_object_notify (G_OBJECT (view), "searching");

            nautilus_directory_unref (directory);
            g_free (uri);
        }
    }
    else
    {
        if (nautilus_view_is_searching (view))
        {
            location = g_steal_pointer (&priv->location_before_search);

            if (G_UNLIKELY (location == NULL))
            {
                g_warn_if_reached ();
                location = g_file_new_for_path (g_get_home_dir ());
                nautilus_window_slot_open_location_full (priv->slot, location, 0, NULL);
                return;
            }

            /* OPTIMIZATION: Fast and cheap loading coming back from search.
             *
             * The search directory has been monitoring its base directory. This
             * means the later's file list is already loaded and up-to-date. By
             * keeping it continuously monitored, we can avoid reloading the
             * directory from scratch, which saves time and disk/network usage.
             *
             * So, we need to keep the search directory alive until after the
             * view sets up its own monitor. This can be achieved by holding
             * adding a reference to the search directory, preventing its
             * destruction during the location change.
             */
            priv->outgoing_search = g_object_ref (priv->directory);

            nautilus_view_set_location (view, location);
        }
    }
}

static GFile *
nautilus_files_view_get_location (NautilusView *view)
{
    NautilusFilesViewPrivate *priv;
    NautilusFilesView *files_view;

    files_view = NAUTILUS_FILES_VIEW (view);
    priv = nautilus_files_view_get_instance_private (files_view);

    return priv->location;
}

static gboolean
nautilus_files_view_is_loading (NautilusView *view)
{
    NautilusFilesViewPrivate *priv;
    NautilusFilesView *files_view;

    files_view = NAUTILUS_FILES_VIEW (view);
    priv = nautilus_files_view_get_instance_private (files_view);

    return priv->loading;
}

static void
nautilus_files_view_iface_init (NautilusViewInterface *iface)
{
    iface->get_location = nautilus_files_view_get_location;
    iface->set_location = nautilus_files_view_set_location;
    iface->get_selection = nautilus_files_view_get_selection;
    iface->set_selection = nautilus_files_view_set_selection;
    iface->get_search_query = nautilus_files_view_get_search_query;
    iface->set_search_query = nautilus_files_view_set_search_query;
    iface->get_toolbar_menu_sections = nautilus_files_view_get_toolbar_menu_sections;
    iface->is_searching = nautilus_files_view_is_searching;
    iface->is_loading = nautilus_files_view_is_loading;
    iface->get_view_id = nautilus_files_view_get_view_id;
    iface->get_templates_menu = nautilus_files_view_get_templates_menu;
    iface->set_templates_menu = nautilus_files_view_set_templates_menu;
    iface->get_extensions_background_menu = nautilus_files_view_get_extensions_background_menu;
    iface->set_extensions_background_menu = nautilus_files_view_set_extensions_background_menu;
}

static void
nautilus_files_view_class_init (NautilusFilesViewClass *klass)
{
    GObjectClass *oclass;
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = nautilus_files_view_dispose;
    oclass->finalize = nautilus_files_view_finalize;
    oclass->get_property = nautilus_files_view_get_property;
    oclass->set_property = nautilus_files_view_set_property;

    widget_class->grab_focus = nautilus_files_view_grab_focus;


    signals[ADD_FILES] =
        g_signal_new ("add-files",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, add_files),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[BEGIN_FILE_CHANGES] =
        g_signal_new ("begin-file-changes",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, begin_file_changes),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[BEGIN_LOADING] =
        g_signal_new ("begin-loading",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, begin_loading),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[CLEAR] =
        g_signal_new ("clear",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, clear),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[END_FILE_CHANGES] =
        g_signal_new ("end-file-changes",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, end_file_changes),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[END_LOADING] =
        g_signal_new ("end-loading",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, end_loading),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
    signals[FILE_CHANGED] =
        g_signal_new ("file-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, file_changed),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
    signals[REMOVE_FILES] =
        g_signal_new ("remove-file",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFilesViewClass, remove_files),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 2, G_TYPE_POINTER /* GList<NautilusFile> */, NAUTILUS_TYPE_DIRECTORY);
    signals[SELECTION_CHANGED] =
        g_signal_new ("selection-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    klass->clear = real_clear;
    klass->add_files = real_add_files;
    klass->remove_files = real_remove_files;
    klass->file_changed = real_file_changed;
    klass->end_file_changes = real_end_file_changes;
    klass->begin_loading = real_begin_loading;
    klass->end_loading = real_end_loading;
    klass->update_context_menus = real_update_context_menus;
    klass->update_actions_state = real_update_actions_state;
    klass->check_empty_states = real_check_empty_states;

    g_object_class_install_property (
        oclass,
        PROP_WINDOW_SLOT,
        g_param_spec_object ("window-slot",
                             "Window Slot",
                             "The parent window slot reference",
                             NAUTILUS_TYPE_WINDOW_SLOT,
                             G_PARAM_WRITABLE |
                             G_PARAM_CONSTRUCT_ONLY));

    g_object_class_override_property (oclass, PROP_LOADING, "loading");
    g_object_class_override_property (oclass, PROP_SEARCHING, "searching");
    g_object_class_override_property (oclass, PROP_LOCATION, "location");
    g_object_class_override_property (oclass, PROP_SELECTION, "selection");
    g_object_class_override_property (oclass, PROP_SEARCH_QUERY, "search-query");
    g_object_class_override_property (oclass, PROP_EXTENSIONS_BACKGROUND_MENU, "extensions-background-menu");
    g_object_class_override_property (oclass, PROP_TEMPLATES_MENU, "templates-menu");

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-files-view.ui");

    gtk_widget_class_bind_template_child_private (widget_class, NautilusFilesView, overlay);
    gtk_widget_class_bind_template_child_private (widget_class, NautilusFilesView, floating_bar);

    /* See also the global accelerators in init() in addition to all the local
     * ones defined below.
     */

    /* Only one delete action is enabled at a time, so we can just activate several
     * delete or trash actions with the same shortcut without worrying: only the
     * enabled one will be activated.
     */
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, GDK_SHIFT_MASK, "view.delete-permanently-shortcut", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK, "view.delete-permanently-shortcut", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, GDK_SHIFT_MASK, "view.permanent-delete-permanently-menu-item", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK, "view.permanent-delete-permanently-menu-item", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, 0, "view.move-to-trash", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, 0, "view.move-to-trash", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, 0, "view.delete-from-trash", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, 0, "view.delete-from-trash", NULL);
    /* When trash is not available, allow the "Delete" keys to delete permanently, that is, when
     * the menu item is available, since we never make both the trash and delete-permanently-menu-item
     * actions active.
     */
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, 0, "view.delete-permanently-menu-item", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, 0, "view.delete-permanently-menu-item", NULL);

    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F2, 0, "view.rename", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Menu, 0, "view.popup-menu", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_F10, GDK_SHIFT_MASK, "view.popup-menu", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_o, GDK_CONTROL_MASK, "view.open-with-default-application", NULL);
    /* This is not necessary per-se, because it's the default activation
     * keybinding. But in order for it to appear in the context menu as a
     * keyboard shortcut, we need to bind it to the menu item action here. */
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, 0, "view.open-with-default-application", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_i, GDK_CONTROL_MASK, "view.properties", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, GDK_ALT_MASK, "view.properties", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_a, GDK_CONTROL_MASK, "view.select-all", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_i, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "view.invert-selection", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_m, GDK_CONTROL_MASK, "view.create-link", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_m, GDK_CONTROL_MASK, "view.create-link-shortcut", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_m, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "view.create-link-in-place", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_m, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "view.create-link-in-place-shortcut", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, GDK_CONTROL_MASK, "view.open-item-new-tab", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Return, GDK_SHIFT_MASK, "view.open-item-new-window", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_o, GDK_CONTROL_MASK | GDK_ALT_MASK, "view.open-item-location", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_c, GDK_CONTROL_MASK, "view.copy", NULL);
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_x, GDK_CONTROL_MASK, "view.cut", NULL);
}

static void
nautilus_files_view_init (NautilusFilesView *view)
{
    NautilusFilesViewPrivate *priv;
    GtkBuilder *builder;
    NautilusDirectory *scripts_directory;
    NautilusDirectory *templates_directory;
    GtkEventController *controller;
    GtkShortcut *shortcut;
    gchar *templates_uri;
    GdkClipboard *clipboard;
    GApplication *app;
    const gchar *zoom_in_accels[] =
    {
        "<control>equal",
        "<control>plus",
        "<control>KP_Add",
        "ZoomIn",
        NULL
    };
    const gchar *zoom_out_accels[] =
    {
        "<control>minus",
        "<control>KP_Subtract",
        "ZoomOut",
        NULL
    };
    const gchar *zoom_standard_accels[] =
    {
        "<control>0",
        "<control>KP_0",
        NULL
    };

    priv = nautilus_files_view_get_instance_private (view);

    /* Own a reference to keep the status page alive even if reparented while
     * switching view modes. */
    priv->empty_view_page = g_object_ref_sink (adw_status_page_new ());
    /* Ensure opaque background, to hide the view underneath it. */
    gtk_widget_add_css_class (priv->empty_view_page, "view");

    priv->model = nautilus_view_model_new ();
    g_signal_connect_object (GTK_SELECTION_MODEL (priv->model),
                             "selection-changed",
                             G_CALLBACK (nautilus_files_view_notify_selection_changed),
                             view,
                             G_CONNECT_SWAPPED);

    /* Toolbar menu */
    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-toolbar-view-menu.ui");
    priv->toolbar_menu_sections = g_new0 (NautilusToolbarMenuSections, 1);
    priv->toolbar_menu_sections->sort_section = G_MENU_MODEL (g_object_ref (gtk_builder_get_object (builder, "sort_section")));

    g_signal_connect (view,
                      "notify::selection",
                      G_CALLBACK (nautilus_files_view_preview_update),
                      view);
    g_signal_connect (view,
                      "notify::parent",
                      G_CALLBACK (on_parent_changed),
                      NULL);

    g_object_unref (builder);

    g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
    gtk_widget_init_template (GTK_WIDGET (view));

    g_signal_connect (priv->floating_bar,
                      "stop",
                      G_CALLBACK (floating_bar_stop_cb),
                      view);

    priv->pending_reveal = g_hash_table_new (NULL, NULL);

    if (set_up_scripts_directory_global ())
    {
        scripts_directory = nautilus_directory_get_by_uri (scripts_directory_uri);
        add_directory_to_scripts_directory_list (view, scripts_directory);
        nautilus_directory_unref (scripts_directory);
    }
    else
    {
        g_warning ("Ignoring scripts directory, it may be a broken link\n");
    }

    if (nautilus_should_use_templates_directory ())
    {
        templates_uri = nautilus_get_templates_directory_uri ();
        templates_directory = nautilus_directory_get_by_uri (templates_uri);
        g_free (templates_uri);
        add_directory_to_templates_directory_list (view, templates_directory);
        nautilus_directory_unref (templates_directory);
    }
    update_templates_directory (view);

    priv->show_hidden_files =
        g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

    g_signal_connect_object (nautilus_trash_monitor_get (), "trash-state-changed",
                             G_CALLBACK (nautilus_files_view_trash_state_changed_callback), view, 0);

    /* React to clipboard changes */
    clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
    g_signal_connect (clipboard, "changed",
                      G_CALLBACK (on_clipboard_owner_changed), view);

    /* Register to menu provider extension signal managing menu updates */
    g_signal_connect_object (nautilus_signaller_get_current (), "popup-menu-changed",
                             G_CALLBACK (schedule_update_context_menus), view, G_CONNECT_SWAPPED);

    g_signal_connect_object (nautilus_preferences,
                             "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
                             G_CALLBACK (update_sort_order_from_metadata_and_preferences),
                             view,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (nautilus_preferences,
                             "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                             G_CALLBACK (update_sort_order_from_metadata_and_preferences),
                             view,
                             G_CONNECT_SWAPPED);
    g_signal_connect_swapped (gtk_filechooser_preferences,
                              "changed::" NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                              G_CALLBACK (show_hidden_files_changed_callback), view);
    g_signal_connect_swapped (gnome_lockdown_preferences,
                              "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                              G_CALLBACK (schedule_update_context_menus), view);

    priv->in_destruction = FALSE;

    priv->view_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (G_ACTION_MAP (priv->view_action_group),
                                     view_entries,
                                     G_N_ELEMENTS (view_entries),
                                     view);
    gtk_widget_insert_action_group (GTK_WIDGET (view),
                                    "view",
                                    G_ACTION_GROUP (priv->view_action_group));
    g_signal_connect_object (priv->view_action_group, "action-state-changed::sort",
                             G_CALLBACK (on_sort_action_state_changed), view, 0);

    app = g_application_get_default ();

    /* NOTE: Please do not add any key here that could interfere with
     * the rest of the app's use of those keys. Some example of keys set here
     * that broke keynav include Enter/Return, Menu, F2 and Delete keys.
     * The accelerators below are set on the whole app level for the sole purpose
     * of making it more convenient when you don't have the focus exactly on the
     * files view, but some keys are used in a contextual way, and those should
     * should be added in nautilus_files_view_class_init() above instead of a
     * global accelerator, unless it really makes sense to have them globally
     * (e.g. Zoom in/out shortcuts).
     */
    nautilus_application_set_accelerators (app, "view.zoom-in", zoom_in_accels);
    nautilus_application_set_accelerators (app, "view.zoom-out", zoom_out_accels);
    nautilus_application_set_accelerator (app, "view.show-hidden-files", "<control>h");
    /* Despite putting copy/cut at the widget scope instead of the global one,
     * we're putting paste globally so that it's easy to switch between apps
     * with e.g. Alt+Tab and paste directly the copied file without having to
     * make sure the focus is on the files view.
     */
    nautilus_application_set_accelerator (app, "view.paste_accel", "<control>v");
    nautilus_application_set_accelerator (app, "view.new-folder", "<control><shift>n");
    nautilus_application_set_accelerator (app, "view.select-pattern", "<control>s");
    nautilus_application_set_accelerators (app, "view.zoom-standard", zoom_standard_accels);

    /* This one should have been a keybinding, because it should trigger only
     * when the view is focused. Unfortunately, children can override bindings,
     * and such is the case of GtkListItemWidget which binds the spacebar to its
     * `|listitem.select` action.
     *
     * So, we make it a local shortcut (like keybindings are), but using the
     * capture phase instead, to trigger it first (keybindings use bubble phase).
     */
    shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_space, 0),
                                 gtk_named_action_new ("view.preview-selection"));

    controller = gtk_shortcut_controller_new ();
    gtk_widget_add_controller (GTK_WIDGET (view), controller);
    /* By default, :scope is GTK_SHORTCUT_SCOPE_LOCAL, so no need to set it. */
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

    priv->starred_cancellable = g_cancellable_new ();
    priv->clipboard_cancellable = g_cancellable_new ();

    priv->rename_file_popover = nautilus_rename_file_popover_new ();
    gtk_widget_set_parent (priv->rename_file_popover, GTK_WIDGET (view));
}

static void
create_inner_view (NautilusFilesView *self,
                   guint              id)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);
    switch (id)
    {
        case NAUTILUS_VIEW_GRID_ID:
        {
            priv->list_base = NAUTILUS_LIST_BASE (nautilus_grid_view_new ());
        }
        break;

        case NAUTILUS_VIEW_LIST_ID:
        {
            priv->list_base = NAUTILUS_LIST_BASE (nautilus_list_view_new ());
        }
        break;

        case NAUTILUS_VIEW_NETWORK_ID:
        {
            priv->list_base = NAUTILUS_LIST_BASE (nautilus_network_view_new ());
        }
        break;

        default:
        {
            g_critical ("Unknown view type ID: %d. Falling back to list.", id);
            priv->list_base = NAUTILUS_LIST_BASE (nautilus_list_view_new ());
        }
    }

    gtk_overlay_set_child (GTK_OVERLAY (priv->overlay), GTK_WIDGET (priv->list_base));
}

void
nautilus_files_view_change (NautilusFilesView *self,
                            guint              id)
{
    NautilusFilesViewPrivate *priv = nautilus_files_view_get_instance_private (self);

    /* Prepare empty page for reuse. It's not destroyed because we own it. */
    gtk_widget_unparent (priv->empty_view_page);

    /* Destroy existing inner view (which is owned by the overlay) */
    gtk_overlay_set_child (GTK_OVERLAY (priv->overlay), NULL);

    /* Avoid subfolder items showing up in grid view. */
    nautilus_view_model_expand_as_a_tree (priv->model, FALSE);
    g_clear_pointer (&priv->subdirectories_loading, g_list_free);
    while (priv->subdirectory_list != NULL)
    {
        nautilus_files_view_remove_subdirectory (self,
                                                 priv->subdirectory_list->data);
    }

    /* Create a new one */
    create_inner_view (self, id);

    /* Prepare directory settings before connecting the model. */
    nautilus_list_base_setup_directory (priv->list_base, priv->directory);

    connect_inner_view (self);

    /* Update actions, because some of them may depend on the view (e.g. zoom
     * actions and visible-columns). At the time of writing, there is no reason
     * to also update context menus. */
    nautilus_files_view_update_actions_state (self);
}

NautilusFilesView *
nautilus_files_view_new (guint               id,
                         NautilusWindowSlot *slot)
{
    NautilusFilesView *view = NULL;

    view = NAUTILUS_FILES_VIEW (g_object_new (NAUTILUS_TYPE_FILES_VIEW,
                                              "window-slot", slot,
                                              NULL));

    if (view == NULL)
    {
        g_assert_not_reached ();
    }
    else if (g_object_is_floating (view))
    {
        g_object_ref_sink (view);
    }

    create_inner_view (view, id);
    connect_inner_view (view);

    return view;
}
