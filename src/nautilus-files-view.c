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

#include <config.h>

#include "nautilus-files-view.h"

#include "nautilus-application.h"
#include "nautilus-desktop-canvas-view.h"
#include "nautilus-error-reporting.h"
#include "nautilus-floating-bar.h"
#include "nautilus-list-view.h"
#include "nautilus-mime-actions.h"
#include "nautilus-previewer.h"
#include "nautilus-properties-window.h"
#include "nautilus-window.h"
#include "nautilus-toolbar.h"
#include "nautilus-view.h"

#if ENABLE_EMPTY_VIEW
#include "nautilus-empty-view.h"
#endif

#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-desktop-directory.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-changes-queue.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-private.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-icon-names.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <gdesktop-enums.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_DIRECTORY_VIEW
#include <libnautilus-private/nautilus-debug.h>

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

#define RENAME_ENTRY_MIN_CHARS 20
#define RENAME_ENTRY_MAX_CHARS 35

#define MAX_QUEUED_UPDATES 500

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30

#define SHORTCUTS_PATH "/nautilus/scripts-accels"

/* Delay to show the duplicated label when creating a folder */
#define FILE_NAME_DUPLICATED_LABEL_TIMEOUT 500

/* Delay to show the Loading... floating bar */
#define FLOATING_BAR_LOADING_DELAY 200 /* ms */

enum {
        ADD_FILE,
        BEGIN_FILE_CHANGES,
        BEGIN_LOADING,
        CLEAR,
        END_FILE_CHANGES,
        END_LOADING,
        FILE_CHANGED,
        MOVE_COPY_ITEMS,
        REMOVE_FILE,
        SELECTION_CHANGED,
        TRASH,
        DELETE,
        LAST_SIGNAL
};

enum {
        PROP_WINDOW_SLOT = 1,
        PROP_SUPPORTS_ZOOMING,
        PROP_ICON,
        PROP_VIEW_WIDGET,
        PROP_IS_SEARCH,
        PROP_IS_LOADING,
        PROP_LOCATION,
        PROP_SEARCH_QUERY,
        NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];

static GdkAtom copied_files_atom;

static char *scripts_directory_uri = NULL;
static int scripts_directory_uri_length;

static GHashTable *script_accels = NULL;

struct NautilusFilesViewDetails
{
        /* Main components */
        GtkWidget *overlay;

        NautilusWindowSlot *slot;
        NautilusDirectory *model;
        NautilusFile *directory_as_file;
        GFile *location;
        guint dir_merge_id;

        NautilusQuery *search_query;

        gint duplicated_label_timeout_id;
        GtkWidget *rename_file_popover;

        gboolean supports_zooming;

        GList *scripts_directory_list;
        GList *templates_directory_list;

        guint display_selection_idle_id;
        guint update_context_menus_timeout_id;
        guint update_status_idle_id;
        guint reveal_selection_idle_id;

        guint display_pending_source_id;
        guint changes_timeout_id;

        guint update_interval;
        guint64 last_queued;

        guint files_added_handler_id;
        guint files_changed_handler_id;
        guint load_error_handler_id;
        guint done_loading_handler_id;
        guint file_changed_handler_id;

        GList *new_added_files;
        GList *new_changed_files;

        GHashTable *non_ready_files;

        GList *old_added_files;
        GList *old_changed_files;

        GList *pending_selection;

        /* whether we are in the active slot */
        gboolean active;

        /* loading indicates whether this view has begun loading a directory.
         * This flag should need not be set inside subclasses. NautilusFilesView automatically
         * sets 'loading' to TRUE before it begins loading a directory's contents and to FALSE
         * after it finishes loading the directory and its view.
         */
        gboolean loading;
        gboolean templates_present;
        gboolean scripts_present;

        gboolean in_destruction;

        gboolean sort_directories_first;

        gboolean show_foreign_files;
        gboolean show_hidden_files;
        gboolean ignore_hidden_file_preferences;

        gboolean batching_selection_level;
        gboolean selection_changed_while_batched;

        gboolean selection_was_removed;

        gboolean metadata_for_directory_as_file_pending;
        gboolean metadata_for_files_in_directory_pending;

        GList *subdirectory_list;

        GdkPoint context_menu_position;

        GMenu *selection_menu;
        GMenu *background_menu;

        GActionGroup *view_action_group;

        GtkWidget *scrolled_window;

        /* Empty states */
        GtkWidget *folder_is_empty_widget;
        GtkWidget *no_search_results_widget;

        /* Floating bar */
        guint floating_bar_set_status_timeout_id;
        guint floating_bar_loading_timeout_id;
        GtkWidget *floating_bar;

        /* View menu */
        GtkWidget *view_menu_widget;
        GtkWidget *view_icon;
        GtkWidget *sort_menu;
        GtkWidget *sort_trash_time;
        GtkWidget *sort_search_relevance;
        GtkWidget *visible_columns;
        GtkWidget *stop;
        GtkWidget *reload;
        GtkAdjustment *zoom_adjustment;
        GtkWidget *zoom_level_scale;

        gulong stop_signal_handler;
        gulong reload_signal_handler;
};

typedef struct {
        NautilusFile *file;
        NautilusDirectory *directory;
} FileAndDirectory;

/* forward declarations */

static gboolean display_selection_info_idle_callback           (gpointer              data);
static void     trash_or_delete_files                          (GtkWindow            *parent_window,
                                                                const GList          *files,
                                                                NautilusFilesView      *view);
static void     load_directory                                 (NautilusFilesView      *view,
                                                                NautilusDirectory    *directory);
static void     clipboard_changed_callback                     (NautilusClipboardMonitor *monitor,
                                                                NautilusFilesView      *view);
static void     open_one_in_new_window                         (gpointer              data,
                                                                gpointer              callback_data);
static void     schedule_update_context_menus                  (NautilusFilesView      *view);
static void     remove_update_context_menus_timeout_callback   (NautilusFilesView      *view);
static void     schedule_update_status                          (NautilusFilesView      *view);
static void     remove_update_status_idle_callback             (NautilusFilesView *view);
static void     reset_update_interval                          (NautilusFilesView      *view);
static void     schedule_idle_display_of_pending_files         (NautilusFilesView      *view);
static void     unschedule_display_of_pending_files            (NautilusFilesView      *view);
static void     disconnect_model_handlers                      (NautilusFilesView      *view);
static void     metadata_for_directory_as_file_ready_callback  (NautilusFile         *file,
                                                                gpointer              callback_data);
static void     metadata_for_files_in_directory_ready_callback (NautilusDirectory    *directory,
                                                                GList                *files,
                                                                gpointer              callback_data);
static void     nautilus_files_view_trash_state_changed_callback     (NautilusTrashMonitor *trash,
                                                                gboolean              state,
                                                                gpointer              callback_data);
static void     nautilus_files_view_select_file                      (NautilusFilesView      *view,
                                                                NautilusFile         *file);

static void     update_templates_directory                     (NautilusFilesView *view);

static void     check_empty_states                             (NautilusFilesView *view);

static gboolean nautilus_files_view_is_searching               (NautilusView      *view);

static void     nautilus_files_view_iface_init                 (NautilusViewInterface *view);

static void     set_search_query_internal                      (NautilusFilesView *files_view,
                                                                NautilusQuery     *query,
                                                                NautilusDirectory *base_model);

static gboolean nautilus_files_view_is_read_only               (NautilusFilesView *view);

G_DEFINE_TYPE_WITH_CODE (NautilusFilesView,
                         nautilus_files_view,
                         GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW, nautilus_files_view_iface_init));

static const struct {
        unsigned int keyval;
        const char *action;
} extra_view_keybindings [] = {
#ifdef HAVE_X11_XF86KEYSYM_H
        /* View actions */
        { XF86XK_ZoomIn,        "zoom-in" },
        { XF86XK_ZoomOut,        "zoom-out" },

#endif
};

/*
 * Floating Bar code
 */
static void
remove_loading_floating_bar (NautilusFilesView *view)
{
        if (view->details->floating_bar_loading_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_loading_timeout_id);
                view->details->floating_bar_loading_timeout_id = 0;
        }

        gtk_widget_hide (view->details->floating_bar);
        nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (view->details->floating_bar));
}

static void
real_setup_loading_floating_bar (NautilusFilesView *view)
{
        gboolean disable_chrome;

        g_object_get (nautilus_files_view_get_window (view),
                      "disable-chrome", &disable_chrome,
                      NULL);

        if (disable_chrome) {
                gtk_widget_hide (view->details->floating_bar);
                return;
        }

        nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (view->details->floating_bar));
        nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (view->details->floating_bar),
                                                 nautilus_view_is_searching (NAUTILUS_VIEW (view)) ? _("Searching…") : _("Loading…"));
        nautilus_floating_bar_set_details_label (NAUTILUS_FLOATING_BAR (view->details->floating_bar), NULL);
        nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (view->details->floating_bar), view->details->loading);
        nautilus_floating_bar_add_action (NAUTILUS_FLOATING_BAR (view->details->floating_bar),
                                          "process-stop-symbolic",
                                          NAUTILUS_FLOATING_BAR_ACTION_ID_STOP);

        gtk_widget_set_halign (view->details->floating_bar, GTK_ALIGN_END);
        gtk_widget_show (view->details->floating_bar);
}

static gboolean
setup_loading_floating_bar_timeout_cb (gpointer user_data)
{
        NautilusFilesView *view = user_data;

        view->details->floating_bar_loading_timeout_id = 0;
        real_setup_loading_floating_bar (view);

        return FALSE;
}

static void
setup_loading_floating_bar (NautilusFilesView *view)
{
        /* setup loading overlay */
        if (view->details->floating_bar_set_status_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_set_status_timeout_id);
                view->details->floating_bar_set_status_timeout_id = 0;
        }

        if (view->details->floating_bar_loading_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_loading_timeout_id);
                view->details->floating_bar_loading_timeout_id = 0;
        }

        view->details->floating_bar_loading_timeout_id =
                g_timeout_add (FLOATING_BAR_LOADING_DELAY, setup_loading_floating_bar_timeout_cb, view);
}

static void
floating_bar_action_cb (NautilusFloatingBar *floating_bar,
                        gint                 action,
                        NautilusFilesView   *view)
{
        if (action == NAUTILUS_FLOATING_BAR_ACTION_ID_STOP) {
                remove_loading_floating_bar (view);
                nautilus_window_slot_stop_loading (view->details->slot);
        }
}

static void
real_floating_bar_set_short_status (NautilusFilesView *view,
                                    const gchar       *primary_status,
                                    const gchar       *detail_status)
{
        gboolean disable_chrome;

        if (view->details->loading)
          return;

        nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (view->details->floating_bar));
        nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (view->details->floating_bar),
                                                FALSE);

        g_object_get (nautilus_files_view_get_window (view),
                      "disable-chrome", &disable_chrome,
                      NULL);

        if ((primary_status == NULL && detail_status == NULL) || disable_chrome) {
                gtk_widget_hide (view->details->floating_bar);
                return;
        }

        nautilus_floating_bar_set_labels (NAUTILUS_FLOATING_BAR (view->details->floating_bar),
                                          primary_status,
                                          detail_status);

        gtk_widget_show (view->details->floating_bar);
}

typedef struct {
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
        FloatingBarSetStatusData *status_data = data;

        status_data->view->details->floating_bar_set_status_timeout_id = 0;
        real_floating_bar_set_short_status (status_data->view,
                                            status_data->primary_status,
                                            status_data->detail_status);

        return FALSE;
}

static void
set_floating_bar_status (NautilusFilesView *view,
                         const gchar       *primary_status,
                         const gchar       *detail_status)
{
        GtkSettings *settings;
        gint double_click_time;
        FloatingBarSetStatusData *status_data;

        if (view->details->floating_bar_set_status_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_set_status_timeout_id);
                view->details->floating_bar_set_status_timeout_id = 0;
        }

        settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (view)));
        g_object_get (settings,
                      "gtk-double-click-time", &double_click_time,
                      NULL);

        status_data = g_slice_new0 (FloatingBarSetStatusData);
        status_data->primary_status = g_strdup (primary_status);
        status_data->detail_status = g_strdup (detail_status);
        status_data->view = view;

        /* waiting for half of the double-click-time before setting
         * the status seems to be a good approximation of not setting it
         * too often and not delaying the statusbar too much.
         */
        view->details->floating_bar_set_status_timeout_id =
                g_timeout_add_full (G_PRIORITY_DEFAULT,
                                    (guint) (double_click_time / 2),
                                    floating_bar_set_status_timeout_cb,
                                    status_data,
                                    floating_bar_set_status_data_free);
}

static char *
real_get_backing_uri (NautilusFilesView *view)
{
        NautilusDirectory *directory;
        char *uri;

        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        if (view->details->model == NULL) {
                return NULL;
        }

        directory = view->details->model;

        if (NAUTILUS_IS_DESKTOP_DIRECTORY (directory)) {
                directory = nautilus_desktop_directory_get_real_directory (NAUTILUS_DESKTOP_DIRECTORY (directory));
        } else {
                nautilus_directory_ref (directory);
        }

        uri = nautilus_directory_get_uri (directory);

        nautilus_directory_unref (directory);

        return uri;
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

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_backing_uri (view);
}

/**
 * nautilus_files_view_select_all:
 *
 * select all the items in the view
 *
 **/
static void
nautilus_files_view_select_all (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->select_all (view);
}

static void
nautilus_files_view_select_first (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->select_first (view);
}

static void
nautilus_files_view_call_set_selection (NautilusFilesView *view, GList *selection)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->set_selection (view, selection);
}

static GList *
nautilus_files_view_get_selection_for_file_transfer (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection_for_file_transfer (view);
}

static void
nautilus_files_view_invert_selection (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->invert_selection (view);
}

/**
 * nautilus_files_view_reveal_selection:
 *
 * Scroll as necessary to reveal the selected items.
 **/
static void
nautilus_files_view_reveal_selection (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->reveal_selection (view);
}

static gboolean
nautilus_files_view_using_manual_layout (NautilusFilesView  *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return         NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->using_manual_layout (view);
}

/**
 * nautilus_files_view_get_icon:
 * @view: a #NautilusView
 *
 * Retrieves the #GIcon that represents @view.
 *
 * Returns: (transfer none): the #Gicon that represents @view
 */
static GIcon*
nautilus_files_view_get_icon (NautilusView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_icon (NAUTILUS_FILES_VIEW (view));
}

/**
 * nautilus_files_view_get_view_widget:
 * @view: a #NautilusFilesView
 *
 * Retrieves the view menu, as a #GtkWidget. If it's %NULL,
 * the button renders insensitive.
 *
 * Returns: (transfer none): a #GtkWidget for the view menu
 */
static GtkWidget*
nautilus_files_view_get_view_widget (NautilusView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return NAUTILUS_FILES_VIEW (view)->details->view_menu_widget;
}

static gboolean
showing_trash_directory (NautilusFilesView *view)
{
        NautilusFile *file;

        file = nautilus_files_view_get_directory_as_file (view);
        if (file != NULL) {
                return nautilus_file_is_in_trash (file);
        }
        return FALSE;
}

static gboolean
showing_recent_directory (NautilusFilesView *view)
{
        NautilusFile *file;

        file = nautilus_files_view_get_directory_as_file (view);
        if (file != NULL) {
                return nautilus_file_is_in_recent (file);
        }
        return FALSE;
}

static gboolean
nautilus_files_view_supports_creating_files (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return !nautilus_files_view_is_read_only (view)
                && !showing_trash_directory (view)
                && !showing_recent_directory (view);
}

static gboolean
nautilus_files_view_is_empty (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_empty (view);
}

/**
 * nautilus_files_view_bump_zoom_level:
 *
 * bump the current zoom level by invoking the relevant subclass through the slot
 *
 **/
void
nautilus_files_view_bump_zoom_level (NautilusFilesView *view,
                               int zoom_increment)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        if (!nautilus_files_view_supports_zooming (view)) {
                return;
        }

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->bump_zoom_level (view, zoom_increment);
}

/**
 * nautilus_files_view_can_zoom_in:
 *
 * Determine whether the view can be zoomed any closer.
 * @view: The zoomable NautilusFilesView.
 *
 * Return value: TRUE if @view can be zoomed any closer, FALSE otherwise.
 *
 **/
gboolean
nautilus_files_view_can_zoom_in (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        if (!nautilus_files_view_supports_zooming (view)) {
                return FALSE;
        }

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_in (view);
}

/**
 * nautilus_files_view_can_zoom_out:
 *
 * Determine whether the view can be zoomed any further away.
 * @view: The zoomable NautilusFilesView.
 *
 * Return value: TRUE if @view can be zoomed any further away, FALSE otherwise.
 *
 **/
gboolean
nautilus_files_view_can_zoom_out (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        if (!nautilus_files_view_supports_zooming (view)) {
                return FALSE;
        }

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_out (view);
}

gboolean
nautilus_files_view_supports_zooming (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return view->details->supports_zooming;
}

/**
 * nautilus_files_view_restore_default_zoom_level:
 *
 * restore to the default zoom level by invoking the relevant subclass through the slot
 *
 **/
void
nautilus_files_view_restore_default_zoom_level (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        if (!nautilus_files_view_supports_zooming (view)) {
                return;
        }

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->restore_default_zoom_level (view);
}

gboolean
nautilus_files_view_is_searching (NautilusView *view)
{
  NautilusFilesView *files_view;

  files_view = NAUTILUS_FILES_VIEW (view);

  if (!files_view->details->model)
    return FALSE;

  return NAUTILUS_IS_SEARCH_DIRECTORY (files_view->details->model);
}

guint
nautilus_files_view_get_view_id (NautilusFilesView *view)
{
        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_view_id (view);
}

char *
nautilus_files_view_get_first_visible_file (NautilusFilesView *view)
{
        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_first_visible_file (view);
}

void
nautilus_files_view_scroll_to_file (NautilusFilesView *view,
                              const char *uri)
{
        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->scroll_to_file (view, uri);
}

/**
 * nautilus_files_view_get_selection:
 *
 * Get a list of NautilusFile pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: NautilusFilesView whose selected items are of interest.
 *
 * Return value: GList of NautilusFile pointers representing the selection.
 *
 **/
static GList*
nautilus_files_view_get_selection (NautilusView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection (NAUTILUS_FILES_VIEW (view));
}

typedef struct {
        NautilusFile *file;
        NautilusFilesView *directory_view;
} ScriptLaunchParameters;

typedef struct {
        NautilusFile *file;
        NautilusFilesView *directory_view;
} CreateTemplateParameters;

static GList *
file_and_directory_list_to_files (GList *fad_list)
{
        GList *res, *l;
        FileAndDirectory *fad;

        res = NULL;
        for (l = fad_list; l != NULL; l = l->next) {
                fad = l->data;
                res = g_list_prepend (res, nautilus_file_ref (fad->file));
        }
        return g_list_reverse (res);
}


static GList *
file_and_directory_list_from_files (NautilusDirectory *directory,
                                    GList             *files)
{
        GList *res, *l;
        FileAndDirectory *fad;

        res = NULL;
        for (l = files; l != NULL; l = l->next) {
                fad = g_new0 (FileAndDirectory, 1);
                fad->directory = nautilus_directory_ref (directory);
                fad->file = nautilus_file_ref (l->data);
                res = g_list_prepend (res, fad);
        }
        return g_list_reverse (res);
}

static void
file_and_directory_free (FileAndDirectory *fad)
{
        nautilus_directory_unref (fad->directory);
        nautilus_file_unref (fad->file);
        g_free (fad);
}


static void
file_and_directory_list_free (GList *list)
{
        GList *l;

        for (l = list; l != NULL; l = l->next) {
                file_and_directory_free (l->data);
        }

        g_list_free (list);
}

static gboolean
file_and_directory_equal (gconstpointer v1,
                          gconstpointer v2)
{
        const FileAndDirectory *fad1, *fad2;
        fad1 = v1;
        fad2 = v2;

        return (fad1->file == fad2->file &&
                fad1->directory == fad2->directory);
}

static guint
file_and_directory_hash  (gconstpointer  v)
{
        const FileAndDirectory *fad;

        fad = v;
        return GPOINTER_TO_UINT (fad->file) ^ GPOINTER_TO_UINT (fad->directory);
}

static ScriptLaunchParameters *
script_launch_parameters_new (NautilusFile *file,
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

NautilusWindow *
nautilus_files_view_get_window (NautilusFilesView  *view)
{
        return nautilus_window_slot_get_window (view->details->slot);
}

NautilusWindowSlot *
nautilus_files_view_get_nautilus_window_slot (NautilusFilesView  *view)
{
        g_assert (view->details->slot != NULL);

        return view->details->slot;
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
        if (window == NULL) {
                return NULL;
        }

        return GTK_WINDOW (window);
}

static gboolean
nautilus_files_view_confirm_multiple (GtkWindow *parent_window,
                                      int        count,
                                      gboolean   tabs)
{
        GtkDialog *dialog;
        char *prompt;
        char *detail;
        int response;

        if (count <= SILENT_WINDOW_OPEN_LIMIT) {
                return TRUE;
        }

        prompt = _("Are you sure you want to open all files?");
        if (tabs) {
                detail = g_strdup_printf (ngettext("This will open %'d separate tab.",
                                                   "This will open %'d separate tabs.", count), count);
        } else {
                detail = g_strdup_printf (ngettext("This will open %'d separate window.",
                                                   "This will open %'d separate windows.", count), count);
        }
        dialog = eel_show_yes_no_dialog (prompt, detail,
                                         _("_OK"), _("_Cancel"),
                                         parent_window);
        g_free (detail);

        response = gtk_dialog_run (dialog);
        gtk_widget_destroy (GTK_WIDGET (dialog));

        return response == GTK_RESPONSE_YES;
}

static gboolean
selection_not_empty_in_menu_callback (NautilusFilesView *view,
                                      GList             *selection)
{
        if (selection != NULL) {
                return TRUE;
        }

        return FALSE;
}

static char *
get_view_directory (NautilusFilesView *view)
{
        char *uri, *path;
        GFile *f;

        uri = nautilus_directory_get_uri (view->details->model);
        if (eel_uri_is_desktop (uri)) {
                g_free (uri);
                uri = nautilus_get_desktop_directory_uri ();

        }
        f = g_file_new_for_uri (uri);
        path = g_file_get_path (f);
        g_object_unref (f);
        g_free (uri);

        return path;
}

void
nautilus_files_view_preview_files (NautilusFilesView *view,
                                   GList             *files,
                                   GArray            *locations)
{
        gchar *uri;
        guint xid = 0;
        GtkWidget *toplevel;
        GdkWindow *window;

        uri = nautilus_file_get_uri (files->data);
        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

#ifdef GDK_WINDOWING_X11
        window = gtk_widget_get_window (toplevel);
        if (GDK_IS_X11_WINDOW (window))
          xid = gdk_x11_window_get_xid (gtk_widget_get_window (toplevel));
#endif

        nautilus_previewer_call_show_file (uri, xid, TRUE);

        g_free (uri);
}

void
nautilus_files_view_activate_selection (NautilusFilesView *view)
{
        GList *selection;

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        nautilus_files_view_activate_files (view,
                                      selection,
                                      0,
                                      TRUE);
        nautilus_file_list_free (selection);
}

void
nautilus_files_view_activate_files (NautilusFilesView       *view,
                                    GList                   *files,
                                    NautilusWindowOpenFlags  flags,
                                    gboolean                 confirm_multiple)
{
        char *path;

        path = get_view_directory (view);
        nautilus_mime_activate_files (nautilus_files_view_get_containing_window (view),
                                      view->details->slot,
                                      files,
                                      path,
                                      flags,
                                      confirm_multiple);

        g_free (path);
}

static void
nautilus_files_view_activate_file (NautilusFilesView       *view,
                                   NautilusFile            *file,
                                   NautilusWindowOpenFlags  flags)
{
        char *path;

        path = get_view_directory (view);
        nautilus_mime_activate_file (nautilus_files_view_get_containing_window (view),
                                     view->details->slot,
                                     file,
                                     path,
                                     flags);

        g_free (path);
}

static void
action_open_with_default_application (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       user_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (user_data);
        nautilus_files_view_activate_selection (view);
}
static void
action_open_file_and_close_window (GSimpleAction *action,
                                   GVariant      *state,
                                   gpointer       user_data)
{
        GList *selection;
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        nautilus_files_view_activate_files (view,
                                      selection,
                                      NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND,
                                      TRUE);
        nautilus_file_list_free (selection);
}

static void
got_it_clicked (GtkDialog *dialog,
                gint       response_id,
                gpointer   user_data)
{
        g_settings_set_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_SHOW_MOVE_TO_TRASH_SHORTCUT_CHANGED_DIALOG,
                                FALSE);
}

static void
action_show_move_to_trash_shortcut_changed_dialog (GSimpleAction *action,
                                                   GVariant      *state,
                                                   gpointer       user_data)
{
        NautilusFilesView *view;
        GtkWindow *dialog;
        GtkBuilder *builder;
        gboolean show_dialog_preference;

        view = NAUTILUS_FILES_VIEW (user_data);
        show_dialog_preference = g_settings_get_boolean (nautilus_preferences,
                                                         NAUTILUS_PREFERENCES_SHOW_MOVE_TO_TRASH_SHORTCUT_CHANGED_DIALOG);
        if (show_dialog_preference) {
                builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-move-to-trash-shortcut-changed.ui");
                dialog = GTK_WINDOW (gtk_builder_get_object (builder, "move_to_trash_shortcut_changed_dialog"));

                gtk_window_set_transient_for (dialog, GTK_WINDOW (nautilus_files_view_get_window (view)));
                  g_signal_connect (dialog, "response",
                                  G_CALLBACK (got_it_clicked),
                                  view);

                gtk_widget_show (GTK_WIDGET (dialog));
                gtk_dialog_run(GTK_DIALOG (dialog));
                gtk_widget_destroy (GTK_WIDGET (dialog));

                  g_object_unref (builder);
        }
}
static void
action_open_item_location (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;
        NautilusFile *item;
        GFile *activation_location;
        NautilusFile *activation_file;
        NautilusFile *location;

        view = NAUTILUS_FILES_VIEW (user_data);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

        if (!selection)
                return;

        item = NAUTILUS_FILE (selection->data);
        activation_location = nautilus_file_get_activation_location (item);
        activation_file = nautilus_file_get (activation_location);
        location = nautilus_file_get_parent (activation_file);

        nautilus_files_view_activate_file (view, location, 0);

        nautilus_file_unref (location);
        nautilus_file_unref (activation_file);
        g_object_unref (activation_location);
        nautilus_file_list_free (selection);
}

static void
action_open_item_new_tab (GSimpleAction *action,
                          GVariant      *state,
                          gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;
        GtkWindow *window;

        view = NAUTILUS_FILES_VIEW (user_data);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

        window = nautilus_files_view_get_containing_window (view);

        if (nautilus_files_view_confirm_multiple (window, g_list_length (selection), TRUE)) {
                nautilus_files_view_activate_files (view,
                                              selection,
                                              NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB |
                                              NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE,
                                              FALSE);
        }

        nautilus_file_list_free (selection);
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
                goto out;

        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));

        g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");

        nautilus_launch_application (info, files, parent_window);

        g_object_unref (info);
out:
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
choose_program (NautilusFilesView *view,
                GList             *files)
{
        GtkWidget *dialog;
        gchar *mime_type;
        GtkWindow *parent_window;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        mime_type = nautilus_file_get_mime_type (files->data);
        parent_window = nautilus_files_view_get_containing_window (view);

        dialog = gtk_app_chooser_dialog_new_for_content_type (parent_window,
                                                              GTK_DIALOG_MODAL |
                                                              GTK_DIALOG_DESTROY_WITH_PARENT |
                                                              GTK_DIALOG_USE_HEADER_BAR,
                                                              mime_type);
        g_object_set_data_full (G_OBJECT (dialog),
                                "directory-view:files",
                                files,
                                (GDestroyNotify) nautilus_file_list_free);
        gtk_widget_show (dialog);

        g_signal_connect_object (dialog, "response",
                                 G_CALLBACK (app_chooser_dialog_response_cb),
                                 parent_window, 0);
}

static void
open_with_other_program (NautilusFilesView *view)
{
        GList *selection;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));

               selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        choose_program (view, selection);
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
trash_or_delete_selected_files (NautilusFilesView *view)
{
        GList *selection;

        /* This might be rapidly called multiple times for the same selection
         * when using keybindings. So we remember if the current selection
         * was already removed (but the view doesn't know about it yet).
         */
        if (!view->details->selection_was_removed) {
                selection = nautilus_files_view_get_selection_for_file_transfer (view);
                trash_or_delete_files (nautilus_files_view_get_containing_window (view),
                                       selection,
                                       view);
                nautilus_file_list_free (selection);
                view->details->selection_was_removed = TRUE;
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
        /* TODO:implement a set of functions for this, is very confusing to
         * call trash_or_delete_file to remove from recent, even if it does like
         * that not deleting/moving the files to trash */
        trash_or_delete_selected_files (NAUTILUS_FILES_VIEW (user_data));
}

static void
delete_selected_files (NautilusFilesView *view)
{
        GList *selection;
        GList *node;
        GList *locations;

        selection = nautilus_files_view_get_selection_for_file_transfer (view);
        if (selection == NULL) {
                return;
        }

        locations = NULL;
        for (node = selection; node != NULL; node = node->next) {
                locations = g_list_prepend (locations,
                                            nautilus_file_get_location ((NautilusFile *) node->data));
        }
        locations = g_list_reverse (locations);

        nautilus_file_operations_delete (locations, nautilus_files_view_get_containing_window (view), NULL, NULL);

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
pattern_select_response_cb (GtkWidget *dialog,
                            int        response,
                            gpointer   user_data)
{
        NautilusFilesView *view;
        NautilusDirectory *directory;
        GtkWidget *entry;
        GList *selection;

        view = NAUTILUS_FILES_VIEW (user_data);

        switch (response) {
        case GTK_RESPONSE_OK :
                entry = g_object_get_data (G_OBJECT (dialog), "entry");
                directory = nautilus_files_view_get_model (view);
                selection = nautilus_directory_match_pattern (directory,
                                                              gtk_entry_get_text (GTK_ENTRY (entry)));

                if (selection) {
                        nautilus_files_view_call_set_selection (view, selection);
                        nautilus_file_list_free (selection);

                        nautilus_files_view_reveal_selection(view);
                }
                /* fall through */
        case GTK_RESPONSE_NONE :
        case GTK_RESPONSE_DELETE_EVENT :
        case GTK_RESPONSE_CANCEL :
                gtk_widget_destroy (GTK_WIDGET (dialog));
                break;
        default :
                g_assert_not_reached ();
        }
}

static void
select_pattern (NautilusFilesView *view)
{
        GtkWidget *dialog;
        GtkWidget *label;
        GtkWidget *example;
        GtkWidget *grid;
        GtkWidget *entry;
        char *example_pattern;

        dialog = gtk_dialog_new_with_buttons (_("Select Items Matching"),
                                              nautilus_files_view_get_containing_window (view),
                                              GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                              _("_Cancel"),
                                              GTK_RESPONSE_CANCEL,
                                              _("_Select"),
                                              GTK_RESPONSE_OK,
                                              NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                         GTK_RESPONSE_OK);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

        label = gtk_label_new_with_mnemonic (_("_Pattern:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);

        example = gtk_label_new (NULL);
        gtk_widget_set_halign (example, GTK_ALIGN_START);
        example_pattern = g_strdup_printf ("%s<i>%s</i> ",
                                           _("Examples: "),
                                           "*.png, file\?\?.txt, pict*.\?\?\?");
        gtk_label_set_markup (GTK_LABEL (example), example_pattern);
        g_free (example_pattern);

        entry = gtk_entry_new ();
        gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
        gtk_widget_set_hexpand (entry, TRUE);

        grid = gtk_grid_new ();
        g_object_set (grid,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "border-width", 6,
                      "row-spacing", 6,
                      "column-spacing", 12,
                      NULL);

        gtk_container_add (GTK_CONTAINER (grid), label);
        gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
                                 GTK_POS_RIGHT, 1, 1);
        gtk_grid_attach_next_to (GTK_GRID (grid), example, entry,
                                 GTK_POS_BOTTOM, 1, 1);

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
        gtk_widget_show_all (grid);
        gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid);
        g_object_set_data (G_OBJECT (dialog), "entry", entry);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (pattern_select_response_cb),
                          view);
        gtk_widget_show_all (dialog);
}

static void
action_select_pattern (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        select_pattern(user_data);
}

static void
zoom_level_changed (GtkRange          *range,
                    NautilusFilesView *view)
{
        g_action_group_change_action_state (view->details->view_action_group,
                                            "zoom-to-level",
                                            g_variant_new_int32 (gtk_range_get_value (range)));
}

static void
reveal_newly_added_folder (NautilusFilesView *view,
                           NautilusFile      *new_file,
                           NautilusDirectory *directory,
                           GFile             *target_location)
{
        GFile *location;

        location = nautilus_file_get_location (new_file);
        if (g_file_equal (location, target_location)) {
                g_signal_handlers_disconnect_by_func (view,
                                                      G_CALLBACK (reveal_newly_added_folder),
                                                      (void *) target_location);
                nautilus_files_view_select_file (view, new_file);
                nautilus_files_view_reveal_selection (view);
        }
        g_object_unref (location);
}

typedef struct {
        NautilusFilesView *directory_view;
        GHashTable *added_locations;
        GList *selection;
} NewFolderData;

typedef struct {
        NautilusFilesView *directory_view;
        GHashTable *to_remove_locations;
        NautilusFile *new_folder;
} NewFolderSelectionData;

static void
track_newly_added_locations (NautilusFilesView *view,
                             NautilusFile      *new_file,
                             NautilusDirectory *directory,
                             gpointer           user_data)
{
        NewFolderData *data;

        data = user_data;

        g_hash_table_insert (data->added_locations, nautilus_file_get_location (new_file), NULL);
}

static void
new_folder_done (GFile    *new_folder,
                 gboolean  success,
                 gpointer  user_data)
{
        NautilusFilesView *directory_view;
        NautilusFile *file;
        char screen_string[32];
        GdkScreen *screen;
        NewFolderData *data;

        data = (NewFolderData *)user_data;

        directory_view = data->directory_view;

        if (directory_view == NULL) {
                goto fail;
        }

        g_signal_handlers_disconnect_by_func (directory_view,
                                              G_CALLBACK (track_newly_added_locations),
                                              (void *) data);

        if (new_folder == NULL) {
                goto fail;
        }

        screen = gtk_widget_get_screen (GTK_WIDGET (directory_view));
        g_snprintf (screen_string, sizeof (screen_string), "%d", gdk_screen_get_number (screen));


        file = nautilus_file_get (new_folder);
        nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_SCREEN,
                                   NULL,
                                   screen_string);

        if (data->selection != NULL) {
                GList *uris, *l;
                char *target_uri;

                uris = NULL;
                for (l = data->selection; l != NULL; l = l->next) {
                        uris = g_list_prepend (uris, nautilus_file_get_uri ((NautilusFile *) l->data));
                }
                uris = g_list_reverse (uris);

                target_uri = nautilus_file_get_uri (file);

                nautilus_files_view_move_copy_items (directory_view,
                                                     uris,
                                                     NULL,
                                                     target_uri,
                                                     GDK_ACTION_MOVE,
                                                     0, 0);
                g_list_free_full (uris, g_free);
                g_free (target_uri);
        } else {
                if (g_hash_table_lookup_extended (data->added_locations, new_folder, NULL, NULL)) {
                        /* The file was already added */
                        nautilus_files_view_select_file (directory_view, file);
                        nautilus_files_view_reveal_selection (directory_view);
                } else {
                        /* We need to run after the default handler adds the folder we want to
                         * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
                         * must use connect_after.
                         */
                        g_signal_connect_data (directory_view,
                                               "add-file",
                                               G_CALLBACK (reveal_newly_added_folder),
                                               g_object_ref (new_folder),
                                               (GClosureNotify)g_object_unref,
                                               G_CONNECT_AFTER);
                }
        }

        nautilus_file_unref (file);

 fail:
        g_hash_table_destroy (data->added_locations);

        if (data->directory_view != NULL) {
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
        data->added_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
                                                       g_object_unref, NULL);
        if (with_selection) {
                data->selection = nautilus_files_view_get_selection_for_file_transfer (directory_view);
        } else {
                data->selection = NULL;
        }
        g_object_add_weak_pointer (G_OBJECT (data->directory_view),
                                   (gpointer *) &data->directory_view);

        return data;
}

static GdkPoint *
context_menu_to_file_operation_position (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        if (nautilus_files_view_using_manual_layout (view)
            && view->details->context_menu_position.x >= 0
            && view->details->context_menu_position.y >= 0) {
                NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->widget_to_file_operation_position
                        (view, &view->details->context_menu_position);
                return &view->details->context_menu_position;
        } else {
                return NULL;
        }
}

typedef struct {
        NautilusFilesView *view;
        GtkWidget *widget;
        GtkWidget *error_label;
        GtkWidget *name_entry;
        GtkWidget *activate_button;
        gboolean target_is_folder;
        NautilusFile *target_file;
        gboolean duplicated_is_folder;
        void (*on_name_accepted) (gpointer data);
        /* For create folder only */
        gboolean with_selection;
} FileNameWidgetData;

static gboolean
duplicated_file_label_show (FileNameWidgetData *data)
{
        if (data->duplicated_is_folder)
                gtk_label_set_label (GTK_LABEL (data->error_label), _("A folder with that name already exists."));
        else
                gtk_label_set_label (GTK_LABEL (data->error_label), _("A file with that name already exists."));

        data->view->details->duplicated_label_timeout_id = 0;

        return G_SOURCE_REMOVE;
}

static gchar*
validate_file_name (const gchar *name,
                    gboolean     is_folder)
{
        gchar *error_message = NULL;

        if (strstr (name, "/") != NULL) {
                if (is_folder)
                        error_message = _("Folder names cannot contain “/”.");
                else
                        error_message = _("Files names cannot contain “/”.");
        } else if (strcmp (name, ".") == 0){
                if (is_folder)
                        error_message = _("A folder can not be called “.”.");
                else
                        error_message = _("A file can not be called “.”.");
        } else if (strcmp (name, "..") == 0){
                if (is_folder)
                        error_message = _("A folder can not be called “..”.");
                else
                        error_message = _("A file can not be called “..”.");
        }

        return error_message;
}

static void
file_name_widget_entry_on_changed (gpointer user_data)
{
        FileNameWidgetData *data;
        NautilusFile *existing_file;
        gchar *name;
        gchar *error_message;
        gboolean valid_name;
        gboolean duplicated;

        data = (FileNameWidgetData *) user_data;
        name = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (data->name_entry))));
        error_message = validate_file_name (name, data->target_is_folder);
        gtk_label_set_label (GTK_LABEL (data->error_label), error_message);

        existing_file = nautilus_directory_get_file_by_name (data->view->details->model, name);

        valid_name = strlen (name) > 0 && error_message == NULL;
        /* If there is a target file and the name is the same, we don't show it
         * as duplicated. This is the case for renaming. */
        duplicated = existing_file != NULL &&
                     (data->target_file == NULL ||
                      nautilus_file_compare_display_name (data->target_file, name) != 0);
        gtk_widget_set_sensitive (data->activate_button, valid_name && !duplicated);

        if (data->view->details->duplicated_label_timeout_id > 0) {
                g_source_remove (data->view->details->duplicated_label_timeout_id);
                data->view->details->duplicated_label_timeout_id = 0;
        }

        /* Report duplicated file only if not other message shown (for instance,
         * folders like "." or ".." will always exists, but we consider it as an
         * error, not as a duplicated file or if the name is the same as the file
         * we are renaming also don't report as a duplicated */
        if (duplicated && valid_name) {
                data->duplicated_is_folder = nautilus_file_is_directory (existing_file);
                data->view->details->duplicated_label_timeout_id =
                        g_timeout_add (FILE_NAME_DUPLICATED_LABEL_TIMEOUT,
                                       (GSourceFunc)duplicated_file_label_show,
                                       data);
        }

        if (existing_file != NULL)
                nautilus_file_unref (existing_file);

        g_free (name);
}

static void
create_folder_dialog_on_response (GtkDialog *dialog,
                                  gint       response_id,
                                  gpointer   user_data)
{
        FileNameWidgetData *widget_data;

        widget_data = (FileNameWidgetData *) user_data;

        if (response_id == GTK_RESPONSE_OK) {
                NewFolderData *data;
                GdkPoint *pos;
                char *parent_uri;
                gchar *name;

                data = new_folder_data_new (widget_data->view, widget_data->with_selection);

                name = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (widget_data->name_entry))));
                g_signal_connect_data (widget_data->view,
                                       "add-file",
                                       G_CALLBACK (track_newly_added_locations),
                                       data,
                                       (GClosureNotify)NULL,
                                       G_CONNECT_AFTER);

                pos = context_menu_to_file_operation_position (widget_data->view);

                parent_uri = nautilus_files_view_get_backing_uri (widget_data->view);
                nautilus_file_operations_new_folder (GTK_WIDGET (widget_data->view),
                                                     pos, parent_uri, name,
                                                     new_folder_done, data);

                g_free (parent_uri);
                g_free (name);
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));
        g_free (user_data);
}


static void
create_folder_on_name_accepted (gpointer user_data)
{
        FileNameWidgetData *data;

        data = (FileNameWidgetData *) user_data;
        gtk_dialog_response (GTK_DIALOG (data->widget),
                             GTK_RESPONSE_OK);
}

static void
rename_file_on_name_accepted (gpointer user_data)
{
        FileNameWidgetData *data;
        gchar *name;

        data = (FileNameWidgetData *) user_data;

        name = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (data->name_entry))));
        nautilus_rename_file (data->target_file, name, NULL, NULL);

        nautilus_files_view_select_file (data->view, data->target_file);
        nautilus_files_view_reveal_selection (data->view);

        gtk_widget_hide (data->widget);

        g_free (name);
}

static void
file_name_widget_on_activate (gpointer user_data)
{
        FileNameWidgetData *data;
        NautilusFile *existing_file;
        gchar *name;
        gchar *error_message;
        gboolean valid_name;
        gboolean duplicated;

        data = (FileNameWidgetData *) user_data;
        name = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (data->name_entry))));
        existing_file = nautilus_directory_get_file_by_name (data->view->details->model, name);
        error_message = validate_file_name (name, data->target_is_folder);
        valid_name = strlen (name) > 0 && error_message == NULL;
        duplicated = existing_file != NULL &&
                     (data->target_file == NULL ||
                      nautilus_file_compare_display_name (data->target_file, name) != 0);

        if (data->view->details->duplicated_label_timeout_id > 0) {
                g_source_remove (data->view->details->duplicated_label_timeout_id);
                data->view->details->duplicated_label_timeout_id = 0;
        }

        if (valid_name && !duplicated) {
                data->on_name_accepted ((gpointer) data);
        } else {
                /* Report duplicated file only if not other message shown (for instance,
                 * folders like "." or ".." will always exists, but we consider it as an
                 * error, not as a duplicated file) */
                if (existing_file != NULL && valid_name) {
                        data->duplicated_is_folder = nautilus_file_is_directory (existing_file);
                        /* Show it inmediatily since the user tried to trigger the action */
                        duplicated_file_label_show (data);
                }
        }

        if (existing_file != NULL)
                nautilus_file_unref (existing_file);

}

static void
rename_file_popover_on_closed (GtkPopover *popover,
                               gpointer    user_data)
{
        FileNameWidgetData *widget_data;

        widget_data = (FileNameWidgetData *) user_data;
        widget_data->view->details->rename_file_popover = NULL;
        if (widget_data->view->details->duplicated_label_timeout_id > 0) {
                g_source_remove (widget_data->view->details->duplicated_label_timeout_id);
                widget_data->view->details->duplicated_label_timeout_id = 0;
        }
        g_free (widget_data);
}

static GdkRectangle*
nautilus_files_view_compute_rename_popover_relative_to (NautilusFilesView *view)
{
        return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->compute_rename_popover_relative_to (view);
}

static void
nautilus_files_view_rename_file_popover_new (NautilusFilesView *view,
                                             NautilusFile      *target_file)
{
        FileNameWidgetData *widget_data;
        GtkWidget *label_file_name;
        GtkBuilder *builder;
        gint start_offset, end_offset;
        GdkRectangle *relative_to;
        gint n_chars;

        if (view->details->rename_file_popover != NULL)
          return;

        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-rename-file-popover.ui");
        label_file_name = GTK_WIDGET (gtk_builder_get_object (builder, "name_label"));

        widget_data = g_new (FileNameWidgetData, 1);
        widget_data->view = view;
        widget_data->on_name_accepted = rename_file_on_name_accepted;
        widget_data->widget = GTK_WIDGET (gtk_builder_get_object (builder, "rename_file_popover"));
        widget_data->activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "rename_button"));
        widget_data->error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
        widget_data->name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
        widget_data->target_is_folder = nautilus_file_is_directory (target_file);
        widget_data->target_file = target_file;

        view->details->rename_file_popover = widget_data->widget;

        /* Connect signals */
        gtk_builder_add_callback_symbols (builder,
                                          "file_name_widget_entry_on_changed",
                                          G_CALLBACK (file_name_widget_entry_on_changed),
                                          "file_name_widget_on_activate",
                                          G_CALLBACK (file_name_widget_on_activate),
                                          "rename_file_popover_on_closed",
                                          G_CALLBACK (rename_file_popover_on_closed),
                                          "rename_file_popover_on_unmap",
                                          G_CALLBACK (gtk_widget_destroy),
                                          NULL);

        gtk_builder_connect_signals (builder, widget_data);

        if (widget_data->target_is_folder)
                gtk_label_set_text (GTK_LABEL (label_file_name), _("Folder name"));
        else
                gtk_label_set_text (GTK_LABEL (label_file_name), _("File name"));
        gtk_entry_set_text (GTK_ENTRY (widget_data->name_entry), nautilus_file_get_display_name (target_file));

        relative_to = nautilus_files_view_compute_rename_popover_relative_to (view);
        gtk_popover_set_default_widget (GTK_POPOVER (widget_data->widget),
                                        widget_data->activate_button);
        gtk_popover_set_pointing_to (GTK_POPOVER (widget_data->widget), relative_to);
        gtk_popover_set_relative_to (GTK_POPOVER (widget_data->widget),
                                     GTK_WIDGET (view));
        gtk_widget_show (widget_data->widget);
        gtk_widget_grab_focus (widget_data->name_entry);

        /* Select the name part withouth the file extension */
        eel_filename_get_rename_region (nautilus_file_get_display_name (target_file),
                                        &start_offset, &end_offset);
        n_chars = g_utf8_strlen (nautilus_file_get_display_name (target_file), -1);
        gtk_entry_set_width_chars (GTK_ENTRY (widget_data->name_entry),
                                   MIN (MAX (n_chars, RENAME_ENTRY_MIN_CHARS), RENAME_ENTRY_MAX_CHARS));
        gtk_editable_select_region (GTK_EDITABLE (widget_data->name_entry),
                                    start_offset, end_offset);

        /* Update the rename button status */
        file_name_widget_entry_on_changed (widget_data);


        g_object_unref (builder);
}

static void
nautilus_files_view_new_folder_dialog_new (NautilusFilesView *view,
                                           gboolean           with_selection)
{
        FileNameWidgetData *widget_data;
        GtkWidget *label_file_name;
        GtkBuilder *builder;

        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-create-folder-dialog.ui");
        label_file_name = GTK_WIDGET (gtk_builder_get_object (builder, "name_label"));

        widget_data = g_new (FileNameWidgetData, 1);
        widget_data->view = view;
        widget_data->on_name_accepted = create_folder_on_name_accepted;
        widget_data->widget = GTK_WIDGET (gtk_builder_get_object (builder, "create_folder_dialog"));
        widget_data->activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));
        widget_data->error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
        widget_data->name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
        widget_data->target_is_folder = TRUE;
        widget_data->target_file = NULL;
        widget_data->with_selection = with_selection;

        gtk_window_set_transient_for (GTK_WINDOW (widget_data->widget),
                                      GTK_WINDOW (nautilus_files_view_get_window (view)));

        /* Connect signals */
        gtk_builder_add_callback_symbols (builder,
                                          "file_name_widget_entry_on_changed",
                                          G_CALLBACK (file_name_widget_entry_on_changed),
                                          "file_name_widget_on_activate",
                                          G_CALLBACK (file_name_widget_on_activate),
                                          "create_folder_dialog_on_response",
                                          G_CALLBACK (create_folder_dialog_on_response),
                                          NULL);

        gtk_builder_connect_signals (builder, widget_data);
        gtk_button_set_label (GTK_BUTTON (widget_data->activate_button),
                              _("Create"));
        gtk_label_set_text (GTK_LABEL (label_file_name), _("Folder name"));
        gtk_window_set_title (GTK_WINDOW (widget_data->widget), _("New Folder"));

        gtk_widget_show_all (widget_data->widget);
        /* Update the ok button status */
        file_name_widget_entry_on_changed (widget_data);

        g_object_unref (builder);
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
                               "add-file",
                               G_CALLBACK (track_newly_added_locations),
                               data,
                               (GClosureNotify)NULL,
                               G_CONNECT_AFTER);

        return data;
}

void
nautilus_files_view_new_file_with_initial_contents (NautilusFilesView *view,
                                                    const char        *parent_uri,
                                                    const char        *filename,
                                                    const char        *initial_contents,
                                                    int                length,
                                                    GdkPoint          *pos)
{
        NewFolderData *data;

        g_assert (parent_uri != NULL);

        data = setup_new_folder_data (view);

        if (pos == NULL) {
                pos = context_menu_to_file_operation_position (view);
        }

        nautilus_file_operations_new_file (GTK_WIDGET (view),
                                           pos, parent_uri, filename,
                                           initial_contents, length,
                                           new_folder_done, data);
}

static void
nautilus_files_view_new_file (NautilusFilesView *directory_view,
                              const char        *parent_uri,
                              NautilusFile      *source)
{
        GdkPoint *pos;
        NewFolderData *data;
        char *source_uri;
        char *container_uri;

        container_uri = NULL;
        if (parent_uri == NULL) {
                container_uri = nautilus_files_view_get_backing_uri (directory_view);
                g_assert (container_uri != NULL);
        }

        if (source == NULL) {
                nautilus_files_view_new_file_with_initial_contents (directory_view,
                                                              parent_uri != NULL ? parent_uri : container_uri,
                                                              NULL,
                                                              NULL,
                                                              0,
                                                              NULL);
                g_free (container_uri);
                return;
        }

        g_return_if_fail (nautilus_file_is_local (source));

        pos = context_menu_to_file_operation_position (directory_view);

        data = setup_new_folder_data (directory_view);

        source_uri = nautilus_file_get_uri (source);

        nautilus_file_operations_new_file_from_template (GTK_WIDGET (directory_view),
                                                         pos,
                                                         parent_uri != NULL ? parent_uri : container_uri,
                                                         NULL,
                                                         source_uri,
                                                         new_folder_done, data);

        g_free (source_uri);
        g_free (container_uri);
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
action_properties (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;
        GList *files;

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        view = NAUTILUS_FILES_VIEW (user_data);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        if (g_list_length (selection) == 0) {
                if (view->details->directory_as_file != NULL) {
                        files = g_list_append (NULL, nautilus_file_ref (view->details->directory_as_file));

                        nautilus_properties_window_present (files, GTK_WIDGET (view), NULL);

                        nautilus_file_list_free (files);
                }
        } else {
                nautilus_properties_window_present (selection, GTK_WIDGET (view), NULL);
        }
        nautilus_file_list_free (selection);
}

static void
nautilus_files_view_set_show_hidden_files (NautilusFilesView *view,
                                           gboolean           show_hidden)
{
        if (view->details->ignore_hidden_file_preferences) {
                return;
        }

        if (show_hidden != view->details->show_hidden_files) {
                view->details->show_hidden_files = show_hidden;

                g_settings_set_boolean (gtk_filechooser_preferences,
                                        NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                                        show_hidden);

                if (view->details->model != NULL) {
                        load_directory (view, view->details->model);
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
action_zoom_in (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
        NautilusFilesView *view;

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        view = NAUTILUS_FILES_VIEW (user_data);

        nautilus_files_view_bump_zoom_level (view, 1);
}

static void
action_zoom_out (GSimpleAction *action,
                 GVariant      *state,
                 gpointer       user_data)
{
        NautilusFilesView *view;

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        view = NAUTILUS_FILES_VIEW (user_data);

        nautilus_files_view_bump_zoom_level (view, -1);
}

static void
action_zoom_default (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        nautilus_files_view_restore_default_zoom_level (user_data);
}

static void
action_open_item_new_window (GSimpleAction *action,
                             GVariant      *state,
                             gpointer       user_data)
{
        NautilusFilesView *view;
        GtkWindow *window;
        GList *selection;

        view = NAUTILUS_FILES_VIEW (user_data);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        window = GTK_WINDOW (nautilus_files_view_get_containing_window (view));

        if (nautilus_files_view_confirm_multiple (window, g_list_length (selection), TRUE)) {
                g_list_foreach (selection, open_one_in_new_window, view);
        }

        nautilus_file_list_free (selection);
}

static void
handle_clipboard_data (NautilusFilesView *view,
                       GtkSelectionData  *selection_data,
                       char              *destination_uri,
                       GdkDragAction      action)
{
        GList *item_uris;

        item_uris = nautilus_clipboard_get_uri_list_from_selection_data (selection_data, NULL,
                                                                         copied_files_atom);

        if (item_uris != NULL && destination_uri != NULL) {
                nautilus_files_view_move_copy_items (view, item_uris, NULL, destination_uri,
                                                     action,
                                                     0, 0);

                /* If items are cut then remove from clipboard */
                if (action == GDK_ACTION_MOVE) {
                        gtk_clipboard_clear (nautilus_clipboard_get (GTK_WIDGET (view)));
                }

                g_list_free_full (item_uris, g_free);
        }
}

static void
paste_clipboard_data (NautilusFilesView *view,
                      GtkSelectionData  *selection_data,
                      char              *destination_uri)
{
        GdkDragAction action;

        action = nautilus_clipboard_monitor_is_cut (nautilus_clipboard_monitor_get ()) ?
                 GDK_ACTION_MOVE : GDK_ACTION_COPY;

        handle_clipboard_data (view, selection_data, destination_uri, action);
}

static void
paste_clipboard_received_callback (GtkClipboard     *clipboard,
                                   GtkSelectionData *selection_data,
                                   gpointer          data)
{
        NautilusFilesView *view;
        char *view_uri;

        view = NAUTILUS_FILES_VIEW (data);

        view_uri = nautilus_files_view_get_backing_uri (view);

        if (view->details->slot != NULL) {
                paste_clipboard_data (view, selection_data, view_uri);
        }

        g_free (view_uri);

        g_object_unref (view);
}

static void
action_paste_files (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
        NautilusFilesView *view;

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        view = NAUTILUS_FILES_VIEW (user_data);

        g_object_ref (view);
        gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
                                        copied_files_atom,
                                        paste_clipboard_received_callback,
                                        view);
}

static void
create_links_clipboard_received_callback (GtkClipboard     *clipboard,
                                          GtkSelectionData *selection_data,
                                          gpointer          data)
{
        NautilusFilesView *view;
        char *view_uri;

        view = NAUTILUS_FILES_VIEW (data);

        view_uri = nautilus_files_view_get_backing_uri (view);

        if (view->details->slot != NULL) {
                handle_clipboard_data (view, selection_data, view_uri, GDK_ACTION_LINK);
        }

        g_free (view_uri);

        g_object_unref (view);
}

static void
action_create_links (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        NautilusFilesView *view;

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        view = NAUTILUS_FILES_VIEW (user_data);

        g_object_ref (view);
        gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
                                        copied_files_atom,
                                        create_links_clipboard_received_callback,
                                        view);
}

static void
click_policy_changed_callback (gpointer callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->click_policy_changed (view);
}

gboolean
nautilus_files_view_should_sort_directories_first (NautilusFilesView *view)
{
        return view->details->sort_directories_first;
}

static void
sort_directories_first_changed_callback (gpointer callback_data)
{
        NautilusFilesView *view;
        gboolean preference_value;

        view = NAUTILUS_FILES_VIEW (callback_data);

        preference_value =
                g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);

        if (preference_value != view->details->sort_directories_first) {
                view->details->sort_directories_first = preference_value;
                NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->sort_directories_first_changed (view);
        }
}

static void
show_hidden_files_changed_callback (gpointer callback_data)
{
        NautilusFilesView *view;
        gboolean preference_value;

        view = NAUTILUS_FILES_VIEW (callback_data);

        preference_value =
                g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

        nautilus_files_view_set_show_hidden_files (view, preference_value);
}

static gboolean
set_up_scripts_directory_global (void)
{
        char *old_scripts_directory_path;
        char *scripts_directory_path;
        const char *override;

        if (scripts_directory_uri != NULL) {
                return TRUE;
        }

        scripts_directory_path = nautilus_get_scripts_directory_path ();

        override = g_getenv ("GNOME22_USER_DIR");

        if (override) {
                old_scripts_directory_path = g_build_filename (override,
                                                               "nautilus-scripts",
                                                               NULL);
        } else {
                old_scripts_directory_path = g_build_filename (g_get_home_dir (),
                                                               ".gnome2",
                                                               "nautilus-scripts",
                                                               NULL);
        }

        if (g_file_test (old_scripts_directory_path, G_FILE_TEST_IS_DIR)
            && !g_file_test (scripts_directory_path, G_FILE_TEST_EXISTS)) {
                char *updated;
                const char *message;

                /* test if we already attempted to migrate first */
                updated = g_build_filename (old_scripts_directory_path, "DEPRECATED-DIRECTORY", NULL);
                message = _("Nautilus 3.6 deprecated this directory and tried migrating "
                            "this configuration to ~/.local/share/nautilus");
                if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
                        char *parent_dir;

                        parent_dir = g_path_get_dirname (scripts_directory_path);
                        if (g_mkdir_with_parents (parent_dir, 0700) == 0) {
                                int fd, res;

                                /* rename() works fine if the destination directory is
                                 * empty.
                                 */
                                res = g_rename (old_scripts_directory_path, scripts_directory_path);
                                if (res == -1) {
                                        fd = g_creat (updated, 0600);
                                        if (fd != -1) {
                                                res = write (fd, message, strlen (message));
                                                close (fd);
                                        }
                                }
                        }
                        g_free (parent_dir);
                }

                g_free (updated);
        }

        if (g_mkdir_with_parents (scripts_directory_path, 0700) == 0) {
                scripts_directory_uri = g_filename_to_uri (scripts_directory_path, NULL, NULL);
                scripts_directory_uri_length = strlen (scripts_directory_uri);
        }

        g_free (scripts_directory_path);
        g_free (old_scripts_directory_path);

        return (scripts_directory_uri != NULL) ? TRUE : FALSE;
}

static void
scripts_added_or_changed_callback (NautilusDirectory *directory,
                                   GList             *files,
                                   gpointer           callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);

        if (view->details->active) {
                schedule_update_context_menus (view);
        }
}

static void
templates_added_or_changed_callback (NautilusDirectory *directory,
                                     GList             *files,
                                     gpointer           callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);

        if (view->details->active) {
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

        if (g_list_find (*directory_list, directory) == NULL) {
                nautilus_directory_ref (directory);

                attributes =
                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
                        NAUTILUS_FILE_ATTRIBUTE_INFO |
                        NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

                nautilus_directory_file_monitor_add (directory, directory_list,
                                                     FALSE, attributes,
                                                     (NautilusDirectoryCallback)changed_callback, view);

                g_signal_connect_object (directory, "files-added",
                                         G_CALLBACK (changed_callback), view, 0);
                g_signal_connect_object (directory, "files-changed",
                                         G_CALLBACK (changed_callback), view, 0);

                *directory_list = g_list_append        (*directory_list, directory);
        }
}

static void
remove_directory_from_directory_list (NautilusFilesView  *view,
                                      NautilusDirectory  *directory,
                                      GList             **directory_list,
                                      GCallback           changed_callback)
{
        *directory_list = g_list_remove        (*directory_list, directory);

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
        add_directory_to_directory_list (view, directory,
                                         &view->details->scripts_directory_list,
                                         G_CALLBACK (scripts_added_or_changed_callback));
}

static void
remove_directory_from_scripts_directory_list (NautilusFilesView *view,
                                              NautilusDirectory *directory)
{
        remove_directory_from_directory_list (view, directory,
                                              &view->details->scripts_directory_list,
                                              G_CALLBACK (scripts_added_or_changed_callback));
}

static void
add_directory_to_templates_directory_list (NautilusFilesView *view,
                                           NautilusDirectory *directory)
{
        add_directory_to_directory_list (view, directory,
                                         &view->details->templates_directory_list,
                                         G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (NautilusFilesView *view,
                                                NautilusDirectory *directory)
{
        remove_directory_from_directory_list (view, directory,
                                              &view->details->templates_directory_list,
                                              G_CALLBACK (templates_added_or_changed_callback));
}

static void
slot_active (NautilusWindowSlot *slot,
             NautilusFilesView  *view)
{
        if (view->details->active) {
                return;
        }

        view->details->active = TRUE;

        /* Avoid updating the toolbar withouth making sure the toolbar
         * zoom slider has the correct adjustment that changes when the
         * view mode changes
         */
        nautilus_files_view_update_context_menus(view);
        nautilus_files_view_update_toolbar_menus (view);

        schedule_update_context_menus (view);

        gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                        "view",
                                        G_ACTION_GROUP (view->details->view_action_group));
}

static void
slot_inactive (NautilusWindowSlot *slot,
               NautilusFilesView  *view)
{
        if (!view->details->active) {
                return;
        }

        view->details->active = FALSE;

        remove_update_context_menus_timeout_callback (view);
        gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                        "view",
                                        NULL);
}

static void
nautilus_files_view_grab_focus (GtkWidget *widget)
{
        /* focus the child of the scrolled window if it exists */
        NautilusFilesView *view;
        GtkWidget *child;

        view = NAUTILUS_FILES_VIEW (widget);
        child = gtk_bin_get_child (GTK_BIN (view->details->scrolled_window));

        GTK_WIDGET_CLASS (nautilus_files_view_parent_class)->grab_focus (widget);

        if (child) {
                gtk_widget_grab_focus (GTK_WIDGET (child));
        }
}

static void
nautilus_files_view_set_selection (NautilusView *nautilus_files_view,
                                   GList        *selection)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (nautilus_files_view);

        if (!view->details->loading) {
                /* If we aren't still loading, set the selection right now,
                 * and reveal the new selection.
                 */
                nautilus_files_view_call_set_selection (view, selection);
                nautilus_files_view_reveal_selection (view);
        } else {
                /* If we are still loading, set the list of pending URIs instead.
                 * done_loading() will eventually select the pending URIs and reveal them.
                 */
                g_list_free_full (view->details->pending_selection, g_object_unref);
                view->details->pending_selection =
                        g_list_copy_deep (selection, (GCopyFunc) g_object_ref, NULL);
        }
}

static char *
get_bulk_rename_tool ()
{
        char *bulk_rename_tool;
        g_settings_get (nautilus_preferences, NAUTILUS_PREFERENCES_BULK_RENAME_TOOL, "^ay", &bulk_rename_tool);
        return g_strstrip (bulk_rename_tool);
}

static gboolean
have_bulk_rename_tool ()
{
        char *bulk_rename_tool;
        gboolean have_tool;

        bulk_rename_tool = get_bulk_rename_tool ();
        have_tool = ((bulk_rename_tool != NULL) && (*bulk_rename_tool != '\0'));
        g_free (bulk_rename_tool);
        return have_tool;
}

static void
nautilus_files_view_destroy (GtkWidget *object)
{
        NautilusFilesView *view;
        GList *node, *next;

        view = NAUTILUS_FILES_VIEW (object);

        view->details->in_destruction = TRUE;
        nautilus_files_view_stop_loading (view);

        if (view->details->model) {
                nautilus_directory_unref (view->details->model);
                view->details->model = NULL;
        }

        for (node = view->details->scripts_directory_list; node != NULL; node = next) {
                next = node->next;
                remove_directory_from_scripts_directory_list (view, node->data);
        }

        for (node = view->details->templates_directory_list; node != NULL; node = next) {
                next = node->next;
                remove_directory_from_templates_directory_list (view, node->data);
        }

        while (view->details->subdirectory_list != NULL) {
                nautilus_files_view_remove_subdirectory (view,
                                                   view->details->subdirectory_list->data);
        }

        remove_update_context_menus_timeout_callback (view);
        remove_update_status_idle_callback (view);

        if (view->details->display_selection_idle_id != 0) {
                g_source_remove (view->details->display_selection_idle_id);
                view->details->display_selection_idle_id = 0;
        }

        if (view->details->reveal_selection_idle_id != 0) {
                g_source_remove (view->details->reveal_selection_idle_id);
                view->details->reveal_selection_idle_id = 0;
        }

        if (view->details->floating_bar_set_status_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_set_status_timeout_id);
                view->details->floating_bar_set_status_timeout_id = 0;
        }

        if (view->details->floating_bar_loading_timeout_id != 0) {
                g_source_remove (view->details->floating_bar_loading_timeout_id);
                view->details->floating_bar_loading_timeout_id = 0;
        }

        g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                              schedule_update_context_menus, view);
        g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                              click_policy_changed_callback, view);
        g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                              sort_directories_first_changed_callback, view);
        g_signal_handlers_disconnect_by_func (gtk_filechooser_preferences,
                                              show_hidden_files_changed_callback, view);
        g_signal_handlers_disconnect_by_func (nautilus_window_state,
                                              nautilus_files_view_display_selection_info, view);
        g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
                                              schedule_update_context_menus, view);
        g_signal_handlers_disconnect_by_func (nautilus_trash_monitor_get (),
                                              nautilus_files_view_trash_state_changed_callback, view);
        g_signal_handlers_disconnect_by_func (nautilus_clipboard_monitor_get (),
                                              clipboard_changed_callback, view);

        nautilus_file_unref (view->details->directory_as_file);
        view->details->directory_as_file = NULL;

        g_clear_object (&view->details->search_query);
        g_clear_object (&view->details->location);

        /* We don't own the slot, so no unref */
        view->details->slot = NULL;

        GTK_WIDGET_CLASS (nautilus_files_view_parent_class)->destroy (object);
}

static void
nautilus_files_view_finalize (GObject *object)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (object);

        g_clear_object (&view->details->view_action_group);
        g_clear_object (&view->details->background_menu);
        g_clear_object (&view->details->selection_menu);
        g_clear_object (&view->details->view_menu_widget);

        if (view->details->rename_file_popover != NULL) {
                gtk_popover_set_relative_to (GTK_POPOVER (view->details->rename_file_popover),
                                             NULL);
        }

        g_hash_table_destroy (view->details->non_ready_files);

        G_OBJECT_CLASS (nautilus_files_view_parent_class)->finalize (object);
}

/**
 * nautilus_files_view_display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: NautilusFilesView for which to display selection info.
 *
 **/
void
nautilus_files_view_display_selection_info (NautilusFilesView *view)
{
        GList *selection;
        goffset non_folder_size;
        gboolean non_folder_size_known;
        guint non_folder_count, folder_count, folder_item_count;
        gboolean folder_item_count_known;
        guint file_item_count;
        GList *p;
        char *first_item_name;
        char *non_folder_count_str;
        char *non_folder_item_count_str;
        char *folder_count_str;
        char *folder_item_count_str;
        char *primary_status;
        char *detail_status;
        NautilusFile *file;

        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

        folder_item_count_known = TRUE;
        folder_count = 0;
        folder_item_count = 0;
        non_folder_count = 0;
        non_folder_size_known = FALSE;
        non_folder_size = 0;
        first_item_name = NULL;
        folder_count_str = NULL;
        folder_item_count_str = NULL;
        non_folder_count_str = NULL;
        non_folder_item_count_str = NULL;

        for (p = selection; p != NULL; p = p->next) {
                file = p->data;
                if (nautilus_file_is_directory (file)) {
                        folder_count++;
                        if (nautilus_file_get_directory_item_count (file, &file_item_count, NULL)) {
                                folder_item_count += file_item_count;
                        } else {
                                folder_item_count_known = FALSE;
                        }
                } else {
                        non_folder_count++;
                        if (!nautilus_file_can_get_size (file)) {
                                non_folder_size_known = TRUE;
                                non_folder_size += nautilus_file_get_size (file);
                        }
                }

                if (first_item_name == NULL) {
                        first_item_name = nautilus_file_get_display_name (file);
                }
        }

        nautilus_file_list_free (selection);

        /* Break out cases for localization's sake. But note that there are still pieces
         * being assembled in a particular order, which may be a problem for some localizers.
         */

        if (folder_count != 0) {
                if (folder_count == 1 && non_folder_count == 0) {
                        folder_count_str = g_strdup_printf (_("“%s” selected"), first_item_name);
                } else {
                        folder_count_str = g_strdup_printf (ngettext("%'d folder selected",
                                                                     "%'d folders selected",
                                                                     folder_count),
                                                            folder_count);
                }

                if (folder_count == 1) {
                        if (!folder_item_count_known) {
                                folder_item_count_str = g_strdup ("");
                        } else {
                                folder_item_count_str = g_strdup_printf (ngettext("(containing %'d item)",
                                                                                  "(containing %'d items)",
                                                                                  folder_item_count),
                                                                         folder_item_count);
                        }
                }
                else {
                        if (!folder_item_count_known) {
                                folder_item_count_str = g_strdup ("");
                        } else {
                                /* translators: this is preceded with a string of form 'N folders' (N more than 1) */
                                folder_item_count_str = g_strdup_printf (ngettext("(containing a total of %'d item)",
                                                                                  "(containing a total of %'d items)",
                                                                                  folder_item_count),
                                                                         folder_item_count);
                        }

                }
        }

        if (non_folder_count != 0) {
                if (folder_count == 0) {
                        if (non_folder_count == 1) {
                                non_folder_count_str = g_strdup_printf (_("“%s” selected"),
                                                                        first_item_name);
                        } else {
                                non_folder_count_str = g_strdup_printf (ngettext("%'d item selected",
                                                                                 "%'d items selected",
                                                                                 non_folder_count),
                                                                        non_folder_count);
                        }
                } else {
                        /* Folders selected also, use "other" terminology */
                        non_folder_count_str = g_strdup_printf (ngettext("%'d other item selected",
                                                                         "%'d other items selected",
                                                                         non_folder_count),
                                                                non_folder_count);
                }

                if (non_folder_size_known) {
                        char *size_string;

                        size_string = g_format_size (non_folder_size);
                        /* This is marked for translation in case a localiser
                         * needs to use something other than parentheses. The
                         * the message in parentheses is the size of the selected items.
                         */
                        non_folder_item_count_str = g_strdup_printf (_("(%s)"), size_string);
                        g_free (size_string);
                } else {
                        non_folder_item_count_str = g_strdup ("");
                }
        }

        if (folder_count == 0 && non_folder_count == 0)        {
                primary_status = NULL;
                detail_status = NULL;
        } else if (folder_count == 0) {
                primary_status = g_strdup (non_folder_count_str);
                detail_status = g_strdup (non_folder_item_count_str);
        } else if (non_folder_count == 0) {
                primary_status = g_strdup (folder_count_str);
                detail_status  = g_strdup (folder_item_count_str);
        } else {
                /* This is marked for translation in case a localizer
                 * needs to change ", " to something else. The comma
                 * is between the message about the number of folders
                 * and the number of items in those folders and the
                 * message about the number of other items and the
                 * total size of those items.
                 */
                primary_status = g_strdup_printf (_("%s %s, %s %s"),
                                                  folder_count_str,
                                                  folder_item_count_str,
                                                  non_folder_count_str,
                                                  non_folder_item_count_str);
                detail_status = NULL;
        }

        g_free (first_item_name);
        g_free (folder_count_str);
        g_free (folder_item_count_str);
        g_free (non_folder_count_str);
        g_free (non_folder_item_count_str);

        set_floating_bar_status (view, primary_status, detail_status);

        g_free (primary_status);
        g_free (detail_status);
}

static void
nautilus_files_view_send_selection_change (NautilusFilesView *view)
{
        g_signal_emit (view, signals[SELECTION_CHANGED], 0);
}

static void
nautilus_files_view_set_location (NautilusView *view,
                                  GFile        *location)
{
        NautilusDirectory *directory;
        NautilusFilesView *files_view;

        nautilus_profile_start (NULL);
        files_view = NAUTILUS_FILES_VIEW (view);
        directory = nautilus_directory_get (location);

        nautilus_files_view_stop_loading (files_view);
        /* In case we want to load a previous search we need to extract the real
         * location and the search location, and load the directory when everything
         * is ready. That's why we cannot use the nautilus_view_set_query, because
         * to set a query we need a previous location loaded, but to load a search
         * location we need to know the real location behind it. */
        if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
                NautilusQuery *previous_query;
                NautilusDirectory *base_model;

                base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (directory));
                previous_query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
                set_search_query_internal (files_view, previous_query, base_model);
                g_object_unref (previous_query);
        } else {
                load_directory (NAUTILUS_FILES_VIEW (view), directory);
        }
        nautilus_directory_unref (directory);
        nautilus_profile_end (NULL);
}

static gboolean
reveal_selection_idle_callback (gpointer data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (data);

        view->details->reveal_selection_idle_id = 0;
        nautilus_files_view_reveal_selection (view);

        return FALSE;
}

static void
check_empty_states (NautilusFilesView *view)
{
        gtk_widget_hide (view->details->no_search_results_widget);
        gtk_widget_hide (view->details->folder_is_empty_widget);
        if (!view->details->loading &&
            !NAUTILUS_IS_DESKTOP_CANVAS_VIEW (view) &&
            nautilus_files_view_is_empty (view)) {
                if (nautilus_view_is_searching (NAUTILUS_VIEW (view))) {
                        gtk_widget_show (view->details->no_search_results_widget);
                } else {
                        gtk_widget_show (view->details->folder_is_empty_widget);
                }
        }
}

static void
done_loading (NautilusFilesView *view,
              gboolean           all_files_seen)
{
        GList *pending_selection;
        GList *selection;
        gboolean do_reveal = FALSE;

        if (!view->details->loading) {
                return;
        }

        nautilus_profile_start (NULL);

        if (!view->details->in_destruction) {
                remove_loading_floating_bar (view);
                schedule_update_context_menus (view);
                schedule_update_status (view);
                nautilus_files_view_update_toolbar_menus (view);
                reset_update_interval (view);

                pending_selection = view->details->pending_selection;
                selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

                if (nautilus_view_is_searching (NAUTILUS_VIEW (view)) &&
                    all_files_seen && !selection && !pending_selection) {
                        nautilus_files_view_select_first (view);
                        do_reveal = TRUE;
                } else if (pending_selection != NULL && all_files_seen) {
                        view->details->pending_selection = NULL;

                        nautilus_files_view_call_set_selection (view, pending_selection);
                        do_reveal = TRUE;
                }

                if (selection)
                        g_list_free_full (selection, g_object_unref);

                if (pending_selection)
                        g_list_free_full (pending_selection, g_object_unref);

                if (do_reveal) {
                        if (NAUTILUS_IS_LIST_VIEW (view)) {
                                /* HACK: We should be able to directly call reveal_selection here,
                                 * but at this point the GtkTreeView hasn't allocated the new nodes
                                 * yet, and it has a bug in the scroll calculation dealing with this
                                 * special case. It would always make the selection the top row, even
                                 * if no scrolling would be neccessary to reveal it. So we let it
                                 * allocate before revealing.
                                 */
                                if (view->details->reveal_selection_idle_id != 0) {
                                        g_source_remove (view->details->reveal_selection_idle_id);
                                }
                                view->details->reveal_selection_idle_id =
                                        g_idle_add (reveal_selection_idle_callback, view);
                        } else {
                                nautilus_files_view_reveal_selection (view);
                        }
                }
                nautilus_files_view_display_selection_info (view);
        }

        view->details->loading = FALSE;
        g_signal_emit (view, signals[END_LOADING], 0, all_files_seen);
        g_object_notify (G_OBJECT (view), "is-loading");

        check_empty_states (view);

        nautilus_profile_end (NULL);
}


typedef struct {
        GHashTable *debuting_files;
        GList           *added_files;
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
debuting_files_add_file_callback (NautilusFilesView *view,
                                  NautilusFile      *new_file,
                                  NautilusDirectory *directory,
                                  DebutingFilesData *data)
{
        GFile *location;

        nautilus_profile_start (NULL);

        location = nautilus_file_get_location (new_file);

        if (g_hash_table_remove (data->debuting_files, location)) {
                nautilus_file_ref (new_file);
                data->added_files = g_list_prepend (data->added_files, new_file);

                if (g_hash_table_size (data->debuting_files) == 0) {
                        nautilus_files_view_call_set_selection (view, data->added_files);
                        nautilus_files_view_reveal_selection (view);
                        g_signal_handlers_disconnect_by_func (view,
                                                              G_CALLBACK (debuting_files_add_file_callback),
                                                              data);
                }
        }

        nautilus_profile_end (NULL);

        g_object_unref (location);
}

typedef struct {
        GList                *added_files;
        NautilusFilesView *directory_view;
} CopyMoveDoneData;

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
        g_assert (data != NULL);

        if (data->directory_view != NULL) {
                g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
                                              (gpointer *) &data->directory_view);
        }

        nautilus_file_list_free (data->added_files);
        g_free (data);
}

static void
pre_copy_move_add_file_callback (NautilusFilesView *view,
                                 NautilusFile      *new_file,
                                 NautilusDirectory *directory,
                                 CopyMoveDoneData  *data)
{
        nautilus_file_ref (new_file);
        data->added_files = g_list_prepend (data->added_files, new_file);
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
         * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
         * must use connect_after.
         */
        g_signal_connect (directory_view, "add-file",
                          G_CALLBACK (pre_copy_move_add_file_callback), copy_move_done_data);

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

        if (GPOINTER_TO_INT (value)) {
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
        NautilusFilesView  *directory_view;
        CopyMoveDoneData *copy_move_done_data;
        DebutingFilesData  *debuting_files_data;
        GList *failed_files;

        copy_move_done_data = (CopyMoveDoneData *) data;
        directory_view = copy_move_done_data->directory_view;

        if (directory_view != NULL) {
                g_assert (NAUTILUS_IS_FILES_VIEW (directory_view));

                debuting_files_data = g_new (DebutingFilesData, 1);
                debuting_files_data->debuting_files = g_hash_table_ref (debuting_files);
                debuting_files_data->added_files = nautilus_file_list_filter (copy_move_done_data->added_files,
                                                                              &failed_files,
                                                                              copy_move_done_partition_func,
                                                                              debuting_files);
                nautilus_file_list_free (copy_move_done_data->added_files);
                copy_move_done_data->added_files = failed_files;

                /* We're passed the same data used by pre_copy_move_add_file_callback, so disconnecting
                 * it will free data. We've already siphoned off the added_files we need, and stashed the
                 * directory_view pointer.
                 */
                g_signal_handlers_disconnect_by_func (directory_view,
                                                      G_CALLBACK (pre_copy_move_add_file_callback),
                                                      data);

                /* Any items in the debuting_files hash table that have
                 * "FALSE" as their value aren't really being copied
                 * or moved, so we can't wait for an add_file signal
                 * to come in for those.
                 */
                g_hash_table_foreach_remove (debuting_files,
                                             remove_not_really_moved_files,
                                             &debuting_files_data->added_files);

                if (g_hash_table_size (debuting_files) == 0) {
                        /* on the off-chance that all the icons have already been added */
                        if (debuting_files_data->added_files != NULL) {
                                nautilus_files_view_call_set_selection (directory_view,
                                                                  debuting_files_data->added_files);
                                nautilus_files_view_reveal_selection (directory_view);
                        }
                        debuting_files_data_free (debuting_files_data);
                } else {
                        /* We need to run after the default handler adds the folder we want to
                         * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
                         * must use connect_after.
                         */
                        g_signal_connect_data (directory_view,
                                               "add-file",
                                               G_CALLBACK (debuting_files_add_file_callback),
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
                         NautilusFile      *file,
                         NautilusDirectory *directory)
{
        if (view->details->model != directory &&
            g_list_find (view->details->subdirectory_list, directory) == NULL) {
                return FALSE;
        }

        return nautilus_directory_contains_file (directory, file);
}

static gboolean
still_should_show_file (NautilusFilesView *view,
                        NautilusFile      *file,
                        NautilusDirectory *directory)
{
        return nautilus_files_view_should_show_file (view, file) &&
                view_file_still_belongs (view, file, directory);
}

static gboolean
ready_to_load (NautilusFile *file)
{
        return nautilus_file_check_if_ready (file,
                                             NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
}

static int
compare_files_cover (gconstpointer a,
                     gconstpointer b,
                     gpointer      callback_data)
{
        const FileAndDirectory *fad1, *fad2;
        NautilusFilesView *view;

        view = callback_data;
        fad1 = a; fad2 = b;

        if (fad1->directory < fad2->directory) {
                return -1;
        } else if (fad1->directory > fad2->directory) {
                return 1;
        } else {
                return NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->compare_files (view, fad1->file, fad2->file);
        }
}
static void
sort_files (NautilusFilesView  *view,
            GList             **list)
{
        *list = g_list_sort_with_data (*list, compare_files_cover, view);

}

/* Go through all the new added and changed files.
 * Put any that are not ready to load in the non_ready_files hash table.
 * Add all the rest to the old_added_files and old_changed_files lists.
 * Sort the old_*_files lists if anything was added to them.
 */
static void
process_new_files (NautilusFilesView *view)
{
        GList *new_added_files, *new_changed_files, *old_added_files, *old_changed_files;
        GHashTable *non_ready_files;
        GList *node, *next;
        FileAndDirectory *pending;
        gboolean in_non_ready;

        new_added_files = view->details->new_added_files;
        view->details->new_added_files = NULL;
        new_changed_files = view->details->new_changed_files;
        view->details->new_changed_files = NULL;

        non_ready_files = view->details->non_ready_files;

        old_added_files = view->details->old_added_files;
        old_changed_files = view->details->old_changed_files;

        /* Newly added files go into the old_added_files list if they're
         * ready, and into the hash table if they're not.
         */
        for (node = new_added_files; node != NULL; node = next) {
                next = node->next;
                pending = (FileAndDirectory *)node->data;
                in_non_ready = g_hash_table_lookup (non_ready_files, pending) != NULL;
                if (nautilus_files_view_should_show_file (view, pending->file)) {
                        if (ready_to_load (pending->file)) {
                                if (in_non_ready) {
                                        g_hash_table_remove (non_ready_files, pending);
                                }
                                new_added_files = g_list_delete_link (new_added_files, node);
                                old_added_files = g_list_prepend (old_added_files, pending);
                        } else {
                                if (!in_non_ready) {
                                        new_added_files = g_list_delete_link (new_added_files, node);
                                        g_hash_table_insert (non_ready_files, pending, pending);
                                }
                        }
                }
        }
        file_and_directory_list_free (new_added_files);

        /* Newly changed files go into the old_added_files list if they're ready
         * and were seen non-ready in the past, into the old_changed_files list
         * if they are read and were not seen non-ready in the past, and into
         * the hash table if they're not ready.
         */
        for (node = new_changed_files; node != NULL; node = next) {
                next = node->next;
                pending = (FileAndDirectory *)node->data;
                if (!still_should_show_file (view, pending->file, pending->directory) || ready_to_load (pending->file)) {
                        if (g_hash_table_lookup (non_ready_files, pending) != NULL) {
                                g_hash_table_remove (non_ready_files, pending);
                                if (still_should_show_file (view, pending->file, pending->directory)) {
                                        new_changed_files = g_list_delete_link (new_changed_files, node);
                                        old_added_files = g_list_prepend (old_added_files, pending);
                                }
                        } else {
                                new_changed_files = g_list_delete_link (new_changed_files, node);
                                old_changed_files = g_list_prepend (old_changed_files, pending);
                        }
                }
        }
        file_and_directory_list_free (new_changed_files);

        /* If any files were added to old_added_files, then resort it. */
        if (old_added_files != view->details->old_added_files) {
                view->details->old_added_files = old_added_files;
                sort_files (view, &view->details->old_added_files);
        }

        /* Resort old_changed_files too, since file attributes
         * relevant to sorting could have changed.
         */
        if (old_changed_files != view->details->old_changed_files) {
                view->details->old_changed_files = old_changed_files;
                sort_files (view, &view->details->old_changed_files);
        }

}

static void
on_end_file_changes (NautilusFilesView *view)
{
        /* Addition and removal of files modify the empty state */
        check_empty_states (view);
        /* If the view is empty, zoom slider and sort menu are insensitive */
        nautilus_files_view_update_toolbar_menus (view);
}

static void
process_old_files (NautilusFilesView *view)
{
        GList *files_added, *files_changed, *node;
        FileAndDirectory *pending;
        GList *selection, *files;
        gboolean send_selection_change;

        files_added = view->details->old_added_files;
        files_changed = view->details->old_changed_files;

        send_selection_change = FALSE;

        if (files_added != NULL || files_changed != NULL) {
                g_signal_emit (view, signals[BEGIN_FILE_CHANGES], 0);

                for (node = files_added; node != NULL; node = node->next) {
                        pending = node->data;
                        g_signal_emit (view,
                                       signals[ADD_FILE], 0, pending->file, pending->directory);
                }

                for (node = files_changed; node != NULL; node = node->next) {
                        pending = node->data;
                        g_signal_emit (view,
                                       signals[still_should_show_file (view, pending->file, pending->directory)
                                               ? FILE_CHANGED : REMOVE_FILE], 0,
                                       pending->file, pending->directory);
                }

                g_signal_emit (view, signals[END_FILE_CHANGES], 0);

                if (files_changed != NULL) {
                        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
                        files = file_and_directory_list_to_files (files_changed);
                        send_selection_change = eel_g_lists_sort_and_check_for_intersection
                                (&files, &selection);
                        nautilus_file_list_free (files);
                        nautilus_file_list_free (selection);
                }

                file_and_directory_list_free (view->details->old_added_files);
                view->details->old_added_files = NULL;

                file_and_directory_list_free (view->details->old_changed_files);
                view->details->old_changed_files = NULL;
        }

        if (send_selection_change) {
                /* Send a selection change since some file names could
                 * have changed.
                 */
                nautilus_files_view_send_selection_change (view);
        }
}

static void
display_pending_files (NautilusFilesView *view)
{
        process_new_files (view);
        process_old_files (view);

        if (!nautilus_files_view_get_selection (NAUTILUS_VIEW (view)) &&
            !view->details->pending_selection) {
                nautilus_files_view_select_first (view);

            }

        if (view->details->model != NULL
            && nautilus_directory_are_all_files_seen (view->details->model)
            && g_hash_table_size (view->details->non_ready_files) == 0) {
                done_loading (view, TRUE);
        }
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (data);

        g_object_ref (G_OBJECT (view));

        view->details->display_selection_idle_id = 0;
        nautilus_files_view_display_selection_info (view);
        nautilus_files_view_send_selection_change (view);

        g_object_unref (G_OBJECT (view));

        return FALSE;
}

static void
remove_update_context_menus_timeout_callback (NautilusFilesView *view)
{
        if (view->details->update_context_menus_timeout_id != 0) {
                g_source_remove (view->details->update_context_menus_timeout_id);
                view->details->update_context_menus_timeout_id = 0;
        }
}

static void
update_context_menus_if_pending (NautilusFilesView *view)
{
        remove_update_context_menus_timeout_callback (view);

        nautilus_files_view_update_context_menus(view);
}

static gboolean
update_context_menus_timeout_callback (gpointer data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (data);

        g_object_ref (G_OBJECT (view));

        view->details->update_context_menus_timeout_id = 0;
        nautilus_files_view_update_context_menus(view);

        g_object_unref (G_OBJECT (view));

        return FALSE;
}

static gboolean
display_pending_callback (gpointer data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (data);

        g_object_ref (G_OBJECT (view));

        view->details->display_pending_source_id = 0;

        display_pending_files (view);

        g_object_unref (G_OBJECT (view));

        return FALSE;
}

static void
schedule_idle_display_of_pending_files (NautilusFilesView *view)
{
        /* Get rid of a pending source as it might be a timeout */
        unschedule_display_of_pending_files (view);

        /* We want higher priority than the idle that handles the relayout
           to avoid a resort on each add. But we still want to allow repaints
           and other hight prio events while we have pending files to show. */
        view->details->display_pending_source_id =
                g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
                                 display_pending_callback, view, NULL);
}

static void
schedule_timeout_display_of_pending_files (NautilusFilesView *view,
                                           guint              interval)
{
         /* No need to schedule an update if there's already one pending. */
        if (view->details->display_pending_source_id != 0) {
                 return;
        }

        view->details->display_pending_source_id =
                g_timeout_add (interval, display_pending_callback, view);
}

static void
unschedule_display_of_pending_files (NautilusFilesView *view)
{
        /* Get rid of source if it's active. */
        if (view->details->display_pending_source_id != 0) {
                g_source_remove (view->details->display_pending_source_id);
                view->details->display_pending_source_id = 0;
        }
}

static void
queue_pending_files (NautilusFilesView  *view,
                     NautilusDirectory  *directory,
                     GList              *files,
                     GList             **pending_list)
{
        if (files == NULL) {
                return;
        }

        *pending_list = g_list_concat (file_and_directory_list_from_files (directory, files),
                                       *pending_list);
        /* Generally we don't want to show the files while the directory is loading
         * the files themselves, so we avoid jumping and oddities. However, for
         * search it can be a long wait, and we actually want to show files as
         * they are getting found. So for search is fine if not all files are
         * seen */
        if (!view->details->loading ||
            (nautilus_directory_are_all_files_seen (directory) ||
             nautilus_view_is_searching (NAUTILUS_VIEW (view)))) {
                schedule_timeout_display_of_pending_files (view, view->details->update_interval);
        }
}

static void
remove_changes_timeout_callback (NautilusFilesView *view)
{
        if (view->details->changes_timeout_id != 0) {
                g_source_remove (view->details->changes_timeout_id);
                view->details->changes_timeout_id = 0;
        }
}

static void
reset_update_interval (NautilusFilesView *view)
{
        view->details->update_interval = UPDATE_INTERVAL_MIN;
        remove_changes_timeout_callback (view);
        /* Reschedule a pending timeout to idle */
        if (view->details->display_pending_source_id != 0) {
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

        view = NAUTILUS_FILES_VIEW (data);

        g_object_ref (G_OBJECT (view));

        now = g_get_monotonic_time ();
        time_delta = now - view->details->last_queued;

        if (time_delta < UPDATE_INTERVAL_RESET*1000) {
                if (view->details->update_interval < UPDATE_INTERVAL_MAX &&
                    view->details->loading) {
                        /* Increase */
                        view->details->update_interval += UPDATE_INTERVAL_INC;
                }
                ret = TRUE;
        } else {
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
        /* Remember when the change was queued */
        view->details->last_queued = g_get_monotonic_time ();

        /* No need to schedule if there are already changes pending or during loading */
        if (view->details->changes_timeout_id != 0 ||
            view->details->loading) {
                return;
        }

        view->details->changes_timeout_id =
                g_timeout_add (UPDATE_INTERVAL_TIMEOUT_INTERVAL, changes_timeout_callback, view);
}

static void
files_added_callback (NautilusDirectory *directory,
                      GList             *files,
                      gpointer           callback_data)
{
        NautilusFilesView *view;
        GtkWindow *window;
        char *uri;

        view = NAUTILUS_FILES_VIEW (callback_data);

        nautilus_profile_start (NULL);

        window = nautilus_files_view_get_containing_window (view);
        uri = nautilus_files_view_get_uri (view);
        DEBUG_FILES (files, "Files added in window %p: %s",
                     window, uri ? uri : "(no directory)");
        g_free (uri);

        schedule_changes (view);

        queue_pending_files (view, directory, files, &view->details->new_added_files);

        /* The number of items could have changed */
        schedule_update_status (view);

        nautilus_profile_end (NULL);
}

static void
files_changed_callback (NautilusDirectory *directory,
                        GList             *files,
                        gpointer           callback_data)
{
        NautilusFilesView *view;
        GtkWindow *window;
        char *uri;

        view = NAUTILUS_FILES_VIEW (callback_data);

        window = nautilus_files_view_get_containing_window (view);
        uri = nautilus_files_view_get_uri (view);
        DEBUG_FILES (files, "Files changed in window %p: %s",
                     window, uri ? uri : "(no directory)");
        g_free (uri);

        schedule_changes (view);

        queue_pending_files (view, directory, files, &view->details->new_changed_files);

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

        nautilus_profile_start (NULL);
        process_new_files (view);
        if (g_hash_table_size (view->details->non_ready_files) == 0) {
                /* Unschedule a pending update and schedule a new one with the minimal
                 * update interval. This gives the view a short chance at gathering the
                 * (cached) deep counts.
                 */
                unschedule_display_of_pending_files (view);
                schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);

                remove_loading_floating_bar (view);
        }
        nautilus_profile_end (NULL);
}

static void
load_error_callback (NautilusDirectory *directory,
                     GError            *error,
                     gpointer           callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);

        /* FIXME: By doing a stop, we discard some pending files. Is
         * that OK?
         */
        nautilus_files_view_stop_loading (view);

        nautilus_report_error_loading_directory
                (nautilus_files_view_get_directory_as_file (view),
                 error,
                 nautilus_files_view_get_containing_window (view));
}

void
nautilus_files_view_add_subdirectory (NautilusFilesView *view,
                                      NautilusDirectory *directory)
{
        NautilusFileAttributes attributes;

        g_assert (!g_list_find (view->details->subdirectory_list, directory));

        nautilus_directory_ref (directory);

        attributes =
                NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
                NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
                NAUTILUS_FILE_ATTRIBUTE_INFO |
                NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
                NAUTILUS_FILE_ATTRIBUTE_MOUNT |
                NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

        nautilus_directory_file_monitor_add (directory,
                                             &view->details->model,
                                             view->details->show_hidden_files,
                                             attributes,
                                             files_added_callback, view);

        g_signal_connect
                (directory, "files-added",
                 G_CALLBACK (files_added_callback), view);
        g_signal_connect
                (directory, "files-changed",
                 G_CALLBACK (files_changed_callback), view);

        view->details->subdirectory_list = g_list_prepend (
                                                           view->details->subdirectory_list, directory);
}

void
nautilus_files_view_remove_subdirectory (NautilusFilesView *view,
                                         NautilusDirectory *directory)
{
        g_assert (g_list_find (view->details->subdirectory_list, directory));

        view->details->subdirectory_list = g_list_remove (
                                                          view->details->subdirectory_list, directory);

        g_signal_handlers_disconnect_by_func (directory,
                                              G_CALLBACK (files_added_callback),
                                              view);
        g_signal_handlers_disconnect_by_func (directory,
                                              G_CALLBACK (files_changed_callback),
                                              view);

        nautilus_directory_file_monitor_remove (directory, &view->details->model);

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
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return view->details->loading;
}

/**
 * nautilus_files_view_get_model:
 *
 * Get the model for this NautilusFilesView.
 * @view: NautilusFilesView of interest.
 *
 * Return value: NautilusDirectory for this view.
 *
 **/
NautilusDirectory *
nautilus_files_view_get_model (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return view->details->model;
}

GtkWidget*
nautilus_files_view_get_content_widget (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

        return view->details->scrolled_window;
}

GdkAtom
nautilus_files_view_get_copied_files_atom (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), GDK_NONE);

        return copied_files_atom;
}

static void
offset_drop_points (GArray *relative_item_points,
                    int     x_offset,
                    int     y_offset)
{
        guint index;

        if (relative_item_points == NULL) {
                return;
        }

        for (index = 0; index < relative_item_points->len; index++) {
                g_array_index (relative_item_points, GdkPoint, index).x += x_offset;
                g_array_index (relative_item_points, GdkPoint, index).y += y_offset;
        }
}

/* special_link_in_selection
 *
 * Return TRUE if one of our special links is in the selection.
 * Special links include the following:
 *         NAUTILUS_DESKTOP_LINK_TRASH, NAUTILUS_DESKTOP_LINK_HOME, NAUTILUS_DESKTOP_LINK_MOUNT
 */

static gboolean
special_link_in_selection (GList *selection)
{
        gboolean saw_link;
        GList *node;
        NautilusFile *file;

        saw_link = FALSE;

        for (node = selection; node != NULL; node = node->next) {
                file = NAUTILUS_FILE (node->data);

                saw_link = NAUTILUS_IS_DESKTOP_ICON_FILE (file);

                if (saw_link) {
                        break;
                }
        }

        return saw_link;
}

/* desktop_or_home_dir_in_selection
 *
 * Return TRUE if either the desktop or the home directory is in the selection.
 */

static gboolean
desktop_or_home_dir_in_selection (GList *selection)
{
        gboolean saw_desktop_or_home_dir;
        GList *node;
        NautilusFile *file;

        saw_desktop_or_home_dir = FALSE;

        for (node = selection; node != NULL; node = node->next) {
                file = NAUTILUS_FILE (node->data);

                saw_desktop_or_home_dir =
                        nautilus_file_is_home (file)
                        || nautilus_file_is_desktop_directory (file);

                if (saw_desktop_or_home_dir) {
                        break;
                }
        }

        return saw_desktop_or_home_dir;
}

static void
trash_or_delete_done_cb (GHashTable        *debuting_uris,
                         gboolean           user_cancel,
                         NautilusFilesView *view)
{
        if (user_cancel) {
                view->details->selection_was_removed = FALSE;
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
        for (node = files; node != NULL; node = node->next) {
                locations = g_list_prepend (locations,
                                            nautilus_file_get_location ((NautilusFile *) node->data));
        }

        locations = g_list_reverse (locations);

        nautilus_file_operations_trash_or_delete (locations,
                                                  parent_window,
                                                  (NautilusDeleteCallback) trash_or_delete_done_cb,
                                                  view);
        g_list_free_full (locations, g_object_unref);
}

static void
open_one_in_new_window (gpointer data,
                        gpointer callback_data)
{
        g_assert (NAUTILUS_IS_FILE (data));
        g_assert (NAUTILUS_IS_FILES_VIEW (callback_data));

        nautilus_files_view_activate_file (NAUTILUS_FILES_VIEW (callback_data),
                                     NAUTILUS_FILE (data),
                                     NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
update_context_menu_position_from_event (NautilusFilesView *view,
                                         GdkEventButton    *event)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        if (event != NULL) {
                view->details->context_menu_position.x = event->x;
                view->details->context_menu_position.y = event->y;
        } else {
                view->details->context_menu_position.x = -1;
                view->details->context_menu_position.y = -1;
        }
}

NautilusFile *
nautilus_files_view_get_directory_as_file (NautilusFilesView *view)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        return view->details->directory_as_file;
}

static GdkPixbuf *
get_menu_icon_for_file (NautilusFile *file,
                        GtkWidget    *widget)
{
        NautilusIconInfo *info;
        GdkPixbuf *pixbuf;
        int size, scale;

        size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
        scale = gtk_widget_get_scale_factor (widget);

        info = nautilus_file_get_icon (file, size, scale, 0);
        pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info, size);
        g_object_unref (info);

        return pixbuf;
}

static GList *
get_extension_selection_menu_items (NautilusFilesView *view)
{
        NautilusWindow *window;
        GList *items;
        GList *providers;
        GList *l;
        GList *selection;

        window = nautilus_files_view_get_window (view);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
        items = NULL;

        for (l = providers; l != NULL; l = l->next) {
                NautilusMenuProvider *provider;
                GList *file_items;

                provider = NAUTILUS_MENU_PROVIDER (l->data);
                file_items = nautilus_menu_provider_get_file_items (provider,
                                                                    GTK_WIDGET (window),
                                                                    selection);
                items = g_list_concat (items, file_items);
        }

        nautilus_module_extension_list_free (providers);

        return items;
}

static GList *
get_extension_background_menu_items (NautilusFilesView *view)
{
        NautilusWindow *window;
        GList *items;
        GList *providers;
        GList *l;

        window = nautilus_files_view_get_window (view);
        providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
        items = NULL;

        for (l = providers; l != NULL; l = l->next) {
                NautilusMenuProvider *provider;
                GList *file_items;

                provider = NAUTILUS_MENU_PROVIDER (l->data);
                file_items = nautilus_menu_provider_get_background_items (provider,
                                                                          GTK_WIDGET (window),
                                                                          view->details->directory_as_file);
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
        gboolean sensitive;
        GSimpleAction *action;

        g_object_get (item,
                      "sensitive", &sensitive,
                      NULL);

        action = g_simple_action_new (action_name, NULL);
        g_signal_connect_data (action, "activate",
                               G_CALLBACK (extension_action_callback),
                               g_object_ref (item),
                               (GClosureNotify) g_object_unref, 0);

        g_action_map_add_action (G_ACTION_MAP (view->details->view_action_group),
                                 G_ACTION (action));
        g_simple_action_set_enabled (action, sensitive);

        g_object_unref (action);
}

static GMenu *
build_menu_for_extension_menu_items (NautilusFilesView *view,
                                     const gchar       *extension_prefix,
                                     GList             *menu_items)
{
        GList *l;
        GMenu *gmenu;
        gint idx = 0;

        gmenu = g_menu_new ();

        for (l = menu_items; l; l = l->next) {
                NautilusMenuItem *item;
                NautilusMenu *menu;
                GMenuItem *menu_item;
                char *name, *label;
                char *extension_id, *prefix, *parsed_name, *detailed_action_name;

                item = NAUTILUS_MENU_ITEM (l->data);

                g_object_get (item,
                              "label", &label,
                              "menu", &menu,
                              "name", &name,
                              NULL);

                extension_id = g_strdup_printf ("%s_%d", extension_prefix, idx);
                prefix = g_strdup_printf ("extension_%s_", extension_id);
                parsed_name = nautilus_escape_action_name (name, prefix);
                add_extension_action (view, item, parsed_name);

                detailed_action_name =  g_strconcat ("view.", parsed_name, NULL);
                menu_item = g_menu_item_new (label, detailed_action_name);

                if (menu != NULL) {
                        GList *children;
                        GMenu *children_menu;

                        children = nautilus_menu_get_items (menu);
                        children_menu = build_menu_for_extension_menu_items (view, extension_id, children);
                        g_menu_item_set_submenu (menu_item, G_MENU_MODEL (children_menu));

                        nautilus_menu_item_list_free (children);
                        g_object_unref (children_menu);
                }

                g_menu_append_item (gmenu, menu_item);
                idx++;

                g_free (extension_id);
                g_free (parsed_name);
                g_free (prefix);
                g_free (detailed_action_name);
                g_free (name);
                g_free (label);
                g_object_unref (menu_item);
        }

        return gmenu;
}

static void
add_extension_menu_items (NautilusFilesView *view,
                          const gchar       *extension_prefix,
                          GList             *menu_items,
                          GMenu             *insertion_menu)
{
        GMenu *menu;

        menu = build_menu_for_extension_menu_items (view, extension_prefix, menu_items);
        nautilus_gmenu_merge (insertion_menu,
                              menu,
                              "extensions",
                              FALSE);

        g_object_unref (menu);
}

static void
update_extensions_menus (NautilusFilesView *view)
{
        GList *selection_items, *background_items;

        selection_items = get_extension_selection_menu_items (view);
        if (selection_items != NULL) {
                add_extension_menu_items (view,
                                          "selection",
                                          selection_items,
                                          view->details->selection_menu);
                nautilus_menu_item_list_free (selection_items);
        }

        background_items = get_extension_background_menu_items (view);
        if (background_items != NULL) {
                add_extension_menu_items (view,
                                          "background",
                                          background_items,
                                          view->details->background_menu);
                nautilus_menu_item_list_free (background_items);
        }
}

static char *
change_to_view_directory (NautilusFilesView *view)
{
        char *path;
        char *old_path;

        old_path = g_get_current_dir ();

        path = get_view_directory (view);

        /* FIXME: What to do about non-local directories? */
        if (path != NULL) {
                g_chdir (path);
        }

        g_free (path);

        return old_path;
}

static char **
get_file_names_as_parameter_array (GList             *selection,
                                   NautilusDirectory *model)
{
        NautilusFile *file;
        char **parameters;
        GList *node;
        GFile *file_location;
        GFile *model_location;
        int i;

        if (model == NULL) {
                return NULL;
        }

        parameters = g_new (char *, g_list_length (selection) + 1);

        model_location = nautilus_directory_get_location (model);

        for (node = selection, i = 0; node != NULL; node = node->next, i++) {
                file = NAUTILUS_FILE (node->data);

                if (!nautilus_file_is_local (file)) {
                        parameters[i] = NULL;
                        g_strfreev (parameters);
                        return NULL;
                }

                file_location = nautilus_file_get_location (NAUTILUS_FILE (node->data));
                parameters[i] = g_file_get_relative_path (model_location, file_location);
                if (parameters[i] == NULL) {
                        parameters[i] = g_file_get_path (file_location);
                }
                g_object_unref (file_location);
        }

        g_object_unref (model_location);

        parameters[i] = NULL;
        return parameters;
}

static char *
get_file_paths_or_uris_as_newline_delimited_string (GList    *selection,
                                                    gboolean  get_paths)
{
        char *path;
        char *uri;
        char *result;
        NautilusDesktopLink *link;
        GString *expanding_string;
        GList *node;
        GFile *location;

        expanding_string = g_string_new ("");
        for (node = selection; node != NULL; node = node->next) {
                uri = NULL;
                if (NAUTILUS_IS_DESKTOP_ICON_FILE (node->data)) {
                        link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (node->data));
                        if (link != NULL) {
                                location = nautilus_desktop_link_get_activation_location (link);
                                uri = g_file_get_uri (location);
                                g_object_unref (location);
                                g_object_unref (G_OBJECT (link));
                        }
                } else {
                        uri = nautilus_file_get_uri (NAUTILUS_FILE (node->data));
                }
                if (uri == NULL) {
                        continue;
                }

                if (get_paths) {
                        path = g_filename_from_uri (uri, NULL, NULL);
                        if (path != NULL) {
                                g_string_append (expanding_string, path);
                                g_free (path);
                                g_string_append (expanding_string, "\n");
                        }
                } else {
                        g_string_append (expanding_string, uri);
                        g_string_append (expanding_string, "\n");
                }
                g_free (uri);
        }

        result = expanding_string->str;
        g_string_free (expanding_string, FALSE);

        return result;
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

/* returns newly allocated strings for setting the environment variables */
static void
get_strings_for_environment_variables (NautilusFilesView  *view,
                                       GList              *selected_files,
                                       char              **file_paths,
                                       char              **uris,
                                       char              **uri)
{
        char *directory_uri;

        /* We need to check that the directory uri starts with "file:" since
         * nautilus_directory_is_local returns FALSE for nfs.
         */
        directory_uri = nautilus_directory_get_uri (view->details->model);
        if (g_str_has_prefix (directory_uri, "file:") ||
            eel_uri_is_desktop (directory_uri) ||
            eel_uri_is_trash (directory_uri)) {
                *file_paths = get_file_paths_as_newline_delimited_string (selected_files);
        } else {
                *file_paths = g_strdup ("");
        }
        g_free (directory_uri);

        *uris = get_file_uris_as_newline_delimited_string (selected_files);

        *uri = nautilus_directory_get_uri (view->details->model);
        if (eel_uri_is_desktop (*uri)) {
                g_free (*uri);
                *uri = nautilus_get_desktop_directory_uri ();
        }
}

/*
 * Set up some environment variables that scripts can use
 * to take advantage of the current Nautilus state.
 */
static void
set_script_environment_variables (NautilusFilesView *view,
                                  GList             *selected_files)
{
        char *file_paths;
        char *uris;
        char *uri;
        char *geometry_string;

        get_strings_for_environment_variables (view, selected_files,
                                               &file_paths, &uris, &uri);

        g_setenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);
        g_free (file_paths);

        g_setenv ("NAUTILUS_SCRIPT_SELECTED_URIS", uris, TRUE);
        g_free (uris);

        g_setenv ("NAUTILUS_SCRIPT_CURRENT_URI", uri, TRUE);
        g_free (uri);

        geometry_string = eel_gtk_window_get_geometry_string
                (GTK_WINDOW (nautilus_files_view_get_containing_window (view)));
        g_setenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY", geometry_string, TRUE);
        g_free (geometry_string);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
        g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_FILE_PATHS");
        g_unsetenv ("NAUTILUS_SCRIPT_SELECTED_URIS");
        g_unsetenv ("NAUTILUS_SCRIPT_CURRENT_URI");
        g_unsetenv ("NAUTILUS_SCRIPT_WINDOW_GEOMETRY");
}

static void
run_script (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
        ScriptLaunchParameters *launch_parameters;
        GdkScreen *screen;
        GList *selected_files;
        char *file_uri;
        char *local_file_path;
        char *quoted_path;
        char *old_working_dir;
        char **parameters;

        launch_parameters = (ScriptLaunchParameters *) user_data;

        file_uri = nautilus_file_get_uri (launch_parameters->file);
        local_file_path = g_filename_from_uri (file_uri, NULL, NULL);
        g_assert (local_file_path != NULL);
        g_free (file_uri);

        quoted_path = g_shell_quote (local_file_path);
        g_free (local_file_path);

        old_working_dir = change_to_view_directory (launch_parameters->directory_view);

        selected_files = nautilus_view_get_selection (NAUTILUS_VIEW (launch_parameters->directory_view));
        set_script_environment_variables (launch_parameters->directory_view, selected_files);

        parameters = get_file_names_as_parameter_array (selected_files,
                                                        launch_parameters->directory_view->details->model);

        screen = gtk_widget_get_screen (GTK_WIDGET (launch_parameters->directory_view));

        DEBUG ("run_script, script_path=“%s” (omitting script parameters)",
               local_file_path);

        nautilus_launch_application_from_command_array (screen, quoted_path, FALSE,
                                                        (const char * const *) parameters);
        g_strfreev (parameters);

        nautilus_file_list_free (selected_files);
        unset_script_environment_variables ();
        g_chdir (old_working_dir);
        g_free (old_working_dir);
        g_free (quoted_path);
}

static void
add_script_to_scripts_menus (NautilusFilesView *view,
                             NautilusFile      *file,
                             GMenu             *menu)
{
        gchar *name;
        GdkPixbuf *mimetype_icon;
        gchar *action_name, *detailed_action_name;
        ScriptLaunchParameters *launch_parameters;
        GAction *action;
        GMenuItem *menu_item;
	const gchar *shortcut;

        launch_parameters = script_launch_parameters_new (file, view);

        name = nautilus_file_get_display_name (file);
        action_name = nautilus_escape_action_name (name, "script_");

        action = G_ACTION (g_simple_action_new (action_name, NULL));

        g_signal_connect_data (action, "activate",
                               G_CALLBACK (run_script),
                               launch_parameters,
                               (GClosureNotify)script_launch_parameters_free, 0);

        g_action_map_add_action (G_ACTION_MAP (view->details->view_action_group), action);

        g_object_unref (action);

        detailed_action_name =  g_strconcat ("view.", action_name, NULL);
        menu_item = g_menu_item_new (name, detailed_action_name);

        mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
        if (mimetype_icon != NULL) {
                g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
                g_object_unref (mimetype_icon);
        }

        g_menu_append_item (menu, menu_item);

	if ((shortcut = g_hash_table_lookup (script_accels, name))) {
		nautilus_application_add_accelerator (g_application_get_default(),
						      detailed_action_name, shortcut);
	}

        g_free (name);
        g_free (action_name);
        g_free (detailed_action_name);
        g_object_unref (menu_item);
}

static gboolean
directory_belongs_in_scripts_menu (const char *uri)
{
        int num_levels;
        int i;

        if (!g_str_has_prefix (uri, scripts_directory_uri)) {
                return FALSE;
        }

        num_levels = 0;
        for (i = scripts_directory_uri_length; uri[i] != '\0'; i++) {
                if (uri[i] == '/') {
                        num_levels++;
                }
        }

        if (num_levels > MAX_MENU_LEVELS) {
                return FALSE;
        }

        return TRUE;
}

/* Expected format: accel script_name */
static void
nautilus_load_custom_accel_for_scripts (void)
{
	gchar *path, *contents;
	gchar **lines, **result;
	GError *error = NULL;
	const int max_len = 100;
	int i;

	path = g_build_filename (g_get_user_config_dir (), SHORTCUTS_PATH, NULL);

	if (g_file_get_contents (path, &contents, NULL, &error)) {
		lines = g_strsplit (contents, "\n", -1);
		for (i = 0; lines[i] && (strstr (lines[i], " ") > 0); i++) {
			result = g_strsplit (lines[i], " ", 2);
			g_hash_table_insert (script_accels,
					     g_strndup (result[1], max_len),
					     g_strndup (result[0], max_len));
			g_strfreev (result);
		}

		g_free (contents);
		g_strfreev (lines);
	} else {
		DEBUG ("Unable to open '%s', error message: %s", path, error->message);
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
        gchar *file_name;
        int num;

        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
        g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

	if (script_accels == NULL) {
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
        for (node = filtered; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++) {
                file = node->data;
                if (nautilus_file_is_directory (file)) {
                        uri = nautilus_file_get_uri (file);
                        if (directory_belongs_in_scripts_menu (uri)) {
                                dir = nautilus_directory_get_by_uri (uri);
                                add_directory_to_scripts_directory_list (view, dir);

                                children_menu = update_directory_in_scripts_menu (view, dir);

                                if (children_menu != NULL) {
                                        file_name = nautilus_file_get_display_name (file);
                                        menu_item = g_menu_item_new_submenu (file_name,
                                                                             G_MENU_MODEL (children_menu));
                                        g_menu_append_item (menu, menu_item);
                                        any_scripts = TRUE;
                                        g_object_unref (menu_item);
                                        g_object_unref (children_menu);
                                        g_free (file_name);
                                }

                                nautilus_directory_unref (dir);
                        }
                        g_free (uri);
                } else if (nautilus_file_is_launchable (file)) {
                        add_script_to_scripts_menus (view, file, menu);
                        any_scripts = TRUE;
                }
        }

        nautilus_file_list_free (filtered);

        if (!any_scripts) {
                g_object_unref (menu);
                menu = NULL;
        }

        return menu;
}



static void
update_scripts_menu (NautilusFilesView *view)
{
        GList *sorted_copy, *node;
        NautilusDirectory *directory;
        GMenu *submenu;
        char *uri;

        sorted_copy = nautilus_directory_list_sort_by_uri
                (nautilus_directory_list_copy (view->details->scripts_directory_list));

        for (node = sorted_copy; node != NULL; node = node->next) {
                directory = node->data;

                uri = nautilus_directory_get_uri (directory);
                if (!directory_belongs_in_scripts_menu (uri)) {
                        remove_directory_from_scripts_directory_list (view, directory);
                }
                g_free (uri);
        }
        nautilus_directory_list_free (sorted_copy);

        directory = nautilus_directory_get_by_uri (scripts_directory_uri);
        submenu = update_directory_in_scripts_menu (view, directory);
        if (submenu != NULL) {
                nautilus_gmenu_merge (view->details->selection_menu,
                                      submenu,
                                      "scripts-submenu",
                                      TRUE);
                g_object_unref (submenu);
        }

        view->details->scripts_present = submenu != NULL;
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
        char *tmp, *uri, *name;
        GdkPixbuf *mimetype_icon;
        char *action_name, *detailed_action_name;
        CreateTemplateParameters *parameters;
        GAction *action;
        GMenuItem *menu_item;

        tmp = nautilus_file_get_display_name (file);
        name = eel_filename_strip_extension (tmp);
        g_free (tmp);

        uri = nautilus_file_get_uri (file);
        action_name = nautilus_escape_action_name (uri, "template_");
        action = G_ACTION (g_simple_action_new (action_name, NULL));
        parameters = create_template_parameters_new (file, view);

        g_signal_connect_data (action, "activate",
                               G_CALLBACK (create_template),
                               parameters,
                               (GClosureNotify)create_templates_parameters_free, 0);

        g_action_map_add_action (G_ACTION_MAP (view->details->view_action_group), action);

        detailed_action_name =  g_strconcat ("view.", action_name, NULL);
        menu_item = g_menu_item_new (name, detailed_action_name);

        mimetype_icon = get_menu_icon_for_file (file, GTK_WIDGET (view));
        if (mimetype_icon != NULL) {
                g_menu_item_set_icon (menu_item, G_ICON (mimetype_icon));
                g_object_unref (mimetype_icon);
        }

        g_menu_append_item (menu, menu_item);

        g_free (name);
        g_free (uri);
        g_free (action_name);
        g_free (detailed_action_name);
        g_object_unref (action);
        g_object_unref (menu_item);
}

static void
update_templates_directory (NautilusFilesView *view)
{
        NautilusDirectory *templates_directory;
        GList *node, *next;
        char *templates_uri;

        for (node = view->details->templates_directory_list; node != NULL; node = next) {
                next = node->next;
                remove_directory_from_templates_directory_list (view, node->data);
        }

        if (nautilus_should_use_templates_directory ()) {
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

        if (templates_directory_uri == NULL) {
                return FALSE;
        }

        if (!g_str_has_prefix (uri, templates_directory_uri)) {
                return FALSE;
        }

        num_levels = 0;
        for (i = strlen (templates_directory_uri); uri[i] != '\0'; i++) {
                if (uri[i] == '/') {
                        num_levels++;
                }
        }

        if (num_levels > MAX_MENU_LEVELS) {
                return FALSE;
        }

        return TRUE;
}

static GMenu *
update_directory_in_templates_menu (NautilusFilesView *view,
                                    NautilusDirectory *directory)
{
        GList *file_list, *filtered, *node;
        GMenu *menu, *children_menu;
        GMenuItem *menu_item;
        gboolean any_templates;
        NautilusFile *file;
        NautilusDirectory *dir;
        char *uri;
        char *templates_directory_uri;
        int num;

        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
        g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

        file_list = nautilus_directory_get_file_list (directory);
        filtered = nautilus_file_list_filter_hidden (file_list, FALSE);
        nautilus_file_list_free (file_list);
        templates_directory_uri = nautilus_get_templates_directory_uri ();
        menu = g_menu_new ();

        filtered = nautilus_file_list_sort_by_display_name (filtered);

        num = 0;
        any_templates = FALSE;
        for (node = filtered; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++) {
                file = node->data;
                if (nautilus_file_is_directory (file)) {
                        uri = nautilus_file_get_uri (file);
                        if (directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
                                dir = nautilus_directory_get_by_uri (uri);
                                add_directory_to_templates_directory_list (view, dir);

                                children_menu = update_directory_in_templates_menu (view, dir);

                                if (children_menu != NULL) {
                                        menu_item = g_menu_item_new_submenu (nautilus_file_get_display_name (file),
                                                                             G_MENU_MODEL (children_menu));
                                        g_menu_append_item (menu, menu_item);
                                        any_templates = TRUE;
                                        g_object_unref (menu_item);
                                        g_object_unref (children_menu);
                                }

                                nautilus_directory_unref (dir);
                        }
                        g_free (uri);
                } else if (nautilus_file_can_read (file)) {
                        add_template_to_templates_menus (view, file, menu);
                        any_templates = TRUE;
                }
        }

        nautilus_file_list_free (filtered);
        g_free (templates_directory_uri);

        if (!any_templates) {
                g_object_unref (menu);
                menu = NULL;
        }

        return menu;
}



static void
update_templates_menu (NautilusFilesView *view)
{
        GList *sorted_copy, *node;
        NautilusDirectory *directory;
        GMenu *submenu;
        char *uri;
        char *templates_directory_uri;

        if (nautilus_should_use_templates_directory ()) {
                templates_directory_uri = nautilus_get_templates_directory_uri ();
        } else {
                view->details->templates_present = FALSE;
                return;
        }


        sorted_copy = nautilus_directory_list_sort_by_uri
                (nautilus_directory_list_copy (view->details->templates_directory_list));

        for (node = sorted_copy; node != NULL; node = node->next) {
                directory = node->data;

                uri = nautilus_directory_get_uri (directory);
                if (!directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
                        remove_directory_from_templates_directory_list (view, directory);
                }
                g_free (uri);
        }
        nautilus_directory_list_free (sorted_copy);

        directory = nautilus_directory_get_by_uri (templates_directory_uri);
        submenu = update_directory_in_templates_menu (view, directory);
        if (submenu != NULL) {
                nautilus_gmenu_merge (view->details->background_menu,
                                      submenu,
                                      "templates-submenu",
                                      FALSE);
                g_object_unref (submenu);
        }

        view->details->templates_present = submenu != NULL;

        g_free (templates_directory_uri);
}


static void
action_open_scripts_folder (GSimpleAction *action,
                            GVariant      *state,
                            gpointer       user_data)
{
        static GFile *location = NULL;

        if (location == NULL) {
                location = g_file_new_for_uri (scripts_directory_uri);
        }

        nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                 location, 0, NULL, NULL, NULL);
}

typedef struct _CopyCallbackData {
        NautilusFilesView   *view;
        GtkFileChooser *chooser;
        GHashTable     *locations;
        GList          *selection;
        gboolean        is_move;
} CopyCallbackData;

static void
copy_data_free (CopyCallbackData *data)
{
        nautilus_file_list_free (data->selection);
        g_hash_table_destroy (data->locations);
        g_free (data);
}

static gboolean
uri_is_parent_of_selection (GList      *selection,
                            const char *uri)
{
        gboolean found;
        GList *l;
        GFile *file;

        found = FALSE;

        file = g_file_new_for_uri (uri);
        for (l = selection; !found && l != NULL; l = l->next) {
                GFile *parent;
                parent = nautilus_file_get_parent_location (l->data);
                found = g_file_equal (file, parent);
                g_object_unref (parent);
        }
        g_object_unref (file);
        return found;
}

static void
on_destination_dialog_folder_changed (GtkFileChooser *chooser,
                                      gpointer        user_data)
{
        CopyCallbackData *copy_data = user_data;
        char *uri;
        gboolean found;

        uri = gtk_file_chooser_get_current_folder_uri (chooser);
        found = uri_is_parent_of_selection (copy_data->selection, uri);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, !found);
        g_free (uri);
}

static void
on_destination_dialog_response (GtkDialog *dialog,
                                gint       response_id,
                                gpointer   user_data)
{
        CopyCallbackData *copy_data = user_data;

        if (response_id == GTK_RESPONSE_OK) {
                char *target_uri;
                GList *uris, *l;

                target_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

                uris = NULL;
                for (l = copy_data->selection; l != NULL; l = l->next) {
                        uris = g_list_prepend (uris,
                                               nautilus_file_get_uri ((NautilusFile *) l->data));
                }
                uris = g_list_reverse (uris);

                nautilus_files_view_move_copy_items (copy_data->view, uris, NULL, target_uri,
                                                     copy_data->is_move ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
                                                     0, 0);

                g_list_free_full (uris, g_free);
                g_free (target_uri);
        }

        copy_data_free (copy_data);
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
destination_dialog_filter_cb (const GtkFileFilterInfo *filter_info,
                              gpointer                 user_data)
{
        GList *selection = user_data;
        GList *l;

        for (l = selection; l != NULL; l = l->next) {
                char *uri;
                uri = nautilus_file_get_uri (l->data);
                if (strcmp (uri, filter_info->uri) == 0) {
                        g_free (uri);
                        return FALSE;
                }
                g_free (uri);
        }

        return TRUE;
}

static GList *
get_selected_folders (GList *selection)
{
        GList *folders;
        GList *l;

        folders = NULL;
        for (l = selection; l != NULL; l = l->next) {
                if (nautilus_file_is_directory (l->data))
                        folders = g_list_prepend (folders, nautilus_file_ref (l->data));
        }
        return g_list_reverse (folders);
}

static void
copy_or_move_selection (NautilusFilesView *view,
                        gboolean           is_move)
{
        GtkWidget *dialog;
        char *uri;
        CopyCallbackData *copy_data;
        GList *selection;
        const gchar *title;

        if (is_move) {
                title = _("Select Move Destination");
        } else {
                title = _("Select Copy Destination");
        }

        selection = nautilus_files_view_get_selection_for_file_transfer (view);

        dialog = gtk_file_chooser_dialog_new (title,
                                              GTK_WINDOW (nautilus_files_view_get_window (view)),
                                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                              _("_Cancel"), GTK_RESPONSE_CANCEL,
                                              _("_Select"), GTK_RESPONSE_OK,
                                              NULL);

        gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                         GTK_RESPONSE_OK);

        gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        copy_data = g_new0 (CopyCallbackData, 1);
        copy_data->view = view;
        copy_data->selection = selection;
        copy_data->is_move = is_move;
        copy_data->chooser = GTK_FILE_CHOOSER (dialog);
        copy_data->locations = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        if (selection != NULL) {
                GtkFileFilter *filter;
                GList *folders;

                folders = get_selected_folders (selection);

                filter = gtk_file_filter_new ();
                gtk_file_filter_add_custom (filter,
                                            GTK_FILE_FILTER_URI,
                                            destination_dialog_filter_cb,
                                            folders,
                                            (GDestroyNotify)nautilus_file_list_free);
                gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
        }

        uri = nautilus_directory_get_uri (view->details->model);
        gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog), uri);
        g_free (uri);
        g_signal_connect (dialog, "current-folder-changed",
                          G_CALLBACK (on_destination_dialog_folder_changed),
                          copy_data);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (on_destination_dialog_response),
                          copy_data);

        gtk_widget_show_all (dialog);
}

static void
copy_or_cut_files (NautilusFilesView *view,
                   GList             *clipboard_contents,
                   gboolean           cut)
{
        NautilusClipboardInfo info;
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

        info.files = clipboard_contents;
        info.cut = cut;

        target_list = gtk_target_list_new (NULL, 0);
        gtk_target_list_add (target_list, copied_files_atom, 0, 0);
        gtk_target_list_add_uri_targets (target_list, 0);
        gtk_target_list_add_text_targets (target_list, 0);

        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref (target_list);

        gtk_clipboard_set_with_data (nautilus_clipboard_get (GTK_WIDGET (view)),
                                     targets, n_targets,
                                     nautilus_get_clipboard_callback, nautilus_clear_clipboard_callback,
                                     NULL);
        gtk_target_table_free (targets, n_targets);

        nautilus_clipboard_monitor_set_clipboard_info (nautilus_clipboard_monitor_get (), &info);
}

static void
action_copy (GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_files_view_get_selection_for_file_transfer (view);
        copy_or_cut_files (view, selection, FALSE);
        nautilus_file_list_free (selection);
}

static void
action_cut (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_files_view_get_selection_for_file_transfer (view);
        copy_or_cut_files (view, selection, TRUE);
        nautilus_file_list_free (selection);
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
        for (l = selection; l != NULL; l = l->next) {
            item_uris = g_list_prepend (item_uris, nautilus_file_get_uri(l->data));
        }
        item_uris = g_list_reverse (item_uris);

        destination_uri = nautilus_files_view_get_backing_uri (view);

        nautilus_files_view_move_copy_items (view, item_uris, NULL, destination_uri,
                                             GDK_ACTION_LINK,
                                             0, 0);

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

typedef struct {
        NautilusFilesView *view;
        NautilusFile *target;
} PasteIntoData;

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
                                        GtkSelectionData *selection_data,
                                        gpointer          callback_data)
{
        PasteIntoData *data;
        NautilusFilesView *view;
        char *directory_uri;

        data = (PasteIntoData *) callback_data;

        view = NAUTILUS_FILES_VIEW (data->view);

        if (view->details->slot != NULL) {
                directory_uri = nautilus_file_get_activation_uri (data->target);

                paste_clipboard_data (view, selection_data, directory_uri);

                g_free (directory_uri);
        }

        g_object_unref (view);
        nautilus_file_unref (data->target);
        g_free (data);
}

static void
paste_into (NautilusFilesView *view,
            NautilusFile      *target)
{
        PasteIntoData *data;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));
        g_assert (NAUTILUS_IS_FILE (target));

        data = g_new (PasteIntoData, 1);

        data->view = g_object_ref (view);
        data->target = nautilus_file_ref (target);

        gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
                                        copied_files_atom,
                                        paste_into_clipboard_received_callback,
                                        data);
}

static void
action_paste_files_into (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
        NautilusFilesView *view;
        GList *selection;

        view = NAUTILUS_FILES_VIEW (user_data);
        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        if (selection != NULL) {
                paste_into (view, NAUTILUS_FILE (selection->data));
                nautilus_file_list_free (selection);
        }

}

static void
invoke_external_bulk_rename_utility (NautilusFilesView *view,
                                     GList             *selection)
{
        GString *cmd;
        char *parameter;
        char *quoted_parameter;
        char *bulk_rename_tool;
        GList *walk;
        NautilusFile *file;

        /* assemble command line */
        bulk_rename_tool = get_bulk_rename_tool ();
        cmd = g_string_new (bulk_rename_tool);
        g_free (bulk_rename_tool);
        for (walk = selection; walk; walk = walk->next) {
                file = walk->data;
                parameter = nautilus_file_get_uri (file);
                quoted_parameter = g_shell_quote (parameter);
                g_free (parameter);
                cmd = g_string_append (cmd, " ");
                cmd = g_string_append (cmd, quoted_parameter);
                g_free (quoted_parameter);
        }

        /* spawning and error handling */
        nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (view)),
                                                  cmd->str, FALSE, NULL);
        g_string_free (cmd, TRUE);
}

static void
real_action_rename (NautilusFilesView *view,
                    gboolean           select_all)
{
        NautilusFile *file;
        GList *selection;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

        if (selection_not_empty_in_menu_callback (view, selection)) {
                /* If there is more than one file selected, invoke a batch renamer */
                if (selection->next != NULL) {
                        if (have_bulk_rename_tool ()) {
                                invoke_external_bulk_rename_utility (view, selection);
                        }
                } else {
                        file = NAUTILUS_FILE (selection->data);
                        if (!select_all) {
                                /* directories don't have a file extension, so
                                 * they are always pre-selected as a whole */
                                select_all = nautilus_file_is_directory (file);
                        }
                        nautilus_files_view_rename_file_popover_new (view, file);
                }
        }

        nautilus_file_list_free (selection);
}

static void
action_rename (GSimpleAction *action,
               GVariant      *state,
               gpointer       user_data)
{
        real_action_rename (NAUTILUS_FILES_VIEW (user_data), FALSE);
}

#define BG_KEY_PRIMARY_COLOR      "primary-color"
#define BG_KEY_SECONDARY_COLOR    "secondary-color"
#define BG_KEY_COLOR_TYPE         "color-shading-type"
#define BG_KEY_PICTURE_PLACEMENT  "picture-options"
#define BG_KEY_PICTURE_URI        "picture-uri"

static void
set_uri_as_wallpaper (const char *uri)
{
        GSettings *settings;

        settings = gnome_background_preferences;

        g_settings_delay (settings);

        if (uri == NULL)
                uri = "";

        g_settings_set_string (settings, BG_KEY_PICTURE_URI, uri);
        g_settings_set_string (settings, BG_KEY_PRIMARY_COLOR, "#000000");
        g_settings_set_string (settings, BG_KEY_SECONDARY_COLOR, "#000000");
        g_settings_set_enum (settings, BG_KEY_COLOR_TYPE, G_DESKTOP_BACKGROUND_SHADING_SOLID);
        g_settings_set_enum (settings, BG_KEY_PICTURE_PLACEMENT, G_DESKTOP_BACKGROUND_STYLE_ZOOM);

        /* Apply changes atomically. */
        g_settings_apply (settings);
}

static void
wallpaper_copy_done_callback (GHashTable *debuting_files,
                              gboolean    success,
                              gpointer    data)
{
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, debuting_files);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
                char *uri;
                uri = g_file_get_uri (G_FILE (key));
                set_uri_as_wallpaper (uri);
                g_free (uri);
                break;
        }
}

static gboolean
can_set_wallpaper (GList *selection)
{
        NautilusFile *file;

        if (g_list_length (selection) != 1) {
                return FALSE;
        }

        file = NAUTILUS_FILE (selection->data);
        if (!nautilus_file_is_mime_type (file, "image/*")) {
                return FALSE;
        }

        /* FIXME: check file size? */

        return TRUE;
}

static void
action_set_as_wallpaper (GSimpleAction *action,
                         GVariant      *state,
                         gpointer       user_data)
{
        GList *selection;

        /* Copy the item to Pictures/Wallpaper since it may be
           remote. Then set it as the current wallpaper. */

        g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

        selection = nautilus_view_get_selection (user_data);

        if (can_set_wallpaper (selection)
            && selection_not_empty_in_menu_callback (user_data, selection)) {
                NautilusFile *file;
                char *target_uri;
                GList *uris;
                GFile *parent;
                GFile *target;

                file = NAUTILUS_FILE (selection->data);

                parent = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
                target = g_file_get_child (parent, "Wallpapers");
                g_object_unref (parent);
                g_file_make_directory_with_parents (target, NULL, NULL);
                target_uri = g_file_get_uri (target);
                g_object_unref (target);
                uris = g_list_prepend (NULL, nautilus_file_get_uri (file));
                nautilus_file_operations_copy_move (uris,
                                                    NULL,
                                                    target_uri,
                                                    GDK_ACTION_COPY,
                                                    GTK_WIDGET (user_data),
                                                    wallpaper_copy_done_callback,
                                                    NULL);
                g_free (target_uri);
                g_list_free_full (uris, g_free);
        }

        nautilus_file_list_free (selection);
}

static void
file_mount_callback (NautilusFile *file,
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
              error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
                char *text;
                char *name;
                name = nautilus_file_get_display_name (file);
                /* Translators: %s is a file name formatted for display */
                text = g_strdup_printf (_("Unable to access “%s”"), name);
                eel_show_error_dialog (text, error->message,
                                       GTK_WINDOW (nautilus_files_view_get_window (view)));
                g_free (text);
                g_free (name);
        }
}

static void
file_unmount_callback (NautilusFile *file,
                       GFile        *result_location,
                       GError       *error,
                       gpointer      callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);
        g_object_unref (view);

        if (error != NULL &&
            (error->domain != G_IO_ERROR ||
             (error->code != G_IO_ERROR_CANCELLED &&
              error->code != G_IO_ERROR_FAILED_HANDLED))) {
                char *text;
                char *name;
                name = nautilus_file_get_display_name (file);
                /* Translators: %s is a file name formatted for display */
                text = g_strdup_printf (_("Unable to remove “%s”"), name);
                eel_show_error_dialog (text, error->message,
                                       GTK_WINDOW (nautilus_files_view_get_window (view)));
                g_free (text);
                g_free (name);
        }
}

static void
file_eject_callback (NautilusFile *file,
                     GFile        *result_location,
                     GError       *error,
                     gpointer      callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);
        g_object_unref (view);

        if (error != NULL &&
            (error->domain != G_IO_ERROR ||
             (error->code != G_IO_ERROR_CANCELLED &&
              error->code != G_IO_ERROR_FAILED_HANDLED))) {
                char *text;
                char *name;
                name = nautilus_file_get_display_name (file);
                /* Translators: %s is a file name formatted for display */
                text = g_strdup_printf (_("Unable to eject “%s”"), name);
                eel_show_error_dialog (text, error->message,
                                       GTK_WINDOW (nautilus_files_view_get_window (view)));
                g_free (text);
                g_free (name);
        }
}

static void
file_stop_callback (NautilusFile *file,
                    GFile        *result_location,
                    GError       *error,
                    gpointer      callback_data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (callback_data);

        if (error != NULL &&
            (error->domain != G_IO_ERROR ||
             (error->code != G_IO_ERROR_CANCELLED &&
              error->code != G_IO_ERROR_FAILED_HANDLED))) {
                eel_show_error_dialog (_("Unable to stop drive"),
                                       error->message,
                                       GTK_WINDOW (nautilus_files_view_get_window (view)));
        }
}

static void
action_mount_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        NautilusFile *file;
        GList *selection, *l;
        NautilusFilesView *view;
        GMountOperation *mount_op;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                if (nautilus_file_can_mount (file)) {
                        mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
                        g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
                        nautilus_file_mount (file, mount_op, NULL,
                                             file_mount_callback,
                                             view);
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
        NautilusFile *file;
        GList *selection, *l;
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);
                if (nautilus_file_can_unmount (file)) {
                        GMountOperation *mount_op;
                        mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
                        nautilus_file_unmount (file, mount_op, NULL,
                                               file_unmount_callback, g_object_ref (view));
                        g_object_unref (mount_op);
                }
        }
        nautilus_file_list_free (selection);
}

static void
action_eject_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        NautilusFile *file;
        GList *selection, *l;
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                if (nautilus_file_can_eject (file)) {
                        GMountOperation *mount_op;
                        mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
                        nautilus_file_eject (file, mount_op, NULL,
                                             file_eject_callback, g_object_ref (view));
                        g_object_unref (mount_op);
                }
        }
        nautilus_file_list_free (selection);
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
              error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
                char *text;
                char *name;
                name = nautilus_file_get_display_name (file);
                /* Translators: %s is a file name formatted for display */
                text = g_strdup_printf (_("Unable to start “%s”"), name);
                eel_show_error_dialog (text, error->message,
                                       GTK_WINDOW (nautilus_files_view_get_window (view)));
                g_free (text);
                g_free (name);
        }
}

static void
action_start_volume (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        NautilusFile *file;
        GList *selection, *l;
        NautilusFilesView *view;
        GMountOperation *mount_op;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file)) {
                        mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
                        nautilus_file_start (file, mount_op, NULL,
                                             file_start_callback, view);
                        g_object_unref (mount_op);
                }
        }
        nautilus_file_list_free (selection);
}

static void
action_stop_volume (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data)
{
        NautilusFile *file;
        GList *selection, *l;
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (user_data);

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                if (nautilus_file_can_stop (file)) {
                        GMountOperation *mount_op;
                        mount_op = gtk_mount_operation_new (nautilus_files_view_get_containing_window (view));
                        nautilus_file_stop (file, mount_op, NULL,
                                            file_stop_callback, view);
                        g_object_unref (mount_op);
                }
        }
        nautilus_file_list_free (selection);
}

static void
action_detect_media (GSimpleAction *action,
                     GVariant      *state,
                     gpointer       user_data)
{
        NautilusFile *file;
        GList *selection, *l;
        NautilusView *view;

        view = NAUTILUS_VIEW (user_data);

        selection = nautilus_view_get_selection (view);
        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file)) {
                        nautilus_file_poll_for_media (file);
                }
        }
        nautilus_file_list_free (selection);
}

const GActionEntry view_entries[] = {
        /* Toolbar menu */
        { "zoom-in",  action_zoom_in },
        { "zoom-out", action_zoom_out },
        { "zoom-default", action_zoom_default },
        { "show-hidden-files", NULL, NULL, "true", action_show_hidden_files },
        /* Background menu */
        { "new-folder", action_new_folder },
        { "select-all", action_select_all },
        { "paste", action_paste_files },
        { "create-link", action_create_links },
        { "new-document" },
        /* Selection menu */
        { "scripts" },
        { "new-folder-with-selection", action_new_folder_with_selection },
        { "open-scripts-folder", action_open_scripts_folder },
        { "open-item-location", action_open_item_location },
        { "open-with-default-application", action_open_with_default_application },
        { "open-with-other-application", action_open_with_other_application },
        { "open-item-new-window", action_open_item_new_window },
        { "open-item-new-tab", action_open_item_new_tab },
        { "cut", action_cut},
        { "copy", action_copy},
        { "create-link-in-place", action_create_links_in_place },
        { "move-to", action_move_to},
        { "copy-to", action_copy_to},
        { "move-to-trash", action_move_to_trash},
        { "delete-from-trash", action_delete },
        /* We separate the shortcut and the menu item since we want the shortcut
         * to always be available, but we don't want the menu item shown if not
         * completely necesary. Since the visibility of the menu item is based on
         * the action enability, we need to split the actions for the menu and the
         * shortcut. */
        { "delete-permanently-shortcut", action_delete },
        { "delete-permanently-menu-item", action_delete },
        /* This is only shown when the setting to show always delete permanently
         * is set and when the common use cases for delete permanently which uses
         * Delete as a shortcut are not needed. For instance this will be only
         * present when the setting is true and when it can trash files */
        { "permanent-delete-permanently-menu-item", action_delete },
        { "remove-from-recent", action_remove_from_recent },
        { "restore-from-trash", action_restore_from_trash},
        { "paste-into", action_paste_files_into },
        { "rename", action_rename},
        { "properties", action_properties},
        { "set-as-wallpaper", action_set_as_wallpaper },
        { "mount-volume", action_mount_volume },
        { "unmount-volume", action_unmount_volume },
        { "eject-volume", action_eject_volume },
        { "start-volume", action_start_volume },
        { "stop-volume", action_stop_volume },
        { "detect-media", action_detect_media },
        /* Only accesible by shorcuts */
        { "select-pattern", action_select_pattern },
        { "invert-selection", action_invert_selection },
        { "open-file-and-close-window", action_open_file_and_close_window },
        /* Warning dialog for the change of the shorcut to move to trash */
        { "show-move-to-trash-shortcut-changed-dialog", action_show_move_to_trash_shortcut_changed_dialog }
};

static gboolean
can_paste_into_file (NautilusFile *file)
{
        if (nautilus_file_is_directory (file) &&
            nautilus_file_can_write (file)) {
                return TRUE;
        }
        if (nautilus_file_has_activation_uri (file)) {
                GFile *location;
                NautilusFile *activation_file;
                gboolean res;

                location = nautilus_file_get_activation_location (file);
                activation_file = nautilus_file_get (location);
                g_object_unref (location);

                /* The target location might not have data for it read yet,
                   and we can't want to do sync I/O, so treat the unknown
                   case as can-write */
                res = (nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_UNKNOWN) ||
                        (nautilus_file_get_file_type (activation_file) == G_FILE_TYPE_DIRECTORY &&
                         nautilus_file_can_write (activation_file));

                nautilus_file_unref (activation_file);

                return res;
        }

        return FALSE;
}

static void
clipboard_targets_received (GtkClipboard *clipboard,
                            GdkAtom      *targets,
                            int           n_targets,
                            gpointer      user_data)
{
        NautilusFilesView *view;
        gboolean is_data_copied;
        int i;
        GAction *action;

        view = NAUTILUS_FILES_VIEW (user_data);
        is_data_copied = FALSE;

        if (view->details->slot == NULL ||
            !view->details->active) {
                /* We've been destroyed or became inactive since call */
                g_object_unref (view);
                return;
        }

        if (targets) {
                for (i = 0; i < n_targets; i++) {
                        if (targets[i] == copied_files_atom) {
                                is_data_copied = TRUE;
                        }
                }
        }

        action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
                                             "paste");
        /* Take into account if the action was previously disabled for other reasons,
         * like the directory not being writabble */
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     is_data_copied && g_action_get_enabled (action));

        action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
                                             "paste-into");

        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     is_data_copied && g_action_get_enabled (action));

        action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
                                             "create-link");

        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     is_data_copied && g_action_get_enabled (action));

        g_object_unref (view);
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

        if (nautilus_file_can_eject (file)) {
                *show_eject = TRUE;
        }

        if (nautilus_file_can_mount (file)) {
                *show_mount = TRUE;
        }

        if (nautilus_file_can_start (file) || nautilus_file_can_start_degraded (file)) {
                *show_start = TRUE;
        }

        if (nautilus_file_can_stop (file)) {
                *show_stop = TRUE;
        }

        /* Dot not show both Unmount and Eject/Safe Removal; too confusing to
         * have too many menu entries */
        if (nautilus_file_can_unmount (file) && !*show_eject && !*show_stop) {
                *show_unmount = TRUE;
        }

        if (nautilus_file_can_poll_for_media (file) && !nautilus_file_is_media_check_automatic (file)) {
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

        if (files != NULL) {
                if (g_list_length (files) == 1) {
                        original_file = nautilus_file_get_trash_original_file (files->data);
                } else {
                        original_dirs_hash = nautilus_trashed_files_get_original_directories (files, NULL);
                        if (original_dirs_hash != NULL) {
                                original_dirs = g_hash_table_get_keys (original_dirs_hash);
                                if (g_list_length (original_dirs) == 1) {
                                        original_dir = nautilus_file_ref (NAUTILUS_FILE (original_dirs->data));
                                }
                        }
                }
        }

        can_restore = original_file != NULL || original_dirs != NULL;

        nautilus_file_unref (original_file);
        nautilus_file_unref (original_dir);
        g_list_free (original_dirs);

        if (original_dirs_hash != NULL) {
                g_hash_table_destroy (original_dirs_hash);
        }
        return can_restore;
}

static void
clipboard_changed_callback (NautilusClipboardMonitor *monitor,
                            NautilusFilesView        *view)
{
        /* Update paste menu item */
        nautilus_files_view_update_context_menus (view);
}

static gboolean
can_delete_all (GList *files)
{
        NautilusFile *file;
        GList *l;

        for (l = files; l != NULL; l = l->next) {
                file = l->data;
                if (!nautilus_file_can_delete (file)) {
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

        for (l = files; l != NULL; l = l->next) {
                file = l->data;
                if (!nautilus_file_can_trash (file)) {
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

        for (l = files; l != NULL; l = l->next) {
                file = l->data;
                if (!nautilus_file_is_in_trash (file)) {
                        return FALSE;
                }
        }
        return TRUE;
}

GActionGroup *
nautilus_files_view_get_action_group (NautilusFilesView *view)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        return view->details->view_action_group;
}

static void
real_update_actions_state (NautilusFilesView *view)
{
        GList *selection, *l;
        NautilusFile *file;
        gint selection_count;
        gboolean selection_contains_special_link;
        gboolean selection_contains_desktop_or_home_dir;
        gboolean selection_contains_recent;
        gboolean selection_contains_search;
        gboolean selection_all_in_trash;
        gboolean selection_is_read_only;
        gboolean can_create_files;
        gboolean can_delete_files;
        gboolean can_move_files;
        gboolean can_trash_files;
        gboolean can_copy_files;
        gboolean can_link_from_copied_files;
        gboolean can_paste_files_into;
        gboolean item_opens_in_view;
        gboolean is_read_only;
        GAction *action;
        gboolean show_properties;
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

        view_action_group = view->details->view_action_group;

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        selection_count = g_list_length (selection);
        selection_contains_special_link = special_link_in_selection (selection);
        selection_contains_desktop_or_home_dir = desktop_or_home_dir_in_selection (selection);
        selection_contains_recent = showing_recent_directory (view);
        selection_contains_search = nautilus_view_is_searching (NAUTILUS_VIEW (view));
        selection_is_read_only = selection_count == 1 &&
                (!nautilus_file_can_write (NAUTILUS_FILE (selection->data)) &&
                 !nautilus_file_has_activation_uri (NAUTILUS_FILE (selection->data)));
        selection_all_in_trash = all_in_trash (selection);

        is_read_only = nautilus_files_view_is_read_only (view);
        can_create_files = nautilus_files_view_supports_creating_files (view);
        can_delete_files =
                can_delete_all (selection) &&
                selection_count != 0 &&
                !selection_contains_special_link &&
                !selection_contains_desktop_or_home_dir;
        can_trash_files =
                can_trash_all (selection) &&
                selection_count != 0 &&
                !selection_contains_special_link &&
                !selection_contains_desktop_or_home_dir;
        can_copy_files = selection_count != 0
                && !selection_contains_special_link;
        can_link_from_copied_files = !nautilus_clipboard_monitor_is_cut (nautilus_clipboard_monitor_get ()) &&
                                     !selection_contains_recent && !is_read_only;
        can_move_files = can_delete_files && !selection_contains_recent;
        can_paste_files_into = (!selection_contains_recent &&
                                selection_count == 1 &&
                                can_paste_into_file (NAUTILUS_FILE (selection->data)));
        show_properties = !NAUTILUS_IS_DESKTOP_CANVAS_VIEW (view) || selection_count > 0;
         settings_show_delete_permanently = g_settings_get_boolean (nautilus_preferences,
                                                                    NAUTILUS_PREFERENCES_SHOW_DELETE_PERMANENTLY);
         settings_show_create_link = g_settings_get_boolean (nautilus_preferences,
                                                             NAUTILUS_PREFERENCES_SHOW_CREATE_LINK);

        /* Right click actions */
        /* Selection menu actions */
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "new-folder-with-selection");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_create_files && can_delete_files && (selection_count > 1) && !selection_contains_recent);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "rename");
        if (selection_count > 1) {
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                             have_bulk_rename_tool ());
        } else {
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                             selection_count == 1 &&
                                             nautilus_file_can_rename (selection->data));
        }

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "open-item-location");

        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     selection_count == 1 &&
                                     (selection_contains_recent || selection_contains_search));

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "new-folder");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_create_files);


        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        selection_count = g_list_length (selection);

        item_opens_in_view = selection_count != 0;

        for (l = selection; l != NULL; l = l->next) {
                NautilusFile *file;

                file = NAUTILUS_FILE (selection->data);

                if (!nautilus_mime_file_opens_in_view (file)) {
                        item_opens_in_view = FALSE;
                }

                if (!item_opens_in_view) {
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
                                             "set-as-wallpaper");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),         can_set_wallpaper (selection));
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
                                     !selection_all_in_trash && !selection_contains_recent);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "permanent-delete-permanently-menu-item");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_delete_files && can_trash_files &&
                                     settings_show_delete_permanently &&
                                     !selection_all_in_trash && !selection_contains_recent);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "remove-from-recent");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     selection_contains_recent && selection_count > 0);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "cut");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_move_files && !selection_contains_recent);
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
                                             "copy-to");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_copy_files);
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "move-to");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_move_files && !selection_contains_recent);

        /* Drive menu */
        show_mount = show_unmount = show_eject = show_start = show_stop = show_detect_media = FALSE;
        for (l = selection; l != NULL && (show_mount || show_unmount
                                          || show_eject
                                          || show_start || show_stop
                                          || show_detect_media);
             l = l->next) {
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

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "scripts");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     view->details->scripts_present);

        /* Background menu actions */
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "new-folder");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_create_files);
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "paste");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     !is_read_only && !selection_contains_recent);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "create-link");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_link_from_copied_files &&
                                     settings_show_create_link);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "paste-into");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     !selection_is_read_only && !selection_contains_recent &&
                                     can_paste_files_into);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "properties");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     show_properties);
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "new-document");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     can_create_files &&
                                     !selection_contains_recent &&
                                     view->details->templates_present);

        /* Ask the clipboard */
        g_object_ref (view); /* Need to keep the object alive until we get the reply */
        gtk_clipboard_request_targets (nautilus_clipboard_get (GTK_WIDGET (view)),
                                       clipboard_targets_received,
                                       view);

        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "select-all");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     !nautilus_files_view_is_empty (view));

        /* Toolbar menu actions */
        g_action_group_change_action_state (view_action_group,
                                            "show-hidden-files",
                                            g_variant_new_boolean (view->details->show_hidden_files));

        /* Zoom */
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "zoom-in");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     nautilus_files_view_can_zoom_in (view));
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "zoom-out");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     nautilus_files_view_can_zoom_out (view));
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "zoom-default");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     nautilus_files_view_supports_zooming (view));
        action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group),
                                             "zoom-to-level");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                     !nautilus_files_view_is_empty (view));
}

/* Convenience function to be called when updating menus,
 * so children can subclass it and it will be called when
 * they chain up to the parent in update_context_menus
 * or update_toolbar_menus
 */
void
nautilus_files_view_update_actions_state (NautilusFilesView *view)
{
        g_assert(NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_actions_state (view);
}

static void
update_selection_menu (NautilusFilesView *view)
{
        GList *selection, *l;
        NautilusFile *file;
        gint selection_count;
        gboolean show_app, show_run;
        gboolean item_opens_in_view;
        gchar *item_label;
        GAppInfo *app;
        GIcon *app_icon;
        GMenuItem *menu_item;
        gboolean show_mount;
        gboolean show_unmount;
        gboolean show_eject;
        gboolean show_start;
        gboolean show_stop;
        gboolean show_detect_media;
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
        nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
                                             menu_item,
                                             "new-folder-with-selection-section",
                                             FALSE);
        g_object_unref (menu_item);
        g_free (item_label);

        /* Open With <App> menu item */
        show_app = show_run = item_opens_in_view = selection_count != 0;
        for (l = selection; l != NULL; l = l->next) {
                NautilusFile *file;

                file = NAUTILUS_FILE (selection->data);

                if (!nautilus_mime_file_opens_in_external_app (file)) {
                        show_app = FALSE;
                }

                if (!nautilus_mime_file_launches (file)) {
                        show_run = FALSE;
                }

                if (!nautilus_mime_file_opens_in_view (file)) {
                        item_opens_in_view = FALSE;
                }

                if (!show_app && !show_run && !item_opens_in_view) {
                        break;
                }
        }

        item_label = NULL;
        app = NULL;
        app_icon = NULL;
        if (show_app) {
                app = nautilus_mime_get_default_application_for_files (selection);
        }

        char *escaped_app;

        if (app != NULL) {
                escaped_app = eel_str_double_underscores (g_app_info_get_name (app));
                item_label = g_strdup_printf (_("Open With %s"), escaped_app);

                app_icon = g_app_info_get_icon (app);
                if (app_icon != NULL) {
                        g_object_ref (app_icon);
                }
                g_free (escaped_app);
                g_object_unref (app);
        } else if (show_run) {
                item_label = g_strdup (_("Run"));
        } else {
                item_label = g_strdup (_("Open"));
        }

        menu_item = g_menu_item_new (item_label, "view.open-with-default-application");
        if (app_icon != NULL)
                g_menu_item_set_icon (menu_item, app_icon);

        nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
                                             menu_item,
                                             "open-with-default-application-section",
                                             FALSE);

        g_free (item_label);
        g_object_unref (menu_item);

        /* Drives */
        for (l = selection; l != NULL && (show_mount || show_unmount
                                          || show_eject
                                          || show_start || show_stop
                                          || show_detect_media);
             l = l->next) {
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

        if (show_start) {
                switch (start_stop_type) {
                default:
                case G_DRIVE_START_STOP_TYPE_UNKNOWN:
                case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
                        item_label = _("_Start");
                        break;
                case G_DRIVE_START_STOP_TYPE_NETWORK:
                        item_label = _("_Connect");
                        break;
                case G_DRIVE_START_STOP_TYPE_MULTIDISK:
                        item_label = _("_Start Multi-disk Drive");
                        break;
                case G_DRIVE_START_STOP_TYPE_PASSWORD:
                        item_label = _("U_nlock Drive");
                        break;
                }

                menu_item = g_menu_item_new (item_label, "view.start-volume");
                nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
                                                     menu_item,
                                                     "drive-section",
                                                     FALSE);
                g_object_unref (menu_item);
        }

        if (show_stop) {
                switch (start_stop_type) {
                default:
                case G_DRIVE_START_STOP_TYPE_UNKNOWN:
                        item_label = _("Stop Drive");
                        break;
                case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
                        item_label = _("_Safely Remove Drive");
                        break;
                case G_DRIVE_START_STOP_TYPE_NETWORK:
                        item_label = _("_Disconnect");
                        break;
                case G_DRIVE_START_STOP_TYPE_MULTIDISK:
                        item_label = _("_Stop Multi-disk Drive");
                        break;
                case G_DRIVE_START_STOP_TYPE_PASSWORD:
                        item_label = _("_Lock Drive");
                        break;
                }

                menu_item = g_menu_item_new (item_label, "view.stop-volume");
                nautilus_gmenu_add_item_in_submodel (view->details->selection_menu,
                                                     menu_item,
                                                     "drive-section",
                                                     FALSE);
                g_object_unref (menu_item);
        }

        update_scripts_menu (view);
}

static void
update_background_menu (NautilusFilesView *view)
{

        if (nautilus_files_view_supports_creating_files (view) &&
            !showing_recent_directory (view))
                update_templates_menu (view);
}

static void
real_update_context_menus (NautilusFilesView *view)
{
        g_clear_object (&view->details->background_menu);
        g_clear_object (&view->details->selection_menu);

        GtkBuilder *builder;
        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-files-view-context-menus.ui");
        view->details->background_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "background-menu")));
        view->details->selection_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "selection-menu")));
        g_object_unref (builder);

        update_selection_menu (view);
        update_background_menu (view);
        update_extensions_menus (view);

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
        g_assert(NAUTILUS_IS_FILES_VIEW (view));

        NAUTILUS_FILES_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_context_menus (view);
}

static void
nautilus_files_view_reset_view_menu (NautilusFilesView *view)
{
        GActionGroup *view_action_group;
        GVariant *variant;
        GVariantIter iter;
        gboolean show_sort_trash, show_sort_search, show_sort_access, show_sort_modification, sort_available;
        const gchar *hint;

        view_action_group = nautilus_files_view_get_action_group (view);

        gtk_widget_set_visible (view->details->visible_columns,
                                g_action_group_has_action (view_action_group, "visible-columns"));

        sort_available = g_action_group_get_action_enabled (view_action_group, "sort");
        show_sort_trash = show_sort_search = show_sort_modification = show_sort_access = FALSE;
        gtk_widget_set_visible (view->details->sort_menu, sort_available);

        /* We want to make insensitive available actions but that are not current
         * available due to the directory
         */
        gtk_widget_set_sensitive (view->details->sort_menu,
                                  !nautilus_files_view_is_empty (view));
        gtk_widget_set_sensitive (view->details->zoom_level_scale,
                                  !nautilus_files_view_is_empty (view));

        if (sort_available) {
                variant = g_action_group_get_action_state_hint (view_action_group, "sort");
                g_variant_iter_init (&iter, variant);

                while (g_variant_iter_next (&iter, "&s", &hint)) {
                        if (g_strcmp0 (hint, "trash-time") == 0)
                                show_sort_trash = TRUE;
                        if (g_strcmp0 (hint, "search-relevance") == 0)
                                show_sort_search = TRUE;
                }

                g_variant_unref (variant);
        }

        gtk_widget_set_visible (view->details->sort_trash_time, show_sort_trash);
        gtk_widget_set_visible (view->details->sort_search_relevance, show_sort_search);

        variant = g_action_group_get_action_state (view_action_group, "zoom-to-level");
        gtk_adjustment_set_value (view->details->zoom_adjustment,
                                  g_variant_get_int32 (variant));
        g_variant_unref (variant);
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

        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        /* Don't update after destroy (#349551),
         * or if we are not active.
         */
        if (view->details->slot == NULL ||
            !view->details->active) {
                return;
        }
        window = nautilus_files_view_get_window (view);
        nautilus_window_reset_menus (window);

        nautilus_files_view_update_actions_state (view);
        nautilus_files_view_reset_view_menu (view);
}

/**
 * nautilus_files_view_pop_up_selection_context_menu
 *
 * Pop up a context menu appropriate to the selected items.
 * @view: NautilusFilesView of interest.
 * @event: The event that triggered this context menu.
 *
 **/
void
nautilus_files_view_pop_up_selection_context_menu  (NautilusFilesView *view,
                                                    GdkEventButton    *event)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        /* Make the context menu items not flash as they update to proper disabled,
         * etc. states by forcing menus to update now.
         */
        update_context_menus_if_pending (view);

        update_context_menu_position_from_event (view, event);

        nautilus_pop_up_context_menu (GTK_WIDGET (view), view->details->selection_menu, event);
}

/**
 * nautilus_files_view_pop_up_background_context_menu
 *
 * Pop up a context menu appropriate to the view globally at the last right click location.
 * @view: NautilusFilesView of interest.
 *
 **/
void
nautilus_files_view_pop_up_background_context_menu (NautilusFilesView *view,
                                                    GdkEventButton    *event)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        /* Make the context menu items not flash as they update to proper disabled,
         * etc. states by forcing menus to update now.
         */
        update_context_menus_if_pending (view);

        update_context_menu_position_from_event (view, event);

        nautilus_pop_up_context_menu (GTK_WIDGET (view), view->details->background_menu, event);
}

static void
schedule_update_context_menus (NautilusFilesView *view)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        /* Don't schedule updates after destroy (#349551),
          * or if we are not active.
         */
        if (view->details->slot == NULL ||
            !view->details->active) {
                return;
        }

        /* Schedule a menu update with the current update interval */
        if (view->details->update_context_menus_timeout_id == 0) {
                view->details->update_context_menus_timeout_id
                        = g_timeout_add (view->details->update_interval, update_context_menus_timeout_callback, view);
        }
}

static void
remove_update_status_idle_callback (NautilusFilesView *view)
{
        if (view->details->update_status_idle_id != 0) {
                g_source_remove (view->details->update_status_idle_id);
                view->details->update_status_idle_id = 0;
        }
}

static gboolean
update_status_idle_callback (gpointer data)
{
        NautilusFilesView *view;

        view = NAUTILUS_FILES_VIEW (data);
        nautilus_files_view_display_selection_info (view);
        view->details->update_status_idle_id = 0;
        return FALSE;
}

static void
schedule_update_status (NautilusFilesView *view)
{
        g_assert (NAUTILUS_IS_FILES_VIEW (view));

        /* Make sure we haven't already destroyed it */
        if (view->details->slot == NULL) {
                return;
        }

        if (view->details->loading) {
                /* Don't update status bar while loading the dir */
                return;
        }

        if (view->details->update_status_idle_id == 0) {
                view->details->update_status_idle_id =
                        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
                                         update_status_idle_callback, view, NULL);
        }
}

/**
 * nautilus_files_view_notify_selection_changed:
 *
 * Notify this view that the selection has changed. This is normally
 * called only by subclasses.
 * @view: NautilusFilesView whose selection has changed.
 *
 **/
void
nautilus_files_view_notify_selection_changed (NautilusFilesView *view)
{
        GtkWindow *window;
        GList *selection;

        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        window = nautilus_files_view_get_containing_window (view);
        DEBUG_FILES (selection, "Selection changed in window %p", window);
        nautilus_file_list_free (selection);

        view->details->selection_was_removed = FALSE;

        /* Schedule a display of the new selection. */
        if (view->details->display_selection_idle_id == 0) {
                view->details->display_selection_idle_id
                        = g_idle_add (display_selection_info_idle_callback,
                                      view);
        }

        if (view->details->batching_selection_level != 0) {
                view->details->selection_changed_while_batched = TRUE;
        } else {
                /* Here is the work we do only when we're not
                 * batching selection changes. In other words, it's the slower
                 * stuff that we don't want to slow down selection techniques
                 * such as rubberband-selecting in icon view.
                 */

                /* Schedule an update of menu item states to match selection */
                schedule_update_context_menus (view);
        }
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

        g_assert (NAUTILUS_IS_FILES_VIEW (view));
        g_assert (NAUTILUS_IS_DIRECTORY (directory));

        nautilus_profile_start (NULL);

        nautilus_files_view_stop_loading (view);
        g_signal_emit (view, signals[CLEAR], 0);

        view->details->loading = TRUE;

        setup_loading_floating_bar (view);

        /* Update menus when directory is empty, before going to new
         * location, so they won't have any false lingering knowledge
         * of old selection.
         */
        schedule_update_context_menus (view);

        while (view->details->subdirectory_list != NULL) {
                nautilus_files_view_remove_subdirectory (view,
                                                   view->details->subdirectory_list->data);
        }

        /* Avoid freeing it and won't be able to ref it */
        if (view->details->model != directory) {
                nautilus_directory_unref (view->details->model);
                view->details->model = nautilus_directory_ref (directory);
        }

        nautilus_file_unref (view->details->directory_as_file);
        view->details->directory_as_file = nautilus_directory_get_corresponding_file (directory);

        g_clear_object (&view->details->location);
        view->details->location = nautilus_directory_get_location (directory);

        g_object_notify (G_OBJECT (view), "location");
        g_object_notify (G_OBJECT (view), "is-loading");
        g_object_notify (G_OBJECT (view), "is-searching");

        /* FIXME bugzilla.gnome.org 45062: In theory, we also need to monitor metadata here (as
         * well as doing a call when ready), in case external forces
         * change the directory's file metadata.
         */
        attributes =
                NAUTILUS_FILE_ATTRIBUTE_INFO |
                NAUTILUS_FILE_ATTRIBUTE_MOUNT |
                NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
        view->details->metadata_for_directory_as_file_pending = TRUE;
        view->details->metadata_for_files_in_directory_pending = TRUE;
        nautilus_file_call_when_ready
                (view->details->directory_as_file,
                 attributes,
                 metadata_for_directory_as_file_ready_callback, view);
        nautilus_directory_call_when_ready
                (view->details->model,
                 attributes,
                 FALSE,
                 metadata_for_files_in_directory_ready_callback, view);

        /* If capabilities change, then we need to update the menus
         * because of New Folder, and relative emblems.
         */
        attributes =
                NAUTILUS_FILE_ATTRIBUTE_INFO |
                NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO;
        nautilus_file_monitor_add (view->details->directory_as_file,
                                   &view->details->directory_as_file,
                                   attributes);

        view->details->file_changed_handler_id = g_signal_connect
                (view->details->directory_as_file, "changed",
                 G_CALLBACK (file_changed_callback), view);

        nautilus_profile_end (NULL);
}

static void
finish_loading (NautilusFilesView *view)
{
        NautilusFileAttributes attributes;

        nautilus_profile_start (NULL);

        /* Tell interested parties that we've begun loading this directory now.
         * Subclasses use this to know that the new metadata is now available.
         */
        nautilus_profile_start ("BEGIN_LOADING");
        g_signal_emit (view, signals[BEGIN_LOADING], 0);
        nautilus_profile_end ("BEGIN_LOADING");

        check_empty_states (view);

        if (nautilus_directory_are_all_files_seen (view->details->model)) {
                /* Unschedule a pending update and schedule a new one with the minimal
                 * update interval. This gives the view a short chance at gathering the
                 * (cached) deep counts.
                 */
                unschedule_display_of_pending_files (view);
                schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
        }

        /* Start loading. */

        /* Connect handlers to learn about loading progress. */
        view->details->done_loading_handler_id = g_signal_connect
                (view->details->model, "done-loading",
                 G_CALLBACK (done_loading_callback), view);
        view->details->load_error_handler_id = g_signal_connect
                (view->details->model, "load-error",
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
                NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
                NAUTILUS_FILE_ATTRIBUTE_MOUNT |
                NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO;

        nautilus_directory_file_monitor_add (view->details->model,
                                             &view->details->model,
                                             view->details->show_hidden_files,
                                             attributes,
                                             files_added_callback, view);

            view->details->files_added_handler_id = g_signal_connect
                (view->details->model, "files-added",
                 G_CALLBACK (files_added_callback), view);
        view->details->files_changed_handler_id = g_signal_connect
                (view->details->model, "files-changed",
                 G_CALLBACK (files_changed_callback), view);

        nautilus_profile_end (NULL);
}

static void
finish_loading_if_all_metadata_loaded (NautilusFilesView *view)
{
        if (!view->details->metadata_for_directory_as_file_pending &&
            !view->details->metadata_for_files_in_directory_pending) {
                finish_loading (view);
        }
}

static void
metadata_for_directory_as_file_ready_callback (NautilusFile *file,
                                               gpointer      callback_data)
{
        NautilusFilesView *view;

        view = callback_data;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));
        g_assert (view->details->directory_as_file == file);
        g_assert (view->details->metadata_for_directory_as_file_pending);

        nautilus_profile_start (NULL);

        view->details->metadata_for_directory_as_file_pending = FALSE;

        finish_loading_if_all_metadata_loaded (view);
        nautilus_profile_end (NULL);
}

static void
metadata_for_files_in_directory_ready_callback (NautilusDirectory *directory,
                                                GList             *files,
                                                gpointer           callback_data)
{
        NautilusFilesView *view;

        view = callback_data;

        g_assert (NAUTILUS_IS_FILES_VIEW (view));
        g_assert (view->details->model == directory);
        g_assert (view->details->metadata_for_files_in_directory_pending);

        nautilus_profile_start (NULL);

        view->details->metadata_for_files_in_directory_pending = FALSE;

        finish_loading_if_all_metadata_loaded (view);
        nautilus_profile_end (NULL);
}

static void
disconnect_handler (GObject *object,
                    guint   *id)
{
        if (*id != 0) {
                g_signal_handler_disconnect (object, *id);
                *id = 0;
        }
}

static void
disconnect_directory_handler (NautilusFilesView *view,
                              guint             *id)
{
        disconnect_handler (G_OBJECT (view->details->model), id);
}

static void
disconnect_directory_as_file_handler (NautilusFilesView *view,
                                      guint             *id)
{
        disconnect_handler (G_OBJECT (view->details->directory_as_file), id);
}

static void
disconnect_model_handlers (NautilusFilesView *view)
{
        if (view->details->model == NULL) {
                return;
        }
        disconnect_directory_handler (view, &view->details->files_added_handler_id);
        disconnect_directory_handler (view, &view->details->files_changed_handler_id);
        disconnect_directory_handler (view, &view->details->done_loading_handler_id);
        disconnect_directory_handler (view, &view->details->load_error_handler_id);
        disconnect_directory_as_file_handler (view, &view->details->file_changed_handler_id);
        nautilus_file_cancel_call_when_ready (view->details->directory_as_file,
                                              metadata_for_directory_as_file_ready_callback,
                                              view);
        nautilus_directory_cancel_callback (view->details->model,
                                            metadata_for_files_in_directory_ready_callback,
                                            view);
        nautilus_directory_file_monitor_remove (view->details->model,
                                                &view->details->model);
        nautilus_file_monitor_remove (view->details->directory_as_file,
                                      &view->details->directory_as_file);
}

static void
nautilus_files_view_select_file (NautilusFilesView *view,
                                 NautilusFile      *file)
{
        GList file_list;

        file_list.data = file;
        file_list.next = NULL;
        file_list.prev = NULL;
        nautilus_files_view_call_set_selection (view, &file_list);
}

static gboolean
remove_all (gpointer key,
            gpointer value,
            gpointer callback_data)
{
        return TRUE;
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
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        unschedule_display_of_pending_files (view);
        reset_update_interval (view);

        /* Free extra undisplayed files */
        file_and_directory_list_free (view->details->new_added_files);
        view->details->new_added_files = NULL;

        file_and_directory_list_free (view->details->new_changed_files);
        view->details->new_changed_files = NULL;

        g_hash_table_foreach_remove (view->details->non_ready_files, remove_all, NULL);

        file_and_directory_list_free (view->details->old_added_files);
        view->details->old_added_files = NULL;

        file_and_directory_list_free (view->details->old_changed_files);
        view->details->old_changed_files = NULL;

        g_list_free_full (view->details->pending_selection, g_object_unref);
        view->details->pending_selection = NULL;

        done_loading (view, FALSE);

        disconnect_model_handlers (view);
}

gboolean
nautilus_files_view_is_editable (NautilusFilesView *view)
{
        NautilusDirectory *directory;

        directory = nautilus_files_view_get_model (view);

        if (directory != NULL) {
                return nautilus_directory_is_editable (directory);
        }

        return TRUE;
}

static gboolean
nautilus_files_view_is_read_only (NautilusFilesView *view)
{
        NautilusFile *file;

        if (!nautilus_files_view_is_editable (view)) {
                return TRUE;
        }

        file = nautilus_files_view_get_directory_as_file (view);
        if (file != NULL) {
                return !nautilus_file_can_write (file);
        }
        return FALSE;
}

/**
 * nautilus_files_view_should_show_file
 *
 * Returns whether or not this file should be displayed based on
 * current filtering options.
 */
gboolean
nautilus_files_view_should_show_file (NautilusFilesView *view,
                                      NautilusFile      *file)
{
        return nautilus_file_should_show (file,
                                          view->details->show_hidden_files,
                                          view->details->show_foreign_files);
}

static gboolean
real_using_manual_layout (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), FALSE);

        return FALSE;
}

void
nautilus_files_view_ignore_hidden_file_preferences (NautilusFilesView *view)
{
        g_return_if_fail (view->details->model == NULL);

        if (view->details->ignore_hidden_file_preferences) {
                return;
        }

        view->details->show_hidden_files = FALSE;
        view->details->ignore_hidden_file_preferences = TRUE;
}

void
nautilus_files_view_set_show_foreign (NautilusFilesView *view,
                                      gboolean           show_foreign)
{
        view->details->show_foreign_files = show_foreign;
}

char *
nautilus_files_view_get_uri (NautilusFilesView *view)
{
        g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
        if (view->details->model == NULL) {
                return NULL;
        }
        return nautilus_directory_get_uri (view->details->model);
}

void
nautilus_files_view_move_copy_items (NautilusFilesView *view,
                                     const GList       *item_uris,
                                     GArray            *relative_item_points,
                                     const char        *target_uri,
                                     int                copy_action,
                                     int                x,
                                     int                y)
{
        NautilusFile *target_file;

        g_assert (relative_item_points == NULL
                  || relative_item_points->len == 0
                  || g_list_length ((GList *)item_uris) == relative_item_points->len);

        /* add the drop location to the icon offsets */
        offset_drop_points (relative_item_points, x, y);

        target_file = nautilus_file_get_existing_by_uri (target_uri);
        /* special-case "command:" here instead of starting a move/copy */
        if (target_file != NULL && nautilus_file_is_launcher (target_file)) {
                nautilus_file_unref (target_file);
                nautilus_launch_desktop_file (
                                              gtk_widget_get_screen (GTK_WIDGET (view)),
                                              target_uri, item_uris,
                                              nautilus_files_view_get_containing_window (view));
                return;
        } else if (copy_action == GDK_ACTION_COPY &&
                   nautilus_is_file_roller_installed () &&
                   target_file != NULL &&
                   nautilus_file_is_archive (target_file)) {
                char *command, *quoted_uri, *tmp;
                const GList *l;
                GdkScreen  *screen;

                /* Handle dropping onto a file-roller archiver file, instead of starting a move/copy */

                nautilus_file_unref (target_file);

                quoted_uri = g_shell_quote (target_uri);
                command = g_strconcat ("file-roller -a ", quoted_uri, NULL);
                g_free (quoted_uri);

                for (l = item_uris; l != NULL; l = l->next) {
                        quoted_uri = g_shell_quote ((char *) l->data);

                        tmp = g_strconcat (command, " ", quoted_uri, NULL);
                        g_free (command);
                        command = tmp;

                        g_free (quoted_uri);
                }

                screen = gtk_widget_get_screen (GTK_WIDGET (view));
                if (screen == NULL) {
                        screen = gdk_screen_get_default ();
                }

                nautilus_launch_application_from_command (screen, command, FALSE, NULL);
                g_free (command);

                return;
        }
        nautilus_file_unref (target_file);

        nautilus_file_operations_copy_move
                (item_uris, relative_item_points,
                 target_uri, copy_action, GTK_WIDGET (view),
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

void
nautilus_files_view_start_batching_selection_changes (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));

        ++view->details->batching_selection_level;
        view->details->selection_changed_while_batched = FALSE;
}

void
nautilus_files_view_stop_batching_selection_changes (NautilusFilesView *view)
{
        g_return_if_fail (NAUTILUS_IS_FILES_VIEW (view));
        g_return_if_fail (view->details->batching_selection_level > 0);

        if (--view->details->batching_selection_level == 0) {
                if (view->details->selection_changed_while_batched) {
                        nautilus_files_view_notify_selection_changed (view);
                }
        }
}

static GArray *
real_get_selected_icon_locations (NautilusFilesView *view)
{
        /* By default, just return an empty list. */
        return g_array_new (FALSE, TRUE, sizeof (GdkPoint));
}

static void
nautilus_files_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        NautilusFilesView *view = NAUTILUS_FILES_VIEW (object);

        switch (prop_id) {
        case PROP_ICON:
                g_value_set_object (value, nautilus_view_get_icon (NAUTILUS_VIEW (view)));
                break;

        case PROP_VIEW_WIDGET:
                g_value_set_object (value, nautilus_view_get_view_widget (NAUTILUS_VIEW (view)));
                break;

        case PROP_IS_LOADING:
                g_value_set_boolean (value, nautilus_view_is_loading (NAUTILUS_VIEW (view)));
                break;

        case PROP_IS_SEARCH:
                g_value_set_boolean (value, nautilus_view_is_searching (NAUTILUS_VIEW (view)));
                break;

        case PROP_LOCATION:
                g_value_set_object (value, nautilus_view_get_location (NAUTILUS_VIEW (view)));
                break;

        case PROP_SEARCH_QUERY:
                g_value_set_object (value, view->details->search_query);
                break;

        default:
                g_assert_not_reached ();

        }
}

static void
nautilus_files_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        NautilusFilesView *directory_view;
        NautilusWindowSlot *slot;

        directory_view = NAUTILUS_FILES_VIEW (object);

        switch (prop_id)  {
        case PROP_WINDOW_SLOT:
                g_assert (directory_view->details->slot == NULL);

                slot = NAUTILUS_WINDOW_SLOT (g_value_get_object (value));
                directory_view->details->slot = slot;

                g_signal_connect_object (directory_view->details->slot,
                                         "active", G_CALLBACK (slot_active),
                                         directory_view, 0);
                g_signal_connect_object (directory_view->details->slot,
                                         "inactive", G_CALLBACK (slot_inactive),
                                         directory_view, 0);
                break;
        case PROP_SUPPORTS_ZOOMING:
                directory_view->details->supports_zooming = g_value_get_boolean (value);
                break;

        case PROP_LOCATION:
                nautilus_view_set_location (NAUTILUS_VIEW (directory_view), g_value_get_object (value));
                break;

        case PROP_SEARCH_QUERY:
                nautilus_view_set_search_query (NAUTILUS_VIEW (directory_view), g_value_get_object (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}


gboolean
nautilus_files_view_handle_scroll_event (NautilusFilesView *directory_view,
                                         GdkEventScroll    *event)
{
        static gdouble total_delta_y = 0;
        gdouble delta_x, delta_y;

        if (event->state & GDK_CONTROL_MASK) {
                switch (event->direction) {
                case GDK_SCROLL_UP:
                        /* Zoom In */
                        nautilus_files_view_bump_zoom_level (directory_view, 1);
                        return TRUE;

                case GDK_SCROLL_DOWN:
                        /* Zoom Out */
                        nautilus_files_view_bump_zoom_level (directory_view, -1);
                        return TRUE;

                case GDK_SCROLL_SMOOTH:
                        gdk_event_get_scroll_deltas ((const GdkEvent *) event,
                                                     &delta_x, &delta_y);

                        /* try to emulate a normal scrolling event by summing deltas */
                        total_delta_y += delta_y;

                        if (total_delta_y >= 1) {
                                total_delta_y = 0;
                                /* emulate scroll down */
                                nautilus_files_view_bump_zoom_level (directory_view, -1);
                                return TRUE;
                        } else if (total_delta_y <= - 1) {
                                total_delta_y = 0;
                                /* emulate scroll up */
                                nautilus_files_view_bump_zoom_level (directory_view, 1);
                                return TRUE;
                        } else {
                                /* eat event */
                                return TRUE;
                        }

                case GDK_SCROLL_LEFT:
                case GDK_SCROLL_RIGHT:
                        break;

                default:
                        g_assert_not_reached ();
                }
        }

        return FALSE;
}

/* handle Shift+Scroll, which will cause a zoom-in/out */
static gboolean
nautilus_files_view_scroll_event (GtkWidget      *widget,
                                  GdkEventScroll *event)
{
        NautilusFilesView *directory_view;

        directory_view = NAUTILUS_FILES_VIEW (widget);
        if (nautilus_files_view_handle_scroll_event (directory_view, event)) {
                return TRUE;
        }

        return FALSE;
}


static void
action_reload_enabled_changed (GActionGroup      *action_group,
                               gchar             *action_name,
                               gboolean           enabled,
                               NautilusFilesView *view)
{
        gtk_widget_set_visible (view->details->reload, enabled);
}

static void
action_stop_enabled_changed (GActionGroup      *action_group,
                             gchar             *action_name,
                             gboolean           enabled,
                             NautilusFilesView *view)
{
        gtk_widget_set_visible (view->details->stop, enabled);
}

static void
nautilus_files_view_parent_set (GtkWidget *widget,
                                GtkWidget *old_parent)
{
        NautilusWindow *window;
        NautilusFilesView *view;
        GtkWidget *parent;

        view = NAUTILUS_FILES_VIEW (widget);

        parent = gtk_widget_get_parent (widget);
        window = nautilus_files_view_get_window (view);
        g_assert (parent == NULL || old_parent == NULL);

        if (GTK_WIDGET_CLASS (nautilus_files_view_parent_class)->parent_set != NULL) {
                GTK_WIDGET_CLASS (nautilus_files_view_parent_class)->parent_set (widget, old_parent);
        }

        if (view->details->stop_signal_handler > 0) {
                g_signal_handler_disconnect (window, view->details->stop_signal_handler);
                view->details->stop_signal_handler = 0;
        }

        if (view->details->reload_signal_handler > 0) {
                g_signal_handler_disconnect (window, view->details->reload_signal_handler);
                view->details->reload_signal_handler = 0;
        }

        if (parent != NULL) {
                g_assert (old_parent == NULL);

                if (view->details->slot == nautilus_window_get_active_slot (window)) {
                        view->details->active = TRUE;
                        gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                                        "view",
                                                        G_ACTION_GROUP (view->details->view_action_group));
                }

                view->details->stop_signal_handler =
                                g_signal_connect (window,
                                                  "action-enabled-changed::stop",
                                                  G_CALLBACK (action_stop_enabled_changed),
                                                  view);
                view->details->reload_signal_handler =
                                g_signal_connect (window,
                                                  "action-enabled-changed::reload",
                                                  G_CALLBACK (action_reload_enabled_changed),
                                                  view);
        } else {
                remove_update_context_menus_timeout_callback (view);
                gtk_widget_insert_action_group (GTK_WIDGET (nautilus_files_view_get_window (view)),
                                                "view",
                                                NULL);
        }
}

static gboolean
nautilus_files_view_key_press_event (GtkWidget   *widget,
                                     GdkEventKey *event)
{
        NautilusFilesView *view;
        gint i;

        view = NAUTILUS_FILES_VIEW (widget);

        for (i = 0; i < G_N_ELEMENTS (extra_view_keybindings); i++) {
                if (extra_view_keybindings[i].keyval == event->keyval) {
                        GAction *action;

                        action = g_action_map_lookup_action (G_ACTION_MAP (view->details->view_action_group),
                                                             extra_view_keybindings[i].action);

                        if (g_action_get_enabled (action)) {
                                g_action_activate (action, NULL);
                                return GDK_EVENT_STOP;
                        }

                        break;
                }
        }

        return GDK_EVENT_PROPAGATE;
}

static NautilusQuery*
nautilus_files_view_get_search_query (NautilusView *view)
{
        return NAUTILUS_FILES_VIEW (view)->details->search_query;
}

static void
set_search_query_internal (NautilusFilesView *files_view,
                           NautilusQuery     *query,
                           NautilusDirectory *base_model)
{
        GFile *location;

        location = NULL;

        g_set_object (&files_view->details->search_query, query);
        g_object_notify (G_OBJECT (files_view), "search-query");

        if (!nautilus_query_is_empty (query)) {
                if (nautilus_view_is_searching (NAUTILUS_VIEW (files_view))) {
                        /*
                         * Reuse the search directory and reload it.
                         */
                        nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (files_view->details->model), query);
                        /* It's important to use load_directory instead of set_location,
                         * since the location is already correct, however we need
                         * to reload the directory with the new query set. But
                         * set_location has a check for wheter the location is a
                         * search directory, so setting the location to a search
                         * directory when is already serching will enter a loop.
                         */
                        load_directory (files_view, files_view->details->model);
                } else {
                        NautilusDirectory *directory;
                        gchar *uri;

                        uri = nautilus_search_directory_generate_new_uri ();
                        location = g_file_new_for_uri (uri);

                        directory = nautilus_directory_get (location);
                        g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));
                        nautilus_search_directory_set_base_model (NAUTILUS_SEARCH_DIRECTORY (directory), base_model);
                        nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory), query);

                        load_directory (files_view, directory);

                        g_object_notify (G_OBJECT (files_view), "is-searching");

                        nautilus_directory_unref (directory);
                        g_free (uri);
                }
        } else {
                 if (nautilus_view_is_searching (NAUTILUS_VIEW (files_view))) {
                        location = nautilus_directory_get_location (base_model);

                        nautilus_view_set_location (NAUTILUS_VIEW (files_view), location);
                }
        }
        g_clear_object (&location);
}

static void
nautilus_files_view_set_search_query (NautilusView  *view,
                                      NautilusQuery *query)
{
        NautilusDirectory *base_model;
        NautilusFilesView *files_view;

        files_view = NAUTILUS_FILES_VIEW (view);

        if (nautilus_view_is_searching (view)) {
                base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (files_view->details->model));
        } else {
                base_model = files_view->details->model;
        }

        set_search_query_internal (NAUTILUS_FILES_VIEW (view), query, base_model);
}

static GFile*
nautilus_files_view_get_location (NautilusView *view)
{
        NautilusFilesView *files_view;

        files_view = NAUTILUS_FILES_VIEW (view);

        return files_view->details->location;
}

static gboolean
nautilus_files_view_is_loading (NautilusView *view)
{
        NautilusFilesView *files_view;

        files_view = NAUTILUS_FILES_VIEW (view);

        return files_view->details->loading;
}

static void
nautilus_files_view_iface_init (NautilusViewInterface *iface)
{
        iface->get_icon = nautilus_files_view_get_icon;
        iface->get_location = nautilus_files_view_get_location;
        iface->set_location = nautilus_files_view_set_location;
        iface->get_selection = nautilus_files_view_get_selection;
        iface->set_selection = nautilus_files_view_set_selection;
        iface->get_search_query = nautilus_files_view_get_search_query;
        iface->set_search_query = nautilus_files_view_set_search_query;
        iface->get_view_widget = nautilus_files_view_get_view_widget;
        iface->is_searching = nautilus_files_view_is_searching;
        iface->is_loading = nautilus_files_view_is_loading;
}


static void
nautilus_files_view_class_init (NautilusFilesViewClass *klass)
{
        GObjectClass *oclass;
        GtkWidgetClass *widget_class;

        widget_class = GTK_WIDGET_CLASS (klass);
        oclass = G_OBJECT_CLASS (klass);

        oclass->finalize = nautilus_files_view_finalize;
        oclass->get_property = nautilus_files_view_get_property;
        oclass->set_property = nautilus_files_view_set_property;

        widget_class->destroy = nautilus_files_view_destroy;
        widget_class->key_press_event = nautilus_files_view_key_press_event;
        widget_class->scroll_event = nautilus_files_view_scroll_event;
        widget_class->parent_set = nautilus_files_view_parent_set;
        widget_class->grab_focus = nautilus_files_view_grab_focus;

        g_type_class_add_private (klass, sizeof (NautilusFilesViewDetails));

        signals[ADD_FILE] =
                g_signal_new ("add-file",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NautilusFilesViewClass, add_file),
                              NULL, NULL,
                              g_cclosure_marshal_generic,
                              G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
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
        signals[REMOVE_FILE] =
                g_signal_new ("remove-file",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NautilusFilesViewClass, remove_file),
                              NULL, NULL,
                              g_cclosure_marshal_generic,
                              G_TYPE_NONE, 2, NAUTILUS_TYPE_FILE, NAUTILUS_TYPE_DIRECTORY);
        signals[SELECTION_CHANGED] =
                g_signal_new ("selection-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        klass->get_selected_icon_locations = real_get_selected_icon_locations;
        klass->get_backing_uri = real_get_backing_uri;
        klass->using_manual_layout = real_using_manual_layout;
        klass->get_window = nautilus_files_view_get_window;
        klass->update_context_menus = real_update_context_menus;
        klass->update_actions_state = real_update_actions_state;

        copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);

        g_object_class_install_property (
                oclass,
                PROP_WINDOW_SLOT,
                g_param_spec_object ("window-slot",
                                     "Window Slot",
                                     "The parent window slot reference",
                                     NAUTILUS_TYPE_WINDOW_SLOT,
                                     G_PARAM_WRITABLE |
                                     G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (
                oclass,
                PROP_SUPPORTS_ZOOMING,
                g_param_spec_boolean ("supports-zooming",
                                      "Supports zooming",
                                      "Whether the view supports zooming",
                                      TRUE,
                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_STRINGS));

        g_object_class_override_property (oclass, PROP_ICON, "icon");
        g_object_class_override_property (oclass, PROP_VIEW_WIDGET, "view-widget");
        g_object_class_override_property (oclass, PROP_IS_LOADING, "is-loading");
        g_object_class_override_property (oclass, PROP_IS_SEARCH, "is-searching");
        g_object_class_override_property (oclass, PROP_LOCATION, "location");
        g_object_class_override_property (oclass, PROP_SEARCH_QUERY, "search-query");
}

static void
nautilus_files_view_init (NautilusFilesView *view)
{
        GtkBuilder *builder;
        AtkObject *atk_object;
        NautilusDirectory *scripts_directory;
        NautilusDirectory *templates_directory;
        gchar *templates_uri;
        GApplication *app;
        const gchar *open_accels[] = {
                "<control>o",
                "<alt>Down",
                NULL
        };
        const gchar *open_properties[] = {
                "<control>i",
                "<alt>Return",
                NULL
        };

        nautilus_profile_start (NULL);

        view->details = G_TYPE_INSTANCE_GET_PRIVATE (view, NAUTILUS_TYPE_FILES_VIEW,
                                                     NautilusFilesViewDetails);

        /* View menu */
        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-toolbar-view-menu.ui");
        view->details->view_menu_widget =  g_object_ref_sink (gtk_builder_get_object (builder, "view_menu_widget"));
        view->details->zoom_level_scale = GTK_WIDGET (gtk_builder_get_object (builder, "zoom_level_scale"));
        view->details->zoom_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "zoom_adjustment"));

        view->details->sort_menu =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_menu"));
        view->details->sort_trash_time =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_trash_time"));
        view->details->sort_search_relevance =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_search_relevance"));
        view->details->visible_columns =  GTK_WIDGET (gtk_builder_get_object (builder, "visible_columns"));
        view->details->reload =  GTK_WIDGET (gtk_builder_get_object (builder, "reload"));
        view->details->stop =  GTK_WIDGET (gtk_builder_get_object (builder, "stop"));

        g_signal_connect (view->details->zoom_level_scale, "value-changed",
                          G_CALLBACK (zoom_level_changed), view);

        g_signal_connect (view,
                          "end-file-changes",
                          G_CALLBACK (on_end_file_changes),
                          view);

        g_object_unref (builder);

        /* Main widgets */
        gtk_orientable_set_orientation (GTK_ORIENTABLE (view), GTK_ORIENTATION_VERTICAL);
        view->details->overlay = gtk_overlay_new ();
        gtk_widget_set_vexpand (view->details->overlay, TRUE);
        gtk_widget_set_hexpand (view->details->overlay, TRUE);
        gtk_container_add (GTK_CONTAINER (view), view->details->overlay);
        gtk_widget_show (view->details->overlay);

        /* NautilusFloatingBar listen to its parent's 'enter-notify-event' signal
         * and GtkOverlay doesn't have it enabled by default, so we have to add them
         * here.
         */
        gtk_widget_add_events (GTK_WIDGET (view->details->overlay),
                               GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

        /* Scrolled Window */
        view->details->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->details->scrolled_window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_widget_show (view->details->scrolled_window);

        g_signal_connect_swapped (view->details->scrolled_window,
                                  "scroll-event",
                                  G_CALLBACK (nautilus_files_view_scroll_event),
                                  view);

        gtk_container_add (GTK_CONTAINER (view->details->overlay), view->details->scrolled_window);

        /* Empty states */
        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-no-search-results.ui");
        view->details->no_search_results_widget = GTK_WIDGET (gtk_builder_get_object (builder, "no_search_results"));
        gtk_overlay_add_overlay (GTK_OVERLAY (view->details->overlay), view->details->no_search_results_widget);
        gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (view->details->overlay),
                                              view->details->no_search_results_widget,
                                              TRUE);
        g_object_unref (builder);

        builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-folder-is-empty.ui");
        view->details->folder_is_empty_widget = GTK_WIDGET (gtk_builder_get_object (builder, "folder_is_empty"));
        gtk_overlay_add_overlay (GTK_OVERLAY (view->details->overlay), view->details->folder_is_empty_widget);
        gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (view->details->overlay),
                                              view->details->folder_is_empty_widget,
                                              TRUE);
        g_object_unref (builder);

        /* Floating bar */
        view->details->floating_bar = nautilus_floating_bar_new (NULL, NULL, FALSE);
        gtk_widget_set_halign (view->details->floating_bar, GTK_ALIGN_END);
        gtk_widget_set_valign (view->details->floating_bar, GTK_ALIGN_END);
        gtk_overlay_add_overlay (GTK_OVERLAY (view->details->overlay), view->details->floating_bar);

        g_signal_connect (view->details->floating_bar,
                          "action",
                          G_CALLBACK (floating_bar_action_cb),
                          view);

        /* Default to true; desktop-icon-view sets to false */
        view->details->show_foreign_files = TRUE;

        view->details->non_ready_files =
                g_hash_table_new_full (file_and_directory_hash,
                                       file_and_directory_equal,
                                       (GDestroyNotify)file_and_directory_free,
                                       NULL);

        gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (view)),
                                              GTK_JUNCTION_TOP | GTK_JUNCTION_LEFT);

        if (set_up_scripts_directory_global ()) {
                scripts_directory = nautilus_directory_get_by_uri (scripts_directory_uri);
                add_directory_to_scripts_directory_list (view, scripts_directory);
                nautilus_directory_unref (scripts_directory);
        } else {
                g_warning ("Ignoring scripts directory, it may be a broken link\n");
        }

        if (nautilus_should_use_templates_directory ()) {
                templates_uri = nautilus_get_templates_directory_uri ();
                templates_directory = nautilus_directory_get_by_uri (templates_uri);
                g_free (templates_uri);
                add_directory_to_templates_directory_list (view, templates_directory);
                nautilus_directory_unref (templates_directory);
        }
        update_templates_directory (view);

        view->details->sort_directories_first =
                g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
        view->details->show_hidden_files =
                g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);

        g_signal_connect_object (nautilus_trash_monitor_get (), "trash-state-changed",
                                 G_CALLBACK (nautilus_files_view_trash_state_changed_callback), view, 0);

        /* React to clipboard changes */
        g_signal_connect_object (nautilus_clipboard_monitor_get (), "clipboard-changed",
                                 G_CALLBACK (clipboard_changed_callback), view, 0);

        /* Register to menu provider extension signal managing menu updates */
        g_signal_connect_object (nautilus_signaller_get_current (), "popup-menu-changed",
                                 G_CALLBACK (schedule_update_context_menus), view, G_CONNECT_SWAPPED);

        gtk_widget_show (GTK_WIDGET (view));

        g_signal_connect_swapped (nautilus_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_CLICK_POLICY,
                                  G_CALLBACK (click_policy_changed_callback),
                                  view);
        g_signal_connect_swapped (nautilus_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                                  G_CALLBACK (sort_directories_first_changed_callback), view);
        g_signal_connect_swapped (gtk_filechooser_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                                  G_CALLBACK (show_hidden_files_changed_callback), view);
        g_signal_connect_swapped (gnome_lockdown_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                                  G_CALLBACK (schedule_update_context_menus), view);

        view->details->in_destruction = FALSE;

        /* Accessibility */
        atk_object = gtk_widget_get_accessible (GTK_WIDGET (view));
        atk_object_set_name (atk_object, _("Content View"));
        atk_object_set_description (atk_object, _("View of the current folder"));

        view->details->view_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
        g_action_map_add_action_entries (G_ACTION_MAP (view->details->view_action_group),
                                        view_entries,
                                        G_N_ELEMENTS (view_entries),
                                        view);
        gtk_widget_insert_action_group (GTK_WIDGET (view),
                                        "view",
                                        G_ACTION_GROUP (view->details->view_action_group));
        app = g_application_get_default ();

        /* Toolbar menu */
        nautilus_application_add_accelerator (app, "view.zoom-in", "<control>plus");
        nautilus_application_add_accelerator (app, "view.zoom-out", "<control>minus");
        nautilus_application_add_accelerator (app, "view.show-hidden-files", "<control>h");
        /* Background menu */
        nautilus_application_add_accelerator (app, "view.select-all", "<control>a");
        nautilus_application_add_accelerator (app, "view.paste", "<control>v");
        nautilus_application_add_accelerator (app, "view.create-link", "<control>m");
        /* Selection menu */
        gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                               "view.open-with-default-application", open_accels);
        nautilus_application_add_accelerator (app, "view.open-item-new-tab", "<shift><control>t");
        nautilus_application_add_accelerator (app, "view.open-item-new-window", "<shift><control>w");
        nautilus_application_add_accelerator (app, "view.move-to-trash", "Delete");
        nautilus_application_add_accelerator (app, "view.delete-from-trash", "Delete");
        nautilus_application_add_accelerator (app, "view.delete-permanently-shortcut", "<shift>Delete");
        /* When trash is not available, allow the "Delete" key to delete permanently, that is, when
         * the menu item is available, since we never make both the trash and delete-permanently-menu-item
         * actions active */
        nautilus_application_add_accelerator (app, "view.delete-permanently-menu-item", "Delete");
        nautilus_application_add_accelerator (app, "view.permanent-delete-permanently-menu-item", "<shift>Delete");
        gtk_application_set_accels_for_action (GTK_APPLICATION (app), "view.properties", open_properties);
        nautilus_application_add_accelerator (app, "view.open-item-location", "<control><alt>o");
        nautilus_application_add_accelerator (app, "view.rename", "F2");
        nautilus_application_add_accelerator (app, "view.cut", "<control>x");
        nautilus_application_add_accelerator (app, "view.copy", "<control>c");
        nautilus_application_add_accelerator (app, "view.create-link-in-place", "<control><shift>m");
        nautilus_application_add_accelerator (app, "view.new-folder", "<control><shift>n");
        /* Only accesible by shorcuts */
        nautilus_application_add_accelerator (app, "view.select-pattern", "<control>s");
        nautilus_application_add_accelerator (app, "view.zoom-default", "<control>0");
        nautilus_application_add_accelerator (app, "view.invert-selection", "<shift><control>i");
        nautilus_application_add_accelerator (app, "view.open-file-and-close-window", "<control><shift>Down");

        /* Show a warning dialog to inform the user that the shorcut for move to trash
         * changed */
        nautilus_application_add_accelerator (app, "view.show-move-to-trash-shortcut-changed-dialog", "<control>Delete");

        nautilus_profile_end (NULL);
}

NautilusFilesView *
nautilus_files_view_new (guint                id,
                         NautilusWindowSlot *slot)
{
        NautilusFilesView *view = NULL;

        switch (id) {
        case NAUTILUS_VIEW_GRID_ID:
                view = nautilus_canvas_view_new (slot);
        break;
        case NAUTILUS_VIEW_LIST_ID:
                view = nautilus_list_view_new (slot);
        break;
        case NAUTILUS_VIEW_DESKTOP_ID:
                view = nautilus_desktop_canvas_view_new (slot);
        break;
#if ENABLE_EMPTY_VIEW
        case NAUTILUS_VIEW_EMPTY_ID:
                view = nautilus_empty_view_new (slot);
        break;
#endif
        }

        if (view == NULL) {
                g_critical ("Unknown view type ID: %d", id);
        } else if (g_object_is_floating (view)) {
                g_object_ref_sink (view);
        }

        return view;
}
